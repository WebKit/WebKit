/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
#import "ArgumentCodersCocoa.h"

#if PLATFORM(COCOA)

#import "ArgumentCodersCF.h"
#import "CocoaColor.h"
#import "CocoaFont.h"
#import "CoreTextHelpers.h"
#import <CoreText/CTFont.h>
#import <CoreText/CTFontDescriptor.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/HashSet.h>
#import <wtf/cf/CFURLExtras.h>
#import <wtf/cocoa/NSURLExtras.h>

#if USE(APPKIT)
#import <WebCore/ColorMac.h>
#else
#import <WebCore/ColorIOS.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIColor.h>
#import <UIKit/UIFont.h>
#import <UIKit/UIFontDescriptor.h>
#endif

NS_ASSUME_NONNULL_BEGIN

@interface WKSecureCodingArchivingDelegate : NSObject <NSKeyedArchiverDelegate, NSKeyedUnarchiverDelegate>
@end

@interface WKSecureCodingURLWrapper : NSURL <NSSecureCoding>
- (instancetype _Nullable)initWithURL:(NSURL *)wrappedURL;
@property (nonatomic, readonly) NSURL * wrappedURL;
@end

@implementation WKSecureCodingArchivingDelegate

- (nullable id)archiver:(NSKeyedArchiver *)archiver willEncodeObject:(id)object
{
#if HAVE(NSFONT_WITH_OPTICAL_SIZING_BUG)
    if (auto font = dynamic_objc_cast<NSFont>(object)) {
        // Recreate any serialized fonts after normalizing the
        // font attributes to work around <rdar://problem/51657880>.
        return WebKit::fontWithAttributes(font.fontDescriptor.fontAttributes, 0);
    }
#endif

    if (auto unwrappedURL = dynamic_objc_cast<NSURL>(object))
        return adoptNS([[WKSecureCodingURLWrapper alloc] initWithURL:unwrappedURL]).autorelease();

    return object;
}

- (id _Nullable)unarchiver:(NSKeyedUnarchiver *)unarchiver didDecodeObject:(id _Nullable) NS_RELEASES_ARGUMENT object NS_RETURNS_RETAINED
{
    auto adoptedObject = adoptNS(object);
    if (auto wrapper = dynamic_objc_cast<WKSecureCodingURLWrapper>(adoptedObject.get()))
        return retainPtr(wrapper.wrappedURL).leakRef();

    return adoptedObject.leakRef();
}

@end

@implementation WKSecureCodingURLWrapper {
    RetainPtr<NSURL> m_wrappedURL;
}

