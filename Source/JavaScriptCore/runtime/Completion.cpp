/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "Completion.h"

#include "BuiltinNames.h"
#include "BytecodeCacheError.h"
#include "CodeCache.h"
#include "Exception.h"
#include "IdentifierInlines.h"
#include "Interpreter.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSModuleLoader.h"
#include "JSPromise.h"
#include "JSWithScope.h"
#include "ModuleAnalyzer.h"
#include "Parser.h"
#include "ScriptProfilingScope.h"
#include "TopExceptionScope.h"
#include <wtf/text/MakeString.h>

namespace JSC {

static inline bool checkSyntaxInternal(VM& vm, const SourceCode& source, ParserError& error)
{
    return !!parseRootNode<ProgramNode>(
        vm, source, ImplementationVisibility::Public, JSParserBuiltinMode::NotBuiltin,
        NoLexicallyScopedFeatures, JSParserScriptMode::Classic, SourceParseMode::ProgramMode, error);
}

static Identifier createEntrypointModuleKey(VM&)
{
    // Generate the unique key for the source-provided module.
    PrivateName privateName(PrivateName::Description, "EntrypointModule"_s);
    return Identifier::fromUid(privateName);
}

bool checkSyntax(JSGlobalObject* globalObject, const SourceCode& source, JSValue* returnedException)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::currentSingleton().atomStringTable());

    ParserError error;
    if (checkSyntaxInternal(vm, source, error))
        return true;
    ASSERT(error.isValid());
    if (returnedException)
        *returnedException = error.toErrorObject(globalObject, source);
    return false;
}

bool checkSyntax(VM& vm, const SourceCode& source, ParserError& error)
{
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::currentSingleton().atomStringTable());
    return checkSyntaxInternal(vm, source, error);
}

bool checkModuleSyntax(JSGlobalObject* globalObject, const SourceCode& source, ParserError& error)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::currentSingleton().atomStringTable());
    std::unique_ptr<ModuleProgramNode> moduleProgramNode = parseRootNode<ModuleProgramNode>(
        vm, source, ImplementationVisibility::Public, JSParserBuiltinMode::NotBuiltin,
        StrictModeLexicallyScopedFeature, JSParserScriptMode::Module, SourceParseMode::ModuleAnalyzeMode, error);
    if (!moduleProgramNode)
        return false;

    PrivateName privateName(PrivateName::Description, "EntrypointModule"_s);
    ModuleAnalyzer moduleAnalyzer(globalObject, Identifier::fromUid(privateName), source, moduleProgramNode->varDeclarations(), moduleProgramNode->lexicalVariables(), moduleProgramNode->features());
    return !!moduleAnalyzer.analyze(*moduleProgramNode);
}

RefPtr<CachedBytecode> generateProgramBytecode(VM& vm, const SourceCode& source, FileSystem::FileHandle& fileHandle, BytecodeCacheError& error)
{
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::currentSingleton().atomStringTable());

    LexicallyScopedFeatures lexicallyScopedFeatures = NoLexicallyScopedFeatures;
    JSParserScriptMode scriptMode = JSParserScriptMode::Classic;
    EvalContextType evalContextType = EvalContextType::None;

    ParserError parserError;
    UnlinkedCodeBlock* unlinkedCodeBlock = recursivelyGenerateUnlinkedCodeBlockForProgram(vm, source, lexicallyScopedFeatures, scriptMode, { }, parserError, evalContextType);
    if (parserError.isValid())
        error = parserError;
    if (!unlinkedCodeBlock)
        return nullptr;

    return serializeBytecode(vm, unlinkedCodeBlock, source, SourceCodeType::ProgramType, lexicallyScopedFeatures, scriptMode, fileHandle, error, { });
}

RefPtr<CachedBytecode> generateModuleBytecode(VM& vm, const SourceCode& source, FileSystem::FileHandle& fileHandle, BytecodeCacheError& error)
{
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::currentSingleton().atomStringTable());

    LexicallyScopedFeatures lexicallyScopedFeatures = StrictModeLexicallyScopedFeature;
    JSParserScriptMode scriptMode = JSParserScriptMode::Module;
    EvalContextType evalContextType = EvalContextType::None;

    ParserError parserError;
    UnlinkedCodeBlock* unlinkedCodeBlock = recursivelyGenerateUnlinkedCodeBlockForModuleProgram(vm, source, lexicallyScopedFeatures, scriptMode, { }, parserError, evalContextType);
    if (parserError.isValid())
        error = parserError;
    if (!unlinkedCodeBlock)
        return nullptr;
    return serializeBytecode(vm, unlinkedCodeBlock, source, SourceCodeType::ModuleType, lexicallyScopedFeatures, scriptMode, fileHandle, error, { });
}

