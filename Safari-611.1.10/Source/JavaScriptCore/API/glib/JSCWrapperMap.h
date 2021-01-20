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

#include "JSBase.h"
#include "VM.h"
#include "WeakGCMap.h"
#include <glib.h>
#include <wtf/HashMap.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

typedef struct _JSCClass JSCClass;
typedef struct _JSCContext JSCContext;
typedef struct _JSCValue JSCValue;

namespace JSC {

class JSObject;

class WrapperMap {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WrapperMap(JSGlobalContextRef);
    ~WrapperMap();

    GRefPtr<JSCValue> gobjectWrapper(JSCContext*, JSValueRef);
    void unwrap(JSValueRef);

    void registerClass(JSCClass*);
    JSCClass* registeredClass(JSClassRef) const;

    JSObject* createJSWrappper(JSGlobalContextRef, JSClassRef, JSValueRef prototype, gpointer, GDestroyNotify);
    JSGlobalContextRef createContextWithJSWrappper(JSContextGroupRef, JSClassRef, JSValueRef prototype, gpointer, GDestroyNotify);
    JSObject* jsWrapper(gpointer wrappedObject) const;
    gpointer wrappedObject(JSGlobalContextRef, JSObjectRef) const;

private:
    HashMap<JSValueRef, JSCValue*> m_cachedGObjectWrappers;
    std::unique_ptr<JSC::WeakGCMap<gpointer, JSC::JSObject>> m_cachedJSWrappers;
    HashMap<JSClassRef, GRefPtr<JSCClass>> m_classMap;
};

} // namespace JSC
