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

#include "GStreamerCommon.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebCore {
class MediaPlayerPrivateGStreamer;

class WebKitVideoSinkProbeOwner final : public WTF::ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<WebKitVideoSinkProbeOwner> {
    WTF_MAKE_TZONE_ALLOCATED(WebKitVideoSinkProbeOwner);

public:
    static RefPtr<WebKitVideoSinkProbeOwner> create(const ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer>& player)
    {
        return adoptRef(*new WebKitVideoSinkProbeOwner(player));
    }

    void handleFlushEvent([[maybe_unused]] const GRefPtr<GstPad>&, GstPadProbeInfo*);

    GstPadProbeReturn doProbe([[maybe_unused]] const GRefPtr<GstPad>&, GstPadProbeInfo*);

private:
    WebKitVideoSinkProbeOwner(const ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer>& player)
        : m_player(player)
    {
    }

    ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer> m_player;
    bool m_isFlushing { false };
};

struct WebKitVideoSinkSignalIdentifiers {
    unsigned long newSample { 0 };
    unsigned long newPreroll { 0 };
    unsigned long notifyCaps { 0 };
    RefPtr<WebKitVideoSinkProbeOwner> padProbeOwner;
    RefPtr<PadProbeHandle<WebKitVideoSinkProbeOwner>> padProbeHandle;
};

WebKitVideoSinkSignalIdentifiers webKitVideoSinkSetMediaPlayerPrivate(GstElement*, const ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer>&);

void webKitVideoSinkDisconnectSignalHandlers(GstElement*, const WebKitVideoSinkSignalIdentifiers&);

} // namespace WebCore

#endif // ENABLE(VIDEO)
