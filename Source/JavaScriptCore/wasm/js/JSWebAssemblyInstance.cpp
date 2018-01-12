/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyInstance.h"

#if ENABLE(WEBASSEMBLY)

#include "AbstractModuleRecord.h"
#include "JSCInlines.h"
#include "JSModuleEnvironment.h"
#include "JSModuleNamespaceObject.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyLinkError.h"
#include "JSWebAssemblyMemory.h"
#include "JSWebAssemblyModule.h"
#include "WebAssemblyModuleRecord.h"
#include "WebAssemblyToJSCallee.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

const ClassInfo JSWebAssemblyInstance::s_info = { "WebAssembly.Instance", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyInstance) };

Structure* JSWebAssemblyInstance::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSWebAssemblyInstance::JSWebAssemblyInstance(VM& vm, Structure* structure, Ref<Wasm::Instance>&& instance)
    : Base(vm, structure)
    , m_instance(WTFMove(instance))
{
    for (unsigned i = 0; i < this->instance().numImportFunctions(); ++i)
        new (this->instance().importFunction<PoisonedBarrier<JSObject>>(i)) PoisonedBarrier<JSObject>();
}

void JSWebAssemblyInstance::finishCreation(VM& vm, JSWebAssemblyModule* module, JSModuleNamespaceObject* moduleNamespaceObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    m_module.set(vm, this, module);
    m_moduleNamespaceObject.set(vm, this, moduleNamespaceObject);
    m_callee.set(vm, this, module->callee());

    heap()->reportExtraMemoryAllocated(m_instance->extraMemoryAllocated());
}

void JSWebAssemblyInstance::destroy(JSCell* cell)
{
    static_cast<JSWebAssemblyInstance*>(cell)->JSWebAssemblyInstance::~JSWebAssemblyInstance();
}

void JSWebAssemblyInstance::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    auto* thisObject = jsCast<JSWebAssemblyInstance*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_module);
    visitor.append(thisObject->m_codeBlock);
    visitor.append(thisObject->m_moduleNamespaceObject);
    visitor.append(thisObject->m_memory);
    visitor.append(thisObject->m_table);
    visitor.append(thisObject->m_callee);
    visitor.reportExtraMemoryVisited(thisObject->m_instance->extraMemoryAllocated());
    for (unsigned i = 0; i < thisObject->instance().numImportFunctions(); ++i)
        visitor.append(*thisObject->instance().importFunction<PoisonedBarrier<JSObject>>(i)); // This also keeps the functions' JSWebAssemblyInstance alive.
}

void JSWebAssemblyInstance::finalizeCreation(VM& vm, ExecState* exec, Ref<Wasm::CodeBlock>&& wasmCodeBlock)
{
    m_instance->finalizeCreation(this, wasmCodeBlock.copyRef());

    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!wasmCodeBlock->runnable()) {
        throwException(exec, scope, JSWebAssemblyLinkError::create(exec, vm, globalObject()->WebAssemblyLinkErrorStructure(), wasmCodeBlock->errorMessage()));
        return;
    }

    RELEASE_ASSERT(wasmCodeBlock->isSafeToRun(memoryMode()));
    JSWebAssemblyCodeBlock* jsCodeBlock = m_module->codeBlock(memoryMode());
    if (jsCodeBlock) {
        // A CodeBlock might have already been compiled. If so, it means
        // that the CodeBlock we are trying to compile must be the same
        // because we will never compile a CodeBlock again once it's
        // runnable.
        ASSERT(&jsCodeBlock->codeBlock() == wasmCodeBlock.ptr());
        m_codeBlock.set(vm, this, jsCodeBlock);
    } else {
        jsCodeBlock = JSWebAssemblyCodeBlock::create(vm, WTFMove(wasmCodeBlock), module()->module().moduleInformation());
        if (UNLIKELY(!jsCodeBlock->runnable())) {
            throwException(exec, scope, JSWebAssemblyLinkError::create(exec, vm, globalObject()->WebAssemblyLinkErrorStructure(), jsCodeBlock->errorMessage()));
            return;
        }
        m_codeBlock.set(vm, this, jsCodeBlock);
        m_module->setCodeBlock(vm, memoryMode(), jsCodeBlock);
    }

    for (unsigned importFunctionNum = 0; importFunctionNum < instance().numImportFunctions(); ++importFunctionNum) {
        auto* info = instance().importFunctionInfo(importFunctionNum);
        info->wasmToEmbedderStubExecutableAddress = m_codeBlock->wasmToEmbedderStubExecutableAddress(importFunctionNum);
    }

    auto* moduleRecord = jsCast<WebAssemblyModuleRecord*>(m_moduleNamespaceObject->moduleRecord());
    moduleRecord->link(exec, m_module.get(), this);
    RETURN_IF_EXCEPTION(scope, void());

    JSValue startResult = moduleRecord->evaluate(exec);
    UNUSED_PARAM(startResult);
    RETURN_IF_EXCEPTION(scope, void());
}

