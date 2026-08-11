/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_CODECS)

#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class WebCodecsImageTrack final : public RefCounted<WebCodecsImageTrack> {
    WTF_MAKE_TZONE_ALLOCATED(WebCodecsImageTrack);
public:
    static Ref<WebCodecsImageTrack> create();
    static Ref<WebCodecsImageTrack> create(float repetitionCount, size_t frameCount, bool animated, bool selected);

    float repetitionCount() const { return m_repetitionCount; }
    size_t frameCount() const { return m_frameCount; }
    bool animated() const { return m_animated; }

    bool selected() const { return m_selected; }
    void setSelected(bool selected) { m_selected = selected; }

private:
    WebCodecsImageTrack() = default;
    WebCodecsImageTrack(float repetitionCount, size_t frameCount, bool animated, bool selected);

    float m_repetitionCount { 1 };
    size_t m_frameCount { 0 };
    bool m_animated { false };
    bool m_selected { false };
};

} // namespace WebCore

#endif
