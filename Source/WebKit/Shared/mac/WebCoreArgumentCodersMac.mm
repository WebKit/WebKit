/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Company 100 Inc. All rights reserved.
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
#import "CoreIPCError.h"
#import "DataReference.h"
#import "StreamConnectionEncoder.h"
#import <WebCore/AppKitControlSystemImage.h>
#import <WebCore/CertificateInfo.h>
#import <WebCore/ContentFilterUnblockHandler.h>
#import <WebCore/Credential.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/ProtectionSpace.h>
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/ScrollbarTrackCornerSystemImageMac.h>
#import <WebCore/SerializedPlatformDataCueMac.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/MachSendRight.h>
#import <wtf/cf/TypeCastsCF.h>

namespace IPC {

void ArgumentCoder<WebCore::ResourceError>::encodePlatformData(Encoder& encoder, const WebCore::ResourceError& resourceError)
{
    encoder << WebKit::CoreIPCError(resourceError.nsError());
}



bool ArgumentCoder<WebCore::ResourceError>::decodePlatformData(Decoder& decoder, WebCore::ResourceError& resourceError)
{
    std::optional<WebKit::CoreIPCError> error;
    decoder >> error;
    if (!error)
        return false;
    resourceError = WebCore::ResourceError(error->toID().get());
    return true;
}

void ArgumentCoder<WebCore::Credential>::encodePlatformData(Encoder& encoder, const WebCore::Credential& credential)
{
    NSURLCredential *nsCredential = credential.nsCredential();
    encoder << nsCredential;
}

bool ArgumentCoder<WebCore::Credential>::decodePlatformData(Decoder& decoder, WebCore::Credential& credential)
{
    std::optional<RetainPtr<NSURLCredential>> nsCredential = decoder.decode<RetainPtr<NSURLCredential>>();
    if (!nsCredential)
        return false;
    credential = WebCore::Credential { nsCredential->get() };
    return true;
}

#if ENABLE(VIDEO)
void ArgumentCoder<WebCore::SerializedPlatformDataCueValue>::encodePlatformData(Encoder& encoder, const WebCore::SerializedPlatformDataCueValue& value)
{
    ASSERT(value.platformType() == WebCore::SerializedPlatformDataCueValue::PlatformType::ObjC);
    if (value.platformType() == WebCore::SerializedPlatformDataCueValue::PlatformType::ObjC)
        encodeObjectWithWrapper(encoder, value.nativeValue().get());
}

std::optional<WebCore::SerializedPlatformDataCueValue>  ArgumentCoder<WebCore::SerializedPlatformDataCueValue>::decodePlatformData(Decoder& decoder, WebCore::SerializedPlatformDataCueValue::PlatformType platformType)
{
    ASSERT(platformType == WebCore::SerializedPlatformDataCueValue::PlatformType::ObjC);

    if (platformType != WebCore::SerializedPlatformDataCueValue::PlatformType::ObjC)
        return std::nullopt;

    auto object = decodeObjectFromWrapper(decoder, WebCore::SerializedPlatformDataCueMac::allowedClassesForNativeValues());
    if (!object)
        return std::nullopt;

    return WebCore::SerializedPlatformDataCueValue { platformType, object.value().get() };
}
#endif

#if USE(APPKIT)

template<typename Encoder>
void ArgumentCoder<WebCore::AppKitControlSystemImage>::encode(Encoder& encoder, const WebCore::AppKitControlSystemImage& systemImage)
{
    encoder << systemImage.controlType();
    encoder << systemImage.useDarkAppearance();
    encoder << systemImage.tintColor();

    switch (systemImage.controlType()) {
    case WebCore::AppKitControlSystemImageType::ScrollbarTrackCorner:
        return;
    }

    ASSERT_NOT_REACHED();
}

template
void ArgumentCoder<WebCore::AppKitControlSystemImage>::encode<Encoder>(Encoder&, const WebCore::AppKitControlSystemImage&);
template
void ArgumentCoder<WebCore::AppKitControlSystemImage>::encode<StreamConnectionEncoder>(StreamConnectionEncoder&, const WebCore::AppKitControlSystemImage&);

std::optional<Ref<WebCore::AppKitControlSystemImage>> ArgumentCoder<WebCore::AppKitControlSystemImage>::decode(Decoder& decoder)
{
    std::optional<WebCore::AppKitControlSystemImageType> controlType;
    decoder >> controlType;
    if (!controlType)
        return std::nullopt;

    std::optional<bool> useDarkAppearance;
    decoder >> useDarkAppearance;
    if (!useDarkAppearance)
        return std::nullopt;

    std::optional<WebCore::Color> tintColor;
    decoder >> tintColor;
    if (!tintColor)
        return std::nullopt;

    std::optional<Ref<WebCore::AppKitControlSystemImage>> control;
    switch (*controlType) {
    case WebCore::AppKitControlSystemImageType::ScrollbarTrackCorner:
        control = WebCore::ScrollbarTrackCornerSystemImageMac::create();
    }

    if (!control)
        return std::nullopt;

    (*control)->setTintColor(*tintColor);
    (*control)->setUseDarkAppearance(*useDarkAppearance);

    return control;
}

#endif // USE(APPKIT)

} // namespace IPC
