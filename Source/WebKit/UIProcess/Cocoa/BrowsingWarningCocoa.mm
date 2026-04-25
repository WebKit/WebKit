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
#import "BrowsingWarning.h"

#import "SafeBrowsingSPI.h"
#import <WebCore/LocalizedStrings.h>
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <wtf/Language.h>
#import <wtf/StdLibExtras.h>
#import <wtf/text/MakeString.h>

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/BrowsingWarningAdditions.mm>)
#import <WebKitAdditions/BrowsingWarningAdditions.mm>
#endif

#if !defined(BROWSING_WARNING_PROVIDER_ADDITIONS)
#define BROWSING_WARNING_PROVIDER_ADDITIONS
#endif

#if !defined(BROWSING_WARNING_PROVIDER_SHORT_NAME_ADDITIONS)
#define BROWSING_WARNING_PROVIDER_SHORT_NAME_ADDITIONS
#endif

#if !defined(BROWSING_WARNING_DETAILS_TEXT_ADDITIONS)
#define BROWSING_WARNING_DETAILS_TEXT_ADDITIONS
#endif

NSString * const SSBProviderGoogle = @"SSBProviderGoogle";
NSString * const SSBProviderTencent = @"SSBProviderTencent";
NSString * const SSBProviderApple = @"SSBProviderApple";
BROWSING_WARNING_PROVIDER_ADDITIONS

