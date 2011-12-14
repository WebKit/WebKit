/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebIDBKey_h
#define WebIDBKey_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"
#include "WebString.h"

namespace WebCore { class IDBKey; }

namespace WebKit {

class WebIDBKeyPath;
class WebSerializedScriptValue;

class WebIDBKey {
public:
    // Please use one of the factory methods. This is public only to allow WebVector.
    WebIDBKey() { }
    ~WebIDBKey() { reset(); }

    WEBKIT_API static WebIDBKey createNull();
    WEBKIT_API static WebIDBKey createString(const WebString&);
    WEBKIT_API static WebIDBKey createDate(double);
    WEBKIT_API static WebIDBKey createNumber(double);
    WEBKIT_API static WebIDBKey createInvalid();
    WEBKIT_API static WebIDBKey createFromValueAndKeyPath(const WebSerializedScriptValue&, const WebIDBKeyPath&);
    WEBKIT_API static WebSerializedScriptValue injectIDBKeyIntoSerializedValue(const WebIDBKey&, const WebSerializedScriptValue&, const WebIDBKeyPath&);

    WebIDBKey(const WebIDBKey& e) { assign(e); }
    WebIDBKey& operator=(const WebIDBKey& e)
    {
        assign(e);
        return *this;
    }

    WEBKIT_API void assign(const WebIDBKey&);
    WEBKIT_API void assignNull();
    WEBKIT_API void assignString(const WebString&);
    WEBKIT_API void assignDate(double);
    WEBKIT_API void assignNumber(double);
    WEBKIT_API void assignInvalid();
    WEBKIT_API void reset();

    enum Type {
        NullType = 0,
        StringType,
        DateType,
        NumberType,
        // Types not in WebCore::IDBKey:
        InvalidType
    };

    WEBKIT_API Type type() const;
    WEBKIT_API WebString string() const; // Only valid for StringType.
    WEBKIT_API double date() const; // Only valid for DateType.
    WEBKIT_API double number() const; // Only valid for NumberType.

#if WEBKIT_IMPLEMENTATION
    WebIDBKey(const WTF::PassRefPtr<WebCore::IDBKey>&);
    WebIDBKey& operator=(const WTF::PassRefPtr<WebCore::IDBKey>&);
    operator WTF::PassRefPtr<WebCore::IDBKey>() const;
#endif

private:
    WebPrivatePtr<WebCore::IDBKey> m_private;
};

} // namespace WebKit

#endif // WebIDBKey_h
