/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

// FIXME: This is a .cpp but has ObjC in it?

#include "config.h"
#include "ArgumentCodersCF.h"

#include "ArgumentCoders.h"
#include "DataReference.h"
#include "Decoder.h"
#include "Encoder.h"
#include "StreamConnectionEncoder.h"
#include <CoreGraphics/CoreGraphics.h>
#include <WebCore/Color.h>
#include <wtf/EnumTraits.h>
#include <wtf/HashSet.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/Vector.h>
#include <wtf/cf/CFURLExtras.h>
#include <wtf/spi/cocoa/SecuritySPI.h>

#if USE(FOUNDATION)
#import <Foundation/Foundation.h>
#endif

namespace IPC {
using namespace WebCore;

CFTypeRef tokenNullptrTypeRef()
{
    static CFStringRef tokenNullptrType = CFSTR("WKNull");
    return tokenNullptrType;
}

enum class CFType : uint8_t {
    CFArray,
    CFBoolean,
    CFCharacterSet,
    CFData,
    CFDate,
    CFDictionary,
    CFNull,
    CFNumber,
    CFString,
    CFURL,
    SecCertificate,
#if HAVE(SEC_KEYCHAIN)
    SecKeychainItem,
#endif
#if HAVE(SEC_ACCESS_CONTROL)
    SecAccessControl,
#endif
    SecTrust,
    CGColorSpace,
    CGColor,
    Nullptr,
    Unknown,
};

static CFType typeFromCFTypeRef(CFTypeRef type)
{
    ASSERT(type);

    if (type == tokenNullptrTypeRef())
        return CFType::Nullptr;

    CFTypeID typeID = CFGetTypeID(type);
    if (typeID == CFArrayGetTypeID())
        return CFType::CFArray;
    if (typeID == CFBooleanGetTypeID())
        return CFType::CFBoolean;
    if (typeID == CFCharacterSetGetTypeID())
        return CFType::CFCharacterSet;
    if (typeID == CFDataGetTypeID())
        return CFType::CFData;
    if (typeID == CFDateGetTypeID())
        return CFType::CFDate;
    if (typeID == CFDictionaryGetTypeID())
        return CFType::CFDictionary;
    if (typeID == CFNullGetTypeID())
        return CFType::CFNull;
    if (typeID == CFNumberGetTypeID())
        return CFType::CFNumber;
    if (typeID == CFStringGetTypeID())
        return CFType::CFString;
    if (typeID == CFURLGetTypeID())
        return CFType::CFURL;
    if (typeID == CGColorSpaceGetTypeID())
        return CFType::CGColorSpace;
    if (typeID == CGColorGetTypeID())
        return CFType::CGColor;
    if (typeID == SecCertificateGetTypeID())
        return CFType::SecCertificate;
#if HAVE(SEC_KEYCHAIN)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (typeID == SecKeychainItemGetTypeID())
        return CFType::SecKeychainItem;
ALLOW_DEPRECATED_DECLARATIONS_END
#endif
#if HAVE(SEC_ACCESS_CONTROL)
    if (typeID == SecAccessControlGetTypeID())
        return CFType::SecAccessControl;
#endif
    if (typeID == SecTrustGetTypeID())
        return CFType::SecTrust;

    // If you're hitting this, it probably means that you've put an NS type inside a CF container.
    // Try round-tripping the container through an NS type instead.
    ASSERT_NOT_REACHED();
    return CFType::Unknown;
}


template<typename Encoder>
void ArgumentCoder<CFTypeRef>::encode(Encoder& encoder, CFTypeRef typeRef)
{
    auto type = typeFromCFTypeRef(typeRef);
    encoder << type;

    switch (type) {
    case CFType::CFArray:
        encoder << static_cast<CFArrayRef>(typeRef);
        return;
    case CFType::CFBoolean:
        encoder << static_cast<CFBooleanRef>(typeRef);
        return;
    case CFType::CFCharacterSet:
        encoder << static_cast<CFCharacterSetRef>(typeRef);
        return;
    case CFType::CFData:
        encoder << static_cast<CFDataRef>(typeRef);
        return;
    case CFType::CFDate:
        encoder << static_cast<CFDateRef>(typeRef);
        return;
    case CFType::CFDictionary:
        encoder << static_cast<CFDictionaryRef>(typeRef);
        return;
    case CFType::CFNull:
        return;
    case CFType::CFNumber:
        encoder << static_cast<CFNumberRef>(typeRef);
        return;
    case CFType::CFString:
        encoder << static_cast<CFStringRef>(typeRef);
        return;
    case CFType::CFURL:
        encoder << static_cast<CFURLRef>(typeRef);
        return;
    case CFType::CGColorSpace:
        encoder << static_cast<CGColorSpaceRef>(const_cast<void*>(typeRef));
        return;
    case CFType::CGColor:
        encoder << static_cast<CGColorRef>(const_cast<void*>(typeRef));
        return;
    case CFType::SecCertificate:
        encoder << static_cast<SecCertificateRef>(const_cast<void*>(typeRef));
        return;
#if HAVE(SEC_KEYCHAIN)
    case CFType::SecKeychainItem:
        encoder << static_cast<SecKeychainItemRef>(const_cast<void*>(typeRef));
        return;
#endif
#if HAVE(SEC_ACCESS_CONTROL)
    case CFType::SecAccessControl:
        encoder << static_cast<SecAccessControlRef>(const_cast<void*>(typeRef));
        return;
#endif
    case CFType::SecTrust:
        encoder << static_cast<SecTrustRef>(const_cast<void*>(typeRef));
        return;
    case CFType::Nullptr:
        return;
    case CFType::Unknown:
        break;
    }

    ASSERT_NOT_REACHED();
}

template void ArgumentCoder<CFTypeRef>::encode<Encoder>(Encoder&, CFTypeRef);
template void ArgumentCoder<CFTypeRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFTypeRef);

