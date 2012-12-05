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

#ifndef AreaAllocator_h
#define AreaAllocator_h

#include <IntPoint.h>
#include <IntRect.h>
#include <IntSize.h>

#if USE(COORDINATED_GRAPHICS)

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
} // namespace WebCore

namespace WebKit {

class AreaAllocator {
public:
    AreaAllocator(const WebCore::IntSize&);
    virtual ~AreaAllocator();

    WebCore::IntSize size() const { return m_size; }

    WebCore::IntSize minimumAllocation() const { return m_minAlloc; }
    void setMinimumAllocation(const WebCore::IntSize& size) { m_minAlloc = size; }

    WebCore::IntSize margin() const { return m_margin; }
    void setMargin(const WebCore::IntSize &margin) { m_margin = margin; }

    virtual void expand(const WebCore::IntSize&);
    void expandBy(const WebCore::IntSize&);

    virtual WebCore::IntRect allocate(const WebCore::IntSize&) = 0;
    virtual void release(const WebCore::IntRect&);

    virtual int overhead() const;

protected:
    WebCore::IntSize m_size;
    WebCore::IntSize m_minAlloc;
    WebCore::IntSize m_margin;

    WebCore::IntSize roundAllocation(const WebCore::IntSize&) const;
};

class GeneralAreaAllocator : public AreaAllocator {
public:
    GeneralAreaAllocator(const WebCore::IntSize&);
    virtual ~GeneralAreaAllocator();

    void expand(const WebCore::IntSize&);
    WebCore::IntRect allocate(const WebCore::IntSize&);
    void release(const WebCore::IntRect&);
    int overhead() const;

private:
    enum Split { SplitOnX, SplitOnY };

    struct Node {
        WebCore::IntRect rect;
        WebCore::IntSize largestFree;
        Node* parent;
        Node* left;
        Node* right;
    };

    Node* m_root;
    int m_nodeCount;

    static void freeNode(Node*);
    WebCore::IntPoint allocateFromNode(const WebCore::IntSize&, Node*);
    Node* splitNode(Node*, Split);
    static void updateLargestFree(Node*);
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

#endif // AreaAllocator_h
