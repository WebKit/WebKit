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

#import "config.h"
#import "SafeBrowsingResult.h"

#import "SafeBrowsingSPI.h"
#import <WebCore/LocalizedStrings.h>

namespace WebKit {

#if HAVE(SAFE_BROWSING)

// FIXME: These ought to be API calls to the SafariSafeBrowsing framework when such SPI is available.
static const char* malwareDetailsBase(const String& provider)
{
    if (provider == String(SSBProviderTencent))
        return "https://www.urlsec.qq.com/check.html?tpl=safari";
    return "https://google.com/safebrowsing/diagnostic?tpl=safari";
}

static WebCore::URL learnMore(const String& provider)
{
    if (provider == String(SSBProviderTencent))
        return {{ }, "https://www.urlsec.qq.com/standard/s1.html?tpl=safari"};
    return {{ }, "https://www.google.com/support/bin/answer.py?answer=106318"};
}

static const char* reportAnErrorBase(const String& provider)
{
    if (provider == String(SSBProviderTencent))
        return "https://www.urlsec.qq.com/complain.html?tpl=safari";
    return "https://www.google.com/safebrowsing/report_error/?tpl=safari";
}

static String localizedProvider(const String& provider)
{
    if (provider == String(SSBProviderTencent))
        return WEB_UI_NSSTRING(@"Tencent Safe Browsing", "Tencent Safe Browsing");
    return WEB_UI_NSSTRING(@"Google Safe Browsing", "Google Safe Browsing");
}

SafeBrowsingResult::SafeBrowsingResult(WebCore::URL&& url, SSBServiceLookupResult *result)
    : m_url(WTFMove(url))
    , m_isPhishing([result isPhishing])
    , m_isMalware([result isMalware])
    , m_isUnwantedSoftware([result isUnwantedSoftware])
    , m_provider([result provider])
    , m_localizedProviderName(localizedProvider([result provider]))
    , m_malwareDetailsURLBase(malwareDetailsBase([result provider]))
    , m_reportAnErrorURLBase(reportAnErrorBase([result provider]))
    , m_learnMoreURL(learnMore([result provider]))
{
}
#endif

}
