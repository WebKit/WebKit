/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "JSGlobalObjectInlines.h"
#include "JSModuleEnvironment.h"
#include "JSObjectInlines.h"
#include "JSWebAssemblyGlobal.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyLinkError.h"
#include "JSWebAssemblyModule.h"
#include "JSWebAssemblyTag.h"
#include "ObjectConstructor.h"
#include "WasmTypeDefinitionInlines.h"
#include "WebAssemblyFunction.h"

namespace JSC {

const ClassInfo WebAssemblyModuleRecord::s_info = { "WebAssemblyModuleRecord"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblyModuleRecord) };

Structure* WebAssemblyModuleRecord::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

WebAssemblyModuleRecord* WebAssemblyModuleRecord::create(JSGlobalObject* globalObject, VM& vm, Structure* structure, const Identifier& moduleKey, const Wasm::ModuleInformation& moduleInformation)
{
    WebAssemblyModuleRecord* instance = new (NotNull, allocateCell<WebAssemblyModuleRecord>(vm)) WebAssemblyModuleRecord(vm, structure, moduleKey);
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
    ASSERT(inherits(info()));
    for (const auto& exp : moduleInformation.exports) {
        Identifier field = Identifier::fromString(vm, String::fromUTF8(exp.field));
        addExportEntry(ExportEntry::createLocal(field, field));
    }
}

template<typename Visitor>
void WebAssemblyModuleRecord::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    WebAssemblyModuleRecord* thisObject = jsCast<WebAssemblyModuleRecord*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_instance);
    visitor.append(thisObject->m_startFunction);
    visitor.append(thisObject->m_exportsObject);
}

DEFINE_VISIT_CHILDREN(WebAssemblyModuleRecord);

void WebAssemblyModuleRecord::prepareLink(VM& vm, JSWebAssemblyInstance* instance)
{
    RELEASE_ASSERT(!m_instance);
    m_instance.set(vm, this, instance);
}

Synchronousness WebAssemblyModuleRecord::link(JSGlobalObject* globalObject, JSValue)
{
    VM& vm = globalObject->vm();

    RELEASE_ASSERT(m_instance);

    JSWebAssemblyModule* module = m_instance->module();
    SymbolTable* exportSymbolTable = module->exportSymbolTable();

    JSModuleEnvironment* moduleEnvironment = JSModuleEnvironment::create(vm, globalObject, nullptr, exportSymbolTable, jsTDZValue(), this);
    setModuleEnvironment(globalObject, moduleEnvironment);

    return Synchronousness::Sync;
}

