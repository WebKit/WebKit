/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#import "WebCoreArgumentCoders.h"

#import "ArgumentCodersCF.h"
#import "ArgumentCodersCocoa.h"
#import <CoreText/CoreText.h>
#import <WebCore/AttributedString.h>
#import <WebCore/DictionaryPopupInfo.h>
#import <WebCore/Font.h>
#import <WebCore/FontAttributes.h>
#import <WebCore/FontCustomPlatformData.h>
#import <WebCore/ResourceRequest.h>
#import <pal/spi/cf/CoreTextSPI.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIFont.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#import <WebCore/MediaPlaybackTargetContext.h>
#import <objc/runtime.h>
#endif

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WebCoreArgumentCodersCocoaAdditions.mm>
#endif

#if ENABLE(IMAGE_EXTRACTION)
#import <WebCore/ImageExtractionResult.h>
#endif

#if ENABLE(APPLE_PAY)
#import "DataReference.h"
#import <WebCore/PaymentAuthorizationStatus.h>
#import <pal/cocoa/PassKitSoftLink.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#import <pal/cocoa/AVFoundationSoftLink.h>
#endif

#if ENABLE(DATA_DETECTION)
#import <pal/cocoa/DataDetectorsCoreSoftLink.h>
#endif

namespace IPC {
using namespace WebCore;

void ArgumentCoder<WebCore::AttributedString>::encode(Encoder& encoder, const WebCore::AttributedString& attributedString)
{
    encoder << attributedString.string << attributedString.documentAttributes;
}

Optional<WebCore::AttributedString> ArgumentCoder<WebCore::AttributedString>::decode(Decoder& decoder)
{
    RetainPtr<NSAttributedString> attributedString;
    if (!IPC::decode(decoder, attributedString))
        return std::nullopt;
    RetainPtr<NSDictionary> documentAttributes;
    if (!IPC::decode(decoder, documentAttributes))
        return std::nullopt;
    return { { WTFMove(attributedString), WTFMove(documentAttributes) } };
}

#if ENABLE(APPLE_PAY)

#if HAVE(PASSKIT_INSTALLMENTS)

void ArgumentCoder<WebCore::PaymentInstallmentConfiguration>::encode(Encoder& encoder, const WebCore::PaymentInstallmentConfiguration& configuration)
{
    encoder << configuration.platformConfiguration();
}

Optional<WebCore::PaymentInstallmentConfiguration> ArgumentCoder<WebCore::PaymentInstallmentConfiguration>::decode(Decoder& decoder)
{
    auto configuration = IPC::decode<PKPaymentInstallmentConfiguration>(decoder, PAL::getPKPaymentInstallmentConfigurationClass());
    if (!configuration)
        return std::nullopt;

    return { WTFMove(*configuration) };
}

#endif // HAVE(PASSKIT_INSTALLMENTS)

void ArgumentCoder<WebCore::Payment>::encode(Encoder& encoder, const WebCore::Payment& payment)
{
    encoder << payment.pkPayment();
}

Optional<WebCore::Payment> ArgumentCoder<WebCore::Payment>::decode(Decoder& decoder)
{
    auto payment = IPC::decode<PKPayment>(decoder, PAL::getPKPaymentClass());
    if (!payment)
        return std::nullopt;

    return Payment { WTFMove(*payment) };
}

void ArgumentCoder<WebCore::PaymentAuthorizationResult>::encode(Encoder& encoder, const WebCore::PaymentAuthorizationResult& result)
{
    encoder << result.status;
    encoder << result.errors;
}

Optional<WebCore::PaymentAuthorizationResult> ArgumentCoder<WebCore::PaymentAuthorizationResult>::decode(Decoder& decoder)
{
    Optional<PaymentAuthorizationStatus> status;
    decoder >> status;
    if (!status)
        return std::nullopt;

    Optional<Vector<RefPtr<ApplePayError>>> errors;
    decoder >> errors;
    if (!errors)
        return std::nullopt;

    return {{ WTFMove(*status), WTFMove(*errors) }};
}

void ArgumentCoder<WebCore::PaymentContact>::encode(Encoder& encoder, const WebCore::PaymentContact& paymentContact)
{
    encoder << paymentContact.pkContact();
}

Optional<WebCore::PaymentContact> ArgumentCoder<WebCore::PaymentContact>::decode(Decoder& decoder)
{
    auto contact = IPC::decode<PKContact>(decoder, PAL::getPKContactClass());
    if (!contact)
        return std::nullopt;

    return WebCore::PaymentContact { WTFMove(*contact) };
}

void ArgumentCoder<WebCore::PaymentMerchantSession>::encode(Encoder& encoder, const WebCore::PaymentMerchantSession& paymentMerchantSession)
{
    encoder << paymentMerchantSession.pkPaymentMerchantSession();
}

Optional<WebCore::PaymentMerchantSession> ArgumentCoder<WebCore::PaymentMerchantSession>::decode(Decoder& decoder)
{
    auto paymentMerchantSession = IPC::decode<PKPaymentMerchantSession>(decoder, PAL::getPKPaymentMerchantSessionClass());
    if (!paymentMerchantSession)
        return std::nullopt;

    return WebCore::PaymentMerchantSession { WTFMove(*paymentMerchantSession) };
}

void ArgumentCoder<WebCore::PaymentMethod>::encode(Encoder& encoder, const WebCore::PaymentMethod& paymentMethod)
{
    encoder << paymentMethod.pkPaymentMethod();
}

Optional<WebCore::PaymentMethod> ArgumentCoder<WebCore::PaymentMethod>::decode(Decoder& decoder)
{
    auto paymentMethod = IPC::decode<PKPaymentMethod>(decoder, PAL::getPKPaymentMethodClass());
    if (!paymentMethod)
        return std::nullopt;

    return PaymentMethod { WTFMove(*paymentMethod) };
}

void ArgumentCoder<ApplePaySessionPaymentRequest>::encode(Encoder& encoder, const ApplePaySessionPaymentRequest& request)
{
    encoder << request.countryCode();
    encoder << request.currencyCode();
    encoder << request.requiredBillingContactFields();
    encoder << request.billingContact();
    encoder << request.requiredShippingContactFields();
    encoder << request.shippingContact();
    encoder << request.merchantCapabilities();
    encoder << request.supportedNetworks();
    encoder << request.shippingType();
    encoder << request.shippingMethods();
    encoder << request.lineItems();
    encoder << request.total();
    encoder << request.applicationData();
    encoder << request.supportedCountries();
    encoder << request.requester();
#if ENABLE(APPLE_PAY_INSTALLMENTS)
    encoder << request.installmentConfiguration();
#endif
#if defined(WebCoreArgumentCodersCocoaAdditions_ApplePaySessionPaymentRequest_encode)
    WebCoreArgumentCodersCocoaAdditions_ApplePaySessionPaymentRequest_encode
#endif
}

bool ArgumentCoder<ApplePaySessionPaymentRequest>::decode(Decoder& decoder, ApplePaySessionPaymentRequest& request)
{
    String countryCode;
    if (!decoder.decode(countryCode))
        return false;
    request.setCountryCode(countryCode);

    String currencyCode;
    if (!decoder.decode(currencyCode))
        return false;
    request.setCurrencyCode(currencyCode);

    ApplePaySessionPaymentRequest::ContactFields requiredBillingContactFields;
    if (!decoder.decode((requiredBillingContactFields)))
        return false;
    request.setRequiredBillingContactFields(requiredBillingContactFields);

    Optional<PaymentContact> billingContact;
    decoder >> billingContact;
    if (!billingContact)
        return false;
    request.setBillingContact(*billingContact);

    ApplePaySessionPaymentRequest::ContactFields requiredShippingContactFields;
    if (!decoder.decode((requiredShippingContactFields)))
        return false;
    request.setRequiredShippingContactFields(requiredShippingContactFields);

    Optional<PaymentContact> shippingContact;
    decoder >> shippingContact;
    if (!shippingContact)
        return false;
    request.setShippingContact(*shippingContact);

    ApplePaySessionPaymentRequest::MerchantCapabilities merchantCapabilities;
    if (!decoder.decode(merchantCapabilities))
        return false;
    request.setMerchantCapabilities(merchantCapabilities);

    Vector<String> supportedNetworks;
    if (!decoder.decode(supportedNetworks))
        return false;
    request.setSupportedNetworks(supportedNetworks);

    ApplePaySessionPaymentRequest::ShippingType shippingType;
    if (!decoder.decode(shippingType))
        return false;
    request.setShippingType(shippingType);

    Vector<ApplePayShippingMethod> shippingMethods;
    if (!decoder.decode(shippingMethods))
        return false;
    request.setShippingMethods(shippingMethods);

    Vector<ApplePayLineItem> lineItems;
    if (!decoder.decode(lineItems))
        return false;
    request.setLineItems(lineItems);

    Optional<ApplePayLineItem> total;
    decoder >> total;
    if (!total)
        return false;
    request.setTotal(*total);

    String applicationData;
    if (!decoder.decode(applicationData))
        return false;
    request.setApplicationData(applicationData);

    Vector<String> supportedCountries;
    if (!decoder.decode(supportedCountries))
        return false;
    request.setSupportedCountries(WTFMove(supportedCountries));

    ApplePaySessionPaymentRequest::Requester requester;
    if (!decoder.decode(requester))
        return false;
    request.setRequester(requester);
    
#if ENABLE(APPLE_PAY_INSTALLMENTS)
    Optional<WebCore::PaymentInstallmentConfiguration> installmentConfiguration;
    decoder >> installmentConfiguration;
    if (!installmentConfiguration)
        return false;

    request.setInstallmentConfiguration(WTFMove(*installmentConfiguration));
#endif

#if defined(WebCoreArgumentCodersCocoaAdditions_ApplePaySessionPaymentRequest_decode)
    WebCoreArgumentCodersCocoaAdditions_ApplePaySessionPaymentRequest_decode
#endif

    return true;
}

void ArgumentCoder<ApplePaySessionPaymentRequest::ContactFields>::encode(Encoder& encoder, const ApplePaySessionPaymentRequest::ContactFields& contactFields)
{
    encoder << contactFields.postalAddress;
    encoder << contactFields.phone;
    encoder << contactFields.email;
    encoder << contactFields.name;
    encoder << contactFields.phoneticName;
}

bool ArgumentCoder<ApplePaySessionPaymentRequest::ContactFields>::decode(Decoder& decoder, ApplePaySessionPaymentRequest::ContactFields& contactFields)
{
    if (!decoder.decode(contactFields.postalAddress))
        return false;
    if (!decoder.decode(contactFields.phone))
        return false;
    if (!decoder.decode(contactFields.email))
        return false;
    if (!decoder.decode(contactFields.name))
        return false;
    if (!decoder.decode(contactFields.phoneticName))
        return false;

    return true;
}

void ArgumentCoder<ApplePaySessionPaymentRequest::MerchantCapabilities>::encode(Encoder& encoder, const ApplePaySessionPaymentRequest::MerchantCapabilities& merchantCapabilities)
{
    encoder << merchantCapabilities.supports3DS;
    encoder << merchantCapabilities.supportsEMV;
    encoder << merchantCapabilities.supportsCredit;
    encoder << merchantCapabilities.supportsDebit;
}

bool ArgumentCoder<ApplePaySessionPaymentRequest::MerchantCapabilities>::decode(Decoder& decoder, ApplePaySessionPaymentRequest::MerchantCapabilities& merchantCapabilities)
{
    if (!decoder.decode(merchantCapabilities.supports3DS))
        return false;
    if (!decoder.decode(merchantCapabilities.supportsEMV))
        return false;
    if (!decoder.decode(merchantCapabilities.supportsCredit))
        return false;
    if (!decoder.decode(merchantCapabilities.supportsDebit))
        return false;

    return true;
}

void ArgumentCoder<RefPtr<ApplePayError>>::encode(Encoder& encoder, const RefPtr<ApplePayError>& error)
{
    encoder << !!error;
    if (error)
        encoder << *error;
}

Optional<RefPtr<ApplePayError>> ArgumentCoder<RefPtr<ApplePayError>>::decode(Decoder& decoder)
{
    Optional<bool> isValid;
    decoder >> isValid;
    if (!isValid)
        return std::nullopt;

    RefPtr<ApplePayError> error;
    if (!*isValid)
        return { nullptr };

    error = ApplePayError::decode(decoder);
    if (!error)
        return std::nullopt;
    return error;
}

void ArgumentCoder<WebCore::PaymentSessionError>::encode(Encoder& encoder, const WebCore::PaymentSessionError& error)
{
    encoder << error.platformError();
}

Optional<WebCore::PaymentSessionError> ArgumentCoder<WebCore::PaymentSessionError>::decode(Decoder& decoder)
{
    auto platformError = IPC::decode<NSError>(decoder);
    if (!platformError)
        return std::nullopt;

    return { WTFMove(*platformError) };
}

#endif // ENABLE(APPLEPAY)

void ArgumentCoder<WebCore::DictionaryPopupInfo>::encodePlatformData(Encoder& encoder, const WebCore::DictionaryPopupInfo& info)
{
    encoder << info.options << info.attributedString;
}

bool ArgumentCoder<WebCore::DictionaryPopupInfo>::decodePlatformData(Decoder& decoder, WebCore::DictionaryPopupInfo& result)
{
    if (!IPC::decode(decoder, result.options))
        return false;
    if (!IPC::decode(decoder, result.attributedString))
        return false;
    return true;
}

void ArgumentCoder<WebCore::FontAttributes>::encodePlatformData(Encoder& encoder, const WebCore::FontAttributes& attributes)
{
    encoder << attributes.font;
}

Optional<FontAttributes> ArgumentCoder<WebCore::FontAttributes>::decodePlatformData(Decoder& decoder, WebCore::FontAttributes& attributes)
{
    if (!IPC::decode(decoder, attributes.font))
        return std::nullopt;
    return attributes;
}

void ArgumentCoder<Ref<Font>>::encodePlatformData(Encoder& encoder, const Ref<WebCore::Font>& font)
{
    const auto& platformData = font->platformData();
    encoder << platformData.orientation();
    encoder << platformData.widthVariant();
    encoder << platformData.textRenderingMode();
    encoder << platformData.size();
    encoder << platformData.syntheticBold();
    encoder << platformData.syntheticOblique();

    auto ctFont = platformData.font();
    auto fontDescriptor = adoptCF(CTFontCopyFontDescriptor(ctFont));
    auto attributes = adoptCF(CTFontDescriptorCopyAttributes(fontDescriptor.get()));
    encoder << attributes;

    const auto& creationData = platformData.creationData();
    encoder << static_cast<bool>(creationData);
    if (creationData) {
        encoder << creationData->fontFaceData;
        encoder << creationData->itemInCollection;
    } else {
        auto referenceURL = adoptCF(static_cast<CFURLRef>(CTFontCopyAttribute(ctFont, kCTFontReferenceURLAttribute)));
        auto string = CFURLGetString(referenceURL.get());
        encoder << String(string);
        encoder << String(adoptCF(CTFontCopyPostScriptName(ctFont)).get());
    }
}

static RetainPtr<CTFontDescriptorRef> findFontDescriptor(const String& referenceURL, const String& postScriptName)
{
    auto url = adoptCF(CFURLCreateWithString(kCFAllocatorDefault, referenceURL.createCFString().get(), nullptr));
    if (!url)
        return nullptr;
    auto fontDescriptors = adoptCF(CTFontManagerCreateFontDescriptorsFromURL(url.get()));
    if (!fontDescriptors || !CFArrayGetCount(fontDescriptors.get()))
        return nullptr;
    if (CFArrayGetCount(fontDescriptors.get()) == 1)
        return static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(fontDescriptors.get(), 0));

