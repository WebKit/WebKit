/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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
#include "OpenSSLHelper.h"

#include <openssl/x509v3.h>
#include <wtf/DateMath.h>
#include <wtf/HexNumber.h>
#include <wtf/Seconds.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace OpenSSL {

template<typename> struct deleter;
template<> struct deleter<X509> {
    void operator()(X509* x509)
    {
        if (x509)
            X509_free(x509);
    }
};

class StackOfGeneralName {
public:
    StackOfGeneralName(const X509* x509, int nid)
        : m_names { static_cast<STACK_OF(GENERAL_NAME)*>(X509_get_ext_d2i(x509, nid, nullptr, nullptr)) }
    {
    }

    ~StackOfGeneralName()
    {
        if (m_names)
            sk_GENERAL_NAME_pop_free(m_names, GENERAL_NAME_free);
    }

    operator bool() { return m_names; }

    int count() { return sk_GENERAL_NAME_num(m_names); }
    GENERAL_NAME* item(int i) { return sk_GENERAL_NAME_value(m_names, i); }

private:
    STACK_OF(GENERAL_NAME)* m_names { nullptr };
};

class StackOfX509 {
public:
    StackOfX509(STACK_OF(X509)* certs)
        : m_certs { certs }
        , m_owner { false }
    {
    }

    StackOfX509(X509_STORE_CTX* ctx)
        : m_certs { X509_STORE_CTX_get1_chain(ctx) }
        , m_owner { true }
    {
    }

    ~StackOfX509()
    {
        if (m_certs && m_owner)
            sk_X509_pop_free(m_certs, X509_free);
    }

    int count() { return sk_X509_num(m_certs); }
    X509* item(int i) { return sk_X509_value(m_certs, i); }

private:
    STACK_OF(X509)* m_certs;
    bool m_owner;
};

class BIO {
public:
    BIO()
        : m_bio { ::BIO_new(::BIO_s_mem()) }
    {
    }

    BIO(X509* data)
        : m_bio { ::BIO_new(::BIO_s_mem()) }
    {
        ::PEM_write_bio_X509(m_bio, data);
    }

    BIO(const Vector<uint8_t>& data)
        : m_bio { ::BIO_new_mem_buf(data.span().data(), data.size()) }
    {
    }

    ~BIO()
    {
        if (m_bio)
            ::BIO_free(m_bio);
    }

    std::optional<Vector<uint8_t>> getDataAsVector() const
    {
        uint8_t* data { nullptr };
        auto length = ::BIO_get_mem_data(m_bio, &data);
        if (length < 0)
            return std::nullopt;

        return Vector(std::span<const uint8_t> { data, static_cast<size_t>(length) });
    }

    RefPtr<StringImpl> getDataAsString() const
    {
        uint8_t* data { nullptr };
        auto length = ::BIO_get_mem_data(m_bio, &data);
        if (length < 0)
            return nullptr;

        return StringImpl::createOrNull(std::span { byteCast<Latin1Character>(data), static_cast<size_t>(length) });
    }

    std::unique_ptr<X509, deleter<X509>> readX509()
    {
        return std::unique_ptr<X509, deleter<X509>>(::PEM_read_bio_X509(m_bio, nullptr, 0, nullptr));
    }

    ::BIO* get() { return m_bio; }

private:
    ::BIO* m_bio { nullptr };
};


static WebCore::CertificateInfo::CertificateChain pemDataFromCtx(StackOfX509&& certs)
{
    WebCore::CertificateInfo::CertificateChain result;

    for (int i = 0; i < certs.count(); i++) {
        BIO bio(certs.item(i));

        if (auto certificate = bio.getDataAsVector())
            result.append(WTF::move(*certificate));
        else
            return { };
    }

    return result;
}

std::unique_ptr<WebCore::CertificateInfo> createCertificateInfo(std::optional<long>&& verifyResult, SSL* ssl)
{
    if (!verifyResult || !ssl)
        return nullptr;

    auto certChain = SSL_get_peer_cert_chain(ssl);

    return makeUnique<WebCore::CertificateInfo>(*verifyResult, pemDataFromCtx(StackOfX509(certChain)));
}

WebCore::CertificateInfo::CertificateChain createCertificateChain(X509_STORE_CTX* ctx)
{
    if (!ctx)
        return { };

    return pemDataFromCtx(StackOfX509(ctx));
}

