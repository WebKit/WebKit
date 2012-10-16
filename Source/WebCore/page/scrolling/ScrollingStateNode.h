/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef ScrollingStateNode_h
#define ScrollingStateNode_h

#if ENABLE(THREADED_SCROLLING)

#include "GraphicsLayer.h"
#include "ScrollingCoordinator.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

class ScrollingStateTree;

class ScrollingStateNode {
    WTF_MAKE_NONCOPYABLE(ScrollingStateNode);

public:
    ScrollingStateNode(ScrollingStateTree*, ScrollingNodeID);
    virtual ~ScrollingStateNode();

    virtual bool isScrollingStateScrollingNode() { return false; }

    virtual PassOwnPtr<ScrollingStateNode> cloneAndResetNode() = 0;
    void cloneAndResetChildNodes(ScrollingStateNode*);

    virtual bool hasChangedProperties() const = 0;
    virtual unsigned changedProperties() const = 0;
    virtual void resetChangedProperties() = 0;
    virtual void setHasChangedProperties() { setScrollLayerDidChange(true); }

    PlatformLayer* platformScrollLayer() const;
    void setScrollLayer(const GraphicsLayer*);
    void setScrollLayer(PlatformLayer*);

    bool scrollLayerDidChange() const { return m_scrollLayerDidChange; }
    void setScrollLayerDidChange(bool scrollLayerDidChange) { m_scrollLayerDidChange = scrollLayerDidChange; }

    ScrollingStateTree* scrollingStateTree() const { return m_scrollingStateTree; }
    void setScrollingStateTree(ScrollingStateTree* tree) { m_scrollingStateTree = tree; }

    ScrollingNodeID scrollingNodeID() const { return m_nodeID; }

    ScrollingStateNode* parent() const { return m_parent; }
    void setParent(ScrollingStateNode* parent) { m_parent = parent; }

    Vector<OwnPtr<ScrollingStateNode> >* children() const { return m_children.get(); }

    void appendChild(PassOwnPtr<ScrollingStateNode>);
    void removeChild(ScrollingStateNode*);

protected:
    ScrollingStateNode(ScrollingStateNode*);

    ScrollingStateTree* m_scrollingStateTree;

private:
    ScrollingNodeID m_nodeID;

    ScrollingStateNode* m_parent;
    OwnPtr<Vector<OwnPtr<ScrollingStateNode> > > m_children;

    bool m_scrollLayerDidChange;

#if PLATFORM(MAC)
    RetainPtr<PlatformLayer> m_platformScrollLayer;
#endif

};

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)

#endif // ScrollingStateNode_h