    // There's supposed to only be a single item in the array, but we can be defensive here.
    for (CFIndex i = 0; i < CFArrayGetCount(fontDescriptors.get()); ++i) {
        auto fontDescriptor = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(fontDescriptors.get(), i));
        auto currentPostScriptName = adoptCF(static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontNameAttribute)));
        if (String(currentPostScriptName.get()) == postScriptName)
            return fontDescriptor;
    }
    return nullptr;
}

Optional<FontPlatformData> ArgumentCoder<Ref<Font>>::decodePlatformData(Decoder& decoder)
{
    Optional<FontOrientation> orientation;
    decoder >> orientation;
    if (!orientation.hasValue())
        return std::nullopt;

    Optional<FontWidthVariant> widthVariant;
    decoder >> widthVariant;
    if (!widthVariant.hasValue())
        return std::nullopt;

    Optional<TextRenderingMode> textRenderingMode;
    decoder >> textRenderingMode;
    if (!textRenderingMode.hasValue())
        return std::nullopt;

    Optional<float> size;
    decoder >> size;
    if (!size.hasValue())
        return std::nullopt;

    Optional<bool> syntheticBold;
    decoder >> syntheticBold;
    if (!syntheticBold.hasValue())
        return std::nullopt;

    Optional<bool> syntheticOblique;
    decoder >> syntheticOblique;
    if (!syntheticOblique.hasValue())
        return std::nullopt;

    Optional<RetainPtr<CFDictionaryRef>> attributes;
    decoder >> attributes;
    if (!attributes)
        return std::nullopt;

    Optional<bool> includesCreationData;
    decoder >> includesCreationData;
    if (!includesCreationData.hasValue())
        return std::nullopt;

    if (includesCreationData.value()) {
        Optional<Ref<SharedBuffer>> fontFaceData;
        decoder >> fontFaceData;
        if (!fontFaceData.hasValue())
            return std::nullopt;

        Optional<String> itemInCollection;
        decoder >> itemInCollection;
        if (!itemInCollection.hasValue())
            return std::nullopt;

        auto fontCustomPlatformData = createFontCustomPlatformData(fontFaceData.value(), itemInCollection.value());
        if (!fontCustomPlatformData)
            return std::nullopt;
        auto baseFontDescriptor = fontCustomPlatformData->fontDescriptor.get();
        if (!baseFontDescriptor)
            return std::nullopt;
        auto fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(baseFontDescriptor, attributes->get()));
        auto ctFont = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), size.value(), nullptr));

        auto creationData = FontPlatformData::CreationData { fontFaceData.value(), itemInCollection.value() };
        return FontPlatformData(ctFont.get(), size.value(), syntheticBold.value(), syntheticOblique.value(), orientation.value(), widthVariant.value(), textRenderingMode.value(), &creationData);
    }

    Optional<String> referenceURL;
    decoder >> referenceURL;
    if (!referenceURL.hasValue())
        return std::nullopt;

    Optional<String> postScriptName;
    decoder >> postScriptName;
    if (!postScriptName.hasValue())
        return std::nullopt;

    RetainPtr<CTFontDescriptorRef> fontDescriptor = findFontDescriptor(referenceURL.value(), postScriptName.value());
    if (!fontDescriptor)
        return std::nullopt;
    fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor.get(), attributes->get()));
    auto ctFont = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), size.value(), nullptr));

    return FontPlatformData(ctFont.get(), size.value(), syntheticBold.value(), syntheticOblique.value(), orientation.value(), widthVariant.value(), textRenderingMode.value());
}

