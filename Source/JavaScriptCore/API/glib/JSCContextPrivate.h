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

#include "APICast.h"
#include "JSCContext.h"
#include "JSCValue.h"
#include "JSContextRef.h"
#include <wtf/glib/GRefPtr.h>

GRefPtr<JSCContext> jscContextGetOrCreate(JSGlobalContextRef);
JSGlobalContextRef jscContextGetJSContext(JSCContext*);
GRefPtr<JSCValue> jscContextGetOrCreateValue(JSCContext*, JSValueRef);
void jscContextValueDestroyed(JSCContext*, JSValueRef);
JSC::JSObject* jscContextGetJSWrapper(JSCContext*, gpointer);
JSC::JSObject* jscContextGetOrCreateJSWrapper(JSCContext*, JSClassRef, JSValueRef prototype = nullptr, gpointer = nullptr, GDestroyNotify = nullptr);
JSGlobalContextRef jscContextCreateContextWithJSWrapper(JSCContext*, JSClassRef, JSValueRef prototype = nullptr, gpointer = nullptr, GDestroyNotify = nullptr);
gpointer jscContextWrappedObject(JSCContext*, JSObjectRef);
JSCClass* jscContextGetRegisteredClass(JSCContext*, JSClassRef);

struct CallbackData {
    GRefPtr<JSCContext> context;
    GRefPtr<JSCException> preservedException;
    JSValueRef calleeValue;
    JSValueRef thisValue;
    size_t argumentCount;
    const JSValueRef* arguments;

    CallbackData* next;
};
CallbackData jscContextPushCallback(JSCContext*, JSValueRef calleeValue, JSValueRef thisValue, size_t argumentCount, const JSValueRef* arguments);
void jscContextPopCallback(JSCContext*, CallbackData&&);

bool jscContextHandleExceptionIfNeeded(JSCContext*, JSValueRef);
JSValueRef jscContextGArrayToJSArray(JSCContext*, GPtrArray*, JSValueRef* exception);
JSValueRef jscContextGValueToJSValue(JSCContext*, const GValue*, JSValueRef* exception);
void jscContextJSValueToGValue(JSCContext*, JSValueRef, GType, GValue*, JSValueRef* exception);
