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

#if USE(CURL)

#include "Test.h"
#include <WebKit/WKCertificateInfo.h>
#include <WebKit/WKCertificateInfoCurl.h>
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

namespace Curl {

static const char* PEM1 =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBsjCCAVygAwIBAgIJALO5qdAF7ldHMA0GCSqGSIb3DQEBCwUAMDMxCzAJBgNV\n"
    "BAYTAlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMQ8wDQYDVQQKDAZXZWJLaXQwHhcN\n"
    "MTgxMTE2MDEwMDM4WhcNMjgwODE1MDEwMDM4WjAzMQswCQYDVQQGEwJVUzETMBEG\n"
    "A1UECAwKQ2FsaWZvcm5pYTEPMA0GA1UECgwGV2ViS2l0MFwwDQYJKoZIhvcNAQEB\n"
    "BQADSwAwSAJBAOfoq7BspXakZ1EFEIkw+PqcX0KkichT0DK/6ghxdbcS5TDGnUOM\n"
    "rfYhnI/vzKMcrvOXmqyYRGj7FwthWkX9FfMCAwEAAaNTMFEwHQYDVR0OBBYEFArX\n"
    "jKITLym24m6IwYv6F5Gg5yw3MB8GA1UdIwQYMBaAFArXjKITLym24m6IwYv6F5Gg\n"
    "5yw3MA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADQQC8pO97bNmeMb0P\n"
    "m+Yvc7eI6Vu2eXpNUIUSHHn7f+Hj584ZeG7gMgCeJqlEeFs9zM3uQMhn4+Nd7pDa\n"
    "GdGdcOzA\n"
    "-----END CERTIFICATE-----\n";

static const char* PEM2 =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBVDCB/wIJAPATaMb5RLofMA0GCSqGSIb3DQEBCwUAMDMxCzAJBgNVBAYTAlVT\n"
    "MRMwEQYDVQQIDApDYWxpZm9ybmlhMQ8wDQYDVQQKDAZXZWJLaXQwHhcNMTgxMTE2\n"
    "MDEwNDAxWhcNMjgxMTEzMDEwNDAxWjAwMQswCQYDVQQGEwJKUDEOMAwGA1UECAwF\n"
    "VG9reW8xETAPBgNVBAoMCFdlYktpdEpQMFwwDQYJKoZIhvcNAQEBBQADSwAwSAJB\n"
    "AK9SYK8fewlFAFU9EMnE6zQSNlqVMhaKiaxBFK8qdoK5iFdEffC+4may1sdSujNr\n"
    "m4uOGBpgTMcjeDWjorItMMECAwEAATANBgkqhkiG9w0BAQsFAANBACsY4D8ieDvJ\n"
    "gdTpbzgAqF0YyNGbWAWQ75ELVhZS3RJgwV5jTe+wc7zB12BIyzexoHzzlWpqU6Y5\n"
    "iA28XceqQR0=\n"
    "-----END CERTIFICATE-----\n";

static auto makeCertificateInfo(std::vector<const char*> pems)
{
    auto certificateChain = adoptWK(WKMutableArrayCreate());

    for (auto pem : pems) {
        auto data = adoptWK(WKDataCreate(reinterpret_cast<const uint8_t*>(pem), std::strlen(pem)));
        WKArrayAppendItem(certificateChain.get(), data.get());
    }

    return WKCertificateInfoCreateWithCertficateChain(certificateChain.get());
}

static bool isSamePEM(WKDataRef data, const char* pem)
{
    size_t size = std::strlen(pem);
    if (size != WKDataGetSize(data))
        return false;

    return !std::memcmp(WKDataGetBytes(data), pem, size);
}

TEST(Curl, CertificateAPI)
{
    auto certificateInfo = adoptWK(makeCertificateInfo({ PEM1, PEM2 }));

    auto size = WKCertificateInfoGetCertificateChainSize(certificateInfo.get());
    ASSERT_EQ(size, 2);
    ASSERT_EQ(WKCertificateInfoGetVerificationError(certificateInfo.get()), 0);
    ASSERT_TRUE(WKStringIsEqualToUTF8CString(adoptWK(WKCertificateInfoCopyVerificationErrorDescription(certificateInfo.get())).get(), "ok"));
    ASSERT_TRUE(isSamePEM(adoptWK(WKCertificateInfoCopyCertificateAtIndex(certificateInfo.get(), 0)).get(), PEM1));
    ASSERT_TRUE(isSamePEM(adoptWK(WKCertificateInfoCopyCertificateAtIndex(certificateInfo.get(), 1)).get(), PEM2));
}

}

} // namespace TestWebKitAPI

#endif // USE(CURL)
