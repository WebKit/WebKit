/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "BuiltinExecutables.h"

#include "BuiltinNames.h"
#include "Executable.h"
#include "JSCInlines.h"
#include "Parser.h"

namespace JSC {

BuiltinExecutables::BuiltinExecutables(VM& vm)
    : m_vm(vm)
#define INITIALIZE_BUILTIN_SOURCE_MEMBERS(name, functionName, length) , m_##name##Source(makeSource(StringImpl::createFromLiteral(s_##name, length), StringImpl::createFromLiteral("<builtin>")))
    JSC_FOREACH_BUILTIN(INITIALIZE_BUILTIN_SOURCE_MEMBERS)
#undef EXPOSE_BUILTIN_STRINGS
{
}

UnlinkedFunctionExecutable* BuiltinExecutables::createBuiltinExecutable(const SourceCode& source, const Identifier& name)
{
    JSTextPosition positionBeforeLastNewline;
    ParserError error;
    RefPtr<ProgramNode> program = parse<ProgramNode>(&m_vm, source, 0, Identifier(), JSParseBuiltin, JSParseProgramCode, error, &positionBeforeLastNewline);

    if (!program) {
        dataLog("Fatal error compiling builtin function '", name.string(), "': ", error.m_message);
        CRASH();
    }

    StatementNode* exprStatement = program->singleStatement();
    RELEASE_ASSERT(exprStatement);
    RELEASE_ASSERT(exprStatement->isExprStatement());
    ExpressionNode* funcExpr = static_cast<ExprStatementNode*>(exprStatement)->expr();
    RELEASE_ASSERT(funcExpr);
    RELEASE_ASSERT(funcExpr->isFuncExprNode());
    FunctionBodyNode* body = static_cast<FuncExprNode*>(funcExpr)->body();
    RELEASE_ASSERT(!program->hasCapturedVariables());
    
    body->setEndPosition(positionBeforeLastNewline);
    RELEASE_ASSERT(body);
    RELEASE_ASSERT(body->ident().isNull());
    
    // This function assumes an input string that would result in a single anonymous function expression.
    body->setEndPosition(positionBeforeLastNewline);
    RELEASE_ASSERT(body);
    for (const auto& closedVariable : program->closedVariables()) {
        if (closedVariable == m_vm.propertyNames->arguments.impl())
        continue;
        
        if (closedVariable == m_vm.propertyNames->undefinedKeyword.impl())
            continue;
        RELEASE_ASSERT(closedVariable->isEmptyUnique());
    }
    UnlinkedFunctionExecutable* functionExecutable = UnlinkedFunctionExecutable::create(&m_vm, source, body, true, UnlinkedBuiltinFunction);
    functionExecutable->m_nameValue.set(m_vm, functionExecutable, jsString(&m_vm, name.string()));
    return functionExecutable;
}

#define DEFINE_BUILTIN_EXECUTABLES(name, functionName, length) \
UnlinkedFunctionExecutable* BuiltinExecutables::name##Executable() \
{\
    if (!m_##name##Executable)\
        m_##name##Executable = createBuiltinExecutable(m_##name##Source, m_vm.propertyNames->builtinNames().functionName());\
    return m_##name##Executable.get();\
}
JSC_FOREACH_BUILTIN(DEFINE_BUILTIN_EXECUTABLES)
#undef EXPOSE_BUILTIN_SOURCES

}
