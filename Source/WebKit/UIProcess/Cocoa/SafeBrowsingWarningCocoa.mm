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

// FIXME: These four functions ought to be API calls to the SafariSafeBrowsing framework when such SPI is available.
// That way WebKit does not need to know about the SafariSafeBrowsing framework's possible providers.
static const char* malwareDetailsBase(SSBServiceLookupResult *result)
{
    if ([result.provider isEqualToString:SSBProviderTencent])
        return "https://www.urlsec.qq.com/check.html?tpl=safari";
    return "https://google.com/safebrowsing/diagnostic?tpl=safari";
}

static NSURL *learnMoreURL(SSBServiceLookupResult *result)
{
    if ([result.provider isEqualToString:SSBProviderTencent])
        return [NSURL URLWithString:@"https://www.urlsec.qq.com/standard/s1.html?tpl=safari"];
    return [NSURL URLWithString:@"https://www.google.com/support/bin/answer.py?answer=106318"];
}

static const char* reportAnErrorBase(SSBServiceLookupResult *result)
{
    if ([result.provider isEqualToString:SSBProviderTencent])
        return "https://www.urlsec.qq.com/complain.html?tpl=safari";
    return "https://www.google.com/safebrowsing/report_error/?tpl=safari";
}

static String localizedProvider(SSBServiceLookupResult *result)
{
    if ([result.provider isEqualToString:SSBProviderTencent])
        return WEB_UI_NSSTRING(@"Tencent Safe Browsing", "Tencent Safe Browsing");
    return WEB_UI_NSSTRING(@"Google Safe Browsing", "Google Safe Browsing");
}


static void replace(NSMutableAttributedString *string, NSString *toReplace, NSString *replaceWith)
{
    [string replaceCharactersInRange:[string.string rangeOfString:toReplace] withString:replaceWith];
}

static void addLinkAndReplace(NSMutableAttributedString *string, NSString *toReplace, NSString *replaceWith, NSURL *linkTarget)
{
    NSMutableAttributedString *stringWithLink = [[[NSMutableAttributedString alloc] initWithString:replaceWith] autorelease];
    [stringWithLink addAttributes:@{
        NSLinkAttributeName: linkTarget,
        NSUnderlineStyleAttributeName: @1
    } range:NSMakeRange(0, replaceWith.length)];
    [string replaceCharactersInRange:[string.string rangeOfString:toReplace] withAttributedString:stringWithLink];
}

static NSURL *reportAnErrorURL(const WebCore::URL& url, SSBServiceLookupResult *result)
{
    return WebCore::URL({ }, makeString(reportAnErrorBase(result), "&url=", encodeWithURLEscapeSequences(url), "&hl=", defaultLanguage()));
}

static NSURL *malwareDetailsURL(const WebCore::URL& url, SSBServiceLookupResult *result)
{
    return WebCore::URL({ }, makeString(malwareDetailsBase(result), "&site=", url.host(), "&hl=", defaultLanguage()));
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

static NSMutableAttributedString *safeBrowsingDetailsText(const WebCore::URL& url, SSBServiceLookupResult *result)
{
    if (result.isPhishing) {
        NSString *phishingDescription = WEB_UI_NSSTRING(@"Warnings are shown for websites that have been reported as deceptive. Deceptive websites try to trick you into believing they are legitimate websites you trust.", "Phishing warning description");
        NSString *learnMore = WEB_UI_NSSTRING(@"Learn more…", "Action from safe browsing warning");
        NSString *phishingActions = WEB_UI_NSSTRING(@"If you believe this website is safe, you can %report-an-error%. Or, if you understand the risks involved, you can %bypass-link%.", "Phishing warning description");
        NSString *reportAnError = WEB_UI_NSSTRING(@"report an error", "Action from safe browsing warning");
        NSString *visitUnsafeWebsite = WEB_UI_NSSTRING(@"visit this unsafe website", "Action from safe browsing warning");

        NSMutableAttributedString *attributedString = [[[NSMutableAttributedString alloc] initWithString:[NSString stringWithFormat:@"%@ %@\n\n%@", phishingDescription, learnMore, phishingActions]] autorelease];
        addLinkAndReplace(attributedString, learnMore, learnMore, learnMoreURL(result));
        addLinkAndReplace(attributedString, @"%report-an-error%", reportAnError, reportAnErrorURL(url, result));
        addLinkAndReplace(attributedString, @"%bypass-link%", visitUnsafeWebsite, SafeBrowsingWarning::visitUnsafeWebsiteSentinel());
        return attributedString;
    }

    auto malwareOrUnwantedSoftwareDetails = [&] (NSString *description, NSString *statusStringToReplace, bool confirmMalware) {
        NSMutableAttributedString *malwareDescription = [[[NSMutableAttributedString alloc] initWithString:description] autorelease];
        replace(malwareDescription, @"%safeBrowsingProvider%", localizedProvider(result));
        NSMutableAttributedString *statusLink = [[[NSMutableAttributedString alloc] initWithString:WEB_UI_NSSTRING(@"the status of “%site%”", "Part of malware description")] autorelease];
        replace(statusLink, @"%site%", url.host().toString());
        addLinkAndReplace(malwareDescription, statusStringToReplace, statusLink.string, malwareDetailsURL(url, result));

        NSMutableAttributedString *ifYouUnderstand = [[[NSMutableAttributedString alloc] initWithString:WEB_UI_NSSTRING(@"If you understand the risks involved, you can %visit-this-unsafe-site-link%.", "Action from safe browsing warning")] autorelease];
        addLinkAndReplace(ifYouUnderstand, @"%visit-this-unsafe-site-link%", WEB_UI_NSSTRING(@"visit this unsafe website", "Action from safe browsing warning"), confirmMalware ? SafeBrowsingWarning::confirmMalwareSentinel() : SafeBrowsingWarning::visitUnsafeWebsiteSentinel());

        [malwareDescription appendAttributedString:[[[NSMutableAttributedString alloc] initWithString:@"\n\n"] autorelease]];
        [malwareDescription appendAttributedString:ifYouUnderstand];
        return malwareDescription;
    };

    if (result.isMalware)
        return malwareOrUnwantedSoftwareDetails(WEB_UI_NSSTRING(@"Warnings are shown for websites where malicious software has been detected. You can check the %status-link% on the %safeBrowsingProvider% diagnostic page.", "Malware warning description"), @"%status-link%", true);
    ASSERT(result.isUnwantedSoftware);
    return malwareOrUnwantedSoftwareDetails(WEB_UI_NSSTRING(@"Warnings are shown for websites where harmful software has been detected. You can check %the-status-of-site% on the %safeBrowsingProvider% diagnostic page.", "Unwanted software warning description"), @"%the-status-of-site%", false);
}

SafeBrowsingWarning::SafeBrowsingWarning(const WebCore::URL& url, SSBServiceLookupResult *result)
    : m_title(safeBrowsingTitleText(result))
    , m_warning(safeBrowsingWarningText(result))
    , m_details(safeBrowsingDetailsText(url, result))
{
}
#endif

SafeBrowsingWarning::SafeBrowsingWarning(String&& title, String&& warning, RetainPtr<NSAttributedString>&& details)
    : m_title(WTFMove(title))
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
