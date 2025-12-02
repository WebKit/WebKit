/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 * Copyright (C) 2024 Sosuke Suzuki <aosukeke@gmail.com>.
 * Copyright (C) 2024 Tetsuharu Ohzeki <tetsuharu.ohzeki@gmail.com>.
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
#include "JSGlobalObject.h"

#include "AggregateError.h"
#include "AggregateErrorConstructorInlines.h"
#include "AggregateErrorPrototypeInlines.h"
#include "ArrayConstructor.h"
#include "ArrayConstructorInlines.h"
#include "ArrayIteratorPrototypeInlines.h"
#include "ArrayPrototypeInlines.h"
#include "AsyncDisposableStackConstructor.h"
#include "AsyncDisposableStackPrototype.h"
#include "AsyncDisposableStackPrototypeInlines.h"
#include "AsyncFromSyncIteratorPrototypeInlines.h"
#include "AsyncFunctionConstructorInlines.h"
#include "AsyncFunctionPrototypeInlines.h"
#include "AsyncGeneratorFunctionConstructor.h"
#include "AsyncGeneratorFunctionPrototypeInlines.h"
#include "AsyncGeneratorPrototypeInlines.h"
#include "AsyncIteratorPrototypeInlines.h"
#include "AtomicsObject.h"
#include "BigIntConstructorInlines.h"
#include "BigIntObjectInlines.h"
#include "BigIntPrototypeInlines.h"
#include "BooleanConstructorInlines.h"
#include "BooleanObjectInlines.h"
#include "BooleanPrototypeInlines.h"
#include "BuiltinNames.h"
#include "CatchScope.h"
#include "ChainedWatchpoint.h"
#include "ClonedArguments.h"
#include "CodeBlock.h"
#include "CodeBlockSetInlines.h"
#include "ConsoleClient.h"
#include "ConsoleObjectInlines.h"
#include "DateConstructorInlines.h"
#include "DateInstanceInlines.h"
#include "DatePrototypeInlines.h"
#include "Debugger.h"
#include "DebuggerScope.h"
#include "DeferTermination.h"
#include "DirectArguments.h"
#include "DisposableStackConstructor.h"
#include "DisposableStackPrototype.h"
#include "DisposableStackPrototypeInlines.h"
#include "ErrorConstructorInlines.h"
#include "ErrorInstanceInlines.h"
#include "ErrorPrototypeInlines.h"
#include "FinalizationRegistryConstructorInlines.h"
#include "FinalizationRegistryPrototypeInlines.h"
#include "FunctionConstructorInlines.h"
#include "FunctionPrototypeInlines.h"
#include "GeneratorFunctionConstructorInlines.h"
#include "GeneratorFunctionPrototypeInlines.h"
#include "GeneratorPrototypeInlines.h"
#include "GetterSetter.h"
#include "GlobalObjectMethodTable.h"
#include "HeapIterationScope.h"
#include "ImportMap.h"
#include "IntlCollator.h"
#include "IntlCollatorPrototype.h"
#include "IntlDateTimeFormat.h"
#include "IntlDateTimeFormatConstructor.h"
#include "IntlDateTimeFormatPrototype.h"
#include "IntlDisplayNames.h"
#include "IntlDisplayNamesPrototype.h"
#include "IntlDurationFormat.h"
#include "IntlDurationFormatPrototype.h"
#include "IntlListFormat.h"
#include "IntlListFormatPrototype.h"
#include "IntlLocale.h"
#include "IntlLocalePrototype.h"
#include "IntlNumberFormat.h"
#include "IntlNumberFormatConstructor.h"
#include "IntlNumberFormatPrototype.h"
#include "IntlObject.h"
#include "IntlPluralRules.h"
#include "IntlPluralRulesPrototype.h"
#include "IntlRelativeTimeFormat.h"
#include "IntlRelativeTimeFormatPrototype.h"
#include "IntlSegmentIterator.h"
#include "IntlSegmentIteratorPrototype.h"
#include "IntlSegmenter.h"
#include "IntlSegmenterPrototype.h"
#include "IntlSegments.h"
#include "IntlSegmentsPrototype.h"
#include "JSAPIWrapperObject.h"
#include "JSArrayBuffer.h"
#include "JSArrayBufferConstructor.h"
#include "JSArrayBufferPrototype.h"
#include "JSArrayIterator.h"
#include "JSAsyncDisposableStack.h"
#include "JSAsyncDisposableStackInlines.h"
#include "JSAsyncFromSyncIterator.h"
#include "JSAsyncFromSyncIteratorInlines.h"
#include "JSAsyncFunctionInlines.h"
#include "JSAsyncGenerator.h"
#include "JSAsyncGeneratorFunctionInlines.h"
#include "JSBoundFunctionInlines.h"
#include "JSCallbackConstructor.h"
#include "JSCallbackFunction.h"
#include "JSCallbackObject.h"
#include "JSCalleeInlines.h"
#include "JSCast.h"
#include "JSCustomGetterFunctionInlines.h"
#include "JSCustomSetterFunctionInlines.h"
#include "JSDataView.h"
#include "JSDataViewPrototype.h"
#include "JSDisposableStack.h"
#include "JSDisposableStackInlines.h"
#include "JSDollarVM.h"
#include "JSFinalizationRegistry.h"
#include "JSFunction.h"
#include "JSFunctionWithFields.h"
#include "JSGenerator.h"
#include "JSGeneratorFunctionInlines.h"
#include "JSGenericTypedArrayViewConstructorInlines.h"
#include "JSGenericTypedArrayViewInlines.h"
#include "JSGenericTypedArrayViewPrototypeInlines.h"
#include "JSGlobalLexicalEnvironmentInlines.h"
#include "JSGlobalObjectFunctions.h"
#include "JSGlobalObjectInlines.h"
#include "JSGlobalProxyInlines.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseConstructor.h"
#include "JSInternalPromisePrototype.h"
#include "JSIterator.h"
#include "JSIteratorConstructor.h"
#include "JSIteratorHelper.h"
#include "JSIteratorHelperPrototypeInlines.h"
#include "JSIteratorPrototype.h"
#include "JSIteratorPrototypeInlines.h"
#include "JSLexicalEnvironmentInlines.h"
#include "JSMapInlines.h"
#include "JSMapIteratorInlines.h"
#include "JSMicrotask.h"
#include "JSModuleEnvironmentInlines.h"
#include "JSModuleLoaderInlines.h"
#include "JSModuleNamespaceObjectInlines.h"
#include "JSModuleRecord.h"
#include "JSNativeStdFunctionInlines.h"
#include "JSONObjectInlines.h"
#include "JSPromise.h"
#include "JSPromiseConstructor.h"
#include "JSPromisePrototype.h"
#include "JSRawJSONObject.h"
#include "JSRegExpStringIteratorInlines.h"
#include "JSRemoteFunctionInlines.h"
#include "JSSetInlines.h"
#include "JSSetIteratorInlines.h"
#include "JSStringIteratorInlines.h"
#include "JSTypedArrayConstructors.h"
#include "JSTypedArrayPrototypes.h"
#include "JSTypedArrayViewConstructor.h"
#include "JSTypedArrayViewPrototype.h"
#include "JSTypedArrays.h"
#include "JSWeakMapInlines.h"
#include "JSWeakObjectRefInlines.h"
#include "JSWeakSetInlines.h"
#include "JSWebAssembly.h"
#include "JSWebAssemblyArrayInlines.h"
#include "JSWebAssemblyCompileError.h"
#include "JSWebAssemblyException.h"
#include "JSWebAssemblyGlobal.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyLinkError.h"
#include "JSWebAssemblyMemory.h"
#include "JSWebAssemblyModule.h"
#include "JSWebAssemblyRuntimeError.h"
#include "JSWebAssemblyStruct.h"
#include "JSWebAssemblyTable.h"
#include "JSWebAssemblyTag.h"
#include "JSWithScope.h"
#include "JSWrapForValidIteratorInlines.h"
#include "JSPromiseAllContextInlines.h"
#include "JSPromiseAllGlobalContext.h"
#include "JSPromiseReaction.h"
#include "LazyClassStructureInlines.h"
#include "LazyPropertyInlines.h"
#include "LinkTimeConstant.h"
#include "MapConstructorInlines.h"
#include "MapIteratorPrototypeInlines.h"
#include "MapPrototypeInlines.h"
#include "MarkedSpaceInlines.h"
#include "MathObjectInlines.h"
#include "NativeErrorConstructorInlines.h"
#include "NativeErrorPrototypeInlines.h"
#include "NullGetterFunctionInlines.h"
#include "NullSetterFunctionInlines.h"
#include "NumberConstructorInlines.h"
#include "NumberPrototypeInlines.h"
#include "ObjCCallbackFunction.h"
#include "ObjectAdaptiveStructureWatchpoint.h"
#include "ObjectConstructorInlines.h"
#include "ObjectPropertyChangeAdaptiveWatchpoint.h"
#include "ObjectPropertyConditionSet.h"
#include "ObjectPrototypeInlines.h"
#include "ProfilerSupport.h"
#include "ProxyConstructorInlines.h"
#include "ProxyObjectInlines.h"
#include "ProxyRevokeInlines.h"
#include "ReflectObjectInlines.h"
#include "RegExpConstructorInlines.h"
#include "RegExpGlobalDataInlines.h"
#include "RegExpMatchesArray.h"
#include "RegExpObjectInlines.h"
#include "RegExpPrototypeInlines.h"
#include "RegExpStringIteratorPrototypeInlines.h"
#include "SamplingProfiler.h"
#include "ScopedArguments.h"
#include "SetConstructorInlines.h"
#include "SetIteratorPrototypeInlines.h"
#include "SetPrototypeInlines.h"
#include "ShadowRealmConstructorInlines.h"
#include "ShadowRealmObjectInlines.h"
#include "ShadowRealmPrototypeInlines.h"
#include "SourceCodeKey.h"
#include "StrictEvalActivationInlines.h"
#include "StringConstructorInlines.h"
#include "StringIteratorPrototypeInlines.h"
#include "StringObjectInlines.h"
#include "StringPrototypeInlines.h"
#include "SuppressedError.h"
#include "SuppressedErrorConstructorInlines.h"
#include "SuppressedErrorPrototypeInlines.h"
#include "SymbolConstructorInlines.h"
#include "SymbolObjectInlines.h"
#include "SymbolPrototypeInlines.h"
#include "SyntheticModuleRecord.h"
#include "TemporalCalendar.h"
#include "TemporalCalendarPrototype.h"
#include "TemporalDuration.h"
#include "TemporalDurationPrototype.h"
#include "TemporalInstant.h"
#include "TemporalInstantPrototype.h"
#include "TemporalObject.h"
#include "TemporalPlainDate.h"
#include "TemporalPlainDatePrototype.h"
#include "TemporalPlainDateTime.h"
#include "TemporalPlainDateTimePrototype.h"
#include "TemporalPlainMonthDay.h"
#include "TemporalPlainMonthDayPrototype.h"
#include "TemporalPlainTime.h"
#include "TemporalPlainTimePrototype.h"
#include "TemporalPlainYearMonth.h"
#include "TemporalPlainYearMonthPrototype.h"
#include "TemporalTimeZone.h"
#include "TemporalTimeZonePrototype.h"
#include "VMTrapsInlines.h"
#include "WaiterListManager.h"
#include "WasmCapabilities.h"
#include "WeakMapConstructorInlines.h"
#include "WeakMapPrototypeInlines.h"
#include "WeakObjectRefConstructorInlines.h"
#include "WeakObjectRefPrototypeInlines.h"
#include "WeakSetConstructorInlines.h"
#include "WeakSetPrototypeInlines.h"
#include "WebAssemblyArrayConstructor.h"
#include "WebAssemblyArrayPrototype.h"
#include "WebAssemblyCompileErrorConstructor.h"
#include "WebAssemblyCompileErrorPrototype.h"
#include "WebAssemblyExceptionConstructor.h"
#include "WebAssemblyExceptionPrototype.h"
#include "WebAssemblyFunction.h"
#include "WebAssemblyGlobalConstructor.h"
#include "WebAssemblyGlobalPrototype.h"
#include "WebAssemblyInstanceConstructor.h"
#include "WebAssemblyInstancePrototype.h"
#include "WebAssemblyLinkErrorConstructor.h"
#include "WebAssemblyLinkErrorPrototype.h"
#include "WebAssemblyMemoryConstructor.h"
#include "WebAssemblyMemoryPrototype.h"
#include "WebAssemblyModuleConstructor.h"
#include "WebAssemblyModulePrototype.h"
#include "WebAssemblyModuleRecord.h"
#include "WebAssemblyRuntimeErrorConstructor.h"
#include "WebAssemblyRuntimeErrorPrototype.h"
#include "WebAssemblyStructConstructor.h"
#include "WebAssemblyStructPrototype.h"
#include "WebAssemblyTableConstructor.h"
#include "WebAssemblyTablePrototype.h"
#include "WebAssemblyTagConstructor.h"
#include "WebAssemblyTagPrototype.h"
#include "WrapForValidIteratorPrototypeInlines.h"
#include "runtime/VM.h"
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/FileHandle.h>
#include <wtf/FixedVector.h>
#include <wtf/SystemTracing.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/MakeString.h>

#if ENABLE(REMOTE_INSPECTOR)
#include "JSGlobalObjectDebuggable.h"
#include "JSGlobalObjectInspectorController.h"
#endif

#ifdef JSC_GLIB_API_ENABLED
#include "JSCCallbackFunction.h"
#include "JSCWrapperMap.h"
#endif

