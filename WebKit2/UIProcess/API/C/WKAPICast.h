/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WKAPICast_h
#define WKAPICast_h

#include "WKBase.h"

#if defined(WIN32) || defined(_WIN32)
#include "WKAPICastWin.h"
#endif

namespace WebCore {
    class StringImpl;
}

namespace WebKit {
    class WebContext;
    class WebFramePolicyListenerProxy;
    class WebFrameProxy;
    class WebPageNamespace;
    class WebPageProxy;
    class WebPreferences;
    class KURLWrapper;
}

/* Opaque typing convenience methods */

inline WebKit::WebFrameProxy* toWK(WKFrameRef f)
{
    return reinterpret_cast<WebKit::WebFrameProxy*>(f);
}

inline WKFrameRef toRef(WebKit::WebFrameProxy* f)
{
    return reinterpret_cast<WKFrameRef>(f);
}

inline WebKit::WebPageProxy* toWK(WKPageRef p)
{
    return reinterpret_cast<WebKit::WebPageProxy*>(p);
}

inline WKPageRef toRef(WebKit::WebPageProxy* p)
{
    return reinterpret_cast<WKPageRef>(p);
}

inline WebKit::WebContext* toWK(WKContextRef c)
{
    return reinterpret_cast<WebKit::WebContext*>(c);
}

inline WKContextRef toRef(WebKit::WebContext* c)
{
    return reinterpret_cast<WKContextRef>(c);
}

inline WebKit::WebPageNamespace* toWK(WKPageNamespaceRef p)
{
    return reinterpret_cast<WebKit::WebPageNamespace*>(p);
}

inline WKPageNamespaceRef toRef(WebKit::WebPageNamespace* p)
{
    return reinterpret_cast<WKPageNamespaceRef>(p);
}

inline WebKit::WebFramePolicyListenerProxy* toWK(WKFramePolicyListenerRef l)
{
    return reinterpret_cast<WebKit::WebFramePolicyListenerProxy*>(l);
}

inline WKFramePolicyListenerRef toRef(WebKit::WebFramePolicyListenerProxy* l)
{
    return reinterpret_cast<WKFramePolicyListenerRef>(l);
}

inline WebKit::WebPreferences* toWK(WKPreferencesRef p)
{
    return reinterpret_cast<WebKit::WebPreferences*>(p);
}

inline WKPreferencesRef toRef(WebKit::WebPreferences* p)
{
    return reinterpret_cast<WKPreferencesRef>(p);
}

inline WebCore::StringImpl* toWK(WKStringRef s)
{
    return reinterpret_cast<WebCore::StringImpl*>(s);
}

inline WKStringRef toRef(WebCore::StringImpl* s)
{
    return reinterpret_cast<WKStringRef>(s);
}

inline WebKit::KURLWrapper* toWK(WKURLRef u)
{
    return reinterpret_cast<WebKit::KURLWrapper*>(u);
}

inline WKURLRef toRef(WebKit::KURLWrapper* u)
{
    return reinterpret_cast<WKURLRef>(u);
}

#endif // WKAPICast_h
