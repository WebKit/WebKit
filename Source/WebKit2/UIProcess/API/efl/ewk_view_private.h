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
class IntRect;
class IntSize;
}

#if ENABLE(WEB_INTENTS)
typedef struct _Ewk_Intent Ewk_Intent;
#endif
#if ENABLE(WEB_INTENTS_TAG)
typedef struct _Ewk_Intent_Service Ewk_Intent_Service;
#endif

void ewk_view_display(Evas_Object* ewkView, const WebCore::IntRect& rect);
void ewk_view_image_data_set(Evas_Object* ewkView, void* imageData, const WebCore::IntSize& size);
void ewk_view_title_changed(Evas_Object* ewkView, const char* title);

Evas_Object* ewk_view_base_add(Evas* canvas, WKContextRef, WKPageGroupRef);

#if ENABLE(WEB_INTENTS)
void ewk_view_intent_request_new(Evas_Object* ewkView, const Ewk_Intent* ewkIntent);
#endif
#if ENABLE(WEB_INTENTS_TAG)
void ewk_view_intent_service_register(Evas_Object* ewkView, const Ewk_Intent_Service* ewkIntentService);
#endif

WebKit::WebPageProxy* ewk_view_page_get(const Evas_Object* ewkView);

#endif // ewk_view_private_h
