/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 * Copyright (C) 2020, Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO) && ENABLE(VIDEO)

#include "MediaElementAudioSourceNode.h"

#include "AudioContext.h"
#include "AudioNodeOutput.h"
#include "AudioSourceProvider.h"
#include "AudioUtilities.h"
#include "Logging.h"
#include "MediaElementAudioSourceOptions.h"
#include "MediaPlayer.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Locker.h>

// These are somewhat arbitrary limits, but we need to do some kind of sanity-checking.
constexpr unsigned minSampleRate = 8000;
constexpr unsigned maxSampleRate = 192000;

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaElementAudioSourceNode);

ExceptionOr<Ref<MediaElementAudioSourceNode>> MediaElementAudioSourceNode::create(BaseAudioContext& context, MediaElementAudioSourceOptions&& options)
{
    RELEASE_ASSERT(options.mediaElement);

    if (options.mediaElement->audioSourceNode())
        return Exception { InvalidStateError, "Media element is already associated with an audio source node"_s };

    auto node = adoptRef(*new MediaElementAudioSourceNode(context, *options.mediaElement));

    options.mediaElement->setAudioSourceNode(node.ptr());

    // context keeps reference until node is disconnected.
    context.sourceNodeWillBeginPlayback(node);

    return node;
}

MediaElementAudioSourceNode::MediaElementAudioSourceNode(BaseAudioContext& context, Ref<HTMLMediaElement>&& mediaElement)
    : AudioNode(context, NodeTypeMediaElementAudioSource)
    , m_mediaElement(WTFMove(mediaElement))
{
    // Default to stereo. This could change depending on what the media element .src is set to.
    addOutput(2);

    initialize();
}

MediaElementAudioSourceNode::~MediaElementAudioSourceNode()
{
    m_mediaElement->setAudioSourceNode(nullptr);
    uninitialize();
}

void MediaElementAudioSourceNode::setFormat(size_t numberOfChannels, float sourceSampleRate)
{
    auto protectedThis = makeRef(*this);

    // Synchronize with process().
    Locker locker { m_processLock };

    m_muted = wouldTaintOrigin();

    if (numberOfChannels != m_sourceNumberOfChannels || sourceSampleRate != m_sourceSampleRate) {
        if (!numberOfChannels || numberOfChannels > AudioContext::maxNumberOfChannels || sourceSampleRate < minSampleRate || sourceSampleRate > maxSampleRate) {
            // process() will generate silence for these uninitialized values.
            LOG(Media, "MediaElementAudioSourceNode::setFormat(%u, %f) - unhandled format change", static_cast<unsigned>(numberOfChannels), sourceSampleRate);
            m_sourceNumberOfChannels = 0;
            m_sourceSampleRate = 0;
            return;
        }

        m_sourceNumberOfChannels = numberOfChannels;
        m_sourceSampleRate = sourceSampleRate;

        if (sourceSampleRate != sampleRate()) {
            double scaleFactor = sourceSampleRate / sampleRate();
            m_multiChannelResampler = makeUnique<MultiChannelResampler>(scaleFactor, numberOfChannels, AudioUtilities::renderQuantumSize, std::bind(&MediaElementAudioSourceNode::provideInput, this, std::placeholders::_1, std::placeholders::_2));
        } else {
            // Bypass resampling.
            m_multiChannelResampler = nullptr;
        }

        {
            // The context must be locked when changing the number of output channels.
            Locker contextLocker { context().graphLock() };

            // Do any necesssary re-configuration to the output's number of channels.
            output(0)->setNumberOfChannels(numberOfChannels);
        }
    }
}

void MediaElementAudioSourceNode::provideInput(AudioBus* bus, size_t framesToProcess)
{
    ASSERT(bus);
    if (auto* provider = mediaElement().audioSourceProvider())
        provider->provideInput(bus, framesToProcess);
    else {
        // Either this port doesn't yet support HTMLMediaElement audio stream access,
        // or the stream is not yet available.
        bus->zero();
    }
}

bool MediaElementAudioSourceNode::wouldTaintOrigin()
{
    // If the resource is redirected to another origin, treat it as tainted if the crossorigin attribute
    // is not set. This is done for consistency with Blink.
    if (!m_mediaElement->hasSingleSecurityOrigin() && m_mediaElement->crossOrigin().isNull())
        return true;

    if (m_mediaElement->didPassCORSAccessCheck())
        return false;

    if (auto* origin = context().origin())
        return m_mediaElement->wouldTaintOrigin(*origin);

    return true;
}

void MediaElementAudioSourceNode::process(size_t numberOfFrames)
{
    AudioBus* outputBus = output(0)->bus();

    // Use tryLock() to avoid contention in the real-time audio thread.
    // If we fail to acquire the lock then the HTMLMediaElement must be in the middle of
    // reconfiguring its playback engine, so we output silence in this case.
    if (!m_processLock.tryLock()) {
        // We failed to acquire the lock.
        outputBus->zero();
        return;
    }

    Locker locker { AdoptLock, m_processLock };

    if (m_muted || !m_sourceNumberOfChannels || !m_sourceSampleRate || m_sourceNumberOfChannels != outputBus->numberOfChannels()) {
        outputBus->zero();
        return;
    }

    if (m_multiChannelResampler) {
        ASSERT(m_sourceSampleRate != sampleRate());
        m_multiChannelResampler->process(outputBus, numberOfFrames);
    } else {
        // Bypass the resampler completely if the source is at the context's sample-rate.
        ASSERT(m_sourceSampleRate == sampleRate());
        provideInput(outputBus, numberOfFrames);
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
