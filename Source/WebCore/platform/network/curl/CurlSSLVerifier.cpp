/*
 * Copyright (C) 2013 University of Szeged
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
#include <openssl/ssl.h>

namespace WebCore {

static Vector<CertificateInfo::Certificate> pemDataFromCtx(X509StoreCTX*);
static CurlSSLVerifier::SSLCertificateFlags convertToSSLCertificateFlags(unsigned);

CurlSSLVerifier::CurlSSLVerifier(CurlHandle& curlHandle, void* sslCtx)
    : m_curlHandle(curlHandle)
{
    auto* ctx = static_cast<SSL_CTX*>(sslCtx);
    const auto& sslHandle = CurlContext::singleton().sslHandle();

    SSL_CTX_set_app_data(ctx, this);
    SSL_CTX_set_verify(ctx, SSL_CTX_get_verify_mode(ctx), verifyCallback);

#if defined(LIBRESSL_VERSION_NUMBER)
    if (auto data = WTF::get_if<CertificateInfo::Certificate>(sslHandle.getCACertInfo()))
        SSL_CTX_load_verify_mem(ctx, static_cast<void*>(const_cast<uint8_t*>(data->data())), data->size());
#endif

#if (!defined(LIBRESSL_VERSION_NUMBER))
    const auto& signatureAlgorithmsList = sslHandle.getSignatureAlgorithmsList();
    if (!signatureAlgorithmsList.isEmpty())
        SSL_CTX_set1_sigalgs_list(ctx, signatureAlgorithmsList->utf8().data());
#endif

    const auto& curvesList = sslHandle.getCurvesList();
    if (!curvesList.isEmpty())
        SSL_CTX_set1_curves_list(ctx, curvesList.utf8().data());
}

void CurlSSLVerifier::collectInfo(X509StoreCTX* ctx)
{
    m_certificateInfo = CertificateInfo { X509_STORE_CTX_get_error(ctx), pemDataFromCtx(ctx) };

    if (auto error = m_certificateInfo.verificationError())
        m_sslErrors = static_cast<int>(convertToSSLCertificateFlags(error));
}

int CurlSSLVerifier::verifyCallback(int preverified, X509StoreCTX* ctx)
{
    auto ssl = static_cast<SSL*>(X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
    auto sslCtx = SSL_get_SSL_CTX(ssl);
    auto verifier = static_cast<CurlSSLVerifier*>(SSL_CTX_get_app_data(sslCtx));

    verifier->collectInfo(ctx);
    // whether the verification of the certificate in question was passed (preverified=1) or not (preverified=0)
    return preverified;
}

class StackOfX509 {
public:
    explicit StackOfX509(X509StoreCTX* ctx)
        : m_certs { X509_STORE_CTX_get1_chain(ctx) }
    {
    }

    ~StackOfX509()
    {
        if (m_certs)
            sk_X509_pop_free(m_certs, X509_free);
    }

    unsigned count() { return sk_X509_num(m_certs); }
    X509* item(unsigned i) { return sk_X509_value(m_certs, i); }

private:
    STACK_OF(X509)* m_certs { nullptr };
};

class BIOHolder {
public:
    BIOHolder()
        : m_bio { BIO_new(BIO_s_mem()) }
    {
    }

    ~BIOHolder()
    {
        if (m_bio)
            BIO_free(m_bio);
    }

    bool write(X509* data) { return PEM_write_bio_X509(m_bio, data); }
    CertificateInfo::Certificate asCertificate()
    {
        uint8_t* data;
        long length = BIO_get_mem_data(m_bio, &data);
        if (length < 0)
            return CertificateInfo::Certificate();

        auto cert = CertificateInfo::makeCertificate(data, length);
        return cert;
    }

private:
    BIO* m_bio { nullptr };
};

static Vector<CertificateInfo::Certificate> pemDataFromCtx(X509StoreCTX* ctx)
{
    Vector<CertificateInfo::Certificate> result;
    StackOfX509 certs { ctx };
    for (int i = 0; i < certs.count(); i++) {
        BIOHolder bio;

        if (!bio.write(certs.item(i)))
            return Vector<CertificateInfo::Certificate> { };

        auto certificate = bio.asCertificate();
        if (certificate.isEmpty())
            return Vector<CertificateInfo::Certificate> { };

        result.append(WTFMove(certificate));
    }

    return result;
}

static CurlSSLVerifier::SSLCertificateFlags convertToSSLCertificateFlags(unsigned sslError)
{
    switch (sslError) {
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT : // the issuer certificate could not be found: this occurs if the issuer certificate of an untrusted certificate cannot be found.
    case X509_V_ERR_UNABLE_TO_GET_CRL : // the CRL of a certificate could not be found.
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY : // the issuer certificate of a locally looked up certificate could not be found. This normally means the list of trusted certificates is not complete.
        return CurlSSLVerifier::SSLCertificateFlags::SSL_CERTIFICATE_UNKNOWN_CA;
    case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE : // the certificate signature could not be decrypted. This means that the actual signature value could not be determined rather than it not matching the expected value, this is only meaningful for RSA keys.
    case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE : // the CRL signature could not be decrypted: this means that the actual signature value could not be determined rather than it not matching the expected value. Unused.
    case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY : // the public key in the certificate SubjectPublicKeyInfo could not be read.
    case X509_V_ERR_CERT_SIGNATURE_FAILURE : // the signature of the certificate is invalid.
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT : // the passed certificate is self signed and the same certificate cannot be found in the list of trusted certificates.
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN : // the certificate chain could be built up using the untrusted certificates but the root could not be found locally.
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE : // no signatures could be verified because the chain contains only one certificate and it is not self signed.
    case X509_V_ERR_INVALID_PURPOSE : // the supplied certificate cannot be used for the specified purpose.
    case X509_V_ERR_CERT_UNTRUSTED : // the root CA is not marked as trusted for the specified purpose.
    case X509_V_ERR_CERT_REJECTED : // the root CA is marked to reject the specified purpose.
    case X509_V_ERR_NO_EXPLICIT_POLICY : // the verification flags were set to require and explicit policy but none was present.
    case X509_V_ERR_DIFFERENT_CRL_SCOPE : // the only CRLs that could be found did not match the scope of the certificate.
        return CurlSSLVerifier::SSLCertificateFlags::SSL_CERTIFICATE_INSECURE;
    case X509_V_ERR_CERT_NOT_YET_VALID : // the certificate is not yet valid: the notBefore date is after the current time.
    case X509_V_ERR_CRL_NOT_YET_VALID : // the CRL is not yet valid.
        return CurlSSLVerifier::SSLCertificateFlags::SSL_CERTIFICATE_NOT_ACTIVATED;
    case X509_V_ERR_CERT_HAS_EXPIRED : // the certificate has expired: that is the notAfter date is before the current time.
    case X509_V_ERR_CRL_HAS_EXPIRED : // the CRL has expired.
        return CurlSSLVerifier::SSLCertificateFlags::SSL_CERTIFICATE_EXPIRED;
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD : // the certificate notBefore field contains an invalid time.
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD : // the certificate notAfter field contains an invalid time.
    case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD : // the CRL lastUpdate field contains an invalid time.
    case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD : // the CRL nextUpdate field contains an invalid time.
    case X509_V_ERR_OUT_OF_MEM : // an error occurred trying to allocate memory. This should never happen.
    case X509_V_ERR_CERT_CHAIN_TOO_LONG : // the certificate chain length is greater than the supplied maximum depth. Unused.
    case X509_V_ERR_PATH_LENGTH_EXCEEDED : // the basicConstraints pathlength parameter has been exceeded.
    case X509_V_ERR_INVALID_EXTENSION : // a certificate extension had an invalid value (for example an incorrect encoding) or some value inconsistent with other extensions.
    case X509_V_ERR_INVALID_POLICY_EXTENSION : // a certificate policies extension had an invalid value (for example an incorrect encoding) or some value inconsistent with other extensions. This error only occurs if policy processing is enabled.
    case X509_V_ERR_UNSUPPORTED_EXTENSION_FEATURE : // some feature of a certificate extension is not supported. Unused.
    case X509_V_ERR_PERMITTED_VIOLATION : // a name constraint violation occured in the permitted subtrees.
    case X509_V_ERR_EXCLUDED_VIOLATION : // a name constraint violation occured in the excluded subtrees.
    case X509_V_ERR_SUBTREE_MINMAX : // a certificate name constraints extension included a minimum or maximum field: this is not supported.
    case X509_V_ERR_UNSUPPORTED_CONSTRAINT_TYPE : // an unsupported name constraint type was encountered. OpenSSL currently only supports directory name, DNS name, email and URI types.
    case X509_V_ERR_UNSUPPORTED_CONSTRAINT_SYNTAX : // the format of the name constraint is not recognised: for example an email address format of a form not mentioned in RFC3280. This could be caused by a garbage extension or some new feature not currently supported.
    case X509_V_ERR_CRL_PATH_VALIDATION_ERROR : // an error occured when attempting to verify the CRL path. This error can only happen if extended CRL checking is enabled.
    case X509_V_ERR_APPLICATION_VERIFICATION : // an application specific error. This will never be returned unless explicitly set by an application.
        return CurlSSLVerifier::SSLCertificateFlags::SSL_CERTIFICATE_GENERIC_ERROR;
    case X509_V_ERR_CERT_REVOKED : // the certificate has been revoked.
        return CurlSSLVerifier::SSLCertificateFlags::SSL_CERTIFICATE_REVOKED;
    case X509_V_ERR_INVALID_CA : // a CA certificate is invalid. Either it is not a CA or its extensions are not consistent with the supplied purpose.
    case X509_V_ERR_SUBJECT_ISSUER_MISMATCH : // the current candidate issuer certificate was rejected because its subject name did not match the issuer name of the current certificate. This is only set if issuer check debugging is enabled it is used for status notification and is not in itself an error.
    case X509_V_ERR_AKID_SKID_MISMATCH : // the current candidate issuer certificate was rejected because its subject key identifier was present and did not match the authority key identifier current certificate. This is only set if issuer check debugging is enabled it is used for status notification and is not in itself an error.
    case X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH : // the current candidate issuer certificate was rejected because its issuer name and serial number was present and did not match the authority key identifier of the current certificate. This is only set if issuer check debugging is enabled it is used for status notification and is not in itself an error.
    case X509_V_ERR_KEYUSAGE_NO_CERTSIGN : // the current candidate issuer certificate was rejected because its keyUsage extension does not permit certificate signing. This is only set if issuer check debugging is enabled it is used for status notification and is not in itself an error.
        return CurlSSLVerifier::SSLCertificateFlags::SSL_CERTIFICATE_BAD_IDENTITY;
    default :
        return CurlSSLVerifier::SSLCertificateFlags::SSL_CERTIFICATE_GENERIC_ERROR;
    }
}

}

#endif
