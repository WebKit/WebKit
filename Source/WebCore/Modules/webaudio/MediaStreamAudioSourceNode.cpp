/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#if ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)

#include "MediaStreamAudioSourceNode.h"

#include "AudioContext.h"
#include "AudioNodeOutput.h"
#include "AudioSourceProvider.h"
#include "AudioUtilities.h"
#include "Logging.h"
#include "MediaStreamAudioSourceOptions.h"
#include "WebAudioSourceProvider.h"
#include <wtf/Locker.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(MediaStreamAudioSourceNode);

ExceptionOr<Ref<MediaStreamAudioSourceNode>> MediaStreamAudioSourceNode::create(BaseAudioContext& context, MediaStreamAudioSourceOptions&& options)
{
    RELEASE_ASSERT(options.mediaStream);

    auto audioTracks = options.mediaStream->getAudioTracks();
    if (audioTracks.isEmpty())
        return Exception { ExceptionCode::InvalidStateError, "Media stream has no audio tracks"_s };

    RefPtr<WebAudioSourceProvider> provider;
    for (auto& track : audioTracks) {
        provider = track->createAudioSourceProvider();
        if (provider)
            break;
    }
    if (!provider)
        return Exception { ExceptionCode::InvalidStateError, "Could not find an audio track with an audio source provider"_s };

    auto node = adoptRef(*new MediaStreamAudioSourceNode(context, *options.mediaStream, provider.releaseNonNull()));
    node->setFormat(2, context.sampleRate());

    // Context keeps reference until node is disconnected.
    context.sourceNodeWillBeginPlayback(node);

    return node;
}

MediaStreamAudioSourceNode::MediaStreamAudioSourceNode(BaseAudioContext& context, MediaStream& mediaStream, Ref<WebAudioSourceProvider>&& provider)
    : AudioNode(context, NodeTypeMediaStreamAudioSource)
    , m_mediaStream(mediaStream)
    , m_provider(provider)
{
    m_provider->setClient(this);
    
    // Default to stereo. This could change depending on the format of the MediaStream's audio track.
    addOutput(2);

    initialize();
}

MediaStreamAudioSourceNode::~MediaStreamAudioSourceNode()
{
    m_provider->setClient(nullptr);
    uninitialize();
}

void MediaStreamAudioSourceNode::setFormat(size_t numberOfChannels, float sourceSampleRate)
{
    // Synchronize with process().
    Locker locker { m_processLock };

    if (numberOfChannels == m_sourceNumberOfChannels && sourceSampleRate == m_sourceSampleRate)
        return;

    // The sample-rate must be equal to the context's sample-rate.
    if (!numberOfChannels || numberOfChannels > AudioContext::maxNumberOfChannels) {
        // process() will generate silence for these uninitialized values.
        LOG(Media, "MediaStreamAudioSourceNode::setFormat(%u, %f) - unhandled format change", static_cast<unsigned>(numberOfChannels), sourceSampleRate);
        m_sourceNumberOfChannels = 0;
        return;
    }

    m_sourceNumberOfChannels = numberOfChannels;
    m_sourceSampleRate = sourceSampleRate;

    float sampleRate = this->sampleRate();
    if (sourceSampleRate == sampleRate)
        m_multiChannelResampler = nullptr;
    else {
        double scaleFactor = sourceSampleRate / sampleRate;
        m_multiChannelResampler = makeUnique<MultiChannelResampler>(scaleFactor, numberOfChannels, AudioUtilities::renderQuantumSize, std::bind(&MediaStreamAudioSourceNode::provideInput, this, std::placeholders::_1, std::placeholders::_2));
    }

    m_sourceNumberOfChannels = numberOfChannels;

    {
        // The context must be locked when changing the number of output channels.
        Locker contextLocker { context().graphLock() };

        // Do any necesssary re-configuration to the output's number of channels.
        checkedOutput(0)->setNumberOfChannels(numberOfChannels);
    }
}

void MediaStreamAudioSourceNode::provideInput(AudioBus& bus, size_t framesToProcess)
{
    m_provider->provideInput(bus, framesToProcess);
}

void MediaStreamAudioSourceNode::process(size_t numberOfFrames)
{
    Ref outputBus = checkedOutput(0)->bus();

    // Use tryLock() to avoid contention in the real-time audio thread.
    // If we fail to acquire the lock then the MediaStream must be in the middle of
    // a format change, so we output silence in this case.
    if (!m_processLock.tryLock()) {
        // We failed to acquire the lock.
        outputBus->zero();
        return;
    }
    Locker locker { AdoptLock, m_processLock };

    if (!m_sourceNumberOfChannels || !m_sourceSampleRate || m_sourceNumberOfChannels != outputBus->numberOfChannels()) {
        outputBus->zero();
        return;
    }

    if (numberOfFrames > outputBus->length())
        numberOfFrames = outputBus->length();

    if (m_multiChannelResampler) {
        ASSERT(m_sourceSampleRate != sampleRate());
        m_multiChannelResampler->process(outputBus.get(), numberOfFrames);
    } else {
        // Bypass the resampler completely if the source is at the context's sample-rate.
        ASSERT(m_sourceSampleRate == sampleRate());
        provideInput(outputBus, numberOfFrames);
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)
