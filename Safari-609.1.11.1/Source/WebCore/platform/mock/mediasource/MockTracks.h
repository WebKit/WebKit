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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#if ENABLE(MEDIA_SOURCE)

#include "AudioTrackPrivate.h"
#include "InbandTextTrackPrivate.h"
#include "MockBox.h"
#include "VideoTrackPrivate.h"

namespace WebCore {

class MockAudioTrackPrivate : public AudioTrackPrivate {
public:
    static Ref<MockAudioTrackPrivate> create(const MockTrackBox& box) { return adoptRef(*new MockAudioTrackPrivate(box)); }
    virtual ~MockAudioTrackPrivate() = default;

    virtual AtomString id() const { return m_id; }

protected:
    MockAudioTrackPrivate(const MockTrackBox& box)
        : m_box(box)
        , m_id(AtomString::number(box.trackID()))
    {
    }
    MockTrackBox m_box;
    AtomString m_id;
};

class MockTextTrackPrivate : public InbandTextTrackPrivate {
public:
    static Ref<MockTextTrackPrivate> create(const MockTrackBox& box) { return adoptRef(*new MockTextTrackPrivate(box)); }
    virtual ~MockTextTrackPrivate() = default;

    virtual AtomString id() const { return m_id; }

protected:
    MockTextTrackPrivate(const MockTrackBox& box)
        : InbandTextTrackPrivate(InbandTextTrackPrivate::Generic)
        , m_box(box)
        , m_id(AtomString::number(box.trackID()))
    {
    }
    MockTrackBox m_box;
    AtomString m_id;
};


class MockVideoTrackPrivate : public VideoTrackPrivate {
public:
    static Ref<MockVideoTrackPrivate> create(const MockTrackBox& box) { return adoptRef(*new MockVideoTrackPrivate(box)); }
    virtual ~MockVideoTrackPrivate() = default;

    virtual AtomString id() const { return m_id; }

protected:
    MockVideoTrackPrivate(const MockTrackBox& box)
        : m_box(box)
        , m_id(AtomString::number(box.trackID()))
    {
    }
    MockTrackBox m_box;
    AtomString m_id;
};

}

#endif
