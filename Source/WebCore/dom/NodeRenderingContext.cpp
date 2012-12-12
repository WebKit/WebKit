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

#include "config.h"
#include "NodeRenderingContext.h"

#include "ComposedShadowTreeWalker.h"
#include "ContainerNode.h"
#include "ContentDistributor.h"
#include "ElementShadow.h"
#include "FlowThreadController.h"
#include "HTMLContentElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLShadowElement.h"
#include "Node.h"
#include "PseudoElement.h"
#include "RenderFullScreen.h"
#include "RenderNamedFlowThread.h"
#include "RenderObject.h"
#include "RenderText.h"
#include "RenderView.h"
#include "ShadowRoot.h"
#include "StyleInheritedData.h"
#include "StyleResolver.h"
#include "Text.h"

#if ENABLE(SVG)
#include "SVGNames.h"
#endif

namespace WebCore {

using namespace HTMLNames;

NodeRenderingContext::NodeRenderingContext(Node* node)
    : m_node(node)
    , m_parentFlowRenderer(0)
{
    ComposedShadowTreeWalker::findParent(m_node, &m_parentDetails);
}

NodeRenderingContext::NodeRenderingContext(Node* node, RenderStyle* style)
    : m_node(node)
    , m_style(style)
    , m_parentFlowRenderer(0)
{
}

NodeRenderingContext::~NodeRenderingContext()
{
}

RenderObject* NodeRenderingContext::nextRenderer() const
{
    if (RenderObject* renderer = m_node->renderer())
        return renderer->nextSibling();

    if (m_parentFlowRenderer)
        return m_parentFlowRenderer->nextRendererForNode(m_node);

    // Avoid an O(N^2) problem with this function by not checking for
    // nextRenderer() when the parent element hasn't attached yet.
    if (m_parentDetails.node() && !m_parentDetails.node()->attached())
        return 0;

    ComposedShadowTreeWalker walker(m_node);
    for (walker.pseudoAwareNextSibling(); walker.get(); walker.pseudoAwareNextSibling()) {
        if (RenderObject* renderer = walker.get()->renderer()) {
            // Renderers for elements attached to a flow thread should be skipped because they are parented differently.
            if (renderer->node()->isElementNode() && renderer->style() && !renderer->style()->flowThread().isEmpty())
                continue;
            return renderer;
        }
    }

    return 0;
}

RenderObject* NodeRenderingContext::previousRenderer() const
{
    if (RenderObject* renderer = m_node->renderer())
        return renderer->previousSibling();

    if (m_parentFlowRenderer)
        return m_parentFlowRenderer->previousRendererForNode(m_node);

    // FIXME: We should have the same O(N^2) avoidance as nextRenderer does
    // however, when I tried adding it, several tests failed.

    ComposedShadowTreeWalker walker(m_node);
    for (walker.pseudoAwarePreviousSibling(); walker.get(); walker.pseudoAwarePreviousSibling()) {
        if (RenderObject* renderer = walker.get()->renderer()) {
            // Renderers for elements attached to a flow thread should be skipped because they are parented differently.
            if (renderer->node()->isElementNode() && renderer->style() && !renderer->style()->flowThread().isEmpty())
                continue;
            return renderer;
        }
    }

    return 0;
}

RenderObject* NodeRenderingContext::parentRenderer() const
{
    if (RenderObject* renderer = m_node->renderer())
        return renderer->parent();
    if (m_parentFlowRenderer)
        return m_parentFlowRenderer;

    return m_parentDetails.node() ? m_parentDetails.node()->renderer() : 0;
}

bool NodeRenderingContext::shouldCreateRenderer() const
{
    if (!m_node->document()->shouldCreateRenderers())
        return false;
    if (!m_parentDetails.node())
        return false;
    RenderObject* parentRenderer = this->parentRenderer();
    if (!parentRenderer)
        return false;
    if (!parentRenderer->canHaveChildren())
        return false;
    if (!m_parentDetails.node()->childShouldCreateRenderer(*this))
        return false;
    return true;
}

void NodeRenderingContext::moveToFlowThreadIfNeeded()
{
    ASSERT(m_node->isElementNode());
    ASSERT(m_style);
    if (!m_node->document()->cssRegionsEnabled())
        return;

    if (m_style->flowThread().isEmpty())
        return;

    // FIXME: Do not collect elements if they are in shadow tree.
    if (m_node->isInShadowTree())
        return;

#if ENABLE(SVG)
    // Allow only svg root elements to be directly collected by a render flow thread.
    if (m_node->isSVGElement()
        && (!(m_node->hasTagName(SVGNames::svgTag) && m_node->parentNode() && !m_node->parentNode()->isSVGElement())))
        return;
#endif

    m_flowThread = m_style->flowThread();
    ASSERT(m_node->document()->renderView());
    FlowThreadController* flowThreadController = m_node->document()->renderView()->flowThreadController();
    m_parentFlowRenderer = flowThreadController->ensureRenderFlowThreadWithName(m_flowThread);
    flowThreadController->registerNamedFlowContentNode(m_node, m_parentFlowRenderer);
}

bool NodeRenderingContext::isOnEncapsulationBoundary() const
{
    return isOnUpperEncapsulationBoundary() || isLowerEncapsulationBoundary(m_parentDetails.insertionPoint()) || isLowerEncapsulationBoundary(m_node->parentNode());
}

bool NodeRenderingContext::isOnUpperEncapsulationBoundary() const
{
    return m_node->parentNode() && m_node->parentNode()->isShadowRoot();
}

#if ENABLE(DIALOG_ELEMENT)
static void adjustInsertionPointForTopLayerElement(Element* element, RenderObject*& parentRenderer, RenderObject*& nextRenderer)
{
    parentRenderer = parentRenderer->view();
    nextRenderer = 0;
    const Vector<RefPtr<Element> >& topLayerElements = element->document()->topLayerElements();
    size_t topLayerPosition = topLayerElements.find(element);
    ASSERT(topLayerPosition != notFound);
    // Find the next top layer renderer that's stacked above this element. Note that the immediate next element in the top layer
    // stack might not have a renderer (due to display: none, or possibly it is not attached yet).
    for (size_t i = topLayerPosition + 1; i < topLayerElements.size(); ++i) {
        nextRenderer = topLayerElements[i]->renderer();
        if (nextRenderer) {
            ASSERT(nextRenderer->parent() == parentRenderer);
            break;
        }
    }
}
#endif

void NodeRenderingContext::createRendererForElementIfNeeded()
{
    ASSERT(!m_node->renderer());

    Element* element = toElement(m_node);

    if (!shouldCreateRenderer())
        return;
    m_style = element->styleForRenderer();
    ASSERT(m_style);

    moveToFlowThreadIfNeeded();

    if (!element->rendererIsNeeded(*this))
        return;

    RenderObject* parentRenderer = this->parentRenderer();
    RenderObject* nextRenderer = this->nextRenderer();
    
#if ENABLE(DIALOG_ELEMENT)
    if (element->isInTopLayer())
        adjustInsertionPointForTopLayerElement(element, parentRenderer, nextRenderer);
#endif

    Document* document = element->document();
    RenderObject* newRenderer = element->createRenderer(document->renderArena(), m_style.get());
    if (!newRenderer)
        return;
    if (!parentRenderer->isChildAllowed(newRenderer, m_style.get())) {
        newRenderer->destroy();
        return;
    }

    // Make sure the RenderObject already knows it is going to be added to a RenderFlowThread before we set the style
    // for the first time. Otherwise code using inRenderFlowThread() in the styleWillChange and styleDidChange will fail.
    newRenderer->setInRenderFlowThread(parentRenderer->inRenderFlowThread());

    element->setRenderer(newRenderer);
    newRenderer->setAnimatableStyle(m_style.release()); // setAnimatableStyle() can depend on renderer() already being set.

#if ENABLE(FULLSCREEN_API)
    if (document->webkitIsFullScreen() && document->webkitCurrentFullScreenElement() == element) {
        newRenderer = RenderFullScreen::wrapRenderer(newRenderer, parentRenderer, document);
        if (!newRenderer)
            return;
    }
#endif
    // Note: Adding newRenderer instead of renderer(). renderer() may be a child of newRenderer.
    parentRenderer->addChild(newRenderer, nextRenderer);
}

void NodeRenderingContext::createRendererForTextIfNeeded()
{
    ASSERT(!m_node->renderer());

    Text* textNode = toText(m_node);

    if (!shouldCreateRenderer())
        return;

    RenderObject* parentRenderer = this->parentRenderer();
    ASSERT(parentRenderer);
    Document* document = textNode->document();

    if (resetStyleInheritance())
        m_style = document->styleResolver()->defaultStyleForElement();
    else
        m_style = parentRenderer->style();

    if (!textNode->textRendererIsNeeded(*this))
        return;
    RenderText* newRenderer = textNode->createTextRenderer(document->renderArena(), m_style.get());
    if (!newRenderer)
        return;
    if (!parentRenderer->isChildAllowed(newRenderer, m_style.get())) {
        newRenderer->destroy();
        return;
    }

    // Make sure the RenderObject already knows it is going to be added to a RenderFlowThread before we set the style
    // for the first time. Otherwise code using inRenderFlowThread() in the styleWillChange and styleDidChange will fail.
    newRenderer->setInRenderFlowThread(parentRenderer->inRenderFlowThread());

    RenderObject* nextRenderer = this->nextRenderer();
    textNode->setRenderer(newRenderer);
    // Parent takes care of the animations, no need to call setAnimatableStyle.
    newRenderer->setStyle(m_style.release());
    parentRenderer->addChild(newRenderer, nextRenderer);
}

}
