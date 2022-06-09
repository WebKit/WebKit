/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if !ENABLE(C_LOOP)

#include "RegisterSet.h"

namespace JSC {

class HashableRegisterSet : public RegisterSet {
    enum State {
        Normal,
        Empty,
        Deleted
    };

public:
    constexpr HashableRegisterSet() { }

    template<typename... Regs>
    constexpr explicit HashableRegisterSet(Regs... regs)
        : RegisterSet(regs...)
    {
    }

    enum EmptyValueTag { EmptyValue };
    enum DeletedValueTag { DeletedValue };
    
    HashableRegisterSet(EmptyValueTag)
        : m_state(Empty)
    {
    }
    
    HashableRegisterSet(DeletedValueTag)
        : m_state(Deleted)
    {
    }
    
    bool isEmptyValue() const
    {
        return m_state == Empty;
    }
    
    bool isDeletedValue() const
    {
        return m_state == Deleted;
    }
    
private:
    State m_state { Normal };
};

struct RegisterSetHash {
    static unsigned hash(const HashableRegisterSet& set) { return set.hash(); }
    static bool equal(const HashableRegisterSet& a, const HashableRegisterSet& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::HashableRegisterSet> : JSC::RegisterSetHash { };

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::HashableRegisterSet> : public CustomHashTraits<JSC::HashableRegisterSet> { };

} // namespace WTF

#endif // !ENABLE(C_LOOP)

