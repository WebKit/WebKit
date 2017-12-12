/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#if USE(COORDINATED_GRAPHICS)

#include "IntPoint.h"
#include "IntRect.h"
#include "IntSize.h"

namespace WebCore {

inline int nextPowerOfTwo(int number)
{
    // This is a fast trick to get nextPowerOfTwo for an integer.
    --number;
    number |= number >> 1;
    number |= number >> 2;
    number |= number >> 4;
    number |= number >> 8;
    number |= number >> 16;
    number++;
    return number;
}

inline IntSize nextPowerOfTwo(const IntSize& size)
{
    return IntSize(nextPowerOfTwo(size.width()), nextPowerOfTwo(size.height()));
}

class AreaAllocator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit AreaAllocator(const IntSize&);
    virtual ~AreaAllocator();

    IntSize size() const { return m_size; }

    IntSize minimumAllocation() const { return m_minAlloc; }
    void setMinimumAllocation(const IntSize& size) { m_minAlloc = size; }

    IntSize margin() const { return m_margin; }
    void setMargin(const IntSize &margin) { m_margin = margin; }

    virtual void expand(const IntSize&);
    void expandBy(const IntSize&);

    virtual IntRect allocate(const IntSize&) = 0;
    virtual void release(const IntRect&);

    virtual int overhead() const;

protected:
    IntSize m_size;
    IntSize m_minAlloc;
    IntSize m_margin;

    IntSize roundAllocation(const IntSize&) const;
};

class GeneralAreaAllocator final : public AreaAllocator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit GeneralAreaAllocator(const IntSize&);
    virtual ~GeneralAreaAllocator();

    void expand(const IntSize&) override;
    IntRect allocate(const IntSize&) override;
    void release(const IntRect&) override;
    int overhead() const override;

private:
    enum Split { SplitOnX, SplitOnY };

    struct Node {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        IntRect rect;
        IntSize largestFree;
        Node* parent { nullptr };
        Node* left { nullptr };
        Node* right { nullptr };
    };

    Node* m_root;
    int m_nodeCount;

    static void freeNode(Node*);
    IntPoint allocateFromNode(const IntSize&, Node*);
    Node* splitNode(Node*, Split);
    static void updateLargestFree(Node*);
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
