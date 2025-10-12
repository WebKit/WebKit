/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "CoreIPCCFType.h"

#import "ArgumentCodersCocoa.h"
#import "CoreIPCBoolean.h"
#import "CoreIPCCFArray.h"
#import "CoreIPCCFCharacterSet.h"
#import "CoreIPCCFDictionary.h"
#import "CoreIPCCFURL.h"
#import "CoreIPCCGColorSpace.h"
#import "CoreIPCSecAccessControl.h"
#import "CoreIPCSecCertificate.h"
#import "CoreIPCSecTrust.h"
#import "CoreIPCTypes.h"
#import "StreamConnectionEncoder.h"

namespace WebKit {

static CFObjectValue variantFromCFType(CFTypeRef cfType)
{
    switch (IPC::typeFromCFTypeRef(cfType)) {
    case IPC::CFType::CFArray:
        return CoreIPCCFArray((CFArrayRef)cfType);
    case IPC::CFType::CFBoolean:
        return CoreIPCBoolean((CFBooleanRef)cfType);
    case IPC::CFType::CFCharacterSet:
        return CoreIPCCFCharacterSet((CFCharacterSetRef)cfType);
    case IPC::CFType::CFData:
        return CoreIPCData((CFDataRef)cfType);
    case IPC::CFType::CFDate:
        return CoreIPCDate((CFDateRef)cfType);
    case IPC::CFType::CFDictionary:
        return CoreIPCCFDictionary((CFDictionaryRef)cfType);
    case IPC::CFType::CFNull:
        return CoreIPCNull((CFNullRef)cfType);
    case IPC::CFType::CFNumber:
        return CoreIPCNumber((CFNumberRef)cfType);
    case IPC::CFType::CFString:
        return CoreIPCString((CFStringRef)cfType);
    case IPC::CFType::CFURL:
        return CoreIPCCFURL((CFURLRef)cfType);
    case IPC::CFType::SecCertificate:
        return CoreIPCSecCertificate((SecCertificateRef)const_cast<void*>(cfType));
    case IPC::CFType::SecAccessControl:
        return CoreIPCSecAccessControl((SecAccessControlRef)const_cast<void*>(cfType));
    case IPC::CFType::SecTrust:
        return CoreIPCSecTrust((SecTrustRef)const_cast<void*>(cfType));
    case IPC::CFType::CGColorSpace:
        return CoreIPCCGColorSpace((CGColorSpaceRef)const_cast<void*>(cfType));
    case IPC::CFType::CGColor:
        return WebCore::Color::createAndPreserveColorSpace((CGColorRef)const_cast<void*>(cfType));
    case IPC::CFType::Nullptr:
        return nullptr;
    case IPC::CFType::Unknown:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

CoreIPCCFType::CoreIPCCFType(CFTypeRef cfType)
    : m_object(makeUniqueRefWithoutFastMallocCheck<CFObjectValue>(variantFromCFType(cfType))) { }

CoreIPCCFType::CoreIPCCFType(CoreIPCCFType&&) = default;

CoreIPCCFType::CoreIPCCFType(UniqueRef<CFObjectValue>&& object)
    : m_object(WTFMove(object)) { }

CoreIPCCFType::~CoreIPCCFType() = default;

RetainPtr<id> CoreIPCCFType::toID() const
{
    return (__bridge id)(toCFType().get());
}

RetainPtr<CFTypeRef> CoreIPCCFType::toCFType() const
{
    return WTF::switchOn(m_object.get(), [] (const std::nullptr_t&) -> RetainPtr<CFTypeRef> {
        return nullptr;
    }, [] (const CoreIPCCFArray& array) -> RetainPtr<CFTypeRef> {
        return array.createCFArray();
    }, [] (const CoreIPCBoolean& boolean) -> RetainPtr<CFTypeRef> {
        return boolean.createBoolean();
    }, [] (const CoreIPCCFCharacterSet& characterSet) -> RetainPtr<CFTypeRef> {
        return characterSet.toCF();
    }, [] (const CoreIPCData& data) -> RetainPtr<CFTypeRef> {
        return data.data();
    }, [] (const CoreIPCDate& date) -> RetainPtr<CFTypeRef> {
        return date.createCFDate();
    }, [] (const CoreIPCCFDictionary& dictionary) -> RetainPtr<CFTypeRef> {
        return dictionary.createCFDictionary();
    }, [] (const CoreIPCNull& nullRef) -> RetainPtr<CFTypeRef> {
        return nullRef.toCFObject();
    }, [] (const CoreIPCNumber& number) -> RetainPtr<CFTypeRef> {
        return number.createCFNumber();
    }, [] (const CoreIPCString& string) -> RetainPtr<CFTypeRef> {
        return (CFStringRef)string.toID().get();
    }, [] (const CoreIPCCFURL& url) -> RetainPtr<CFTypeRef> {
        return url.createCFURL();
    }, [] (const CoreIPCSecCertificate& certificate) -> RetainPtr<CFTypeRef> {
        return certificate.createSecCertificate();
    }, [] (const CoreIPCSecTrust& trust) -> RetainPtr<CFTypeRef> {
        return trust.createSecTrust();
    }, [] (const CoreIPCCGColorSpace& colorSpace) -> RetainPtr<CFTypeRef> {
        return colorSpace.toCF();
    }, [] (const WebCore::Color& color) -> RetainPtr<CFTypeRef> {
        return WebCore::cachedCGColor(color);
    }, [] (const CoreIPCSecAccessControl& accessControl) -> RetainPtr<CFTypeRef> {
        return accessControl.createSecAccessControl();
    });
}

} // namespace WebKit

namespace IPC {

CFType typeFromCFTypeRef(CFTypeRef type)
{
    if (!type)
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
    if (typeID == SecAccessControlGetTypeID())
        return CFType::SecAccessControl;
    if (typeID == SecTrustGetTypeID())
        return CFType::SecTrust;

    // If you're hitting this, it probably means that you've put an NS type inside a CF container.
    // Try round-tripping the container through an NS type instead.
    return CFType::Unknown;
}

template<typename Encoder>
void ArgumentCoder<UniqueRef<WebKit::CFObjectValue>>::encode(Encoder& encoder, const UniqueRef<WebKit::CFObjectValue>& object)
{
    encoder << object.get();
}

template
void ArgumentCoder<UniqueRef<WebKit::CFObjectValue>>::encode<Encoder>(Encoder&, const UniqueRef<WebKit::CFObjectValue>&);
template
void ArgumentCoder<UniqueRef<WebKit::CFObjectValue>>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const UniqueRef<WebKit::CFObjectValue>&);

std::optional<UniqueRef<WebKit::CFObjectValue>> ArgumentCoder<UniqueRef<WebKit::CFObjectValue>>::decode(Decoder& decoder)
{
    auto object = decoder.decode<WebKit::CFObjectValue>();
    if (!object)
        return std::nullopt;
    return makeUniqueRefWithoutFastMallocCheck<WebKit::CFObjectValue>(WTFMove(*object));
}

} // namespace IPC
