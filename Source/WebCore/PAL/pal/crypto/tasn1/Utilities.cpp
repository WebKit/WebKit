/*
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
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
#include "Utilities.h"

#include <mutex>

namespace PAL {
namespace TASN1 {

static asn1_node asn1Definitions()
{
    // Generated with `asn1Parser WebCrypto.asn`.

    static asn1_node s_definitions;
    static const asn1_static_node s_WebCryptoASN1[] = {
        { "WebCrypto", 536872976, nullptr },
        { nullptr, 1073741836, nullptr },
        { "SubjectPublicKeyInfo", 1610612741, nullptr },
        { "algorithm", 1073741826, "AlgorithmIdentifier"},
        { "subjectPublicKey", 6, nullptr },
        { "AlgorithmIdentifier", 1610612741, nullptr },
        { "algorithm", 1073741836, nullptr },
        { "parameters", 541081613, nullptr },
        { "algorithm", 1, nullptr },
        { "PrivateKeyInfo", 1610612741, nullptr },
        { "version", 1073741826, "Version"},
        { "privateKeyAlgorithm", 1073741826, "PrivateKeyAlgorithmIdentifier"},
        { "privateKey", 1073741826, "PrivateKey"},
        { "attributes", 536895490, "Attributes"},
        { nullptr, 4104, "0"},
        { "Version", 1073741827, nullptr },
        { "PrivateKeyAlgorithmIdentifier", 1073741826, "AlgorithmIdentifier"},
        { "PrivateKey", 1073741831, nullptr },
        { "Attributes", 1610612751, nullptr },
        { nullptr, 2, "Attribute"},
        { "Attribute", 1610612741, nullptr },
        { "type", 1073741836, nullptr },
        { "values", 2, "AttributeSetValue"},
        { "AttributeSetValue", 1610612751, nullptr },
        { nullptr, 13, nullptr },
        { "ECParameters", 1610612754, nullptr },
        { "namedCurve", 12, nullptr },
        { "ECPrivateKey", 1610612741, nullptr },
        { "version", 1073741827, nullptr },
        { "privateKey", 1073741831, nullptr },
        { "parameters", 1610637314, "ECParameters"},
        { nullptr, 2056, "0"},
        { "publicKey", 536895494, nullptr },
        { nullptr, 2056, "1"},
        { "RSAPublicKey", 1610612741, nullptr },
        { "modulus", 1073741827, nullptr },
        { "publicExponent", 3, nullptr },
        { "RSAPrivateKey", 1610612741, nullptr },
        { "version", 1073741826, "Version"},
        { "modulus", 1073741827, nullptr },
        { "publicExponent", 1073741827, nullptr },
        { "privateExponent", 1073741827, nullptr },
        { "prime1", 1073741827, nullptr },
        { "prime2", 1073741827, nullptr },
        { "exponent1", 1073741827, nullptr },
        { "exponent2", 1073741827, nullptr },
        { "coefficient", 1073741827, nullptr },
        { "otherPrimeInfos", 16386, "OtherPrimeInfos"},
        { "OtherPrimeInfos", 1612709899, nullptr },
        { "MAX", 1074266122, "1"},
        { nullptr, 2, "OtherPrimeInfo"},
        { "OtherPrimeInfo", 536870917, nullptr },
        { "prime", 1073741827, nullptr },
        { "exponent", 1073741827, nullptr },
        { "coefficient", 3, nullptr },
        { nullptr, 0, nullptr }
    };

    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag, [] { asn1_array2tree(s_WebCryptoASN1, &s_definitions, nullptr); });

    return s_definitions;
}

bool createStructure(const char* elementName, asn1_node* root)
{
    int ret = asn1_create_element(asn1Definitions(), elementName, root);
    return ret == ASN1_SUCCESS;
}

bool decodeStructure(asn1_node* root, const char* elementName, const Vector<uint8_t>& data)
{
    if (!createStructure(elementName, root))
        return false;

    int dataSize = data.size();
    int ret = asn1_der_decoding2(root, data.data(), &dataSize, ASN1_DECODE_FLAG_STRICT_DER, nullptr);
    return ret == ASN1_SUCCESS;
}

Optional<Vector<uint8_t>> elementData(asn1_node root, const char* elementName)
{
    int length = 0;
    unsigned type = 0;
    int ret = asn1_read_value_type(root, elementName, nullptr, &length, &type);
    if (ret != ASN1_MEM_ERROR)
        return WTF::nullopt;

    if (type == ASN1_ETYPE_BIT_STRING) {
        if (length % 8)
            return WTF::nullopt;
        length /= 8;
    }

    Vector<uint8_t> data(length);
    ret = asn1_read_value(root, elementName, data.data(), &length);
    if (ret != ASN1_SUCCESS)
        return WTF::nullopt;

    return data;
}

Optional<Vector<uint8_t>> encodedData(asn1_node root, const char* elementName)
{
    int length = 0;
    int ret = asn1_der_coding(root, elementName, nullptr, &length, nullptr);
    if (ret != ASN1_MEM_ERROR)
        return WTF::nullopt;

    Vector<uint8_t> data(length);
    ret = asn1_der_coding(root, elementName, data.data(), &length, nullptr);
    if (ret != ASN1_SUCCESS)
        return WTF::nullopt;

    return data;
}

bool writeElement(asn1_node root, const char* elementName, const void* data, size_t dataSize)
{
    int ret = asn1_write_value(root, elementName, data, dataSize);
    return ret == ASN1_SUCCESS;
}

} // namespace TASN1
} // namespace PAL
