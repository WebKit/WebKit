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

#ifndef WebIDBKeyPath_h
#define WebIDBKeyPath_h

#include "platform/WebCommon.h"
#include "platform/WebPrivateOwnPtr.h"
#include "platform/WebString.h"
#include "platform/WebVector.h"

namespace WebCore { class IDBKeyPath; }

namespace WebKit {

class WebIDBKeyPath {
public:
    WEBKIT_EXPORT static WebIDBKeyPath create(const WebString&);
    WEBKIT_EXPORT static WebIDBKeyPath create(const WebVector<WebString>&);
    WEBKIT_EXPORT static WebIDBKeyPath createNull();

    WebIDBKeyPath(const WebIDBKeyPath& keyPath) { assign(keyPath); }
    virtual ~WebIDBKeyPath() { reset(); }
    WebIDBKeyPath& operator=(const WebIDBKeyPath& keyPath)
    {
        assign(keyPath);
        return *this;
    }

    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void assign(const WebIDBKeyPath&);

    enum Type {
        NullType = 0,
        StringType,
        ArrayType,
    };

    WEBKIT_EXPORT bool isValid() const;
    WEBKIT_EXPORT Type type() const;
    WEBKIT_EXPORT WebVector<WebString> array() const; // Only valid for ArrayType.
    WEBKIT_EXPORT WebString string() const; // Only valid for StringType.

#if WEBKIT_IMPLEMENTATION
    WebIDBKeyPath(const WebCore::IDBKeyPath&);
    WebIDBKeyPath& operator=(const WebCore::IDBKeyPath&);
    operator const WebCore::IDBKeyPath&() const;
#endif

private:
    WebPrivateOwnPtr<WebCore::IDBKeyPath> m_private;
};

} // namespace WebKit

#endif // WebIDBKeyPath_h