- (NSURL *)wrappedURL
{
    return m_wrappedURL.get();
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

static constexpr NSString *baseURLKey = @"WK.baseURL";

- (void)encodeWithCoder:(NSCoder *)coder
{
    RELEASE_ASSERT(m_wrappedURL);
    auto baseURL = m_wrappedURL.get().baseURL;
    BOOL hasBaseURL = !!baseURL;

    [coder encodeValueOfObjCType:"c" at:&hasBaseURL];
    if (hasBaseURL)
        [coder encodeObject:baseURL forKey:baseURLKey];

    WTF::URLCharBuffer urlBytes;
    WTF::getURLBytes((__bridge CFURLRef)m_wrappedURL.get(), urlBytes);
    [coder encodeBytes:urlBytes.data() length:urlBytes.size()];
}

- (_Nullable instancetype)initWithCoder:(NSCoder *)coder
{
    auto selfPtr = adoptNS([super initWithString:@""]);
    if (!selfPtr)
        return nil;

    BOOL hasBaseURL;
    [coder decodeValueOfObjCType:"c" at:&hasBaseURL size:sizeof(hasBaseURL)];

    RetainPtr<NSURL> baseURL;
    if (hasBaseURL)
        baseURL = (NSURL *)[coder decodeObjectOfClass:NSURL.class forKey:baseURLKey];

    NSUInteger length;
    if (auto bytes = (UInt8 *)[coder decodeBytesWithReturnedLength:&length]) {
        m_wrappedURL = adoptNS((__bridge NSURL*)CFURLCreateAbsoluteURLWithBytes(nullptr, bytes, length, kCFStringEncodingUTF8, (__bridge CFURLRef)baseURL.get(), true));
        if (!m_wrappedURL)
            LOG_ERROR("Failed to decode NSURL due to invalid encoding of length %d. Substituting a blank URL", length);
    }

    if (!m_wrappedURL)
        m_wrappedURL = [NSURL URLWithString:@""];

    return selfPtr.leakRef();
}

- (_Nullable instancetype)initWithURL:(NSURL *)url
{
    if (self = [super initWithString:@""])
        m_wrappedURL = url;

    return self;
}

@end

namespace IPC {
using namespace WebCore;

#pragma mark - Types

enum class NSType {
    Array,
    Color,
    Data,
    Date,
    Dictionary,
    Font,
    Number,
    SecureCoding,
    String,
    URL,
    Unknown,
};

#pragma mark - Helpers

static NSType typeFromObject(id object)
{
    ASSERT(object);

    // Specific classes handled.
    if ([object isKindOfClass:[NSArray class]])
        return NSType::Array;
    if ([object isKindOfClass:[CocoaColor class]])
        return NSType::Color;
    if ([object isKindOfClass:[NSData class]])
        return NSType::Data;
    if ([object isKindOfClass:[NSDate class]])
        return NSType::Date;
    if ([object isKindOfClass:[NSDictionary class]])
        return NSType::Dictionary;
    if ([object isKindOfClass:[CocoaFont class]])
        return NSType::Font;
    if ([object isKindOfClass:[NSNumber class]])
        return NSType::Number;
    if ([object isKindOfClass:[NSString class]])
        return NSType::String;
    if ([object isKindOfClass:[NSURL class]])
        return NSType::URL;

    // Check NSSecureCoding after the specific cases since
    // most of the classes above conform to NSSecureCoding,
    // and we want our special case coders for them instead.
    if ([object conformsToProtocol:@protocol(NSSecureCoding)])
        return NSType::SecureCoding;

    ASSERT_NOT_REACHED();
    return NSType::Unknown;
}

static inline bool isSerializableFont(CTFontRef font)
{
    return adoptCF(CTFontCopyAttribute(font, kCTFontURLAttribute));
}

static inline bool isSerializableValue(id value)
{
    if ([value isKindOfClass:[CocoaFont class]])
        return isSerializableFont((__bridge CTFontRef)value);
    return typeFromObject(value) != NSType::Unknown;
}

#pragma mark - NSArray

static void encodeArrayInternal(Encoder& encoder, NSArray *array)
{
    // Even though NSArray is toll free bridged with CFArrayRef, values may not be,
    // so we should stay within this file's code.

    if (!array.count) {
        encoder << static_cast<uint64_t>(0);
        return;
    }

    HashSet<NSUInteger> invalidIndicies;
    for (NSUInteger i = 0; i < array.count; ++i) {
        id value = array[i];

        // Ignore values we don't support.
        ASSERT(isSerializableValue(value));
        if (!isSerializableValue(value))
            invalidIndicies.add(i);
    }

    encoder << static_cast<uint64_t>(array.count - invalidIndicies.size());

    for (NSUInteger i = 0; i < array.count; ++i) {
        if (invalidIndicies.contains(i))
            continue;
        encodeObject(encoder, array[i]);
    }
}

static Optional<RetainPtr<id>> decodeArrayInternal(Decoder& decoder, NSArray<Class> *allowedClasses)
{
    uint64_t size;
    if (!decoder.decode(size))
        return WTF::nullopt;

    auto array = adoptNS([[NSMutableArray alloc] init]);
    for (uint64_t i = 0; i < size; ++i) {
        auto value = decodeObject(decoder, allowedClasses);
        if (!value || !value.value())
            return WTF::nullopt;
        [array addObject:value.value().get()];
    }

    return { array };
}

#pragma mark - NSColor / UIColor

#if USE(APPKIT)
static inline void encodeColorInternal(Encoder& encoder, NSColor *color)
{
    encoder << colorFromNSColor(color);
}

static inline Optional<RetainPtr<id>> decodeColorInternal(Decoder& decoder)
{
    Color color;
    if (!decoder.decode(color))
        return WTF::nullopt;
    return { nsColor(color) };
}
#else
static inline void encodeColorInternal(Encoder& encoder, UIColor *color)
{
    encoder << colorFromUIColor(color);
}

static inline Optional<RetainPtr<id>> decodeColorInternal(Decoder& decoder)
{
    Color color;
    if (!decoder.decode(color))
        return WTF::nullopt;
    return { adoptNS([[UIColor alloc] initWithCGColor:cachedCGColor(color)]) };
}
#endif

#pragma mark - NSData

static inline void encodeDataInternal(Encoder& encoder, NSData *data)
{
    encode(encoder, (__bridge CFDataRef)data);
}

static inline Optional<RetainPtr<id>> decodeDataInternal(Decoder& decoder)
{
    RetainPtr<CFDataRef> data;
    if (!decode(decoder, data))
        return WTF::nullopt;
    return { adoptNS((NSData *)data.leakRef()) };
}

#pragma mark - NSDate

static inline void encodeDateInternal(Encoder& encoder, NSDate *date)
{
    encode(encoder, (__bridge CFDateRef)date);
}

static inline Optional<RetainPtr<id>> decodeDateInternal(Decoder& decoder)
{
    RetainPtr<CFDateRef> date;
    if (!decode(decoder, date))
        return WTF::nullopt;
    return { adoptNS((NSDate *)date.leakRef()) };
}

#pragma mark - NSDictionary

static void encodeDictionaryInternal(Encoder& encoder, NSDictionary *dictionary)
{
    // Even though NSDictionary is toll free bridged with CFDictionaryRef, keys/values may not be,
    // so we should stay within this file's code.

    if (!dictionary.count) {
        encoder << static_cast<uint64_t>(0);
        return;
    }

    HashSet<id> invalidKeys;
    for (id key in dictionary) {
        id value = dictionary[key];
        ASSERT(value);

        // Ignore values we don't support.
        ASSERT(isSerializableValue(key));
        ASSERT(isSerializableValue(value));
        if (!isSerializableValue(key) || !isSerializableValue(value))
            invalidKeys.add(key);
    }

    encoder << static_cast<uint64_t>(dictionary.count - invalidKeys.size());

    for (id key in dictionary) {
        if (invalidKeys.contains(key))
            continue;
        encodeObject(encoder, key);
        encodeObject(encoder, dictionary[key]);
    }
}

static Optional<RetainPtr<id>> decodeDictionaryInternal(Decoder& decoder, NSArray<Class> *allowedClasses)
{
    uint64_t size;
    if (!decoder.decode(size))
        return WTF::nullopt;

    RetainPtr<NSMutableDictionary> dictionary = adoptNS([[NSMutableDictionary alloc] initWithCapacity:size]);
    for (uint64_t i = 0; i < size; ++i) {
        auto key = decodeObject(decoder, allowedClasses);
        if (!key)
            return WTF::nullopt;

        auto value = decodeObject(decoder, allowedClasses);
        if (!value)
            return WTF::nullopt;

        [dictionary setObject:value.value().get() forKey:key.value().get()];
    }

    return { dictionary };
}

#pragma mark - NSFont / UIFont

static inline void encodeFontInternal(Encoder& encoder, CocoaFont *font)
{
    encode(encoder, font.fontDescriptor.fontAttributes);
}

static Optional<RetainPtr<id>> decodeFontInternal(Decoder& decoder)
{
    RetainPtr<NSDictionary> fontAttributes;
    if (!decode(decoder, fontAttributes))
        return WTF::nullopt;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    return { WebKit::fontWithAttributes(fontAttributes.get(), 0) };

    END_BLOCK_OBJC_EXCEPTIONS

    return { };
}

#pragma mark - NSNumber

static inline void encodeNumberInternal(Encoder& encoder, NSNumber *number)
{
    encode(encoder, (__bridge CFNumberRef)number);
}

static inline Optional<RetainPtr<id>> decodeNumberInternal(Decoder& decoder)
{
    RetainPtr<CFNumberRef> number;
    if (!decode(decoder, number))
        return WTF::nullopt;
    return { adoptNS((NSNumber *)number.leakRef()) };
}

#pragma mark - id <NSSecureCoding>

static void encodeSecureCodingInternal(Encoder& encoder, id <NSObject, NSSecureCoding> object)
{
    auto archiver = adoptNS([[NSKeyedArchiver alloc] initRequiringSecureCoding:YES]);

    auto delegate = adoptNS([[WKSecureCodingArchivingDelegate alloc] init]);
    [archiver setDelegate:delegate.get()];

    [archiver encodeObject:object forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];
    [archiver setDelegate:nil];

    encode(encoder, (__bridge CFDataRef)[archiver encodedData]);
}

static Optional<RetainPtr<id>> decodeSecureCodingInternal(Decoder& decoder, NSArray<Class> *allowedClasses)
{
    ASSERT(allowedClasses && allowedClasses.count);
    if (!allowedClasses || !allowedClasses.count)
        return WTF::nullopt;

    RetainPtr<CFDataRef> data;
    if (!decode(decoder, data))
        return WTF::nullopt;

    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:(__bridge NSData *)data.get() error:nullptr]);
    unarchiver.get().decodingFailurePolicy = NSDecodingFailurePolicyRaiseException;

    auto delegate = adoptNS([[WKSecureCodingArchivingDelegate alloc] init]);
    unarchiver.get().delegate = delegate.get();

    auto allowedClassSet = adoptNS([[NSMutableSet alloc] initWithArray:allowedClasses]);
    [allowedClassSet addObject:WKSecureCodingURLWrapper.class];
    
    @try {
        id result = [unarchiver decodeObjectOfClasses:allowedClassSet.get() forKey:NSKeyedArchiveRootObjectKey];
        ASSERT(!result || [result conformsToProtocol:@protocol(NSSecureCoding)]);
        return { result };
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to decode object of classes %@: %@", allowedClasses, exception);
        return WTF::nullopt;
    } @finally {
        [unarchiver finishDecoding];
        unarchiver.get().delegate = nil;
    }
}

