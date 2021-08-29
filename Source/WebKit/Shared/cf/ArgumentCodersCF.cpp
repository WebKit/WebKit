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
#if HAVE(SEC_TRUST_SERIALIZATION)
    SecTrust,
#endif
    CGColorSpace,
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
    if (typeID == SecCertificateGetTypeID())
        return CFType::SecCertificate;
#if HAVE(SEC_KEYCHAIN)
    if (typeID == SecKeychainItemGetTypeID())
        return CFType::SecKeychainItem;
#endif
#if HAVE(SEC_ACCESS_CONTROL)
    if (typeID == SecAccessControlGetTypeID())
        return CFType::SecAccessControl;
#endif
#if HAVE(SEC_TRUST_SERIALIZATION)
    if (typeID == SecTrustGetTypeID())
        return CFType::SecTrust;
#endif

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
#if HAVE(SEC_TRUST_SERIALIZATION)
    case CFType::SecTrust:
        encoder << static_cast<SecTrustRef>(const_cast<void*>(typeRef));
        return;
#endif
    case CFType::Nullptr:
        return;
    case CFType::Unknown:
        break;
    }

    ASSERT_NOT_REACHED();
}

template void ArgumentCoder<CFTypeRef>::encode<Encoder>(Encoder&, CFTypeRef);
template void ArgumentCoder<CFTypeRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFTypeRef);

bool ArgumentCoder<RetainPtr<CFTypeRef>>::decode(Decoder& decoder, RetainPtr<CFTypeRef>& result)
{
    std::optional<CFType> type;
    decoder >> type;
    if (!type)
        return false;

    switch (*type) {
    case CFType::CFArray: {
        RetainPtr<CFArrayRef> array;
        if (!decoder.decode(array))
            return false;
        result = WTFMove(array);
        return true;
    }
    case CFType::CFBoolean: {
        RetainPtr<CFBooleanRef> boolean;
        if (!decoder.decode(boolean))
            return false;
        result = WTFMove(boolean);
        return true;
    }
    case CFType::CFData: {
        RetainPtr<CFDataRef> data;
        if (!decoder.decode(data))
            return false;
        result = WTFMove(data);
        return true;
    }
    case CFType::CFDate: {
        RetainPtr<CFDateRef> date;
        if (!decoder.decode(date))
            return false;
        result = WTFMove(date);
        return true;
    }
    case CFType::CFDictionary: {
        RetainPtr<CFDictionaryRef> dictionary;
        if (!decoder.decode(dictionary))
            return false;
        result = WTFMove(dictionary);
        return true;
    }
    case CFType::CFNull:
        result = adoptCF(kCFNull);
        return true;
    case CFType::CFNumber: {
        RetainPtr<CFNumberRef> number;
        if (!decoder.decode(number))
            return false;
        result = WTFMove(number);
        return true;
    }
    case CFType::CFString: {
        RetainPtr<CFStringRef> string;
        if (!decoder.decode(string))
            return false;
        result = WTFMove(string);
        return true;
    }
    case CFType::CFURL: {
        RetainPtr<CFURLRef> url;
        if (!decoder.decode(url))
            return false;
        result = WTFMove(url);
        return true;
    }
    case CFType::CGColorSpace: {
        RetainPtr<CGColorSpaceRef> colorSpace;
        if (!decoder.decode(colorSpace))
            return false;
        result = WTFMove(colorSpace);
        return true;
    }
    case CFType::SecCertificate: {
        RetainPtr<SecCertificateRef> certificate;
        if (!decoder.decode(certificate))
            return false;
        result = WTFMove(certificate);
        return true;
    }
#if HAVE(SEC_KEYCHAIN)
    case CFType::SecKeychainItem: {
        RetainPtr<SecKeychainItemRef> keychainItem;
        if (!decoder.decode(keychainItem))
            return false;
        result = WTFMove(keychainItem);
        return true;
    }
#endif
#if HAVE(SEC_ACCESS_CONTROL)
    case CFType::SecAccessControl: {
        RetainPtr<SecAccessControlRef> accessControl;
        if (!decoder.decode(accessControl))
            return false;
        result = WTFMove(accessControl);
        return true;
    }
#endif
#if HAVE(SEC_TRUST_SERIALIZATION)
    case CFType::SecTrust: {
        RetainPtr<SecTrustRef> trust;
        if (!decoder.decode(trust))
            return false;
        result = WTFMove(trust);
        return true;
    }
#endif
    case CFType::Nullptr:
        result = tokenNullptrTypeRef();
        return true;
    case CFType::Unknown:
        break;
    }

    ASSERT_NOT_REACHED();
    return false;
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

bool ArgumentCoder<RetainPtr<CFArrayRef>>::decode(Decoder& decoder, RetainPtr<CFArrayRef>& result)
{
    bool isNull = false;
    if (!decoder.decode(isNull))
        return false;

    if (isNull) {
        result = nullptr;
        return true;
    }

    uint64_t size;
    if (!decoder.decode(size))
        return false;

    auto array = adoptCF(CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));

    for (size_t i = 0; i < size; ++i) {
        RetainPtr<CFTypeRef> element;
        if (!decoder.decode(element))
            return false;

        if (!element)
            continue;
        
        CFArrayAppendValue(array.get(), element.get());
    }

    result = adoptCF(array.leakRef());
    return true;
}

