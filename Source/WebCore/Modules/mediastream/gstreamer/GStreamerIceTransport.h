/*
 *  Copyright (C) 2025 Igalia S.L. All rights reserved.
 *  Copyright (C) 2025 Metrological Group B.V.
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

#include "GStreamerIceAgent.h"

#include <glib-object.h>
#include <wtf/Forward.h>
#include <wtf/Identified.h>
#include <wtf/Noncopyable.h>
#include <wtf/ObjectIdentifier.h>

typedef struct _WebKitGstIceTransport WebKitGstIceTransport;
typedef struct _WebKitGstIceTransportClass WebKitGstIceTransportClass;

namespace WebCore {
class ScriptExecutionContext;
class SocketProvider;
class GStreamerIceTransport;
using GStreamerIceTransportIdentifier = AtomicObjectIdentifier<GStreamerIceTransport>;

class GStreamerIceTransport : public Identified<GStreamerIceTransportIdentifier> {
    WTF_MAKE_NONCOPYABLE(GStreamerIceTransport);
public:
    static RefPtr<GStreamerIceTransport> create(SocketProvider&);

    void ref() { refGStreamerIceTransport(); }
    void deref() { derefGStreamerIceTransport(); }

protected:
    GStreamerIceTransport() = default;
    virtual ~GStreamerIceTransport() = default;
    virtual void refGStreamerIceTransport() = 0;
    virtual void derefGStreamerIceTransport() = 0;
};

} // namespace WebCore

#define WEBKIT_TYPE_GST_WEBRTC_ICE_TRANSPORT (webkit_gst_webrtc_ice_transport_get_type())
#define WEBKIT_GST_WEBRTC_ICE_TRANSPORT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_GST_WEBRTC_ICE_TRANSPORT, WebKitGstIceTransport))
#define WEBKIT_GST_WEBRTC_ICE_TRANSPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_GST_WEBRTC_ICE_TRANSPORT, WebKitGstIceTransportClass))
#define WEBKIT_IS_GST_WEBRTC_ICE_TRANSPORT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_GST_WEBRTC_ICE_TRANSPORT))
#define WEBKIT_IS_GST_WEBRTC_ICE_TRANSPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_GST_WEBRTC_ICE_TRANSPORT))

GType webkit_gst_webrtc_ice_transport_get_type();

using ReadDataCallback = WTF::Function<void(unsigned, unsigned, std::span<uint8_t>&&)>;

WebKitGstIceTransport* webkitGstWebRTCCreateIceTransport(WebKitGstIceAgent*, unsigned, GstWebRTCICEComponent, bool);
void webkitGstWebRTCIceTransportHandleIncomingData(WebKitGstIceTransport*, std::span<const uint8_t>&&);

#endif // USE(GSTREAMER_WEBRTC)
