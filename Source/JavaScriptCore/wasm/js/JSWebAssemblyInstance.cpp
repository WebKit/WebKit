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

    vm.heap.reportExtraMemoryAllocated(m_instance->extraMemoryAllocated());
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

void JSWebAssemblyInstance::finalizeCreation(VM& vm, ExecState* exec, Ref<Wasm::CodeBlock>&& wasmCodeBlock, JSObject* importObject, Wasm::CreationMode creationMode)
{
    m_instance->finalizeCreation(this, wasmCodeBlock.copyRef());

    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!wasmCodeBlock->runnable()) {
        throwException(exec, scope, JSWebAssemblyLinkError::create(exec, vm, globalObject(vm)->WebAssemblyLinkErrorStructure(), wasmCodeBlock->errorMessage()));
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
            throwException(exec, scope, JSWebAssemblyLinkError::create(exec, vm, globalObject(vm)->WebAssemblyLinkErrorStructure(), jsCodeBlock->errorMessage()));
            return;
        }
        m_codeBlock.set(vm, this, jsCodeBlock);
        m_module->setCodeBlock(vm, memoryMode(), jsCodeBlock);
    }

    for (unsigned importFunctionNum = 0; importFunctionNum < instance().numImportFunctions(); ++importFunctionNum) {
        auto* info = instance().importFunctionInfo(importFunctionNum);
        info->wasmToEmbedderStub = m_codeBlock->wasmToEmbedderStub(importFunctionNum);
    }

    auto* moduleRecord = jsCast<WebAssemblyModuleRecord*>(m_moduleNamespaceObject->moduleRecord());
    moduleRecord->prepareLink(vm, this);

    if (creationMode == Wasm::CreationMode::FromJS) {
        moduleRecord->link(exec, jsNull(), importObject, creationMode);
        RETURN_IF_EXCEPTION(scope, void());

        JSValue startResult = moduleRecord->evaluate(exec);
        UNUSED_PARAM(startResult);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

Identifier JSWebAssemblyInstance::createPrivateModuleKey()
{
    return Identifier::fromUid(PrivateName(PrivateName::Description, "WebAssemblyInstance"));
}

JSWebAssemblyInstance* JSWebAssemblyInstance::create(VM& vm, ExecState* exec, const Identifier& moduleKey, JSWebAssemblyModule* jsModule, JSObject* importObject, Structure* instanceStructure, Ref<Wasm::Module>&& module, Wasm::CreationMode creationMode)
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
    bool hasMemoryImport = false;

    if (creationMode == Wasm::CreationMode::FromJS) {
        // If the list of module.imports is not empty and Type(importObject) is not Object, a TypeError is thrown.
        if (moduleInformation.imports.size() && !importObject)
            return exception(createTypeError(exec, "can't make WebAssembly.Instance because there is no imports Object and the WebAssembly.Module requires imports"_s));
    }

    // For each import i in module.imports:
    for (auto& import : moduleInformation.imports) {
        Identifier moduleName = Identifier::fromString(&vm, String::fromUTF8(import.module));
        Identifier fieldName = Identifier::fromString(&vm, String::fromUTF8(import.field));
        moduleRecord->appendRequestedModule(moduleName);
        moduleRecord->addImportEntry(WebAssemblyModuleRecord::ImportEntry {
            WebAssemblyModuleRecord::ImportEntryType::Single,
            moduleName,
            fieldName,
            Identifier::fromUid(PrivateName(PrivateName::Description, "WebAssemblyImportName")),
        });

        // Skip Wasm::ExternalKind::Function validation here. It will be done in WebAssemblyModuleRecord::link.
        // Eventually we will move all the linking code here to WebAssemblyModuleRecord::link.
        switch (import.kind) {
        case Wasm::ExternalKind::Function:
        case Wasm::ExternalKind::Global:
        case Wasm::ExternalKind::Table:
            continue;
        case Wasm::ExternalKind::Memory:
            break;
        }

        JSValue value;
        if (creationMode == Wasm::CreationMode::FromJS) {
            // 1. Let o be the resultant value of performing Get(importObject, i.module_name).
            JSValue importModuleValue = importObject->get(exec, moduleName);
            RETURN_IF_EXCEPTION(throwScope, nullptr);
            // 2. If Type(o) is not Object, throw a TypeError.
            if (!importModuleValue.isObject())
                return exception(createTypeError(exec, importFailMessage(import, "import", "must be an object"), defaultSourceAppender, runtimeTypeForValue(vm, importModuleValue)));

            // 3. Let v be the value of performing Get(o, i.item_name)
            JSObject* object = jsCast<JSObject*>(importModuleValue);
            value = object->get(exec, fieldName);
            RETURN_IF_EXCEPTION(throwScope, nullptr);
        }
        if (!value)
            value = jsUndefined();

        switch (import.kind) {
        case Wasm::ExternalKind::Function:
        case Wasm::ExternalKind::Global:
        case Wasm::ExternalKind::Table:
            break;

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
        }
    }
    ASSERT(moduleRecord->importEntries().size() == moduleInformation.imports.size());

    {
        if (!!moduleInformation.memory && moduleInformation.memory.isImport()) {
            // We should either have a Memory import or we should have thrown an exception.
            RELEASE_ASSERT(hasMemoryImport);
        }

        if (moduleInformation.memory && !hasMemoryImport) {
            // We create a memory when it's a memory definition.
            RELEASE_ASSERT(!moduleInformation.memory.isImport());

            auto* jsMemory = JSWebAssemblyMemory::create(exec, vm, globalObject->WebAssemblyMemoryStructure());
            RETURN_IF_EXCEPTION(throwScope, nullptr);

            RefPtr<Wasm::Memory> memory = Wasm::Memory::tryCreate(moduleInformation.memory.initial(), moduleInformation.memory.maximum(),
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
    
    if (!jsInstance->memory()) {
        // Make sure we have a dummy memory, so that wasm -> wasm thunks avoid checking for a nullptr Memory when trying to set pinned registers.
        auto* jsMemory = JSWebAssemblyMemory::create(exec, vm, globalObject->WebAssemblyMemoryStructure());
        jsMemory->adopt(Wasm::Memory::create());
        jsInstance->setMemory(vm, jsMemory);
        RETURN_IF_EXCEPTION(throwScope, nullptr);
    }
    
    return jsInstance;
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