template<typename Encoder>
void ArgumentCoder<CFBooleanRef>::encode(Encoder& encoder, CFBooleanRef boolean)
{
    encoder << static_cast<bool>(CFBooleanGetValue(boolean));
}

template void ArgumentCoder<CFBooleanRef>::encode<Encoder>(Encoder&, CFBooleanRef);
template void ArgumentCoder<CFBooleanRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFBooleanRef);

bool ArgumentCoder<RetainPtr<CFBooleanRef>>::decode(Decoder& decoder, RetainPtr<CFBooleanRef>& result)
{
    bool boolean;
    if (!decoder.decode(boolean))
        return false;

    result = adoptCF(boolean ? kCFBooleanTrue : kCFBooleanFalse);
    return true;
}

template<typename Encoder>
void ArgumentCoder<CFDataRef>::encode(Encoder& encoder, CFDataRef data)
{
    encoder << IPC::DataReference(CFDataGetBytePtr(data), CFDataGetLength(data));
}

template void ArgumentCoder<CFDataRef>::encode<Encoder>(Encoder&, CFDataRef);
template void ArgumentCoder<CFDataRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFDataRef);

bool ArgumentCoder<RetainPtr<CFDataRef>>::decode(Decoder& decoder, RetainPtr<CFDataRef>& result)
{
    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    result = adoptCF(CFDataCreate(0, dataReference.data(), dataReference.size()));
    return true;
}

template<typename Encoder>
void ArgumentCoder<CFDateRef>::encode(Encoder& encoder, CFDateRef date)
{
    encoder << static_cast<double>(CFDateGetAbsoluteTime(date));
}

template void ArgumentCoder<CFDateRef>::encode<Encoder>(Encoder&, CFDateRef);
template void ArgumentCoder<CFDateRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFDateRef);

