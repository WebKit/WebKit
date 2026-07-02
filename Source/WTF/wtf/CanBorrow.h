/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <utility>
#include <wtf/Assertions.h>

namespace WTF {

template<typename T> class Borrow;

/**
 * @brief CanBorrow is a base class that provides support for Borrow<T>.
 *
 * Inheriting from CanBorrow gives a class the bookkeeping needed by Borrow:
 * a boolean flag that tracks whether the object is currently borrowed, and a
 * destructor that crashes if the object is destroyed while borrowed.
 *
 * Classes that want tighter control over memory layout may implement the
 * CanBorrow protocol by hand (providing setIsBorrowed() and
 * crashIfBorrowed()) instead of inheriting from this class.
 *
 * @code
 * class MyContainer : public CanBorrow {
 *     void mutate() {
 *         crashIfBorrowed(); // Ensure no outstanding Borrow before mutating.
 *         // ... destructive operation ...
 *     }
 * };
 * @endcode
 *
 * @see Borrow
 */
class CanBorrow {
public:
    ~CanBorrow()
    {
        // FIXME: Make this a RELEASE_ASSERT once we've proven it's stable.
        ASSERT(!m_isBorrowed);
    }

    void crashIfBorrowed()
    {
        // FIXME: Make this a RELEASE_ASSERT once we've proven it's stable.
        ASSERT(!m_isBorrowed);
    }

private:
    template<typename T> friend class Borrow;

    bool setIsBorrowed(bool isBorrowed) const
    {
        return std::exchange(m_isBorrowed, isBorrowed);
    }

    mutable bool m_isBorrowed { false };
};

} // namespace WTF

using WTF::CanBorrow;
