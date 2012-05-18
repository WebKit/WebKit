/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#include "V8AudioBufferSourceNode.h"

#include "AudioBuffer.h"
#include "AudioBufferSourceNode.h"
#include "ExceptionCode.h"
#include "V8AudioBuffer.h"
#include "V8Binding.h"
#include "V8Proxy.h"

namespace WebCore {

void V8AudioBufferSourceNode::bufferAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.AudioBufferSourceNode.buffer._set");
    v8::Handle<v8::Object> holder = info.Holder();
    AudioBufferSourceNode* imp = V8AudioBufferSourceNode::toNative(holder);

    AudioBuffer* buffer = 0;
    if (V8AudioBuffer::HasInstance(value)) {
        buffer = V8AudioBuffer::toNative(value->ToObject());
        if (buffer && !imp->setBuffer(buffer)) {
            V8Proxy::throwTypeError("AudioBuffer unsupported number of channels");
            return;
        }
    }
    
    if (!buffer) {
        V8Proxy::throwTypeError("Value is not of type AudioBuffer");
        return;
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
