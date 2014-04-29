/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013 Igalia S.L. All rights reserved.
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

#ifndef OrderIterator_h
#define OrderIterator_h

#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {

class RenderBox;

class OrderIterator {
public:
    friend class OrderIteratorPopulator;

    RenderBox* currentChild() const;
    RenderBox* first();
    RenderBox* next();

    void invalidate();

private:
    void reset();

    Vector<std::pair<RenderBox*, int>> m_children;
    size_t m_childrenIndex;
};

class OrderIteratorPopulator {
public:
    OrderIteratorPopulator(OrderIterator& iterator)
        : m_iterator(iterator)
        , m_childIndex(0)
        , m_allChildrenHaveDefaultOrderValue(true)
    {
        m_iterator.invalidate();
    }

    ~OrderIteratorPopulator();

    void collectChild(RenderBox&);

private:
    OrderIterator& m_iterator;
    size_t m_childIndex;
    bool m_allChildrenHaveDefaultOrderValue;
};

} // namespace WebCore

#endif //  OrderIterator_h
