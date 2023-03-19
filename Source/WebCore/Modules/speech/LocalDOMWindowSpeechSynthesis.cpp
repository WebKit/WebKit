/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LocalDOMWindowSpeechSynthesis.h"

#if ENABLE(SPEECH_SYNTHESIS)

#include "LocalDOMWindow.h"
#include "Page.h"

namespace WebCore {

LocalDOMWindowSpeechSynthesis::LocalDOMWindowSpeechSynthesis(LocalDOMWindow* window)
    : LocalDOMWindowProperty(window)
{
}

LocalDOMWindowSpeechSynthesis::~LocalDOMWindowSpeechSynthesis() = default;

const char* LocalDOMWindowSpeechSynthesis::supplementName()
{
    return "LocalDOMWindowSpeechSynthesis";
}

// static
LocalDOMWindowSpeechSynthesis* LocalDOMWindowSpeechSynthesis::from(LocalDOMWindow* window)
{
    auto* supplement = static_cast<LocalDOMWindowSpeechSynthesis*>(Supplement<LocalDOMWindow>::from(window, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<LocalDOMWindowSpeechSynthesis>(window);
        supplement = newSupplement.get();
        provideTo(window, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

// static
SpeechSynthesis* LocalDOMWindowSpeechSynthesis::speechSynthesis(LocalDOMWindow& window)
{
    return LocalDOMWindowSpeechSynthesis::from(&window)->speechSynthesis();
}

SpeechSynthesis* LocalDOMWindowSpeechSynthesis::speechSynthesis()
{
    if (!m_speechSynthesis && frame() && frame()->document())
        m_speechSynthesis = SpeechSynthesis::create(*frame()->document());
    return m_speechSynthesis.get();
}

} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS)