JSWebAssemblyInstance* JSWebAssemblyInstance::create(VM& vm, ExecState* exec, JSWebAssemblyModule* jsModule, JSObject* importObject, Structure* instanceStructure, Ref<Wasm::Module>&& module)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* globalObject = exec->lexicalGlobalObject();

    const Wasm::ModuleInformation& moduleInformation = jsModule->moduleInformation();

    auto exception = [&] (JSObject* error) {
        throwException(exec, throwScope, error);
        return nullptr;
    };

    if (!globalObject->webAssemblyEnabled())
        return exception(createEvalError(exec, globalObject->webAssemblyDisabledErrorMessage()));

    auto importFailMessage = [&] (const Wasm::Import& import, const char* before, const char* after) {
        return makeString(before, " ", String::fromUTF8(import.module), ":", String::fromUTF8(import.field), " ", after);
    };

    // If the list of module.imports is not empty and Type(importObject) is not Object, a TypeError is thrown.
    if (moduleInformation.imports.size() && !importObject)
        return exception(createTypeError(exec, ASCIILiteral("can't make WebAssembly.Instance because there is no imports Object and the WebAssembly.Module requires imports")));

    Identifier moduleKey = Identifier::fromUid(PrivateName(PrivateName::Description, "WebAssemblyInstance"));
    WebAssemblyModuleRecord* moduleRecord = WebAssemblyModuleRecord::create(exec, vm, globalObject->webAssemblyModuleRecordStructure(), moduleKey, moduleInformation);
    RETURN_IF_EXCEPTION(throwScope, nullptr);

    JSModuleNamespaceObject* moduleNamespace = moduleRecord->getModuleNamespace(exec);

    auto storeTopCallFrame = [&vm] (void* topCallFrame) {
        vm.topCallFrame = bitwise_cast<ExecState*>(topCallFrame);
    };

    // FIXME: These objects could be pretty big we should try to throw OOM here.
    auto* jsInstance = new (NotNull, allocateCell<JSWebAssemblyInstance>(vm.heap)) JSWebAssemblyInstance(vm, instanceStructure, 
        Wasm::Instance::create(&vm.wasmContext, WTFMove(module), &vm.topEntryFrame, vm.addressOfSoftStackLimit(), WTFMove(storeTopCallFrame)));
    jsInstance->finishCreation(vm, jsModule, moduleNamespace);
    RETURN_IF_EXCEPTION(throwScope, nullptr);

    // Let funcs, memories and tables be initially-empty lists of callable JavaScript objects, WebAssembly.Memory objects and WebAssembly.Table objects, respectively.
    // Let imports be an initially-empty list of external values.
    unsigned numImportFunctions = 0;
    unsigned numImportGlobals = 0;

    bool hasMemoryImport = false;
    bool hasTableImport = false;
    // For each import i in module.imports:
    for (auto& import : moduleInformation.imports) {
        // 1. Let o be the resultant value of performing Get(importObject, i.module_name).
        JSValue importModuleValue = importObject->get(exec, Identifier::fromString(&vm, String::fromUTF8(import.module)));
        RETURN_IF_EXCEPTION(throwScope, nullptr);
        // 2. If Type(o) is not Object, throw a TypeError.
        if (!importModuleValue.isObject())
            return exception(createTypeError(exec, importFailMessage(import, "import", "must be an object"), defaultSourceAppender, runtimeTypeForValue(importModuleValue)));

        // 3. Let v be the value of performing Get(o, i.item_name)
        JSObject* object = jsCast<JSObject*>(importModuleValue);
        JSValue value = object->get(exec, Identifier::fromString(&vm, String::fromUTF8(import.field)));
        RETURN_IF_EXCEPTION(throwScope, nullptr);

        switch (import.kind) {
        case Wasm::ExternalKind::Function: {
            // 4. If i is a function import:
            // i. If IsCallable(v) is false, throw a WebAssembly.LinkError.
            if (!value.isFunction())
                return exception(createJSWebAssemblyLinkError(exec, vm, importFailMessage(import, "import function", "must be callable")));

            Wasm::Instance* calleeInstance = nullptr;
            Wasm::WasmEntrypointLoadLocation wasmEntrypoint = nullptr;
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
                    wasmEntrypoint = wasmFunction->wasmEntrypointLoadLocation();
                }
                else {
                    importedSignatureIndex = wasmWrapperFunction->signatureIndex();
                    // b. Let closure be v.[[Closure]].
                    function = wasmWrapperFunction->function();
                }
                Wasm::SignatureIndex expectedSignatureIndex = moduleInformation.importFunctionSignatureIndices[import.kindIndex];
                if (importedSignatureIndex != expectedSignatureIndex)
                    return exception(createJSWebAssemblyLinkError(exec, vm, importFailMessage(import, "imported function", "signature doesn't match the provided WebAssembly function's signature")));
            }
            // iii. Otherwise:
            // a. Let closure be a new host function of the given signature which calls v by coercing WebAssembly arguments to JavaScript arguments via ToJSValue and returns the result, if any, by coercing via ToWebAssemblyValue.
            // Note: done as part of Plan compilation.
            // iv. Append v to funcs.
            // Note: adding the JSCell to the instance list fulfills closure requirements b. above (the WebAssembly.Instance wil be kept alive) and v. below (the JSFunction).

            ASSERT(numImportFunctions == import.kindIndex);
            auto* info = jsInstance->instance().importFunctionInfo(numImportFunctions);
            info->targetInstance = calleeInstance;
            info->wasmEntrypoint = wasmEntrypoint;
            jsInstance->instance().importFunction<PoisonedBarrier<JSObject>>(numImportFunctions)->set(vm, jsInstance, function);
            ++numImportFunctions;
            // v. Append closure to imports.
            break;
        }
        case Wasm::ExternalKind::Table: {
            RELEASE_ASSERT(!hasTableImport); // This should be guaranteed by a validation failure.
            // 7. Otherwise (i is a table import):
            hasTableImport = true;
            JSWebAssemblyTable* table = jsDynamicCast<JSWebAssemblyTable*>(vm, value);
            // i. If v is not a WebAssembly.Table object, throw a WebAssembly.LinkError.
            if (!table)
                return exception(createJSWebAssemblyLinkError(exec, vm, importFailMessage(import, "Table import", "is not an instance of WebAssembly.Table")));

            uint32_t expectedInitial = moduleInformation.tableInformation.initial();
            uint32_t actualInitial = table->length();
            if (actualInitial < expectedInitial)
                return exception(createJSWebAssemblyLinkError(exec, vm, importFailMessage(import, "Table import", "provided an 'initial' that is too small")));

            if (std::optional<uint32_t> expectedMaximum = moduleInformation.tableInformation.maximum()) {
                std::optional<uint32_t> actualMaximum = table->maximum();
                if (!actualMaximum)
                    return exception(createJSWebAssemblyLinkError(exec, vm, importFailMessage(import, "Table import", "does not have a 'maximum' but the module requires that it does")));
                if (*actualMaximum > *expectedMaximum)
                    return exception(createJSWebAssemblyLinkError(exec, vm, importFailMessage(import, "Imported Table", "'maximum' is larger than the module's expected 'maximum'")));
            }

            // ii. Append v to tables.
            // iii. Append v.[[Table]] to imports.
            jsInstance->setTable(vm, table);
            RETURN_IF_EXCEPTION(throwScope, nullptr);
            break;
        }

        case Wasm::ExternalKind::Memory: {
            // 6. If i is a memory import:
            RELEASE_ASSERT(!hasMemoryImport); // This should be guaranteed by a validation failure.
            RELEASE_ASSERT(moduleInformation.memory);
            hasMemoryImport = true;
            JSWebAssemblyMemory* memory = jsDynamicCast<JSWebAssemblyMemory*>(vm, value);
            // i. If v is not a WebAssembly.Memory object, throw a WebAssembly.LinkError.
            if (!memory)
                return exception(createJSWebAssemblyLinkError(exec, vm, importFailMessage(import, "Memory import", "is not an instance of WebAssembly.Memory")));

            Wasm::PageCount declaredInitial = moduleInformation.memory.initial();
            Wasm::PageCount importedInitial = memory->memory().initial();
            if (importedInitial < declaredInitial)
                return exception(createJSWebAssemblyLinkError(exec, vm, importFailMessage(import, "Memory import", "provided an 'initial' that is smaller than the module's declared 'initial' import memory size")));

            if (Wasm::PageCount declaredMaximum = moduleInformation.memory.maximum()) {
                Wasm::PageCount importedMaximum = memory->memory().maximum();
                if (!importedMaximum)
                    return exception(createJSWebAssemblyLinkError(exec, vm, importFailMessage(import, "Memory import", "did not have a 'maximum' but the module requires that it does")));

                if (importedMaximum > declaredMaximum)
                    return exception(createJSWebAssemblyLinkError(exec, vm, importFailMessage(import, "Memory import", "provided a 'maximum' that is larger than the module's declared 'maximum' import memory size")));
            }

            // ii. Append v to memories.
            // iii. Append v.[[Memory]] to imports.
            jsInstance->setMemory(vm, memory);
            RETURN_IF_EXCEPTION(throwScope, nullptr);
            break;
        }
        case Wasm::ExternalKind::Global: {
            // 5. If i is a global import:
            // i. If i is not an immutable global, throw a TypeError.
            ASSERT(moduleInformation.globals[import.kindIndex].mutability == Wasm::Global::Immutable);
            // ii. If the global_type of i is i64 or Type(v) is not Number, throw a WebAssembly.LinkError.
            if (moduleInformation.globals[import.kindIndex].type == Wasm::I64)
                return exception(createJSWebAssemblyLinkError(exec, vm, importFailMessage(import, "imported global", "cannot be an i64")));
            if (!value.isNumber())
                return exception(createJSWebAssemblyLinkError(exec, vm, importFailMessage(import, "imported global", "must be a number")));
            // iii. Append ToWebAssemblyValue(v) to imports.
            ASSERT(numImportGlobals == import.kindIndex);
            switch (moduleInformation.globals[import.kindIndex].type) {
            case Wasm::I32:
                jsInstance->instance().setGlobal(numImportGlobals++, value.toInt32(exec));
                break;
            case Wasm::F32:
                jsInstance->instance().setGlobal(numImportGlobals++, bitwise_cast<uint32_t>(value.toFloat(exec)));
                break;
            case Wasm::F64:
                jsInstance->instance().setGlobal(numImportGlobals++, bitwise_cast<uint64_t>(value.asNumber()));
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            throwScope.assertNoException();
            break;
        }
        }
    }

    {
        if (!!moduleInformation.memory && moduleInformation.memory.isImport()) {
            // We should either have a Memory import or we should have thrown an exception.
            RELEASE_ASSERT(hasMemoryImport);
        }

        if (moduleInformation.memory && !hasMemoryImport) {
            // We create a memory when it's a memory definition.
            RELEASE_ASSERT(!moduleInformation.memory.isImport());

            auto* jsMemory = JSWebAssemblyMemory::create(exec, vm, exec->lexicalGlobalObject()->WebAssemblyMemoryStructure());
            RETURN_IF_EXCEPTION(throwScope, nullptr);

            RefPtr<Wasm::Memory> memory = Wasm::Memory::create(moduleInformation.memory.initial(), moduleInformation.memory.maximum(),
                [&vm] (Wasm::Memory::NotifyPressure) { vm.heap.collectAsync(CollectionScope::Full); },
                [&vm] (Wasm::Memory::SyncTryToReclaim) { vm.heap.collectSync(CollectionScope::Full); },
                [&vm, jsMemory] (Wasm::Memory::GrowSuccess, Wasm::PageCount oldPageCount, Wasm::PageCount newPageCount) { jsMemory->growSuccessCallback(vm, oldPageCount, newPageCount); });
            if (!memory)
                return exception(createOutOfMemoryError(exec));

            jsMemory->adopt(memory.releaseNonNull());
            jsInstance->setMemory(vm, jsMemory);
            RETURN_IF_EXCEPTION(throwScope, nullptr);
        }
    }

    {
        if (!!moduleInformation.tableInformation && moduleInformation.tableInformation.isImport()) {
            // We should either have a Table import or we should have thrown an exception.
            RELEASE_ASSERT(hasTableImport);
        }

        if (!!moduleInformation.tableInformation && !hasTableImport) {
            RELEASE_ASSERT(!moduleInformation.tableInformation.isImport());
            // We create a Table when it's a Table definition.
            RefPtr<Wasm::Table> wasmTable = Wasm::Table::create(moduleInformation.tableInformation.initial(), moduleInformation.tableInformation.maximum());
            if (!wasmTable)
                return exception(createJSWebAssemblyLinkError(exec, vm, "couldn't create Table"));
            JSWebAssemblyTable* table = JSWebAssemblyTable::create(exec, vm, exec->lexicalGlobalObject()->WebAssemblyTableStructure(), wasmTable.releaseNonNull());
            // We should always be able to allocate a JSWebAssemblyTable we've defined.
            // If it's defined to be too large, we should have thrown a validation error.
            throwScope.assertNoException();
            ASSERT(table);
            jsInstance->setTable(vm, table);
            RETURN_IF_EXCEPTION(throwScope, nullptr);
        }
    }
    
    if (!jsInstance->memory()) {
        // Make sure we have a dummy memory, so that wasm -> wasm thunks avoid checking for a nullptr Memory when trying to set pinned registers.
        auto* jsMemory = JSWebAssemblyMemory::create(exec, vm, exec->lexicalGlobalObject()->WebAssemblyMemoryStructure());
        jsMemory->adopt(Wasm::Memory::create().releaseNonNull());
        jsInstance->setMemory(vm, jsMemory);
        RETURN_IF_EXCEPTION(throwScope, nullptr);
    }
    
    // Globals
    {
        ASSERT(numImportGlobals == moduleInformation.firstInternalGlobal);
        for (size_t globalIndex = numImportGlobals; globalIndex < moduleInformation.globals.size(); ++globalIndex) {
            const auto& global = moduleInformation.globals[globalIndex];
            ASSERT(global.initializationType != Wasm::Global::IsImport);
            if (global.initializationType == Wasm::Global::FromGlobalImport) {
                ASSERT(global.initialBitsOrImportNumber < numImportGlobals);
                jsInstance->instance().setGlobal(globalIndex, jsInstance->instance().loadI64Global(global.initialBitsOrImportNumber));
            } else
                jsInstance->instance().setGlobal(globalIndex, global.initialBitsOrImportNumber);
        }
    }

    return jsInstance;
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
