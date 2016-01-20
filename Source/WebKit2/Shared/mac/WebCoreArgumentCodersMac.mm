/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#import "DataReference.h"
#import "WebKitSystemInterface.h"
#import <WebCore/CertificateInfo.h>
#import <WebCore/ContentFilterUnblockHandler.h>
#import <WebCore/Credential.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/MachSendRight.h>
#import <WebCore/ProtectionSpace.h>
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceRequest.h>

#if USE(CFNETWORK)
#import <CFNetwork/CFURLRequest.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#import <WebCore/AVFoundationSPI.h>
#import <WebCore/MediaPlaybackTargetContext.h>
#import <WebCore/SoftLinking.h>
#import <objc/runtime.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVOutputContext)
#endif

using namespace WebCore;

namespace IPC {

#if USE(CFNETWORK)
void ArgumentCoder<ResourceRequest>::encodePlatformData(ArgumentEncoder& encoder, const ResourceRequest& resourceRequest)
{
    RetainPtr<CFURLRequestRef> requestToSerialize = resourceRequest.cfURLRequest(DoNotUpdateHTTPBody);

    bool requestIsPresent = requestToSerialize;
    encoder << requestIsPresent;

    if (!requestIsPresent)
        return;

    // We don't send HTTP body over IPC for better performance.
    // Also, it's not always possible to do, as streams can only be created in process that does networking.
    RetainPtr<CFDataRef> requestHTTPBody = adoptCF(CFURLRequestCopyHTTPRequestBody(requestToSerialize.get()));
    RetainPtr<CFReadStreamRef> requestHTTPBodyStream = adoptCF(CFURLRequestCopyHTTPRequestBodyStream(requestToSerialize.get()));
    if (requestHTTPBody || requestHTTPBodyStream) {
        CFMutableURLRequestRef mutableRequest = CFURLRequestCreateMutableCopy(0, requestToSerialize.get());
        requestToSerialize = adoptCF(mutableRequest);
        CFURLRequestSetHTTPRequestBody(mutableRequest, nil);
        CFURLRequestSetHTTPRequestBodyStream(mutableRequest, nil);
    }

    RetainPtr<CFDictionaryRef> dictionary = adoptCF(WKCFURLRequestCreateSerializableRepresentation(requestToSerialize.get(), IPC::tokenNullTypeRef()));
    IPC::encode(encoder, dictionary.get());

    // The fallback array is part of CFURLRequest, but it is not encoded by WKCFURLRequestCreateSerializableRepresentation.
    encoder << resourceRequest.responseContentDispositionEncodingFallbackArray();
    encoder.encodeEnum(resourceRequest.requester());
}
#else
void ArgumentCoder<ResourceRequest>::encodePlatformData(ArgumentEncoder& encoder, const ResourceRequest& resourceRequest)
{
    RetainPtr<NSURLRequest> requestToSerialize = resourceRequest.nsURLRequest(DoNotUpdateHTTPBody);

    bool requestIsPresent = requestToSerialize;
    encoder << requestIsPresent;

    if (!requestIsPresent)
        return;

    // We don't send HTTP body over IPC for better performance.
    // Also, it's not always possible to do, as streams can only be created in process that does networking.
    if ([requestToSerialize HTTPBody] || [requestToSerialize HTTPBodyStream]) {
        requestToSerialize = adoptNS([requestToSerialize mutableCopy]);
        [(NSMutableURLRequest *)requestToSerialize setHTTPBody:nil];
        [(NSMutableURLRequest *)requestToSerialize setHTTPBodyStream:nil];
    }

    RetainPtr<CFDictionaryRef> dictionary = adoptCF(WKNSURLRequestCreateSerializableRepresentation(requestToSerialize.get(), IPC::tokenNullTypeRef()));
    IPC::encode(encoder, dictionary.get());

    // The fallback array is part of NSURLRequest, but it is not encoded by WKNSURLRequestCreateSerializableRepresentation.
    encoder << resourceRequest.responseContentDispositionEncodingFallbackArray();
    encoder.encodeEnum(resourceRequest.requester());
}
#endif

bool ArgumentCoder<ResourceRequest>::decodePlatformData(ArgumentDecoder& decoder, ResourceRequest& resourceRequest)
{
    bool requestIsPresent;
    if (!decoder.decode(requestIsPresent))
        return false;

    if (!requestIsPresent) {
        resourceRequest = ResourceRequest();
        return true;
    }

    RetainPtr<CFDictionaryRef> dictionary;
    if (!IPC::decode(decoder, dictionary))
        return false;

#if USE(CFNETWORK)
    RetainPtr<CFURLRequestRef> cfURLRequest = adoptCF(WKCreateCFURLRequestFromSerializableRepresentation(dictionary.get(), IPC::tokenNullTypeRef()));
    if (!cfURLRequest)
        return false;

    resourceRequest = ResourceRequest(cfURLRequest.get());
#else
    RetainPtr<NSURLRequest> nsURLRequest = WKNSURLRequestFromSerializableRepresentation(dictionary.get(), IPC::tokenNullTypeRef());
    if (!nsURLRequest)
        return false;

    resourceRequest = ResourceRequest(nsURLRequest.get());
#endif
    
    Vector<String> responseContentDispositionEncodingFallbackArray;
    if (!decoder.decode(responseContentDispositionEncodingFallbackArray))
        return false;

    resourceRequest.setResponseContentDispositionEncodingFallbackArray(
        responseContentDispositionEncodingFallbackArray.size() > 0 ? responseContentDispositionEncodingFallbackArray[0] : String(),
        responseContentDispositionEncodingFallbackArray.size() > 1 ? responseContentDispositionEncodingFallbackArray[1] : String(),
        responseContentDispositionEncodingFallbackArray.size() > 2 ? responseContentDispositionEncodingFallbackArray[2] : String()
    );

    ResourceRequest::Requester requester;
    if (!decoder.decodeEnum(requester))
        return false;
    resourceRequest.setRequester(requester);

    return true;
}

void ArgumentCoder<CertificateInfo>::encode(ArgumentEncoder& encoder, const CertificateInfo& certificateInfo)
{
    CFArrayRef certificateChain = certificateInfo.certificateChain();
    if (!certificateChain) {
        encoder << false;
        return;
    }

    encoder << true;
    IPC::encode(encoder, certificateChain);
}

bool ArgumentCoder<CertificateInfo>::decode(ArgumentDecoder& decoder, CertificateInfo& certificateInfo)
{
    bool hasCertificateChain;
    if (!decoder.decode(hasCertificateChain))
        return false;

    if (!hasCertificateChain)
        return true;

    RetainPtr<CFArrayRef> certificateChain;
    if (!IPC::decode(decoder, certificateChain))
        return false;

    certificateInfo.setCertificateChain(certificateChain.get());

    return true;
}

static void encodeNSError(ArgumentEncoder& encoder, NSError *nsError)
{
    String domain = [nsError domain];
    encoder << domain;

    int64_t code = [nsError code];
    encoder << code;

    NSDictionary *userInfo = [nsError userInfo];

    RetainPtr<CFMutableDictionaryRef> filteredUserInfo = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, userInfo.count, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    [userInfo enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL*) {
        if ([value isKindOfClass:[NSString class]] || [value isKindOfClass:[NSURL class]] || [value isKindOfClass:[NSNumber class]])
            CFDictionarySetValue(filteredUserInfo.get(), key, value);
    }];

