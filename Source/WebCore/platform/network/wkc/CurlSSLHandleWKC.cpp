/*
 * WKC platform TLS defaults for the modern libcurl networking backend.
 *
 * Keep certificate and hostname verification enabled. The actual trusted CA
 * bundle is supplied by the libcurl/OpenSSL runtime configuration; this file
 * only selects interoperable cipher suites and elliptic curves.
 */

#include "config.h"
#include "CurlSSLHandle.h"

namespace WebCore {

void CurlSSLHandle::platformInitialize()
{
    constexpr auto cipherList =
        "ECDHE-ECDSA-AES128-GCM-SHA256:"
        "ECDHE-RSA-AES128-GCM-SHA256:"
        "ECDHE-ECDSA-CHACHA20-POLY1305:"
        "ECDHE-RSA-CHACHA20-POLY1305:"
        "ECDHE-ECDSA-AES256-GCM-SHA384:"
        "ECDHE-RSA-AES256-GCM-SHA384:"
        "AES128-GCM-SHA256:"
        "AES256-GCM-SHA384";

    constexpr auto ecCurves = "X25519:P-256:P-384:P-521";

    setCipherList(cipherList);
    setECCurves(ecCurves);
    setIgnoreSSLErrors(false);
}

} // namespace WebCore