#if OS(WINDOWS)
static RefPtr<StringImpl> toString(const ASN1_STRING* name)
{
    unsigned char* data { nullptr };
    auto length = ASN1_STRING_to_UTF8(&data, name);
    if (length <= 0)
        return nullptr;

    RefPtr<StringImpl> result = StringImpl::createOrNull(std::span { byteCast<Latin1Character>(data), static_cast<size_t>(length) });
    OPENSSL_free(data);
    return result;
}
#endif

static RefPtr<StringImpl> getSubjectEntry(const X509* x509, int nid)
{
    auto subjectName = X509_get_subject_name(x509);
    if (!subjectName)
        return nullptr;

    auto index = X509_NAME_get_index_by_NID(subjectName, nid, -1);
    if (index < 0)
        return nullptr;

    auto entry = X509_NAME_get_entry(subjectName, index);
    if (!entry)
        return nullptr;

    auto entryData = X509_NAME_ENTRY_get_data(entry);
    if (!entryData)
        return nullptr;

    return toString(entryData);
}

static RefPtr<StringImpl> getCommonName(const X509* x509)
{
    return getSubjectEntry(x509, NID_commonName);
}

static RefPtr<StringImpl> getOrganizationName(const X509* x509)
{
    return getSubjectEntry(x509, NID_organizationName);
}

static RefPtr<StringImpl> getSubjectName(const X509* x509)
{
    static const unsigned long flags = (ASN1_STRFLGS_RFC2253 | ASN1_STRFLGS_ESC_QUOTE | XN_FLAG_SEP_CPLUS_SPC | XN_FLAG_DN_REV | XN_FLAG_FN_NONE | XN_FLAG_SPC_EQ) & ~ASN1_STRFLGS_ESC_MSB;

    auto subjectName = X509_get_subject_name(x509);
    if (!subjectName)
        return nullptr;

    BIO bio;
    auto length = X509_NAME_print_ex(bio.get(), subjectName, 0, flags);
    if (length <= 0)
        return nullptr;

    return bio.getDataAsString();
}

static std::optional<Seconds> convertASN1TimeToSeconds(const ASN1_TIME* ans1Time)
{
    if (!ans1Time)
        return std::nullopt;

    if ((ans1Time->type != V_ASN1_UTCTIME && ans1Time->type != V_ASN1_GENERALIZEDTIME) || !ans1Time->data)
        return std::nullopt;

    // UTCTIME         : YYmmddHHMM[SS]
    // GENERALIZEDTIME : YYYYmmddHHMM[SS]
    int digitLength = ans1Time->type == V_ASN1_UTCTIME ? 10 : 12;
    if (ans1Time->length < digitLength)
        return std::nullopt;

    auto data = ans1Time->data;
    for (int i = 0; i < digitLength; i++) {
        if (!isASCIIDigit(data[i]))
            return std::nullopt;
    }

    struct tm time { };

    if (ans1Time->type == V_ASN1_UTCTIME) {
        time.tm_year = (data[0] - '0') * 10 + (data[1] - '0');
        if (time.tm_year < 50)
            time.tm_year += 100;
    } else {
        time.tm_year = (data[0] - '0') * 1000 + (data[1] - '0') * 100 + (data[2] - '0') * 10 + (data[3] - '0');
        time.tm_year -= 1900;
    }

    data += ans1Time->type == V_ASN1_UTCTIME ? 2 : 4;

    time.tm_mon = (data[0] - '0') * 10 + (data[1] - '0') - 1;
    if (time.tm_mon < 0 || time.tm_mon > 11)
        return std::nullopt;
    time.tm_mday = (data[2] - '0') * 10 + (data[3] - '0');
    time.tm_hour = (data[4] - '0') * 10 + (data[5] - '0');
    time.tm_min = (data[6] - '0') * 10 + (data[7] - '0');

    if ((ans1Time->length >= digitLength + 2) && isASCIIDigit(data[8]) && isASCIIDigit(data[9]))
        time.tm_sec = (data[8] - '0') * 10 + (data[9] - '0');

    auto gmtTime = mktime(&time);
    auto localTimeOffset = calculateLocalTimeOffset(gmtTime * 1000.0);
    return Seconds(gmtTime + (localTimeOffset.offset / 1000.0));
}

