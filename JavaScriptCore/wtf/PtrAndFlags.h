/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef PtrAndFlags_h
#define PtrAndFlags_h

#include <wtf/Assertions.h>

namespace WTF {
    template<class T, typename FlagEnum> class PtrAndFlagsBase {
    public:
        bool isFlagSet(FlagEnum flagNumber) const { ASSERT(flagNumber < 2); return m_ptrAndFlags & (1 << flagNumber); }
        void setFlag(FlagEnum flagNumber) { ASSERT(flagNumber < 2); m_ptrAndFlags |= (1 << flagNumber);}
        void clearFlag(FlagEnum flagNumber) { ASSERT(flagNumber < 2); m_ptrAndFlags &= ~(1 << flagNumber);}
        T* get() const { return reinterpret_cast<T*>(m_ptrAndFlags & ~3); }
        void set(T* ptr)
        {
            ASSERT(!(reinterpret_cast<intptr_t>(ptr) & 3));
            m_ptrAndFlags = reinterpret_cast<intptr_t>(ptr) | (m_ptrAndFlags & 3);
#ifndef NDEBUG
            m_leaksPtr = ptr;
#endif
        }

        bool operator!() const { return !get(); }
        T* operator->() const { return reinterpret_cast<T*>(m_ptrAndFlags & ~3); }

    protected:
        intptr_t m_ptrAndFlags;
#ifndef NDEBUG
        void* m_leaksPtr; // Only used to allow tools like leaks on OSX to detect that the memory is referenced.
#endif
    };

    template<class T, typename FlagEnum> class PtrAndFlags : public PtrAndFlagsBase<T, FlagEnum> {
    public:
        PtrAndFlags()
        {
            PtrAndFlagsBase<T, FlagEnum>::m_ptrAndFlags = 0;
        }
        PtrAndFlags(T* ptr)
        {
            PtrAndFlagsBase<T, FlagEnum>::m_ptrAndFlags = 0;
            PtrAndFlagsBase<T, FlagEnum>::set(ptr);
        }
    };
} // namespace WTF

using WTF::PtrAndFlagsBase;
using WTF::PtrAndFlags;

#endif // PtrAndFlags_h
