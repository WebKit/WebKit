/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
#include "WKProtectionSpaceCurl.h"

#include "APIArray.h"
#include "APIData.h"
#include "WKAPICast.h"
#include "WebCredential.h"
#include "WebProtectionSpace.h"
#include <WebCore/CertificateInfo.h>

WKCertificateInfoRef WKProtectionSpaceCopyCertificateInfo(WKProtectionSpaceRef protectionSpaceRef)
{
    return nullptr;
}

WKArrayRef WKProtectionSpaceCopyCertificateChain(WKProtectionSpaceRef protectionSpace)
{
    auto& certificateChain = WebKit::toImpl(protectionSpace)->protectionSpace().certificateInfo().certificateChain();
    auto certs = WTF::map(certificateChain, [](auto&& certificate) -> RefPtr<API::Object> {
        return API::Data::create(reinterpret_cast<const unsigned char*>(certificate.data()), certificate.size());
    });
    return WebKit::toAPI(API::Array::create(WTFMove(certs)).leakRef());
}

int WKProtectionSpaceGetCertificateVerificationError(WKProtectionSpaceRef protectionSpace)
{
    auto& certificateInfo = WebKit::toImpl(protectionSpace)->protectionSpace().certificateInfo();
    return certificateInfo.verificationError();
}

WKStringRef WKProtectionSpaceCopyCertificateVerificationErrorDescription(WKProtectionSpaceRef protectionSpace)
{
    auto& certificateInfo = WebKit::toImpl(protectionSpace)->protectionSpace().certificateInfo();
    return WebKit::toCopiedAPI(certificateInfo.verificationErrorDescription());
}
