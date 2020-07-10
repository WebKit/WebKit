/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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
#import "Internals.h"

#import "AGXCompilerService.h"
#import "DOMURL.h"
#import "DictionaryLookup.h"
#import "Document.h"
#import "EventHandler.h"
#import "HTMLMediaElement.h"
#import "HitTestResult.h"
#import "MediaPlayerPrivate.h"
#import "Range.h"
#import "SimpleRange.h"
#import "UTIUtilities.h"
#import <AVFoundation/AVPlayer.h>
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/spi/darwin/SandboxSPI.h>

#if PLATFORM(IOS_FAMILY)
#import <pal/ios/UIKitSoftLink.h>
#endif

namespace WebCore {

String Internals::userVisibleString(const DOMURL& url)
{
    return WTF::userVisibleString(url.href());
}

bool Internals::userPrefersReducedMotion() const
{
#if PLATFORM(IOS_FAMILY)
    return PAL::softLink_UIKit_UIAccessibilityIsReduceMotionEnabled();
#else
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
#endif
}

#if PLATFORM(MAC)

ExceptionOr<RefPtr<Range>> Internals::rangeForDictionaryLookupAtLocation(int x, int y)
{
    auto* document = contextDocument();
    if (!document || !document->frame())
        return Exception { InvalidAccessError };

    document->updateLayoutIgnorePendingStylesheets();

    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::ReadOnly, HitTestRequest::Active, HitTestRequest::DisallowUserAgentShadowContent, HitTestRequest::AllowChildFrameContent };
    auto result = document->frame()->mainFrame().eventHandler().hitTestResultAtPoint(IntPoint(x, y), hitType);
    auto range = DictionaryLookup::rangeAtHitTestResult(result);
    if (!range)
        return nullptr;

    return RefPtr<Range> { createLiveRange(std::get<SimpleRange>(*range)) };
}

#endif

#if ENABLE(VIDEO)
double Internals::privatePlayerVolume(const HTMLMediaElement& element)
{
    auto corePlayer = element.player();
    if (!corePlayer)
        return 0;
    auto player = corePlayer->objCAVFoundationAVPlayer();
    if (!player)
        return 0;
    return [player volume];
}
#endif

String Internals::encodedPreferenceValue(const String& domain, const String& key)
{
    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:domain]);
    id value = [userDefaults objectForKey:key];
    auto data = adoptNS([NSKeyedArchiver archivedDataWithRootObject:value requiringSecureCoding:YES error:nullptr]);
    return [data base64EncodedStringWithOptions:0];
}

String Internals::getUTIFromTag(const String& tagClass, const String& tag, const String& conformingToUTI)
{
    return UTIFromTag(tagClass, tag, conformingToUTI);
}

bool Internals::isRemoteUIAppForAccessibility()
{
#if PLATFORM(MAC)
    return [NSAccessibilityRemoteUIElement isRemoteUIApp];
#else
    return false;
#endif
}

bool Internals::hasSandboxIOKitOpenAccessToClass(const String& process, const String& ioKitClass)
{
    UNUSED_PARAM(process); // TODO: add support for getting PID of other WebKit processes.
    pid_t pid = getpid();

    return !sandbox_check(pid, "iokit-open", static_cast<enum sandbox_filter_type>(SANDBOX_FILTER_IOKIT_CONNECTION | SANDBOX_CHECK_NO_REPORT), ioKitClass.utf8().data());
}

}
