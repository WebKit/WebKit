/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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
#include "JSModuleNamespaceObject.h"
#include "JSWebAssemblyCompileError.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyLinkError.h"
#include "JSWebAssemblyMemory.h"
#include "JSWebAssemblyModule.h"
#include "Register.h"
#include "WasmConstExprGenerator.h"
#include "WasmModuleInformation.h"
#include "WasmTag.h"
#include "WasmTypeDefinitionInlines.h"
#include "WebAssemblyModuleRecord.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>

namespace JSC {

using namespace Wasm;

const ClassInfo JSWebAssemblyInstance::s_info = { "WebAssembly.Instance"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyInstance) };

Structure* JSWebAssemblyInstance::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(WebAssemblyInstanceType, StructureFlags), info());
}

JSWebAssemblyInstance::JSWebAssemblyInstance(VM& vm, Structure* structure, JSWebAssemblyModule* module, WebAssemblyModuleRecord* moduleRecord)
    : Base(vm, structure)
    , m_vm(&vm)
    , m_jsModule(module, WriteBarrierEarlyInit)
    , m_moduleRecord(moduleRecord, WriteBarrierEarlyInit)
    , m_tables(module->module().moduleInformation().tableCount())
    , m_softStackLimit(vm.softStackLimit())
    , m_module(module->module())
    , m_globalsToMark(m_module.get().moduleInformation().globalCount())
    , m_globalsToBinding(m_module.get().moduleInformation().globalCount())
    , m_numImportFunctions(m_module->moduleInformation().importFunctionCount())
    , m_passiveElements(m_module->moduleInformation().elementCount())
    , m_passiveDataSegments(m_module->moduleInformation().dataSegmentsCount())
    , m_tags(m_module->moduleInformation().exceptionIndexSpaceSize())
{
    static_assert(static_cast<ptrdiff_t>(JSWebAssemblyInstance::offsetOfCachedMemory() + sizeof(void*)) == JSWebAssemblyInstance::offsetOfCachedBoundsCheckingSize());
    for (unsigned i = 0; i < m_numImportFunctions; ++i) {
        auto* info = new (importFunctionInfo(i)) ImportFunctionInfo();
        info->callLinkInfo.initialize(vm, nullptr, CallLinkInfo::CallType::Call, CodeOrigin { });
    }
    m_globals = bitwise_cast<Global::Value*>(bitwise_cast<char*>(this) + offsetOfGlobalPtr(m_numImportFunctions, m_module->moduleInformation().tableCount(), 0));
    memset(bitwise_cast<char*>(m_globals), 0, m_module->moduleInformation().globalCount() * sizeof(Global::Value));
    for (unsigned i = 0; i < m_module->moduleInformation().globals.size(); ++i) {
        const GlobalInformation& global = m_module.get().moduleInformation().globals[i];
        if (global.bindingMode == GlobalInformation::BindingMode::Portable) {
            // This is kept alive by JSWebAssemblyInstance -> JSWebAssemblyGlobal -> binding.
            m_globalsToBinding.set(i);
        } else if (isRefType(global.type)) {
            // This is kept alive by JSWebAssemblyInstance -> binding.
            m_globalsToMark.set(i);
        }
    }
    memset(bitwise_cast<char*>(this) + offsetOfTablePtr(m_numImportFunctions, 0), 0, m_module->moduleInformation().tableCount() * sizeof(Table*));
    for (unsigned elementIndex = 0; elementIndex < m_module->moduleInformation().elementCount(); ++elementIndex) {
        const auto& element = m_module->moduleInformation().elements[elementIndex];
        if (element.isPassive())
            m_passiveElements.quickSet(elementIndex);
    }

    for (unsigned dataSegmentIndex = 0; dataSegmentIndex < m_module->moduleInformation().dataSegmentsCount(); ++dataSegmentIndex) {
        const auto& dataSegment = m_module->moduleInformation().data[dataSegmentIndex];
        if (dataSegment->isPassive())
            m_passiveDataSegments.quickSet(dataSegmentIndex);
    }
}

