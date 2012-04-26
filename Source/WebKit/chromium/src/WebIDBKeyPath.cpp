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

#include "config.h"
#include "WebIDBKeyPath.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBKeyPath.h"
#include "platform/WebString.h"
#include "platform/WebVector.h"
#include <wtf/Vector.h>

using namespace WebCore;

namespace WebKit {

WebIDBKeyPath WebIDBKeyPath::create(const WebVector<WebString>&)
{
    // FIXME: Array-type key paths not yet supported. http://webkit.org/b/84207
    WEBKIT_ASSERT_NOT_REACHED();
    return createNull();
}

WebIDBKeyPath WebIDBKeyPath::create(const WebString& keyPath)
{
    if (keyPath.isNull())
        return createNull();

    WTF::Vector<WTF::String> idbElements;
    IDBKeyPathParseError idbError;
    IDBParseKeyPath(keyPath, idbElements, idbError);
    return WebIDBKeyPath(idbElements, static_cast<int>(idbError));
}

WebIDBKeyPath WebIDBKeyPath::createNull()
{
    return WebIDBKeyPath(WebString());
}

WebIDBKeyPath::WebIDBKeyPath(const WebIDBKeyPath& keyPath)
{
    assign(keyPath);
}

WebIDBKeyPath::WebIDBKeyPath(const WTF::Vector<WTF::String>& elements, int parseError)
    : m_private(new WTF::Vector<WTF::String>(elements))
    , m_parseError(parseError)
{
}

bool WebIDBKeyPath::isValid() const
{
    return m_parseError == IDBKeyPathParseErrorNone;
}

WebIDBKeyPath::Type WebIDBKeyPath::type() const
{
    return m_private.get() ? StringType : NullType;
}

WebString WebIDBKeyPath::string() const
{
    if (!m_private.get())
        return WebString();

    // FIXME: Store the complete string instead of rebuilding it.
    // http://webkit.org/b/84207
    WTF::String string("");
    WTF::Vector<WTF::String>& array = *m_private.get();
    for (size_t i = 0; i < array.size(); ++i) {
        if (i)
            string.append(".");
        string.append(array[i]);
    }
    return WebString(string);
}

WebIDBKeyPath::WebIDBKeyPath(const WebString& keyPath)
    : m_parseError(IDBKeyPathParseErrorNone)
{
    if (!keyPath.isNull()) {
        m_private.reset(new WTF::Vector<WTF::String>());
        IDBKeyPathParseError idbParseError;
        IDBParseKeyPath(keyPath, *m_private.get(), idbParseError);
        m_parseError = idbParseError;
    }
}

int WebIDBKeyPath::parseError() const
{
    return m_parseError;
}

void WebIDBKeyPath::assign(const WebIDBKeyPath& keyPath)
{
    m_parseError = keyPath.m_parseError;
    if (keyPath.m_private.get())
        m_private.reset(new WTF::Vector<WTF::String>(keyPath));
    else
        m_private.reset(0);
}

void WebIDBKeyPath::reset()
{
    m_private.reset(0);
}

WebIDBKeyPath::operator const WTF::Vector<WTF::String, 0>&() const
{
    ASSERT(m_private.get());
    return *m_private.get();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
