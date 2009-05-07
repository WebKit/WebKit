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

#include "SourceProvider.h"
#include "Debugger.h"
#include "Nodes.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

namespace JSC {

    class FunctionBodyNode;
    class ProgramNode;
    class UString;

    template <typename T>
    struct ParserRefCountedData : ParserRefCounted {
        ParserRefCountedData(JSGlobalData* globalData)
            : ParserRefCounted(globalData)
        {
        }

        T data;
    };

    class Parser : Noncopyable {
    public:
        template <class ParsedNode> PassRefPtr<ParsedNode> parse(ExecState*, Debugger*, const SourceCode&, int* errLine = 0, UString* errMsg = 0);
        template <class ParsedNode> PassRefPtr<ParsedNode> reparse(JSGlobalData*, ParsedNode*);
        void reparseInPlace(JSGlobalData*, FunctionBodyNode*);

        void didFinishParsing(SourceElements*, ParserRefCountedData<DeclarationStacks::VarStack>*, 
                              ParserRefCountedData<DeclarationStacks::FunctionStack>*, CodeFeatures features, int lastLine, int numConstants);

    private:
        void parse(JSGlobalData*, int* errLine, UString* errMsg);

        const SourceCode* m_source;
        RefPtr<SourceElements> m_sourceElements;
        RefPtr<ParserRefCountedData<DeclarationStacks::VarStack> > m_varDeclarations;
        RefPtr<ParserRefCountedData<DeclarationStacks::FunctionStack> > m_funcDeclarations;
        CodeFeatures m_features;
        int m_lastLine;
        int m_numConstants;
    };

    template <class ParsedNode> PassRefPtr<ParsedNode> Parser::parse(ExecState* exec, Debugger* debugger, const SourceCode& source, int* errLine, UString* errMsg)
    {
        m_source = &source;
        parse(&exec->globalData(), errLine, errMsg);
        RefPtr<ParsedNode> result;
        if (m_sourceElements) {
            result = ParsedNode::create(&exec->globalData(),
                                         m_sourceElements.get(),
                                         m_varDeclarations ? &m_varDeclarations->data : 0, 
                                         m_funcDeclarations ? &m_funcDeclarations->data : 0,
                                         *m_source,
                                         m_features,
                                         m_numConstants);
            result->setLoc(m_source->firstLine(), m_lastLine);
        }

        exec->globalData().parserObjects.shrink(0);

        m_source = 0;
        m_sourceElements = 0;
        m_varDeclarations = 0;
        m_funcDeclarations = 0;

        if (debugger)
            debugger->sourceParsed(exec, source, *errLine, *errMsg);
        return result.release();
    }

    template <class ParsedNode> PassRefPtr<ParsedNode> Parser::reparse(JSGlobalData* globalData, ParsedNode* oldParsedNode)
    {
        m_source = &oldParsedNode->source();
        parse(globalData, 0, 0);
        RefPtr<ParsedNode> result;
        if (m_sourceElements) {
            result = ParsedNode::create(globalData,
                                        m_sourceElements.get(),
                                        m_varDeclarations ? &m_varDeclarations->data : 0, 
                                        m_funcDeclarations ? &m_funcDeclarations->data : 0,
                                        *m_source,
                                        oldParsedNode->features(),
                                        m_numConstants);
            result->setLoc(m_source->firstLine(), m_lastLine);
        }

        globalData->parserObjects.shrink(0);

        m_source = 0;
        m_sourceElements = 0;
        m_varDeclarations = 0;
        m_funcDeclarations = 0;

        return result.release();
    }

} // namespace JSC

#endif // Parser_h
