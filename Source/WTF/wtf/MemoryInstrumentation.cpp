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
{
}

MemoryInstrumentation::~MemoryInstrumentation()
{
}

void MemoryInstrumentation::reportEdge(const void* target, const char* name, MemberType memberType)
{
    m_client->reportEdge(target, name, memberType);
}

MemoryObjectType MemoryInstrumentation::getObjectType(MemoryObjectInfo* objectInfo)
{
    return objectInfo->objectType();
}

void MemoryInstrumentation::reportLinkToBuffer(const void* buffer, MemoryObjectType ownerObjectType, size_t size, const char* className, const char* edgeName)
{
    MemoryObjectInfo memoryObjectInfo(this, ownerObjectType, 0);
    memoryObjectInfo.reportObjectInfo(buffer, ownerObjectType, size);
    memoryObjectInfo.setClassName(className);
    m_client->reportLeaf(memoryObjectInfo, edgeName);
}

MemoryInstrumentation::WrapperBase::WrapperBase(MemoryObjectType objectType, const void* pointer)
    : m_pointer(pointer)
    , m_ownerObjectType(objectType)
{
#if DEBUG_POINTER_INSTRUMENTATION
    m_callStackSize = s_maxCallStackSize;
    WTFGetBacktrace(m_callStack, &m_callStackSize);
#endif
}

void MemoryInstrumentation::WrapperBase::process(MemoryInstrumentation* memoryInstrumentation)
{
    processPointer(memoryInstrumentation, false);
}

void MemoryInstrumentation::WrapperBase::processPointer(MemoryInstrumentation* memoryInstrumentation, bool isRoot)
{
    MemoryObjectInfo memoryObjectInfo(memoryInstrumentation, m_ownerObjectType, m_pointer);
    if (isRoot)
        memoryObjectInfo.markAsRoot();
    callReportMemoryUsage(&memoryObjectInfo);

    const void* realAddress = memoryObjectInfo.reportedPointer();
    ASSERT(realAddress);

    if (memoryObjectInfo.firstVisit()) {
        memoryInstrumentation->countObjectSize(realAddress, memoryObjectInfo.objectType(), memoryObjectInfo.objectSize());
        memoryInstrumentation->m_client->reportNode(memoryObjectInfo);
    }

    if (realAddress != m_pointer)
        memoryInstrumentation->m_client->reportBaseAddress(m_pointer, realAddress);

    if (memoryObjectInfo.firstVisit()
        && !memoryObjectInfo.customAllocation()
        && !memoryInstrumentation->checkCountedObject(realAddress)) {
#if DEBUG_POINTER_INSTRUMENTATION
        fputs("Unknown object counted:\n", stderr);
        WTFPrintBacktrace(m_callStack, m_callStackSize);
#endif
    }
}

void MemoryInstrumentation::WrapperBase::processRootObjectRef(MemoryInstrumentation* memoryInstrumentation)
{
    MemoryObjectInfo memoryObjectInfo(memoryInstrumentation, m_ownerObjectType, m_pointer);
    memoryObjectInfo.markAsRoot();
    callReportMemoryUsage(&memoryObjectInfo);

    ASSERT(m_pointer == memoryObjectInfo.reportedPointer());
    memoryInstrumentation->m_client->reportNode(memoryObjectInfo);
}

#if COMPILER(MSVC)
static const char* className(const char* functionName, char* buffer, const int maxLength)
{
    // MSVC generates names like this: 'WTF::FN<class WebCore::SharedBuffer>::fn'
    static const char prefix[] = "WTF::FN<";
    ASSERT(!strncmp(functionName, prefix, sizeof(prefix) - 1));
    const char* begin = strchr(functionName, ' ');
    if (!begin) { // Fallback.
        strncpy(buffer, functionName, maxLength);
        buffer[maxLength - 1] = 0;
        return buffer;
    }
    const char* end = strrchr(begin, '>');
    ASSERT(end);
    int length = end - begin;
    length = length < maxLength ? length : maxLength - 1;
    memcpy(buffer, begin, length);
    buffer[length] = 0;
    return buffer;
}
#else
static const char* className(const char* functionName, char* buffer, const int maxLength)
{
#if COMPILER(CLANG)
    static const char prefix[] = "[T =";
#elif COMPILER(GCC)
    static const char prefix[] = "[with T =";
#else
    static const char prefix[] = "T =";
#endif
    const char* begin = strstr(functionName, prefix);
    if (!begin) { // Fallback.
        strncpy(buffer, functionName, maxLength);
        buffer[maxLength - 1] = 0;
        return buffer;
    }
    begin += sizeof(prefix);
    const char* end = strchr(begin, ']');
    ASSERT(end);
    int length = end - begin;
    length = length < maxLength ? length : maxLength - 1;
    memcpy(buffer, begin, length);
    buffer[length] = 0;
    return buffer;
}
#endif

void MemoryClassInfo::callReportObjectInfo(MemoryObjectInfo* memoryObjectInfo, const void* objectAddress, const char* stringWithClassName, MemoryObjectType objectType, size_t actualSize)
{
    memoryObjectInfo->reportObjectInfo(objectAddress, objectType, actualSize);
    char buffer[256];
    memoryObjectInfo->setClassName(className(stringWithClassName, buffer, sizeof(buffer)));
}

void MemoryClassInfo::init(const void* objectAddress, const char* stringWithClassName, MemoryObjectType objectType, size_t actualSize)
{
    callReportObjectInfo(m_memoryObjectInfo, objectAddress, stringWithClassName, objectType, actualSize);
    m_memoryInstrumentation = m_memoryObjectInfo->memoryInstrumentation();
    m_objectType = m_memoryObjectInfo->objectType();
    m_skipMembers = !m_memoryObjectInfo->firstVisit();
}

void MemoryClassInfo::addRawBuffer(const void* buffer, size_t size, const char* className, const char* edgeName)
{
    if (!m_skipMembers)
        m_memoryInstrumentation->addRawBuffer(buffer, m_objectType, size, className, edgeName);
}

void MemoryClassInfo::addPrivateBuffer(size_t size, MemoryObjectType ownerObjectType, const char* className, const char* edgeName)
{
    if (!size)
        return;
    if (m_skipMembers)
        return;
    if (!ownerObjectType)
        ownerObjectType = m_objectType;
    m_memoryInstrumentation->countObjectSize(0, ownerObjectType, size);
    m_memoryInstrumentation->reportLinkToBuffer(0, ownerObjectType, size, className, edgeName);
}

void MemoryClassInfo::setCustomAllocation(bool customAllocation)
{
    m_memoryObjectInfo->setCustomAllocation(customAllocation);
}

} // namespace WTF
