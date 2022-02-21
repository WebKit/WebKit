/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CaptionUserPreferencesMediaAF.h"

#if ENABLE(VIDEO) && PLATFORM(COCOA)

#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/spi/cocoa/NSObjCRuntimeSPI.h>

@interface WebCaptionUserPreferencesMediaAFWeakObserver : NSObject {
    WeakPtr<WebCore::CaptionUserPreferencesMediaAF> m_weakPtr;
}
@property (nonatomic, readonly, direct) RefPtr<WebCore::CaptionUserPreferencesMediaAF> userPreferences;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithWeakPtr:(WeakPtr<WebCore::CaptionUserPreferencesMediaAF>&&)weakPtr NS_DESIGNATED_INITIALIZER;
@end

NS_DIRECT_MEMBERS
@implementation WebCaptionUserPreferencesMediaAFWeakObserver
- (instancetype)initWithWeakPtr:(WeakPtr<WebCore::CaptionUserPreferencesMediaAF>&&)weakPtr
{
    if ((self = [super init]))
        m_weakPtr = WTFMove(weakPtr);
    return self;
}

- (RefPtr<WebCore::CaptionUserPreferencesMediaAF>)userPreferences
{
    return m_weakPtr.get();
}
@end

namespace WebCore {

RetainPtr<WebCaptionUserPreferencesMediaAFWeakObserver> CaptionUserPreferencesMediaAF::createWeakObserver(CaptionUserPreferencesMediaAF* thisPtr)
{
    return adoptNS([[WebCaptionUserPreferencesMediaAFWeakObserver alloc] initWithWeakPtr:WeakPtr { *thisPtr }]);
}

RefPtr<CaptionUserPreferencesMediaAF> CaptionUserPreferencesMediaAF::extractCaptionUserPreferencesMediaAF(void* observer)
{
    RetainPtr strongObserver { dynamic_objc_cast<WebCaptionUserPreferencesMediaAFWeakObserver>(reinterpret_cast<id>(observer)) };
    if (!strongObserver)
        return nullptr;
    return [strongObserver userPreferences];
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && PLATFORM(COCOA)