std::optional<RetainPtr<CFTypeRef>> ArgumentCoder<RetainPtr<CFTypeRef>>::decode(Decoder& decoder)
{
    std::optional<CFType> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    switch (*type) {
    case CFType::CFArray: {
        std::optional<RetainPtr<CFArrayRef>> array;
        decoder >> array;
        if (!array)
            return std::nullopt;
        return WTFMove(*array);
    }
    case CFType::CFBoolean: {
        std::optional<RetainPtr<CFBooleanRef>> boolean;
        decoder >> boolean;
        if (!boolean)
            return std::nullopt;
        return WTFMove(*boolean);
    }
    case CFType::CFCharacterSet: {
        std::optional<RetainPtr<CFCharacterSetRef>> characterSet;
        decoder >> characterSet;
        if (!characterSet)
            return std::nullopt;
        return WTFMove(*characterSet);
    }
    case CFType::CFData: {
        std::optional<RetainPtr<CFDataRef>> data;
        decoder >> data;
        if (!data)
            return std::nullopt;
        return WTFMove(*data);
    }
    case CFType::CFDate: {
        std::optional<RetainPtr<CFDateRef>> date;
        decoder >> date;
        if (!date)
            return std::nullopt;
        return WTFMove(*date);
    }
    case CFType::CFDictionary: {
        std::optional<RetainPtr<CFDictionaryRef>> dictionary;
        decoder >> dictionary;
        if (!dictionary)
            return std::nullopt;
        return WTFMove(*dictionary);
    }
    case CFType::CFNull:
        return RetainPtr<CFNullRef>(kCFNull);
    case CFType::CFNumber: {
        std::optional<RetainPtr<CFNumberRef>> number;
        decoder >> number;
        if (!number)
            return std::nullopt;
        return WTFMove(*number);
    }
    case CFType::CFString: {
        std::optional<RetainPtr<CFStringRef>> string;
        decoder >> string;
        if (!string)
            return std::nullopt;
        return WTFMove(*string);
    }
    case CFType::CFURL: {
        std::optional<RetainPtr<CFURLRef>> url;
        decoder >> url;
        if (!url)
            return std::nullopt;
        return WTFMove(*url);
    }
    case CFType::CGColorSpace: {
        std::optional<RetainPtr<CGColorSpaceRef>> colorSpace;
        decoder >> colorSpace;
        if (!colorSpace)
            return std::nullopt;
        return WTFMove(*colorSpace);
    }
    case CFType::CGColor: {
        std::optional<RetainPtr<CGColorRef>> color;
        decoder >> color;
        if (!color)
            return std::nullopt;
        return WTFMove(*color);
    }
    case CFType::SecCertificate: {
        std::optional<RetainPtr<SecCertificateRef>> certificate;
        decoder >> certificate;
        if (!certificate)
            return std::nullopt;
        return WTFMove(*certificate);
    }
#if HAVE(SEC_KEYCHAIN)
    case CFType::SecKeychainItem: {
        std::optional<RetainPtr<SecKeychainItemRef>> keychainItem;
        decoder >> keychainItem;
        if (!keychainItem)
            return std::nullopt;
        return WTFMove(*keychainItem);
    }
#endif
#if HAVE(SEC_ACCESS_CONTROL)
    case CFType::SecAccessControl: {
        std::optional<RetainPtr<SecAccessControlRef>> accessControl;
        decoder >> accessControl;
        if (!accessControl)
            return std::nullopt;
        return WTFMove(*accessControl);
    }
#endif
    case CFType::SecTrust: {
        std::optional<RetainPtr<SecTrustRef>> trust;
        decoder >> trust;
        if (!trust)
            return std::nullopt;
        return WTFMove(*trust);
    }
    case CFType::Nullptr:
        return tokenNullptrTypeRef();
    case CFType::Unknown:
        break;
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

template<typename Encoder>
void ArgumentCoder<CFArrayRef>::encode(Encoder& encoder, CFArrayRef array)
{
    if (!array) {
        encoder << true;
        return;
    }

    encoder << false;

    CFIndex size = CFArrayGetCount(array);
    Vector<CFTypeRef, 32> values(size);

    CFArrayGetValues(array, CFRangeMake(0, size), values.data());

    HashSet<CFIndex> invalidIndices;
    for (CFIndex i = 0; i < size; ++i) {
        // Ignore values we don't support.
        ASSERT(typeFromCFTypeRef(values[i]) != CFType::Unknown);
        if (typeFromCFTypeRef(values[i]) == CFType::Unknown)
            invalidIndices.add(i);
    }

    encoder << static_cast<uint64_t>(size - invalidIndices.size());

    for (CFIndex i = 0; i < size; ++i) {
        if (invalidIndices.contains(i))
            continue;
        encoder << values[i];
    }
}

template void ArgumentCoder<CFArrayRef>::encode<Encoder>(Encoder&, CFArrayRef);
template void ArgumentCoder<CFArrayRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFArrayRef);

std::optional<RetainPtr<CFArrayRef>> ArgumentCoder<RetainPtr<CFArrayRef>>::decode(Decoder& decoder)
{
    std::optional<bool> isNull;
    decoder >> isNull;
    if (!isNull)
        return std::nullopt;

    if (*isNull)
        return {{ nullptr }};

    std::optional<uint64_t> size;
    decoder >> size;
    if (!size)
        return std::nullopt;

    auto array = adoptCF(CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));

    for (size_t i = 0; i < *size; ++i) {
        std::optional<RetainPtr<CFTypeRef>> element;
        decoder >> element;
        if (!element)
            return std::nullopt;

        if (!*element)
            continue;
        
        CFArrayAppendValue(array.get(), element->get());
    }

    return WTFMove(array);
}

