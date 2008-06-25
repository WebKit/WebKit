// -*- c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2006, 2007 Apple Inc.
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
#include "Parser.h"
#include "debugger.h"

#include "lexer.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

extern int kjsyyparse(void*);

namespace KJS {

Parser::Parser()
    : m_sourceId(0)
{
}

void Parser::parse(ExecState* exec, const UString& sourceURL, int startingLineNumber,
                   PassRefPtr<SourceProvider> prpSource,
                   int* sourceId, int* errLine, UString* errMsg)
{
    ASSERT(!m_sourceElements);
    
    int defaultSourceId;
    int defaultErrLine;
    UString defaultErrMsg;
    
    RefPtr<SourceProvider> source = prpSource;

    if (!sourceId)
        sourceId = &defaultSourceId;
    if (!errLine)
        errLine = &defaultErrLine;
    if (!errMsg)
        errMsg = &defaultErrMsg;

    *errLine = -1;
    *errMsg = 0;
        
    Lexer& lexer = *exec->lexer();

    if (startingLineNumber <= 0)
        startingLineNumber = 1;

    lexer.setCode(startingLineNumber, source);
    *sourceId = ++m_sourceId;

    int parseError = kjsyyparse(&exec->globalData());
    bool lexError = lexer.sawError();
    lexer.clear();

    ParserRefCounted::deleteNewObjects(&exec->globalData());

    if (parseError || lexError) {
        *errLine = lexer.lineNo();
        *errMsg = "Parse error";
        m_sourceElements.clear();
    }
    
    if (Debugger* debugger = exec->dynamicGlobalObject()->debugger())
        debugger->sourceParsed(exec, *sourceId, sourceURL, *source, startingLineNumber, *errLine, *errMsg);
}

void Parser::didFinishParsing(SourceElements* sourceElements, ParserRefCountedData<DeclarationStacks::VarStack>* varStack, 
                              ParserRefCountedData<DeclarationStacks::FunctionStack>* funcStack, bool usesEval, bool needsClosure, int lastLine)
{
    m_sourceElements = sourceElements;
    m_varDeclarations = varStack;
    m_funcDeclarations = funcStack;
    m_usesEval = usesEval;
    m_needsClosure = needsClosure;
    m_lastLine = lastLine;
}

} // namespace KJS
