/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LocalMediaStream.h"

#if ENABLE(MEDIA_STREAM)

#include "Event.h"
#include "EventNames.h"
#include "ExclusiveTrackList.h"
#include "MediaStreamFrameController.h"
#include "MultipleTrackList.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

class LocalMediaStream::DispatchUpdateTask : public ScriptExecutionContext::Task {
public:
    typedef void (LocalMediaStream::*Callback)();

    static PassOwnPtr<DispatchUpdateTask> create(PassRefPtr<LocalMediaStream> object, Callback callback)
    {
        return adoptPtr(new DispatchUpdateTask(object, callback));
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        (m_object.get()->*m_callback)();
    }

public:
    DispatchUpdateTask(PassRefPtr<LocalMediaStream> object, Callback callback)
        : m_object(object)
        , m_callback(callback) { }

    RefPtr<LocalMediaStream> m_object;
    Callback m_callback;
};

PassRefPtr<LocalMediaStream> LocalMediaStream::create(MediaStreamFrameController* frameController, const String& label, PassRefPtr<MultipleTrackList> audioTracks, PassRefPtr<ExclusiveTrackList> videoTracks)
{
    return adoptRef(new LocalMediaStream(frameController, label, audioTracks, videoTracks));
}

LocalMediaStream::LocalMediaStream(MediaStreamFrameController* frameController, const String& label, PassRefPtr<MultipleTrackList> audioTracks, PassRefPtr<ExclusiveTrackList> videoTracks)
    : MediaStream(frameController, label, true)
    , m_audioTracks(audioTracks)
    , m_videoTracks(videoTracks)
{
    ASSERT(m_audioTracks);
    ASSERT(m_videoTracks);
    m_audioTracks->associateStream(label);
    m_videoTracks->associateStream(label);
}

LocalMediaStream::~LocalMediaStream()
{
}

LocalMediaStream* LocalMediaStream::toLocalMediaStream()
{
    return this;
}

void LocalMediaStream::detachEmbedder()
{
    // Assuming we should stop any live streams when losing access to the embedder.
    stop();

    MediaStream::detachEmbedder();
}

void LocalMediaStream::streamEnded()
{
    m_audioTracks->clear();
    m_videoTracks->clear();

    MediaStream::streamEnded();
}

PassRefPtr<MultipleTrackList> LocalMediaStream::audioTracks() const
{
    return m_audioTracks;
}

PassRefPtr<ExclusiveTrackList> LocalMediaStream::videoTracks() const
{
    return m_videoTracks;
}

void LocalMediaStream::stop()
{
    if (!mediaStreamFrameController() || m_readyState == ENDED)
        return;

    mediaStreamFrameController()->stopGeneratedStream(label());
    m_audioTracks->clear();
    m_videoTracks->clear();

    m_readyState = ENDED;

    // Don't assert since it can be null in degenerate cases like frames detached from their pages.
    if (!scriptExecutionContext())
        return;

    ASSERT(scriptExecutionContext()->isContextThread());
    scriptExecutionContext()->postTask(DispatchUpdateTask::create(this, &LocalMediaStream::onStop));
}

void LocalMediaStream::onStop()
{
    dispatchEvent(Event::create(eventNames().endedEvent, false, false));
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
