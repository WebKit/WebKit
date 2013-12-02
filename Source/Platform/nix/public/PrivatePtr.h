/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef Nix_PrivatePtr_h
#define Nix_PrivatePtr_h

#include "Common.h"

#if BUILDING_NIX__
#include <wtf/PassRefPtr.h>
#endif

#include <cassert>

namespace Nix {

// This class is an implementation detail of the Nix API. It exists
// to help simplify the implementation of Nix interfaces that merely
// wrap a reference counted WebCore class.
template <typename T>
class PrivatePtr {
public:
    PrivatePtr() : m_ptr(0) { }
    ~PrivatePtr() { assert(!m_ptr); }

    bool isNull() const { return !m_ptr; }

#if BUILDING_NIX__
    PrivatePtr(const PassRefPtr<T>& prp)
        : m_ptr(prp.leakRef())
    {
    }

    void reset()
    {
        assign(0);
    }

    PrivatePtr<T>& operator=(const PrivatePtr<T>& other)
    {
        T* p = other.m_ptr;
        if (p)
            p->ref();
        assign(p);
        return *this;
    }

    PrivatePtr<T>& operator=(const PassRefPtr<T>& prp)
    {
        assign(prp.leakRef());
        return *this;
    }

    T* get() const
    {
        return m_ptr;
    }

    T* operator->() const
    {
        assert(m_ptr);
        return m_ptr;
    }

private:
    void assign(T* p)
    {
        // p is already ref'd for us by the caller
        if (m_ptr)
            m_ptr->deref();
        m_ptr = p;
    }
#endif

private:

    T* m_ptr;
};

} // namespace Nix

#endif // Nix_PrivatePtr_h

