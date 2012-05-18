/*
    Copyright (C) 2012 Intel Corporation

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef ewk_intent_private_h
#define ewk_intent_private_h

#if ENABLE(WEB_INTENTS)
// forward declarations
namespace WebCore {
class Intent;
class IntentRequest;
}

Ewk_Intent* ewk_intent_new(WebCore::Intent* core);
void ewk_intent_free(Ewk_Intent* intent);
Ewk_Intent_Request* ewk_intent_request_new(PassRefPtr<WebCore::IntentRequest> core);
#endif

#endif // ewk_intent_private_h