bool ArgumentCoder<RetainPtr<CFDateRef>>::decode(Decoder& decoder, RetainPtr<CFDateRef>& result)
{
    double absoluteTime;
    if (!decoder.decode(absoluteTime))
        return false;

    result = adoptCF(CFDateCreate(0, absoluteTime));
    return true;
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

bool ArgumentCoder<RetainPtr<CFDictionaryRef>>::decode(Decoder& decoder, RetainPtr<CFDictionaryRef>& result)
{
    bool isNull = false;
    if (!decoder.decode(isNull))
        return false;

    if (isNull) {
        result = nullptr;
        return true;
    }

    uint64_t size;
    if (!decoder.decode(size))
        return false;

    RetainPtr<CFMutableDictionaryRef> dictionary = adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    for (uint64_t i = 0; i < size; ++i) {
        RetainPtr<CFTypeRef> key;
        if (!decoder.decode(key) || !key)
            return false;

        RetainPtr<CFTypeRef> value;
        if (!decoder.decode(value) || !value)
            return false;

        CFDictionarySetValue(dictionary.get(), key.get(), value.get());
    }

    result = WTFMove(dictionary);
    return true;
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

bool ArgumentCoder<RetainPtr<CFNumberRef>>::decode(Decoder& decoder, RetainPtr<CFNumberRef>& result)
{
    std::optional<uint8_t> numberTypeFromIPC;
    decoder >> numberTypeFromIPC;
    if (!numberTypeFromIPC || *numberTypeFromIPC > kCFNumberMaxType)
        return false;
    auto numberType = static_cast<CFNumberType>(*numberTypeFromIPC);

    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    size_t neededBufferSize = sizeForNumberType(numberType);
    if (!neededBufferSize || dataReference.size() != neededBufferSize)
        return false;

    ASSERT(dataReference.data());
    result = adoptCF(CFNumberCreate(0, numberType, dataReference.data()));

    return true;
}

template<typename Encoder>
void ArgumentCoder<CFStringRef>::encode(Encoder& encoder, CFStringRef string)
{
    CFIndex length = CFStringGetLength(string);
    CFStringEncoding encoding = CFStringGetFastestEncoding(string);

    CFRange range = CFRangeMake(0, length);
    CFIndex bufferLength = 0;

    CFIndex numConvertedBytes = CFStringGetBytes(string, range, encoding, 0, false, 0, 0, &bufferLength);
    ASSERT(numConvertedBytes == length);

    Vector<UInt8, 128> buffer(bufferLength);
    numConvertedBytes = CFStringGetBytes(string, range, encoding, 0, false, buffer.data(), buffer.size(), &bufferLength);
    ASSERT(numConvertedBytes == length);

    encoder << static_cast<uint32_t>(encoding) << IPC::DataReference(buffer);
}

template void ArgumentCoder<CFStringRef>::encode<Encoder>(Encoder&, CFStringRef);
template void ArgumentCoder<CFStringRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFStringRef);

bool ArgumentCoder<RetainPtr<CFStringRef>>::decode(Decoder& decoder, RetainPtr<CFStringRef>& result)
{
    std::optional<uint32_t> encodingFromIPC;
    decoder >> encodingFromIPC;
    if (!encodingFromIPC)
        return false;
    auto encoding = static_cast<CFStringEncoding>(*encodingFromIPC);

    if (!CFStringIsEncodingAvailable(encoding))
        return false;
    
    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    auto string = adoptCF(CFStringCreateWithBytes(0, dataReference.data(), dataReference.size(), encoding, false));
    if (!string)
        return false;

    result = WTFMove(string);
    return true;
}

template<typename Encoder>
void ArgumentCoder<CFURLRef>::encode(Encoder& encoder, CFURLRef url)
{
    CFURLRef baseURL = CFURLGetBaseURL(url);
    encoder << static_cast<bool>(baseURL);
    if (baseURL)
        encoder << baseURL;

    WTF::URLCharBuffer urlBytes;
    WTF::getURLBytes(url, urlBytes);
    encoder << IPC::DataReference(reinterpret_cast<const uint8_t*>(urlBytes.data()), urlBytes.size());
}

template void ArgumentCoder<CFURLRef>::encode<Encoder>(Encoder&, CFURLRef);
template void ArgumentCoder<CFURLRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, CFURLRef);

bool ArgumentCoder<RetainPtr<CFURLRef>>::decode(Decoder& decoder, RetainPtr<CFURLRef>& result)
{
    RetainPtr<CFURLRef> baseURL;
    bool hasBaseURL;
    if (!decoder.decode(hasBaseURL))
        return false;
    if (hasBaseURL) {
        if (!decoder.decode(baseURL))
            return false;
    }

    IPC::DataReference urlBytes;
    if (!decoder.decode(urlBytes))
        return false;

#if USE(FOUNDATION)
    // FIXME: Move this to ArgumentCodersCFMac.mm and change this file back to be C++
    // instead of Objective-C++.
    if (urlBytes.isEmpty()) {
        // CFURL can't hold an empty URL, unlike NSURL.
        // FIXME: This discards base URL, which seems incorrect.
        result = (__bridge CFURLRef)[NSURL URLWithString:@""];
        return true;
    }
#endif

    result = adoptCF(CFURLCreateAbsoluteURLWithBytes(nullptr, reinterpret_cast<const UInt8*>(urlBytes.data()), urlBytes.size(), kCFStringEncodingUTF8, baseURL.get(), true));
    return result;
}

enum class CGColorSpaceEncodingScheme { Name, PropertyList };

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

bool ArgumentCoder<RetainPtr<CGColorSpaceRef>>::decode(Decoder& decoder, RetainPtr<CGColorSpaceRef>& result)
{
    CGColorSpaceEncodingScheme encodingScheme;
    if (!decoder.decode(encodingScheme))
        return false;

    switch (encodingScheme) {
    case CGColorSpaceEncodingScheme::Name: {
        RetainPtr<CFStringRef> name;
        if (!decoder.decode(name))
            return false;

        auto colorSpace = adoptCF(CGColorSpaceCreateWithName(name.get()));
        if (!colorSpace)
            return false;

        result = WTFMove(colorSpace);
        return true;
    }
    case CGColorSpaceEncodingScheme::PropertyList: {
        RetainPtr<CFTypeRef> propertyList;
        if (!decoder.decode(propertyList))
            return false;

        auto colorSpace = adoptCF(CGColorSpaceCreateWithPropertyList(propertyList.get()));
        if (!colorSpace)
            return false;

        result = WTFMove(colorSpace);
        return true;
    }
    }

    ASSERT_NOT_REACHED();
    return false;
}


