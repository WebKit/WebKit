/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
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
#include "StringBuilder.h"
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
    delete m_codeBlockForCall;
    delete m_codeBlockForConstruct;
}

JSObject* EvalExecutable::compile(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    int errLine;
    UString errMsg;
    JSGlobalData* globalData = &exec->globalData();
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();
    RefPtr<EvalNode> evalNode = globalData->parser->parse<EvalNode>(globalData, lexicalGlobalObject->debugger(), exec, m_source, &errLine, &errMsg);
    if (!evalNode)
        return addErrorInfo(globalData, createSyntaxError(lexicalGlobalObject, errMsg), errLine, m_source);
    recordParse(evalNode->features(), evalNode->lineNo(), evalNode->lastLine());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    ASSERT(!m_evalCodeBlock);
    m_evalCodeBlock = new EvalCodeBlock(this, globalObject, source().provider(), scopeChain.localDepth());
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(evalNode.get(), globalObject->debugger(), scopeChain, m_evalCodeBlock->symbolTable(), m_evalCodeBlock)));
    generator->generate();
    
    evalNode->destroyData();
    return 0;
}

JSObject* ProgramExecutable::checkSyntax(ExecState* exec)
{
    int errLine;
    UString errMsg;
    JSGlobalData* globalData = &exec->globalData();
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();
    RefPtr<ProgramNode> programNode = globalData->parser->parse<ProgramNode>(globalData, lexicalGlobalObject->debugger(), exec, m_source, &errLine, &errMsg);
    if (!programNode)
        return addErrorInfo(globalData, createSyntaxError(lexicalGlobalObject, errMsg), errLine, m_source);
    return 0;
}

JSObject* ProgramExecutable::compile(ExecState* exec, ScopeChainNode* scopeChainNode)
{
    int errLine;
    UString errMsg;
    JSGlobalData* globalData = &exec->globalData();
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();
    RefPtr<ProgramNode> programNode = globalData->parser->parse<ProgramNode>(globalData, lexicalGlobalObject->debugger(), exec, m_source, &errLine, &errMsg);
    if (!programNode)
        return addErrorInfo(globalData, createSyntaxError(lexicalGlobalObject, errMsg), errLine, m_source);
    recordParse(programNode->features(), programNode->lineNo(), programNode->lastLine());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();
    
    ASSERT(!m_programCodeBlock);
    m_programCodeBlock = new ProgramCodeBlock(this, GlobalCode, globalObject, source().provider());
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(programNode.get(), globalObject->debugger(), scopeChain, &globalObject->symbolTable(), m_programCodeBlock)));
    generator->generate();

    programNode->destroyData();
    return 0;
}

bool FunctionExecutable::compileForCall(ExecState*, ScopeChainNode* scopeChainNode)
{
    JSGlobalData* globalData = scopeChainNode->globalData;
    RefPtr<FunctionBodyNode> body = globalData->parser->parse<FunctionBodyNode>(globalData, 0, 0, m_source);
    if (!body)
        return false;
    if (m_forceUsesArguments)
        body->setUsesArguments();
    body->finishParsing(m_parameters, m_name);
    recordParse(body->features(), body->lineNo(), body->lastLine());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    ASSERT(!m_codeBlockForCall);
    m_codeBlockForCall = new FunctionCodeBlock(this, FunctionCode, source().provider(), source().startOffset(), false);
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(body.get(), globalObject->debugger(), scopeChain, m_codeBlockForCall->symbolTable(), m_codeBlockForCall)));
    generator->generate();
    m_numParametersForCall = m_codeBlockForCall->m_numParameters;
    ASSERT(m_numParametersForCall);
    m_numVariables = m_codeBlockForCall->m_numVars;
    m_symbolTable = m_codeBlockForCall->sharedSymbolTable();

    body->destroyData();
    return true;
}

