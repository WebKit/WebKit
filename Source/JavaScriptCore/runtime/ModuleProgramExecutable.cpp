/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
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

#include "CodeCache.h"
#include "Debugger.h"

namespace JSC {

const ClassInfo ModuleProgramExecutable::s_info = { "ModuleProgramExecutable"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ModuleProgramExecutable) };

ModuleProgramExecutable::ModuleProgramExecutable(JSGlobalObject* globalObject, const SourceCode& source)
    : Base(globalObject->vm().moduleProgramExecutableStructure.get(), globalObject->vm(), source, false, DerivedContextType::None, false, false, EvalContextType::None, NoIntrinsic)
{
    ASSERT(source.provider()->sourceType() == SourceProviderSourceType::Module);
    VM& vm = globalObject->vm();
    if (vm.typeProfiler() || vm.controlFlowProfiler())
        vm.functionHasExecutedCache()->insertUnexecutedRange(sourceID(), typeProfilingStartOffset(), typeProfilingEndOffset());
}


UnlinkedModuleProgramCodeBlock* ModuleProgramExecutable::getUnlinkedCodeBlock(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    UnlinkedModuleProgramCodeBlock* unlinkedModuleProgramCode = unlinkedCodeBlock();
    if (unlinkedModuleProgramCode)
        RELEASE_AND_RETURN(throwScope, unlinkedModuleProgramCode);

    ParserError error;
    OptionSet<CodeGenerationMode> codeGenerationMode = globalObject->defaultCodeGenerationMode();
    unlinkedModuleProgramCode = vm.codeCache()->getUnlinkedModuleProgramCodeBlock(vm, this, source(), codeGenerationMode, error);

    if (globalObject->hasDebugger())
        globalObject->debugger()->sourceParsed(globalObject, source().provider(), error.line(), error.message());

    if (error.isValid()) {
        throwVMError(globalObject, throwScope, error.toErrorObject(globalObject, source()));
        return nullptr;
    }

    m_unlinkedCodeBlock.set(vm, this, unlinkedModuleProgramCode);
    VirtualRegister symbolTableReg = VirtualRegister(unlinkedModuleProgramCode->moduleEnvironmentSymbolTableConstantRegisterOffset());
    SymbolTable* symbolTable = jsCast<SymbolTable*>(unlinkedModuleProgramCode->getConstant(symbolTableReg));
    m_moduleEnvironmentSymbolTable.set(vm, this, symbolTable->cloneScopePart(vm));
    RELEASE_AND_RETURN(throwScope, unlinkedModuleProgramCode);
}

ModuleProgramExecutable* ModuleProgramExecutable::create(JSGlobalObject* globalObject, const SourceCode& source)
{
    VM& vm = globalObject->vm();
    ModuleProgramExecutable* executable = new (NotNull, allocateCell<ModuleProgramExecutable>(vm)) ModuleProgramExecutable(globalObject, source);
    executable->finishCreation(vm);
    executable->getUnlinkedCodeBlock(globalObject); // This generates and binds unlinked code block.
    return executable;
}

void ModuleProgramExecutable::destroy(JSCell* cell)
{
    static_cast<ModuleProgramExecutable*>(cell)->ModuleProgramExecutable::~ModuleProgramExecutable();
}

auto ModuleProgramExecutable::ensureTemplateObjectMap(VM&) -> TemplateObjectMap&
{
    return ensureTemplateObjectMapImpl(m_templateObjectMap);
}

template<typename Visitor>
void ModuleProgramExecutable::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    ModuleProgramExecutable* thisObject = jsCast<ModuleProgramExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_moduleEnvironmentSymbolTable);
    if (TemplateObjectMap* map = thisObject->m_templateObjectMap.get()) {
        Locker locker { thisObject->cellLock() };
        for (auto& entry : *map)
            visitor.append(entry.value);
    }
}

DEFINE_VISIT_CHILDREN(ModuleProgramExecutable);

} // namespace JSC
