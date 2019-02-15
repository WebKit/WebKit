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
#include "CurlSSLHandle.h"

#include <wtf/Environment.h>

#if USE(CF)
#if OS(WINDOWS)
#include "WebCoreBundleWin.h"
#endif

#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

static String getCACertPathEnv()
{
    if (auto envPath = Environment::get("CURL_CA_BUNDLE_PATH"))
        return *envPath;

#if USE(CF)
    CFBundleRef webKitBundleRef = webKitBundle();
    if (webKitBundleRef) {
        RetainPtr<CFURLRef> certURLRef = adoptCF(CFBundleCopyResourceURL(webKitBundleRef, CFSTR("cacert"), CFSTR("pem"), CFSTR("certificates")));
        if (certURLRef) {
            char path[MAX_PATH];
            if (CFURLGetFileSystemRepresentation(certURLRef.get(), false, reinterpret_cast<UInt8*>(path), MAX_PATH) && *path)
                return String(path);
        }
    }
#endif

    return String();
}

void CurlSSLHandle::platformInitialize()
{
    auto caCertPath = getCACertPathEnv();
    if (!caCertPath.isEmpty())
        setCACertPath(WTFMove(caCertPath));
}

}
