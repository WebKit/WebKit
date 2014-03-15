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

#ifndef MockBox_h
#define MockBox_h

#if ENABLE(MEDIA_SOURCE)

#include <wtf/MediaTime.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class ArrayBuffer;
}

namespace WebCore {

// A MockBox represents a ISO-BMFF like data structure. Data in the
// structure is little-endian. The layout of the data structure as follows:
//
// 4 bytes : 4CC : identifier
// 4 bytes : unsigned : length
// 
class MockBox {
public:
    static String peekType(JSC::ArrayBuffer*);
    static size_t peekLength(JSC::ArrayBuffer*);

    size_t length() const { return m_length; }
    const String& type() const { return m_type; }

protected:
    MockBox(JSC::ArrayBuffer*);

    size_t m_length;
    String m_type;
};

// A MockTrackBox extends MockBox and expects the following
// data structure:
//
// 4 bytes : 4CC : identifier = 'trak'
// 4 bytes : unsigned : length = 17
// 4 bytes : signed : track ID
// 4 bytes : 4CC : codec
// 1 byte  : unsigned : kind
// 
class MockTrackBox final : public MockBox {
public:
    static const String& type();
    MockTrackBox(JSC::ArrayBuffer*);

    int32_t trackID() const { return m_trackID; }

    const String& codec() const { return m_codec; }

    enum TrackKind { Audio, Video, Text };
    TrackKind kind() const { return m_kind; }

protected:
    uint8_t m_trackID;
    String m_codec;
    TrackKind m_kind;
};

// A MockInitializationBox extends MockBox and contains 0 or more
// MockTrackBoxes. It expects the following data structure:
//
// 4 bytes : 4CC : identifier = 'init'
// 4 bytes : unsigned : length = 16 + (13 * num tracks)
// 4 bytes : signed : duration time value
// 4 bytes : signed : duration time scale
// N bytes : MockTrackBoxes : tracks
//
class MockInitializationBox final : public MockBox {
public:
    static const String& type();
    MockInitializationBox(JSC::ArrayBuffer*);

    MediaTime duration() const { return m_duration; }
    const Vector<MockTrackBox>& tracks() const { return m_tracks; }

protected:
    MediaTime m_duration;
    Vector<MockTrackBox> m_tracks;
};

// A MockSampleBox extends MockBox and expects the following data structure:
//
// 4 bytes : 4CC : identifier = 'smpl'
// 4 bytes : unsigned : length = 29
// 4 bytes : signed : time scale
// 4 bytes : signed : presentation time value
// 4 bytes : signed : decode time value
// 4 bytes : signed : duration time value
// 4 bytes : signed : track ID
// 1 byte  : unsigned : flags
//
class MockSampleBox final : public MockBox {
public:
    static const String& type();
    MockSampleBox(JSC::ArrayBuffer*);

    MediaTime presentationTimestamp() const { return m_presentationTimestamp; }
    MediaTime decodeTimestamp() const { return m_decodeTimestamp; }
    MediaTime duration() const { return m_duration; }
    int32_t trackID() const { return m_trackID; }
    uint8_t flags() const { return m_flags; }

    enum {
        IsSync = 1 << 0,
        IsCorrupted = 1 << 1,
        IsDropped = 1 << 2,
        IsDelayed = 1 << 3,
    };
    bool isSync() const { return m_flags & IsSync; }
    bool isCorrupted() const { return m_flags & IsCorrupted; }
    bool isDropped() const { return m_flags & IsDropped; }
    bool isDelayed() const { return m_flags & IsDelayed; }

protected:
    MediaTime m_presentationTimestamp;
    MediaTime m_decodeTimestamp;
    MediaTime m_duration;
    int32_t m_trackID;
    uint8_t m_flags;
};

}

#endif

#endif
