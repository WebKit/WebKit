/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "nodes.h"
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
        template <class ParsedNode>
        PassRefPtr<ParsedNode> parse(ExecState*, const UString& sourceURL, int startingLineNumber, PassRefPtr<SourceProvider> source,
                                     int* sourceId = 0, int* errLine = 0, UString* errMsg = 0);

        UString sourceURL() const { return m_sourceURL; }
        int sourceId() const { return m_sourceId; }

        void didFinishParsing(SourceElements*, ParserRefCountedData<DeclarationStacks::VarStack>*, 
                              ParserRefCountedData<DeclarationStacks::FunctionStack>*, bool usesEval, bool needsClosure, int lastLine, int numConstants);

    private:
        friend class JSGlobalData;
        Parser();

        void parse(ExecState*, const UString& sourceURL, int startingLineNumber, PassRefPtr<SourceProvider> source,
                   int* sourceId, int* errLine, UString* errMsg);

        UString m_sourceURL;
        int m_sourceId;
        RefPtr<SourceElements> m_sourceElements;
        RefPtr<ParserRefCountedData<DeclarationStacks::VarStack> > m_varDeclarations;
        RefPtr<ParserRefCountedData<DeclarationStacks::FunctionStack> > m_funcDeclarations;
        bool m_usesEval;
        bool m_needsClosure;
        int m_lastLine;
        int m_numConstants;
    };

    template <class ParsedNode>
    PassRefPtr<ParsedNode> Parser::parse(ExecState* exec, const UString& sourceURL, int startingLineNumber, PassRefPtr<SourceProvider> source,
                                         int* sourceId, int* errLine, UString* errMsg)
    {
        m_sourceURL = sourceURL;
        RefPtr<SourceProvider> sourceProvider = source;
        parse(exec, sourceURL, startingLineNumber, sourceProvider.get(), sourceId, errLine, errMsg);
        if (!m_sourceElements) {
            m_sourceURL = UString();
            return 0;
        }
        RefPtr<ParsedNode> node = ParsedNode::create(&exec->globalData(),
                                                     m_sourceElements.release().get(),
                                                     m_varDeclarations ? &m_varDeclarations->data : 0, 
                                                     m_funcDeclarations ? &m_funcDeclarations->data : 0,
                                                     sourceProvider.get(),
                                                     m_usesEval,
                                                     m_needsClosure,
                                                     m_numConstants);
        m_varDeclarations = 0;
        m_funcDeclarations = 0;
        m_sourceURL = UString();
        node->setLoc(startingLineNumber, m_lastLine);
        return node.release();
    }

} // namespace JSC

#endif // Parser_h
