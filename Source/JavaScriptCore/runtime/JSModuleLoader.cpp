/*
 * Copyright (C) 2015-2021, 2026 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSModuleLoader.h"
#include "ProgramExecutable.h"

#include "BuiltinNames.h"
#include "Completion.h"
#include "GlobalObjectMethodTable.h"
#include "JSCInlines.h"
#include "JSModuleNamespaceObject.h"
#include "JSModuleRecord.h"
#include "JSPromise.h"
#include "JSSourceCode.h"
#include "JSWebAssembly.h"
#include "Microtask.h"
#include "ModuleAnalyzer.h"
#include "ModuleLoadingContext.h"
#include "ModuleRegistryEntry.h"
#include "Nodes.h"
#include "ObjectConstructor.h"
#include "Parser.h"
#include "ParserError.h"
#include "Symbol.h"
#include "SyntheticModuleRecord.h"
#include "TopExceptionScope.h"
#include "VMTrapsInlines.h"
#include <wtf/text/MakeString.h>

namespace JSC {

namespace JSModuleLoaderInternal {
static constexpr unsigned maximumResolutionFailures = 128;
}

static Identifier jsValueToSpecifier(JSGlobalObject* globalObject, JSValue value)
{
    if (value.isSymbol())
        return Identifier::fromUid(uncheckedDowncast<Symbol>(value)->privateName());
    ASSERT(value.isString());
    return asString(value)->toIdentifier(globalObject);
}

bool JSModuleLoader::isFetchError(JSGlobalObject* globalObject, ErrorInstance* error)
{
    VM& vm = globalObject->vm();
    return error->hasOwnProperty(globalObject, vm.propertyNames->builtinNames().moduleFetchFailureKindPrivateName());
}

// Web Platform Tests requires that import() promises reject with difference error object instances for fetch errors.
// It also requires that errors be cached. To satisfy both requirements, it's necessary to duplicate errors.
ErrorInstance* JSModuleLoader::duplicateTypeError(JSGlobalObject* globalObject, ErrorInstance* error)
{
    ASSERT(error);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* copy = uncheckedDowncast<ErrorInstance>(createTypeErrorCopy(globalObject, error));
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto copyField = [&](const Identifier& name) -> bool {
        if (JSValue value = error->getDirect(vm, name)) {
            copy->putDirect(vm, name, value);
            RETURN_IF_EXCEPTION(scope, false);
        }

        return true;
    };

    const BuiltinNames& names = vm.propertyNames->builtinNames();

    if (!copyField(names.moduleFetchFailureKindPrivateName()))
        return nullptr;

    if (!copyField(names.moduleFailureModuleRecordPrivateName()))
        return nullptr;

    if (!copyField(names.moduleFailureModuleKeyPrivateName()))
        return nullptr;

    if (!copyField(names.moduleFailureModuleTypePrivateName()))
        return nullptr;

    if (!copyField(names.moduleFailureKindPrivateName()))
        return nullptr;

    return copy;
}

ErrorInstance* JSModuleLoader::duplicateError(JSGlobalObject* globalObject, ErrorInstance* error)
{
    ASSERT(error);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (error->errorType() == ErrorType::TypeError)
        RELEASE_AND_RETURN(scope, JSModuleLoader::duplicateTypeError(globalObject, error));

    String message = error->sanitizedMessageString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    ErrorInstance* out = ErrorInstance::create(globalObject, WTF::move(message), error->errorType(), { }, { }, { });
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto copyField = [&](const Identifier& name) -> bool {
        if (JSValue value = error->getDirect(vm, name)) {
            out->putDirect(vm, name, value);
            RETURN_IF_EXCEPTION(scope, false);
        }
        return true;
    };

    const BuiltinNames& names = vm.propertyNames->builtinNames();

    if (!copyField(names.moduleFetchFailureKindPrivateName()))
        return nullptr;

    if (!copyField(names.moduleFailureModuleRecordPrivateName()))
        return nullptr;

    if (!copyField(names.moduleFailureModuleKeyPrivateName()))
        return nullptr;

    if (!copyField(names.moduleFailureModuleTypePrivateName()))
        return nullptr;

    if (!copyField(names.moduleFailureKindPrivateName()))
        return nullptr;

    return out;
}

ErrorInstance* JSModuleLoader::maybeDuplicateFetchError(JSGlobalObject* globalObject, ErrorInstance* error)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (error->hasOwnProperty(globalObject, vm.propertyNames->builtinNames().moduleFetchFailureKindPrivateName())) {
        error = JSModuleLoader::duplicateError(globalObject, error);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    RELEASE_AND_RETURN(scope, error);
}

JSModuleLoader::ModuleFailure::ModuleFailure(AbstractModuleRecord* source, ScriptFetchParameters::Type type, Kind kind)
    : m_source(source)
    , m_key(source->moduleKey())
    , m_type(type)
    , m_kind(kind)
{
}

JSModuleLoader::ModuleFailure::ModuleFailure(Identifier key, ScriptFetchParameters::Type type, Kind kind)
    : m_source(nullptr)
    , m_key(WTF::move(key))
    , m_type(type)
    , m_kind(kind)
{
}

bool JSModuleLoader::ModuleFailure::isEvaluationError(const Identifier& expectedSpecifier, ScriptFetchParameters::Type expectedType) const
{
    return m_kind == ModuleFailure::Kind::Evaluation || (!m_key.isNull() && m_key != expectedSpecifier) || (m_type != ScriptFetchParameters::None && m_type != expectedType);
}

JSModuleLoader::ModuleFailure::operator bool() const
{
    return m_source != nullptr || !m_key.isNull() || m_type != ScriptFetchParameters::Type::None || m_kind != ModuleFailure::Kind::Unknown;
}

JSModuleLoader::ModuleFailure JSModuleLoader::getErrorInfo(JSGlobalObject* globalObject, ErrorInstance* error)
{
    VM& vm = globalObject->vm();
    JSModuleLoader::ModuleFailure failure;
    if (JSValue sourceValue = error->getDirect(vm, vm.propertyNames->builtinNames().moduleFailureModuleRecordPrivateName()))
        failure.m_source = dynamicDowncast<AbstractModuleRecord>(sourceValue);
    if (JSValue keyValue = error->getDirect(vm, vm.propertyNames->builtinNames().moduleFailureModuleKeyPrivateName()))
        failure.m_key = jsValueToSpecifier(globalObject, keyValue);
    if (JSValue typeValue = error->getDirect(vm, vm.propertyNames->builtinNames().moduleFailureModuleTypePrivateName()))
        failure.m_type = static_cast<ScriptFetchParameters::Type>(typeValue.asInt32());
    if (JSValue kindValue = error->getDirect(vm, vm.propertyNames->builtinNames().moduleFailureKindPrivateName()))
        failure.m_kind = static_cast<JSModuleLoader::ModuleFailure::Kind>(kindValue.asInt32());
    return failure;
}

void JSModuleLoader::attachErrorInfo(JSGlobalObject* globalObject, ErrorInstance* error, AbstractModuleRecord* source, const Identifier& key, ScriptFetchParameters::Type type, JSModuleLoader::ModuleFailure::Kind kind)
{
    VM& vm = globalObject->vm();
    if (source)
        error->putDirect(vm, vm.propertyNames->builtinNames().moduleFailureModuleRecordPrivateName(), source);
    if (!key.isNull())
        error->putDirect(vm, vm.propertyNames->builtinNames().moduleFailureModuleKeyPrivateName(), identifierToJSValue(vm, key));
    if (type != ScriptFetchParameters::Type::None)
        error->putDirect(vm, vm.propertyNames->builtinNames().moduleFailureModuleTypePrivateName(), jsNumber(std::to_underlying(type)));
    if (kind != JSModuleLoader::ModuleFailure::Kind::Unknown)
        error->putDirect(vm, vm.propertyNames->builtinNames().moduleFailureKindPrivateName(), jsNumber(std::to_underlying(kind)));
}

bool JSModuleLoader::attachErrorInfo(JSGlobalObject* globalObject, Exception* exception, AbstractModuleRecord* source, const Identifier& key, ScriptFetchParameters::Type type, ModuleFailure::Kind kind)
{
    if (auto* error = dynamicDowncast<ErrorInstance>(exception->value())) {
        attachErrorInfo(globalObject, error, source, key, type, kind);
        return true;
    }
    return false;
}

bool JSModuleLoader::attachErrorInfo(JSGlobalObject* globalObject, ThrowScope& scope, AbstractModuleRecord* source, const Identifier& key, ScriptFetchParameters::Type type, ModuleFailure::Kind kind)
{
    if (Exception* exception = scope.exception())
        return attachErrorInfo(globalObject, exception, source, key, type, kind);
    return false;
}

const ClassInfo JSModuleLoader::s_info = { "ModuleLoader"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSModuleLoader) };

JSModuleLoader::JSModuleLoader(VM& vm, Structure* structure)
    : JSCell(vm, structure)
{
}

void JSModuleLoader::destroy(JSCell* cell)
{
    SUPPRESS_MEMORY_UNSAFE_CAST auto* thisObject = static_cast<JSModuleLoader*>(cell);
    thisObject->JSModuleLoader::~JSModuleLoader();
}

void JSModuleLoader::finishCreation(JSGlobalObject*, VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

template<typename Visitor>
void JSModuleLoader::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSModuleLoader* thisObject = uncheckedDowncast<JSModuleLoader>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    Locker locker { thisObject->cellLock() };
    auto moduleMapValues = thisObject->m_moduleMap.values();
    visitor.append(moduleMapValues.begin(), moduleMapValues.end());
    auto resolutionFailuresValues = thisObject->m_resolutionFailures.values();
    visitor.append(resolutionFailuresValues.begin(), resolutionFailuresValues.end());
    for (auto& [key, loadedModule] : thisObject->m_loadedModules)
        visitor.append(loadedModule.m_module);
}

DEFINE_VISIT_CHILDREN(JSModuleLoader);

// ------------------------------ Functions --------------------------------

static String printableModuleKey(JSGlobalObject* globalObject, JSValue key)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    if (key.isString() || key.isSymbol()) {
        auto propertyName = key.toPropertyKey(globalObject);
        scope.assertNoExceptionExceptTermination(); // This is OK since this function is just for debugging purpose.
        return propertyName.impl();
    }
    return vm.propertyNames->emptyIdentifier.impl();
}

JSArray* JSModuleLoader::dependencyKeysIfEvaluated(JSGlobalObject* globalObject, const String& key)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier ident = Identifier::fromString(vm, key);

    auto iter = m_moduleMap.find({ ident.impl(), ScriptFetchParameters::Type::JavaScript });

    if (iter == m_moduleMap.end())
        iter = m_moduleMap.find({ ident.impl(), ScriptFetchParameters::Type::WebAssembly });

    if (iter == m_moduleMap.end())
        RELEASE_AND_RETURN(scope, nullptr);

    ModuleRegistryEntry* entry = iter->value.get();
    AbstractModuleRecord* record = entry->record();

    if (record == nullptr)
        RELEASE_AND_RETURN(scope, nullptr);

    if (auto status = entry->status(); status != ModuleRegistryEntry::Status::Fetched && status != ModuleRegistryEntry::Status::EvaluationFailed)
        RELEASE_AND_RETURN(scope, nullptr);

    if (auto* cyclic = dynamicDowncast<CyclicModuleRecord>(record); cyclic && cyclic->status() != CyclicModuleRecord::Status::Evaluated)
        RELEASE_AND_RETURN(scope, nullptr);

    const Vector<AbstractModuleRecord::ModuleRequest>& requests = record->requestedModules();

    JSArray* array = constructEmptyArray(globalObject, nullptr, requests.size());
    RETURN_IF_EXCEPTION(scope, nullptr);

    for (unsigned index = 0; const AbstractModuleRecord::ModuleRequest& request : requests) {
        Identifier resolved = resolve(globalObject, request.m_specifier, ident, nullptr, /* useImportMap */ true);
        RETURN_IF_EXCEPTION(scope, nullptr);
        array->putDirectIndex(globalObject, index++, identifierToJSValue(vm, resolved));
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    RELEASE_AND_RETURN(scope, array);
}

