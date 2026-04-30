/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "CyclicModuleRecord.h"

#include "BuiltinNames.h"
#include "Interpreter.h"
#include "JSAsyncFunction.h"
#include "JSAsyncGeneratorFunction.h"
#include "JSCInlines.h"
#include "JSGeneratorFunction.h"
#include "JSModuleEnvironment.h"
#include "JSModuleLoader.h"
#include "JSModuleNamespaceObject.h"
#include "JSModuleRecord.h"
#include "JSPromise.h"
#include "ModuleProgramExecutable.h"
#include "SourceProfiler.h"
#include "UnlinkedModuleProgramCodeBlock.h"
#include "WebAssemblyModuleRecord.h"
#include <wtf/Scope.h>
#include <wtf/text/MakeString.h>

namespace JSC {

const ClassInfo CyclicModuleRecord::s_info = { "CyclicModuleRecord"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(CyclicModuleRecord) };

CyclicModuleRecord::CyclicModuleRecord(VM& vm, Structure* structure, const Identifier& moduleKey)
    : Base(vm, structure, moduleKey)
{
}

void CyclicModuleRecord::finishCreation(JSGlobalObject* globalObject, VM& vm)
{
    Base::finishCreation(globalObject, vm);
    ASSERT(inherits(info()));
}

template<typename Visitor>
void CyclicModuleRecord::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    CyclicModuleRecord* thisObject = uncheckedDowncast<CyclicModuleRecord>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_evaluationError);
}

DEFINE_VISIT_CHILDREN(CyclicModuleRecord);

