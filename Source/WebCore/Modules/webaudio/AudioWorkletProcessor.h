/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_AUDIO)
#include "AudioArray.h"
#include "ExceptionOr.h"
#include "JSValueInWrappedObject.h"
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC {
class JSArray;
class MarkedArgumentBufferBase;
}

namespace WebCore {

class AudioBus;
class AudioWorkletProcessorConstructionData;
class JSCallbackDataStrong;
class MessagePort;
class ScriptExecutionContext;

class AudioWorkletProcessor : public ThreadSafeRefCounted<AudioWorkletProcessor> {
public:
    static ExceptionOr<Ref<AudioWorkletProcessor>> create(ScriptExecutionContext&);
    ~AudioWorkletProcessor();

    const String& name() const { return m_name; }
    MessagePort& port() { return m_port.get(); }

    bool process(const Vector<RefPtr<AudioBus>>& inputs, Vector<Ref<AudioBus>>& outputs, const HashMap<String, std::unique_ptr<AudioFloatArray>>& paramValuesMap, bool& threwException);

    void setProcessCallback(std::unique_ptr<JSCallbackDataStrong>&&);

    JSValueInWrappedObject& jsInputsWrapper() { return m_jsInputs; }
    JSValueInWrappedObject& jsOutputsWrapper() { return m_jsOutputs; }
    JSValueInWrappedObject& jsParamValuesWrapper() { return m_jsParamValues; }

private:
    explicit AudioWorkletProcessor(const AudioWorkletProcessorConstructionData&);
    void buildJSArguments(JSC::VM&, JSC::JSGlobalObject&, JSC::MarkedArgumentBufferBase&, const Vector<RefPtr<AudioBus>>& inputs, Vector<Ref<AudioBus>>& outputs, const HashMap<String, std::unique_ptr<AudioFloatArray>>& paramValuesMap);

    String m_name;
    Ref<MessagePort> m_port;
    std::unique_ptr<JSCallbackDataStrong> m_processCallback;
    JSValueInWrappedObject m_jsInputs;
    JSValueInWrappedObject m_jsOutputs;
    JSValueInWrappedObject m_jsParamValues;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
