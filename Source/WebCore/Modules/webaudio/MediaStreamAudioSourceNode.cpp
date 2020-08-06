/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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
#include "Logging.h"
#include "MediaStreamAudioSourceOptions.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Locker.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaStreamAudioSourceNode);

ExceptionOr<Ref<MediaStreamAudioSourceNode>> MediaStreamAudioSourceNode::create(BaseAudioContext& context, MediaStreamAudioSourceOptions&& options)
{
    RELEASE_ASSERT(options.mediaStream);

    if (context.isStopped())
        return Exception { InvalidStateError };

    auto audioTracks = options.mediaStream->getAudioTracks();
    if (audioTracks.isEmpty())
        return Exception { InvalidStateError, "Media stream has no audio tracks"_s };

    MediaStreamTrack* providerTrack = nullptr;
    for (auto& track : audioTracks) {
        if (track->audioSourceProvider()) {
            providerTrack = track.get();
            break;
        }
    }
    if (!providerTrack)
        return Exception { InvalidStateError, "Could not find an audio track with an audio source provider"_s };

    context.lazyInitialize();

    auto node = adoptRef(*new MediaStreamAudioSourceNode(context, *options.mediaStream, *providerTrack));
    node->setFormat(2, context.sampleRate());

    context.refNode(node); // context keeps reference until node is disconnected

    return node;
}

MediaStreamAudioSourceNode::MediaStreamAudioSourceNode(BaseAudioContext& context, MediaStream& mediaStream, MediaStreamTrack& audioTrack)
    : AudioNode(context, context.sampleRate())
    , m_mediaStream(mediaStream)
    , m_audioTrack(audioTrack)
{
    setNodeType(NodeTypeMediaStreamAudioSource);
    
    AudioSourceProvider* audioSourceProvider = m_audioTrack->audioSourceProvider();
    ASSERT(audioSourceProvider);

    audioSourceProvider->setClient(this);
    
    // Default to stereo. This could change depending on the format of the MediaStream's audio track.
    addOutput(makeUnique<AudioNodeOutput>(this, 2));

    initialize();
}

MediaStreamAudioSourceNode::~MediaStreamAudioSourceNode()
{
    AudioSourceProvider* audioSourceProvider = m_audioTrack->audioSourceProvider();
    ASSERT(audioSourceProvider);
    audioSourceProvider->setClient(nullptr);
    uninitialize();
}

void MediaStreamAudioSourceNode::setFormat(size_t numberOfChannels, float sourceSampleRate)
{
    float sampleRate = this->sampleRate();
    if (numberOfChannels == m_sourceNumberOfChannels && sourceSampleRate == m_sourceSampleRate)
        return;

    // The sample-rate must be equal to the context's sample-rate.
    if (!numberOfChannels || numberOfChannels > AudioContext::maxNumberOfChannels()) {
        // process() will generate silence for these uninitialized values.
        LOG(Media, "MediaStreamAudioSourceNode::setFormat(%u, %f) - unhandled format change", static_cast<unsigned>(numberOfChannels), sourceSampleRate);
        m_sourceNumberOfChannels = 0;
        return;
    }

    // Synchronize with process().
    auto locker = holdLock(m_processMutex);

    m_sourceNumberOfChannels = numberOfChannels;
    m_sourceSampleRate = sourceSampleRate;

    if (sourceSampleRate == sampleRate)
        m_multiChannelResampler = nullptr;
    else {
        double scaleFactor = sourceSampleRate / sampleRate;
        m_multiChannelResampler = makeUnique<MultiChannelResampler>(scaleFactor, numberOfChannels);
    }

    m_sourceNumberOfChannels = numberOfChannels;

    {
        // The context must be locked when changing the number of output channels.
        AudioContext::AutoLocker contextLocker(context());

        // Do any necesssary re-configuration to the output's number of channels.
        output(0)->setNumberOfChannels(numberOfChannels);
    }
}

void MediaStreamAudioSourceNode::process(size_t numberOfFrames)
{
    AudioBus* outputBus = output(0)->bus();
    AudioSourceProvider* provider = m_audioTrack->audioSourceProvider();

    if (!mediaStream() || !m_sourceNumberOfChannels || !m_sourceSampleRate || !provider) {
        outputBus->zero();
        return;
    }

    // Use std::try_to_lock to avoid contention in the real-time audio thread.
    // If we fail to acquire the lock then the MediaStream must be in the middle of
    // a format change, so we output silence in this case.
    std::unique_lock<Lock> lock(m_processMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        // We failed to acquire the lock.
        outputBus->zero();
        return;
    }
    if (m_sourceNumberOfChannels != outputBus->numberOfChannels()) {
        outputBus->zero();
        return;
    }

    if (numberOfFrames > outputBus->length())
        numberOfFrames = outputBus->length();

    if (m_multiChannelResampler.get()) {
        ASSERT(m_sourceSampleRate != sampleRate());
        m_multiChannelResampler->process(provider, outputBus, numberOfFrames);
    } else {
        // Bypass the resampler completely if the source is at the context's sample-rate.
        ASSERT(m_sourceSampleRate == sampleRate());
        provider->provideInput(outputBus, numberOfFrames);
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)
