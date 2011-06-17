/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebCoreArgumentCoders.h"

#if USE(CFNETWORK)
#include "ArgumentCodersCF.h"
#include "PlatformCertificateInfo.h"
#include <CFNetwork/CFURLRequestPriv.h>
#include <WebCore/CertificateCFWin.h>
#endif
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#if USE(CFNETWORK)
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#endif

using namespace WebCore;
#if USE(CFNETWORK)
using namespace WebKit;
#endif

namespace CoreIPC {

// FIXME: These coders should really go in a WebCoreArgumentCodersCFNetwork file.

void ArgumentCoder<ResourceRequest>::encode(ArgumentEncoder* encoder, const ResourceRequest& resourceRequest)
{
#if USE(CFNETWORK)
    bool requestIsPresent = resourceRequest.cfURLRequest();
    encoder->encode(requestIsPresent);

    if (!requestIsPresent)
        return;

    RetainPtr<CFDictionaryRef> dictionary(AdoptCF, wkCFURLRequestCreateSerializableRepresentation(resourceRequest.cfURLRequest(), CoreIPC::tokenNullTypeRef()));
    CoreIPC::encode(encoder, dictionary.get());
#endif
}

bool ArgumentCoder<ResourceRequest>::decode(ArgumentDecoder* decoder, ResourceRequest& resourceRequest)
{
#if USE(CFNETWORK)
    bool requestIsPresent;
    if (!decoder->decode(requestIsPresent))
        return false;

    if (!requestIsPresent) {
        resourceRequest = ResourceRequest();
        return true;
    }

    RetainPtr<CFDictionaryRef> dictionary;
    if (!CoreIPC::decode(decoder, dictionary))
        return false;

    RetainPtr<CFURLRequestRef> cfURLRequest(AdoptCF, wkCFURLRequestCreateFromSerializableRepresentation(dictionary.get(), CoreIPC::tokenNullTypeRef()));
    if (!cfURLRequest)
        return false;
    RetainPtr<CFMutableURLRequestRef> mutableCFURLRequest(AdoptCF, CFURLRequestCreateMutableCopy(0, cfURLRequest.get()));
#if USE(CFURLSTORAGESESSIONS)
    wkSetRequestStorageSession(ResourceHandle::currentStorageSession(), mutableCFURLRequest.get());
#endif

    resourceRequest = ResourceRequest(mutableCFURLRequest.get());
    return true;
#else
    return false;
#endif
}


void ArgumentCoder<ResourceResponse>::encode(ArgumentEncoder* encoder, const ResourceResponse& resourceResponse)
{
#if USE(CFNETWORK)
    bool responseIsPresent = resourceResponse.cfURLResponse();
    encoder->encode(responseIsPresent);

    if (!responseIsPresent)
        return;

    RetainPtr<CFDictionaryRef> dictionary(AdoptCF, wkCFURLResponseCreateSerializableRepresentation(resourceResponse.cfURLResponse(), CoreIPC::tokenNullTypeRef()));
    CoreIPC::encode(encoder, dictionary.get());
#endif
}

bool ArgumentCoder<ResourceResponse>::decode(ArgumentDecoder* decoder, ResourceResponse& resourceResponse)
{
#if USE(CFNETWORK)
    bool responseIsPresent;
    if (!decoder->decode(responseIsPresent))
        return false;

    if (!responseIsPresent) {
        resourceResponse = ResourceResponse();
        return true;
    }

    RetainPtr<CFDictionaryRef> dictionary;
    if (!CoreIPC::decode(decoder, dictionary))
        return false;

    RetainPtr<CFURLResponseRef> cfURLResponse(AdoptCF, wkCFURLResponseCreateFromSerializableRepresentation(dictionary.get(), CoreIPC::tokenNullTypeRef()));
    if (!cfURLResponse)
        return false;

    resourceResponse = ResourceResponse(cfURLResponse.get());
    return true;
#else
    return false;
#endif
}


void ArgumentCoder<ResourceError>::encode(ArgumentEncoder* encoder, const ResourceError& resourceError)
{
    encoder->encode(resourceError.domain());
    encoder->encode(resourceError.errorCode());
    encoder->encode(resourceError.failingURL()); 
    encoder->encode(resourceError.localizedDescription());

#if USE(CFNETWORK)
    encoder->encode(PlatformCertificateInfo(resourceError.certificate()));
#endif
}

bool ArgumentCoder<ResourceError>::decode(ArgumentDecoder* decoder, ResourceError& resourceError)
{
    String domain;
    if (!decoder->decode(domain))
        return false;

    int errorCode;
    if (!decoder->decode(errorCode))
        return false;

    String failingURL;
    if (!decoder->decode(failingURL))
        return false;

    String localizedDescription;
    if (!decoder->decode(localizedDescription))
        return false;

#if USE(CFNETWORK)
    PlatformCertificateInfo certificate;
    if (!decoder->decode(certificate))
        return false;
    
    const Vector<PCCERT_CONTEXT> certificateChain = certificate.certificateChain();
    if (!certificateChain.isEmpty()) {
        ASSERT(certificateChain.size() == 1);
        resourceError = ResourceError(domain, errorCode, failingURL, localizedDescription, copyCertificateToData(certificateChain.first()).get());
        return true;
    }
#endif

    resourceError = ResourceError(domain, errorCode, failingURL, localizedDescription);
    return true;
}

} // namespace CoreIPC
