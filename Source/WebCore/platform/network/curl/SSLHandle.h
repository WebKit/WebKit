/*
 * Copyright (C) 2013 University of Szeged
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

#ifndef SSLHandle_h
#define SSLHandle_h

#include "ResourceHandle.h"

#include <wtf/text/WTFString.h>

namespace WebCore {

typedef enum {
    SSL_CERTIFICATE_UNKNOWN_CA = (1 << 0), // The signing certificate authority is not known.
    SSL_CERTIFICATE_BAD_IDENTITY = (1 << 1), // The certificate does not match the expected identity of the site that it was retrieved from.
    SSL_CERTIFICATE_NOT_ACTIVATED = (1 << 2), // The certificate's activation time is still in the future
    SSL_CERTIFICATE_EXPIRED = (1 << 3), // The certificate has expired
    SSL_CERTIFICATE_REVOKED = (1 << 4), // The certificate has been revoked
    SSL_CERTIFICATE_INSECURE = (1 << 5), // The certificate's algorithm is considered insecure.
    SSL_CERTIFICATE_GENERIC_ERROR = (1 << 6) // Some other error occurred validating the certificate
} SSLCertificateFlags;


void addAllowedClientCertificate(const String&, const String&, const String&);
void allowsAnyHTTPSCertificateHosts(const String&);
bool sslIgnoreHTTPSCertificate(const String&, const String&);
void setSSLVerifyOptions(ResourceHandle*);
void setSSLClientCertificate(ResourceHandle*);

}

#endif
