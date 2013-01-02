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

#include "V8OscillatorNode.h"

#include "ExceptionCode.h"
#include "OscillatorNode.h"
#include "V8Binding.h"

namespace WebCore {

void V8OscillatorNode::typeAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.OscillatorNode.type._set");
    v8::Handle<v8::Object> holder = info.Holder();
    OscillatorNode* imp = V8OscillatorNode::toNative(holder);

#if ENABLE(LEGACY_WEB_AUDIO)    
    if (value->IsNumber()) {
        bool ok = false;
        uint32_t type = toUInt32(value, ok);
        if (!ok || !imp->setType(type))
            throwError(v8TypeError, "Illegal OscillatorNode type", info.GetIsolate());
        return;
    }
#endif

    if (value->IsString()) {
        String type = toWebCoreString(value);
        if (type == "sine" || type == "square" || type == "sawtooth" || type == "triangle") {
            imp->setType(type);
            return;
        }
    }
    
    throwError(v8TypeError, "Illegal OscillatorNode type", info.GetIsolate());
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