void CyclicModuleRecord::initializeEnvironment(JSGlobalObject* globalObject, RefPtr<ScriptFetcher> scriptFetcher)
{
    // InitializeEnvironment
    // https://tc39.es/ecma262/#sec-source-text-module-record-initialize-environment

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (m_initialized)
        return;

    auto* jsModule = dynamicDowncast<JSModuleRecord>(this);
#if ENABLE(WEBASSEMBLY)
    auto* wasmModule = !jsModule ? uncheckedDowncast<WebAssemblyModuleRecord>(this) : nullptr;
#else
    ASSERT(jsModule);
#endif

    ModuleProgramExecutable* moduleProgramExecutable = nullptr;
    JSModuleEnvironment* env = nullptr;

    // 1. For each ExportEntry Record e of module.[[IndirectExportEntries]], do
    for (const auto& [key, e] : exportEntries()) {
        if (e.type != ExportEntry::Type::Indirect)
            continue;
        // 1.a. Assert: e.[[ExportName]] is not null.
        ASSERT(!e.exportName.isNull());
        // 1.b. Let resolution be module.ResolveExport(e.[[ExportName]]).
        Resolution resolution = resolveExport(globalObject, e.exportName);
        RETURN_IF_EXCEPTION(scope, void());
        // 1.c. If resolution is either null or AMBIGUOUS, throw a SyntaxError exception.
        switch (resolution.type) {
        case Resolution::Type::NotFound:
            throwSyntaxError(globalObject, scope, makeString("Indirectly exported binding name '"_s, StringView(e.exportName.impl()), "' is not found."_s));
            return;

        case Resolution::Type::Ambiguous:
            throwSyntaxError(globalObject, scope, makeString("Indirectly exported binding name '"_s, StringView(e.exportName.impl()), "' cannot be resolved due to ambiguous multiple bindings."_s));
            return;

        case Resolution::Type::Error:
            throwSyntaxError(globalObject, scope, "Indirectly exported binding name 'default' cannot be resolved by star export entries."_s);
            return;

        default:
            // 1.d. Assert: resolution is a ResolvedBinding Record.
            ASSERT(resolution.type == Resolution::Type::Resolved);
        }
    }
    // 2. Assert: All named exports from module are resolvable.
#if ASSERT_ENABLED
    for (const auto& [key, e] : exportEntries()) {
        if (e.type != ExportEntry::Type::Local || e.localName.isNull())
            continue;
        ASSERT(resolveExport(globalObject, e.exportName).type == Resolution::Type::Resolved);
        scope.assertNoExceptionExceptTermination();
    }
#endif
    // 3. Let realm be module.[[Realm]].
    // 4. Assert: realm is not undefined.
    SymbolTable* symbolTable = nullptr;
    if (jsModule) {
        // 5. Let env be NewModuleEnvironment(realm.[[GlobalEnv]]).
        moduleProgramExecutable = jsModule->getOrMakeExecutable(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
        symbolTable = moduleProgramExecutable->moduleEnvironmentSymbolTable();
        env = JSModuleEnvironment::create(vm, globalObject, globalObject->globalLexicalEnvironment(), symbolTable, jsTDZValue(), this);
        RETURN_IF_EXCEPTION(scope, void());
        // 6. Set module.[[Environment]] to env.
        setModuleEnvironment(globalObject, env);
        RETURN_IF_EXCEPTION(scope, void());
    }

    auto cleanup = WTF::makeScopeExit([&] {
        if (!m_initialized)
            m_moduleEnvironment.clear();
    });

    // 7. For each ImportEntry Record in of module.[[ImportEntries]], do
    if (jsModule) {
        for (const auto& [key, in] : importEntries()) {
            // 7.a. Let importedModule be GetImportedModule(module, in.[[ModuleRequest]]).
            AbstractModuleRecord* importedModule = hostResolveImportedModule(globalObject, in.moduleRequest);
            RETURN_IF_EXCEPTION(scope, void());
#if CPU(ADDRESS64)
            // rdar://107531050: Speculative crash mitigation
            if (importedModule == std::bit_cast<AbstractModuleRecord*>(encodedJSUndefined())) [[unlikely]] {
                RELEASE_ASSERT(vm.exceptionForInspection(), vm.traps().maybeNeedHandling(), vm.exceptionForInspection(), importedModule);
                RELEASE_ASSERT(vm.traps().maybeNeedHandling(), vm.traps().maybeNeedHandling(), vm.exceptionForInspection(), importedModule);
                if (!vm.exceptionForInspection() || !vm.traps().maybeNeedHandling()) {
                    throwSyntaxError(globalObject, scope, makeString("Importing module '"_s, String(in.moduleRequest.impl()), "' is not found."_s));
                    return;
                }
            }
#endif
            // 7.b. If in.[[ImportName]] is NAMESPACE-OBJECT, then
            if (in.type == ImportEntryType::Namespace) {
                // 7.b.i. Let namespace be GetModuleNamespace(importedModule).
                JSModuleNamespaceObject* ns = importedModule->getModuleNamespace(globalObject);
                RETURN_IF_EXCEPTION(scope, void());
                // 7.b.ii. Perform ! env.CreateImmutableBinding(in.[[LocalName]], true).
                // 7.b.iii. Perform ! env.InitializeBinding(in.[[LocalName]], namespace).
                bool putResult = false;
                symbolTablePutTouchWatchpointSet(env, globalObject, in.localName, ns, /* shouldThrowReadOnlyError */ false, /* ignoreReadOnlyErrors */ true, putResult);
                RETURN_IF_EXCEPTION(scope, void());
            // 7.c. Else,
            } else {
                ASSERT(in.type == ImportEntryType::Single);
                // 7.c.i. Let resolution be importedModule.ResolveExport(in.[[ImportName]]).
                Resolution resolution = importedModule->resolveExport(globalObject, in.importName);
                RETURN_IF_EXCEPTION(scope, void());
                switch (resolution.type) {
                // 7.c.ii. If resolution is either null or AMBIGUOUS, throw a SyntaxError exception.
                case Resolution::Type::NotFound:
                    throwSyntaxError(globalObject, scope, makeString("Importing binding name '"_s, StringView(in.importName.impl()), "' is not found."_s));
                    return;

                case Resolution::Type::Ambiguous:
                    throwSyntaxError(globalObject, scope, makeString("Importing binding name '"_s, StringView(in.importName.impl()), "' cannot be resolved due to ambiguous multiple bindings."_s));
                    return;

                case Resolution::Type::Error:
                    throwSyntaxError(globalObject, scope, "Importing binding name 'default' cannot be resolved by star export entries."_s);
                    return;

                case Resolution::Type::Resolved:
                    // 7.c.iii. If resolution.[[BindingName]] is NAMESPACE, then
                    if (vm.propertyNames->starNamespacePrivateName == resolution.localName) {
                        // 7.c.iii.1. Let namespace be GetModuleNamespace(resolution.[[Module]]).
                        JSModuleNamespaceObject* ns = resolution.moduleRecord->getModuleNamespace(globalObject); // Force module namespace object materialization.
                        RETURN_IF_EXCEPTION(scope, void());
                        // 7.c.iii.2. Perform ! env.CreateImmutableBinding(in.[[LocalName]], true).
                        // 7.c.iii.3. Perform ! env.InitializeBinding(in.[[LocalName]], namespace).
                        bool putResult = false;
                        symbolTablePutTouchWatchpointSet(env, globalObject, in.localName, ns, /* shouldThrowReadOnlyError */ false, /* ignoreReadOnlyErrors */ true, putResult);
                        RETURN_IF_EXCEPTION(scope, void());
                    // 7.c.iv. Else,
                    } else {
                        // 7.c.iv.1. Perform CreateImportBinding(env, in.[[LocalName]], resolution.[[Module]], resolution.[[BindingName]]).
                        // (Already handled through lazy resolution.)
                    }
                    break;
                }
            }
        }
    }

    // 8. Let moduleContext be a new ECMAScript code execution context.
    // 9. Set the Function of moduleContext to null.
    // 10. Assert: module.[[Realm]] is not undefined.
    // 11. Set the Realm of moduleContext to module.[[Realm]].
    // 12. Set the ScriptOrModule of moduleContext to module.
    // 13. Set the VariableEnvironment of moduleContext to module.[[Environment]].
    // 14. Set the LexicalEnvironment of moduleContext to module.[[Environment]].
    // 15. Set the PrivateEnvironment of moduleContext to null.
    // 16. Set module.[[Context]] to moduleContext.
    // 17. Push moduleContext onto the execution context stack; moduleContext is now the running execution context.

    // (8 through 17 not implemented here; handled during module evaluation.)

#if ENABLE(WEBASSEMBLY)
    if (!jsModule) {
        ASSERT(wasmModule);
        if (!wasmModule->moduleEnvironmentMayBeNull()) {
            wasmModule->link(globalObject, scriptFetcher);
            JSModuleLoader::attachErrorInfo(globalObject, scope, wasmModule, wasmModule->moduleKey(), ScriptFetchParameters::WebAssembly, JSModuleLoader::ModuleFailure::Kind::Instantiation);
            RETURN_IF_EXCEPTION(scope, void());
        }
        env = wasmModule->moduleEnvironment();
        ASSERT(env);
        m_initialized = true;
        return;
    }
#else
    if (!jsModule)
        return;
#endif

    // 18. Let code be module.[[ECMAScriptCode]].
    // 19. Let varDeclarations be the VarScopedDeclarations of code.
    // 20. Let declaredVarNames be a new empty List.
    // 21. For each element d of varDeclarations, do
    for (const auto& variable : jsModule->declaredVariables()) {
        // 21.a. For each element dn of the BoundNames of d, do
        // 21.a.i. If declaredVarNames does not contain dn, then
        // 21.a.i.1. Perform ! env.CreateMutableBinding(dn, false).
        // 21.a.i.2. Perform ! env.InitializeBinding(dn, undefined).
        // 21.a.i.3. Append dn to declaredVarNames.
        // Module environment contains the heap allocated "var", "function", "let", "const", and "class".
        // When creating the environment, we initialized all the slots with empty, it's ok for lexical values.
        // But for "var" and "function", we should initialize it with undefined. They are contained in the declared variables.
        SymbolTableEntry entry = symbolTable->get(variable.key.get());
        VarOffset offset = entry.varOffset();
        if (!offset.isStack()) {
            bool putResult = false;
            symbolTablePutTouchWatchpointSet(env, globalObject, Identifier::fromUid(vm, variable.key.get()), jsUndefined(), /* shouldThrowReadOnlyError */ false, /* ignoreReadOnlyErrors */ true, putResult);
            RETURN_IF_EXCEPTION(scope, void());
        }
    }

    // 22. Let lexDeclarations be the LexicallyScopedDeclarations of code.
    UnlinkedModuleProgramCodeBlock* unlinkedCodeBlock = moduleProgramExecutable->unlinkedCodeBlock();
    // 23. Let privateEnv be null.
    // 24. For each element d of lexDeclarations, do
    for (size_t i = 0, numberOfFunctions = unlinkedCodeBlock->numberOfFunctionDecls(); i < numberOfFunctions; ++i) {
        // 24.a. For each element dn of the BoundNames of d, do
        // 24.a.i. If IsConstantDeclaration of d is true, then
        // 24.a.i.1. Perform ! env.CreateImmutableBinding(dn, true).
        // 24.a.ii. Else,
        // 24.a.ii.1. Perform ! env.CreateMutableBinding(dn, false).
        UnlinkedFunctionExecutable* unlinkedFunctionExecutable = unlinkedCodeBlock->functionDecl(i);
        SymbolTableEntry entry = symbolTable->get(unlinkedFunctionExecutable->name().impl());
        VarOffset offset = entry.varOffset();
        if (!offset.isStack()) {
            ASSERT(!unlinkedFunctionExecutable->name().isEmpty());
            if (vm.typeProfiler() || vm.controlFlowProfiler()) {
                vm.functionHasExecutedCache()->insertUnexecutedRange(moduleProgramExecutable->sourceID(),
                    unlinkedFunctionExecutable->unlinkedFunctionStart(),
                    unlinkedFunctionExecutable->unlinkedFunctionEnd());
            }
            // 24.a.iii. If d is either a FunctionDeclaration, a GeneratorDeclaration, an AsyncFunctionDeclaration, or an AsyncGeneratorDeclaration, then
            // 24.a.iii.1. Let fo be InstantiateFunctionObject of d with arguments env and privateEnv.
            auto* executable = unlinkedFunctionExecutable->link(vm, moduleProgramExecutable, moduleProgramExecutable->source());
            RETURN_IF_EXCEPTION(scope, void());
            SourceParseMode parseMode = executable->parseMode();
            JSFunction* function = nullptr;
            if (isAsyncGeneratorWrapperParseMode(parseMode))
                function = JSAsyncGeneratorFunction::create(vm, globalObject, executable, env);
            else if (isGeneratorWrapperParseMode(parseMode))
                function = JSGeneratorFunction::create(vm, globalObject, executable, env);
            else if (isAsyncFunctionWrapperParseMode(parseMode))
                function = JSAsyncFunction::create(vm, globalObject, executable, env);
            else
                function = JSFunction::create(vm, globalObject, executable, env);
            RETURN_IF_EXCEPTION(scope, void());
            // 24.a.iii.2. Perform ! env.InitializeBinding(dn, fo).
            bool putResult = false;
            symbolTablePutTouchWatchpointSet(env, globalObject, unlinkedFunctionExecutable->name(), function, /* shouldThrowReadOnlyError */ false, /* ignoreReadOnlyErrors */ true, putResult);
            RETURN_IF_EXCEPTION(scope, void());
        }
    }

    if (jsModule->features() & ImportMetaFeature) {
        JSObject* metaProperties = globalObject->moduleLoader()->createImportMetaProperties(globalObject, identifierToJSValue(vm, moduleKey()), jsModule, scriptFetcher);
        RETURN_IF_EXCEPTION(scope, void());
        bool putResult = false;
        symbolTablePutTouchWatchpointSet(env, globalObject, vm.propertyNames->builtinNames().metaPrivateName(), metaProperties, /* shouldThrowReadOnlyError */ false, /* ignoreReadOnlyErrors */ true, putResult);
        RETURN_IF_EXCEPTION(scope, void());
    }

    // 25. Remove moduleContext from the execution context stack.
    // 26. Return UNUSED.
    m_initialized = true;
}

void CyclicModuleRecord::link(JSGlobalObject* globalObject, RefPtr<ScriptFetcher> scriptFetcher)
{
    // Link()
    // https://tc39.es/ecma262/#sec-moduledeclarationlinking

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Assert: module.[[Status]] is one of UNLINKED, LINKED, EVALUATING-ASYNC, or EVALUATED.
    ASSERT(status() == Status::Unlinked || status() == Status::Linked || status() == Status::EvaluatingAsync || status() == Status::Evaluated);
    // 2. Let stack be a new empty List.
    Vector<CyclicModuleRecord*, 8> stack;
    // 3. Let result be Completion(InnerModuleLinking(module, stack, 0)).
    innerModuleLinking(globalObject, stack, 0, WTF::move(scriptFetcher));
    // 4. If result is an abrupt completion, then
    if (Exception* exception = scope.exception()) {
        // 4.a. For each Cyclic Module Record m of stack, do
        for (CyclicModuleRecord* m : stack) {
            // 4.a.i. Assert: m.[[Status]] is LINKING.
            ASSERT(m->status() == Status::Linking);
            // 4.a.ii. Set m.[[Status]] to UNLINKED.
            m->setStatus(Status::Unlinked);
        }
        // 4.b. Assert: module.[[Status]] is UNLINKED.
        ASSERT(status() == Status::Unlinked);
        // 4.c. Return ? result.
        JSModuleLoader::attachErrorInfo(globalObject, exception, this, moduleKey(), moduleType(), JSModuleLoader::ModuleFailure::Kind::Instantiation);
        return;
    }
    // 5. Assert: module.[[Status]] is one of LINKED, EVALUATING-ASYNC, or EVALUATED.
    ASSERT(status() == Status::Linked || status() == Status::EvaluatingAsync || status() == Status::Evaluated);
    // 6. Assert: stack is empty.
    ASSERT(stack.isEmpty());
    // 7. Return UNUSED.
}

JSPromise* CyclicModuleRecord::evaluate(JSGlobalObject* globalObject)
{
    // https://tc39.es/ecma262/#sec-moduleevaluation

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Assert: This call to Evaluate is not happening at the same time as another call to Evaluate within the surrounding agent.
    // FIXME: is this needed?
    // 2. Assert: module.[[Status]] is one of LINKED, EVALUATING-ASYNC, or EVALUATED.
    ASSERT(m_status == Status::Linked || m_status == Status::EvaluatingAsync || m_status == Status::Evaluated);
    CyclicModuleRecord* module = this;
    // 3. If module.[[Status]] is either EVALUATING-ASYNC or EVALUATED, then
    if (m_status == Status::EvaluatingAsync || m_status == Status::Evaluated) {
        // 3.a. If module.[[CycleRoot]] is not EMPTY, then
        if (CyclicModuleRecord* root = m_cycleRoot.get()) {
            // 3.a.i. Set module to module.[[CycleRoot]].
            module = root;
        // 3.b. Else,
        } else {
            // 3.b.i. Assert: module.[[Status]] is EVALUATED and module.[[EvaluationError]] is a throw completion.
            ASSERT(m_status == Status::Evaluated);
            ASSERT(m_evaluationError);
        }
    }
    // 4. If module.[[TopLevelCapability]] is not EMPTY, then
    if (JSPromise* promise = module->topLevelCapability()) {
        // 4.a. Return module.[[TopLevelCapability]].[[Promise]].
        RELEASE_AND_RETURN(scope, promise);
    }
    // 5. Let stack be a new empty List.
    Vector<AbstractModuleRecord*, 8> stack;
    // 6. Let capability be ! NewPromiseCapability(%Promise%).
    JSPromise* capability = JSPromise::create(vm, globalObject->promiseStructure());
    // 7. Set module.[[TopLevelCapability]] to capability.
    module->setTopLevelCapability(vm, capability);
    // 8. Let result be Completion(InnerModuleEvaluation(module, stack, 0)).
    module->innerModuleEvaluation(globalObject, stack, 0);
    // 9. If result is an abrupt completion, then
    if (Exception* exception = scope.exception()) {
        JSModuleLoader::attachErrorInfo(globalObject, exception, module, moduleKey(), moduleType(), JSModuleLoader::ModuleFailure::Kind::Evaluation);
        // 9.a. For each Cyclic Module Record m of stack, do
        for (AbstractModuleRecord* abstractRecord : stack) {
            // 9.a.i. Assert: m.[[Status]] is EVALUATING.
            auto* cyclic = uncheckedDowncast<CyclicModuleRecord>(abstractRecord);
            ASSERT(cyclic->status() == Status::Evaluating);
            // 9.a.ii. Set m.[[Status]] to EVALUATED.
            cyclic->setStatus(Status::Evaluated);
            // 9.a.iii. Set m.[[EvaluationError]] to result.
            cyclic->setEvaluationError(vm, exception->value());
        }
        // 9.b. Assert: module.[[Status]] is EVALUATED.
        ASSERT(module->status() == Status::Evaluated);
        // 9.c. Assert: module.[[EvaluationError]] and result are the same Completion Record.
        ASSERT(module->evaluationError() == exception->value());
        // 9.d. Perform ! Call(capability.[[Reject]], undefined, « result.[[Value]] »).
        capability->rejectWithCaughtException(globalObject, scope);
    // 10. Else,
    } else {
        // 10.a. Assert: module.[[Status]] is either EVALUATING-ASYNC or EVALUATED.
        ASSERT(module->status() == Status::EvaluatingAsync || module->status() == Status::Evaluated);
        // 10.b. Assert: module.[[EvaluationError]] is EMPTY.
        ASSERT(module->evaluationError() == nullptr);
        // 10.c. If module.[[Status]] is EVALUATED, then
        if (module->status() == Status::Evaluated) {
            // 10.c.i. Assert: module.[[AsyncEvaluationOrder]] is either UNSET or DONE.
            ASSERT(module->asyncEvaluationOrder().isUnset() || module->asyncEvaluationOrder().isDone());
            // 10.c.ii. NOTE: module.[[AsyncEvaluationOrder]] is DONE if and only if module had already been evaluated and that evaluation was asynchronous.
            // 10.c.iii. Perform ! Call(capability.[[Resolve]], undefined, « undefined »).
            capability->fulfill(vm, globalObject, jsUndefined());
        }
        // 10.d. Assert: stack is empty.
        ASSERT(stack.isEmpty());
    }
    // 11. Return capability.[[Promise]].
    RELEASE_AND_RETURN(scope, capability);
}

void CyclicModuleRecord::execute(JSGlobalObject* globalObject, JSPromise* capability)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

#if ENABLE(WEBASSEMBLY)
    if (auto* wasmModule = dynamicDowncast<WebAssemblyModuleRecord>(this)) {
        wasmModule->initializeImports(globalObject, nullptr, Wasm::CreationMode::FromModuleLoader);
        RETURN_IF_EXCEPTION(scope, void());
        wasmModule->initializeExports(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
        JSValue result = wasmModule->evaluate(globalObject);
        if (capability) {
            if (Exception* exception = scope.exception()) {
                JSModuleLoader::attachErrorInfo(globalObject, exception, wasmModule, wasmModule->moduleKey(), ScriptFetchParameters::WebAssembly, JSModuleLoader::ModuleFailure::Kind::Evaluation);
                capability->rejectWithCaughtException(globalObject, scope);
            } else
                capability->fulfill(vm, globalObject, result);
            return;
        }
        RELEASE_AND_RETURN(scope, void());
    }
#endif
    RELEASE_AND_RETURN(scope, uncheckedDowncast<JSModuleRecord>(this)->execute(globalObject, capability));
}

void CyclicModuleRecord::executeAsync(JSGlobalObject* globalObject)
{
    // ExecuteAsyncModule(module)
    // https://tc39.es/ecma262/#sec-execute-async-module

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Assert: module.[[Status]] is either EVALUATING or EVALUATING-ASYNC.
    ASSERT(status() == Status::Evaluating || status() == Status::EvaluatingAsync);
    // 2. Assert: module.[[HasTLA]] is true.
    ASSERT(hasTLA());
    // 3. Let capability be ! NewPromiseCapability(%Promise%).
    JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
    // 4. Let fulfilledClosure be a new Abstract Closure with no parameters that captures module and performs the following steps when called:
    //     4.a. Perform AsyncModuleExecutionFulfilled(module).
    //     4.b. Return NormalCompletion(undefined).
    // 5. Let onFulfilled be CreateBuiltinFunction(fulfilledClosure, 0, "", « »).
    // Handled in JSMicrotask.cpp.
    // 6. Let rejectedClosure be a new Abstract Closure with parameters (error) that captures module and performs the following steps when called:
    //     6.a. Perform AsyncModuleExecutionRejected(module, error).
    //     6.b. Return NormalCompletion(undefined).
    // 7. Let onRejected be CreateBuiltinFunction(rejectedClosure, 0, "", « »).
    // Also handled in JSMicrotask.cpp.
    // 8. Perform PerformPromiseThen(capability.[[Promise]], onFulfilled, onRejected).
    promise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::AsyncModuleExecutionDone, jsUndefined(), this);
    // 9. Perform ! module.ExecuteModule(capability).
    execute(globalObject, promise);
    RETURN_IF_EXCEPTION(scope, void());
    // 10. Return UNUSED.
    RELEASE_AND_RETURN(scope, void());
}

