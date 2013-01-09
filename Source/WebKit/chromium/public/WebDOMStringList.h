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

#ifndef WebDOMStringList_h
#define WebDOMStringList_h

#include <public/WebCommon.h>
#include <public/WebPrivatePtr.h>
#include <public/WebString.h>

namespace WebCore { class DOMStringList; }
#if WEBKIT_IMPLEMENTATION
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace WebKit {

class WebDOMStringList {
public:
    ~WebDOMStringList() { reset(); }

    WEBKIT_EXPORT WebDOMStringList();
    WebDOMStringList(const WebDOMStringList& l) { assign(l); }
    WebDOMStringList& operator=(const WebDOMStringList& l)
    {
        assign(l);
        return *this;
    }

    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void assign(const WebDOMStringList&);

    WEBKIT_EXPORT void append(const WebString&);
    WEBKIT_EXPORT unsigned length() const;
    WEBKIT_EXPORT WebString item(unsigned) const;

#if WEBKIT_IMPLEMENTATION
    WebDOMStringList(const WTF::PassRefPtr<WebCore::DOMStringList>&);
    WebDOMStringList& operator=(const WTF::PassRefPtr<WebCore::DOMStringList>&);
    operator WTF::PassRefPtr<WebCore::DOMStringList>() const;
#endif

private:
    WebPrivatePtr<WebCore::DOMStringList> m_private;
};

} // namespace WebKit

#endif
