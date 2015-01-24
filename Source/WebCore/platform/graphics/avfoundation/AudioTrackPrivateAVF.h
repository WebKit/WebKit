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

#ifndef AudioTrackPrivateAVF_h
#define AudioTrackPrivateAVF_h

#if ENABLE(VIDEO_TRACK)

#include "AudioTrackPrivate.h"

namespace WebCore {

class AudioTrackPrivateAVF : public AudioTrackPrivate {
    WTF_MAKE_NONCOPYABLE(AudioTrackPrivateAVF)
public:

    virtual Kind kind() const { return m_kind; }
    virtual AtomicString id() const { return m_id; }
    virtual AtomicString label() const { return m_label; }
    virtual AtomicString language() const { return m_language; }

protected:
    void setKind(Kind kind) { m_kind = kind; }
    void setId(AtomicString newId) { m_id = newId; }
    void setLabel(AtomicString label) { m_label = label; }
    void setLanguage(AtomicString language) { m_language = language; }

    Kind m_kind;
    AtomicString m_id;
    AtomicString m_label;
    AtomicString m_language;

    AudioTrackPrivateAVF()
    : m_kind(None)
    {
    }
};

}

#endif // ENABLE(VIDEO_TRACK)

#endif // AudioTrackPrivateAVF_h
