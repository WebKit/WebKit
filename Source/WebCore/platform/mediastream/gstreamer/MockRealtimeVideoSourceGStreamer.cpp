/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro <alex@igalia.com>
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
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
#include "MockRealtimeVideoSourceGStreamer.h"

#include "MediaSampleGStreamer.h"
#include "MockRealtimeMediaSourceCenter.h"

namespace WebCore {

CaptureSourceOrError MockRealtimeVideoSource::create(String&& deviceID, String&& name, String&& hashSalt, const MediaConstraints* constraints)
{
#ifndef NDEBUG
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(deviceID);
    ASSERT(device);
    if (!device)
        return { "No mock camera device"_s };
#endif

    auto source = adoptRef(*new MockRealtimeVideoSourceGStreamer(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt)));
    if (constraints) {
        if (auto error = source->applyConstraints(*constraints))
            return WTFMove(error->message);
    }

    return CaptureSourceOrError(RealtimeVideoSource::create(WTFMove(source)));
}

Ref<MockRealtimeVideoSource> MockRealtimeVideoSourceGStreamer::createForMockDisplayCapturer(String&& deviceID, String&& name, String&& hashSalt)
{
    return adoptRef(*new MockRealtimeVideoSourceGStreamer(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt)));
}

MockRealtimeVideoSourceGStreamer::MockRealtimeVideoSourceGStreamer(String&& deviceID, String&& name, String&& hashSalt)
    : MockRealtimeVideoSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt))
{
    ensureGStreamerInitialized();
}

void MockRealtimeVideoSourceGStreamer::updateSampleBuffer()
{
    auto imageBuffer = this->imageBuffer();
    if (!imageBuffer)
        return;

    auto sample = MediaSampleGStreamer::createImageSample(imageBuffer->toBGRAData(), captureSize(), size(), frameRate());
    sample->offsetTimestampsBy(MediaTime::createWithDouble((elapsedTime() + 100_ms).seconds()));
    dispatchMediaSampleToObservers(sample.get());
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
