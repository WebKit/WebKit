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
#include "AudioUtilities.h"
#include "Logging.h"
#include "MediaStreamAudioSourceOptions.h"
#include "MultiChannelResampler.h"
#include "WebAudioSourceProvider.h"
#include <algorithm>
#include <wtf/Locker.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringView.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaStreamAudioSourceNode);

ExceptionOr<Ref<MediaStreamAudioSourceNode>> MediaStreamAudioSourceNode::create(BaseAudioContext& context, MediaStreamAudioSourceOptions&& options)
{
    Ref mediaStream = WTF::move(options.mediaStream);

    auto audioTracks = mediaStream->getAudioTracks();
    if (audioTracks.isEmpty())
        return Exception { ExceptionCode::InvalidStateError, "Media stream has no audio tracks"_s };

    // https://webaudio.github.io/web-audio-api/#mediastreamaudiosourcenode: the input track is the
    // first track in the media stream after sorting the audio tracks by their id using a code unit ordering.
    Ref inputTrack = *std::min_element(audioTracks.begin(), audioTracks.end(), [](auto& a, auto& b) {
        return codePointCompareLessThan(a->id(), b->id());
    });

    RefPtr provider = inputTrack->createAudioSourceProvider();
    if (!provider)
        return Exception { ExceptionCode::InvalidStateError, "Could not find an audio track with an audio source provider"_s };

    auto node = adoptRef(*new MediaStreamAudioSourceNode(context, WTF::move(mediaStream), provider.releaseNonNull()));
    node->preparePlayback();
    return node;
}

MediaStreamAudioSourceNodeBase::MediaStreamAudioSourceNodeBase(BaseAudioContext& context, NodeType type, Ref<WebAudioSourceProvider>&& provider)
    : AudioNode(context, type)
    , m_provider(WTF::move(provider))
{
    m_provider->setClient(this);
    addOutput(2);
    initialize();
}

MediaStreamAudioSourceNodeBase::~MediaStreamAudioSourceNodeBase()
{
    detachProvider();
}

void MediaStreamAudioSourceNodeBase::detachProvider()
{
    m_provider->setClient(nullptr);
    uninitialize();
}

void MediaStreamAudioSourceNodeBase::preparePlayback()
{
    setFormat(2, sampleRate());
    Ref node = *this;
    protect(context())->sourceNodeWillBeginPlayback(node);
}

MediaStreamAudioSourceNode::MediaStreamAudioSourceNode(BaseAudioContext& context, Ref<MediaStream>&& mediaStream, Ref<WebAudioSourceProvider>&& provider)
    : MediaStreamAudioSourceNodeBase(context, NodeTypeMediaStreamAudioSource, WTF::move(provider))
    , m_mediaStream(WTF::move(mediaStream))
{
}

MediaStreamAudioSourceNode::~MediaStreamAudioSourceNode()
{
    detachProvider();
}

void MediaStreamAudioSourceNodeBase::setFormat(size_t numberOfChannels, float sourceSampleRate)
{
    Locker locker { m_processLock };

    if (numberOfChannels == m_sourceNumberOfChannels && sourceSampleRate == m_sourceSampleRate)
        return;

    if (!numberOfChannels || numberOfChannels > AudioContext::maxNumberOfChannels) {
        LOG(Media, "MediaStreamAudioSourceNodeBase::setFormat(%u, %f) - unhandled format change", static_cast<unsigned>(numberOfChannels), sourceSampleRate);
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
        m_multiChannelResampler = makeUnique<MultiChannelResampler>(scaleFactor, numberOfChannels, AudioUtilities::renderQuantumSize, [protectedThis = Ref { *this }](AudioBus& bus, size_t framesToProcess) {
            protectedThis->provideInput(bus, framesToProcess);
        });
    }

    {
        Locker contextLocker { context().graphLock() };
        protect(output(0))->setNumberOfChannels(numberOfChannels);
    }
}

void MediaStreamAudioSourceNodeBase::provideInput(AudioBus& bus, size_t framesToProcess)
{
    m_provider->provideInput(bus, framesToProcess);
}

void MediaStreamAudioSourceNodeBase::process(size_t numberOfFrames)
{
    Ref outputBus = output(0)->bus();

    if (!m_processLock.tryLock()) {
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
        ASSERT(m_sourceSampleRate == sampleRate());
        provideInput(outputBus, numberOfFrames);
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)