template<typename Encoder>
void ArgumentCoder<CFBooleanRef>::encode(Encoder& encoder, CFBooleanRef boolean)
{
    encoder << static_cast<bool>(CFBooleanGetValue(boolean));
}

template void ArgumentCoder<CFBooleanRef>::encode<Encoder>(Encoder&, CFBooleanRef);
template void ArgumentCoder<CFBooleanRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFBooleanRef);

std::optional<RetainPtr<CFBooleanRef>> ArgumentCoder<RetainPtr<CFBooleanRef>>::decode(Decoder& decoder)
{
    std::optional<bool> boolean;
    decoder >> boolean;
    if (!boolean)
        return std::nullopt;

    return adoptCF(*boolean ? kCFBooleanTrue : kCFBooleanFalse);
}

template<typename Encoder>
void ArgumentCoder<CFCharacterSetRef>::encode(Encoder& encoder, CFCharacterSetRef characterSet)
{
    auto data = adoptCF(CFCharacterSetCreateBitmapRepresentation(nullptr, characterSet));
    if (!data) {
        encoder << false;
        return;
    }

    encoder << true << data;
}

template void ArgumentCoder<CFCharacterSetRef>::encode<Encoder>(Encoder&, CFCharacterSetRef);
template void ArgumentCoder<CFCharacterSetRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFCharacterSetRef);

