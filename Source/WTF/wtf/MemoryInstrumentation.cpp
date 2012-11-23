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
    , m_rootObjectInfo(adoptPtr(new MemoryObjectInfo(this, 0)))
{
}

MemoryInstrumentation::~MemoryInstrumentation()
{
}

MemoryObjectType MemoryInstrumentation::getObjectType(MemoryObjectInfo* objectInfo)
{
    return objectInfo->objectType();
}

void MemoryInstrumentation::callReportObjectInfo(MemoryObjectInfo* memoryObjectInfo, const void* pointer, MemoryObjectType objectType, size_t objectSize)
{
    memoryObjectInfo->reportObjectInfo(pointer, objectType, objectSize);
}

MemoryInstrumentation::InstrumentedPointerBase::InstrumentedPointerBase(MemoryObjectInfo* memoryObjectInfo)
    : m_ownerObjectType(memoryObjectInfo->objectType())
{
#if DEBUG_POINTER_INSTRUMENTATION
    m_callStackSize = s_maxCallStackSize;
    WTFGetBacktrace(m_callStack, &m_callStackSize);
#endif
}

void MemoryInstrumentation::InstrumentedPointerBase::process(MemoryInstrumentation* memoryInstrumentation)
{
    MemoryObjectInfo memoryObjectInfo(memoryInstrumentation, m_ownerObjectType);
    const void* originalPointer = callReportMemoryUsage(&memoryObjectInfo);

    const void* pointer = memoryObjectInfo.reportedPointer();
    ASSERT(pointer);
    if (pointer != originalPointer && memoryInstrumentation->visited(pointer))
        return;
    memoryInstrumentation->countObjectSize(pointer, memoryObjectInfo.objectType(), memoryObjectInfo.objectSize());
    if (!memoryInstrumentation->checkCountedObject(pointer)) {
#if DEBUG_POINTER_INSTRUMENTATION
        fputs("Unknown object counted:\n", stderr);
        WTFPrintBacktrace(m_callStack, m_callStackSize);
#endif
    }
}

void MemoryClassInfo::init(const void* pointer, MemoryObjectType objectType, size_t actualSize)
{
    m_memoryObjectInfo->reportObjectInfo(pointer, objectType, actualSize);
    m_memoryInstrumentation = m_memoryObjectInfo->memoryInstrumentation();
    m_objectType = m_memoryObjectInfo->objectType();
}

void MemoryClassInfo::addRawBuffer(const void* const& buffer, size_t size)
{
    m_memoryInstrumentation->addRawBuffer(buffer, m_objectType, size);
}

void MemoryClassInfo::addPrivateBuffer(size_t size, MemoryObjectType ownerObjectType)
{
    if (!size)
        return;
    if (!ownerObjectType)
        ownerObjectType = m_objectType;
    m_memoryInstrumentation->countObjectSize(0, ownerObjectType, size);
}

} // namespace WTF