void JSModuleLoader::provideFetch(JSGlobalObject* globalObject, const Identifier& key, ScriptFetchParameters::Type type, SourceCode&& sourceCode)
{
    ModuleRegistryEntry* entry = ensureRegistered(globalObject, key, type);
    if (entry->status() == ModuleRegistryEntry::Status::New)
        entry->provideFetch(globalObject, WTF::move(sourceCode)); // can throw
}

void JSModuleLoader::provideFetch(JSGlobalObject* globalObject, const Identifier& key, ScriptFetchParameters::Type type, JSSourceCode* jsSourceCode)
{
    ModuleRegistryEntry* entry = ensureRegistered(globalObject, key, type);
    if (entry->status() == ModuleRegistryEntry::Status::New)
        entry->provideFetch(globalObject, jsSourceCode); // can throw
}

JSPromise* JSModuleLoader::loadModule(JSGlobalObject* globalObject, const Identifier& specifier, RefPtr<ScriptFetchParameters> parameters, RefPtr<ScriptFetcher> scriptFetcher, bool evaluate, bool dynamic, bool useImportMap)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSPromise* promise = nullptr;

    ScriptFetchParameters::Type type = parameters ? parameters->type() : ScriptFetchParameters::Type::JavaScript;

    if (ModuleRegistryEntry* entry = getRegisteredMayBeNull(specifier, type)) {
        JSValue error = entry->error(globalObject);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (error)
            return JSPromise::rejectedPromise(globalObject, error);

        if (entry->status() != ModuleRegistryEntry::Status::New)
            promise = entry->ensureFetchPromise(globalObject);
    }

    if (!promise) {
        promise = fetch(globalObject, identifierToJSValue(vm, specifier), WTF::move(parameters), scriptFetcher);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    AbstractModuleRecord::ModuleRequest request { specifier, ScriptFetchParameters::create(type) };
    auto* context = ModuleLoadingContext::create(vm, request, WTF::move(scriptFetcher), evaluate, dynamic, useImportMap);

    JSPromise* intermediatePromise = JSPromise::create(vm, globalObject->promiseStructure());
    intermediatePromise->markAsHandled();
    promise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ModuleLoadTopSettled, intermediatePromise, context);

    JSPromise* resultPromise = JSPromise::create(vm, globalObject->promiseStructure());
    resultPromise->markAsHandled();
    intermediatePromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ModuleLoadTopRejected, resultPromise, context);

    return resultPromise;
}

