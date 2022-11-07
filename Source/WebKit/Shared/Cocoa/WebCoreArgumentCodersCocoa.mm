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
#import <WebCore/DataDetectorElementInfo.h>
#import <WebCore/DictionaryPopupInfo.h>
#import <WebCore/Font.h>
#import <WebCore/FontAttributes.h>
#import <WebCore/FontCustomPlatformData.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/TextRecognitionResult.h>
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

#if USE(AVFOUNDATION)
#import <wtf/MachSendRight.h>
#endif

#if ENABLE(APPLE_PAY)
#import "DataReference.h"
#import <pal/cocoa/PassKitSoftLink.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#import <pal/cocoa/AVFoundationSoftLink.h>
#endif

#if ENABLE(DATA_DETECTION)
#import <pal/cocoa/DataDetectorsCoreSoftLink.h>
#endif

#if USE(AVFOUNDATION)
#import <WebCore/CoreVideoSoftLink.h>
#endif

#import <pal/cocoa/VisionKitCoreSoftLink.h>

namespace IPC {

void ArgumentCoder<WebCore::AttributedString>::encode(Encoder& encoder, const WebCore::AttributedString& attributedString)
{
    encoder << attributedString.string << attributedString.documentAttributes;
}

std::optional<WebCore::AttributedString> ArgumentCoder<WebCore::AttributedString>::decode(Decoder& decoder)
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

std::optional<WebCore::PaymentInstallmentConfiguration> ArgumentCoder<WebCore::PaymentInstallmentConfiguration>::decode(Decoder& decoder)
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

std::optional<WebCore::Payment> ArgumentCoder<WebCore::Payment>::decode(Decoder& decoder)
{
    auto payment = IPC::decode<PKPayment>(decoder, PAL::getPKPaymentClass());
    if (!payment)
        return std::nullopt;

    return WebCore::Payment { WTFMove(*payment) };
}

void ArgumentCoder<WebCore::PaymentContact>::encode(Encoder& encoder, const WebCore::PaymentContact& paymentContact)
{
    encoder << paymentContact.pkContact();
}

std::optional<WebCore::PaymentContact> ArgumentCoder<WebCore::PaymentContact>::decode(Decoder& decoder)
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

std::optional<WebCore::PaymentMerchantSession> ArgumentCoder<WebCore::PaymentMerchantSession>::decode(Decoder& decoder)
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

std::optional<WebCore::PaymentMethod> ArgumentCoder<WebCore::PaymentMethod>::decode(Decoder& decoder)
{
    auto paymentMethod = IPC::decode<PKPaymentMethod>(decoder, PAL::getPKPaymentMethodClass());
    if (!paymentMethod)
        return std::nullopt;

    return WebCore::PaymentMethod { WTFMove(*paymentMethod) };
}

void ArgumentCoder<WebCore::ApplePaySessionPaymentRequest>::encode(Encoder& encoder, const WebCore::ApplePaySessionPaymentRequest& request)
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
#if ENABLE(APPLE_PAY_COUPON_CODE)
    encoder << request.supportsCouponCode();
    encoder << request.couponCode();
#endif
#if ENABLE(APPLE_PAY_SHIPPING_CONTACT_EDITING_MODE)
    encoder << request.shippingContactEditingMode();
#endif
#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
    encoder << request.recurringPaymentRequest();
#endif
#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
    encoder << request.automaticReloadPaymentRequest();
#endif
#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
    encoder << request.multiTokenContexts();
#endif
}