JSValue evaluate(JSGlobalObject* globalObject, const SourceCode& source, JSValue thisValue, NakedPtr<Exception>& returnedException)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::currentSingleton().atomStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    if (!thisValue || thisValue.isUndefinedOrNull())
        thisValue = globalObject;
    JSObject* thisObj = uncheckedDowncast<JSObject>(thisValue.toThis(globalObject, ECMAMode::sloppy()));
    JSValue result = vm.interpreter.executeProgram(source, globalObject, thisObj);

    if (scope.exception()) [[unlikely]] {
        returnedException = scope.exception();
        scope.clearException();
        return jsUndefined();
    }

    RELEASE_ASSERT(result);
    return result;
}

JSValue profiledEvaluate(JSGlobalObject* globalObject, ProfilingReason reason, const SourceCode& source, JSValue thisValue, NakedPtr<Exception>& returnedException)
{
    ScriptProfilingScope profilingScope(globalObject, reason);
    return evaluate(globalObject, source, thisValue, returnedException);
}

JSValue evaluateWithScopeExtension(JSGlobalObject* globalObject, const SourceCode& source, JSObject* scopeExtensionObject, NakedPtr<Exception>& returnedException)
{
    VM& vm = globalObject->vm();

    if (scopeExtensionObject) {
        JSScope* ignoredPreviousScope = globalObject->globalScope();
        globalObject->setGlobalScopeExtension(JSWithScope::create(vm, globalObject, ignoredPreviousScope, scopeExtensionObject));
    }

    JSValue returnValue = JSC::evaluate(globalObject, source, globalObject, returnedException);

    if (scopeExtensionObject)
        globalObject->clearGlobalScopeExtension();

    return returnValue;
}

static JSPromise* rejectPromise(ThrowScope& scope, JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
    return promise->rejectWithCaughtException(globalObject, scope);
}

JSPromise* loadAndEvaluateModule(JSGlobalObject* globalObject, const String& moduleName, RefPtr<ScriptFetchParameters> parameters, RefPtr<ScriptFetcher> scriptFetcher)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::currentSingleton().atomStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    Identifier resolved = globalObject->moduleLoader()->resolve(globalObject, Identifier::fromString(vm, moduleName), { }, scriptFetcher, /* useImportMap */ false);
    RETURN_IF_EXCEPTION(scope, nullptr);

    RELEASE_AND_RETURN(scope, globalObject->moduleLoader()->loadModule(globalObject, resolved, WTF::move(parameters), WTF::move(scriptFetcher), /* evaluate */ true, /* dynamic */ false, /* useImportMap */ false));
}

static ScriptFetchParameters::Type getSourceType(const SourceCode& source)
{
    switch (source.provider()->sourceType()) {
    case SourceProviderSourceType::JSON:
        return ScriptFetchParameters::Type::JSON;
    case SourceProviderSourceType::WebAssembly:
        return ScriptFetchParameters::Type::WebAssembly;
    case SourceProviderSourceType::Module:
        return ScriptFetchParameters::Type::JavaScript;
    default:
        return ScriptFetchParameters::Type::None;
    }
}

JSPromise* loadAndEvaluateModule(JSGlobalObject* globalObject, SourceCode&& source, RefPtr<ScriptFetcher> scriptFetcher)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::currentSingleton().atomStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    Identifier key = createEntrypointModuleKey(vm);
    ScriptFetchParameters::Type type = getSourceType(source);

    // Insert the given source code to the ModuleLoader registry as the fetched registry entry.
    globalObject->moduleLoader()->provideFetch(globalObject, key, type, WTF::move(source));
    RETURN_IF_EXCEPTION(scope, rejectPromise(scope, globalObject));

    JSPromise* statePromise = JSPromise::create(vm, globalObject->promiseStructure());
    RETURN_IF_EXCEPTION(scope, rejectPromise(scope, globalObject));
    auto graphLoadingState = ModuleGraphLoadingState::create(vm, statePromise, scriptFetcher);
    RETURN_IF_EXCEPTION(scope, rejectPromise(scope, globalObject));

    AbstractModuleRecord::ModuleRequest request { WTF::move(key), ScriptFetchParameters::create(type) };

    JSPromise* promise = globalObject->moduleLoader()->loadModule(globalObject, globalObject, request, graphLoadingState, WTF::move(scriptFetcher), /* evaluate */ true, /* useImportMap */ false);
    RETURN_IF_EXCEPTION(scope, rejectPromise(scope, globalObject));

    JSPromise* resultPromise = JSPromise::create(vm, globalObject->promiseStructure());
    resultPromise->markAsHandled();
    promise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ModuleLoadReturnModuleKey, resultPromise, jsUndefined());

    return resultPromise;
}

