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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "V8AudioContext.h"

#include "AudioBuffer.h"
#include "AudioContext.h"
#include "BindingState.h"
#include "Frame.h"
#include "OfflineAudioContext.h"
#include "V8ArrayBuffer.h"
#include "V8AudioBuffer.h"
#include "V8Binding.h"
#include "V8OfflineAudioContext.h"
#include <wtf/ArrayBuffer.h>

namespace WebCore {

v8::Handle<v8::Value> V8AudioContext::constructorCustom(const v8::Arguments& args)
{
    Document* document = currentDocument(BindingState::instance());

    RefPtr<AudioContext> audioContext;
    
    if (!args.Length()) {
        // Constructor for default AudioContext which talks to audio hardware.
        ExceptionCode ec = 0;
        audioContext = AudioContext::create(document, ec);
        if (ec)
            return setDOMException(ec, args.GetIsolate());
        if (!audioContext.get())
            return throwError(v8SyntaxError, "audio resources unavailable for AudioContext construction", args.GetIsolate());
    } else {
#if ENABLE(LEGACY_WEB_AUDIO)
        // Constructor for offline (render-target) AudioContext which renders into an AudioBuffer.
        // new AudioContext(in unsigned long numberOfChannels, in unsigned long numberOfFrames, in float sampleRate);
        document->addConsoleMessage(JSMessageSource, WarningMessageLevel,
            "Deprecated AudioContext constructor: use OfflineAudioContext instead");

        return V8OfflineAudioContext::constructorCallback(args);
#else
        return throwError(v8SyntaxError, "Illegal AudioContext constructor", args.GetIsolate());
#endif
    }

    if (!audioContext.get())
        return throwError(v8SyntaxError, "Error creating AudioContext", args.GetIsolate());
    
    // Transform the holder into a wrapper object for the audio context.
    v8::Handle<v8::Object> wrapper = args.Holder();
    V8DOMWrapper::associateObjectWithWrapper(audioContext.release(), &info, wrapper, args.GetIsolate(), WrapperConfiguration::Dependent);
    
    return wrapper;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