std::optional<RetainPtr<CFCharacterSetRef>> ArgumentCoder<RetainPtr<CFCharacterSetRef>>::decode(Decoder& decoder)
{
    std::optional<bool> hasData;
    decoder >> hasData;
    if (!hasData)
        return std::nullopt;

    if (!*hasData)
        return { nullptr };

    std::optional<RetainPtr<CFDataRef>> data;
    decoder >> data;
    if (!data)
        return std::nullopt;

    auto characterSet = adoptCF(CFCharacterSetCreateWithBitmapRepresentation(nullptr, data->get()));
    if (!characterSet)
        return std::nullopt;

    return WTFMove(characterSet);
}

template<typename Encoder>
void ArgumentCoder<CFDataRef>::encode(Encoder& encoder, CFDataRef data)
{
    encoder << IPC::DataReference(CFDataGetBytePtr(data), CFDataGetLength(data));
}

template void ArgumentCoder<CFDataRef>::encode<Encoder>(Encoder&, CFDataRef);
template void ArgumentCoder<CFDataRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFDataRef);

template<typename Decoder>
std::optional<RetainPtr<CFDataRef>> ArgumentCoder<RetainPtr<CFDataRef>>::decode(Decoder& decoder)
{
    std::optional<IPC::DataReference> dataReference;
    decoder >> dataReference;
    if (!dataReference)
        return std::nullopt;

    return adoptCF(CFDataCreate(0, dataReference->data(), dataReference->size()));
}

template std::optional<RetainPtr<CFDataRef>> ArgumentCoder<RetainPtr<CFDataRef>>::decode<Decoder>(Decoder&);

template<typename Encoder>
void ArgumentCoder<CFDateRef>::encode(Encoder& encoder, CFDateRef date)
{
    encoder << static_cast<double>(CFDateGetAbsoluteTime(date));
}

template void ArgumentCoder<CFDateRef>::encode<Encoder>(Encoder&, CFDateRef);
template void ArgumentCoder<CFDateRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFDateRef);

std::optional<RetainPtr<CFDateRef>> ArgumentCoder<RetainPtr<CFDateRef>>::decode(Decoder& decoder)
{
    std::optional<double> absoluteTime;
    decoder >> absoluteTime;
    if (!absoluteTime)
        return std::nullopt;

    return adoptCF(CFDateCreate(0, *absoluteTime));
}

template<typename Encoder>
void ArgumentCoder<CFDictionaryRef>::encode(Encoder& encoder, CFDictionaryRef dictionary)
{
    if (!dictionary) {
        encoder << true;
        return;
    }

    encoder << false;

    CFIndex size = CFDictionaryGetCount(dictionary);
    Vector<CFTypeRef, 32> keys(size);
    Vector<CFTypeRef, 32> values(size);
    
    CFDictionaryGetKeysAndValues(dictionary, keys.data(), values.data());

    HashSet<CFTypeRef> invalidKeys;
    for (CFIndex i = 0; i < size; ++i) {
        ASSERT(keys[i]);
        ASSERT(values[i]);

        // Ignore keys/values we don't support.
        ASSERT(typeFromCFTypeRef(keys[i]) != CFType::Unknown);
        ASSERT(typeFromCFTypeRef(values[i]) != CFType::Unknown);
        if (typeFromCFTypeRef(keys[i]) == CFType::Unknown || typeFromCFTypeRef(values[i]) == CFType::Unknown)
            invalidKeys.add(keys[i]);
    }

    encoder << static_cast<uint64_t>(size - invalidKeys.size());

    for (CFIndex i = 0; i < size; ++i) {
        if (invalidKeys.contains(keys[i]))
            continue;

        encoder << keys[i] << values[i];
    }
}