bool ArgumentCoder<WebCore::ApplePaySessionPaymentRequest>::decode(Decoder& decoder, WebCore::ApplePaySessionPaymentRequest& request)
{
    String countryCode;
    if (!decoder.decode(countryCode))
        return false;
    request.setCountryCode(countryCode);

    String currencyCode;
    if (!decoder.decode(currencyCode))
        return false;
    request.setCurrencyCode(currencyCode);

    WebCore::ApplePaySessionPaymentRequest::ContactFields requiredBillingContactFields;
    if (!decoder.decode((requiredBillingContactFields)))
        return false;
    request.setRequiredBillingContactFields(requiredBillingContactFields);

    std::optional<WebCore::PaymentContact> billingContact;
    decoder >> billingContact;
    if (!billingContact)
        return false;
    request.setBillingContact(*billingContact);

    WebCore::ApplePaySessionPaymentRequest::ContactFields requiredShippingContactFields;
    if (!decoder.decode((requiredShippingContactFields)))
        return false;
    request.setRequiredShippingContactFields(requiredShippingContactFields);

    std::optional<WebCore::PaymentContact> shippingContact;
    decoder >> shippingContact;
    if (!shippingContact)
        return false;
    request.setShippingContact(*shippingContact);

    WebCore::ApplePaySessionPaymentRequest::MerchantCapabilities merchantCapabilities;
    if (!decoder.decode(merchantCapabilities))
        return false;
    request.setMerchantCapabilities(merchantCapabilities);

    Vector<String> supportedNetworks;
    if (!decoder.decode(supportedNetworks))
        return false;
    request.setSupportedNetworks(supportedNetworks);

    WebCore::ApplePaySessionPaymentRequest::ShippingType shippingType;
    if (!decoder.decode(shippingType))
        return false;
    request.setShippingType(shippingType);

    Vector<WebCore::ApplePayShippingMethod> shippingMethods;
    if (!decoder.decode(shippingMethods))
        return false;
    request.setShippingMethods(shippingMethods);

    Vector<WebCore::ApplePayLineItem> lineItems;
    if (!decoder.decode(lineItems))
        return false;
    request.setLineItems(lineItems);

    std::optional<WebCore::ApplePayLineItem> total;
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

    WebCore::ApplePaySessionPaymentRequest::Requester requester;
    if (!decoder.decode(requester))
        return false;
    request.setRequester(requester);
    
#if ENABLE(APPLE_PAY_INSTALLMENTS)
    std::optional<WebCore::PaymentInstallmentConfiguration> installmentConfiguration;
    decoder >> installmentConfiguration;
    if (!installmentConfiguration)
        return false;

    request.setInstallmentConfiguration(WTFMove(*installmentConfiguration));
#endif

#if ENABLE(APPLE_PAY_COUPON_CODE)
    std::optional<std::optional<bool>> supportsCouponCode;
    decoder >> supportsCouponCode;
    if (!supportsCouponCode)
        return false;
    request.setSupportsCouponCode(WTFMove(*supportsCouponCode));

    std::optional<String> couponCode;
    decoder >> couponCode;
    if (!couponCode)
        return false;
    request.setCouponCode(WTFMove(*couponCode));
#endif

#if ENABLE(APPLE_PAY_SHIPPING_CONTACT_EDITING_MODE)
    std::optional<std::optional<WebCore::ApplePayShippingContactEditingMode>> shippingContactEditingMode;
    decoder >> shippingContactEditingMode;
    if (!shippingContactEditingMode)
        return false;
    request.setShippingContactEditingMode(WTFMove(*shippingContactEditingMode));
#endif

#if ENABLE(APPLE_PAY_RECURRING_PAYMENTS)
    std::optional<std::optional<WebCore::ApplePayRecurringPaymentRequest>> recurringPaymentRequest;
    decoder >> recurringPaymentRequest;
    if (!recurringPaymentRequest)
        return false;
    request.setRecurringPaymentRequest(WTFMove(*recurringPaymentRequest));
#endif

#if ENABLE(APPLE_PAY_AUTOMATIC_RELOAD_PAYMENTS)
    std::optional<std::optional<WebCore::ApplePayAutomaticReloadPaymentRequest>> automaticReloadPaymentRequest;
    decoder >> automaticReloadPaymentRequest;
    if (!automaticReloadPaymentRequest)
        return false;
    request.setAutomaticReloadPaymentRequest(WTFMove(*automaticReloadPaymentRequest));
#endif

#if ENABLE(APPLE_PAY_MULTI_MERCHANT_PAYMENTS)
    std::optional<std::optional<Vector<WebCore::ApplePayPaymentTokenContext>>> multiTokenContexts;
    decoder >> multiTokenContexts;
    if (!multiTokenContexts)
        return false;
    request.setMultiTokenContexts(WTFMove(*multiTokenContexts));
#endif

    return true;
}

void ArgumentCoder<WebCore::ApplePaySessionPaymentRequest::ContactFields>::encode(Encoder& encoder, const WebCore::ApplePaySessionPaymentRequest::ContactFields& contactFields)
{
    encoder << contactFields.postalAddress;
    encoder << contactFields.phone;
    encoder << contactFields.email;
    encoder << contactFields.name;
    encoder << contactFields.phoneticName;
}

bool ArgumentCoder<WebCore::ApplePaySessionPaymentRequest::ContactFields>::decode(Decoder& decoder, WebCore::ApplePaySessionPaymentRequest::ContactFields& contactFields)
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

