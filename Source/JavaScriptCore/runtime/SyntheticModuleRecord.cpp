/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "SyntheticModuleRecord.h"

#include "BuiltinNames.h"
#include "JSCInlines.h"
#include "JSInternalPromise.h"
#include "JSModuleEnvironment.h"
#include "JSModuleNamespaceObject.h"
#include "JSONObject.h"

namespace JSC {

const ClassInfo SyntheticModuleRecord::s_info = { "ModuleRecord"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(SyntheticModuleRecord) };


Structure* SyntheticModuleRecord::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

SyntheticModuleRecord* SyntheticModuleRecord::create(JSGlobalObject* globalObject, VM& vm, Structure* structure, const Identifier& moduleKey)
{
    SyntheticModuleRecord* instance = new (NotNull, allocateCell<SyntheticModuleRecord>(vm)) SyntheticModuleRecord(vm, structure, moduleKey);
    instance->finishCreation(globalObject, vm);
    return instance;
}

SyntheticModuleRecord::SyntheticModuleRecord(VM& vm, Structure* structure, const Identifier& moduleKey)
    : Base(vm, structure, moduleKey)
{
}

void SyntheticModuleRecord::destroy(JSCell* cell)
{
    SyntheticModuleRecord* thisObject = static_cast<SyntheticModuleRecord*>(cell);
    thisObject->SyntheticModuleRecord::~SyntheticModuleRecord();
}

void SyntheticModuleRecord::finishCreation(JSGlobalObject* globalObject, VM& vm)
{
    Base::finishCreation(globalObject, vm);
    ASSERT(inherits(info()));
}

template<typename Visitor>
void SyntheticModuleRecord::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    SyntheticModuleRecord* thisObject = jsCast<SyntheticModuleRecord*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(SyntheticModuleRecord);

Synchronousness SyntheticModuleRecord::link(JSGlobalObject*, JSValue)
{
    return Synchronousness::Sync;
}

JSValue SyntheticModuleRecord::evaluate(JSGlobalObject*)
{
    return jsUndefined();
}


SyntheticModuleRecord* SyntheticModuleRecord::tryCreateWithExportNamesAndValues(JSGlobalObject* globalObject, const Identifier& moduleKey, const Vector<Identifier, 4>& exportNames, const MarkedArgumentBuffer& exportValues)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(exportNames.size() == exportValues.size());

    auto* moduleRecord = create(globalObject, vm, globalObject->syntheticModuleRecordStructure(), moduleKey);

    SymbolTable* exportSymbolTable = SymbolTable::create(vm);
    {
        auto offset = exportSymbolTable->takeNextScopeOffset(NoLockingNecessary);
        exportSymbolTable->set(NoLockingNecessary, vm.propertyNames->starNamespacePrivateName.impl(), SymbolTableEntry(VarOffset(offset)));
    }
    for (auto& exportName : exportNames) {
        auto offset = exportSymbolTable->takeNextScopeOffset(NoLockingNecessary);
        exportSymbolTable->set(NoLockingNecessary, exportName.impl(), SymbolTableEntry(VarOffset(offset)));
        moduleRecord->addExportEntry(ExportEntry::createLocal(exportName, exportName));
    }

    JSModuleEnvironment* moduleEnvironment = JSModuleEnvironment::create(vm, globalObject, nullptr, exportSymbolTable, jsTDZValue(), moduleRecord);
    moduleRecord->setModuleEnvironment(globalObject, moduleEnvironment);
    RETURN_IF_EXCEPTION(scope, { });

    for (unsigned index = 0; index < exportNames.size(); ++index) {
        PropertyName exportName = exportNames[index];
        JSValue exportValue = exportValues.at(index);
        constexpr bool shouldThrowReadOnlyError = false;
        constexpr bool ignoreReadOnlyErrors = true;
        bool putResult = false;
        symbolTablePutTouchWatchpointSet(moduleEnvironment, globalObject, exportName, exportValue, shouldThrowReadOnlyError, ignoreReadOnlyErrors, putResult);
        RETURN_IF_EXCEPTION(scope, { });
        ASSERT(putResult);
    }

    return moduleRecord;

}

SyntheticModuleRecord* SyntheticModuleRecord::tryCreateDefaultExportSyntheticModule(JSGlobalObject* globalObject, const Identifier& moduleKey, JSValue defaultExport)
{
    VM& vm = globalObject->vm();

    Vector<Identifier, 4> exportNames;
    MarkedArgumentBuffer exportValues;

    exportNames.append(vm.propertyNames->defaultKeyword);
    exportValues.appendWithCrashOnOverflow(defaultExport);

    return tryCreateWithExportNamesAndValues(globalObject, moduleKey, exportNames, exportValues);
}

SyntheticModuleRecord* SyntheticModuleRecord::parseJSONModule(JSGlobalObject* globalObject, const Identifier& moduleKey, SourceCode&& sourceCode)
{
    // https://tc39.es/proposal-json-modules/#sec-parse-json-module
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue result = JSONParseWithException(globalObject, sourceCode.view());
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, SyntheticModuleRecord::tryCreateDefaultExportSyntheticModule(globalObject, moduleKey, result));
}

} // namespace JSC
