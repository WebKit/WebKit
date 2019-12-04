/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "WebAssemblyModuleRecord.h"

#if ENABLE(WEBASSEMBLY)

#include "Error.h"
#include "JSCInlines.h"
#include "JSLexicalEnvironment.h"
#include "JSModuleEnvironment.h"
#include "JSWebAssemblyGlobal.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyLinkError.h"
#include "JSWebAssemblyModule.h"
#include "ProtoCallFrame.h"
#include "WasmSignatureInlines.h"
#include "WebAssemblyFunction.h"
#include <limits>

namespace JSC {

const ClassInfo WebAssemblyModuleRecord::s_info = { "WebAssemblyModuleRecord", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblyModuleRecord) };

Structure* WebAssemblyModuleRecord::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

WebAssemblyModuleRecord* WebAssemblyModuleRecord::create(JSGlobalObject* globalObject, VM& vm, Structure* structure, const Identifier& moduleKey, const Wasm::ModuleInformation& moduleInformation)
{
    WebAssemblyModuleRecord* instance = new (NotNull, allocateCell<WebAssemblyModuleRecord>(vm.heap)) WebAssemblyModuleRecord(vm, structure, moduleKey);
    instance->finishCreation(globalObject, vm, moduleInformation);
    return instance;
}

WebAssemblyModuleRecord::WebAssemblyModuleRecord(VM& vm, Structure* structure, const Identifier& moduleKey)
    : Base(vm, structure, moduleKey)
{
}

void WebAssemblyModuleRecord::destroy(JSCell* cell)
{
    WebAssemblyModuleRecord* thisObject = static_cast<WebAssemblyModuleRecord*>(cell);
    thisObject->WebAssemblyModuleRecord::~WebAssemblyModuleRecord();
}

void WebAssemblyModuleRecord::finishCreation(JSGlobalObject* globalObject, VM& vm, const Wasm::ModuleInformation& moduleInformation)
{
    Base::finishCreation(globalObject, vm);
    ASSERT(inherits(vm, info()));
    for (const auto& exp : moduleInformation.exports) {
        Identifier field = Identifier::fromString(vm, String::fromUTF8(exp.field));
        addExportEntry(ExportEntry::createLocal(field, field));
    }
}

void WebAssemblyModuleRecord::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    WebAssemblyModuleRecord* thisObject = jsCast<WebAssemblyModuleRecord*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_instance);
    visitor.append(thisObject->m_startFunction);
}

void WebAssemblyModuleRecord::prepareLink(VM& vm, JSWebAssemblyInstance* instance)
{
    RELEASE_ASSERT(!m_instance);
    m_instance.set(vm, this, instance);
}

