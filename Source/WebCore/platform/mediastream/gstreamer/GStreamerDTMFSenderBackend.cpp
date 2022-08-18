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

#include "config.h"
#include "GStreamerDTMFSenderBackend.h"

#if ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)

#include "NotImplemented.h"
#include <wtf/MainThread.h>

namespace WebCore {

GStreamerDTMFSenderBackend::GStreamerDTMFSenderBackend()
{
    notImplemented();
}

GStreamerDTMFSenderBackend::~GStreamerDTMFSenderBackend()
{
    notImplemented();
}

bool GStreamerDTMFSenderBackend::canInsertDTMF()
{
    notImplemented();
    return false;
}

void GStreamerDTMFSenderBackend::playTone(const char, size_t, size_t)
{
    notImplemented();
}

String GStreamerDTMFSenderBackend::tones() const
{
    notImplemented();
    return { };
}

size_t GStreamerDTMFSenderBackend::duration() const
{
    notImplemented();
    return 0;
}

size_t GStreamerDTMFSenderBackend::interToneGap() const
{
    notImplemented();
    return 0;
}

void GStreamerDTMFSenderBackend::onTonePlayed(Function<void()>&& onTonePlayed)
{
    m_onTonePlayed = WTFMove(onTonePlayed);
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
