/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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
#import <wtf/spi/darwin/dyldSPI.h>
#import <wtf/text/WTFString.h>

namespace WebCore {

#if !ASSERT_MSG_DISABLED
static bool applicationBundleIdentifierOverrideWasQueried;
#endif

// The application bundle identifier gets set to the UIProcess bundle identifier by the WebProcess and
// the Networking upon initialization. It is unset otherwise.
static String& bundleIdentifierOverride()
{
    static NeverDestroyed<String> identifierOverride;
#if !ASSERT_MSG_DISABLED
    applicationBundleIdentifierOverrideWasQueried = true;
#endif
    return identifierOverride;
}

static String& bundleIdentifier()
{
    static NeverDestroyed<String> identifier;
    return identifier;
}

String applicationBundleIdentifier()
{
    // The override only gets set in WebKitTestRunner. If unset, we use the bundle identifier set
    // in WebKit2's WebProcess and NetworkProcess. If that is also unset, we use the main bundle identifier.
    auto identifier = bundleIdentifierOverride();
    ASSERT(identifier.isNull() || RunLoop::isMain());
    if (identifier.isNull())
        identifier = bundleIdentifier();
    return identifier.isNull() ? String([[NSBundle mainBundle] bundleIdentifier]) : identifier;
}

void setApplicationBundleIdentifier(const String& identifier)
{
    ASSERT(RunLoop::isMain());
    bundleIdentifier() = identifier;
}

void setApplicationBundleIdentifierOverride(const String& identifier)
{
    ASSERT(RunLoop::isMain());
    ASSERT_WITH_MESSAGE(!applicationBundleIdentifierOverrideWasQueried, "applicationBundleIsEqualTo() and applicationBundleStartsWith() should not be called before setApplicationBundleIdentifier()");
    bundleIdentifierOverride() = identifier;
}

void clearApplicationBundleIdentifierTestingOverride()
{
    ASSERT(RunLoop::isMain());
    bundleIdentifierOverride() = String();
#if !ASSERT_MSG_DISABLED
    applicationBundleIdentifierOverrideWasQueried = false;
#endif
}

bool isInWebProcess()
{
    static bool mainBundleIsWebProcess = [[[NSBundle mainBundle] bundleIdentifier] hasPrefix:@"com.apple.WebKit.WebContent"];
    return mainBundleIsWebProcess;
}

bool isInNetworkProcess()
{
    static bool mainBundleIsNetworkProcess = [[[NSBundle mainBundle] bundleIdentifier] hasPrefix:@"com.apple.WebKit.Networking"];
    return mainBundleIsNetworkProcess;
}

static bool applicationBundleIsEqualTo(const String& bundleIdentifierString)
{
    return applicationBundleIdentifier() == bundleIdentifierString;
}

#if PLATFORM(MAC)

bool MacApplication::isSafari()
{
    static bool isSafari = applicationBundleIsEqualTo("com.apple.Safari"_s)
        || applicationBundleIsEqualTo("com.apple.SafariTechnologyPreview"_s)
        || applicationBundleIdentifier().startsWith("com.apple.Safari.");
    return isSafari;
}

bool MacApplication::isAppleMail()
{
    static bool isAppleMail = applicationBundleIsEqualTo("com.apple.mail"_s);
    return isAppleMail;
}

bool MacApplication::isIBooks()
{
    static bool isIBooks = applicationBundleIsEqualTo("com.apple.iBooksX"_s);
    return isIBooks;
}

bool MacApplication::isITunes()
{
    static bool isITunes = applicationBundleIsEqualTo("com.apple.iTunes"_s);
    return isITunes;
}

bool MacApplication::isMicrosoftMessenger()
{
    static bool isMicrosoftMessenger = applicationBundleIsEqualTo("com.microsoft.Messenger"_s);
    return isMicrosoftMessenger;
}

bool MacApplication::isAdobeInstaller()
{
    static bool isAdobeInstaller = applicationBundleIsEqualTo("com.adobe.Installers.Setup"_s);
    return isAdobeInstaller;
}

bool MacApplication::isAOLInstantMessenger()
{
    static bool isAOLInstantMessenger = applicationBundleIsEqualTo("com.aol.aim.desktop"_s);
    return isAOLInstantMessenger;
}

bool MacApplication::isMicrosoftMyDay()
{
    static bool isMicrosoftMyDay = applicationBundleIsEqualTo("com.microsoft.myday"_s);
    return isMicrosoftMyDay;
}

bool MacApplication::isMicrosoftOutlook()
{
    static bool isMicrosoftOutlook = applicationBundleIsEqualTo("com.microsoft.Outlook"_s);
    return isMicrosoftOutlook;
}

bool MacApplication::isMiniBrowser()
{
    static bool isMiniBrowser = applicationBundleIsEqualTo("org.webkit.MiniBrowser"_s);
    return isMiniBrowser;
}

bool MacApplication::isQuickenEssentials()
{
    static bool isQuickenEssentials = applicationBundleIsEqualTo("com.intuit.QuickenEssentials"_s);
    return isQuickenEssentials;
}

bool MacApplication::isAperture()
{
    static bool isAperture = applicationBundleIsEqualTo("com.apple.Aperture"_s);
    return isAperture;
}

bool MacApplication::isVersions()
{
    static bool isVersions = applicationBundleIsEqualTo("com.blackpixel.versions"_s);
    return isVersions;
}

bool MacApplication::isHRBlock()
{
    static bool isHRBlock = applicationBundleIsEqualTo("com.hrblock.tax.2010"_s);
    return isHRBlock;
}

bool MacApplication::isIAdProducer()
{
    static bool isIAdProducer = applicationBundleIsEqualTo("com.apple.iAdProducer"_s);
    return isIAdProducer;
}

bool MacApplication::isSolidStateNetworksDownloader()
{
    static bool isSolidStateNetworksDownloader = applicationBundleIsEqualTo("com.solidstatenetworks.awkhost"_s);
    return isSolidStateNetworksDownloader;
}

bool MacApplication::isEpsonSoftwareUpdater()
{
    static bool isEpsonSoftwareUpdater = applicationBundleIsEqualTo("com.epson.EPSON_Software_Updater"_s);
    return isEpsonSoftwareUpdater;
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)

static bool applicationBundleStartsWith(const String& bundleIdentifierPrefix)
{
    return applicationBundleIdentifier().startsWith(bundleIdentifierPrefix);
}

bool IOSApplication::isMobileMail()
{
    static bool isMobileMail = applicationBundleIsEqualTo("com.apple.mobilemail"_s);
    return isMobileMail;
}

bool IOSApplication::isMailCompositionService()
{
    static bool isMailCompositionService = applicationBundleIsEqualTo("com.apple.MailCompositionService"_s);
    return isMailCompositionService;
}

bool IOSApplication::isMobileSafari()
{
    static bool isMobileSafari = applicationBundleIsEqualTo("com.apple.mobilesafari"_s);
    return isMobileSafari;
}

bool IOSApplication::isSafariViewService()
{
    static bool isSafariViewService = applicationBundleIsEqualTo("com.apple.SafariViewService"_s);
    return isSafariViewService;
}

bool IOSApplication::isIMDb()
{
    static bool isIMDb = applicationBundleIsEqualTo("com.imdb.imdb"_s);
    return isIMDb;
}

bool IOSApplication::isWebBookmarksD()
{
    static bool isWebBookmarksD = applicationBundleIsEqualTo("com.apple.webbookmarksd"_s);
    return isWebBookmarksD;
}

bool IOSApplication::isDumpRenderTree()
{
    // We use a prefix match instead of strict equality since multiple instances of DumpRenderTree
    // may be launched, where the bundle identifier of each instance has a unique suffix.
    static bool isDumpRenderTree = applicationBundleIsEqualTo("org.webkit.DumpRenderTree"_s); // e.g. org.webkit.DumpRenderTree0
    return isDumpRenderTree;
}

bool IOSApplication::isMobileStore()
{
    static bool isMobileStore = applicationBundleIsEqualTo("com.apple.MobileStore"_s);
    return isMobileStore;
}

bool IOSApplication::isSpringBoard()
{
    static bool isSpringBoard = applicationBundleIsEqualTo("com.apple.springboard"_s);
    return isSpringBoard;
}

// FIXME: this needs to be changed when the WebProcess is changed to an XPC service.
bool IOSApplication::isWebProcess()
{
    return isInWebProcess();
}

bool IOSApplication::isIBooks()
{
    static bool isIBooks = applicationBundleIsEqualTo("com.apple.iBooks"_s);
    return isIBooks;
}

bool IOSApplication::isIBooksStorytime()
{
    static bool isIBooksStorytime = applicationBundleIsEqualTo("com.apple.TVBooks"_s);
    return isIBooksStorytime;
}

bool IOSApplication::isTheSecretSocietyHiddenMystery()
{
    static bool isTheSecretSocietyHiddenMystery = applicationBundleIsEqualTo("com.g5e.secretsociety"_s);
    return isTheSecretSocietyHiddenMystery;
}

bool IOSApplication::isCardiogram()
{
    static bool isCardiogram = applicationBundleIsEqualTo("com.cardiogram.ios.heart"_s);
    return isCardiogram;
}

bool IOSApplication::isNike()
{
    static bool isNike = applicationBundleIsEqualTo("com.nike.omega"_s);
    return isNike;
}

bool IOSApplication::isMoviStarPlus()
{
    static bool isMoviStarPlus = applicationBundleIsEqualTo("com.prisatv.yomvi"_s);
    return isMoviStarPlus;
}

bool IOSApplication::isFirefox()
{
    static bool isFirefox = applicationBundleIsEqualTo("org.mozilla.ios.Firefox"_s);
    return isFirefox;
}

bool IOSApplication::isAppleApplication()
{
    static bool isAppleApplication = applicationBundleStartsWith("com.apple."_s);
    return isAppleApplication;
}

bool IOSApplication::isEvernote()
{
    static bool isEvernote = applicationBundleIsEqualTo("com.evernote.iPhone.Evernote"_s);
    return isEvernote;
}

bool IOSApplication::isEventbrite()
{
    static bool isEventbrite = applicationBundleIsEqualTo("com.eventbrite.attendee"_s);
    return isEventbrite;
}

bool IOSApplication::isDataActivation()
{
    static bool isDataActivation = applicationBundleIsEqualTo("com.apple.DataActivation"_s);
    return isDataActivation;
}

bool IOSApplication::isMiniBrowser()
{
    static bool isMiniBrowser = applicationBundleIsEqualTo("org.webkit.MiniBrowser"_s);
    return isMiniBrowser;
}

bool IOSApplication::isNews()
{
    static bool isNews = applicationBundleIsEqualTo("com.apple.news"_s);
    return isNews;
}

bool IOSApplication::isStocks()
{
    static bool isStocks = applicationBundleIsEqualTo("com.apple.stocks"_s);
    return isStocks;
}

bool IOSApplication::isFeedly()
{
    static bool isFeedly = applicationBundleIsEqualTo("com.devhd.feedly"_s);
    return isFeedly;
}

bool IOSApplication::isPocketCity()
{
    static bool isPocketCity = applicationBundleIsEqualTo("com.codebrewgames.pocketcity"_s);
    return isPocketCity;
}

bool IOSApplication::isEssentialSkeleton()
{
    static bool isEssentialSkeleton = applicationBundleIsEqualTo("com.3d4medical.EssentialSkeleton"_s);
    return isEssentialSkeleton;
}

bool IOSApplication::isLaBanquePostale()
{
    static bool isLaBanquePostale = applicationBundleIsEqualTo("fr.labanquepostale.moncompte"_s);
    return isLaBanquePostale;
}

bool IOSApplication::isESPNFantasySports()
{
    static bool isESPNFantasySports = applicationBundleIsEqualTo("com.espn.fantasyFootball"_s);
    return isESPNFantasySports;
}

bool IOSApplication::isDoubleDown()
{
    static bool isDoubleDown = applicationBundleIsEqualTo("com.doubledowninteractive.DDCasino"_s);
    return isDoubleDown;
}

bool IOSApplication::isFIFACompanion()
{
    static bool isFIFACompanion = applicationBundleIsEqualTo("com.ea.ios.fifaultimate"_s);
    return isFIFACompanion;
}

bool IOSApplication::isNoggin()
{
    static bool isNoggin = applicationBundleIsEqualTo("com.mtvn.noggin"_s);
    return isNoggin;
}

bool IOSApplication::isOKCupid()
{
    static bool isOKCupid = applicationBundleIsEqualTo("com.okcupid.app"_s);
    return isOKCupid;
}

bool IOSApplication::isJWLibrary()
{
    static bool isJWLibrary = applicationBundleIsEqualTo("org.jw.jwlibrary"_s);
    return isJWLibrary;
}

bool IOSApplication::isPaperIO()
{
    static bool isPaperIO = applicationBundleIsEqualTo("io.voodoo.paperio"_s);
    return isPaperIO;
}

#endif

} // namespace WebCore

#endif // PLATFORM(COCOA)