namespace WebKit {

#if HAVE(SAFE_BROWSING)

static String malwareDetailsBase(SSBServiceLookupResult *result)
{
    return result.malwareDetailsBaseURLString;
}

static NSURL *learnMoreURL(SSBServiceLookupResult *result)
{
    return result.learnMoreURL;
}

static String reportAnErrorBase(SSBServiceLookupResult *result)
{
    return result.reportAnErrorBaseURLString;
}

static String localizedProviderDisplayName(SSBServiceLookupResult *result)
{
    return result.localizedProviderDisplayName;
}

static String localizedProviderShortName(SSBServiceLookupResult *result)
{
    if ([result respondsToSelector:@selector(localizedProviderShortName)])
        return result.localizedProviderShortName;
    if ([result.provider isEqual:SSBProviderGoogle])
        return "Google"_s;
    if ([result.provider isEqual:SSBProviderTencent])
        return "Tencent"_s;
    if ([result.provider isEqual:SSBProviderApple])
        return "Apple"_s;
    BROWSING_WARNING_PROVIDER_SHORT_NAME_ADDITIONS
    ASSERT_NOT_REACHED();
    return ""_s;
}

static void replace(NSMutableAttributedString *string, NSString *toReplace, NSString *replaceWith)
{
    [string replaceCharactersInRange:[retainPtr(string.string) rangeOfString:toReplace] withString:replaceWith];
}

static void addLinkAndReplace(NSMutableAttributedString *string, NSString *toReplace, NSString *replaceWith, NSURL *linkTarget)
{
    auto stringWithLink = adoptNS([[NSMutableAttributedString alloc] initWithString:replaceWith]);
    [stringWithLink addAttributes:@{
        NSLinkAttributeName: linkTarget,
        NSUnderlineStyleAttributeName: @1
    } range:NSMakeRange(0, replaceWith.length)];
    [string replaceCharactersInRange:[retainPtr(string.string) rangeOfString:toReplace] withAttributedString:stringWithLink.get()];
}

static RetainPtr<NSURL> reportAnErrorURL(const URL& url, SSBServiceLookupResult *result)
{
    return URL({ }, makeString(reportAnErrorBase(result), "&url="_s, encodeWithURLEscapeSequences(url.string()), "&hl="_s, defaultLanguage())).createNSURL();
}

static RetainPtr<NSURL> malwareDetailsURL(const URL& url, SSBServiceLookupResult *result)
{
    return URL({ }, makeString(malwareDetailsBase(result), "&site="_s, url.host(), "&hl="_s, defaultLanguage())).createNSURL();
}

static NSString *browsingWarningTitleText(SSBServiceLookupResult *result)
{
    if (result.isPhishing)
        return WEB_UI_NSSTRING(@"Deceptive Website Warning", "Phishing warning title");
    if (result.isMalware)
        return WEB_UI_NSSTRING(@"Malware Website Warning", "Malware warning title");
    ASSERT(result.isUnwantedSoftware);
    return WEB_UI_NSSTRING(@"Website With Harmful Software Warning", "Unwanted software warning title");
}

static NSString *browsingWarningTitleText(BrowsingWarning::Data data)
{
    return WTF::switchOn(data, [&] (BrowsingWarning::SafeBrowsingWarningData data) {
        return browsingWarningTitleText(data.result.get());
    }, [&] (BrowsingWarning::HTTPSNavigationFailureData) {
        return WEB_UI_NSSTRING(@"This Connection Is Not Secure", "Not Secure Connection warning page title");
    });
}

static NSString *browsingWarningText(SSBServiceLookupResult *result)
{
    if (result.isPhishing)
        return WEB_UI_NSSTRING(@"This website may try to trick you into doing something dangerous, like installing software or disclosing personal or financial information, like passwords, phone numbers, or credit cards.", "Phishing warning");
    if (result.isMalware)
        return WEB_UI_NSSTRING(@"This website may attempt to install dangerous software, which could harm your computer or steal your personal or financial information, like passwords, photos, or credit cards.", "Malware warning");

    ASSERT(result.isUnwantedSoftware);
    return WEB_UI_NSSTRING(@"This website may try to trick you into installing software that harms your browsing experience, like changing your settings without your permission or showing you unwanted ads. Once installed, it may be difficult to remove.", "Unwanted software warning");
}

static NSString *browsingWarningText(BrowsingWarning::Data data)
{
    return WTF::switchOn(data, [&] (BrowsingWarning::SafeBrowsingWarningData data) {
        return browsingWarningText(data.result.get());
    }, [&] (BrowsingWarning::HTTPSNavigationFailureData) {
        return WEB_UI_NSSTRING(@"This website does not support connecting securely over HTTPS. The information you see and enter on this website, including credit cards, phone numbers, and passwords, can be read and altered by other people.", "Not Secure Connection warning text");
    });
}

static NSMutableAttributedString *browsingDetailsText(const URL& url, SSBServiceLookupResult *result)
{
    BROWSING_WARNING_DETAILS_TEXT_ADDITIONS

    if (result.isPhishing) {
        RetainPtr phishingDescription = WEB_UI_NSSTRING(@"Warnings are shown for websites that have been reported as deceptive. Deceptive websites try to trick you into believing they are legitimate websites you trust.", "Phishing warning description");
        RetainPtr learnMore = WEB_UI_NSSTRING(@"Learn more…", "Action from safe browsing warning");
        RetainPtr phishingActions = WEB_UI_NSSTRING(@"This website was reported as deceptive by %provider-display-name%. If you believe this website is safe, you can %report-an-error% to %provider%. Or, if you understand the risks involved, you can %bypass-link%.", "Phishing warning description");
        RetainPtr reportAnError = WEB_UI_NSSTRING(@"report an error", "Action from safe browsing warning");
        RetainPtr visitUnsafeWebsite = WEB_UI_NSSTRING(@"visit this unsafe website", "Action from safe browsing warning");

        RetainPtr attributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:adoptNS([[NSString alloc] initWithFormat:@"%@ %@\n\n%@", phishingDescription.get(), learnMore.get(), phishingActions.get()]).get()]);
        addLinkAndReplace(attributedString.get(), learnMore.get(), learnMore.get(), RetainPtr { learnMoreURL(result) }.get());
        replace(attributedString.get(), @"%provider-display-name%", localizedProviderDisplayName(result).createNSString().get());
        replace(attributedString.get(), @"%provider%", localizedProviderShortName(result).createNSString().get());
        addLinkAndReplace(attributedString.get(), @"%report-an-error%", reportAnError.get(), reportAnErrorURL(url, result).get());
        addLinkAndReplace(attributedString.get(), @"%bypass-link%", visitUnsafeWebsite.get(), BrowsingWarning::visitUnsafeWebsiteSentinel().get());
        return attributedString.autorelease();
    }

    auto malwareOrUnwantedSoftwareDetails = [&] (NSString *description, NSString *statusStringToReplace, bool confirmMalware) {
        auto malwareDescription = adoptNS([[NSMutableAttributedString alloc] initWithString:description]);
        replace(malwareDescription.get(), @"%safeBrowsingProvider%", localizedProviderDisplayName(result).createNSString().get());
        auto statusLink = adoptNS([[NSMutableAttributedString alloc] initWithString:RetainPtr { WEB_UI_NSSTRING(@"the status of “%site%”", "Part of malware description") }.get()]);
        replace(statusLink.get(), @"%site%", url.host().createNSString().get());
        addLinkAndReplace(malwareDescription.get(), statusStringToReplace, retainPtr([statusLink string]).get(), malwareDetailsURL(url, result).get());

        auto ifYouUnderstand = adoptNS([[NSMutableAttributedString alloc] initWithString:RetainPtr { WEB_UI_NSSTRING(@"If you understand the risks involved, you can %visit-this-unsafe-site-link%.", "Action from safe browsing warning") }.get()]);
        addLinkAndReplace(ifYouUnderstand.get(), @"%visit-this-unsafe-site-link%", RetainPtr { WEB_UI_NSSTRING(@"visit this unsafe website", "Action from safe browsing warning") }.get(), confirmMalware ? BrowsingWarning::confirmMalwareSentinel().get() : BrowsingWarning::visitUnsafeWebsiteSentinel().get());

        [malwareDescription appendAttributedString:adoptNS([[NSMutableAttributedString alloc] initWithString:@"\n\n"]).get()];
        [malwareDescription appendAttributedString:ifYouUnderstand.get()];
        return malwareDescription.autorelease();
    };

    if (result.isMalware)
        return malwareOrUnwantedSoftwareDetails(RetainPtr { WEB_UI_NSSTRING(@"Warnings are shown for websites where malicious software has been detected. You can check the %status-link% on the %safeBrowsingProvider% diagnostic page.", "Malware warning description") }.get(), @"%status-link%", true);
    ASSERT(result.isUnwantedSoftware);
    return malwareOrUnwantedSoftwareDetails(RetainPtr { WEB_UI_NSSTRING(@"Warnings are shown for websites where harmful software has been detected. You can check %the-status-of-site% on the %safeBrowsingProvider% diagnostic page.", "Unwanted software warning description") }.get(), @"%the-status-of-site%", false);
}

