/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "RemoteLegacyCDMIdentifier.h"
#include "RemoteLegacyCDMSessionIdentifier.h"
#include "WebProcessSupplement.h"
#include <WebCore/LegacyCDM.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebKit {
class RemoteLegacyCDMFactory;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::RemoteLegacyCDMFactory> : std::true_type { };
}

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
class Settings;
}

namespace WebCore {
class CDMPrivateInterface;
class LegacyCDM;
}

namespace WebKit {

class GPUProcessConnection;
class RemoteLegacyCDM;
class RemoteLegacyCDMSession;
class WebProcess;


class RemoteLegacyCDMFactory final
    : public WebProcessSupplement
    , public CanMakeWeakPtr<RemoteLegacyCDMFactory> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLegacyCDMFactory);
public:
    explicit RemoteLegacyCDMFactory(WebProcess&);
    virtual ~RemoteLegacyCDMFactory();

    void registerFactory();

    static ASCIILiteral supplementName();

    GPUProcessConnection& gpuProcessConnection();

    void addSession(RemoteLegacyCDMSessionIdentifier, RemoteLegacyCDMSession&);
    void removeSession(RemoteLegacyCDMSessionIdentifier);

    RemoteLegacyCDM* findCDM(WebCore::CDMPrivateInterface*) const;

private:
    bool supportsKeySystem(const String&);
    bool supportsKeySystemAndMimeType(const String&, const String&);
    std::unique_ptr<WebCore::CDMPrivateInterface> createCDM(WebCore::LegacyCDM*);

    HashMap<RemoteLegacyCDMSessionIdentifier, WeakPtr<RemoteLegacyCDMSession>> m_sessions;
    HashMap<RemoteLegacyCDMIdentifier, WeakPtr<RemoteLegacyCDM>> m_cdms;
    HashMap<String, bool> m_supportsKeySystemCache;
    HashMap<std::pair<String, String>, bool> m_supportsKeySystemAndMimeTypeCache;
};

}

#endif