static void getSubjectAltName(const X509* x509, Vector<RefPtr<StringImpl>>& dnsNames, Vector<RefPtr<StringImpl>>& ipAddresses)
{
    StackOfGeneralName sanList(x509, NID_subject_alt_name);
    if (!sanList)
        return;

    auto num = sanList.count();
    for (auto i = 0; i < num; i++) {
        auto* value = sanList.item(i);
        if (!value)
            continue;

        if (value->type == GEN_DNS) {
            auto dnsName = toString(value->d.dNSName);
            if (dnsName)
                dnsNames.append(WTF::move(dnsName));
        } else if (value->type == GEN_IPADD) {
            auto data = value->d.iPAddress->data;
            if (value->d.iPAddress->length == 4)
                ipAddresses.append(makeString(data[0], '.', data[1], '.', data[2], '.', data[3]));
            else if (value->d.iPAddress->length == 16) {
                std::span<uint8_t, 16> dataSpan { data, 16 };
                ipAddresses.append(canonicalizeIPv6Address(dataSpan));
            }
        }
    }
}

RefPtr<StringImpl> canonicalizeIPv6Address(std::span<uint8_t, 16> data)
{
    bool compressCurrentSection = false;
    size_t maxZeros = 0;
    std::optional<size_t> startRunner;
    std::optional<size_t> endRunner;
    std::optional<size_t> start;
    std::optional<size_t> end;

    for (int i = 0; i < 8; i++) {
        compressCurrentSection = !data[2 * i] && !data[2 * i + 1];
        if (compressCurrentSection) {
            startRunner = !startRunner.has_value() ? i : startRunner;
            endRunner = i;
            size_t len = endRunner.value() - startRunner.value() + 1;
            if (len > maxZeros) {
                start = startRunner;
                end = endRunner;
                maxZeros = len;
            }
        } else
            startRunner.reset();
    }

    size_t minimum = 1;
    StringBuilder ipAddress;
    RefPtr<StringImpl> oldSection = ""_si;
    StringBuilder newSection;
    for (int j = 0; j < 8; j++) {
        if (j == start && maxZeros > minimum) {
            if (ipAddress.isEmpty())
                ipAddress.append(':');
            ipAddress.append(':');

            j = end.value();
            continue;
        }
        oldSection = makeString(hex(data[2 * j] << 8 | data[2 * j + 1], 4, Lowercase));
        newSection = StringBuilder();
        for (int k = 0; k < 4; k++) {
            if (codeUnitAt(oldSection, k) != '0' || !newSection.isEmpty() || k == 3)
                newSection.append(codeUnitAt(oldSection, k));
        }
        ipAddress.append(newSection.toString());

        if (j != 7)
            ipAddress.append(':');
    }
    return ipAddress.toString();
}

std::optional<WebCore::CertificateSummary> createSummaryInfo(const Vector<uint8_t>& pem)
{
    BIO bio { pem };
    auto x509 = bio.readX509();
    if (!x509)
        return std::nullopt;

    WebCore::CertificateSummary summaryInfo;

    // Prefer the CN attribute, then O, to match the behavior of SecCertificateCopySubjectSummary on macOS.
    summaryInfo.subject = getCommonName(x509.get());
    if (!summaryInfo.subject)
        summaryInfo.subject = getOrganizationName(x509.get());
    if (!summaryInfo.subject)
        summaryInfo.subject = getSubjectName(x509.get());

    if (auto notBefore = convertASN1TimeToSeconds(X509_get0_notBefore(x509.get())))
        summaryInfo.validFrom = *notBefore;

    if (auto notAfter = convertASN1TimeToSeconds(X509_get0_notAfter(x509.get())))
        summaryInfo.validUntil = *notAfter;

    getSubjectAltName(x509.get(), summaryInfo.dnsNames, summaryInfo.ipAddresses);

    return summaryInfo;
}

RefPtr<StringImpl> tlsVersion(const SSL* ssl)
{
    return StringImpl::fromLatin1(SSL_get_version(ssl));
}

RefPtr<StringImpl> tlsCipherName(const SSL* ssl)
{
    return StringImpl::fromLatin1(SSL_CIPHER_get_name(SSL_get_current_cipher(ssl)));
}

}
