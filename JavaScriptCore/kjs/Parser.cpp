// -*- c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2006 Apple Computer, Inc.
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

int Parser::sid = 0;

static RefPtr<ProgramNode>* progNode;
static HashSet<Node*>* nodeCycles;

void Parser::noteNodeCycle(Node *node)
{
    if (!nodeCycles)
        nodeCycles = new HashSet<Node*>;
    nodeCycles->add(node);
}

void Parser::removeNodeCycle(Node *node)
{
    ASSERT(nodeCycles);
    nodeCycles->remove(node);
}

static void clearNewNodes()
{
    if (nodeCycles) {
        for (HashSet<Node*>::iterator it = nodeCycles->begin(); it != nodeCycles->end(); ++it)
            (*it)->breakCycle();
        delete nodeCycles;
        nodeCycles = 0;
    }
    Node::clearNewNodes();
}

PassRefPtr<ProgramNode> Parser::parse(const UString& sourceURL, int startingLineNumber,
    const UChar* code, unsigned length,
    int* sourceId, int* errLine, UString* errMsg)
{
    if (errLine)
        *errLine = -1;
    if (errMsg)
        *errMsg = 0;
    if (!progNode)
        progNode = new RefPtr<ProgramNode>;

    Lexer::curr()->setCode(sourceURL, startingLineNumber, code, length);
    *progNode = 0;
    sid++;
    if (sourceId)
        *sourceId = sid;

    // Enable this and the #define YYDEBUG in grammar.y to debug a parse error
    //extern int kjsyydebug;
    //kjsyydebug=1;

    int parseError = kjsyyparse();
    bool lexError = Lexer::curr()->sawError();
    Lexer::curr()->doneParsing();
    PassRefPtr<ProgramNode> prog = progNode->release();
    *progNode = 0;

    clearNewNodes();

    if (parseError || lexError) {
        int eline = Lexer::curr()->lineNo();
        if (errLine)
            *errLine = eline;
        if (errMsg)
            *errMsg = "Parse error";
        return 0;
    }

    return prog;
}

void Parser::accept(PassRefPtr<ProgramNode> prog)
{
    *progNode = prog;
}

}
