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

void encode(Encoder& encoder, CFTypeRef typeRef)
{
    CFType type = typeFromCFTypeRef(typeRef);
    encoder << type;

    switch (type) {
    case CFType::CFArray:
        encode(encoder, static_cast<CFArrayRef>(typeRef));
        return;
    case CFType::CFBoolean:
        encode(encoder, static_cast<CFBooleanRef>(typeRef));
        return;
    case CFType::CFData:
        encode(encoder, static_cast<CFDataRef>(typeRef));
        return;
    case CFType::CFDate:
        encode(encoder, static_cast<CFDateRef>(typeRef));
        return;
    case CFType::CFDictionary:
        encode(encoder, static_cast<CFDictionaryRef>(typeRef));
        return;
    case CFType::CFNull:
        return;
    case CFType::CFNumber:
        encode(encoder, static_cast<CFNumberRef>(typeRef));
        return;
    case CFType::CFString:
        encode(encoder, static_cast<CFStringRef>(typeRef));
        return;
    case CFType::CFURL:
        encode(encoder, static_cast<CFURLRef>(typeRef));
        return;
    case CFType::SecCertificate:
        encode(encoder, static_cast<SecCertificateRef>(const_cast<void*>(typeRef)));
        return;
#if HAVE(SEC_KEYCHAIN)
    case CFType::SecKeychainItem:
        encode(encoder, static_cast<SecKeychainItemRef>(const_cast<void*>(typeRef)));
        return;
#endif
#if HAVE(SEC_ACCESS_CONTROL)
    case CFType::SecAccessControl:
        encode(encoder, static_cast<SecAccessControlRef>(const_cast<void*>(typeRef)));
        return;
#endif
#if HAVE(SEC_TRUST_SERIALIZATION)
    case CFType::SecTrust:
        encode(encoder, static_cast<SecTrustRef>(const_cast<void*>(typeRef)));
        return;
#endif
    case CFType::Nullptr:
        return;
    case CFType::Unknown:
        break;
    }

    ASSERT_NOT_REACHED();
}

