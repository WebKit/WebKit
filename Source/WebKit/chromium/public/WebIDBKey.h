/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "platform/WebCommon.h"
#include "platform/WebPrivatePtr.h"
#include "platform/WebString.h"
#include "platform/WebVector.h"

namespace WebCore { class IDBKey; }

namespace WebKit {

class WebIDBKey {
public:
    // Please use one of the factory methods. This is public only to allow WebVector.
    WebIDBKey() { }
    ~WebIDBKey() { reset(); }

    WEBKIT_EXPORT static WebIDBKey createArray(const WebVector<WebIDBKey>&);
    WEBKIT_EXPORT static WebIDBKey createString(const WebString&);
    WEBKIT_EXPORT static WebIDBKey createDate(double);
    WEBKIT_EXPORT static WebIDBKey createNumber(double);
    WEBKIT_EXPORT static WebIDBKey createInvalid();
    WEBKIT_EXPORT static WebIDBKey createNull();

    WebIDBKey(const WebIDBKey& e) { assign(e); }
    WebIDBKey& operator=(const WebIDBKey& e)
    {
        assign(e);
        return *this;
    }

    WEBKIT_EXPORT void assign(const WebIDBKey&);
    WEBKIT_EXPORT void assignArray(const WebVector<WebIDBKey>&);
    WEBKIT_EXPORT void assignString(const WebString&);
    WEBKIT_EXPORT void assignDate(double);
    WEBKIT_EXPORT void assignNumber(double);
    WEBKIT_EXPORT void assignInvalid();
    WEBKIT_EXPORT void assignNull();
    WEBKIT_EXPORT void reset();

    enum Type {
        InvalidType = 0,
        ArrayType,
        StringType,
        DateType,
        NumberType,
        NullType,
    };

    WEBKIT_EXPORT Type type() const;
    WEBKIT_EXPORT bool isValid() const;
    WEBKIT_EXPORT WebVector<WebIDBKey> array() const; // Only valid for ArrayType.
    WEBKIT_EXPORT WebString string() const; // Only valid for StringType.
    WEBKIT_EXPORT double date() const; // Only valid for DateType.
    WEBKIT_EXPORT double number() const; // Only valid for NumberType.

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
