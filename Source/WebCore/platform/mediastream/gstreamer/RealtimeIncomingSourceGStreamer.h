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

#pragma once

#if USE(GSTREAMER_WEBRTC)

#include "GRefPtrGStreamer.h"
#include "RealtimeMediaSource.h"

namespace WebCore {

class RealtimeIncomingSourceGStreamer : public RealtimeMediaSource, public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer> {
public:
    void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer>::ref(); }
    void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer>::deref(); }
    ThreadSafeWeakPtrControlBlock& controlBlock() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RealtimeIncomingSourceGStreamer>::controlBlock(); }

    GstElement* bin() { return m_bin.get(); }

    virtual void configureForInputCaps(const GRefPtr<GstCaps>&) { createParser(); }

    int registerClient(GRefPtr<GstElement>&&);
    void unregisterClient(int);

    void handleUpstreamEvent(GRefPtr<GstEvent>&&, int clientId);
    bool handleUpstreamQuery(GstQuery*, int clientId);

protected:
    RealtimeIncomingSourceGStreamer(const CaptureDevice&);

    void createParser();

    GRefPtr<GstElement> m_valve;
    GRefPtr<GstElement> m_tee;

private:
    // RealtimeMediaSource API
    void startProducingData() final;
    void stopProducingData() final;
    const RealtimeMediaSourceCapabilities& capabilities() final;

    virtual void dispatchSample(GRefPtr<GstSample>&&) { }

    GRefPtr<GstElement> m_bin;
    GQuark m_clientQuark { 0 };
    HashMap<int, GRefPtr<GstElement>> m_clients;
};

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
