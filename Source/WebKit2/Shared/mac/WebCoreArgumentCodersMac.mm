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
#import <WebCore/ContentFilter.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/ProtectionSpace.h>
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceRequest.h>

#if USE(CFNETWORK)
#import <CFNetwork/CFURLRequest.h>
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

    return true;
}

void ArgumentCoder<ResourceResponse>::encodePlatformData(ArgumentEncoder& encoder, const ResourceResponse& resourceResponse)
{
    bool responseIsPresent = resourceResponse.platformResponseIsUpToDate() && resourceResponse.nsURLResponse();
    encoder << responseIsPresent;

    if (!responseIsPresent)
        return;

    RetainPtr<CFDictionaryRef> dictionary = adoptCF(WKNSURLResponseCreateSerializableRepresentation(resourceResponse.nsURLResponse(), IPC::tokenNullTypeRef()));
    IPC::encode(encoder, dictionary.get());
}

bool ArgumentCoder<ResourceResponse>::decodePlatformData(ArgumentDecoder& decoder, ResourceResponse& resourceResponse)
{
    bool responseIsPresent;
    if (!decoder.decode(responseIsPresent))
        return false;

    if (!responseIsPresent) {
        resourceResponse = ResourceResponse();
        return true;
    }

    RetainPtr<CFDictionaryRef> dictionary;
    if (!IPC::decode(decoder, dictionary))
        return false;

    RetainPtr<NSURLResponse> nsURLResponse = WKNSURLResponseFromSerializableRepresentation(dictionary.get(), IPC::tokenNullTypeRef());

    if (!nsURLResponse)
        return false;

    resourceResponse = ResourceResponse(nsURLResponse.get());
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

void ArgumentCoder<ResourceError>::encodePlatformData(ArgumentEncoder& encoder, const ResourceError& resourceError)
{
    bool errorIsNull = resourceError.isNull();
    encoder << errorIsNull;

    if (errorIsNull)
        return;

    NSError *nsError = resourceError.nsError();

    String domain = [nsError domain];
    encoder << domain;

    int64_t code = [nsError code];
    encoder << code;

    NSDictionary *userInfo = [nsError userInfo];

    RetainPtr<CFMutableDictionaryRef> filteredUserInfo = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, userInfo.count, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    [userInfo enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL*) {
        if ([value isKindOfClass:[NSString class]] || [value isKindOfClass:[NSURL class]])
            CFDictionarySetValue(filteredUserInfo.get(), key, value);
    }];

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

    RetainPtr<NSError> nsError = adoptNS([[NSError alloc] initWithDomain:domain code:code userInfo:(NSDictionary *)userInfo.get()]);

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

void ArgumentCoder<ContentFilter>::encode(ArgumentEncoder& encoder, const ContentFilter& contentFilter)
{
    RetainPtr<NSMutableData> data = adoptNS([[NSMutableData alloc] init]);
    RetainPtr<NSKeyedArchiver> archiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);
    [archiver setRequiresSecureCoding:YES];
    contentFilter.encode(archiver.get());
    [archiver finishEncoding];
    IPC::encode(encoder, reinterpret_cast<CFDataRef>(data.get()));
}

bool ArgumentCoder<ContentFilter>::decode(ArgumentDecoder& decoder, ContentFilter& contentFilter)
{
    RetainPtr<CFDataRef> data;
    if (!IPC::decode(decoder, data))
        return false;

    RetainPtr<NSKeyedUnarchiver> unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingWithData:(NSData *)data.get()]);
    [unarchiver setRequiresSecureCoding:YES];
    if (!ContentFilter::decode(unarchiver.get(), contentFilter))
        return false;

    [unarchiver finishDecoding];
    return true;
}

} // namespace IPC
