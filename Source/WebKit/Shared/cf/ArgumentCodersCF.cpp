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

#include "ArgumentCodersCocoa.h"
#include "DataReference.h"
#include "Decoder.h"
#include "Encoder.h"
#include "StreamConnectionEncoder.h"
#include <CoreGraphics/CoreGraphics.h>
#include <WebCore/Color.h>
#include <WebCore/ColorSpace.h>
#include <WebCore/ColorSpaceCG.h>
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
        return nullptr;
    case CFType::Unknown:
        break;
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
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
