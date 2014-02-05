#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <iostream>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <algorithm>

#include <sinsp.h>
#include "sysdig.h"
#include "chisel.h"

#ifdef _WIN32
#include "win32/getopt.h"
#include <io.h>
#else
#include <unistd.h>
#include <getopt.h>
#endif

bool ctrl_c_pressed = false;

static void usage();

static void signal_callback(int signal)
{
	ctrl_c_pressed = true;
}

//
// Program help
//
static void usage()
{
    printf(
"sysdig version " SYSDIG_VERSION "\n"
"Usage: sysdig [options] [-p <output_format>] [filter]\n\n"
"Options:\n"
" -a, --abstime      Show absolute event timestamps\n"
" -c <chiselname> <chiselargs>, --chisel  <chiselname> <chiselargs>\n"
"                    run the specified chisel. If the chisel require arguments,\n"
"                    they must be specified in the command line after the name.\n"
" -cl, --list-chisels\n"
"                    lists the available chisels. Looks for chisels in .,\n" 
"                    ./chisels, ~/chisels and /usr/share/sysdig/chisels.\n"
" -d, --displayflt   Make the given filter a dsiplay one\n"
"                    Setting this option causes the events to be filtered\n" 
"                    after being parsed by the state system. Events are\n"
"                    normally filtered before being analyzed, which is more\n"
"                    efficient, but can cause state (e.g. FD names) to be lost\n"
" -h, --help         Print this page\n"
" -j, --json         Emit output as json\n"
" -l, --list         List the fields that can be used for filtering and output\n"
"                    formatting\n"
" -L, --list-events  List the events that the engine supports\n"
" -n <num>, --numevents=<num>\n"
"                    Stop capturing after <num> events\n"
" -p <output_format>, --print=<output_format>\n"
"                    Specify the format to be used when printing the events.\n"
"                    See the examples section below for more info.\n"
" -q, --quiet        Don't print events on the screen.\n"
"                    Useful when dumping to disk.\n"
" -r <readfile>, --read=<readfile>\n"
"                    Read the events from <readfile>.\n"
" -S, --summary      print the event summary (i.e. the list of the top events)\n"
"                    when the capture ends.\n"
" -s <len>, --snaplen=<len>\n"
"                    Capture the first <len> bytes of each I/O buffer.\n"
"                    By default, the first 80 bytes are captured. Use this\n"
"                    option with caution, it can generate huge trace files.\n"
" -v, --verbose      Verbose output\n"
" -w <writefile>, --write=<writefile>\n"
"                    Write the captured events to <writefile>.\n"
"\n"
"Output format:\n\n"
"By default, sysdig prints the information for each event on a single line,\n"
"with the following format:\n\n"
"%%evt.time %%evt.cpu %%proc.name (%%thread.tid) %%evt.dir %%evt.type %%evt.args\n\n"
"where:\n"
" evt.time is the event timestamo\n"
" evt.cpu is the CPU number where the event was captured\n"
" proc.name is the name of the process that generated the event\n"
" thread.tid id the TID that generated the event, which corresponds to the\n" 
"   PID for single thread processes\n" 
" evt.dir is the event direction, > for enter events and < for exit events\n" 
" evt.type is the name of the event, e.g. 'open' or 'read'\n"
" evt.args is the list of event arguments.\n\n"
"The output format can be customized with the -p switch, use 'sysdig -l'\n"
"to list the available fields.\n\n"
"Examples:\n\n"
" Capture all the events from the live system and print them to screen\n"
"   $ sysdig\n\n"
" Capture all the events from the live system and save them to disk\n"
"   $ sysdig -qw dumpfile.scap\n\n"
" Read events from a file and print them to screen\n"
"   $ sysdig -r dumpfile.scap\n\n"
" Print all the open system calls invoked by cat\n"
"   $ sysdig proc.name=cat and evt.type=open\n\n"
" Print the name of the files opened by cat\n"
"   $ ./sysdig -p\"%%evt.arg.name\" proc.name=cat and evt.type=open\n\n"
    );
}

