/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "AXTextMarker.h"

#import <Foundation/NSRange.h>
#if PLATFORM(MAC)
#import "AXIsolatedObject.h"
#import "WebAccessibilityObjectWrapperMac.h"
#import <pal/spi/mac/HIServicesSPI.h>
#else // PLATFORM(IOS_FAMILY)
#import "WebAccessibilityObjectWrapperIOS.h"
#endif

namespace WebCore {

AXTextMarker::AXTextMarker(PlatformTextMarkerData platformData)
{
    if (!platformData)
        return;

#if PLATFORM(MAC)
    if (CFGetTypeID(platformData) != AXTextMarkerGetTypeID()) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (AXTextMarkerGetLength(platformData) != sizeof(m_data)) {
        ASSERT_NOT_REACHED();
        return;
    }

    memcpy(&m_data, AXTextMarkerGetBytePtr(platformData), sizeof(m_data));
#else // PLATFORM(IOS_FAMILY)
    [platformData getBytes:&m_data length:sizeof(m_data)];
#endif
}

RetainPtr<PlatformTextMarkerData> AXTextMarker::platformData() const
{
#if PLATFORM(MAC)
    return adoptCF(AXTextMarkerCreate(kCFAllocatorDefault, (const UInt8*)&m_data, sizeof(m_data)));
#else // PLATFORM(IOS_FAMILY)
    return [NSData dataWithBytes:&m_data length:sizeof(m_data)];
#endif
}

#if ENABLE(AX_THREAD_TEXT_APIS)
RetainPtr<NSAttributedString> AXTextMarkerRange::toAttributedString() const
{
    RELEASE_ASSERT(!isMainThread());

    auto start = m_start.toTextRunMarker();
    if (!start.isValid())
        return nil;
    auto end = m_end.toTextRunMarker();
    if (!end.isValid())
        return nil;

    if (start.isolatedObject() == end.isolatedObject()) {
        size_t minOffset = std::min(start.offset(), end.offset());
        size_t maxOffset = std::max(start.offset(), end.offset());
        return start.isolatedObject()->createAttributedString(start.runs()->substring(minOffset, maxOffset - minOffset)).autorelease();
    }
    // FIXME: Handle ranges that span multiple objects.
    return nil;
}
#endif // ENABLE(AX_THREAD_TEXT_APIS)

#if PLATFORM(MAC)

AXTextMarkerRange::AXTextMarkerRange(AXTextMarkerRangeRef textMarkerRangeRef)
{
    if (!textMarkerRangeRef || CFGetTypeID(textMarkerRangeRef) != AXTextMarkerRangeGetTypeID()) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto start = AXTextMarkerRangeCopyStartMarker(textMarkerRangeRef);
    auto end = AXTextMarkerRangeCopyEndMarker(textMarkerRangeRef);

    m_start = start;
    m_end = end;

    CFRelease(start);
    CFRelease(end);
}

RetainPtr<AXTextMarkerRangeRef> AXTextMarkerRange::platformData() const
{
    return adoptCF(AXTextMarkerRangeCreate(kCFAllocatorDefault
        , m_start.platformData().autorelease()
        , m_end.platformData().autorelease()
    ));
}

#elif PLATFORM(IOS_FAMILY)

AXTextMarkerRange::AXTextMarkerRange(NSArray *markers)
{
    if (markers.count != 2)
        return;

    WebAccessibilityTextMarker *start = [markers objectAtIndex:0];
    WebAccessibilityTextMarker *end = [markers objectAtIndex:1];
    if (![start isKindOfClass:[WebAccessibilityTextMarker class]] || ![end isKindOfClass:[WebAccessibilityTextMarker class]])
        return;

    m_start = { [start textMarkerData ] };
    m_end = { [end textMarkerData] };
}

RetainPtr<NSArray> AXTextMarkerRange::platformData() const
{
    if (!*this)
        return nil;

    RefPtr object = m_start.object();
    ASSERT(object); // Since *this is not null.
    auto* cache = object->axObjectCache();
    if (!cache)
        return nil;

    auto start = adoptNS([[WebAccessibilityTextMarker alloc] initWithTextMarker:&m_start.m_data cache:cache]);
    auto end = adoptNS([[WebAccessibilityTextMarker alloc] initWithTextMarker:&m_end.m_data cache:cache]);
    return adoptNS([[NSArray alloc] initWithObjects:start.get(), end.get(), nil]);
}

#endif // PLATFORM(IOS_FAMILY)

std::optional<NSRange> AXTextMarkerRange::nsRange() const
{
    return characterRange();
}

} // namespace WebCore