void ArgumentCoder<WebCore::ResourceRequest>::encodePlatformData(Encoder& encoder, const WebCore::ResourceRequest& resourceRequest)
{
    auto requestToSerialize = retainPtr(resourceRequest.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody));

    if (Class requestClass = [requestToSerialize class]; UNLIKELY(requestClass != [NSURLRequest class] && requestClass != [NSMutableURLRequest class])) {
        WebCore::ResourceRequest request(requestToSerialize.get());
        request.replacePlatformRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
        requestToSerialize = retainPtr(request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody));
    }
    ASSERT([requestToSerialize class] == [NSURLRequest class] || [requestToSerialize class] == [NSMutableURLRequest class]);

    bool requestIsPresent = requestToSerialize;
    encoder << requestIsPresent;

    if (!requestIsPresent)
        return;

    // We don't send HTTP body over IPC for better performance.
    // Also, it's not always possible to do, as streams can only be created in process that does networking.
    if ([requestToSerialize HTTPBody] || [requestToSerialize HTTPBodyStream]) {
        auto mutableRequest = adoptNS([requestToSerialize mutableCopy]);
        [mutableRequest setHTTPBody:nil];
        [mutableRequest setHTTPBodyStream:nil];
        requestToSerialize = WTFMove(mutableRequest);
    }

    IPC::encode(encoder, requestToSerialize.get());

    encoder << resourceRequest.requester();
    encoder << resourceRequest.isAppBound();
}

