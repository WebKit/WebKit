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

#if ENABLE(WEB_RTC)

#include "ExceptionOr.h"
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class CryptoKey;

class RTCRtpSFrameTransformer : public ThreadSafeRefCounted<RTCRtpSFrameTransformer, WTF::DestructionThread::Main> {
public:
    WEBCORE_EXPORT static Ref<RTCRtpSFrameTransformer> create();
    WEBCORE_EXPORT ~RTCRtpSFrameTransformer();

    void setIsSending(bool);
    void setIsProcessingAudio(bool);

    WEBCORE_EXPORT ExceptionOr<void> setEncryptionKey(const Vector<uint8_t>& rawKey, Optional<uint64_t>);
    WEBCORE_EXPORT ExceptionOr<Vector<uint8_t>> transform(const uint8_t*, size_t);

    const Vector<uint8_t>& authenticationKey() const { return m_authenticationKey; }
    const Vector<uint8_t>& encryptionKey() const { return m_encryptionKey; }
    const Vector<uint8_t>& saltKey() const { return m_saltKey; }

    uint64_t counter() const { return m_counter; }
    void setCounter(uint64_t counter) { m_counter = counter; }

private:
    WEBCORE_EXPORT RTCRtpSFrameTransformer();

    ExceptionOr<Vector<uint8_t>> decryptFrame(const uint8_t*, size_t);
    ExceptionOr<Vector<uint8_t>> encryptFrame(const uint8_t*, size_t);

    ExceptionOr<Vector<uint8_t>> computeSaltKey(const Vector<uint8_t>&);
    ExceptionOr<Vector<uint8_t>> computeAuthenticationKey(const Vector<uint8_t>&);
    ExceptionOr<Vector<uint8_t>> computeEncryptionKey(const Vector<uint8_t>&);

    ExceptionOr<Vector<uint8_t>> encryptData(const uint8_t*, size_t, const Vector<uint8_t>& iv, const Vector<uint8_t>& key);
    ExceptionOr<Vector<uint8_t>> decryptData(const uint8_t*, size_t, const Vector<uint8_t>& iv, const Vector<uint8_t>& key);
    Vector<uint8_t> computeEncryptedDataSignature(const uint8_t*, size_t, const Vector<uint8_t>& key);

    Lock m_keyLock;
    bool m_hasKey { false };
    Vector<uint8_t> m_authenticationKey;
    Vector<uint8_t> m_encryptionKey;
    Vector<uint8_t> m_saltKey;

    bool m_isSending { false };
    uint64_t m_authenticationSize { 10 };
    uint64_t m_keyId { 0 };
    uint64_t m_counter { 0 };
};

inline void RTCRtpSFrameTransformer::setIsSending(bool isSending)
{
    m_isSending = isSending;
}

inline void RTCRtpSFrameTransformer::setIsProcessingAudio(bool isProcessingAudio)
{
    m_authenticationSize = isProcessingAudio ? 4 : 10;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
