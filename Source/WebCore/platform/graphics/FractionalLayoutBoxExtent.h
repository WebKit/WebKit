/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
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

#ifndef FractionalLayoutBoxExtent_h
#define FractionalLayoutBoxExtent_h

#include "FractionalLayoutUnit.h"

namespace WebCore {

class RenderStyle;

class FractionalLayoutBoxExtent {
public:
    FractionalLayoutBoxExtent() : m_top(0), m_right(0), m_bottom(0), m_left(0) { }
    FractionalLayoutBoxExtent(FractionalLayoutUnit top, FractionalLayoutUnit right, FractionalLayoutUnit bottom, FractionalLayoutUnit left)
        : m_top(top), m_right(right), m_bottom(bottom), m_left(left) { }

    inline FractionalLayoutUnit top() const { return m_top; }
    inline FractionalLayoutUnit right() const { return m_right; }
    inline FractionalLayoutUnit bottom() const { return m_bottom; }
    inline FractionalLayoutUnit left() const { return m_left; }

    inline void setTop(FractionalLayoutUnit value) { m_top = value; }
    inline void setRight(FractionalLayoutUnit value) { m_right = value; }
    inline void setBottom(FractionalLayoutUnit value) { m_bottom = value; }
    inline void setLeft(FractionalLayoutUnit value) { m_left = value; }

    FractionalLayoutUnit logicalTop(const RenderStyle*) const;
    FractionalLayoutUnit logicalBottom(const RenderStyle*) const;
    FractionalLayoutUnit logicalLeft(const RenderStyle*) const;
    FractionalLayoutUnit logicalRight(const RenderStyle*) const;

    FractionalLayoutUnit before(const RenderStyle*) const;
    FractionalLayoutUnit after(const RenderStyle*) const;
    FractionalLayoutUnit start(const RenderStyle*) const;
    FractionalLayoutUnit end(const RenderStyle*) const;

    void setBefore(const RenderStyle*, FractionalLayoutUnit);
    void setAfter(const RenderStyle*, FractionalLayoutUnit);
    void setStart(const RenderStyle*, FractionalLayoutUnit);
    void setEnd(const RenderStyle*, FractionalLayoutUnit);

    FractionalLayoutUnit& mutableLogicalLeft(const RenderStyle*);
    FractionalLayoutUnit& mutableLogicalRight(const RenderStyle*);

    FractionalLayoutUnit& mutableBefore(const RenderStyle*);
    FractionalLayoutUnit& mutableAfter(const RenderStyle*);

private:
    FractionalLayoutUnit m_top;
    FractionalLayoutUnit m_right;
    FractionalLayoutUnit m_bottom;
    FractionalLayoutUnit m_left;
};

} // namespace WebCore

#endif // FractionalLayoutBoxExtent_h
