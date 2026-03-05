/*
 *  Copyright (C) 2025 Igalia S.L. All rights reserved.
 *  Copyright (C) 2025 Metrological Group B.V.
 *  Copyright (C) 2024 Matthew Waters <matthew@centricular.com>
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
#include <wtf/glib/GRefPtr.h>

typedef struct _AgentSource AgentSource;

GRefPtr<GSource> agentSourceNew(GThreadSafeWeakPtr<WebKitGstIceAgent>&&);

#endif // USE(GSTREAMER_WEBRTC) && USE(LIBRICE)