namespace JSC {

#define CHECK_FEATURE_FLAG_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
static_assert(std::is_same_v<std::remove_cv_t<decltype(featureFlag)>, bool> || std::is_same_v<std::remove_cv_t<decltype(featureFlag)>, bool&>);

FOR_EACH_SIMPLE_BUILTIN_TYPE(CHECK_FEATURE_FLAG_TYPE)
FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(CHECK_FEATURE_FLAG_TYPE)
FOR_EACH_LAZY_BUILTIN_TYPE(CHECK_FEATURE_FLAG_TYPE)

static JSC_DECLARE_HOST_FUNCTION(createPrivateSymbol);
static JSC_DECLARE_HOST_FUNCTION(jsonParse);
static JSC_DECLARE_HOST_FUNCTION(jsonStringify);
static JSC_DECLARE_HOST_FUNCTION(enableSuperSampler);
static JSC_DECLARE_HOST_FUNCTION(disableSuperSampler);
static JSC_DECLARE_HOST_FUNCTION(resolvePromise);
static JSC_DECLARE_HOST_FUNCTION(rejectPromise);
static JSC_DECLARE_HOST_FUNCTION(fulfillPromise);
static JSC_DECLARE_HOST_FUNCTION(resolveWithoutPromise);
static JSC_DECLARE_HOST_FUNCTION(rejectWithoutPromise);
static JSC_DECLARE_HOST_FUNCTION(fulfillWithoutPromise);
static JSC_DECLARE_HOST_FUNCTION(resolvePromiseWithFirstResolvingFunctionCallCheck);
static JSC_DECLARE_HOST_FUNCTION(rejectPromiseWithFirstResolvingFunctionCallCheck);
static JSC_DECLARE_HOST_FUNCTION(fulfillPromiseWithFirstResolvingFunctionCallCheck);
static JSC_DECLARE_HOST_FUNCTION(resolveWithoutPromiseForAsyncAwait);
static JSC_DECLARE_HOST_FUNCTION(driveAsyncFunction);
static JSC_DECLARE_HOST_FUNCTION(awaitValue);
static JSC_DECLARE_HOST_FUNCTION(newHandledRejectedPromise);
static JSC_DECLARE_HOST_FUNCTION(promiseOnRejectedWithContext);
static JSC_DECLARE_HOST_FUNCTION(promiseAllOnFulfilled);
static JSC_DECLARE_HOST_FUNCTION(promiseEmptyOnFulfilled);
static JSC_DECLARE_HOST_FUNCTION(promiseEmptyOnRejected);
static JSC_DECLARE_HOST_FUNCTION(promiseResolve);
static JSC_DECLARE_HOST_FUNCTION(promiseReject);
static JSC_DECLARE_HOST_FUNCTION(performPromiseThen);
#if ASSERT_ENABLED
static JSC_DECLARE_HOST_FUNCTION(assertCall);
#endif
#if ENABLE(SAMPLING_PROFILER)
static JSC_DECLARE_HOST_FUNCTION(enableSamplingProfiler);
static JSC_DECLARE_HOST_FUNCTION(disableSamplingProfiler);
static JSC_DECLARE_HOST_FUNCTION(dumpAndClearSamplingProfilerSamples);
#endif

static JSC_DECLARE_HOST_FUNCTION(tracePointStart);
static JSC_DECLARE_HOST_FUNCTION(tracePointStop);
static JSC_DECLARE_HOST_FUNCTION(signpostStart);
static JSC_DECLARE_HOST_FUNCTION(signpostStop);

static JSValue initializeEvalFunction(VM&, JSObject* object)
{
    return jsCast<JSGlobalObject*>(object)->evalFunction();
}

static JSValue createProxyProperty(VM& vm, JSObject* object)
{
    JSGlobalObject* global = jsCast<JSGlobalObject*>(object);
    return ProxyConstructor::create(vm, ProxyConstructor::createStructure(vm, global, global->functionPrototype()));
}

static JSValue createJSONProperty(VM& vm, JSObject* object)
{
    JSGlobalObject* global = jsCast<JSGlobalObject*>(object);
    return JSONObject::create(vm, global, JSONObject::createStructure(vm, global, global->objectPrototype()));
}

static JSValue createMathProperty(VM& vm, JSObject* object)
{
    JSGlobalObject* global = jsCast<JSGlobalObject*>(object);
    return MathObject::create(vm, global, MathObject::createStructure(vm, global, global->objectPrototype()));
}

static JSValue createReflectProperty(VM& vm, JSObject* object)
{
    JSGlobalObject* global = jsCast<JSGlobalObject*>(object);
    return ReflectObject::create(vm, global, ReflectObject::createStructure(vm, global, global->objectPrototype()));
}

static JSValue createAtomicsProperty(VM& vm, JSObject *object)
{
    JSGlobalObject* global = jsCast<JSGlobalObject*>(object);
    return AtomicsObject::create(vm, global, AtomicsObject::createStructure(vm, global, global->objectPrototype()));
}

static JSValue createConsoleProperty(VM& vm, JSObject* object)
{
    JSGlobalObject* global = jsCast<JSGlobalObject*>(object);
    return ConsoleObject::create(vm, global, ConsoleObject::createStructure(vm, global, constructEmptyObject(global)));
}

// FIXME: use a bytecode or intrinsic for creating a private symbol.
// https://bugs.webkit.org/show_bug.cgi?id=212782
JSC_DEFINE_HOST_FUNCTION(createPrivateSymbol, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto description = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(Symbol::create(vm, PrivateSymbolImpl::create(*description.impl()).get()));
}

JSC_DEFINE_HOST_FUNCTION(jsonParse, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto json = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    return JSValue::encode(JSONParse(globalObject, json));
}

JSC_DEFINE_HOST_FUNCTION(jsonStringify, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto result = JSONStringify(globalObject, callFrame->argument(0), callFrame->argument(1));
    return JSValue::encode(jsString(vm, result));
}

#if ASSERT_ENABLED
JSC_DEFINE_HOST_FUNCTION(assertCall, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    RELEASE_ASSERT(callFrame->argument(0).isBoolean());
    if (callFrame->argument(0).asBoolean())
        return JSValue::encode(jsUndefined());

    bool iteratedOnce = false;
    CodeBlock* codeBlock = nullptr;
    LineColumn lineColumn;
    StackVisitor::visit(callFrame, globalObject->vm(), [&] (StackVisitor& visitor) {
        if (!iteratedOnce) {
            iteratedOnce = true;
            return IterationStatus::Continue;
        }

        RELEASE_ASSERT(visitor->hasLineAndColumnInfo());
        lineColumn = visitor->computeLineAndColumn();
        codeBlock = visitor->codeBlock();
        return IterationStatus::Done;
    });
    RELEASE_ASSERT(!!codeBlock);
    RELEASE_ASSERT_WITH_MESSAGE(false, "JS assertion failed at line %u in:\n%s\n", lineColumn.line, codeBlock->sourceCodeForTools().data());
    return JSValue::encode(jsUndefined());
}
#endif // ASSERT_ENABLED

#if ENABLE(SAMPLING_PROFILER)
JSC_DEFINE_HOST_FUNCTION(enableSamplingProfiler, (JSGlobalObject* globalObject, CallFrame*))
{
    globalObject->vm().enableSamplingProfiler();
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(disableSamplingProfiler, (JSGlobalObject* globalObject, CallFrame*))
{
    globalObject->vm().disableSamplingProfiler();
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(dumpAndClearSamplingProfilerSamples, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue argument = callFrame->argument(0);
    auto filenamePrefix = emptyString();
    if (!argument.isUndefinedOrNull()) {
        filenamePrefix = argument.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
    }

    auto json = vm.takeSamplingProfilerSamplesAsJSON();
    if (!json) [[unlikely]]
        return JSValue::encode(jsUndefined());

    auto jsonData = json->toJSONString();
    {
        auto [tempFilePath, fileHandle] = FileSystem::openTemporaryFile(filenamePrefix);
        if (!fileHandle) {
            dataLogLn("Dumping sampling profiler samples failed to open temporary file");
            return JSValue::encode(jsUndefined());
        }

        CString utf8String = jsonData.utf8();

        fileHandle.write(byteCast<uint8_t>(utf8String.span()));
        dataLogLn("Dumped sampling profiler samples to ", tempFilePath);
    }

    return JSValue::encode(jsUndefined());
}
#endif

static uint64_t asTracePointInt(JSGlobalObject* globalObject, JSValue v)
{
    if (v.isUndefined())
        return 0;
    return v.toNumber(globalObject);
}

JSC_DEFINE_HOST_FUNCTION(tracePointStart, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto getValue = [&] (unsigned arg) {
        JSValue v = callFrame->argument(arg);
        return asTracePointInt(globalObject, v);
    };

    uint64_t one = getValue(0);
    RETURN_IF_EXCEPTION(scope, EncodedJSValue());
    uint64_t two = getValue(1);
    RETURN_IF_EXCEPTION(scope, EncodedJSValue());
    uint64_t three = getValue(2);
    RETURN_IF_EXCEPTION(scope, EncodedJSValue());
    uint64_t four = getValue(3);
    RETURN_IF_EXCEPTION(scope, EncodedJSValue());

    tracePoint(TracePointCode::FromJSStart, one, two, three, four);

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(tracePointStop, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto getValue = [&] (unsigned arg) {
        JSValue v = callFrame->argument(arg);
        return asTracePointInt(globalObject, v);
    };

    uint64_t one = getValue(0);
    RETURN_IF_EXCEPTION(scope, EncodedJSValue());
    uint64_t two = getValue(1);
    RETURN_IF_EXCEPTION(scope, EncodedJSValue());
    uint64_t three = getValue(2);
    RETURN_IF_EXCEPTION(scope, EncodedJSValue());
    uint64_t four = getValue(3);
    RETURN_IF_EXCEPTION(scope, EncodedJSValue());

    tracePoint(TracePointCode::FromJSStop, one, two, three, four);
    return JSValue::encode(jsUndefined());
}

std::atomic<unsigned> activeJSGlobalObjectSignpostIntervalCount { 0 };

static String asSignpostString(JSGlobalObject* globalObject, JSValue v)
{
    if (v.isUndefined())
        return emptyString();
    return v.toWTFString(globalObject);
}

JSC_DEFINE_HOST_FUNCTION(signpostStart, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto message = asSignpostString(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, EncodedJSValue());

    globalObject->startSignpost(WTFMove(message));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(signpostStop, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto message = asSignpostString(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, EncodedJSValue());

    globalObject->stopSignpost(WTFMove(message));
    return JSValue::encode(jsUndefined());
}

void JSGlobalObject::startSignpost(String&& message)
{
    ++activeJSGlobalObjectSignpostIntervalCount;
    auto* identifier = std::bit_cast<void*>(static_cast<uintptr_t>(m_signposts.ensure(message, [] {
        return JSCJSGlobalObjectSignpostIdentifier::generate();
    }).iterator->value.toUInt64()));
    UNUSED_VARIABLE(identifier);
    auto string = message.ascii();
    WTFBeginSignpostAlways(identifier, JSCJSGlobalObject, "%" PUBLIC_LOG_STRING, string.data());
    ProfilerSupport::markStart(identifier, ProfilerSupport::Category::JSGlobalObjectSignpost, WTFMove(string));
}

void JSGlobalObject::stopSignpost(String&& message)
{
    void* identifier = std::bit_cast<void*>(this);
    if (auto stored = m_signposts.takeOptional(message))
        identifier = std::bit_cast<void*>(static_cast<uintptr_t>(stored->toUInt64()));
    UNUSED_VARIABLE(identifier);
    auto string = message.ascii();
    WTFEndSignpostAlways(identifier, JSCJSGlobalObject, "%" PUBLIC_LOG_STRING, string.data());
    ProfilerSupport::markEnd(identifier, ProfilerSupport::Category::JSGlobalObjectSignpost, WTFMove(string));
    --activeJSGlobalObjectSignpostIntervalCount;
}

JSC_DEFINE_HOST_FUNCTION(enableSuperSampler, (JSGlobalObject*, CallFrame*))
{
    enableSuperSampler();
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(disableSuperSampler, (JSGlobalObject*, CallFrame*))
{
    disableSuperSampler();
    return JSValue::encode(jsUndefined());
}

} // namespace JSC

#include "JSGlobalObject.lut.h"

namespace JSC {

const ClassInfo JSGlobalObject::s_info = { "GlobalObject"_s, &Base::s_info, &globalObjectTable, nullptr, CREATE_METHOD_TABLE(JSGlobalObject) };

const GlobalObjectMethodTable* JSGlobalObject::baseGlobalObjectMethodTable()
{
    static constexpr GlobalObjectMethodTable table = {
        &supportsRichSourceInfo,
        &shouldInterruptScript,
        &javaScriptRuntimeFlags,
        &queueMicrotaskToEventLoop,
        &shouldInterruptScriptBeforeTimeout,
        nullptr, // moduleLoaderImportModule
        nullptr, // moduleLoaderResolve
        nullptr, // moduleLoaderFetch
        nullptr, // moduleLoaderCreateImportMetaProperties
        nullptr, // moduleLoaderEvaluate
        &promiseRejectionTracker,
        &reportUncaughtExceptionAtEventLoop,
        &currentScriptExecutionOwner,
        &scriptExecutionStatus,
        &reportViolationForUnsafeEval,
        nullptr, // defaultLanguage
        nullptr, // compileStreaming
        nullptr, // instantiateStreaming
        &deriveShadowRealmGlobalObject,
        &codeForEval,
        &canCompileStrings,
        &trustedScriptStructure,
    };
    return &table;
};

/* Source for JSGlobalObject.lut.h
@begin globalObjectTable
  isNaN                 globalFuncIsNaN                              DontEnum|Function 1         GlobalIsNaNIntrinsic
  isFinite              globalFuncIsFinite                           DontEnum|Function 1         GlobalIsFiniteIntrinsic
  escape                globalFuncEscape                             DontEnum|Function 1
  unescape              globalFuncUnescape                           DontEnum|Function 1
  decodeURI             globalFuncDecodeURI                          DontEnum|Function 1
  decodeURIComponent    globalFuncDecodeURIComponent                 DontEnum|Function 1
  encodeURI             globalFuncEncodeURI                          DontEnum|Function 1
  encodeURIComponent    globalFuncEncodeURIComponent                 DontEnum|Function 1
  eval                  initializeEvalFunction                       DontEnum|PropertyCallback
  globalThis            JSGlobalObject::m_globalThis                 DontEnum|CellProperty
  parseInt              JSGlobalObject::m_parseIntFunction           DontEnum|CellProperty
  parseFloat            JSGlobalObject::m_parseFloatFunction         DontEnum|CellProperty
  ArrayBuffer           JSGlobalObject::m_arrayBufferStructure       DontEnum|ClassStructure
  EvalError             JSGlobalObject::m_evalErrorStructure         DontEnum|ClassStructure
  RangeError            JSGlobalObject::m_rangeErrorStructure        DontEnum|ClassStructure
  ReferenceError        JSGlobalObject::m_referenceErrorStructure    DontEnum|ClassStructure
  SyntaxError           JSGlobalObject::m_syntaxErrorStructure       DontEnum|ClassStructure
  TypeError             JSGlobalObject::m_typeErrorStructure         DontEnum|ClassStructure
  URIError              JSGlobalObject::m_URIErrorStructure          DontEnum|ClassStructure
  AggregateError        JSGlobalObject::m_aggregateErrorStructure    DontEnum|ClassStructure
  Proxy                 createProxyProperty                          DontEnum|PropertyCallback
  Reflect               createReflectProperty                        DontEnum|PropertyCallback
  JSON                  createJSONProperty                           DontEnum|PropertyCallback
  Math                  createMathProperty                           DontEnum|PropertyCallback
  Atomics               createAtomicsProperty                        DontEnum|PropertyCallback
  console               createConsoleProperty                        DontEnum|PropertyCallback
  Int8Array             JSGlobalObject::m_typedArrayInt8             DontEnum|ClassStructure
  Int16Array            JSGlobalObject::m_typedArrayInt16            DontEnum|ClassStructure
  Int32Array            JSGlobalObject::m_typedArrayInt32            DontEnum|ClassStructure
  Uint8Array            JSGlobalObject::m_typedArrayUint8            DontEnum|ClassStructure
  Uint8ClampedArray     JSGlobalObject::m_typedArrayUint8Clamped     DontEnum|ClassStructure
  Uint16Array           JSGlobalObject::m_typedArrayUint16           DontEnum|ClassStructure
  Uint32Array           JSGlobalObject::m_typedArrayUint32           DontEnum|ClassStructure
  Float16Array          JSGlobalObject::m_typedArrayFloat16          DontEnum|ClassStructure
  Float32Array          JSGlobalObject::m_typedArrayFloat32          DontEnum|ClassStructure
  Float64Array          JSGlobalObject::m_typedArrayFloat64          DontEnum|ClassStructure
  BigInt64Array         JSGlobalObject::m_typedArrayBigInt64         DontEnum|ClassStructure
  BigUint64Array        JSGlobalObject::m_typedArrayBigUint64        DontEnum|ClassStructure
  DataView              JSGlobalObject::m_typedArrayDataView         DontEnum|ClassStructure
  Date                  JSGlobalObject::m_dateStructure              DontEnum|ClassStructure
  Error                 JSGlobalObject::m_errorStructure             DontEnum|ClassStructure
  Boolean               JSGlobalObject::m_booleanObjectStructure     DontEnum|ClassStructure
  Map                   JSGlobalObject::m_mapStructure               DontEnum|ClassStructure
  Number                JSGlobalObject::m_numberObjectStructure      DontEnum|ClassStructure
  Set                   JSGlobalObject::m_setStructure               DontEnum|ClassStructure
  WeakMap               JSGlobalObject::m_weakMapStructure           DontEnum|ClassStructure
  WeakSet               JSGlobalObject::m_weakSetStructure           DontEnum|ClassStructure
@end
*/

JSC_DEFINE_HOST_FUNCTION(resolvePromise, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* promise = jsCast<JSPromise*>(callFrame->uncheckedArgument(0));
    JSValue argument = callFrame->uncheckedArgument(1);
    promise->resolvePromise(globalObject, argument);
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(rejectPromise, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* promise = jsCast<JSPromise*>(callFrame->uncheckedArgument(0));
    JSValue argument = callFrame->uncheckedArgument(1);
    promise->rejectPromise(globalObject->vm(), globalObject, argument);
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(fulfillPromise, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* promise = jsCast<JSPromise*>(callFrame->uncheckedArgument(0));
    JSValue argument = callFrame->uncheckedArgument(1);
    promise->fulfillPromise(globalObject->vm(), globalObject, argument);
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(resolveWithoutPromise, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSPromise::resolveWithoutPromise(globalObject, callFrame->uncheckedArgument(0), callFrame->uncheckedArgument(1), callFrame->uncheckedArgument(2), callFrame->uncheckedArgument(3));
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(rejectWithoutPromise, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSPromise::rejectWithoutPromise(globalObject, callFrame->uncheckedArgument(0), callFrame->uncheckedArgument(1), callFrame->uncheckedArgument(2), callFrame->uncheckedArgument(3));
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(fulfillWithoutPromise, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSPromise::fulfillWithoutPromise(globalObject, callFrame->uncheckedArgument(0), callFrame->uncheckedArgument(1), callFrame->uncheckedArgument(2), callFrame->uncheckedArgument(3));
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(resolvePromiseWithFirstResolvingFunctionCallCheck, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* promise = jsCast<JSPromise*>(callFrame->uncheckedArgument(0));
    JSValue argument = callFrame->uncheckedArgument(1);
    promise->resolve(globalObject, argument);
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(rejectPromiseWithFirstResolvingFunctionCallCheck, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* promise = jsCast<JSPromise*>(callFrame->uncheckedArgument(0));
    JSValue argument = callFrame->uncheckedArgument(1);
    promise->reject(globalObject->vm(), globalObject, argument);
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(fulfillPromiseWithFirstResolvingFunctionCallCheck, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* promise = jsCast<JSPromise*>(callFrame->uncheckedArgument(0));
    JSValue argument = callFrame->uncheckedArgument(1);
    promise->fulfill(globalObject->vm(), globalObject, argument);
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(resolveWithoutPromiseForAsyncAwait, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue resolution = callFrame->uncheckedArgument(0);
    JSValue onFulfilled = callFrame->uncheckedArgument(1);
    JSValue onRejected = callFrame->uncheckedArgument(2);
    JSValue context = callFrame->uncheckedArgument(3);
    JSPromise::resolveWithoutPromiseForAsyncAwait(globalObject, resolution, onFulfilled, onRejected, context);
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(driveAsyncFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue resolution = callFrame->uncheckedArgument(0);
    JSValue context = callFrame->uncheckedArgument(1);
    JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, resolution, InternalMicrotask::AsyncFunctionResume, context);
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(awaitValue, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue generator = callFrame->uncheckedArgument(0);
    JSValue value = callFrame->uncheckedArgument(1);
    JSValue onFulfilled = callFrame->uncheckedArgument(2);
    JSPromise::resolveWithoutPromiseForAsyncAwait(globalObject, value, onFulfilled, globalObject->asyncGeneratorYieldOnRejectedFunction(), generator);
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(newHandledRejectedPromise, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue argument = callFrame->uncheckedArgument(0);
    auto* promise = JSPromise::rejectedPromise(globalObject, argument);
    promise->markAsHandled();
    return JSValue::encode(promise);
}

JSC_DEFINE_HOST_FUNCTION(promiseOnRejectedWithContext, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue argument = callFrame->uncheckedArgument(0);
    auto* context = jsCast<JSPromiseAllContext*>(callFrame->uncheckedArgument(1));
    auto* globalContext = jsCast<JSPromiseAllGlobalContext*>(context->globalContext());
    auto* promise = jsCast<JSPromise*>(globalContext->promise());
    promise->reject(globalObject->vm(), globalObject, argument);
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(promiseAllOnFulfilled, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue argument = callFrame->uncheckedArgument(0);
    auto* context = jsCast<JSPromiseAllContext*>(callFrame->uncheckedArgument(1));
    auto* globalContext = jsCast<JSPromiseAllGlobalContext*>(context->globalContext());

    auto* values = jsCast<JSArray*>(globalContext->values());
    uint64_t index = context->index().toIndex(globalObject, "exceeding index size"_s);
    RETURN_IF_EXCEPTION(scope, { });

    values->putByIndexInline(globalObject, index, argument, true);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    RETURN_IF_EXCEPTION(scope, { });

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        auto* promise = jsCast<JSPromise*>(globalContext->promise());
        scope.release();
        promise->resolve(globalObject, values);
    }
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(promiseEmptyOnFulfilled, (JSGlobalObject*, CallFrame* callFrame))
{
    return JSValue::encode(callFrame->argument(0));
}

JSC_DEFINE_HOST_FUNCTION(promiseEmptyOnRejected, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue argument = callFrame->argument(0);
    scope.throwException(globalObject, argument);
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(promiseResolve, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame->argumentCount() == 2);
    JSObject* constructor = jsCast<JSObject*>(callFrame->uncheckedArgument(0));
    JSValue argument = callFrame->uncheckedArgument(1);
    return JSValue::encode(JSPromise::promiseResolve(globalObject, constructor, argument));
}

JSC_DEFINE_HOST_FUNCTION(promiseReject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame->argumentCount() == 2);
    JSObject* constructor = jsCast<JSObject*>(callFrame->uncheckedArgument(0));
    JSValue argument = callFrame->uncheckedArgument(1);
    return JSValue::encode(JSPromise::promiseReject(globalObject, constructor, argument));
}

JSC_DEFINE_HOST_FUNCTION(performPromiseThen, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* promise = jsCast<JSPromise*>(callFrame->uncheckedArgument(0));
    JSValue onFulfilled = callFrame->uncheckedArgument(1);
    JSValue onRejected = callFrame->uncheckedArgument(2);
    JSValue promiseOrCapability = callFrame->uncheckedArgument(3);
    JSValue context = callFrame->uncheckedArgument(4);
    promise->performPromiseThen(globalObject->vm(), globalObject, onFulfilled, onRejected, promiseOrCapability, context);
    return encodedJSUndefined();
}

JS_GLOBAL_OBJECT_ADDITIONS_2;

JSGlobalObject::JSGlobalObject(VM& vm, Structure* structure, const GlobalObjectMethodTable* globalObjectMethodTable)
    : Base(vm, structure, nullptr)
    , m_vm(&vm)
    , m_linkTimeConstants(numberOfLinkTimeConstants)
    , m_structureCache(vm)
    , m_masqueradesAsUndefinedWatchpointSet(WatchpointSet::create(IsWatched))
    , m_havingABadTimeWatchpointSet(WatchpointSet::create(IsWatched))
    , m_varInjectionWatchpointSet(WatchpointSet::create(IsWatched))
    , m_varReadOnlyWatchpointSet(WatchpointSet::create(IsWatched))
    , m_regExpRecompiledWatchpointSet(WatchpointSet::create(IsWatched))
    , m_arrayBufferDetachWatchpointSet(WatchpointSet::create(IsWatched))
    , m_weakRandom(Options::forceWeakRandomSeed() ? Options::forcedWeakRandomSeed() : cryptographicallyRandomNumber<uint32_t>())
    , m_runtimeFlags()
    , m_stackTraceLimit(Options::defaultErrorStackTraceLimit())
    , m_customGetterFunctionSet(vm)
    , m_customSetterFunctionSet(vm)
    , m_importMap(ImportMap::create())
    , m_globalObjectMethodTable(globalObjectMethodTable ? globalObjectMethodTable : baseGlobalObjectMethodTable())
{
}

JSGlobalObject::~JSGlobalObject()
{
    clearWeakTickets();
#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorController->globalObjectDestroyed();
    m_inspectorDebuggable->globalObjectDestroyed();
#endif

    if (m_debugger)
        m_debugger->detach(this, Debugger::GlobalObjectIsDestructing);
}

void JSGlobalObject::destroy(JSCell* cell)
{
    static_cast<JSGlobalObject*>(cell)->JSGlobalObject::~JSGlobalObject();
}

void JSGlobalObject::setGlobalThis(VM& vm, JSObject* globalThis)
{
    m_globalThis.set(vm, this, globalThis);
}

static GetterSetter* getGetterById(JSGlobalObject* globalObject, JSObject* base, const Identifier& ident)
{
    VM& vm = globalObject->vm();
    JSValue baseValue = JSValue(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::VMInquiry, &vm);
    baseValue.getPropertySlot(globalObject, ident, slot);
    return jsCast<GetterSetter*>(slot.getPureResult());
}

static ObjectPropertyCondition setupAdaptiveWatchpoint(JSGlobalObject* globalObject, JSObject* base, const Identifier& ident)
{
    // Performing these gets should not throw.
    VM& vm = globalObject->vm();
    DeferTerminationForAWhile deferScope(vm);
    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    PropertySlot slot(base, PropertySlot::InternalMethodType::VMInquiry, &vm);
    bool result = base->getOwnPropertySlot(base, globalObject, ident, slot);
    ASSERT_UNUSED(result, result);
    catchScope.assertNoException();
    RELEASE_ASSERT(slot.isCacheableValue() || slot.isCacheableGetter());
    JSValue functionValue = slot.isCacheableValue() ? slot.getValue(globalObject, ident) : slot.getterSetter();
    catchScope.assertNoException();
    ASSERT(jsDynamicCast<JSFunction*>(functionValue) || jsDynamicCast<GetterSetter*>(functionValue));

    ObjectPropertyCondition condition = generateConditionForSelfEquivalence(vm, nullptr, base, ident.impl());
    RELEASE_ASSERT(condition.requiredValue() == functionValue);

    bool isWatchable = condition.isWatchable(PropertyCondition::EnsureWatchability);
    RELEASE_ASSERT(isWatchable); // We allow this to install the necessary watchpoints.

    return condition;
}

static ObjectPropertyCondition setupAbsenceAdaptiveWatchpoint(JSGlobalObject* globalObject, JSObject* base, PropertyName propertyName, JSObject* prototype)
{
    // Performing these gets should not throw.
    VM& vm = globalObject->vm();
    DeferTerminationForAWhile deferScope(vm);
    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    PropertySlot slot(base, PropertySlot::InternalMethodType::VMInquiry, &vm);
    bool result = base->getOwnPropertySlot(base, globalObject, propertyName, slot);
    RELEASE_ASSERT(!result);
    catchScope.assertNoException();
    RELEASE_ASSERT(slot.isUnset());
    RELEASE_ASSERT(base->getPrototypeDirect() == (prototype ? JSValue(prototype) : jsNull()));
    ObjectPropertyCondition condition = ObjectPropertyCondition::absence(vm, globalObject, base, propertyName.uid(), prototype);

    bool isWatchable = condition.isWatchable(PropertyCondition::EnsureWatchability);
    RELEASE_ASSERT(isWatchable); // We allow this to install the necessary watchpoints.

    return condition;
}

template<ErrorType errorType>
void JSGlobalObject::initializeErrorConstructor(LazyClassStructure::Initializer& init)
{
    init.setPrototype(NativeErrorPrototype::create(init.vm, NativeErrorPrototype::createStructure(init.vm, this, m_errorStructure.prototype(this)), errorTypeName(errorType)));
    init.setStructure(ErrorInstance::createStructure(init.vm, this, init.prototype));
    init.setConstructor(NativeErrorConstructor<errorType>::create(init.vm, NativeErrorConstructor<errorType>::createStructure(init.vm, this, m_errorStructure.constructor(this)), jsCast<NativeErrorPrototype*>(init.prototype)));
}

void JSGlobalObject::initializeAggregateErrorConstructor(LazyClassStructure::Initializer& init)
{
    init.setPrototype(AggregateErrorPrototype::create(init.vm, AggregateErrorPrototype::createStructure(init.vm, this, m_errorStructure.prototype(this))));
    init.setStructure(ErrorInstance::createStructure(init.vm, this, init.prototype));
    init.setConstructor(AggregateErrorConstructor::create(init.vm, AggregateErrorConstructor::createStructure(init.vm, this, m_errorStructure.constructor(this)), jsCast<AggregateErrorPrototype*>(init.prototype)));
}

void JSGlobalObject::initializeSuppressedErrorConstructor(LazyClassStructure::Initializer& init)
{
    init.setPrototype(SuppressedErrorPrototype::create(init.vm, SuppressedErrorPrototype::createStructure(init.vm, this, m_errorStructure.prototype(this))));
    init.setStructure(ErrorInstance::createStructure(init.vm, this, init.prototype));
    init.setConstructor(SuppressedErrorConstructor::create(init.vm, SuppressedErrorConstructor::createStructure(init.vm, this, m_errorStructure.constructor(this)), jsCast<SuppressedErrorPrototype*>(init.prototype)));
}

SUPPRESS_ASAN inline void JSGlobalObject::initStaticGlobals(VM& vm)
{
    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(vm.propertyNames->NaN, jsNaN(), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->Infinity, jsNumber(std::numeric_limits<double>::infinity()), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
        GlobalPropertyInfo(vm.propertyNames->undefinedKeyword, jsUndefined(), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
#if ASSERT_ENABLED
        GlobalPropertyInfo(vm.propertyNames->builtinNames().assertPrivateName(), JSFunction::create(vm, this, 1, String(), assertCall, ImplementationVisibility::Public), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
#endif
    };
    addStaticGlobals(staticGlobals, std::size(staticGlobals));
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

void JSGlobalObject::init(VM& vm)
{
    ASSERT(vm.traps().isDeferringTermination());
    ASSERT(vm.currentThreadIsHoldingAPILock());
    auto catchScope = DECLARE_CATCH_SCOPE(vm);

    convertToDictionary(vm);

    m_debugger = nullptr;

#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorController = makeUnique<Inspector::JSGlobalObjectInspectorController>(*this);
    m_inspectorDebuggable = JSGlobalObjectDebuggable::create(*this);
    m_inspectorDebuggable->init();
    m_consoleClient = m_inspectorController->consoleClient();
#endif

    m_functionPrototype.set(vm, this, FunctionPrototype::create(vm, FunctionPrototype::createStructure(vm, this, jsNull()))); // The real prototype will be set once ObjectPrototype is created.
    m_calleeStructure.set(vm, this, JSCallee::createStructure(vm, this, jsNull()));

    m_globalLexicalEnvironment.set(vm, this, JSGlobalLexicalEnvironment::create(vm, JSGlobalLexicalEnvironment::createStructure(vm, this), this));

    // Need to create the callee structure (above) before creating the callee.
    JSCallee* globalCallee = JSCallee::create(vm, this, globalScope());
    m_globalCallee.set(vm, this, globalCallee);

    JSCallee* evalCallee = JSCallee::create(vm, this, globalScope());
    m_evalCallee.set(vm, this, evalCallee);

    m_zombieFrameCallee.set(vm, this, JSCallee::create(vm, this, globalScope()));

    m_hostFunctionStructure.set(vm, this, JSFunction::createStructure(vm, this, m_functionPrototype.get()));

    auto initFunctionStructures = [&] (FunctionStructures& structures) {
        structures.strictFunctionStructure.set(vm, this, JSStrictFunction::createStructure(vm, this, m_functionPrototype.get()));
        structures.strictMethodStructure.set(vm, this, JSStrictFunction::createStructure(vm, this, m_functionPrototype.get()));
        structures.sloppyFunctionStructure.set(vm, this, JSSloppyFunction::createStructure(vm, this, m_functionPrototype.get()));
        structures.sloppyMethodStructure.set(vm, this, JSSloppyFunction::createStructure(vm, this, m_functionPrototype.get()));
        structures.arrowFunctionStructure.set(vm, this, JSArrowFunction::createStructure(vm, this, m_functionPrototype.get()));
    };
    initFunctionStructures(m_builtinFunctions);
    initFunctionStructures(m_ordinaryFunctions);
    m_boundFunctionStructure.set(vm, this, JSBoundFunction::createStructure(vm, this, m_functionPrototype.get()));

    m_customGetterFunctionStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSCustomGetterFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });
    m_customSetterFunctionStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSCustomSetterFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });
    m_nativeStdFunctionStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSNativeStdFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });
    m_remoteFunctionStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSRemoteFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });
    JSFunction* callFunction = nullptr;
    JSFunction* applyFunction = nullptr;
    JSFunction* hasInstanceSymbolFunction = nullptr;
    m_functionPrototype->addFunctionProperties(vm, this, &callFunction, &applyFunction, &hasInstanceSymbolFunction);
    m_objectProtoToStringFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, init.owner, 0, init.vm.propertyNames->toString.string(), objectProtoFuncToString, ImplementationVisibility::Public, ObjectToStringIntrinsic));
        });
    m_arrayProtoToStringFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, init.owner, 0, init.vm.propertyNames->toString.string(), arrayProtoFuncToString, ImplementationVisibility::Public));
        });
    m_arrayProtoValuesFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, init.owner, 0, init.vm.propertyNames->builtinNames().valuesPublicName().string(), arrayProtoFuncValues, ImplementationVisibility::Public, ArrayValuesIntrinsic));
        });

