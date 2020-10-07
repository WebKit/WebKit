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

#include "config.h"

#if ENABLE(WEB_AUDIO)
#include "AudioWorkletGlobalScope.h"

#include "AudioParamDescriptor.h"
#include "AudioWorklet.h"
#include "AudioWorkletMessagingProxy.h"
#include "AudioWorkletProcessorConstructionData.h"
#include "JSAudioWorkletProcessor.h"
#include "JSAudioWorkletProcessorConstructor.h"
#include "JSDOMConvert.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AudioWorkletGlobalScope);

AudioWorkletGlobalScope::AudioWorkletGlobalScope(AudioWorkletThread& thread, const WorkletParameters& parameters)
    : WorkletGlobalScope(parameters)
    , m_thread(thread)
    , m_sampleRate(parameters.sampleRate)
{
    ASSERT(!isMainThread());
}

AudioWorkletGlobalScope::~AudioWorkletGlobalScope() = default;

// https://www.w3.org/TR/webaudio/#dom-audioworkletglobalscope-registerprocessor
ExceptionOr<void> AudioWorkletGlobalScope::registerProcessor(String&& name, Ref<JSAudioWorkletProcessorConstructor>&& processorContructor)
{
    ASSERT(!isMainThread());

    if (name.isEmpty())
        return Exception { NotSupportedError, "Name cannot be the empty string"_s };

    if (m_processorConstructorMap.contains(name))
        return Exception { NotSupportedError, "A processor was already registered with this name"_s };

    JSC::JSObject* jsConstructor = processorContructor->callbackData()->callback();
    auto* globalObject = jsConstructor->globalObject();
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!jsConstructor->isConstructor(vm))
        return Exception { TypeError, "Class definitition passed to registerProcessor() is not a constructor"_s };

    auto prototype = jsConstructor->getPrototype(vm, globalObject);
    RETURN_IF_EXCEPTION(scope, Exception { TypeError });

    if (!prototype.isObject())
        return Exception { TypeError, "Class definitition passed to registerProcessor() has invalid prototype"_s };

    auto parameterDescriptorsValue = jsConstructor->get(globalObject, JSC::Identifier::fromString(vm, "parameterDescriptors"));
    RETURN_IF_EXCEPTION(scope, Exception { TypeError });

    Vector<AudioParamDescriptor> parameterDescriptors;
    if (!parameterDescriptorsValue.isUndefined()) {
        parameterDescriptors = convert<IDLSequence<IDLDictionary<AudioParamDescriptor>>>(*globalObject, parameterDescriptorsValue);
        RETURN_IF_EXCEPTION(scope, Exception { TypeError });
        UNUSED_PARAM(parameterDescriptors);
        HashSet<String> paramNames;
        for (auto& descriptor : parameterDescriptors) {
            auto addResult = paramNames.add(descriptor.name);
            if (!addResult.isNewEntry)
                return Exception { NotSupportedError, makeString("parameterDescriptors contain duplicate AudioParam name: ", name) };
            if (descriptor.defaultValue < descriptor.minValue)
                return Exception { InvalidStateError, makeString("AudioParamDescriptor with name '", name, "' has a defaultValue that is less than the minValue") };
            if (descriptor.defaultValue > descriptor.maxValue)
                return Exception { InvalidStateError, makeString("AudioParamDescriptor with name '", name, "' has a defaultValue that is greater than the maxValue") };
        }
    }

    m_processorConstructorMap.add(name, WTFMove(processorContructor));

    thread().messagingProxy().postTaskToAudioWorklet([name = name.isolatedCopy(), parameterDescriptors = crossThreadCopy(parameterDescriptors)](AudioWorklet& worklet) mutable {
        ASSERT(isMainThread());
        if (auto* audioContext = worklet.audioContext())
            audioContext->addAudioParamDescriptors(name, WTFMove(parameterDescriptors));
    });

    return { };
}

RefPtr<AudioWorkletProcessor> AudioWorkletGlobalScope::createProcessor(const String& name, TransferredMessagePort port, Ref<SerializedScriptValue>&& options)
{
    auto constructor = m_processorConstructorMap.get(name);
    ASSERT(constructor);
    if (!constructor)
        return nullptr;

    JSC::JSObject* jsConstructor = constructor->callbackData()->callback();
    auto* globalObject = jsConstructor->globalObject();
    JSC::VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSC::JSLockHolder lock { globalObject };

    m_pendingProcessorConstructionData = makeUnique<AudioWorkletProcessorConstructionData>(String { name }, MessagePort::entangle(*this, WTFMove(port)));

    JSC::MarkedArgumentBuffer args;
    auto arg = options->deserialize(*globalObject, globalObject, SerializationErrorMode::NonThrowing);
    RETURN_IF_EXCEPTION(scope, nullptr);
    args.append(arg);
    ASSERT(!args.hasOverflowed());

    auto* object = JSC::construct(globalObject, jsConstructor, args, "Failed to construct AudioWorkletProcessor");
    ASSERT(!!scope.exception() == !object);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto& jsProcessor = *JSC::jsCast<JSAudioWorkletProcessor*>(object);
    return &jsProcessor.wrapped();
}

void AudioWorkletGlobalScope::prepareForTermination()
{
    if (auto* defaultTaskGroup = this->defaultTaskGroup())
        defaultTaskGroup->stopAndDiscardAllTasks();
    stopActiveDOMObjects();

    m_processorConstructorMap.clear();

    // Event listeners would keep DOMWrapperWorld objects alive for too long. Also, they have references to JS objects,
    // which become dangling once Heap is destroyed.
    removeAllEventListeners();

    // MicrotaskQueue and RejectedPromiseTracker reference Heap.
    if (auto* eventLoop = this->existingEventLoop())
        eventLoop->clearMicrotaskQueue();
    removeRejectedPromiseTracker();
}

void AudioWorkletGlobalScope::postTask(Task&& task)
{
    thread().runLoop().postTask(WTFMove(task));
}

std::unique_ptr<AudioWorkletProcessorConstructionData> AudioWorkletGlobalScope::takePendingProcessorConstructionData()
{
    return std::exchange(m_pendingProcessorConstructionData, nullptr);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
