/*
 * Copyright (C) 2019 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"
#include "RTCDTMFSenderBackend.h"
#include <wtf/WeakPtr.h>

ALLOW_UNUSED_PARAMETERS_BEGIN
ALLOW_COMMA_BEGIN
#include <webrtc/api/dtmf_sender_interface.h>
#include <webrtc/api/scoped_refptr.h>

ALLOW_UNUSED_PARAMETERS_END
ALLOW_COMMA_END

namespace WebCore {

// Use eager initialization for the WeakPtrFactory since we construct WeakPtrs on another thread.
class LibWebRTCDTMFSenderBackend final : public RTCDTMFSenderBackend, private webrtc::DtmfSenderObserverInterface, public CanMakeWeakPtr<LibWebRTCDTMFSenderBackend, WeakPtrFactoryInitialization::Eager> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit LibWebRTCDTMFSenderBackend(rtc::scoped_refptr<webrtc::DtmfSenderInterface>&&);
    ~LibWebRTCDTMFSenderBackend();

private:
    // RTCDTMFSenderBackend
    bool canInsertDTMF() final;
    void playTone(const char tone, size_t duration, size_t interToneGap) final;
    void onTonePlayed(Function<void()>&&) final;
    String tones() const final;
    size_t duration() const final;
    size_t interToneGap() const final;

    // DtmfSenderObserverInterface
    void OnToneChange(const std::string& tone, const std::string&) final;

    rtc::scoped_refptr<webrtc::DtmfSenderInterface> m_sender;
    Function<void()> m_onTonePlayed;
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
