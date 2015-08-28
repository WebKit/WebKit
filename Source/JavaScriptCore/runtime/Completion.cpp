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
        JSParserStrictMode::NotStrict, SourceParseMode::ProgramMode, error);
}

bool checkModuleSyntax(ExecState* exec, const SourceCode& source, ParserError& error)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    RELEASE_ASSERT(vm.atomicStringTable() == wtfThreadData().atomicStringTable());
    std::unique_ptr<ModuleProgramNode> moduleProgramNode = parse<ModuleProgramNode>(
        &vm, source, Identifier(), JSParserBuiltinMode::NotBuiltin,
        JSParserStrictMode::Strict, SourceParseMode::ModuleAnalyzeMode, error);
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

static JSInternalPromise* evaluateModule(const JSLockHolder&, ExecState* exec, JSGlobalObject* globalObject, JSValue moduleName, JSValue referrer)
{
    return globalObject->moduleLoader()->loadModule(exec, moduleName, referrer);
}

static JSInternalPromise* evaluateModule(const JSLockHolder& lock, ExecState* exec, JSGlobalObject* globalObject, const Identifier& moduleName)
{
    JSValue moduleNameValue;
    if (moduleName.isSymbol())
        moduleNameValue = Symbol::create(exec->vm(), static_cast<SymbolImpl&>(*moduleName.impl()));
    else
        moduleNameValue = jsString(&exec->vm(), moduleName.impl());

    return evaluateModule(lock, exec, globalObject, moduleNameValue, jsUndefined());
}

JSInternalPromise* evaluateModule(ExecState* exec, const SourceCode& source)
{
    JSLockHolder lock(exec);
    RELEASE_ASSERT(exec->vm().atomicStringTable() == wtfThreadData().atomicStringTable());
    RELEASE_ASSERT(!exec->vm().isCollectorBusy());

    JSGlobalObject* globalObject = exec->vmEntryGlobalObject();

    // Generate the unique key for the source-provided module.
    PrivateName privateName(PrivateName::Description, "EntryPointModule");
    Symbol* key = Symbol::create(exec->vm(), *privateName.uid());

    // Insert the given source code to the ModuleLoader registry as the fetched registry entry.
    globalObject->moduleLoader()->provide(exec, key, ModuleLoaderObject::Status::Fetch, source.toString());
    if (exec->hadException()) {
        JSValue exception = exec->exception()->value();
        exec->clearException();
        JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::create(exec, globalObject);
        deferred->reject(exec, exception);
        return deferred->promise();
    }

    return evaluateModule(lock, exec, globalObject, key, jsUndefined());
}

JSInternalPromise* evaluateModule(ExecState* exec, const Identifier& moduleName)
{
    JSLockHolder lock(exec);
    RELEASE_ASSERT(exec->vm().atomicStringTable() == wtfThreadData().atomicStringTable());
    RELEASE_ASSERT(!exec->vm().isCollectorBusy());

    return evaluateModule(lock, exec, exec->vmEntryGlobalObject(), moduleName);
}

JSInternalPromise* evaluateModule(ExecState* exec, const String& moduleName)
{
    JSLockHolder lock(exec);
    RELEASE_ASSERT(exec->vm().atomicStringTable() == wtfThreadData().atomicStringTable());
    RELEASE_ASSERT(!exec->vm().isCollectorBusy());

    return evaluateModule(lock, exec, exec->vmEntryGlobalObject(), Identifier::fromString(exec, moduleName));
}

} // namespace JSC
