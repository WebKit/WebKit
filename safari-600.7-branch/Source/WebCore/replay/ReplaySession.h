/*
 * Copyright (C) 2013 University of Washington. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ReplaySession_h
#define ReplaySession_h

#if ENABLE(WEB_REPLAY)

#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class ReplaySessionSegment;

typedef Vector<RefPtr<ReplaySessionSegment>>::const_iterator SegmentIterator;

class ReplaySession : public RefCounted<ReplaySession> {
    WTF_MAKE_NONCOPYABLE(ReplaySession);
public:
    static PassRefPtr<ReplaySession> create();
    ~ReplaySession();

    double timestamp() const { return m_timestamp; }
    unsigned identifier() const { return m_identifier; }

    size_t size() const { return m_segments.size(); }
    PassRefPtr<ReplaySessionSegment> at(size_t position) const;

    SegmentIterator begin() const { return m_segments.begin(); }
    SegmentIterator end() const { return m_segments.end(); }

    void appendSegment(PassRefPtr<ReplaySessionSegment>);
    void insertSegment(size_t position, PassRefPtr<ReplaySessionSegment>);
    void removeSegment(size_t position);

private:
    ReplaySession();

    Vector<RefPtr<ReplaySessionSegment>> m_segments;
    unsigned m_identifier;
    double m_timestamp;
};

} // namespace WebCore

#endif // ENABLE(WEB_REPLAY)

#endif // ReplaySession_h
