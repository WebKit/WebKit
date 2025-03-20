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

#include "WKRetainPtr.h"
#include <JavaScriptCore/APICast.h>
#include <WebCore/SerializedScriptValue.h>
#include <optional>
#include <wtf/HashMap.h>
#include <wtf/ObjectIdentifier.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
OBJC_CLASS NSMutableArray;
OBJC_CLASS NSMutableDictionary;
#endif

namespace API {
class Array;
class Dictionary;
class Object;
class SerializedScriptValue;
}

namespace WebCore {
struct ExceptionDetails;
}

namespace WebKit {

class CoreIPCNumber;
class CoreIPCDate;

struct JSObjectIDType;
using JSObjectID = ObjectIdentifier<JSObjectIDType>;

class JavaScriptEvaluationResult {
public:
#if PLATFORM(COCOA)
    enum class NullType : bool { NullPointer, NSNull };
    using Variant = std::variant<NullType, bool, CoreIPCNumber, String, Seconds, Vector<JSObjectID>, HashMap<JSObjectID, JSObjectID>>;

    JavaScriptEvaluationResult(JSObjectID, HashMap<JSObjectID, Variant>&&);
    static Expected<JavaScriptEvaluationResult, std::optional<WebCore::ExceptionDetails>> extract(JSGlobalContextRef, JSValueRef);
    static std::optional<JavaScriptEvaluationResult> extract(id);
#else
    JavaScriptEvaluationResult(std::span<const uint8_t> wireBytes)
        : m_wireBytes(wireBytes) { }
#endif

    JavaScriptEvaluationResult(JSGlobalContextRef, JSValueRef);
    JavaScriptEvaluationResult(JavaScriptEvaluationResult&&);
    JavaScriptEvaluationResult& operator=(JavaScriptEvaluationResult&&);
    ~JavaScriptEvaluationResult();

#if PLATFORM(COCOA)
    JSObjectID root() const { return m_root; }
    const HashMap<JSObjectID, Variant>& map() const { return m_map; }

    RetainPtr<id> toID();
#else
    std::span<const uint8_t> wireBytes() const { return m_wireBytes; }
    Ref<API::SerializedScriptValue> legacySerializedScriptValue() const;
#endif

    WKRetainPtr<WKTypeRef> toWK();

    JSValueRef toJS(JSGlobalContextRef);

private:
#if PLATFORM(COCOA)
    JavaScriptEvaluationResult(id);

    RetainPtr<id> toID(Variant&&);
    RefPtr<API::Object> toAPI(Variant&&);

    Variant toVariant(id);
    JSObjectID addObjectToMap(id);

    Variant toVariant(JSGlobalContextRef, JSValueRef);
    JSObjectID addObjectToMap(JSGlobalContextRef, JSValueRef);

    // Used for deserializing from IPC to ObjC
    HashMap<JSObjectID, RetainPtr<id>> m_instantiatedNSObjects;
    Vector<std::pair<HashMap<JSObjectID, JSObjectID>, RetainPtr<NSMutableDictionary>>> m_nsDictionaries;
    Vector<std::pair<Vector<JSObjectID>, RetainPtr<NSMutableArray>>> m_nsArrays;

    // Used for deserializing from IPC to WKTypeRef
    HashMap<JSObjectID, RefPtr<API::Object>> m_instantiatedObjects;
    Vector<std::pair<HashMap<JSObjectID, JSObjectID>, Ref<API::Dictionary>>> m_dictionaries;
    Vector<std::pair<Vector<JSObjectID>, Ref<API::Array>>> m_arrays;

    // Used for serializing to IPC
    HashMap<JSValueRef, JSObjectID> m_jsObjectsInMap;
    HashMap<RetainPtr<id>, JSObjectID> m_objectsInMap;
    std::optional<JSObjectID> m_nullObjectID;

    // IPC representation
    HashMap<JSObjectID, Variant> m_map;
    JSObjectID m_root;
#else
    RefPtr<WebCore::SerializedScriptValue> m_valueFromJS;
    std::span<const uint8_t> m_wireBytes;
#endif
};

}
