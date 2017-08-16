/* GStreamer EME Utilities class
 *
 * Copyright (C) 2017 Metrological
 * Copyright (C) 2017 Igalia S.L
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GStreamerEMEUtilities.h"

#if ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)

namespace WebCore {

const char* GStreamerEMEUtilities::s_ClearKeyUUID = WEBCORE_GSTREAMER_EME_UTILITIES_CLEARKEY_UUID;
const char* GStreamerEMEUtilities::s_ClearKeyKeySystem = "org.w3.clearkey";

}

#endif // ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)
