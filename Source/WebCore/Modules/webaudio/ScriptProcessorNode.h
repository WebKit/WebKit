/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#pragma once

#include "ActiveDOMObject.h"
#include "AudioBus.h"
#include "AudioNode.h"
#include "EventListener.h"
#include "EventTarget.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class AudioBuffer;
class AudioContext;
class AudioProcessingEvent;

// ScriptProcessorNode is an AudioNode which allows for arbitrary synthesis or processing directly using JavaScript.
// The API allows for a variable number of inputs and outputs, although it must have at least one input or output.
// This basic implementation supports no more than one input and output.
// The "onaudioprocess" attribute is an event listener which will get called periodically with an AudioProcessingEvent which has
// AudioBuffers for each input and output.

class ScriptProcessorNode final : public AudioNode, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(ScriptProcessorNode);
public:
    // bufferSize must be one of the following values: 256, 512, 1024, 2048, 4096, 8192, 16384.
    // This value controls how frequently the onaudioprocess event handler is called and how many sample-frames need to be processed each call.
    // Lower numbers for bufferSize will result in a lower (better) latency. Higher numbers will be necessary to avoid audio breakup and glitches.
    // The value chosen must carefully balance between latency and audio quality.
    static Ref<ScriptProcessorNode> create(BaseAudioContext&, size_t bufferSize, unsigned numberOfInputChannels, unsigned numberOfOutputChannels);

    virtual ~ScriptProcessorNode();

    const char* activeDOMObjectName() const override { return "ScriptProcessorNode"; }

    // AudioNode
    void process(size_t framesToProcess) override;
    void initialize() override;
    void uninitialize() override;

    size_t bufferSize() const { return m_bufferSize; }

    ExceptionOr<void> setChannelCount(unsigned) final;
    ExceptionOr<void> setChannelCountMode(ChannelCountMode) final;

private:
    double tailTime() const override;
    double latencyTime() const override;
    bool requiresTailProcessing() const final;

    ScriptProcessorNode(BaseAudioContext&, size_t bufferSize, unsigned numberOfInputChannels, unsigned numberOfOutputChannels);

    bool virtualHasPendingActivity() const final;
    void eventListenersDidChange() final;
    void fireProcessEvent(unsigned bufferIndex);

    RefPtr<AudioBuffer> createInputBufferForJS(AudioBuffer*) const;
    RefPtr<AudioBuffer> createOutputBufferForJS(AudioBuffer&) const;

    // Double buffering.
    static constexpr unsigned bufferCount = 2;
    unsigned bufferIndex() const { return m_bufferIndex; }
    void swapBuffers() { m_bufferIndex = (m_bufferIndex + 1) % bufferCount; }

    unsigned m_bufferIndex { 0 };
    std::array<Lock, bufferCount> m_bufferLocks;
    std::array<RefPtr<AudioBuffer>, bufferCount> m_inputBuffers;
    std::array<RefPtr<AudioBuffer>, bufferCount> m_outputBuffers;
    mutable RefPtr<AudioBuffer> m_cachedInputBufferForJS;
    mutable RefPtr<AudioBuffer> m_cachedOutputBufferForJS;

    size_t m_bufferSize;
    unsigned m_bufferReadWriteIndex { 0 };

    unsigned m_numberOfInputChannels;
    unsigned m_numberOfOutputChannels;

    RefPtr<AudioBus> m_internalInputBus;
    bool m_hasAudioProcessEventListener { false };
};

} // namespace WebCore