template void ArgumentCoder<CFDictionaryRef>::encode<Encoder>(Encoder&, CFDictionaryRef);
template void ArgumentCoder<CFDictionaryRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFDictionaryRef);

std::optional<RetainPtr<CFDictionaryRef>> ArgumentCoder<RetainPtr<CFDictionaryRef>>::decode(Decoder& decoder)
{
    std::optional<bool> isNull;
    decoder >> isNull;
    if (!isNull)
        return std::nullopt;

    if (*isNull)
        return {{ nullptr }};

    std::optional<uint64_t> size;
    decoder >> size;
    if (!size)
        return std::nullopt;

    RetainPtr<CFMutableDictionaryRef> dictionary = adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    for (uint64_t i = 0; i < *size; ++i) {
        std::optional<RetainPtr<CFTypeRef>> key;
        decoder >> key;
        if (!key || !*key)
            return std::nullopt;

        std::optional<RetainPtr<CFTypeRef>> value;
        decoder >> value;
        if (!value || !*value)
            return std::nullopt;

        CFDictionarySetValue(dictionary.get(), key->get(), value->get());
    }

    return WTFMove(dictionary);
}

template<typename Encoder>
void ArgumentCoder<CFNumberRef>::encode(Encoder& encoder, CFNumberRef number)
{
    CFNumberType numberType = CFNumberGetType(number);

    Vector<uint8_t> buffer(CFNumberGetByteSize(number));
    bool result = CFNumberGetValue(number, numberType, buffer.data());
    ASSERT_UNUSED(result, result);

    encoder << static_cast<uint8_t>(numberType) << IPC::DataReference(buffer);
}

template void ArgumentCoder<CFNumberRef>::encode<Encoder>(Encoder&, CFNumberRef);
template void ArgumentCoder<CFNumberRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFNumberRef);

static size_t sizeForNumberType(CFNumberType numberType)
{
    switch (numberType) {
    case kCFNumberSInt8Type:
        return sizeof(SInt8);
    case kCFNumberSInt16Type:
        return sizeof(SInt16);
    case kCFNumberSInt32Type:
        return sizeof(SInt32);
    case kCFNumberSInt64Type:
        return sizeof(SInt64);
    case kCFNumberFloat32Type:
        return sizeof(Float32);
    case kCFNumberFloat64Type:
        return sizeof(Float64);
    case kCFNumberCharType:
        return sizeof(char);
    case kCFNumberShortType:
        return sizeof(short);
    case kCFNumberIntType:
        return sizeof(int);
    case kCFNumberLongType:
        return sizeof(long);
    case kCFNumberLongLongType:
        return sizeof(long long);
    case kCFNumberFloatType:
        return sizeof(float);
    case kCFNumberDoubleType:
        return sizeof(double);
    case kCFNumberCFIndexType:
        return sizeof(CFIndex);
    case kCFNumberNSIntegerType:
        return sizeof(long);
    case kCFNumberCGFloatType:
        return sizeof(double);
    }

    return 0;
}

std::optional<RetainPtr<CFNumberRef>> ArgumentCoder<RetainPtr<CFNumberRef>>::decode(Decoder& decoder)
{
    std::optional<uint8_t> numberTypeFromIPC;
    decoder >> numberTypeFromIPC;
    if (!numberTypeFromIPC || *numberTypeFromIPC > kCFNumberMaxType)
        return std::nullopt;
    auto numberType = static_cast<CFNumberType>(*numberTypeFromIPC);

    std::optional<IPC::DataReference> dataReference;
    decoder >> dataReference;
    if (!dataReference)
        return std::nullopt;

    size_t neededBufferSize = sizeForNumberType(numberType);
    if (!neededBufferSize || dataReference->size() != neededBufferSize)
        return std::nullopt;

    ASSERT(dataReference->data());
    return adoptCF(CFNumberCreate(0, numberType, dataReference->data()));
}