void ArgumentCoder<WebCore::ApplePaySessionPaymentRequest::MerchantCapabilities>::encode(Encoder& encoder, const WebCore::ApplePaySessionPaymentRequest::MerchantCapabilities& merchantCapabilities)
{
    encoder << merchantCapabilities.supports3DS;
    encoder << merchantCapabilities.supportsEMV;
    encoder << merchantCapabilities.supportsCredit;
    encoder << merchantCapabilities.supportsDebit;
}

bool ArgumentCoder<WebCore::ApplePaySessionPaymentRequest::MerchantCapabilities>::decode(Decoder& decoder, WebCore::ApplePaySessionPaymentRequest::MerchantCapabilities& merchantCapabilities)
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

void ArgumentCoder<RefPtr<WebCore::ApplePayError>>::encode(Encoder& encoder, const RefPtr<WebCore::ApplePayError>& error)
{
    encoder << !!error;
    if (error)
        encoder << *error;
}

std::optional<RefPtr<WebCore::ApplePayError>> ArgumentCoder<RefPtr<WebCore::ApplePayError>>::decode(Decoder& decoder)
{
    std::optional<bool> isValid;
    decoder >> isValid;
    if (!isValid)
        return std::nullopt;

    if (!*isValid)
        return { nullptr };

    std::optional<Ref<WebCore::ApplePayError>> error;
    decoder >> error;
    if (!error)
        return std::nullopt;

    return error;
}

void ArgumentCoder<WebCore::PaymentSessionError>::encode(Encoder& encoder, const WebCore::PaymentSessionError& error)
{
    encoder << error.platformError();
}

std::optional<WebCore::PaymentSessionError> ArgumentCoder<WebCore::PaymentSessionError>::decode(Decoder& decoder)
{
    auto platformError = IPC::decode<NSError>(decoder);
    if (!platformError)
        return std::nullopt;

    return { WTFMove(*platformError) };
}

#endif // ENABLE(APPLEPAY)

void ArgumentCoder<WebCore::Font>::encodePlatformData(Encoder& encoder, const WebCore::Font& font)
{
    const auto& platformData = font.platformData();
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
        auto options = CTFontDescriptorGetOptions(fontDescriptor.get());
        encoder << options;
        auto referenceURL = adoptCF(static_cast<CFURLRef>(CTFontCopyAttribute(ctFont, kCTFontReferenceURLAttribute)));
        auto string = CFURLGetString(referenceURL.get());
        encoder << string;
        encoder << adoptCF(CTFontCopyPostScriptName(ctFont)).get();
    }
}

static RetainPtr<CTFontDescriptorRef> findFontDescriptor(CFStringRef referenceURL, CFStringRef postScriptName)
{
    auto url = adoptCF(CFURLCreateWithString(kCFAllocatorDefault, referenceURL, nullptr));
    if (!url)
        return nullptr;
    auto fontDescriptors = adoptCF(CTFontManagerCreateFontDescriptorsFromURL(url.get()));
    if (!fontDescriptors || !CFArrayGetCount(fontDescriptors.get()))
        return nullptr;
    if (CFArrayGetCount(fontDescriptors.get()) == 1)
        return static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(fontDescriptors.get(), 0));

    for (CFIndex i = 0; i < CFArrayGetCount(fontDescriptors.get()); ++i) {
        auto fontDescriptor = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(fontDescriptors.get(), i));
        auto currentPostScriptName = adoptCF(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontNameAttribute));
        if (CFEqual(currentPostScriptName.get(), postScriptName))
            return fontDescriptor;
    }
    return nullptr;
}

static RetainPtr<CTFontRef> createCTFont(CFDictionaryRef attributes, float size, CTFontDescriptorOptions options, CFStringRef referenceURL, CFStringRef desiredPostScriptName)
{
    auto fontDescriptor = adoptCF(CTFontDescriptorCreateWithAttributes(attributes));
    if (fontDescriptor) {
        auto font = adoptCF(CTFontCreateWithFontDescriptorAndOptions(fontDescriptor.get(), size, nullptr, options));
        auto actualPostScriptName = adoptCF(CTFontCopyPostScriptName(font.get()));
        if (CFEqual(actualPostScriptName.get(), desiredPostScriptName))
            return font;
    }

    // CoreText couldn't round-trip the font.
    // We can fall back to doing our best to find it ourself.
    fontDescriptor = findFontDescriptor(referenceURL, desiredPostScriptName);
    if (!fontDescriptor) {
        ASSERT_NOT_REACHED();
        fontDescriptor = adoptCF(CTFontDescriptorCreateLastResort());
    }
    ASSERT(fontDescriptor);
    return adoptCF(CTFontCreateWithFontDescriptorAndOptions(fontDescriptor.get(), size, nullptr, options));
}

