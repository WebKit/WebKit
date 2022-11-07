/*
 *  Copyright (C) 2019-2022 Igalia S.L. All rights reserved.
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

#pragma once

#if ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)

#include "GRefPtrGStreamer.h"

#include <wtf/CompletionHandler.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class RTCStatsReport;

class GStreamerStatsCollector : public ThreadSafeRefCounted<GStreamerStatsCollector, WTF::DestructionThread::Main> {
public:
    using CollectorCallback = CompletionHandler<void(RefPtr<RTCStatsReport>&&)>;
    static Ref<GStreamerStatsCollector> create() { return adoptRef(*new GStreamerStatsCollector()); }

    void setElement(GstElement* element) { m_webrtcBin = element; }
    void getStats(CollectorCallback&&, GstPad*, const GstStructure*);

private:
    GRefPtr<GstElement> m_webrtcBin;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