template<typename Encoder>
void ArgumentCoder<CFStringRef>::encode(Encoder& encoder, CFStringRef string)
{
    CFIndex length = CFStringGetLength(string);
    CFStringEncoding encoding = CFStringGetFastestEncoding(string);

    CFRange range = CFRangeMake(0, length);
    CFIndex bufferLength = 0;

    CFIndex numConvertedBytes = CFStringGetBytes(string, range, encoding, 0, false, 0, 0, &bufferLength);
    ASSERT_UNUSED(numConvertedBytes, numConvertedBytes == length);

    Vector<UInt8, 128> buffer(bufferLength);
    numConvertedBytes = CFStringGetBytes(string, range, encoding, 0, false, buffer.data(), buffer.size(), &bufferLength);
    ASSERT(numConvertedBytes == length);

    encoder << static_cast<uint32_t>(encoding) << IPC::DataReference(buffer);
}

template void ArgumentCoder<CFStringRef>::encode<Encoder>(Encoder&, CFStringRef);
template void ArgumentCoder<CFStringRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFStringRef);

std::optional<RetainPtr<CFStringRef>> ArgumentCoder<RetainPtr<CFStringRef>>::decode(Decoder& decoder)
{
    std::optional<uint32_t> encodingFromIPC;
    decoder >> encodingFromIPC;
    if (!encodingFromIPC)
        return std::nullopt;
    auto encoding = static_cast<CFStringEncoding>(*encodingFromIPC);

    if (!CFStringIsEncodingAvailable(encoding))
        return std::nullopt;
    
    std::optional<IPC::DataReference> dataReference;
    decoder >> dataReference;
    if (!dataReference)
        return std::nullopt;

    auto string = adoptCF(CFStringCreateWithBytes(0, dataReference->data(), dataReference->size(), encoding, false));
    if (!string)
        return std::nullopt;

    return WTFMove(string);
}

template<typename Encoder>
void ArgumentCoder<CFURLRef>::encode(Encoder& encoder, CFURLRef url)
{
    CFURLRef baseURL = CFURLGetBaseURL(url);
    encoder << static_cast<bool>(baseURL);
    if (baseURL)
        encoder << baseURL;

    encoder << IPC::DataReference(bytesAsVector(url));
}

template void ArgumentCoder<CFURLRef>::encode<Encoder>(Encoder&, CFURLRef);
template void ArgumentCoder<CFURLRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFURLRef);

std::optional<RetainPtr<CFURLRef>> ArgumentCoder<RetainPtr<CFURLRef>>::decode(Decoder& decoder)
{
    RetainPtr<CFURLRef> baseURL;
    std::optional<bool> hasBaseURL;
    decoder >> hasBaseURL;
    if (!hasBaseURL)
        return std::nullopt;
    if (*hasBaseURL) {
        std::optional<RetainPtr<CFURLRef>> decodedBaseURL;
        decoder >> decodedBaseURL;
        if (!decodedBaseURL)
            return std::nullopt;
        baseURL = WTFMove(*decodedBaseURL);
    }

    std::optional<IPC::DataReference> urlBytes;
    decoder >> urlBytes;
    if (!urlBytes)
        return std::nullopt;

#if USE(FOUNDATION)
    // FIXME: Move this to ArgumentCodersCFMac.mm and change this file back to be C++
    // instead of Objective-C++.
    if (urlBytes->empty()) {
        // CFURL can't hold an empty URL, unlike NSURL.
        // FIXME: This discards base URL, which seems incorrect.
        return {{ (__bridge CFURLRef)[NSURL URLWithString:@""] }};
    }
#endif

    auto result = adoptCF(CFURLCreateAbsoluteURLWithBytes(nullptr, reinterpret_cast<const UInt8*>(urlBytes->data()), urlBytes->size(), kCFStringEncodingUTF8, baseURL.get(), true));
    if (!result)
        return std::nullopt;
    return WTFMove(result);
}

enum class CGColorSpaceEncodingScheme : bool { Name, PropertyList };

