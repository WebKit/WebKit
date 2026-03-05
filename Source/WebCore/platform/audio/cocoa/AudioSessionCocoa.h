/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>
#if USE(AUDIO_SESSION) && PLATFORM(COCOA)

#include <WebCore/AudioSession.h>
#include <wtf/TZoneMalloc.h>

namespace WTF {
class WorkQueue;
}

namespace WebCore {

class AudioSessionCocoa : public AudioSession, public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AudioSessionCocoa> {
    WTF_MAKE_TZONE_ALLOCATED(AudioSessionCocoa);
public:
    virtual ~AudioSessionCocoa();

    bool isEligibleForSmartRouting() const { return m_isEligibleForSmartRouting; }

    enum class ForceUpdate : bool { No, Yes };
    void setEligibleForSmartRouting(bool, ForceUpdate = ForceUpdate::No);

    // AudioSession.
    WTF_ABSTRACT_THREAD_SAFE_REF_COUNTED_AND_CAN_MAKE_WEAK_PTR_IMPL;

protected:
    AudioSessionCocoa();

    static void setEligibleForSmartRoutingInternal(bool);

    // AudioSession
    bool tryToSetActiveInternal(bool) final;
    void setCategory(CategoryType, Mode, RouteSharingPolicy) override;

    bool m_isEligibleForSmartRouting { false };
    const Ref<WTF::WorkQueue> m_workQueue;
};

}

#endif
