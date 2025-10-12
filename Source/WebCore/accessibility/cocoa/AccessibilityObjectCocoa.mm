/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#import "config.h"
#import "AccessibilityObjectInlines.h"

#if PLATFORM(COCOA)

#import "AXObjectCacheInlines.h"
#import "TextIterator.h"
#import "WebAccessibilityObjectWrapperBase.h"
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebCore {

OptionSet<SpeakAs> AccessibilityObject::speakAs() const
{
    if (auto* style = this->style())
        return style->speakAs();
    return { };
}

FloatPoint AccessibilityObject::screenRelativePosition() const
{
    auto rect = snappedIntRect(elementRect());
    // The Cocoa accessibility API wants the lower-left corner.
    return convertRectToPlatformSpace(FloatRect(FloatPoint(rect.x(), rect.maxY()), FloatSize()), AccessibilityConversionSpace::Screen).location();
}

AXTextMarkerRange AccessibilityObject::textMarkerRangeForNSRange(const NSRange& range) const
{
    if (range.location == NSNotFound)
        return { };

    if (!isTextControl())
        return { visiblePositionForIndex(range.location), visiblePositionForIndex(range.location + range.length) };

    if (range.location + range.length > text().length())
        return { };

    if (auto* cache = axObjectCache()) {
        auto start = cache->characterOffsetForIndex(range.location, this);
        auto end = cache->characterOffsetForIndex(range.location + range.length, this);
        return cache->rangeForUnorderedCharacterOffsets(start, end);
    }
    return { };
}

std::optional<NSRange> AccessibilityObject::visibleCharacterRange() const
{
    std::optional range = simpleRange();
    if (!range)
        return std::nullopt;

    auto contentRect = unobscuredContentRect();
    auto elementRect = snappedIntRect(this->elementRect());
    return makeNSRange(visibleCharacterRangeInternal(*range, contentRect, elementRect));
}

// NSAttributedString support.

#ifndef NSAttachmentCharacter
#define NSAttachmentCharacter 0xfffc
#endif

static void addObjectWrapperToArray(const AccessibilityObject& object, NSMutableArray *array)
{
    auto* wrapper = object.wrapper();
    if (!wrapper)
        return;

    // Don't add the same object twice.
    if ([array containsObject:wrapper])
        return;

#if PLATFORM(IOS_FAMILY)
    // Explicitly set that this is a new element, in case other logic tries to override.
    [wrapper setValue:@YES forKey:@"isAccessibilityElement"];
#endif

    [array addObject:wrapper];
}

void attributedStringSetNumber(NSMutableAttributedString *attrString, NSString *attribute, NSNumber *number, const NSRange& range)
{
    if (!attributedStringContainsRange(attrString, range))
        return;

    if (number)
        [attrString addAttribute:attribute value:number range:range];
}

static void attributedStringAppendWrapper(NSMutableAttributedString *attrString, WebAccessibilityObjectWrapper *wrapper)
{
    const auto attachmentCharacter = static_cast<UniChar>(NSAttachmentCharacter);
    [attrString appendAttributedString:adoptNS([[NSMutableAttributedString alloc] initWithString:[NSString stringWithCharacters:&attachmentCharacter length:1]
#if PLATFORM(MAC)
        attributes:@{ NSAccessibilityAttachmentTextAttribute : (__bridge id)adoptCF(NSAccessibilityCreateAXUIElementRef(wrapper)).get() }
#else
        attributes:@{ AccessibilityTokenAttachment : wrapper }
#endif
    ]).get()];
}

RetainPtr<NSArray> AccessibilityObject::contentForRange(const SimpleRange& range, SpellCheck spellCheck) const
{
    auto result = adoptNS([[NSMutableArray alloc] init]);

    // Iterate over the range to build the AX attributed strings.
    TextIterator it = textIteratorIgnoringFullSizeKana(range);
    for (; !it.atEnd(); it.advance()) {
        Ref node = it.range().start.container;

        // Non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX).
        if (it.text().length()) {
            auto listMarkerText = listMarkerTextForNodeAndPosition(node.ptr(), makeContainerOffsetPosition(it.range().start));
            if (!listMarkerText.isEmpty()) {
                if (auto attrString = attributedStringCreate(node.get(), listMarkerText, it.range(), SpellCheck::No))
                    [result addObject:attrString.get()];
            }

            if (auto attrString = attributedStringCreate(node.get(), it.text(), it.range(), spellCheck))
                [result addObject:attrString.get()];
        } else {
            if (RefPtr replacedNode = it.node()) {
                auto* cache = axObjectCache();
                if (RefPtr object = cache ? cache->getOrCreate(*replacedNode) : nullptr)
                    addObjectWrapperToArray(*object, result.get());
            }
        }
    }

    return result;
}

RetainPtr<NSAttributedString> AccessibilityObject::attributedStringForTextMarkerRange(AXTextMarkerRange&& textMarkerRange, SpellCheck spellCheck) const
{
#if PLATFORM(MAC)
    auto range = rangeForTextMarkerRange(axObjectCache(), textMarkerRange);
#else
    auto range = textMarkerRange.simpleRange();
#endif
    return range ? attributedStringForRange(*range, spellCheck) : nil;
}

RetainPtr<NSAttributedString> AccessibilityObject::attributedStringForRange(const SimpleRange& range, SpellCheck spellCheck) const
{
    auto result = adoptNS([[NSMutableAttributedString alloc] init]);

    auto contents = contentForRange(range, spellCheck);
    for (id content in contents.get()) {
        auto item = retainPtr(content);
        if ([item isKindOfClass:[WebAccessibilityObjectWrapper class]]) {
            attributedStringAppendWrapper(result.get(), item.get());
            continue;
        }

        if (![item isKindOfClass:[NSAttributedString class]])
            continue;

        [result appendAttributedString:item.get()];
    }

    return result;
}

RetainPtr<CTFontRef> fontFrom(const RenderStyle& style)
{
    return style.fontCascade().primaryFont()->ctFont();
}

Color textColorFrom(const RenderStyle& style)
{
    return style.visitedDependentColor(CSSPropertyColor);
}

Color backgroundColorFrom(const RenderStyle& style)
{
    return style.visitedDependentColor(CSSPropertyBackgroundColor);
}

RetainPtr<CTFontRef> AccessibilityObject::font() const
{
    const auto* style = this->style();
    return style ? fontFrom(*style) : nil;
}

#if ENABLE(AX_THREAD_TEXT_APIS)
FontOrientation AccessibilityObject::fontOrientation() const
{
    if (CheckedPtr style = this->style())
        return const_cast<RenderStyle*>(style.get())->fontAndGlyphOrientation().first;
    return FontOrientation::Horizontal;
}
#endif

Color AccessibilityObject::textColor() const
{
    const auto* style = this->style();
    return style ? textColorFrom(*style) : Color();
}

Color AccessibilityObject::backgroundColor() const
{
    const auto* style = this->style();
    return style ? backgroundColorFrom(*style) : Color();
}

bool AccessibilityObject::isSubscript() const
{
    const auto* style = this->style();
    return style && WTF::holdsAlternative<CSS::Keyword::Sub>(style->verticalAlign());
}

bool AccessibilityObject::isSuperscript() const
{
    const auto* style = this->style();
    return style && WTF::holdsAlternative<CSS::Keyword::Super>(style->verticalAlign());
}

bool AccessibilityObject::hasTextShadow() const
{
    const auto* style = this->style();
    return style && style->hasTextShadow();
}

LineDecorationStyle AccessibilityObject::lineDecorationStyle() const
{
    CheckedPtr renderer = this->renderer();
    return renderer ? LineDecorationStyle(*renderer) : LineDecorationStyle();
}

AttributedStringStyle AccessibilityObject::stylesForAttributedString() const
{
    const CheckedPtr style = this->style();
    if (!style)
        return { };

    auto& alignment = style->verticalAlign();
    return {
        fontFrom(*style),
        textColorFrom(*style),
        backgroundColorFrom(*style),
        WTF::holdsAlternative<CSS::Keyword::Sub>(alignment),
        WTF::holdsAlternative<CSS::Keyword::Super>(alignment),
        style->hasTextShadow(),
        lineDecorationStyle()
    };
}

} // namespace WebCore

#endif // PLATFORM(COCOA)
