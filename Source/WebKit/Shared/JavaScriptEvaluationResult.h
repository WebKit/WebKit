/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "NodeInfo.h"
#include "Protected.h"
#include "WKRetainPtr.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/Strong.h>
#include <optional>
#include <wtf/HashMap.h>
#include <wtf/ObjectIdentifier.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
OBJC_CLASS NSMutableArray;
OBJC_CLASS NSMutableDictionary;
#endif

#if USE(GLIB)
#include <wtf/glib/GRefPtr.h>
typedef struct _GVariant GVariant;
typedef struct _JSCValue JSCValue;
#endif

namespace API {
class Array;
class Dictionary;
class Object;
}

namespace WebKit {

struct JSObjectIDType;
using JSObjectID = ObjectIdentifier<JSObjectIDType>;

class JavaScriptEvaluationResult {
public:
    enum class EmptyType : bool { Undefined, Null };
    using Value = Variant<EmptyType, bool, double, String, Seconds, Vector<JSObjectID>, HashMap<JSObjectID, JSObjectID>, NodeInfo>;

    JavaScriptEvaluationResult(JSObjectID, HashMap<JSObjectID, Value>&&);
    static std::optional<JavaScriptEvaluationResult> extract(JSGlobalContextRef, JSValueRef);

    JavaScriptEvaluationResult(JavaScriptEvaluationResult&&);
    JavaScriptEvaluationResult& operator=(JavaScriptEvaluationResult&&);
    ~JavaScriptEvaluationResult();

    JSObjectID root() const { return m_root; }
    const HashMap<JSObjectID, Value>& map() const { return m_map; }

    String toString() const;

#if PLATFORM(COCOA)
    static std::optional<JavaScriptEvaluationResult> extract(id);
    RetainPtr<id> toID();
#endif

#if USE(GLIB)
    static std::optional<JavaScriptEvaluationResult> extract(GVariant*);
    GRefPtr<JSCValue> toJSC();
#endif

    WKRetainPtr<WKTypeRef> toWK();
    RefPtr<API::Object> toAPI();

    Protected<JSValueRef> toJS(JSGlobalContextRef);

private:
    JavaScriptEvaluationResult(JSGlobalContextRef, JSValueRef);

#if USE(GLIB)
    explicit JavaScriptEvaluationResult(GVariant*);
    Value toValue(GVariant*);
    JSObjectID addObjectToMap(GVariant*);
#endif

#if PLATFORM(COCOA)
    JavaScriptEvaluationResult(id);
    RetainPtr<id> toID(Value&&);
    Value toValue(id);
    JSObjectID addObjectToMap(id);
#endif

    RefPtr<API::Object> toAPI(Value&&);
    JSValueRef toJS(JSGlobalContextRef, Value&&);

    Value toValue(JSGlobalContextRef, JSValueRef);
    JSObjectID addObjectToMap(JSGlobalContextRef, JSValueRef);

#if PLATFORM(COCOA)
    // Used for deserializing from IPC to ObjC
    Vector<std::pair<HashMap<JSObjectID, JSObjectID>, RetainPtr<NSMutableDictionary>>> m_nsDictionaries;
    Vector<std::pair<Vector<JSObjectID>, RetainPtr<NSMutableArray>>> m_nsArrays;

    HashMap<JSObjectID, RetainPtr<id>> m_instantiatedNSObjects;
    HashMap<RetainPtr<id>, JSObjectID> m_objectsInMap;
#endif

    // Used for deserializing from IPC to WKTypeRef
    HashMap<JSObjectID, RefPtr<API::Object>> m_instantiatedObjects;
    Vector<std::pair<HashMap<JSObjectID, JSObjectID>, Ref<API::Dictionary>>> m_dictionaries;
    Vector<std::pair<Vector<JSObjectID>, Ref<API::Array>>> m_arrays;

    // Used for serializing to IPC
    HashMap<Protected<JSValueRef>, JSObjectID> m_jsObjectsInMap;
    std::optional<JSObjectID> m_nullObjectID;

    // Used for deserializing from IPC to JS
    HashMap<JSObjectID, Protected<JSValueRef>> m_instantiatedJSObjects;
    Vector<std::pair<HashMap<JSObjectID, JSObjectID>, Protected<JSObjectRef>>> m_jsDictionaries;
    Vector<std::pair<Vector<JSObjectID>, Protected<JSValueRef>>> m_jsArrays;

    // IPC representation
    HashMap<JSObjectID, Value> m_map;
    JSObjectID m_root;
};

}