    m_numberProtoToStringFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, init.vm.propertyNames->toString.string(), numberProtoFuncToString, ImplementationVisibility::Public, NumberPrototypeToStringIntrinsic));
        });

    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::stringSubstring)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "substring"_s, stringProtoFuncSubstring, ImplementationVisibility::Public, StringPrototypeSubstringIntrinsic));
        });

    m_functionProtoHasInstanceSymbolFunction.set(vm, this, hasInstanceSymbolFunction);
    m_nullGetterFunction.set(vm, this, NullGetterFunction::create(vm, NullGetterFunction::createStructure(vm, this, m_functionPrototype.get())));
    Structure* nullSetterFunctionStructure = NullSetterFunction::createStructure(vm, this, m_functionPrototype.get());
    m_nullSetterFunction.set(vm, this, NullSetterFunction::create(vm, nullSetterFunctionStructure, ECMAMode::sloppy()));
    m_nullSetterStrictFunction.set(vm, this, NullSetterFunction::create(vm, nullSetterFunctionStructure, ECMAMode::strict()));
    m_objectPrototype.set(vm, this, ObjectPrototype::create(vm, this, ObjectPrototype::createStructure(vm, this, jsNull())));
    // We have to manually set this here because we make it a prototype without transition below.
    m_objectPrototype.get()->didBecomePrototype(vm);
    GetterSetter* protoAccessor = GetterSetter::create(vm, this,
        JSFunction::create(vm, this, 0, makeString("get "_s, vm.propertyNames->underscoreProto.string()), globalFuncProtoGetter, ImplementationVisibility::Public, UnderscoreProtoIntrinsic),
        JSFunction::create(vm, this, 0, makeString("set "_s, vm.propertyNames->underscoreProto.string()), globalFuncProtoSetter, ImplementationVisibility::Public));
    m_objectPrototype->putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->underscoreProto, protoAccessor, PropertyAttribute::Accessor | PropertyAttribute::DontEnum);
    m_functionPrototype->structure()->setPrototypeWithoutTransition(vm, m_objectPrototype.get());
    m_objectStructureForObjectConstructor.set(vm, this, m_structureCache.emptyObjectStructureForPrototype(this, m_objectPrototype.get(), JSFinalObject::defaultInlineCapacity));
    m_objectProtoValueOfFunction.set(vm, this, jsCast<JSFunction*>(objectPrototype()->getDirect(vm, vm.propertyNames->valueOf)));

    JS_GLOBAL_OBJECT_ADDITIONS_3;

    m_arraySpeciesGetterSetter.set(vm, this, GetterSetter::create(vm, this, JSFunction::create(vm, this, 0, "get [Symbol.species]"_s, globalFuncSpeciesGetter, ImplementationVisibility::Public, SpeciesGetterIntrinsic), nullptr));
    m_typedArraySpeciesGetterSetter.set(vm, this, GetterSetter::create(vm, this, JSFunction::create(vm, this, 0, "get [Symbol.species]"_s, globalFuncSpeciesGetter, ImplementationVisibility::Public, SpeciesGetterIntrinsic), nullptr));
    m_arrayBufferSpeciesGetterSetter.set(vm, this, GetterSetter::create(vm, this, JSFunction::create(vm, this, 0, "get [Symbol.species]"_s, globalFuncSpeciesGetter, ImplementationVisibility::Public, SpeciesGetterIntrinsic), nullptr));
    m_sharedArrayBufferSpeciesGetterSetter.set(vm, this, GetterSetter::create(vm, this, JSFunction::create(vm, this, 0, "get [Symbol.species]"_s, globalFuncSpeciesGetter, ImplementationVisibility::Public, SpeciesGetterIntrinsic), nullptr));
    m_promiseSpeciesGetterSetter.set(vm, this, GetterSetter::create(vm, this, JSFunction::create(vm, this, 0, "get [Symbol.species]"_s, globalFuncSpeciesGetter, ImplementationVisibility::Public, SpeciesGetterIntrinsic), nullptr));

    m_throwTypeErrorArgumentsCalleeGetterSetter.initLater(
        [] (const Initializer<GetterSetter>& init) {
            JSFunction* thrower = JSFunction::create(init.vm, init.owner, 0, emptyString(), globalFuncThrowTypeErrorArgumentsCalleeAndCaller, ImplementationVisibility::Public);
            thrower->freeze(init.vm);
            init.set(GetterSetter::create(init.vm, init.owner, thrower, thrower));
        });
    m_typedArrayProto.initLater(
        [] (const Initializer<JSTypedArrayViewPrototype>& init) {
            init.set(JSTypedArrayViewPrototype::create(init.vm, init.owner, JSTypedArrayViewPrototype::createStructure(init.vm, init.owner, init.owner->m_objectPrototype.get())));
            
            // Make sure that the constructor gets initialized, too.
            init.owner->m_typedArraySuperConstructor.get(init.owner);
        });
    m_typedArraySuperConstructor.initLater(
        [] (const Initializer<JSTypedArrayViewConstructor>& init) {
            JSTypedArrayViewPrototype* prototype = init.owner->m_typedArrayProto.get(init.owner);
            JSTypedArrayViewConstructor* constructor = JSTypedArrayViewConstructor::create(init.vm, init.owner, JSTypedArrayViewConstructor::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()), prototype);
            prototype->putDirectWithoutTransition(init.vm, init.vm.propertyNames->constructor, constructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
            init.set(constructor);
        });
    
#define INIT_TYPED_ARRAY_LATER(type) \
    m_typedArray ## type.initLater( \
        [] (LazyClassStructure::Initializer& init) { \
            init.setPrototype(JS ## type ## ArrayPrototype::create(init.vm, init.global, JS ## type ## ArrayPrototype::createStructure(init.vm, init.global, init.global->m_typedArrayProto.get(init.global)))); \
            init.setStructure(JS ## type ## Array::createStructure(init.vm, init.global, init.prototype)); \
            init.setConstructor(JS ## type ## ArrayConstructor::create(init.vm, init.global, JS ## type ## ArrayConstructor::createStructure(init.vm, init.global, init.global->m_typedArraySuperConstructor.get(init.global)), init.prototype, #type "Array"_s)); \
            init.global->typedArrayStructure(Type##type, /* isResizableOrGrowableShared */ true); /* Initialize resizable Structure too */ \
        }); \
    m_resizableOrGrowableSharedTypedArray ## type ## Structure.initLater( \
        [] (const Initializer<Structure>& init) { \
            init.set(JSResizableOrGrowableShared ## type ## Array::createStructure(init.vm, init.owner, init.owner->typedArrayPrototype(Type##type))); \
            init.owner->typedArrayStructure(Type##type, /* isResizableOrGrowableShared */ false); /* Initialize non-resizable Structure too */ \
        }); \
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::type##Array)].initLater([](const Initializer<JSCell>& init) { \
            init.set(jsCast<JSGlobalObject*>(init.owner)->typedArrayConstructor(TypedArrayType::Type##type)); \
        });
    FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(INIT_TYPED_ARRAY_LATER)
#undef INIT_TYPED_ARRAY_LATER
    
    m_typedArrayDataView.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.setPrototype(JSDataViewPrototype::create(init.vm, init.global, JSDataViewPrototype::createStructure(init.vm, init.global, init.global->m_objectPrototype.get())));
            init.setStructure(JSDataView::createStructure(init.vm, init.global, init.prototype));
            init.setConstructor(JSDataViewConstructor::create(init.vm, init.global, JSDataViewConstructor::createStructure(init.vm, init.global, init.global->m_functionPrototype.get()), init.prototype, "DataView"_s));
            init.global->typedArrayStructure(TypeDataView, /* isResizableOrGrowableShared */ true); /* Initialize resizable Structure too */
        });
    m_resizableOrGrowableSharedTypedArrayDataViewStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSResizableOrGrowableSharedDataView::createStructure(init.vm, init.owner, init.owner->typedArrayPrototype(TypeDataView)));
            init.owner->typedArrayStructure(TypeDataView, /* isResizableOrGrowableShared */ false); /* Initialize non-resizable Structure too */
        });

    m_lexicalEnvironmentStructure.set(vm, this, JSLexicalEnvironment::createStructure(vm, this));
    m_moduleEnvironmentStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSModuleEnvironment::createStructure(init.vm, init.owner));
        });
    m_strictEvalActivationStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(StrictEvalActivation::createStructure(init.vm, init.owner, jsNull()));
        });
    m_debuggerScopeStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(DebuggerScope::createStructure(init.vm, init.owner));
        });
    m_withScopeStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSWithScope::createStructure(init.vm, init.owner, jsNull()));
        });
    
    m_nullPrototypeObjectStructure.set(vm, this, JSFinalObject::createStructure(vm, this, jsNull(), JSFinalObject::defaultInlineCapacity));
    
    m_callbackFunctionStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSCallbackFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });
    m_directArgumentsStructure.set(vm, this, DirectArguments::createStructure(vm, this, m_objectPrototype.get()));
    m_scopedArgumentsStructure.set(vm, this, ScopedArguments::createStructure(vm, this, m_objectPrototype.get()));
    m_clonedArgumentsStructure.set(vm, this, ClonedArguments::createStructure(vm, this, m_objectPrototype.get()));
    m_callbackConstructorStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSCallbackConstructor::createStructure(init.vm, init.owner, init.owner->m_objectPrototype.get()));
        });
    m_callbackObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSCallbackObject<JSNonFinalObject>::createStructure(init.vm, init.owner, init.owner->m_objectPrototype.get()));
        });
    m_rawJSONObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSRawJSONObject::createStructure(init.vm, init.owner, jsNull()));
        });

#if JSC_OBJC_API_ENABLED
    m_objcCallbackFunctionStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(ObjCCallbackFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });
    m_objcWrapperObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSCallbackObject<JSAPIWrapperObject>::createStructure(init.vm, init.owner, init.owner->m_objectPrototype.get()));
        });
#endif
#ifdef JSC_GLIB_API_ENABLED
    m_glibCallbackFunctionStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSCCallbackFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });
    m_glibWrapperObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSCallbackObject<JSAPIWrapperObject>::createStructure(init.vm, init.owner, init.owner->m_objectPrototype.get()));
        });
#endif
    m_arrayPrototype.set(vm, this, ArrayPrototype::create(vm, this, ArrayPrototype::createStructure(vm, this, m_objectPrototype.get())));
    
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(UndecidedShape)].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithUndecided));
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(Int32Shape)].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithInt32));

    Structure* arrayWithContiguousStructure = JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithContiguous);
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(DoubleShape)].set(vm, this,
        Options::allowDoubleShape() ? JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithDouble) : arrayWithContiguousStructure);
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(ContiguousShape)].set(vm, this, arrayWithContiguousStructure);

    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(ArrayStorageShape)].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithArrayStorage));
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(SlowPutArrayStorageShape)].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), ArrayWithSlowPutArrayStorage));
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(CopyOnWriteArrayWithInt32)].set(vm, this, JSArray::createStructure(vm, this, m_arrayPrototype.get(), CopyOnWriteArrayWithInt32));

    Structure* copyOnWriteArrayWithContiguous = JSArray::createStructure(vm, this, m_arrayPrototype.get(), CopyOnWriteArrayWithContiguous);
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(CopyOnWriteArrayWithDouble)].set(vm, this,
        Options::allowDoubleShape() ? JSArray::createStructure(vm, this, m_arrayPrototype.get(), CopyOnWriteArrayWithDouble) : copyOnWriteArrayWithContiguous);
    m_originalArrayStructureForIndexingShape[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous)].set(vm, this, copyOnWriteArrayWithContiguous);

    for (unsigned i = 0; i < NumberOfArrayIndexingModes; ++i)
        m_arrayStructureForIndexingShapeDuringAllocation[i] = m_originalArrayStructureForIndexingShape[i];

    m_shadowRealmPrototype.set(vm, this, ShadowRealmPrototype::create(vm, ShadowRealmPrototype::createStructure(vm, this, m_objectPrototype.get())));
    m_shadowRealmObjectStructure.set(vm, this, ShadowRealmObject::createStructure(vm, this, m_shadowRealmPrototype.get()));

    m_regExpPrototype.set(vm, this, RegExpPrototype::create(vm, this, RegExpPrototype::createStructure(vm, this, m_objectPrototype.get())));
    m_regExpStructure.set(vm, this, RegExpObject::createStructure(vm, this, m_regExpPrototype.get()));
    m_regExpMatchesArrayStructure.set(vm, this, createRegExpMatchesArrayStructure(vm, this));
    m_regExpMatchesArrayWithIndicesStructure.set(vm, this, createRegExpMatchesArrayWithIndicesStructure(vm, this));
    m_regExpMatchesIndicesArrayStructure.set(vm, this, createRegExpMatchesIndicesArrayStructure(vm, this));

    m_trustedScriptStructure.setMayBeNull(vm, this, globalObjectMethodTable()->trustedScriptStructure(this));

    m_moduleRecordStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSModuleRecord::createStructure(init.vm, init.owner, jsNull()));
        });
    m_syntheticModuleRecordStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(SyntheticModuleRecord::createStructure(init.vm, init.owner, jsNull()));
        });
    m_moduleNamespaceObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(JSModuleNamespaceObject::createStructure(init.vm, init.owner, jsNull()));
        });
    m_proxyObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            bool isCallable = false;
            init.set(ProxyObject::createStructure(init.vm, init.owner, jsNull(), isCallable));
        });
    m_callableProxyObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            bool isCallable = true;
            init.set(ProxyObject::createStructure(init.vm, init.owner, jsNull(), isCallable));
        });
    m_proxyRevokeStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(ProxyRevoke::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
        });

    m_parseIntFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, init.owner, 2, init.vm.propertyNames->parseInt.string(), globalFuncParseInt, ImplementationVisibility::Public, ParseIntIntrinsic));
        });
    m_parseFloatFunction.initLater(
        [] (const Initializer<JSFunction>& init) {
            init.set(JSFunction::create(init.vm, init.owner, 1, init.vm.propertyNames->parseFloat.string(), globalFuncParseFloat, ImplementationVisibility::Public));
        });

    m_sharedArrayBufferStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.setPrototype(JSArrayBufferPrototype::create(init.vm, init.global, JSArrayBufferPrototype::createStructure(init.vm, init.global, init.global->m_objectPrototype.get()), ArrayBufferSharingMode::Shared));
            init.setStructure(JSArrayBuffer::createStructure(init.vm, init.global, init.prototype));
            init.setConstructor(JSSharedArrayBufferConstructor::create(init.vm, JSSharedArrayBufferConstructor::createStructure(init.vm, init.global, init.global->m_functionPrototype.get()), jsCast<JSArrayBufferPrototype*>(init.prototype)));
        });

    m_disposableStackStructure.initLater(
        [] (LazyClassStructure::Initializer& init) -> void {
            init.setPrototype(DisposableStackPrototype::create(init.vm, init.global, DisposableStackPrototype::createStructure(init.vm, init.global, init.global->m_objectPrototype.get())));
            init.setStructure(JSDisposableStack::createStructure(init.vm, init.global, init.prototype));
            init.setConstructor(DisposableStackConstructor::create(init.vm, init.global, DisposableStackConstructor::createStructure(init.vm, init.global, init.global->m_functionPrototype.get()), jsCast<DisposableStackPrototype*>(init.prototype)));
        });
    m_asyncDisposableStackStructure.initLater(
        [] (LazyClassStructure::Initializer& init) -> void {
            init.setPrototype(AsyncDisposableStackPrototype::create(init.vm, init.global, AsyncDisposableStackPrototype::createStructure(init.vm, init.global, init.global->m_objectPrototype.get())));
            init.setStructure(JSAsyncDisposableStack::createStructure(init.vm, init.global, init.prototype));
            init.setConstructor(AsyncDisposableStackConstructor::create(init.vm, init.global, AsyncDisposableStackConstructor::createStructure(init.vm, init.global, init.global->m_functionPrototype.get()), jsCast<AsyncDisposableStackPrototype*>(init.prototype)));
        });

    m_iteratorPrototype.set(vm, this, JSIteratorPrototype::create(vm, this, JSIteratorPrototype::createStructure(vm, this, m_objectPrototype.get())));

    m_iteratorStructure.set(vm, this, JSIterator::createStructure(vm, this, m_iteratorPrototype.get()));

    m_iteratorHelperPrototype.set(vm, this, JSIteratorHelperPrototype::create(vm, this, JSIteratorHelperPrototype::createStructure(vm, this, m_iteratorPrototype.get())));
    m_iteratorHelperStructure.set(vm, this, JSIteratorHelper::createStructure(vm, this, m_iteratorHelperPrototype.get()));

    m_asyncIteratorPrototype.set(vm, this, AsyncIteratorPrototype::create(vm, this, AsyncIteratorPrototype::createStructure(vm, this, m_objectPrototype.get())));

    m_generatorPrototype.set(vm, this, GeneratorPrototype::create(vm, this, GeneratorPrototype::createStructure(vm, this, m_iteratorPrototype.get())));
    m_asyncGeneratorPrototype.set(vm, this, AsyncGeneratorPrototype::create(vm, this, AsyncGeneratorPrototype::createStructure(vm, this, m_asyncIteratorPrototype.get())));

    auto* arrayIteratorPrototype = ArrayIteratorPrototype::create(vm, this, ArrayIteratorPrototype::createStructure(vm, this, m_iteratorPrototype.get()));
    m_arrayIteratorPrototype.set(vm, this, arrayIteratorPrototype);
    m_arrayIteratorStructure.set(vm, this, JSArrayIterator::createStructure(vm, this, arrayIteratorPrototype));

    auto* mapIteratorPrototype = MapIteratorPrototype::create(vm, this, MapIteratorPrototype::createStructure(vm, this, m_iteratorPrototype.get()));
    m_mapIteratorPrototype.set(vm, this, mapIteratorPrototype);
    m_mapIteratorStructure.set(vm, this, JSMapIterator::createStructure(vm, this, mapIteratorPrototype));

    auto* setIteratorPrototype = SetIteratorPrototype::create(vm, this, SetIteratorPrototype::createStructure(vm, this, m_iteratorPrototype.get()));
    m_setIteratorPrototype.set(vm, this, setIteratorPrototype);
    m_setIteratorStructure.set(vm, this, JSSetIterator::createStructure(vm, this, setIteratorPrototype));

    auto* wrapForValidIteratorPrototype = WrapForValidIteratorPrototype::create(vm, this, WrapForValidIteratorPrototype::createStructure(vm, this, m_iteratorPrototype.get()));
    m_wrapForValidIteratorStructure.set(vm, this, JSWrapForValidIterator::createStructure(vm, this, wrapForValidIteratorPrototype));

    m_promiseAllContextStructure.set(vm, this, JSPromiseAllContext::createStructure(vm, this, jsNull()));

    m_promiseAllGlobalContextStructure.set(vm, this, JSPromiseAllGlobalContext::createStructure(vm, this, jsNull()));

    auto* asyncFromSyncIteratorPrototype = AsyncFromSyncIteratorPrototype::create(vm, this, AsyncFromSyncIteratorPrototype::createStructure(vm, this, m_iteratorPrototype.get()));
    m_asyncFromSyncIteratorStructure.set(vm, this, JSAsyncFromSyncIterator::createStructure(vm, this, asyncFromSyncIteratorPrototype));

    auto* regExpStringIteratorPrototype = RegExpStringIteratorPrototype::create(vm, this, RegExpStringIteratorPrototype::createStructure(vm, this, m_iteratorPrototype.get()));
    m_regExpStringIteratorStructure.set(vm, this, JSRegExpStringIterator::createStructure(vm, this, regExpStringIteratorPrototype));

    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::sentinelString)].set(vm, this, vm.smallStrings.sentinelString());

    JSFunction* defaultPromiseThen = JSFunction::create(vm, this, 2, vm.propertyNames->then.impl(), promiseProtoFuncThen, ImplementationVisibility::Public, PromisePrototypeThenIntrinsic);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::defaultPromiseThen)].set(vm, this, defaultPromiseThen);

#define CREATE_PROTOTYPE_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) if (featureFlag) { \
        m_ ## lowerName ## Prototype.set(vm, this, capitalName##Prototype::create(vm, this, capitalName##Prototype::createStructure(vm, this, m_ ## prototypeBase ## Prototype.get()))); \
        m_ ## properName ## Structure.set(vm, this, instanceType::createStructure(vm, this, m_ ## lowerName ## Prototype.get())); \
    }
    
    FOR_EACH_SIMPLE_BUILTIN_TYPE(CREATE_PROTOTYPE_FOR_SIMPLE_TYPE)
    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(CREATE_PROTOTYPE_FOR_SIMPLE_TYPE)
    
#undef CREATE_PROTOTYPE_FOR_SIMPLE_TYPE

#define CREATE_PROTOTYPE_FOR_LAZY_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) if (featureFlag) {  \
    m_ ## properName ## Structure.initLater(\
        [] (LazyClassStructure::Initializer& init) { \
            init.setPrototype(capitalName##Prototype::create(init.vm, init.global, capitalName##Prototype::createStructure(init.vm, init.global, init.global->m_ ## prototypeBase ## Prototype.get()))); \
            init.setStructure(instanceType::createStructure(init.vm, init.global, init.prototype)); \
            init.setConstructor(capitalName ## Constructor::create(init.vm, capitalName ## Constructor::createStructure(init.vm, init.global, init.global->m_functionPrototype.get()), jsCast<capitalName ## Prototype*>(init.prototype))); \
        }); \
    }
    
    FOR_EACH_LAZY_BUILTIN_TYPE(CREATE_PROTOTYPE_FOR_LAZY_TYPE)

#undef CREATE_PROTOTYPE_FOR_LAZY_TYPE
    
    // Constructors

    ObjectConstructor* objectConstructor = ObjectConstructor::create(vm, this, ObjectConstructor::createStructure(vm, this, m_functionPrototype.get()), m_objectPrototype.get());
    m_objectConstructor.set(vm, this, objectConstructor);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::Object)].set(vm, this, objectConstructor);

    JSFunction* throwTypeErrorFunction = JSFunction::create(vm, this, 0, String(), globalFuncThrowTypeError, ImplementationVisibility::Public);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::throwTypeErrorFunction)].set(vm, this, throwTypeErrorFunction);

    FunctionConstructor* functionConstructor = FunctionConstructor::create(vm, FunctionConstructor::createStructure(vm, this, m_functionPrototype.get()), m_functionPrototype.get());
    m_functionConstructor.set(vm, this, functionConstructor);

    ArrayConstructor* arrayConstructor = ArrayConstructor::create(vm, this, ArrayConstructor::createStructure(vm, this, m_functionPrototype.get()), m_arrayPrototype.get());
    m_arrayConstructor.set(vm, this, arrayConstructor);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::Array)].set(vm, this, arrayConstructor);

    ShadowRealmConstructor* shadowRealmConstructor = ShadowRealmConstructor::create(vm, ShadowRealmConstructor::createStructure(vm, this, m_functionPrototype.get()), m_shadowRealmPrototype.get());
    m_shadowRealmConstructor.set(vm, this, shadowRealmConstructor);

    RegExpConstructor* regExpConstructor = RegExpConstructor::create(vm, RegExpConstructor::createStructure(vm, this, m_functionPrototype.get()), m_regExpPrototype.get());
    m_regExpConstructor.set(vm, this, regExpConstructor);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::RegExp)].set(vm, this, regExpConstructor);
    m_regExpGlobalData.cachedResult().record(vm, this, nullptr, jsEmptyString(vm), MatchResult(0, 0), /*oneCharacterMatch */ false);