JSPromise* loadModule(JSGlobalObject* globalObject, const Identifier& moduleKey, RefPtr<ScriptFetchParameters> parameters, RefPtr<ScriptFetcher> scriptFetcher)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::currentSingleton().atomStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    return globalObject->moduleLoader()->loadModule(globalObject, moduleKey, WTF::move(parameters), WTF::move(scriptFetcher), /* evaluate */ false, /* dynamic */ false, /* useImportMap */ false);
}

JSPromise* loadModule(JSGlobalObject* globalObject, SourceCode&& source, RefPtr<ScriptFetcher> scriptFetcher)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::currentSingleton().atomStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    Identifier key = createEntrypointModuleKey(vm);

    // Insert the given source code to the ModuleLoader registry as the fetched registry entry.
    globalObject->moduleLoader()->provideFetch(globalObject, key, getSourceType(source), WTF::move(source));
    RETURN_IF_EXCEPTION(scope, rejectPromise(scope, globalObject));
    RELEASE_AND_RETURN(scope, globalObject->moduleLoader()->loadModule(globalObject, key, nullptr, WTF::move(scriptFetcher), /* evaluate */ false, /* dynamic */ false, /* useImportMap */ false));
}

JSPromise* linkAndEvaluateModule(JSGlobalObject* globalObject, const Identifier& moduleKey, RefPtr<ScriptFetcher> scriptFetcher)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::currentSingleton().atomStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    return globalObject->moduleLoader()->linkAndEvaluateModule(globalObject, moduleKey, nullptr, WTF::move(scriptFetcher));
}

JSPromise* importModule(JSGlobalObject* globalObject, const Identifier& moduleName, const Identifier& referrer, RefPtr<ScriptFetchParameters> parameters, RefPtr<ScriptFetcher> scriptFetcher)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::currentSingleton().atomStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    return globalObject->moduleLoader()->requestImportModule(globalObject, moduleName, referrer, WTF::move(parameters), WTF::move(scriptFetcher));
}

UncheckedKeyHashMap<RefPtr<UniquedStringImpl>, String> retrieveImportAttributesFromDynamicImportOptions(JSGlobalObject* globalObject, JSValue options, const Vector<RefPtr<UniquedStringImpl>>& supportedImportAttributes)
{
    // https://tc39.es/proposal-import-attributes/#sec-evaluate-import-call

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (options.isUndefined())
        return { };

    auto* optionsObject = dynamicDowncast<JSObject>(options);
    if (!optionsObject) [[unlikely]] {
        throwTypeError(globalObject, scope, "dynamic import's options should be an object"_s);
        return { };
    }

    JSValue attributes = optionsObject->get(globalObject, vm.propertyNames->withKeyword);
    RETURN_IF_EXCEPTION(scope, { });

    if (attributes.isUndefined())
        return { };

    auto* attributesObject = dynamicDowncast<JSObject>(attributes);
    if (!attributesObject) [[unlikely]] {
        throwTypeError(globalObject, scope, "dynamic import's options.with should be an object"_s);
        return { };
    }

    PropertyNameArrayBuilder properties(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
    attributesObject->methodTable()->getOwnPropertyNames(attributesObject, globalObject, properties, DontEnumPropertiesMode::Exclude);
    RETURN_IF_EXCEPTION(scope, { });

    UncheckedKeyHashMap<RefPtr<UniquedStringImpl>, String> result;
    for (auto& key : properties) {
        JSValue value = attributesObject->get(globalObject, key);
        RETURN_IF_EXCEPTION(scope, { });

        if (!value.isString()) [[unlikely]] {
            throwTypeError(globalObject, scope, "dynamic import's options.with includes non string property"_s);
            return { };
        }

        String valueString = value.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        result.add(key.impl(), WTF::move(valueString));
    }

    for (auto& [key, value] : result) {
        if (!supportedImportAttributes.contains(key.get())) [[unlikely]] {
            throwTypeError(globalObject, scope, makeString("dynamic import's options.with includes unsupported attribute \""_s, StringView(key.get()), "\""_s));
            return { };
        }
    }

    return result;
}

std::optional<ScriptFetchParameters::Type> retrieveTypeImportAttribute(JSGlobalObject* globalObject, const UncheckedKeyHashMap<RefPtr<UniquedStringImpl>, String>& attributes)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (attributes.isEmpty())
        return { };

    auto iterator = attributes.find(vm.propertyNames->type.impl());
    if (iterator == attributes.end())
        return { };

    String value = iterator->value;
    if (auto result = ScriptFetchParameters::parseType(value))
        return result;

    throwTypeError(globalObject, scope, makeString("Import attribute type \""_s, value, "\" is not valid"_s));
    return { };
}

} // namespace JSC
