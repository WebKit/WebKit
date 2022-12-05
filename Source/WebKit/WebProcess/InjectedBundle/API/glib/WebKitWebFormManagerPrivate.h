/*
 * Copyright (C) 2022 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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

#pragma once

#include "WebKitFrame.h"
#include "WebKitWebFormManager.h"
#include <jsc/jsc.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>

WebKitWebFormManager* webkitWebFormManagerCreate();
void webkitWebFormManagerDidAssociateFormControls(WebKitWebFormManager*, WebKitFrame*, Vector<GRefPtr<JSCValue>>&&);
void webkitWebFormManagerWillSendSubmitEvent(WebKitWebFormManager*, GRefPtr<JSCValue>&&, WebKitFrame*, WebKitFrame*);
void webkitWebFormManagerWillSubmitForm(WebKitWebFormManager*, GRefPtr<JSCValue>&&, WebKitFrame*, WebKitFrame*);
