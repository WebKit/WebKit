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
class WebNavigationData;
class WebPageNamespace;
class WebPageProxy;
class WebPreferences;

template<typename APIType> struct APITypeInfo { };
template<> struct APITypeInfo<WKFrameRef>                       { typedef WebFrameProxy* ImplType; };
template<> struct APITypeInfo<WKPageRef>                        { typedef WebPageProxy* ImplType; };
template<> struct APITypeInfo<WKContextRef>                     { typedef WebContext* ImplType; };
template<> struct APITypeInfo<WKPageNamespaceRef>               { typedef WebPageNamespace* ImplType; };
template<> struct APITypeInfo<WKFramePolicyListenerRef>         { typedef WebFramePolicyListenerProxy* ImplType; };
template<> struct APITypeInfo<WKPreferencesRef>                 { typedef WebPreferences* ImplType; };
template<> struct APITypeInfo<WKStringRef>                      { typedef WebCore::StringImpl* ImplType; };
template<> struct APITypeInfo<WKURLRef>                         { typedef WebCore::StringImpl* ImplType; };
template<> struct APITypeInfo<WKNavigationDataRef>              { typedef WebNavigationData* ImplType; };

template<typename ImplType> struct ImplTypeInfo { };
template<> struct ImplTypeInfo<WebFrameProxy*>                  { typedef WKFrameRef APIType; };
template<> struct ImplTypeInfo<WebPageProxy*>                   { typedef WKPageRef APIType; };
template<> struct ImplTypeInfo<WebContext*>                     { typedef WKContextRef APIType; };
template<> struct ImplTypeInfo<WebPageNamespace*>               { typedef WKPageNamespaceRef APIType; };
template<> struct ImplTypeInfo<WebFramePolicyListenerProxy*>    { typedef WKFramePolicyListenerRef APIType; };
template<> struct ImplTypeInfo<WebPreferences*>                 { typedef WKPreferencesRef APIType; };
template<> struct ImplTypeInfo<WebCore::StringImpl*>            { typedef WKStringRef APIType; };
template<> struct ImplTypeInfo<WebNavigationData*>              { typedef WKNavigationDataRef APIType; };

} // namespace WebKit

/* Opaque typing convenience methods */

template<typename T>
inline typename WebKit::APITypeInfo<T>::ImplType toWK(T t)
{
    return reinterpret_cast<typename WebKit::APITypeInfo<T>::ImplType>(t);
}

template<typename T>
inline typename WebKit::ImplTypeInfo<T>::APIType toRef(T t)
{
    return reinterpret_cast<typename WebKit::ImplTypeInfo<T>::APIType>(t);
}

/* Special case for WKURLRef which also uses StringImpl as an implementation. */

inline WKURLRef toURLRef(WebCore::StringImpl* s)
{
    return reinterpret_cast<WKURLRef>(s);
}

#endif // WKAPICast_h
