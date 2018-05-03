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

#pragma once

#include <openssl/crypto.h>
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/StringHash.h>

// all version of LibreSSL and OpenSSL prior to 1.1.0 need thread support
#if defined(LIBRESSL_VERSION_NUMBER) || (defined(OPENSSL_VERSION_NUMBER) && OPENSSL_VERSION_NUMBER < 0x1010000fL)
#define NEED_OPENSSL_THREAD_SUPPORT 1
#else
#define NEED_OPENSSL_THREAD_SUPPORT 0
#endif

namespace WebCore {

class CurlSSLHandle {
    WTF_MAKE_NONCOPYABLE(CurlSSLHandle);
    friend NeverDestroyed<CurlSSLHandle>;

public:
    CurlSSLHandle();

    using ClientCertificate = std::pair<String, String>;

    bool shouldIgnoreSSLErrors() const { return m_ignoreSSLErrors; }
    const char* getCACertPath() const { return m_caCertPath.data(); }

    WEBCORE_EXPORT void setHostAllowsAnyHTTPSCertificate(const String&);
    bool isAllowedHTTPSCertificateHost(const String&);
    bool canIgnoredHTTPSCertificate(const String&, const ListHashSet<String>&);

    void setClientCertificateInfo(const String&, const String&, const String&);
    std::optional<ClientCertificate> getSSLClientCertificate(const String&);

private:
    CString getCACertPathEnv();

#if NEED_OPENSSL_THREAD_SUPPORT
    class ThreadSupport {
        friend NeverDestroyed<CurlSSLHandle::ThreadSupport>;
        
    public:
        static void setup();

    private:
        static ThreadSupport& singleton()
        {
            static NeverDestroyed<CurlSSLHandle::ThreadSupport> shared;
            return shared;
        }

        ThreadSupport();
        void lock(int type) { m_locks[type].lock(); }
        void unlock(int type) { m_locks[type].unlock(); }

        Lock m_locks[CRYPTO_NUM_LOCKS];

        static void lockingCallback(int mode, int type, const char* file, int line);
#if OS(WINDOWS)
        static void threadIdCallback(CRYPTO_THREADID*);
#endif
    };
#endif

    bool m_ignoreSSLErrors { false };
    CString m_caCertPath;

    Lock m_mutex;
    HashMap<String, ListHashSet<String>, ASCIICaseInsensitiveHash> m_allowedHosts;
    HashMap<String, ClientCertificate, ASCIICaseInsensitiveHash> m_allowedClientHosts;
};

} // namespace WebCore
