/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#include <wtf/LoggerHelper.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class Element;
class HTMLMediaElement;
class SourceBuffer;

class TrackBase
    : public RefCounted<TrackBase>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    virtual ~TrackBase() = default;

    enum Type { BaseTrack, TextTrack, AudioTrack, VideoTrack };
    Type type() const { return m_type; }

    virtual void setMediaElement(HTMLMediaElement*);
    HTMLMediaElement* mediaElement() { return m_mediaElement; }
    virtual Element* element();

    virtual AtomString id() const { return m_id; }
    virtual void setId(const AtomString& id) { m_id = id; }

    AtomString label() const { return m_label; }
    void setLabel(const AtomString& label) { m_label = label; }

    AtomString validBCP47Language() const;
    AtomString language() const { return m_language; }
    virtual void setLanguage(const AtomString&);

    virtual void clearClient() = 0;

    virtual int uniqueId() const { return m_uniqueId; }

#if ENABLE(MEDIA_SOURCE)
    SourceBuffer* sourceBuffer() const { return m_sourceBuffer; }
    void setSourceBuffer(SourceBuffer* buffer) { m_sourceBuffer = buffer; }
#endif

    virtual bool enabled() const = 0;

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { ASSERT(m_logger); return *m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
#endif

protected:
    TrackBase(Type, const AtomString& id, const AtomString& label, const AtomString& language);

    HTMLMediaElement* m_mediaElement { nullptr };

#if ENABLE(MEDIA_SOURCE)
    SourceBuffer* m_sourceBuffer { nullptr };
#endif

private:
    Type m_type;
    int m_uniqueId;
    AtomString m_id;
    AtomString m_label;
    AtomString m_language;
    AtomString m_validBCP47Language;
#if !RELEASE_LOG_DISABLED
    RefPtr<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

class MediaTrackBase : public TrackBase {
public:
    const AtomString& kind() const { return m_kind; }
    virtual void setKind(const AtomString&);

protected:
    MediaTrackBase(Type, const AtomString& id, const AtomString& label, const AtomString& language);

    void setKindInternal(const AtomString&);

private:
    virtual bool isValidKind(const AtomString&) const = 0;

    AtomString m_kind;
};

} // namespace WebCore

#endif
