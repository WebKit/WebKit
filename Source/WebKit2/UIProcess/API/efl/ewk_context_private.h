/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef ewk_context_private_h
#define ewk_context_private_h

#include <WebKit2/WKBase.h>

typedef struct _Ewk_Context Ewk_Context;
typedef struct _Ewk_Download_Job Ewk_Download_Job;
typedef struct _Ewk_Url_Scheme_Request Ewk_Url_Scheme_Request;

WKContextRef ewk_context_WKContext_get(const Ewk_Context*);
Ewk_Context* ewk_context_new_from_WKContext(WKContextRef);
WKSoupRequestManagerRef ewk_context_request_manager_get(const Ewk_Context*);
void ewk_context_url_scheme_request_received(Ewk_Context*, Ewk_Url_Scheme_Request*);

void ewk_context_download_job_add(Ewk_Context*, Ewk_Download_Job*);
Ewk_Download_Job* ewk_context_download_job_get(const Ewk_Context*, uint64_t downloadId);
void ewk_context_download_job_remove(Ewk_Context*, uint64_t downloadId);

#endif // ewk_context_private_h
