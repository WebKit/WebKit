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

#pragma once

#include "AudioBus.h"
#include <iterator>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class BaseAudioContext;
class AudioNodeOutput;

// An AudioSummingJunction represents a point where zero, one, or more AudioNodeOutputs connect.

class AudioSummingJunction {
public:
    explicit AudioSummingJunction(BaseAudioContext&);
    virtual ~AudioSummingJunction();

    // Can be called from any thread.
    BaseAudioContext* context() { return m_context.get(); }
    const BaseAudioContext* context() const { return m_context.get(); }

    // This copies m_outputs to m_renderingOutputs. Please see comments for these lists below.
    // This must be called when we own the context's graph lock in the audio thread at the very start or end of the render quantum.
    void updateRenderingState();

    class RenderingOutputCollection {
    public:
        RenderingOutputCollection() = default;
        RenderingOutputCollection(const HashSet<AudioNodeOutput*>& outputs)
            : m_outputs(copyToVectorOf<RenderingOutput>(outputs))
        {
        }

        bool isEmpty() const { return begin() == end(); }
        size_t size() const { return std::distance(begin(), end()); }

        void append(AudioNodeOutput& output) { m_outputs.append(&output); }
        bool remove(AudioNodeOutput& output) { return m_outputs.removeFirst(&output); }
        void updatedEnabledState(AudioNodeOutput&);

        struct RenderingOutput {
            RenderingOutput(AudioNodeOutput*);
            bool operator==(const AudioNodeOutput* other) const { return output == other; }
            AudioNodeOutput* output;
            bool isEnabled;
        };

        class ConstIterator {
        public:
            using difference_type = size_t;
            using value_type = AudioNodeOutput*;
            using pointer = AudioNodeOutput**;
            using reference = AudioNodeOutput*&;
            using iterator_category = std::forward_iterator_tag;

            ConstIterator(const Vector<RenderingOutput>& outputs, Vector<RenderingOutput>::const_iterator position)
                : m_position(position)
                , m_end(outputs.end())
            {
                skipDisabledOutputs();
            }

            AudioNodeOutput* operator*() const { return m_position->output; }
            constexpr bool operator==(const ConstIterator& other) const { return m_position == other.m_position; }
            constexpr bool operator!=(const ConstIterator& other) const { return !(*this == other); }
            ConstIterator& operator++()
            {
                ASSERT(m_position != m_end);
                ++m_position;
                skipDisabledOutputs();
                return *this;
            }

        private:
            void skipDisabledOutputs()
            {
                while (m_position != m_end && !m_position->isEnabled)
                    ++m_position;
            }

            Vector<RenderingOutput>::const_iterator m_position;
            Vector<RenderingOutput>::const_iterator m_end;
        };

        ConstIterator begin() const { return ConstIterator { m_outputs, m_outputs.begin() }; }
        ConstIterator end() const { return ConstIterator { m_outputs, m_outputs.end() }; }

    private:
        Vector<RenderingOutput> m_outputs;
    };

    // Rendering code accesses its version of the current connections here.
    const RenderingOutputCollection& renderingOutputs() const { return m_renderingOutputs; }
    unsigned numberOfRenderingConnections() const { return m_renderingOutputs.size(); }
    bool isConnected() const { return !m_renderingOutputs.isEmpty(); }

    virtual bool canUpdateState() = 0;
    virtual void didUpdate() = 0;

    bool addOutput(AudioNodeOutput&);
    bool removeOutput(AudioNodeOutput&);

    void markRenderingStateAsDirty();

    virtual void outputEnabledStateChanged(AudioNodeOutput&);

protected:
    unsigned maximumNumberOfChannels() const;

private:
    WeakPtr<BaseAudioContext> m_context;

    // m_renderingOutputs is a copy of m_outputs which will never be modified during the graph rendering on the audio thread.
    // This is the list which is used by the rendering code.
    // Whenever m_outputs is modified, the context is told so it can later update m_renderingOutputs from m_outputs at a safe time.
    // Most of the time, m_renderingOutputs is identical to m_outputs.
    RenderingOutputCollection m_renderingOutputs;

    // m_renderingStateNeedUpdating keeps track if m_outputs is modified.
    bool m_renderingStateNeedUpdating { false };

    // m_outputs contains the AudioNodeOutputs representing current connections which are not disabled.
    // The rendering code should never use this directly, but instead uses m_renderingOutputs.
    HashSet<AudioNodeOutput*> m_outputs;
    Optional<RenderingOutputCollection> m_pendingRenderingOutputs;
};

} // namespace WebCore
