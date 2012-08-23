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
#include "V8ArrayBuffer.h"
#include "V8AudioBuffer.h"
#include "V8Binding.h"
#include <wtf/ArrayBuffer.h>

namespace WebCore {

v8::Handle<v8::Value> V8AudioContext::constructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.AudioContext.Contructor");

    if (!args.IsConstructCall())
        return throwTypeError("AudioContext constructor cannot be called as a function.", args.GetIsolate());

    if (ConstructorMode::current() == ConstructorMode::WrapExistingObject)
        return args.Holder();

    Frame* frame = currentFrame(BindingState::instance());
    if (!frame)
        return throwError(ReferenceError, "AudioContext constructor associated frame is unavailable", args.GetIsolate());

    Document* document = frame->document();
    if (!document)
        return throwError(ReferenceError, "AudioContext constructor associated document is unavailable", args.GetIsolate());

    RefPtr<AudioContext> audioContext;
    
    if (!args.Length()) {
        // Constructor for default AudioContext which talks to audio hardware.
        ExceptionCode ec = 0;
        audioContext = AudioContext::create(document, ec);
        if (ec)
            return setDOMException(ec, args.GetIsolate());
        if (!audioContext.get())
            return throwError(SyntaxError, "audio resources unavailable for AudioContext construction", args.GetIsolate());
    } else {
        // Constructor for offline (render-target) AudioContext which renders into an AudioBuffer.
        // new AudioContext(in unsigned long numberOfChannels, in unsigned long numberOfFrames, in float sampleRate);
        if (args.Length() < 3)
            return throwNotEnoughArgumentsError(args.GetIsolate());

        bool ok = false;

        int32_t numberOfChannels = toInt32(args[0], ok);
        if (!ok || numberOfChannels <= 0 || numberOfChannels > 10)
            return throwError(SyntaxError, "Invalid number of channels", args.GetIsolate());

        int32_t numberOfFrames = toInt32(args[1], ok);
        if (!ok || numberOfFrames <= 0)
            return throwError(SyntaxError, "Invalid number of frames", args.GetIsolate());

        float sampleRate = toFloat(args[2]);
        if (sampleRate <= 0)
            return throwError(SyntaxError, "Invalid sample rate", args.GetIsolate());

        ExceptionCode ec = 0;
        audioContext = AudioContext::createOfflineContext(document, numberOfChannels, numberOfFrames, sampleRate, ec);
        if (ec)
            return setDOMException(ec, args.GetIsolate());
    }

    if (!audioContext.get())
        return throwError(SyntaxError, "Error creating AudioContext", args.GetIsolate());
    
    // Transform the holder into a wrapper object for the audio context.
    V8DOMWrapper::setDOMWrapper(args.Holder(), &info, audioContext.get());
    audioContext->ref();
    
    return args.Holder();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
