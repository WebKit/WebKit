/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#ifndef HighFidelityLog_h
#define HighFidelityLog_h

#include "JSCJSValue.h"
#include "HighFidelityTypeProfiler.h"
#include "Structure.h"
#include <wtf/ByteSpinLock.h>

namespace JSC {

class TypeLocation;

class HighFidelityLog {

public:
    struct LogEntry {
        public:
        JSValue value;
        TypeLocation* location; 
        StructureID structureID;
    };

    HighFidelityLog()
        : m_logStartPtr(0)
    {
        initializeHighFidelityLog();
    }

    ~HighFidelityLog();

    ALWAYS_INLINE void recordTypeInformationForLocation(JSValue value, TypeLocation* location)
    {
        ASSERT(m_logStartPtr);
        ASSERT(m_currentOffset < m_highFidelityLogSize);
    
        LogEntry* entry = m_logStartPtr + m_currentOffset;
    
        entry->location = location;
        entry->value = value;
        entry->structureID = (value.isCell() ? value.asCell()->structureID() : 0);
    
        m_currentOffset += 1;
        if (m_currentOffset == m_highFidelityLogSize)
            processHighFidelityLog("Log Full");
    }

    void processHighFidelityLog(String);

private:
    void initializeHighFidelityLog();

    unsigned m_highFidelityLogSize;
    size_t m_currentOffset;
    LogEntry* m_logStartPtr;
    LogEntry* m_nextBuffer;

    ByteSpinLock m_lock;

    struct ThreadData {
        public:
        LogEntry* m_processLogPtr;
        size_t m_proccessLogToOffset;
        ByteSpinLocker* m_locker;
    };
};

} //namespace JSC

#endif //HighFidelityLog_h
