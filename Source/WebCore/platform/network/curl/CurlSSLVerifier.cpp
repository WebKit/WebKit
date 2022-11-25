/*
 * Copyright (C) 2013 University of Szeged
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CurlSSLVerifier.h"

#if USE(CURL)
#include "CurlContext.h"
#include "CurlSSLHandle.h"

namespace WebCore {

CurlSSLVerifier::CurlSSLVerifier(void* sslCtx)
{
    auto* ctx = static_cast<SSL_CTX*>(sslCtx);
    const auto& sslHandle = CurlContext::singleton().sslHandle();

    SSL_CTX_set_app_data(ctx, this);
    SSL_CTX_set_verify(ctx, SSL_CTX_get_verify_mode(ctx), verifyCallback);

#if (!defined(LIBRESSL_VERSION_NUMBER))
    if (const auto& signatureAlgorithmsList = sslHandle.signatureAlgorithmsList(); !signatureAlgorithmsList.isNull())
        SSL_CTX_set1_sigalgs_list(ctx, signatureAlgorithmsList.data());
#endif
}

std::unique_ptr<WebCore::CertificateInfo> CurlSSLVerifier::createCertificateInfo(std::optional<long>&& verifyResult)
{
    if (!verifyResult)
        return nullptr;

    if (m_certificateChain.isEmpty())
        return nullptr;

    return makeUnique<CertificateInfo>(*verifyResult, WTFMove(m_certificateChain));
}

void CurlSSLVerifier::collectInfo(X509_STORE_CTX* ctx)
{
    if (!ctx)
        return;

    m_certificateChain = OpenSSL::createCertificateChain(ctx);
}

int CurlSSLVerifier::verifyCallback(int preverified, X509_STORE_CTX* ctx)
{
    auto ssl = static_cast<SSL*>(X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
    auto sslCtx = SSL_get_SSL_CTX(ssl);
    auto verifier = static_cast<CurlSSLVerifier*>(SSL_CTX_get_app_data(sslCtx));

    verifier->collectInfo(ctx);
    // whether the verification of the certificate in question was passed (preverified=1) or not (preverified=0)
    return preverified;
}

}
#endif
