/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
#include "AudioWorkletProcessor.h"

#include "AudioBus.h"
#include "AudioChannel.h"
#include "AudioWorkletGlobalScope.h"
#include "AudioWorkletProcessorConstructionData.h"
#include "JSCallbackData.h"
#include "JSDOMExceptionHandling.h"
#include "MessagePort.h"
#include "WebCoreOpaqueRoot.h"
#include <JavaScriptCore/JSTypedArrays.h>
#include <wtf/GetPtr.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AudioWorkletProcessor);

using namespace JSC;

static unsigned busChannelCount(const AudioBus& bus)
{
    return bus.numberOfChannels();
}

static unsigned busChannelCount(const AudioBus* bus)
{
    return bus ? busChannelCount(*bus) : 0;
}

static JSArray* toJSArray(JSValueInWrappedObject& wrapper)
{
    return wrapper ? jsDynamicCast<JSArray*>(wrapper.getValue()) : nullptr;
}

static JSObject* toJSObject(JSValueInWrappedObject& wrapper)
{
    return wrapper ? jsDynamicCast<JSObject*>(wrapper.getValue()) : nullptr;
}

static JSFloat32Array* constructJSFloat32Array(JSGlobalObject& globalObject, unsigned length, const float* data = nullptr)
{
    constexpr bool isResizableOrGrowableShared = false;
    auto* jsArray = JSFloat32Array::create(&globalObject, globalObject.typedArrayStructure(TypeFloat32, isResizableOrGrowableShared), length);
    if (data)
        memcpy(jsArray->typedVector(), data, sizeof(float) * length);
    return jsArray;
}

static JSObject* constructFrozenKeyValueObject(VM& vm, JSGlobalObject& globalObject, const MemoryCompactLookupOnlyRobinHoodHashMap<String, std::unique_ptr<AudioFloatArray>>& paramValuesMap)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* plainObjectStructure = JSFinalObject::createStructure(vm, &globalObject, globalObject.objectPrototype(), 0);
    auto* object = JSFinalObject::create(vm, plainObjectStructure);
    for (auto& pair : paramValuesMap) {
        PutPropertySlot slot(object, false, PutPropertySlot::PutById);
        // Per the specification, if the value is constant, we pass the JS an array with length 1, with the array item being the constant.
        unsigned jsArraySize = pair.value->containsConstantValue() ? 1 : pair.value->size();
        object->putInline(&globalObject, Identifier::fromString(vm, pair.key), constructJSFloat32Array(globalObject, jsArraySize, pair.value->data()), slot);
    }
    JSC::objectConstructorFreeze(&globalObject, object);
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception());
    return object;
}

enum class ShouldPopulateWithBusData : bool { No, Yes };

template <typename T>
static JSArray* constructFrozenJSArray(VM& vm, JSGlobalObject& globalObject, JSC::ThrowScope& scope, const T& bus, ShouldPopulateWithBusData shouldPopulateWithBusData)
{
    unsigned numberOfChannels = busChannelCount(bus.get());
    auto* channelsData = JSArray::create(vm, globalObject.originalArrayStructureForIndexingType(ArrayWithContiguous), numberOfChannels);
    for (unsigned j = 0; j < numberOfChannels; ++j) {
        auto* channel = bus->channel(j);
        channelsData->setIndexQuickly(vm, j, constructJSFloat32Array(globalObject, channel->length(), shouldPopulateWithBusData == ShouldPopulateWithBusData::Yes ? channel->data() : nullptr));
    }
    JSC::objectConstructorFreeze(&globalObject, channelsData);
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception());
    return channelsData;
}

template <typename T>
static JSArray* constructFrozenJSArray(VM& vm, JSGlobalObject& globalObject, const Vector<T>& buses, ShouldPopulateWithBusData shouldPopulateWithBusData)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* array = JSArray::create(vm, globalObject.originalArrayStructureForIndexingType(ArrayWithContiguous), buses.size());
    for (unsigned i = 0; i < buses.size(); ++i)
        array->setIndexQuickly(vm, i, constructFrozenJSArray(vm, globalObject, scope, buses[i], shouldPopulateWithBusData));
    JSC::objectConstructorFreeze(&globalObject, array);
    EXCEPTION_ASSERT(!scope.exception());
    return array;
}

