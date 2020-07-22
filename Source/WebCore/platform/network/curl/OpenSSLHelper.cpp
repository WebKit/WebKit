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
    StackOfX509(X509_STORE_CTX* ctx)
        : m_certs { X509_STORE_CTX_get1_chain(ctx) }
    {
    }

    ~StackOfX509()
    {
        if (m_certs)
            sk_X509_pop_free(m_certs, X509_free);
    }

    int count() { return sk_X509_num(m_certs); }
    X509* item(int i) { return sk_X509_value(m_certs, i); }

private:
    STACK_OF(X509)* m_certs { nullptr };
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
        : m_bio { ::BIO_new_mem_buf(data.data(), data.size()) }
    {
    }

    ~BIO()
    {
        if (m_bio)
            ::BIO_free(m_bio);
    }

    Optional<Vector<uint8_t>> getDataAsVector() const
    {
        uint8_t* data { nullptr };
        auto length = ::BIO_get_mem_data(m_bio, &data);
        if (length < 0)
            return WTF::nullopt;

        Vector<uint8_t> result;
        result.append(data, length);
        return result;
    }

    String getDataAsString() const
    {
        uint8_t* data { nullptr };
        auto length = ::BIO_get_mem_data(m_bio, &data);
        if (length < 0)
            return String();

        return String(data, length);
    }

    std::unique_ptr<X509, deleter<X509>> readX509()
    {
        return std::unique_ptr<X509, deleter<X509>>(::PEM_read_bio_X509(m_bio, nullptr, 0, nullptr));
    }

    ::BIO* get() { return m_bio; }

private:
    ::BIO* m_bio { nullptr };
};


static Vector<WebCore::CertificateInfo::Certificate> pemDataFromCtx(X509_STORE_CTX* ctx)
{
    Vector<WebCore::CertificateInfo::Certificate> result;
    StackOfX509 certs { ctx };

    for (int i = 0; i < certs.count(); i++) {
        BIO bio(certs.item(i));

        if (auto certificate = bio.getDataAsVector())
            result.append(WTFMove(*certificate));
        else
            return { };
    }

    return result;
}

Optional<WebCore::CertificateInfo> createCertificateInfo(X509_STORE_CTX* ctx)
{
    if (!ctx)
        return WTF::nullopt;

    return WebCore::CertificateInfo(X509_STORE_CTX_get_error(ctx), pemDataFromCtx(ctx));
}

static String toString(const ASN1_STRING* name)
{
    unsigned char* data { nullptr };
    auto length = ASN1_STRING_to_UTF8(&data, name);
    if (length <= 0)
        return String();

    String result(data, length);
    OPENSSL_free(data);
    return result;
}

static String getCommonName(const X509* x509)
{
    auto subjectName = X509_get_subject_name(x509);
    if (!subjectName)
        return String();

    auto index = X509_NAME_get_index_by_NID(subjectName, NID_commonName, -1);
    if (index < 0)
        return String();

    auto commonNameEntry = X509_NAME_get_entry(subjectName, index);
    if (!commonNameEntry)
        return String();

    auto commonNameEntryData = X509_NAME_ENTRY_get_data(commonNameEntry);
    if (!commonNameEntryData)
        return String();

    return toString(commonNameEntryData);
}

static String getSubjectName(const X509* x509)
{
    static const unsigned long flags = (ASN1_STRFLGS_RFC2253 | ASN1_STRFLGS_ESC_QUOTE | XN_FLAG_SEP_CPLUS_SPC | XN_FLAG_DN_REV | XN_FLAG_FN_NONE | XN_FLAG_SPC_EQ) & ~ASN1_STRFLGS_ESC_MSB;

    auto subjectName = X509_get_subject_name(x509);
    if (!subjectName)
        return String();

    BIO bio;
    auto length = X509_NAME_print_ex(bio.get(), subjectName, 0, flags);
    if (length <= 0)
        return String();

    return bio.getDataAsString();
}

static Optional<Seconds> convertASN1TimeToSeconds(const ASN1_TIME* ans1Time)
{
    if (!ans1Time)
        return WTF::nullopt;

    if ((ans1Time->type != V_ASN1_UTCTIME && ans1Time->type != V_ASN1_GENERALIZEDTIME) || !ans1Time->data)
        return WTF::nullopt;

    // UTCTIME         : YYmmddHHMM[SS]
    // GENERALIZEDTIME : YYYYmmddHHMM[SS]
    int digitLength = ans1Time->type == V_ASN1_UTCTIME ? 10 : 12;
    if (ans1Time->length < digitLength)
        return WTF::nullopt;

    auto data = ans1Time->data;
    for (int i = 0; i < digitLength; i++) {
        if (!isASCIIDigit(data[i]))
            return WTF::nullopt;
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
        return WTF::nullopt;
    time.tm_mday = (data[2] - '0') * 10 + (data[3] - '0');
    time.tm_hour = (data[4] - '0') * 10 + (data[5] - '0');
    time.tm_min = (data[6] - '0') * 10 + (data[7] - '0');

    if ((ans1Time->length >= digitLength + 2) && isASCIIDigit(data[8]) && isASCIIDigit(data[9]))
        time.tm_sec = (data[8] - '0') * 10 + (data[9] - '0');

    auto gmtTime = mktime(&time);
    auto localTimeOffset = calculateLocalTimeOffset(gmtTime * 1000.0);
    return Seconds(gmtTime + (localTimeOffset.offset / 1000.0));
}

static void getSubjectAltName(const X509* x509, Vector<String>& dnsNames, Vector<String>& ipAddresses)
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
            if (!dnsName.isNull())
                dnsNames.append(WTFMove(dnsName));
        } else if (value->type == GEN_IPADD) {
            auto data = value->d.iPAddress->data;
            if (value->d.iPAddress->length == 4)
                ipAddresses.append(makeString(data[0], ".", data[1], ".", data[2], ".", data[3]));
            else if (value->d.iPAddress->length == 16) {
                StringBuilder ipAddress;
                for (int i = 0; i < 8; i++) {
                    ipAddress.append(makeString(hex(data[0] << 8 | data[1], 4)));
                    if (i != 7)
                        ipAddress.append(":");
                    data += 2;
                }
                ipAddresses.append(ipAddress.toString());
            }
        }
    }
}

Optional<WebCore::CertificateSummary> createSummaryInfo(const Vector<uint8_t>& pem)
{
    BIO bio { pem };
    auto x509 = bio.readX509();
    if (!x509)
        return WTF::nullopt;

    WebCore::CertificateSummary summaryInfo;

    summaryInfo.subject = getCommonName(x509.get());
    if (summaryInfo.subject.isNull())
        summaryInfo.subject = getSubjectName(x509.get());

    if (auto notBefore = convertASN1TimeToSeconds(X509_get0_notBefore(x509.get())))
        summaryInfo.validFrom = *notBefore;

    if (auto notAfter = convertASN1TimeToSeconds(X509_get0_notAfter(x509.get())))
        summaryInfo.validUntil = *notAfter;

    getSubjectAltName(x509.get(), summaryInfo.dnsNames, summaryInfo.ipAddresses);

    return summaryInfo;
}

}
