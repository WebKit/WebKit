/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2019 Apple Inc.
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

#include "BytecodeCacheError.h"
#include "CatchScope.h"
#include "CodeCache.h"
#include "Exception.h"
#include "IdentifierInlines.h"
#include "Interpreter.h"
#include "JSGlobalObject.h"
#include "JSInternalPromise.h"
#include "JSLock.h"
#include "JSModuleLoader.h"
#include "JSWithScope.h"
#include "ModuleAnalyzer.h"
#include "Parser.h"
#include "ScriptProfilingScope.h"

namespace JSC {

static inline bool checkSyntaxInternal(VM& vm, const SourceCode& source, ParserError& error)
{
    return !!parse<ProgramNode>(
        vm, source, Identifier(), JSParserBuiltinMode::NotBuiltin,
        JSParserStrictMode::NotStrict, JSParserScriptMode::Classic, SourceParseMode::ProgramMode, SuperBinding::NotNeeded, error);
}

bool checkSyntax(JSGlobalObject* globalObject, const SourceCode& source, JSValue* returnedException)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::current().atomStringTable());

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
    RELEASE_ASSERT(vm.atomStringTable() == Thread::current().atomStringTable());
    return checkSyntaxInternal(vm, source, error);
}

bool checkModuleSyntax(JSGlobalObject* globalObject, const SourceCode& source, ParserError& error)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::current().atomStringTable());
    std::unique_ptr<ModuleProgramNode> moduleProgramNode = parse<ModuleProgramNode>(
        vm, source, Identifier(), JSParserBuiltinMode::NotBuiltin,
        JSParserStrictMode::Strict, JSParserScriptMode::Module, SourceParseMode::ModuleAnalyzeMode, SuperBinding::NotNeeded, error);
    if (!moduleProgramNode)
        return false;

    PrivateName privateName(PrivateName::Description, "EntryPointModule");
    ModuleAnalyzer moduleAnalyzer(globalObject, Identifier::fromUid(privateName), source, moduleProgramNode->varDeclarations(), moduleProgramNode->lexicalVariables());
    moduleAnalyzer.analyze(*moduleProgramNode);
    return true;
}

RefPtr<CachedBytecode> generateProgramBytecode(VM& vm, const SourceCode& source, FileSystem::PlatformFileHandle fd, BytecodeCacheError& error)
{
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::current().atomStringTable());

    VariableEnvironment variablesUnderTDZ;
    JSParserStrictMode strictMode = JSParserStrictMode::NotStrict;
    JSParserScriptMode scriptMode = JSParserScriptMode::Classic;
    EvalContextType evalContextType = EvalContextType::None;

    ParserError parserError;
    UnlinkedCodeBlock* unlinkedCodeBlock = recursivelyGenerateUnlinkedCodeBlockForProgram(vm, source, strictMode, scriptMode, { }, parserError, evalContextType, &variablesUnderTDZ);
    if (parserError.isValid())
        error = parserError;
    if (!unlinkedCodeBlock)
        return nullptr;

    return serializeBytecode(vm, unlinkedCodeBlock, source, SourceCodeType::ProgramType, strictMode, scriptMode, fd, error, { });
}

RefPtr<CachedBytecode> generateModuleBytecode(VM& vm, const SourceCode& source, FileSystem::PlatformFileHandle fd, BytecodeCacheError& error)
{
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::current().atomStringTable());

    VariableEnvironment variablesUnderTDZ;
    JSParserStrictMode strictMode = JSParserStrictMode::Strict;
    JSParserScriptMode scriptMode = JSParserScriptMode::Module;
    EvalContextType evalContextType = EvalContextType::None;

    ParserError parserError;
    UnlinkedCodeBlock* unlinkedCodeBlock = recursivelyGenerateUnlinkedCodeBlockForModuleProgram(vm, source, strictMode, scriptMode, { }, parserError, evalContextType, &variablesUnderTDZ);
    if (parserError.isValid())
        error = parserError;
    if (!unlinkedCodeBlock)
        return nullptr;
    return serializeBytecode(vm, unlinkedCodeBlock, source, SourceCodeType::ModuleType, strictMode, scriptMode, fd, error, { });
}