static void copyDataFromJSArrayToBuses(JSGlobalObject& globalObject, const JSArray& jsArray, Vector<Ref<AudioBus>>& buses)
{
    // We can safely make assumptions about the structure of the JSArray since we use frozen arrays.
    for (unsigned i = 0; i < buses.size(); ++i) {
        auto& bus = buses[i];
        auto* channelsArray = jsDynamicCast<JSArray*>(jsArray.getIndex(&globalObject, i));
        if (UNLIKELY(!channelsArray)) {
            bus->zero();
            continue;
        }
        for (unsigned j = 0; j < bus->numberOfChannels(); ++j) {
            auto* channel = bus->channel(j);
            auto* jsChannelData = jsDynamicCast<JSFloat32Array*>(channelsArray->getIndex(&globalObject, j));
            if (LIKELY(jsChannelData && jsChannelData->length() == channel->length()))
                memcpy(channel->mutableData(), jsChannelData->typedVector(), sizeof(float) * channel->length());
            else
                channel->zero();
        }
    }
}

static bool copyDataFromBusesToJSArray(JSGlobalObject& globalObject, const Vector<RefPtr<AudioBus>>& buses, JSArray* jsArray)
{
    if (!jsArray)
        return false;

    for (size_t busIndex = 0; busIndex < buses.size(); ++busIndex) {
        auto& bus = buses[busIndex];
        auto* jsChannelsArray = jsDynamicCast<JSArray*>(jsArray->getIndex(&globalObject, busIndex));
        unsigned numberOfChannels = busChannelCount(bus.get());
        if (!jsChannelsArray || jsChannelsArray->length() != numberOfChannels)
            return false;
        for (unsigned channelIndex = 0; channelIndex < numberOfChannels; ++channelIndex) {
            auto* channel = bus->channel(channelIndex);
            auto* jsChannelArray = jsDynamicCast<JSFloat32Array*>(jsChannelsArray->getIndex(&globalObject, channelIndex));
            if (!jsChannelArray || jsChannelArray->length() != channel->length())
                return false;
            memcpy(jsChannelArray->typedVector(), channel->mutableData(), sizeof(float) * jsChannelArray->length());
        }
    }
    return true;
}

static bool copyDataFromParameterMapToJSObject(VM& vm, JSGlobalObject& globalObject, const MemoryCompactLookupOnlyRobinHoodHashMap<String, std::unique_ptr<AudioFloatArray>>& paramValuesMap, JSObject* jsObject)
{
    if (!jsObject)
        return false;

    for (auto& pair : paramValuesMap) {
        auto* jsTypedArray = jsDynamicCast<JSFloat32Array*>(jsObject->get(&globalObject, Identifier::fromString(vm, pair.key)));
        if (!jsTypedArray)
            return false;
        unsigned expectedLength = pair.value->containsConstantValue() ? 1 : pair.value->size();
        if (jsTypedArray->length() != expectedLength)
            return false;
        memcpy(jsTypedArray->typedVector(), pair.value->data(), sizeof(float) * jsTypedArray->length());
    }
    return true;
}

static bool zeroJSArray(JSGlobalObject& globalObject, const Vector<Ref<AudioBus>>& outputs, JSArray* jsArray)
{
    if (!jsArray)
        return false;

    for (size_t busIndex = 0; busIndex < outputs.size(); ++busIndex) {
        auto& bus = outputs[busIndex];
        auto* jsChannelsArray = jsDynamicCast<JSArray*>(jsArray->getIndex(&globalObject, busIndex));
        unsigned numberOfChannels = busChannelCount(bus.get());
        if (!jsChannelsArray || jsChannelsArray->length() != numberOfChannels)
            return false;
        for (unsigned channelIndex = 0; channelIndex < numberOfChannels; ++channelIndex) {
            auto* channel = bus->channel(channelIndex);
            auto* jsChannelArray = jsDynamicCast<JSFloat32Array*>(jsChannelsArray->getIndex(&globalObject, channelIndex));
            if (!jsChannelArray || jsChannelArray->length() != channel->length())
                return false;
            memset(jsChannelArray->typedVector(), 0, sizeof(float) * jsChannelArray->length());
        }
    }
    return true;
}

