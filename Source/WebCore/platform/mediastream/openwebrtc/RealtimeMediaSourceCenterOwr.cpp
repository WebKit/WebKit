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

#include "CaptureDevice.h"
#include "MediaStreamPrivate.h"
#include "NotImplemented.h"
#include "OpenWebRTCUtilities.h"
#include "RealtimeAudioSourceOwr.h"
#include "RealtimeMediaSourceCapabilities.h"
#include "RealtimeVideoSourceOwr.h"
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

void RealtimeMediaSourceCenterOwr::validateRequestConstraints(ValidConstraintsHandler validHandler, InvalidConstraintsHandler invalidHandler, const MediaConstraints& audioConstraints, const MediaConstraints& videoConstraints)
{
    m_validConstraintsHandler = WTFMove(validHandler);
    m_invalidConstraintsHandler = WTFMove(invalidHandler);

    Vector<String> audioSources;
    Vector<String> videoSources;

    // FIXME: Actually do constraints validation. The MediaConstraints
    // need to comply with the available audio/video device(s)
    // capabilities. See bug #123345.
    if (audioConstraints.isValid())
        audioSources.append(String("audio"));

    if (videoConstraints.isValid())
        videoSources.append(String("video"));

    m_validConstraintsHandler(WTFMove(audioSources), WTFMove(videoSources));
    m_validConstraintsHandler = nullptr;
    m_invalidConstraintsHandler = nullptr;
}

void RealtimeMediaSourceCenterOwr::createMediaStream(NewMediaStreamHandler completionHandler, const String& audioDeviceID, const String& videoDeviceID, const MediaConstraints*, const MediaConstraints*)
{
    int types = OWR_MEDIA_TYPE_UNKNOWN;

    if (!audioDeviceID.isEmpty())
        types |= OWR_MEDIA_TYPE_AUDIO;
    if (!videoDeviceID.isEmpty())
        types |= OWR_MEDIA_TYPE_VIDEO;

    m_completionHandler = completionHandler;

    owr_get_capture_sources(static_cast<OwrMediaType>(types), mediaSourcesAvailableCallback, this);
}

Vector<CaptureDevice> RealtimeMediaSourceCenterOwr::getMediaStreamDevices()
{
    notImplemented();
    return Vector<CaptureDevice>();
}

void RealtimeMediaSourceCenterOwr::mediaSourcesAvailable(GList* sources)
{
    Vector<Ref<RealtimeMediaSource>> audioSources;
    Vector<Ref<RealtimeMediaSource>> videoSources;

    for (auto item = sources; item; item = item->next) {
        OwrMediaSource* source = OWR_MEDIA_SOURCE(item->data);

        GUniqueOutPtr<gchar> name;
        OwrMediaType mediaType;
        g_object_get(source, "media-type", &mediaType, "name", &name.outPtr(), NULL);
        String sourceName(name.get());
        String id(createCanonicalUUIDString());

        if (g_getenv("OWR_USE_TEST_SOURCES")) {
            OwrSourceType sourceType = OWR_SOURCE_TYPE_UNKNOWN;
            g_object_get(source, "type", &sourceType, NULL);
            if (sourceType != OWR_SOURCE_TYPE_TEST)
                continue;
        }

        RealtimeMediaSource::Type mediaSourceType;
        if (mediaType & OWR_MEDIA_TYPE_AUDIO)
            mediaSourceType = RealtimeMediaSource::Audio;
        else if (mediaType & OWR_MEDIA_TYPE_VIDEO)
            mediaSourceType = RealtimeMediaSource::Video;
        else {
            mediaSourceType = RealtimeMediaSource::None;
            ASSERT_NOT_REACHED();
        }

        RefPtr<RealtimeMediaSourceOwr> mediaSource;
        if (mediaSourceType == RealtimeMediaSource::Audio)
            mediaSource = adoptRef(new RealtimeAudioSourceOwr(source, id, mediaSourceType, sourceName));
        else
            mediaSource = adoptRef(new RealtimeVideoSourceOwr(source, id, mediaSourceType, sourceName));

        RealtimeMediaSourceOwrMap::iterator sourceIterator = m_sourceMap.find(id);
        if (sourceIterator == m_sourceMap.end())
            m_sourceMap.add(id, mediaSource.copyRef());

        if (mediaType & OWR_MEDIA_TYPE_AUDIO)
            audioSources.append(mediaSource.releaseNonNull());
        else if (mediaType & OWR_MEDIA_TYPE_VIDEO)
            videoSources.append(mediaSource.releaseNonNull());

    }

    if (videoSources.isEmpty() && audioSources.isEmpty())
        m_completionHandler(nullptr);
    else
        m_completionHandler(MediaStreamPrivate::create(audioSources, videoSources));

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
