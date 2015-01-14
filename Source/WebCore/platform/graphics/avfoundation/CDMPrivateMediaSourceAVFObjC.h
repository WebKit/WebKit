/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef CDMPrivateMediaSourceAVFObjC_h
#define CDMPrivateMediaSourceAVFObjC_h

#if ENABLE(ENCRYPTED_MEDIA_V2) && ENABLE(MEDIA_SOURCE)

#include "CDMPrivate.h"

namespace WebCore {

class CDM;

class CDMPrivateMediaSourceAVFObjC : public CDMPrivateInterface {
public:
    explicit CDMPrivateMediaSourceAVFObjC(CDM* cdm)
        : m_cdm(cdm)
    { }
    virtual ~CDMPrivateMediaSourceAVFObjC() { }

    static std::unique_ptr<CDMPrivateInterface> create(CDM* cdm) { return std::make_unique<CDMPrivateMediaSourceAVFObjC>(cdm); }
    static bool supportsKeySystem(const String&);
    static bool supportsKeySystemAndMimeType(const String& keySystem, const String& mimeType);

    virtual bool supportsMIMEType(const String& mimeType) override;
    virtual std::unique_ptr<CDMSession> createSession() override;

    CDM* cdm() const { return m_cdm; }

protected:
    CDM* m_cdm;
};

}

#endif // ENABLE(ENCRYPTED_MEDIA_V2) && ENABLE(MEDIA_SOURCE)

#endif // CDMPrivateMediaSourceAVFObjC_h
