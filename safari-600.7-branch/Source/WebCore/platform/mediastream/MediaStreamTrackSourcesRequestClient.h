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

#ifndef MediaStreamTrackSourcesRequestClient_h
#define MediaStreamTrackSourcesRequestClient_h

#if ENABLE(MEDIA_STREAM)

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class TrackSourceInfo : public RefCounted<TrackSourceInfo> {
public:
    enum SourceKind { Audio, Video };

    static PassRefPtr<TrackSourceInfo> create(const AtomicString& id, SourceKind kind, const AtomicString& label)
    {
        return adoptRef(new TrackSourceInfo(id, kind, label));
    }

    const AtomicString& id() const { return m_id; }
    const AtomicString& label() const { return m_label; }
    SourceKind kind() const { return m_kind; }

private:
    TrackSourceInfo(const AtomicString& id, SourceKind kind, const AtomicString& label)
        : m_id(id)
        , m_kind(kind)
        , m_label(label)
    {
    }
    
    AtomicString m_id;
    SourceKind m_kind;
    AtomicString m_label;
};

class MediaStreamTrackSourcesRequestClient : public RefCounted<MediaStreamTrackSourcesRequestClient> {
public:
    virtual ~MediaStreamTrackSourcesRequestClient() { }

    virtual const String& requestOrigin() const = 0;
    virtual void didCompleteRequest(const Vector<RefPtr<TrackSourceInfo>>&) = 0;

};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStreamTrackSourcesRequestClient_h
