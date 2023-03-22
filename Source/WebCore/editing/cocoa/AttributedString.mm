/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "AttributedString.h"

#import <Foundation/Foundation.h>
#if PLATFORM(MAC)
#import <AppKit/AppKit.h>
#else
#import <pal/ios/UIKitSoftLink.h>
#endif

namespace WebCore {

bool AttributedString::rangesAreSafe(const String& string, const Vector<std::pair<Range, HashMap<String, AttributeValue>>>& vector)
{
    auto stringLength = string.length();
    for (auto& pair : vector) {
        auto& range = pair.first;
        if (range.location > stringLength
            || range.length > stringLength
            || range.location + range.length > stringLength)
            return false;
    }
    return true;
}

static RetainPtr<NSObject> toNSObject(const AttributedString::AttributeValue& value)
{
    return WTF::switchOn(value.value, [] (double value) -> RetainPtr<NSObject> {
        return adoptNS([[NSNumber alloc] initWithDouble:value]);
    }, [] (const String& value) -> RetainPtr<NSObject> {
        return (NSString *)value;
    }, [] (const RetainPtr<NSParagraphStyle>& value) -> RetainPtr<NSObject> {
        return value;
    }, [] (const URL& value) -> RetainPtr<NSObject> {
        return (NSURL *)value;
#if PLATFORM(MAC)
    }, [] (const RetainPtr<NSFont>& value) -> RetainPtr<NSObject> {
        return value;
    }, [] (const RetainPtr<NSColor>& value) -> RetainPtr<NSObject> {
        return value;
    }, [] (const RetainPtr<NSTextAttachment>& value) -> RetainPtr<NSObject> {
        return value;
#else
    }, [] (const RetainPtr<UIFont>& value) -> RetainPtr<NSObject> {
        return value;
    }, [] (const RetainPtr<UIColor>& value) -> RetainPtr<NSObject> {
        return value;
#endif
    });
}

static RetainPtr<NSDictionary> toNSDictionary(const HashMap<String, AttributedString::AttributeValue>& map)
{
    auto result = adoptNS([[NSMutableDictionary alloc] initWithCapacity:map.size()]);
    for (auto& pair : map)
        [result setObject:toNSObject(pair.value).get() forKey:(NSString *)pair.key];
    return result;
}

RetainPtr<NSDictionary> AttributedString::documentAttributesAsNSDictionary() const
{
    return toNSDictionary(documentAttributes);
}

RetainPtr<NSAttributedString> AttributedString::nsAttributedString() const
{
    auto result = adoptNS([[NSMutableAttributedString alloc] initWithString:(NSString *)string]);
    for (auto& pair : attributes) {
        auto& map = pair.second;
        auto& range = pair.first;
        [result addAttributes:toNSDictionary(map).get() range:NSMakeRange(range.location, range.length)];
    }
    return result;
}

static std::optional<AttributedString::AttributeValue> extractValue(id value)
{
    if (auto* number = dynamic_objc_cast<NSNumber>(value))
        return { { { number.doubleValue } } };
    if (auto* string = dynamic_objc_cast<NSString>(value))
        return { { { String { string } } } };
    if (auto* url = dynamic_objc_cast<NSURL>(value))
        return { { { URL { url } } } };
#if PLATFORM(MAC)
    if (auto* paragraphStyle = dynamic_objc_cast<NSParagraphStyle>(value))
        return { { { RetainPtr { paragraphStyle } } } };
    if (auto* font = dynamic_objc_cast<NSFont>(value))
        return { { { RetainPtr { font } } } };
    if (auto* color = dynamic_objc_cast<NSColor>(value))
        return { { { RetainPtr { color } } } };
    if (auto* textAttachment = dynamic_objc_cast<NSTextAttachment>(value))
        return { { { RetainPtr { textAttachment } } } };
#else
    if ([value isKindOfClass:PAL::getNSParagraphStyleClass()])
        return { { { RetainPtr { (NSParagraphStyle *)value } } } };
    if ([value isKindOfClass:PAL::getUIFontClass()])
        return { { { RetainPtr { (UIFont *)value } } } };
    if ([value isKindOfClass:PAL::getUIColorClass()])
        return { { { RetainPtr { (UIColor *)value } } } };
#endif
    if (value) {
        LOG_ERROR("NSAttributedString extraction failed for class <%@>", NSStringFromClass([value class]));
        ASSERT_NOT_REACHED();
    }
    return std::nullopt;
}

static HashMap<String, AttributedString::AttributeValue> extractDictionary(NSDictionary *dictionary)
{
    __block HashMap<String, AttributedString::AttributeValue> result;
    [dictionary enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL *) {
        if (![key isKindOfClass:NSString.class]) {
            ASSERT_NOT_REACHED();
            return;
        }
        auto extractedValue = extractValue(value);
        if (!extractedValue) {
            ASSERT_NOT_REACHED();
            return;
        }
        result.set((NSString *)key, WTFMove(*extractedValue));
    }];
    return result;
}

AttributedString AttributedString::fromNSAttributedStringAndDocumentAttributes(RetainPtr<NSAttributedString>&& string, RetainPtr<NSDictionary>&& dictionary)
{
    __block AttributedString result;
    result.string = [string string];
    [string enumerateAttributesInRange:NSMakeRange(0, [string length]) options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired usingBlock: ^(NSDictionary<NSAttributedStringKey, id> *attributes, NSRange range, BOOL *) {
        result.attributes.append({ Range { range.location, range.length }, extractDictionary(attributes) });
    }];
    result.documentAttributes = extractDictionary(dictionary.get());
    return result;
}

}
