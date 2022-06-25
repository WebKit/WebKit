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

#import "config.h"
#import "WKCertificateInfoMac.h"

#import "WKAPICast.h"
#import "WebCertificateInfo.h"

WKCertificateInfoRef WKCertificateInfoCreateWithServerTrust(SecTrustRef serverTrust)
{
#if HAVE(SEC_TRUST_SERIALIZATION)
    RefPtr<WebKit::WebCertificateInfo> certificateInfo = WebKit::WebCertificateInfo::create(WebCore::CertificateInfo(serverTrust));
    return WebKit::toAPI(certificateInfo.leakRef());
#else
    return nullptr;
#endif
}

WKCertificateInfoRef WKCertificateInfoCreateWithCertficateChain(CFArrayRef certificateChain)
{
    RefPtr<WebKit::WebCertificateInfo> certificateInfo = WebKit::WebCertificateInfo::create(WebCore::CertificateInfo(certificateChain));
    return WebKit::toAPI(certificateInfo.leakRef());
}

CFArrayRef WKCertificateInfoGetCertificateChain(WKCertificateInfoRef certificateInfoRef)
{
    return WebKit::toImpl(certificateInfoRef)->certificateInfo().certificateChain();
}

SecTrustRef WKCertificateInfoGetServerTrust(WKCertificateInfoRef certificateInfoRef)
{
#if HAVE(SEC_TRUST_SERIALIZATION)
    return WebKit::toImpl(certificateInfoRef)->certificateInfo().trust();
#else
    return nullptr;
#endif
}