ExceptionOr<Ref<AudioWorkletProcessor>> AudioWorkletProcessor::create(ScriptExecutionContext& context)
{
    auto& globalScope = downcast<AudioWorkletGlobalScope>(context);
    auto constructionData = globalScope.takePendingProcessorConstructionData();
    if (!constructionData)
        return Exception { ExceptionCode::TypeError, "No pending construction data for this worklet processor"_s };

    return adoptRef(*new AudioWorkletProcessor(globalScope, *constructionData));
}

AudioWorkletProcessor::~AudioWorkletProcessor() = default;

AudioWorkletProcessor::AudioWorkletProcessor(AudioWorkletGlobalScope& globalScope, const AudioWorkletProcessorConstructionData& constructionData)
    : m_globalScope(globalScope)
    , m_name(constructionData.name())
    , m_port(constructionData.port())
{
    ASSERT(!isMainThread());
}

void AudioWorkletProcessor::buildJSArguments(VM& vm, JSGlobalObject& globalObject, MarkedArgumentBuffer& args, const Vector<RefPtr<AudioBus>>& inputs, Vector<Ref<AudioBus>>& outputs, const MemoryCompactLookupOnlyRobinHoodHashMap<String, std::unique_ptr<AudioFloatArray>>& paramValuesMap)
{
    // For performance reasons, we cache the arrays passed to JS and reconstruct them only when the topology changes.
    if (!copyDataFromBusesToJSArray(globalObject, inputs, toJSArray(m_jsInputs)))
        m_jsInputs.setWeakly(constructFrozenJSArray(vm, globalObject, inputs, ShouldPopulateWithBusData::Yes));
    args.append(m_jsInputs.getValue());

    if (!zeroJSArray(globalObject, outputs, toJSArray(m_jsOutputs)))
        m_jsOutputs.setWeakly(constructFrozenJSArray(vm, globalObject, outputs, ShouldPopulateWithBusData::No));
    args.append(m_jsOutputs.getValue());

    if (!copyDataFromParameterMapToJSObject(vm, globalObject, paramValuesMap, toJSObject(m_jsParamValues)))
        m_jsParamValues.setWeakly(constructFrozenKeyValueObject(vm, globalObject, paramValuesMap));
    args.append(m_jsParamValues.getValue());
}

bool AudioWorkletProcessor::process(const Vector<RefPtr<AudioBus>>& inputs, Vector<Ref<AudioBus>>& outputs, const MemoryCompactLookupOnlyRobinHoodHashMap<String, std::unique_ptr<AudioFloatArray>>& paramValuesMap, bool& threwException)
{
    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

    ASSERT(wrapper());
    auto* globalObject = jsDynamicCast<JSDOMGlobalObject*>(m_globalScope.globalObject());
    if (UNLIKELY(!globalObject))
        return false;

    ASSERT(globalObject->scriptExecutionContext());
    ASSERT(globalObject->scriptExecutionContext()->isContextThread());

    auto& vm = globalObject->vm();
    JSLockHolder lock(vm);

    MarkedArgumentBuffer args;
    buildJSArguments(vm, *globalObject, args, inputs, outputs, paramValuesMap);
    ASSERT(!args.hasOverflowed());

    NakedPtr<JSC::Exception> returnedException;
    auto result = JSCallbackData::invokeCallback(*globalObject, wrapper(), jsUndefined(), args, JSCallbackData::CallbackType::Object, Identifier::fromString(vm, "process"_s), returnedException);
    if (returnedException) {
        reportException(globalObject, returnedException);
        threwException = true;
        return false;
    }

    copyDataFromJSArrayToBuses(*globalObject, *toJSArray(m_jsOutputs), outputs);

    return result.toBoolean(globalObject);
}

WebCoreOpaqueRoot root(AudioWorkletProcessor* processor)
{
    return WebCoreOpaqueRoot { processor };
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
