/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007, 2013 Apple Inc.
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
#include "CodeProfiling.h"
#include "Debugger.h"
#include "Exception.h"
#include "Interpreter.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseDeferred.h"
#include "JSLock.h"
#include "JSModuleRecord.h"
#include "ModuleAnalyzer.h"
#include "ModuleLoaderObject.h"
#include "Parser.h"
#include "ScriptProfilingScope.h"
#include <wtf/WTFThreadData.h>

namespace JSC {

bool checkSyntax(ExecState* exec, const SourceCode& source, JSValue* returnedException)
{
    JSLockHolder lock(exec);
    RELEASE_ASSERT(exec->vm().atomicStringTable() == wtfThreadData().atomicStringTable());

    ProgramExecutable* program = ProgramExecutable::create(exec, source);
    JSObject* error = program->checkSyntax(exec);
    if (error) {
        if (returnedException)
            *returnedException = error;
        return false;
    }

    return true;
}
    
bool checkSyntax(VM& vm, const SourceCode& source, ParserError& error)
{
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == wtfThreadData().atomicStringTable());
    return !!parse<ProgramNode>(
        &vm, source, Identifier(), JSParserBuiltinMode::NotBuiltin,
        JSParserStrictMode::NotStrict, SourceParseMode::ProgramMode, SuperBinding::NotNeeded, error);
}

bool checkModuleSyntax(ExecState* exec, const SourceCode& source, ParserError& error)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == wtfThreadData().atomicStringTable());
    std::unique_ptr<ModuleProgramNode> moduleProgramNode = parse<ModuleProgramNode>(
        &vm, source, Identifier(), JSParserBuiltinMode::NotBuiltin,
        JSParserStrictMode::Strict, SourceParseMode::ModuleAnalyzeMode, SuperBinding::NotNeeded, error);
    if (!moduleProgramNode)
        return false;

    PrivateName privateName(PrivateName::Description, "EntryPointModule");
    ModuleAnalyzer moduleAnalyzer(exec, Identifier::fromUid(privateName), source, moduleProgramNode->varDeclarations(), moduleProgramNode->lexicalVariables());
    moduleAnalyzer.analyze(*moduleProgramNode);
    return true;
}

JSValue evaluate(ExecState* exec, const SourceCode& source, JSValue thisValue, NakedPtr<Exception>& returnedException)
{
    JSLockHolder lock(exec);
    RELEASE_ASSERT(exec->vm().atomicStringTable() == wtfThreadData().atomicStringTable());
    RELEASE_ASSERT(!exec->vm().isCollectorBusy());

    CodeProfiling profile(source);

    ProgramExecutable* program = ProgramExecutable::create(exec, source);
    if (!program) {
        returnedException = exec->vm().exception();
        exec->vm().clearException();
        return jsUndefined();
    }

    if (!thisValue || thisValue.isUndefinedOrNull())
        thisValue = exec->vmEntryGlobalObject();
    JSObject* thisObj = jsCast<JSObject*>(thisValue.toThis(exec, NotStrictMode));
    JSValue result = exec->interpreter()->execute(program, exec, thisObj);

    if (exec->hadException()) {
        returnedException = exec->exception();
        exec->clearException();
        return jsUndefined();
    }

    RELEASE_ASSERT(result);
    return result;
}

JSValue profiledEvaluate(ExecState* exec, ProfilingReason reason, const SourceCode& source, JSValue thisValue, NakedPtr<Exception>& returnedException)
{
    ScriptProfilingScope profilingScope(exec->vmEntryGlobalObject(), reason);
    return evaluate(exec, source, thisValue, returnedException);
}

static JSValue identifierToJSValue(VM& vm, const Identifier& identifier)
{
    if (identifier.isSymbol())
        return Symbol::create(vm, static_cast<SymbolImpl&>(*identifier.impl()));
    return jsString(&vm, identifier.impl());
}

static Symbol* createSymbolForEntryPointModule(VM& vm)
{
    // Generate the unique key for the source-provided module.
    PrivateName privateName(PrivateName::Description, "EntryPointModule");
    return Symbol::create(vm, *privateName.uid());
}

static JSInternalPromise* rejectPromise(ExecState* exec, JSGlobalObject* globalObject)
{
    ASSERT(exec->hadException());
    JSValue exception = exec->exception()->value();
    exec->clearException();
    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::create(exec, globalObject);
    deferred->reject(exec, exception);
    return deferred->promise();
}

