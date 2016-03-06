/*
 * Copyright (C) 2009, 2011 Apple Inc. All rights reserved.
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

#ifndef RuntimeApplicationChecks_h
#define RuntimeApplicationChecks_h

#if PLATFORM(COCOA)
#include <wtf/Forward.h>

namespace WebCore {

WEBCORE_EXPORT void setApplicationBundleIdentifier(const String&);

#if PLATFORM(MAC)

namespace MacApplication {

WEBCORE_EXPORT bool isAOLInstantMessenger();
WEBCORE_EXPORT bool isAdobeInstaller();
WEBCORE_EXPORT bool isAperture();
WEBCORE_EXPORT bool isAppleMail();
WEBCORE_EXPORT bool isIBooks();
WEBCORE_EXPORT bool isITunes();
WEBCORE_EXPORT bool isMicrosoftMessenger();
WEBCORE_EXPORT bool isMicrosoftMyDay();
WEBCORE_EXPORT bool isMicrosoftOutlook();
bool isQuickenEssentials();
WEBCORE_EXPORT bool isSafari();
bool isSolidStateNetworksDownloader();
WEBCORE_EXPORT bool isVersions();
WEBCORE_EXPORT bool isHRBlock();
WEBCORE_EXPORT bool isHipChat();

} // MacApplication

#endif // PLATFORM(MAC)

#if PLATFORM(IOS)

namespace IOSApplication {

WEBCORE_EXPORT bool isMobileMail();
WEBCORE_EXPORT bool isMobileSafari();
bool isDumpRenderTree();
bool isMobileStore();
WEBCORE_EXPORT bool isWebApp();
WEBCORE_EXPORT bool isOkCupid();
WEBCORE_EXPORT bool isFacebook();
WEBCORE_EXPORT bool isDaijisenDictionary();
bool isNASAHD();
WEBCORE_EXPORT bool isTheEconomistOnIphone();
WEBCORE_EXPORT bool isWebProcess();
bool isIBooks();

} // IOSApplication

#endif // PLATFORM(IOS)

} // namespace WebCore

#endif // PLATFORM(COCOA)

#endif // RuntimeApplicationChecks_h