#pragma mark - NSString

static inline void encodeStringInternal(Encoder& encoder, NSString *string)
{
    encode(encoder, (__bridge CFStringRef)string);
}

static inline Optional<RetainPtr<id>> decodeStringInternal(Decoder& decoder)
{
    RetainPtr<CFStringRef> string;
    if (!decode(decoder, string))
        return WTF::nullopt;
    return { adoptNS((NSString *)string.leakRef()) };
}

#pragma mark - NSURL

static inline void encodeURLInternal(Encoder& encoder, NSURL *URL)
{
    encode(encoder, (__bridge CFURLRef)URL);
}

static inline Optional<RetainPtr<id>> decodeURLInternal(Decoder& decoder)
{
    RetainPtr<CFURLRef> URL;
    if (!decode(decoder, URL))
        return WTF::nullopt;
    return { adoptNS((NSURL *)URL.leakRef()) };
}

#pragma mark - Entry Point Encoder / Decoder

void encodeObject(Encoder& encoder, id object)
{
    encoder << static_cast<bool>(!object);
    if (!object)
        return;

    NSType type = typeFromObject(object);
    encoder << type;

    switch (type) {
    case NSType::Array:
        encodeArrayInternal(encoder, static_cast<NSArray *>(object));
        return;
    case NSType::Color:
        encodeColorInternal(encoder, static_cast<CocoaColor *>(object));
        return;
    case NSType::Dictionary:
        encodeDictionaryInternal(encoder, static_cast<NSDictionary *>(object));
        return;
    case NSType::Font:
        encodeFontInternal(encoder, static_cast<CocoaFont *>(object));
        return;
    case NSType::Number:
        encodeNumberInternal(encoder, static_cast<NSNumber *>(object));
        return;
    case NSType::SecureCoding:
        encodeSecureCodingInternal(encoder, static_cast<id <NSObject, NSSecureCoding>>(object));
        return;
    case NSType::String:
        encodeStringInternal(encoder, static_cast<NSString *>(object));
        return;
    case NSType::Date:
        encodeDateInternal(encoder, static_cast<NSDate *>(object));
        return;
    case NSType::Data:
        encodeDataInternal(encoder, static_cast<NSData *>(object));
        return;
    case NSType::URL:
        encodeURLInternal(encoder, static_cast<NSURL *>(object));
        return;
    case NSType::Unknown:
        break;
    }

    ASSERT_NOT_REACHED();
}

