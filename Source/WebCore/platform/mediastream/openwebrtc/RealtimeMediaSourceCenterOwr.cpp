/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2015 Igalia S.L. All rights reserved.
 * Copyright (C) 2015 Metrological. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM) && USE(OPENWEBRTC)
#include "RealtimeMediaSourceCenterOwr.h"

#include "MediaStreamCreationClient.h"
#include "MediaStreamPrivate.h"
#include "MediaStreamTrackSourcesRequestClient.h"
#include "NotImplemented.h"
#include "OpenWebRTCUtilities.h"
#include "RealtimeMediaSource.h"
#include "RealtimeMediaSourceCapabilities.h"
#include "UUID.h"
#include <owr/owr.h>
#include <owr/owr_local.h>
#include <owr/owr_media_source.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>


namespace WebCore {

RealtimeMediaSourceCenter& RealtimeMediaSourceCenter::platformCenter()
{
    ASSERT(isMainThread());
    static NeverDestroyed<RealtimeMediaSourceCenterOwr> center;
    return center;
}

static void mediaSourcesAvailableCallback(GList* sources, gpointer userData)
{
    RealtimeMediaSourceCenterOwr* center = reinterpret_cast<RealtimeMediaSourceCenterOwr*>(userData);
    center->mediaSourcesAvailable(sources);
}

RealtimeMediaSourceCenterOwr::RealtimeMediaSourceCenterOwr()
{
    initializeOpenWebRTC();
}

RealtimeMediaSourceCenterOwr::~RealtimeMediaSourceCenterOwr()
{
}

void RealtimeMediaSourceCenterOwr::validateRequestConstraints(PassRefPtr<MediaStreamCreationClient> prpClient, PassRefPtr<MediaConstraints> prpAudioConstraints, PassRefPtr<MediaConstraints> prpVideoConstraints)
{
    m_client = prpClient;
    ASSERT(m_client);

    // FIXME: Actually do constraints validation. The MediaConstraints
    // need to comply with the available audio/video device(s)
    // capabilities. See bug #123345.
    RefPtr<MediaConstraints> audioConstraints = prpAudioConstraints;
    RefPtr<MediaConstraints> videoConstraints = prpVideoConstraints;

    int types = OWR_MEDIA_TYPE_UNKNOWN;
    if (audioConstraints)
        types |= OWR_MEDIA_TYPE_AUDIO;
    if (videoConstraints)
        types |= OWR_MEDIA_TYPE_VIDEO;

    owr_get_capture_sources(static_cast<OwrMediaType>(types), mediaSourcesAvailableCallback, this);
}

void RealtimeMediaSourceCenterOwr::createMediaStream(PassRefPtr<MediaStreamCreationClient> prpQueryClient, PassRefPtr<MediaConstraints> audioConstraints, PassRefPtr<MediaConstraints> videoConstraints)
{
    RefPtr<MediaStreamCreationClient> client = prpQueryClient;
    ASSERT(client);

    UNUSED_PARAM(audioConstraints);
    UNUSED_PARAM(videoConstraints);

    Vector<RefPtr<RealtimeMediaSource>> audioSources;
    Vector<RefPtr<RealtimeMediaSource>> videoSources;

    if (audioConstraints) {
        // TODO: verify constraints according to registered
        // sources. For now, unconditionally pick the first source, see bug #123345.
        RefPtr<RealtimeMediaSource> audioSource = firstSource(RealtimeMediaSource::Audio);
        if (audioSource) {
            audioSource->reset();
            audioSources.append(audioSource.release());
        }
    }

    if (videoConstraints) {
        // TODO: verify constraints according to registered
        // sources. For now, unconditionally pick the first source, see bug #123345.
        RefPtr<RealtimeMediaSource> videoSource = firstSource(RealtimeMediaSource::Video);
        if (videoSource) {
            videoSource->reset();
            videoSources.append(videoSource.release());
        }
    }

    client->didCreateStream(MediaStreamPrivate::create(audioSources, videoSources));
}

bool RealtimeMediaSourceCenterOwr::getMediaStreamTrackSources(PassRefPtr<MediaStreamTrackSourcesRequestClient>)
{
    notImplemented();
    return false;
}

void RealtimeMediaSourceCenterOwr::mediaSourcesAvailable(GList* sources)
{
    for (auto item = sources; item; item = item->next) {
        OwrMediaSource* source = OWR_MEDIA_SOURCE(item->data);

        GUniqueOutPtr<gchar> name;
        OwrMediaType mediaType;
        g_object_get(source, "media-type", &mediaType, "name", &name.outPtr(), NULL);
        String sourceName(name.get());
        String id(createCanonicalUUIDString());

        RealtimeMediaSource::Type sourceType;
        if (mediaType & OWR_MEDIA_TYPE_AUDIO)
            sourceType = RealtimeMediaSource::Audio;
        else if (mediaType & OWR_MEDIA_TYPE_VIDEO)
            sourceType = RealtimeMediaSource::Video;
        else {
            sourceType = RealtimeMediaSource::None;
            ASSERT_NOT_REACHED();
        }

        RefPtr<RealtimeMediaSourceOwr> mediaSource = adoptRef(new RealtimeMediaSourceOwr(source, id, sourceType, sourceName));

        RealtimeMediaSourceOwrMap::iterator sourceIterator = m_sourceMap.find(id);
        if (sourceIterator == m_sourceMap.end())
            m_sourceMap.add(id, mediaSource);

    }

    // TODO: Make sure contraints are actually validated by checking source types.
    m_client->constraintsValidated();
}

PassRefPtr<RealtimeMediaSource> RealtimeMediaSourceCenterOwr::firstSource(RealtimeMediaSource::Type type)
{
    for (auto iter = m_sourceMap.begin(); iter != m_sourceMap.end(); ++iter) {
        RefPtr<RealtimeMediaSource> source = iter->value;
        if (source->type() == type)
            return source;
    }

    return nullptr;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(OPENWEBRTC)
