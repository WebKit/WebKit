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

#include "CallFrame.h"
#include "CatchScope.h"
#include "CodeCache.h"
#include "CodeProfiling.h"
#include "Exception.h"
#include "IdentifierInlines.h"
#include "Interpreter.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseDeferred.h"
#include "JSLock.h"
#include "JSModuleLoader.h"
#include "JSModuleRecord.h"
#include "JSWithScope.h"
#include "ModuleAnalyzer.h"
#include "Parser.h"
#include "ProgramExecutable.h"
#include "ScriptProfilingScope.h"

namespace JSC {

static inline bool checkSyntaxInternal(VM& vm, const SourceCode& source, ParserError& error)
{
    return !!parse<ProgramNode>(
        &vm, source, Identifier(), JSParserBuiltinMode::NotBuiltin,
        JSParserStrictMode::NotStrict, JSParserScriptMode::Classic, SourceParseMode::ProgramMode, SuperBinding::NotNeeded, error);
}

bool checkSyntax(ExecState* exec, const SourceCode& source, JSValue* returnedException)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == Thread::current().atomicStringTable());

    ParserError error;
    if (checkSyntaxInternal(vm, source, error))
        return true;
    ASSERT(error.isValid());
    if (returnedException)
        *returnedException = error.toErrorObject(exec->lexicalGlobalObject(), source);
    return false;
}

bool checkSyntax(VM& vm, const SourceCode& source, ParserError& error)
{
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == Thread::current().atomicStringTable());
    return checkSyntaxInternal(vm, source, error);
}

bool checkModuleSyntax(ExecState* exec, const SourceCode& source, ParserError& error)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == Thread::current().atomicStringTable());
    std::unique_ptr<ModuleProgramNode> moduleProgramNode = parse<ModuleProgramNode>(
        &vm, source, Identifier(), JSParserBuiltinMode::NotBuiltin,
        JSParserStrictMode::Strict, JSParserScriptMode::Module, SourceParseMode::ModuleAnalyzeMode, SuperBinding::NotNeeded, error);
    if (!moduleProgramNode)
        return false;

    PrivateName privateName(PrivateName::Description, "EntryPointModule");
    ModuleAnalyzer moduleAnalyzer(exec, Identifier::fromUid(privateName), source, moduleProgramNode->varDeclarations(), moduleProgramNode->lexicalVariables());
    moduleAnalyzer.analyze(*moduleProgramNode);
    return true;
}

CachedBytecode generateBytecode(VM& vm, const SourceCode& source, ParserError& error)
{
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == Thread::current().atomicStringTable());

    VariableEnvironment variablesUnderTDZ;
    JSParserStrictMode strictMode = JSParserStrictMode::NotStrict;
    JSParserScriptMode scriptMode = JSParserScriptMode::Classic;
    DebuggerMode debuggerMode = DebuggerOff;
    EvalContextType evalContextType = EvalContextType::None;

    UnlinkedCodeBlock* unlinkedCodeBlock = recursivelyGenerateUnlinkedCodeBlock<UnlinkedProgramCodeBlock>(vm, source, strictMode, scriptMode, debuggerMode, error, evalContextType, &variablesUnderTDZ);
    if (!unlinkedCodeBlock)
        return { };
    return serializeBytecode(vm, unlinkedCodeBlock, source, SourceCodeType::ProgramType, strictMode, scriptMode, debuggerMode);
}

CachedBytecode generateModuleBytecode(VM& vm, const SourceCode& source, ParserError& error)
{
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == Thread::current().atomicStringTable());

    VariableEnvironment variablesUnderTDZ;
    JSParserStrictMode strictMode = JSParserStrictMode::Strict;
    JSParserScriptMode scriptMode = JSParserScriptMode::Module;
    DebuggerMode debuggerMode = DebuggerOff;
    EvalContextType evalContextType = EvalContextType::None;

    UnlinkedCodeBlock* unlinkedCodeBlock = recursivelyGenerateUnlinkedCodeBlock<UnlinkedModuleProgramCodeBlock>(vm, source, strictMode, scriptMode, debuggerMode, error, evalContextType, &variablesUnderTDZ);
    if (!unlinkedCodeBlock)
        return { };
    return serializeBytecode(vm, unlinkedCodeBlock, source, SourceCodeType::ModuleType, strictMode, scriptMode, debuggerMode);
}

JSValue evaluate(ExecState* exec, const SourceCode& source, JSValue thisValue, NakedPtr<Exception>& returnedException)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == Thread::current().atomicStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    CodeProfiling profile(source);

    if (!thisValue || thisValue.isUndefinedOrNull())
        thisValue = vm.vmEntryGlobalObject(exec);
    JSObject* thisObj = jsCast<JSObject*>(thisValue.toThis(exec, NotStrictMode));
    JSValue result = vm.interpreter->executeProgram(source, exec, thisObj);

    if (scope.exception()) {
        returnedException = scope.exception();
        scope.clearException();
        return jsUndefined();
    }

    RELEASE_ASSERT(result);
    return result;
}

