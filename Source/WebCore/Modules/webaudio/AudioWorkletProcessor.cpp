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
#include "AudioWorkletProcessor.h"

#include "AudioBus.h"
#include "AudioChannel.h"
#include "AudioWorkletGlobalScope.h"
#include "AudioWorkletProcessorConstructionData.h"
#include "JSCallbackData.h"
#include "JSDOMExceptionHandling.h"
#include "MessagePort.h"
#include <JavaScriptCore/JSTypedArrays.h>
#include <wtf/GetPtr.h>

namespace WebCore {

using namespace JSC;

static JSFloat32Array* constructJSFloat32Array(JSGlobalObject& globalObject, unsigned length, const float* data = nullptr)
{
    auto* jsArray = JSFloat32Array::create(&globalObject, globalObject.typedArrayStructure(TypeFloat32), length);
    if (data)
        memcpy(jsArray->typedVector(), data, sizeof(float) * length);
    return jsArray;
}

static JSObject* constructFrozenKeyValueObject(VM& vm, JSGlobalObject& globalObject, const HashMap<String, std::unique_ptr<AudioFloatArray>>& paramValuesMap)
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
static JSArray* constructFrozenJSArray(VM& vm, JSGlobalObject& globalObject, JSC::ThrowScope& scope, AudioBus* bus, ShouldPopulateWithBusData shouldPopulateWithBusData)
{
    unsigned numberOfChannels = bus ? bus->numberOfChannels() : 0;
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
        array->setIndexQuickly(vm, i, constructFrozenJSArray(vm, globalObject, scope, WTF::getPtr(buses[i]), shouldPopulateWithBusData));
    JSC::objectConstructorFreeze(&globalObject, array);
    EXCEPTION_ASSERT(!scope.exception());
    return array;
}

static void copyDataFromJSArrayToBuses(JSGlobalObject& globalObject, JSArray& jsArray, Vector<Ref<AudioBus>>& buses)
{
    // We can safely make assumptions about the structure of the JSArray since we use frozen arrays.
    for (unsigned i = 0; i < buses.size(); ++i) {
        auto& bus = buses[i];
        auto* channelsArray = jsCast<JSArray*>(jsArray.getIndex(&globalObject, i));
        for (unsigned j = 0; j < bus->numberOfChannels(); ++j) {
            auto* channel = bus->channel(j);
            auto* jsChannelData = jsCast<JSFloat32Array*>(channelsArray->getIndex(&globalObject, j));
            if (jsChannelData->length() == channel->length())
                memcpy(channel->mutableData(), jsChannelData->typedVector(), sizeof(float) * channel->length());
            else
                channel->zero();
        }
    }
}

ExceptionOr<Ref<AudioWorkletProcessor>> AudioWorkletProcessor::create(ScriptExecutionContext& context)
{
    auto constructionData = downcast<AudioWorkletGlobalScope>(context).takePendingProcessorConstructionData();
    if (!constructionData)
        return Exception { TypeError, "No pending construction data for this worklet processor"_s };

    return adoptRef(*new AudioWorkletProcessor(*constructionData));
}

AudioWorkletProcessor::~AudioWorkletProcessor() = default;

AudioWorkletProcessor::AudioWorkletProcessor(const AudioWorkletProcessorConstructionData& constructionData)
    : m_name(constructionData.name())
    , m_port(constructionData.port())
{
    ASSERT(!isMainThread());
}

bool AudioWorkletProcessor::process(const Vector<RefPtr<AudioBus>>& inputs, Vector<Ref<AudioBus>>& outputs, const HashMap<String, std::unique_ptr<AudioFloatArray>>& paramValuesMap, bool& threwException)
{
    ASSERT(m_processCallback);
    auto& globalObject = *m_processCallback->globalObject();
    ASSERT(globalObject.scriptExecutionContext());
    ASSERT(globalObject.scriptExecutionContext()->isContextThread());

    auto& vm = globalObject.vm();
    JSLockHolder lock(vm);

    MarkedArgumentBuffer args;
    // FIXME: We should consider caching the JSArrays & JSObject and only update their items
    // every time process() is called, for performance reasons.
    args.append(constructFrozenJSArray(vm, globalObject, inputs, ShouldPopulateWithBusData::Yes));
    auto* ouputJSArray = constructFrozenJSArray(vm, globalObject, outputs, ShouldPopulateWithBusData::No);
    args.append(ouputJSArray);
    args.append(constructFrozenKeyValueObject(vm, globalObject, paramValuesMap));

    NakedPtr<JSC::Exception> returnedException;
    auto result = m_processCallback->invokeCallback(jsUndefined(), args, JSCallbackData::CallbackType::Object, Identifier::fromString(vm, "process"), returnedException);
    if (returnedException) {
        reportException(&globalObject, returnedException);
        threwException = true;
        return false;
    }

    copyDataFromJSArrayToBuses(globalObject, *ouputJSArray, outputs);

    return result.toBoolean(&globalObject);
}

void AudioWorkletProcessor::setProcessCallback(std::unique_ptr<JSCallbackDataStrong>&& processCallback)
{
    m_processCallback = WTFMove(processCallback);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
