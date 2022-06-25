/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#ifndef WKAPICastGtk_h
#define WKAPICastGtk_h

#ifndef WKAPICast_h
#error "Please #include \"WKAPICast.h\" instead of this file directly."
#endif

#include "WebGrammarDetail.h"

typedef struct _WebKitWebViewBase WebKitWebViewBase;

namespace WebKit {

WK_ADD_API_MAPPING(WKGrammarDetailRef, WebGrammarDetail)
WK_ADD_API_MAPPING(WKViewRef, WebKitWebViewBase)

inline ProxyingRefPtr<WebGrammarDetail> toAPI(const WebCore::GrammarDetail& grammarDetail)
{
    return ProxyingRefPtr<WebGrammarDetail>(WebGrammarDetail::create(grammarDetail));
}

template<>
inline WKViewRef toAPI<>(WebKitWebViewBase* view)
{
    return reinterpret_cast<WKViewRef>(static_cast<void*>(view));
}

template<>
inline WebKitWebViewBase* toImpl<>(WKViewRef view)
{
    return static_cast<WebKitWebViewBase*>(static_cast<void*>(const_cast<typename std::remove_const<typename std::remove_pointer<WKViewRef>::type>::type*>(view)));
}

}

#endif // WKAPICastGtk_h
