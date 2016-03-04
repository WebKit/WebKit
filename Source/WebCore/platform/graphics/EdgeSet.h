/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef EdgeSet_h
#define EdgeSet_h

namespace WebCore {

template <typename T>
class EdgeSet {
public:
    EdgeSet(T top, T right, T bottom, T left)
        : m_top(top)
        , m_right(right)
        , m_bottom(bottom)
        , m_left(left)
    {
    }

    T top() const { return m_top; }
    void setTop(T top) { m_top = top; }

    T right() const { return m_right; }
    void setRight(T right) { m_right = right; }

    T bottom() const { return m_bottom; }
    void setBottom(T bottom) { m_bottom = bottom; }

    T left() const { return m_left; }
    void setLeft(T left) { m_left = left; }

    bool operator==(const EdgeSet<T>& b) const
    {
        return top() == b.top()
            && right() == b.right()
            && bottom() == b.bottom()
            && left() == b.left();
    }

    bool operator!=(const EdgeSet<T>& b) const
    {
        return !(*this == b);
    }

private:
    T m_top;
    T m_right;
    T m_bottom;
    T m_left;
};

} // namespace WebCore

#endif // EdgeSet_h
