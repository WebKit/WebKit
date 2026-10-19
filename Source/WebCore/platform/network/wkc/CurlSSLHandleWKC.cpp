/*
 * WKC platform TLS defaults for the modern libcurl networking backend.
 *
 * The Wii U port uses libcurl built against mbedTLS. Leave cipher-suite and
 * elliptic-curve selection to that backend's compiled-in defaults: the
 * OpenSSL-style cipher/curve strings used by other ports are not supported by
 * libcurl's mbedTLS backend. Peer and hostname verification stay enabled.
 */

#include "config.h"
#include "CurlSSLHandle.h"

namespace WebCore {

void CurlSSLHandle::platformInitialize()
{
    setIgnoreSSLErrors(false);
}

} // namespace WebCore