template<typename Encoder>
void ArgumentCoder<CGColorSpaceRef>::encode(Encoder& encoder, CGColorSpaceRef colorSpace)
{
    if (auto name = CGColorSpaceGetName(colorSpace)) {
        encoder << CGColorSpaceEncodingScheme::Name << name;
        return;
    }

    encoder << CGColorSpaceEncodingScheme::PropertyList << adoptCF(CGColorSpaceCopyPropertyList(colorSpace));
}

template void ArgumentCoder<CGColorSpaceRef>::encode<Encoder>(Encoder&, CGColorSpaceRef);
template void ArgumentCoder<CGColorSpaceRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CGColorSpaceRef);

std::optional<RetainPtr<CGColorSpaceRef>> ArgumentCoder<RetainPtr<CGColorSpaceRef>>::decode(Decoder& decoder)
{
    std::optional<CGColorSpaceEncodingScheme> encodingScheme;
    decoder >> encodingScheme;
    if (!encodingScheme)
        return std::nullopt;

    switch (*encodingScheme) {
    case CGColorSpaceEncodingScheme::Name: {
        std::optional<RetainPtr<CFStringRef>> name;
        decoder >> name;
        if (!name)
            return std::nullopt;

        auto colorSpace = adoptCF(CGColorSpaceCreateWithName(name->get()));
        if (!colorSpace)
            return std::nullopt;

        return WTFMove(colorSpace);
    }
    case CGColorSpaceEncodingScheme::PropertyList: {
        std::optional<RetainPtr<CFTypeRef>> propertyList;
        decoder >> propertyList;
        if (!propertyList)
            return std::nullopt;

        auto colorSpace = adoptCF(CGColorSpaceCreateWithPropertyList(propertyList->get()));
        if (!colorSpace)
            return std::nullopt;

        return WTFMove(colorSpace);
    }
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

template<typename Encoder>
void ArgumentCoder<CGColorRef>::encode(Encoder& encoder, CGColorRef color)
{
    encoder << WebCore::Color::createAndPreserveColorSpace(color);
}

template void ArgumentCoder<CGColorRef>::encode<Encoder>(Encoder&, CGColorRef);
template void ArgumentCoder<CGColorRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CGColorRef);

std::optional<RetainPtr<CGColorRef>> ArgumentCoder<RetainPtr<CGColorRef>>::decode(Decoder& decoder)
{
    std::optional<WebCore::Color> color;
    decoder >> color;
    if (!color)
        return std::nullopt;
    return cachedCGColor(*color);
}

template<typename Encoder>
void ArgumentCoder<SecCertificateRef>::encode(Encoder& encoder, SecCertificateRef certificate)
{
    encoder << adoptCF(SecCertificateCopyData(certificate));
}

template void ArgumentCoder<SecCertificateRef>::encode<Encoder>(Encoder&, SecCertificateRef);
template void ArgumentCoder<SecCertificateRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, SecCertificateRef);

std::optional<RetainPtr<SecCertificateRef>> ArgumentCoder<RetainPtr<SecCertificateRef>>::decode(Decoder& decoder)
{
    std::optional<RetainPtr<CFDataRef>> data;
    decoder >> data;
    if (!data)
        return std::nullopt;

    return adoptCF(SecCertificateCreateWithData(0, data->get()));
}

#if HAVE(SEC_KEYCHAIN)
template<typename Encoder>
void ArgumentCoder<SecKeychainItemRef>::encode(Encoder& encoder, SecKeychainItemRef keychainItem)
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessCredentials));

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CFDataRef data;
    if (SecKeychainItemCreatePersistentReference(keychainItem, &data) == errSecSuccess) {
        encoder << data;
        CFRelease(data);
    }
ALLOW_DEPRECATED_DECLARATIONS_END
}

template void ArgumentCoder<SecKeychainItemRef>::encode<Encoder>(Encoder&, SecKeychainItemRef);
template void ArgumentCoder<SecKeychainItemRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, SecKeychainItemRef);

