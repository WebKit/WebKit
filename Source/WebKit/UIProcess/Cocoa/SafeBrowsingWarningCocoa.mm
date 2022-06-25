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
#import "SafeBrowsingWarning.h"

#import "SafeBrowsingSPI.h"
#import <WebCore/LocalizedStrings.h>
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <wtf/Language.h>

namespace WebKit {

#if HAVE(SAFE_BROWSING)

static String malwareDetailsBase(SSBServiceLookupResult *result)
{
#if HAVE(SAFE_BROWSING_RESULT_DETAILS)
    return result.malwareDetailsBaseURLString;
#else
    if ([result.provider isEqualToString:SSBProviderTencent])
        return "https://www.urlsec.qq.com/check.html?tpl=safari"_s;
    return "https://google.com/safebrowsing/diagnostic?tpl=safari"_s;
#endif
}

static NSURL *learnMoreURL(SSBServiceLookupResult *result)
{
#if HAVE(SAFE_BROWSING_RESULT_DETAILS)
    return result.learnMoreURL;
#else
    if ([result.provider isEqualToString:SSBProviderTencent])
        return [NSURL URLWithString:@"https://www.urlsec.qq.com/standard/s1.html?tpl=safari"];
    return [NSURL URLWithString:@"https://www.google.com/support/bin/answer.py?answer=106318"];
#endif
}

static String reportAnErrorBase(SSBServiceLookupResult *result)
{
#if HAVE(SAFE_BROWSING_RESULT_DETAILS)
    return result.reportAnErrorBaseURLString;
#else
    if ([result.provider isEqualToString:SSBProviderTencent])
        return "https://www.urlsec.qq.com/complain.html?tpl=safari"_s;
    return "https://www.google.com/safebrowsing/report_error/?tpl=safari"_s;
#endif
}

static String localizedProvider(SSBServiceLookupResult *result)
{
#if HAVE(SAFE_BROWSING_RESULT_DETAILS)
    return result.localizedProviderDisplayName;
#else
    if ([result.provider isEqualToString:SSBProviderTencent])
        return WEB_UI_NSSTRING(@"Tencent Safe Browsing", "Tencent Safe Browsing");
    return WEB_UI_NSSTRING(@"Google Safe Browsing", "Google Safe Browsing");
#endif
}


static void replace(NSMutableAttributedString *string, NSString *toReplace, NSString *replaceWith)
{
    [string replaceCharactersInRange:[string.string rangeOfString:toReplace] withString:replaceWith];
}

static void addLinkAndReplace(NSMutableAttributedString *string, NSString *toReplace, NSString *replaceWith, NSURL *linkTarget)
{
    auto stringWithLink = adoptNS([[NSMutableAttributedString alloc] initWithString:replaceWith]);
    [stringWithLink addAttributes:@{
        NSLinkAttributeName: linkTarget,
        NSUnderlineStyleAttributeName: @1
    } range:NSMakeRange(0, replaceWith.length)];
    [string replaceCharactersInRange:[string.string rangeOfString:toReplace] withAttributedString:stringWithLink.get()];
}

static NSURL *reportAnErrorURL(const URL& url, SSBServiceLookupResult *result)
{
    return URL({ }, makeString(reportAnErrorBase(result), "&url=", encodeWithURLEscapeSequences(url.string()), "&hl=", defaultLanguage()));
}

static NSURL *malwareDetailsURL(const URL& url, SSBServiceLookupResult *result)
{
    return URL({ }, makeString(malwareDetailsBase(result), "&site=", url.host(), "&hl=", defaultLanguage()));
}

static NSString *safeBrowsingTitleText(SSBServiceLookupResult *result)
{
    if (result.isPhishing)
        return WEB_UI_NSSTRING(@"Deceptive Website Warning", "Phishing warning title");
    if (result.isMalware)
        return WEB_UI_NSSTRING(@"Malware Website Warning", "Malware warning title");
    ASSERT(result.isUnwantedSoftware);
    return WEB_UI_NSSTRING(@"Website With Harmful Software Warning", "Unwanted software warning title");
}

static NSString *safeBrowsingWarningText(SSBServiceLookupResult *result)
{
    if (result.isPhishing)
        return WEB_UI_NSSTRING(@"This website may try to trick you into doing something dangerous, like installing software or disclosing personal or financial information, like passwords, phone numbers, or credit cards.", "Phishing warning");
    if (result.isMalware)
        return WEB_UI_NSSTRING(@"This website may attempt to install dangerous software, which could harm your computer or steal your personal or financial information, like passwords, photos, or credit cards.", "Malware warning");

    ASSERT(result.isUnwantedSoftware);
    return WEB_UI_NSSTRING(@"This website may try to trick you into installing software that harms your browsing experience, like changing your settings without your permission or showing you unwanted ads. Once installed, it may be difficult to remove.", "Unwanted software warning");
}

static NSMutableAttributedString *safeBrowsingDetailsText(const URL& url, SSBServiceLookupResult *result)
{
    if (result.isPhishing) {
        NSString *phishingDescription = WEB_UI_NSSTRING(@"Warnings are shown for websites that have been reported as deceptive. Deceptive websites try to trick you into believing they are legitimate websites you trust.", "Phishing warning description");
        NSString *learnMore = WEB_UI_NSSTRING(@"Learn more…", "Action from safe browsing warning");
        NSString *phishingActions = WEB_UI_NSSTRING(@"If you believe this website is safe, you can %report-an-error%. Or, if you understand the risks involved, you can %bypass-link%.", "Phishing warning description");
        NSString *reportAnError = WEB_UI_NSSTRING(@"report an error", "Action from safe browsing warning");
        NSString *visitUnsafeWebsite = WEB_UI_NSSTRING(@"visit this unsafe website", "Action from safe browsing warning");

        auto attributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:[NSString stringWithFormat:@"%@ %@\n\n%@", phishingDescription, learnMore, phishingActions]]);
        addLinkAndReplace(attributedString.get(), learnMore, learnMore, learnMoreURL(result));
        addLinkAndReplace(attributedString.get(), @"%report-an-error%", reportAnError, reportAnErrorURL(url, result));
        addLinkAndReplace(attributedString.get(), @"%bypass-link%", visitUnsafeWebsite, SafeBrowsingWarning::visitUnsafeWebsiteSentinel());
        return attributedString.autorelease();
    }

    auto malwareOrUnwantedSoftwareDetails = [&] (NSString *description, NSString *statusStringToReplace, bool confirmMalware) {
        auto malwareDescription = adoptNS([[NSMutableAttributedString alloc] initWithString:description]);
        replace(malwareDescription.get(), @"%safeBrowsingProvider%", localizedProvider(result));
        auto statusLink = adoptNS([[NSMutableAttributedString alloc] initWithString:WEB_UI_NSSTRING(@"the status of “%site%”", "Part of malware description")]);
        replace(statusLink.get(), @"%site%", url.host().toString());
        addLinkAndReplace(malwareDescription.get(), statusStringToReplace, [statusLink string], malwareDetailsURL(url, result));

        auto ifYouUnderstand = adoptNS([[NSMutableAttributedString alloc] initWithString:WEB_UI_NSSTRING(@"If you understand the risks involved, you can %visit-this-unsafe-site-link%.", "Action from safe browsing warning")]);
        addLinkAndReplace(ifYouUnderstand.get(), @"%visit-this-unsafe-site-link%", WEB_UI_NSSTRING(@"visit this unsafe website", "Action from safe browsing warning"), confirmMalware ? SafeBrowsingWarning::confirmMalwareSentinel() : SafeBrowsingWarning::visitUnsafeWebsiteSentinel());

        [malwareDescription appendAttributedString:adoptNS([[NSMutableAttributedString alloc] initWithString:@"\n\n"]).get()];
        [malwareDescription appendAttributedString:ifYouUnderstand.get()];
        return malwareDescription.autorelease();
    };

    if (result.isMalware)
        return malwareOrUnwantedSoftwareDetails(WEB_UI_NSSTRING(@"Warnings are shown for websites where malicious software has been detected. You can check the %status-link% on the %safeBrowsingProvider% diagnostic page.", "Malware warning description"), @"%status-link%", true);
    ASSERT(result.isUnwantedSoftware);
    return malwareOrUnwantedSoftwareDetails(WEB_UI_NSSTRING(@"Warnings are shown for websites where harmful software has been detected. You can check %the-status-of-site% on the %safeBrowsingProvider% diagnostic page.", "Unwanted software warning description"), @"%the-status-of-site%", false);
}

SafeBrowsingWarning::SafeBrowsingWarning(const URL& url, bool forMainFrameNavigation, SSBServiceLookupResult *result)
    : m_url(url)
    , m_title(safeBrowsingTitleText(result))
    , m_warning(safeBrowsingWarningText(result))
    , m_forMainFrameNavigation(forMainFrameNavigation)
    , m_details(safeBrowsingDetailsText(url, result))
{
}
#endif

SafeBrowsingWarning::SafeBrowsingWarning(URL&& url, String&& title, String&& warning, RetainPtr<NSAttributedString>&& details)
    : m_url(WTFMove(url))
    , m_title(WTFMove(title))
    , m_warning(WTFMove(warning))
    , m_details(WTFMove(details))
{
}

NSURL *SafeBrowsingWarning::visitUnsafeWebsiteSentinel()
{
    return [NSURL URLWithString:@"WKVisitUnsafeWebsiteSentinel"];
}

NSURL *SafeBrowsingWarning::confirmMalwareSentinel()
{
    return [NSURL URLWithString:@"WKConfirmMalwareSentinel"];
}

}