#define CREATE_CONSTRUCTOR_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
capitalName ## Constructor* lowerName ## Constructor = featureFlag ? capitalName ## Constructor::create(vm, capitalName ## Constructor::createStructure(vm, this, m_functionPrototype.get()), m_ ## lowerName ## Prototype.get()) : nullptr; \
    if (featureFlag) \
        m_ ## lowerName ## Prototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, lowerName ## Constructor, static_cast<unsigned>(PropertyAttribute::DontEnum)); \

    FOR_EACH_SIMPLE_BUILTIN_TYPE(CREATE_CONSTRUCTOR_FOR_SIMPLE_TYPE)
    
#undef CREATE_CONSTRUCTOR_FOR_SIMPLE_TYPE

    m_promiseConstructor.set(vm, this, promiseConstructor);
    m_internalPromiseConstructor.set(vm, this, internalPromiseConstructor);
    m_stringConstructor.set(vm, this, stringConstructor);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::Promise)].set(vm, this, promiseConstructor);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::InternalPromise)].set(vm, this, internalPromiseConstructor);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::String)].set(vm, this, stringConstructor);

    m_evalErrorStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.global->initializeErrorConstructor<ErrorType::EvalError>(init);
        });
    m_rangeErrorStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.global->initializeErrorConstructor<ErrorType::RangeError>(init);
        });
    m_referenceErrorStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.global->initializeErrorConstructor<ErrorType::ReferenceError>(init);
        });
    m_syntaxErrorStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.global->initializeErrorConstructor<ErrorType::SyntaxError>(init);
        });
    m_typeErrorStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.global->initializeErrorConstructor<ErrorType::TypeError>(init);
        });
    m_URIErrorStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.global->initializeErrorConstructor<ErrorType::URIError>(init);
        });
    m_aggregateErrorStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.global->initializeAggregateErrorConstructor(init);
        });
    if (Options::useExplicitResourceManagement()) {
        m_suppressedErrorStructure.initLater(
            [] (LazyClassStructure::Initializer& init) {
                init.global->initializeSuppressedErrorConstructor(init);
            });
    }

    m_generatorFunctionPrototype.set(vm, this, GeneratorFunctionPrototype::create(vm, GeneratorFunctionPrototype::createStructure(vm, this, m_functionPrototype.get())));
    GeneratorFunctionConstructor* generatorFunctionConstructor = GeneratorFunctionConstructor::create(vm, GeneratorFunctionConstructor::createStructure(vm, this, functionConstructor), m_generatorFunctionPrototype.get());
    m_generatorFunctionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, generatorFunctionConstructor, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    m_generatorFunctionStructure.set(vm, this, JSGeneratorFunction::createStructure(vm, this, m_generatorFunctionPrototype.get()));

    m_generatorPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, m_generatorFunctionPrototype.get(), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    m_generatorFunctionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->prototype, m_generatorPrototype.get(), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    m_generatorStructure.set(vm, this, JSGenerator::createStructure(vm, this, m_generatorPrototype.get()));

    m_asyncFunctionPrototype.set(vm, this, AsyncFunctionPrototype::create(vm, AsyncFunctionPrototype::createStructure(vm, this, m_functionPrototype.get())));
    AsyncFunctionConstructor* asyncFunctionConstructor = AsyncFunctionConstructor::create(vm, AsyncFunctionConstructor::createStructure(vm, this, functionConstructor), m_asyncFunctionPrototype.get());
    m_asyncFunctionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, asyncFunctionConstructor, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    m_asyncFunctionStructure.set(vm, this, JSAsyncFunction::createStructure(vm, this, m_asyncFunctionPrototype.get()));
    m_functionWithFieldsStructure.set(vm, this, JSFunctionWithFields::createStructure(vm, this, m_functionPrototype.get()));

    m_asyncGeneratorFunctionPrototype.set(vm, this, AsyncGeneratorFunctionPrototype::create(vm, AsyncGeneratorFunctionPrototype::createStructure(vm, this, m_functionPrototype.get())));
    AsyncGeneratorFunctionConstructor* asyncGeneratorFunctionConstructor = AsyncGeneratorFunctionConstructor::create(vm, AsyncGeneratorFunctionConstructor::createStructure(vm, this, functionConstructor), m_asyncGeneratorFunctionPrototype.get());
    m_asyncGeneratorFunctionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, asyncGeneratorFunctionConstructor, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    m_asyncGeneratorFunctionStructure.set(vm, this, JSAsyncGeneratorFunction::createStructure(vm, this, m_asyncGeneratorFunctionPrototype.get()));

    m_asyncGeneratorPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, m_asyncGeneratorFunctionPrototype.get(), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    m_asyncGeneratorFunctionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->prototype, m_asyncGeneratorPrototype.get(), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    m_asyncGeneratorStructure.set(vm, this, JSAsyncGenerator::createStructure(vm, this, m_asyncGeneratorPrototype.get()));
    
    m_objectPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, objectConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    m_functionPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, functionConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    m_arrayPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, arrayConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    m_regExpPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, regExpConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    m_shadowRealmPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, shadowRealmConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    
    putDirectWithoutTransition(vm, vm.propertyNames->Object, objectConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->Function, functionConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->Array, arrayConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->RegExp, regExpConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSIteratorConstructor* iteratorConstructor = JSIteratorConstructor::create(vm, this, JSIteratorConstructor::createStructure(vm, this, m_functionPrototype.get()), m_iteratorPrototype.get());
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::Iterator)].set(vm, this, iteratorConstructor);
    m_iteratorConstructor.set(vm, this, iteratorConstructor);
    putDirectWithoutTransition(vm, vm.propertyNames->Iterator, iteratorConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));

    if (Options::useSharedArrayBuffer())
        putDirectWithoutTransition(vm, vm.propertyNames->SharedArrayBuffer, m_sharedArrayBufferStructure.constructor(this), static_cast<unsigned>(PropertyAttribute::DontEnum));

    if (Options::useExplicitResourceManagement()) {
        putDirectWithoutTransition(vm, vm.propertyNames->SuppressedError, m_suppressedErrorStructure.constructor(this), static_cast<unsigned>(PropertyAttribute::DontEnum));
        putDirectWithoutTransition(vm, vm.propertyNames->DisposableStack, m_disposableStackStructure.constructor(this), static_cast<unsigned>(PropertyAttribute::DontEnum));
        putDirectWithoutTransition(vm, vm.propertyNames->AsyncDisposableStack, m_asyncDisposableStackStructure.constructor(this), static_cast<unsigned>(PropertyAttribute::DontEnum));
    }

#define PUT_CONSTRUCTOR_FOR_SIMPLE_TYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
    if (featureFlag) \
        putDirectWithoutTransition(vm, vm.propertyNames-> jsName, lowerName ## Constructor, static_cast<unsigned>(PropertyAttribute::DontEnum));


    FOR_EACH_SIMPLE_BUILTIN_TYPE_WITH_CONSTRUCTOR(PUT_CONSTRUCTOR_FOR_SIMPLE_TYPE)

#undef PUT_CONSTRUCTOR_FOR_SIMPLE_TYPE
    m_iteratorResultObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(createIteratorResultObjectStructure(init.vm, *init.owner));
        });
    m_dataPropertyDescriptorObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(createDataPropertyDescriptorObjectStructure(init.vm, *init.owner));
        });
    m_accessorPropertyDescriptorObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(createAccessorPropertyDescriptorObjectStructure(init.vm, *init.owner));
        });
    m_promiseCapabilityObjectStructure.initLater(
        [] (const Initializer<Structure>& init) {
            init.set(createPromiseCapabilityObjectStructure(init.vm, *init.owner));
        });

    m_collatorStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlCollatorPrototype* collatorPrototype = IntlCollatorPrototype::create(init.vm, globalObject, IntlCollatorPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlCollator::createStructure(init.vm, globalObject, collatorPrototype));
        });
    m_displayNamesStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlDisplayNamesPrototype* displayNamesPrototype = IntlDisplayNamesPrototype::create(init.vm, IntlDisplayNamesPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlDisplayNames::createStructure(init.vm, globalObject, displayNamesPrototype));
        });
    m_durationFormatStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlDurationFormatPrototype* durationFormatPrototype = IntlDurationFormatPrototype::create(init.vm, IntlDurationFormatPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlDurationFormat::createStructure(init.vm, globalObject, durationFormatPrototype));
        });
    m_listFormatStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlListFormatPrototype* listFormatPrototype = IntlListFormatPrototype::create(init.vm, IntlListFormatPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlListFormat::createStructure(init.vm, globalObject, listFormatPrototype));
        });
    m_localeStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlLocalePrototype* localePrototype = IntlLocalePrototype::create(init.vm, IntlLocalePrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlLocale::createStructure(init.vm, globalObject, localePrototype));
        });
    m_pluralRulesStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlPluralRulesPrototype* pluralRulesPrototype = IntlPluralRulesPrototype::create(init.vm, globalObject, IntlPluralRulesPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlPluralRules::createStructure(init.vm, globalObject, pluralRulesPrototype));
        });
    m_relativeTimeFormatStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlRelativeTimeFormatPrototype* relativeTimeFormatPrototype = IntlRelativeTimeFormatPrototype::create(init.vm, IntlRelativeTimeFormatPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlRelativeTimeFormat::createStructure(init.vm, globalObject, relativeTimeFormatPrototype));
        });
    m_segmentIteratorStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlSegmentIteratorPrototype* segmentIteratorPrototype = IntlSegmentIteratorPrototype::create(init.vm, IntlSegmentIteratorPrototype::createStructure(init.vm, globalObject, globalObject->iteratorPrototype()));
            init.set(IntlSegmentIterator::createStructure(init.vm, globalObject, segmentIteratorPrototype));
        });
    m_segmenterStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlSegmenterPrototype* segmenterPrototype = IntlSegmenterPrototype::create(init.vm, IntlSegmenterPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlSegmenter::createStructure(init.vm, globalObject, segmenterPrototype));
        });
    m_segmentsStructure.initLater(
        [] (const Initializer<Structure>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            IntlSegmentsPrototype* segmentsPrototype = IntlSegmentsPrototype::create(init.vm, globalObject, IntlSegmentsPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
            init.set(IntlSegments::createStructure(init.vm, globalObject, segmentsPrototype));
        });

    m_dateTimeFormatStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.setPrototype(IntlDateTimeFormatPrototype::create(init.vm, init.global, IntlDateTimeFormatPrototype::createStructure(init.vm, init.global, init.global->objectPrototype())));
            init.setStructure(IntlDateTimeFormat::createStructure(init.vm, init.global, init.prototype));
            init.setConstructor(IntlDateTimeFormatConstructor::create(init.vm, IntlDateTimeFormatConstructor::createStructure(init.vm, init.global, init.global->functionPrototype()), jsCast<IntlDateTimeFormatPrototype*>(init.prototype)));
        });
    m_numberFormatStructure.initLater(
        [] (LazyClassStructure::Initializer& init) {
            init.setPrototype(IntlNumberFormatPrototype::create(init.vm, init.global, IntlNumberFormatPrototype::createStructure(init.vm, init.global, init.global->objectPrototype())));
            init.setStructure(IntlNumberFormat::createStructure(init.vm, init.global, init.prototype));
            init.setConstructor(IntlNumberFormatConstructor::create(init.vm, IntlNumberFormatConstructor::createStructure(init.vm, init.global, init.global->functionPrototype()), jsCast<IntlNumberFormatPrototype*>(init.prototype)));
        });

    m_defaultCollator.initLater(
        [] (const Initializer<IntlCollator>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            VM& vm = init.vm;
            auto scope = DECLARE_THROW_SCOPE(vm);
            IntlCollator* collator = IntlCollator::create(vm, globalObject->collatorStructure());
            collator->initializeCollator(globalObject, jsUndefined(), jsUndefined());
            RETURN_IF_EXCEPTION(scope, void());
            init.set(collator);
        });

    m_defaultNumberFormat.initLater(
        [] (const Initializer<IntlNumberFormat>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            VM& vm = init.vm;
            auto scope = DECLARE_THROW_SCOPE(vm);
            auto* numberFormat = IntlNumberFormat::create(vm, globalObject->numberFormatStructure());
            numberFormat->initializeNumberFormat(globalObject, jsUndefined(), jsUndefined());
            RETURN_IF_EXCEPTION(scope, void());
            init.set(numberFormat);
        });

    IntlObject* intl = IntlObject::create(vm, this, IntlObject::createStructure(vm, this, m_objectPrototype.get()));
    putDirectWithoutTransition(vm, vm.propertyNames->Intl, intl, static_cast<unsigned>(PropertyAttribute::DontEnum));

    if (Options::useTemporal()) {
        m_calendarStructure.initLater(
            [] (const Initializer<Structure>& init) {
                JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
                TemporalCalendarPrototype* calendarPrototype = TemporalCalendarPrototype::create(init.vm, globalObject, TemporalCalendarPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
                init.set(TemporalCalendar::createStructure(init.vm, globalObject, calendarPrototype));
            });

        m_durationStructure.initLater(
            [] (const Initializer<Structure>& init) {
                JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
                TemporalDurationPrototype* durationPrototype = TemporalDurationPrototype::create(init.vm, TemporalDurationPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
                init.set(TemporalDuration::createStructure(init.vm, globalObject, durationPrototype));
            });

        m_instantStructure.initLater(
            [] (const Initializer<Structure>& init) {
                JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
                TemporalInstantPrototype* instantPrototype = TemporalInstantPrototype::create(init.vm, TemporalInstantPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
                init.set(TemporalInstant::createStructure(init.vm, globalObject, instantPrototype));
            });

        m_plainDateStructure.initLater(
            [] (const Initializer<Structure>& init) {
                auto* globalObject = jsCast<JSGlobalObject*>(init.owner);
                auto* plainDatePrototype = TemporalPlainDatePrototype::create(init.vm, globalObject, TemporalPlainDatePrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
                init.set(TemporalPlainDate::createStructure(init.vm, globalObject, plainDatePrototype));
            });

        m_plainDateTimeStructure.initLater(
            [] (const Initializer<Structure>& init) {
                auto* globalObject = jsCast<JSGlobalObject*>(init.owner);
                auto* plainDateTimePrototype = TemporalPlainDateTimePrototype::create(init.vm, globalObject, TemporalPlainDateTimePrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
                init.set(TemporalPlainDateTime::createStructure(init.vm, globalObject, plainDateTimePrototype));
            });

        m_plainMonthDayStructure.initLater(
            [] (const Initializer<Structure>& init) {
                auto* globalObject = jsCast<JSGlobalObject*>(init.owner);
                auto* plainMonthDayPrototype = TemporalPlainMonthDayPrototype::create(init.vm, globalObject, TemporalPlainMonthDayPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
                init.set(TemporalPlainMonthDay::createStructure(init.vm, globalObject, plainMonthDayPrototype));
            });

        m_plainTimeStructure.initLater(
            [] (const Initializer<Structure>& init) {
                auto* globalObject = jsCast<JSGlobalObject*>(init.owner);
                auto* plainTimePrototype = TemporalPlainTimePrototype::create(init.vm, globalObject, TemporalPlainTimePrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
                init.set(TemporalPlainTime::createStructure(init.vm, globalObject, plainTimePrototype));
            });

        m_plainYearMonthStructure.initLater(
            [] (const Initializer<Structure>& init) {
                auto* globalObject = jsCast<JSGlobalObject*>(init.owner);
                auto* plainYearMonthPrototype = TemporalPlainYearMonthPrototype::create(init.vm, globalObject, TemporalPlainYearMonthPrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
                init.set(TemporalPlainYearMonth::createStructure(init.vm, globalObject, plainYearMonthPrototype));
            });

        m_timeZoneStructure.initLater(
            [] (const Initializer<Structure>& init) {
                JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
                TemporalTimeZonePrototype* timeZonePrototype = TemporalTimeZonePrototype::create(init.vm, globalObject, TemporalTimeZonePrototype::createStructure(init.vm, globalObject, globalObject->objectPrototype()));
                init.set(TemporalTimeZone::createStructure(init.vm, globalObject, timeZonePrototype));
            });

        TemporalObject* temporal = TemporalObject::create(vm, TemporalObject::createStructure(vm, this));
        putDirectWithoutTransition(vm, vm.propertyNames->Temporal, temporal, static_cast<unsigned>(PropertyAttribute::DontEnum));
    }
    if (Options::useShadowRealm())
        putDirectWithoutTransition(vm, vm.propertyNames->ShadowRealm, shadowRealmConstructor, static_cast<unsigned>(PropertyAttribute::DontEnum));

    m_moduleLoader.initLater(
        [] (const Initializer<JSModuleLoader>& init) {
            auto catchScope = DECLARE_CATCH_SCOPE(init.vm);
            init.set(JSModuleLoader::create(init.owner, init.vm, JSModuleLoader::createStructure(init.vm, init.owner, jsNull())));
            catchScope.releaseAssertNoException();
        });
    if (Options::exposeInternalModuleLoader())
        putDirectWithoutTransition(vm, vm.propertyNames->Loader, moduleLoader(), static_cast<unsigned>(PropertyAttribute::DontEnum));

    GetterSetter* regExpProtoFlagsGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->flags);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoFlagsGetter)].set(vm, this, regExpProtoFlagsGetter);
    GetterSetter* regExpProtoHasIndicesGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->hasIndices);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoHasIndicesGetter)].set(vm, this, regExpProtoHasIndicesGetter);
    GetterSetter* regExpProtoGlobalGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->global);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoGlobalGetter)].set(vm, this, regExpProtoGlobalGetter);
    GetterSetter* regExpProtoIgnoreCaseGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->ignoreCase);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoIgnoreCaseGetter)].set(vm, this, regExpProtoIgnoreCaseGetter);
    GetterSetter* regExpProtoMultilineGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->multiline);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoMultilineGetter)].set(vm, this, regExpProtoMultilineGetter);
    GetterSetter* regExpProtoSourceGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->source);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoSourceGetter)].set(vm, this, regExpProtoSourceGetter);
    GetterSetter* regExpProtoStickyGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->sticky);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoStickyGetter)].set(vm, this, regExpProtoStickyGetter);
    GetterSetter* regExpProtoUnicodeGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->unicode);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoUnicodeGetter)].set(vm, this, regExpProtoUnicodeGetter);
    GetterSetter* regExpProtoDotAllGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->dotAll);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoDotAllGetter)].set(vm, this, regExpProtoDotAllGetter);
    GetterSetter* regExpProtoUnicodeSetsGetter = getGetterById(this, m_regExpPrototype.get(), vm.propertyNames->unicodeSets);
    catchScope.assertNoException();
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpProtoUnicodeSetsGetter)].set(vm, this, regExpProtoUnicodeSetsGetter);
    JSFunction* regExpSymbolReplace = jsCast<JSFunction*>(m_regExpPrototype->getDirect(vm, vm.propertyNames->replaceSymbol));
    m_regExpProtoSymbolReplace.set(vm, this, regExpSymbolReplace);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpBuiltinExec)].set(vm, this, jsCast<JSFunction*>(m_regExpPrototype->getDirect(vm, vm.propertyNames->exec)));
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpPrototypeSymbolMatch)].set(vm, this, m_regExpPrototype->getDirect(vm, vm.propertyNames->matchSymbol).asCell());
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpPrototypeSymbolReplace)].set(vm, this, m_regExpPrototype->getDirect(vm, vm.propertyNames->replaceSymbol).asCell());

    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::isArray)].set(vm, this, arrayConstructor->getDirect(vm, vm.propertyNames->isArray).asCell());
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::callFunction)].set(vm, this, callFunction);
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::applyFunction)].set(vm, this, applyFunction);

    {
        JSValue hasOwnPropertyFunction = jsCast<JSFunction*>(objectPrototype()->get(this, vm.propertyNames->hasOwnProperty));
        catchScope.assertNoException();
        RELEASE_ASSERT(!!jsDynamicCast<JSFunction*>(hasOwnPropertyFunction));
        m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::hasOwnPropertyFunction)].set(vm, this, jsCast<JSFunction*>(hasOwnPropertyFunction));
    }