JSPromise* JSModuleLoader::linkAndEvaluateModule(JSGlobalObject* globalObject, const Identifier& moduleKey, RefPtr<ScriptFetchParameters> parameters, RefPtr<ScriptFetcher> scriptFetcher)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ScriptFetchParameters::Type type = parameters ? parameters->type() : ScriptFetchParameters::Type::JavaScript;
    ModuleRegistryEntry* entry = ensureRegistered(globalObject, moduleKey, type);
    RETURN_IF_EXCEPTION(scope, nullptr);

    AbstractModuleRecord* record = entry->record();
    ASSERT(record);

    JSValue error = entry->error(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (error) {
        scope.throwException(globalObject, error);
        return nullptr;
    }

    record->link(globalObject, WTF::move(scriptFetcher));
    if (Exception* exception = scope.exception()) {
        attachErrorInfo(globalObject, scope, record, entry->key(), entry->moduleType(), ModuleFailure::Kind::Instantiation);
        entry->setInstantiationError(globalObject, exception->value());
        if (auto* cyclic = dynamicDowncast<CyclicModuleRecord>(record))
            cyclic->setEvaluationError(vm, exception->value());
        return nullptr;
    }

    JSPromise* promise = record->evaluate(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    error = entry->error(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (error) {
        // At this point, the promise exists but won't be returned. However, it will be rejected later.
        // To prevent an unhandled promise rejection error, we have to explicitly mark the promise as handled.
        promise->markAsHandled();
        scope.throwException(globalObject, error);
        return nullptr;
    }

    return promise;
}

JSPromise* JSModuleLoader::requestImportModule(JSGlobalObject* globalObject, const Identifier& moduleName, const Identifier& referrer, RefPtr<ScriptFetchParameters> parameters, RefPtr<ScriptFetcher> scriptFetcher)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier resolved = resolve(globalObject, moduleName, referrer, scriptFetcher, /* useImportMap */ true);
    RETURN_IF_EXCEPTION(scope, nullptr);

    JSPromise* promise = loadModule(globalObject, resolved, WTF::move(parameters), WTF::move(scriptFetcher), /* evaluate */ true, /* dynamic */ true, /* useImportMap */ false);
    RETURN_IF_EXCEPTION(scope, nullptr);

    JSPromise* resultPromise = JSPromise::create(vm, globalObject->promiseStructure());
    resultPromise->markAsHandled();
    promise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ImportModuleNamespace, resultPromise, jsUndefined());

    return resultPromise;
}

JSPromise* JSModuleLoader::importModule(JSGlobalObject* globalObject, JSString* moduleName, JSValue parameters, const SourceOrigin& referrer)
{
    dataLogLnIf(Options::dumpModuleLoadingState(), "Loader [import] ", printableModuleKey(globalObject, moduleName));

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto attributes = retrieveImportAttributesFromDynamicImportOptions(globalObject, parameters, { vm.propertyNames->type.impl() });
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto type = retrieveTypeImportAttribute(globalObject, attributes);
    RETURN_IF_EXCEPTION(scope, nullptr);

    RefPtr<ScriptFetchParameters> fetchParams;
    if (type)
        fetchParams = ScriptFetchParameters::create(type.value());

    if (globalObject->globalObjectMethodTable()->moduleLoaderImportModule)
        RELEASE_AND_RETURN(scope, globalObject->globalObjectMethodTable()->moduleLoaderImportModule(globalObject, this, moduleName, WTF::move(fetchParams), referrer));

    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());
    auto moduleNameString = moduleName->value(globalObject);
    RETURN_IF_EXCEPTION(scope, promise->rejectWithCaughtException(globalObject, scope));

    scope.release();
    promise->reject(vm, globalObject, createError(globalObject, makeString("Could not import the module '"_s, moduleNameString.data, "'."_s)));
    return promise;
}

