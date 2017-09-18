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
#include "CurlSSLHandle.h"

#if USE(CURL)

#if USE(CF)
#if OS(WINDOWS)
#include "WebCoreBundleWin.h"
#endif

#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

CurlSSLHandle::CurlSSLHandle()
    : m_caCertPath(getCACertPathEnv())
{
    char* ignoreSSLErrors = getenv("WEBKIT_IGNORE_SSL_ERRORS");
    if (ignoreSSLErrors)
        m_ignoreSSLErrors = true;
}

CString CurlSSLHandle::getCACertPathEnv()
{
    char* envPath = getenv("CURL_CA_BUNDLE_PATH");
    if (envPath)
        return envPath;

#if USE(CF)
    CFBundleRef webKitBundleRef = webKitBundle();
    if (webKitBundleRef) {
        RetainPtr<CFURLRef> certURLRef = adoptCF(CFBundleCopyResourceURL(webKitBundleRef, CFSTR("cacert"), CFSTR("pem"), CFSTR("certificates")));
        if (certURLRef) {
            char path[MAX_PATH];
            CFURLGetFileSystemRepresentation(certURLRef.get(), false, reinterpret_cast<UInt8*>(path), MAX_PATH);
            return path;
        }
    }
#endif

    return CString();
}

void CurlSSLHandle::setHostAllowsAnyHTTPSCertificate(const String& hostName)
{
    LockHolder mutex(m_mutex);

    ListHashSet<String> certificates;
    m_allowedHosts.set(hostName, certificates);
}

bool CurlSSLHandle::isAllowedHTTPSCertificateHost(const String& hostName)
{
    LockHolder mutex(m_mutex);

    auto it = m_allowedHosts.find(hostName);
    return (it != m_allowedHosts.end());
}

bool CurlSSLHandle::canIgnoredHTTPSCertificate(const String& hostName, const ListHashSet<String>& certificates)
{
    LockHolder mutex(m_mutex);

    auto found = m_allowedHosts.find(hostName);
    if (found == m_allowedHosts.end())
        return false;

    auto& value = found->value;
    if (value.isEmpty()) {
        value = certificates;
        return true;
    }

    return std::equal(certificates.begin(), certificates.end(), value.begin());
}

void CurlSSLHandle::setClientCertificateInfo(const String& hostName, const String& certificate, const String& key)
{
    LockHolder mutex(m_mutex);

    ClientCertificate clientInfo(certificate, key);
    m_allowedClientHosts.set(hostName, clientInfo);
}

std::optional<CurlSSLHandle::ClientCertificate> CurlSSLHandle::getSSLClientCertificate(const String& hostName)
{
    LockHolder mutex(m_mutex);

    auto it = m_allowedClientHosts.find(hostName);
    if (it == m_allowedClientHosts.end())
        return std::nullopt;

    return it->value;
}

}

#endif
