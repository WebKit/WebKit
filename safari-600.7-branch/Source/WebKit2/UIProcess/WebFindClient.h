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

#ifndef WebFindClient_h
#define WebFindClient_h

#include "APIClient.h"
#include "WKPage.h"
#include <wtf/Forward.h>

namespace API {
class Array;

template<> struct ClientTraits<WKPageFindClientBase> {
    typedef std::tuple<WKPageFindClientV0> Versions;
};

template<> struct ClientTraits<WKPageFindMatchesClientBase> {
    typedef std::tuple<WKPageFindMatchesClientV0> Versions;
};
}

namespace WebKit {

class WebPageProxy;
class WebImage;

class WebFindClient : public API::Client<WKPageFindClientBase> {
public:
    void didFindString(WebPageProxy*, const String&, uint32_t matchCount, int32_t matchIndex);
    void didFailToFindString(WebPageProxy*, const String&);
    void didCountStringMatches(WebPageProxy*, const String&, uint32_t matchCount);
};

class WebFindMatchesClient : public API::Client<WKPageFindMatchesClientBase> {
public:
    void didFindStringMatches(WebPageProxy*, const String&, API::Array*, int);
    void didGetImageForMatchResult(WebPageProxy*, WebImage*, uint32_t);
};

} // namespace WebKit

#endif // WebFindClient_h
