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
#import <WebCore/CertificateInfo.h>
#import <WebCore/ContentFilterUnblockHandler.h>
#import <WebCore/Credential.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/ProtectionSpace.h>
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/SerializedPlatformDataCueMac.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/MachSendRight.h>
#import <wtf/cf/TypeCastsCF.h>

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#import <WebCore/MediaPlaybackTargetContext.h>
#import <objc/runtime.h>
#import <pal/cocoa/AVFoundationSoftLink.h>
#endif

namespace IPC {

void ArgumentCoder<WebCore::CertificateInfo>::encode(Encoder& encoder, const WebCore::CertificateInfo& certificateInfo)
{
    encoder << certificateInfo.type();

    switch (certificateInfo.type()) {
#if HAVE(SEC_TRUST_SERIALIZATION)
    case WebCore::CertificateInfo::Type::Trust:
        IPC::encode(encoder, certificateInfo.trust());
        break;
#endif
    case WebCore::CertificateInfo::Type::CertificateChain:
        IPC::encode(encoder, certificateInfo.certificateChain());
        break;
    case WebCore::CertificateInfo::Type::None:
        // Do nothing.
        break;
    }
}

bool ArgumentCoder<WebCore::CertificateInfo>::decode(Decoder& decoder, WebCore::CertificateInfo& certificateInfo)
{
    WebCore::CertificateInfo::Type certificateInfoType;
    if (!decoder.decode(certificateInfoType))
        return false;

    switch (certificateInfoType) {
#if HAVE(SEC_TRUST_SERIALIZATION)
    case WebCore::CertificateInfo::Type::Trust: {
        RetainPtr<SecTrustRef> trust;
        if (!IPC::decode(decoder, trust) || !trust)
            return false;

        certificateInfo = WebCore::CertificateInfo(WTFMove(trust));
        return true;
    }
#endif
    case WebCore::CertificateInfo::Type::CertificateChain: {
        RetainPtr<CFArrayRef> certificateChain;
        if (!IPC::decode(decoder, certificateChain) || !certificateChain)
            return false;

        certificateInfo = WebCore::CertificateInfo(WTFMove(certificateChain));
        return true;
    }    
    case WebCore::CertificateInfo::Type::None:
        // Do nothing.
        break;
    }

    return true;
}

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
            CFIndex count = SecTrustGetCertificateCount(peerTrust);
            peerCertificateChain = [NSMutableArray arrayWithCapacity:count];
            for (CFIndex i = 0; i < count; ++i)
                [peerCertificateChain addObject:(__bridge id)SecTrustGetCertificateAtIndex(peerTrust, i)];
        }
    }
    ASSERT(!peerCertificateChain || [peerCertificateChain isKindOfClass:[NSArray class]]);
    if (peerCertificateChain)
        CFDictionarySetValue(filteredUserInfo.get(), CFSTR("NSErrorPeerCertificateChainKey"), (__bridge CFTypeRef)peerCertificateChain);

#if HAVE(SEC_TRUST_SERIALIZATION)
    if (SecTrustRef peerTrust = (__bridge SecTrustRef)[userInfo objectForKey:NSURLErrorFailingURLPeerTrustErrorKey])
        CFDictionarySetValue(filteredUserInfo.get(), (__bridge CFStringRef)NSURLErrorFailingURLPeerTrustErrorKey, peerTrust);
#endif

    IPC::encode(encoder, filteredUserInfo.get());

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
    if (!IPC::decode(decoder, userInfo) || !userInfo)
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

void ArgumentCoder<MachSendRight>::encode(Encoder& encoder, const MachSendRight& sendRight)
{
    encoder << Attachment(sendRight.copySendRight().leakSendRight(), MACH_MSG_TYPE_MOVE_SEND);
}

void ArgumentCoder<MachSendRight>::encode(Encoder& encoder, MachSendRight&& sendRight)
{
    encoder << Attachment(sendRight.leakSendRight(), MACH_MSG_TYPE_MOVE_SEND);
}

bool ArgumentCoder<MachSendRight>::decode(Decoder& decoder, MachSendRight& sendRight)
{
    Attachment attachment;
    if (!decoder.decode(attachment))
        return false;

    if (attachment.disposition() != MACH_MSG_TYPE_MOVE_SEND)
        return false;

    sendRight = MachSendRight::adopt(attachment.port());
    return true;
}

void ArgumentCoder<WebCore::KeypressCommand>::encode(Encoder& encoder, const WebCore::KeypressCommand& keypressCommand)
{
    encoder << keypressCommand.commandName << keypressCommand.text;
}
    
Optional<WebCore::KeypressCommand> ArgumentCoder<WebCore::KeypressCommand>::decode(Decoder& decoder)
{
    Optional<String> commandName;
    decoder >> commandName;
    if (!commandName)
        return WTF::nullopt;
    
    Optional<String> text;
    decoder >> text;
    if (!text)
        return WTF::nullopt;
    
    WebCore::KeypressCommand command;
    command.commandName = WTFMove(*commandName);
    command.text = WTFMove(*text);
    return WTFMove(command);
}

