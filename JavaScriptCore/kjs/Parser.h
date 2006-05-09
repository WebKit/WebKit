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

#ifndef Parser_h
#define Parser_h

#include <wtf/Forward.h>

namespace KJS {

    class Node;
    class ProgramNode;
    class UChar;
    class UString;

    /**
     * @internal
     *
     * Parses ECMAScript source code and converts into ProgramNode objects, which
     * represent the root of a parse tree. This class provides a convenient workaround
     * for the problem of the bison parser working in a static context.
     */
    class Parser {
    public:
        static PassRefPtr<ProgramNode> parse(const UString& sourceURL, int startingLineNumber,
            const UChar* code, unsigned length,
            int* sourceId = 0, int* errLine = 0, UString* errMsg = 0);

        static void accept(PassRefPtr<ProgramNode>);

        static void saveNewNode(Node*);
        static void noteNodeCycle(Node*);
        static void removeNodeCycle(Node*);

        static int sid;
    };

} // namespace

#endif
