/*
*  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
*  Copyright (C) 2001 Peter Kelly (pmk@post.com)
*  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
*  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
*  Copyright (C) 2007 Maks Orlovich
*  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
*
*  This library is free software; you can redistribute it and/or
*  modify it under the terms of the GNU Library General Public
*  License as published by the Free Software Foundation; either
*  version 2 of the License, or (at your option) any later version.
*
*  This library is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*  Library General Public License for more details.
*
*  You should have received a copy of the GNU Library General Public License
*  along with this library; see the file COPYING.LIB.  If not, write to
*  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
*  Boston, MA 02110-1301, USA.
*
*/

#include "config.h"
#include "Nodes.h"
#include "NodeConstructors.h"

#include "BytecodeGenerator.h"
#include "CallFrame.h"
#include "Debugger.h"
#include "JIT.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSStaticScopeObject.h"
#include "LabelScope.h"
#include "Lexer.h"
#include "Operations.h"
#include "Parser.h"
#include "PropertyNameArray.h"
#include "RegExpObject.h"
#include "SamplingTool.h"
#include <wtf/Assertions.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/Threading.h>

using namespace WTF;

namespace JSC {


// ------------------------------ StatementNode --------------------------------

void StatementNode::setLoc(int firstLine, int lastLine)
{
    m_lineNumber = firstLine;
    m_lastLine = lastLine;
}

// ------------------------------ SourceElements --------------------------------

void SourceElements::append(StatementNode* statement)
{
    if (statement->isEmptyStatement())
        return;
    m_statements.append(statement);
}

StatementNode* SourceElements::singleStatement() const
{
    size_t size = m_statements.size();
    return size == 1 ? m_statements[0] : 0;
}

// ------------------------------ ScopeNode -----------------------------

ScopeNode::ScopeNode(JSGlobalData* globalData, int lineNumber, bool inStrictContext)
    : StatementNode(lineNumber)
    , ParserArenaRefCounted(globalData)
    , m_features(inStrictContext ? StrictModeFeature : NoFeatures)
    , m_numConstants(0)
    , m_statements(0)
{
}

ScopeNode::ScopeNode(JSGlobalData* globalData, int lineNumber, const SourceCode& source, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, IdentifierSet& capturedVariables, CodeFeatures features, int numConstants)
    : StatementNode(lineNumber)
    , ParserArenaRefCounted(globalData)
    , m_features(features)
    , m_source(source)
    , m_numConstants(numConstants)
    , m_statements(children)
{
    m_arena.swap(*globalData->parserArena);
    if (varStack)
        m_varStack.swap(*varStack);
    if (funcStack)
        m_functionStack.swap(*funcStack);
    m_capturedVariables.swap(capturedVariables);
}

StatementNode* ScopeNode::singleStatement() const
{
    return m_statements ? m_statements->singleStatement() : 0;
}

// ------------------------------ ProgramNode -----------------------------

inline ProgramNode::ProgramNode(JSGlobalData* globalData, int lineNumber, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, IdentifierSet& capturedVariables, const SourceCode& source, CodeFeatures features, int numConstants)
    : ScopeNode(globalData, lineNumber, source, children, varStack, funcStack, capturedVariables, features, numConstants)
{
}

PassRefPtr<ProgramNode> ProgramNode::create(JSGlobalData* globalData, int lineNumber, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, IdentifierSet& capturedVariables, const SourceCode& source, CodeFeatures features, int numConstants)
{
    RefPtr<ProgramNode> node = new ProgramNode(globalData, lineNumber, children, varStack, funcStack,  capturedVariables, source, features, numConstants);

    ASSERT(node->m_arena.last() == node);
    node->m_arena.removeLast();
    ASSERT(!node->m_arena.contains(node.get()));

    return node.release();
}

// ------------------------------ EvalNode -----------------------------

inline EvalNode::EvalNode(JSGlobalData* globalData, int lineNumber, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, IdentifierSet& capturedVariables, const SourceCode& source, CodeFeatures features, int numConstants)
    : ScopeNode(globalData, lineNumber, source, children, varStack, funcStack, capturedVariables, features, numConstants)
{
}

PassRefPtr<EvalNode> EvalNode::create(JSGlobalData* globalData, int lineNumber, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, IdentifierSet& capturedVariables, const SourceCode& source, CodeFeatures features, int numConstants)
{
    RefPtr<EvalNode> node = new EvalNode(globalData, lineNumber, children, varStack, funcStack, capturedVariables, source, features, numConstants);

    ASSERT(node->m_arena.last() == node);
    node->m_arena.removeLast();
    ASSERT(!node->m_arena.contains(node.get()));

    return node.release();
}

// ------------------------------ FunctionBodyNode -----------------------------

FunctionParameters::FunctionParameters(ParameterNode* firstParameter)
{
    for (ParameterNode* parameter = firstParameter; parameter; parameter = parameter->nextParam())
        append(parameter->ident());
}

inline FunctionBodyNode::FunctionBodyNode(JSGlobalData* globalData, int lineNumber, bool inStrictContext)
    : ScopeNode(globalData, lineNumber, inStrictContext)
{
}

inline FunctionBodyNode::FunctionBodyNode(JSGlobalData* globalData, int lineNumber, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, IdentifierSet& capturedVariables, const SourceCode& sourceCode, CodeFeatures features, int numConstants)
    : ScopeNode(globalData, lineNumber, sourceCode, children, varStack, funcStack, capturedVariables, features, numConstants)
{
}

void FunctionBodyNode::finishParsing(const SourceCode& source, ParameterNode* firstParameter, const Identifier& ident)
{
    setSource(source);
    finishParsing(FunctionParameters::create(firstParameter), ident);
}

void FunctionBodyNode::finishParsing(PassRefPtr<FunctionParameters> parameters, const Identifier& ident)
{
    ASSERT(!source().isNull());
    m_parameters = parameters;
    m_ident = ident;
}

FunctionBodyNode* FunctionBodyNode::create(JSGlobalData* globalData, int lineNumber, bool inStrictContext)
{
    return new FunctionBodyNode(globalData, lineNumber, inStrictContext);
}

PassRefPtr<FunctionBodyNode> FunctionBodyNode::create(JSGlobalData* globalData, int lineNumber, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, IdentifierSet& capturedVariables, const SourceCode& sourceCode, CodeFeatures features, int numConstants)
{
    RefPtr<FunctionBodyNode> node = new FunctionBodyNode(globalData, lineNumber, children, varStack, funcStack, capturedVariables, sourceCode, features, numConstants);

    ASSERT(node->m_arena.last() == node);
    node->m_arena.removeLast();
    ASSERT(!node->m_arena.contains(node.get()));

    return node.release();
}

} // namespace JSC
