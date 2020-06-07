/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "VideoTrackPrivate.h"

namespace WebCore {

class VideoTrackPrivateAVF : public VideoTrackPrivate {
    WTF_MAKE_NONCOPYABLE(VideoTrackPrivateAVF)
public:
    int trackIndex() const override { return m_index; }
    Kind kind() const override { return m_kind; }
    AtomString id() const override { return m_id; }
    AtomString label() const override { return m_label; }
    AtomString language() const override { return m_language; }

protected:
    void setKind(Kind kind) { m_kind = kind; }
    void setId(const AtomString& newId) { m_id = newId; }
    void setLabel(const AtomString& label) { m_label = label; }
    void setLanguage(const AtomString& language) { m_language = language; }
    void setTrackIndex(int index) { m_index = index; }

    Kind m_kind { None };
    AtomString m_id;
    AtomString m_label;
    AtomString m_language;
    int m_index { 0 };

    VideoTrackPrivateAVF() = default;
};

}

#endif // ENABLE(VIDEO)