void ArgumentCoder<CGRect>::encode(Encoder& encoder, CGRect rect)
{
    encoder << rect.origin << rect.size;
}

Optional<CGRect> ArgumentCoder<CGRect>::decode(Decoder& decoder)
{
    Optional<CGPoint> origin;
    decoder >> origin;
    if (!origin)
        return { };

    Optional<CGSize> size;
    decoder >> size;
    if (!size)
        return { };

    return CGRect { *origin, *size };
}

void ArgumentCoder<CGSize>::encode(Encoder& encoder, CGSize size)
{
    encoder << size.width << size.height;
}

Optional<CGSize> ArgumentCoder<CGSize>::decode(Decoder& decoder)
{
    CGSize size;
    if (!decoder.decode(size.width))
        return { };
    if (!decoder.decode(size.height))
        return { };
    return size;
}

void ArgumentCoder<CGPoint>::encode(Encoder& encoder, CGPoint point)
{
    encoder << point.x << point.y;
}

Optional<CGPoint> ArgumentCoder<CGPoint>::decode(Decoder& decoder)
{
    CGPoint point;
    if (!decoder.decode(point.x))
        return { };
    if (!decoder.decode(point.y))
        return { };
    return point;
}

void ArgumentCoder<CGAffineTransform>::encode(Encoder& encoder, CGAffineTransform transform)
{
    encoder << transform.a << transform.b << transform.c << transform.d << transform.tx << transform.ty;
}

Optional<CGAffineTransform> ArgumentCoder<CGAffineTransform>::decode(Decoder& decoder)
{
    CGAffineTransform transform;
    if (!decoder.decode(transform.a))
        return { };
    if (!decoder.decode(transform.b))
        return { };
    if (!decoder.decode(transform.c))
        return { };
    if (!decoder.decode(transform.d))
        return { };
    if (!decoder.decode(transform.tx))
        return { };
    if (!decoder.decode(transform.ty))
        return { };
    return transform;
}

#if ENABLE(CONTENT_FILTERING)

void ArgumentCoder<WebCore::ContentFilterUnblockHandler>::encode(Encoder& encoder, const WebCore::ContentFilterUnblockHandler& contentFilterUnblockHandler)
{
    auto archiver = adoptNS([[NSKeyedArchiver alloc] initRequiringSecureCoding:YES]);
    contentFilterUnblockHandler.encode(archiver.get());
    IPC::encode(encoder, (__bridge CFDataRef)archiver.get().encodedData);
}

bool ArgumentCoder<WebCore::ContentFilterUnblockHandler>::decode(Decoder& decoder, WebCore::ContentFilterUnblockHandler& contentFilterUnblockHandler)
{
    RetainPtr<CFDataRef> data;
    if (!IPC::decode(decoder, data) || !data)
        return false;

    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:(__bridge NSData *)data.get() error:nullptr]);
    unarchiver.get().decodingFailurePolicy = NSDecodingFailurePolicyRaiseException;
    if (!WebCore::ContentFilterUnblockHandler::decode(unarchiver.get(), contentFilterUnblockHandler))
        return false;

    [unarchiver finishDecoding];
    return true;
}

#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

void ArgumentCoder<WebCore::MediaPlaybackTargetContext>::encodePlatformData(Encoder& encoder, const WebCore::MediaPlaybackTargetContext& target)
{
    if ([PAL::getAVOutputContextClass() conformsToProtocol:@protocol(NSSecureCoding)])
        encoder << target.avOutputContext();
}

bool ArgumentCoder<WebCore::MediaPlaybackTargetContext>::decodePlatformData(Decoder& decoder, WebCore::MediaPlaybackTargetContext& target)
{
    if (![PAL::getAVOutputContextClass() conformsToProtocol:@protocol(NSSecureCoding)])
        return false;

    auto context = IPC::decode<AVOutputContext>(decoder, PAL::getAVOutputContextClass());
    if (!context)
        return false;

    target = WebCore::MediaPlaybackTargetContext { context->get() };
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

Optional<WebCore::SerializedPlatformDataCueValue>  ArgumentCoder<WebCore::SerializedPlatformDataCueValue>::decodePlatformData(Decoder& decoder, WebCore::SerializedPlatformDataCueValue::PlatformType platformType)
{
    ASSERT(platformType == WebCore::SerializedPlatformDataCueValue::PlatformType::ObjC);

    if (platformType != WebCore::SerializedPlatformDataCueValue::PlatformType::ObjC)
        return WTF::nullopt;

    auto object = decodeObject(decoder, WebCore::SerializedPlatformDataCueMac::allowedClassesForNativeValues());
    if (!object)
        return WTF::nullopt;

    return WebCore::SerializedPlatformDataCueValue { platformType, object.value().get() };
}
#endif

} // namespace IPC
