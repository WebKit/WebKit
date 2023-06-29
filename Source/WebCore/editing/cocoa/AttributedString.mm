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

#import "Font.h"
#import "Logging.h"
#import <Foundation/Foundation.h>
#import <wtf/cocoa/VectorCocoa.h>
#if PLATFORM(MAC)
#import <AppKit/AppKit.h>
#else
#import <pal/ios/UIKitSoftLink.h>
#import "UIFoundationSoftLink.h"
#endif

namespace WebCore {

AttributedString::AttributedString() = default;

AttributedString::~AttributedString() = default;

AttributedString::AttributedString(AttributedString&&) = default;

AttributedString& AttributedString::operator=(AttributedString&&) = default;

AttributedString::AttributedString(const AttributedString&) = default;

AttributedString& AttributedString::operator=(const AttributedString&) = default;

AttributedString::AttributedString(String&& string, Vector<std::pair<Range, HashMap<String, AttributeValue>>>&& attributes, std::optional<HashMap<String, AttributeValue>>&& documentAttributes)
    : string(WTFMove(string))
    , attributes(WTFMove(attributes))
    , documentAttributes(WTFMove(documentAttributes)) { }

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

static RetainPtr<id> toNSObject(const AttributedString::AttributeValue& value)
{
    return WTF::switchOn(value.value, [] (double value) -> RetainPtr<id> {
        return adoptNS([[NSNumber alloc] initWithDouble:value]);
    }, [] (const String& value) -> RetainPtr<id> {
        return (NSString *)value;
    }, [] (const RetainPtr<NSParagraphStyle>& value) -> RetainPtr<id> {
        return value;
    }, [] (const RetainPtr<NSPresentationIntent>& value) -> RetainPtr<id> {
        return value;
    }, [] (const URL& value) -> RetainPtr<id> {
        return (NSURL *)value;
    }, [] (const Vector<String>& value) -> RetainPtr<id> {
        return createNSArray(value, [] (const String& string) {
            return (NSString *)string;
        });
    }, [] (const Vector<double>& value) -> RetainPtr<id> {
        return createNSArray(value, [] (double number) {
            return adoptNS([[NSNumber alloc] initWithDouble:number]);
        });
    }, [] (const RetainPtr<NSTextAttachment>& value) -> RetainPtr<id> {
        return value;
    }, [] (const RetainPtr<NSShadow>& value) -> RetainPtr<id> {
        return value;
    }, [] (const RetainPtr<NSDate>& value) -> RetainPtr<id> {
        return value;
    }, [] (const Ref<Font>& font) -> RetainPtr<id> {
        return (__bridge PlatformFont *)(font->getCTFont());
    }, [] (const RetainPtr<PlatformColor>& value) -> RetainPtr<id> {
        return value;
    }, [] (const RetainPtr<CGColorRef>& value) -> RetainPtr<id> {
        return (__bridge id)value.get();
    });
}

static RetainPtr<NSDictionary> toNSDictionary(const HashMap<String, AttributedString::AttributeValue>& map)
{
    auto result = adoptNS([[NSMutableDictionary alloc] initWithCapacity:map.size()]);
    for (auto& pair : map) {
        if (auto nsObject = toNSObject(pair.value))
            [result setObject:nsObject.get() forKey:(NSString *)pair.key];
    }
    return result;
}

RetainPtr<NSDictionary> AttributedString::documentAttributesAsNSDictionary() const
{
    if (!documentAttributes)
        return nullptr;
    return toNSDictionary(*documentAttributes);
}

RetainPtr<NSAttributedString> AttributedString::nsAttributedString() const
{
    if (string.isNull())
        return nullptr;

    auto result = adoptNS([[NSMutableAttributedString alloc] initWithString:(NSString *)string]);
    for (auto& pair : attributes) {
        auto& map = pair.second;
        auto& range = pair.first;
        [result addAttributes:toNSDictionary(map).get() range:NSMakeRange(range.location, range.length)];
    }
    return result;
}

static std::optional<AttributedString::AttributeValue> extractArray(NSArray *array)
{
    size_t arrayLength = array.count;
    if (!arrayLength)
        return { { { Vector<String>() } } };
    if ([array[0] isKindOfClass:NSString.class]) {
        Vector<String> result;
        result.reserveInitialCapacity(arrayLength);
        for (id element in array) {
            if ([element isKindOfClass:NSString.class])
                result.uncheckedAppend((NSString *)element);
            else
                RELEASE_LOG_ERROR(Editing, "NSAttributedString extraction failed with array containing <%@>", NSStringFromClass([element class]));
        }
        return { { { WTFMove(result) } } };
    }
    if ([array[0] isKindOfClass:NSNumber.class]) {
        Vector<double> result;
        result.reserveInitialCapacity(arrayLength);
        for (id element in array) {
            if ([element isKindOfClass:NSNumber.class])
                result.uncheckedAppend([(NSNumber *)element doubleValue]);
            else
                RELEASE_LOG_ERROR(Editing, "NSAttributedString extraction failed with array containing <%@>", NSStringFromClass([element class]));
        }
        return { { { WTFMove(result) } } };
    }
    RELEASE_LOG_ERROR(Editing, "NSAttributedString extraction failed with array of unknown values");
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

static std::optional<AttributedString::AttributeValue> extractValue(id value)
{
    if (CFGetTypeID((CFTypeRef)value) == CGColorGetTypeID())
        return { { { RetainPtr<CGColorRef> { (CGColorRef) value } } } };
    if (auto* number = dynamic_objc_cast<NSNumber>(value))
        return { { { number.doubleValue } } };
    if (auto* string = dynamic_objc_cast<NSString>(value))
        return { { { String { string } } } };
    if (auto* url = dynamic_objc_cast<NSURL>(value))
        return { { { URL { url } } } };
    if (auto* array = dynamic_objc_cast<NSArray>(value))
        return extractArray(array);
    if (auto* date = dynamic_objc_cast<NSDate>(value))
        return { { { RetainPtr { date } } } };
    if ([value isKindOfClass:PlatformNSShadow])
        return { { { RetainPtr { (NSShadow *)value } } } };
    if ([value isKindOfClass:PlatformNSParagraphStyle])
        return { { { RetainPtr { (NSParagraphStyle *)value } } } };
    if ([value isKindOfClass:PlatformNSPresentationIntent])
        return { { { RetainPtr { (NSPresentationIntent *)value } } } };
    if ([value isKindOfClass:PlatformNSTextAttachment])
        return { { { RetainPtr { (NSTextAttachment *)value } } } };
    if ([value isKindOfClass:PlatformFontClass])
        return { { { Font::create(FontPlatformData((__bridge CTFontRef)value, [(PlatformFont *)value pointSize])) } } };
    if ([value isKindOfClass:PlatformColorClass])
        return { { { RetainPtr { (PlatformColor *)value } } } };
    if (value) {
        RELEASE_LOG_ERROR(Editing, "NSAttributedString extraction failed for class <%@>", NSStringFromClass([value class]));
        ASSERT_NOT_REACHED();
        return std::nullopt;
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

AttributedString AttributedString::fromNSAttributedString(RetainPtr<NSAttributedString>&& string)
{
    return fromNSAttributedStringAndDocumentAttributes(WTFMove(string), nullptr);
}

AttributedString AttributedString::fromNSAttributedStringAndDocumentAttributes(RetainPtr<NSAttributedString>&& string, RetainPtr<NSDictionary>&& dictionary)
{
    __block AttributedString result;
    result.string = [string string];
    [string enumerateAttributesInRange:NSMakeRange(0, [string length]) options:NSAttributedStringEnumerationLongestEffectiveRangeNotRequired usingBlock: ^(NSDictionary<NSAttributedStringKey, id> *attributes, NSRange range, BOOL *) {
        result.attributes.append({ Range { range.location, range.length }, extractDictionary(attributes) });
    }];
    if (dictionary)
        result.documentAttributes = extractDictionary(dictionary.get());
    return { WTFMove(result) };
}

}
