/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class ImageAnimationTimeline final : public ThreadSafeRefCounted<ImageAnimationTimeline> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(ImageAnimationTimeline, WEBCORE_EXPORT);
public:
    enum class State : uint8_t { Normal, Stopped, Paused, Running };

    WEBCORE_EXPORT static ImageAnimationTimeline& normal();
    static ImageAnimationTimeline& stopped();

    static Ref<ImageAnimationTimeline> createPaused() { return adoptRef(*new ImageAnimationTimeline(State::Paused)); }
    static Ref<ImageAnimationTimeline> createRunning() { return adoptRef(*new ImageAnimationTimeline(State::Running)); }

    State state() const { return m_state; }

    bool allowsAnimation() const { return m_state == State::Normal || m_state == State::Running; }

private:
    explicit ImageAnimationTimeline(State state)
        : m_state(state)
    {
    }

    const State m_state;
};

} // namespace WebCore
