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

#include "ArrayBuffer.h"
#include "AudioBuffer.h"
#include "AudioContext.h"
#include "Frame.h"
#include "V8ArrayBuffer.h"
#include "V8AudioBuffer.h"
#include "V8Binding.h"
#include "V8Proxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8AudioContext::constructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.AudioContext.Contructor");

    Frame* frame = V8Proxy::retrieveFrameForCurrentContext();
    if (!frame)
        return throwError("AudioContext constructor associated frame is unavailable", V8Proxy::ReferenceError);

    Document* document = frame->document();
    if (!document)
        return throwError("AudioContext constructor associated document is unavailable", V8Proxy::ReferenceError);

    RefPtr<AudioContext> audioContext;
    
    if (!args.Length()) {
        // Constructor for default AudioContext which talks to audio hardware.
        audioContext = AudioContext::create(document);
    } else {
        // Constructor for offline (render-target) AudioContext which renders into an AudioBuffer.
        // new AudioContext(in unsigned long numberOfChannels, in unsigned long numberOfFrames, in float sampleRate);
        if (args.Length() < 3)
            return throwError("Not enough arguments", V8Proxy::SyntaxError);

        bool ok = false;

        unsigned numberOfChannels = toInt32(args[0], ok);
        if (!ok)
            return throwError("Invalid number of channels", V8Proxy::SyntaxError);

        unsigned numberOfFrames = toInt32(args[1], ok);
        if (!ok)
            return throwError("Invalid number of frames", V8Proxy::SyntaxError);

        float sampleRate = toFloat(args[2]);

        audioContext = AudioContext::createOfflineContext(document, numberOfChannels, numberOfFrames, sampleRate);
    }

    if (!audioContext.get())
        return throwError("Error creating AudioContext", V8Proxy::SyntaxError);
    
    // Transform the holder into a wrapper object for the audio context.
    V8DOMWrapper::setDOMWrapper(args.Holder(), &info, audioContext.get());
    audioContext->ref();
    
    return args.Holder();
}

v8::Handle<v8::Value> V8AudioContext::createBufferCallback(const v8::Arguments& args)
{
    if (args.Length() < 2)
        return throwError("Not enough arguments", V8Proxy::SyntaxError);

    AudioContext* audioContext = toNative(args.Holder());
    ASSERT(audioContext);

    v8::Handle<v8::Value> arg = args[0];
    
    // AudioBuffer createBuffer(in ArrayBuffer buffer, in boolean mixToMono);
    if (V8ArrayBuffer::HasInstance(arg)) {
        v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(arg);
        ArrayBuffer* arrayBuffer = V8ArrayBuffer::toNative(object);
        ASSERT(arrayBuffer);

        if (arrayBuffer) {
            bool mixToMono = args[1]->ToBoolean()->Value();

            RefPtr<AudioBuffer> audioBuffer = audioContext->createBuffer(arrayBuffer, mixToMono);
            if (!audioBuffer.get())
                return throwError("Error decoding audio file data", V8Proxy::SyntaxError);

            return toV8(audioBuffer.get());
        }
        
        return v8::Undefined();
    }
    
    // AudioBuffer createBuffer(in unsigned long numberOfChannels, in unsigned long numberOfFrames, in float sampleRate);
    if (args.Length() < 3)
        return throwError("Not enough arguments", V8Proxy::SyntaxError);

    bool ok = false;
    
    unsigned numberOfChannels = toInt32(args[0], ok);
    if (!ok)
        return throwError("Invalid number of channels", V8Proxy::SyntaxError);
    
    unsigned numberOfFrames = toInt32(args[1], ok);
    if (!ok)
        return throwError("Invalid number of frames", V8Proxy::SyntaxError);
    
    float sampleRate = toFloat(args[2]);
    
    RefPtr<AudioBuffer> audioBuffer = audioContext->createBuffer(numberOfChannels, numberOfFrames, sampleRate);
    if (!audioBuffer.get())
        return throwError("Error creating AudioBuffer", V8Proxy::SyntaxError);

    return toV8(audioBuffer.get());
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
