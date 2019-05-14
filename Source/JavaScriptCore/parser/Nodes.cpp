/*
*  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
*  Copyright (C) 2001 Peter Kelly (pmk@post.com)
*  Copyright (C) 2003-2009, 2013, 2016 Apple Inc. All rights reserved.
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

#include "JSCInlines.h"
#include "ModuleScopeData.h"
#include <wtf/Assertions.h>

namespace JSC {

// ------------------------------ StatementNode --------------------------------

void StatementNode::setLoc(unsigned firstLine, unsigned lastLine, int startOffset, int lineStartOffset)
{
    m_lastLine = lastLine;
    m_position = JSTextPosition(firstLine, startOffset, lineStartOffset);
    ASSERT(m_position.offset >= m_position.lineStartOffset);
}

// ------------------------------ SourceElements --------------------------------

void SourceElements::append(StatementNode* statement)
{
    if (statement->isEmptyStatement())
        return;

    if (!m_head) {
        m_head = statement;
        m_tail = statement;
        return;
    }

    m_tail->setNext(statement);
    m_tail = statement;
}

StatementNode* SourceElements::singleStatement() const
{
    return m_head == m_tail ? m_head : nullptr;
}

StatementNode* SourceElements::lastStatement() const
{
    return m_tail;
}

bool SourceElements::hasCompletionValue() const
{
    for (StatementNode* statement = m_head; statement; statement = statement->next()) {
        if (statement->hasCompletionValue())
            return true;
    }

    return false;
}

bool SourceElements::hasEarlyBreakOrContinue() const
{
    for (StatementNode* statement = m_head; statement; statement = statement->next()) {
        if (statement->isBreak() || statement->isContinue())
            return true;
        if (statement->hasCompletionValue())
            return false;
    }

    return false;
}

// ------------------------------ BlockNode ------------------------------------

StatementNode* BlockNode::lastStatement() const
{
    return m_statements ? m_statements->lastStatement() : nullptr;
}

StatementNode* BlockNode::singleStatement() const
{
    return m_statements ? m_statements->singleStatement() : nullptr;
}

bool BlockNode::hasCompletionValue() const
{
    return m_statements ? m_statements->hasCompletionValue() : false;
}

bool BlockNode::hasEarlyBreakOrContinue() const
{
    return m_statements ? m_statements->hasEarlyBreakOrContinue() : false;
}

// ------------------------------ ScopeNode -----------------------------

ScopeNode::ScopeNode(ParserArena& parserArena, const JSTokenLocation& startLocation, const JSTokenLocation& endLocation, bool inStrictContext)
    : StatementNode(endLocation)
    , ParserArenaRoot(parserArena)
    , m_startLineNumber(startLocation.line)
    , m_startStartOffset(startLocation.startOffset)
    , m_startLineStartOffset(startLocation.lineStartOffset)
    , m_features(inStrictContext ? StrictModeFeature : NoFeatures)
    , m_innerArrowFunctionCodeFeatures(NoInnerArrowFunctionFeatures)
    , m_numConstants(0)
    , m_statements(0)
{
}

ScopeNode::ScopeNode(ParserArena& parserArena, const JSTokenLocation& startLocation, const JSTokenLocation& endLocation, const SourceCode& source, SourceElements* children, VariableEnvironment& varEnvironment, FunctionStack&& funcStack, VariableEnvironment& lexicalVariables, UniquedStringImplPtrSet&& sloppyModeHoistedFunctions, CodeFeatures features, InnerArrowFunctionCodeFeatures innerArrowFunctionCodeFeatures, int numConstants)
    : StatementNode(endLocation)
    , ParserArenaRoot(parserArena)
    , VariableEnvironmentNode(lexicalVariables, WTFMove(funcStack))
    , m_startLineNumber(startLocation.line)
    , m_startStartOffset(startLocation.startOffset)
    , m_startLineStartOffset(startLocation.lineStartOffset)
    , m_features(features)
    , m_innerArrowFunctionCodeFeatures(innerArrowFunctionCodeFeatures)
    , m_source(source)
    , m_sloppyModeHoistedFunctions(WTFMove(sloppyModeHoistedFunctions))
    , m_numConstants(numConstants)
    , m_statements(children)
{
    m_varDeclarations.swap(varEnvironment);
}

StatementNode* ScopeNode::singleStatement() const
{
    return m_statements ? m_statements->singleStatement() : nullptr;
}

bool ScopeNode::hasCompletionValue() const
{
    return m_statements ? m_statements->hasCompletionValue() : false;
}

bool ScopeNode::hasEarlyBreakOrContinue() const
{
    return m_statements ? m_statements->hasEarlyBreakOrContinue() : false;
}

// ------------------------------ ProgramNode -----------------------------

ProgramNode::ProgramNode(ParserArena& parserArena, const JSTokenLocation& startLocation, const JSTokenLocation& endLocation, unsigned startColumn, unsigned endColumn, SourceElements* children, VariableEnvironment& varEnvironment, FunctionStack&& funcStack, VariableEnvironment& lexicalVariables, UniquedStringImplPtrSet&& sloppyModeHoistedFunctions, FunctionParameters*, const SourceCode& source, CodeFeatures features, InnerArrowFunctionCodeFeatures innerArrowFunctionCodeFeatures, int numConstants, RefPtr<ModuleScopeData>&&)
    : ScopeNode(parserArena, startLocation, endLocation, source, children, varEnvironment, WTFMove(funcStack), lexicalVariables, WTFMove(sloppyModeHoistedFunctions), features, innerArrowFunctionCodeFeatures, numConstants)
    , m_startColumn(startColumn)
    , m_endColumn(endColumn)
{
}

// ------------------------------ ModuleProgramNode -----------------------------

ModuleProgramNode::ModuleProgramNode(ParserArena& parserArena, const JSTokenLocation& startLocation, const JSTokenLocation& endLocation, unsigned startColumn, unsigned endColumn, SourceElements* children, VariableEnvironment& varEnvironment, FunctionStack&& funcStack, VariableEnvironment& lexicalVariables, UniquedStringImplPtrSet&& sloppyModeHoistedFunctions, FunctionParameters*, const SourceCode& source, CodeFeatures features, InnerArrowFunctionCodeFeatures innerArrowFunctionCodeFeatures, int numConstants, RefPtr<ModuleScopeData>&& moduleScopeData)
    : ScopeNode(parserArena, startLocation, endLocation, source, children, varEnvironment, WTFMove(funcStack), lexicalVariables, WTFMove(sloppyModeHoistedFunctions), features, innerArrowFunctionCodeFeatures, numConstants)
    , m_startColumn(startColumn)
    , m_endColumn(endColumn)
    , m_moduleScopeData(*WTFMove(moduleScopeData))
{
}

// ------------------------------ EvalNode -----------------------------

EvalNode::EvalNode(ParserArena& parserArena, const JSTokenLocation& startLocation, const JSTokenLocation& endLocation, unsigned, unsigned endColumn, SourceElements* children, VariableEnvironment& varEnvironment, FunctionStack&& funcStack, VariableEnvironment& lexicalVariables, UniquedStringImplPtrSet&& sloppyModeHoistedFunctions, FunctionParameters*, const SourceCode& source, CodeFeatures features, InnerArrowFunctionCodeFeatures innerArrowFunctionCodeFeatures, int numConstants, RefPtr<ModuleScopeData>&&)
    : ScopeNode(parserArena, startLocation, endLocation, source, children, varEnvironment, WTFMove(funcStack), lexicalVariables, WTFMove(sloppyModeHoistedFunctions), features, innerArrowFunctionCodeFeatures, numConstants)
    , m_endColumn(endColumn)
{
}

// ------------------------------ FunctionMetadataNode -----------------------------

FunctionMetadataNode::FunctionMetadataNode(
    ParserArena&, const JSTokenLocation& startLocation, 
    const JSTokenLocation& endLocation, unsigned startColumn, unsigned endColumn, 
    int functionKeywordStart, int functionNameStart, int parametersStart, bool isInStrictContext, 
    ConstructorKind constructorKind, SuperBinding superBinding, unsigned parameterCount, SourceParseMode mode, bool isArrowFunctionBodyExpression)
        : Node(endLocation)
        , m_isInStrictContext(isInStrictContext)
        , m_superBinding(static_cast<unsigned>(superBinding))
        , m_constructorKind(static_cast<unsigned>(constructorKind))
        , m_isArrowFunctionBodyExpression(isArrowFunctionBodyExpression)
        , m_parseMode(mode)
        , m_startColumn(startColumn)
        , m_endColumn(endColumn)
        , m_functionKeywordStart(functionKeywordStart)
        , m_functionNameStart(functionNameStart)
        , m_parametersStart(parametersStart)
        , m_startStartOffset(startLocation.startOffset)
        , m_parameterCount(parameterCount)
{
    ASSERT(m_superBinding == static_cast<unsigned>(superBinding));
    ASSERT(m_constructorKind == static_cast<unsigned>(constructorKind));
}

FunctionMetadataNode::FunctionMetadataNode(
    const JSTokenLocation& startLocation, 
    const JSTokenLocation& endLocation, unsigned startColumn, unsigned endColumn, 
    int functionKeywordStart, int functionNameStart, int parametersStart, bool isInStrictContext, 
    ConstructorKind constructorKind, SuperBinding superBinding, unsigned parameterCount, SourceParseMode mode, bool isArrowFunctionBodyExpression)
        : Node(endLocation)
        , m_isInStrictContext(isInStrictContext)
        , m_superBinding(static_cast<unsigned>(superBinding))
        , m_constructorKind(static_cast<unsigned>(constructorKind))
        , m_isArrowFunctionBodyExpression(isArrowFunctionBodyExpression)
        , m_parseMode(mode)
        , m_startColumn(startColumn)
        , m_endColumn(endColumn)
        , m_functionKeywordStart(functionKeywordStart)
        , m_functionNameStart(functionNameStart)
        , m_parametersStart(parametersStart)
        , m_startStartOffset(startLocation.startOffset)
        , m_parameterCount(parameterCount)
{
    ASSERT(m_superBinding == static_cast<unsigned>(superBinding));
    ASSERT(m_constructorKind == static_cast<unsigned>(constructorKind));
}

void FunctionMetadataNode::finishParsing(const SourceCode& source, const Identifier& ident, FunctionMode functionMode)
{
    m_source = source;
    m_ident = ident;
    m_functionMode = functionMode;
}

void FunctionMetadataNode::setEndPosition(JSTextPosition position)
{
    m_lastLine = position.line;
    m_endColumn = position.offset - position.lineStartOffset;
}

bool FunctionMetadataNode::operator==(const FunctionMetadataNode& other) const
{
    return m_parseMode== other.m_parseMode
        && m_isInStrictContext == other.m_isInStrictContext
        && m_superBinding == other.m_superBinding
        && m_constructorKind == other.m_constructorKind
        && m_isArrowFunctionBodyExpression == other.m_isArrowFunctionBodyExpression
        && m_ident == other.m_ident
        && m_ecmaName == other.m_ecmaName
        && m_functionMode== other.m_functionMode
        && m_startColumn== other.m_startColumn
        && m_endColumn== other.m_endColumn
        && m_functionKeywordStart== other.m_functionKeywordStart
        && m_functionNameStart== other.m_functionNameStart
        && m_parametersStart== other.m_parametersStart
        && m_source== other.m_source
        && m_classSource== other.m_classSource
        && m_startStartOffset== other.m_startStartOffset
        && m_parameterCount== other.m_parameterCount
        && m_lastLine== other.m_lastLine
        && m_position == other.m_position;
}

void FunctionMetadataNode::dump(PrintStream& stream) const
{
    stream.println("m_parseMode ", static_cast<uint32_t>(m_parseMode));
    stream.println("m_isInStrictContext ", m_isInStrictContext);
    stream.println("m_superBinding ", m_superBinding);
    stream.println("m_constructorKind ", m_constructorKind);
    stream.println("m_isArrowFunctionBodyExpression ", m_isArrowFunctionBodyExpression);
    stream.println("m_ident ", m_ident);
    stream.println("m_ecmaName ", m_ecmaName);
    stream.println("m_functionMode ", static_cast<uint32_t>(m_functionMode));
    stream.println("m_startColumn ", m_startColumn);
    stream.println("m_endColumn ", m_endColumn);
    stream.println("m_functionKeywordStart ", m_functionKeywordStart);
    stream.println("m_functionNameStart ", m_functionNameStart);
    stream.println("m_parametersStart ", m_parametersStart);
    stream.println("m_classSource.isNull() ", m_classSource.isNull());
    stream.println("m_startStartOffset ", m_startStartOffset);
    stream.println("m_parameterCount ", m_parameterCount);
    stream.println("m_lastLine ", m_lastLine);
    stream.println("position().line ", position().line);
    stream.println("position().offset ", position().offset);
    stream.println("position().lineStartOffset ", position().lineStartOffset);
}

// ------------------------------ FunctionNode -----------------------------

FunctionNode::FunctionNode(ParserArena& parserArena, const JSTokenLocation& startLocation, const JSTokenLocation& endLocation, unsigned startColumn, unsigned endColumn, SourceElements* children, VariableEnvironment& varEnvironment, FunctionStack&& funcStack, VariableEnvironment& lexicalVariables, UniquedStringImplPtrSet&& sloppyModeHoistedFunctions, FunctionParameters* parameters, const SourceCode& sourceCode, CodeFeatures features, InnerArrowFunctionCodeFeatures innerArrowFunctionCodeFeatures, int numConstants, RefPtr<ModuleScopeData>&&)
    : ScopeNode(parserArena, startLocation, endLocation, sourceCode, children, varEnvironment, WTFMove(funcStack), lexicalVariables, WTFMove(sloppyModeHoistedFunctions), features, innerArrowFunctionCodeFeatures, numConstants)
    , m_parameters(parameters)
    , m_startColumn(startColumn)
    , m_endColumn(endColumn)
{
}

void FunctionNode::finishParsing(const Identifier& ident, FunctionMode functionMode)
{
    ASSERT(!source().isNull());
    m_ident = ident;
    m_functionMode = functionMode;
}

bool PropertyListNode::hasStaticallyNamedProperty(const Identifier& propName)
{
    PropertyListNode* list = this;
    while (list) {
        if (list->m_node->isStaticClassProperty()) {
            const Identifier* currentNodeName = list->m_node->name();
            if (currentNodeName && *currentNodeName == propName)
                return true;
        }
        list = list->m_next;
    }
    return false;
}

VariableEnvironmentNode::VariableEnvironmentNode(VariableEnvironment& lexicalVariables)
{
    m_lexicalVariables.swap(lexicalVariables);
}

VariableEnvironmentNode::VariableEnvironmentNode(VariableEnvironment& lexicalVariables, FunctionStack&& functionStack)
{
    m_lexicalVariables.swap(lexicalVariables);
    m_functionStack = WTFMove(functionStack);
}

} // namespace JSC