Identifier JSModuleLoader::resolve(JSGlobalObject* globalObject, JSValue name, JSValue referrer, RefPtr<ScriptFetcher> scriptFetcher, bool useImportMap)
{
    dataLogLnIf(Options::dumpModuleLoadingState(), "Loader [resolve] ", printableModuleKey(globalObject, name));

    if (globalObject->globalObjectMethodTable()->moduleLoaderResolve)
        return globalObject->globalObjectMethodTable()->moduleLoaderResolve(globalObject, this, name, referrer, WTF::move(scriptFetcher), useImportMap);
    return name.toPropertyKey(globalObject);
}

Identifier JSModuleLoader::resolve(JSGlobalObject* globalObject, const Identifier& name, const Identifier& referrer, RefPtr<ScriptFetcher> scriptFetcher, bool useImportMap)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue nameValue = name.isSymbol() || name.isNull() ? jsUndefined() : jsString(vm, name.string());
    RETURN_IF_EXCEPTION(scope, { });

    JSValue referrerValue = referrer.isSymbol() || referrer.isNull() ? jsUndefined() : jsString(vm, referrer.string());
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, resolve(globalObject, nameValue, referrerValue, WTF::move(scriptFetcher), useImportMap));
}

JSPromise* JSModuleLoader::fetch(JSGlobalObject* globalObject, JSValue key, RefPtr<ScriptFetchParameters> parameters, RefPtr<ScriptFetcher> scriptFetcher)
{
    dataLogLnIf(Options::dumpModuleLoadingState(), "Loader [fetch] ", printableModuleKey(globalObject, key));

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (globalObject->globalObjectMethodTable()->moduleLoaderFetch)
        RELEASE_AND_RETURN(scope, globalObject->globalObjectMethodTable()->moduleLoaderFetch(globalObject, this, key, WTF::move(parameters), WTF::move(scriptFetcher)));

    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());
    String moduleKey = key.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, promise->rejectWithCaughtException(globalObject, scope));

    scope.release();
    promise->reject(vm, globalObject, createError(globalObject, makeString("Could not open the module '"_s, moduleKey, "'."_s)));
    return promise;
}

JSObject* JSModuleLoader::createImportMetaProperties(JSGlobalObject* globalObject, JSValue key, JSModuleRecord* moduleRecord, RefPtr<ScriptFetcher> scriptFetcher)
{
    if (globalObject->globalObjectMethodTable()->moduleLoaderCreateImportMetaProperties)
        return globalObject->globalObjectMethodTable()->moduleLoaderCreateImportMetaProperties(globalObject, this, key, moduleRecord, WTF::move(scriptFetcher));
    return constructEmptyObject(globalObject->vm(), globalObject->nullPrototypeObjectStructure());
}

JSValue JSModuleLoader::evaluate(JSGlobalObject* globalObject, JSValue key, JSValue moduleRecordValue, RefPtr<ScriptFetcher> scriptFetcher, JSValue sentValue, JSValue resumeMode)
{
    dataLogLnIf(Options::dumpModuleLoadingState(), "Loader [evaluate] ", printableModuleKey(globalObject, key));

    if (globalObject->globalObjectMethodTable()->moduleLoaderEvaluate)
        return globalObject->globalObjectMethodTable()->moduleLoaderEvaluate(globalObject, this, key, moduleRecordValue, WTF::move(scriptFetcher), sentValue, resumeMode);

    return evaluateNonVirtual(globalObject, key, moduleRecordValue, nullptr, sentValue, resumeMode);
}

JSValue JSModuleLoader::evaluateNonVirtual(JSGlobalObject* globalObject, JSValue, JSValue moduleRecordValue, RefPtr<ScriptFetcher>, JSValue sentValue, JSValue resumeMode)
{
    if (auto* moduleRecord = dynamicDowncast<AbstractModuleRecord>(moduleRecordValue))
        return moduleRecord->evaluate(globalObject, sentValue, resumeMode);
    return jsUndefined();
}

JSModuleNamespaceObject* JSModuleLoader::getModuleNamespaceObject(JSGlobalObject* globalObject, JSValue moduleRecordValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* moduleRecord = dynamicDowncast<AbstractModuleRecord>(moduleRecordValue);
    if (!moduleRecord) {
        throwTypeError(globalObject, scope);
        return nullptr;
    }

    RELEASE_AND_RETURN(scope, moduleRecord->getModuleNamespace(globalObject));
}

AbstractModuleRecord* JSModuleLoader::getImportedModule(AbstractModuleRecord* referrer, const AbstractModuleRecord::ModuleRequest& request)
{
    // GetImportedModule(referrer, request)
    // https://tc39.es/ecma262/#sec-GetImportedModule

    // 1. Let records be a List consisting of each LoadedModuleRequest Record r of referrer.[[LoadedModules]] such that ModuleRequestsEqual(r, request) is true.
    auto iter = referrer->loadedModules().find(ModuleMapKey { request.m_specifier.impl(), request.type() });
    // 2. Assert: records has exactly one element, since LoadRequestedModules has completed successfully on referrer prior to invoking this abstract operation.
    ASSERT(iter != referrer->loadedModules().end());
    // 3. Let record be the sole element of records.
    // 4. Return record.[[Module]].
    return iter->value.m_module.get();
}

AbstractModuleRecord* JSModuleLoader::maybeGetImportedModule(AbstractModuleRecord* referrer, const Identifier& moduleKey)
{
    for (const auto& [key, loadedModuleRequest] : referrer->loadedModules()) {
        if (loadedModuleRequest.m_specifier == moduleKey)
            return loadedModuleRequest.m_module.get();
    }
    return nullptr;
}

