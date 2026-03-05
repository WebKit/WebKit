/*
 * Copyright (C) 2020 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if USE(GSTREAMER)

#include "GRefPtrGStreamer.h"
#include <wtf/DataMutex.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>
#include <wtf/text/CStringView.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class GStreamerAudioMixer {
    friend NeverDestroyed<GStreamerAudioMixer>;
public:
    static bool isAvailable();
    static GStreamerAudioMixer& singleton();

    void ensureState(GstStateChange, const String& deviceId = { });
    GRefPtr<GstPad> registerProducer(GstElement*, std::optional<int> forcedSampleRate, const String& deviceId = { }, const GRefPtr<GstDevice>& = nullptr);
    void unregisterProducer(const GRefPtr<GstPad>&);

    void configureSourcePeriodTime(CStringView sourceName, uint64_t periodTime);

private:
    GStreamerAudioMixer();

    struct MixerPipeline {
        GRefPtr<GstElement> pipeline;
        GRefPtr<GstElement> mixer;
        RunLoop::Timer teardownTimer;
    };

    struct StreamingMembers {
        HashMap<String, std::unique_ptr<MixerPipeline>> m_pipelines;
        HashMap<GstPad*, String> m_padToDeviceId; // Reverse lookup: pad → deviceId.
    };

    MixerPipeline& ensureMixerPipeline(DataMutexLocker<StreamingMembers>&, const String& deviceId, const GRefPtr<GstDevice>&);
    void teardownPipeline(const String& deviceId);

    static constexpr Seconds s_teardownTimeout = 60_s;
    DataMutex<StreamingMembers> m_streamingMembers;
};

} // namespace WebCore

#endif // USE(GSTREAMER)
