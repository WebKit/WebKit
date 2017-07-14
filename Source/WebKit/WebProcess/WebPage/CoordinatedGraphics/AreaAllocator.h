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

#if USE(COORDINATED_GRAPHICS)

#include <WebCore/IntPoint.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>

namespace WebKit {

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

inline WebCore::IntSize nextPowerOfTwo(const WebCore::IntSize& size)
{
    return WebCore::IntSize(nextPowerOfTwo(size.width()), nextPowerOfTwo(size.height()));
}

class AreaAllocator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit AreaAllocator(const WebCore::IntSize&);
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

class GeneralAreaAllocator final : public AreaAllocator {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit GeneralAreaAllocator(const WebCore::IntSize&);
    virtual ~GeneralAreaAllocator();

    void expand(const WebCore::IntSize&) override;
    WebCore::IntRect allocate(const WebCore::IntSize&) override;
    void release(const WebCore::IntRect&) override;
    int overhead() const override;

private:
    enum Split { SplitOnX, SplitOnY };

    struct Node {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        WebCore::IntRect rect;
        WebCore::IntSize largestFree;
        Node* parent { nullptr };
        Node* left { nullptr };
        Node* right { nullptr };
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
