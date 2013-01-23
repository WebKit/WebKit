/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef ScriptWrappable_h
#define ScriptWrappable_h

#include "WebCoreMemoryInstrumentation.h"
#include <v8.h>

namespace WebCore {

class ScriptWrappable {
public:
    ScriptWrappable() { }

    v8::Persistent<v8::Object> wrapper() const
    {
        return v8::Persistent<v8::Object>(maskOrUnmaskPointer(*m_maskedWrapper));
    }

    void setWrapper(v8::Persistent<v8::Object> wrapper)
    {
        m_maskedWrapper = maskOrUnmaskPointer(*wrapper);
    }

    void clearWrapper()
    {
        ASSERT(!m_maskedWrapper.IsEmpty());
        m_maskedWrapper.Clear();
    }

    void disposeWrapper()
    {
        ASSERT(!m_maskedWrapper.IsEmpty());
        m_maskedWrapper = wrapper();
        m_maskedWrapper.Dispose();
        m_maskedWrapper.Clear();
    }

    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
        info.ignoreMember(m_maskedWrapper);
    }

private:
    v8::Persistent<v8::Object> m_maskedWrapper;

    static inline v8::Object* maskOrUnmaskPointer(const v8::Object* object)
    {
        const uintptr_t objectPointer = reinterpret_cast<uintptr_t>(object);
        const uintptr_t randomMask = ~(reinterpret_cast<uintptr_t>(&WebCoreMemoryTypes::DOM) >> 13); // Entropy via ASLR.
        return reinterpret_cast<v8::Object*>((objectPointer ^ randomMask) & (!objectPointer - 1)); // Preserve null without branching.
    }
};

} // namespace WebCore

#endif // ScriptWrappable_h
