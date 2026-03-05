/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GeneratedWebKitSecureCoding.h"

#include "ArgumentCodersCocoa.h"
#if USE(AVFOUNDATION)
#include <pal/cocoa/AVFoundationSoftLink.h>
#endif
#if ENABLE(DATA_DETECTION)
#include <pal/cocoa/DataDetectorsCoreSoftLink.h>
#endif

namespace WebKit {

static RetainPtr<NSDictionary> dictionaryForWebKitSecureCodingTypeFromWKKeyedCoder(id object)
{
    auto archiver = adoptNS([WKKeyedCoder new]);
    [object encodeWithCoder:archiver.get()];
    return [archiver accumulatedDictionary];
}

[[maybe_unused]] static RetainPtr<NSDictionary> dictionaryForWebKitSecureCodingType(id object)
{
    if (WebKit::conformsToWebKitSecureCoding(object))
        return [object _webKitPropertyListData];

    return dictionaryForWebKitSecureCodingTypeFromWKKeyedCoder(object);
}

template<typename T> static RetainPtr<NSDictionary> dictionaryFromVector(const Vector<std::pair<String, RetainPtr<T>>>& vector)
{
    NSMutableDictionary *dictionary = [NSMutableDictionary dictionaryWithCapacity:vector.size()];
    for (auto& pair : vector)
        dictionary[pair.first] = pair.second;
    return dictionary;
}

template<typename T> static RetainPtr<NSDictionary> dictionaryFromOptionalVector(const std::optional<Vector<std::pair<String, RetainPtr<T>>>>& vector)
{
    if (!vector)
        return nil;
    return dictionaryFromVector<T>(*vector);
}

template<typename T> static Vector<std::pair<String, RetainPtr<T>>> vectorFromDictionary(NSDictionary *dictionary)
{
    if (![dictionary isKindOfClass:NSDictionary.class])
        return { };
    __block Vector<std::pair<String, RetainPtr<T>>> result;
    [dictionary enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL*){
        if ([key isKindOfClass:NSString.class] && [value isKindOfClass:IPC::getClass<T>()])
            result.append((NSString *)key, (T)value);
    }];
    return result;
}

template<typename T> static std::optional<Vector<std::pair<String, RetainPtr<T>>>> optionalVectorFromDictionary(NSDictionary *dictionary)
{
    if (![dictionary isKindOfClass:NSDictionary.class])
        return std::nullopt;
    return vectorFromDictionary<T>(dictionary);
}

template<typename T> static RetainPtr<NSArray> arrayFromVector(const Vector<RetainPtr<T>>& vector)
{
    return createNSArray(vector, [] (auto& t) {
        return t.get();
    });
}

template<typename T> static RetainPtr<NSArray> arrayFromOptionalVector(const std::optional<Vector<RetainPtr<T>>>& vector)
{
    if (!vector)
        return nil;
    return arrayFromVector<T>(*vector);
}

template<typename T> static Vector<RetainPtr<T>> vectorFromArray(NSArray *array)
{
    if (![array isKindOfClass:NSArray.class])
        return { };
    Vector<RetainPtr<T>> result;
    for (id element in array) {
        SUPPRESS_UNRETAINED_ARG if ([element isKindOfClass:retainPtr(IPC::getClass<T>()).get()])
            result.append((T *)element);
    }
    return result;
}

template<typename T> static std::optional<Vector<RetainPtr<T>>> optionalVectorFromArray(NSArray *array)
{
    if (![array isKindOfClass:NSArray.class])
        return std::nullopt;
    return vectorFromArray<T>(array);
}

#if USE(AVFOUNDATION)
CoreIPCAVOutputContext::CoreIPCAVOutputContext(
    RetainPtr<NSString>&& AVOutputContextSerializationKeyContextID,
    RetainPtr<NSString>&& AVOutputContextSerializationKeyContextType
)
    : m_AVOutputContextSerializationKeyContextID(WTF::move(AVOutputContextSerializationKeyContextID))
    , m_AVOutputContextSerializationKeyContextType(WTF::move(AVOutputContextSerializationKeyContextType))
{
}