JSPromise* JSModuleLoader::hostLoadImportedModule(JSGlobalObject* globalObject, const ModuleReferrer& referrer, const ModuleRequest& moduleRequest, JSCell* payload, RefPtr<ScriptFetcher> scriptFetcher, bool useImportMap)
{
    // HostLoadImportedModule(referrer, moduleRequest, loadState, payload)
    // https://html.spec.whatwg.org/multipage/webappapis.html#hostloadimportedmodule

    ASSERT(isModuleLoaderHostDefinedPayload(payload));

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Identifier referrerKey;
    CyclicModuleRecord* record = referrer.getModule();

    if (record)
        referrerKey = record->moduleKey();

    ModuleRegistryEntry* mapEntry = nullptr;
    const Identifier& specifier = moduleRequest.m_specifier;
    auto type = moduleRequest.type();

    if (specifier.isSymbol())
        mapEntry = getRegisteredMayBeNull(specifier, type);

    ModuleMapKey moduleMapKey { specifier.impl(), type };
    ResolutionMapKey resolutionKey { referrerKey.impl(), specifier.impl() };

    if (auto error = m_resolutionFailures.get(resolutionKey)) {
        JSValue errorValue = error.get();
        ASSERT(errorValue);
        JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
        promise->reject(vm, globalObject, errorValue);
        // 9.1. If loadState is not undefined and loadState.[[ErrorToRethrow]] is null, set loadState.[[ErrorToRethrow]] to resolutionError.
        // (Unused.)
        // 9.2. Perform FinishLoadingImportedModule(referrer, moduleRequest, payload, ThrowCompletion(resolutionError)).
        finishLoadingImportedModule(globalObject, referrer, moduleRequest, payload, Exception::create(vm, errorValue), scriptFetcher);
        // 9.3. Return.
        RELEASE_AND_RETURN(scope, promise);
    }

    Identifier resolved;

    if (!mapEntry) {
        // 8. Let url be the result of resolving a module specifier given referencingScript and moduleRequest.[[Specifier]], catching any exceptions. If they throw an exception, let resolutionError be the thrown exception.
        resolved = resolve(globalObject, specifier, referrerKey, scriptFetcher, useImportMap);
        // 9. If the previous step threw an exception, then:
        if (Exception* resolutionError = scope.exception()) {
            attachErrorInfo(globalObject, resolutionError, nullptr, specifier, moduleRequest.type(), ModuleFailure::Kind::Instantiation);
            // Cache the resolution error so subsequent calls for the same specifier return the same error object.
            JSValue errorValue = resolutionError->value();
            addResolutionFailure(vm, resolutionKey, errorValue);

            JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
            promise->rejectWithCaughtException(globalObject, scope);
            // 9.1. If loadState is not undefined and loadState.[[ErrorToRethrow]] is null, set loadState.[[ErrorToRethrow]] to resolutionError.
            // (Unused.)
            // 9.2. Perform FinishLoadingImportedModule(referrer, moduleRequest, payload, ThrowCompletion(resolutionError)).
            finishLoadingImportedModule(globalObject, referrer, moduleRequest, payload, Exception::create(vm, errorValue), scriptFetcher);
            // 9.3. Return.
            RELEASE_AND_RETURN(scope, promise);
        }

        moduleMapKey.first = resolved.impl();

        if (auto iter = m_moduleMap.find(moduleMapKey); iter != m_moduleMap.end())
            mapEntry = iter->value.get();
    } else
        resolved = specifier;

    if (mapEntry) {
        ASSERT(mapEntry->status() != ModuleRegistryEntry::Status::New);

        // Only fetching errors should be checked here, not instantiation or evaluation errors.
        // This allows fetch errors to propagate first because they're required to have priority.
        // To avoid a race condition, instantiation errors need to be checked later, not here.
        if (JSValue fetchError = mapEntry->fetchError()) {
            if (auto* errorInstance = dynamicDowncast<ErrorInstance>(fetchError)) {
                fetchError = JSModuleLoader::duplicateError(globalObject, errorInstance);
                RETURN_IF_EXCEPTION(scope, nullptr);
            }

            finishLoadingImportedModule(globalObject, referrer, moduleRequest, payload, Exception::create(vm, fetchError), scriptFetcher);
            RETURN_IF_EXCEPTION(scope, nullptr);
            return JSPromise::rejectedPromise(globalObject, fetchError);
        }

        JSPromise* promise = mapEntry->loadPromise();
        if (promise) {
            if (mapEntry->record()) {
                finishLoadingImportedModule(globalObject, referrer, moduleRequest, payload, mapEntry->record(), scriptFetcher);
                RETURN_IF_EXCEPTION(scope, nullptr);
            } else {
                auto* context = ModuleLoadingContext::create(vm, ModuleLoadingContext::Step::Cached, referrer, moduleRequest, payload, mapEntry, scriptFetcher);
                JSPromise* resultPromise = JSPromise::create(vm, globalObject->promiseStructure());
                resultPromise->markAsHandled();
                promise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ModuleLoadStep, resultPromise, context);
                promise = resultPromise;
            }

            return promise;
        }
    } else {
        mapEntry = ModuleRegistryEntry::create(vm, resolved, type, scriptFetcher);
        Locker locker { cellLock() };
        m_moduleMap.add(moduleMapKey, WriteBarrier<ModuleRegistryEntry>(vm, this, mapEntry));
    }

    if (mapEntry->status() == ModuleRegistryEntry::Status::New) {
        JSPromise* promise = fetch(globalObject, identifierToJSValue(vm, resolved), moduleRequest.m_attributes, scriptFetcher);
        RETURN_IF_EXCEPTION(scope, nullptr);

        mapEntry->setStatus(ModuleRegistryEntry::Status::Fetching);
        mapEntry->ensureFetchPromise(globalObject)->pipeFrom(vm, promise);
    }

    JSPromise* modulePromise = mapEntry->ensureModulePromise(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto* context = ModuleLoadingContext::create(vm, ModuleLoadingContext::Step::Main, referrer, moduleRequest, payload, mapEntry, scriptFetcher);
    JSPromise* loadPromise = JSPromise::create(vm, globalObject->promiseStructure());
    loadPromise->markAsHandled();

    modulePromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ModuleLoadStep, loadPromise, context);

    mapEntry->setLoadPromise(vm, loadPromise);

    return loadPromise;
}

