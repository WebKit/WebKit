/*
 * Copyright (C) 2012, Intel Corporation. All rights reserved.
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

#include "AudioNode.h"

namespace WebCore {

// AudioBasicInspectorNode is an AudioNode with one input and one output where the output might not necessarily connect to another node's input.
// If the output is not connected to any other node, then the AudioBasicInspectorNode's processIfNecessary() function will be called automatically by
// AudioContext before the end of each render quantum so that it can inspect the audio stream.
class AudioBasicInspectorNode : public AudioNode {
    WTF_MAKE_ISO_ALLOCATED(AudioBasicInspectorNode);
public:
    AudioBasicInspectorNode(BaseAudioContext&, NodeType);

protected:
    bool m_needAutomaticPull { false }; // When setting to true, AudioBasicInspectorNode will be pulled automatically by AudioContext before the end of each render quantum.

private:
    void pullInputs(size_t framesToProcess) override;
    void checkNumberOfChannelsForInput(AudioNodeInput*) override;

    void updatePullStatus() override;
};

} // namespace WebCore
