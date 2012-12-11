/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MemoryInstrumentation.h"

#include <wtf/MemoryObjectInfo.h>

#if DEBUG_POINTER_INSTRUMENTATION
#include <stdio.h>
#include <wtf/Assertions.h>
#endif

namespace WTF {

MemoryInstrumentation::MemoryInstrumentation(MemoryInstrumentationClient* client)
    : m_client(client)
    , m_rootObjectInfo(adoptPtr(new MemoryObjectInfo(this, 0, 0)))
{
}

MemoryInstrumentation::~MemoryInstrumentation()
{
}

void MemoryInstrumentation::reportEdge(MemoryObjectInfo* ownerObjectInfo, const void* target, const char* name)
{
    m_client->reportEdge(ownerObjectInfo->reportedPointer(), target, name);
}

MemoryObjectType MemoryInstrumentation::getObjectType(MemoryObjectInfo* objectInfo)
{
    return objectInfo->objectType();
}

void MemoryInstrumentation::callReportObjectInfo(MemoryObjectInfo* memoryObjectInfo, const void* pointer, MemoryObjectType objectType, size_t objectSize)
{
    memoryObjectInfo->reportObjectInfo(pointer, objectType, objectSize);
}

void MemoryInstrumentation::reportLinkToBuffer(const void* owner, const void* buffer, MemoryObjectType ownerObjectType, size_t size, const char* nodeName, const char* edgeName)
{
    MemoryObjectInfo memoryObjectInfo(this, ownerObjectType, 0);
    memoryObjectInfo.reportObjectInfo(buffer, ownerObjectType, size);
    memoryObjectInfo.setName(nodeName);
    m_client->reportLeaf(owner, memoryObjectInfo, edgeName);
}

MemoryInstrumentation::InstrumentedPointerBase::InstrumentedPointerBase(MemoryObjectInfo* memoryObjectInfo, const void* pointer)
    : m_pointer(pointer)
    , m_ownerObjectType(memoryObjectInfo->objectType())
{
#if DEBUG_POINTER_INSTRUMENTATION
    m_callStackSize = s_maxCallStackSize;
    WTFGetBacktrace(m_callStack, &m_callStackSize);
#endif
}

void MemoryInstrumentation::InstrumentedPointerBase::process(MemoryInstrumentation* memoryInstrumentation)
{
    MemoryObjectInfo memoryObjectInfo(memoryInstrumentation, m_ownerObjectType, m_pointer);
    callReportMemoryUsage(&memoryObjectInfo);

    const void* realAddress = memoryObjectInfo.reportedPointer();
    ASSERT(realAddress);
    if (realAddress != m_pointer) {
        memoryInstrumentation->m_client->reportBaseAddress(m_pointer, realAddress);
        if (!memoryObjectInfo.firstVisit())
            return;
    }
    memoryInstrumentation->countObjectSize(realAddress, memoryObjectInfo.objectType(), memoryObjectInfo.objectSize());
    memoryInstrumentation->m_client->reportNode(memoryObjectInfo);
    if (!memoryInstrumentation->checkCountedObject(realAddress)) {
#if DEBUG_POINTER_INSTRUMENTATION
        fputs("Unknown object counted:\n", stderr);
        WTFPrintBacktrace(m_callStack, m_callStackSize);
#endif
    }
}

void MemoryClassInfo::init(const void* objectAddress, MemoryObjectType objectType, size_t actualSize)
{
    m_memoryObjectInfo->reportObjectInfo(objectAddress, objectType, actualSize);
    m_memoryInstrumentation = m_memoryObjectInfo->memoryInstrumentation();
    m_objectType = m_memoryObjectInfo->objectType();
    m_skipMembers = !m_memoryObjectInfo->firstVisit();
}

void MemoryClassInfo::addRawBuffer(const void* buffer, size_t size, const char* nodeName, const char* edgeName)
{
    if (!m_skipMembers)
        m_memoryInstrumentation->addRawBuffer(m_memoryObjectInfo->reportedPointer(), buffer, m_objectType, size, nodeName, edgeName);
}

void MemoryClassInfo::addPrivateBuffer(size_t size, MemoryObjectType ownerObjectType, const char* nodeName, const char* edgeName)
{
    if (!size)
        return;
    if (m_skipMembers)
        return;
    if (!ownerObjectType)
        ownerObjectType = m_objectType;
    m_memoryInstrumentation->countObjectSize(0, ownerObjectType, size);
    m_memoryInstrumentation->reportLinkToBuffer(m_memoryObjectInfo->reportedPointer(), 0, ownerObjectType, size, nodeName, edgeName);
}

} // namespace WTF