CoreIPCAVOutputContext::CoreIPCAVOutputContext(AVOutputContext *object)
{
    auto dictionary = dictionaryForWebKitSecureCodingType(object);
    m_AVOutputContextSerializationKeyContextID = (NSString *)[dictionary objectForKey:@"AVOutputContextSerializationKeyContextID"];
    SUPPRESS_UNRETAINED_ARG if (![m_AVOutputContextSerializationKeyContextID isKindOfClass:IPC::getClass<NSString>()])
        m_AVOutputContextSerializationKeyContextID = nullptr;

    m_AVOutputContextSerializationKeyContextType = (NSString *)[dictionary objectForKey:@"AVOutputContextSerializationKeyContextType"];
    SUPPRESS_UNRETAINED_ARG if (![m_AVOutputContextSerializationKeyContextType isKindOfClass:IPC::getClass<NSString>()])
        m_AVOutputContextSerializationKeyContextType = nullptr;

}

RetainPtr<id> CoreIPCAVOutputContext::toID() const
{
    auto propertyList = [NSMutableDictionary dictionaryWithCapacity:2];
    if (m_AVOutputContextSerializationKeyContextID)
        propertyList[@"AVOutputContextSerializationKeyContextID"] = m_AVOutputContextSerializationKeyContextID.get();
    if (m_AVOutputContextSerializationKeyContextType)
        propertyList[@"AVOutputContextSerializationKeyContextType"] = m_AVOutputContextSerializationKeyContextType.get();
    if ([PAL::getAVOutputContextClassSingleton() instancesRespondToSelector:@selector(_initWithWebKitPropertyListData:)])
        return adoptNS([[PAL::getAVOutputContextClassSingleton() alloc] _initWithWebKitPropertyListData:propertyList]);

    auto unarchiver = adoptNS([[WKKeyedCoder alloc] initWithDictionary:propertyList]);
    return adoptNS([[PAL::getAVOutputContextClassSingleton() alloc] initWithCoder:unarchiver.get()]);
}
#endif // USE(AVFOUNDATION)

CoreIPCNSSomeFoundationType::CoreIPCNSSomeFoundationType(
    RetainPtr<NSString>&& StringKey,
    RetainPtr<NSNumber>&& NumberKey,
    RetainPtr<NSNumber>&& OptionalNumberKey,
    RetainPtr<NSArray>&& ArrayKey,
    RetainPtr<NSArray>&& OptionalArrayKey,
    RetainPtr<NSDictionary>&& DictionaryKey,
    RetainPtr<NSDictionary>&& OptionalDictionaryKey
)
    : m_StringKey(WTF::move(StringKey))
    , m_NumberKey(WTF::move(NumberKey))
    , m_OptionalNumberKey(WTF::move(OptionalNumberKey))
    , m_ArrayKey(WTF::move(ArrayKey))
    , m_OptionalArrayKey(WTF::move(OptionalArrayKey))
    , m_DictionaryKey(WTF::move(DictionaryKey))
    , m_OptionalDictionaryKey(WTF::move(OptionalDictionaryKey))
{
}

CoreIPCNSSomeFoundationType::CoreIPCNSSomeFoundationType(NSSomeFoundationType *object)
{
    auto dictionary = dictionaryForWebKitSecureCodingType(object);
    m_StringKey = (NSString *)[dictionary objectForKey:@"StringKey"];
    SUPPRESS_UNRETAINED_ARG if (![m_StringKey isKindOfClass:IPC::getClass<NSString>()])
        m_StringKey = nullptr;

    m_NumberKey = (NSNumber *)[dictionary objectForKey:@"NumberKey"];
    SUPPRESS_UNRETAINED_ARG if (![m_NumberKey isKindOfClass:IPC::getClass<NSNumber>()])
        m_NumberKey = nullptr;

    m_OptionalNumberKey = (NSNumber *)[dictionary objectForKey:@"OptionalNumberKey"];
    SUPPRESS_UNRETAINED_ARG if (![m_OptionalNumberKey isKindOfClass:IPC::getClass<NSNumber>()])
        m_OptionalNumberKey = nullptr;

    m_ArrayKey = (NSArray *)[dictionary objectForKey:@"ArrayKey"];
    SUPPRESS_UNRETAINED_ARG if (![m_ArrayKey isKindOfClass:IPC::getClass<NSArray>()])
        m_ArrayKey = nullptr;

    m_OptionalArrayKey = (NSArray *)[dictionary objectForKey:@"OptionalArrayKey"];
    SUPPRESS_UNRETAINED_ARG if (![m_OptionalArrayKey isKindOfClass:IPC::getClass<NSArray>()])
        m_OptionalArrayKey = nullptr;

    m_DictionaryKey = (NSDictionary *)[dictionary objectForKey:@"DictionaryKey"];
    SUPPRESS_UNRETAINED_ARG if (![m_DictionaryKey isKindOfClass:IPC::getClass<NSDictionary>()])
        m_DictionaryKey = nullptr;

    m_OptionalDictionaryKey = (NSDictionary *)[dictionary objectForKey:@"OptionalDictionaryKey"];
    SUPPRESS_UNRETAINED_ARG if (![m_OptionalDictionaryKey isKindOfClass:IPC::getClass<NSDictionary>()])
        m_OptionalDictionaryKey = nullptr;

}