// https://webassembly.github.io/spec/js-api/#read-the-imports
void WebAssemblyModuleRecord::initializeImports(JSGlobalObject* globalObject, JSObject* importObject, Wasm::CreationMode creationMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT(m_instance);

    JSWebAssemblyModule* module = m_instance->module();
    const Wasm::ModuleInformation& moduleInformation = module->moduleInformation();

    auto exception = [&] (JSObject* error) {
        throwException(globalObject, scope, error);
    };

    auto importFailMessage = [&] (const Wasm::Import& import, const char* before, const char* after) {
        return makeString(before, " ", String::fromUTF8(import.module), ":", String::fromUTF8(import.field), " ", after);
    };

    for (const auto& import : moduleInformation.imports) {
        Identifier moduleName = Identifier::fromString(vm, String::fromUTF8(import.module));
        Identifier fieldName = Identifier::fromString(vm, String::fromUTF8(import.field));
        JSValue value;
        if (creationMode == Wasm::CreationMode::FromJS) {
            // 1. Let o be the resultant value of performing Get(importObject, i.module_name).
            JSValue importModuleValue = importObject->get(globalObject, moduleName);
            RETURN_IF_EXCEPTION(scope, void());
            // 2. If Type(o) is not Object, throw a TypeError.
            if (!importModuleValue.isObject())
                return exception(createTypeError(globalObject, importFailMessage(import, "import", "must be an object"), defaultSourceAppender, runtimeTypeForValue(importModuleValue)));

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
            if (!value.isCallable())
                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "import function", "must be callable")));

            Wasm::Instance* calleeInstance = nullptr;
            WasmToWasmImportableFunction::LoadLocation entrypointLoadLocation = nullptr;
            JSObject* function = jsCast<JSObject*>(value);

            // ii. If v is an Exported Function Exotic Object:
            WebAssemblyFunction* wasmFunction;
            WebAssemblyWrapperFunction* wasmWrapperFunction;
            if (isWebAssemblyHostFunction(function, wasmFunction, wasmWrapperFunction)) {
                // a. If the signature of v does not match the signature of i, throw a WebAssembly.LinkError.
                Wasm::TypeIndex importedTypeIndex;
                if (wasmFunction) {
                    importedTypeIndex = wasmFunction->typeIndex();
                    calleeInstance = &wasmFunction->instance()->instance();
                    entrypointLoadLocation = wasmFunction->entrypointLoadLocation();
                } else {
                    importedTypeIndex = wasmWrapperFunction->typeIndex();
                    // b. Let closure be v.[[Closure]].
                    function = wasmWrapperFunction->function();
                }
                Wasm::TypeIndex expectedTypeIndex = moduleInformation.importFunctionTypeIndices[import.kindIndex];
                if (importedTypeIndex != expectedTypeIndex)
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
            m_instance->instance().importFunction(import.kindIndex).set(vm, m_instance.get(), function);
            break;
        }

        case Wasm::ExternalKind::Global: {
            // 5. If i is a global import:
            const Wasm::GlobalInformation& global = moduleInformation.globals[import.kindIndex];
            if (global.mutability == Wasm::Immutable) {
                if (value.inherits<JSWebAssemblyGlobal>()) {
                    JSWebAssemblyGlobal* globalValue = jsCast<JSWebAssemblyGlobal*>(value);
                    if (!isSubtype(globalValue->global()->type(), global.type))
                        return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "must be a same type")));
                    if (globalValue->global()->mutability() != Wasm::Immutable)
                        return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "must be a same mutability")));
                    const auto& declaredGlobalType = moduleInformation.globals[import.kindIndex].type;
                    switch (declaredGlobalType.kind) {
                    case Wasm::TypeKind::I32:
                    case Wasm::TypeKind::I64:
                    case Wasm::TypeKind::F32:
                    case Wasm::TypeKind::F64:
                        m_instance->instance().setGlobal(import.kindIndex, globalValue->global()->getPrimitive());
                        break;
                    default:
                        if (Wasm::isExternref(declaredGlobalType)) {
                            value = globalValue->global()->get(globalObject);
                            RETURN_IF_EXCEPTION(scope, void());
                            if (!global.type.isNullable() && value.isNull())
                                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "non-null externref cannot be null")));
                            m_instance->instance().setGlobal(import.kindIndex, value);
                        } else if (Wasm::isFuncref(declaredGlobalType) || Wasm::isRefWithTypeIndex(declaredGlobalType)) {
                            WebAssemblyFunction* wasmFunction = nullptr;
                            WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
                            value = globalValue->global()->get(globalObject);
                            RETURN_IF_EXCEPTION(scope, void());
                            if (!isWebAssemblyHostFunction(value, wasmFunction, wasmWrapperFunction) && (!global.type.isNullable() || !value.isNull())) {
                                const char* msg = global.type.isNullable() ? "must be a wasm exported function or null" : "must be a wasm exported function";
                                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", msg)));
                            }

                            if (Wasm::isRefWithTypeIndex(declaredGlobalType) && !value.isNull()) {
                                Wasm::TypeIndex paramIndex = global.type.index;
                                Wasm::TypeIndex argIndex = wasmFunction ? wasmFunction->typeIndex() : wasmWrapperFunction->typeIndex();
                                if (paramIndex != argIndex)
                                    return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "Argument function did not match the reference type")));
                            }

                            m_instance->instance().setGlobal(import.kindIndex, value);
                        } else
                            RELEASE_ASSERT_NOT_REACHED();
                    }
                } else {
                    const auto globalType = moduleInformation.globals[import.kindIndex].type;
                    if (!isRefType(globalType)) {
                        // ii. If the global_type of i is i64 or Type(v) is Number, throw a WebAssembly.LinkError.
                        if (globalType.isI64()) {
                            if (!value.isBigInt())
                                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "must be a BigInt")));
                        } else {
                            if (!value.isNumber())
                                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "must be a number")));
                        }
                    }

                    // iii. Append ToWebAssemblyValue(v) to imports.
                    switch (globalType.kind) {
                    case Wasm::TypeKind::I32:
                        m_instance->instance().setGlobal(import.kindIndex, value.toInt32(globalObject));
                        break;
                    case Wasm::TypeKind::I64: {
                        int64_t bits = value.toBigInt64(globalObject);
                        RETURN_IF_EXCEPTION(scope, void());
                        m_instance->instance().setGlobal(import.kindIndex, bits);
                        break;
                    }
                    case Wasm::TypeKind::F32:
                        m_instance->instance().setGlobal(import.kindIndex, bitwise_cast<uint32_t>(value.toFloat(globalObject)));
                        break;
                    case Wasm::TypeKind::F64:
                        m_instance->instance().setGlobal(import.kindIndex, bitwise_cast<uint64_t>(value.asNumber()));
                        break;
                    case Wasm::TypeKind::V128:
                        return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "cannot be v128")));
                        break;
                    default:
                        if (Wasm::isExternref(globalType)) {
                            if (!globalType.isNullable() && value.isNull())
                                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "must be a non-null value")));
                            m_instance->instance().setGlobal(import.kindIndex, value);
                        } else if (Wasm::isFuncref(globalType) || Wasm::isRefWithTypeIndex(globalType)) {
                            WebAssemblyFunction* wasmFunction = nullptr;
                            WebAssemblyWrapperFunction* wasmWrapperFunction = nullptr;
                            if (!isWebAssemblyHostFunction(value, wasmFunction, wasmWrapperFunction) && (!globalType.isNullable() || !value.isNull())) {
                                const char* msg = globalType.isNullable() ? "must be a wasm exported function or null" : "must be a wasm exported function";
                                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", msg)));
                            }

                            if (Wasm::isRefWithTypeIndex(globalType) && !value.isNull()) {
                                Wasm::TypeIndex paramIndex = global.type.index;
                                Wasm::TypeIndex argIndex = wasmFunction ? wasmFunction->typeIndex() : wasmWrapperFunction->typeIndex();
                                if (paramIndex != argIndex)
                                    return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported global", "Argument function did not match the reference type")));
                            }

                            m_instance->instance().setGlobal(import.kindIndex, value);
                        } else
                            RELEASE_ASSERT_NOT_REACHED();
                    }
                }
            } else {
                if (!value.inherits<JSWebAssemblyGlobal>())
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
            // 7. If i is a table import:
            JSWebAssemblyTable* table = jsDynamicCast<JSWebAssemblyTable*>(value);
            // i. If v is not a WebAssembly.Table object, throw a WebAssembly.LinkError.
            if (!table)
                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "Table import", "is not an instance of WebAssembly.Table")));

            uint32_t expectedInitial = moduleInformation.tables[import.kindIndex].initial();
            uint32_t actualInitial = table->length();
            if (actualInitial < expectedInitial)
                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "Table import", "provided an 'initial' that is too small")));

            if (std::optional<uint32_t> expectedMaximum = moduleInformation.tables[import.kindIndex].maximum()) {
                std::optional<uint32_t> actualMaximum = table->maximum();
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

        case Wasm::ExternalKind::Exception: {
            JSWebAssemblyTag* tag = jsDynamicCast<JSWebAssemblyTag*>(value);
            if (!tag)
                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "Tag import", "is not an instance of WebAssembly.Tag")));

            Wasm::TypeIndex expectedTypeIndex = moduleInformation.importExceptionTypeIndices[import.kindIndex];

            if (expectedTypeIndex != tag->tag().typeIndex())
                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "imported Tag", "signature doesn't match the imported WebAssembly Tag's signature")));

            m_instance->instance().setTag(import.kindIndex, tag->tag());
            break;
        }

        case Wasm::ExternalKind::Memory:
            JSWebAssemblyMemory* memory = jsDynamicCast<JSWebAssemblyMemory*>(value);
            // i. If v is not a WebAssembly.Memory object, throw a WebAssembly.LinkError.
            if (!memory)
                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "Memory import", "is not an instance of WebAssembly.Memory")));

            PageCount declaredInitial = moduleInformation.memory.initial();
            size_t importedSize = memory->memory().size();
            if (importedSize < declaredInitial.bytes())
                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "Memory import", "provided a 'size' that is smaller than the module's declared 'initial' import memory size")));

            if (PageCount declaredMaximum = moduleInformation.memory.maximum()) {
                PageCount importedMaximum = memory->memory().maximum();
                if (!importedMaximum)
                    return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "Memory import", "did not have a 'maximum' but the module requires that it does")));

                if (importedMaximum > declaredMaximum)
                    return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "Memory import", "provided a 'maximum' that is larger than the module's declared 'maximum' import memory size")));
            }

            if ((memory->memory().sharingMode() == MemorySharingMode::Shared) != moduleInformation.memory.isShared())
                return exception(createJSWebAssemblyLinkError(globalObject, vm, importFailMessage(import, "Memory import", "provided a 'shared' that is different from the module's declared 'shared' import memory attribute")));

            // ii. Append v to memories.
            // iii. Append v.[[Memory]] to imports.
            m_instance->setMemory(vm, memory);
            RETURN_IF_EXCEPTION(scope, void());
            break;
        }
    }
}