std::optional<RetainPtr<SecKeychainItemRef>> ArgumentCoder<RetainPtr<SecKeychainItemRef>>::decode(Decoder& decoder)
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessCredentials));

    std::optional<RetainPtr<CFDataRef>> data;
    decoder >> data;
    if (!data)
        return std::nullopt;

    CFDataRef dref = data->get();
    // SecKeychainItemCopyFromPersistentReference() cannot handle 0-length CFDataRefs.
    if (!CFDataGetLength(dref))
        return std::nullopt;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    SecKeychainItemRef item;
    if (SecKeychainItemCopyFromPersistentReference(dref, &item) != errSecSuccess || !item)
        return std::nullopt;
ALLOW_DEPRECATED_DECLARATIONS_END

    return adoptCF(item);
}
#endif

#if HAVE(SEC_ACCESS_CONTROL)
template<typename Encoder>
void ArgumentCoder<SecAccessControlRef>::encode(Encoder& encoder, SecAccessControlRef accessControl)
{
    auto data = adoptCF(SecAccessControlCopyData(accessControl));
    if (data)
        encoder << data;
}

template void ArgumentCoder<SecAccessControlRef>::encode<Encoder>(Encoder&, SecAccessControlRef);
template void ArgumentCoder<SecAccessControlRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, SecAccessControlRef);

std::optional<RetainPtr<SecAccessControlRef>> ArgumentCoder<RetainPtr<SecAccessControlRef>>::decode(Decoder& decoder)
{
    std::optional<RetainPtr<CFDataRef>> data;
    decoder >> data;
    if (!data)
        return std::nullopt;

    auto result = adoptCF(SecAccessControlCreateFromData(kCFAllocatorDefault, data->get(), nullptr));
    if (!result)
        return std::nullopt;

    return WTFMove(result);
}
#endif

template<typename Encoder>
void ArgumentCoder<SecTrustRef>::encode(Encoder& encoder, SecTrustRef trust)
{
    auto data = adoptCF(SecTrustSerialize(trust, nullptr));
    if (!data) {
        encoder << false;
        return;
    }

    encoder << true << data;
}

template void ArgumentCoder<SecTrustRef>::encode<Encoder>(Encoder&, SecTrustRef);
template void ArgumentCoder<SecTrustRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, SecTrustRef);

template<typename Decoder>
std::optional<RetainPtr<SecTrustRef>> ArgumentCoder<RetainPtr<SecTrustRef>>::decode(Decoder& decoder)
{
    std::optional<bool> hasTrust;
    decoder >> hasTrust;
    if (!hasTrust)
        return std::nullopt;

    if (!*hasTrust)
        return { nullptr };

    std::optional<RetainPtr<CFDataRef>> trustData;
    decoder >> trustData;
    if (!trustData)
        return std::nullopt;

    auto trust = adoptCF(SecTrustDeserialize(trustData->get(), nullptr));
    if (!trust)
        return std::nullopt;

    return WTFMove(trust);
}

template std::optional<RetainPtr<SecTrustRef>> ArgumentCoder<RetainPtr<SecTrustRef>>::decode<Decoder>(Decoder&);

} // namespace IPC

namespace WTF {

template<> struct EnumTraits<IPC::CFType> {
    using values = EnumValues<
        IPC::CFType,
        IPC::CFType::CFArray,
        IPC::CFType::CFBoolean,
        IPC::CFType::CFCharacterSet,
        IPC::CFType::CFData,
        IPC::CFType::CFDate,
        IPC::CFType::CFDictionary,
        IPC::CFType::CFNull,
        IPC::CFType::CFNumber,
        IPC::CFType::CFString,
        IPC::CFType::CFURL,
        IPC::CFType::SecCertificate,
#if HAVE(SEC_KEYCHAIN)
        IPC::CFType::SecKeychainItem,
#endif
#if HAVE(SEC_ACCESS_CONTROL)
        IPC::CFType::SecAccessControl,
#endif
        IPC::CFType::SecTrust,
        IPC::CFType::CGColorSpace,
        IPC::CFType::CGColor,
        IPC::CFType::Nullptr,
        IPC::CFType::Unknown
    >;
};

} // namespace WTF
