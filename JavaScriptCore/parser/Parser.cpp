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

#include "config.h"
#include "Parser.h"

#include "Debugger.h"
#include "Lexer.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

extern int kjsyyparse(void*);

namespace JSC {

void Parser::parse(JSGlobalData* globalData, int* errLine, UString* errMsg)
{
    ASSERT(!m_sourceElements);

    int defaultErrLine;
    UString defaultErrMsg;

    if (!errLine)
        errLine = &defaultErrLine;
    if (!errMsg)
        errMsg = &defaultErrMsg;

    *errLine = -1;
    *errMsg = 0;

    Lexer& lexer = *globalData->lexer;
    lexer.setCode(*m_source);

    int parseError = kjsyyparse(globalData);
    bool lexError = lexer.sawError();
    lexer.clear();

    ParserRefCounted::deleteNewObjects(globalData);

    if (parseError || lexError) {
        *errLine = lexer.lineNo();
        *errMsg = "Parse error";
        m_sourceElements.clear();
    }
}

void Parser::didFinishParsing(SourceElements* sourceElements, ParserRefCountedData<DeclarationStacks::VarStack>* varStack, 
                              ParserRefCountedData<DeclarationStacks::FunctionStack>* funcStack, CodeFeatures features, int lastLine, int numConstants)
{
    m_sourceElements = sourceElements;
    m_varDeclarations = varStack;
    m_funcDeclarations = funcStack;
    m_features = features;
    m_lastLine = lastLine;
    m_numConstants = numConstants;
}

} // namespace JSC
