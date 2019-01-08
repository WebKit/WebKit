/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/CookiesStrategy.h>
#include <WebCore/LoaderStrategy.h>
#include <WebCore/PlatformStrategies.h>
#include <wtf/Forward.h>

class WebPlatformStrategies : public WebCore::PlatformStrategies, private WebCore::CookiesStrategy {
public:
    static void initialize();
    
private:
    friend NeverDestroyed<WebPlatformStrategies>;
    WebPlatformStrategies();

    // WebCore::PlatformStrategies
    virtual WebCore::CookiesStrategy* createCookiesStrategy();
    virtual WebCore::LoaderStrategy* createLoaderStrategy();
    virtual WebCore::PasteboardStrategy* createPasteboardStrategy();
    virtual WebCore::BlobRegistry* createBlobRegistry();

    // WebCore::CookiesStrategy
    std::pair<String, bool> cookiesForDOM(const PAL::SessionID&, const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, Optional<uint64_t> frameID, Optional<uint64_t> pageID, WebCore::IncludeSecureCookies) override;
    virtual void setCookiesFromDOM(const PAL::SessionID&, const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, Optional<uint64_t> frameID, Optional<uint64_t> pageID, const String&);
    virtual bool cookiesEnabled(const PAL::SessionID&);
    std::pair<String, bool> cookieRequestHeaderFieldValue(const PAL::SessionID&, const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, Optional<uint64_t> frameID, Optional<uint64_t> pageID, WebCore::IncludeSecureCookies) override;
    virtual bool getRawCookies(const PAL::SessionID&, const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, Optional<uint64_t> frameID, Optional<uint64_t> pageID, Vector<WebCore::Cookie>&);
    virtual void deleteCookie(const PAL::SessionID&, const URL&, const String&);
};