#define INIT_PRIVATE_GLOBAL(funcName, code) \
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::funcName)].initLater([] (const Initializer<JSCell>& init) { \
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner); \
            init.set(JSFunction::create(init.vm, globalObject, code ## CodeGenerator(init.vm), globalObject)); \
        });
    JSC_FOREACH_BUILTIN_LINK_TIME_CONSTANT(INIT_PRIVATE_GLOBAL)
#undef INIT_PRIVATE_GLOBAL

    // AsyncFromSyncIterator Helpers
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::asyncFromSyncIteratorCreate)].initLater([](const Initializer<JSCell>& init) {
        init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "asyncFromSyncIteratorCreate"_s, asyncFromSyncIteratorPrivateFuncCreate, ImplementationVisibility::Private, AsyncFromSyncIteratorCreateIntrinsic));
    });

    // PromiseAllContext Helpers
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::promiseAllContextCreate)].initLater([](const Initializer<JSCell>& init) {
        init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "promiseAllContextCreate"_s, promiseAllContextPrivateFuncCreate, ImplementationVisibility::Private, PromiseAllContextCreateIntrinsic));
    });

    // PromiseAllGlobalContext Helpers
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::promiseAllGlobalContextCreate)].initLater([](const Initializer<JSCell>& init) {
        init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 3, "promiseAllGlobalContextCreate"_s, promiseAllGlobalContextPrivateFuncCreate, ImplementationVisibility::Private, PromiseAllGlobalContextCreateIntrinsic));
    });

    // RegExpStringIteratorHelpers
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpStringIteratorCreate)].initLater([](const Initializer<JSCell>& init) {
        init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 4, "regExpStringIteratorCreate"_s, regExpStringIteratorPrivateFuncCreate, ImplementationVisibility::Private, RegExpStringIteratorCreateIntrinsic));
    });

    // WrapForValidIterator Helpers
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::wrapForValidIteratorCreate)].initLater([](const Initializer<JSCell>& init) {
        init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "wrapForValidIteratorCreate"_s, wrapForValidIteratorPrivateFuncCreate, ImplementationVisibility::Private, WrapForValidIteratorCreateIntrinsic));
    });

    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::iteratorHelperCreate)].initLater([](const Initializer<JSCell>& init) {
        init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "iteratorHelperCreate"_s, iteratorHelperPrivateFuncCreate, ImplementationVisibility::Private, IteratorHelperCreateIntrinsic));
    });

    // Global object and function helpers.
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::isFinite)].initLater([] (const Initializer<JSCell>& init) {
        init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "isFinite"_s, globalFuncIsFinite, ImplementationVisibility::Private, GlobalIsFiniteIntrinsic));
    });

    // Map and Set helpers.
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::Set)].initLater([] (const Initializer<JSCell>& init) {
            init.set(jsCast<JSGlobalObject*>(init.owner)->setConstructor());
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::Map)].initLater([] (const Initializer<JSCell>& init) {
            init.set(jsCast<JSGlobalObject*>(init.owner)->mapConstructor());
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::mapIterationNext)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "mapIterationNext"_s, mapPrivateFuncMapIterationNext, ImplementationVisibility::Private, JSMapIterationNextIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::mapIterationEntry)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "mapIterationEntry"_s, mapPrivateFuncMapIterationEntry, ImplementationVisibility::Private, JSMapIterationEntryIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::mapStorage)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "mapStorage"_s, mapPrivateFuncMapStorage, ImplementationVisibility::Private, JSMapStorageIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::mapIteratorNext)].initLater([](const Initializer<JSCell>& init) {
        init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "mapIteratorNext"_s, mapIteratorPrivateFuncMapIteratorNext, ImplementationVisibility::Private, JSMapIteratorNextIntrinsic));
    });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::mapIteratorKey)].initLater([](const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "mapIteratorKey"_s, mapIteratorPrivateFuncMapIteratorKey, ImplementationVisibility::Private, JSMapIteratorKeyIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::mapIteratorValue)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "mapIteratorValue"_s, mapIteratorPrivateFuncMapIteratorValue, ImplementationVisibility::Private, JSMapIteratorValueIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::mapIterationEntryKey)].initLater([](const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "mapIterationEntryKey"_s, mapPrivateFuncMapIterationEntryKey, ImplementationVisibility::Private, JSMapIterationEntryKeyIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::mapIterationEntryValue)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "mapIterationEntryValue"_s, mapPrivateFuncMapIterationEntryValue, ImplementationVisibility::Private, JSMapIterationEntryValueIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::setIterationNext)].initLater([](const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "setIterationNext"_s, setPrivateFuncSetIterationNext, ImplementationVisibility::Private, JSSetIterationNextIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::setIterationEntry)].initLater([](const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "setIterationEntry"_s, setPrivateFuncSetIterationEntry, ImplementationVisibility::Private, JSSetIterationEntryIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::setIterationEntryKey)].initLater([](const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "setIterationEntryKey"_s, setPrivateFuncSetIterationEntryKey, ImplementationVisibility::Private, JSSetIterationEntryKeyIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::setIteratorNext)].initLater([](const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "setIteratorNext"_s, setIteratorPrivateFuncSetIteratorNext, ImplementationVisibility::Private, JSSetIteratorNextIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::setIteratorKey)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "setIteratorKey"_s, setIteratorPrivateFuncSetIteratorKey, ImplementationVisibility::Private, JSSetIteratorKeyIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::setStorage)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "setStorage"_s, setPrivateFuncSetStorage, ImplementationVisibility::Private, JSSetStorageIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::setClone)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "setClone"_s, setPrivateFuncClone, ImplementationVisibility::Private));
        });

    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::importModule)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "importModule"_s, globalFuncImportModule, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::copyDataProperties)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "copyDataProperties"_s, globalFuncCopyDataProperties, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::cloneObject)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "cloneObject"_s, globalFuncCloneObject, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::resolvePromise)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "resolvePromise"_s, resolvePromise, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::rejectPromise)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "rejectPromise"_s, rejectPromise, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::fulfillPromise)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "fulfillPromise"_s, fulfillPromise, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::resolveWithoutPromise)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 4, "resolveWithoutPromise"_s, resolveWithoutPromise, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::rejectWithoutPromise)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 4, "rejectWithoutPromise"_s, rejectWithoutPromise, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::fulfillWithoutPromise)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 4, "fulfillWithoutPromise"_s, fulfillWithoutPromise, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::resolvePromiseWithFirstResolvingFunctionCallCheck)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "resolvePromiseWithFirstResolvingFunctionCallCheck"_s, resolvePromiseWithFirstResolvingFunctionCallCheck, ImplementationVisibility::Private, ResolvePromiseWithFirstResolvingFunctionCallCheckIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::rejectPromiseWithFirstResolvingFunctionCallCheck)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "rejectPromiseWithFirstResolvingFunctionCallCheck"_s, rejectPromiseWithFirstResolvingFunctionCallCheck, ImplementationVisibility::Private, RejectPromiseWithFirstResolvingFunctionCallCheckIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::fulfillPromiseWithFirstResolvingFunctionCallCheck)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "fulfillPromiseWithFirstResolvingFunctionCallCheck"_s, fulfillPromiseWithFirstResolvingFunctionCallCheck, ImplementationVisibility::Private, FulfillPromiseWithFirstResolvingFunctionCallCheckIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::resolveWithoutPromiseForAsyncAwait)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 4, "resolveWithoutPromiseForAsyncAwait"_s, resolveWithoutPromiseForAsyncAwait, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::driveAsyncFunction)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "driveAsyncFunction"_s, driveAsyncFunction, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::awaitValue)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "awaitValue"_s, awaitValue, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::newHandledRejectedPromise)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "newHandledRejectedPromise"_s, newHandledRejectedPromise, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::promiseOnRejectedWithContext)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "promiseOnRejectedWithContext"_s, promiseOnRejectedWithContext, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::promiseAllOnFulfilled)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "promiseAllOnFulfilled"_s, promiseAllOnFulfilled, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::promiseEmptyOnFulfilled)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "promiseEmptyOnFulfilled"_s, promiseEmptyOnFulfilled, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::promiseEmptyOnRejected)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "promiseEmptyOnRejected"_s, promiseEmptyOnRejected, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::promiseResolve)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "promiseResolve"_s, promiseResolve, ImplementationVisibility::Private, PromiseResolveIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::promiseReject)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "promiseReject"_s, promiseReject, ImplementationVisibility::Private, PromiseRejectIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::performPromiseThen)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 5, "performPromiseThen"_s, performPromiseThen, ImplementationVisibility::Private));
        });

    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::makeTypeError)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "makeTypeError"_s, globalFuncMakeTypeError, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::AggregateError)].initLater([] (const Initializer<JSCell>& init) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
            init.set(globalObject->m_aggregateErrorStructure.constructor(globalObject));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::ReferenceError)].initLater([] (const Initializer<JSCell>& init) {
        JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
        init.set(globalObject->m_referenceErrorStructure.constructor(globalObject));
    });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::SuppressedError)].initLater([] (const Initializer<JSCell>& init) {
        JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
        init.set(globalObject->m_suppressedErrorStructure.constructor(globalObject));
    });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::DisposableStack)].initLater([] (const Initializer<JSCell>& init) {
        JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
        init.set(globalObject->m_disposableStackStructure.constructor(globalObject));
    });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::AsyncDisposableStack)].initLater([] (const Initializer<JSCell>& init) {
        JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(init.owner);
        init.set(globalObject->m_asyncDisposableStackStructure.constructor(globalObject));
    });

    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::typedArrayLength)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "typedArrayViewLength"_s, typedArrayViewPrivateFuncLength, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::isTypedArrayView)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "typedArrayViewIsTypedArrayView"_s, typedArrayViewPrivateFuncIsTypedArrayView, ImplementationVisibility::Private, IsTypedArrayViewIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::isSharedTypedArrayView)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "typedArrayViewIsSharedTypedArrayView"_s, typedArrayViewPrivateFuncIsSharedTypedArrayView, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::isResizableOrGrowableSharedTypedArrayView)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "typedArrayViewPrivateFuncIsResizableOrGrowableSharedTypedArrayView"_s, typedArrayViewPrivateFuncIsResizableOrGrowableSharedTypedArrayView, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::typedArrayFromFast)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "typedArrayViewTypedArrayFromFast"_s, typedArrayViewPrivateFuncTypedArrayFromFast, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::arrayFromFastWithoutMapFn)].initLater([] (const Initializer<JSCell>& init) {
        init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "arrayFromFastWithoutMapFn"_s, arrayConstructorPrivateFromFastWithoutMapFn, ImplementationVisibility::Private));
    });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::isDetached)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "typedArrayViewIsDetached"_s, typedArrayViewPrivateFuncIsDetached, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::isBoundFunction)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "isBound"_s, isBoundFunction, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::hasInstanceBoundFunction)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "hasInstanceBound"_s, hasInstanceBoundFunction, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::instanceOf)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "instanceOf"_s, objectPrivateFuncInstanceOf, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::BuiltinLog)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "BuiltinLog"_s, globalFuncBuiltinLog, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::BuiltinDescribe)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "BuiltinDescribe"_s, globalFuncBuiltinDescribe, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::min)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "min"_s, mathProtoFuncMin, ImplementationVisibility::Private, MinIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::repeatCharacter)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "repeatCharacter"_s, stringProtoFuncRepeatCharacter, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::isArraySlow)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "isArraySlow"_s, arrayConstructorPrivateFuncIsArraySlow, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::importInRealm)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "importInRealm"_s, importInRealm, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::evalFunction)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, init.vm.propertyNames->eval.string(), globalFuncEval, ImplementationVisibility::Public));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::evalInRealm)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "evalInRealm"_s, evalInRealm, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::moveFunctionToRealm)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "moveFunctionToRealm"_s, moveFunctionToRealm, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::sameValue)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "is"_s, objectConstructorIs, ImplementationVisibility::Private, ObjectIsIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::setPrototypeDirect)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "setPrototypeDirect"_s, globalFuncSetPrototypeDirect, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::setPrototypeDirectOrThrow)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "setPrototypeDirectOrThrow"_s, globalFuncSetPrototypeDirectOrThrow, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::toIntegerOrInfinity)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "toIntegerOrInfinity"_s, globalFuncToIntegerOrInfinity, ImplementationVisibility::Private, ToIntegerOrInfinityIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::toLength)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "toLength"_s, globalFuncToLength, ImplementationVisibility::Private, ToLengthIntrinsic));
        });

    // RegExp.prototype helpers.
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpCreate)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "esSpecRegExpCreate"_s, esSpecRegExpCreate, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::isRegExp)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "esSpecIsRegExp"_s, esSpecIsRegExp, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpMatchFast)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "regExpMatchFast"_s, regExpProtoFuncMatchFast, ImplementationVisibility::Private, RegExpMatchFastIntrinsic));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::regExpSplitFast)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "regExpSplitFast"_s, regExpProtoFuncSplitFast, ImplementationVisibility::Private));
        });

    // String.prototype helpers.
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::stringIncludesInternal)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "stringIncludesInternal"_s, builtinStringIncludesInternal, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::stringIndexOfInternal)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "stringIndexOfInternal"_s, builtinStringIndexOfInternal, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::stringSplitFast)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "stringSplitFast"_s, stringProtoFuncSplitFast, ImplementationVisibility::Private));
        });

    // Proxy helpers.
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::handleNegativeProxyHasTrapResult)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "handleNegativeProxyHasTrapResult"_s, globalFuncHandleNegativeProxyHasTrapResult, ImplementationVisibility::Private));
        });

    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::handleProxyGetTrapResult)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 3, "handleProxyGetTrapResult"_s, globalFuncHandleProxyGetTrapResult, ImplementationVisibility::Private));
        });

    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::handlePositiveProxySetTrapResult)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 3, "handlePositiveProxySetTrapResult"_s, globalFuncHandlePositiveProxySetTrapResult, ImplementationVisibility::Private));
        });

    // PrivateSymbols / PrivateNames
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::createPrivateSymbol)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "createPrivateSymbol"_s, createPrivateSymbol, ImplementationVisibility::Private));
        });

    // JSON helpers
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::jsonParse)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "parse"_s, jsonParse, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::jsonStringify)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 2, "stringify"_s, jsonStringify, ImplementationVisibility::Private));
        });

    // ShadowRealms
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::createRemoteFunction)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "createRemoteFunction"_s, createRemoteFunction, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::isRemoteFunction)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 0, "isRemoteFunction"_s, isRemoteFunction, ImplementationVisibility::Private));
        });

#if ENABLE(WEBASSEMBLY)
    // WebAssembly Streaming API
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::webAssemblyCompileStreamingInternal)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "compileStreamingInternal"_s, webAssemblyCompileStreamingInternal, ImplementationVisibility::Private));
        });
    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::webAssemblyInstantiateStreamingInternal)].initLater([] (const Initializer<JSCell>& init) {
            init.set(JSFunction::create(init.vm, jsCast<JSGlobalObject*>(init.owner), 1, "instantiateStreamingInternal"_s, webAssemblyInstantiateStreamingInternal, ImplementationVisibility::Private));
        });
#endif

    m_linkTimeConstants[static_cast<unsigned>(LinkTimeConstant::emptyPropertyNameEnumerator)].initLater([] (const Initializer<JSCell>& init) {
        init.set(init.vm.emptyPropertyNameEnumerator());
    });

    m_performProxyObjectHasFunction.set(vm, this, jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::performProxyObjectHas)));
    m_performProxyObjectHasByValFunction.set(vm, this, jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::performProxyObjectHasByVal)));
    m_performProxyObjectGetFunction.set(vm, this, jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::performProxyObjectGet)));
    m_performProxyObjectGetByValFunction.set(vm, this, jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::performProxyObjectGetByVal)));
    m_performProxyObjectSetStrictFunction.set(vm, this, jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::performProxyObjectSetStrict)));
    m_performProxyObjectSetSloppyFunction.set(vm, this, jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::performProxyObjectSetSloppy)));
    m_performProxyObjectSetByValStrictFunction.set(vm, this, jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::performProxyObjectSetByValStrict)));
    m_performProxyObjectSetByValSloppyFunction.set(vm, this, jsCast<JSFunction*>(linkTimeConstant(LinkTimeConstant::performProxyObjectSetByValSloppy)));

    if (Options::exposeProfilersOnGlobalObject()) {
#if ENABLE(SAMPLING_PROFILER)
        putDirectWithoutTransition(vm, Identifier::fromString(vm, "__enableSamplingProfiler"_s), JSFunction::create(vm, this, 1, "enableSamplingProfiler"_s, enableSamplingProfiler, ImplementationVisibility::Public), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
        putDirectWithoutTransition(vm, Identifier::fromString(vm, "__disableSamplingProfiler"_s), JSFunction::create(vm, this, 1, "disableSamplingProfiler"_s, disableSamplingProfiler, ImplementationVisibility::Public), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
        putDirectWithoutTransition(vm, Identifier::fromString(vm, "__dumpAndClearSamplingProfilerSamples"_s), JSFunction::create(vm, this, 1, "dumpAndClearSamplingProfilerSamples"_s, dumpAndClearSamplingProfilerSamples, ImplementationVisibility::Public), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
#endif
        putDirectWithoutTransition(vm, Identifier::fromString(vm, "__enableSuperSampler"_s), JSFunction::create(vm, this, 1, "enableSuperSampler"_s, enableSuperSampler, ImplementationVisibility::Public), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
        putDirectWithoutTransition(vm, Identifier::fromString(vm, "__disableSuperSampler"_s), JSFunction::create(vm, this, 1, "disableSuperSampler"_s, disableSuperSampler, ImplementationVisibility::Public), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);

        putDirectWithoutTransition(vm, Identifier::fromString(vm, "__tracePointStart"_s), JSFunction::create(vm, this, 4, "tracePointStart"_s, tracePointStart, ImplementationVisibility::Public), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
        putDirectWithoutTransition(vm, Identifier::fromString(vm, "__tracePointStop"_s), JSFunction::create(vm, this, 4, "tracePointStop"_s, tracePointStop, ImplementationVisibility::Public), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
        putDirectWithoutTransition(vm, Identifier::fromString(vm, "__signpostStart"_s), JSFunction::create(vm, this, 1, "signpostStart"_s, signpostStart, ImplementationVisibility::Public), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
        putDirectWithoutTransition(vm, Identifier::fromString(vm, "__signpostStop"_s), JSFunction::create(vm, this, 1, "signpostStop"_s, signpostStop, ImplementationVisibility::Public), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    }

    initStaticGlobals(vm);
    
    if (Options::useDollarVM()) [[unlikely]]
        exposeDollarVM(vm);

#if ENABLE(WEBASSEMBLY)
    if (Wasm::isSupported()) {
        m_webAssemblyModuleRecordStructure.initLater(
            [] (const Initializer<Structure>& init) {
                init.set(WebAssemblyModuleRecord::createStructure(init.vm, init.owner, init.owner->m_objectPrototype.get()));
            });
        m_webAssemblyFunctionStructure.initLater(
            [] (const Initializer<Structure>& init) {
                init.set(WebAssemblyFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
            });
        m_webAssemblyWrapperFunctionStructure.initLater(
            [] (const Initializer<Structure>& init) {
                init.set(WebAssemblyWrapperFunction::createStructure(init.vm, init.owner, init.owner->m_functionPrototype.get()));
            });
        m_webAssemblyJSTag.initLater(
            [] (const Initializer<JSWebAssemblyTag>& init) {
                init.set(JSWebAssemblyTag::create(init.vm, init.owner, init.owner->webAssemblyTagStructure(), Wasm::Tag::jsExceptionTag()));
            });
        auto* webAssembly = JSWebAssembly::create(vm, this, JSWebAssembly::createStructure(vm, this, m_objectPrototype.get()));
        putDirectWithoutTransition(vm, Identifier::fromString(vm, "WebAssembly"_s), webAssembly, static_cast<unsigned>(PropertyAttribute::DontEnum));

#define CREATE_WEBASSEMBLY_PROTOTYPE(capitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) \
    if (featureFlag) {\
        m_ ## properName ## Structure.initLater(\
            [] (LazyClassStructure::Initializer& init) { \
                init.setPrototype(capitalName##Prototype::create(init.vm, init.global, capitalName##Prototype::createStructure(init.vm, init.global, init.global->prototypeBase ## Prototype()))); \
                init.setStructure(instanceType::createStructure(init.vm, init.global, init.prototype)); \
                auto* constructorPrototype = strcmp(#prototypeBase, "error") == 0 ? init.global->m_errorStructure.constructor(init.global) : init.global->functionPrototype(); \
                init.setConstructor(capitalName ## Constructor::create(init.vm, capitalName ## Constructor::createStructure(init.vm, init.global, constructorPrototype), jsCast<capitalName ## Prototype*>(init.prototype))); \
            }); \
    }

        FOR_EACH_WEBASSEMBLY_CONSTRUCTOR_TYPE(CREATE_WEBASSEMBLY_PROTOTYPE)

#undef CREATE_WEBASSEMBLY_PROTOTYPE
    }
#endif // ENABLE(WEBASSEMBLY)

    // Detect property change.
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, arrayIteratorPrototype, vm.propertyNames->next), m_arrayIteratorProtocolWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, this->arrayPrototype(), vm.propertyNames->iteratorSymbol), m_arrayIteratorProtocolWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, this->arrayPrototype(), vm.propertyNames->join), m_arrayJoinWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, this->arrayPrototype(), vm.propertyNames->toString), m_arrayToStringWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, mapIteratorPrototype, vm.propertyNames->next), m_mapIteratorProtocolWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, setIteratorPrototype, vm.propertyNames->next), m_setIteratorProtocolWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, m_stringIteratorPrototype.get(), vm.propertyNames->next), m_stringIteratorProtocolWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, m_stringPrototype.get(), vm.propertyNames->iteratorSymbol), m_stringIteratorProtocolWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, m_stringPrototype.get(), vm.propertyNames->toString), m_stringToStringWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, m_stringPrototype.get(), vm.propertyNames->valueOf), m_stringValueOfWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, m_regExpPrototype.get(), vm.propertyNames->exec), m_regExpPrimordialPropertiesWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, m_regExpPrototype.get(), vm.propertyNames->flags), m_regExpPrimordialPropertiesWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, m_regExpPrototype.get(), vm.propertyNames->dotAll), m_regExpPrimordialPropertiesWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, m_regExpPrototype.get(), vm.propertyNames->global), m_regExpPrimordialPropertiesWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, m_regExpPrototype.get(), vm.propertyNames->hasIndices), m_regExpPrimordialPropertiesWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, m_regExpPrototype.get(), vm.propertyNames->ignoreCase), m_regExpPrimordialPropertiesWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, m_regExpPrototype.get(), vm.propertyNames->multiline), m_regExpPrimordialPropertiesWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, m_regExpPrototype.get(), vm.propertyNames->sticky), m_regExpPrimordialPropertiesWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, m_regExpPrototype.get(), vm.propertyNames->unicode), m_regExpPrimordialPropertiesWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, m_regExpPrototype.get(), vm.propertyNames->unicodeSets), m_regExpPrimordialPropertiesWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, m_regExpPrototype.get(), vm.propertyNames->replaceSymbol), m_regExpPrimordialPropertiesWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, jsSetPrototype(), vm.propertyNames->has), m_setPrimordialPropertiesWatchpointSet);
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, jsSetPrototype(), vm.propertyNames->keys), m_setPrimordialPropertiesWatchpointSet);

    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, promisePrototype(), vm.propertyNames->then), m_promiseThenWatchpointSet);

    // Detect property absence.
    installObjectAdaptiveStructureWatchpoint(setupAbsenceAdaptiveWatchpoint(this, m_stringPrototype.get(), vm.propertyNames->replaceSymbol, objectPrototype()), m_stringSymbolReplaceWatchpointSet);
    installObjectAdaptiveStructureWatchpoint(setupAbsenceAdaptiveWatchpoint(this, m_objectPrototype.get(), vm.propertyNames->replaceSymbol, nullptr), m_stringSymbolReplaceWatchpointSet);
    installObjectAdaptiveStructureWatchpoint(setupAbsenceAdaptiveWatchpoint(this, m_stringPrototype.get(), vm.propertyNames->toPrimitiveSymbol, objectPrototype()), m_stringSymbolToPrimitiveWatchpointSet);
    installObjectAdaptiveStructureWatchpoint(setupAbsenceAdaptiveWatchpoint(this, m_objectPrototype.get(), vm.propertyNames->toPrimitiveSymbol, nullptr), m_stringSymbolToPrimitiveWatchpointSet);
    installObjectAdaptiveStructureWatchpoint(setupAbsenceAdaptiveWatchpoint(this, m_arrayPrototype.get(), vm.propertyNames->toPrimitiveSymbol, objectPrototype()), m_arraySymbolToPrimitiveWatchpointSet);
    installObjectAdaptiveStructureWatchpoint(setupAbsenceAdaptiveWatchpoint(this, m_objectPrototype.get(), vm.propertyNames->toPrimitiveSymbol, nullptr), m_arraySymbolToPrimitiveWatchpointSet);
    installObjectAdaptiveStructureWatchpoint(setupAbsenceAdaptiveWatchpoint(this, m_arrayPrototype.get(), vm.propertyNames->negativeOneIdentifier, objectPrototype()), m_arrayNegativeOneWatchpointSet);
    installObjectAdaptiveStructureWatchpoint(setupAbsenceAdaptiveWatchpoint(this, m_objectPrototype.get(), vm.propertyNames->negativeOneIdentifier, nullptr), m_arrayNegativeOneWatchpointSet);
    installObjectAdaptiveStructureWatchpoint(setupAbsenceAdaptiveWatchpoint(this, m_arrayPrototype.get(), vm.propertyNames->isConcatSpreadableSymbol, objectPrototype()), m_arrayIsConcatSpreadableWatchpointSet);
    installObjectAdaptiveStructureWatchpoint(setupAbsenceAdaptiveWatchpoint(this, m_objectPrototype.get(), vm.propertyNames->isConcatSpreadableSymbol, nullptr), m_arrayIsConcatSpreadableWatchpointSet);

    // Array Species watchpoint.
    {
        RELEASE_ASSERT(!m_arrayPrototypeConstructorWatchpoint);
        RELEASE_ASSERT(!m_arrayConstructorSpeciesWatchpoint);
        tryInstallSpeciesWatchpoint(this->arrayPrototype(), arrayConstructor, m_arrayPrototypeConstructorWatchpoint, m_arrayConstructorSpeciesWatchpoint, m_arraySpeciesWatchpointSet, HasSpeciesProperty::Yes, arraySpeciesGetterSetter());
        catchScope.assertNoException();
    }
    {
        tryInstallSpeciesWatchpoint(this->promisePrototype(), promiseConstructor, m_promisePrototypeConstructorWatchpoint, m_promiseConstructorSpeciesWatchpoint, m_promiseSpeciesWatchpointSet, HasSpeciesProperty::Yes, promiseSpeciesGetterSetter());
        catchScope.assertNoException();
    }

    installSaneChainWatchpoints();

    // Unfortunately, the prototype objects of the builtin objects can be touched from concurrent compilers. So eagerly initialize them only if we use JIT.
    if (Options::useJIT()) {
        this->booleanPrototype();
        this->numberPrototype();
        this->symbolPrototype();
    }

    fixupPrototypeChainWithObjectPrototype(vm);

    if (Options::alwaysHaveABadTime()) [[unlikely]]
        this->haveABadTime(vm);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

