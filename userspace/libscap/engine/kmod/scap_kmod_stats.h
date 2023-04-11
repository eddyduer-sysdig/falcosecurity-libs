/*
Copyright (C) 2023 The Falco Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#pragma once

typedef enum kmod_kernel_counters_stats {
	KMOD_N_EVTS = 0,
	KMOD_N_DROPS_BUFFER_TOTAL,
	KMOD_N_DROPS_BUFFER_CLONE_FORK_ENTER,
	KMOD_N_DROPS_BUFFER_CLONE_FORK_EXIT,
	KMOD_N_DROPS_BUFFER_EXECVE_ENTER,
	KMOD_N_DROPS_BUFFER_EXECVE_EXIT,
	KMOD_N_DROPS_BUFFER_CONNECT_ENTER,
	KMOD_N_DROPS_BUFFER_CONNECT_EXIT,
	KMOD_N_DROPS_BUFFER_OPEN_ENTER,
	KMOD_N_DROPS_BUFFER_OPEN_EXIT,
	KMOD_N_DROPS_BUFFER_DIR_FILE_ENTER,
	KMOD_N_DROPS_BUFFER_DIR_FILE_EXIT,
	KMOD_N_DROPS_BUFFER_OTHER_INTEREST_ENTER,
	KMOD_N_DROPS_BUFFER_OTHER_INTEREST_EXIT,
	KMOD_N_DROPS_PAGE_FAULTS,
	KMOD_N_DROPS_BUG,
	KMOD_N_DROPS,
	KMOD_N_PREEMPTIONS,
	KMOD_MAX_KERNEL_COUNTERS_STATS
}kmod_kernel_counters_stats;

static const char * const kmod_kernel_counters_stats_names[] = {
	[KMOD_N_EVTS] = "n_evts",
	[KMOD_N_DROPS_BUFFER_TOTAL] = "n_drops_buffer_total",
	[KMOD_N_DROPS_BUFFER_CLONE_FORK_ENTER] = "n_drops_buffer_clone_fork_enter",
	[KMOD_N_DROPS_BUFFER_CLONE_FORK_EXIT] = "n_drops_buffer_clone_fork_exit",
	[KMOD_N_DROPS_BUFFER_EXECVE_ENTER] = "n_drops_buffer_execve_enter",
	[KMOD_N_DROPS_BUFFER_EXECVE_EXIT] = "n_drops_buffer_execve_exit",
	[KMOD_N_DROPS_BUFFER_CONNECT_ENTER] = "n_drops_buffer_connect_enter",
	[KMOD_N_DROPS_BUFFER_CONNECT_EXIT] = "n_drops_buffer_connect_exit",
	[KMOD_N_DROPS_BUFFER_OPEN_ENTER] = "n_drops_buffer_open_enter",
	[KMOD_N_DROPS_BUFFER_OPEN_EXIT] = "n_drops_buffer_open_exit",
	[KMOD_N_DROPS_BUFFER_DIR_FILE_ENTER] = "n_drops_buffer_dir_file_enter",
	[KMOD_N_DROPS_BUFFER_DIR_FILE_EXIT] = "n_drops_buffer_dir_file_exit",
	[KMOD_N_DROPS_BUFFER_OTHER_INTEREST_ENTER] = "n_drops_buffer_other_interest_enter",
	[KMOD_N_DROPS_BUFFER_OTHER_INTEREST_EXIT] = "n_drops_buffer_other_interest_exit",
	[KMOD_N_DROPS_PAGE_FAULTS] = "n_drops_page_faults",
	[KMOD_N_DROPS_BUG] = "n_drops_bug",
	[KMOD_N_DROPS] = "n_drops",
	[KMOD_N_PREEMPTIONS] = "n_preemptions",
};