static void gatherAvailableAncestors(CyclicModuleRecord* module, Vector<CyclicModuleRecord*, 8>& execList)
{
    // GatherAvailableAncestors(module, execList)
    // https://tc39.es/ecma262/#sec-gather-available-ancestors

    // 1. For each Cyclic Module Record m of module.[[AsyncParentModules]], do
    for (const WriteBarrier<AbstractModuleRecord>& barrier : module->asyncParentModules()) {
        auto* m = uncheckedDowncast<CyclicModuleRecord>(barrier.get());
        // 1.a. If execList does not contain m and m.[[CycleRoot]].[[EvaluationError]] is empty, then
        // (Probable spec bug (https://github.com/tc39/ecma262/issues/3766). We need an additional check here that m.[[CycleRoot]] isn't empty.)
        ASSERT_IMPLIES(!m->cycleRoot(), m->evaluationError());
        if (CyclicModuleRecord* root = m->cycleRoot(); root && root->evaluationError() == nullptr && !execList.contains(m)) {
            // 1.a.i. Assert: m.[[Status]] is EVALUATING-ASYNC.
            ASSERT(m->status() == CyclicModuleRecord::Status::EvaluatingAsync);
            // 1.a.ii. Assert: m.[[EvaluationError]] is EMPTY.
            ASSERT(m->evaluationError() == nullptr);
            // 1.a.iii. Assert: m.[[AsyncEvaluationOrder]] is an integer.
            ASSERT(m->asyncEvaluationOrder().hasOrder());
            // 1.a.iv. Assert: m.[[PendingAsyncDependencies]] > 0.
            ASSERT(m->pendingAsyncDependencies() > 0);
            // 1.a.v. Set m.[[PendingAsyncDependencies]] to m.[[PendingAsyncDependencies]] - 1.
            int newDependencies = m->pendingAsyncDependencies().value() - 1;
            m->setPendingAsyncDependencies(newDependencies);
            // 1.a.vi. If m.[[PendingAsyncDependencies]] = 0, then
            if (!newDependencies) {
                // 1.a.vi.1. Append m to execList.
                execList.append(m);
                // 1.a.vi.2. If m.[[HasTLA]] is false, perform GatherAvailableAncestors(m, execList).
                if (!m->hasTLA())
                    gatherAvailableAncestors(m, execList);
            }
        }
    }
    // 2. Return UNUSED.
}