bool decode(Decoder& decoder, RetainPtr<CFTypeRef>& result)
{
    CFType type;
    if (!decoder.decode(type))
        return false;

    switch (type) {
    case CFType::CFArray: {
        RetainPtr<CFArrayRef> array;
        if (!decode(decoder, array))
            return false;
        result = adoptCF(array.leakRef());
        return true;
    }
    case CFType::CFBoolean: {
        RetainPtr<CFBooleanRef> boolean;
        if (!decode(decoder, boolean))
            return false;
        result = adoptCF(boolean.leakRef());
        return true;
    }
    case CFType::CFData: {
        RetainPtr<CFDataRef> data;
        if (!decode(decoder, data))
            return false;
        result = adoptCF(data.leakRef());
        return true;
    }
    case CFType::CFDate: {
        RetainPtr<CFDateRef> date;
        if (!decode(decoder, date))
            return false;
        result = adoptCF(date.leakRef());
        return true;
    }
    case CFType::CFDictionary: {
        RetainPtr<CFDictionaryRef> dictionary;
        if (!decode(decoder, dictionary))
            return false;
        result = adoptCF(dictionary.leakRef());
        return true;
    }
    case CFType::CFNull:
        result = adoptCF(kCFNull);
        return true;
    case CFType::CFNumber: {
        RetainPtr<CFNumberRef> number;
        if (!decode(decoder, number))
            return false;
        result = adoptCF(number.leakRef());
        return true;
    }
    case CFType::CFString: {
        RetainPtr<CFStringRef> string;
        if (!decode(decoder, string))
            return false;
        result = adoptCF(string.leakRef());
        return true;
    }
    case CFType::CFURL: {
        RetainPtr<CFURLRef> url;
        if (!decode(decoder, url))
            return false;
        result = adoptCF(url.leakRef());
        return true;
    }
    case CFType::SecCertificate: {
        RetainPtr<SecCertificateRef> certificate;
        if (!decode(decoder, certificate))
            return false;
        result = adoptCF(certificate.leakRef());
        return true;
    }
#if HAVE(SEC_KEYCHAIN)
    case CFType::SecKeychainItem: {
        RetainPtr<SecKeychainItemRef> keychainItem;
        if (!decode(decoder, keychainItem))
            return false;
        result = adoptCF(keychainItem.leakRef());
        return true;
    }
#endif
#if HAVE(SEC_ACCESS_CONTROL)
    case CFType::SecAccessControl: {
        RetainPtr<SecAccessControlRef> accessControl;
        if (!decode(decoder, accessControl))
            return false;
        result = adoptCF(accessControl.leakRef());
        return true;
    }
#endif
#if HAVE(SEC_TRUST_SERIALIZATION)
    case CFType::SecTrust: {
        RetainPtr<SecTrustRef> trust;
        if (!decode(decoder, trust))
            return false;
        result = adoptCF(trust.leakRef());
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

void encode(Encoder& encoder, CFArrayRef array)
{
    if (!array) {
        encoder << true;
        return;
    }

    encoder << false;

    CFIndex size = CFArrayGetCount(array);
    Vector<CFTypeRef, 32> values(size);

    CFArrayGetValues(array, CFRangeMake(0, size), values.data());

    HashSet<CFIndex> invalidIndicies;
    for (CFIndex i = 0; i < size; ++i) {
        // Ignore values we don't support.
        ASSERT(typeFromCFTypeRef(values[i]) != CFType::Unknown);
        if (typeFromCFTypeRef(values[i]) == CFType::Unknown)
            invalidIndicies.add(i);
    }

    encoder << static_cast<uint64_t>(size - invalidIndicies.size());

    for (CFIndex i = 0; i < size; ++i) {
        if (invalidIndicies.contains(i))
            continue;
        encode(encoder, values[i]);
    }
}

bool decode(Decoder& decoder, RetainPtr<CFArrayRef>& result)
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
        if (!decode(decoder, element))
            return false;

        if (!element)
            continue;
        
        CFArrayAppendValue(array.get(), element.get());
    }

    result = adoptCF(array.leakRef());
    return true;
}

void encode(Encoder& encoder, CFBooleanRef boolean)
{
    encoder << static_cast<bool>(CFBooleanGetValue(boolean));
}

bool decode(Decoder& decoder, RetainPtr<CFBooleanRef>& result)
{
    bool boolean;
    if (!decoder.decode(boolean))
        return false;

    result = adoptCF(boolean ? kCFBooleanTrue : kCFBooleanFalse);
    return true;
}

void encode(Encoder& encoder, CFDataRef data)
{
    CFIndex length = CFDataGetLength(data);
    const UInt8* bytePtr = CFDataGetBytePtr(data);

    encoder << IPC::DataReference(bytePtr, length);
}

bool decode(Decoder& decoder, RetainPtr<CFDataRef>& result)
{
    IPC::DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    result = adoptCF(CFDataCreate(0, dataReference.data(), dataReference.size()));
    return true;
}

void encode(Encoder& encoder, CFDateRef date)
{
    encoder << static_cast<double>(CFDateGetAbsoluteTime(date));
}

bool decode(Decoder& decoder, RetainPtr<CFDateRef>& result)
{
    double absoluteTime;
    if (!decoder.decode(absoluteTime))
        return false;

    result = adoptCF(CFDateCreate(0, absoluteTime));
    return true;
}

void encode(Encoder& encoder, CFDictionaryRef dictionary)
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

        encode(encoder, keys[i]);
        encode(encoder, values[i]);
    }
}

bool decode(Decoder& decoder, RetainPtr<CFDictionaryRef>& result)
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
        if (!decode(decoder, key) || !key)
            return false;

        RetainPtr<CFTypeRef> value;
        if (!decode(decoder, value) || !value)
            return false;

        CFDictionarySetValue(dictionary.get(), key.get(), value.get());
    }

    result = adoptCF(dictionary.leakRef());
    return true;
}

