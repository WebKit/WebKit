/*
 * Copyright (C) 2023 Igalia S.L
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
#include "PlatformRawAudioData.h"

#include "AudioSampleFormat.h"
#include "NotImplemented.h"
#include "WebCodecsAudioDataAlgorithms.h"
#include <wtf/RefPtr.h>

#if ENABLE(WEB_CODECS)

namespace WebCore {

#if !USE(GSTREAMER) && !USE(AVFOUNDATION)
RefPtr<PlatformRawAudioData> PlatformRawAudioData::create(std::span<const uint8_t>, AudioSampleFormat, float, int64_t, size_t, size_t)
{
    notImplemented();
    return nullptr;
}

void PlatformRawAudioData::copyTo(std::span<uint8_t>, AudioSampleFormat, size_t, std::optional<size_t>, std::optional<size_t>, unsigned long)
{
    notImplemented();
}
#endif

void PlatformRawAudioData::copyToInterleaved(PlaneData source, std::span<uint8_t> destination, AudioSampleFormat format, unsigned long copyElementCount)
{
    ASSERT(!(copyElementCount % numberOfChannels()));

    auto copyElements = [numberOfChannels = numberOfChannels()]<typename T>(std::span<T> destination, const auto& source, size_t frames)
    {
        RELEASE_ASSERT(destination.size() >= frames * numberOfChannels);
        RELEASE_ASSERT(source[0].size() >= frames); // All planes have the exact same size.
        size_t index = 0;
        for (size_t frame = 0; frame < frames; frame++) {
            for (size_t channel = 0; channel < source.size(); channel++)
                destination[index++] = convertAudioSample<T>(source[channel][frame]);
        }
    };

    size_t numberOfFrames = copyElementCount / numberOfChannels();
    WTF::switchOn(audioElementSpan(format, destination), [&source, &numberOfFrames, &copyElements](auto dst) {
        switchOn(source, [&](auto& src) {
            copyElements(dst, src, numberOfFrames);
        });
    });
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