JSPromise* JSModuleLoader::loadModule(JSGlobalObject* globalObject, const ModuleReferrer& referrer, const ModuleRequest& moduleRequest, JSCell* payload, RefPtr<ScriptFetcher> scriptFetcher, bool evaluate, bool useImportMap)
{
    ASSERT(isModuleLoaderHostDefinedPayload(payload));

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSPromise* promise = hostLoadImportedModule(globalObject, referrer, moduleRequest, payload, scriptFetcher, useImportMap);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto* context = ModuleLoadingContext::create(vm, moduleRequest, WTF::move(scriptFetcher), evaluate, /* dynamic */ false, useImportMap);
    JSPromise* resultPromise = JSPromise::create(vm, globalObject->promiseStructure());
    resultPromise->markAsHandled();

    promise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ModuleLoadLinkEvaluateSettled, resultPromise, context);
    resultPromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ModuleLoadStoreError, jsUndefined(), context);

    return resultPromise;
}

static void checkSafeToRecurse(JSGlobalObject* globalObject, ThrowScope& scope)
{
    if (!globalObject->vm().isSafeToRecurse())
        throwRangeError(globalObject, scope, "Maximum call stack size exceeded"_s);
}

void JSModuleLoader::innerModuleLoading(JSGlobalObject* globalObject, ModuleGraphLoadingState *state, AbstractModuleRecord* module)
{
    // InnerModuleLoading(state, module)
    // https://tc39.es/ecma262/#sec-InnerModuleLoading

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Assert: state.[[IsLoading]] is true.
    ASSERT(state->isLoading());
    // 2. If module is a Cyclic Module Record, module.[[Status]] is NEW, and state.[[Visited]] does not contain module, then
    if (auto* cyclic = dynamicDowncast<CyclicModuleRecord>(module); cyclic && cyclic->status() == CyclicModuleRecord::Status::New && !state->containsVisited(cyclic)) {
        // 2.a. Append module to state.[[Visited]].
        state->appendVisited(vm, cyclic);
        // 2.b. Let requestedModulesCount be the number of elements in module.[[RequestedModules]].
        size_t requestedModulesCount = module->requestedModules().size();
        // 2.c. Set state.[[PendingModulesCount]] to state.[[PendingModulesCount]] + requestedModulesCount.
        state->setPendingModulesCount(state->pendingModulesCount() + requestedModulesCount);
        // 2.d. For each ModuleRequest Record request of module.[[RequestedModules]], do
        for (const AbstractModuleRecord::ModuleRequest& request : module->requestedModules()) {
            // 2.d.i. If AllImportAttributesSupported(request.[[Attributes]]) is false, then
            // 2.d.i.1. Let error be ThrowCompletion(a newly created SyntaxError object).
            // 2.d.i.2. Perform ContinueModuleLoading(state, error).
            // (Not possible.)
            // 2.d.ii. Else if module.[[LoadedModules]] contains a LoadedModuleRequest Record record such that ModuleRequestsEqual(record, request) is true, then
            if (auto iter = module->loadedModules().find(ModuleMapKey { request.m_specifier.impl(), request.type() }); iter != module->loadedModules().end()) {
                checkSafeToRecurse(globalObject, scope);
                RETURN_IF_EXCEPTION(scope, void());
                // 2.d.ii.1. Perform InnerModuleLoading(state, record.[[Module]]).
                innerModuleLoading(globalObject, state, iter->value.m_module.get());
                RETURN_IF_EXCEPTION(scope, void());
                // 2.d.iii. Else,
            } else {
                // 2.d.iii.1. Perform HostLoadImportedModule(module, request, state.[[HostDefined]], state).
                JSPromise* promise = hostLoadImportedModule(globalObject, cyclic, request, state, state->scriptFetcher(), true);
                RETURN_IF_EXCEPTION(scope, void());
                promise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ModuleGraphLoadingError, jsUndefined(), state);
                // 2.d.iii.2. NOTE: HostLoadImportedModule will call FinishLoadingImportedModule, which re-enters the graph loading process through ContinueModuleLoading.
            }
            // 2.d.iv. If state.[[IsLoading]] is false, return UNUSED.
            if (!state->isLoading())
                RELEASE_AND_RETURN(scope, void());
        }
    }
    // 3. Assert: state.[[PendingModulesCount]] ≥ 1.
    ASSERT(state->pendingModulesCount() >= 1);
    // 4. Set state.[[PendingModulesCount]] to state.[[PendingModulesCount]] - 1.
    state->setPendingModulesCount(state->pendingModulesCount() - 1);
    // 5. If state.[[PendingModulesCount]] = 0, then
    if (!state->pendingModulesCount()) {
        // 5.a. Set state.[[IsLoading]] to false.
        state->setIsLoading(false);
        // 5.b. For each Cyclic Module Record loaded of state.[[Visited]], do
        state->iterateVisited([](CyclicModuleRecord* loaded) {
            // 5.b.i. If loaded.[[Status]] is NEW, set loaded.[[Status]] to UNLINKED.
            if (loaded->status() == CyclicModuleRecord::Status::New)
                loaded->setStatus(CyclicModuleRecord::Status::Unlinked);
        });
        // 5.c. Perform ! Call(state.[[PromiseCapability]].[[Resolve]], undefined, « undefined »).
        state->promise()->fulfill(vm, globalObject, module);
    }
    // 6. Return UNUSED.
    scope.release();
}

