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

static void encodeNSError(Encoder& encoder, NSError *nsError)
{
    String domain = [nsError domain];
    encoder << domain;

    int64_t code = [nsError code];
    encoder << code;

    NSDictionary *userInfo = [nsError userInfo];

    RetainPtr<CFMutableDictionaryRef> filteredUserInfo = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, userInfo.count, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    [userInfo enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL*) {
        if ([value isKindOfClass:[NSString class]] || [value isKindOfClass:[NSURL class]] || [value isKindOfClass:[NSNumber class]])
            CFDictionarySetValue(filteredUserInfo.get(), (__bridge CFTypeRef)key, (__bridge CFTypeRef)value);
    }];

    if (NSArray *clientIdentityAndCertificates = [userInfo objectForKey:@"NSErrorClientCertificateChainKey"]) {
        ASSERT([clientIdentityAndCertificates isKindOfClass:[NSArray class]]);
        ASSERT(^{
            for (id object in clientIdentityAndCertificates) {
                if (CFGetTypeID((__bridge CFTypeRef)object) != SecIdentityGetTypeID() && CFGetTypeID((__bridge CFTypeRef)object) != SecCertificateGetTypeID())
                    return false;
            }
            return true;
        }());

        // Turn SecIdentity members into SecCertificate to strip out private key information.
        id clientCertificates = [NSMutableArray arrayWithCapacity:clientIdentityAndCertificates.count];
        for (id object in clientIdentityAndCertificates) {
            if (CFGetTypeID((__bridge CFTypeRef)object) != SecIdentityGetTypeID()) {
                [clientCertificates addObject:object];
                continue;
            }
            SecCertificateRef certificate = nil;
            OSStatus status = SecIdentityCopyCertificate((SecIdentityRef)object, &certificate);
            RetainPtr<SecCertificateRef> retainCertificate = adoptCF(certificate);
            // The SecIdentity member is the key information of this attribute. Without it, we should nil
            // the attribute.
            if (status != errSecSuccess) {
                LOG_ERROR("Failed to encode nsError.userInfo[NSErrorClientCertificateChainKey]: %d", status);
                clientCertificates = nil;
                break;
            }
            [clientCertificates addObject:(__bridge id)certificate];
        }
        CFDictionarySetValue(filteredUserInfo.get(), CFSTR("NSErrorClientCertificateChainKey"), (__bridge CFTypeRef)clientCertificates);
    }

    id peerCertificateChain = [userInfo objectForKey:@"NSErrorPeerCertificateChainKey"];
    if (!peerCertificateChain) {
        if (SecTrustRef peerTrust = (__bridge SecTrustRef)[userInfo objectForKey:NSURLErrorFailingURLPeerTrustErrorKey]) {
#if HAVE(SEC_TRUST_COPY_CERTIFICATE_CHAIN)
            peerCertificateChain = (__bridge NSArray *)adoptCF(SecTrustCopyCertificateChain(peerTrust)).autorelease();
#else
            CFIndex count = SecTrustGetCertificateCount(peerTrust);
            peerCertificateChain = [NSMutableArray arrayWithCapacity:count];
            for (CFIndex i = 0; i < count; ++i)
                [peerCertificateChain addObject:(__bridge id)SecTrustGetCertificateAtIndex(peerTrust, i)];
#endif
        }
    }
    ASSERT(!peerCertificateChain || [peerCertificateChain isKindOfClass:[NSArray class]]);
    if (peerCertificateChain)
        CFDictionarySetValue(filteredUserInfo.get(), CFSTR("NSErrorPeerCertificateChainKey"), (__bridge CFTypeRef)peerCertificateChain);

    if (SecTrustRef peerTrust = (__bridge SecTrustRef)[userInfo objectForKey:NSURLErrorFailingURLPeerTrustErrorKey])
        CFDictionarySetValue(filteredUserInfo.get(), (__bridge CFStringRef)NSURLErrorFailingURLPeerTrustErrorKey, peerTrust);

    encoder << static_cast<CFDictionaryRef>(filteredUserInfo.get());

    if (id underlyingError = [userInfo objectForKey:NSUnderlyingErrorKey]) {
        ASSERT([underlyingError isKindOfClass:[NSError class]]);
        encoder << true;
        encodeNSError(encoder, underlyingError);
    } else
        encoder << false;
}

void ArgumentCoder<WebCore::ResourceError>::encodePlatformData(Encoder& encoder, const WebCore::ResourceError& resourceError)
{
    encodeNSError(encoder, resourceError.nsError());
}

