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

#ifndef WebPerformance_h
#define WebPerformance_h

#include "WebCommon.h"
#include "WebNavigationType.h"
#include "WebPrivatePtr.h"

namespace WebCore { class Performance; }

namespace WebKit {

class WebPerformance {
public:
    ~WebPerformance() { reset(); }

    WebPerformance() { }

    WebPerformance(const WebPerformance& p) { assign(p); }

    WebPerformance& operator=(const WebPerformance& p)
    {
        assign(p);
        return *this;
    }

    WEBKIT_API void reset();
    WEBKIT_API void assign(const WebPerformance&);

    // This only returns one of {Other|Reload|BackForward}.
    // Form submits and link clicks all fall under other.
    WEBKIT_API WebNavigationType navigationType() const;

    WEBKIT_API double navigationStart() const;
    WEBKIT_API double unloadEventEnd() const;
    WEBKIT_API double redirectStart() const;
    WEBKIT_API double redirectEnd() const;
    WEBKIT_API unsigned short redirectCount() const;
    WEBKIT_API double fetchStart() const;
    WEBKIT_API double domainLookupStart() const;
    WEBKIT_API double domainLookupEnd() const;
    WEBKIT_API double connectStart() const;
    WEBKIT_API double connectEnd() const;
    WEBKIT_API double requestStart() const;
    WEBKIT_API double responseStart() const;
    WEBKIT_API double responseEnd() const;
    WEBKIT_API double loadEventStart() const;
    WEBKIT_API double loadEventEnd() const;

#if WEBKIT_IMPLEMENTATION
    WebPerformance(const WTF::PassRefPtr<WebCore::Performance>&);
    WebPerformance& operator=(const WTF::PassRefPtr<WebCore::Performance>&);
    operator WTF::PassRefPtr<WebCore::Performance>() const;
#endif

private:
    WebPrivatePtr<WebCore::Performance> m_private;
};

} // namespace WebKit

#endif