void print_summary_table(sinsp* inspector, 
						 vector<summary_table_entry>* summary_table, 
						 uint32_t nentries)
{
	sinsp_evttables* einfo = inspector->get_event_info_tables();

	cout << "----------------------\n";
	string tstr = string("Event");
	tstr.resize(16, ' ');
	tstr += "#Calls\n";
	cout << tstr;
	cout << "----------------------\n";

	sort(summary_table->begin(), summary_table->end(), 
		summary_table_entry_rsort_comparer());

	for(uint32_t j = 0; j < nentries; j++)
	{
		summary_table_entry* e = &summary_table->at(j);

		if(e->m_ncalls == 0)
		{
			break;
		}

		if(e->m_is_unsupported_syscall)
		{
			tstr = einfo->m_syscall_info_table[e->m_id / 2].name;
			tstr.resize(16, ' ');

			printf("%s%s%" PRIu64 "\n", 
				(PPME_IS_ENTER(e->m_id))? "> ": "< ",
				tstr.c_str(), 
				e->m_ncalls);
		}
		else
		{
			tstr = einfo->m_event_info[e->m_id].name;
			tstr.resize(16, ' ');

			printf("%s%s%" PRIu64 "\n", 
				(PPME_IS_ENTER(e->m_id))? "> ": "< ",
				tstr.c_str(), 
				e->m_ncalls);
		}
	}
}

//
// Event processing loop
//
captureinfo do_inspect(sinsp* inspector, 
					   uint64_t cnt, 
					   bool quiet, 
					   bool absolute_times,
					   string format,
					   sinsp_filter* display_filter,
					   vector<sinsp_chisel*>* chisels,
					   vector<summary_table_entry>* summary_table)
{
	captureinfo retval;
	int32_t res;
	sinsp_evt* ev;
	uint64_t ts;
	uint64_t deltats = 0;
	uint64_t firstts = 0;
	string line;
	sinsp_evt_formatter formatter(inspector, format);

	//
	// Loop through the events
	//
	while(1)
	{
		if(retval.m_nevts == cnt || ctrl_c_pressed)
		{
			break;
		}

		res = inspector->next(&ev);

		if(res == SCAP_TIMEOUT)
		{
			continue;
		}
		else if(res == SCAP_EOF)
		{
			for(vector<sinsp_chisel*>::iterator it = chisels->begin(); it != chisels->end(); ++it)
			{
				(*it)->on_capture_end();
			}

			break;
		}
		else if(res != SCAP_SUCCESS)
		{
			cerr << "res = " << res << endl;
			throw sinsp_exception(inspector->getlasterr().c_str());
		}

		retval.m_nevts++;

		ts = ev->get_ts();
		if(firstts == 0)
		{
			firstts = ts;
		}
		deltats = ts - firstts;

		//
		// If there are chisels to run, run them
		//
		if(!chisels->empty())
		{
			for(vector<sinsp_chisel*>::iterator it = chisels->begin(); it != chisels->end(); ++it)
			{
				(*it)->run(ev);
			}
		}
		else
		{		
			//
			// If we're supposed to summarize, increase the count for this event
			//
			if(summary_table != NULL)
			{
				uint16_t etype = ev->get_type();
				
				if(etype == PPME_GENERIC_E)
				{
					sinsp_evt_param *parinfo = ev->get_param(0);
					uint16_t id = *(int16_t *)parinfo->m_val;
					((*summary_table)[PPM_EVENT_MAX + id * 2]).m_ncalls++;
				}
				else if(etype == PPME_GENERIC_X)
				{
					sinsp_evt_param *parinfo = ev->get_param(0);
					uint16_t id = *(int16_t *)parinfo->m_val;
					((*summary_table)[PPM_EVENT_MAX + id * 2 + 1]).m_ncalls++;
				}
				else
				{
					((*summary_table)[etype]).m_ncalls++;
				}
			}

			//
			// When the quiet flag is specified, we don't do any kind of processing other
			// than counting the events.
			//
			if(quiet)
			{
				continue;
			}

			//
			// Output the line
			//
			if(display_filter)
			{
				if(!display_filter->run(ev))
				{
					continue;
				}
			}

			if(formatter.tostring(ev, &line))
			{
				cout << line << endl;
			}
		}
	}

	retval.m_time = deltats;
	return retval;
}