Optional<RetainPtr<id>> decodeObject(Decoder& decoder, NSArray<Class> *allowedClasses)
{
    bool isNull;
    if (!decoder.decode(isNull))
        return WTF::nullopt;
    if (isNull)
        return { nullptr };

    NSType type;
    if (!decoder.decode(type))
        return WTF::nullopt;

    switch (type) {
    case NSType::Array:
        return decodeArrayInternal(decoder, allowedClasses);
    case NSType::Color:
        return decodeColorInternal(decoder);
    case NSType::Dictionary:
        return decodeDictionaryInternal(decoder, allowedClasses);
    case NSType::Font:
        return decodeFontInternal(decoder);
    case NSType::Number:
        return decodeNumberInternal(decoder);
    case NSType::SecureCoding:
        return decodeSecureCodingInternal(decoder, allowedClasses);
    case NSType::String:
        return decodeStringInternal(decoder);
    case NSType::Date:
        return decodeDateInternal(decoder);
    case NSType::Data:
        return decodeDataInternal(decoder);
    case NSType::URL:
        return decodeURLInternal(decoder);
    case NSType::Unknown:
        break;
    }

    ASSERT_NOT_REACHED();
    return WTF::nullopt;
}

} // namespace IPC

namespace WTF {
template<> struct EnumTraits<IPC::NSType> {
    using values = EnumValues<
        IPC::NSType,
        IPC::NSType::Array,
        IPC::NSType::Color,
        IPC::NSType::Data,
        IPC::NSType::Date,
        IPC::NSType::Dictionary,
        IPC::NSType::Font,
        IPC::NSType::Number,
        IPC::NSType::SecureCoding,
        IPC::NSType::String,
        IPC::NSType::URL,
        IPC::NSType::Unknown
    >;
};
} // namespace WTF

NS_ASSUME_NONNULL_END

#endif // PLATFORM(COCOA)