bool JSGlobalObject::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(cell);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));

    if (isThisValueAltered(slot, thisObject)) [[unlikely]] {
        SymbolTableEntry::Fast entry = thisObject->symbolTable()->get(propertyName.uid());
        if (!entry.isNull()) {
            if (entry.isReadOnly())
                return typeError(globalObject, scope, slot.isStrictMode(), ReadonlyPropertyWriteError);
            RELEASE_AND_RETURN(scope, JSObject::definePropertyOnReceiver(globalObject, propertyName, value, slot));
        }
        RELEASE_AND_RETURN(scope, Base::put(thisObject, globalObject, propertyName, value, slot));
    }

    bool shouldThrowReadOnlyError = slot.isStrictMode();
    bool ignoreReadOnlyErrors = false;
    bool putResult = false;
    bool done = symbolTablePutTouchWatchpointSet(thisObject, globalObject, propertyName, value, shouldThrowReadOnlyError, ignoreReadOnlyErrors, putResult);
    EXCEPTION_ASSERT((!!scope.exception() == (done && !putResult)) || !shouldThrowReadOnlyError);
    if (done)
        return putResult;
    RELEASE_AND_RETURN(scope, Base::put(thisObject, globalObject, propertyName, value, slot));
}

bool JSGlobalObject::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(object);

    SymbolTableEntry entry;
    PropertyDescriptor currentDescriptor;
    if (symbolTableGet(thisObject, propertyName, entry, currentDescriptor)) {
        bool isExtensible = false; // ignored since current descriptor is present
        bool isCurrentDefined = true;
        bool isCompatibleDescriptor = validateAndApplyPropertyDescriptor(globalObject, nullptr, propertyName, isExtensible, descriptor, isCurrentDefined, currentDescriptor, shouldThrow);
        RETURN_IF_EXCEPTION(scope, false);
        if (!isCompatibleDescriptor)
            return false;

        if (descriptor.value()) {
            bool ignoreReadOnlyErrors = true;
            bool putResult = false;
            if (symbolTablePutTouchWatchpointSet(thisObject, globalObject, propertyName, descriptor.value(), shouldThrow, ignoreReadOnlyErrors, putResult))
                ASSERT(putResult);
            RETURN_IF_EXCEPTION(scope, false);
        }
        if (descriptor.writablePresent() && !descriptor.writable() && !entry.isReadOnly()) {
            entry.setReadOnly();
            thisObject->symbolTable()->set(propertyName.uid(), entry);
            thisObject->varReadOnlyWatchpointSet().fireAll(vm, "GlobalVar was redefined as ReadOnly");
        }
        return true;
    }

    RELEASE_AND_RETURN(scope, Base::defineOwnProperty(thisObject, globalObject, propertyName, descriptor, shouldThrow));
}

// https://tc39.es/ecma262/#sec-candeclareglobalfunction
bool JSGlobalObject::canDeclareGlobalFunction(const Identifier& ident)
{
    auto scope = DECLARE_THROW_SCOPE(vm());

    PropertySlot slot(this, PropertySlot::InternalMethodType::GetOwnProperty);
    bool hasProperty = getOwnPropertySlot(this, this, ident, slot);
    RETURN_IF_EXCEPTION(scope, { });
    if (!hasProperty) [[likely]]
        return isStructureExtensible();

    bool isConfigurable = !(slot.attributes() & PropertyAttribute::DontDelete);
    if (isConfigurable)
        return true;
    bool isDataDescriptor = !(slot.attributes() & (PropertyAttribute::Accessor | PropertyAttribute::CustomAccessor));
    bool isWritableAndEnumerable = !(slot.attributes() & (PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum));
    return isDataDescriptor && isWritableAndEnumerable;
}

// https://tc39.es/ecma262/#sec-createglobalfunctionbinding
template<BindingCreationContext context>
void JSGlobalObject::createGlobalFunctionBinding(const Identifier& ident)
{
    VM& vm = this->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    PropertySlot slot(this, PropertySlot::InternalMethodType::GetOwnProperty);
    bool hasProperty = getOwnPropertySlot(this, this, ident, slot);
    RETURN_IF_EXCEPTION(scope, void());
    if (hasProperty) [[unlikely]] {
        if (slot.attributes() & PropertyAttribute::DontDelete) {
            ASSERT(!(slot.attributes() & PropertyAttribute::ReadOnly));
            // Nothing to do here: there is either a symbol table entry or non-configurable writable property
            // on the structure that will be updated with real function by put_to_scope.
        } else {
            unsigned newAttributes = 0;
            if constexpr (context == BindingCreationContext::Global)
                newAttributes |= PropertyAttribute::DontDelete;
            putDirect(vm, ident, jsUndefined(), newAttributes);
        }
    } else {
        ASSERT(isStructureExtensible());
        if constexpr (context == BindingCreationContext::Global)
            addSymbolTableEntry(ident);
        else
            putDirect(vm, ident, jsUndefined());
    }
}

template void JSGlobalObject::createGlobalFunctionBinding<BindingCreationContext::Global>(const Identifier&);
template void JSGlobalObject::createGlobalFunctionBinding<BindingCreationContext::Eval>(const Identifier&);

void JSGlobalObject::addSymbolTableEntry(const Identifier& ident)
{
    ConcurrentJSLocker locker(symbolTable()->m_lock);
    ASSERT(!symbolTable()->contains(locker, ident.impl()));
    
    ScopeOffset offset = symbolTable()->takeNextScopeOffset(locker);
    SymbolTableEntry newEntry(VarOffset(offset), 0);
    newEntry.prepareToWatch();
    symbolTable()->add(locker, ident.impl(), WTFMove(newEntry));
    
    ScopeOffset offsetForAssert = addVariables(1, jsUndefined());
    RELEASE_ASSERT(offsetForAssert == offset);
}

void JSGlobalObject::setGlobalScopeExtension(JSScope* scope)
{
    m_globalScopeExtension.set(vm(), this, scope);
}

void JSGlobalObject::clearGlobalScopeExtension()
{
    m_globalScopeExtension.clear();
}

void JSGlobalObject::notifyArrayBufferDetachingSlow()
{
    m_arrayBufferDetachWatchpointSet->fireAll(vm(), "ArrayBuffer detached");
}

static inline JSObject* lastInPrototypeChain(JSObject* object)
{
    JSObject* o = object;
    while (o->getPrototypeDirect().isObject())
        o = asObject(o->getPrototypeDirect());
    return o;
}

// Private namespace for helpers for JSGlobalObject::haveABadTime()
namespace {

class GlobalObjectDependencyFinder : public MarkedBlock::VoidFunctor {
public:
    GlobalObjectDependencyFinder() = default;

    IterationStatus operator()(HeapCell*, HeapCell::Kind) const;

    void addDependency(JSGlobalObject* key, JSGlobalObject* dependent);
    UncheckedKeyHashSet<JSGlobalObject*>* dependentsFor(JSGlobalObject* key);

private:
    void visit(JSObject*);

    UncheckedKeyHashMap<JSGlobalObject*, UncheckedKeyHashSet<JSGlobalObject*>> m_dependencies;
};

inline void GlobalObjectDependencyFinder::addDependency(JSGlobalObject* key, JSGlobalObject* dependent)
{
    auto keyResult = m_dependencies.add(key, UncheckedKeyHashSet<JSGlobalObject*>());
    keyResult.iterator->value.add(dependent);
}

inline UncheckedKeyHashSet<JSGlobalObject*>* GlobalObjectDependencyFinder::dependentsFor(JSGlobalObject* key)
{
    auto iterator = m_dependencies.find(key);
    if (iterator == m_dependencies.end())
        return nullptr;
    return &iterator->value;
}

inline void GlobalObjectDependencyFinder::visit(JSObject* object)
{
    if (!object->mayBePrototype())
        return;

    JSObject* current = object;
    JSGlobalObject* objectGlobalObject = object->globalObject();
    do {
        JSValue prototypeValue = current->getPrototypeDirect();
        if (prototypeValue.isNull())
            return;
        current = asObject(prototypeValue);

        JSGlobalObject* protoGlobalObject = current->globalObject();
        if (protoGlobalObject != objectGlobalObject)
            addDependency(protoGlobalObject, objectGlobalObject);
    } while (true);
}

IterationStatus GlobalObjectDependencyFinder::operator()(HeapCell* cell, HeapCell::Kind kind) const
{
    if (isJSCellKind(kind) && static_cast<JSCell*>(cell)->isObject()) {
        // FIXME: This const_cast exists because this isn't a C++ lambda.
        // https://bugs.webkit.org/show_bug.cgi?id=159644
        const_cast<GlobalObjectDependencyFinder*>(this)->visit(jsCast<JSObject*>(static_cast<JSCell*>(cell)));
    }
    return IterationStatus::Continue;
}

enum class BadTimeFinderMode {
    SingleGlobal,
    MultipleGlobals
};

template<BadTimeFinderMode mode>
class ObjectsWithBrokenIndexingFinder : public MarkedBlock::VoidFunctor {
public:
    ObjectsWithBrokenIndexingFinder(Vector<JSObject*>&, JSGlobalObject*);
    ObjectsWithBrokenIndexingFinder(Vector<JSObject*>&, UncheckedKeyHashSet<JSGlobalObject*>&);

    bool needsMultiGlobalsScan() const { return m_needsMultiGlobalsScan; }
    IterationStatus operator()(HeapCell*, HeapCell::Kind) const;

private:
    IterationStatus visit(JSObject*);

