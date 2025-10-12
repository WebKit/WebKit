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
#import <wtf/StdLibExtras.h>

#if PLATFORM(MAC)
#import "AXIsolatedObject.h"
#import "WebAccessibilityObjectWrapperMac.h"
#import <pal/spi/mac/HIServicesSPI.h>
#else // PLATFORM(IOS_FAMILY)
#import "WebAccessibilityObjectWrapperIOS.h"
#endif

namespace WebCore {

using namespace Accessibility;

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

    memcpySpan(asMutableByteSpan(m_data), AXTextMarkerGetByteSpan(platformData));
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
// FIXME: There's a lot of duplicated code between this function and AXTextMarkerRange::toString().
RetainPtr<NSAttributedString> AXTextMarkerRange::toAttributedString(AXCoreObject::SpellCheck spellCheck) const
{
    ASSERT(!isMainThread());

    auto start = m_start.toTextRunMarker();
    if (!start.isValid())
        return nil;
    auto end = m_end.toTextRunMarker();
    if (!end.isValid())
        return nil;

    String listMarkerText = listMarkerTextOnSameLine(start);

    RefPtr startObject = start.isolatedObject();
    if (startObject == end.isolatedObject()) {
        size_t minOffset = std::min(start.offset(), end.offset());
        size_t maxOffset = std::max(start.offset(), end.offset());

        StringView substringView = start.runs()->substring(minOffset, maxOffset - minOffset);
        if (listMarkerText.isEmpty()) {
            // In the common case where there is no list marker text, we don't need to allocate a new string,
            // instead passing just the substring StringView to createAttributedString.
            return startObject->createAttributedString(substringView, spellCheck).autorelease();
        }
        return startObject->createAttributedString(makeString(listMarkerText, substringView), spellCheck).autorelease();
    }

    RetainPtr<NSMutableAttributedString> result = nil;
    StringView substringView = start.runs()->substring(start.offset());
    // As done above, only allocate a new string via makeString if we actually have listMarkerText.
    if (listMarkerText.isEmpty())
        result = startObject->createAttributedString(substringView, spellCheck);
    else
        result = startObject->createAttributedString(makeString(listMarkerText, substringView), spellCheck);

    auto appendToResult = [&result] (RetainPtr<NSMutableAttributedString>&& string) {
        if (!string)
            return;

        if (result)
            [result appendAttributedString:string.autorelease()];
        else
            result = WTFMove(string);
    };

    auto emitNewlineOnExit = [&] (AXIsolatedObject& object) {
        // FIXME: This function should not just be emitting newlines, but instead handling every character type in TextEmissionBehavior.
        auto behavior = object.textEmissionBehavior();
        if (behavior != TextEmissionBehavior::Newline && behavior != TextEmissionBehavior::DoubleNewline)
            return;

        auto length = [result length];
        // Like TextIterator, don't emit a newline if the most recently emitted character was already a newline.
        if (length && [[result string] characterAtIndex:length - 1] != '\n') {
            // FIXME: This is super inefficient. We are creating a whole new dictionary and attributed string just to append newline(s).
            NSString *newlineString = behavior == TextEmissionBehavior::Newline ? @"\n" : @"\n\n";
            NSDictionary *attributes = [result attributesAtIndex:length - 1 effectiveRange:nil];
            appendToResult(adoptNS([[NSMutableAttributedString alloc] initWithString:newlineString attributes:attributes]));
        }
    };

    // FIXME: If we've been given reversed markers, i.e. the end marker actually comes before the start marker,
    // we may want to detect this and try searching AXDirection::Previous?
    RefPtr current = findObjectWithRuns(*startObject, AXDirection::Next, std::nullopt, emitNewlineOnExit);
    while (current && current->objectID() != end.objectID()) {
        appendToResult(current->createAttributedString(current->textRuns()->toStringView(), spellCheck));
        current = findObjectWithRuns(*current, AXDirection::Next, std::nullopt, emitNewlineOnExit);
    }
    appendToResult(end.isolatedObject()->createAttributedString(end.runs()->substring(0, end.offset()), spellCheck));

    return result;
}
#endif // ENABLE(AX_THREAD_TEXT_APIS)

#if PLATFORM(MAC)

AXTextMarkerRange::AXTextMarkerRange(AXTextMarkerRangeRef textMarkerRangeRef)
{
    if (!textMarkerRangeRef || CFGetTypeID(textMarkerRangeRef) != AXTextMarkerRangeGetTypeID()) {
        ASSERT_NOT_REACHED();
        return;
    }

    RetainPtr start = adoptCF(AXTextMarkerRangeCopyStartMarker(textMarkerRangeRef));
    RetainPtr end = adoptCF(AXTextMarkerRangeCopyEndMarker(textMarkerRangeRef));

    m_start = start.get();
    m_end = end.get();
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

    RefPtr object = downcast<AccessibilityObject>(m_start.object());
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