RetainPtr<id> CoreIPCNSSomeFoundationType::toID() const
{
    auto propertyList = [NSMutableDictionary dictionaryWithCapacity:7];
    if (m_StringKey)
        propertyList[@"StringKey"] = m_StringKey.get();
    if (m_NumberKey)
        propertyList[@"NumberKey"] = m_NumberKey.get();
    if (m_OptionalNumberKey)
        propertyList[@"OptionalNumberKey"] = m_OptionalNumberKey.get();
    if (m_ArrayKey)
        propertyList[@"ArrayKey"] = m_ArrayKey.get();
    if (m_OptionalArrayKey)
        propertyList[@"OptionalArrayKey"] = m_OptionalArrayKey.get();
    if (m_DictionaryKey)
        propertyList[@"DictionaryKey"] = m_DictionaryKey.get();
    if (m_OptionalDictionaryKey)
        propertyList[@"OptionalDictionaryKey"] = m_OptionalDictionaryKey.get();
    RELEASE_ASSERT([NSSomeFoundationType instancesRespondToSelector:@selector(_initWithWebKitPropertyListData:)]);
    if ([NSSomeFoundationType instancesRespondToSelector:@selector(_initWithWebKitPropertyListData:)])
        return adoptNS([[NSSomeFoundationType alloc] _initWithWebKitPropertyListData:propertyList]);

    auto unarchiver = adoptNS([[WKKeyedCoder alloc] initWithDictionary:propertyList]);
    return adoptNS([[NSSomeFoundationType alloc] initWithCoder:unarchiver.get()]);
}

CoreIPCclass NSSomeOtherFoundationType::CoreIPCclass NSSomeOtherFoundationType(
    RetainPtr<NSDictionary>&& DictionaryKey
)
    : m_DictionaryKey(WTF::move(DictionaryKey))
{
}

CoreIPCclass NSSomeOtherFoundationType::CoreIPCclass NSSomeOtherFoundationType(class NSSomeOtherFoundationType *object)
{
    auto dictionary = dictionaryForWebKitSecureCodingTypeFromWKKeyedCoder(object);
    m_DictionaryKey = (NSDictionary *)[dictionary objectForKey:@"DictionaryKey"];
    SUPPRESS_UNRETAINED_ARG if (![m_DictionaryKey isKindOfClass:IPC::getClass<NSDictionary>()])
        m_DictionaryKey = nullptr;

}

RetainPtr<id> CoreIPCclass NSSomeOtherFoundationType::toID() const
{
    auto propertyList = [NSMutableDictionary dictionaryWithCapacity:1];
    if (m_DictionaryKey)
        propertyList[@"DictionaryKey"] = m_DictionaryKey.get();

    auto unarchiver = adoptNS([[WKKeyedCoder alloc] initWithDictionary:propertyList]);
    return adoptNS([[class NSSomeOtherFoundationType alloc] initWithCoder:unarchiver.get()]);
}

#if ENABLE(DATA_DETECTION)
CoreIPCDDScannerResult::CoreIPCDDScannerResult(
    RetainPtr<NSString>&& StringKey,
    RetainPtr<NSNumber>&& NumberKey,
    RetainPtr<NSNumber>&& OptionalNumberKey,
    Vector<RetainPtr<DDScannerResult>>&& ArrayKey,
    std::optional<Vector<RetainPtr<DDScannerResult>>>&& OptionalArrayKey,
    Vector<std::pair<String, RetainPtr<Number>>>&& DictionaryKey,
    std::optional<Vector<std::pair<String, RetainPtr<DDScannerResult>>>>&& OptionalDictionaryKey,
    Vector<RetainPtr<NSData>>&& DataArrayKey,
    Vector<RetainPtr<SecTrustRef>>&& SecTrustArrayKey
)
    : m_StringKey(WTF::move(StringKey))
    , m_NumberKey(WTF::move(NumberKey))
    , m_OptionalNumberKey(WTF::move(OptionalNumberKey))
    , m_ArrayKey(WTF::move(ArrayKey))
    , m_OptionalArrayKey(WTF::move(OptionalArrayKey))
    , m_DictionaryKey(WTF::move(DictionaryKey))
    , m_OptionalDictionaryKey(WTF::move(OptionalDictionaryKey))
    , m_DataArrayKey(WTF::move(DataArrayKey))
    , m_SecTrustArrayKey(WTF::move(SecTrustArrayKey))
{
}

