/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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
#import "ObjCObjectGraph.h"

#import "ArgumentCodersCocoa.h"
#import "Decoder.h"
#import "Encoder.h"
#import "UserData.h"
#import "WKAPICast.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKTypeRefWrapper.h"
#import <wtf/EnumTraits.h>
#import <wtf/Optional.h>

namespace WebKit {

static bool shouldTransformGraph(id object, const ObjCObjectGraph::Transformer& transformer)
{
    if (NSArray *array = dynamic_objc_cast<NSArray>(object)) {
        for (id element in array) {
            if (shouldTransformGraph(element, transformer))
                return true;
        }
    }

    if (NSDictionary *dictionary = dynamic_objc_cast<NSDictionary>(object)) {
        bool result = false;
        [dictionary enumerateKeysAndObjectsUsingBlock:[&transformer, &result](id key, id object, BOOL* stop) {
            if (shouldTransformGraph(object, transformer)) {
                result = true;
                *stop = YES;
            }
        }];

        return result;
    }

    return transformer.shouldTransformObject(object);
}

static RetainPtr<id> transformGraph(id object, const ObjCObjectGraph::Transformer& transformer)
{
    if (NSArray *array = dynamic_objc_cast<NSArray>(object)) {
        auto result = adoptNS([[NSMutableArray alloc] initWithCapacity:array.count]);
        for (id element in array)
            [result addObject:transformGraph(element, transformer).get()];

        return result;
    }

    if (NSDictionary *dictionary = dynamic_objc_cast<NSDictionary>(object)) {
        auto result = adoptNS([[NSMutableDictionary alloc] initWithCapacity:dictionary.count]);
        [dictionary enumerateKeysAndObjectsUsingBlock:[&result, &transformer](id key, id object, BOOL*) {
            [result setObject:transformGraph(object, transformer).get() forKey:key];
        }];

        return result;
    }

    return transformer.transformObject(object);
}

RetainPtr<id> ObjCObjectGraph::transform(id object, const Transformer& transformer)
{
    if (!object)
        return nullptr;

    if (!shouldTransformGraph(object, transformer))
        return object;

    return transformGraph(object, transformer);
}

enum class ObjCType : uint8_t {
    Null,

    NSArray,
    NSData,
    NSDate,
    NSDictionary,
    NSNumber,
    NSString,