bool FunctionExecutable::compileForConstruct(ExecState*, ScopeChainNode* scopeChainNode)
{
    JSGlobalData* globalData = scopeChainNode->globalData;
    RefPtr<FunctionBodyNode> body = globalData->parser->parse<FunctionBodyNode>(globalData, 0, 0, m_source);
    if (!body)
        return false;
    if (m_forceUsesArguments)
        body->setUsesArguments();
    body->finishParsing(m_parameters, m_name);
    recordParse(body->features(), body->lineNo(), body->lastLine());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    ASSERT(!m_codeBlockForConstruct);
    m_codeBlockForConstruct = new FunctionCodeBlock(this, FunctionCode, source().provider(), source().startOffset(), true);
    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(body.get(), globalObject->debugger(), scopeChain, m_codeBlockForConstruct->symbolTable(), m_codeBlockForConstruct)));
    generator->generate();
    m_numParametersForConstruct = m_codeBlockForConstruct->m_numParameters;
    ASSERT(m_numParametersForConstruct);
    m_numVariables = m_codeBlockForConstruct->m_numVars;
    m_symbolTable = m_codeBlockForConstruct->sharedSymbolTable();

    body->destroyData();
    return true;
}

#if ENABLE(JIT)

void EvalExecutable::generateJITCode(ExecState* exec, ScopeChainNode* scopeChainNode)
{
#if ENABLE(INTERPRETER)
    ASSERT(exec->globalData().canUseJIT());
#endif
    CodeBlock* codeBlock = &bytecode(exec, scopeChainNode);
    m_jitCodeForCall = JIT::compile(scopeChainNode->globalData, codeBlock);

#if !ENABLE(OPCODE_SAMPLING)
    if (!BytecodeGenerator::dumpsGeneratedCode())
        codeBlock->discardBytecode();
#endif
}

void ProgramExecutable::generateJITCode(ExecState* exec, ScopeChainNode* scopeChainNode)
{
#if ENABLE(INTERPRETER)
    ASSERT(exec->globalData().canUseJIT());
#endif
    CodeBlock* codeBlock = &bytecode(exec, scopeChainNode);
    m_jitCodeForCall = JIT::compile(scopeChainNode->globalData, codeBlock);

#if !ENABLE(OPCODE_SAMPLING)
    if (!BytecodeGenerator::dumpsGeneratedCode())
        codeBlock->discardBytecode();
#endif
}

void FunctionExecutable::generateJITCodeForCall(ExecState* exec, ScopeChainNode* scopeChainNode)
{
#if ENABLE(INTERPRETER)
    ASSERT(exec->globalData().canUseJIT());
#endif
    CodeBlock* codeBlock = bytecodeForCall(exec, scopeChainNode);
    m_jitCodeForCall = JIT::compile(scopeChainNode->globalData, codeBlock, &m_jitCodeForCallWithArityCheck);

#if !ENABLE(OPCODE_SAMPLING)
    if (!BytecodeGenerator::dumpsGeneratedCode())
        codeBlock->discardBytecode();
#endif
}

void FunctionExecutable::generateJITCodeForConstruct(ExecState* exec, ScopeChainNode* scopeChainNode)
{
#if ENABLE(INTERPRETER)
    ASSERT(exec->globalData().canUseJIT());
#endif
    CodeBlock* codeBlock = bytecodeForConstruct(exec, scopeChainNode);
    m_jitCodeForConstruct = JIT::compile(scopeChainNode->globalData, codeBlock, &m_jitCodeForConstructWithArityCheck);

#if !ENABLE(OPCODE_SAMPLING)
    if (!BytecodeGenerator::dumpsGeneratedCode())
        codeBlock->discardBytecode();
#endif
}

#endif

void FunctionExecutable::markAggregate(MarkStack& markStack)
{
    if (m_codeBlockForCall)
        m_codeBlockForCall->markAggregate(markStack);
    if (m_codeBlockForConstruct)
        m_codeBlockForConstruct->markAggregate(markStack);
}