JSValue profiledEvaluate(ExecState* exec, ProfilingReason reason, const SourceCode& source, JSValue thisValue, NakedPtr<Exception>& returnedException)
{
    VM& vm = exec->vm();
    ScriptProfilingScope profilingScope(vm.vmEntryGlobalObject(exec), reason);
    return evaluate(exec, source, thisValue, returnedException);
}

JSValue evaluateWithScopeExtension(ExecState* exec, const SourceCode& source, JSObject* scopeExtensionObject, NakedPtr<Exception>& returnedException)
{
    VM& vm = exec->vm();
    JSGlobalObject* globalObject = vm.vmEntryGlobalObject(exec);

    if (scopeExtensionObject) {
        JSScope* ignoredPreviousScope = globalObject->globalScope();
        globalObject->setGlobalScopeExtension(JSWithScope::create(vm, globalObject, ignoredPreviousScope, scopeExtensionObject));
    }

    JSValue returnValue = JSC::evaluate(globalObject->globalExec(), source, globalObject, returnedException);

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

static JSInternalPromise* rejectPromise(ExecState* exec, JSGlobalObject* globalObject)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    scope.assertNoException();
    JSValue exception = scope.exception()->value();
    scope.clearException();
    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::tryCreate(exec, globalObject);
    scope.releaseAssertNoException();
    deferred->reject(exec, exception);
    scope.releaseAssertNoException();
    return deferred->promise();
}

JSInternalPromise* loadAndEvaluateModule(ExecState* exec, Symbol* moduleId, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == Thread::current().atomicStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    return vm.vmEntryGlobalObject(exec)->moduleLoader()->loadAndEvaluateModule(exec, moduleId, parameters, scriptFetcher);
}

JSInternalPromise* loadAndEvaluateModule(ExecState* exec, const String& moduleName, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == Thread::current().atomicStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    return vm.vmEntryGlobalObject(exec)->moduleLoader()->loadAndEvaluateModule(exec, identifierToJSValue(vm, Identifier::fromString(exec, moduleName)), parameters, scriptFetcher);
}

JSInternalPromise* loadAndEvaluateModule(ExecState* exec, const SourceCode& source, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == Thread::current().atomicStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    Symbol* key = createSymbolForEntryPointModule(vm);

    JSGlobalObject* globalObject = vm.vmEntryGlobalObject(exec);

    // Insert the given source code to the ModuleLoader registry as the fetched registry entry.
    globalObject->moduleLoader()->provideFetch(exec, key, source);
    RETURN_IF_EXCEPTION(scope, rejectPromise(exec, globalObject));

    return globalObject->moduleLoader()->loadAndEvaluateModule(exec, key, jsUndefined(), scriptFetcher);
}

JSInternalPromise* loadModule(ExecState* exec, const String& moduleName, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == Thread::current().atomicStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    return vm.vmEntryGlobalObject(exec)->moduleLoader()->loadModule(exec, identifierToJSValue(vm, Identifier::fromString(exec, moduleName)), parameters, scriptFetcher);
}

JSInternalPromise* loadModule(ExecState* exec, const SourceCode& source, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == Thread::current().atomicStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    Symbol* key = createSymbolForEntryPointModule(vm);

    JSGlobalObject* globalObject = vm.vmEntryGlobalObject(exec);

    // Insert the given source code to the ModuleLoader registry as the fetched registry entry.
    // FIXME: Introduce JSSourceCode object to wrap around this source.
    globalObject->moduleLoader()->provideFetch(exec, key, source);
    RETURN_IF_EXCEPTION(scope, rejectPromise(exec, globalObject));

    return globalObject->moduleLoader()->loadModule(exec, key, jsUndefined(), scriptFetcher);
}

JSValue linkAndEvaluateModule(ExecState* exec, const Identifier& moduleKey, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == Thread::current().atomicStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    JSGlobalObject* globalObject = vm.vmEntryGlobalObject(exec);
    return globalObject->moduleLoader()->linkAndEvaluateModule(exec, identifierToJSValue(vm, moduleKey), scriptFetcher);
}

JSInternalPromise* importModule(ExecState* exec, const Identifier& moduleKey, JSValue parameters, JSValue scriptFetcher)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == Thread::current().atomicStringTable());
    RELEASE_ASSERT(!vm.isCollectorBusyOnCurrentThread());

    return vm.vmEntryGlobalObject(exec)->moduleLoader()->requestImportModule(exec, moduleKey, parameters, scriptFetcher);
}

} // namespace JSC