void CyclicModuleRecord::asyncExecutionRejected(JSGlobalObject* globalObject, JSValue error)
{
    // AsyncModuleExecutionRejected(module, error)
    // https://tc39.es/ecma262/#sec-async-module-execution-rejected

    VM& vm = globalObject->vm();

    // 1. If module.[[Status]] is EVALUATED, then
    if (status() == CyclicModuleRecord::Status::Evaluated) {
        // 1.a. Assert: module.[[EvaluationError]] is not EMPTY.
        ASSERT(evaluationError() != nullptr);
        // 1.b. Return UNUSED.
        return;
    }
    // 2. Assert: module.[[Status]] is EVALUATING-ASYNC.
    ASSERT(status() == CyclicModuleRecord::Status::EvaluatingAsync);
    // 3. Assert: module.[[AsyncEvaluationOrder]] is an integer.
    ASSERT(asyncEvaluationOrder().hasOrder());
    // 4. Assert: module.[[EvaluationError]] is EMPTY.
    ASSERT(evaluationError() == nullptr);
    // 5. Set module.[[EvaluationError]] to ThrowCompletion(error).
    setEvaluationError(vm, error);
    // 6. Set module.[[Status]] to EVALUATED.
    setStatus(CyclicModuleRecord::Status::Evaluated);
    // 7. Set module.[[AsyncEvaluationOrder]] to DONE.
    setAsyncEvaluationOrder(AbstractModuleRecord::AsyncEvaluationOrder::done());
    // 8. NOTE: module.[[AsyncEvaluationOrder]] is set to DONE for symmetry with AsyncModuleExecutionFulfilled. In InnerModuleEvaluation, the value of a module's [[AsyncEvaluationOrder]] internal slot is unused when its [[EvaluationError]] internal slot is not EMPTY.
    // 9. If module.[[TopLevelCapability]] is not EMPTY, then
    if (auto* topLevel = topLevelCapability()) {
        // 9.a. Assert: module.[[CycleRoot]] and module are the same Module Record.
        ASSERT(cycleRoot() == this);
        // 9.b. Perform ! Call(module.[[TopLevelCapability]].[[Reject]], undefined, « error »).
        topLevel->reject(vm, globalObject, error);
    }
    // 10. For each Cyclic Module Record m of module.[[AsyncParentModules]], do
    for (const WriteBarrier<AbstractModuleRecord>& m : asyncParentModules()) {
        // 10.a. Perform AsyncModuleExecutionRejected(m, error).
        uncheckedDowncast<CyclicModuleRecord>(m.get())->asyncExecutionRejected(globalObject, error);
    }
    // 11. Return UNUSED.
}

