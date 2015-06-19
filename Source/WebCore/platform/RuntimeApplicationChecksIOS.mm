/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RuntimeApplicationChecksIOS.h"

// FIXME: We should consider merging this file with RuntimeApplicationChecks.cpp.
#if PLATFORM(IOS)

namespace WebCore {

bool applicationIsMobileMail()
{
    static const bool isMobileMail = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.mobilemail"];
    return isMobileMail;
}

bool applicationIsMobileSafari()
{
    static const bool isMobileSafari = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.mobilesafari"];
    return isMobileSafari;
}

bool applicationIsDumpRenderTree()
{
    // We use a prefix match instead of strict equality since LayoutTestRelay may launch multiple instances of
    // DumpRenderTree where the bundle identifier of each instance has a unique suffix.
    static const bool isDumpRenderTree = [[[NSBundle mainBundle] bundleIdentifier] hasPrefix:@"org.webkit.DumpRenderTree"]; // e.g. org.webkit.DumpRenderTree0
    return isDumpRenderTree;
}

bool applicationIsMobileStore()
{
    static const bool isMobileStore = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.MobileStore"];
    return isMobileStore;
}

bool applicationIsWebApp()
{
    static const bool isWebApp = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.webapp"];
    return isWebApp;
}

bool applicationIsOkCupid()
{
    static const bool isOkCupid = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.okcupid.app"];
    return isOkCupid;
}

bool applicationIsFacebook()
{
    static const bool isFacebook = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.facebook.Facebook"];
    return isFacebook;
}

bool applicationIsEpicurious()
{
    static const bool isEpicurious = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.condenet.Epicurious"];
    return isEpicurious;
}

bool applicationIsDaijisenDictionary()
{
    static const bool isDaijisenDictionary = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"jp.co.shogakukan.daijisen2009i"];
    return isDaijisenDictionary;
}

bool applicationIsNASAHD()
{
    static const bool isNASAHD = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"gov.nasa.NASAHD"];
    return isNASAHD;
}

bool applicationIsMASH()
{
    static const bool isMASH = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.magnateinteractive.mashgame"]
        || [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.magnateinteractive.mashlitegame"]
        || [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.magnateinteractive.mashchristmas"]
        || [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.magnateinteractive.mashhalloween"]
        || [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.magnateinteractive.mashvalentines"];
    return isMASH;
}

bool applicationIsTheEconomistOnIPhone()
{
    static const bool isTheEconomistOnIPhone = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.economist.iphone"];
    return isTheEconomistOnIPhone;
}

// FIXME: this needs to be changed when the WebProcess is changed to an XPC service.
bool applicationIsWebProcess()
{
    static const bool isWebProcess = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.WebKit.WebContent.Development"] || [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.WebKit.WebContent"] || [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.WebProcess"];
    return isWebProcess;
}

bool applicationIsIBooksOnIOS()
{
    static const bool isIBooksOnIOS = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.apple.iBooks"];
    return isIBooksOnIOS;
}

} // namespace WebCore

#endif // PLATFORM(IOS)
