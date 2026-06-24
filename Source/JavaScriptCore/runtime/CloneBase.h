/*
 * Copyright (C) 2009-2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/ArgList.h>
#include <JavaScriptCore/StructuredCloneTags.h>
#include <wtf/DataLog.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/StackCheck.h>
#include <wtf/Vector.h>

namespace JSC {

inline constexpr bool verboseCloneTrace = false;

#define SERIALIZE_TRACE(...) do { \
        if constexpr (::JSC::verboseCloneTrace) \
            dataLogLn("TRACE ", __VA_ARGS__, " @ ", __LINE__); \
    } while (false)

class JSGlobalObject;
class CachedString;

// Shared infrastructure for both CloneSerializerBase and CloneDeserializerBase.
class CloneBase {
    WTF_FORBID_HEAP_ALLOCATION;
    friend class CachedString;
protected:
    CloneBase(JSGlobalObject* lexicalGlobalObject)
        : m_lexicalGlobalObject(lexicalGlobalObject)
        , m_failed(false)
    {
    }

    void fail()
    {
        m_failed = true;
    }

#if ASSERT_ENABLED
public:
    const Vector<SerializationTag>& objectPoolTags() const { return m_objectPoolTags; }

protected:
    void appendObjectPoolTag(SerializationTag tag)
    {
        m_objectPoolTags.append(tag);
    }
#else
    ALWAYS_INLINE void appendObjectPoolTag(SerializationTag) { }
#endif
    bool isSafeToRecurse()
    {
        return m_stackCheck.isSafeToRecurse();
    }

    JSGlobalObject* const m_lexicalGlobalObject { nullptr };
    bool m_failed { false };
    MarkedArgumentBuffer m_keepAliveBuffer;
    MarkedArgumentBuffer m_objectPool;
#if ASSERT_ENABLED
    Vector<SerializationTag> m_objectPoolTags;
#endif
    StackCheck m_stackCheck;
};

} // namespace JSC