void CyclicModuleRecord::asyncExecutionFulfilled(JSGlobalObject* globalObject)
{
    // AsyncModuleExecutionFulfilled(module)
    // https://tc39.es/ecma262/#sec-async-module-execution-fulfilled

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. If module.[[Status]] is EVALUATED, then
    if (status() == CyclicModuleRecord::Status::Evaluated) {
        // 1.a. Assert: module.[[EvaluationError]] is not EMPTY.
        ASSERT(evaluationError() != nullptr);
        // 1.b. Return UNUSED.
        RELEASE_AND_RETURN(scope, void());
    }
    // 2. Assert: module.[[Status]] is EVALUATING-ASYNC.
    ASSERT(status() == CyclicModuleRecord::Status::EvaluatingAsync);
    // 3. Assert: module.[[AsyncEvaluationOrder]] is an integer.
    ASSERT(asyncEvaluationOrder().hasOrder());
    // 4. Assert: module.[[EvaluationError]] is EMPTY.
    ASSERT(evaluationError() == nullptr);
    // 5. Set module.[[AsyncEvaluationOrder]] to DONE.
    setAsyncEvaluationOrder(AbstractModuleRecord::AsyncEvaluationOrder::done());
    // 6. Set module.[[Status]] to EVALUATED.
    setStatus(CyclicModuleRecord::Status::Evaluated);
    // 7. If module.[[TopLevelCapability]] is not EMPTY, then
    if (auto* capability = topLevelCapability()) {
        // 7.a. Assert: module.[[CycleRoot]] and module are the same Module Record.
        ASSERT(cycleRoot() == this);
        // 7.b. Perform ! Call(module.[[TopLevelCapability]].[[Resolve]], undefined, « undefined »).
        capability->fulfill(vm, globalObject, jsUndefined());
    }
    // 8. Let execList be a new empty List.
    // (Note: it's safe to use a Vector instead of a MarkedArgumentsBuffer here because all the contents are accessed through WriteBarriers starting at `this`.)
    Vector<CyclicModuleRecord*, 8> execList;
    // 9. Perform GatherAvailableAncestors(module, execList).
    gatherAvailableAncestors(this, execList);
    // 10. Assert: All elements of execList have their [[AsyncEvaluationOrder]] field set to an integer, [[PendingAsyncDependencies]] field set to 0, and [[EvaluationError]] field set to EMPTY.
#if ASSERT_ENABLED
    for (CyclicModuleRecord* element : execList) {
        ASSERT(element->asyncEvaluationOrder().hasOrder());
        ASSERT(element->pendingAsyncDependencies() && !*element->pendingAsyncDependencies());
        ASSERT(!element->evaluationError());
    }
#endif
    // 11. Let sortedExecList be a List whose elements are the elements of execList, sorted by their [[AsyncEvaluationOrder]] field in ascending order.
    std::ranges::sort(execList, [](CyclicModuleRecord* left, CyclicModuleRecord* right) {
        return left->asyncEvaluationOrder().order() < right->asyncEvaluationOrder().order();
    });
    // 12. For each Cyclic Module Record m of sortedExecList, do
    for (CyclicModuleRecord* m : execList) {
        // 12.a. If m.[[Status]] is EVALUATED, then
        if (m->status() == CyclicModuleRecord::Status::Evaluated) {
            // 12.a.i. Assert: m.[[EvaluationError]] is not EMPTY.
            ASSERT(m->evaluationError() != nullptr);
        // 12.b. Else if m.[[HasTLA]] is true, then
        } else if (m->hasTLA()) {
            // 12.b.i. Perform ExecuteAsyncModule(m).
            m->executeAsync(globalObject);
            if (Exception* exception = scope.exception()) {
                JSValue error = exception->value();
                TRY_CLEAR_EXCEPTION(scope, void());
                m->asyncExecutionRejected(globalObject, error);
            }
        // 12.c. Else,
        } else {
            // 12.c.i. Let result be m.ExecuteModule().
            m->execute(globalObject);
            // 12.c.ii. If result is an abrupt completion, then
            if (Exception* exception = scope.exception()) {
                JSValue error = exception->value();
                TRY_CLEAR_EXCEPTION(scope, void());
                // 12.c.ii.1. Perform AsyncModuleExecutionRejected(m, result.[[Value]]).
                m->asyncExecutionRejected(globalObject, error);
            // 12.c.iii. Else,
            } else {
                // 12.c.iii.1. Set m.[[AsyncEvaluationOrder]] to DONE.
                m->setAsyncEvaluationOrder(AbstractModuleRecord::AsyncEvaluationOrder::done());
                // 12.c.iii.2. Set m.[[Status]] to EVALUATED.
                m->setStatus(CyclicModuleRecord::Status::Evaluated);
                // 12.c.iii.3. If m.[[TopLevelCapability]] is not EMPTY, then
                if (auto* capability = m->topLevelCapability()) {
                    // 12.c.iii.3.a. Assert: m.[[CycleRoot]] and m are the same Module Record.
                    ASSERT(m->cycleRoot() == m);
                    // 12.c.iii.3.b. Perform ! Call(m.[[TopLevelCapability]].[[Resolve]], undefined, « undefined »).
                    capability->fulfill(vm, globalObject, jsUndefined());
                }
            }
        }
    }
    // 13. Return UNUSED.
}

} // namespace JSC
