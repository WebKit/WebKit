/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "ContextDestructionObserver.h"
#include "WebCoreOpaqueRoot.h"
#include <wtf/LoggerHelper.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class SourceBuffer;
class TrackListBase;

class TrackBase
    : public RefCounted<TrackBase>
    , public ContextDestructionObserver
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    virtual ~TrackBase() = default;

    enum Type { BaseTrack, TextTrack, AudioTrack, VideoTrack };
    Type type() const { return m_type; }

    virtual AtomString id() const { return m_id; }
    AtomString label() const { return m_label; }
    AtomString validBCP47Language() const { return m_validBCP47Language; }
    AtomString language() const { return m_language; }

    virtual int uniqueId() const { return m_uniqueId; }

#if ENABLE(MEDIA_SOURCE)
    SourceBuffer* sourceBuffer() const { return m_sourceBuffer; }
    void setSourceBuffer(SourceBuffer* buffer) { m_sourceBuffer = buffer; }
#endif

    void setTrackList(TrackListBase&);
    void clearTrackList();
    TrackListBase* trackList() const;
    WebCoreOpaqueRoot opaqueRoot();

    virtual bool enabled() const = 0;

#if !RELEASE_LOG_DISABLED
    virtual void setLogger(const Logger&, const void*);
    const Logger& logger() const final { ASSERT(m_logger); return *m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
#endif

protected:
    TrackBase(ScriptExecutionContext*, Type, const AtomString& id, const AtomString& label, const AtomString& language);

    virtual void setId(const AtomString& id) { m_id = id; }
    virtual void setLabel(const AtomString& label) { m_label = label; }
    virtual void setLanguage(const AtomString&);

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
    const void* m_logIdentifier { nullptr };
#endif
    WeakPtr<TrackListBase, WeakPtrImplWithEventTargetData> m_trackList;
};

class MediaTrackBase : public TrackBase {
public:
    const AtomString& kind() const { return m_kind; }
    virtual void setKind(const AtomString&);

protected:
    MediaTrackBase(ScriptExecutionContext*, Type, const AtomString& id, const AtomString& label, const AtomString& language);

    void setKindInternal(const AtomString&);

private:
    virtual bool isValidKind(const AtomString&) const = 0;

    AtomString m_kind;
};

inline WebCoreOpaqueRoot root(TrackBase* track)
{
    return track->opaqueRoot();
}

} // namespace WebCore

#endif
