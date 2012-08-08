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
 *
 */

#ifndef ewk_view_private_h
#define ewk_view_private_h

#include "WebPageProxy.h"
#include <Evas.h>
#include <WebKit2/WKBase.h>

namespace WebCore {
class Cursor;
class IntRect;
class IntSize;
}

typedef struct _Ewk_Download_Job Ewk_Download_Job;
typedef struct _Ewk_Form_Submission_Request Ewk_Form_Submission_Request;
typedef struct _Ewk_Url_Request Ewk_Url_Request;
typedef struct _Ewk_Url_Response Ewk_Url_Response;
typedef struct _Ewk_Web_Error Ewk_Web_Error;
typedef struct _Ewk_Web_Resource Ewk_Web_Resource;
typedef struct _Ewk_Navigation_Policy_Decision Ewk_Navigation_Policy_Decision;
#if ENABLE(WEB_INTENTS)
typedef struct _Ewk_Intent Ewk_Intent;
#endif
#if ENABLE(WEB_INTENTS_TAG)
typedef struct _Ewk_Intent_Service Ewk_Intent_Service;
#endif

void ewk_view_cursor_set(Evas_Object* ewkView, const WebCore::Cursor& cursor);
void ewk_view_display(Evas_Object* ewkView, const WebCore::IntRect& rect);
void ewk_view_download_job_cancelled(Evas_Object* ewkView, Ewk_Download_Job*);
void ewk_view_download_job_failed(Evas_Object* ewkView, Ewk_Download_Job*, Ewk_Web_Error*);
void ewk_view_download_job_finished(Evas_Object* ewkView, Ewk_Download_Job*);
void ewk_view_download_job_requested(Evas_Object* ewkView, Ewk_Download_Job*);
void ewk_view_form_submission_request_new(Evas_Object* ewkView, Ewk_Form_Submission_Request*);
void ewk_view_image_data_set(Evas_Object* ewkView, void* imageData, const WebCore::IntSize& size);
void ewk_view_load_error(Evas_Object* ewkView, const Ewk_Web_Error* error);
void ewk_view_load_finished(Evas_Object* ewkView);
void ewk_view_load_progress_changed(Evas_Object* ewkView, double progress);
void ewk_view_load_provisional_failed(Evas_Object* ewkView, const Ewk_Web_Error* error);
void ewk_view_load_provisional_redirect(Evas_Object* ewkView);
void ewk_view_load_provisional_started(Evas_Object* ewkView);
void ewk_view_navigation_policy_decision(Evas_Object* ewkView, Ewk_Navigation_Policy_Decision* decision);
void ewk_view_new_window_policy_decision(Evas_Object* ewkView, Ewk_Navigation_Policy_Decision* decision);
void ewk_view_page_close(Evas_Object* ewkView);
WKPageRef ewk_view_page_create(Evas_Object* ewkView);
void ewk_view_title_changed(Evas_Object* ewkView, const char* title);
void ewk_view_resource_load_failed(Evas_Object* ewkView, uint64_t resourceIdentifier, Ewk_Web_Error* error);
void ewk_view_resource_load_finished(Evas_Object* ewkView, uint64_t resourceIdentifier);
void ewk_view_resource_load_initiated(Evas_Object* ewkView, uint64_t resourceIdentifier, Ewk_Web_Resource* resource, Ewk_Url_Request* request);
void ewk_view_resource_load_response(Evas_Object* ewkView, uint64_t resourceIdentifier, Ewk_Url_Response* response);
void ewk_view_resource_request_sent(Evas_Object* ewkView, uint64_t resourceIdentifier, Ewk_Url_Request* request, Ewk_Url_Response* redirectResponse);
void ewk_view_text_found(Evas_Object* ewkView, unsigned int matchCount);
void ewk_view_contents_size_changed(const Evas_Object* ewkView, const WebCore::IntSize&);

Evas_Object* ewk_view_base_add(Evas* canvas, WKContextRef, WKPageGroupRef);

#if ENABLE(WEB_INTENTS)
void ewk_view_intent_request_new(Evas_Object* ewkView, const Ewk_Intent* ewkIntent);
#endif
#if ENABLE(WEB_INTENTS_TAG)
void ewk_view_intent_service_register(Evas_Object* ewkView, const Ewk_Intent_Service* ewkIntentService);
#endif

WebKit::WebPageProxy* ewk_view_page_get(const Evas_Object* ewkView);
WebCore::IntSize ewk_view_size_get(const Evas_Object* ewkView);

#if USE(ACCELERATED_COMPOSITING)
bool ewk_view_accelerated_compositing_mode_enter(const Evas_Object* ewkView);
bool ewk_view_accelerated_compositing_mode_exit(const Evas_Object* ewkView);
#endif

#endif // ewk_view_private_h