//
// MAIN
//
int main(int argc, char **argv)
{
	int res = EXIT_SUCCESS;
	sinsp* inspector = NULL;
	string infile;
	string outfile;
	int op;
	uint64_t cnt = -1;
	bool emitjson = false;
	bool quiet = false;
	bool absolute_times = false;
	bool is_filter_display = false;
	bool verbose = false;
	sinsp_filter* display_filter = NULL;
	double duration = 1;
	captureinfo cinfo;
	string output_format;
	uint32_t snaplen = 0;
	int long_index = 0;
	vector<sinsp_chisel*> chisels;
	int32_t n_filterargs = 0;
	int cflag = 0;
	string cname;
	vector<summary_table_entry>* summary_table = NULL;


    static struct option long_options[] = 
	{
        {"abstimes", no_argument, 0, 'a' },
        {"chisel", required_argument, 0, 'c' },
        {"list-chisels", no_argument, &cflag, 1 },
        {"chisel-info", required_argument, &cflag, 2 },
        {"displayflt", no_argument, 0, 'd' },
        {"help", no_argument, 0, 'h' },
        {"json", no_argument, 0, 'j' },
        {"list", no_argument, 0, 'l' },
        {"list-events", no_argument, 0, 'L' },
        {"numevents", required_argument, 0, 'n' },
        {"print", required_argument, 0, 'p' },
        {"quiet", no_argument, 0, 'q' },
        {"readfile", required_argument, 0, 'r' },
        {"snaplen", required_argument, 0, 's' },
        {"summary", no_argument, 0, 'S' },
        {"verbose", no_argument, 0, 'v' },
        {"writefile", required_argument, 0, 'w' },
        {0, 0, 0, 0}
    };

//	output_format = "*%evt.num)%evt.reltime.s.%evt.reltime.ns %evt.cpu %proc.name (%thread.tid) %evt.dir %evt.type %evt.args";
	output_format = DEFAULT_OUTPUT_STR;

	try
	{
		inspector = new sinsp();

		inspector->add_chisel_dir(SYSDIG_INSTALLATION_DIR CHISELS_INSTALLATION_DIR);

		//
		// Parse the args
		//
		while((op = getopt_long(argc, argv, "ac:dhjlLn:p:qr:Ss:vw:", long_options, &long_index)) != -1)
		{
			switch(op)
			{
			case 'a':
				absolute_times = true;
				break;
			case 0:
				if(cflag != 1 && cflag != 2)
				{
					break;
				}

				if(cflag == 2)
				{
					cname = optarg;
				}
			case 'c':
				{
					if(cflag == 0)
					{
						string ostr(optarg);

						if(ostr.size() >= 1)
						{
							if(ostr == "l")
							{
								cflag = 1;
							}
							//else if(ostr[0] == 'i')
							//{
							//	cflag = 2;
							//	cname = ostr.substr(1,  string::npos);
							//}
						}
					}

					if(cflag == 1)
					{
						vector<chisel_desc> chlist;
						sinsp_chisel::get_chisel_list(&chlist);
						list_chisels(&chlist);
						delete inspector;
						return EXIT_SUCCESS;
					}
					if(cflag == 2)
					{
						vector<chisel_desc> chlist;
						sinsp_chisel::get_chisel_list(&chlist);

						for(uint32_t j = 0; j < chlist.size(); j++)
						{
							if(chlist[j].m_name == cname)
							{
								//print_chisel_info(&chlist[j]);
								delete inspector;
								return EXIT_SUCCESS;
							}
						}

						throw sinsp_exception("chisel " + cname + " not found");
					}

					sinsp_chisel* ch = new sinsp_chisel(inspector, optarg);
					uint32_t nargs = ch->get_n_args();
					vector<string> args;

					for(uint32_t j = 0; j < nargs; j++)
					{
						if(optind + j >= (uint32_t)argc)
						{
							throw sinsp_exception("invalid number of arguments for chisel " + string(optarg) + ", " + to_string((long long int)nargs) + " expected.");
						}

						args.push_back(argv[optind + j]);
						n_filterargs++;
					}

					ch->set_args(&args);

					chisels.push_back(ch);
				}
				break;
			case 'd':
				is_filter_display = true;
				break;
			case 'j':
				emitjson = true;
				{
					ASSERT(false);
					throw sinsp_exception("json option not yet implemented");
				}
				break;
			case 'h':
				usage();
				delete inspector;
				return EXIT_SUCCESS;
			case 'l':
				list_fields();
				delete inspector;
				return EXIT_SUCCESS;
			case 'L':
				list_events(inspector);
				delete inspector;
				return EXIT_SUCCESS;
			case 'n':
				cnt = atoi(optarg);
				if(cnt <= 0)
				{
					throw sinsp_exception(string("invalid packet count") + optarg);
					res = EXIT_FAILURE;
					goto exit;
				}
				break;
			case 'p':
				if(string(optarg) == "p")
				{
					//
					// -pp shows the default output format, useful if the user wants to tweak it.
					//
					printf("%s\n", output_format.c_str());
					delete inspector;
					return EXIT_SUCCESS;
				}
				else
				{
					output_format = optarg;
				}

				break;
			case 'r':
				infile = optarg;
				break;
			case 'S':
				summary_table = new vector<summary_table_entry>;

				for(uint32_t j = 0; j < PPM_EVENT_MAX; j++)
				{
					summary_table->push_back(summary_table_entry(j, false)); 
				}

				for(uint32_t j = 0; j < PPM_SC_MAX * 2; j++)
				{
					summary_table->push_back(summary_table_entry(j, true));
				}

				break;
			case 's':
				snaplen = atoi(optarg);
				break;
			case 'q':
				quiet = true;
				break;
			case 'v':
				verbose = true;
				break;
			case 'w':
				outfile = optarg;
				break;
			default:
				break;
			}
		}

		//
		// the filter is at the end of the command line
		//
		if(optind + n_filterargs < argc)
		{
	#ifdef HAS_FILTERING
			string filter;

			for(int32_t j = optind + n_filterargs; j < argc; j++)
			{
				filter += argv[j];
				if(j < argc)
				{
					filter += " ";
				}
			}

			try
			{
				if(is_filter_display)
				{
					display_filter = new sinsp_filter(inspector, filter);
				}
				else
				{
					inspector->set_filter(filter);
				}
			}
			catch(sinsp_exception e)
			{
				cerr << e.what() << endl;
				res = EXIT_FAILURE;
			}
	#else
			fprintf(stderr, "filtering not compiled.\n");
			res = EXIT_FAILURE;
			goto exit;
	#endif
		}

		//
		// Set the CRTL+C signal
		//
		if(signal(SIGINT, signal_callback) == SIG_ERR)
		{
			fprintf(stderr, "An error occurred while setting a signal handler.\n");
			res = EXIT_FAILURE;
			goto exit;
		}

		//
		// Launch the inspeciotn
		//
		if(infile != "")
		{
			inspector->open(infile);
		}
		else
		{
			inspector->open("");
		}

//inspector->start_dropping_mode(32);

		if(snaplen != 0)
		{
			inspector->set_snaplen(snaplen);
		}

		if(outfile != "")
		{
			inspector->autodump_start(outfile);
		}

		duration = ((double)clock()) / CLOCKS_PER_SEC;
			
		cinfo = do_inspect(inspector, 
			cnt, 
			quiet, 
			absolute_times,
			output_format,
			display_filter,
			&chisels,
			summary_table);

		duration = ((double)clock()) / CLOCKS_PER_SEC - duration;

		scap_stats cstats;
		inspector->get_capture_stats(&cstats);

		if(verbose)
		{
			fprintf(stderr, "Driver Events:%" PRIu64 "\nDriver Drops:%" PRIu64 "\n",
				cstats.n_evts,
				cstats.n_drops);

			fprintf(stderr, "Elapsed time: %.3lf, Captured Events: %" PRIu64 ", %.2lf eps\n",
				duration,
				cinfo.m_nevts,
				(double)cinfo.m_nevts / duration);
		}
	}
	catch(sinsp_exception e)
	{
		cerr << e.what() << endl;
		res = EXIT_FAILURE;
	}
	catch(...)
	{
		res = EXIT_FAILURE;
	}

exit:

	//
	// If there's a summary table, sort and print it
	//
	if(summary_table != NULL)
	{
		print_summary_table(inspector, summary_table, 100);
	}

	//
	// Free the chisels
	//
	for(vector<sinsp_chisel*>::iterator it = chisels.begin(); it != chisels.end(); ++it)
	{
		delete *it;
	}

	if(inspector)
	{
		delete inspector;
	}

	if(display_filter)
	{
		delete display_filter;
	}

#ifdef _WIN32
	_CrtDumpMemoryLeaks();
#endif

	return res;
}
