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

void HighFidelityLog::recordTypeInformationForLocation(JSValue v, TypeLocation* location)
{
    ASSERT(m_logStartPtr);
    ASSERT(m_currentOffset < m_highFidelityLogSize);

    LogEntry* entry = m_logStartPtr + m_currentOffset;

    entry->location = location;
    entry->value = v;
    entry->structure = (v.isCell() ? v.asCell()->structure() : nullptr);

    m_currentOffset += 1;
    if (m_currentOffset == m_highFidelityLogSize)
        processHighFidelityLog(true, "Log Full");
}

void HighFidelityLog::processHighFidelityLog(bool asynchronously, String reason)
{
    // This should only be called from the main execution thread.
    if (!m_currentOffset)
        return;

    if (verbose)
        dataLog("Process caller:'", reason,"'");

    ByteSpinLocker* locker = new ByteSpinLocker(m_lock);
    ThreadData* data = new ThreadData;
    data->m_proccessLogToOffset = m_currentOffset;
    data->m_processLogPtr = m_logStartPtr;
    data->m_locker = locker;

    m_currentOffset = 0;
    LogEntry* temp = m_logStartPtr;
    m_logStartPtr = m_nextBuffer;
    m_nextBuffer = temp;
    
    if (asynchronously)
        createThread(actuallyProcessLogThreadFunction, data, "ProcessHighFidelityLog");
    else 
        actuallyProcessLogThreadFunction(data);
}

void HighFidelityLog::actuallyProcessLogThreadFunction(void* arg)
{
    double before  = currentTimeMS();
    ThreadData* data = static_cast<ThreadData*>(arg);
    LogEntry* entry = data->m_processLogPtr;
    size_t processLogToOffset = data->m_proccessLogToOffset; 
    size_t i = 0;
    while (i < processLogToOffset) {
        Structure* structure = entry->structure ? entry->structure : nullptr;
        RefPtr<StructureShape> shape; 
        if (structure)
            shape = structure->toStructureShape();
        if (entry->location->m_globalTypeSet)
            entry->location->m_globalTypeSet->addTypeForValue(entry->value, shape);
        entry->location->m_instructionTypeSet->addTypeForValue(entry->value, shape);
        entry++;
        i++;
    }

    delete data->m_locker;
    delete data;
    double after = currentTimeMS();
    if (verbose)
        dataLogF("Processing the log took: '%f' ms\n", after - before);
}

} //namespace JSC
