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

#if ENABLE(WEB_AUDIO)

#include "AudioSummingJunction.h"

#include "AudioContext.h"
#include "AudioNodeOutput.h"
#include <algorithm>

namespace WebCore {

AudioSummingJunction::AudioSummingJunction(BaseAudioContext& context)
    : m_context(context)
{
}

AudioSummingJunction::~AudioSummingJunction()
{
    if (m_renderingStateNeedUpdating)
        context().removeMarkedSummingJunction(this);
}

void AudioSummingJunction::markRenderingStateAsDirty()
{
    ASSERT(context().isGraphOwner());
    if (!m_renderingStateNeedUpdating && canUpdateState()) {
        context().markSummingJunctionDirty(this);
        m_renderingStateNeedUpdating = true;
    }
}

bool AudioSummingJunction::addOutput(AudioNodeOutput& output)
{
    ASSERT(context().isGraphOwner());
    if (!m_outputs.add(&output).isNewEntry)
        return false;

    if (!output.isEnabled())
        return true;

    if (!m_pendingRenderingOutputs)
        m_pendingRenderingOutputs.emplace(m_outputs);
    else
        m_pendingRenderingOutputs->append(output);

    markRenderingStateAsDirty();
    return true;
}

bool AudioSummingJunction::removeOutput(AudioNodeOutput& output)
{
    ASSERT(context().isGraphOwner());
    if (!m_outputs.remove(&output))
        return false;

    if (!output.isEnabled())
        return true;

    if (!m_pendingRenderingOutputs)
        m_pendingRenderingOutputs.emplace(m_outputs);
    else {
        bool wasRemoved = m_pendingRenderingOutputs->remove(output);
        ASSERT_UNUSED(wasRemoved, wasRemoved);
    }

    markRenderingStateAsDirty();
    return true;
}

void AudioSummingJunction::updateRenderingState()
{
    ASSERT(context().isAudioThread() && context().isGraphOwner());

    if (m_renderingStateNeedUpdating && canUpdateState()) {
        // Copy from m_outputs to m_renderingOutputs.
        if (!m_pendingRenderingOutputs)
            m_renderingOutputs = { };
        else {
            m_renderingOutputs = *std::exchange(m_pendingRenderingOutputs, WTF::nullopt);
            for (auto* output : m_renderingOutputs)
                output->updateRenderingState();
        }

        didUpdate();

        m_renderingStateNeedUpdating = false;
    }
}

unsigned AudioSummingJunction::maximumNumberOfChannels() const
{
    unsigned maxChannels = 0;
    for (auto* output : m_outputs) {
        if (!output->isEnabled())
            continue;

        // Use output()->numberOfChannels() instead of output->bus()->numberOfChannels(),
        // because the calling of AudioNodeOutput::bus() is not safe here.
        maxChannels = std::max(maxChannels, output->numberOfChannels());
    }
    return maxChannels;
}

void AudioSummingJunction::outputEnabledStateChanged(AudioNodeOutput& output)
{
    ASSERT(context().isGraphOwner());
    if (!m_pendingRenderingOutputs)
        m_pendingRenderingOutputs.emplace(m_outputs);
    else
        m_pendingRenderingOutputs->updatedEnabledState(output);
    markRenderingStateAsDirty();
}

inline AudioSummingJunction::RenderingOutputCollection::RenderingOutput::RenderingOutput(AudioNodeOutput* output)
    : output(output)
    , isEnabled(output->isEnabled())
{ }

void WebCore::AudioSummingJunction::RenderingOutputCollection::updatedEnabledState(AudioNodeOutput& output)
{
    auto index = m_outputs.find(&output);
    ASSERT(index != notFound);
    if (index == notFound)
        return;
    m_outputs[index].isEnabled = output.isEnabled();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