static NSMutableAttributedString *browsingDetailsText(const URL& url, BrowsingWarning::Data data)
{
    if (auto* safeBrowsingData = std::get_if<BrowsingWarning::SafeBrowsingWarningData>(&data))
        return browsingDetailsText(url, safeBrowsingData->result.get());
    return nil;
}

BrowsingWarning::BrowsingWarning(const URL& url, bool forMainFrameNavigation, Data&& data)
    : m_url(url)
    , m_title(browsingWarningTitleText(data))
    , m_warning(browsingWarningText(data))
    , m_forMainFrameNavigation(forMainFrameNavigation)
    , m_details(browsingDetailsText(url, data))
    , m_data(WTF::move(data))
{
}
#endif

BrowsingWarning::BrowsingWarning(URL&& url, String&& title, String&& warning, RetainPtr<NSAttributedString>&& details, Data&& data)
    : m_url(WTF::move(url))
    , m_title(WTF::move(title))
    , m_warning(WTF::move(warning))
    , m_details(WTF::move(details))
    , m_data(WTF::move(data))
{
}

RetainPtr<NSURL> BrowsingWarning::visitUnsafeWebsiteSentinel()
{
    return adoptNS([[NSURL alloc] initWithString:@"WKVisitUnsafeWebsiteSentinel"]);
}

RetainPtr<NSURL> BrowsingWarning::confirmMalwareSentinel()
{
    return adoptNS([[NSURL alloc] initWithString:@"WKConfirmMalwareSentinel"]);
}

}