    if (NSArray *clientIdentityAndCertificates = [userInfo objectForKey:@"NSErrorClientCertificateChainKey"]) {
        ASSERT([clientIdentityAndCertificates isKindOfClass:[NSArray class]]);
        ASSERT(^{
            for (id object in clientIdentityAndCertificates) {
                if (CFGetTypeID(object) != SecIdentityGetTypeID() && CFGetTypeID(object) != SecCertificateGetTypeID())
                    return false;
            }
            return true;
        }());

        CFDictionarySetValue(filteredUserInfo.get(), @"NSErrorClientCertificateChainKey", clientIdentityAndCertificates);
    };

    IPC::encode(encoder, filteredUserInfo.get());

    id peerCertificateChain = [userInfo objectForKey:@"NSErrorPeerCertificateChainKey"];
    if (!peerCertificateChain) {
        if (SecTrustRef peerTrust = (SecTrustRef)[userInfo objectForKey:NSURLErrorFailingURLPeerTrustErrorKey]) {
            CFIndex count = SecTrustGetCertificateCount(peerTrust);
            peerCertificateChain = [NSMutableArray arrayWithCapacity:count];
            for (CFIndex i = 0; i < count; ++i)
                [peerCertificateChain addObject:(id)SecTrustGetCertificateAtIndex(peerTrust, i)];
        }
    }
    ASSERT(!peerCertificateChain || [peerCertificateChain isKindOfClass:[NSArray class]]);
    encoder << CertificateInfo((CFArrayRef)peerCertificateChain);

