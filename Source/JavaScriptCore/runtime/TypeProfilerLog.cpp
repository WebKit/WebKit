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

#include "config.h"
#include "TypeProfilerLog.h"

#include "JSCJSValueInlines.h"
#include "TypeLocation.h"
#include <wtf/CurrentTime.h>


namespace JSC {

static const bool verbose = false;

void TypeProfilerLog::initializeLog()
{
    ASSERT(!m_logStartPtr);
    m_logSize = 50000;
    m_logStartPtr = new LogEntry[m_logSize];
    m_currentLogEntryPtr = m_logStartPtr;
    m_logEndPtr = m_logStartPtr + m_logSize;
}

TypeProfilerLog::~TypeProfilerLog()
{
    delete[] m_logStartPtr;
}

void TypeProfilerLog::processLogEntries(String reason)
{
    if (verbose)
        dataLog("Process caller:'", reason, "'");

    double before = currentTimeMS();
    LogEntry* entry = m_logStartPtr;
    HashMap<StructureID, RefPtr<StructureShape>> seenShapes;
    while (entry != m_currentLogEntryPtr) {
        StructureID id = entry->structureID;
        RefPtr<StructureShape> shape; 
        JSValue value = entry->value;
        if (id) {
            auto iter = seenShapes.find(id);
            if (iter == seenShapes.end()) {
                shape = Heap::heap(value.asCell())->structureIDTable().get(entry->structureID)->toStructureShape(value);
                seenShapes.set(id, shape);
            } else
                shape = iter->value;
        }

        RuntimeType type = TypeSet::getRuntimeTypeForValue(value);
        TypeLocation* location = entry->location;
        location->m_lastSeenType = type;
        if (location->m_globalTypeSet)
            location->m_globalTypeSet->addTypeInformation(type, shape, id);
        location->m_instructionTypeSet->addTypeInformation(type, shape, id);

        entry++;
    }

    m_currentLogEntryPtr = m_logStartPtr;

    if (verbose) {
        double after = currentTimeMS();
        dataLogF(" Processing the log took: '%f' ms\n", after - before);
    }
}

} // namespace JSC