static RetainPtr<NSError> decodeNSError(Decoder& decoder)
{
    String domain;
    if (!decoder.decode(domain))
        return nil;

    int64_t code;
    if (!decoder.decode(code))
        return nil;

    RetainPtr<CFDictionaryRef> userInfo;
    if (!decoder.decode(userInfo) || !userInfo)
        return nil;

    bool hasUnderlyingError = false;
    if (!decoder.decode(hasUnderlyingError))
        return nil;

    if (hasUnderlyingError) {
        auto underlyingNSError = decodeNSError(decoder);
        if (!underlyingNSError)
            return nil;

        auto mutableUserInfo = adoptCF(CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(userInfo.get()) + 1, userInfo.get()));
        CFDictionarySetValue(mutableUserInfo.get(), (__bridge CFStringRef)NSUnderlyingErrorKey, (__bridge CFTypeRef)underlyingNSError.get());
        userInfo = WTFMove(mutableUserInfo);
    }

    return adoptNS([[NSError alloc] initWithDomain:domain code:code userInfo:(__bridge NSDictionary *)userInfo.get()]);
}

bool ArgumentCoder<WebCore::ResourceError>::decodePlatformData(Decoder& decoder, WebCore::ResourceError& resourceError)
{
    auto nsError = decodeNSError(decoder);
    if (!nsError)
        return false;

    resourceError = WebCore::ResourceError(nsError.get());
    return true;
}

void ArgumentCoder<WebCore::ProtectionSpace>::encodePlatformData(Encoder& encoder, const WebCore::ProtectionSpace& space)
{
    encoder << space.nsSpace();
}

bool ArgumentCoder<WebCore::ProtectionSpace>::decodePlatformData(Decoder& decoder, WebCore::ProtectionSpace& space)
{
    auto platformData = IPC::decode<NSURLProtectionSpace>(decoder);
    if (!platformData)
        return false;

    space = WebCore::ProtectionSpace { platformData->get() };
    return true;
}

void ArgumentCoder<WebCore::Credential>::encodePlatformData(Encoder& encoder, const WebCore::Credential& credential)
{
    NSURLCredential *nsCredential = credential.nsCredential();
    encoder << nsCredential;
}

bool ArgumentCoder<WebCore::Credential>::decodePlatformData(Decoder& decoder, WebCore::Credential& credential)
{
    auto nsCredential = IPC::decode<NSURLCredential>(decoder);
    if (!nsCredential)
        return false;
    credential = WebCore::Credential { nsCredential->get() };
    return true;
}

void ArgumentCoder<WebCore::KeypressCommand>::encode(Encoder& encoder, const WebCore::KeypressCommand& keypressCommand)
{
    encoder << keypressCommand.commandName << keypressCommand.text;
}
    
std::optional<WebCore::KeypressCommand> ArgumentCoder<WebCore::KeypressCommand>::decode(Decoder& decoder)
{
    std::optional<String> commandName;
    decoder >> commandName;
    if (!commandName)
        return std::nullopt;
    
    std::optional<String> text;
    decoder >> text;
    if (!text)
        return std::nullopt;
    
    WebCore::KeypressCommand command;
    command.commandName = WTFMove(*commandName);
    command.text = WTFMove(*text);
    return WTFMove(command);
}

#if ENABLE(CONTENT_FILTERING)

void ArgumentCoder<WebCore::ContentFilterUnblockHandler>::encode(Encoder& encoder, const WebCore::ContentFilterUnblockHandler& contentFilterUnblockHandler)
{
    auto archiver = adoptNS([[NSKeyedArchiver alloc] initRequiringSecureCoding:YES]);
    contentFilterUnblockHandler.encode(archiver.get());
    encoder << (__bridge CFDataRef)archiver.get().encodedData;
}

bool ArgumentCoder<WebCore::ContentFilterUnblockHandler>::decode(Decoder& decoder, WebCore::ContentFilterUnblockHandler& contentFilterUnblockHandler)
{
    RetainPtr<CFDataRef> data;
    if (!decoder.decode(data) || !data)
        return false;

    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:(__bridge NSData *)data.get() error:nullptr]);
    unarchiver.get().decodingFailurePolicy = NSDecodingFailurePolicyRaiseException;
    if (!WebCore::ContentFilterUnblockHandler::decode(unarchiver.get(), contentFilterUnblockHandler))
        return false;

    [unarchiver finishDecoding];
    return true;
}

#endif

#if ENABLE(VIDEO)
void ArgumentCoder<WebCore::SerializedPlatformDataCueValue>::encodePlatformData(Encoder& encoder, const WebCore::SerializedPlatformDataCueValue& value)
{
    ASSERT(value.platformType() == WebCore::SerializedPlatformDataCueValue::PlatformType::ObjC);
    if (value.platformType() == WebCore::SerializedPlatformDataCueValue::PlatformType::ObjC)
        encodeObject(encoder, value.nativeValue().get());
}

std::optional<WebCore::SerializedPlatformDataCueValue>  ArgumentCoder<WebCore::SerializedPlatformDataCueValue>::decodePlatformData(Decoder& decoder, WebCore::SerializedPlatformDataCueValue::PlatformType platformType)
{
    ASSERT(platformType == WebCore::SerializedPlatformDataCueValue::PlatformType::ObjC);

    if (platformType != WebCore::SerializedPlatformDataCueValue::PlatformType::ObjC)
        return std::nullopt;

    auto object = decodeObject(decoder, WebCore::SerializedPlatformDataCueMac::allowedClassesForNativeValues());
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
