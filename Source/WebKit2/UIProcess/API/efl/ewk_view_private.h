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
#include "ewk_view.h"
#include <Evas.h>
#include <WKEinaSharedString.h>
#include <WebCore/TextDirection.h>
#include <WebKit2/WKBase.h>
#include <wtf/Vector.h>

namespace WebCore {
class IntRect;
class IntSize;
}

namespace WebKit {
class WebPopupItem;
class WebPopupMenuProxyEfl;
}

typedef struct Ewk_Download_Job Ewk_Download_Job;
typedef struct Ewk_Form_Submission_Request Ewk_Form_Submission_Request;
typedef struct Ewk_Url_Request Ewk_Url_Request;
typedef struct Ewk_Url_Response Ewk_Url_Response;
typedef struct Ewk_Error Ewk_Error;
typedef struct Ewk_Resource Ewk_Resource;
typedef struct Ewk_Navigation_Policy_Decision Ewk_Navigation_Policy_Decision;
#if ENABLE(WEB_INTENTS)
typedef struct Ewk_Intent Ewk_Intent;
#endif
#if ENABLE(WEB_INTENTS_TAG)
typedef struct Ewk_Intent_Service Ewk_Intent_Service;
#endif

void ewk_view_page_close(Evas_Object* ewkView);
WKPageRef ewk_view_page_create(Evas_Object* ewkView);
void ewk_view_text_input_state_update(Evas_Object* ewkView);
void ewk_view_url_update(Evas_Object* ewkView);
void ewk_view_contents_size_changed(const Evas_Object* ewkView, const WebCore::IntSize&);

Evas_Object* ewk_view_base_add(Evas* canvas, WKContextRef, WKPageGroupRef);

const Evas_Object* ewk_view_from_page_get(const WebKit::WebPageProxy*);

void ewk_view_popup_menu_request(Evas_Object* ewkView, WebKit::WebPopupMenuProxyEfl* popupMenu, const WebCore::IntRect&, WebCore::TextDirection, double pageScaleFactor, const Vector<WebKit::WebPopupItem>& items, int32_t selectedIndex);
void ewk_view_webprocess_crashed(Evas_Object* ewkView);

void ewk_view_run_javascript_alert(Evas_Object* ewkView, const WKEinaSharedString& message);
bool ewk_view_run_javascript_confirm(Evas_Object* ewkView, const WKEinaSharedString& message);
WKEinaSharedString ewk_view_run_javascript_prompt(Evas_Object* ewkView, const WKEinaSharedString& message, const WKEinaSharedString& defaultValue);

#if ENABLE(SQL_DATABASE)
unsigned long long ewk_view_database_quota_exceeded(Evas_Object* ewkView, const char* databaseName, const char* displayName, unsigned long long currentQuota, unsigned long long currentOriginUsage, unsigned long long currentDatabaseUsage, unsigned long long expectedUsage);
#endif

#endif // ewk_view_private_h
