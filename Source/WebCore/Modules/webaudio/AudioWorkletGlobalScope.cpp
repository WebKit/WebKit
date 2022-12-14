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
#include "BaseAudioContext.h"
#include "CommonVM.h"
#include "JSAudioWorkletProcessor.h"
#include "JSAudioWorkletProcessorConstructor.h"
#include "JSDOMConvert.h"
#include "WebCoreOpaqueRoot.h"
#include <JavaScriptCore/JSLock.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AudioWorkletGlobalScope);

RefPtr<AudioWorkletGlobalScope> AudioWorkletGlobalScope::tryCreate(AudioWorkletThread& thread, const WorkletParameters& parameters)
{
    auto vm = JSC::VM::tryCreate();
    if (!vm)
        return nullptr;
    auto scope = adoptRef(*new AudioWorkletGlobalScope(thread, vm.releaseNonNull(), parameters));
    scope->addToContextsMap();
    return scope;
}

AudioWorkletGlobalScope::AudioWorkletGlobalScope(AudioWorkletThread& thread, Ref<JSC::VM>&& vm, const WorkletParameters& parameters)
    : WorkletGlobalScope(thread, WTFMove(vm), parameters)
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

    if (!jsConstructor->isConstructor())
        return Exception { TypeError, "Class definition passed to registerProcessor() is not a constructor"_s };

    auto prototype = jsConstructor->getPrototype(vm, globalObject);
    RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

    if (!prototype.isObject())
        return Exception { TypeError, "Class definition passed to registerProcessor() has invalid prototype"_s };

    auto parameterDescriptorsValue = jsConstructor->get(globalObject, JSC::Identifier::fromString(vm, "parameterDescriptors"_s));
    RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

    Vector<AudioParamDescriptor> parameterDescriptors;
    if (!parameterDescriptorsValue.isUndefined()) {
        parameterDescriptors = convert<IDLSequence<IDLDictionary<AudioParamDescriptor>>>(*globalObject, parameterDescriptorsValue);
        RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });
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

    auto addResult = m_processorConstructorMap.add(name, WTFMove(processorContructor));

    // We've already checked at the beginning of this function but then we ran some JS so we need to check again.
    if (!addResult.isNewEntry)
        return Exception { NotSupportedError, "A processor was already registered with this name"_s };

    thread().messagingProxy().postTaskToAudioWorklet([name = WTFMove(name).isolatedCopy(), parameterDescriptors = crossThreadCopy(WTFMove(parameterDescriptors))](AudioWorklet& worklet) mutable {
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
    auto* globalObject = constructor->callbackData()->globalObject();
    JSC::VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSC::JSLockHolder lock { globalObject };

    m_pendingProcessorConstructionData = makeUnique<AudioWorkletProcessorConstructionData>(String { name }, MessagePort::entangle(*this, WTFMove(port)));

    JSC::MarkedArgumentBuffer args;
    auto arg = options->deserialize(*globalObject, globalObject, SerializationErrorMode::NonThrowing);
    RETURN_IF_EXCEPTION(scope, nullptr);
    args.append(arg);
    ASSERT(!args.hasOverflowed());

    auto* object = JSC::construct(globalObject, jsConstructor, args, "Failed to construct AudioWorkletProcessor"_s);
    ASSERT(!!scope.exception() == !object);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto* jsProcessor = JSC::jsDynamicCast<JSAudioWorkletProcessor*>(object);
    if (!jsProcessor)
        return nullptr;

    m_processors.add(jsProcessor->wrapped());
    return &jsProcessor->wrapped();
}

void AudioWorkletGlobalScope::prepareForDestruction()
{
    m_processorConstructorMap.clear();

    WorkletGlobalScope::prepareForDestruction();
}

std::unique_ptr<AudioWorkletProcessorConstructionData> AudioWorkletGlobalScope::takePendingProcessorConstructionData()
{
    return std::exchange(m_pendingProcessorConstructionData, nullptr);
}

AudioWorkletThread& AudioWorkletGlobalScope::thread() const
{
    return *static_cast<AudioWorkletThread*>(workerOrWorkletThread());
}

void AudioWorkletGlobalScope::handlePreRenderTasks()
{
    // We grab the JS API lock at the beginning of rendering and release it at the end of rendering.
    // This makes sure that we only drain the MicroTask queue after each render quantum.
    // It is only safe to grab the lock if we are on the context thread. We might get called on
    // another thread if audio rendering started before the audio worklet got started.
    if (isContextThread())
        m_lockDuringRendering.emplace(script()->vm());
}

void AudioWorkletGlobalScope::handlePostRenderTasks(size_t currentFrame)
{
    m_currentFrame = currentFrame;

    {
        // Heap allocations are forbidden on the audio thread for performance reasons so we need to
        // explicitly allow the following allocation(s).
        DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
        // This takes care of processing the MicroTask queue after rendering.
        m_lockDuringRendering = std::nullopt;
    }
}

void AudioWorkletGlobalScope::processorIsNoLongerNeeded(AudioWorkletProcessor& processor)
{
    m_processors.remove(processor);
}

void AudioWorkletGlobalScope::visitProcessors(JSC::AbstractSlotVisitor& visitor)
{
    m_processors.forEach([&](auto& processor) {
        addWebCoreOpaqueRoot(visitor, processor);
    });
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
