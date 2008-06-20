/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSGlobalData.h"

#include "collector.h"
#include "CommonIdentifiers.h"
#include "lexer.h"
#include "list.h"
#include "lookup.h"
#include "Machine.h"
#include "nodes.h"
#include "Parser.h"

#if USE(MULTIPLE_THREADS)
#include <wtf/Threading.h>
#include <wtf/ThreadSpecific.h>
#endif

using namespace WTF;

namespace KJS {

extern const HashTable arrayTable;
extern const HashTable dateTable;
extern const HashTable mathTable;
extern const HashTable numberTable;
extern const HashTable regExpTable;
extern const HashTable regExpConstructorTable;
extern const HashTable stringTable;


JSGlobalData::JSGlobalData()
    : heap(new Heap)
#if USE(MULTIPLE_THREADS)
    , arrayTable(new HashTable(KJS::arrayTable))
    , dateTable(new HashTable(KJS::dateTable))
    , mathTable(new HashTable(KJS::mathTable))
    , numberTable(new HashTable(KJS::numberTable))
    , regExpTable(new HashTable(KJS::regExpTable))
    , regExpConstructorTable(new HashTable(KJS::regExpConstructorTable))
    , stringTable(new HashTable(KJS::stringTable))
#else
    , arrayTable(&KJS::arrayTable)
    , dateTable(&KJS::dateTable)
    , mathTable(&KJS::mathTable)
    , numberTable(&KJS::numberTable)
    , regExpTable(&KJS::regExpTable)
    , regExpConstructorTable(&KJS::regExpConstructorTable)
    , stringTable(&KJS::stringTable)
#endif
    , identifierTable(createIdentifierTable())
    , propertyNames(new CommonIdentifiers(this))
    , lexer(new Lexer(this))
    , parser(new Parser)
    , head(0)
    , machine(new Machine)
{
}

JSGlobalData::~JSGlobalData()
{
#if USE(MULTIPLE_THREADS)
    delete[] arrayTable->table;
    delete[] dateTable->table;
    delete[] mathTable->table;
    delete[] numberTable->table;
    delete[] regExpTable->table;
    delete[] regExpConstructorTable->table;
    delete[] stringTable->table;
    delete arrayTable;
    delete dateTable;
    delete mathTable;
    delete numberTable;
    delete regExpTable;
    delete regExpConstructorTable;
    delete stringTable;

    delete parser;
    delete lexer;

    delete propertyNames;
    deleteIdentifierTable(identifierTable);
#endif
}

JSGlobalData& JSGlobalData::threadInstance()
{
#if USE(MULTIPLE_THREADS)
    static ThreadSpecific<JSGlobalData> sharedInstance;
    return *sharedInstance;
#else
    static JSGlobalData sharedInstance;
    return sharedInstance;
#endif
}

JSGlobalData& JSGlobalData::sharedInstance()
{
    return threadInstance();
/*
#if USE(MULTIPLE_THREADS)
    AtomicallyInitializedStatic(JSGlobalData, sharedInstance);
#else
    static JSGlobalData sharedInstance;
#endif
    return sharedInstance;
*/
}

}
