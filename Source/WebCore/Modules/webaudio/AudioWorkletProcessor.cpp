/*
 * Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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
#include "ScriptWrappableInlines.h"
#include "WebCoreOpaqueRoot.h"
#include <JavaScriptCore/JSTypedArrays.h>
#include <wtf/GetPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AudioWorkletProcessorConstructionData);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(AudioWorkletProcessor);

using namespace JSC;

static unsigned busChannelCount(const AudioBus& bus)
{
    return bus.numberOfChannels();
}

template<typename ArrayType>
static ArrayType* getArrayAtIndex(JSArray& jsArray, JSGlobalObject& globalObject, unsigned index)
{
    // We call getDirectIndex() instead of getIndex() since we only want to consider the values
    // we populated the array with, not the ones the worklet might have set on the Array prototype.
    auto item = jsArray.getDirectIndex(&globalObject, index);
    return item ? jsDynamicCast<ArrayType*>(item) : nullptr;
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

static JSFloat32Array* constructJSFloat32Array(VM& vm, JSGlobalObject& globalObject, unsigned length, std::span<const float> data = { })
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    constexpr bool isResizableOrGrowableShared = false;
    auto* jsArray = JSFloat32Array::create(&globalObject, globalObject.typedArrayStructure(TypeFloat32, isResizableOrGrowableShared), length);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!data.empty())
        memcpySpan(jsArray->typedSpan(), data.first(length));
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
        auto* array = constructJSFloat32Array(vm, globalObject, jsArraySize, pair.value->span());
        RETURN_IF_EXCEPTION(scope, nullptr);
        object->putInline(&globalObject, Identifier::fromString(vm, pair.key), array, slot);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }
    JSC::objectConstructorFreeze(&globalObject, object);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return object;
}

enum class ShouldPopulateWithBusData : bool { No, Yes };

template <typename T>
static JSArray* constructFrozenJSArray(VM& vm, JSGlobalObject& globalObject, const T& bus, ShouldPopulateWithBusData shouldPopulateWithBusData)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned numberOfChannels = busChannelCount(bus.get());
    auto* channelsData = JSArray::create(vm, globalObject.originalArrayStructureForIndexingType(ArrayWithContiguous), numberOfChannels);
    for (unsigned j = 0; j < numberOfChannels; ++j) {
        auto* channel = bus->channel(j);
        auto array = constructJSFloat32Array(vm, globalObject, channel->length(), shouldPopulateWithBusData == ShouldPopulateWithBusData::Yes ? channel->span() : std::span<const float> { });
        RETURN_IF_EXCEPTION(scope, nullptr);
        channelsData->setIndexQuickly(vm, j, array);
    }
    JSC::objectConstructorFreeze(&globalObject, channelsData);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return channelsData;
}

template <typename T>
static JSArray* constructFrozenJSArray(VM& vm, JSGlobalObject& globalObject, const Vector<T>& buses, ShouldPopulateWithBusData shouldPopulateWithBusData)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* array = JSArray::create(vm, globalObject.originalArrayStructureForIndexingType(ArrayWithContiguous), buses.size());
    for (unsigned i = 0; i < buses.size(); ++i) {
        auto* innerArray = constructFrozenJSArray(vm, globalObject, buses[i], shouldPopulateWithBusData);
        RETURN_IF_EXCEPTION(scope, nullptr);
        array->setIndexQuickly(vm, i, innerArray);
    }
    JSC::objectConstructorFreeze(&globalObject, array);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return array;
}

static void copyDataFromJSArrayToBuses(VM& vm, JSGlobalObject& globalObject, JSArray& jsArray, Vector<Ref<AudioBus>>& buses)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // We can safely make assumptions about the structure of the JSArray since we use frozen arrays.
    for (unsigned i = 0; i < buses.size(); ++i) {
        auto& bus = buses[i];
        auto* channelsArray = getArrayAtIndex<JSArray>(jsArray, globalObject, i);
        RETURN_IF_EXCEPTION(scope, void());
        if (!channelsArray) [[unlikely]] {
            bus->zero();
            continue;
        }
        for (unsigned j = 0; j < bus->numberOfChannels(); ++j) {
            auto* channel = bus->channel(j);
            auto* jsChannelData = getArrayAtIndex<JSFloat32Array>(*channelsArray, globalObject, j);
            RETURN_IF_EXCEPTION(scope, void());
            if (jsChannelData && !jsChannelData->isShared() && jsChannelData->length() == channel->length()) [[likely]]
                memcpySpan(channel->mutableSpan(), jsChannelData->typedSpan().first(channel->length()));
            else
                channel->zero();
        }
    }
}

static bool copyDataFromBusesToJSArray(VM& vm, JSGlobalObject& globalObject, const Vector<RefPtr<AudioBus>>& buses, JSArray* jsArray)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!jsArray)
        return false;

    for (size_t busIndex = 0; busIndex < buses.size(); ++busIndex) {
        auto& bus = buses[busIndex];
        auto* jsChannelsArray = getArrayAtIndex<JSArray>(*jsArray, globalObject, busIndex);
        RETURN_IF_EXCEPTION(scope, false);
        unsigned numberOfChannels = busChannelCount(bus.get());
        if (!jsChannelsArray || jsChannelsArray->length() != numberOfChannels)
            return false;
        for (unsigned channelIndex = 0; channelIndex < numberOfChannels; ++channelIndex) {
            auto* channel = bus->channel(channelIndex);
            auto* jsChannelArray = getArrayAtIndex<JSFloat32Array>(*jsChannelsArray, globalObject, channelIndex);
            RETURN_IF_EXCEPTION(scope, false);
            if (!jsChannelArray || jsChannelArray->isShared() || jsChannelArray->length() != channel->length())
                return false;
            memcpySpan(jsChannelArray->typedSpan(), channel->mutableSpan().first(jsChannelArray->length()));
        }
    }
    return true;
}

static bool copyDataFromParameterMapToJSObject(VM& vm, JSGlobalObject& globalObject, const MemoryCompactLookupOnlyRobinHoodHashMap<String, std::unique_ptr<AudioFloatArray>>& paramValuesMap, JSObject* jsObject)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!jsObject)
        return false;

    for (auto& pair : paramValuesMap) {
        auto* jsTypedArray = jsDynamicCast<JSFloat32Array*>(jsObject->get(&globalObject, Identifier::fromString(vm, pair.key)));
        RETURN_IF_EXCEPTION(scope, false);
        if (!jsTypedArray)
            return false;
        unsigned expectedLength = pair.value->containsConstantValue() ? 1 : pair.value->size();
        if (jsTypedArray->length() != expectedLength)
            return false;
        memcpySpan(jsTypedArray->typedSpan(), pair.value->span().first(jsTypedArray->length()));
    }
    return true;
}

static bool zeroJSArray(VM& vm, JSGlobalObject& globalObject, const Vector<Ref<AudioBus>>& outputs, JSArray* jsArray)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!jsArray)
        return false;

    for (size_t busIndex = 0; busIndex < outputs.size(); ++busIndex) {
        auto& bus = outputs[busIndex];
        auto* jsChannelsArray = getArrayAtIndex<JSArray>(*jsArray, globalObject, busIndex);
        RETURN_IF_EXCEPTION(scope, false);
        unsigned numberOfChannels = busChannelCount(bus.get());
        if (!jsChannelsArray || jsChannelsArray->length() != numberOfChannels)
            return false;
        for (unsigned channelIndex = 0; channelIndex < numberOfChannels; ++channelIndex) {
            auto* channel = bus->channel(channelIndex);
            auto* jsChannelArray = getArrayAtIndex<JSFloat32Array>(*jsChannelsArray, globalObject, channelIndex);
            RETURN_IF_EXCEPTION(scope, false);
            if (!jsChannelArray || jsChannelArray->isShared() || jsChannelArray->length() != channel->length())
                return false;
            zeroSpan(jsChannelArray->typedSpan());
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
    auto scope = DECLARE_THROW_SCOPE(vm);
    // For performance reasons, we cache the arrays passed to JS and reconstruct them only when the topology changes.
    bool success = copyDataFromBusesToJSArray(vm, globalObject, inputs, toJSArray(m_jsInputs));
    RETURN_IF_EXCEPTION(scope, void());
    if (!success) {
        auto* array = constructFrozenJSArray(vm, globalObject, inputs, ShouldPopulateWithBusData::Yes);
        RETURN_IF_EXCEPTION(scope, void());
        m_jsInputs.setWeakly(array);
    }
    args.append(m_jsInputs.getValue());

    success = zeroJSArray(vm, globalObject, outputs, toJSArray(m_jsOutputs));
    RETURN_IF_EXCEPTION(scope, void());
    if (!success) {
        auto* array = constructFrozenJSArray(vm, globalObject, outputs, ShouldPopulateWithBusData::No);
        RETURN_IF_EXCEPTION(scope, void());
        m_jsOutputs.setWeakly(array);
    }
    args.append(m_jsOutputs.getValue());

    success = copyDataFromParameterMapToJSObject(vm, globalObject, paramValuesMap, toJSObject(m_jsParamValues));
    RETURN_IF_EXCEPTION(scope, void());
    if (!success)
        m_jsParamValues.setWeakly(constructFrozenKeyValueObject(vm, globalObject, paramValuesMap));

    args.append(m_jsParamValues.getValue());
}

bool AudioWorkletProcessor::process(const Vector<RefPtr<AudioBus>>& inputs, Vector<Ref<AudioBus>>& outputs, const MemoryCompactLookupOnlyRobinHoodHashMap<String, std::unique_ptr<AudioFloatArray>>& paramValuesMap, bool& threwException)
{
    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

    ASSERT(wrapper());
    auto* globalObject = jsDynamicCast<JSDOMGlobalObject*>(m_globalScope->globalObject());
    if (!globalObject) [[unlikely]]
        return false;

    ASSERT(globalObject->scriptExecutionContext());
    ASSERT(globalObject->scriptExecutionContext()->isContextThread());

    auto& vm = globalObject->vm();
    JSLockHolder lock(vm);

    auto scope = DECLARE_THROW_SCOPE(vm);
    MarkedArgumentBuffer args;
    buildJSArguments(vm, *globalObject, args, inputs, outputs, paramValuesMap);
    ASSERT(!args.hasOverflowed());
    if (scope.exception()) [[unlikely]] {
        reportException(globalObject, scope.exception());
        threwException = true;
        return false;
    }

    NakedPtr<JSC::Exception> returnedException;
    auto result = JSCallbackData::invokeCallback(*globalObject, wrapper(), jsUndefined(), args, JSCallbackData::CallbackType::Object, Identifier::fromString(vm, "process"_s), returnedException);
    if (returnedException) {
        reportException(globalObject, returnedException);
        threwException = true;
        return false;
    }

    copyDataFromJSArrayToBuses(vm, *globalObject, *toJSArray(m_jsOutputs), outputs);

    return result.toBoolean(globalObject);
}

WebCoreOpaqueRoot root(AudioWorkletProcessor* processor)
{
    return WebCoreOpaqueRoot { processor };
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
