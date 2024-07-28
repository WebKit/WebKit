/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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
#include "MacroAssemblerCodeRef.h"

#include "CodeBlock.h"
#include "Disassembler.h"
#include "JITCode.h"
#include "JSCPtrTag.h"
#include "WasmCompilationMode.h"
#include <wtf/StringPrintStream.h>

namespace JSC {

bool MacroAssemblerCodeRefBase::tryToDisassemble(CodePtr<DisassemblyPtrTag> codePtr, size_t size, const char* prefix, PrintStream& out)
{
    return JSC::tryToDisassemble(codePtr, size, prefix, out);
}

bool MacroAssemblerCodeRefBase::tryToDisassemble(CodePtr<DisassemblyPtrTag> codePtr, size_t size, const char* prefix)
{
    return tryToDisassemble(codePtr, size, prefix, WTF::dataFile());
}

CString MacroAssemblerCodeRefBase::disassembly(CodePtr<DisassemblyPtrTag> codePtr, size_t size)
{
    StringPrintStream out;
    if (!tryToDisassemble(codePtr, size, "", out))
        return CString();
    return out.toCString();
}

bool shouldDumpDisassemblyFor(CodeBlock* codeBlock)
{
    if (codeBlock && JSC::JITCode::isOptimizingJIT(codeBlock->jitType()) && Options::dumpDFGDisassembly())
        return true;
    return Options::dumpDisassembly();
}

bool shouldDumpDisassemblyFor(Wasm::CompilationMode mode)
{
    if (Options::asyncDisassembly() || Options::dumpDisassembly() || Options::dumpWasmDisassembly())
        return true;
    if (Wasm::isAnyBBQ(mode))
        return Options::dumpBBQDisassembly();
    if (Wasm::isAnyOMG(mode))
        return Options::dumpOMGDisassembly();
    return false;
}

} // namespace JSC