PassOwnPtr<ExceptionInfo> FunctionExecutable::reparseExceptionInfo(JSGlobalData* globalData, ScopeChainNode* scopeChainNode, CodeBlock* codeBlock)
{
    RefPtr<FunctionBodyNode> newFunctionBody = globalData->parser->parse<FunctionBodyNode>(globalData, 0, 0, m_source);
    if (!newFunctionBody)
        return PassOwnPtr<ExceptionInfo>();
    if (m_forceUsesArguments)
        newFunctionBody->setUsesArguments();
    newFunctionBody->finishParsing(m_parameters, m_name);

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    OwnPtr<CodeBlock> newCodeBlock(adoptPtr(new FunctionCodeBlock(this, FunctionCode, source().provider(), source().startOffset(), codeBlock->m_isConstructor)));
    globalData->functionCodeBlockBeingReparsed = newCodeBlock.get();

    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(newFunctionBody.get(), globalObject->debugger(), scopeChain, newCodeBlock->symbolTable(), newCodeBlock.get())));
    generator->setRegeneratingForExceptionInfo(static_cast<FunctionCodeBlock*>(codeBlock));
    generator->generate();

    ASSERT(newCodeBlock->instructionCount() == codeBlock->instructionCount());

#if ENABLE(JIT)
#if ENABLE(INTERPRETER)
    if (globalData->canUseJIT())
#endif
    {
        JITCode newJITCode = JIT::compile(globalData, newCodeBlock.get());
        ASSERT(codeBlock->m_isConstructor ? newJITCode.size() == generatedJITCodeForConstruct().size() : newJITCode.size() == generatedJITCodeForCall().size());
    }
#endif

    globalData->functionCodeBlockBeingReparsed = 0;

    return newCodeBlock->extractExceptionInfo();
}

PassOwnPtr<ExceptionInfo> EvalExecutable::reparseExceptionInfo(JSGlobalData* globalData, ScopeChainNode* scopeChainNode, CodeBlock* codeBlock)
{
    RefPtr<EvalNode> newEvalBody = globalData->parser->parse<EvalNode>(globalData, 0, 0, m_source);
    if (!newEvalBody)
        return PassOwnPtr<ExceptionInfo>();

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    OwnPtr<EvalCodeBlock> newCodeBlock(adoptPtr(new EvalCodeBlock(this, globalObject, source().provider(), scopeChain.localDepth())));

    OwnPtr<BytecodeGenerator> generator(adoptPtr(new BytecodeGenerator(newEvalBody.get(), globalObject->debugger(), scopeChain, newCodeBlock->symbolTable(), newCodeBlock.get())));
    generator->setRegeneratingForExceptionInfo(static_cast<EvalCodeBlock*>(codeBlock));
    generator->generate();

    ASSERT(newCodeBlock->instructionCount() == codeBlock->instructionCount());

#if ENABLE(JIT)
#if ENABLE(INTERPRETER)
    if (globalData->canUseJIT())
#endif
    {
        JITCode newJITCode = JIT::compile(globalData, newCodeBlock.get());
        ASSERT(newJITCode.size() == generatedJITCodeForCall().size());
    }
#endif

    return newCodeBlock->extractExceptionInfo();
}

void FunctionExecutable::recompile(ExecState*)
{
    delete m_codeBlockForCall;
    m_codeBlockForCall = 0;
    delete m_codeBlockForConstruct;
    m_codeBlockForConstruct = 0;
    m_numParametersForCall = NUM_PARAMETERS_NOT_COMPILED;
    m_numParametersForConstruct = NUM_PARAMETERS_NOT_COMPILED;
#if ENABLE(JIT)
    m_jitCodeForCall = JITCode();
    m_jitCodeForConstruct = JITCode();
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
    StringBuilder builder;
    for (size_t pos = 0; pos < parameters.size(); ++pos) {
        if (!builder.isEmpty())
            builder.append(", ");
        builder.append(parameters[pos].ustring());
    }
    return builder.build();
}

PassOwnPtr<ExceptionInfo> ProgramExecutable::reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*)
{
    // CodeBlocks for program code are transient and therefore do not gain from from throwing out their exception information.
    return PassOwnPtr<ExceptionInfo>();
}

}
