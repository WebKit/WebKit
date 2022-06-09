/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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

#include "EvalCodeBlock.h"
#include "FunctionCodeBlock.h"
#include "JSCellInlines.h"
#include "ModuleProgramCodeBlock.h"
#include "NativeExecutable.h"
#include "ProgramCodeBlock.h"
#include <wtf/CommaPrinter.h>

namespace JSC {

const ClassInfo ExecutableBase::s_info = { "Executable", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(ExecutableBase) };

void ExecutableBase::destroy(JSCell* cell)
{
    static_cast<ExecutableBase*>(cell)->ExecutableBase::~ExecutableBase();
}

void ExecutableBase::dump(PrintStream& out) const
{
    ExecutableBase* realThis = const_cast<ExecutableBase*>(this);

    switch (type()) {
    case NativeExecutableType: {
        NativeExecutable* native = jsCast<NativeExecutable*>(realThis);
        out.print("NativeExecutable:", RawPointer(bitwise_cast<void*>(native->function())), "/", RawPointer(bitwise_cast<void*>(native->constructor())));
        return;
    }
    case EvalExecutableType: {
        EvalExecutable* eval = jsCast<EvalExecutable*>(realThis);
        if (CodeBlock* codeBlock = eval->codeBlock())
            out.print(*codeBlock);
        else
            out.print("EvalExecutable w/o CodeBlock");
        return;
    }
    case ProgramExecutableType: {
        ProgramExecutable* eval = jsCast<ProgramExecutable*>(realThis);
        if (CodeBlock* codeBlock = eval->codeBlock())
            out.print(*codeBlock);
        else
            out.print("ProgramExecutable w/o CodeBlock");
        return;
    }
    case ModuleProgramExecutableType: {
        ModuleProgramExecutable* executable = jsCast<ModuleProgramExecutable*>(realThis);
        if (CodeBlock* codeBlock = executable->codeBlock())
            out.print(*codeBlock);
        else
            out.print("ModuleProgramExecutable w/o CodeBlock");
        return;
    }
    case FunctionExecutableType: {
        FunctionExecutable* function = jsCast<FunctionExecutable*>(realThis);
        if (!function->eitherCodeBlock())
            out.print("FunctionExecutable w/o CodeBlock");
        else {
            CommaPrinter comma("/");
            if (function->codeBlockForCall())
                out.print(comma, *function->codeBlockForCall());
            if (function->codeBlockForConstruct())
                out.print(comma, *function->codeBlockForConstruct());
        }
        return;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

CodeBlockHash ExecutableBase::hashFor(CodeSpecializationKind kind) const
{
    if (type() == NativeExecutableType)
        return jsCast<const NativeExecutable*>(this)->hashFor(kind);
    
    return jsCast<const ScriptExecutable*>(this)->hashFor(kind);
}

} // namespace JSC
