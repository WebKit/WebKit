/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebSerializedScriptValue_h
#define WebSerializedScriptValue_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"

namespace WebCore { class SerializedScriptValue; }

#if WEBKIT_USING_V8
namespace v8 {
class Value;
template <class T> class Handle;
}
#endif

namespace WebKit {
class WebString;

// FIXME: Should this class be in platform?
class WebSerializedScriptValue {
public:
    ~WebSerializedScriptValue() { reset(); }

    WebSerializedScriptValue() { }
    WebSerializedScriptValue(const WebSerializedScriptValue& d) { assign(d); }
    WebSerializedScriptValue& operator=(const WebSerializedScriptValue& d)
    {
        assign(d);
        return *this;
    }

    WEBKIT_EXPORT static WebSerializedScriptValue fromString(const WebString&);

#if WEBKIT_USING_V8
    WEBKIT_EXPORT static WebSerializedScriptValue serialize(v8::Handle<v8::Value>);
#endif

    // Create a WebSerializedScriptValue that represents a serialization error.
    WEBKIT_EXPORT static WebSerializedScriptValue createInvalid();

    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void assign(const WebSerializedScriptValue&);

    bool isNull() const { return m_private.isNull(); }

    // Returns a string representation of the WebSerializedScriptValue.
    WEBKIT_EXPORT WebString toString() const;

#if WEBKIT_USING_V8
    // Convert the serialized value to a parsed v8 value.
    WEBKIT_EXPORT v8::Handle<v8::Value> deserialize();
#endif

#if WEBKIT_IMPLEMENTATION
    WebSerializedScriptValue(const WTF::PassRefPtr<WebCore::SerializedScriptValue>&);
    WebSerializedScriptValue& operator=(const WTF::PassRefPtr<WebCore::SerializedScriptValue>&);
    operator WTF::PassRefPtr<WebCore::SerializedScriptValue>() const;
#endif

private:
    WebPrivatePtr<WebCore::SerializedScriptValue> m_private;
};

} // namespace WebKit

#endif