void encode(Encoder& encoder, CFNumberRef number)
{
    CFNumberType numberType = CFNumberGetType(number);

    Vector<uint8_t> buffer(CFNumberGetByteSize(number));
    bool result = CFNumberGetValue(number, numberType, buffer.data());
    ASSERT_UNUSED(result, result);

    encoder << static_cast<uint8_t>(numberType);
    encoder << IPC::DataReference(buffer);
}

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

bool decode(Decoder& decoder, RetainPtr<CFNumberRef>& result)
{
    Optional<uint8_t> numberTypeFromIPC;
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

void encode(Encoder& encoder, CFStringRef string)
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

    encoder << static_cast<uint32_t>(encoding);
    encoder << IPC::DataReference(buffer);
}

bool decode(Decoder& decoder, RetainPtr<CFStringRef>& result)
{
    Optional<uint32_t> encodingFromIPC;
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

void encode(Encoder& encoder, CFURLRef url)
{
    CFURLRef baseURL = CFURLGetBaseURL(url);
    encoder << static_cast<bool>(baseURL);
    if (baseURL)
        encode(encoder, baseURL);

    WTF::URLCharBuffer urlBytes;
    WTF::getURLBytes(url, urlBytes);
    IPC::DataReference dataReference(reinterpret_cast<const uint8_t*>(urlBytes.data()), urlBytes.size());
    encoder << dataReference;
}

bool decode(Decoder& decoder, RetainPtr<CFURLRef>& result)
{
    RetainPtr<CFURLRef> baseURL;
    bool hasBaseURL;
    if (!decoder.decode(hasBaseURL))
        return false;
    if (hasBaseURL) {
        if (!decode(decoder, baseURL))
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

void encode(Encoder& encoder, SecCertificateRef certificate)
{
    RetainPtr<CFDataRef> data = adoptCF(SecCertificateCopyData(certificate));
    encode(encoder, data.get());
}

bool decode(Decoder& decoder, RetainPtr<SecCertificateRef>& result)
{
    RetainPtr<CFDataRef> data;
    if (!decode(decoder, data))
        return false;

    result = adoptCF(SecCertificateCreateWithData(0, data.get()));
    return true;
}

#if HAVE(SEC_KEYCHAIN)
void encode(Encoder& encoder, SecKeychainItemRef keychainItem)
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessCredentials));

    CFDataRef data;
    if (SecKeychainItemCreatePersistentReference(keychainItem, &data) == errSecSuccess) {
        encode(encoder, data);
        CFRelease(data);
    }
}

bool decode(Decoder& decoder, RetainPtr<SecKeychainItemRef>& result)
{
    RELEASE_ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessCredentials));

    RetainPtr<CFDataRef> data;
    if (!IPC::decode(decoder, data))
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
void encode(Encoder& encoder, SecAccessControlRef accessControl)
{
    RetainPtr<CFDataRef> data = adoptCF(SecAccessControlCopyData(accessControl));
    if (data)
        encode(encoder, data.get());
}

bool decode(Decoder& decoder, RetainPtr<SecAccessControlRef>& result)
{
    RetainPtr<CFDataRef> data;
    if (!decode(decoder, data))
        return false;

    result = adoptCF(SecAccessControlCreateFromData(kCFAllocatorDefault, data.get(), nullptr));
    if (!result)
        return false;

    return true;
}
#endif

#if HAVE(SEC_TRUST_SERIALIZATION)
void encode(Encoder& encoder, SecTrustRef trust)
{
    auto data = adoptCF(SecTrustSerialize(trust, nullptr));
    if (!data) {
        encoder << false;
        return;
    }

    encoder << true;
    IPC::encode(encoder, data.get());
}

bool decode(Decoder& decoder, RetainPtr<SecTrustRef>& result)
{
    bool hasTrust;
    if (!decoder.decode(hasTrust))
        return false;

    if (!hasTrust)
        return true;

    RetainPtr<CFDataRef> trustData;
    if (!IPC::decode(decoder, trustData))
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
    IPC::CFType::Nullptr,
    IPC::CFType::Unknown
    >;
};

} // namespace WTF
