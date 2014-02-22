/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef CDM_h
#define CDM_h

#if ENABLE(ENCRYPTED_MEDIA_V2)

#include "CDMSession.h"
#include <runtime/Uint8Array.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CDM;
class CDMPrivateInterface;
class MediaPlayer;

typedef PassOwnPtr<CDMPrivateInterface> (*CreateCDM)(CDM*);
typedef bool (*CDMSupportsKeySystem)(const String&);
typedef bool (*CDMSupportsKeySystemAndMimeType)(const String&, const String&);

class CDMClient {
public:
    virtual ~CDMClient() { }

    virtual MediaPlayer* cdmMediaPlayer(const CDM*) const = 0;
};

class CDM {
public:

    enum CDMErrorCode { UnknownError = 1, ClientError, ServiceError, OutputError, HardwareChangeError, DomainError };
    static bool supportsKeySystem(const String&);
    static bool keySystemSupportsMimeType(const String& keySystem, const String& mimeType);
    static PassOwnPtr<CDM> create(const String& keySystem);
    static void registerCDMFactory(CreateCDM, CDMSupportsKeySystem, CDMSupportsKeySystemAndMimeType);
    ~CDM();

    bool supportsMIMEType(const String&) const;
    std::unique_ptr<CDMSession> createSession();

    const String& keySystem() const { return m_keySystem; }

    CDMClient* client() const { return m_client; }
    void setClient(CDMClient* client) { m_client = client; }

    MediaPlayer* mediaPlayer() const;

private:
    CDM(const String& keySystem);

    String m_keySystem;
    OwnPtr<CDMPrivateInterface> m_private;
    CDMClient* m_client;
};

}

#endif // ENABLE(ENCRYPTED_MEDIA_V2)

#endif // CDM_h
