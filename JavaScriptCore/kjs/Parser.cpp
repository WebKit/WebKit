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

#include "lexer.h"
#include "nodes.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

extern int kjsyyparse();

namespace KJS {

Parser::Parser()
    : m_sourceId(0)
{
}

PassRefPtr<ProgramNode> Parser::parseProgram(const UString& sourceURL, int startingLineNumber,
    const UChar* code, unsigned length,
    int* sourceId, int* errLine, UString* errMsg)
{
    m_sourceURL = sourceURL;
    parse(startingLineNumber, code, length, sourceId, errLine, errMsg);
    if (!m_sourceElements) {
        m_sourceURL = UString();
        return 0;
    }
    RefPtr<ProgramNode> program = new ProgramNode(m_sourceElements.release());
    m_sourceURL = UString();
    program->setLoc(startingLineNumber, m_lastLine);
    return program.release();
}

PassRefPtr<FunctionBodyNode> Parser::parseFunctionBody(const UString& sourceURL, int startingLineNumber,
    const UChar* code, unsigned length,
    int* sourceId, int* errLine, UString* errMsg)
{
    m_sourceURL = sourceURL;
    parse(startingLineNumber, code, length, sourceId, errLine, errMsg);
    if (!m_sourceElements) {
        m_sourceURL = UString();
        return 0;
    }
    RefPtr<FunctionBodyNode> body = new FunctionBodyNode(m_sourceElements.release());
    m_sourceURL = UString();
    body->setLoc(startingLineNumber, m_lastLine);
    return body;
}

void Parser::parse(int startingLineNumber,
    const UChar* code, unsigned length,
    int* sourceId, int* errLine, UString* errMsg)
{
    ASSERT(!m_sourceElements);

    if (errLine)
        *errLine = -1;
    if (errMsg)
        *errMsg = 0;
        
    Lexer& lexer = KJS::lexer();

    lexer.setCode(startingLineNumber, code, length);
    m_sourceId++;
    if (sourceId)
        *sourceId = m_sourceId;

    int parseError = kjsyyparse();
    bool lexError = lexer.sawError();
    lexer.clear();

    Node::clearNewNodes();

    if (parseError || lexError) {
        if (errLine)
            *errLine = lexer.lineNo();
        if (errMsg)
            *errMsg = "Parse error";
        m_sourceElements.clear();
    }
}

Parser& parser()
{
    ASSERT(JSLock::currentThreadIsHoldingLock());

    static Parser& staticParser = *new Parser;
    return staticParser;
}

} // namespace KJS
