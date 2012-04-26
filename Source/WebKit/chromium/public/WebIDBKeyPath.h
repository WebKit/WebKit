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

namespace WTF {
template<typename T, size_t inlineCapacity> class Vector;
class String;
}

namespace WebKit {

class WebIDBKeyPath {
public:
    WEBKIT_EXPORT static WebIDBKeyPath create(const WebString&);
    WEBKIT_EXPORT static WebIDBKeyPath create(const WebVector<WebString>&);
    WEBKIT_EXPORT static WebIDBKeyPath createNull();
    WEBKIT_EXPORT WebIDBKeyPath(const WebIDBKeyPath&);
    ~WebIDBKeyPath() { reset(); }

    enum Type {
        NullType = 0,
        StringType,
        ArrayType,
    };

    WEBKIT_EXPORT bool isValid() const;
    WEBKIT_EXPORT Type type() const;
    // FIXME: Array-type key paths not yet supported. http://webkit.org/b/84207
    WebVector<WebString> array() const { WEBKIT_ASSERT_NOT_REACHED(); return WebVector<WebString>(); }
    WEBKIT_EXPORT WebString string() const;

    // FIXME: Remove these once callers are updated. http://webkit.org/b/84207
    WEBKIT_EXPORT WebIDBKeyPath(const WebString&);
    operator const WebString () const { return string(); }
    WEBKIT_EXPORT int parseError() const;
    WEBKIT_EXPORT void assign(const WebIDBKeyPath&);
    WEBKIT_EXPORT void reset();

#if WEBKIT_IMPLEMENTATION
    operator const WTF::Vector<WTF::String, 0>& () const;
#endif

private:
    WebIDBKeyPath();

#if WEBKIT_IMPLEMENTATION
    WebIDBKeyPath(const WTF::Vector<WTF::String, 0>&, int parseError);
#endif

    WebPrivateOwnPtr<WTF::Vector<WTF::String, 0> > m_private;
    int m_parseError;
};

} // namespace WebKit

#endif // WebIDBKeyPath_h
