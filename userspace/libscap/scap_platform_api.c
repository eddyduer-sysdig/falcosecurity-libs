/*
Copyright (C) 2022 The Falco Authors.

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

#include "scap_platform_api.h"
#include "scap_platform_impl.h"

#include "scap.h"
#include "scap-int.h"

scap_addrlist* scap_get_ifaddr_list(scap_t* handle)
{
	if (handle && handle->m_platform)
	{
		return handle->m_platform->m_addrlist;
	}

	return NULL;
}

void scap_refresh_iflist(scap_t* handle)
{
	if (handle && handle->m_platform && handle->m_platform->m_vtable->refresh_addr_list)
	{
		handle->m_platform->m_vtable->refresh_addr_list(handle->m_platform);
	}
}

scap_userlist* scap_get_user_list(scap_t* handle)
{
	if (handle && handle->m_platform)
	{
		return handle->m_platform->m_userlist;
	}

	return NULL;
}