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

#if USE(GSTREAMER_WEBRTC) && USE(LIBRICE)

#include "GStreamerIceAgent.h"

#include <glib-object.h>

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>
#undef GST_USE_UNSTABLE_API

#include <wtf/Forward.h>
#include <wtf/glib/GThreadSafeWeakPtr.h>

typedef struct _WebKitGstIceStream WebKitGstIceStream;
typedef struct _WebKitGstIceStreamClass WebKitGstIceStreamClass;

typedef struct _WebKitGstIceTransport WebKitGstIceTransport;
typedef struct _WebKitGstIceTransportClass WebKitGstIceTransportClass;

#define WEBKIT_TYPE_GST_WEBRTC_ICE_TRANSPORT (webkit_gst_webrtc_ice_transport_get_type())
#define WEBKIT_GST_WEBRTC_ICE_TRANSPORT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_GST_WEBRTC_ICE_TRANSPORT, WebKitGstIceTransport))
#define WEBKIT_GST_WEBRTC_ICE_TRANSPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_GST_WEBRTC_ICE_TRANSPORT, WebKitGstIceTransportClass))
#define WEBKIT_IS_GST_WEBRTC_ICE_TRANSPORT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_GST_WEBRTC_ICE_TRANSPORT))
#define WEBKIT_IS_GST_WEBRTC_ICE_TRANSPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_GST_WEBRTC_ICE_TRANSPORT))

GType webkit_gst_webrtc_ice_transport_get_type();

WebKitGstIceTransport* webkitGstWebRTCCreateIceTransport(WebKitGstIceAgent*, GThreadSafeWeakPtr<WebKitGstIceStream>&&, GstWebRTCICEComponent, bool);
void webkitGstWebRTCIceTransportHandleIncomingData(WebKitGstIceTransport*, GRefPtr<GstBuffer>&&);

void webkitGstWebRTCIceTransportNewSelectedPair(WebKitGstIceTransport*, RiceAgentSelectedPair&);
bool webkitGstWebRTCIceTransportGetSelectedPair(WebKitGstIceTransport*, GstWebRTCICECandidateStats**, GstWebRTCICECandidateStats**);

#endif // USE(GSTREAMER_WEBRTC) && USE(LIBRICE)
