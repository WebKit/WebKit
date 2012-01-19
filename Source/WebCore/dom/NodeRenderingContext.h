/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef NodeRenderingContext_h
#define NodeRenderingContext_h

#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class ContainerNode;
class Document;
class Node;
class RenderFlowThread;
class RenderObject;
class RenderStyle;
class HTMLContentElement;
class ShadowRoot;

class NodeRenderingContext {
public:
    explicit NodeRenderingContext(Node*);
    NodeRenderingContext(Node*, RenderStyle*);
    ~NodeRenderingContext();

    Node* node() const;
    ContainerNode* parentNodeForRenderingAndStyle() const;
    RenderObject* parentRenderer() const;
    RenderObject* nextRenderer() const;
    RenderObject* previousRenderer() const;
    HTMLContentElement* includer() const;

    RenderStyle* style() const;
    void setStyle(PassRefPtr<RenderStyle>);
    PassRefPtr<RenderStyle> releaseStyle();

    bool shouldCreateRenderer() const;

    void hostChildrenChanged();

    bool hasFlowThreadParent() const { return m_parentFlowRenderer; }
    RenderFlowThread* parentFlowRenderer() const { return m_parentFlowRenderer; }
    void moveToFlowThreadIfNeeded();

private:

    enum TreeLocation {
        LocationUndetermined,
        LocationNotInTree,
        LocationLightChild,
        LocationShadowChild,
    };

    enum AttachPhase {
        AttachStraight,
        AttachContentLight,
        AttachContentForwarded,
        AttachContentFallback,
    };

    TreeLocation m_location;
    AttachPhase m_phase;
    Node* m_node;
    ContainerNode* m_parentNodeForRenderingAndStyle;
    ShadowRoot* m_visualParentShadowRoot;
    HTMLContentElement* m_includer;
    RefPtr<RenderStyle> m_style;
    RenderFlowThread* m_parentFlowRenderer;
    AtomicString m_flowThread;
};

inline Node* NodeRenderingContext::node() const
{
    return m_node;
}

inline ContainerNode* NodeRenderingContext::parentNodeForRenderingAndStyle() const
{
    ASSERT(m_location != LocationUndetermined);
    return m_parentNodeForRenderingAndStyle;
}

inline RenderStyle* NodeRenderingContext::style() const
{
    return m_style.get();
}

inline HTMLContentElement* NodeRenderingContext::includer() const
{
    return m_includer;
}

class NodeRendererFactory {
    WTF_MAKE_NONCOPYABLE(NodeRendererFactory);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit NodeRendererFactory(Node*);

    const NodeRenderingContext& context() const;
    void createRendererIfNeeded();

private:
    RenderObject* createRenderer();

    NodeRenderingContext m_context;
};

inline const NodeRenderingContext& NodeRendererFactory::context() const
{
    return m_context;
}

} // namespace WebCore

#endif
