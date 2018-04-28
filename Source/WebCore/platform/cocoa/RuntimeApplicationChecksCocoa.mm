/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#import <Foundation/NSBundle.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>
#import <wtf/text/WTFString.h>

namespace WebCore {

#if !ASSERT_MSG_DISABLED
static bool applicationBundleIdentifierOverrideWasQueried;
#endif

// The application bundle identifier gets set to the UIProcess bundle identifier by the WebProcess and
// the Networking upon initialization. It is unset otherwise.
static String& applicationBundleIdentifierOverride()
{
    static NeverDestroyed<String> identifier;
#if !ASSERT_MSG_DISABLED
    applicationBundleIdentifierOverrideWasQueried = true;
#endif
    return identifier;
}

String applicationBundleIdentifier()
{
    // The override only gets set in WebKit2's WebProcess and NetworkProcess. If unset, we use the main bundle identifier.
    const auto& identifier = applicationBundleIdentifierOverride();
    ASSERT(identifier.isNull() || RunLoop::isMain());
    return identifier.isNull() ? String([[NSBundle mainBundle] bundleIdentifier]) : identifier;
}

void setApplicationBundleIdentifier(const String& bundleIdentifier)
{
    ASSERT(RunLoop::isMain());
    ASSERT_WITH_MESSAGE(!applicationBundleIdentifierOverrideWasQueried, "applicationBundleIsEqualTo() should not be called before setApplicationBundleIdentifier()");
    applicationBundleIdentifierOverride() = bundleIdentifier;
}

bool isInWebProcess()
{
    static bool mainBundleIsWebProcess = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.WebKit.WebContent.Development"]
        || [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.WebKit.WebContent"]
        || [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.WebProcess"];
    return mainBundleIsWebProcess;
}

static bool applicationBundleIsEqualTo(const String& bundleIdentifierString)
{
    return applicationBundleIdentifier() == bundleIdentifierString;
}

#if PLATFORM(MAC)

bool MacApplication::isSafari()
{
    static bool isSafari = applicationBundleIsEqualTo("com.apple.Safari") || applicationBundleIsEqualTo("com.apple.SafariTechnologyPreview");
    return isSafari;
}

bool MacApplication::isAppleMail()
{
    static bool isAppleMail = applicationBundleIsEqualTo("com.apple.mail");
    return isAppleMail;
}

bool MacApplication::isIBooks()
{
    static bool isIBooks = applicationBundleIsEqualTo("com.apple.iBooksX");
    return isIBooks;
}

bool MacApplication::isITunes()
{
    static bool isITunes = applicationBundleIsEqualTo("com.apple.iTunes");
    return isITunes;
}

bool MacApplication::isMicrosoftMessenger()
{
    static bool isMicrosoftMessenger = applicationBundleIsEqualTo("com.microsoft.Messenger");
    return isMicrosoftMessenger;
}

bool MacApplication::isAdobeInstaller()
{
    static bool isAdobeInstaller = applicationBundleIsEqualTo("com.adobe.Installers.Setup");
    return isAdobeInstaller;
}

bool MacApplication::isAOLInstantMessenger()
{
    static bool isAOLInstantMessenger = applicationBundleIsEqualTo("com.aol.aim.desktop");
    return isAOLInstantMessenger;
}

bool MacApplication::isMicrosoftMyDay()
{
    static bool isMicrosoftMyDay = applicationBundleIsEqualTo("com.microsoft.myday");
    return isMicrosoftMyDay;
}

bool MacApplication::isMicrosoftOutlook()
{
    static bool isMicrosoftOutlook = applicationBundleIsEqualTo("com.microsoft.Outlook");
    return isMicrosoftOutlook;
}

bool MacApplication::isQuickenEssentials()
{
    static bool isQuickenEssentials = applicationBundleIsEqualTo("com.intuit.QuickenEssentials");
    return isQuickenEssentials;
}

bool MacApplication::isAperture()
{
    static bool isAperture = applicationBundleIsEqualTo("com.apple.Aperture");
    return isAperture;
}

bool MacApplication::isVersions()
{
    static bool isVersions = applicationBundleIsEqualTo("com.blackpixel.versions");
    return isVersions;
}

bool MacApplication::isHRBlock()
{
    static bool isHRBlock = applicationBundleIsEqualTo("com.hrblock.tax.2010");
    return isHRBlock;
}

bool MacApplication::isIAdProducer()
{
    static bool isIAdProducer = applicationBundleIsEqualTo("com.apple.iAdProducer");
    return isIAdProducer;
}

bool MacApplication::isSolidStateNetworksDownloader()
{
    static bool isSolidStateNetworksDownloader = applicationBundleIsEqualTo("com.solidstatenetworks.awkhost");
    return isSolidStateNetworksDownloader;
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS)

bool IOSApplication::isMobileMail()
{
    static bool isMobileMail = applicationBundleIsEqualTo("com.apple.mobilemail");
    return isMobileMail;
}

bool IOSApplication::isMobileSafari()
{
    static bool isMobileSafari = applicationBundleIsEqualTo("com.apple.mobilesafari");
    return isMobileSafari;
}

bool IOSApplication::isWebBookmarksD()
{
    static bool isWebBookmarksD = applicationBundleIsEqualTo("com.apple.webbookmarksd");
    return isWebBookmarksD;
}

bool IOSApplication::isDumpRenderTree()
{
    // We use a prefix match instead of strict equality since multiple instances of DumpRenderTree
    // may be launched, where the bundle identifier of each instance has a unique suffix.
    static bool isDumpRenderTree = applicationBundleIsEqualTo("org.webkit.DumpRenderTree"); // e.g. org.webkit.DumpRenderTree0
    return isDumpRenderTree;
}

bool IOSApplication::isMobileStore()
{
    static bool isMobileStore = applicationBundleIsEqualTo("com.apple.MobileStore");
    return isMobileStore;
}

bool IOSApplication::isSpringBoard()
{
    static bool isSpringBoard = applicationBundleIsEqualTo("com.apple.springboard");
    return isSpringBoard;
}

bool IOSApplication::isWebApp()
{
    static bool isWebApp = applicationBundleIsEqualTo("com.apple.webapp");
    return isWebApp;
}

// FIXME: this needs to be changed when the WebProcess is changed to an XPC service.
bool IOSApplication::isWebProcess()
{
    return isInWebProcess();
}

bool IOSApplication::isIBooks()
{
    static bool isIBooks = applicationBundleIsEqualTo("com.apple.iBooks");
    return isIBooks;
}

bool IOSApplication::isIBooksStorytime()
{
    static bool isIBooksStorytime = applicationBundleIsEqualTo("com.apple.TVBooks");
    return isIBooksStorytime;
}

bool IOSApplication::isTheSecretSocietyHiddenMystery()
{
    static bool isTheSecretSocietyHiddenMystery = applicationBundleIsEqualTo("com.g5e.secretsociety");
    return isTheSecretSocietyHiddenMystery;
}

bool IOSApplication::isCardiogram()
{
    static bool isCardiogram = applicationBundleIsEqualTo("com.cardiogram.ios.heart");
    return isCardiogram;
}

bool IOSApplication::isNike()
{
    static bool isNike = applicationBundleIsEqualTo("com.nike.omega");
    return isNike;
}

#endif

} // namespace WebCore

#endif // PLATFORM(COCOA)