    if (id underlyingError = [userInfo objectForKey:NSUnderlyingErrorKey]) {
        ASSERT([underlyingError isKindOfClass:[NSError class]]);
        encoder << true;
        encodeNSError(encoder, underlyingError);
    } else
        encoder << false;
}

void ArgumentCoder<ResourceError>::encodePlatformData(ArgumentEncoder& encoder, const ResourceError& resourceError)
{
    bool errorIsNull = resourceError.isNull();
    encoder << errorIsNull;

    if (errorIsNull)
        return;

    NSError *nsError = resourceError.nsError();
    encodeNSError(encoder, nsError);
}

static bool decodeNSError(ArgumentDecoder& decoder, RetainPtr<NSError>& nsError)
{
    String domain;
    if (!decoder.decode(domain))
        return false;

    int64_t code;
    if (!decoder.decode(code))
        return false;

    RetainPtr<CFDictionaryRef> userInfo;
    if (!IPC::decode(decoder, userInfo))
        return false;

    CertificateInfo certificate;
    if (!decoder.decode(certificate))
        return false;

    if (certificate.certificateChain()) {
        userInfo = adoptCF(CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(userInfo.get()) + 1, userInfo.get()));
        CFDictionarySetValue((CFMutableDictionaryRef)userInfo.get(), CFSTR("NSErrorPeerCertificateChainKey"), (CFArrayRef)certificate.certificateChain());
    }

    bool hasUnderlyingError = false;
    if (!decoder.decode(hasUnderlyingError))
        return false;

    if (hasUnderlyingError) {
        RetainPtr<NSError> underlyingNSError;
        if (!decodeNSError(decoder, underlyingNSError))
            return false;

        userInfo = adoptCF(CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(userInfo.get()) + 1, userInfo.get()));
        CFDictionarySetValue((CFMutableDictionaryRef)userInfo.get(), NSUnderlyingErrorKey, underlyingNSError.get());
    }

    nsError = adoptNS([[NSError alloc] initWithDomain:domain code:code userInfo:(NSDictionary *)userInfo.get()]);
    return true;
}

bool ArgumentCoder<ResourceError>::decodePlatformData(ArgumentDecoder& decoder, ResourceError& resourceError)
{
    bool errorIsNull;
    if (!decoder.decode(errorIsNull))
        return false;
    
    if (errorIsNull) {
        resourceError = ResourceError();
        return true;
    }
    
    RetainPtr<NSError> nsError;
    if (!decodeNSError(decoder, nsError))
        return false;

    resourceError = ResourceError(nsError.get());
    return true;
}

void ArgumentCoder<ProtectionSpace>::encodePlatformData(ArgumentEncoder& encoder, const ProtectionSpace& space)
{
    RetainPtr<NSMutableData> data = adoptNS([[NSMutableData alloc] init]);
    RetainPtr<NSKeyedArchiver> archiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);
    [archiver setRequiresSecureCoding:YES];
    [archiver encodeObject:space.nsSpace() forKey:@"protectionSpace"];
    [archiver finishEncoding];
    IPC::encode(encoder, reinterpret_cast<CFDataRef>(data.get()));
}

