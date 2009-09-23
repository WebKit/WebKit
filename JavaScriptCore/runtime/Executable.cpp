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
#include "Vector.h"

namespace JSC {

#if ENABLE(JIT)
NativeExecutable::~NativeExecutable()
{
}
#endif

VPtrHackExecutable::~VPtrHackExecutable()
{
}

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

JSObject* EvalExecutable::compile(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    int errLine;
    UString errMsg;
    RefPtr<EvalNode> evalNode = exec->globalData().parser->parse<EvalNode>(&exec->globalData(), exec->lexicalGlobalObject()->debugger(), exec, m_source, &errLine, &errMsg);
    if (!evalNode)
        return Error::create(exec, SyntaxError, errMsg, errLine, m_source.provider()->asID(), m_source.provider()->url());
    recordParse(evalNode->features(), evalNode->lineNo(), evalNode->lastLine());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    ASSERT(!m_evalCodeBlock);
    m_evalCodeBlock = new EvalCodeBlock(this, globalObject, source().provider(), scopeChain.localDepth());
    OwnPtr<BytecodeGenerator> generator(new BytecodeGenerator(evalNode.get(), globalObject->debugger(), scopeChain, m_evalCodeBlock->symbolTable(), m_evalCodeBlock));
    generator->generate();
    
    evalNode->destroyData();
    return 0;
}

JSObject* ProgramExecutable::checkSyntax(ExecState* exec)
{
    int errLine;
    UString errMsg;
    RefPtr<ProgramNode> programNode = exec->globalData().parser->parse<ProgramNode>(&exec->globalData(), exec->lexicalGlobalObject()->debugger(), exec, m_source, &errLine, &errMsg);
    if (!programNode)
        return Error::create(exec, SyntaxError, errMsg, errLine, m_source.provider()->asID(), m_source.provider()->url());
    return 0;
}

JSObject* ProgramExecutable::compile(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    int errLine;
    UString errMsg;
    RefPtr<ProgramNode> programNode = exec->globalData().parser->parse<ProgramNode>(&exec->globalData(), exec->lexicalGlobalObject()->debugger(), exec, m_source, &errLine, &errMsg);
    if (!programNode)
        return Error::create(exec, SyntaxError, errMsg, errLine, m_source.provider()->asID(), m_source.provider()->url());
    recordParse(programNode->features(), programNode->lineNo(), programNode->lastLine());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();
    
    ASSERT(!m_programCodeBlock);
    m_programCodeBlock = new ProgramCodeBlock(this, GlobalCode, globalObject, source().provider());
    OwnPtr<BytecodeGenerator> generator(new BytecodeGenerator(programNode.get(), globalObject->debugger(), scopeChain, &globalObject->symbolTable(), m_programCodeBlock));
    generator->generate();

    programNode->destroyData();
    return 0;
}

void FunctionExecutable::compile(ExecState*, ScopeChainNode* scopeChainNode)
{
    JSGlobalData* globalData = scopeChainNode->globalData;
    RefPtr<FunctionBodyNode> body = globalData->parser->parse<FunctionBodyNode>(globalData, 0, 0, m_source);
    if (m_forceUsesArguments)
        body->setUsesArguments();
    body->finishParsing(m_parameters, m_name);
    recordParse(body->features(), body->lineNo(), body->lastLine());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    ASSERT(!m_codeBlock);
    m_codeBlock = new FunctionCodeBlock(this, FunctionCode, source().provider(), source().startOffset());
    OwnPtr<BytecodeGenerator> generator(new BytecodeGenerator(body.get(), globalObject->debugger(), scopeChain, m_codeBlock->symbolTable(), m_codeBlock));
    generator->generate();
    m_numParameters = m_codeBlock->m_numParameters;
    ASSERT(m_numParameters);
    m_numVariables = m_codeBlock->m_numVars;

    body->destroyData();
}

#if ENABLE(JIT)

void EvalExecutable::generateJITCode(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    CodeBlock* codeBlock = &bytecode(exec, scopeChainNode);
    m_jitCode = JIT::compile(scopeChainNode->globalData, codeBlock);

#if !ENABLE(OPCODE_SAMPLING)
    if (!BytecodeGenerator::dumpsGeneratedCode())
        codeBlock->discardBytecode();
#endif
}

void ProgramExecutable::generateJITCode(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    CodeBlock* codeBlock = &bytecode(exec, scopeChainNode);
    m_jitCode = JIT::compile(scopeChainNode->globalData, codeBlock);

#if !ENABLE(OPCODE_SAMPLING)
    if (!BytecodeGenerator::dumpsGeneratedCode())
        codeBlock->discardBytecode();
#endif
}

void FunctionExecutable::generateJITCode(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    CodeBlock* codeBlock = &bytecode(exec, scopeChainNode);
    m_jitCode = JIT::compile(scopeChainNode->globalData, codeBlock);

#if !ENABLE(OPCODE_SAMPLING)
    if (!BytecodeGenerator::dumpsGeneratedCode())
        codeBlock->discardBytecode();
#endif
}

#endif

void FunctionExecutable::markAggregate(MarkStack& markStack)
{
    if (m_codeBlock)
        m_codeBlock->markAggregate(markStack);
}

ExceptionInfo* FunctionExecutable::reparseExceptionInfo(JSGlobalData* globalData, ScopeChainNode* scopeChainNode, CodeBlock* codeBlock)
{
    RefPtr<FunctionBodyNode> newFunctionBody = globalData->parser->parse<FunctionBodyNode>(globalData, 0, 0, m_source);
    if (m_forceUsesArguments)
        newFunctionBody->setUsesArguments();
    newFunctionBody->finishParsing(m_parameters, m_name);

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    OwnPtr<CodeBlock> newCodeBlock(new FunctionCodeBlock(this, FunctionCode, source().provider(), source().startOffset()));
    globalData->functionCodeBlockBeingReparsed = newCodeBlock.get();

    OwnPtr<BytecodeGenerator> generator(new BytecodeGenerator(newFunctionBody.get(), globalObject->debugger(), scopeChain, newCodeBlock->symbolTable(), newCodeBlock.get()));
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
    RefPtr<EvalNode> newEvalBody = globalData->parser->parse<EvalNode>(globalData, 0, 0, m_source);

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    OwnPtr<EvalCodeBlock> newCodeBlock(new EvalCodeBlock(this, globalObject, source().provider(), scopeChain.localDepth()));

    OwnPtr<BytecodeGenerator> generator(new BytecodeGenerator(newEvalBody.get(), globalObject->debugger(), scopeChain, newCodeBlock->symbolTable(), newCodeBlock.get()));
    generator->setRegeneratingForExceptionInfo(static_cast<EvalCodeBlock*>(codeBlock));
    generator->generate();

    ASSERT(newCodeBlock->instructionCount() == codeBlock->instructionCount());

#if ENABLE(JIT)
    JITCode newJITCode = JIT::compile(globalData, newCodeBlock.get());
    ASSERT(newJITCode.size() == generatedJITCode().size());
#endif

    return newCodeBlock->extractExceptionInfo();
}

void FunctionExecutable::recompile(ExecState*)
{
    delete m_codeBlock;
    m_codeBlock = 0;
    m_numParameters = NUM_PARAMETERS_NOT_COMPILED;
#if ENABLE(JIT)
    m_jitCode = JITCode();
#endif
}

PassRefPtr<FunctionExecutable> FunctionExecutable::fromGlobalCode(const Identifier& functionName, ExecState* exec, Debugger* debugger, const SourceCode& source, int* errLine, UString* errMsg)
{
    RefPtr<ProgramNode> program = exec->globalData().parser->parse<ProgramNode>(&exec->globalData(), debugger, exec, source, errLine, errMsg);
    if (!program)
        return 0;

    StatementNode* exprStatement = program->singleStatement();
    ASSERT(exprStatement);
    ASSERT(exprStatement->isExprStatement());
    if (!exprStatement || !exprStatement->isExprStatement())
        return 0;

    ExpressionNode* funcExpr = static_cast<ExprStatementNode*>(exprStatement)->expr();
    ASSERT(funcExpr);
    ASSERT(funcExpr->isFuncExprNode());
    if (!funcExpr || !funcExpr->isFuncExprNode())
        return 0;

    FunctionBodyNode* body = static_cast<FuncExprNode*>(funcExpr)->body();
    ASSERT(body);
    return FunctionExecutable::create(&exec->globalData(), functionName, body->source(), body->usesArguments(), body->parameters(), body->lineNo(), body->lastLine());
}

UString FunctionExecutable::paramString() const
{
    FunctionParameters& parameters = *m_parameters;
    UString s("");
    for (size_t pos = 0; pos < parameters.size(); ++pos) {
        if (!s.isEmpty())
            s += ", ";
        s += parameters[pos].ustring();
    }

    return s;
}

};