std::optional<WebCore::FontPlatformData> ArgumentCoder<WebCore::Font>::decodePlatformData(Decoder& decoder)
{
    std::optional<WebCore::FontOrientation> orientation;
    decoder >> orientation;
    if (!orientation)
        return std::nullopt;

    std::optional<WebCore::FontWidthVariant> widthVariant;
    decoder >> widthVariant;
    if (!widthVariant)
        return std::nullopt;

    std::optional<WebCore::TextRenderingMode> textRenderingMode;
    decoder >> textRenderingMode;
    if (!textRenderingMode)
        return std::nullopt;

    std::optional<float> size;
    decoder >> size;
    if (!size)
        return std::nullopt;

    std::optional<bool> syntheticBold;
    decoder >> syntheticBold;
    if (!syntheticBold)
        return std::nullopt;

    std::optional<bool> syntheticOblique;
    decoder >> syntheticOblique;
    if (!syntheticOblique)
        return std::nullopt;

    std::optional<RetainPtr<CFDictionaryRef>> attributes;
    decoder >> attributes;
    if (!attributes)
        return std::nullopt;

    std::optional<bool> includesCreationData;
    decoder >> includesCreationData;
    if (!includesCreationData)
        return std::nullopt;

    if (*includesCreationData) {
        std::optional<Ref<WebCore::FragmentedSharedBuffer>> fontFaceData;
        decoder >> fontFaceData;
        if (!fontFaceData)
            return std::nullopt;

        auto localFontFaceData = fontFaceData.value()->makeContiguous();

        std::optional<String> itemInCollection;
        decoder >> itemInCollection;
        if (!itemInCollection)
            return std::nullopt;

        auto fontCustomPlatformData = createFontCustomPlatformData(localFontFaceData, *itemInCollection);
        if (!fontCustomPlatformData)
            return std::nullopt;
        auto baseFontDescriptor = fontCustomPlatformData->fontDescriptor.get();
        if (!baseFontDescriptor)
            return std::nullopt;
        auto fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(baseFontDescriptor, attributes->get()));
        auto ctFont = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), *size, nullptr));

        auto creationData = WebCore::FontPlatformData::CreationData { localFontFaceData, *itemInCollection };
        return WebCore::FontPlatformData(ctFont.get(), *size, *syntheticBold, *syntheticOblique, *orientation, *widthVariant, *textRenderingMode, &creationData);
    }

    std::optional<CTFontDescriptorOptions> options;
    decoder >> options;
    if (!options)
        return std::nullopt;

    std::optional<RetainPtr<CFStringRef>> referenceURL;
    decoder >> referenceURL;
    if (!referenceURL || !*referenceURL)
        return std::nullopt;

    std::optional<RetainPtr<CFStringRef>> postScriptName;
    decoder >> postScriptName;
    if (!postScriptName || !*postScriptName)
        return std::nullopt;

    auto ctFont = createCTFont(attributes->get(), *size, *options, referenceURL->get(), postScriptName->get());
    if (!ctFont)
        return std::nullopt;

    return WebCore::FontPlatformData(ctFont.get(), *size, *syntheticBold, *syntheticOblique, *orientation, *widthVariant, *textRenderingMode);
}


#if ENABLE(DATA_DETECTION)

void ArgumentCoder<WebCore::DataDetectorElementInfo>::encode(Encoder& encoder, const WebCore::DataDetectorElementInfo& info)
{
    encoder << info.result.get();
    encoder << info.elementBounds;
}

std::optional<WebCore::DataDetectorElementInfo> ArgumentCoder<WebCore::DataDetectorElementInfo>::decode(Decoder& decoder)
{
    auto result = IPC::decode<DDScannerResult>(decoder, PAL::getDDScannerResultClass());
    if (!result)
        return std::nullopt;

    std::optional<WebCore::IntRect> elementBounds;
    decoder >> elementBounds;
    if (!elementBounds)
        return std::nullopt;

    return std::make_optional<WebCore::DataDetectorElementInfo>({ WTFMove(*result), WTFMove(*elementBounds) });
}

