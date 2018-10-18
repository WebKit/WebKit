/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import "ArgumentCodersMac.h"

#import <CoreText/CoreText.h>
#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#endif

#import "ArgumentCodersCF.h"
#import "Decoder.h"
#import "Encoder.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/ColorMac.h>

namespace IPC {

enum class NSType {
    AttributedString,
#if USE(APPKIT)
    Color,
#endif
    Dictionary,
    Array,
#if USE(APPKIT)
    Font,
#endif
    Number,
    String,
    Date,
    Data,
    URL,
    Unknown,
};

}

namespace IPC {
using namespace WebCore;

static NSType typeFromObject(id object)
{
    ASSERT(object);

    if ([object isKindOfClass:[NSAttributedString class]])
        return NSType::AttributedString;
#if USE(APPKIT)
    if ([object isKindOfClass:[NSColor class]])
        return NSType::Color;
#endif
    if ([object isKindOfClass:[NSDictionary class]])
        return NSType::Dictionary;
#if USE(APPKIT)
    if ([object isKindOfClass:[NSFont class]])
        return NSType::Font;
#endif
    if ([object isKindOfClass:[NSNumber class]])
        return NSType::Number;
    if ([object isKindOfClass:[NSString class]])
        return NSType::String;
    if ([object isKindOfClass:[NSArray class]])
        return NSType::Array;
    if ([object isKindOfClass:[NSDate class]])
        return NSType::Date;
    if ([object isKindOfClass:[NSData class]])
        return NSType::Data;
    if ([object isKindOfClass:[NSURL class]])
        return NSType::URL;

    ASSERT_NOT_REACHED();
    return NSType::Unknown;
}

void encode(Encoder& encoder, id object)
{
    NSType type = typeFromObject(object);
    encoder << type;

    switch (type) {
    case NSType::AttributedString:
        encode(encoder, static_cast<NSAttributedString *>(object));
        return;
#if USE(APPKIT)
    case NSType::Color:
        encode(encoder, static_cast<NSColor *>(object));
        return;
#endif
    case NSType::Dictionary:
        encode(encoder, static_cast<NSDictionary *>(object));
        return;
#if USE(APPKIT)
    case NSType::Font:
        encode(encoder, static_cast<NSFont *>(object));
        return;
#endif
    case NSType::Number:
        encode(encoder, static_cast<NSNumber *>(object));
        return;
    case NSType::String:
        encode(encoder, static_cast<NSString *>(object));
        return;
    case NSType::Array:
        encode(encoder, static_cast<NSArray *>(object));
        return;
    case NSType::Date:
        encode(encoder, static_cast<NSDate *>(object));
        return;
    case NSType::Data:
        encode(encoder, static_cast<NSData *>(object));
        return;
    case NSType::URL:
        encode(encoder, static_cast<NSURL *>(object));
        return;
    case NSType::Unknown:
        break;
    }

    ASSERT_NOT_REACHED();
}

bool decode(Decoder& decoder, RetainPtr<id>& result)
{
    NSType type;
    if (!decoder.decodeEnum(type))
        return false;

    switch (type) {
    case NSType::AttributedString: {
        RetainPtr<NSAttributedString> string;
        if (!decode(decoder, string))
            return false;
        result = string;
        return true;
    }
#if USE(APPKIT)
    case NSType::Color: {
        RetainPtr<NSColor> color;
        if (!decode(decoder, color))
            return false;
        result = color;
        return true;
    }
#endif
    case NSType::Dictionary: {
        RetainPtr<NSDictionary> dictionary;
        if (!decode(decoder, dictionary))
            return false;
        result = dictionary;
        return true;
    }
#if USE(APPKIT)
    case NSType::Font: {
        RetainPtr<NSFont> font;
        if (!decode(decoder, font))
            return false;
        result = font;
        return true;
    }
#endif
    case NSType::Number: {
        RetainPtr<NSNumber> number;
        if (!decode(decoder, number))
            return false;
        result = number;
        return true;
    }
    case NSType::String: {
        RetainPtr<NSString> string;
        if (!decode(decoder, string))
            return false;
        result = string;
        return true;
    }
    case NSType::Array: {
        RetainPtr<NSArray> array;
        if (!decode(decoder, array))
            return false;
        result = array;
        return true;
    }
    case NSType::Date: {
        RetainPtr<NSDate> date;
        if (!decode(decoder, date))
            return false;
        result = date;
        return true;
    }
    case NSType::Data: {
        RetainPtr<NSData> data;
        if (!decode(decoder, data))
            return false;
        result = data;
        return true;
    }
    case NSType::URL: {
        RetainPtr<NSURL> URL;
        if (!decode(decoder, URL))
            return false;
        result = URL;
        return true;
    }
    case NSType::Unknown:
        ASSERT_NOT_REACHED();
        return false;
    }

    return false;
}

static inline bool isSerializableFont(CTFontRef font)
{
    return adoptCF(CTFontCopyAttribute(font, kCTFontURLAttribute));
}

static inline bool isSerializableValue(id value)
{
#if USE(APPKIT)
    auto fontClass = [NSFont class];
#else
    auto fontClass = [UIFont class];
#endif
    return ![value isKindOfClass:fontClass] || isSerializableFont((__bridge CTFontRef)value);
}

static inline RetainPtr<NSDictionary> filterUnserializableValues(NSDictionary *dictionary)
{
    __block bool modificationNecessary = false;
    [dictionary enumerateKeysAndObjectsUsingBlock:^(id key, id object, BOOL *stop) {
        if (!isSerializableValue(object)) {
            modificationNecessary = true;
            *stop = YES;
        }
    }];
    if (!modificationNecessary)
        return dictionary;

    auto result = adoptNS([[NSMutableDictionary alloc] init]);
    [dictionary enumerateKeysAndObjectsUsingBlock:^(id key, id object, BOOL *stop) {
        if (isSerializableValue(object))
            [result setObject:object forKey:key];
    }];
    return result;
}

void encode(Encoder& encoder, NSAttributedString *string)
{
    // Even though NSAttributedString is toll free bridged with CFAttributedStringRef, attributes' values may be not, so we should stay within this file's code.

    NSString *plainString = [string string];
    NSUInteger length = [plainString length];
    IPC::encode(encoder, plainString);

    Vector<std::pair<NSRange, RetainPtr<NSDictionary>>> ranges;

    NSUInteger position = 0;
    while (position < length) {
        // Collect ranges in a vector, becasue the total count should be encoded first.
        NSRange effectiveRange;
        RetainPtr<NSDictionary> attributesAtIndex = [string attributesAtIndex:position effectiveRange:&effectiveRange];
        ASSERT(effectiveRange.location == position);
        ASSERT(effectiveRange.length);
        ASSERT(NSMaxRange(effectiveRange) <= length);

        ranges.append(std::make_pair(effectiveRange, filterUnserializableValues(attributesAtIndex.get())));

        position = NSMaxRange(effectiveRange);
    }

    encoder << static_cast<uint64_t>(ranges.size());

    for (size_t i = 0; i < ranges.size(); ++i) {
        encoder << static_cast<uint64_t>(ranges[i].first.location);
        encoder << static_cast<uint64_t>(ranges[i].first.length);
        IPC::encode(encoder, ranges[i].second.get());
    }
}

bool decode(Decoder& decoder, RetainPtr<NSAttributedString>& result)
{
    RetainPtr<NSString> plainString;
    if (!IPC::decode(decoder, plainString))
        return false;

    NSUInteger stringLength = [plainString length];

    RetainPtr<NSMutableAttributedString> resultString = adoptNS([[NSMutableAttributedString alloc] initWithString:plainString.get()]);

    uint64_t rangeCount;
    if (!decoder.decode(rangeCount))
        return false;

    while (rangeCount--) {
        uint64_t rangeLocation;
        uint64_t rangeLength;
        RetainPtr<NSDictionary> attributes;
        if (!decoder.decode(rangeLocation))
            return false;
        if (!decoder.decode(rangeLength))
            return false;

        ASSERT(rangeLocation + rangeLength > rangeLocation);
        ASSERT(rangeLocation + rangeLength <= stringLength);
        if (rangeLocation + rangeLength <= rangeLocation || rangeLocation + rangeLength > stringLength)
            return false;

        if (!IPC::decode(decoder, attributes))
            return false;
        [resultString addAttributes:attributes.get() range:NSMakeRange(rangeLocation, rangeLength)];
    }

    result = adoptNS(resultString.leakRef());
    return true;
}

#if USE(APPKIT)
void encode(Encoder& encoder, NSColor *color)
{
    encoder << colorFromNSColor(color);
}

bool decode(Decoder& decoder, RetainPtr<NSColor>& result)
{
    Color color;
    if (!decoder.decode(color))
        return false;

    result = nsColor(color);
    return true;
}
#endif

void encode(Encoder& encoder, NSDictionary *dictionary)
{
    // Even though NSDictionary is toll free bridged with CFDictionaryRef, values may be not, so we should stay within this file's code.

    NSUInteger size = [dictionary count];
    NSArray *keys = [dictionary allKeys];
    NSArray *values = [dictionary allValues];

    encoder << static_cast<uint64_t>(size);

    for (NSUInteger i = 0; i < size; ++i) {
        id key = [keys objectAtIndex:i];
        id value = [values objectAtIndex:i];
        ASSERT(key);
        ASSERT(value);
        ASSERT(isSerializableValue(value));

        // Ignore values we don't recognize.
        if (typeFromObject(value) == NSType::Unknown)
            continue;

        encode(encoder, key);
        encode(encoder, value);
    }
}

bool decode(Decoder& decoder, RetainPtr<NSDictionary>& result)
{
    uint64_t size;
    if (!decoder.decode(size))
        return false;

    RetainPtr<NSMutableDictionary> dictionary = adoptNS([[NSMutableDictionary alloc] initWithCapacity:size]);
    for (uint64_t i = 0; i < size; ++i) {
        // Try to decode the key name.
        RetainPtr<id> key;
        if (!decode(decoder, key))
            return false;

        RetainPtr<id> value;
        if (!decode(decoder, value))
            return false;

        [dictionary setObject:value.get() forKey:key.get()];
    }

    result = adoptNS(dictionary.leakRef());
    return true;
}

#if USE(APPKIT)
void encode(Encoder& encoder, NSFont *font)
{
    // NSFont could use CTFontRef code if we had it in ArgumentCodersCF.
    encode(encoder, font.fontDescriptor.fontAttributes);
}

bool decode(Decoder& decoder, RetainPtr<NSFont>& result)
{
    RetainPtr<NSDictionary> fontAttributes;
    if (!decode(decoder, fontAttributes))
        return false;

    NSFontDescriptor *fontDescriptor = [NSFontDescriptor fontDescriptorWithFontAttributes:fontAttributes.get()];
    result = [NSFont fontWithDescriptor:fontDescriptor size:0];

    return result;
}
#endif // USE(APPKIT)

#if PLATFORM(IOS_FAMILY)

void encode(Encoder& encoder, UIFont *font)
{
    encode(encoder, font.fontDescriptor.fontAttributes);
}

bool decode(Decoder& decoder, RetainPtr<UIFont>& result)
{
    RetainPtr<NSDictionary> fontAttributes;
    if (!decode(decoder, fontAttributes))
        return false;

    UIFontDescriptor *fontDescriptor = [UIFontDescriptor fontDescriptorWithFontAttributes:fontAttributes.get()];
    result = [UIFont fontWithDescriptor:fontDescriptor size:0];

    return result;
}

#endif // PLATFORM(IOS_FAMILY)

void encode(Encoder& encoder, NSNumber *number)
{
    encode(encoder, (__bridge CFNumberRef)number);
}

bool decode(Decoder& decoder, RetainPtr<NSNumber>& result)
{
    RetainPtr<CFNumberRef> number;
    if (!decode(decoder, number))
        return false;

    result = adoptNS((NSNumber *)number.leakRef());
    return true;
}

void encode(Encoder& encoder, NSString *string)
{
    encode(encoder, (__bridge CFStringRef)string);
}

bool decode(Decoder& decoder, RetainPtr<NSString>& result)
{
    RetainPtr<CFStringRef> string;
    if (!decode(decoder, string))
        return false;

    result = adoptNS((NSString *)string.leakRef());
    return true;
}

void encode(Encoder& encoder, NSArray *array)
{
    NSUInteger size = [array count];
    encoder << static_cast<uint64_t>(size);

    for (NSUInteger i = 0; i < size; ++i) {
        id value = [array objectAtIndex:i];

        // Ignore values we don't recognize.
        if (typeFromObject(value) == NSType::Unknown)
            continue;

        ASSERT(isSerializableValue(value));

        encode(encoder, value);
    }
}

bool decode(Decoder& decoder, RetainPtr<NSArray>& result)
{
    uint64_t size;
    if (!decoder.decode(size))
        return false;

    RetainPtr<NSMutableArray> array = adoptNS([[NSMutableArray alloc] initWithCapacity:size]);
    for (uint64_t i = 0; i < size; ++i) {
        RetainPtr<id> value;
        if (!decode(decoder, value))
            return false;

        [array addObject:value.get()];
    }

    result = adoptNS(array.leakRef());
    return true;
}

void encode(Encoder& encoder, NSDate *date)
{
    encode(encoder, (__bridge CFDateRef)date);
}

bool decode(Decoder& decoder, RetainPtr<NSDate>& result)
{
    RetainPtr<CFDateRef> date;
    if (!decode(decoder, date))
        return false;

    result = adoptNS((NSDate *)date.leakRef());
    return true;
}

void encode(Encoder& encoder, NSData *data)
{
    encode(encoder, (__bridge CFDataRef)data);
}

bool decode(Decoder& decoder, RetainPtr<NSData>& result)
{
    RetainPtr<CFDataRef> data;
    if (!decode(decoder, data))
        return false;

    result = adoptNS((NSData *)data.leakRef());
    return true;
}

void encode(Encoder& encoder, NSURL *URL)
{
    encode(encoder, (__bridge CFURLRef)URL);
}

bool decode(Decoder& decoder, RetainPtr<NSURL>& result)
{
    RetainPtr<CFURLRef> URL;
    if (!decode(decoder, URL))
        return false;

    result = adoptNS((NSURL *)URL.leakRef());
    return true;
}

} // namespace IPC

namespace WTF {
template<> struct EnumTraits<IPC::NSType> {
    using values = EnumValues<
        IPC::NSType,
        IPC::NSType::AttributedString,
    #if USE(APPKIT)
        IPC::NSType::Color,
    #endif
        IPC::NSType::Dictionary,
        IPC::NSType::Array,
    #if USE(APPKIT)
        IPC::NSType::Font,
    #endif
        IPC::NSType::Number,
        IPC::NSType::String,
        IPC::NSType::Date,
        IPC::NSType::Data,
        IPC::NSType::URL,
        IPC::NSType::Unknown
    >;
};
}