static JSInternalPromise* loadAndEvaluateModule(const JSLockHolder&, ExecState* exec, JSGlobalObject* globalObject, JSValue moduleName, JSValue referrer)
{
    return globalObject->moduleLoader()->loadAndEvaluateModule(exec, moduleName, referrer);
}

static JSInternalPromise* loadAndEvaluateModule(const JSLockHolder& lock, ExecState* exec, JSGlobalObject* globalObject, const Identifier& moduleName)
{
    return loadAndEvaluateModule(lock, exec, globalObject, identifierToJSValue(exec->vm(), moduleName), jsUndefined());
}

JSInternalPromise* loadAndEvaluateModule(ExecState* exec, const String& moduleName)
{
    JSLockHolder lock(exec);
    RELEASE_ASSERT(exec->vm().atomicStringTable() == wtfThreadData().atomicStringTable());
    RELEASE_ASSERT(!exec->vm().isCollectorBusy());

    return loadAndEvaluateModule(lock, exec, exec->vmEntryGlobalObject(), Identifier::fromString(exec, moduleName));
}

JSInternalPromise* loadAndEvaluateModule(ExecState* exec, const SourceCode& source)
{
    JSLockHolder lock(exec);
    RELEASE_ASSERT(exec->vm().atomicStringTable() == wtfThreadData().atomicStringTable());
    RELEASE_ASSERT(!exec->vm().isCollectorBusy());

    Symbol* key = createSymbolForEntryPointModule(exec->vm());

    JSGlobalObject* globalObject = exec->vmEntryGlobalObject();

    // Insert the given source code to the ModuleLoader registry as the fetched registry entry.
    globalObject->moduleLoader()->provide(exec, key, ModuleLoaderObject::Status::Fetch, source.view().toString());
    if (exec->hadException())
        return rejectPromise(exec, globalObject);

    return loadAndEvaluateModule(lock, exec, globalObject, key, jsUndefined());
}

static JSInternalPromise* loadModule(const JSLockHolder&, ExecState* exec, JSGlobalObject* globalObject, JSValue moduleName, JSValue referrer)
{
    return globalObject->moduleLoader()->loadModule(exec, moduleName, referrer);
}

static JSInternalPromise* loadModule(const JSLockHolder& lock, ExecState* exec, JSGlobalObject* globalObject, const Identifier& moduleName)
{
    return loadModule(lock, exec, globalObject, identifierToJSValue(exec->vm(), moduleName), jsUndefined());
}

JSInternalPromise* loadModule(ExecState* exec, const String& moduleName)
{
    JSLockHolder lock(exec);
    RELEASE_ASSERT(exec->vm().atomicStringTable() == wtfThreadData().atomicStringTable());
    RELEASE_ASSERT(!exec->vm().isCollectorBusy());

    return loadModule(lock, exec, exec->vmEntryGlobalObject(), Identifier::fromString(exec, moduleName));
}

JSInternalPromise* loadModule(ExecState* exec, const SourceCode& source)
{
    JSLockHolder lock(exec);
    RELEASE_ASSERT(exec->vm().atomicStringTable() == wtfThreadData().atomicStringTable());
    RELEASE_ASSERT(!exec->vm().isCollectorBusy());

    Symbol* key = createSymbolForEntryPointModule(exec->vm());

    JSGlobalObject* globalObject = exec->vmEntryGlobalObject();

    // Insert the given source code to the ModuleLoader registry as the fetched registry entry.
    globalObject->moduleLoader()->provide(exec, key, ModuleLoaderObject::Status::Fetch, source.view().toString());
    if (exec->hadException())
        return rejectPromise(exec, globalObject);

    return loadModule(lock, exec, globalObject, key, jsUndefined());
}

JSInternalPromise* linkAndEvaluateModule(ExecState* exec, const Identifier& moduleKey)
{
    JSLockHolder lock(exec);
    RELEASE_ASSERT(exec->vm().atomicStringTable() == wtfThreadData().atomicStringTable());
    RELEASE_ASSERT(!exec->vm().isCollectorBusy());

    JSGlobalObject* globalObject = exec->vmEntryGlobalObject();
    return globalObject->moduleLoader()->linkAndEvaluateModule(exec, identifierToJSValue(exec->vm(), moduleKey));
}

} // namespace JSC