bool ArgumentCoder<ProtectionSpace>::decodePlatformData(ArgumentDecoder& decoder, ProtectionSpace& space)
{
    RetainPtr<CFDataRef> data;
    if (!IPC::decode(decoder, data))
        return false;

    RetainPtr<NSKeyedUnarchiver> unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingWithData:(NSData *)data.get()]);
    [unarchiver setRequiresSecureCoding:YES];
    @try {
        if (RetainPtr<NSURLProtectionSpace> nsSpace = [unarchiver decodeObjectOfClass:[NSURLProtectionSpace class] forKey:@"protectionSpace"])
            space = ProtectionSpace(nsSpace.get());
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to decode NSURLProtectionSpace: %@", exception);
    }

    [unarchiver finishDecoding];
    return true;
}

void ArgumentCoder<Credential>::encodePlatformData(ArgumentEncoder& encoder, const Credential& credential)
{
    NSURLCredential *nsCredential = credential.nsCredential();
    // NSURLCredential doesn't serialize identities correctly, so we encode the pieces individually
    // in the identity case. See <rdar://problem/18802434>.
    if (SecIdentityRef identity = nsCredential.identity) {
        encoder << true;
        IPC::encode(encoder, identity);

        if (NSArray *certificates = nsCredential.certificates) {
            encoder << true;
            IPC::encode(encoder, reinterpret_cast<CFArrayRef>(certificates));
        } else
            encoder << false;

        encoder << static_cast<uint64_t>(nsCredential.persistence);
        return;
    }

    encoder << false;
    RetainPtr<NSMutableData> data = adoptNS([[NSMutableData alloc] init]);
    RetainPtr<NSKeyedArchiver> archiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);
    [archiver setRequiresSecureCoding:YES];
    [archiver encodeObject:nsCredential forKey:@"credential"];
    [archiver finishEncoding];
    IPC::encode(encoder, reinterpret_cast<CFDataRef>(data.get()));
}

bool ArgumentCoder<Credential>::decodePlatformData(ArgumentDecoder& decoder, Credential& credential)
{
    bool hasIdentity;
    if (!decoder.decode(hasIdentity))
        return false;

    if (hasIdentity) {
        RetainPtr<SecIdentityRef> identity;
        if (!IPC::decode(decoder, identity))
            return false;

        RetainPtr<CFArrayRef> certificates;
        bool hasCertificates;
        if (!decoder.decode(hasCertificates))
            return false;

        if (hasCertificates) {
            if (!IPC::decode(decoder, certificates))
                return false;
        }

        uint64_t persistence;
        if (!decoder.decode(persistence))
            return false;

        credential = Credential(adoptNS([[NSURLCredential alloc] initWithIdentity:identity.get() certificates:(NSArray *)certificates.get() persistence:(NSURLCredentialPersistence)persistence]).get());
        return true;
    }

    RetainPtr<CFDataRef> data;
    if (!IPC::decode(decoder, data))
        return false;

    RetainPtr<NSKeyedUnarchiver> unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingWithData:(NSData *)data.get()]);
    [unarchiver setRequiresSecureCoding:YES];
    @try {
        if (RetainPtr<NSURLCredential> nsCredential = [unarchiver decodeObjectOfClass:[NSURLCredential class] forKey:@"credential"])
            credential = Credential(nsCredential.get());
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to decode NSURLCredential: %@", exception);
    }

    [unarchiver finishDecoding];
    return true;
}

void ArgumentCoder<MachSendRight>::encode(ArgumentEncoder& encoder, const MachSendRight& sendRight)
{
    encoder << Attachment(sendRight.copySendRight().leakSendRight(), MACH_MSG_TYPE_MOVE_SEND);
}

void ArgumentCoder<MachSendRight>::encode(ArgumentEncoder& encoder, MachSendRight&& sendRight)
{
    encoder << Attachment(sendRight.leakSendRight(), MACH_MSG_TYPE_MOVE_SEND);
}