JSValue evaluate(JSGlobalObject* globalObject, const SourceCode& source, JSValue thisValue, NakedPtr<Exception>& returnedException)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::current().atomStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    if (!thisValue || thisValue.isUndefinedOrNull())
        thisValue = globalObject;
    JSObject* thisObj = jsCast<JSObject*>(thisValue.toThis(globalObject, ECMAMode::sloppy()));
    JSValue result = vm.interpreter->executeProgram(source, globalObject, thisObj);

    if (scope.exception()) {
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

static Symbol* createSymbolForEntryPointModule(VM& vm)
{
    // Generate the unique key for the source-provided module.
    PrivateName privateName(PrivateName::Description, "EntryPointModule");
    return Symbol::create(vm, privateName.uid());
}

static JSInternalPromise* rejectPromise(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    scope.assertNoException();
    JSValue exception = scope.exception()->value();
    scope.clearException();
    JSInternalPromise* promise = JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
    promise->reject(globalObject, exception);
    return promise;
}

JSInternalPromise* loadAndEvaluateModule(JSGlobalObject* globalObject, Symbol* moduleId, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::current().atomStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    return globalObject->moduleLoader()->loadAndEvaluateModule(globalObject, moduleId, parameters, scriptFetcher);
}

JSInternalPromise* loadAndEvaluateModule(JSGlobalObject* globalObject, const String& moduleName, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::current().atomStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    return globalObject->moduleLoader()->loadAndEvaluateModule(globalObject, identifierToJSValue(vm, Identifier::fromString(vm, moduleName)), parameters, scriptFetcher);
}

JSInternalPromise* loadAndEvaluateModule(JSGlobalObject* globalObject, const SourceCode& source, JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::current().atomStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    Symbol* key = createSymbolForEntryPointModule(vm);

    // Insert the given source code to the ModuleLoader registry as the fetched registry entry.
    globalObject->moduleLoader()->provideFetch(globalObject, key, source);
    RETURN_IF_EXCEPTION(scope, rejectPromise(globalObject));

    return globalObject->moduleLoader()->loadAndEvaluateModule(globalObject, key, jsUndefined(), scriptFetcher);
}

JSInternalPromise* loadModule(JSGlobalObject* globalObject, const String& moduleName, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::current().atomStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    return globalObject->moduleLoader()->loadModule(globalObject, identifierToJSValue(vm, Identifier::fromString(vm, moduleName)), parameters, scriptFetcher);
}

JSInternalPromise* loadModule(JSGlobalObject* globalObject, const SourceCode& source, JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::current().atomStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    Symbol* key = createSymbolForEntryPointModule(vm);

    // Insert the given source code to the ModuleLoader registry as the fetched registry entry.
    // FIXME: Introduce JSSourceCode object to wrap around this source.
    globalObject->moduleLoader()->provideFetch(globalObject, key, source);
    RETURN_IF_EXCEPTION(scope, rejectPromise(globalObject));

    return globalObject->moduleLoader()->loadModule(globalObject, key, jsUndefined(), scriptFetcher);
}

JSValue linkAndEvaluateModule(JSGlobalObject* globalObject, const Identifier& moduleKey, JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::current().atomStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    return globalObject->moduleLoader()->linkAndEvaluateModule(globalObject, identifierToJSValue(vm, moduleKey), scriptFetcher);
}

JSInternalPromise* importModule(JSGlobalObject* globalObject, const Identifier& moduleKey, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomStringTable() == Thread::current().atomStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    return globalObject->moduleLoader()->requestImportModule(globalObject, moduleKey, parameters, scriptFetcher);
}

} // namespace JSC