    WKBrowsingContextHandle,
    WKTypeRefWrapper,
};

static Optional<ObjCType> typeFromObject(id object)
{
    ASSERT(object);

    if (dynamic_objc_cast<NSArray>(object))
        return ObjCType::NSArray;
    if (dynamic_objc_cast<NSData>(object))
        return ObjCType::NSData;
    if (dynamic_objc_cast<NSDate>(object))
        return ObjCType::NSDate;
    if (dynamic_objc_cast<NSDictionary>(object))
        return ObjCType::NSDictionary;
    if (dynamic_objc_cast<NSNumber>(object))
        return ObjCType::NSNumber;
    if (dynamic_objc_cast<NSString>(object))
        return ObjCType::NSString;

    if (dynamic_objc_cast<WKBrowsingContextHandle>(object))
        return ObjCType::WKBrowsingContextHandle;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (dynamic_objc_cast<WKTypeRefWrapper>(object))
        return ObjCType::WKTypeRefWrapper;
    ALLOW_DEPRECATED_DECLARATIONS_END

    return WTF::nullopt;
}

void ObjCObjectGraph::encode(IPC::Encoder& encoder, id object)
{
    if (!object) {
        encoder << static_cast<uint32_t>(ObjCType::Null);
        return;
    }

    auto type = typeFromObject(object);
    if (!type)
        [NSException raise:NSInvalidArgumentException format:@"Can not encode objects of class type '%@'", static_cast<NSString *>(NSStringFromClass([object class]))];

    encoder << *type;

    switch (type.value()) {
    case ObjCType::Null:
        return;

    case ObjCType::NSArray: {
        NSArray *array = object;

        encoder << static_cast<uint64_t>(array.count);
        for (id element in array)
            encode(encoder, element);
        return;
    }

    case ObjCType::NSData: {
        IPC::encode(encoder, static_cast<NSData *>(object));
        return;
    }

    case ObjCType::NSDate: {
        IPC::encode(encoder, static_cast<NSDate *>(object));
        return;
    }

    case ObjCType::NSDictionary: {
        NSDictionary *dictionary = object;

        encoder << static_cast<uint64_t>(dictionary.count);
        [dictionary enumerateKeysAndObjectsUsingBlock:[&encoder](id key, id object, BOOL *stop) {
            encode(encoder, key);
            encode(encoder, object);
        }];
        return;
    }

    case ObjCType::NSNumber: {
        IPC::encode(encoder, static_cast<NSNumber *>(object));
        return;
    }

    case ObjCType::NSString: {
        IPC::encode(encoder, static_cast<NSString *>(object));
        return;
    }

    case ObjCType::WKBrowsingContextHandle: {
        encoder << static_cast<WKBrowsingContextHandle *>(object).pageProxyID;
        encoder << static_cast<WKBrowsingContextHandle *>(object).webPageID;
        return;
    }

    case ObjCType::WKTypeRefWrapper: {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        UserData::encode(encoder, toImpl(static_cast<WKTypeRefWrapper *>(object).object));
        ALLOW_DEPRECATED_DECLARATIONS_END
        return;
    }
    }

    ASSERT_NOT_REACHED();
}

void ObjCObjectGraph::encode(IPC::Encoder& encoder) const
{
    encode(encoder, m_rootObject.get());
}

bool ObjCObjectGraph::decode(IPC::Decoder& decoder, RetainPtr<id>& result)
{
    ObjCType type;
    if (!decoder.decode(type))
        return false;

    switch (type) {
    case ObjCType::Null: {
        result = nil;
        return true;
    }

    case ObjCType::NSArray: {
        uint64_t size;
        if (!decoder.decode(size))
            return false;

        auto array = adoptNS([[NSMutableArray alloc] init]);
        for (uint64_t i = 0; i < size; ++i) {
            RetainPtr<id> element;
            if (!decode(decoder, element))
                return false;
            [array addObject:element.get()];
        }

        result = WTFMove(array);
        return true;
    }

    case ObjCType::NSData: {
        RetainPtr<NSData> data;
        if (!IPC::decode(decoder, data))
            return false;

        result = WTFMove(data);
        return true;
    }

    case ObjCType::NSDate: {
        RetainPtr<NSDate> date;
        if (!IPC::decode(decoder, date))
            return false;

        result = WTFMove(date);
        return true;
    }

    case ObjCType::NSDictionary: {
        uint64_t size;
        if (!decoder.decode(size))
            return false;

        auto dictionary = adoptNS([[NSMutableDictionary alloc] init]);
        for (uint64_t i = 0; i < size; ++i) {
            RetainPtr<id> key;
            if (!decode(decoder, key))
                return false;

            RetainPtr<id> object;
            if (!decode(decoder, object))
                return false;

            @try {
                [dictionary setObject:object.get() forKey:key.get()];
            } @catch (id) {
                return false;
            }
        }

        result = WTFMove(dictionary);
        return true;
    }

    case ObjCType::NSNumber: {
        RetainPtr<NSNumber> number;
        if (!IPC::decode(decoder, number))
            return false;

        result = WTFMove(number);
        return true;
    }

    case ObjCType::NSString: {
        RetainPtr<NSString> string;
        if (!IPC::decode(decoder, string))
            return false;

        result = WTFMove(string);
        return true;
    }

    case ObjCType::WKBrowsingContextHandle: {
        Optional<WebPageProxyIdentifier> pageProxyID;
        decoder >> pageProxyID;
        if (!pageProxyID)
            return false;
        Optional<WebCore::PageIdentifier> webPageID;
        decoder >> webPageID;
        if (!webPageID)
            return false;

        result = adoptNS([[WKBrowsingContextHandle alloc] _initWithPageProxyID:*pageProxyID andWebPageID:*webPageID]);
        return true;
    }

    case ObjCType::WKTypeRefWrapper: {
        RefPtr<API::Object> object;
        if (!UserData::decode(decoder, object))
            return false;

        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        result = adoptNS([[WKTypeRefWrapper alloc] initWithObject:toAPI(object.get())]);
        ALLOW_DEPRECATED_DECLARATIONS_END
        return true;
    }
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool ObjCObjectGraph::decode(IPC::Decoder& decoder, RefPtr<API::Object>& result)
{
    RetainPtr<id> rootObject;
    if (!decode(decoder, rootObject))
        return false;

    result = ObjCObjectGraph::create(rootObject.get());
    return true;
}

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::ObjCType> {
    using values = EnumValues<
        WebKit::ObjCType,
        WebKit::ObjCType::Null,
        WebKit::ObjCType::NSArray,
        WebKit::ObjCType::NSData,
        WebKit::ObjCType::NSDate,
        WebKit::ObjCType::NSDictionary,
        WebKit::ObjCType::NSNumber,
        WebKit::ObjCType::NSString,
        WebKit::ObjCType::WKBrowsingContextHandle,
        WebKit::ObjCType::WKTypeRefWrapper
    >;
};

} // namespace WTF