void JSWebAssemblyInstance::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    vm.heap.reportExtraMemoryAllocated(this, extraMemoryAllocated());
}

JSWebAssemblyInstance::~JSWebAssemblyInstance()
{
    clearJSCallICs(*m_vm);
    for (unsigned i = 0; i < m_numImportFunctions; ++i)
        importFunctionInfo(i)->~ImportFunctionInfo();
}

void JSWebAssemblyInstance::destroy(JSCell* cell)
{
    static_cast<JSWebAssemblyInstance*>(cell)->JSWebAssemblyInstance::~JSWebAssemblyInstance();
}

template<typename Visitor>
void JSWebAssemblyInstance::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSWebAssemblyInstance*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_jsModule);
    visitor.append(thisObject->m_moduleRecord);
    visitor.append(thisObject->m_memory);
    for (auto& table : thisObject->m_tables)
        visitor.append(table);
    visitor.reportExtraMemoryVisited(thisObject->extraMemoryAllocated());
    for (unsigned i = 0; i < thisObject->numImportFunctions(); ++i)
        visitor.append(thisObject->importFunction(i)); // This also keeps the functions' JSWebAssemblyInstance alive.

    for (size_t i : thisObject->globalsToBinding()) {
        Global* binding = thisObject->getGlobalBinding(i);
        if (binding)
            visitor.appendUnbarriered(binding->owner());
    }
    for (size_t i : thisObject->globalsToMark())
        visitor.appendUnbarriered(JSValue::decode(thisObject->loadI64Global(i)));

    Locker locker { cell->cellLock() };
    for (auto& wrapper : thisObject->functionWrappers())
        visitor.appendUnbarriered(wrapper.get());
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyInstance);

