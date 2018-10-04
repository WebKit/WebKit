/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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

#if ENABLE(MEDIA_STREAM)
#include "ActiveDOMObject.h"
#include "EventTarget.h"

namespace WebCore {

class Document;
class MediaStream;

class MediaRecorder final : public ActiveDOMObject, public RefCounted<MediaRecorder>, public EventTargetWithInlineData {
public:
    enum class RecordingState { Inactive, Recording, Paused };
    
    struct Options {
        String mimeType;
        unsigned audioBitsPerSecond;
        unsigned videoBitsPerSecond;
        unsigned bitsPerSecond;
    };
    
    static Ref<MediaRecorder> create(Document&, Ref<MediaStream>&&, Options&& = { });
    
    RecordingState state() const { return m_state; }
    
    using RefCounted::ref;
    using RefCounted::deref;
    
private:
    MediaRecorder(Document&, Ref<MediaStream>&&, Options&& = { });
    
    // EventTarget
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    EventTargetInterface eventTargetInterface() const final { return MediaStreamEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    // ActiveDOMObject API.
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;
    
    Options m_options;
    Ref<MediaStream> m_stream;
    RecordingState m_state { RecordingState::Inactive };
};
    
} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
