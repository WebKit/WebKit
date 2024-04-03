/*
 *  Copyright (C) 2017-2022 Igalia S.L. All rights reserved.
 *  Copyright (C) 2022 Metrological Group B.V.
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

#include "config.h"

#if USE(GSTREAMER_WEBRTC)
#include "RealtimeIncomingAudioSourceGStreamer.h"

#include "GStreamerAudioData.h"
#include "GStreamerAudioStreamDescription.h"

GST_DEBUG_CATEGORY(webkit_webrtc_incoming_audio_debug);
#define GST_CAT_DEFAULT webkit_webrtc_incoming_audio_debug

namespace WebCore {

RealtimeIncomingAudioSourceGStreamer::RealtimeIncomingAudioSourceGStreamer(AtomString&& audioTrackId)
    : RealtimeIncomingSourceGStreamer(CaptureDevice { WTFMove(audioTrackId), CaptureDevice::DeviceType::Microphone, emptyString() })
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_incoming_audio_debug, "webkitwebrtcincomingaudio", 0, "WebKit WebRTC incoming audio");
    });
    static Atomic<uint64_t> sourceCounter = 0;
    gst_element_set_name(bin(), makeString("incoming-audio-source-"_s, sourceCounter.exchangeAdd(1)).ascii().data());
    GST_DEBUG_OBJECT(bin(), "New incoming audio source created with ID %s", persistentID().ascii().data());
}

RealtimeIncomingAudioSourceGStreamer::~RealtimeIncomingAudioSourceGStreamer()
{
    stop();
}

const RealtimeMediaSourceSettings& RealtimeIncomingAudioSourceGStreamer::settings()
{
    return m_currentSettings;
}

void RealtimeIncomingAudioSourceGStreamer::dispatchSample(GRefPtr<GstSample>&& sample)
{
    ASSERT(isMainThread());
    auto presentationTime = MediaTime(GST_TIME_AS_USECONDS(GST_BUFFER_PTS(gst_sample_get_buffer(sample.get()))), G_USEC_PER_SEC);
    GStreamerAudioStreamDescription description;
    GStreamerAudioData frames(WTFMove(sample), description.getInfo());
    audioSamplesAvailable(presentationTime, frames, description, 0);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