void JSWebAssemblyInstance::initializeImports(JSGlobalObject* globalObject, JSObject* importObject, CreationMode creationMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    m_moduleRecord->prepareLink(vm, this);
    if (creationMode == CreationMode::FromJS) {
        m_moduleRecord->link(globalObject, jsNull());
        RETURN_IF_EXCEPTION(scope, void());
        m_moduleRecord->initializeImports(globalObject, importObject, creationMode);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

void JSWebAssemblyInstance::finalizeCreation(VM& vm, JSGlobalObject* globalObject, Ref<CalleeGroup>&& wasmCalleeGroup, CreationMode creationMode)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!wasmCalleeGroup->runnable()) {
        throwException(globalObject, scope, createJSWebAssemblyLinkError(globalObject, vm, wasmCalleeGroup->errorMessage()));
        return;
    }

    RELEASE_ASSERT(wasmCalleeGroup->isSafeToRun(memoryMode()));

    // When memory is imported, we will initialize all memory modes with the initial LLInt compilation
    // results, so that later when memory imports become available, the appropriate CalleeGroup can be used.
    // If LLInt is disabled, we instead defer compilation to module evaluation.
    // If the code is already compiled, e.g. the module was already instantiated before, we do not re-initialize.
    if (Options::useWasmLLInt() && module().moduleInformation().hasMemoryImport())
        module().copyInitialCalleeGroupToAllMemoryModes(memoryMode());

    for (unsigned importFunctionNum = 0; importFunctionNum < numImportFunctions(); ++importFunctionNum) {
        auto* info = importFunctionInfo(importFunctionNum);
        if (!info->targetInstance) {
            info->importFunctionStub = module().importFunctionStub(importFunctionNum);
            importCallees.append(adoptRef(*new WasmToJSCallee(importFunctionNum, { nullptr, nullptr })));
            info->boxedTargetCalleeLoadLocation = reinterpret_cast<const uintptr_t*>(importCallees.last().ptr());
        }
        else
            info->importFunctionStub = wasmCalleeGroup->wasmToWasmExitStub(importFunctionNum);
    }

    if (creationMode == CreationMode::FromJS) {
        m_moduleRecord->initializeExports(globalObject);
        RETURN_IF_EXCEPTION(scope, void());

        JSValue startResult = m_moduleRecord->evaluate(globalObject);
        UNUSED_PARAM(startResult);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

Identifier JSWebAssemblyInstance::createPrivateModuleKey()
{
    return Identifier::fromUid(PrivateName(PrivateName::Description, "WebAssemblyInstance"_s));
}

JSWebAssemblyInstance* JSWebAssemblyInstance::tryCreate(VM& vm, Structure* instanceStructure, JSGlobalObject* globalObject, const Identifier& moduleKey, JSWebAssemblyModule* jsModule, JSObject* importObject, CreationMode creationMode)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    const ModuleInformation& moduleInformation = jsModule->moduleInformation();

    auto exception = [&] (JSObject* error) {
        throwException(globalObject, throwScope, error);
        return nullptr;
    };

    // Disabled by CSP: https://w3c.github.io/webappsec-csp/#can-compile-wasm-bytes
    if (!globalObject->webAssemblyEnabled())
        return exception(createJSWebAssemblyCompileError(globalObject, vm, globalObject->webAssemblyDisabledErrorMessage()));

    WebAssemblyModuleRecord* moduleRecord = WebAssemblyModuleRecord::create(globalObject, vm, globalObject->webAssemblyModuleRecordStructure(), moduleKey, moduleInformation);
    RETURN_IF_EXCEPTION(throwScope, nullptr);

    // FIXME: These objects could be pretty big we should try to throw OOM here.
    void* cell = tryAllocateCell<JSWebAssemblyInstance>(vm, allocationSize(moduleInformation.importFunctionCount(), moduleInformation.tableCount(), moduleInformation.globalCount()));
    if (!cell) {
        throwOutOfMemoryError(globalObject, throwScope);
        return nullptr;
    }

    auto* jsInstance = new (NotNull, cell) JSWebAssemblyInstance(vm, instanceStructure, jsModule, moduleRecord);
    jsInstance->finishCreation(vm);
    RETURN_IF_EXCEPTION(throwScope, nullptr);

    if (creationMode == CreationMode::FromJS) {
        // If the list of module.imports is not empty and Type(importObject) is not Object, a TypeError is thrown.
        if (moduleInformation.imports.size() && !importObject)
            return exception(createTypeError(globalObject, "can't make WebAssembly.Instance because there is no imports Object and the WebAssembly.Module requires imports"_s));
    }

    // For each import i in module.imports:
    {
        IdentifierSet specifiers;
        for (auto& import : moduleInformation.imports) {
            auto moduleName = Identifier::fromString(vm, makeAtomString(import.module));
            auto fieldName = Identifier::fromString(vm, makeAtomString(import.field));
            auto result = specifiers.add(moduleName.impl());
            if (result.isNewEntry)
                moduleRecord->appendRequestedModule(moduleName, nullptr);
            moduleRecord->addImportEntry(WebAssemblyModuleRecord::ImportEntry {
                WebAssemblyModuleRecord::ImportEntryType::Single,
                moduleName,
                fieldName,
                Identifier::fromUid(PrivateName(PrivateName::Description, "WebAssemblyImportName"_s)),
            });
        }
        ASSERT(moduleRecord->importEntries().size() == moduleInformation.imports.size());
    }

    bool hasMemoryImport = moduleInformation.memory.isImport();
    if (moduleInformation.memory && !hasMemoryImport) {
        // We create a memory when it's a memory definition.
        auto* jsMemory = JSWebAssemblyMemory::tryCreate(globalObject, vm, globalObject->webAssemblyMemoryStructure());
        RETURN_IF_EXCEPTION(throwScope, nullptr);

        RefPtr<Memory> memory = Memory::tryCreate(vm, moduleInformation.memory.initial(), moduleInformation.memory.maximum(), moduleInformation.memory.isShared() ? MemorySharingMode::Shared: MemorySharingMode::Default,
            [&vm, jsMemory](Memory::GrowSuccess, PageCount oldPageCount, PageCount newPageCount) { jsMemory->growSuccessCallback(vm, oldPageCount, newPageCount); }
        );
        if (!memory)
            return exception(createOutOfMemoryError(globalObject));

        jsMemory->adopt(memory.releaseNonNull());
        jsInstance->setMemory(vm, jsMemory);
        RETURN_IF_EXCEPTION(throwScope, nullptr);
    }

    if (!jsInstance->memory()) {
        // Make sure we have a dummy memory, so that wasm -> wasm thunks avoid checking for a nullptr Memory when trying to set pinned registers.
        // When there is a memory import, this will be replaced later in the module record import initialization.
        auto* jsMemory = JSWebAssemblyMemory::tryCreate(globalObject, vm, globalObject->webAssemblyMemoryStructure());
        RETURN_IF_EXCEPTION(throwScope, nullptr);
        jsMemory->adopt(Memory::create(vm));
        jsInstance->setMemory(vm, jsMemory);
        RETURN_IF_EXCEPTION(throwScope, nullptr);
    }
    
    return jsInstance;
}

void JSWebAssemblyInstance::clearJSCallICs(VM& vm)
{
    for (unsigned index = 0; index < numImportFunctions(); ++index) {
        auto* info = importFunctionInfo(index);
        info->callLinkInfo.unlinkOrUpgrade(vm, nullptr, nullptr);
    }
}

void JSWebAssemblyInstance::finalizeUnconditionally(VM& vm, CollectionScope)
{
    for (unsigned index = 0; index < numImportFunctions(); ++index) {
        auto* info = importFunctionInfo(index);
        info->callLinkInfo.visitWeak(vm);
    }
}

size_t JSWebAssemblyInstance::extraMemoryAllocated() const
{
    return allocationSize(m_numImportFunctions, m_module->moduleInformation().tableCount(), m_module->moduleInformation().globalCount());
}

void JSWebAssemblyInstance::setGlobal(unsigned i, JSValue value)
{
    Global::Value& slot = m_globals[i];
    if (m_globalsToBinding.get(i)) {
        Global* global = getGlobalBinding(i);
        if (!global)
            return;
        global->valuePointer()->m_externref.set(vm(), global->owner(), value);
        return;
    }
    slot.m_externref.set(vm(), this, value);
}

JSValue JSWebAssemblyInstance::getFunctionWrapper(unsigned i) const
{
    JSValue value = m_functionWrappers.get(i).get();
    if (value.isEmpty())
        return jsNull();
    return value;
}

void JSWebAssemblyInstance::setFunctionWrapper(unsigned i, JSValue value)
{
    ASSERT(value.isCallable());
    ASSERT(!m_functionWrappers.contains(i));
    Locker locker { cellLock() };
    m_functionWrappers.set(i, WriteBarrier<Unknown>(vm(), this, value));
    ASSERT(getFunctionWrapper(i) == value);
}

Table* JSWebAssemblyInstance::table(unsigned i)
{
    RELEASE_ASSERT(i < m_module->moduleInformation().tableCount());
    return *bitwise_cast<Table**>(bitwise_cast<char*>(this) + offsetOfTablePtr(m_numImportFunctions, i));
}

void JSWebAssemblyInstance::tableCopy(uint32_t dstOffset, uint32_t srcOffset, uint32_t length, uint32_t dstTableIndex, uint32_t srcTableIndex)
{
    RELEASE_ASSERT(srcTableIndex < m_module->moduleInformation().tableCount());
    RELEASE_ASSERT(dstTableIndex < m_module->moduleInformation().tableCount());

    Table* dstTable = table(dstTableIndex);
    Table* srcTable = table(srcTableIndex);
    RELEASE_ASSERT(dstTable->type() == srcTable->type());

    auto forEachTableElement = [&](auto fn) {
        if (dstTableIndex == srcTableIndex && dstOffset > srcOffset) {
            for (uint32_t index = length; index--;)
                fn(dstTable, srcTable, dstOffset + index, srcOffset + index);
        } else if (dstTableIndex == srcTableIndex && dstOffset == srcOffset)
            return;
        else {
            for (uint32_t index = 0; index < length; ++index)
                fn(dstTable, srcTable, dstOffset + index, srcOffset + index);
        }
    };

    if (dstTable->isExternrefTable()) {
        forEachTableElement([](Table* dstTable, Table* srcTable, uint32_t dstIndex, uint32_t srcIndex) {
            dstTable->copy(srcTable, dstIndex, srcIndex);
        });
        return;
    }

    forEachTableElement([](Table* dstTable, Table* srcTable, uint32_t dstIndex, uint32_t srcIndex) {
        dstTable->asFuncrefTable()->copyFunction(srcTable->asFuncrefTable(), dstIndex, srcIndex);
    });
}

void JSWebAssemblyInstance::elemDrop(uint32_t elementIndex)
{
    m_passiveElements.quickClear(elementIndex);
}

bool JSWebAssemblyInstance::memoryInit(uint32_t dstAddress, uint32_t srcAddress, uint32_t length, uint32_t dataSegmentIndex)
{
    RELEASE_ASSERT(dataSegmentIndex < module().moduleInformation().dataSegmentsCount());

    if (sumOverflows<uint32_t>(srcAddress, length))
        return false;

    const Segment::Ptr& segment = module().moduleInformation().data[dataSegmentIndex];
    const uint32_t segmentSizeInBytes = m_passiveDataSegments.quickGet(dataSegmentIndex) ? segment->sizeInBytes : 0U;
    if (srcAddress + length > segmentSizeInBytes)
        return false;

    const uint8_t* segmentData = !length ? nullptr : &segment->byte(srcAddress);

    ASSERT(memory());
    return memory()->memory().init(dstAddress, segmentData, length);
}

void JSWebAssemblyInstance::dataDrop(uint32_t dataSegmentIndex)
{
    m_passiveDataSegments.quickClear(dataSegmentIndex);
}

const Element* JSWebAssemblyInstance::elementAt(unsigned index) const
{
    RELEASE_ASSERT(index < m_module->moduleInformation().elementCount());

    if (m_passiveElements.quickGet(index))
        return &m_module->moduleInformation().elements[index];
    return nullptr;
}

void JSWebAssemblyInstance::initElementSegment(uint32_t tableIndex, const Element& segment, uint32_t dstOffset, uint32_t srcOffset, uint32_t length)
{
    RELEASE_ASSERT(length <= segment.length());

    JSWebAssemblyTable* jsTable = this->jsTable(tableIndex);
    JSGlobalObject* globalObject = this->globalObject();
    VM& vm = globalObject->vm();

    for (uint32_t index = 0; index < length; ++index) {
        const auto srcIndex = srcOffset + index;
        const auto dstIndex = dstOffset + index;
        const auto initType = segment.initTypes[srcIndex];
        const auto initialBitsOrIndex = segment.initialBitsOrIndices[srcIndex];

        if (initType == Element::InitializationType::FromRefNull) {
            jsTable->clear(dstIndex);
            continue;
        }

        if (initType == Element::InitializationType::FromRefFunc) {
            // FIXME: This essentially means we're exporting an import.
            // We need a story here. We need to create a WebAssemblyFunction
            // for the import.
            // https://bugs.webkit.org/show_bug.cgi?id=165510
            uint32_t functionIndex = static_cast<uint32_t>(initialBitsOrIndex);
            TypeIndex typeIndex = m_module->typeIndexFromFunctionIndexSpace(functionIndex);
            if (isImportFunction(functionIndex)) {
                JSObject* functionImport = importFunction(functionIndex).get();
                if (isWebAssemblyHostFunction(functionImport)) {
                    WebAssemblyFunction* wasmFunction = jsDynamicCast<WebAssemblyFunction*>(functionImport);
                    // If we ever import a WebAssemblyWrapperFunction, we set the import as the unwrapped value.
                    // Because a WebAssemblyWrapperFunction can never wrap another WebAssemblyWrapperFunction,
                    // the only type this could be is WebAssemblyFunction.
                    RELEASE_ASSERT(wasmFunction);
                    jsTable->set(dstIndex, wasmFunction);
                    continue;
                }
                auto* wrapperFunction = WebAssemblyWrapperFunction::create(
                    vm,
                    globalObject,
                    globalObject->webAssemblyWrapperFunctionStructure(),
                    functionImport,
                    functionIndex,
                    this,
                    typeIndex,
                    TypeInformation::getCanonicalRTT(typeIndex));
                jsTable->set(dstIndex, wrapperFunction);
                continue;
            }

            auto& jsEntrypointCallee = calleeGroup()->jsEntrypointCalleeFromFunctionIndexSpace(functionIndex);
            auto* wasmCallee = calleeGroup()->wasmCalleeFromFunctionIndexSpace(functionIndex);
            WasmToWasmImportableFunction::LoadLocation entrypointLoadLocation = calleeGroup()->entrypointLoadLocationFromFunctionIndexSpace(functionIndex);
            const auto& signature = TypeInformation::getFunctionSignature(typeIndex);
            // FIXME: Say we export local function "foo" at function index 0.
            // What if we also set it to the table an Element w/ index 0.
            // Does (new Instance(...)).exports.foo === table.get(0)?
            // https://bugs.webkit.org/show_bug.cgi?id=165825
            WebAssemblyFunction* function = WebAssemblyFunction::create(
                vm,
                globalObject,
                globalObject->webAssemblyFunctionStructure(),
                signature.argumentCount(),
                WTF::makeString(functionIndex),
                this,
                jsEntrypointCallee,
                wasmCallee,
                entrypointLoadLocation,
                typeIndex,
                TypeInformation::getCanonicalRTT(typeIndex));
            jsTable->set(dstIndex, function);
            continue;
        }

        JSValue initValue;
        if (initType == Element::InitializationType::FromGlobal)
            initValue = JSValue::decode(loadI64Global(initialBitsOrIndex));
        else {
            ASSERT(initType == Element::InitializationType::FromExtendedExpression);
            uint64_t result;
            bool success = evaluateConstantExpression(initialBitsOrIndex, segment.elementType, result);
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=264454
            // Currently this should never fail, as the parse phase already validated it.
            RELEASE_ASSERT(success);
            initValue = JSValue::decode(result);
        }

        if (jsTable->table()->isExternrefTable())
            jsTable->set(dstIndex, initValue);
        else {
            // Validation should guarantee that the table is for funcs, and the value is a func as well.
            ASSERT(jsTable->table()->isFuncrefTable());
            ASSERT(initValue.getObject());
            WebAssemblyFunctionBase* func = jsDynamicCast<WebAssemblyFunctionBase*>(initValue.getObject());
            ASSERT(func);
            jsTable->set(dstIndex, func);
        }
    }
}

bool JSWebAssemblyInstance::copyDataSegment(uint32_t segmentIndex, uint32_t offset, uint32_t lengthInBytes, uint8_t* values)
{
    // Fail if the data segment index is out of bounds
    RELEASE_ASSERT(segmentIndex < module().moduleInformation().dataSegmentsCount());
    // Otherwise, get the `segmentIndex`th data segment
    const Segment::Ptr& segment = module().moduleInformation().data[segmentIndex];
    const uint32_t segmentSizeInBytes = m_passiveDataSegments.quickGet(segmentIndex) ? segment->sizeInBytes : 0U;

    // Caller checks that the (offset + lengthInBytes) calculation doesn't overflow
    if ((offset + lengthInBytes) > segmentSizeInBytes) {
        // The segment access would overflow; the caller must handle this error.
        return false;
    }
    // If size is 0, do nothing
    if (!lengthInBytes)
        return true;
    // Cast the data segment to a pointer
    const uint8_t* segmentData = &segment->byte(offset);

    // Copy the data from the segment into the out param vector
    memcpy(values, segmentData, lengthInBytes);

    return true;
}

void JSWebAssemblyInstance::copyElementSegment(const Element& segment, uint32_t srcOffset, uint32_t length, uint64_t* values)
{
    // Caller should have already checked that the (offset + length) calculation doesn't overflow int32,
    // and that the (offset + length) doesn't overflow the element segment
    ASSERT(!sumOverflows<uint32_t>(srcOffset, length));
    ASSERT((srcOffset + length) <= segment.length());

    for (uint32_t i = 0; i < length; i++) {
        uint32_t srcIndex = srcOffset + i;
        const auto initType = segment.initTypes[srcIndex];
        const auto initialBitsOrIndex = segment.initialBitsOrIndices[srcIndex];

        // Represent the null function as the null JS value
        if (initType == Element::InitializationType::FromRefNull) {
            values[i] = static_cast<uint64_t>(JSValue::encode(jsNull()));
            continue;
        }

        if (initType == Element::InitializationType::FromRefFunc) {
            uint32_t functionIndex = static_cast<uint32_t>(initialBitsOrIndex);

            // A wrapper for this function should have been created during parsing.
            // A future optimization would be for the parser to not create the wrappers,
            // and create them here dynamically instead.
            JSValue value = getFunctionWrapper(functionIndex);
            ASSERT(value.isCallable());
            values[i] = static_cast<uint64_t>(JSValue::encode(value));
            continue;
        }

        if (initType == Element::InitializationType::FromGlobal) {
            values[i] = loadI64Global(initialBitsOrIndex);
            continue;
        }

        ASSERT(initType == Element::InitializationType::FromExtendedExpression);
        uint64_t result;
        bool success = evaluateConstantExpression(initialBitsOrIndex, segment.elementType, result);
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=264454
        // Currently this should never fail, as the parse phase already validated it.
        RELEASE_ASSERT(success);
        values[i] = result;
    }
}

bool JSWebAssemblyInstance::evaluateConstantExpression(uint64_t index, Type expectedType, uint64_t& result)
{
    const auto& constantExpression = m_module->moduleInformation().constantExpressions[index];
    auto evalResult = evaluateExtendedConstExpr(constantExpression, this, m_module->moduleInformation(), expectedType);
    if (UNLIKELY(!evalResult.has_value()))
        return false;

    result = evalResult.value();
    return true;
}

void JSWebAssemblyInstance::tableInit(uint32_t dstOffset, uint32_t srcOffset, uint32_t length, uint32_t elementIndex, uint32_t tableIndex)
{
    RELEASE_ASSERT(elementIndex < m_module->moduleInformation().elementCount());
    RELEASE_ASSERT(tableIndex < m_module->moduleInformation().tableCount());

    const Element* elementSegment = elementAt(elementIndex);
    RELEASE_ASSERT(elementSegment);
    RELEASE_ASSERT(elementSegment->isPassive());
    initElementSegment(tableIndex, *elementSegment, dstOffset, srcOffset, length);
}

void JSWebAssemblyInstance::setTable(unsigned i, Ref<Table>&& table)
{
    RELEASE_ASSERT(i < m_module->moduleInformation().tableCount());
    ASSERT(!this->table(i));
    *bitwise_cast<Table**>(bitwise_cast<char*>(this) + offsetOfTablePtr(m_numImportFunctions, i)) = &table.leakRef();
}

void JSWebAssemblyInstance::linkGlobal(unsigned i, Ref<Global>&& global)
{
    m_globals[i].m_pointer = global->valuePointer();
    m_linkedGlobals.set(i, WTFMove(global));
}

void JSWebAssemblyInstance::setTag(unsigned index, Ref<const Tag>&& tag)
{
    m_tags[index] = WTFMove(tag);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