#endif // ENABLE(DATA_DETECTION)

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void ArgumentCoder<WebCore::MediaPlaybackTargetContext>::encodePlatformData(Encoder& encoder, const WebCore::MediaPlaybackTargetContext& target)
{
    if (target.type() == WebCore::MediaPlaybackTargetContext::Type::AVOutputContext) {
        if ([PAL::getAVOutputContextClass() conformsToProtocol:@protocol(NSSecureCoding)])
            encoder << target.outputContext();
    } else if (target.type() == WebCore::MediaPlaybackTargetContext::Type::SerializedAVOutputContext) {
        encoder << target.serializedOutputContext();
        encoder << target.hasActiveRoute();
    } else
        ASSERT_NOT_REACHED();
}

bool ArgumentCoder<WebCore::MediaPlaybackTargetContext>::decodePlatformData(Decoder& decoder, WebCore::MediaPlaybackTargetContext::Type contextType, WebCore::MediaPlaybackTargetContext& target)
{
    ASSERT(contextType != WebCore::MediaPlaybackTargetContext::Type::Mock);

    if (contextType == WebCore::MediaPlaybackTargetContext::Type::AVOutputContext) {
        if (![PAL::getAVOutputContextClass() conformsToProtocol:@protocol(NSSecureCoding)])
            return false;

        auto outputContext = IPC::decode<AVOutputContext>(decoder, PAL::getAVOutputContextClass());
        if (!outputContext)
            return false;

        target = WebCore::MediaPlaybackTargetContext { WTFMove(*outputContext) };
        return true;
    }

    if (contextType == WebCore::MediaPlaybackTargetContext::Type::SerializedAVOutputContext) {
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

#if ENABLE(IMAGE_ANALYSIS) && ENABLE(DATA_DETECTION)

void ArgumentCoder<WebCore::TextRecognitionDataDetector>::encodePlatformData(Encoder& encoder, const WebCore::TextRecognitionDataDetector& info)
{
    encoder << info.result.get();
}

bool ArgumentCoder<WebCore::TextRecognitionDataDetector>::decodePlatformData(Decoder& decoder, WebCore::TextRecognitionDataDetector& result)
{
    auto scannerResult = IPC::decode<DDScannerResult>(decoder, @[ PAL::getDDScannerResultClass() ]);
    if (!scannerResult)
        return false;

    result.result = WTFMove(*scannerResult);
    return true;
}

#endif // ENABLE(IMAGE_ANALYSIS) && ENABLE(DATA_DETECTION)

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

void ArgumentCoder<RetainPtr<VKCImageAnalysis>>::encode(Encoder& encoder, const RetainPtr<VKCImageAnalysis>& data)
{
    if (!PAL::isVisionKitCoreFrameworkAvailable())
        return;

    encoder << data.get();
}

std::optional<RetainPtr<VKCImageAnalysis>> ArgumentCoder<RetainPtr<VKCImageAnalysis>>::decode(Decoder& decoder)
{
    if (!PAL::isVisionKitCoreFrameworkAvailable())
        return nil;

    return IPC::decode<VKCImageAnalysis>(decoder, @[ PAL::getVKCImageAnalysisClass() ]);
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

#if USE(AVFOUNDATION)

void ArgumentCoder<RetainPtr<CVPixelBufferRef>>::encode(Encoder& encoder, const RetainPtr<CVPixelBufferRef>& pixelBuffer)
{
    // Use IOSurface as the means to transfer CVPixelBufferRef.
    MachSendRight sendRight;
    if (pixelBuffer) {
        if (auto surface = WebCore::CVPixelBufferGetIOSurface(pixelBuffer.get()))
            sendRight = MachSendRight::adopt(IOSurfaceCreateMachPort(surface));
    }
    encoder << WTFMove(sendRight);
}

std::optional<RetainPtr<CVPixelBufferRef>> ArgumentCoder<RetainPtr<CVPixelBufferRef>>::decode(Decoder& decoder)
{
    std::optional<MachSendRight> sendRight;
    decoder >> sendRight;
    if (!sendRight)
        return std::nullopt;
    RetainPtr<CVPixelBufferRef> pixelBuffer;
    if (!*sendRight)
        return pixelBuffer;
    {
        auto surface = adoptCF(IOSurfaceLookupFromMachPort(sendRight->sendRight()));
        if (!surface)
            return std::nullopt;
        CVPixelBufferRef rawBuffer = nullptr;
        auto status = WebCore::CVPixelBufferCreateWithIOSurface(kCFAllocatorDefault, surface.get(), nullptr, &rawBuffer);
        if (status != noErr || !rawBuffer)
            return std::nullopt;
        pixelBuffer = adoptCF(rawBuffer);
    }
    return pixelBuffer;
}

#endif

} // namespace IPC
