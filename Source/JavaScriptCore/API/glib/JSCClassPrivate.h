/*
 * Copyright (C) 2018 Igalia S.L.
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

#pragma once

#include "JSCClass.h"
#include "JSCContext.h"
#include "JSCValue.h"
#include <wtf/glib/GRefPtr.h>

namespace JSC {
class JSObject;
}

typedef struct OpaqueJSClass* JSClassRef;
typedef struct OpaqueJSContext* JSGlobalContextRef;

GRefPtr<JSCClass> jscClassCreate(JSCContext*, const char*, JSCClass*, JSCClassVTable*, GDestroyNotify);
JSClassRef jscClassGetJSClass(JSCClass*);
JSC::JSObject* jscClassGetOrCreateJSWrapper(JSCClass*, JSCContext*, gpointer);
JSGlobalContextRef jscClassCreateContextWithJSWrapper(JSCClass*, JSCContext*, gpointer);
void jscClassInvalidate(JSCClass*);
