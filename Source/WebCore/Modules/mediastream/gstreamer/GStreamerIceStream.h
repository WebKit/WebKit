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
#include "GStreamerIceTransport.h"
#include "RTCIceComponent.h"
#include "RTCIceConnectionState.h"
#include "RTCIceProtocol.h"
#include <glib-object.h>
#include <wtf/Forward.h>

typedef struct _WebKitGstIceStream WebKitGstIceStream;
typedef struct _WebKitGstIceStreamClass WebKitGstIceStreamClass;

#define WEBKIT_TYPE_GST_WEBRTC_ICE_STREAM (webkit_gst_webrtc_ice_stream_get_type())
#define WEBKIT_GST_WEBRTC_ICE_STREAM(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_GST_WEBRTC_ICE_STREAM, WebKitGstIceStream))
#define WEBKIT_GST_WEBRTC_ICE_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_GST_WEBRTC_ICE_STREAM, WebKitGstIceStreamClass))
#define WEBKIT_IS_GST_WEBRTC_ICE_STREAM(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_GST_WEBRTC_ICE_STREAM))
#define WEBKIT_IS_GST_WEBRTC_ICE_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_GST_WEBRTC_ICE_STREAM))

GType webkit_gst_webrtc_ice_stream_get_type();

WebKitGstIceStream* webkitGstWebRTCCreateIceStream(WebKitGstIceAgent*, GRefPtr<RiceStream>&&);

GstWebRTCICETransport* webkitGstWebRTCIceStreamFindTransport(GstWebRTCICEStream*, GstWebRTCICEComponent);

void webkitGstWebRTCIceStreamHandleIncomingData(const WebKitGstIceStream*, WebCore::RTCIceProtocol, String&&, String&&, WebCore::SharedMemory::Handle&&);

const GRefPtr<RiceStream>& webkitGstWebRTCIceStreamGetRiceStream(WebKitGstIceStream*);
void webkitGstWebRTCIceStreamSetLocalCredentials(WebKitGstIceStream*, const String& ufrag, const String& pwd);
void webkitGstWebRTCIceStreamSetRemoteCredentials(WebKitGstIceStream*, const String& ufrag, const String& pwd);
bool webkitGstWebRTCIceStreamGatherCandidates(WebKitGstIceStream*);
void webkitGstWebRTCIceStreamGatheringDone(const WebKitGstIceStream*);

void webkitGstWebRTCIceStreamAddLocalGatheredCandidate(const WebKitGstIceStream*, RiceGatheredCandidate&);
void webkitGstWebRTCIceStreamNewSelectedPair(const WebKitGstIceStream*, RiceAgentSelectedPair&);
void webkitGstWebRTCIceStreamComponentStateChanged(const WebKitGstIceStream*, RiceAgentComponentStateChange&);
bool webkitGstWebRTCIceStreamGetSelectedPair(WebKitGstIceStream*, GstWebRTCICECandidateStats**, GstWebRTCICECandidateStats**);

#endif // USE(GSTREAMER_WEBRTC) && USE(LIBRICE)
