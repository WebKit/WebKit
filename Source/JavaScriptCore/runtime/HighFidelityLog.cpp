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
#include "HighFidelityLog.h"

#include "JSCJSValueInlines.h"
#include "TypeLocation.h"
#include <wtf/CurrentTime.h>


namespace JSC {

static const bool verbose = false;

void HighFidelityLog::initializeHighFidelityLog()
{
    ASSERT(!m_logStartPtr);
    m_highFidelityLogSize = 50000;
    m_logStartPtr = new LogEntry[m_highFidelityLogSize];
    m_nextBuffer = new LogEntry[m_highFidelityLogSize];
    m_currentOffset = 0;
}

HighFidelityLog::~HighFidelityLog()
{
    delete[] m_logStartPtr;
    delete[] m_nextBuffer;
}

void HighFidelityLog::processHighFidelityLog(String reason)
{
    if (!m_currentOffset)
        return;

    if (verbose)
        dataLog("Process caller:'", reason,"'");

    double before = currentTimeMS();
    LogEntry* entry = m_logStartPtr;
    HashMap<StructureID, RefPtr<StructureShape>> seenShapes;
    size_t i = 0;
    while (i < m_currentOffset) {
        StructureID id = entry->structureID;
        RefPtr<StructureShape> shape; 
        if (id) {
            auto iter = seenShapes.find(id);
            if (iter == seenShapes.end()) {
                shape = Heap::heap(entry->value.asCell())->structureIDTable().get(entry->structureID)->toStructureShape(entry->value);
                seenShapes.set(id, shape);
            } else 
                shape = iter->value;
        }

        if (entry->location->m_globalTypeSet)
            entry->location->m_globalTypeSet->addTypeForValue(entry->value, shape, id);
        entry->location->m_instructionTypeSet->addTypeForValue(entry->value, shape, id);

        entry++;
        i++;
    }

    m_currentOffset = 0;

    if (verbose) {
        double after = currentTimeMS();
        dataLogF(" Processing the log took: '%f' ms\n", after - before);
    }
}

} //namespace JSC
