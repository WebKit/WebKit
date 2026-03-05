/*
 *  Copyright (C) 2022 Igalia, S.L
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

#if ENABLE(VIDEO)

#include <gst/gst.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebCore {
class MediaPlayerPrivateGStreamer;

struct WebKitVideoSinkSignalIdentifiers {
    unsigned long newSample { 0 };
    unsigned long newPreroll { 0 };
    unsigned long notifyCaps { 0 };
};

} // namespace WebCore

WebCore::WebKitVideoSinkSignalIdentifiers webKitVideoSinkSetMediaPlayerPrivate(GstElement*, const ThreadSafeWeakPtr<WebCore::MediaPlayerPrivateGStreamer>&);

void webKitVideoSinkDisconnectSignalHandlers(GstElement*, const WebCore::WebKitVideoSinkSignalIdentifiers&);

#endif // ENABLE(VIDEO)
