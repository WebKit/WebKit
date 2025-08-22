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

#include "JSHandleInfo.h"
#include "Protected.h"
#include <JavaScriptCore/APICast.h>
#include <WebCore/SerializedNode.h>
#include <optional>
#include <wtf/HashMap.h>
#include <wtf/ObjectIdentifier.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#endif

#if USE(GLIB)
#include <wtf/glib/GRefPtr.h>
typedef struct _GVariant GVariant;
typedef struct _JSCValue JSCValue;
#endif

namespace WebCore {
struct SerializedNode;
}

namespace API {
class Object;
}

namespace WebKit {

struct JSHandleInfo;
struct JSObjectIDType;
using JSObjectID = ObjectIdentifier<JSObjectIDType>;

class JavaScriptEvaluationResult {
public:
    enum class EmptyType : bool { Undefined, Null };
    using ObjectMap = HashMap<JSObjectID, JSObjectID>;
    using Value = Variant<
        EmptyType,
        bool,
        double,
        String,
        Seconds,
        Vector<JSObjectID>,
        ObjectMap,
        UniqueRef<JSHandleInfo>,
        UniqueRef<WebCore::SerializedNode>
    >;
    using Map = HashMap<JSObjectID, Value>;

    JavaScriptEvaluationResult(JSObjectID, Map&&);

    JavaScriptEvaluationResult(JavaScriptEvaluationResult&&);
    JavaScriptEvaluationResult& operator=(JavaScriptEvaluationResult&&);
    ~JavaScriptEvaluationResult();

    JSObjectID root() const { return m_root; }
    const Map& map() const { return m_map; }

    String toString() const;

#if PLATFORM(COCOA)
    static std::optional<JavaScriptEvaluationResult> extract(id);
    RetainPtr<id> toID();
#endif

#if USE(GLIB)
    static std::optional<JavaScriptEvaluationResult> extract(GVariant*);
    GRefPtr<JSCValue> toJSC();
#endif

    static std::optional<JavaScriptEvaluationResult> extract(JSGlobalContextRef, JSValueRef);
    Protected<JSValueRef> toJS(JSGlobalContextRef);

    static std::optional<JavaScriptEvaluationResult> extract(API::Object*);
    RefPtr<API::Object> toAPI();

private:
    static JavaScriptEvaluationResult jsUndefined();

#if PLATFORM(COCOA)
    class ObjCExtractor;
    class ObjCInserter;
#endif

#if USE(GLIB)
    class GLibExtractor;
    // GLib uses JS for insertion.
#endif

    class JSExtractor;
    class JSInserter;

    class APIExtractor;
    class APIInserter;

    Map m_map;
    JSObjectID m_root;
};

}