bool ArgumentCoder<WebCore::ResourceRequest>::decodePlatformData(Decoder& decoder, WebCore::ResourceRequest& resourceRequest)
{
    bool requestIsPresent;
    if (!decoder.decode(requestIsPresent))
        return false;

    if (!requestIsPresent) {
        resourceRequest = WebCore::ResourceRequest();
        return true;
    }

    auto request = IPC::decode<NSURLRequest>(decoder, NSURLRequest.class);
    if (!request)
        return false;
    
    WebCore::ResourceRequest::Requester requester;
    if (!decoder.decode(requester))
        return false;

    bool isAppBound;
    if (!decoder.decode(isAppBound))
        return false;

    resourceRequest = WebCore::ResourceRequest(request->get());
    resourceRequest.setRequester(requester);
    resourceRequest.setIsAppBound(isAppBound);

    return true;
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void ArgumentCoder<WebCore::MediaPlaybackTargetContext>::encodePlatformData(Encoder& encoder, const MediaPlaybackTargetContext& target)
{
    if (target.type() == MediaPlaybackTargetContext::Type::AVOutputContext) {
        if ([PAL::getAVOutputContextClass() conformsToProtocol:@protocol(NSSecureCoding)])
            encoder << target.outputContext();
    } else if (target.type() == MediaPlaybackTargetContext::Type::SerializedAVOutputContext) {
        encoder << target.serializedOutputContext();
        encoder << target.hasActiveRoute();
    } else
        ASSERT_NOT_REACHED();
}

bool ArgumentCoder<WebCore::MediaPlaybackTargetContext>::decodePlatformData(Decoder& decoder, MediaPlaybackTargetContext::Type contextType, MediaPlaybackTargetContext& target)
{
    ASSERT(contextType != MediaPlaybackTargetContext::Type::Mock);

    if (contextType == MediaPlaybackTargetContext::Type::AVOutputContext) {
        if (![PAL::getAVOutputContextClass() conformsToProtocol:@protocol(NSSecureCoding)])
            return false;

        auto outputContext = IPC::decode<AVOutputContext>(decoder, PAL::getAVOutputContextClass());
        if (!outputContext)
            return false;

        target = WebCore::MediaPlaybackTargetContext { WTFMove(*outputContext) };
        return true;
    }

    if (contextType == MediaPlaybackTargetContext::Type::SerializedAVOutputContext) {
        RetainPtr<NSData> serializedOutputContext;
        if (!IPC::decode(decoder, serializedOutputContext) || !serializedOutputContext)
            return false;

        bool hasActiveRoute;
        if (!decoder.decode(hasActiveRoute))
            return false;

        target = WebCore::MediaPlaybackTargetContext { WTFMove(serializedOutputContext), hasActiveRoute };
        return true;
    }

    return false;
}
#endif

#if ENABLE(IMAGE_EXTRACTION) && ENABLE(DATA_DETECTION)

void ArgumentCoder<ImageExtractionDataDetectorInfo>::encodePlatformData(Encoder& encoder, const ImageExtractionDataDetectorInfo& info)
{
    encoder << info.result.get();
}

bool ArgumentCoder<ImageExtractionDataDetectorInfo>::decodePlatformData(Decoder& decoder, ImageExtractionDataDetectorInfo& result)
{
    auto scannerResult = IPC::decode<DDScannerResult>(decoder, @[ PAL::getDDScannerResultClass() ]);
    if (!scannerResult)
        return false;

    result.result = WTFMove(*scannerResult);
    return true;
}

#endif // ENABLE(IMAGE_EXTRACTION) && ENABLE(DATA_DETECTION)

} // namespace IPC
