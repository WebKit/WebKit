/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "CoreIPCNSCFObject.h"

#if PLATFORM(COCOA)

#import "ArgumentCodersCocoa.h"
#import "CoreIPCTypes.h"
#import "GeneratedWebKitSecureCoding.h"
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebKit {

static ObjectValue valueFromID(id object)
{
    if (!object)
        return nullptr;

    switch (IPC::typeFromObject(object)) {
    case IPC::NSType::Array:
        return CoreIPCArray((NSArray *)object);
    case IPC::NSType::Color:
        return CoreIPCColor((WebCore::CocoaColor *)object);
#if USE(PASSKIT)
    case IPC::NSType::PKPaymentMethod:
        return CoreIPCPKPaymentMethod((PKPaymentMethod *)object);
    case IPC::NSType::PKPaymentMerchantSession:
        return CoreIPCPKPaymentMerchantSession((PKPaymentMerchantSession *)object);
    case IPC::NSType::PKPaymentSetupFeature:
        return CoreIPCPKPaymentSetupFeature((PKPaymentSetupFeature *)object);
    case IPC::NSType::PKContact:
        return CoreIPCPKContact((PKContact *)object);
    case IPC::NSType::PKSecureElementPass:
        return CoreIPCPKSecureElementPass((PKSecureElementPass *)object);
    case IPC::NSType::PKPayment:
        return CoreIPCPKPayment((PKPayment *)object);
    case IPC::NSType::PKPaymentToken:
        return CoreIPCPKPaymentToken((PKPaymentToken *)object);
    case IPC::NSType::PKShippingMethod:
        return CoreIPCPKShippingMethod((PKShippingMethod *)object);
    case IPC::NSType::PKDateComponentsRange:
        return CoreIPCPKDateComponentsRange((PKDateComponentsRange *)object);
    case IPC::NSType::CNContact:
        return CoreIPCCNContact((CNContact *)object);
    case IPC::NSType::CNPhoneNumber:
        return CoreIPCCNPhoneNumber((CNPhoneNumber *)object);
    case IPC::NSType::CNPostalAddress:
        return CoreIPCCNPostalAddress((CNPostalAddress *)object);
#endif
#if ENABLE(DATA_DETECTION) && HAVE(WK_SECURE_CODING_DATA_DETECTORS)
    case IPC::NSType::DDScannerResult:
        return CoreIPCDDScannerResult((DDScannerResult *)object);
#if PLATFORM(MAC)
    case IPC::NSType::WKDDActionContext:
        return CoreIPCDDSecureActionContext((WKDDActionContext *)object);
#endif
#endif
    case IPC::NSType::NSDateComponents:
        return CoreIPCDateComponents((NSDateComponents *)object);
    case IPC::NSType::Data:
        return CoreIPCData((NSData *)object);
    case IPC::NSType::Date:
        return CoreIPCDate(bridge_cast((NSDate *)object));
    case IPC::NSType::Dictionary:
        return CoreIPCDictionary((NSDictionary *)object);
    case IPC::NSType::Error:
        return CoreIPCError((NSError *)object);
    case IPC::NSType::Locale:
        return CoreIPCLocale((NSLocale *)object);
    case IPC::NSType::Font:
        return CoreIPCFont((WebCore::CocoaFont *)object);
    case IPC::NSType::NSValue:
        return CoreIPCNSValue((NSValue *)object);
    case IPC::NSType::Number:
        return CoreIPCNumber(bridge_cast((NSNumber *)object));
    case IPC::NSType::Null:
        return CoreIPCNull((NSNull *)object);
#if !HAVE(WK_SECURE_CODING_NSURLREQUEST)
    case IPC::NSType::SecureCoding:
        return CoreIPCSecureCoding((NSObject<NSSecureCoding> *)object);
#endif
    case IPC::NSType::String:
        return CoreIPCString((NSString *)object);
    case IPC::NSType::URL:
        return CoreIPCURL((NSURL *)object);
    case IPC::NSType::CF:
        return CoreIPCCFType((CFTypeRef)object);
    case IPC::NSType::Unknown:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

CoreIPCNSCFObject::CoreIPCNSCFObject(id object)
    : m_value(makeUniqueRefWithoutFastMallocCheck<ObjectValue>(valueFromID(object)))
{
}

CoreIPCNSCFObject::CoreIPCNSCFObject(UniqueRef<ObjectValue>&& value)
    : m_value(WTFMove(value))
{
}

RetainPtr<id> CoreIPCNSCFObject::toID() const
{
    RetainPtr<id> result;

    WTF::switchOn(m_value.get(), [&](auto& object) {
        result = object.toID();
    }, [](std::nullptr_t) {
        // result should be nil, which is the default value initialized above.
    });

    return result;
}

bool CoreIPCNSCFObject::valueIsAllowed(IPC::Decoder& decoder, ObjectValue& value)
{
#if HAVE(WK_SECURE_CODING_NSURLREQUEST)
    UNUSED_PARAM(decoder);
    UNUSED_PARAM(value);
    return true;
#else
    // The Decoder always has a set of allowedClasses,
    // but we only check that set when considering SecureCoding classes
    Class objectClass;
    WTF::switchOn(value,
        [&](CoreIPCSecureCoding& object) {
            objectClass = object.objectClass();
        }, [&](auto& object) {
            objectClass = nullptr;
        }
    );

    return !objectClass || decoder.allowedClasses().contains(objectClass);
#endif
}

} // namespace WebKit

namespace IPC {

void ArgumentCoder<UniqueRef<WebKit::ObjectValue>>::encode(Encoder& encoder, const UniqueRef<WebKit::ObjectValue>& object)
{
    encoder << object.get();
}

std::optional<UniqueRef<WebKit::ObjectValue>> ArgumentCoder<UniqueRef<WebKit::ObjectValue>>::decode(Decoder& decoder)
{
    auto object = decoder.decode<WebKit::ObjectValue>();
    if (!object)
        return std::nullopt;
    return makeUniqueRefWithoutFastMallocCheck<WebKit::ObjectValue>(WTFMove(*object));
}

} // namespace IPC

#endif // PLATFORM(COCOA)