template<typename Encoder>
void ArgumentCoder<SecCertificateRef>::encode(Encoder& encoder, SecCertificateRef certificate)
{
    encoder << adoptCF(SecCertificateCopyData(certificate));
}

template void ArgumentCoder<SecCertificateRef>::encode<Encoder>(Encoder&, SecCertificateRef);
template void ArgumentCoder<SecCertificateRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, SecCertificateRef);

bool ArgumentCoder<RetainPtr<SecCertificateRef>>::decode(Decoder& decoder, RetainPtr<SecCertificateRef>& result)
{
    RetainPtr<CFDataRef> data;
    if (!decoder.decode(data))
        return false;

    result = adoptCF(SecCertificateCreateWithData(0, data.get()));
    return true;
}

#if HAVE(SEC_KEYCHAIN)
template<typename Encoder>
void ArgumentCoder<SecKeychainItemRef>::encode(Encoder& encoder, SecKeychainItemRef keychainItem)
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessCredentials));

    CFDataRef data;
    if (SecKeychainItemCreatePersistentReference(keychainItem, &data) == errSecSuccess) {
        encoder << data;
        CFRelease(data);
    }
}

template void ArgumentCoder<SecKeychainItemRef>::encode<Encoder>(Encoder&, SecKeychainItemRef);
template void ArgumentCoder<SecKeychainItemRef>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, SecKeychainItemRef);

bool ArgumentCoder<RetainPtr<SecKeychainItemRef>>::decode(Decoder& decoder, RetainPtr<SecKeychainItemRef>& result)
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessCredentials));

    RetainPtr<CFDataRef> data;
    if (!decoder.decode(data))
        return false;

    CFDataRef dref = data.get();
    // SecKeychainItemCopyFromPersistentReference() cannot handle 0-length CFDataRefs.
    if (!CFDataGetLength(dref))
        return false;

    SecKeychainItemRef item;
    if (SecKeychainItemCopyFromPersistentReference(dref, &item) != errSecSuccess || !item)
        return false;
    
    result = adoptCF(item);
    return true;
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

bool ArgumentCoder<RetainPtr<SecAccessControlRef>>::decode(Decoder& decoder, RetainPtr<SecAccessControlRef>& result)
{
    RetainPtr<CFDataRef> data;
    if (!decoder.decode(data))
        return false;

    result = adoptCF(SecAccessControlCreateFromData(kCFAllocatorDefault, data.get(), nullptr));
    if (!result)
        return false;

    return true;
}
#endif

#if HAVE(SEC_TRUST_SERIALIZATION)
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

bool ArgumentCoder<RetainPtr<SecTrustRef>>::decode(Decoder& decoder, RetainPtr<SecTrustRef>& result)
{
    bool hasTrust;
    if (!decoder.decode(hasTrust))
        return false;

    if (!hasTrust)
        return true;

    RetainPtr<CFDataRef> trustData;
    if (!decoder.decode(trustData))
        return false;

    auto trust = adoptCF(SecTrustDeserialize(trustData.get(), nullptr));
    if (!trust)
        return false;

    result = WTFMove(trust);
    return true;
}
#endif

} // namespace IPC

namespace WTF {

template<> struct EnumTraits<IPC::CFType> {
    using values = EnumValues<
        IPC::CFType,
        IPC::CFType::CFArray,
        IPC::CFType::CFBoolean,
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
#if HAVE(SEC_TRUST_SERIALIZATION)
        IPC::CFType::SecTrust,
#endif
        IPC::CFType::CGColorSpace,
        IPC::CFType::Nullptr,
        IPC::CFType::Unknown
    >;
};

template<> struct EnumTraits<IPC::CGColorSpaceEncodingScheme> {
    using values = EnumValues<
        IPC::CGColorSpaceEncodingScheme,
        IPC::CGColorSpaceEncodingScheme::Name,
        IPC::CGColorSpaceEncodingScheme::PropertyList
    >;
};

} // namespace WTF
