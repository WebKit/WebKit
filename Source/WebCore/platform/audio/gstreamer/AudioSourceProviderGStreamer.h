/*
 *  Copyright (C) 2014 Igalia S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#if ENABLE(WEB_AUDIO) && ENABLE(VIDEO) && USE(GSTREAMER)

#include "AudioSourceProvider.h"
#include "AudioSourceProviderClient.h"
#include "GRefPtrGStreamer.h"
#include "MainThreadNotifier.h"
#include <gst/gst.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>

#if ENABLE(MEDIA_STREAM)
#include "GStreamerAudioStreamDescription.h"
#include "MediaStreamTrackPrivate.h"
#include "WebAudioSourceProvider.h"
#endif

typedef struct _GstAdapter GstAdapter;
typedef struct _GstAppSink GstAppSink;

namespace WebCore {

#if ENABLE(MEDIA_STREAM)
class AudioSourceProviderGStreamer final : public WebAudioSourceProvider {
public:
    static Ref<AudioSourceProviderGStreamer> create(MediaStreamTrackPrivate& source)
    {
        return adoptRef(*new AudioSourceProviderGStreamer(source));
    }
    AudioSourceProviderGStreamer(MediaStreamTrackPrivate&);
#else
class AudioSourceProviderGStreamer : public AudioSourceProvider {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(AudioSourceProviderGStreamer);
public:
#endif

    AudioSourceProviderGStreamer();
    ~AudioSourceProviderGStreamer();

    void configureAudioBin(GstElement* audioBin, GstElement* audioSink);

    void provideInput(AudioBus*, size_t framesToProcess) override;
    void setClient(WeakPtr<AudioSourceProviderClient>&&) override;
    const AudioSourceProviderClient* client() const { return m_client.get(); }

    void handleNewDeinterleavePad(GstPad*);
    void deinterleavePadsConfigured();
    void handleRemovedDeinterleavePad(GstPad*);

    GstFlowReturn handleSample(GstAppSink*, bool isPreroll);
    void clearAdapters();

private:
#if ENABLE(MEDIA_STREAM)
    GRefPtr<GstElement> m_pipeline;
#endif
    enum MainThreadNotification {
        DeinterleavePadsConfigured = 1 << 0,
    };
    Ref<MainThreadNotifier<MainThreadNotification>> m_notifier;
    GRefPtr<GstElement> m_audioSinkBin;
    WeakPtr<AudioSourceProviderClient> m_client;
    int m_deinterleaveSourcePads { 0 };
    HashMap<int, GRefPtr<GstAdapter>> m_adapters WTF_GUARDED_BY_LOCK(m_adapterLock);
    unsigned long m_deinterleavePadAddedHandlerId { 0 };
    unsigned long m_deinterleaveNoMorePadsHandlerId { 0 };
    unsigned long m_deinterleavePadRemovedHandlerId { 0 };
    Lock m_adapterLock;
};

}
#endif // ENABLE(WEB_AUDIO) && ENABLE(VIDEO) && USE(GSTREAMER)