    Vector<JSObject*>& m_foundObjects;
    JSGlobalObject* const m_globalObject { nullptr }; // Only used for SingleBadTimeGlobal mode.
    UncheckedKeyHashSet<JSGlobalObject*>* m_globalObjects { nullptr }; // Only used for BadTimeGlobalGraph mode;
    bool m_needsMultiGlobalsScan { false };
};

template<>
ObjectsWithBrokenIndexingFinder<BadTimeFinderMode::SingleGlobal>::ObjectsWithBrokenIndexingFinder(Vector<JSObject*>& foundObjects, JSGlobalObject* globalObject)
    : m_foundObjects(foundObjects)
    , m_globalObject(globalObject)
{
}

template<>
ObjectsWithBrokenIndexingFinder<BadTimeFinderMode::MultipleGlobals>::ObjectsWithBrokenIndexingFinder(Vector<JSObject*>& foundObjects, UncheckedKeyHashSet<JSGlobalObject*>& globalObjects)
    : m_foundObjects(foundObjects)
    , m_globalObjects(&globalObjects)
{
}

inline bool hasBrokenIndexing(IndexingType type)
{
    return type && !hasSlowPutArrayStorage(type);
}

inline bool hasBrokenIndexing(JSObject* object)
{
    IndexingType type = object->indexingType();
    return hasBrokenIndexing(type);
}

template<BadTimeFinderMode mode>
inline IterationStatus ObjectsWithBrokenIndexingFinder<mode>::visit(JSObject* object)
{
    // We only want to have a bad time in the affected global object, not in the entire
    // VM. But we have to be careful, since there may be objects that claim to belong to
    // a different global object that have prototypes from our global object.
    auto isInAffectedGlobalObject = [&] (JSObject* object) {
        JSGlobalObject* objectGlobalObject { nullptr };
        bool objectMayBePrototype { false };

        if (mode == BadTimeFinderMode::SingleGlobal) {
            objectGlobalObject = object->globalObject();
            if (objectGlobalObject == m_globalObject)
                return true;

            objectMayBePrototype = object->mayBePrototype();
        }

        for (JSObject* current = object; ;) {
            JSGlobalObject* currentGlobalObject = current->globalObject();
            if (mode == BadTimeFinderMode::SingleGlobal) {
                if (objectMayBePrototype && currentGlobalObject != objectGlobalObject)
                    m_needsMultiGlobalsScan = true;
                if (currentGlobalObject == m_globalObject)
                    return true;
            } else {
                if (m_globalObjects->contains(currentGlobalObject))
                    return true;
            }

            JSValue prototypeValue = current->getPrototypeDirect();
            if (prototypeValue.isNull())
                return false;
            current = asObject(prototypeValue);
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto checkStructureHasRelevantGlobalObject = [&](Structure* structure) -> bool {
        if (hasBrokenIndexing(structure->indexingType())) {
            bool isRelevantGlobalObject =
                (mode == BadTimeFinderMode::SingleGlobal
                    ? m_globalObject == structure->globalObject()
                    : m_globalObjects->contains(structure->globalObject()))
                || (structure->hasMonoProto() && !structure->storedPrototype().isNull() && isInAffectedGlobalObject(asObject(structure->storedPrototype())));
            return isRelevantGlobalObject;
        }
        return false;
    };

    if (object->inherits<JSFunction>()) {
        JSFunction* function = jsCast<JSFunction*>(object);
        if (FunctionRareData* rareData = function->rareData()) {
            // We only use this to cache JSFinalObjects. They do not start off with a broken indexing type.
            ASSERT(!(rareData->objectAllocationStructure() && hasBrokenIndexing(rareData->objectAllocationStructure()->indexingType())));

            if (Structure* structure = rareData->internalFunctionAllocationStructure()) {
                bool isRelevantGlobalObject = checkStructureHasRelevantGlobalObject(structure);
                if (mode == BadTimeFinderMode::SingleGlobal && m_needsMultiGlobalsScan)
                    return IterationStatus::Done; // Bailing early and let the MultipleGlobals path handle everything.
                if (isRelevantGlobalObject)
                    rareData->clearInternalFunctionAllocationProfile("have a bad time breaking internal function allocation");
            }
        }
    }

    if (object->inherits<JSGlobalObject>()) {
        JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(object);
        // If this globalObject is already having a bad time, then structures in its StructureCache
        // does not affect on this new JSGlobalObject's haveABadTime since they are already slow mode.
        if (!globalObject->isHavingABadTime()) {
            VM& vm = globalObject->vm();
            ASSERT(vm.heap.isDeferred());
            bool willClear = false;
            globalObject->structureCache().forEach([&](Structure* structure) {
                bool isRelevantGlobalObject = checkStructureHasRelevantGlobalObject(structure);
                if (mode == BadTimeFinderMode::SingleGlobal && m_needsMultiGlobalsScan)
                    return IterationStatus::Done;
                if (isRelevantGlobalObject)
                    willClear = true;
                return IterationStatus::Continue;
            });
            if (mode == BadTimeFinderMode::SingleGlobal && m_needsMultiGlobalsScan)
                return IterationStatus::Done; // Bailing early and let the MultipleGlobals path handle everything.

            // StructureCache contains Structures which is no longer valid after relevant JSGlobalObject's haveABadTime.
            // We do not make such a JSGlobalObject status haveABadTime since still its own objects are intact.
            if (willClear)
                globalObject->clearStructureCache(vm);
        }
    }

    // Run this filter first, since it's cheap, and ought to filter out a lot of objects.
    if (!hasBrokenIndexing(object))
        return IterationStatus::Continue;

    if (isInAffectedGlobalObject(object))
        m_foundObjects.append(object);

    if (mode == BadTimeFinderMode::SingleGlobal && m_needsMultiGlobalsScan)
        return IterationStatus::Done; // Bailing early and let the MultipleGlobals path handle everything.

    return IterationStatus::Continue;
}

template<BadTimeFinderMode mode>
IterationStatus ObjectsWithBrokenIndexingFinder<mode>::operator()(HeapCell* cell, HeapCell::Kind kind) const
{
    if (isJSCellKind(kind) && static_cast<JSCell*>(cell)->isObject()) {
        // FIXME: This const_cast exists because this isn't a C++ lambda.
        // https://bugs.webkit.org/show_bug.cgi?id=159644
        return const_cast<ObjectsWithBrokenIndexingFinder*>(this)->visit(jsCast<JSObject*>(static_cast<JSCell*>(cell)));
    }
    return IterationStatus::Continue;
}

} // end private namespace for helpers for JSGlobalObject::haveABadTime()

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

void JSGlobalObject::fireWatchpointAndMakeAllArrayStructuresSlowPut(VM& vm)
{
    if (isHavingABadTime())
        return;

    // This must happen first, because the compiler thread may race with haveABadTime.
    // Let R_BT, W_BT <- Read/Fire the watchpoint, R_SC, W_SC <- Read/clear the structure cache.
    // The possible interleavings are:
    // R_BT, R_SC, W_SC, W_BT: Compiler thread installs a watchpoint, and the code is discarded.
    // R_BT, W_SC, R_SC, W_BT: ^ Same
    // R_BT, W_SC, W_BT, W_SC: ^ Same
    // W_SC, R_BT, R_SC, W_BT: ^ Same
    // W_SC, R_BT, W_BT, R_SC: ^ Same
    // W_SC, W_BT, R_BT, R_SC: No watchpoint is installed, but we could not see old structures from the cache.
    clearStructureCache(vm);

    // Make sure that all JSArray allocations that load the appropriate structure from
    // this object now load a structure that uses SlowPut.
    for (unsigned i = 0; i < NumberOfArrayIndexingModes; ++i)
        m_arrayStructureForIndexingShapeDuringAllocation[i].set(vm, this, originalArrayStructureForIndexingType(ArrayWithSlowPutArrayStorage));

    // Same for any special array structures.
    Structure* slowPutStructure;
    slowPutStructure = createRegExpMatchesArraySlowPutStructure(vm, this);
    m_regExpMatchesArrayStructure.set(vm, this, slowPutStructure);
    slowPutStructure = createRegExpMatchesArrayWithIndicesSlowPutStructure(vm, this);
    m_regExpMatchesArrayWithIndicesStructure.set(vm, this, slowPutStructure);
    slowPutStructure = createRegExpMatchesIndicesArraySlowPutStructure(vm, this);
    m_regExpMatchesIndicesArrayStructure.set(vm, this, slowPutStructure);
    slowPutStructure = ClonedArguments::createSlowPutStructure(vm, this, m_objectPrototype.get());
    m_clonedArgumentsStructure.set(vm, this, slowPutStructure);

    // Make sure that all allocations or indexed storage transitions that are inlining
    // the assumption that it's safe to transition to a non-SlowPut array storage don't
    // do so anymore.
    // Note: we are deliberately firing the watchpoint here at the end only after
    // making all the array structures SlowPut. This ensures that the concurrent
    // JIT threads will always get the SlowPut versions of the structures if
    // isHavingABadTime() returns true. The concurrent JIT relies on this.
    m_havingABadTimeWatchpointSet->fireAll(vm, "Having a bad time");
    ASSERT(isHavingABadTime()); // The watchpoint is what tells us that we're having a bad time.
};

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

void JSGlobalObject::clearStructureCache(VM& vm)
{
    m_structureCache.clear(); // We may be caching array structures in here.
    m_structureCacheClearedWatchpointSet.fireAll(vm, "Clearing StructureCache");
}

void JSGlobalObject::haveABadTime(VM& vm)
{
    ASSERT(&vm == &this->vm());
    
    if (isHavingABadTime())
        return;

    DeferGC deferGC(vm);

    // Consider the following objects and prototype chains:
    //    O (of global G1) -> A (of global G1)
    //    B (of global G2) where G2 has a bad time
    //
    // If we set B as the prototype of A, G1 will need to have a bad time.
    // See comments in Structure::mayInterceptIndexedAccesses() for why.
    //
    // Now, consider the following objects and prototype chains:
    //    O1 (of global G1) -> A1 (of global G1) -> B1 (of global G2)
    //    O2 (of global G2) -> A2 (of global G2)
    //    B2 (of global G3) where G3 has a bad time.
    //
    // G1 and G2 does not have a bad time, but G3 already has a bad time.
    // If we set B2 as the prototype of A2, then G2 needs to have a bad time.
    // Note that by induction, G1 also now needs to have a bad time because of
    // O1 -> A1 -> B1.
    //
    // We describe this as global G1 being affected by global G2, and G2 by G3.
    // Similarly, we say that G1 is dependent on G2, and G2 on G3.
    // Hence, when G3 has a bad time, we need to ensure that all globals that
    // are transitively dependent on it also have a bad time (G2 and G1 in this
    // example).
    //
    // Apart from clearing the VM structure cache above, there are 2 more things
    // that we have to do when globals have a bad time:
    // 1. For each affected global:
    //    a. Fire its HaveABadTime watchpoint.
    //    b. Convert all of its array structures to SlowPutArrayStorage.
    // 2. Make sure that all affected objects  switch to the slow kind of
    //    indexed storage. An object is considered to be affected if it has
    //    indexed storage and has a prototype object which may have indexed
    //    accessors. If the prototype object belongs to a global having a bad
    //    time, then the prototype object is considered to possibly have indexed
    //    accessors. See comments in Structure::mayInterceptIndexedAccesses()
    //    for details.
    //
    // Note: step 1 must be completed before step 2 because step 2 relies on
    // the HaveABadTime watchpoint having already been fired on all affected
    // globals.
    //
    // In the common case, only this global will start having a bad time here,
    // and no other globals are affected by it. So, we first proceed on this assumption
    // with a simpler ObjectsWithBrokenIndexingFinder scan to find heap objects
    // affected by this global that need to be converted to SlowPutArrayStorage.
    // We'll also have the finder check for the presence of other global objects
    // depending on this one.
    //
    // If we do discover other globals depending on this one, we'll abort this
    // first ObjectsWithBrokenIndexingFinder scan because it will be insufficient
    // to find all affected objects that need to be converted to SlowPutArrayStorage.
    // It also does not make dependent globals have a bad time. Instead, we'll
    // take a more comprehensive approach of first creating a dependency graph
    // between globals, and then using that graph to determine all affected
    // globals and objects. With that, we can make all affected globals have a
    // bad time, and convert all affected objects to SlowPutArrayStorage.

    fireWatchpointAndMakeAllArrayStructuresSlowPut(vm); // Step 1 above.
    
    Vector<JSObject*> foundObjects;
    ObjectsWithBrokenIndexingFinder<BadTimeFinderMode::SingleGlobal> finder(foundObjects, this);
    {
        HeapIterationScope iterationScope(vm.heap);
        vm.heap.objectSpace().forEachLiveCell(iterationScope, finder); // Attempt step 2 above.
    }

    if (finder.needsMultiGlobalsScan()) {
        foundObjects.clear();

        // Find all globals that will also have a bad time as a side effect of
        // this global having a bad time.
        GlobalObjectDependencyFinder dependencies;
        {
            HeapIterationScope iterationScope(vm.heap);
            vm.heap.objectSpace().forEachLiveCell(iterationScope, dependencies);
        }

        UncheckedKeyHashSet<JSGlobalObject*> globalsHavingABadTime;
        Deque<JSGlobalObject*> globals;

        globals.append(this);
        while (!globals.isEmpty()) {
            JSGlobalObject* global = globals.takeFirst();
            global->fireWatchpointAndMakeAllArrayStructuresSlowPut(vm); // Step 1 above.
            auto result = globalsHavingABadTime.add(global);
            if (result.isNewEntry) {
                if (UncheckedKeyHashSet<JSGlobalObject*>* dependents = dependencies.dependentsFor(global)) {
                    for (JSGlobalObject* dependentGlobal : *dependents)
                        globals.append(dependentGlobal);
                }
            }
        }

        ObjectsWithBrokenIndexingFinder<BadTimeFinderMode::MultipleGlobals> finder(foundObjects, globalsHavingABadTime);
        {
            HeapIterationScope iterationScope(vm.heap);
            vm.heap.objectSpace().forEachLiveCell(iterationScope, finder); // Step 2 above.
        }
    }

    while (!foundObjects.isEmpty()) {
        JSObject* object = asObject(foundObjects.last());
        foundObjects.removeLast();
        ASSERT(hasBrokenIndexing(object));
        object->switchToSlowPutArrayStorage(vm);
    }
}

void JSGlobalObject::fixupPrototypeChainWithObjectPrototype(VM& vm)
{
    JSObject* oldLastInPrototypeChain = lastInPrototypeChain(this);
    JSObject* objectPrototype = m_objectPrototype.get();
    if (oldLastInPrototypeChain != objectPrototype)
        oldLastInPrototypeChain->setPrototypeDirect(vm, objectPrototype);
}

// Set prototype, and also insert the object prototype at the end of the chain.
void JSGlobalObject::resetPrototype(VM& vm, JSValue prototype)
{
    if (getPrototypeDirect() == prototype)
        return;
    setPrototypeDirect(vm, prototype);
    fixupPrototypeChainWithObjectPrototype(vm);
    // Whenever we change the prototype of the global object, we need to create a new JSGlobalProxy with the correct prototype.
    setGlobalThis(vm, JSGlobalProxy::create(vm, JSGlobalProxy::createStructure(vm, this, prototype), this));
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

template<typename Visitor>
void JSGlobalObject::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{ 
    JSGlobalObject* thisObject = jsCast<JSGlobalObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_globalThis);

    visitor.append(thisObject->m_globalLexicalEnvironment);
    visitor.append(thisObject->m_globalScopeExtension);
    visitor.append(thisObject->m_globalCallee);
    visitor.append(thisObject->m_evalCallee);
    visitor.append(thisObject->m_zombieFrameCallee);
    JS_GLOBAL_OBJECT_ADDITIONS_4;
    thisObject->m_evalErrorStructure.visit(visitor);
    thisObject->m_rangeErrorStructure.visit(visitor);
    thisObject->m_referenceErrorStructure.visit(visitor);
    thisObject->m_syntaxErrorStructure.visit(visitor);
    thisObject->m_typeErrorStructure.visit(visitor);
    thisObject->m_URIErrorStructure.visit(visitor);
    thisObject->m_aggregateErrorStructure.visit(visitor);
    thisObject->m_suppressedErrorStructure.visit(visitor);
    visitor.append(thisObject->m_arrayConstructor);
    visitor.append(thisObject->m_shadowRealmConstructor);
    visitor.append(thisObject->m_regExpConstructor);
    visitor.append(thisObject->m_objectConstructor);
    visitor.append(thisObject->m_functionConstructor);
    visitor.append(thisObject->m_iteratorConstructor);
    visitor.append(thisObject->m_promiseConstructor);
    visitor.append(thisObject->m_internalPromiseConstructor);
    visitor.append(thisObject->m_stringConstructor);

    thisObject->m_defaultCollator.visit(visitor);
    thisObject->m_defaultNumberFormat.visit(visitor);
    thisObject->m_collatorStructure.visit(visitor);
    thisObject->m_displayNamesStructure.visit(visitor);
    thisObject->m_durationFormatStructure.visit(visitor);
    thisObject->m_listFormatStructure.visit(visitor);
    thisObject->m_localeStructure.visit(visitor);
    thisObject->m_pluralRulesStructure.visit(visitor);
    thisObject->m_relativeTimeFormatStructure.visit(visitor);
    thisObject->m_segmentIteratorStructure.visit(visitor);
    thisObject->m_segmenterStructure.visit(visitor);
    thisObject->m_segmentsStructure.visit(visitor);
    thisObject->m_dateTimeFormatStructure.visit(visitor);
    thisObject->m_numberFormatStructure.visit(visitor);

    thisObject->m_calendarStructure.visit(visitor);
    thisObject->m_durationStructure.visit(visitor);
    thisObject->m_instantStructure.visit(visitor);
    thisObject->m_plainDateStructure.visit(visitor);
    thisObject->m_plainDateTimeStructure.visit(visitor);
    thisObject->m_plainMonthDayStructure.visit(visitor);
    thisObject->m_plainTimeStructure.visit(visitor);
    thisObject->m_plainYearMonthStructure.visit(visitor);
    thisObject->m_timeZoneStructure.visit(visitor);

    visitor.append(thisObject->m_nullGetterFunction);
    visitor.append(thisObject->m_nullSetterFunction);
    visitor.append(thisObject->m_nullSetterStrictFunction);

    thisObject->m_parseIntFunction.visit(visitor);
    thisObject->m_parseFloatFunction.visit(visitor);
    thisObject->m_objectProtoToStringFunction.visit(visitor);
    thisObject->m_arrayProtoToStringFunction.visit(visitor);
    thisObject->m_arrayProtoValuesFunction.visit(visitor);
    visitor.append(thisObject->m_objectProtoValueOfFunction);
    thisObject->m_numberProtoToStringFunction.visit(visitor);
    visitor.append(thisObject->m_functionProtoHasInstanceSymbolFunction);
    visitor.append(thisObject->m_performProxyObjectHasFunction);
    visitor.append(thisObject->m_performProxyObjectHasByValFunction);
    visitor.append(thisObject->m_performProxyObjectGetFunction);
    visitor.append(thisObject->m_performProxyObjectGetByValFunction);
    visitor.append(thisObject->m_performProxyObjectSetStrictFunction);
    visitor.append(thisObject->m_performProxyObjectSetSloppyFunction);
    visitor.append(thisObject->m_performProxyObjectSetByValStrictFunction);
    visitor.append(thisObject->m_performProxyObjectSetByValSloppyFunction);
    visitor.append(thisObject->m_regExpProtoSymbolReplace);
    thisObject->m_throwTypeErrorArgumentsCalleeGetterSetter.visit(visitor);
    thisObject->m_moduleLoader.visit(visitor);

    visitor.append(thisObject->m_objectPrototype);
    visitor.append(thisObject->m_functionPrototype);
    visitor.append(thisObject->m_arrayPrototype);
    visitor.append(thisObject->m_iteratorPrototype);
    visitor.append(thisObject->m_iteratorHelperPrototype);
    visitor.append(thisObject->m_generatorFunctionPrototype);
    visitor.append(thisObject->m_generatorPrototype);
    visitor.append(thisObject->m_arrayIteratorPrototype);
    visitor.append(thisObject->m_mapIteratorPrototype);
    visitor.append(thisObject->m_setIteratorPrototype);
    visitor.append(thisObject->m_asyncFunctionPrototype);
    visitor.append(thisObject->m_asyncGeneratorPrototype);
    visitor.append(thisObject->m_asyncIteratorPrototype);
    visitor.append(thisObject->m_asyncGeneratorFunctionPrototype);

    thisObject->m_debuggerScopeStructure.visit(visitor);
    thisObject->m_withScopeStructure.visit(visitor);
    thisObject->m_strictEvalActivationStructure.visit(visitor);
    visitor.append(thisObject->m_lexicalEnvironmentStructure);
    thisObject->m_moduleEnvironmentStructure.visit(visitor);
    visitor.append(thisObject->m_directArgumentsStructure);
    visitor.append(thisObject->m_scopedArgumentsStructure);
    visitor.append(thisObject->m_clonedArgumentsStructure);
    visitor.append(thisObject->m_objectStructureForObjectConstructor);
    for (unsigned i = 0; i < NumberOfArrayIndexingModes; ++i)
        visitor.append(thisObject->m_originalArrayStructureForIndexingShape[i]);
    for (unsigned i = 0; i < NumberOfArrayIndexingModes; ++i)
        visitor.append(thisObject->m_arrayStructureForIndexingShapeDuringAllocation[i]);
    thisObject->m_callbackConstructorStructure.visit(visitor);
    thisObject->m_callbackFunctionStructure.visit(visitor);
    thisObject->m_callbackObjectStructure.visit(visitor);
    thisObject->m_rawJSONObjectStructure.visit(visitor);
#if JSC_OBJC_API_ENABLED
    thisObject->m_objcCallbackFunctionStructure.visit(visitor);
    thisObject->m_objcWrapperObjectStructure.visit(visitor);
#endif
#ifdef JSC_GLIB_API_ENABLED
    thisObject->m_glibCallbackFunctionStructure.visit(visitor);
    thisObject->m_glibWrapperObjectStructure.visit(visitor);
#endif
    visitor.append(thisObject->m_nullPrototypeObjectStructure);
    visitor.append(thisObject->m_calleeStructure);

    visitor.append(thisObject->m_hostFunctionStructure);
    auto visitFunctionStructures = [&] (FunctionStructures& structures) {
        visitor.append(structures.arrowFunctionStructure);
        visitor.append(structures.sloppyFunctionStructure);
        visitor.append(structures.sloppyMethodStructure);
        visitor.append(structures.strictFunctionStructure);
        visitor.append(structures.strictMethodStructure);
    };
    visitFunctionStructures(thisObject->m_builtinFunctions);
    visitFunctionStructures(thisObject->m_ordinaryFunctions);
    visitor.append(thisObject->m_boundFunctionStructure);
    visitor.append(thisObject->m_trustedScriptStructure);

    thisObject->m_customGetterFunctionStructure.visit(visitor);
    thisObject->m_customSetterFunctionStructure.visit(visitor);
    thisObject->m_nativeStdFunctionStructure.visit(visitor);
    thisObject->m_remoteFunctionStructure.visit(visitor);
    visitor.append(thisObject->m_shadowRealmObjectStructure);
    visitor.append(thisObject->m_regExpStructure);
    visitor.append(thisObject->m_generatorFunctionStructure);
    visitor.append(thisObject->m_asyncFunctionStructure);
    visitor.append(thisObject->m_asyncGeneratorFunctionStructure);
    visitor.append(thisObject->m_generatorStructure);
    visitor.append(thisObject->m_asyncGeneratorStructure);
    visitor.append(thisObject->m_functionWithFieldsStructure);
    visitor.append(thisObject->m_iteratorStructure);
    visitor.append(thisObject->m_iteratorHelperStructure);
    visitor.append(thisObject->m_arrayIteratorStructure);
    visitor.append(thisObject->m_mapIteratorStructure);
    visitor.append(thisObject->m_setIteratorStructure);
    visitor.append(thisObject->m_wrapForValidIteratorStructure);
    visitor.append(thisObject->m_promiseAllContextStructure);
    visitor.append(thisObject->m_promiseAllGlobalContextStructure);
    visitor.append(thisObject->m_asyncFromSyncIteratorStructure);
    visitor.append(thisObject->m_regExpStringIteratorStructure);
    thisObject->m_iteratorResultObjectStructure.visit(visitor);
    thisObject->m_dataPropertyDescriptorObjectStructure.visit(visitor);
    thisObject->m_accessorPropertyDescriptorObjectStructure.visit(visitor);
    thisObject->m_promiseCapabilityObjectStructure.visit(visitor);
    visitor.append(thisObject->m_regExpMatchesArrayStructure);
    visitor.append(thisObject->m_regExpMatchesArrayWithIndicesStructure);
    visitor.append(thisObject->m_regExpMatchesIndicesArrayStructure);
    thisObject->m_moduleRecordStructure.visit(visitor);
    thisObject->m_syntheticModuleRecordStructure.visit(visitor);
    thisObject->m_moduleNamespaceObjectStructure.visit(visitor);
    thisObject->m_proxyObjectStructure.visit(visitor);
    thisObject->m_callableProxyObjectStructure.visit(visitor);
    thisObject->m_proxyRevokeStructure.visit(visitor);
    thisObject->m_sharedArrayBufferStructure.visit(visitor);
    thisObject->m_disposableStackStructure.visit(visitor);
    thisObject->m_asyncDisposableStackStructure.visit(visitor);

    for (auto& property : thisObject->m_linkTimeConstants)
        property.visit(visitor);

#define VISIT_SIMPLE_TYPE_PROTOTYPE(CapitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) if (featureFlag) \
        visitor.append(thisObject->m_ ## lowerName ## Prototype); \

#define VISIT_SIMPLE_TYPE_STRUCTURE(CapitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) if (featureFlag) \
        visitor.append(thisObject->m_ ## properName ## Structure); \

    FOR_EACH_SIMPLE_BUILTIN_TYPE(VISIT_SIMPLE_TYPE_STRUCTURE)
    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(VISIT_SIMPLE_TYPE_STRUCTURE)
    FOR_EACH_SIMPLE_BUILTIN_TYPE(VISIT_SIMPLE_TYPE_PROTOTYPE)
    FOR_EACH_BUILTIN_DERIVED_ITERATOR_TYPE(VISIT_SIMPLE_TYPE_PROTOTYPE)

#define VISIT_LAZY_TYPE(CapitalName, lowerName, properName, instanceType, jsName, prototypeBase, featureFlag) if (featureFlag) \
        thisObject->m_ ## properName ## Structure.visit(visitor);

    FOR_EACH_LAZY_BUILTIN_TYPE(VISIT_LAZY_TYPE)

#if ENABLE(WEBASSEMBLY)
    thisObject->m_webAssemblyModuleRecordStructure.visit(visitor);
    thisObject->m_webAssemblyFunctionStructure.visit(visitor);
    thisObject->m_webAssemblyWrapperFunctionStructure.visit(visitor);
    thisObject->m_webAssemblyJSTag.visit(visitor);
    FOR_EACH_WEBASSEMBLY_CONSTRUCTOR_TYPE(VISIT_LAZY_TYPE)
#endif // ENABLE(WEBASSEMBLY)

#undef VISIT_SIMPLE_TYPE
#undef VISIT_LAZY_TYPE

    for (unsigned i = NumberOfTypedArrayTypes; i--;) {
        thisObject->lazyTypedArrayStructure(indexToTypedArrayType(i)).visit(visitor);
        thisObject->lazyResizableOrGrowableSharedTypedArrayStructure(indexToTypedArrayType(i)).visit(visitor);
    }
    
    visitor.append(thisObject->m_arraySpeciesGetterSetter);
    visitor.append(thisObject->m_typedArraySpeciesGetterSetter);
    visitor.append(thisObject->m_arrayBufferSpeciesGetterSetter);
    visitor.append(thisObject->m_sharedArrayBufferSpeciesGetterSetter);
    visitor.append(thisObject->m_promiseSpeciesGetterSetter);
    visitor.append(thisObject->m_unhandledRejectionCallback);

    thisObject->m_typedArrayProto.visit(visitor);
    thisObject->m_typedArraySuperConstructor.visit(visitor);
    thisObject->m_regExpGlobalData.visitAggregate(visitor);

    {
        if (thisObject->m_weakTickets) {
            Locker locker { thisObject->cellLock() };
            for (Ref<DeferredWorkTimer::TicketData> ticket : *thisObject->m_weakTickets) {
                // FIXME: This seems like it should remove the cancelled ticket? Although, it would likely have to deal with deadlocking somehow.
                if (ticket->isCancelled())
                    continue;
                visitor.appendUnbarriered(ticket->scriptExecutionOwner());
                // The check above is just an optimization since between the check and here the mutator could cancel the ticket.
                constexpr bool mayBeCancelled = true;
                for (auto& dependency : ticket->dependencies(mayBeCancelled))
                    visitor.appendUnbarriered(dependency);
            }
        }
    }
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

DEFINE_VISIT_CHILDREN_WITH_MODIFIER(JS_EXPORT_PRIVATE, JSGlobalObject);

SUPPRESS_ASAN void JSGlobalObject::exposeDollarVM(VM& vm)
{
    RELEASE_ASSERT(g_jscConfig.restrictedOptionsEnabled && Options::useDollarVM());
    PropertySlot slot(this, PropertySlot::InternalMethodType::VMInquiry, &vm);
    if (getOwnPropertySlot(this, this, vm.propertyNames->builtinNames().dollarVMPrivateName(), slot))
        return;

    JSDollarVM* dollarVM = JSDollarVM::create(vm, JSDollarVM::createStructure(vm, this, m_objectPrototype.get()));

    GlobalPropertyInfo extraStaticGlobals[] = {
        GlobalPropertyInfo(vm.propertyNames->builtinNames().dollarVMPrivateName(), dollarVM, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly),
    };
    addStaticGlobals(extraStaticGlobals, std::size(extraStaticGlobals));

    putDirect(vm, Identifier::fromString(vm, "$vm"_s), dollarVM, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

void JSGlobalObject::addStaticGlobals(GlobalPropertyInfo* globals, int count)
{
    ScopeOffset startOffset = addVariables(count, jsUndefined());

    for (int i = 0; i < count; ++i) {
        GlobalPropertyInfo& global = globals[i];
        // This `configurable = false` is necessary condition for static globals,
        // otherwise lexical bindings can change the result of GlobalVar queries too.
        // We won't be able to declare a global lexical variable with the sanem name to
        // the static globals because configurable = false.
        ASSERT(global.attributes & PropertyAttribute::DontDelete);
        
        WatchpointSet* watchpointSet = nullptr;
        WriteBarrierBase<Unknown>* variable = nullptr;
        {
            ConcurrentJSLocker locker(symbolTable()->m_lock);
            ScopeOffset offset = symbolTable()->takeNextScopeOffset(locker);
            RELEASE_ASSERT(offset == startOffset + i);
            SymbolTableEntry newEntry(VarOffset(offset), global.attributes);
            newEntry.prepareToWatch();
            watchpointSet = newEntry.watchpointSet();
            symbolTable()->add(locker, global.identifier.impl(), WTFMove(newEntry));
            variable = &variableAt(offset);
        }
        symbolTablePutTouchWatchpointSet(vm(), this, global.identifier, global.value, variable, watchpointSet);
    }
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

bool JSGlobalObject::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    if (Base::getOwnPropertySlot(object, globalObject, propertyName, slot))
        return true;
    return symbolTableGet(jsCast<JSGlobalObject*>(object), propertyName, slot);
}

void JSGlobalObject::clearRareData(JSCell* cell)
{
    jsCast<JSGlobalObject*>(cell)->m_rareData = nullptr;
}

template<typename SpeciesWatchpoint>
void JSGlobalObject::tryInstallSpeciesWatchpoint(JSObject* prototype, JSObject* constructor, std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>& constructorWatchpoint, std::unique_ptr<SpeciesWatchpoint>& speciesWatchpoint, InlineWatchpointSet& speciesWatchpointSet, HasSpeciesProperty hasSpeciesProperty, GetterSetter* speciesGetterSetter)
{
    RELEASE_ASSERT(!constructorWatchpoint);
    RELEASE_ASSERT(!speciesWatchpoint);

    VM& vm = this->vm();
    DeferTerminationForAWhile deferScope(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    // First we need to make sure that the %prototype%.constructor property points to a %constructor%
    // and that %constructor%[Symbol.species] is the primordial GetterSetter.

    // We only initialize once so flattening the structures does not have any real cost.
    Structure* prototypeStructure = prototype->structure();
    if (prototypeStructure->isDictionary())
        prototypeStructure = prototypeStructure->flattenDictionaryStructure(vm, prototype);
    RELEASE_ASSERT(!prototypeStructure->isDictionary());

    auto invalidateWatchpoint = [&] {
        speciesWatchpointSet.invalidate(vm, StringFireDetail("Was not able to set up species watchpoint."));
    };

    PropertySlot constructorSlot(prototype, PropertySlot::InternalMethodType::VMInquiry, &vm);
    prototype->getOwnPropertySlot(prototype, this, vm.propertyNames->constructor, constructorSlot);
    scope.assertNoException();
    if (constructorSlot.slotBase() != prototype
        || !constructorSlot.isCacheableValue()
        || constructorSlot.getValue(this, vm.propertyNames->constructor) != constructor) {
        invalidateWatchpoint();
        return;
    }

    Structure* constructorStructure = constructor->structure();
    if (constructorStructure->isDictionary())
        constructorStructure = constructorStructure->flattenDictionaryStructure(vm, constructor);

    PropertySlot speciesSlot(constructor, PropertySlot::InternalMethodType::VMInquiry, &vm);
    constructor->getOwnPropertySlot(constructor, this, vm.propertyNames->speciesSymbol, speciesSlot);
    scope.assertNoException();
    switch (hasSpeciesProperty) {
    case HasSpeciesProperty::Yes: {
        if (speciesSlot.slotBase() != constructor
            || !speciesSlot.isCacheableGetter()
            || speciesSlot.getterSetter() != speciesGetterSetter) {
            invalidateWatchpoint();
            return;
        }
        break;
    }
    case HasSpeciesProperty::No: {
        if (!speciesSlot.isUnset()) {
            invalidateWatchpoint();
            return;
        }
        break;
    }
    }

    // Now we need to setup the watchpoints to make sure these conditions remain valid.

    prototypeStructure->startWatchingPropertyForReplacements(vm, constructorSlot.cachedOffset());
    switch (hasSpeciesProperty) {
    case HasSpeciesProperty::Yes:
        constructorStructure->startWatchingPropertyForReplacements(vm, speciesSlot.cachedOffset());
        break;
    case HasSpeciesProperty::No:
        break;
    }

    ObjectPropertyCondition constructorCondition = ObjectPropertyCondition::equivalence(vm, this, prototype, vm.propertyNames->constructor.impl(), constructor);
    ObjectPropertyCondition speciesCondition;
    switch (hasSpeciesProperty) {
    case HasSpeciesProperty::Yes:
        speciesCondition = ObjectPropertyCondition::equivalence(vm, this, constructor, vm.propertyNames->speciesSymbol.impl(), speciesGetterSetter);
        break;
    case HasSpeciesProperty::No:
        speciesCondition = ObjectPropertyCondition::absence(vm, this, constructor, vm.propertyNames->speciesSymbol.impl(), jsDynamicCast<JSObject*>(constructor->getPrototypeDirect()));
        break;
    }

    if (!constructorCondition.isWatchable(PropertyCondition::MakeNoChanges) || !speciesCondition.isWatchable(PropertyCondition::MakeNoChanges)) {
        invalidateWatchpoint();
        return;
    }

    // We only watch this from the DFG, and the DFG makes sure to only start watching if the watchpoint is in the IsWatched state.
    RELEASE_ASSERT(!speciesWatchpointSet.isBeingWatched());
    speciesWatchpointSet.touch(vm, "Set up species watchpoint.");

    constructorWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, constructorCondition, speciesWatchpointSet);
    constructorWatchpoint->install(vm);

    speciesWatchpoint = makeUnique<SpeciesWatchpoint>(this, speciesCondition, speciesWatchpointSet);
    speciesWatchpoint->install(vm);
}

void JSGlobalObject::installSaneChainWatchpoints()
{
    ASSERT(!arrayPrototype()->structure()->mayInterceptIndexedAccesses());
    ASSERT(!arrayPrototype()->structure()->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero());
    ASSERT(!arrayPrototype()->structure()->hasPolyProto());
    ASSERT(arrayPrototype()->structure()->storedPrototype() == objectPrototype());
    ASSERT(!hasIndexedProperties(arrayPrototype()->structure()->indexingType()));
    {
        auto result = ObjectPropertyCondition::absenceOfIndexedProperties(*m_vm, this, arrayPrototype(), objectPrototype());
        ASSERT(result.isWatchable(PropertyCondition::MakeNoChanges));
        installObjectAdaptiveStructureWatchpoint(result, m_arrayPrototypeChainIsSaneWatchpointSet);
    }

    ASSERT(!stringPrototype()->structure()->mayInterceptIndexedAccesses());
    ASSERT(!stringPrototype()->structure()->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero());
    ASSERT(!stringPrototype()->structure()->hasPolyProto());
    ASSERT(stringPrototype()->structure()->storedPrototype() == objectPrototype());
    ASSERT(!hasIndexedProperties(stringPrototype()->structure()->indexingType()));
    {
        auto result = ObjectPropertyCondition::absenceOfIndexedProperties(*m_vm, this, stringPrototype(), objectPrototype());
        ASSERT(result.isWatchable(PropertyCondition::MakeNoChanges));
        installObjectAdaptiveStructureWatchpoint(result, m_stringPrototypeChainIsSaneWatchpointSet);
    }

    ASSERT(!promisePrototype()->structure()->mayInterceptIndexedAccesses());
    ASSERT(!promisePrototype()->structure()->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero());
    ASSERT(!promisePrototype()->structure()->hasPolyProto());
    ASSERT(promisePrototype()->structure()->storedPrototype() == objectPrototype());
    ASSERT(!hasIndexedProperties(promisePrototype()->structure()->indexingType()));
    {
        auto result = ObjectPropertyCondition::absenceOfIndexedProperties(*m_vm, this, promisePrototype(), objectPrototype());
        ASSERT(result.isWatchable(PropertyCondition::MakeNoChanges));
        installObjectAdaptiveStructureWatchpoint(result, m_promisePrototypeChainIsSaneWatchpointSet);
    }

    ASSERT(!objectPrototype()->structure()->mayInterceptIndexedAccesses());
    ASSERT(!objectPrototype()->structure()->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero());
    ASSERT(!objectPrototype()->structure()->hasPolyProto());
    ASSERT(objectPrototype()->structure()->storedPrototype() == jsNull());
    ASSERT(!hasIndexedProperties(objectPrototype()->structure()->indexingType()));
    {
        auto result = ObjectPropertyCondition::absenceOfIndexedProperties(*m_vm, this, objectPrototype(), nullptr);
        ASSERT(result.isWatchable(PropertyCondition::MakeNoChanges));
        installObjectAdaptiveStructureWatchpoint(result, m_objectPrototypeChainIsSaneWatchpointSet);
    }
    installChainedWatchpoint(m_objectPrototypeChainIsSaneWatchpointSet, m_arrayPrototypeChainIsSaneWatchpointSet);
    installChainedWatchpoint(m_objectPrototypeChainIsSaneWatchpointSet, m_stringPrototypeChainIsSaneWatchpointSet);
    installChainedWatchpoint(m_objectPrototypeChainIsSaneWatchpointSet, m_promisePrototypeChainIsSaneWatchpointSet);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

void JSGlobalObject::tryInstallArrayBufferSpeciesWatchpoint(ArrayBufferSharingMode sharingMode)
{
    static_assert(static_cast<unsigned>(ArrayBufferSharingMode::Default) == 0);
    static_assert(static_cast<unsigned>(ArrayBufferSharingMode::Shared) == 1);
    unsigned index = static_cast<unsigned>(sharingMode);
    tryInstallSpeciesWatchpoint(arrayBufferPrototype(sharingMode), arrayBufferConstructor(sharingMode), m_arrayBufferPrototypeConstructorWatchpoints[index], m_arrayBufferConstructorSpeciesWatchpoints[index], arrayBufferSpeciesWatchpointSet(sharingMode), HasSpeciesProperty::Yes, arrayBufferSpeciesGetterSetter(sharingMode));
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

inline std::unique_ptr<ObjectAdaptiveStructureWatchpoint>& JSGlobalObject::typedArrayConstructorSpeciesAbsenceWatchpoint(TypedArrayType type)
{
    switch (type) {
    case NotTypedArray:
        RELEASE_ASSERT_NOT_REACHED();
        return m_typedArrayInt8ConstructorSpeciesAbsenceWatchpoint;
#define TYPED_ARRAY_TYPE_CASE(name) case Type ## name: return m_typedArray ## name ## ConstructorSpeciesAbsenceWatchpoint;
        FOR_EACH_TYPED_ARRAY_TYPE(TYPED_ARRAY_TYPE_CASE)
#undef TYPED_ARRAY_TYPE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
    return m_typedArrayInt8ConstructorSpeciesAbsenceWatchpoint;
}

inline std::unique_ptr<ObjectAdaptiveStructureWatchpoint>& JSGlobalObject::typedArrayPrototypeSymbolIteratorAbsenceWatchpoint(TypedArrayType type)
{
    switch (type) {
    case NotTypedArray:
        RELEASE_ASSERT_NOT_REACHED();
        return m_typedArrayInt8PrototypeSymbolIteratorAbsenceWatchpoint;
#define TYPED_ARRAY_TYPE_CASE(name) case Type ## name: return m_typedArray ## name ## PrototypeSymbolIteratorAbsenceWatchpoint;
        FOR_EACH_TYPED_ARRAY_TYPE(TYPED_ARRAY_TYPE_CASE)
#undef TYPED_ARRAY_TYPE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
    return m_typedArrayInt8PrototypeSymbolIteratorAbsenceWatchpoint;
}

inline std::unique_ptr<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>& JSGlobalObject::typedArrayPrototypeConstructorWatchpoint(TypedArrayType type)
{
    switch (type) {
    case NotTypedArray:
        RELEASE_ASSERT_NOT_REACHED();
        return m_typedArrayInt8PrototypeConstructorWatchpoint;
#define TYPED_ARRAY_TYPE_CASE(name) case Type ## name: return m_typedArray ## name ## PrototypeConstructorWatchpoint;
        FOR_EACH_TYPED_ARRAY_TYPE(TYPED_ARRAY_TYPE_CASE)
#undef TYPED_ARRAY_TYPE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
    return m_typedArrayInt8PrototypeConstructorWatchpoint;
}

void JSGlobalObject::tryInstallTypedArraySpeciesWatchpoint(TypedArrayType type)
{
    VM& vm = this->vm();
    auto* prototype = typedArrayPrototype(type);
    auto* constructor = typedArrayConstructor(type);
    auto& watchpointSet = typedArraySpeciesWatchpointSet(type);
    ASSERT(m_typedArrayConstructorSpeciesWatchpoint);
    if (constructor->getPrototypeDirect() != m_typedArraySuperConstructor.get(this)) {
        watchpointSet.invalidate(vm, StringFireDetail("Was not able to set up species watchpoint."));
        return;
    }
    tryInstallSpeciesWatchpoint(prototype, constructor, typedArrayPrototypeConstructorWatchpoint(type), typedArrayConstructorSpeciesAbsenceWatchpoint(type), watchpointSet, HasSpeciesProperty::No, nullptr);
}

void JSGlobalObject::installTypedArrayConstructorSpeciesWatchpoint(JSTypedArrayViewConstructor* constructor)
{
    VM& vm = this->vm();
    PropertySlot slot(constructor, PropertySlot::InternalMethodType::VMInquiry, &vm);
    constructor->getOwnPropertySlot(constructor, this, vm.propertyNames->speciesSymbol.impl(), slot);
    constructor->structure()->startWatchingPropertyForReplacements(vm, slot.cachedOffset());
    ObjectPropertyCondition speciesCondition = ObjectPropertyCondition::equivalence(vm, nullptr, constructor, vm.propertyNames->speciesSymbol.impl(), typedArraySpeciesGetterSetter());
    m_typedArrayConstructorSpeciesWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, speciesCondition, m_typedArrayConstructorSpeciesWatchpointSet);
    m_typedArrayConstructorSpeciesWatchpoint->install(vm);
}

void JSGlobalObject::installTypedArrayIteratorProtocolWatchpoint(JSObject* base, TypedArrayType typedArrayType)
{
    VM& vm = this->vm();

    DeferTerminationForAWhile deferScope(vm);
    auto catchScope = DECLARE_CATCH_SCOPE(vm);

    auto absenceCondition = [&](PropertyName propertyName) {
        PropertySlot slot(base, PropertySlot::InternalMethodType::VMInquiry, &vm);
        bool result = base->getOwnPropertySlot(base, this, propertyName, slot);
        RELEASE_ASSERT(!result);
        catchScope.assertNoException();
        RELEASE_ASSERT(slot.isUnset());
        RELEASE_ASSERT(base->getPrototypeDirect() == m_typedArrayProto.get(this));
        return ObjectPropertyCondition::absence(vm, this, base, propertyName.uid(), m_typedArrayProto.get(this));
    };

    ObjectPropertyCondition iteratorCondition = absenceCondition(vm.propertyNames->iteratorSymbol);

    if (!iteratorCondition.isWatchable(PropertyCondition::EnsureWatchability)) {
        typedArrayIteratorProtocolWatchpointSet(typedArrayType).invalidate(vm, StringFireDetail("Was not able to set up iterator protocol watchpoint."));
        return;
    }

    RELEASE_ASSERT(!typedArrayIteratorProtocolWatchpointSet(typedArrayType).isBeingWatched());
    typedArrayIteratorProtocolWatchpointSet(typedArrayType).touch(vm, "Set up iterator protocol watchpoint.");

    typedArrayPrototypeSymbolIteratorAbsenceWatchpoint(typedArrayType) = makeUnique<ObjectAdaptiveStructureWatchpoint>(this, iteratorCondition, typedArrayIteratorProtocolWatchpointSet(typedArrayType));
    typedArrayPrototypeSymbolIteratorAbsenceWatchpoint(typedArrayType)->install(vm);
}

void JSGlobalObject::installTypedArrayPrototypeIteratorProtocolWatchpoint(JSTypedArrayViewPrototype* prototype)
{
    VM& vm = this->vm();
    ObjectPropertyCondition condition = setupAdaptiveWatchpoint(this, prototype, vm.propertyNames->iteratorSymbol);
    m_typedArrayPrototypeSymbolIteratorWatchpoint = makeUnique<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, condition, m_typedArrayPrototypeIteratorProtocolWatchpointSet);
    m_typedArrayPrototypeSymbolIteratorWatchpoint->install(vm);
}

void JSGlobalObject::installNumberPrototypeWatchpoint(NumberPrototype* numberPrototype)
{
    VM& vm = this->vm();
    ASSERT(m_numberToStringWatchpointSet.isStillValid());
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, numberPrototype, vm.propertyNames->toString), m_numberToStringWatchpointSet);
}

void JSGlobalObject::installMapPrototypeWatchpoint(MapPrototype* mapPrototype)
{
    VM& vm = this->vm();
    if (m_mapIteratorProtocolWatchpointSet.isStillValid())
        installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, mapPrototype, vm.propertyNames->iteratorSymbol), m_mapIteratorProtocolWatchpointSet);
    ASSERT(m_mapSetWatchpointSet.isStillValid());
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, mapPrototype, vm.propertyNames->set), m_mapSetWatchpointSet);
}

void JSGlobalObject::installSetPrototypeWatchpoint(SetPrototype* setPrototype)
{
    VM& vm = this->vm();
    if (m_setIteratorProtocolWatchpointSet.isStillValid())
        installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, setPrototype, vm.propertyNames->iteratorSymbol), m_setIteratorProtocolWatchpointSet);
    ASSERT(m_setAddWatchpointSet.isStillValid());
    installObjectPropertyChangeAdaptiveWatchpoint(setupAdaptiveWatchpoint(this, setPrototype, vm.propertyNames->add), m_setAddWatchpointSet);
}

void JSGlobalObject::installObjectAdaptiveStructureWatchpoint(const ObjectPropertyCondition& key, InlineWatchpointSet& watchpointSet)
{
    auto watchpoint = makeUniqueRef<ObjectAdaptiveStructureWatchpoint>(this, key, watchpointSet);
    watchpoint->install(*m_vm);
    m_installedObjectAdaptiveStructureWatchpoints.append(WTFMove(watchpoint));
}

void JSGlobalObject::installObjectPropertyChangeAdaptiveWatchpoint(const ObjectPropertyCondition& key, InlineWatchpointSet& watchpointSet)
{
    auto watchpoint = makeUniqueRef<ObjectPropertyChangeAdaptiveWatchpoint<InlineWatchpointSet>>(this, key, watchpointSet);
    watchpoint->install(*m_vm);
    m_installedObjectPropertyChangeAdaptiveWatchpoints.append(WTFMove(watchpoint));
}

void JSGlobalObject::installChainedWatchpoint(InlineWatchpointSet& from, InlineWatchpointSet& to)
{
    auto watchpoint = makeUniqueRef<ChainedWatchpoint>(this, to);
    watchpoint->install(from, *m_vm);
    m_installedChainedWatchpoints.append(WTFMove(watchpoint));
}

void JSGlobalObject::tryInstallPropertyDescriptorFastPathWatchpoint()
{
    VM& vm = this->vm();

    DeferTerminationForAWhile deferScope(vm);
    auto catchScope = DECLARE_CATCH_SCOPE(vm);

    auto invalidate = [&]() {
        m_propertyDescriptorFastPathWatchpointSet.invalidate(vm, StringFireDetail("Was not able to set up property descriptor related names watchpoint set."));
    };

    auto absenceCondition = [&](JSObject* base, PropertyName propertyName) -> std::optional<ObjectPropertyCondition> {
        PropertySlot slot(base, PropertySlot::InternalMethodType::VMInquiry, &vm);
        bool result = base->getOwnPropertySlot(base, this, propertyName, slot);
        if (result)
            return std::nullopt;
        catchScope.assertNoException();
        RELEASE_ASSERT(slot.isUnset());
        return ObjectPropertyCondition::absence(vm, this, base, propertyName.uid(), nullptr);
    };

    if (!objectPrototypeChainIsSane()) {
        invalidate();
        return;
    }

    Vector<ObjectPropertyCondition, 8> conditions;
    {
        auto condition = absenceCondition(objectPrototype(), vm.propertyNames->get);
        if (!condition) {
            invalidate();
            return;
        }
        conditions.append(condition.value());
    }
    {
        auto condition = absenceCondition(objectPrototype(), vm.propertyNames->set);
        if (!condition) {
            invalidate();
            return;
        }
        conditions.append(condition.value());
    }
    {
        auto condition = absenceCondition(objectPrototype(), vm.propertyNames->enumerable);
        if (!condition) {
            invalidate();
            return;
        }
        conditions.append(condition.value());
    }
    {
        auto condition = absenceCondition(objectPrototype(), vm.propertyNames->configurable);
        if (!condition) {
            invalidate();
            return;
        }
        conditions.append(condition.value());
    }
    {
        auto condition = absenceCondition(objectPrototype(), vm.propertyNames->writable);
        if (!condition) {
            invalidate();
            return;
        }
        conditions.append(condition.value());
    }

    for (auto& condition : conditions) {
        if (!condition.isWatchable(PropertyCondition::EnsureWatchability)) {
            invalidate();
            return;
        }
    }

    RELEASE_ASSERT(!m_propertyDescriptorFastPathWatchpointSet.isBeingWatched());
    m_propertyDescriptorFastPathWatchpointSet.touch(vm, "Set up property descriptor fast path watchpoint set.");
    for (auto& condition : conditions)
        installObjectAdaptiveStructureWatchpoint(condition, m_propertyDescriptorFastPathWatchpointSet);
}

void slowValidateCell(JSGlobalObject* globalObject)
{
    RELEASE_ASSERT(globalObject->isGlobalObject());
    ASSERT_GC_OBJECT_INHERITS(globalObject, JSGlobalObject::info());
}

void JSGlobalObject::setInspectable(bool inspectable)
{
#if ENABLE(REMOTE_INSPECTOR)
    // FIXME: <http://webkit.org/b/246237> Local inspection should be controlled by `inspectable` API.
    m_inspectorDebuggable->setInspectable(inspectable);
#else
    UNUSED_PARAM(inspectable);
#endif
}

bool JSGlobalObject::inspectable() const
{
#if ENABLE(REMOTE_INSPECTOR)
    // FIXME: <http://webkit.org/b/246237> Local inspection should be controlled by `inspectable` API.
    return m_inspectorDebuggable->inspectable();
#else
    return false;
#endif
}

void JSGlobalObject::setIsITML()
{
#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorDebuggable->setIsITML();
#endif
}

void JSGlobalObject::setName(const String& name)
{
    m_name = name;

#if ENABLE(REMOTE_INSPECTOR)
    m_inspectorDebuggable->update();
#endif
}

void JSGlobalObject::bumpGlobalLexicalBindingEpoch(VM& vm)
{
    if (++m_globalLexicalBindingEpoch == Options::thresholdForGlobalLexicalBindingEpoch()) {
        // Since the epoch overflows, we should rewrite all the CodeBlock to adjust to the newly started generation.
        m_globalLexicalBindingEpoch = 1;
        vm.heap.codeBlockSet().iterate([&] (CodeBlock* codeBlock) {
            if (codeBlock->globalObject() != this)
                return;
            codeBlock->notifyLexicalBindingUpdate();
        });
    }
}

void JSGlobalObject::queueMicrotaskToEventLoop(JSC::JSGlobalObject& globalObject, JSC::QueuedTask&& task)
{
    if (globalObject.debugger()) [[unlikely]]
        task.setDispatcher(DebuggableMicrotaskDispatcher::create());
    globalObject.vm().queueMicrotask(WTFMove(task));
}

void JSGlobalObject::queueMicrotask(InternalMicrotask job, JSValue argument0, JSValue argument1, JSValue argument2, JSValue argument3)
{
    QueuedTask task { nullptr, job, this, argument0, argument1, argument2, argument3 };
    globalObjectMethodTable()->queueMicrotaskToEventLoop(*this, WTFMove(task));
}

void JSGlobalObject::promiseRejectionTracker(JSGlobalObject* globalObject, JSPromise* promise, JSPromiseRejectionOperation operation)
{
    switch (operation) {
    case JSC::JSPromiseRejectionOperation::Handle: {
        break;
    }
    case JSC::JSPromiseRejectionOperation::Reject: {
        globalObject->vm().promiseRejected(promise);
        break;
    }
    }
}

void JSGlobalObject::reportUncaughtExceptionAtEventLoop(JSGlobalObject*, Exception* exception)
{
    dataLogLn("Uncaught Exception at run loop: ", exception->value());
}

void JSGlobalObject::setConsoleClient(WeakPtr<ConsoleClient>&& consoleClient)
{
    m_consoleClient = WTFMove(consoleClient);
}

WeakPtr<ConsoleClient> JSGlobalObject::consoleClient() const
{
    return m_consoleClient;
}

void JSGlobalObject::setDebugger(Debugger* debugger)
{
    m_debugger = debugger;
    if (debugger)
        vm().ensureShadowChicken();
}

bool JSGlobalObject::hasInteractiveDebugger() const 
{ 
    return m_debugger && m_debugger->isInteractivelyDebugging();
}

#if ENABLE(DFG_JIT)
WatchpointSet* JSGlobalObject::getReferencedPropertyWatchpointSet(UniquedStringImpl* uid)
{
    ConcurrentJSLocker locker(m_referencedGlobalPropertyWatchpointSetsLock);
    return m_referencedGlobalPropertyWatchpointSets.get(uid);
}

WatchpointSet& JSGlobalObject::ensureReferencedPropertyWatchpointSet(UniquedStringImpl* uid)
{
    ConcurrentJSLocker locker(m_referencedGlobalPropertyWatchpointSetsLock);
    return m_referencedGlobalPropertyWatchpointSets.ensure(uid, [] {
        return WatchpointSet::create(IsWatched);
    }).iterator->value.get();
}
#endif

JSGlobalObject* JSGlobalObject::create(VM& vm, Structure* structure)
{
    JSGlobalObject* globalObject = new (NotNull, allocateCell<JSGlobalObject>(vm)) JSGlobalObject(vm, structure);
    globalObject->finishCreation(vm);
    return globalObject;
}

JSGlobalObject* JSGlobalObject::createWithCustomMethodTable(VM& vm, Structure* structure, const GlobalObjectMethodTable* methodTable)
{
    JSGlobalObject* globalObject = new (NotNull, allocateCell<JSGlobalObject>(vm)) JSGlobalObject(vm, structure, methodTable);
    globalObject->finishCreation(vm);
    return globalObject;
}

void JSGlobalObject::finishCreation(VM& vm)
{
    DeferTermination deferTermination(vm);
    Base::finishCreation(vm);
    structure()->setGlobalObject(vm, this);
    m_runtimeFlags = m_globalObjectMethodTable->javaScriptRuntimeFlags(this);
    init(vm);
    setGlobalThis(vm, JSGlobalProxy::create(vm, JSGlobalProxy::createStructure(vm, this, getPrototypeDirect()), this));
    ASSERT(type() == GlobalObjectType);
}

void JSGlobalObject::finishCreation(VM& vm, JSObject* thisValue)
{
    DeferTermination deferTermination(vm);
    Base::finishCreation(vm);
    structure()->setGlobalObject(vm, this);
    m_runtimeFlags = m_globalObjectMethodTable->javaScriptRuntimeFlags(this);
    init(vm);
    setGlobalThis(vm, thisValue);
    ASSERT(type() == GlobalObjectType);
}

#ifdef JSC_GLIB_API_ENABLED
void JSGlobalObject::setWrapperMap(std::unique_ptr<WrapperMap>&& map)
{
    m_wrapperMap = WTFMove(map);
}
#endif

void JSGlobalObject::addWeakTicket(DeferredWorkTimer::Ticket ticket)
{
    Locker locker { cellLock() };
    if (!m_weakTickets) {
        auto weakTickets = makeUnique<ThreadSafeWeakHashSet<DeferredWorkTimer::TicketData>>();
        WTF::storeStoreFence();
        m_weakTickets = WTFMove(weakTickets);
    }
    m_weakTickets->add(*ticket);
    vm().writeBarrier(this);
}
void JSGlobalObject::clearWeakTickets()
{
    if (!m_weakTickets)
        return;

    WaiterListManager::singleton().unregister(this);
    // Clear the rest tickets safely.
    vm().deferredWorkTimer->cancelPendingWorkSafe(this);
}

FunctionExecutable* JSGlobalObject::tryGetCachedFunctionExecutableForFunctionConstructor(const Identifier& name, StringView program, const SourceOrigin& sourceOrigin, SourceTaintedOrigin sourceTaintedOrigin, const String& sourceURL, const TextPosition& startPosition, LexicallyScopedFeatures lexicallyScopedFeatures, FunctionConstructionMode functionConstructionMode)
{
    if (!defaultCodeGenerationMode().isEmpty())
        return nullptr;

    auto* executable = m_executableForCachedFunctionExecutableForFunctionConstructor.get();
    if (!executable)
        return nullptr;

    auto* unlinkedExecutable = executable->unlinkedExecutable();
    if (name != unlinkedExecutable->name())
        return nullptr;

    if (lexicallyScopedFeatures != unlinkedExecutable->lexicallyScopedFeatures())
        return nullptr;

    auto storedSource = executable->source();
    if (OrdinalNumber { } != storedSource.firstLine())
        return nullptr;

    int offset = functionConstructorPrefix(functionConstructionMode).length() + name.length();
    if (offset != storedSource.startColumn().zeroBasedInt())
        return nullptr;

    if (program.substring(offset) != storedSource.view())
        return nullptr;

    RefPtr storedProvider = executable->source().provider();
    if (storedProvider->startPosition() != startPosition)
        return nullptr;

    if (storedProvider->sourceOrigin() != sourceOrigin)
        return nullptr;

    if (storedProvider->sourceURL() != sourceURL)
        return nullptr;

    if (storedProvider->sourceTaintedOrigin() != sourceTaintedOrigin)
        return nullptr;

    return executable;
}

void JSGlobalObject::cachedFunctionExecutableForFunctionConstructor(FunctionExecutable* executable)
{
    if (!defaultCodeGenerationMode().isEmpty())
        return;
    if (executable->source().provider()->couldBeTainted())
        return;
    auto* unlinkedExecutable = executable->unlinkedExecutable();
    if (unlinkedExecutable->features() & NoEvalCacheFeature)
        return;
    m_executableForCachedFunctionExecutableForFunctionConstructor.set(vm(), executable);
}

#if ENABLE(REMOTE_INSPECTOR)
Ref<JSGlobalObjectDebuggable> JSGlobalObject::protectedInspectorDebuggable()
{
    return inspectorDebuggable();
}
#endif

} // namespace JSC
