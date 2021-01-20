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

#pragma once

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include <JavaScriptCore/Uint8Array.h>
#include <wtf/Forward.h>

namespace WebCore {

class LegacyCDMSessionClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~LegacyCDMSessionClient() = default;
    virtual void sendMessage(Uint8Array*, String destinationURL) = 0;

    enum : uint8_t {
        MediaKeyErrorUnknown = 1,
        MediaKeyErrorClient,
        MediaKeyErrorService,
        MediaKeyErrorOutput,
        MediaKeyErrorHardwareChange,
        MediaKeyErrorDomain,
    };
    typedef unsigned short MediaKeyErrorCode;
    virtual void sendError(MediaKeyErrorCode, uint32_t systemCode) = 0;

    virtual String mediaKeysStorageDirectory() const = 0;
};

enum LegacyCDMSessionType {
    CDMSessionTypeUnknown,
    CDMSessionTypeClearKey,
    CDMSessionTypeAVFoundationObjC,
    CDMSessionTypeAVStreamSession,
    CDMSessionTypeAVContentKeySession,
    CDMSessionTypeRemote,
};

class WEBCORE_EXPORT LegacyCDMSession {
public:
    virtual ~LegacyCDMSession() = default;

    virtual LegacyCDMSessionType type() { return CDMSessionTypeUnknown; }
    virtual void setClient(LegacyCDMSessionClient*) = 0;
    virtual const String& sessionId() const = 0;
    virtual RefPtr<Uint8Array> generateKeyRequest(const String& mimeType, Uint8Array* initData, String& destinationURL, unsigned short& errorCode, uint32_t& systemCode) = 0;
    virtual void releaseKeys() = 0;
    virtual bool update(Uint8Array*, RefPtr<Uint8Array>& nextMessage, unsigned short& errorCode, uint32_t& systemCode) = 0;
    virtual RefPtr<ArrayBuffer> cachedKeyForKeyID(const String&) const { return nullptr; }
};

}

#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA)