void JSModuleLoader::finishLoadingImportedModule(JSGlobalObject* globalObject, const ModuleReferrer& referrer, const ModuleRequest& moduleRequest, JSCell* payload, ModuleCompletion result, RefPtr<ScriptFetcher> scriptFetcher)
{
    // FinishLoadingImportedModule(referrer, moduleRequest, payload, result)
    // https://tc39.es/ecma262/#sec-FinishLoadingImportedModule

    ASSERT(isModuleLoaderHostDefinedPayload(payload));

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. If result is a normal completion, then
    if (auto* resultRecord = std::get_if<AbstractModuleRecord*>(&result)) {
        JSCell* owner = nullptr;

        auto& loadedModules = [&] -> ModuleMap<AbstractModuleRecord::LoadedModuleRequest> & {
            if (CyclicModuleRecord* module = referrer.getModule()) {
                owner = module;
                return module->loadedModules();
            }
            ASSERT(referrer.isRealm());
            owner = this;
            return m_loadedModules;
        }();

        ASSERT(owner);

        // 1.a. If referrer.[[LoadedModules]] contains a LoadedModuleRequest Record record such that ModuleRequestsEqual(record, moduleRequest) is true, then
        if (auto iter = loadedModules.find(ModuleMapKey { moduleRequest.m_specifier.impl(), moduleRequest.type() }); iter != loadedModules.end()) {
            // 1.a.i. Assert: record.[[Module]] and result.[[Value]] are the same Module Record.
            ASSERT(iter->value.m_module.get() == *resultRecord);
        // 1.b. Else,
        } else {
            // 1.b.i. Append the LoadedModuleRequest Record { [[Specifier]]: moduleRequest.[[Specifier]], [[Attributes]]: moduleRequest.[[Attributes]], [[Module]]: result.[[Value]] } to referrer.[[LoadedModules]].
            ModuleMapKey key { moduleRequest.m_specifier.impl(), moduleRequest.type() };
            Locker locker { owner->cellLock() };
            AbstractModuleRecord::LoadedModuleRequest value { vm, moduleRequest, *resultRecord, owner };
            loadedModules.add(WTF::move(key), WTF::move(value));
        }
    }

    // 2. If payload is a GraphLoadingState Record, then
    if (auto* state = dynamicDowncast<ModuleGraphLoadingState>(payload)) {
        // 2.a. Perform ContinueModuleLoading(payload, result).
        continueModuleLoading(globalObject, state, result);
        RETURN_IF_EXCEPTION(scope, void());
    // 3. Else,
    } else {
        // 3.a. Perform ContinueDynamicImport(payload, result).
        auto* dynamicPayload = uncheckedDowncast<ModuleLoaderPayload>(payload);
        continueDynamicImport(globalObject, dynamicPayload->promise(), result, WTF::move(scriptFetcher));
        RETURN_IF_EXCEPTION(scope, void());
    }

    // 4. Return UNUSED.
    scope.release();
}

void JSModuleLoader::continueModuleLoading(JSGlobalObject* globalObject, ModuleGraphLoadingState *state, ModuleCompletion moduleCompletion)
{
    // ContinueModuleLoading(state, moduleCompletion)
    // https://tc39.es/ecma262/#sec-ContinueModuleLoading

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. If state.[[IsLoading]] is false, return UNUSED.
    if (!state->isLoading())
        RELEASE_AND_RETURN(scope, void());
    // 2. If moduleCompletion is a normal completion, then
    if (auto* module = std::get_if<AbstractModuleRecord*>(&moduleCompletion)) {
        // 2.a. Perform InnerModuleLoading(state, moduleCompletion.[[Value]]).
        innerModuleLoading(globalObject, state, *module);
        RETURN_IF_EXCEPTION(scope, void());
    // 3. Else,
    } else {
        // 3.a. Set state.[[IsLoading]] to false.
        state->setIsLoading(false);
        // 3.b. Perform ! Call(state.[[PromiseCapability]].[[Reject]], undefined, « moduleCompletion.[[Value]] »).
        state->promise()->reject(vm, globalObject, std::get<Exception*>(moduleCompletion)->value());
    }
    // 4. Return UNUSED.
    scope.release();
}

void JSModuleLoader::continueDynamicImport(JSGlobalObject* globalObject, JSPromise* promise, ModuleCompletion completion, RefPtr<ScriptFetcher> scriptFetcher)
{
    // ContinueDynamicImport(promiseCapability, moduleCompletion)
    // https://tc39.es/ecma262/#sec-ContinueDynamicImport

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. If moduleCompletion is an abrupt completion, then
    if (Exception** exception = std::get_if<Exception*>(&completion)) {
        // 1.a. Perform ! Call(promiseCapability.[[Reject]], undefined, « moduleCompletion.[[Value]] »).
        promise->reject(vm, globalObject, (*exception)->value());
        // 1.b. Return UNUSED.
        scope.assertNoException();
        return;
    }
    // 2. Let module be moduleCompletion.[[Value]].
    auto* module = std::get<AbstractModuleRecord*>(completion);
    // 3. Let loadPromise be module.LoadRequestedModules().
    JSPromise* loadPromise = loadRequestedModules(globalObject, module, WTF::move(scriptFetcher));
    RETURN_IF_EXCEPTION(scope, void());
    // 4-8. Link and evaluate using microtask dispatch instead of closures.
    loadPromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::DynamicImportLoadSettled, promise, module);
    // 9. Return UNUSED.
    scope.release();
}

JSPromise* JSModuleLoader::loadRequestedModules(JSGlobalObject* globalObject, AbstractModuleRecord* module, RefPtr<ScriptFetcher> scriptFetcher)
{
    // LoadRequestedModules([hostDefined])
    // https://tc39.es/ecma262/#sec-LoadRequestedModules

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. If hostDefined is not present, let hostDefined be empty.
    // 2. Let pc be ! NewPromiseCapability(%Promise%).
    JSPromise* pc = JSPromise::create(vm, globalObject->promiseStructure()); // This will eventually be resolved with an AbstractModuleRecord*.
    pc->markAsHandled();
    // 3. Let state be the GraphLoadingState Record { [[IsLoading]]: true, [[PendingModulesCount]]: 1, [[Visited]]: « », [[PromiseCapability]]: pc, [[HostDefined]]: hostDefined }.
    auto state = ModuleGraphLoadingState::create(vm, pc, WTF::move(scriptFetcher));
    RETURN_IF_EXCEPTION(scope, nullptr);
    // 4. Perform InnerModuleLoading(state, module).
    innerModuleLoading(globalObject, state, module);
    RETURN_IF_EXCEPTION(scope, nullptr);
    // 5. Return pc.[[Promise]].
    return pc;
}