// https://webassembly.github.io/spec/js-api/#create-an-exports-object
void WebAssemblyModuleRecord::initializeExports(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT(m_instance);

    JSWebAssemblyModule* module = m_instance->module();
    const Wasm::ModuleInformation& moduleInformation = module->moduleInformation();

    auto exception = [&] (JSObject* error) {
        throwException(globalObject, scope, error);
    };

    if (moduleInformation.hasMemoryImport()) {
        // Usually at this point the module's code block in any memory mode should be
        // runnable due to the LLint tier code being shared among all modes. However,
        // if LLInt is disabled, it is possible that the code needs to be compiled at
        // this point when we know which memory mode to use.
        Wasm::CalleeGroup* calleeGroup = m_instance->instance().calleeGroup();
        if (!calleeGroup || !calleeGroup->runnable()) {
            calleeGroup = m_instance->module()->module().compileSync(vm, m_instance->instance().memory()->mode()).ptr();
            if (!calleeGroup->runnable())
                return exception(createJSWebAssemblyLinkError(globalObject, vm, calleeGroup->errorMessage()));
        }
        RELEASE_ASSERT(calleeGroup->isSafeToRun(m_instance->instance().memory()->mode()));
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
                return exception(createJSWebAssemblyLinkError(globalObject, vm, "couldn't create Table"_s));
            JSWebAssemblyTable* table = JSWebAssemblyTable::tryCreate(globalObject, vm, globalObject->webAssemblyTableStructure(), wasmTable.releaseNonNull());
            // We should always be able to allocate a JSWebAssemblyTable we've defined.
            // If it's defined to be too large, we should have thrown a validation error.
            scope.assertNoException();
            ASSERT(table);
            m_instance->setTable(vm, i, table);
            RETURN_IF_EXCEPTION(scope, void());
        }
    }

    // This needs to be looked up after the memory is initialized, as the codeBlock depends on the memory mode.
    Wasm::CalleeGroup* calleeGroup = m_instance->instance().calleeGroup();

    for (unsigned index = 0; index < moduleInformation.internalExceptionTypeIndices.size(); ++index) {
        Wasm::TypeIndex typeIndex = moduleInformation.internalExceptionTypeIndices[index];
        m_instance->instance().setTag(moduleInformation.importExceptionCount() + index, Wasm::Tag::create(Wasm::TypeInformation::get(typeIndex).expand()));
    }

    unsigned functionImportCount = calleeGroup->functionImportCount();
    auto makeFunctionWrapper = [&] (uint32_t functionIndexSpace) -> JSValue {
        // If we already made a wrapper, do not make a new one.
        JSValue wrapper = m_instance->instance().getFunctionWrapper(functionIndexSpace);

        if (!wrapper.isNull())
            return wrapper;

        // 1. If e is a closure c:
        //   i. If there is an Exported Function Exotic Object func in funcs whose func.[[Closure]] equals c, then return func.
        //   ii. (Note: At most one wrapper is created for any closure, so func is unique, even if there are multiple occurrances in the list. Moreover, if the item was an import that is already an Exported Function Exotic Object, then the original function object will be found. For imports that are regular JS functions, a new wrapper will be created.)
        if (functionIndexSpace < functionImportCount) {
            JSObject* functionImport = m_instance->instance().importFunction(functionIndexSpace).get();
            if (isWebAssemblyHostFunction(functionImport))
                wrapper = functionImport;
            else {
                Wasm::TypeIndex typeIndex = module->typeIndexFromFunctionIndexSpace(functionIndexSpace);
                wrapper = WebAssemblyWrapperFunction::create(vm, globalObject, globalObject->webAssemblyWrapperFunctionStructure(), functionImport, functionIndexSpace, m_instance.get(), typeIndex);
            }
        } else {
            //   iii. Otherwise:
            //     a. Let func be an Exported Function Exotic Object created from c.
            //     b. Append func to funcs.
            //     c. Return func.
            Wasm::Callee& jsEntrypointCallee = calleeGroup->jsEntrypointCalleeFromFunctionIndexSpace(functionIndexSpace);
            Wasm::WasmToWasmImportableFunction::LoadLocation entrypointLoadLocation = calleeGroup->entrypointLoadLocationFromFunctionIndexSpace(functionIndexSpace);
            Wasm::TypeIndex typeIndex = module->typeIndexFromFunctionIndexSpace(functionIndexSpace);
            const auto& signature = Wasm::TypeInformation::getFunctionSignature(typeIndex);
            WebAssemblyFunction* function = WebAssemblyFunction::create(vm, globalObject, globalObject->webAssemblyFunctionStructure(), signature.argumentCount(), makeString(functionIndexSpace), m_instance.get(), jsEntrypointCallee, entrypointLoadLocation, typeIndex);
            wrapper = function;
        }

        ASSERT(wrapper.isCallable());
        m_instance->instance().setFunctionWrapper(functionIndexSpace, wrapper);

        return wrapper;
    };

    for (auto functionIndexSpace : moduleInformation.referencedFunctions())
        makeFunctionWrapper(functionIndexSpace);

    // Globals
    {
        for (size_t globalIndex = moduleInformation.firstInternalGlobal; globalIndex < moduleInformation.globals.size(); ++globalIndex) {
            const auto& global = moduleInformation.globals[globalIndex];
            ASSERT(global.initializationType != Wasm::GlobalInformation::IsImport);

            if (global.type == Wasm::Types::V128) {
                v128_t initialVector;

                if (global.initializationType == Wasm::GlobalInformation::FromGlobalImport) {
                    ASSERT(global.initialBits.initialBitsOrImportNumber < moduleInformation.firstInternalGlobal);
                    initialVector = m_instance->instance().loadV128Global(global.initialBits.initialBitsOrImportNumber);
                } else if (global.initializationType == Wasm::GlobalInformation::FromExpression)
                    initialVector = global.initialBits.initialVector;
                else
                    RELEASE_ASSERT_NOT_REACHED();
                switch (global.bindingMode) {
                case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance: {
                    m_instance->instance().setGlobal(globalIndex, initialVector);
                    break;
                }
                case Wasm::GlobalInformation::BindingMode::Portable: {
                    ASSERT(global.mutability == Wasm::Mutable);
                    Ref<Wasm::Global> globalRef = Wasm::Global::create(global.type, Wasm::Mutability::Mutable, initialVector);
                    JSWebAssemblyGlobal* globalValue = JSWebAssemblyGlobal::tryCreate(globalObject, vm, globalObject->webAssemblyGlobalStructure(), WTFMove(globalRef));
                    scope.assertNoException();
                    m_instance->linkGlobal(vm, globalIndex, globalValue);
                    break;
                }
                }
                continue;
            }
            ASSERT(global.initializationType != Wasm::GlobalInformation::FromVector);

            uint64_t initialBits = 0;
            if (global.initializationType == Wasm::GlobalInformation::FromGlobalImport) {
                ASSERT(global.initialBits.initialBitsOrImportNumber < moduleInformation.firstInternalGlobal);
                initialBits = m_instance->instance().loadI64Global(global.initialBits.initialBitsOrImportNumber);
            } else if (global.initializationType == Wasm::GlobalInformation::FromRefFunc) {
                ASSERT(global.initialBits.initialBitsOrImportNumber < moduleInformation.functionIndexSpaceSize());
                ASSERT(makeFunctionWrapper(global.initialBits.initialBitsOrImportNumber).isCallable());
                initialBits = JSValue::encode(makeFunctionWrapper(global.initialBits.initialBitsOrImportNumber));
            } else
                initialBits = global.initialBits.initialBitsOrImportNumber;

            switch (global.bindingMode) {
            case Wasm::GlobalInformation::BindingMode::EmbeddedInInstance: {
                m_instance->instance().setGlobal(globalIndex, initialBits);
                break;
            }
            case Wasm::GlobalInformation::BindingMode::Portable: {
                ASSERT(global.mutability == Wasm::Mutable);
                Ref<Wasm::Global> globalRef = Wasm::Global::create(global.type, Wasm::Mutability::Mutable, initialBits);
                JSWebAssemblyGlobal* globalValue = JSWebAssemblyGlobal::tryCreate(globalObject, vm, globalObject->webAssemblyGlobalStructure(), WTFMove(globalRef));
                scope.assertNoException();
                m_instance->linkGlobal(vm, globalIndex, globalValue);
                ensureStillAliveHere(initialBits); // Ensure this is kept alive while creating JSWebAssemblyGlobal.
                break;
            }
            }
        }
    }

    // Let exports be a list of (string, JS value) pairs that is mapped from each external value e in instance.exports as follows:
    // https://webassembly.github.io/spec/js-api/index.html#create-an-exports-object
    JSObject* exportsObject = constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());
    JSModuleEnvironment* moduleEnvironment = this->moduleEnvironment();
    for (const auto& exp : moduleInformation.exports) {
        JSValue exportedValue;
        switch (exp.kind) {
        case Wasm::ExternalKind::Function: {
            exportedValue = makeFunctionWrapper(exp.kindIndex);
            ASSERT(exportedValue.isCallable());
            ASSERT(makeFunctionWrapper(exp.kindIndex) == exportedValue);
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
            switch (global.type.kind) {
            case Wasm::TypeKind::Externref:
            case Wasm::TypeKind::Funcref:
            case Wasm::TypeKind::Ref:
            case Wasm::TypeKind::RefNull:
            case Wasm::TypeKind::I32:
            case Wasm::TypeKind::I64:
            case Wasm::TypeKind::F32:
            case Wasm::TypeKind::F64:
            case Wasm::TypeKind::V128: {
                // If global is immutable, we are not creating a binding internally.
                // But we need to create a binding just to export it. This binding is not actually connected. But this is OK since it is immutable.
                if (global.bindingMode == Wasm::GlobalInformation::BindingMode::EmbeddedInInstance) {
                    RefPtr<Wasm::Global> globalRef;
                    if (global.type.kind == Wasm::TypeKind::V128) {
                        v128_t initialValue = m_instance->instance().loadV128Global(exp.kindIndex);
                        globalRef = Wasm::Global::create(global.type, global.mutability, initialValue);
                    } else {
                        uint64_t initialValue = m_instance->instance().loadI64Global(exp.kindIndex);
                        globalRef = Wasm::Global::create(global.type, global.mutability, initialValue);
                    }
                    exportedValue = JSWebAssemblyGlobal::tryCreate(globalObject, vm, globalObject->webAssemblyGlobalStructure(), globalRef.releaseNonNull());
                    scope.assertNoException();
                } else {
                    ASSERT(global.mutability == Wasm::Mutability::Mutable);
                    RefPtr<Wasm::Global> globalRef = m_instance->instance().getGlobalBinding(exp.kindIndex);
                    ASSERT(globalRef);
                    ASSERT(globalRef->type() == global.type);
                    ASSERT(globalRef->mutability() == global.mutability);
                    ASSERT(globalRef->mutability() == Wasm::Mutability::Mutable);
                    ASSERT(globalRef->owner());
                    exportedValue = globalRef->owner();
                }
                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            break;
        }
        case Wasm::ExternalKind::Exception: {
            exportedValue = JSWebAssemblyTag::create(vm, globalObject, globalObject->m_webAssemblyTagStructure.get(globalObject), m_instance->instance().tag(exp.kindIndex));
            break;
        }
        }

        Identifier propertyName = Identifier::fromString(vm, String::fromUTF8(exp.field));

        bool shouldThrowReadOnlyError = false;
        bool ignoreReadOnlyErrors = true;
        bool putResult = false;
        symbolTablePutTouchWatchpointSet(moduleEnvironment, globalObject, propertyName, exportedValue, shouldThrowReadOnlyError, ignoreReadOnlyErrors, putResult);
        scope.assertNoException();
        RELEASE_ASSERT(putResult);

        if (std::optional<uint32_t> index = parseIndex(propertyName)) {
            exportsObject->putDirectIndex(globalObject, index.value(), exportedValue);
            RETURN_IF_EXCEPTION(scope, void());
        } else
            exportsObject->putDirect(vm, propertyName, exportedValue);
    }

    objectConstructorFreeze(globalObject, exportsObject);
    RETURN_IF_EXCEPTION(scope, void());
    m_exportsObject.set(vm, this, exportsObject);

    bool hasStart = !!moduleInformation.startFunctionIndexSpace;
    if (hasStart) {
        auto startFunctionIndexSpace = moduleInformation.startFunctionIndexSpace.value_or(0);
        Wasm::TypeIndex typeIndex = module->typeIndexFromFunctionIndexSpace(startFunctionIndexSpace);
        const auto& signature = Wasm::TypeInformation::getFunctionSignature(typeIndex);
        // The start function must not take any arguments or return anything. This is enforced by the parser.
        ASSERT(!signature.argumentCount());
        ASSERT(signature.returnsVoid());
        if (startFunctionIndexSpace < calleeGroup->functionImportCount()) {
            JSObject* startFunction = m_instance->instance().importFunction(startFunctionIndexSpace).get();
            m_startFunction.set(vm, this, startFunction);
        } else {
            Wasm::Callee& jsEntrypointCallee = calleeGroup->jsEntrypointCalleeFromFunctionIndexSpace(startFunctionIndexSpace);
            Wasm::WasmToWasmImportableFunction::LoadLocation entrypointLoadLocation = calleeGroup->entrypointLoadLocationFromFunctionIndexSpace(startFunctionIndexSpace);
            WebAssemblyFunction* function = WebAssemblyFunction::create(vm, globalObject, globalObject->webAssemblyFunctionStructure(), signature.argumentCount(), "start"_s, m_instance.get(), jsEntrypointCallee, entrypointLoadLocation, typeIndex);
            m_startFunction.set(vm, this, function);
        }
    }
}

template <typename Scope, typename M, typename N, typename ...Args>
NEVER_INLINE static JSValue dataSegmentFail(JSGlobalObject* globalObject, VM& vm, Scope& scope, M memorySize, N segmentSize, N offset, Args... args)
{
    return throwException(globalObject, scope, createJSWebAssemblyRuntimeError(globalObject, vm, makeString("Invalid data segment initialization: segment of "_s, String::number(segmentSize), " bytes memory of "_s, String::number(memorySize), " bytes, at offset "_s, String::number(offset), args...)));
}

JSValue WebAssemblyModuleRecord::evaluate(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Wasm::Module& module = m_instance->instance().module();
    const Wasm::ModuleInformation& moduleInformation = module.moduleInformation();

    const Vector<Wasm::Segment::Ptr>& data = moduleInformation.data;
    
    std::optional<JSValue> exception;

    auto forEachActiveElement = [&] (auto fn) {
        for (const Wasm::Element& element : moduleInformation.elements) {
            if (!element.isActive())
                continue;

            // It should be a validation error to have any elements without a table.
            // Also, it could be that a table wasn't imported, or that the table
            // imported wasn't compatible. However, those should error out before
            // getting here.
            ASSERT(!!m_instance->table(*element.tableIndexIfActive));

            const auto& offset = *element.offsetIfActive;
            const uint32_t elementIndex = offset.isGlobalImport()
                ? static_cast<uint32_t>(m_instance->instance().loadI32Global(offset.globalImportIndex()))
                : offset.constValue();

            if (fn(element, *element.tableIndexIfActive, elementIndex) == IterationStatus::Done)
                break;

            if (exception)
                break;
        }
    };

    auto forEachActiveDataSegment = [&] (auto fn) {
        auto wasmMemory = m_instance->instance().memory();
        uint8_t* memory = reinterpret_cast<uint8_t*>(wasmMemory->basePointer());
        uint64_t sizeInBytes = wasmMemory->size();

        for (const Wasm::Segment::Ptr& segment : data) {
            if (!segment->isActive())
                continue;
            uint32_t offset = segment->offsetIfActive->isGlobalImport()
                ? static_cast<uint32_t>(m_instance->instance().loadI32Global(segment->offsetIfActive->globalImportIndex()))
                : segment->offsetIfActive->constValue();

            if (fn(memory, sizeInBytes, segment, offset) == IterationStatus::Done)
                break;

            if (exception)
                break;
        }
    };

    // Validation of all element ranges comes before all Table and Memory initialization.
    forEachActiveElement([&](const Wasm::Element& element, uint32_t tableIndex, uint32_t elementIndex) {
        int64_t lastWrittenIndex = static_cast<int64_t>(elementIndex) + static_cast<int64_t>(element.functionIndices.size()) - 1;
        if (UNLIKELY(lastWrittenIndex >= m_instance->table(tableIndex)->length())) {
            exception = JSValue(throwException(globalObject, scope, createJSWebAssemblyRuntimeError(globalObject, vm, "Element is trying to set an out of bounds table index"_s)));
            return IterationStatus::Done;
        }

        m_instance->instance().initElementSegment(tableIndex, element, elementIndex, 0U, element.length());
        return IterationStatus::Continue;
    });

    if (UNLIKELY(exception))
        return exception.value();

    // Validation of all segment ranges comes before all Table and Memory initialization.
    forEachActiveDataSegment([&](uint8_t* memory, uint64_t sizeInBytes, const Wasm::Segment::Ptr& segment, uint32_t offset) {
        if (UNLIKELY(sizeInBytes < segment->sizeInBytes)) {
            exception = dataSegmentFail(globalObject, vm, scope, sizeInBytes, segment->sizeInBytes, offset, ", segment is too big"_s);
            return IterationStatus::Done;
        }
        if (UNLIKELY(offset > sizeInBytes - segment->sizeInBytes)) {
            exception = dataSegmentFail(globalObject, vm, scope, sizeInBytes, segment->sizeInBytes, offset, ", segment writes outside of memory"_s);
            return IterationStatus::Done;
        }

        // Empty segments are valid, but only if memory isn't present, which would be undefined behavior in memcpy.
        if (segment->sizeInBytes) {
            RELEASE_ASSERT(memory);
            memcpy(memory + offset, &segment->byte(0), segment->sizeInBytes);
        }
        return IterationStatus::Continue;
    });

    if (UNLIKELY(exception))
        return exception.value();

    ASSERT(!exception);

    if (JSObject* startFunction = m_startFunction.get()) {
        auto callData = JSC::getCallData(startFunction);
        call(globalObject, startFunction, callData, jsUndefined(), *vm.emptyList);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return jsUndefined();
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