CoreIPCDDScannerResult::CoreIPCDDScannerResult(DDScannerResult *object)
{
    auto dictionary = dictionaryForWebKitSecureCodingType(object);
    m_StringKey = (NSString *)[dictionary objectForKey:@"StringKey"];
    SUPPRESS_UNRETAINED_ARG if (![m_StringKey isKindOfClass:IPC::getClass<NSString>()])
        m_StringKey = nullptr;

    m_NumberKey = (NSNumber *)[dictionary objectForKey:@"NumberKey"];
    SUPPRESS_UNRETAINED_ARG if (![m_NumberKey isKindOfClass:IPC::getClass<NSNumber>()])
        m_NumberKey = nullptr;

    m_OptionalNumberKey = (NSNumber *)[dictionary objectForKey:@"OptionalNumberKey"];
    SUPPRESS_UNRETAINED_ARG if (![m_OptionalNumberKey isKindOfClass:IPC::getClass<NSNumber>()])
        m_OptionalNumberKey = nullptr;

    m_ArrayKey = vectorFromArray<DDScannerResult>((NSArray *)retainPtr([dictionary objectForKey:@"ArrayKey"]).get());
    m_OptionalArrayKey = optionalVectorFromArray<DDScannerResult>((NSArray *)retainPtr([dictionary objectForKey:@"OptionalArrayKey"]).get());
    m_DictionaryKey = vectorFromDictionary<Number>((NSDictionary *)retainPtr([dictionary objectForKey:@"DictionaryKey"]).get());
    m_OptionalDictionaryKey = optionalVectorFromDictionary<DDScannerResult>((NSDictionary *)retainPtr([dictionary objectForKey:@"OptionalDictionaryKey"]).get());
    m_DataArrayKey = vectorFromArray<NSData>((NSArray *)retainPtr([dictionary objectForKey:@"DataArrayKey"]).get());
    m_SecTrustArrayKey = vectorFromArray<SecTrustRef>((NSArray *)retainPtr([dictionary objectForKey:@"SecTrustArrayKey"]).get());
}

RetainPtr<id> CoreIPCDDScannerResult::toID() const
{
    auto propertyList = [NSMutableDictionary dictionaryWithCapacity:9];
    if (m_StringKey)
        propertyList[@"StringKey"] = m_StringKey.get();
    if (m_NumberKey)
        propertyList[@"NumberKey"] = m_NumberKey.get();
    if (m_OptionalNumberKey)
        propertyList[@"OptionalNumberKey"] = m_OptionalNumberKey.get();
    propertyList[@"ArrayKey"] = arrayFromVector(m_ArrayKey).get();
    if (auto array = arrayFromOptionalVector(m_OptionalArrayKey))
        propertyList[@"OptionalArrayKey"] = array.get();
    propertyList[@"DictionaryKey"] = dictionaryFromVector(m_DictionaryKey).get();
    if (auto dictionary = dictionaryFromOptionalVector(m_OptionalDictionaryKey))
        propertyList[@"OptionalDictionaryKey"] = dictionary.get();
    propertyList[@"DataArrayKey"] = arrayFromVector(m_DataArrayKey).get();
    propertyList[@"SecTrustArrayKey"] = arrayFromVector(m_SecTrustArrayKey).get();
    RELEASE_ASSERT([PAL::getDDScannerResultClassSingleton() instancesRespondToSelector:@selector(_initWithWebKitPropertyListData:)]);
    if ([PAL::getDDScannerResultClassSingleton() instancesRespondToSelector:@selector(_initWithWebKitPropertyListData:)])
        return adoptNS([[PAL::getDDScannerResultClassSingleton() alloc] _initWithWebKitPropertyListData:propertyList]);

    auto unarchiver = adoptNS([[WKKeyedCoder alloc] initWithDictionary:propertyList]);
    return adoptNS([[PAL::getDDScannerResultClassSingleton() alloc] initWithCoder:unarchiver.get()]);
}
#endif // ENABLE(DATA_DETECTION)

} // namespace WebKit
