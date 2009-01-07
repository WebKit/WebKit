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

#ifndef JSGlobalData_h
#define JSGlobalData_h

#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include "Collector.h"
#include "ExecutableAllocator.h"
#include "SmallStrings.h"
#include "JSValue.h"

struct OpaqueJSClass;
struct OpaqueJSClassContextData;

namespace JSC {

    class ArgList;
    class CommonIdentifiers;
    class Heap;
    class IdentifierTable;
    class JSGlobalObject;
    class JSObject;
    class Lexer;
    class Interpreter;
    class Parser;
    class ParserRefCounted;
    class Structure;
    class UString;
    struct HashTable;

    class JSGlobalData : public RefCounted<JSGlobalData> {
    public:
        static bool sharedInstanceExists();
        static JSGlobalData& sharedInstance();

        static PassRefPtr<JSGlobalData> create();
        static PassRefPtr<JSGlobalData> createLeaked();
        ~JSGlobalData();

#if ENABLE(JSC_MULTIPLE_THREADS)
        // Will start tracking threads that use the heap, which is resource-heavy.
        void makeUsableFromMultipleThreads() { heap.makeUsableFromMultipleThreads(); }
#endif

        Interpreter* interpreter;

        JSValuePtr exception;
#if ENABLE(JIT)
        void* exceptionLocation;
#endif

        const HashTable* arrayTable;
        const HashTable* dateTable;
        const HashTable* mathTable;
        const HashTable* numberTable;
        const HashTable* regExpTable;
        const HashTable* regExpConstructorTable;
        const HashTable* stringTable;
        
        RefPtr<Structure> activationStructure;
        RefPtr<Structure> interruptedExecutionErrorStructure;
        RefPtr<Structure> staticScopeStructure;
        RefPtr<Structure> stringStructure;
        RefPtr<Structure> notAnObjectErrorStubStructure;
        RefPtr<Structure> notAnObjectStructure;
        RefPtr<Structure> numberStructure;

        IdentifierTable* identifierTable;
        CommonIdentifiers* propertyNames;
        const ArgList* emptyList; // Lists are supposed to be allocated on the stack to have their elements properly marked, which is not the case here - but this list has nothing to mark.

        SmallStrings smallStrings;
        
        HashMap<OpaqueJSClass*, OpaqueJSClassContextData*> opaqueJSClassData;

        HashSet<ParserRefCounted*>* newParserObjects;
        HashCountedSet<ParserRefCounted*>* parserObjectExtraRefCounts;

        Lexer* lexer;
        Parser* parser;

        JSGlobalObject* head;
        JSGlobalObject* dynamicGlobalObject;

        bool isSharedInstance;

        struct ClientData {
            virtual ~ClientData() = 0;
        };

        ClientData* clientData;

        HashSet<JSObject*> arrayVisitedElements;

        Heap heap;
#if ENABLE(ASSEMBLER)
        PassRefPtr<ExecutablePool> poolForSize(size_t n) { return m_executableAllocator.poolForSize(n); }
#endif
    private:
        JSGlobalData(bool isShared = false);
#if ENABLE(ASSEMBLER)
        ExecutableAllocator m_executableAllocator;
#endif

        static JSGlobalData*& sharedInstanceInternal();
    };

}

#endif
