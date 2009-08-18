/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "Executable.h"

#include "BytecodeGenerator.h"
#include "CodeBlock.h"
#include "JIT.h"
#include "Parser.h"

namespace JSC {

EvalExecutable::~EvalExecutable()
{
    delete m_evalCodeBlock;
}

ProgramExecutable::~ProgramExecutable()
{
    delete m_programCodeBlock;
}

FunctionExecutable::~FunctionExecutable()
{
    delete m_codeBlock;
}

void EvalExecutable::generateBytecode(ScopeChainNode* scopeChainNode)
{
    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    ASSERT(!m_evalCodeBlock);
    m_evalCodeBlock = new EvalCodeBlock(this, globalObject, source().provider(), scopeChain.localDepth());
    OwnPtr<BytecodeGenerator> generator(new BytecodeGenerator(evalNode(), globalObject->debugger(), scopeChain, &m_evalCodeBlock->symbolTable(), m_evalCodeBlock));
    generator->generate();
    
    evalNode()->partialDestroyData();
}

void ProgramExecutable::generateBytecode(ScopeChainNode* scopeChainNode)
{
    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();
    
    ASSERT(!m_programCodeBlock);
    m_programCodeBlock = new ProgramCodeBlock(this, GlobalCode, globalObject, source().provider());
    OwnPtr<BytecodeGenerator> generator(new BytecodeGenerator(programNode(), globalObject->debugger(), scopeChain, &globalObject->symbolTable(), m_programCodeBlock));
    generator->generate();

    programNode()->destroyData();
}

void FunctionExecutable::generateBytecode(ScopeChainNode* scopeChainNode)
{
    body()->reparseDataIfNecessary(scopeChainNode);

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    ASSERT(!m_codeBlock);
    m_codeBlock = new FunctionCodeBlock(this, FunctionCode, source().provider(), source().startOffset());
    OwnPtr<BytecodeGenerator> generator(new BytecodeGenerator(body(), globalObject->debugger(), scopeChain, &m_codeBlock->symbolTable(), m_codeBlock));
    generator->generate();

    body()->destroyData();
}

#if ENABLE(JIT)

void EvalExecutable::generateJITCode(ScopeChainNode* scopeChainNode)
{
    CodeBlock* codeBlock = &bytecode(scopeChainNode);
    m_jitCode = JIT::compile(scopeChainNode->globalData, codeBlock);

#if !ENABLE(OPCODE_SAMPLING)
    if (!BytecodeGenerator::dumpsGeneratedCode())
        codeBlock->discardBytecode();
#endif
}

void ProgramExecutable::generateJITCode(ScopeChainNode* scopeChainNode)
{
    CodeBlock* codeBlock = &bytecode(scopeChainNode);
    m_jitCode = JIT::compile(scopeChainNode->globalData, codeBlock);

#if !ENABLE(OPCODE_SAMPLING)
    if (!BytecodeGenerator::dumpsGeneratedCode())
        codeBlock->discardBytecode();
#endif
}

void FunctionExecutable::generateJITCode(ScopeChainNode* scopeChainNode)
{
    CodeBlock* codeBlock = &bytecode(scopeChainNode);
    m_jitCode = JIT::compile(scopeChainNode->globalData, codeBlock);

#if !ENABLE(OPCODE_SAMPLING)
    if (!BytecodeGenerator::dumpsGeneratedCode())
        codeBlock->discardBytecode();
#endif
}

#endif

bool FunctionExecutable::isHostFunction() const
{
    return m_codeBlock && m_codeBlock->codeType() == NativeCode;
}

void FunctionExecutable::markAggregate(MarkStack& markStack)
{
    if (m_codeBlock)
        m_codeBlock->markAggregate(markStack);
}

ExceptionInfo* FunctionExecutable::reparseExceptionInfo(JSGlobalData* globalData, ScopeChainNode* scopeChainNode, CodeBlock* codeBlock)
{
    RefPtr<FunctionBodyNode> newFunctionBody = globalData->parser->reparse<FunctionBodyNode>(globalData, body());
    ASSERT(newFunctionBody);
    newFunctionBody->finishParsing(body()->copyParameters(), body()->parameterCount(), body()->ident());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    OwnPtr<CodeBlock> newCodeBlock(new FunctionCodeBlock(this, FunctionCode, source().provider(), source().startOffset()));
    globalData->functionCodeBlockBeingReparsed = newCodeBlock.get();

    OwnPtr<BytecodeGenerator> generator(new BytecodeGenerator(newFunctionBody.get(), globalObject->debugger(), scopeChain, &newCodeBlock->symbolTable(), newCodeBlock.get()));
    generator->setRegeneratingForExceptionInfo(static_cast<FunctionCodeBlock*>(codeBlock));
    generator->generate();

    ASSERT(newCodeBlock->instructionCount() == codeBlock->instructionCount());

#if ENABLE(JIT)
    JITCode newJITCode = JIT::compile(globalData, newCodeBlock.get());
    ASSERT(newJITCode.size() == generatedJITCode().size());
#endif

    globalData->functionCodeBlockBeingReparsed = 0;

    return newCodeBlock->extractExceptionInfo();
}

ExceptionInfo* EvalExecutable::reparseExceptionInfo(JSGlobalData* globalData, ScopeChainNode* scopeChainNode, CodeBlock* codeBlock)
{
    RefPtr<EvalNode> newEvalBody = globalData->parser->reparse<EvalNode>(globalData, evalNode());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    OwnPtr<EvalCodeBlock> newCodeBlock(new EvalCodeBlock(this, globalObject, source().provider(), scopeChain.localDepth()));

    OwnPtr<BytecodeGenerator> generator(new BytecodeGenerator(newEvalBody.get(), globalObject->debugger(), scopeChain, &newCodeBlock->symbolTable(), newCodeBlock.get()));
    generator->setRegeneratingForExceptionInfo(static_cast<EvalCodeBlock*>(codeBlock));
    generator->generate();

    ASSERT(newCodeBlock->instructionCount() == codeBlock->instructionCount());

#if ENABLE(JIT)
    JITCode newJITCode = JIT::compile(globalData, newCodeBlock.get());
    ASSERT(newJITCode.size() == generatedJITCode().size());
#endif

    return newCodeBlock->extractExceptionInfo();
}

void FunctionExecutable::recompile(ExecState* exec)
{
    FunctionBodyNode* oldBody = body();
    RefPtr<FunctionBodyNode> newBody = exec->globalData().parser->parse<FunctionBodyNode>(exec, 0, m_source);
    ASSERT(newBody);
    newBody->finishParsing(oldBody->copyParameters(), oldBody->parameterCount(), oldBody->ident());
    m_node = newBody;
    delete m_codeBlock;
    m_codeBlock = 0;
#if ENABLE(JIT)
    m_jitCode = JITCode();
#endif
}

#if ENABLE(JIT)
FunctionExecutable::FunctionExecutable(ExecState* exec)
    : m_codeBlock(new NativeCodeBlock(this))
    , m_name(Identifier(exec, "<native thunk>"))
{
    m_jitCode = JITCode(JITCode::HostFunction(exec->globalData().jitStubs.ctiNativeCallThunk()));
}
#endif

};


