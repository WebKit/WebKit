/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef Parser_h
#define Parser_h

#include "Debugger.h"
#include "ExceptionHelpers.h"
#include "Executable.h"
#include "JSGlobalObject.h"
#include "Lexer.h"
#include "Nodes.h"
#include "ParserArena.h"
#include "SourceProvider.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

namespace JSC {

    class FunctionBodyNode;
    
    class ProgramNode;
    class UString;

    template <typename T> struct ParserArenaData : ParserArenaDeletable { T data; };

    class Parser : public Noncopyable {
    public:
        template <class ParsedNode>
        PassRefPtr<ParsedNode> parse(JSGlobalData* globalData, JSGlobalObject* lexicalGlobalObject, Debugger*, ExecState*, const SourceCode& source, FunctionParameters*, JSObject** exception);

        void didFinishParsing(SourceElements*, ParserArenaData<DeclarationStacks::VarStack>*, 
                              ParserArenaData<DeclarationStacks::FunctionStack>*, CodeFeatures features,
                              int lastLine, int numConstants, IdentifierSet&);

        ParserArena& arena() { return m_arena; }

    private:
        void parse(JSGlobalData*, FunctionParameters*, int* errLine, UString* errMsg);

        // Used to determine type of error to report.
        bool isFunctionBodyNode(ScopeNode*) { return false; }
        bool isFunctionBodyNode(FunctionBodyNode*) { return true; }

        ParserArena m_arena;
        const SourceCode* m_source;
        SourceElements* m_sourceElements;
        ParserArenaData<DeclarationStacks::VarStack>* m_varDeclarations;
        ParserArenaData<DeclarationStacks::FunctionStack>* m_funcDeclarations;
        IdentifierSet m_capturedVariables;
        CodeFeatures m_features;
        int m_lastLine;
        int m_numConstants;
    };

    template <class ParsedNode>
    PassRefPtr<ParsedNode> Parser::parse(JSGlobalData* globalData, JSGlobalObject* lexicalGlobalObject, Debugger* debugger, ExecState* debuggerExecState, const SourceCode& source, FunctionParameters* parameters, JSObject** exception)
    {
        ASSERT(exception && !*exception);
        int errLine;
        UString errMsg;

        m_source = &source;
        if (ParsedNode::scopeIsFunction)
            globalData->lexer->setIsReparsing();
        parse(globalData, parameters, &errLine, &errMsg);

        RefPtr<ParsedNode> result;
        if (m_sourceElements) {
            result = ParsedNode::create(globalData,
                m_sourceElements,
                m_varDeclarations ? &m_varDeclarations->data : 0,
                m_funcDeclarations ? &m_funcDeclarations->data : 0,
                m_capturedVariables,
                source,
                m_features,
                m_numConstants);
            result->setLoc(m_source->firstLine(), m_lastLine);
        } else if (lexicalGlobalObject) {
            // We can never see a syntax error when reparsing a function, since we should have
            // reported the error when parsing the containing program or eval code. So if we're
            // parsing a function body node, we assume that what actually happened here is that
            // we ran out of stack while parsing. If we see an error while parsing eval or program
            // code we assume that it was a syntax error since running out of stack is much less
            // likely, and we are currently unable to distinguish between the two cases.
            if (isFunctionBodyNode(static_cast<ParsedNode*>(0)))
                *exception = createStackOverflowError(lexicalGlobalObject);
            else
                *exception = addErrorInfo(globalData, createSyntaxError(lexicalGlobalObject, errMsg), errLine, source);
        }

        m_arena.reset();

        m_source = 0;
        m_sourceElements = 0;
        m_varDeclarations = 0;
        m_funcDeclarations = 0;

        if (debugger && !ParsedNode::scopeIsFunction)
            debugger->sourceParsed(debuggerExecState, source, errLine, errMsg);
        return result.release();
    }

} // namespace JSC

#endif // Parser_h
