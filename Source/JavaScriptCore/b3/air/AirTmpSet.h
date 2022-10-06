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

#include "AirTmp.h"
#include <wtf/IndexSet.h>

namespace JSC { namespace B3 { namespace Air {

class TmpSet {
public:
    TmpSet()
    {
    }
    
    bool add(Tmp tmp)
    {
        if (tmp.isGP())
            return m_gp.add(tmp);
        return m_fp.add(tmp);
    }
    
    bool remove(Tmp tmp)
    {
        if (tmp.isGP())
            return m_gp.remove(tmp);
        return m_fp.remove(tmp);
    }
    
    bool contains(Tmp tmp)
    {
        if (tmp.isGP())
            return m_gp.contains(tmp);
        return m_fp.contains(tmp);
    }
    
    size_t size() const
    {
        return m_gp.size() + m_fp.size();
    }
    
    bool isEmpty() const
    {
        return !size();
    }

    class iterator {
    public:
        iterator()
        {
        }
        
        iterator(BitVector::iterator gpIter, BitVector::iterator fpIter)
            : m_gpIter(gpIter)
            , m_fpIter(fpIter)
        {
        }
        
        Tmp operator*()
        {
            if (!m_gpIter.isAtEnd())
                return Tmp::tmpForAbsoluteIndex(GP, *m_gpIter);
            return Tmp::tmpForAbsoluteIndex(FP, *m_fpIter);
        }
        
        iterator& operator++()
        {
            if (!m_gpIter.isAtEnd()) {
                ++m_gpIter;
                return *this;
            }
            ++m_fpIter;
            return *this;
        }
        
        bool operator==(const iterator& other) const
        {
            return m_gpIter == other.m_gpIter
                && m_fpIter == other.m_fpIter;
        }
        
        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }
        
    private:
        BitVector::iterator m_gpIter;
        BitVector::iterator m_fpIter;
    };
    
    iterator begin() const { return iterator(m_gp.indices().begin(), m_fp.indices().begin()); }
    iterator end() const { return iterator(m_gp.indices().end(), m_fp.indices().end()); }

private:
    IndexSet<Tmp::AbsolutelyIndexed<GP>> m_gp;
    IndexSet<Tmp::AbsolutelyIndexed<FP>> m_fp;
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