void WebAssemblyModuleRecord::link(JSGlobalObject* globalObject, JSValue, JSObject* importObject, Wasm::CreationMode creationMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(scope);

    RELEASE_ASSERT(m_instance);

    Wasm::CodeBlock* codeBlock = m_instance->instance().codeBlock();
    JSWebAssemblyModule* module = m_instance->module();
    const Wasm::ModuleInformation& moduleInformation = module->moduleInformation();

    auto exception = [&] (JSObject* error) {
        throwException(globalObject, scope, error);
    };

    auto importFailMessage = [&] (const Wasm::Import& import, const char* before, const char* after) {
        return makeString(before, " ", String::fromUTF8(import.module), ":", String::fromUTF8(import.field), " ", after);
    };

    for (const auto& import : moduleInformation.imports) {
        // Validation and linking other than Wasm::ExternalKind::Function is already done in JSWebAssemblyInstance.
        // Eventually we will move all the linking code in JSWebAssemblyInstance here and remove this switch statement.
        switch (import.kind) {
        case Wasm::ExternalKind::Function:
        case Wasm::ExternalKind::Global:
        case Wasm::ExternalKind::Table:
            break;
        case Wasm::ExternalKind::Memory:
            continue;
        }

        Identifier moduleName = Identifier::fromString(vm, String::fromUTF8(import.module));
        Identifier fieldName = Identifier::fromString(vm, String::fromUTF8(import.field));
        JSValue value;
        if (creationMode == Wasm::CreationMode::FromJS) {
            // 1. Let o be the resultant value of performing Get(importObject, i.module_name).
            JSValue importModuleValue = importObject->get(globalObject, moduleName);
            RETURN_IF_EXCEPTION(scope, void());
            // 2. If Type(o) is not Object, throw a TypeError.
            if (!importModuleValue.isObject())
                return exception(createTypeError(globalObject, importFailMessage(import, "import", "must be an object"), defaultSourceAppender, runtimeTypeForValue(vm, importModuleValue)));

            // 3. Let v be the value of performing Get(o, i.item_name)
            JSObject* object = jsCast<JSObject*>(importModuleValue);
            value = object->get(globalObject, fieldName);
            RETURN_IF_EXCEPTION(scope, void());
        } else {
            AbstractModuleRecord* importedModule = hostResolveImportedModule(globalObject, moduleName);
            RETURN_IF_EXCEPTION(scope, void());
            Resolution resolution = importedModule->resolveExport(globalObject, fieldName);
            RETURN_IF_EXCEPTION(scope, void());
            switch (resolution.type) {
            case Resolution::Type::NotFound:
                throwSyntaxError(globalObject, scope, makeString("Importing binding name '", String(fieldName.impl()), "' is not found."));
                return;

            case Resolution::Type::Ambiguous:
                throwSyntaxError(globalObject, scope, makeString("Importing binding name '", String(fieldName.impl()), "' cannot be resolved due to ambiguous multiple bindings."));
                return;

            case Resolution::Type::Error:
                throwSyntaxError(globalObject, scope, makeString("Importing binding name 'default' cannot be resolved by star export entries."));
                return;

            case Resolution::Type::Resolved:
                break;
            }

            AbstractModuleRecord* importedRecord = resolution.moduleRecord;
            JSModuleEnvironment* importedEnvironment = importedRecord->moduleEnvironmentMayBeNull();
            // It means that target module is not linked yet. In wasm loading, we allow this since we do not solve cyclic resolution as if JS's bindings.
            // At that time, error occurs since |value| is an empty, and later |value| becomes an undefined.
            // https://github.com/WebAssembly/esm-integration/tree/master/proposals/esm-integration#js---wasm-cycle-where-js-is-higher-in-the-module-graph
            if (importedEnvironment) {
                SymbolTable* symbolTable = importedEnvironment->symbolTable();
                ConcurrentJSLocker locker(symbolTable->m_lock);
                auto iter = symbolTable->find(locker, resolution.localName.impl());
                ASSERT(iter != symbolTable->end(locker));
                SymbolTableEntry& entry = iter->value;
                ASSERT(!entry.isNull());
                ASSERT(importedEnvironment->isValidScopeOffset(entry.scopeOffset()));

                // Snapshotting a value.
                value = importedEnvironment->variableAt(entry.scopeOffset()).get();
            }
        }
        if (!value)
            value = jsUndefined();

        switch (import.kind) {
        case Wasm::ExternalKind::Function: {
            // 4. If i is a function import:
            // i. If IsCallable(v) is false, throw a WebAssembly.LinkError.
            if (!value.isFunction(vm))
                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "import function", "must be callable")));

            Wasm::Instance* calleeInstance = nullptr;
            WasmToWasmImportableFunction::LoadLocation entrypointLoadLocation = nullptr;
            JSObject* function = jsCast<JSObject*>(value);

            // ii. If v is an Exported Function Exotic Object:
            WebAssemblyFunction* wasmFunction;
            WebAssemblyWrapperFunction* wasmWrapperFunction;
            if (isWebAssemblyHostFunction(vm, function, wasmFunction, wasmWrapperFunction)) {
                // a. If the signature of v does not match the signature of i, throw a WebAssembly.LinkError.
                Wasm::SignatureIndex importedSignatureIndex;
                if (wasmFunction) {
                    importedSignatureIndex = wasmFunction->signatureIndex();
                    calleeInstance = &wasmFunction->instance()->instance();
                    entrypointLoadLocation = wasmFunction->entrypointLoadLocation();
                } else {
                    importedSignatureIndex = wasmWrapperFunction->signatureIndex();
                    // b. Let closure be v.[[Closure]].
                    function = wasmWrapperFunction->function();
                }
                Wasm::SignatureIndex expectedSignatureIndex = moduleInformation.importFunctionSignatureIndices[import.kindIndex];
                if (importedSignatureIndex != expectedSignatureIndex)
                    return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported function", "signature doesn't match the provided WebAssembly function's signature")));
            }
            // iii. Otherwise:
            // a. Let closure be a new host function of the given signature which calls v by coercing WebAssembly arguments to JavaScript arguments via ToJSValue and returns the result, if any, by coercing via ToWebAssemblyValue.
            // Note: done as part of Plan compilation.
            // iv. Append v to funcs.
            // Note: adding the JSCell to the instance list fulfills closure requirements b. above (the WebAssembly.Instance wil be kept alive) and v. below (the JSFunction).

            auto* info = m_instance->instance().importFunctionInfo(import.kindIndex);
            info->targetInstance = calleeInstance;
            info->wasmEntrypointLoadLocation = entrypointLoadLocation;
            m_instance->instance().importFunction<WriteBarrier<JSObject>>(import.kindIndex)->set(vm, m_instance.get(), function);
            break;
        }

        case Wasm::ExternalKind::Global: {
            // 5. If i is a global import:
            const Wasm::GlobalInformation& global = moduleInformation.globals[import.kindIndex];
            if (global.mutability == Wasm::GlobalInformation::Immutable) {
                if (value.inherits<JSWebAssemblyGlobal>(vm)) {
                    JSWebAssemblyGlobal* globalValue = jsCast<JSWebAssemblyGlobal*>(value);
                    if (globalValue->global()->type() != global.type)
                        return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "must be a same type")));
                    if (globalValue->global()->mutability() != Wasm::GlobalInformation::Immutable)
                        return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "must be a same mutability")));
                    switch (moduleInformation.globals[import.kindIndex].type) {
                    case Wasm::Funcref:
                        value = globalValue->global()->get();
                        if (!isWebAssemblyHostFunction(vm, value) && !value.isNull())
                            return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "must be a wasm exported function or null")));
                        m_instance->instance().setGlobal(import.kindIndex, value);
                        break;
                    case Wasm::Anyref:
                        value = globalValue->global()->get();
                        m_instance->instance().setGlobal(import.kindIndex, value);
                        break;
                    case Wasm::I32:
                    case Wasm::I64:
                    case Wasm::F32:
                    case Wasm::F64:
                        m_instance->instance().setGlobal(import.kindIndex, globalValue->global()->getPrimitive());
                        break;
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                    }
                } else {
                    // ii. If the global_type of i is i64 or Type(v) is not Number, throw a WebAssembly.LinkError.
                    if (moduleInformation.globals[import.kindIndex].type == Wasm::I64)
                        return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "cannot be an i64")));
                    if (!isSubtype(moduleInformation.globals[import.kindIndex].type, Wasm::Anyref) && !value.isNumber())
                        return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "must be a number")));

                    // iii. Append ToWebAssemblyValue(v) to imports.
                    switch (moduleInformation.globals[import.kindIndex].type) {
                    case Wasm::Funcref:
                        if (!isWebAssemblyHostFunction(vm, value) && !value.isNull())
                            return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "must be a wasm exported function or null")));
                        m_instance->instance().setGlobal(import.kindIndex, value);
                        break;
                    case Wasm::Anyref:
                        m_instance->instance().setGlobal(import.kindIndex, value);
                        break;
                    case Wasm::I32:
                        m_instance->instance().setGlobal(import.kindIndex, value.toInt32(globalObject));
                        break;
                    case Wasm::F32:
                        m_instance->instance().setGlobal(import.kindIndex, bitwise_cast<uint32_t>(value.toFloat(globalObject)));
                        break;
                    case Wasm::F64:
                        m_instance->instance().setGlobal(import.kindIndex, bitwise_cast<uint64_t>(value.asNumber()));
                        break;
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                    }
                }
            } else {
                if (!value.inherits<JSWebAssemblyGlobal>(vm))
                    return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "must be a WebAssembly.Global object since it is mutable")));
                JSWebAssemblyGlobal* globalValue = jsCast<JSWebAssemblyGlobal*>(value);
                if (globalValue->global()->type() != global.type)
                    return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "must be a same type")));
                if (globalValue->global()->mutability() != global.mutability)
                    return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "must be a same mutability")));
                m_instance->linkGlobal(vm, import.kindIndex, globalValue);
            }
            scope.assertNoException();
            break;
        }

        case Wasm::ExternalKind::Table: {
            // 7. Otherwise (i is a table import):
            JSWebAssemblyTable* table = jsDynamicCast<JSWebAssemblyTable*>(vm, value);
            // i. If v is not a WebAssembly.Table object, throw a WebAssembly.LinkError.
            if (!table)
                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "Table import", "is not an instance of WebAssembly.Table")));

            uint32_t expectedInitial = moduleInformation.tables[import.kindIndex].initial();
            uint32_t actualInitial = table->length();
            if (actualInitial < expectedInitial)
                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "Table import", "provided an 'initial' that is too small")));

            if (Optional<uint32_t> expectedMaximum = moduleInformation.tables[import.kindIndex].maximum()) {
                Optional<uint32_t> actualMaximum = table->maximum();
                if (!actualMaximum)
                    return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "Table import", "does not have a 'maximum' but the module requires that it does")));
                if (*actualMaximum > *expectedMaximum)
                    return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "Imported Table", "'maximum' is larger than the module's expected 'maximum'")));
            }

            auto expectedType = moduleInformation.tables[import.kindIndex].type();
            auto actualType = table->table()->type();
            if (expectedType != actualType)
                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "Table import", "provided a 'type' that is wrong")));

            // ii. Append v to tables.
            // iii. Append v.[[Table]] to imports.
            m_instance->setTable(vm, import.kindIndex, table);
            RETURN_IF_EXCEPTION(scope, void());
            break;
        }

        case Wasm::ExternalKind::Memory:
            break;
        }
    }

    for (unsigned i = 0; i < moduleInformation.tableCount(); ++i) {
        if (moduleInformation.tables[i].isImport()) {
            // We should either have a Table import or we should have thrown an exception.
            RELEASE_ASSERT(m_instance->table(i));
        }

        if (!m_instance->table(i)) {
            RELEASE_ASSERT(!moduleInformation.tables[i].isImport());
            // We create a Table when it's a Table definition.
            RefPtr<Wasm::Table> wasmTable = Wasm::Table::tryCreate(moduleInformation.tables[i].initial(), moduleInformation.tables[i].maximum(), moduleInformation.tables[i].type());
            if (!wasmTable)
                return exception(createJSWebAssemblyLinkError(globalObject, vm, "couldn't create Table"));
            JSWebAssemblyTable* table = JSWebAssemblyTable::tryCreate(globalObject, vm, globalObject->webAssemblyTableStructure(), wasmTable.releaseNonNull());
            // We should always be able to allocate a JSWebAssemblyTable we've defined.
            // If it's defined to be too large, we should have thrown a validation error.
            scope.assertNoException();
            ASSERT(table);
            m_instance->setTable(vm, i, table);
            RETURN_IF_EXCEPTION(scope, void());
        }
    }

    unsigned functionImportCount = codeBlock->functionImportCount();
    auto makeFunctionWrapper = [&] (const String& field, uint32_t index) -> JSValue {
        // If we already made a wrapper, do not make a new one.
        JSValue wrapper = m_instance->instance().getFunctionWrapper(index);

        if (!wrapper.isNull())
            return wrapper;

        // 1. If e is a closure c:
        //   i. If there is an Exported Function Exotic Object func in funcs whose func.[[Closure]] equals c, then return func.
        //   ii. (Note: At most one wrapper is created for any closure, so func is unique, even if there are multiple occurrances in the list. Moreover, if the item was an import that is already an Exported Function Exotic Object, then the original function object will be found. For imports that are regular JS functions, a new wrapper will be created.)
        if (index < functionImportCount) {
            JSObject* functionImport = m_instance->instance().importFunction<WriteBarrier<JSObject>>(index)->get();
            if (isWebAssemblyHostFunction(vm, functionImport))
                wrapper = functionImport;
            else {
                Wasm::SignatureIndex signatureIndex = module->signatureIndexFromFunctionIndexSpace(index);
                wrapper = WebAssemblyWrapperFunction::create(vm, globalObject, globalObject->webAssemblyWrapperFunctionStructure(), functionImport, index, m_instance.get(), signatureIndex);
            }
        } else {
            //   iii. Otherwise:
            //     a. Let func be an Exported Function Exotic Object created from c.
            //     b. Append func to funcs.
            //     c. Return func.
            Wasm::Callee& embedderEntrypointCallee = codeBlock->embedderEntrypointCalleeFromFunctionIndexSpace(index);
            Wasm::WasmToWasmImportableFunction::LoadLocation entrypointLoadLocation = codeBlock->entrypointLoadLocationFromFunctionIndexSpace(index);
            Wasm::SignatureIndex signatureIndex = module->signatureIndexFromFunctionIndexSpace(index);
            const Wasm::Signature& signature = Wasm::SignatureInformation::get(signatureIndex);
            WebAssemblyFunction* function = WebAssemblyFunction::create(vm, globalObject, globalObject->webAssemblyFunctionStructure(), signature.argumentCount(), field, m_instance.get(), embedderEntrypointCallee, entrypointLoadLocation, signatureIndex);
            wrapper = function;
        }

        ASSERT(wrapper.isFunction(vm));
        m_instance->instance().setFunctionWrapper(index, wrapper);

        return wrapper;
    };

    for (auto index : moduleInformation.referencedFunctions())
        makeFunctionWrapper("Referenced function", index);

    // Globals
    {
        for (size_t globalIndex = moduleInformation.firstInternalGlobal; globalIndex < moduleInformation.globals.size(); ++globalIndex) {
            const auto& global = moduleInformation.globals[globalIndex];
            ASSERT(global.initializationType != Wasm::GlobalInformation::IsImport);
            uint64_t initialBits = 0;
            if (global.initializationType == Wasm::GlobalInformation::FromGlobalImport) {
                ASSERT(global.initialBitsOrImportNumber < moduleInformation.firstInternalGlobal);
                initialBits = m_instance->instance().loadI64Global(global.initialBitsOrImportNumber);
            } else if (global.initializationType == Wasm::GlobalInformation::FromRefFunc) {
                ASSERT(global.initialBitsOrImportNumber < moduleInformation.functionIndexSpaceSize());
                ASSERT(makeFunctionWrapper("Global init expr", global.initialBitsOrImportNumber).isFunction(vm));
                initialBits = JSValue::encode(makeFunctionWrapper("Global init expr", global.initialBitsOrImportNumber));
            } else
                initialBits = global.initialBitsOrImportNumber;

            switch (global.bindingMode) {
            case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance: {
                m_instance->instance().setGlobal(globalIndex, initialBits);
                break;
            }
            case Wasm::GlobalInformation::BindingMode::Portable: {
                ASSERT(global.mutability == Wasm::GlobalInformation::Mutability::Mutable);
                Ref<Wasm::Global> globalRef = Wasm::Global::create(global.type, Wasm::GlobalInformation::Mutability::Mutable, initialBits);
                JSWebAssemblyGlobal* globalValue = JSWebAssemblyGlobal::tryCreate(globalObject, vm, globalObject->webAssemblyGlobalStructure(), WTFMove(globalRef));
                scope.assertNoException();
                m_instance->linkGlobal(vm, globalIndex, globalValue);
                keepAlive(bitwise_cast<void*>(initialBits)); // Ensure this is kept alive while creating JSWebAssemblyGlobal.
                break;
            }
            }
        }
    }

    SymbolTable* exportSymbolTable = module->exportSymbolTable();

    // Let exports be a list of (string, JS value) pairs that is mapped from each external value e in instance.exports as follows:
    JSModuleEnvironment* moduleEnvironment = JSModuleEnvironment::create(vm, globalObject, nullptr, exportSymbolTable, JSValue(), this);
    for (const auto& exp : moduleInformation.exports) {
        JSValue exportedValue;
        switch (exp.kind) {
        case Wasm::ExternalKind::Function: {
            exportedValue = makeFunctionWrapper(String::fromUTF8(exp.field), exp.kindIndex);
            ASSERT(exportedValue.isFunction(vm));
            ASSERT(makeFunctionWrapper(String::fromUTF8(exp.field), exp.kindIndex) == exportedValue);
            break;
        }
        case Wasm::ExternalKind::Table: {
            // This should be guaranteed by module verification.
            RELEASE_ASSERT(m_instance->table(exp.kindIndex));
            exportedValue = m_instance->table(exp.kindIndex);
            break;
        }
        case Wasm::ExternalKind::Memory: {
            ASSERT(exp.kindIndex == 0);

            exportedValue = m_instance->memory();
            break;
        }
        case Wasm::ExternalKind::Global: {
            const Wasm::GlobalInformation& global = moduleInformation.globals[exp.kindIndex];
            switch (global.type) {
            case Wasm::Anyref:
            case Wasm::Funcref:
            case Wasm::I32:
            case Wasm::I64:
            case Wasm::F32:
            case Wasm::F64: {
                // If global is immutable, we are not creating a binding internally.
                // But we need to create a binding just to export it. This binding is not actually connected. But this is OK since it is immutable.
                if (global.bindingMode == Wasm::GlobalInformation::BindingMode::EmbeddedInInstance) {
                    uint64_t initialValue = m_instance->instance().loadI64Global(exp.kindIndex);
                    Ref<Wasm::Global> globalRef = Wasm::Global::create(global.type, global.mutability, initialValue);
                    exportedValue = JSWebAssemblyGlobal::tryCreate(globalObject, vm, globalObject->webAssemblyGlobalStructure(), WTFMove(globalRef));
                    scope.assertNoException();
                } else {
                    ASSERT(global.mutability == Wasm::GlobalInformation::Mutability::Mutable);
                    RefPtr<Wasm::Global> globalRef = m_instance->instance().getGlobalBinding(exp.kindIndex);
                    ASSERT(globalRef);
                    ASSERT(globalRef->type() == global.type);
                    ASSERT(globalRef->mutability() == global.mutability);
                    ASSERT(globalRef->mutability() == Wasm::GlobalInformation::Mutability::Mutable);
                    ASSERT(globalRef->owner<JSWebAssemblyGlobal>());
                    exportedValue = globalRef->owner<JSWebAssemblyGlobal>();
                }
                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            break;
        }
        }

        bool shouldThrowReadOnlyError = false;
        bool ignoreReadOnlyErrors = true;
        bool putResult = false;
        symbolTablePutTouchWatchpointSet(moduleEnvironment, globalObject, Identifier::fromString(vm, String::fromUTF8(exp.field)), exportedValue, shouldThrowReadOnlyError, ignoreReadOnlyErrors, putResult);
        scope.assertNoException();
        RELEASE_ASSERT(putResult);
    }

    bool hasStart = !!moduleInformation.startFunctionIndexSpace;
    if (hasStart) {
        auto startFunctionIndexSpace = moduleInformation.startFunctionIndexSpace.valueOr(0);
        Wasm::SignatureIndex signatureIndex = module->signatureIndexFromFunctionIndexSpace(startFunctionIndexSpace);
        const Wasm::Signature& signature = Wasm::SignatureInformation::get(signatureIndex);
        // The start function must not take any arguments or return anything. This is enforced by the parser.
        ASSERT(!signature.argumentCount());
        ASSERT(signature.returnsVoid());
        if (startFunctionIndexSpace < codeBlock->functionImportCount()) {
            JSObject* startFunction = m_instance->instance().importFunction<WriteBarrier<JSObject>>(startFunctionIndexSpace)->get();
            m_startFunction.set(vm, this, startFunction);
        } else {
            Wasm::Callee& embedderEntrypointCallee = codeBlock->embedderEntrypointCalleeFromFunctionIndexSpace(startFunctionIndexSpace);
            Wasm::WasmToWasmImportableFunction::LoadLocation entrypointLoadLocation = codeBlock->entrypointLoadLocationFromFunctionIndexSpace(startFunctionIndexSpace);
            WebAssemblyFunction* function = WebAssemblyFunction::create(vm, globalObject, globalObject->webAssemblyFunctionStructure(), signature.argumentCount(), "start", m_instance.get(), embedderEntrypointCallee, entrypointLoadLocation, signatureIndex);
            m_startFunction.set(vm, this, function);
        }
    }
    m_moduleEnvironment.set(vm, this, moduleEnvironment);
}

template <typename Scope, typename M, typename N, typename ...Args>
NEVER_INLINE static JSValue dataSegmentFail(JSGlobalObject* globalObject, VM& vm, Scope& scope, M memorySize, N segmentSize, N offset, Args... args)
{
    return throwException(globalObject, scope, createJSWebAssemblyLinkError(globalObject, vm, makeString("Invalid data segment initialization: segment of "_s, String::number(segmentSize), " bytes memory of "_s, String::number(memorySize), " bytes, at offset "_s, String::number(offset), args...)));
}

JSValue WebAssemblyModuleRecord::evaluate(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Wasm::Module& module = m_instance->instance().module();
    Wasm::CodeBlock* codeBlock = m_instance->instance().codeBlock();
    const Wasm::ModuleInformation& moduleInformation = module.moduleInformation();

    const Vector<Wasm::Segment::Ptr>& data = moduleInformation.data;
    
    Optional<JSValue> exception;

    auto forEachElement = [&] (auto fn) {
        for (const Wasm::Element& element : moduleInformation.elements) {
            // It should be a validation error to have any elements without a table.
            // Also, it could be that a table wasn't imported, or that the table
            // imported wasn't compatible. However, those should error out before
            // getting here.
            ASSERT(!!m_instance->table(element.tableIndex));

            if (!element.functionIndices.size())
                continue;

            uint32_t elementIndex = element.offset.isGlobalImport()
                ? static_cast<uint32_t>(m_instance->instance().loadI32Global(element.offset.globalImportIndex()))
                : element.offset.constValue();

            fn(element, element.tableIndex, elementIndex);

            if (exception)
                break;
        }
    };

    auto forEachSegment = [&] (auto fn) {
        uint8_t* memory = reinterpret_cast<uint8_t*>(m_instance->instance().cachedMemory());
        uint64_t sizeInBytes = m_instance->instance().cachedMemorySize();

        for (const Wasm::Segment::Ptr& segment : data) {
            uint32_t offset = segment->offset.isGlobalImport()
                ? static_cast<uint32_t>(m_instance->instance().loadI32Global(segment->offset.globalImportIndex()))
                : segment->offset.constValue();

            fn(memory, sizeInBytes, segment, offset);

            if (exception)
                break;
        }
    };

    // Validation of all element ranges comes before all Table and Memory initialization.
    forEachElement([&] (const Wasm::Element& element, uint32_t tableIndex, uint32_t elementIndex) {
        uint64_t lastWrittenIndex = static_cast<uint64_t>(elementIndex) + static_cast<uint64_t>(element.functionIndices.size()) - 1;
        if (UNLIKELY(lastWrittenIndex >= m_instance->table(tableIndex)->length()))
            exception = JSValue(throwException(globalObject, scope, createJSWebAssemblyLinkError(globalObject, vm, "Element is trying to set an out of bounds table index"_s)));
    });

    if (UNLIKELY(exception))
        return exception.value();

    // Validation of all segment ranges comes before all Table and Memory initialization.
    forEachSegment([&] (uint8_t*, uint64_t sizeInBytes, const Wasm::Segment::Ptr& segment, uint32_t offset) {
        if (UNLIKELY(sizeInBytes < segment->sizeInBytes))
            exception = dataSegmentFail(globalObject, vm, scope, sizeInBytes, segment->sizeInBytes, offset, ", segment is too big"_s);
        else if (UNLIKELY(offset > sizeInBytes - segment->sizeInBytes))
            exception = dataSegmentFail(globalObject, vm, scope, sizeInBytes, segment->sizeInBytes, offset, ", segment writes outside of memory"_s);
    });

    if (UNLIKELY(exception))
        return exception.value();

    forEachElement([&] (const Wasm::Element& element, uint32_t tableIndex, uint32_t elementIndex) {
        for (uint32_t i = 0; i < element.functionIndices.size(); ++i) {
            // FIXME: This essentially means we're exporting an import.
            // We need a story here. We need to create a WebAssemblyFunction
            // for the import.
            // https://bugs.webkit.org/show_bug.cgi?id=165510
            uint32_t functionIndex = element.functionIndices[i];
            Wasm::SignatureIndex signatureIndex = module.signatureIndexFromFunctionIndexSpace(functionIndex);
            if (functionIndex < codeBlock->functionImportCount()) {
                JSObject* functionImport = m_instance->instance().importFunction<WriteBarrier<JSObject>>(functionIndex)->get();
                if (isWebAssemblyHostFunction(vm, functionImport)) {
                    WebAssemblyFunction* wasmFunction = jsDynamicCast<WebAssemblyFunction*>(vm, functionImport);
                    // If we ever import a WebAssemblyWrapperFunction, we set the import as the unwrapped value.
                    // Because a WebAssemblyWrapperFunction can never wrap another WebAssemblyWrapperFunction,
                    // the only type this could be is WebAssemblyFunction.
                    RELEASE_ASSERT(wasmFunction);
                    m_instance->table(tableIndex)->set(elementIndex, wasmFunction);
                    ++elementIndex;
                    continue;
                }

                m_instance->table(tableIndex)->set(elementIndex,
                    WebAssemblyWrapperFunction::create(vm, globalObject, globalObject->webAssemblyWrapperFunctionStructure(), functionImport, functionIndex, m_instance.get(), signatureIndex));
                ++elementIndex;
                continue;
            }

            Wasm::Callee& embedderEntrypointCallee = codeBlock->embedderEntrypointCalleeFromFunctionIndexSpace(functionIndex);
            Wasm::WasmToWasmImportableFunction::LoadLocation entrypointLoadLocation = codeBlock->entrypointLoadLocationFromFunctionIndexSpace(functionIndex);
            const Wasm::Signature& signature = Wasm::SignatureInformation::get(signatureIndex);
            // FIXME: Say we export local function "foo" at function index 0.
            // What if we also set it to the table an Element w/ index 0.
            // Does (new Instance(...)).exports.foo === table.get(0)?
            // https://bugs.webkit.org/show_bug.cgi?id=165825
            WebAssemblyFunction* function = WebAssemblyFunction::create(
                vm, globalObject, globalObject->webAssemblyFunctionStructure(), signature.argumentCount(), String(), m_instance.get(), embedderEntrypointCallee, entrypointLoadLocation, signatureIndex);

            m_instance->table(tableIndex)->set(elementIndex, function);
            ++elementIndex;
        }
    });

    ASSERT(!exception);

    forEachSegment([&] (uint8_t* memory, uint64_t, const Wasm::Segment::Ptr& segment, uint32_t offset) {
        // Empty segments are valid, but only if memory isn't present, which would be undefined behavior in memcpy.
        if (segment->sizeInBytes) {
            RELEASE_ASSERT(memory);
            memcpy(memory + offset, &segment->byte(0), segment->sizeInBytes);
        }
    });

    ASSERT(!exception);

    if (JSObject* startFunction = m_startFunction.get()) {
        CallData callData;
        CallType callType = JSC::getCallData(vm, startFunction, callData);
        call(globalObject, startFunction, callType, callData, jsUndefined(), *vm.emptyList);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return jsUndefined();
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
