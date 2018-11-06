/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include <WebCore/URL.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS SSBServiceLookupResult;

namespace WebKit {

class SafeBrowsingResult : public RefCounted<SafeBrowsingResult> {
public:
#if HAVE(SAFE_BROWSING)
    static Ref<SafeBrowsingResult> create(WebCore::URL&& url, SSBServiceLookupResult *result)
    {
        return adoptRef(*new SafeBrowsingResult(WTFMove(url), result));
    }
#endif

    bool isPhishing() const { return m_isPhishing; }
    bool isMalware() const { return m_isMalware; }
    bool isUnwantedSoftware() const { return m_isUnwantedSoftware; }

    const WebCore::URL& url() const { return m_url; }
    const String& provider() const { return m_provider; }
    const String& localizedProviderName() const { return m_localizedProviderName; }
    const String& malwareDetailsURLBase() const { return m_malwareDetailsURLBase; }
    const String& reportAnErrorURLBase() const { return m_reportAnErrorURLBase; }
    const WebCore::URL& learnMoreURL() const { return m_learnMoreURL; }

    bool needsSafeBrowsingWarning() const { return m_isPhishing || m_isMalware || m_isUnwantedSoftware; }

private:
#if HAVE(SAFE_BROWSING)
    SafeBrowsingResult(WebCore::URL&&, SSBServiceLookupResult *);
#endif
    WebCore::URL m_url;
    bool m_isPhishing { false };
    bool m_isMalware { false };
    bool m_isUnwantedSoftware { false };

    String m_provider;
    String m_localizedProviderName;
    String m_malwareDetailsURLBase;
    String m_reportAnErrorURLBase;
    WebCore::URL m_learnMoreURL;
};

} // namespace WebKit
