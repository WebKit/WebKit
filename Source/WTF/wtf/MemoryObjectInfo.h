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

#ifndef MemoryObjectInfo_h
#define MemoryObjectInfo_h

#include <wtf/MemoryInstrumentation.h>
#include <wtf/text/WTFString.h>

namespace WTF {

class MemoryClassInfo;
class MemoryInstrumentation;

typedef const char* MemoryObjectType;

class MemoryObjectInfo {
public:
    MemoryObjectInfo(MemoryInstrumentation* memoryInstrumentation, MemoryObjectType ownerObjectType, const void* pointer)
        : m_memoryInstrumentation(memoryInstrumentation)
        , m_objectType(ownerObjectType)
        , m_objectSize(0)
        , m_pointer(pointer)
        , m_firstVisit(true)
    { }

    typedef MemoryClassInfo ClassInfo;

    MemoryObjectType objectType() const { return m_objectType; }
    size_t objectSize() const { return m_objectSize; }
    const void* reportedPointer() const { return m_pointer; }
    bool firstVisit() const { return m_firstVisit; }

    void setClassName(const String& className) { m_className = className; }
    const String& className() const { return m_className; }
    void setName(const String& name) { m_name = name; }
    const String& name() const { return m_name; }

    MemoryInstrumentation* memoryInstrumentation() { return m_memoryInstrumentation; }

private:
    friend class MemoryClassInfo;
    friend class MemoryInstrumentation;

    void reportObjectInfo(const void* pointer, MemoryObjectType objectType, size_t objectSize)
    {
        if (!m_objectSize) {
            if (m_pointer != pointer && m_pointer && m_memoryInstrumentation->visited(pointer))
                m_firstVisit = false;
            m_pointer = pointer;
            m_objectSize = objectSize;
            if (objectType)
                m_objectType = objectType;
        }
    }

    MemoryInstrumentation* m_memoryInstrumentation;
    MemoryObjectType m_objectType;
    size_t m_objectSize;
    const void* m_pointer;
    bool m_firstVisit;
    String m_className;
    String m_name;
};

} // namespace WTF

#endif // !defined(MemoryObjectInfo_h)
