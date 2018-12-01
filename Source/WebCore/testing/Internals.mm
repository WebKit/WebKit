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

#import "DOMURL.h"
#import "DictionaryLookup.h"
#import "Document.h"
#import "EventHandler.h"
#import "HitTestResult.h"
#import "Range.h"
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/NSURLExtras.h>

#if PLATFORM(IOS_FAMILY)
SOFT_LINK_FRAMEWORK(UIKit)
SOFT_LINK(UIKit, UIAccessibilityIsReduceMotionEnabled, BOOL, (void), ())
#endif

namespace WebCore {

String Internals::userVisibleString(const DOMURL& url)
{
    return WTF::userVisibleString(url.href());
}

bool Internals::userPrefersReducedMotion() const
{
#if PLATFORM(IOS_FAMILY)
    return UIAccessibilityIsReduceMotionEnabled();
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

    HitTestResult result = document->frame()->mainFrame().eventHandler().hitTestResultAtPoint(IntPoint(x, y));
    RefPtr<Range> range;
    std::tie(range, std::ignore) = DictionaryLookup::rangeAtHitTestResult(result);
    return WTFMove(range);
}

#endif

}
