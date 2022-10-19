/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

#include "AirCode.h"
#include "AirTmp.h"
#include <wtf/IndexMap.h>

namespace JSC { namespace B3 { namespace Air {

// As an alternative to this, you could use IndexMap<Tmp::LinearlyIndexed, ...>, but this would fail
// as soon as you added a new GP tmp.

template<typename Value>
class TmpMap {
public:
    TmpMap() = default;
    TmpMap(TmpMap&&) = default;
    TmpMap& operator=(TmpMap&&) = default;
    
    template<typename... Args>
    TmpMap(Code& code, const Args&... args)
        : m_gp(Tmp::absoluteIndexEnd(code, GP), args...)
        , m_fp(Tmp::absoluteIndexEnd(code, FP), args...)
    {
    }
    
    template<typename... Args>
    void resize(Code& code, const Args&... args)
    {
        m_gp.resize(Tmp::absoluteIndexEnd(code, GP), args...);
        m_fp.resize(Tmp::absoluteIndexEnd(code, FP), args...);
    }
    
    template<typename... Args>
    void clear(const Args&... args)
    {
        m_gp.remove(args...);
        m_fp.remove(args...);
    }
    
    const Value& operator[](Tmp tmp) const
    {
        if (tmp.isGP())
            return m_gp[tmp];
        return m_fp[tmp];
    }

    Value& operator[](Tmp tmp)
    {
        if (tmp.isGP())
            return m_gp[tmp];
        return m_fp[tmp];
    }
    
    template<typename PassedValue>
    void append(Tmp tmp, PassedValue&& value)
    {
        if (tmp.isGP())
            m_gp.append(tmp, std::forward<PassedValue>(value));
        else
            m_fp.append(tmp, std::forward<PassedValue>(value));
    }

private:
    IndexMap<Tmp::AbsolutelyIndexed<GP>, Value> m_gp;
    IndexMap<Tmp::AbsolutelyIndexed<FP>, Value> m_fp;
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