bool ArgumentCoder<MachSendRight>::decode(ArgumentDecoder& decoder, MachSendRight& sendRight)
{
    Attachment attachment;
    if (!decoder.decode(attachment))
        return false;

    if (attachment.disposition() != MACH_MSG_TYPE_MOVE_SEND)
        return false;

    sendRight = MachSendRight::adopt(attachment.port());
    return true;
}

void ArgumentCoder<KeypressCommand>::encode(ArgumentEncoder& encoder, const KeypressCommand& keypressCommand)
{
    encoder << keypressCommand.commandName << keypressCommand.text;
}
    
bool ArgumentCoder<KeypressCommand>::decode(ArgumentDecoder& decoder, KeypressCommand& keypressCommand)
{
    if (!decoder.decode(keypressCommand.commandName))
        return false;

    if (!decoder.decode(keypressCommand.text))
        return false;

    return true;
}

void ArgumentCoder<ContentFilterUnblockHandler>::encode(ArgumentEncoder& encoder, const ContentFilterUnblockHandler& contentFilterUnblockHandler)
{
    RetainPtr<NSMutableData> data = adoptNS([[NSMutableData alloc] init]);
    RetainPtr<NSKeyedArchiver> archiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);
    [archiver setRequiresSecureCoding:YES];
    contentFilterUnblockHandler.encode(archiver.get());
    [archiver finishEncoding];
    IPC::encode(encoder, reinterpret_cast<CFDataRef>(data.get()));
}

bool ArgumentCoder<ContentFilterUnblockHandler>::decode(ArgumentDecoder& decoder, ContentFilterUnblockHandler& contentFilterUnblockHandler)
{
    RetainPtr<CFDataRef> data;
    if (!IPC::decode(decoder, data))
        return false;

    RetainPtr<NSKeyedUnarchiver> unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingWithData:(NSData *)data.get()]);
    [unarchiver setRequiresSecureCoding:YES];
    if (!ContentFilterUnblockHandler::decode(unarchiver.get(), contentFilterUnblockHandler))
        return false;

    [unarchiver finishDecoding];
    return true;
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

static NSString *deviceContextKey()
{
    static NSString * const key = @"deviceContext";
    return key;
}

void ArgumentCoder<MediaPlaybackTargetContext>::encodePlatformData(ArgumentEncoder& encoder, const MediaPlaybackTargetContext& target)
{
    ASSERT(target.type == MediaPlaybackTargetContext::AVOutputContextType);

    RetainPtr<NSMutableData> data = adoptNS([[NSMutableData alloc] init]);
    RetainPtr<NSKeyedArchiver> archiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);
    [archiver setRequiresSecureCoding:YES];

    if ([getAVOutputContextClass() conformsToProtocol:@protocol(NSSecureCoding)])
        [archiver encodeObject:target.context.avOutputContext forKey:deviceContextKey()];

    [archiver finishEncoding];
    IPC::encode(encoder, reinterpret_cast<CFDataRef>(data.get()));

}

bool ArgumentCoder<MediaPlaybackTargetContext>::decodePlatformData(ArgumentDecoder& decoder, MediaPlaybackTargetContext& target)
{
    ASSERT(target.type == MediaPlaybackTargetContext::AVOutputContextType);

    if (![getAVOutputContextClass() conformsToProtocol:@protocol(NSSecureCoding)])
        return false;

    RetainPtr<CFDataRef> data;
    if (!IPC::decode(decoder, data))
        return false;

    RetainPtr<NSKeyedUnarchiver> unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingWithData:(NSData *)data.get()]);
    [unarchiver setRequiresSecureCoding:YES];

    AVOutputContext *context = nil;
    @try {
        context = [unarchiver decodeObjectOfClass:getAVOutputContextClass() forKey:deviceContextKey()];
    } @catch (NSException *exception) {
        LOG_ERROR("The target picker being decoded is not a AVOutputContext.");
        return false;
    }

    target.context.avOutputContext = context;
    
    [unarchiver finishDecoding];
    return true;
}
#endif

} // namespace IPC
