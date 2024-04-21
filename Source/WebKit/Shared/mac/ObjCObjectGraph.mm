/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
#import <wtf/EnumTraits.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

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
};

static std::optional<ObjCType> typeFromObject(id object)
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

    return std::nullopt;
}

void ObjCObjectGraph::encode(IPC::Encoder& encoder, id object)
{
    if (!object) {
        encoder << static_cast<uint32_t>(ObjCType::Null);
        return;
    }

    auto type = typeFromObject(object);
    if (!type)
        [NSException raise:NSInvalidArgumentException format:@"Can not encode objects of class type '%@'", checked_objc_cast<NSString>(NSStringFromClass([object class]))];

    encoder << *type;

    switch (type.value()) {
    case ObjCType::Null:
        return;

    case ObjCType::NSArray: {
        IPC::encode(encoder, checked_objc_cast<NSArray>(object));
        return;
    }

    case ObjCType::NSData: {
        IPC::encode(encoder, checked_objc_cast<NSData>(object));
        return;
    }

    case ObjCType::NSDate: {
        IPC::encode(encoder, checked_objc_cast<NSDate>(object));
        return;
    }

    case ObjCType::NSDictionary: {
        IPC::encode(encoder, checked_objc_cast<NSDictionary>(object));
        return;
    }

    case ObjCType::NSNumber: {
        IPC::encode(encoder, checked_objc_cast<NSNumber>(object));
        return;
    }

    case ObjCType::NSString: {
        IPC::encode(encoder, checked_objc_cast<NSString>(object));
        return;
    }

    case ObjCType::WKBrowsingContextHandle: {
        encoder << checked_objc_cast<WKBrowsingContextHandle>(object).pageProxyID;
        encoder << checked_objc_cast<WKBrowsingContextHandle>(object).webPageID;
        return;
    }
    }

    ASSERT_NOT_REACHED();
}

bool ObjCObjectGraph::decode(IPC::Decoder& decoder, RetainPtr<id>& result)
{
    auto type = decoder.decode<ObjCType>();
    if (!type)
        return false;

    switch (*type) {
    case ObjCType::Null: {
        result = nil;
        return true;
    }

    case ObjCType::NSArray: {
        auto array = decoder.decode<RetainPtr<NSArray>>();
        if (!array)
            return false;

        result = WTFMove(*array);
        return true;
    }

    case ObjCType::NSData: {
        auto data = decoder.decode<RetainPtr<NSData>>();
        if (!data)
            return false;

        result = WTFMove(*data);
        return true;
    }

    case ObjCType::NSDate: {
        auto date = decoder.decode<RetainPtr<NSDate>>();
        if (!date)
            return false;

        result = WTFMove(*date);
        return true;
    }

    case ObjCType::NSDictionary: {
        auto dictionary = decoder.decode<RetainPtr<NSDictionary>>();
        if (!dictionary)
            return false;

        result = WTFMove(*dictionary);
        return true;
    }

    case ObjCType::NSNumber: {
        auto number = decoder.decode<RetainPtr<NSNumber>>();
        if (!number)
            return false;

        result = WTFMove(*number);
        return true;
    }

    case ObjCType::NSString: {
        auto string = decoder.decode<RetainPtr<NSString>>();
        if (!string)
            return false;

        result = WTFMove(*string);
        return true;
    }

    case ObjCType::WKBrowsingContextHandle: {
        std::optional<WebPageProxyIdentifier> pageProxyID;
        decoder >> pageProxyID;
        if (!pageProxyID)
            return false;
        std::optional<WebCore::PageIdentifier> webPageID;
        decoder >> webPageID;
        if (!webPageID)
            return false;

        result = adoptNS([[WKBrowsingContextHandle alloc] _initWithPageProxyID:*pageProxyID andWebPageID:*webPageID]);
        return true;
    }
    }

    ASSERT_NOT_REACHED();
    return false;
}

} // namespace WebKit

namespace IPC {

std::optional<Ref<WebKit::ObjCObjectGraph>> ArgumentCoder<WebKit::ObjCObjectGraph>::decode(IPC::Decoder& decoder)
{
    RetainPtr<id> rootObject;
    if (!WebKit::ObjCObjectGraph::decode(decoder, rootObject))
        return std::nullopt;

    return WebKit::ObjCObjectGraph::create(rootObject.get());
}

void ArgumentCoder<WebKit::ObjCObjectGraph>::encode(IPC::Encoder& encoder, const WebKit::ObjCObjectGraph& object)
{
    WebKit::ObjCObjectGraph::encode(encoder, object.rootObject());
}

} // namespace IPC

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
        WebKit::ObjCType::WKBrowsingContextHandle
    >;
};

} // namespace WTF
