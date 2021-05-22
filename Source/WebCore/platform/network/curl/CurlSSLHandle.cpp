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

#if NEED_OPENSSL_THREAD_SUPPORT && OS(WINDOWS)
#include <wtf/Threading.h>
#endif

namespace WebCore {

CurlSSLHandle::CurlSSLHandle()
{
#if NEED_OPENSSL_THREAD_SUPPORT
    ThreadSupport::setup();
#endif

    platformInitialize();
}

void CurlSSLHandle::setCACertPath(String&& caCertPath)
{
    RELEASE_ASSERT(!caCertPath.isEmpty());
    m_caCertInfo = WTFMove(caCertPath);
}

void CurlSSLHandle::setCACertData(CertificateInfo::Certificate&& caCertData)
{
    RELEASE_ASSERT(!caCertData.isEmpty());
    m_caCertInfo = WTFMove(caCertData);
}

void CurlSSLHandle::clearCACertInfo()
{
    m_caCertInfo = WTF::Monostate { };
}

void CurlSSLHandle::allowAnyHTTPSCertificatesForHost(const String& host)
{
    LockHolder mutex(m_allowedHostsLock);

    m_allowedHosts.addVoid(host);
}

bool CurlSSLHandle::canIgnoreAnyHTTPSCertificatesForHost(const String& host) const
{
    LockHolder mutex(m_allowedHostsLock);

    return m_allowedHosts.contains(host);
}

void CurlSSLHandle::setClientCertificateInfo(const String& hostName, const String& certificate, const String& key)
{
    LockHolder mutex(m_allowedClientHostsLock);

    m_allowedClientHosts.set(hostName, ClientCertificate { certificate, key });
}

Optional<CurlSSLHandle::ClientCertificate> CurlSSLHandle::getSSLClientCertificate(const String& hostName) const
{
    LockHolder mutex(m_allowedClientHostsLock);

    auto it = m_allowedClientHosts.find(hostName);
    if (it == m_allowedClientHosts.end())
        return WTF::nullopt;

    return it->value;
}

#if NEED_OPENSSL_THREAD_SUPPORT

void CurlSSLHandle::ThreadSupport::setup()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        singleton();
    });
}

CurlSSLHandle::ThreadSupport::ThreadSupport()
{
    CRYPTO_set_locking_callback(lockingCallback);
#if OS(WINDOWS)
    CRYPTO_THREADID_set_callback(threadIdCallback);
#endif
}

void CurlSSLHandle::ThreadSupport::lockingCallback(int mode, int type, const char*, int)
{
    RELEASE_ASSERT(type >= 0 && type < CRYPTO_NUM_LOCKS);
    auto& locker = ThreadSupport::singleton();

    if (mode & CRYPTO_LOCK)
        locker.lock(type);
    else
        locker.unlock(type);
}

#if OS(WINDOWS)

void CurlSSLHandle::ThreadSupport::threadIdCallback(CRYPTO_THREADID* threadId)
{
    CRYPTO_THREADID_set_numeric(threadId, static_cast<unsigned long>(Thread::currentID()));
}

#endif

#endif

}

#endif