ModuleRegistryEntry* JSModuleLoader::ensureRegistered(JSGlobalObject* globalObject, const Identifier& key, ScriptFetchParameters::Type type)
{
    VM& vm = globalObject->vm();

    ModuleMapKey moduleMapKey { key.impl(), type };

    if (auto iter = m_moduleMap.find(moduleMapKey); iter != m_moduleMap.end())
        return iter->value.get();

    ModuleRegistryEntry* entry = ModuleRegistryEntry::create(vm, key, type, nullptr);

    Locker locker { cellLock() };
    m_moduleMap.add(moduleMapKey, WriteBarrier<ModuleRegistryEntry>(vm, this, entry));

    return entry;
}

ModuleRegistryEntry* JSModuleLoader::getRegisteredMayBeNull(const Identifier& key, ScriptFetchParameters::Type type)
{
    if (auto iter = m_moduleMap.find({ key.impl(), type }); iter != m_moduleMap.end())
        return iter->value.get();
    return nullptr;
}

void JSModuleLoader::addResolutionFailure(VM& vm, const ResolutionMapKey& key, JSValue error)
{
    Locker locker { cellLock() };
    if (m_resolutionFailures.size() >= JSModuleLoaderInternal::maximumResolutionFailures)
        m_resolutionFailures.remove(m_resolutionFailures.begin());
    m_resolutionFailures.add(key, WriteBarrier<Unknown>(vm, this, error));
}

ProgramExecutable* JSModuleLoader::ModuleReferrer::getScript() const
{
    auto option = std::get_if<ProgramExecutable*>(this);
    return option ? *option : nullptr;
}

CyclicModuleRecord* JSModuleLoader::ModuleReferrer::getModule() const
{
    auto option = std::get_if<CyclicModuleRecord*>(this);
    return option ? *option : nullptr;
}

JSGlobalObject* JSModuleLoader::ModuleReferrer::getRealm() const
{
    auto option = std::get_if<JSGlobalObject*>(this);
    return option ? *option : nullptr;
}

bool JSModuleLoader::ModuleReferrer::isScript() const
{
    return std::holds_alternative<ProgramExecutable*>(*this);
}

bool JSModuleLoader::ModuleReferrer::isModule() const
{
    return std::holds_alternative<CyclicModuleRecord*>(*this);
}

bool JSModuleLoader::ModuleReferrer::isRealm() const
{
    return std::holds_alternative<JSGlobalObject*>(*this);
}

JSValue JSModuleLoader::ModuleReferrer::toJSValue() const
{
    if (auto executable = std::get_if<ProgramExecutable*>(this))
        return *executable;
    if (auto module = std::get_if<CyclicModuleRecord*>(this))
        return *module;
    ASSERT(isRealm());
    return std::get<JSGlobalObject*>(*this);
}

JSPromise* JSModuleLoader::makeModule(JSGlobalObject* globalObject, const Identifier& moduleKey, JSSourceCode* jsSourceCode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    const SourceCode& sourceCode = jsSourceCode->sourceCode();

    JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
    promise->markAsHandled();

#if ENABLE(WEBASSEMBLY)
    if (sourceCode.provider()->sourceType() == SourceProviderSourceType::WebAssembly)
        RELEASE_AND_RETURN(scope, uncheckedDowncast<JSPromise>(JSWebAssembly::instantiate(globalObject, promise, sourceCode.provider(), moduleKey, jsSourceCode)));
#endif

    // https://tc39.es/proposal-json-modules/#sec-parse-json-module
    if (sourceCode.provider()->sourceType() == SourceProviderSourceType::JSON) {
        auto* moduleRecord = SyntheticModuleRecord::parseJSONModule(globalObject, moduleKey, SourceCode { sourceCode });
        attachErrorInfo(globalObject, scope, moduleRecord, moduleKey, ScriptFetchParameters::JSON, ModuleFailure::Kind::Evaluation);
        RETURN_IF_EXCEPTION(scope, promise->rejectWithCaughtException(globalObject, scope));
        scope.release();
        promise->fulfill(vm, globalObject, moduleRecord);
        return promise;
    }

    ParserError error;
    std::unique_ptr<ModuleProgramNode> moduleProgramNode = parseRootNode<ModuleProgramNode>(
        vm, sourceCode, ImplementationVisibility::Public, JSParserBuiltinMode::NotBuiltin,
        StrictModeLexicallyScopedFeature, JSParserScriptMode::Module, SourceParseMode::ModuleAnalyzeMode, error);
    if (error.isValid()) {
        auto* errorInstance = uncheckedDowncast<ErrorInstance>(error.toErrorObject(globalObject, sourceCode));
        attachErrorInfo(globalObject, errorInstance, nullptr, moduleKey, ScriptFetchParameters::JavaScript, ModuleFailure::Kind::Evaluation);
        promise->reject(vm, globalObject, errorInstance);
        RELEASE_AND_RETURN(scope, promise);
    }
    ASSERT(moduleProgramNode);

    ModuleAnalyzer moduleAnalyzer(globalObject, moduleKey, sourceCode, moduleProgramNode->varDeclarations(), moduleProgramNode->lexicalVariables(), moduleProgramNode->features());
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto result = moduleAnalyzer.analyze(*moduleProgramNode);
    if (!result) {
        auto [errorType, message] = WTF::move(result.error());
        auto* errorInstance = uncheckedDowncast<ErrorInstance>(createError(globalObject, errorType, message));
        attachErrorInfo(globalObject, errorInstance, nullptr, moduleKey, ScriptFetchParameters::JavaScript, ModuleFailure::Kind::Evaluation);
        promise->reject(vm, globalObject, errorInstance);
        RELEASE_AND_RETURN(scope, promise);
    }

    promise->fulfill(vm, globalObject, result.value());
    RELEASE_AND_RETURN(scope, promise);
}

} // namespace JSC
