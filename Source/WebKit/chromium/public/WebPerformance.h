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

#include "WebNavigationType.h"
#include "platform/WebCommon.h"
#include "platform/WebPrivatePtr.h"

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

    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void assign(const WebPerformance&);

    // This only returns one of {Other|Reload|BackForward}.
    // Form submits and link clicks all fall under other.
    WEBKIT_EXPORT WebNavigationType navigationType() const;

    WEBKIT_EXPORT double navigationStart() const;
    WEBKIT_EXPORT double unloadEventEnd() const;
    WEBKIT_EXPORT double redirectStart() const;
    WEBKIT_EXPORT double redirectEnd() const;
    WEBKIT_EXPORT unsigned short redirectCount() const;
    WEBKIT_EXPORT double fetchStart() const;
    WEBKIT_EXPORT double domainLookupStart() const;
    WEBKIT_EXPORT double domainLookupEnd() const;
    WEBKIT_EXPORT double connectStart() const;
    WEBKIT_EXPORT double connectEnd() const;
    WEBKIT_EXPORT double requestStart() const;
    WEBKIT_EXPORT double responseStart() const;
    WEBKIT_EXPORT double responseEnd() const;
    WEBKIT_EXPORT double domLoading() const;
    WEBKIT_EXPORT double domInteractive() const;
    WEBKIT_EXPORT double domContentLoadedEventStart() const;
    WEBKIT_EXPORT double domContentLoadedEventEnd() const;
    WEBKIT_EXPORT double domComplete() const;
    WEBKIT_EXPORT double loadEventStart() const;
    WEBKIT_EXPORT double loadEventEnd() const;

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
