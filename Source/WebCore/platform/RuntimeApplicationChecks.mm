/*
 * Copyright (C) 2011, 2016 Apple Inc. All rights reserved.
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
#import "RuntimeApplicationChecks.h"

#import <Foundation/NSBundle.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>
#import <wtf/text/WTFString.h>

namespace WebCore {

// The application bundle identifier gets set to the UIProcess bundle identifier by the WebProcess and
// the networking process upon initialization.
// If not explicitly set, this will return the main bundle identifier which is accurate for WK1 or
// the WK2 UIProcess.
static String& applicationBundleIdentifier()
{
    ASSERT(RunLoop::isMain());

    static NeverDestroyed<String> identifier([[NSBundle mainBundle] bundleIdentifier]);

    return identifier;
}

void setApplicationBundleIdentifier(const String& bundleIdentifier)
{
    applicationBundleIdentifier() = bundleIdentifier;
}

static bool applicationBundleIsEqualTo(const String& bundleIdentifierString)
{
    return applicationBundleIdentifier() == bundleIdentifierString;
}

#if PLATFORM(MAC)

bool MacApplication::isSafari()
{
    static bool isSafari = applicationBundleIsEqualTo("com.apple.Safari");
    ASSERT_WITH_MESSAGE(isSafari == applicationBundleIsEqualTo("com.apple.Safari"), "Should not be called before setApplicationBundleIdentifier()");
    return isSafari;
}

bool MacApplication::isAppleMail()
{
    static bool isAppleMail = applicationBundleIsEqualTo("com.apple.mail");
    ASSERT_WITH_MESSAGE(isAppleMail == applicationBundleIsEqualTo("com.apple.mail"), "Should not be called before setApplicationBundleIdentifier()");
    return isAppleMail;
}

bool MacApplication::isIBooks()
{
    static bool isIBooks = applicationBundleIsEqualTo("com.apple.iBooksX");
    ASSERT_WITH_MESSAGE(isIBooks == applicationBundleIsEqualTo("com.apple.iBooksX"), "Should not be called before setApplicationBundleIdentifier()");
    return isIBooks;
}

bool MacApplication::isITunes()
{
    static bool isITunes = applicationBundleIsEqualTo("com.apple.iTunes");
    ASSERT_WITH_MESSAGE(isITunes == applicationBundleIsEqualTo("com.apple.iTunes"), "Should not be called before setApplicationBundleIdentifier()");
    return isITunes;
}

bool MacApplication::isMicrosoftMessenger()
{
    static bool isMicrosoftMessenger = applicationBundleIsEqualTo("com.microsoft.Messenger");
    ASSERT_WITH_MESSAGE(isMicrosoftMessenger == applicationBundleIsEqualTo("com.microsoft.Messenger"), "Should not be called before setApplicationBundleIdentifier()");
    return isMicrosoftMessenger;
}

bool MacApplication::isAdobeInstaller()
{
    static bool isAdobeInstaller = applicationBundleIsEqualTo("com.adobe.Installers.Setup");
    ASSERT_WITH_MESSAGE(isAdobeInstaller == applicationBundleIsEqualTo("com.adobe.Installers.Setup"), "Should not be called before setApplicationBundleIdentifier()");
    return isAdobeInstaller;
}

bool MacApplication::isAOLInstantMessenger()
{
    static bool isAOLInstantMessenger = applicationBundleIsEqualTo("com.aol.aim.desktop");
    ASSERT_WITH_MESSAGE(isAOLInstantMessenger == applicationBundleIsEqualTo("com.aol.aim.desktop"), "Should not be called before setApplicationBundleIdentifier()");
    return isAOLInstantMessenger;
}

bool MacApplication::isMicrosoftMyDay()
{
    static bool isMicrosoftMyDay = applicationBundleIsEqualTo("com.microsoft.myday");
    ASSERT_WITH_MESSAGE(isMicrosoftMyDay == applicationBundleIsEqualTo("com.microsoft.myday"), "Should not be called before setApplicationBundleIdentifier()");
    return isMicrosoftMyDay;
}

bool MacApplication::isMicrosoftOutlook()
{
    static bool isMicrosoftOutlook = applicationBundleIsEqualTo("com.microsoft.Outlook");
    ASSERT_WITH_MESSAGE(isMicrosoftOutlook == applicationBundleIsEqualTo("com.microsoft.Outlook"), "Should not be called before setApplicationBundleIdentifier()");
    return isMicrosoftOutlook;
}

bool MacApplication::isQuickenEssentials()
{
    static bool isQuickenEssentials = applicationBundleIsEqualTo("com.intuit.QuickenEssentials");
    ASSERT_WITH_MESSAGE(isQuickenEssentials == applicationBundleIsEqualTo("com.intuit.QuickenEssentials"), "Should not be called before setApplicationBundleIdentifier()");
    return isQuickenEssentials;
}

bool MacApplication::isAperture()
{
    static bool isAperture = applicationBundleIsEqualTo("com.apple.Aperture");
    ASSERT_WITH_MESSAGE(isAperture == applicationBundleIsEqualTo("com.apple.Aperture"), "Should not be called before setApplicationBundleIdentifier()");
    return isAperture;
}

bool MacApplication::isVersions()
{
    static bool isVersions = applicationBundleIsEqualTo("com.blackpixel.versions");
    ASSERT_WITH_MESSAGE(isVersions == applicationBundleIsEqualTo("com.blackpixel.versions"), "Should not be called before setApplicationBundleIdentifier()");
    return isVersions;
}

bool MacApplication::isHRBlock()
{
    static bool isHRBlock = applicationBundleIsEqualTo("com.hrblock.tax.2010");
    ASSERT_WITH_MESSAGE(isHRBlock == applicationBundleIsEqualTo("com.hrblock.tax.2010"), "Should not be called before setApplicationBundleIdentifier()");
    return isHRBlock;
}

bool MacApplication::isSolidStateNetworksDownloader()
{
    static bool isSolidStateNetworksDownloader = applicationBundleIsEqualTo("com.solidstatenetworks.awkhost");
    ASSERT_WITH_MESSAGE(isSolidStateNetworksDownloader == applicationBundleIsEqualTo("com.solidstatenetworks.awkhost"), "Should not be called before setApplicationBundleIdentifier()");

    return isSolidStateNetworksDownloader;
}

bool MacApplication::isHipChat()
{
    static bool isHipChat = applicationBundleIsEqualTo("com.hipchat.HipChat");
    ASSERT_WITH_MESSAGE(isHipChat == applicationBundleIsEqualTo("com.hipchat.HipChat"), "Should not be called before setApplicationBundleIdentifier()");
    return isHipChat;
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS)

bool IOSApplication::isMobileMail()
{
    static bool isMobileMail = applicationBundleIsEqualTo("com.apple.mobilemail");
    ASSERT_WITH_MESSAGE(isMobileMail == applicationBundleIsEqualTo("com.apple.mobilemail"), "Should not be called before setApplicationBundleIdentifier()");
    return isMobileMail;
}

bool IOSApplication::isMobileSafari()
{
    static bool isMobileSafari = applicationBundleIsEqualTo("com.apple.mobilesafari");
    ASSERT_WITH_MESSAGE(isMobileSafari == applicationBundleIsEqualTo("com.apple.mobilesafari"), "Should not be called before setApplicationBundleIdentifier()");
    return isMobileSafari;
}

bool IOSApplication::isDumpRenderTree()
{
    // We use a prefix match instead of strict equality since LayoutTestRelay may launch multiple instances of
    // DumpRenderTree where the bundle identifier of each instance has a unique suffix.
    static bool isDumpRenderTree = applicationBundleIsEqualTo("org.webkit.DumpRenderTree"); // e.g. org.webkit.DumpRenderTree0
    ASSERT_WITH_MESSAGE(isDumpRenderTree == applicationBundleIsEqualTo("org.webkit.DumpRenderTree"), "Should not be called before setApplicationBundleIdentifier()");
    return isDumpRenderTree;
}

bool IOSApplication::isMobileStore()
{
    static bool isMobileStore = applicationBundleIsEqualTo("com.apple.MobileStore");
    ASSERT_WITH_MESSAGE(isMobileStore == applicationBundleIsEqualTo("com.apple.MobileStore"), "Should not be called before setApplicationBundleIdentifier()");
    return isMobileStore;
}

bool IOSApplication::isWebApp()
{
    static bool isWebApp = applicationBundleIsEqualTo("com.apple.webapp");
    ASSERT_WITH_MESSAGE(isWebApp == applicationBundleIsEqualTo("com.apple.webapp"), "Should not be called before setApplicationBundleIdentifier()");
    return isWebApp;
}

bool IOSApplication::isOkCupid()
{
    static bool isOkCupid = applicationBundleIsEqualTo("com.okcupid.app");
    ASSERT_WITH_MESSAGE(isOkCupid == applicationBundleIsEqualTo("com.okcupid.app"), "Should not be called before setApplicationBundleIdentifier()");
    return isOkCupid;
}

bool IOSApplication::isFacebook()
{
    static bool isFacebook = applicationBundleIsEqualTo("com.facebook.Facebook");
    ASSERT_WITH_MESSAGE(isFacebook == applicationBundleIsEqualTo("com.facebook.Facebook"), "Should not be called before setApplicationBundleIdentifier()");
    return isFacebook;
}

bool IOSApplication::isDaijisenDictionary()
{
    static bool isDaijisenDictionary = applicationBundleIsEqualTo("jp.co.shogakukan.daijisen2009i");
    ASSERT_WITH_MESSAGE(isDaijisenDictionary == applicationBundleIsEqualTo("jp.co.shogakukan.daijisen2009i"), "Should not be called before setApplicationBundleIdentifier()");
    return isDaijisenDictionary;
}

bool IOSApplication::isNASAHD()
{
    static bool isNASAHD = applicationBundleIsEqualTo("gov.nasa.NASAHD");
    ASSERT_WITH_MESSAGE(isNASAHD == applicationBundleIsEqualTo("gov.nasa.NASAHD"), "Should not be called before setApplicationBundleIdentifier()");
    return isNASAHD;
}

bool IOSApplication::isTheEconomistOnIphone()
{
    static bool isTheEconomistOnIphone = applicationBundleIsEqualTo("com.economist.iphone");
    ASSERT_WITH_MESSAGE(isTheEconomistOnIphone == applicationBundleIsEqualTo("com.economist.iphone"), "Should not be called before setApplicationBundleIdentifier()");
    return isTheEconomistOnIphone;
}

// FIXME: this needs to be changed when the WebProcess is changed to an XPC service.
bool IOSApplication::isWebProcess()
{
    static bool isWebProcess = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.WebKit.WebContent.Development"]
        || [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.WebKit.WebContent"]
        || [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.WebProcess"];
    return isWebProcess;
}

bool IOSApplication::isIBooks()
{
    static bool isIBooks = applicationBundleIsEqualTo("com.apple.iBooks");
    ASSERT_WITH_MESSAGE(isIBooks == applicationBundleIsEqualTo("com.apple.iBooks"), "Should not be called before setApplicationBundleIdentifier()");
    return isIBooks;
}

#endif

} // namespace WebCore
