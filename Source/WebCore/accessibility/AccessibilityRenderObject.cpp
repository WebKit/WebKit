/*
* Copyright (C) 2008-2020 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1.  Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
* 2.  Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in the
*     documentation and/or other materials provided with the distribution.
* 3.  Neither the name of Apple Inc. ("Apple") nor the names of
*     its contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "AccessibilityRenderObject.h"

#include "AXLogger.h"
#include "AXObjectCache.h"
#include "AccessibilityImageMapLink.h"
#include "AccessibilityListBox.h"
#include "AccessibilitySVGRoot.h"
#include "AccessibilitySpinButton.h"
#include "AccessibilityTable.h"
#include "CachedImage.h"
#include "ComposedTreeIterator.h"
#include "DocumentSVG.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "ElementAncestorIteratorInlines.h"
#include "FloatRect.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "HTMLAreaElement.h"
#include "HTMLBRElement.h"
#include "HTMLDetailsElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLMapElement.h"
#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLOptionsCollection.h"
#include "HTMLSelectElement.h"
#include "HTMLSummaryElement.h"
#include "HTMLTableElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLVideoElement.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Image.h"
#include "LegacyRenderSVGRoot.h"
#include "LegacyRenderSVGShape.h"
#include "LocalFrame.h"
#include "LocalizedStrings.h"
#include "NodeList.h"
#include "Page.h"
#include "PathUtilities.h"
#include "PluginViewBase.h"
#include "ProgressTracker.h"
#include "Range.h"
#include "RenderButton.h"
#include "RenderFileUploadControl.h"
#include "RenderHTMLCanvas.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "RenderLineBreak.h"
#include "RenderListBox.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderMathMLBlock.h"
#include "RenderMenuList.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGRoot.h"
#include "RenderSVGShape.h"
#include "RenderTableCell.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderTextControlSingleLine.h"
#include "RenderTextFragment.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "RenderedPosition.h"
#include "SVGDocument.h"
#include "SVGElementTypeHelpers.h"
#include "SVGImage.h"
#include "SVGSVGElement.h"
#include "Text.h"
#include "TextControlInnerElements.h"
#include "TextIterator.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "VisibleUnits.h"
#include <algorithm>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>
#include <wtf/unicode/CharacterNames.h>

#if ENABLE(APPLE_PAY)
#include "ApplePayButtonPart.h"
#endif

namespace WebCore {

using namespace HTMLNames;

AccessibilityRenderObject::AccessibilityRenderObject(RenderObject& renderer)
    : AccessibilityNodeObject(renderer.node())
    , m_renderer(renderer)
{
#if ASSERT_ENABLED
    renderer.setHasAXObject(true);
#endif
}

AccessibilityRenderObject::AccessibilityRenderObject(Node& node)
    : AccessibilityNodeObject(&node)
{
    // We should only ever create an instance of this class with a node if that node has no renderer (i.e. because of display:contents).
    ASSERT(!node.renderer());
}

AccessibilityRenderObject::~AccessibilityRenderObject()
{
    ASSERT(isDetached());
}

Ref<AccessibilityRenderObject> AccessibilityRenderObject::create(RenderObject& renderer)
{
    return adoptRef(*new AccessibilityRenderObject(renderer));
}

void AccessibilityRenderObject::detachRemoteParts(AccessibilityDetachmentType detachmentType)
{
    AccessibilityNodeObject::detachRemoteParts(detachmentType);
    
    detachRemoteSVGRoot();
    
#if ASSERT_ENABLED
    if (m_renderer)
        m_renderer->setHasAXObject(false);
#endif
    m_renderer = nullptr;
}

static inline bool isInlineWithContinuation(RenderObject& renderer)
{
    auto* renderInline = dynamicDowncast<RenderInline>(renderer);
    return renderInline && renderInline->continuation();
}

static inline RenderObject* firstChildInContinuation(RenderBoxModelObject& renderer)
{
    WeakPtr continuation = renderer.continuation();
    while (continuation) {
        if (is<RenderBlock>(*continuation))
            return continuation.get();

        if (auto* child = continuation->firstChild())
            return child;

        continuation = continuation->continuation();
    }

    return nullptr;
}

static inline RenderObject* firstChildConsideringContinuation(RenderObject& renderer)
{
    RenderObject* firstChild = renderer.firstChildSlow();

    // We don't want to include the end of a continuation as the firstChild of the
    // anonymous parent, because everything has already been linked up via continuation.
    // CSS first-letter selector is an example of this case.
    if (renderer.isAnonymous()) {
        auto* renderInline = dynamicDowncast<RenderInline>(firstChild);
        if (renderInline && renderInline->isContinuation())
            firstChild = nullptr;
    }

    if (!firstChild && isInlineWithContinuation(renderer))
        firstChild = firstChildInContinuation(uncheckedDowncast<RenderInline>(renderer));

    return firstChild;
}


static inline RenderObject* lastChildConsideringContinuation(RenderObject& renderer)
{
    if (!is<RenderInline>(renderer) && !is<RenderBlock>(renderer))
        return &renderer;

    auto& boxModelObject = uncheckedDowncast<RenderBoxModelObject>(renderer);
    WeakPtr lastChild = boxModelObject.lastChild();
    for (auto* current = &boxModelObject; current; ) {
        if (auto* newLastChild = current->lastChild())
            lastChild = newLastChild;

        current = current->inlineContinuation();
    }

    return lastChild.get();
}

AccessibilityObject* AccessibilityRenderObject::firstChild() const
{
    if (!m_renderer)
        return AccessibilityNodeObject::firstChild();
    
    if (auto* firstChild = firstChildConsideringContinuation(*m_renderer)) {
        auto* cache = axObjectCache();
        return cache ? cache->getOrCreate(*firstChild) : nullptr;
    }

    // If an object can't have children, then it is using this method to help
    // calculate some internal property (like its description).
    // In this case, it should check the Node level for children in case they're
    // not rendered (like a <meter> element).
    if (!canHaveChildren())
        return AccessibilityNodeObject::firstChild();

    return nullptr;
}

AccessibilityObject* AccessibilityRenderObject::lastChild() const
{
    if (!m_renderer)
        return AccessibilityNodeObject::lastChild();

    if (auto* lastChild = lastChildConsideringContinuation(*m_renderer)) {
        auto* cache = axObjectCache();
        return cache ? cache->getOrCreate(lastChild) : nullptr;
    }

    if (!canHaveChildren())
        return AccessibilityNodeObject::lastChild();

    return nullptr;
}

static inline RenderInline* startOfContinuations(RenderObject& renderer)
{
    WeakPtr renderElement = dynamicDowncast<RenderElement>(renderer);
    if (!renderElement)
        return nullptr;

    if (is<RenderInline>(*renderElement) && renderElement->isContinuation() && is<RenderInline>(renderElement->element()->renderer()))
        return uncheckedDowncast<RenderInline>(renderer.node()->renderer());

    // Blocks with a previous continuation always have a next continuation
    if (auto* renderBlock = dynamicDowncast<RenderBlock>(*renderElement); renderBlock && renderBlock->inlineContinuation())
        return uncheckedDowncast<RenderInline>(renderBlock->inlineContinuation()->element()->renderer());
    return nullptr;
}

static inline RenderObject* endOfContinuations(RenderObject& renderer)
{
    if (!is<RenderInline>(renderer) && !is<RenderBlock>(renderer))
        return &renderer;

    auto* previous = uncheckedDowncast<RenderBoxModelObject>(&renderer);
    for (auto* current = previous; current; ) {
        previous = current;
        current = current->inlineContinuation();
    }

    return previous;
}


static inline RenderObject* childBeforeConsideringContinuations(RenderInline* renderer, RenderObject* child)
{
    RenderObject* previous = nullptr;
    for (RenderBoxModelObject* currentContainer = renderer; currentContainer; ) {
        if (is<RenderInline>(*currentContainer)) {
            auto* current = currentContainer->firstChild();
            while (current) {
                if (current == child)
                    return previous;
                previous = current;
                current = current->nextSibling();
            }

            currentContainer = currentContainer->continuation();
        } else if (is<RenderBlock>(*currentContainer)) {
            if (currentContainer == child)
                return previous;

            previous = currentContainer;
            currentContainer = currentContainer->inlineContinuation();
        }
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

static inline bool firstChildIsInlineContinuation(RenderElement& renderer)
{
    auto* renderInline = dynamicDowncast<RenderInline>(renderer.firstChild());
    return renderInline && renderInline->isContinuation();
}

AccessibilityObject* AccessibilityRenderObject::previousSibling() const
{
    if (!m_renderer)
        return AccessibilityNodeObject::previousSibling();

    RenderObject* previousSibling = nullptr;

    // Case 1: The node is a block and is an inline's continuation. In that case, the inline's
    // last child is our previous sibling (or further back in the continuation chain)
    RenderInline* startOfConts;
    WeakPtr renderBlock = dynamicDowncast<RenderBlock>(*m_renderer);
    if (renderBlock && (startOfConts = startOfContinuations(*m_renderer)))
        previousSibling = childBeforeConsideringContinuations(startOfConts, renderer());

    // Case 2: Anonymous block parent of the end of a continuation - skip all the way to before
    // the parent of the start, since everything in between will be linked up via the continuation.
    else if (renderBlock && m_renderer->isAnonymousBlock() && firstChildIsInlineContinuation(*renderBlock)) {
        auto* firstParent = startOfContinuations(*renderBlock->firstChild())->parent();
        ASSERT(firstParent);
        while (firstChildIsInlineContinuation(*firstParent))
            firstParent = startOfContinuations(*firstParent->firstChild())->parent();
        previousSibling = firstParent->previousSibling();
    }

    // Case 3: The node has an actual previous sibling
    else if (RenderObject* ps = m_renderer->previousSibling())
        previousSibling = ps;

    // Case 4: This node has no previous siblings, but its parent is an inline,
    // and is another node's inline continutation. Follow the continuation chain.
    else if (is<RenderInline>(m_renderer->parent()) && (startOfConts = startOfContinuations(*m_renderer->parent())))
        previousSibling = childBeforeConsideringContinuations(startOfConts, m_renderer->parent()->firstChild());

    if (!previousSibling)
        return nullptr;

    auto* cache = axObjectCache();
    return cache ? cache->getOrCreate(*previousSibling) : nullptr;
}

static inline bool lastChildHasContinuation(RenderElement& renderer)
{
    RenderObject* child = renderer.lastChild();
    return child && isInlineWithContinuation(*child);
}

AccessibilityObject* AccessibilityRenderObject::nextSibling() const
{
    if (!m_renderer)
        return AccessibilityNodeObject::nextSibling();

    if (is<RenderView>(m_renderer))
        return nullptr;

    RenderObject* nextSibling = nullptr;

    // Case 1: node is a block and has an inline continuation. Next sibling is the inline continuation's
    // first child.
    RenderInline* inlineContinuation;
    WeakPtr renderBlock = dynamicDowncast<RenderBlock>(*m_renderer);
    if (renderBlock && (inlineContinuation = renderBlock->inlineContinuation()))
        nextSibling = firstChildConsideringContinuation(*inlineContinuation);

    // Case 2: Anonymous block parent of the start of a continuation - skip all the way to
    // after the parent of the end, since everything in between will be linked up via the continuation.
    else if (renderBlock && m_renderer->isAnonymousBlock() && lastChildHasContinuation(*renderBlock)) {
        auto* lastParent = endOfContinuations(*renderBlock->lastChild())->parent();
        ASSERT(lastParent);
        while (lastChildHasContinuation(*lastParent))
            lastParent = endOfContinuations(*lastParent->lastChild())->parent();
        nextSibling = lastParent->nextSibling();
    }

    // Case 3: node has an actual next sibling
    else if (RenderObject* ns = m_renderer->nextSibling())
        nextSibling = ns;

    // Case 4: node is an inline with a continuation. Next sibling is the next sibling of the end 
    // of the continuation chain.
    else if (isInlineWithContinuation(*m_renderer))
        nextSibling = endOfContinuations(*m_renderer)->nextSibling();

    // Case 5: node has no next sibling, and its parent is an inline with a continuation.
    // Case 5.1: After case 4, (the element was inline w/ continuation but had no sibling), then check it's parent.
    if (!nextSibling && m_renderer->parent() && isInlineWithContinuation(*m_renderer->parent())) {
        auto& continuation = *downcast<RenderInline>(*m_renderer->parent()).continuation();

        // Case 5a: continuation is a block - in this case the block itself is the next sibling.
        if (is<RenderBlock>(continuation))
            nextSibling = &continuation;
        // Case 5b: continuation is an inline - in this case the inline's first child is the next sibling
        else
            nextSibling = firstChildConsideringContinuation(continuation);
    }

    if (!nextSibling)
        return nullptr;

    CheckedPtr cache = axObjectCache();
    if (!cache)
        return nullptr;

    // After case 4, there are chances that nextSibling has the same node as the current renderer,
    // which might lead to adding the same child repeatedly.
    if (nextSibling->node() && nextSibling->node() == m_renderer->node()) {
        if (auto* nextObject = cache->getOrCreate(*nextSibling))
            return nextObject->nextSibling();
    }

    auto* nextObject = cache->getOrCreate(*nextSibling);
    auto* nextAXRenderObject = dynamicDowncast<AccessibilityRenderObject>(nextObject);
    auto* nextRenderParent = nextAXRenderObject ? cache->getOrCreate(nextAXRenderObject->renderParentObject()) : nullptr;

    // Make sure the next sibling has the same render parent.
    return !nextRenderParent || nextRenderParent == cache->getOrCreate(renderParentObject()) ? nextObject : nullptr;
}

static RenderBoxModelObject* nextContinuation(RenderObject& renderer)
{
    if (!renderer.isReplacedOrInlineBlock()) {
        if (auto* renderInline = dynamicDowncast<RenderInline>(renderer))
            return renderInline->continuation();
    }

    auto* renderBlock = dynamicDowncast<RenderBlock>(renderer);
    return renderBlock ? renderBlock->inlineContinuation() : nullptr;
}
    
RenderObject* AccessibilityRenderObject::renderParentObject() const
{
    if (!m_renderer)
        return nullptr;

    RenderElement* parent = m_renderer->parent();

    // Case 1: node is a block and is an inline's continuation. Parent
    // is the start of the continuation chain.
    RenderInline* startOfConts = nullptr;
    RenderObject* firstChild = nullptr;
    if (is<RenderBlock>(*m_renderer) && (startOfConts = startOfContinuations(*m_renderer)))
        parent = startOfConts;

    // Case 2: node's parent is an inline which is some node's continuation; parent is 
    // the earliest node in the continuation chain.
    else if (is<RenderInline>(parent) && (startOfConts = startOfContinuations(*parent)))
        parent = startOfConts;
    
    // Case 3: The first sibling is the beginning of a continuation chain. Find the origin of that continuation.
    else if (parent && (firstChild = parent->firstChild()) && firstChild->node()) {
        // Get the node's renderer and follow that continuation chain until the first child is found
        RenderObject* nodeRenderFirstChild = firstChild->node()->renderer();
        while (nodeRenderFirstChild != firstChild) {
            for (RenderObject* contsTest = nodeRenderFirstChild; contsTest; contsTest = nextContinuation(*contsTest)) {
                if (contsTest == firstChild) {
                    parent = nodeRenderFirstChild->parent();
                    break;
                }
            }
            RenderObject* parentFirstChild = parent->firstChild();
            if (firstChild == parentFirstChild)
                break;
            firstChild = parentFirstChild;
            if (!firstChild->node())
                break;
            nodeRenderFirstChild = firstChild->node()->renderer();
        }
    }
        
    return parent;
}

AccessibilityObject* AccessibilityRenderObject::parentObjectIfExists() const
{
    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return nullptr;

    // WebArea's parent should be the scroll view containing it.
    if (m_renderer && isWebArea())
        return cache->get(&m_renderer->view().frameView());

    if (auto* ownerParent = ownerParentObject())
        return ownerParent;

    if (auto* displayContentsParent = this->displayContentsParent())
        return displayContentsParent;

    return cache->get(renderParentObject());
}
    
AccessibilityObject* AccessibilityRenderObject::parentObject() const
{
    if (auto* ownerParent = ownerParentObject())
        return ownerParent;

    if (auto* displayContentsParent = this->displayContentsParent())
        return displayContentsParent;

    if (!m_renderer)
        return AccessibilityNodeObject::parentObject();

    WeakPtr cache = axObjectCache();
    if (!cache)
        return nullptr;

    if (ariaRoleAttribute() == AccessibilityRole::MenuBar)
        return cache->getOrCreate(m_renderer->parent());

    // menuButton and its corresponding menu are DOM siblings, but Accessibility needs them to be parent/child
    if (ariaRoleAttribute() == AccessibilityRole::Menu) {
        AccessibilityObject* parent = menuButtonForMenu();
        if (parent)
            return parent;
    }

#if USE(ATSPI)
    // Expose markers that are not direct children of a list item too.
    if (m_renderer->isRenderListMarker()) {
        if (auto* listItem = ancestorsOfType<RenderListItem>(*m_renderer).first()) {
            auto* parent = axObjectCache()->getOrCreate(*listItem);
            if (parent && uncheckedDowncast<AccessibilityRenderObject>(*parent).markerRenderer() == m_renderer)
                return parent;
        }
    }
#endif

    if (auto* parentObject = renderParentObject())
        return cache->getOrCreate(*parentObject);

    // WebArea's parent should be the scroll view containing it.
    if (isWebArea())
        return cache->getOrCreate(m_renderer->view().frameView());

    return nullptr;
}

bool AccessibilityRenderObject::isAttachment() const
{
    auto* widget = this->widget();
    // WebKit2 plugins need to be treated differently than attachments, so return false here.
    // Only WebKit1 plugins have an associated platformWidget.
    if (is<PluginViewBase>(widget) && !widget->platformWidget())
        return false;

    auto* renderer = this->renderer();
    // Widgets are the replaced elements that we represent to AX as attachments
    return renderer && renderer->isRenderWidget();
}

bool AccessibilityRenderObject::isOffScreen() const
{
    if (!m_renderer)
        return true;

    IntRect contentRect = snappedIntRect(m_renderer->absoluteClippedOverflowRectForSpatialNavigation());
    // FIXME: unclear if we need LegacyIOSDocumentVisibleRect.
    IntRect viewRect = m_renderer->view().frameView().visibleContentRect(ScrollableArea::LegacyIOSDocumentVisibleRect);
    viewRect.intersect(contentRect);
    return viewRect.isEmpty();
}

Element* AccessibilityRenderObject::anchorElement() const
{
    if (!m_renderer)
        return nullptr;

    WeakPtr cache = axObjectCache();
    if (!cache)
        return nullptr;

    RenderObject* currentRenderer;

    // Search up the render tree for a RenderObject with a DOM node.  Defer to an earlier continuation, though.
    for (currentRenderer = renderer(); currentRenderer && !currentRenderer->node(); currentRenderer = currentRenderer->parent()) {
        if (currentRenderer->isAnonymousBlock()) {
            if (auto* continuation = uncheckedDowncast<RenderBlock>(*currentRenderer).continuation())
                return cache->getOrCreate(*continuation)->anchorElement();
        }
    }

    // Bail if none found.
    if (!currentRenderer)
        return nullptr;

    // Search up the DOM tree for an anchor element.
    // NOTE: this assumes that any non-image with an anchor is an HTMLAnchorElement.
    for (RefPtr node = currentRenderer->node(); node; node = node->parentNode()) {
        if (auto* anchor = dynamicDowncast<HTMLAnchorElement>(node.get()))
            return anchor;

        auto* object = cache ? cache->getOrCreate(node->renderer()) : nullptr;
        if (object && object->isLink())
            return dynamicDowncast<Element>(*node);
    }

    return nullptr;
}

String AccessibilityRenderObject::helpText() const
{
    if (!m_renderer)
        return AccessibilityNodeObject::helpText();

    const auto& ariaHelp = getAttribute(aria_helpAttr);
    if (UNLIKELY(!ariaHelp.isEmpty()))
        return ariaHelp;

    String describedBy = ariaDescribedByAttribute();
    if (!describedBy.isEmpty())
        return describedBy;

    String description = this->description();
    for (auto* ancestor = renderer(); ancestor; ancestor = ancestor->parent()) {
        if (auto* element = dynamicDowncast<HTMLElement>(ancestor->node())) {
            const auto& summary = element->getAttribute(summaryAttr);
            if (!summary.isEmpty())
                return summary;

            // The title attribute should be used as help text unless it is already being used as descriptive text.
            const auto& title = element->getAttribute(titleAttr);
            if (!title.isEmpty() && description != title)
                return title;
        }

        // Only take help text from an ancestor element if its a group or an unknown role. If help was 
        // added to those kinds of elements, it is likely it was meant for a child element.
        if (auto* axAncestor = axObjectCache()->getOrCreate(*ancestor)) {
            if (!axAncestor->isGroup() && axAncestor->roleValue() != AccessibilityRole::Unknown)
                break;
        }
    }

    return { };
}

String AccessibilityRenderObject::textUnderElement(TextUnderElementMode mode) const
{
    // If we are within a hidden context, we don't want to add any text for this object, instead
    // just fanning out to nodes within our subtree to search for un-hidden nodes.
    // AccessibilityNodeObject::textUnderElement takes care of this, so call it directly.
    if (!m_renderer || mode.isHidden())
        return AccessibilityNodeObject::textUnderElement(WTFMove(mode));

    if (auto* fileUpload = dynamicDowncast<RenderFileUploadControl>(*m_renderer))
        return fileUpload->buttonValue();

    // Reflect when a content author has explicitly marked a line break.
    if (m_renderer->isBR())
        return "\n"_s;

    if (shouldGetTextFromNode(mode))
        return AccessibilityNodeObject::textUnderElement(WTFMove(mode));

    // We use a text iterator for text objects AND for those cases where we are
    // explicitly asking for the full text under a given element.
    if (is<RenderText>(*m_renderer) || mode.childrenInclusion == TextUnderElementMode::Children::IncludeAllChildren) {
        // If possible, use a text iterator to get the text, so that whitespace
        // is handled consistently.
        Document* nodeDocument = nullptr;
        std::optional<SimpleRange> textRange;
        if (Node* node = m_renderer->node()) {
            nodeDocument = &node->document();
            textRange = makeRangeSelectingNodeContents(*node);
        } else {
            // For anonymous blocks, we work around not having a direct node to create a range from
            // defining one based in the two external positions defining the boundaries of the subtree.
            RenderObject* firstChildRenderer = m_renderer->firstChildSlow();
            RenderObject* lastChildRenderer = m_renderer->lastChildSlow();
            if (firstChildRenderer && firstChildRenderer->node() && lastChildRenderer && lastChildRenderer->node()) {
                // We define the start and end positions for the range as the ones right before and after
                // the first and the last nodes in the DOM tree that is wrapped inside the anonymous block.
                auto& firstNodeInBlock = *firstChildRenderer->node();
                nodeDocument = &firstNodeInBlock.document();
                textRange = makeSimpleRange(positionInParentBeforeNode(&firstNodeInBlock), positionInParentAfterNode(lastChildRenderer->node()));
            }
        }

        if (nodeDocument && textRange) {
            if (auto* frame = nodeDocument->frame()) {
                // catch stale WebCoreAXObject (see <rdar://problem/3960196>)
                if (frame->document() != nodeDocument)
                    return { };

                return plainText(*textRange, textIteratorBehaviorForTextRange());
            }
        }

        // Sometimes text fragments don't have Nodes associated with them (like when
        // CSS content is used to insert text or when a RenderCounter is used.)
        if (WeakPtr renderText = dynamicDowncast<RenderText>(*m_renderer)) {
            if (WeakPtr renderTextFragment = dynamicDowncast<RenderTextFragment>(*renderText)) {
                // The alt attribute may be set on a text fragment through CSS, which should be honored.
                if (auto& altText = renderTextFragment->altText(); !altText.isNull())
                    return altText;
                return renderTextFragment->contentString();
            }
            return renderText->text();
        }
    }

    return AccessibilityNodeObject::textUnderElement(WTFMove(mode));
}

bool AccessibilityRenderObject::shouldGetTextFromNode(TextUnderElementMode mode) const
{
    if (!m_renderer)
        return true;

    // AccessibilityRenderObject::textUnderElement() gets the text of anonymous blocks by using
    // the child nodes to define positions. CSS tables and their anonymous descendants lack
    // children with nodes.
    if (m_renderer->isAnonymous() && m_renderer->isTablePart())
        return mode.childrenInclusion == TextUnderElementMode::Children::IncludeAllChildren;

    // AccessibilityRenderObject::textUnderElement() calls rangeOfContents() to create the text
    // range. rangeOfContents() does not include CSS-generated content.
    if (m_renderer->isBeforeOrAfterContent())
        return true;
    if (Node* node = m_renderer->node()) {
        Node* firstChild = node->pseudoAwareFirstChild();
        Node* lastChild = node->pseudoAwareLastChild();
        if ((firstChild && firstChild->isPseudoElement()) || (lastChild && lastChild->isPseudoElement()))
            return true;
    }

    return false;
}

Node* AccessibilityRenderObject::node() const
{
    if (m_renderer)
        return LIKELY(!m_renderer->isRenderView()) ? m_renderer->node() : &m_renderer->document();
    return AccessibilityNodeObject::node();
}

String AccessibilityRenderObject::stringValue() const
{
    if (!m_renderer)
        return AccessibilityNodeObject::stringValue();

    if (isStaticText() || isTextControl() || isSecureField()) {
        // A combobox is considered a text control, and its value is handled in AXNodeObject.
        if (isComboBox())
            return AccessibilityNodeObject::stringValue();
        return text();
    }

    if (is<RenderText>(m_renderer.get()))
        return textUnderElement();

    if (auto* renderMenuList = dynamicDowncast<RenderMenuList>(m_renderer.get())) {
        // RenderMenuList will go straight to the text() of its selected item.
        // This has to be overridden in the case where the selected item has an ARIA label.
        auto& selectElement = renderMenuList->selectElement();
        int selectedIndex = selectElement.selectedIndex();
        const auto& listItems = selectElement.listItems();
        if (selectedIndex >= 0 && static_cast<size_t>(selectedIndex) < listItems.size()) {
            if (RefPtr selectedItem = listItems[selectedIndex].get()) {
                auto overriddenDescription = selectedItem->attributeTrimmedWithDefaultARIA(aria_labelAttr);
                if (!overriddenDescription.isEmpty())
                    return overriddenDescription;
            }
        }
        return renderMenuList->text();
    }

#if PLATFORM(COCOA)
    if (is<RenderListItem>(m_renderer.get()))
        return textUnderElement();
#endif

    if (auto* renderListMarker = dynamicDowncast<RenderListMarker>(m_renderer.get())) {
#if USE(ATSPI)
        return renderListMarker->textWithSuffix().toString();
#else
        return renderListMarker->textWithoutSuffix().toString();
#endif
    }

    if (isWebArea())
        return { };

    if (isDateTime()) {
        if (RefPtr input = dynamicDowncast<HTMLInputElement>(node()))
            return input->visibleValue();

        // As fallback, gather the static text under this.
        String value;
        Accessibility::enumerateDescendants(*const_cast<AccessibilityRenderObject*>(this), false, [&value] (const auto& object) {
            if (object.isStaticText())
                value = makeString(value, object.stringValue());
        });
        return value;
    }

    if (auto* renderFileUploadControl = dynamicDowncast<RenderFileUploadControl>(m_renderer.get()))
        return renderFileUploadControl->fileTextValue();

    // FIXME: We might need to implement a value here for more types
    // FIXME: It would be better not to advertise a value at all for the types for which we don't implement one;
    // this would require subclassing or making accessibilityAttributeNames do something other than return a
    // single static array.
    return { };
}

bool AccessibilityRenderObject::canHavePlainText() const
{
    return isARIAStaticText() || is<RenderText>(*m_renderer) || isTextControl();
}

// The boundingBox for elements within the remote SVG element needs to be offset by its position
// within the parent page, otherwise they are in relative coordinates only.
void AccessibilityRenderObject::offsetBoundingBoxForRemoteSVGElement(LayoutRect& rect) const
{
    for (AccessibilityObject* parent = parentObject(); parent; parent = parent->parentObject()) {
        if (parent->isAccessibilitySVGRoot()) {
            rect.moveBy(parent->parentObject()->boundingBoxRect().location());
            break;
        }
    }
}

LayoutRect AccessibilityRenderObject::boundingBoxRect() const
{
    WeakPtr renderer = this->renderer();
    if (!renderer)
        return AccessibilityNodeObject::boundingBoxRect();

    if (renderer->node()) // If we are a continuation, we want to make sure to use the primary renderer.
        renderer = renderer->node()->renderer();

    // absoluteFocusRingQuads will query the hierarchy below this element, which for large webpages can be very slow.
    // For a web area, which will have the most elements of any element, absoluteQuads should be used.
    // We should also use absoluteQuads for SVG elements, otherwise transforms won't be applied.
    Vector<FloatQuad> quads;
    bool isSVGRoot = false;

    if (renderer->isRenderOrLegacyRenderSVGRoot())
        isSVGRoot = true;

    if (auto* renderText = dynamicDowncast<RenderText>(*renderer))
        quads = renderText->absoluteQuadsClippedToEllipsis();
    else if (isWebArea() || isSVGRoot)
        renderer->absoluteQuads(quads);
    else
        renderer->absoluteFocusRingQuads(quads);

    LayoutRect result = boundingBoxForQuads(renderer.get(), quads);

    Document* document = this->document();
    if (document && document->isSVGDocument())
        offsetBoundingBoxForRemoteSVGElement(result);

    // The size of the web area should be the content size, not the clipped size.
    if (isWebArea())
        result.setSize(renderer->view().frameView().contentsSize());

    return result;
}

bool AccessibilityRenderObject::isNonLayerSVGObject() const
{
    auto* renderer = this->renderer();
    return renderer ? is<RenderSVGInlineText>(renderer) || is<LegacyRenderSVGModelObject>(renderer) : false;
}

bool AccessibilityRenderObject::supportsPath() const
{
    return is<RenderText>(renderer()) || (renderer() && renderer()->isRenderOrLegacyRenderSVGShape());
}

Path AccessibilityRenderObject::elementPath() const
{
    if (!m_renderer)
        return AccessibilityNodeObject::elementPath();

    if (auto* renderText = dynamicDowncast<RenderText>(*m_renderer)) {
        Vector<LayoutRect> rects;
        renderText->boundingRects(rects, flooredLayoutPoint(m_renderer->localToAbsolute()));
        // If only 1 rect, don't compute path since the bounding rect will be good enough.
        if (rects.size() < 2)
            return { };

        // Compute the path only if this is the last part of a line followed by the beginning of the next line.
        const auto& style = m_renderer->style();
        bool rightToLeftText = style.direction() == TextDirection::RTL;
        static const auto xTolerance = 5_lu;
        static const auto yTolerance = 5_lu;
        bool needsPath = false;
        auto unionRect = rects[0];
        for (size_t i = 1; i < rects.size(); ++i) {
            needsPath = absoluteValue(rects[i].y() - unionRect.maxY()) < yTolerance // This rect is in a new line.
                && (rightToLeftText ? rects[i].x() - unionRect.x() > xTolerance
                    : unionRect.x() - rects[i].x() > xTolerance); // And this rect is to right/left of all previous rects.

            if (needsPath)
                break;

            unionRect.unite(rects[i]);
        }
        if (!needsPath)
            return { };

        float outlineOffset = style.outlineOffset();
        float deviceScaleFactor = m_renderer->document().deviceScaleFactor();
        Vector<FloatRect> pixelSnappedRects;
        for (auto rect : rects) {
            rect.inflate(outlineOffset);
            pixelSnappedRects.append(snapRectToDevicePixels(rect, deviceScaleFactor));
        }

        return PathUtilities::pathWithShrinkWrappedRects(pixelSnappedRects, 0);
    }

    if (auto* renderSVGShape = dynamicDowncast<LegacyRenderSVGShape>(*m_renderer); renderSVGShape && renderSVGShape->hasPath()) {
        Path path = renderSVGShape->path();

        auto* cache = axObjectCache();
        if (!cache)
            return path;

        // The SVG path is in terms of the parent's bounding box. The path needs to be offset to frame coordinates.
        // FIXME: This seems wrong for SVG inside HTML.
        if (auto svgRoot = ancestorsOfType<LegacyRenderSVGRoot>(*m_renderer).first()) {
            LayoutPoint parentOffset = cache->getOrCreate(&*svgRoot)->elementRect().location();
            path.transform(AffineTransform().translate(parentOffset.x(), parentOffset.y()));
        }
        return path;
    }

    if (auto* renderSVGShape = dynamicDowncast<RenderSVGShape>(*m_renderer); renderSVGShape && renderSVGShape->hasPath()) {
        Path path = renderSVGShape->path();

        auto* cache = axObjectCache();
        if (!cache)
            return path;

        // The SVG path is in terms of the parent's bounding box. The path needs to be offset to frame coordinates.
        if (auto svgRoot = ancestorsOfType<RenderSVGRoot>(*m_renderer).first()) {
            LayoutPoint parentOffset = cache->getOrCreate(&*svgRoot)->elementRect().location();
            path.transform(AffineTransform().translate(parentOffset.x(), parentOffset.y()));
        }
        return path;
    }

    return { };
}

#if ENABLE(APPLE_PAY)
String AccessibilityRenderObject::applePayButtonDescription() const
{
    switch (applePayButtonType()) {
    case ApplePayButtonType::Plain:
        return AXApplePayPlainLabel();
    case ApplePayButtonType::Buy:
        return AXApplePayBuyLabel();
    case ApplePayButtonType::SetUp:
        return AXApplePaySetupLabel();
    case ApplePayButtonType::Donate:
        return AXApplePayDonateLabel();
    case ApplePayButtonType::CheckOut:
        return AXApplePayCheckOutLabel();
    case ApplePayButtonType::Book:
        return AXApplePayBookLabel();
    case ApplePayButtonType::Subscribe:
        return AXApplePaySubscribeLabel();
#if ENABLE(APPLE_PAY_NEW_BUTTON_TYPES)
    case ApplePayButtonType::Reload:
        return AXApplePayReloadLabel();
    case ApplePayButtonType::AddMoney:
        return AXApplePayAddMoneyLabel();
    case ApplePayButtonType::TopUp:
        return AXApplePayTopUpLabel();
    case ApplePayButtonType::Order:
        return AXApplePayOrderLabel();
    case ApplePayButtonType::Rent:
        return AXApplePayRentLabel();
    case ApplePayButtonType::Support:
        return AXApplePaySupportLabel();
    case ApplePayButtonType::Contribute:
        return AXApplePayContributeLabel();
    case ApplePayButtonType::Tip:
        return AXApplePayTipLabel();
#endif
    }
}
#endif

void AccessibilityRenderObject::labelText(Vector<AccessibilityText>& textOrder) const
{
#if ENABLE(APPLE_PAY)
    if (isApplePayButton()) {
        textOrder.append(AccessibilityText(applePayButtonDescription(), AccessibilityTextSource::Alternative));
        return;
    }
#endif
    AccessibilityNodeObject::labelText(textOrder);
}

AXCoreObject* AccessibilityRenderObject::titleUIElement() const
{
    if (m_renderer && isFieldset())
        return axObjectCache()->getOrCreate(dynamicDowncast<RenderBlock>(*m_renderer)->findFieldsetLegend(RenderBlock::FieldsetIncludeFloatingOrOutOfFlow));
    return AccessibilityNodeObject::titleUIElement();
}
    
bool AccessibilityRenderObject::isAllowedChildOfTree() const
{
    // Determine if this is in a tree. If so, we apply special behavior to make it work like an AXOutline.
    AccessibilityObject* axObj = parentObject();
    bool isInTree = false;
    bool isTreeItemDescendant = false;
    while (axObj) {
        if (axObj->roleValue() == AccessibilityRole::TreeItem)
            isTreeItemDescendant = true;
        if (axObj->isTree()) {
            isInTree = true;
            break;
        }
        axObj = axObj->parentObject();
    }
    
    // If the object is in a tree, only tree items should be exposed (and the children of tree items).
    if (isInTree) {
        AccessibilityRole role = roleValue();
        if (role != AccessibilityRole::TreeItem && role != AccessibilityRole::StaticText && !isTreeItemDescendant)
            return false;
    }
    return true;
}

static AccessibilityObjectInclusion objectInclusionFromAltText(const String& altText)
{
    // Don't ignore an image that has an alt tag.
    if (!altText.containsOnly<isASCIIWhitespace>())
        return AccessibilityObjectInclusion::IncludeObject;

    // The informal standard is to ignore images with zero-length alt strings:
    // https://www.w3.org/WAI/tutorials/images/decorative/.
    if (!altText.isNull())
        return AccessibilityObjectInclusion::IgnoreObject;

    return AccessibilityObjectInclusion::DefaultBehavior;
}

static bool webAreaIsPresentational(RenderObject* renderer)
{
    if (!renderer || !is<RenderView>(*renderer))
        return false;
    
    if (auto ownerElement = renderer->document().ownerElement())
        return nodeHasPresentationRole(ownerElement);
    
    return false;
}

bool AccessibilityRenderObject::computeAccessibilityIsIgnored() const
{
#ifndef NDEBUG
    ASSERT(m_initialized);
#endif

    if (!m_renderer)
        return AccessibilityNodeObject::computeAccessibilityIsIgnored();

    // Check first if any of the common reasons cause this element to be ignored.
    // Then process other use cases that need to be applied to all the various roles
    // that AccessibilityRenderObjects take on.
    AccessibilityObjectInclusion decision = defaultObjectInclusion();
    if (decision == AccessibilityObjectInclusion::IncludeObject)
        return false;
    if (decision == AccessibilityObjectInclusion::IgnoreObject)
        return true;

    if (roleValue() == AccessibilityRole::Ignored)
        return true;

    if (ignoredFromPresentationalRole())
        return true;

    // WebAreas should be ignored if their iframe container is marked as presentational.
    if (webAreaIsPresentational(renderer()))
        return true;

    // An ARIA tree can only have tree items and static text as children.
    if (!isAllowedChildOfTree())
        return true;

    // Allow the platform to decide if the attachment is ignored or not.
    if (isAttachment())
        return accessibilityIgnoreAttachment();

#if PLATFORM(COCOA)
    // If this widget has an underlying AX object, don't ignore it.
    if (widget() && widget()->accessibilityObject())
        return false;
#endif

    // ignore popup menu items because AppKit does
    if (m_renderer && ancestorsOfType<RenderMenuList>(*m_renderer).first())
        return true;

    // https://webkit.org/b/161276 Getting the controlObject might cause the m_renderer to be nullptr.
    if (!m_renderer)
        return true;

    if (m_renderer->isBR()) {
#if ENABLE(AX_THREAD_TEXT_APIS)
        // We need to preserve BRs within editable contexts (e.g. inside contenteditable) for serving
        // text APIs off the main-thread because this allows them to be part of the AX tree, which is
        // traversed to compute text markers.
        auto* node = this->node();
        return !node || !node->hasEditableStyle();
#else
        return true;
#endif
    }

    if (WeakPtr renderText = dynamicDowncast<RenderText>(m_renderer.get())) {
        // Text elements with no rendered text, or only whitespace should not be part of the AX tree.
        if (!renderText->hasRenderedText()) {
            // Layout must be clean to make the right decision here (because hasRenderedText() can return false solely because layout is dirty).
            ASSERT(!renderText->needsLayout() || !renderText->text().length());
            return true;
        }

        if (renderText->text().containsOnly<isASCIIWhitespace>()) {
#if ENABLE(AX_THREAD_TEXT_APIS)
            // Preserve whitespace-only text within editable contexts because ignoring it would cause
            // accessibility's representation of text to be different than what is actually rendered.
            // FIXME: This actually isn't enough — we likely need _all_ whitespace RenderTexts (within an editable or not) to compute StringForTextMarkerRange correctly.
            // e.g. This is necessary for ax-thread-text-apis/display-contents-end-text-marker.html.
            auto* node = this->node();
            return !node || !node->hasEditableStyle();
#else
            return true;
#endif
        }

        if (renderText->parent()->isFirstLetter())
            return true;

        // The alt attribute may be set on a text fragment through CSS, which should be honored.
        if (auto* renderTextFragment = dynamicDowncast<RenderTextFragment>(renderText.get())) {
            auto altTextInclusion = objectInclusionFromAltText(renderTextFragment->altText());
            if (altTextInclusion == AccessibilityObjectInclusion::IgnoreObject)
                return true;
            if (altTextInclusion == AccessibilityObjectInclusion::IncludeObject)
                return false;
        }

        bool checkForIgnored = true;
        for (RefPtr ancestor = parentObject(); ancestor; ancestor = ancestor->parentObject()) {
            // Static text beneath TextControls is reported along with the text control text so it's ignored.
            // FIXME: Why does this not check for the other text-control roles (e.g. textarea)?
            if (ancestor->roleValue() == AccessibilityRole::TextField)
                return true;

            if (checkForIgnored && !ancestor->accessibilityIsIgnored()) {
                checkForIgnored = false;
                // Static text beneath MenuItems and MenuButtons are just reported along with the menu item, so it's ignored on an individual level.
                if (ancestor->isMenuItem() || ancestor->isMenuButton())
                    return true;
            }
        }

        // If iterating the ancestry has caused m_renderer to be destroyed, ignore `this`.
        return !renderText;
    }
    
    if (isHeading())
        return false;
    
    if (isLink())
        return false;
    
    if (isLandmark())
        return false;

    // all controls are accessible
    if (isControl())
        return false;
    
    if (isFigureElement() || isSummary())
        return false;

    switch (roleValue()) {
    case AccessibilityRole::Audio:
    case AccessibilityRole::DescriptionListTerm:
    case AccessibilityRole::DescriptionListDetail:
    case AccessibilityRole::Details:
    case AccessibilityRole::DocumentArticle:
    case AccessibilityRole::Footer:
    case AccessibilityRole::LandmarkRegion:
    case AccessibilityRole::ListItem:
    case AccessibilityRole::Time:
    case AccessibilityRole::Video:
        return false;
    default:
        break;
    }

    if (isImage()) {
        // If the image can take focus, it should not be ignored, lest the user not be able to interact with something important.
        if (canSetFocusAttribute())
            return false;

        // webkit.org/b/173870 - If an image has other alternative text, don't ignore it if alt text is empty.
        // This means we should process title and aria-label first.

        // If an image has an accname, accessibility should be lenient and allow it to appear in the hierarchy (according to WAI-ARIA).
        if (hasAccNameAttribute())
            return false;

        // First check the RenderImage's altText (which can be set through a style sheet, or come from the Element).
        // However, if this is not a native image, fallback to the attribute on the Element.
        AccessibilityObjectInclusion altTextInclusion = AccessibilityObjectInclusion::DefaultBehavior;
        WeakPtr image = dynamicDowncast<RenderImage>(*m_renderer);
        if (image)
            altTextInclusion = objectInclusionFromAltText(image->altText());
        else
            altTextInclusion = objectInclusionFromAltText(getAttribute(altAttr).string());

        if (altTextInclusion == AccessibilityObjectInclusion::IgnoreObject)
            return true;
        if (altTextInclusion == AccessibilityObjectInclusion::IncludeObject)
            return false;

        if (image) {
            // check for one-dimensional image
            if (image->height() <= 1 || image->width() <= 1)
                return true;

            // check whether rendered image was stretched from one-dimensional file image
            if (image->cachedImage()) {
                LayoutSize imageSize = image->cachedImage()->imageSizeForRenderer(image.get(), image->view().zoomFactor());
                return imageSize.height() <= 1 || imageSize.width() <= 1;
            }
        }
        return false;
    }

    // Objects that are between table rows and cells should be ignored, otherwise the table hierarchy will be
    // incorrect, preventing the table content from being accessible to ATs.
    if (!ancestorFlagsAreInitialized() || isInRow()) {
        if (!isTableCell() && canHaveChildren() && ignoredByRowAncestor())
            return true;
    }

    if (ariaRoleAttribute() != AccessibilityRole::Unknown)
        return false;

    if (roleValue() == AccessibilityRole::HorizontalRule)
        return false;
    
    // don't ignore labels, because they serve as TitleUIElements
    Node* node = m_renderer->node();
    if (is<HTMLLabelElement>(node))
        return false;
    
    // Anything that is content editable should not be ignored.
    // However, one cannot just call node->hasEditableStyle() since that will ask if its parents
    // are also editable. Only the top level content editable region should be exposed.
    if (hasContentEditableAttributeSet())
        return false;

    // if this element has aria attributes on it, it should not be ignored.
    if (supportsARIAAttributes())
        return false;

#if ENABLE(MATHML)
    // First check if this is a special case within the math tree that needs to be ignored.
    if (isIgnoredElementWithinMathTree())
        return true;
    // Otherwise all other math elements are in the tree.
    if (isMathElement())
        return false;
#endif

    WeakPtr blockFlow = dynamicDowncast<RenderBlockFlow>(*m_renderer);
    if (blockFlow && m_renderer->childrenInline() && !canSetFocusAttribute())
        return !blockFlow->hasLines() && !mouseButtonListener();

    if (isCanvas()) {
        if (canvasHasFallbackContent())
            return false;

        if (WeakPtr canvasBox = dynamicDowncast<RenderBox>(*m_renderer)) {
            if (canvasBox->height() <= 1 || canvasBox->width() <= 1)
                return true;
        }
        // Otherwise fall through; use presence of help text, title, or description to decide.
    }

    if (m_renderer->isRenderListMarker()) {
        AXCoreObject* parent = parentObjectUnignored();
        return parent && !parent->isListItem();
    }

    if (isWebArea())
        return false;
    
    // The render tree of meter includes a RenderBlock (meter) and a RenderMeter (div).
    // We expose the latter and thus should ignore the former. However, if the author
    // includes a title attribute on the element, hasAttributesRequiredForInclusion()
    // will return true, potentially resulting in a redundant accessible object.
    if (is<HTMLMeterElement>(node))
        return true;

    // Using the presence of an accessible name to decide an element's visibility is not
    // as definitive as previous checks, so this should remain as one of the last.
    if (hasAttributesRequiredForInclusion())
        return false;

    // Don't ignore generic focusable elements like <div tabindex=0>
    // unless they're completely empty, with no children.
    if (isGenericFocusableElement() && node->firstChild())
        return false;

    // <span> tags are inline tags and not meant to convey information if they have no other aria
    // information on them. If we don't ignore them, they may emit signals expected to come from
    // their parent. In addition, because included spans are AccessibilityRole::Group objects, and AccessibilityRole::Group
    // objects are often containers with meaningful information, the inclusion of a span can have
    // the side effect of causing the immediate parent accessible to be ignored. This is especially
    // problematic for platforms which have distinct roles for textual block elements.
    if (node && node->hasTagName(spanTag))
        return true;

    // Other non-ignored host language elements
    if (node && node->hasTagName(dfnTag))
        return false;
    
    if (isStyleFormatGroup())
        return false;

    switch (m_renderer->style().display()) {
    case DisplayType::Ruby:
    case DisplayType::RubyBlock:
    case DisplayType::RubyAnnotation:
    case DisplayType::RubyBase:
        return false;
    default:
        break;
    }

    // Find out if this element is inside of a label element.
    // If so, it may be ignored because it's the label for a checkbox or radio button.
    auto* controlObject = controlForLabelElement();
    if (controlObject && controlObject->isCheckboxOrRadio() && !controlObject->titleUIElement())
        return true;

    // By default, objects should be ignored so that the AX hierarchy is not
    // filled with unnecessary items.
    return true;
}

int AccessibilityRenderObject::layoutCount() const
{
    auto* view = dynamicDowncast<RenderView>(m_renderer.get());
    return view ? view->frameView().layoutContext().layoutCount() : 0;
}

CharacterRange AccessibilityRenderObject::documentBasedSelectedTextRange() const
{
    auto selectedVisiblePositionRange = this->selectedVisiblePositionRange();
    if (selectedVisiblePositionRange.isNull())
        return { };

    int start = indexForVisiblePosition(selectedVisiblePositionRange.start);
    if (start < 0)
        start = 0;
    int end = indexForVisiblePosition(selectedVisiblePositionRange.end);
    if (end < 0)
        end = 0;
    return { static_cast<unsigned>(start), static_cast<unsigned>(end - start) };
}

String AccessibilityRenderObject::selectedText() const
{
    ASSERT(isTextControl());
    
    if (isSecureField())
        return String(); // need to return something distinct from empty string

    if (isNativeTextControl()) {
        auto& textControl = uncheckedDowncast<RenderTextControl>(*m_renderer).textFormControlElement();
        return textControl.selectedText();
    }

    return doAXStringForRange(documentBasedSelectedTextRange());
}

CharacterRange AccessibilityRenderObject::selectedTextRange() const
{
    ASSERT(isTextControl());

    if (shouldReturnEmptySelectedText())
        return { };

    // Use the text control native range if it's a native object.
    if (isNativeTextControl()) {
        auto& textControl = uncheckedDowncast<RenderTextControl>(*m_renderer).textFormControlElement();
        return { textControl.selectionStart(), textControl.selectionEnd() - textControl.selectionStart() };
    }

    return documentBasedSelectedTextRange();
}

#if ENABLE(AX_THREAD_TEXT_APIS)
AXTextRuns AccessibilityRenderObject::textRuns()
{
    if (auto* renderLineBreak = dynamicDowncast<RenderLineBreak>(renderer())) {
        auto box = InlineIterator::boxFor(*renderLineBreak);
        return { renderLineBreak->containingBlock(), { AXTextRun(box->lineIndex(), makeString('\n').isolatedCopy()) } };
    }

    if (RefPtr inputElement = dynamicDowncast<HTMLInputElement>(node())) {
        // The text within input elements is not actually part of the accessibility tree, meaning we need to do a bit of extra work to expose that text here.
        for (const auto& node : composedTreeDescendants(*inputElement)) {
            if (!node.isTextNode())
                continue;
            auto* renderer = node.renderer();
            auto* containingBlock = renderer ? renderer->containingBlock() : nullptr;
            return containingBlock ? AXTextRuns(containingBlock, { AXTextRun(0, inputElement->value().isolatedCopy()) }) : AXTextRuns();
        }
        return { };
    }

    WeakPtr renderText = dynamicDowncast<RenderText>(renderer());
    if (!renderText)
        return { };

    // FIXME: Need to handle PseudoId::FirstLetter. Right now, it will be chopped off from the other
    // other text in the line, and AccessibilityRenderObject::computeAccessibilityIsIgnored ignores the
    // first-letter RenderText, meaning we can't recover it later by combining text across AX objects.

    Vector<AXTextRun> runs;
    StringBuilder lineString;
    // Appends text to the current lineString, collapsing whitespace as necessary (similar to how TextIterator::handleTextRun() does).
    auto appendToLineString = [&] (const InlineIterator::TextBoxIterator& textBox) {
        auto text = textBox->originalText();
        if (text.isEmpty())
            return;
        bool collapseTabs = textBox->style().collapseWhiteSpace();
        bool collapseNewlines = !textBox->style().preserveNewline();
        if (!collapseTabs && !collapseNewlines) {
            lineString.append(text);
            return;
        }

        lineString.reserveCapacity(lineString.length() + text.length());
        for (unsigned i = 0; i < text.length(); i++) {
            UChar character = text[i];
            if (character == '\t' && collapseTabs)
                lineString.append(' ');
            else if (character == '\n' && collapseNewlines)
                lineString.append(' ');
            else
                lineString.append(character);
        }
    };

    auto textBox = InlineIterator::firstTextBoxFor(*renderText);
    size_t currentLineIndex = textBox ? textBox->lineIndex() : 0;
    for (; textBox; textBox.traverseNextTextBox()) {
        size_t newLineIndex = textBox->lineIndex();
        if (newLineIndex != currentLineIndex) {
            // FIXME: Currently, this is only ever called to ship text runs off to the accessibility thread. But maybe we should we make the isolatedCopy()s in this function optional based on a parameter?
            runs.append({ currentLineIndex, lineString.toString().isolatedCopy() });
            lineString.clear();
        }
        currentLineIndex = newLineIndex;

        appendToLineString(textBox);
    }

    if (!lineString.isEmpty())
        runs.append({ currentLineIndex, lineString.toString().isolatedCopy() });
    return { renderText->containingBlock(), WTFMove(runs) };
}
#endif // ENABLE(AX_THREAD_TEXT_APIS)

int AccessibilityRenderObject::insertionPointLineNumber() const
{
    ASSERT(isTextControl());

    // Use the text control native range if it's a native object.
    if (isNativeTextControl()) {
        auto& textControl = uncheckedDowncast<RenderTextControl>(*m_renderer).textFormControlElement();
        int start = textControl.selectionStart();
        int end = textControl.selectionEnd();

        // If the selection range is not a collapsed range, we don't know whether the insertion point is the start or the end, thus return -1.
        // FIXME: for non-collapsed selection, determine the insertion point based on the TextFieldSelectionDirection.
        if (start != end)
            return -1;

        return lineForPosition(textControl.visiblePositionForIndex(start));
    }

    auto* frame = this->frame();
    if (!frame)
        return -1;

    auto selectedTextRange = frame->selection().selection().firstRange();
    // If the selection range is not a collapsed range, we don't know whether the insertion point is the start or the end, thus return -1.
    if (!selectedTextRange || !selectedTextRange->collapsed())
        return -1;

    return lineForPosition(makeDeprecatedLegacyPosition(selectedTextRange->start));
}

static void setTextSelectionIntent(AXObjectCache* cache, AXTextStateChangeType type)
{
    if (!cache)
        return;
    AXTextStateChangeIntent intent(type, AXTextSelection { AXTextSelectionDirectionDiscontiguous, AXTextSelectionGranularityUnknown, false });
    cache->setTextSelectionIntent(intent);
    cache->setIsSynchronizingSelection(true);
}

static void clearTextSelectionIntent(AXObjectCache* cache)
{
    if (!cache)
        return;
    cache->setTextSelectionIntent(AXTextStateChangeIntent());
    cache->setIsSynchronizingSelection(false);
}

void AccessibilityRenderObject::setSelectedTextRange(CharacterRange&& range)
{
    setTextSelectionIntent(axObjectCache(), range.length ? AXTextStateChangeTypeSelectionExtend : AXTextStateChangeTypeSelectionMove);

    auto* client = m_renderer ? m_renderer->document().editor().client() : nullptr;
    if (client)
        client->willChangeSelectionForAccessibility();

    if (isNativeTextControl()) {
        auto& textControl = uncheckedDowncast<RenderTextControl>(*m_renderer).textFormControlElement();
        textControl.setSelectionRange(range.location, range.location + range.length);
    } else if (m_renderer) {
        ASSERT(node());
        auto& node = *this->node();
        auto elementRange = simpleRange();
        auto start = visiblePositionForIndexUsingCharacterIterator(node, range.location);
        if (!contains<ComposedTree>(*elementRange, makeBoundaryPoint(start)))
            start = makeContainerOffsetPosition(elementRange->start);
        auto end = visiblePositionForIndexUsingCharacterIterator(node, range.location + range.length);
        if (!contains<ComposedTree>(*elementRange, makeBoundaryPoint(end)))
            end = makeContainerOffsetPosition(elementRange->start);
        m_renderer->frame().selection().setSelection(VisibleSelection(start, end), FrameSelection::defaultSetSelectionOptions(UserTriggered::Yes));
    }

    clearTextSelectionIntent(axObjectCache());

    if (client)
        client->didChangeSelectionForAccessibility();
}

URL AccessibilityRenderObject::url() const
{
    if (m_renderer && isWebArea())
        return m_renderer->document().url();
    return AccessibilityNodeObject::url();
}

bool AccessibilityRenderObject::setValue(const String& string)
{
    if (!m_renderer)
        return false;

    auto& renderer = *m_renderer;

    RefPtr element = dynamicDowncast<Element>(renderer.node());
    if (!element)
        return false;

    // We should use the editor's insertText to mimic typing into the field.
    // Also only do this when the field is in editing mode.
    if (auto* frame = renderer.document().frame()) {
        auto& editor = frame->editor();
        if (element->shouldUseInputMethod()) {
            editor.clearText();
            editor.insertText(string, nullptr);
            return true;
        }
    }
    // FIXME: Do we want to do anything here for ARIA textboxes?
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(*element); input && renderer.isRenderTextControlSingleLine()) {
        input->setValue(string);
        return true;
    }
    if (RefPtr textarea = dynamicDowncast<HTMLTextAreaElement>(*element); element && renderer.isRenderTextControlMultiLine()) {
        textarea->setValue(string);
        return true;
    }

    return false;
}

Document* AccessibilityRenderObject::document() const
{
    if (!m_renderer)
        return AccessibilityNodeObject::document();
    return &m_renderer->document();
}

bool AccessibilityRenderObject::isWidget() const
{
    return widget();
}

Widget* AccessibilityRenderObject::widget() const
{
    auto* renderWidget = dynamicDowncast<RenderWidget>(renderer());
    return renderWidget ? renderWidget->widget() : nullptr;
}

AccessibilityObject* AccessibilityRenderObject::associatedAXImage(HTMLMapElement& map) const
{
    CheckedPtr cache = axObjectCache();
    return cache ? cache->getOrCreate(map.imageElement().get()) : nullptr;
}

AXCoreObject::AccessibilityChildrenVector AccessibilityRenderObject::documentLinks()
{
    if (!m_renderer)
        return { };

    AccessibilityChildrenVector result;
    Document& document = m_renderer->document();
    Ref<HTMLCollection> links = document.links();
    for (unsigned i = 0; auto* current = links->item(i); ++i) {
        if (auto* renderer = current->renderer()) {
            RefPtr<AccessibilityObject> axObject = document.axObjectCache()->getOrCreate(*renderer);
            ASSERT(axObject);
            if (!axObject->accessibilityIsIgnored() && axObject->isLink())
                result.append(axObject);
        } else {
            auto* parent = current->parentNode();
            if (auto* parentMap = dynamicDowncast<HTMLMapElement>(parent); parentMap && is<HTMLAreaElement>(*current)) {
                RefPtr parentImage = parentMap->imageElement();
                auto* parentImageRenderer = parentImage ? parentImage->renderer() : nullptr;
                if (auto* parentImageAxObject = document.axObjectCache()->getOrCreate(parentImageRenderer)) {
                    for (const auto& child : parentImageAxObject->children()) {
                        if (is<AccessibilityImageMapLink>(child) && !result.contains(child))
                            result.append(child);
                    }
                } else {
                    // We couldn't retrieve the already existing image-map links from the parent image, so create a new one.
                    ASSERT_NOT_REACHED("Unexpectedly missing image-map link parent AX object.");
                    auto& areaObject = uncheckedDowncast<AccessibilityImageMapLink>(*axObjectCache()->create(AccessibilityRole::ImageMapLink));
                    areaObject.setHTMLAreaElement(uncheckedDowncast<HTMLAreaElement>(current));
                    areaObject.setHTMLMapElement(parentMap);
                    areaObject.setParent(associatedAXImage(*parentMap));
                    result.append(&areaObject);
                }
            }
        }
    }

    return result;
}

LocalFrameView* AccessibilityRenderObject::documentFrameView() const 
{ 
    if (!m_renderer)
        return AccessibilityNodeObject::documentFrameView();

    return &m_renderer->view().frameView();
}

Widget* AccessibilityRenderObject::widgetForAttachmentView() const
{
    if (!isAttachment())
        return nullptr;
    auto* renderWidget = dynamicDowncast<RenderWidget>(*m_renderer);
    return renderWidget ? renderWidget->widget() : nullptr;
}

VisiblePosition AccessibilityRenderObject::visiblePositionForIndex(int index) const
{
    if (m_renderer) {
        if (isNativeTextControl()) {
            auto& textControl = uncheckedDowncast<RenderTextControl>(*m_renderer).textFormControlElement();
            return textControl.visiblePositionForIndex(std::clamp(index, 0, static_cast<int>(textControl.value().length())));
        }

        if (!allowsTextRanges() && !is<RenderText>(*m_renderer))
            return { };
    }
    return AccessibilityNodeObject::visiblePositionForIndex(index);
}

int AccessibilityRenderObject::indexForVisiblePosition(const VisiblePosition& position) const
{
    if (m_renderer) {
        if (isNativeTextControl())
            return uncheckedDowncast<RenderTextControl>(*m_renderer).textFormControlElement().indexForVisiblePosition(position);

        if (!allowsTextRanges() && !is<RenderText>(*m_renderer))
            return 0;
    }
    return AccessibilityNodeObject::indexForVisiblePosition(position);
}

RefPtr<Element> AccessibilityRenderObject::rootEditableElementForPosition(const Position& position) const
{
    // Find the root editable or pseudo-editable (i.e. having an editable ARIA role) element.
    RefPtr<Element> result;
    RefPtr rootEditableElement = position.rootEditableElement();

    for (RefPtr ancestor = position.anchorElementAncestor(); ancestor && ancestor != rootEditableElement; ancestor = ancestor->parentElement()) {
        if (nodeIsTextControl(*ancestor))
            result = ancestor;
        if (ancestor->hasTagName(bodyTag))
            break;
    }
    return result ? result : rootEditableElement;
}

bool AccessibilityRenderObject::nodeIsTextControl(const Node& node) const
{
    auto* cache = axObjectCache();
    auto* axObject = cache ? cache->getOrCreate(const_cast<Node&>(node)) : nullptr;
    return axObject && axObject->isTextControl();
}

bool AccessibilityRenderObject::isVisiblePositionRangeInDifferentDocument(const VisiblePositionRange& range) const
{
    if (range.start.isNull() || range.end.isNull())
        return false;
    
    VisibleSelection newSelection = VisibleSelection(range.start, range.end);
    if (Document* newSelectionDocument = newSelection.base().document()) {
        if (RefPtr newSelectionFrame = newSelectionDocument->frame()) {
            auto* frame = this->frame();
            if (!frame || (newSelectionFrame != frame && newSelectionDocument != frame->document()))
                return true;
        }
    }
    
    return false;
}

void AccessibilityRenderObject::setSelectedVisiblePositionRange(const VisiblePositionRange& range) const
{
    if (range.isNull())
        return;

    // In WebKit1, when the top web area sets the selection to be an input element in an iframe, the caret will disappear.
    // FrameSelection::setSelectionWithoutUpdatingAppearance is setting the selection on the new frame in this case, and causing this behavior.
    if (isWebArea() && parentObject() && parentObject()->isAttachment()
        && isVisiblePositionRangeInDifferentDocument(range))
        return;

    auto* client = m_renderer ? m_renderer->document().editor().client() : nullptr;
    if (client)
        client->willChangeSelectionForAccessibility();

    if (isNativeTextControl()) {
        // isNativeTextControl returns true only if this->node() is<HTMLTextAreaElement> or is<HTMLInputElement>.
        // Since both HTMLTextAreaElement and HTMLInputElement derive from HTMLTextFormControlElement, it is safe to downcast here.
        auto* textControl = uncheckedDowncast<HTMLTextFormControlElement>(node());
        int start = textControl->indexForVisiblePosition(range.start);
        int end = textControl->indexForVisiblePosition(range.end);

        // For ranges entirely contained in textControl, the start or end position may not be inside textControl.innerTextElement.
        // This would cause that the above indexes will be 0, leading to an incorrect selected range
        // (see HTMLTextFormControlElement::indexForVisiblePosition). This is
        // the case when range is obtained from AXObjectCache::rangeForNodeContents
        // for the HTMLTextFormControlElement.
        // Thus, the following corrects the start and end indexes in such a case..
        if (range.start.deepEquivalent().anchorNode() == range.end.deepEquivalent().anchorNode()
            && range.start.deepEquivalent().anchorNode() == textControl) {
            if (auto innerText = textControl->innerTextElement()) {
                auto textControlRange = makeVisiblePositionRange(AXObjectCache::rangeForNodeContents(*textControl));
                auto innerRange = makeVisiblePositionRange(AXObjectCache::rangeForNodeContents(*innerText));

                if (range.start.equals(textControlRange.end))
                    start = textControl->value().length();
                else if (range.start <= innerRange.start)
                    start = 0;

                if (range.end >= innerRange.end
                    || range.end.equals(textControlRange.end))
                    end = textControl->value().length();
            }
        }

        setTextSelectionIntent(axObjectCache(), start == end ? AXTextStateChangeTypeSelectionMove : AXTextStateChangeTypeSelectionExtend);
        textControl->setSelectionRange(start, end);
    } else if (m_renderer) {
        // Make selection and tell the document to use it. If it's zero length, then move to that position.
        if (range.start == range.end) {
            setTextSelectionIntent(axObjectCache(), AXTextStateChangeTypeSelectionMove);

            auto start = range.start;
            if (auto elementRange = simpleRange()) {
                if (!contains<ComposedTree>(*elementRange, makeBoundaryPoint(start)))
                    start = makeContainerOffsetPosition(elementRange->start);
            }

            m_renderer->frame().selection().moveTo(start, UserTriggered::Yes);
        } else {
            setTextSelectionIntent(axObjectCache(), AXTextStateChangeTypeSelectionExtend);

            VisibleSelection newSelection = VisibleSelection(range.start, range.end);
            m_renderer->frame().selection().setSelection(newSelection, FrameSelection::defaultSetSelectionOptions(UserTriggered::Yes));
        }
    }

    clearTextSelectionIntent(axObjectCache());

    if (client)
        client->didChangeSelectionForAccessibility();
}

// NOTE: Consider providing this utility method as AX API
VisiblePosition AccessibilityRenderObject::visiblePositionForIndex(unsigned indexValue, bool lastIndexOK) const
{
    if (!isTextControl())
        return VisiblePosition();
    
    // lastIndexOK specifies whether the position after the last character is acceptable
    if (indexValue >= text().length()) {
        if (!lastIndexOK || indexValue > text().length())
            return VisiblePosition();
    }
    VisiblePosition position = visiblePositionForIndex(indexValue);
    position.setAffinity(Affinity::Downstream);
    return position;
}

// NOTE: Consider providing this utility method as AX API
int AccessibilityRenderObject::index(const VisiblePosition& position) const
{
    if (position.isNull() || !isTextControl())
        return -1;

    if (renderObjectContainsPosition(renderer(), position.deepEquivalent()))
        return indexForVisiblePosition(position);
    
    return -1;
}

static bool isHardLineBreak(const VisiblePosition& position)
{
    if (!isEndOfLine(position))
        return false;

    auto next = position.next();

    auto lineBreakRange = makeSimpleRange(position, next);
    if (!lineBreakRange)
        return false;

    TextIterator it(*lineBreakRange);
    if (it.atEnd())
        return false;

    if (is<HTMLBRElement>(it.node()))
        return true;

    if (it.node() != position.deepEquivalent().anchorNode())
        return false;

    return it.text().length() == 1 && it.text()[0] == '\n';
}

// Given a line number, the range of characters of the text associated with this accessibility
// object that contains the line number.
CharacterRange AccessibilityRenderObject::doAXRangeForLine(unsigned lineNumber) const
{
    if (!isTextControl())
        return { };

    // Iterate to the specified line.
    auto lineStart = visiblePositionForIndex(0);
    for (unsigned lineCount = lineNumber; lineCount; --lineCount) {
        auto nextLineStart = nextLinePosition(lineStart, 0);
        if (nextLineStart.isNull() || nextLineStart == lineStart)
            return { };
        lineStart = nextLineStart;
    }

    // Get the end of the line based on the starting position.
    auto lineEnd = endOfLine(lineStart);

    int index1 = indexForVisiblePosition(lineStart);
    int index2 = indexForVisiblePosition(lineEnd);

    if (isHardLineBreak(lineEnd))
        ++index2;

    if (index1 < 0 || index2 < 0 || index2 <= index1)
        return { };

    return { static_cast<unsigned>(index1), static_cast<unsigned>(index2 - index1) };
}

// The composed character range in the text associated with this accessibility object that
// is specified by the given index value. This parameterized attribute returns the complete
// range of characters (including surrogate pairs of multi-byte glyphs) at the given index.
CharacterRange AccessibilityRenderObject::doAXRangeForIndex(unsigned index) const
{
    if (!isTextControl())
        return { };

    String elementText = text();
    if (!elementText.length() || index > elementText.length() - 1)
        return { };
    return { index, 1 };
}

// A substring of the text associated with this accessibility object that is
// specified by the given character range.
String AccessibilityRenderObject::doAXStringForRange(const CharacterRange& range) const
{
    if (!range.length || !isTextControl())
        return { };
    return text().substring(range.location, range.length);
}

// The bounding rectangle of the text associated with this accessibility object that is
// specified by the given range. This is the bounding rectangle a sighted user would see
// on the display screen, in pixels.
IntRect AccessibilityRenderObject::doAXBoundsForRange(const CharacterRange& range) const
{
    if (allowsTextRanges())
        return boundsForVisiblePositionRange(visiblePositionRangeForRange(range));
    return IntRect();
}

IntRect AccessibilityRenderObject::doAXBoundsForRangeUsingCharacterOffset(const CharacterRange& characterRange) const
{
    if (!allowsTextRanges())
        return { };
    auto range = rangeForCharacterRange(characterRange);
    if (!range)
        return { };
    return boundsForRange(*range);
}

AccessibilityObject* AccessibilityRenderObject::accessibilityImageMapHitTest(HTMLAreaElement& area, const IntPoint& point) const
{
    auto* mapAncestor = ancestorsOfType<HTMLMapElement>(area).first();
    auto* associatedImage = mapAncestor ? associatedAXImage(*mapAncestor) : nullptr;
    if (!associatedImage)
        return nullptr;

    for (const auto& child : associatedImage->children()) {
        if (child->elementRect().contains(point))
            return dynamicDowncast<AccessibilityObject>(child.get());
    }

    return nullptr;
}

AccessibilityObject* AccessibilityRenderObject::remoteSVGElementHitTest(const IntPoint& point) const
{
    AccessibilityObject* remote = remoteSVGRootElement(Create);
    if (!remote)
        return nullptr;
    
    IntSize offset = point - roundedIntPoint(boundingBoxRect().location());
    return remote->accessibilityHitTest(IntPoint(offset));
}

AccessibilityObject* AccessibilityRenderObject::elementAccessibilityHitTest(const IntPoint& point) const
{
    if (isSVGImage())
        return remoteSVGElementHitTest(point);
    
    return AccessibilityObject::elementAccessibilityHitTest(point);
}
    
AccessibilityObject* AccessibilityRenderObject::accessibilityHitTest(const IntPoint& point) const
{
    if (!m_renderer || !m_renderer->hasLayer())
        return nullptr;

    m_renderer->document().updateLayout();
    // Layout may have destroyed this renderer or layer, so re-check their presence.
    if (!m_renderer || !m_renderer->hasLayer())
        return nullptr;

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AccessibilityHitTest };
    HitTestResult hitTestResult { point };

    dynamicDowncast<RenderLayerModelObject>(*m_renderer)->layer()->hitTest(hitType, hitTestResult);
    RefPtr node = hitTestResult.innerNode();
    if (!node)
        return nullptr;

    if (auto* area = dynamicDowncast<HTMLAreaElement>(*node))
        return accessibilityImageMapHitTest(*area, point);

    if (auto* option = dynamicDowncast<HTMLOptionElement>(*node))
        node = option->ownerSelectElement();

    auto* renderer = node->renderer();
    auto* cache = renderer ? renderer->document().axObjectCache() : nullptr;
    RefPtr result = cache ? cache->getOrCreate(*renderer) : nullptr;
    if (!result)
        return nullptr;

    result->updateChildrenIfNecessary();
    // Allow the element to perform any hit-testing it might need to do to reach non-render children.
    result = result->elementAccessibilityHitTest(point);

    if (result && result->accessibilityIsIgnored()) {
        // If this element is the label of a control, a hit test should return the control.
        auto* controlObject = result->controlForLabelElement();
        if (controlObject && !controlObject->titleUIElement())
            return controlObject;

        result = result->parentObjectUnignored();
    }
    return result.get();
}

bool AccessibilityRenderObject::renderObjectIsObservable(RenderObject& renderer) const
{
    // AX clients will listen for AXValueChange on a text control.
    if (is<RenderTextControl>(renderer))
        return true;
    
    // AX clients will listen for AXSelectedChildrenChanged on listboxes.
    auto* node = renderer.node();
    if (!node)
        return false;
    
    if (auto* renderBox = dynamicDowncast<RenderBoxModelObject>(renderer); (renderBox && renderBox->isRenderListBox()) || nodeHasRole(node, "listbox"_s))
        return true;

    // Textboxes should send out notifications.
    if (auto* element = dynamicDowncast<Element>(*node); (element && contentEditableAttributeIsEnabled(element)) || nodeHasRole(node, "textbox"_s))
        return true;
    
    return false;
}
    
AccessibilityObject* AccessibilityRenderObject::observableObject() const
{
    // Find the object going up the parent chain that is used in accessibility to monitor certain notifications.
    for (RenderObject* renderer = this->renderer(); renderer && renderer->node(); renderer = renderer->parent()) {
        if (renderObjectIsObservable(*renderer)) {
            if (AXObjectCache* cache = axObjectCache())
                return cache->getOrCreate(*renderer);
        }
    }

    return nullptr;
}

String AccessibilityRenderObject::expandedTextValue() const
{
    if (AccessibilityObject* parent = parentObject()) {
        if (parent->hasTagName(abbrTag) || parent->hasTagName(acronymTag))
            return parent->getAttribute(titleAttr);
    }

    return String();
}

bool AccessibilityRenderObject::supportsExpandedTextValue() const
{
    if (roleValue() == AccessibilityRole::StaticText) {
        if (AccessibilityObject* parent = parentObject())
            return parent->hasTagName(abbrTag) || parent->hasTagName(acronymTag);
    }
    
    return false;
}

bool AccessibilityRenderObject::shouldIgnoreAttributeRole() const
{
    return m_ariaRole == AccessibilityRole::Document && hasContentEditableAttributeSet();
}

AccessibilityRole AccessibilityRenderObject::determineAccessibilityRole()
{
    if (!m_renderer)
        return AccessibilityNodeObject::determineAccessibilityRole();

#if ENABLE(APPLE_PAY)
    if (isApplePayButton())
        return AccessibilityRole::Button;
#endif

    // Sometimes we need to ignore the attribute role. Like if a tree is malformed,
    // we want to ignore the treeitem's attribute role.
    if ((m_ariaRole = determineAriaRoleAttribute()) != AccessibilityRole::Unknown && !shouldIgnoreAttributeRole())
        return m_ariaRole;

    Node* node = m_renderer->node();

    if (m_renderer->isRenderListItem())
        return AccessibilityRole::ListItem;
    if (m_renderer->isRenderListMarker())
        return AccessibilityRole::ListMarker;
    if (m_renderer->isRenderText())
        return AccessibilityRole::StaticText;
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (m_renderer->isBR())
        return AccessibilityRole::LineBreak;
#endif
    if (RefPtr img = dynamicDowncast<HTMLImageElement>(node); img && img->hasAttributeWithoutSynchronization(usemapAttr))
        return AccessibilityRole::ImageMap;
    if (m_renderer->isImage()) {
        if (is<HTMLInputElement>(node))
            return hasPopup() ? AccessibilityRole::PopUpButton : AccessibilityRole::Button;

        if (auto* svgRoot = remoteSVGRootElement(Create)) {
            if (svgRoot->hasAccessibleContent())
                return AccessibilityRole::SVGRoot;
        }
        return AccessibilityRole::Image;
    }

    if (m_renderer->isRenderView())
        return AccessibilityRole::WebArea;
    if (m_renderer->isRenderTextControlSingleLine()) {
        if (RefPtr input = dynamicDowncast<HTMLInputElement>(node))
            return roleFromInputElement(*input);
    }
    if (m_renderer->isRenderTextControlMultiLine())
        return AccessibilityRole::TextArea;
    if (m_renderer->isRenderMenuList())
        return AccessibilityRole::PopUpButton;

    if (m_renderer->isRenderOrLegacyRenderSVGRoot())
        return AccessibilityRole::SVGRoot;
    
    // Check for Ruby elements
    switch (m_renderer->style().display()) {
    case DisplayType::Ruby:
        return AccessibilityRole::RubyInline;
    case DisplayType::RubyBlock:
        return AccessibilityRole::RubyBlock;
    case DisplayType::RubyAnnotation:
        return AccessibilityRole::RubyText;
    case DisplayType::RubyBase:
        return AccessibilityRole::RubyBase;
    default:
        break;
    }

    if (m_renderer->isAnonymous() && (is<RenderTableCell>(m_renderer.get()) || is<RenderTableRow>(m_renderer.get()) || is<RenderTable>(m_renderer.get())))
        return AccessibilityRole::Ignored;

    // This return value is what will be used if AccessibilityTableCell determines
    // the cell should not be treated as a cell (e.g. because it is a layout table.
    if (is<RenderTableCell>(m_renderer.get()))
        return AccessibilityRole::TextGroup;
    // Table sections should be ignored.
    if (m_renderer->isRenderTableSection())
        return AccessibilityRole::Ignored;

    auto treatStyleFormatGroupAsInline = is<RenderInline>(*m_renderer) ? TreatStyleFormatGroupAsInline::Yes : TreatStyleFormatGroupAsInline::No;
    auto roleFromNode = determineAccessibilityRoleFromNode(treatStyleFormatGroupAsInline);
    if (roleFromNode != AccessibilityRole::Unknown)
        return roleFromNode;

#if !PLATFORM(COCOA)
    // This block should be deleted for all platforms, but doing so causes a lot of test failures that need to be sorted out.
    if (m_renderer->isRenderBlockFlow())
        return m_renderer->isAnonymousBlock() ? AccessibilityRole::TextGroup : AccessibilityRole::Generic;
#endif
    
    // InlineRole is the final fallback before assigning AccessibilityRole::Unknown to an object. It makes it
    // possible to distinguish truly unknown objects from non-focusable inline text elements
    // which have an event handler or attribute suggesting possible inclusion by the platform.
    if (is<RenderInline>(*m_renderer)
        && (hasAttributesRequiredForInclusion()
            || (node && node->hasEventListeners())
            || (supportsDatetimeAttribute() && !getAttribute(datetimeAttr).isEmpty())))
        return AccessibilityRole::Inline;

    return AccessibilityRole::Unknown;
}

bool AccessibilityRenderObject::inheritsPresentationalRole() const
{
    // ARIA states if an item can get focus, it should not be presentational.
    if (canSetFocusAttribute())
        return false;
    
    // ARIA spec says that when a parent object is presentational, and it has required child elements,
    // those child elements are also presentational. For example, <li> becomes presentational from <ul>.
    // http://www.w3.org/WAI/PF/aria/complete#presentation

    std::span<decltype(aTag)* const> parentTags;
    switch (roleValue()) {
    case AccessibilityRole::ListItem:
    case AccessibilityRole::ListMarker: {
        static constexpr std::array listItemParents { &dlTag, &menuTag, &olTag, &ulTag };
        parentTags = listItemParents;
        break;
    }
    case AccessibilityRole::GridCell:
    case AccessibilityRole::Cell: {
        static constexpr std::array tableCellParents { &tableTag };
        parentTags = tableCellParents;
        break;
    }
    default:
        // Not all elements need to do the following check, only ones that are required children.
        return false;
    }

    for (auto* parent = parentObject(); parent; parent = parent->parentObject()) {
        auto* accessibilityRenderer = dynamicDowncast<AccessibilityRenderObject>(*parent);
        if (!accessibilityRenderer)
            continue;

        RefPtr node = dynamicDowncast<Element>(accessibilityRenderer->node());
        if (!node)
            continue;

        // If native tag of the parent element matches an acceptable name, then return
        // based on its presentational status.
        auto& name = node->tagQName();
        if (std::any_of(parentTags.begin(), parentTags.end(), [&name] (auto* possibleName) { return possibleName->get() == name; }))
            return parent->roleValue() == AccessibilityRole::Presentational;
    }

    return false;
}
    
void AccessibilityRenderObject::addImageMapChildren()
{
    auto* renderImage = dynamicDowncast<RenderImage>(renderer());
    if (!renderImage)
        return;

    RefPtr map = renderImage->imageMap();
    if (!map)
        return;

    for (auto& area : descendantsOfType<HTMLAreaElement>(*map)) {
        // add an <area> element for this child if it has a link
        if (!area.isLink())
            continue;
        auto& areaObject = uncheckedDowncast<AccessibilityImageMapLink>(*axObjectCache()->create(AccessibilityRole::ImageMapLink));
        areaObject.setHTMLAreaElement(&area);
        areaObject.setHTMLMapElement(map.get());
        areaObject.setParent(this);
        if (!areaObject.accessibilityIsIgnored())
            addChild(&areaObject);
        else
            axObjectCache()->remove(areaObject.objectID());
    }
}

void AccessibilityRenderObject::addTextFieldChildren()
{
    RefPtr node = dynamicDowncast<HTMLInputElement>(this->node());
    if (!node)
        return;

    auto* spinButtonElement = dynamicDowncast<SpinButtonElement>(node->innerSpinButtonElement());
    if (!spinButtonElement)
        return;

    auto& axSpinButton = uncheckedDowncast<AccessibilitySpinButton>(*axObjectCache()->create(AccessibilityRole::SpinButton));
    axSpinButton.setSpinButtonElement(spinButtonElement);
    axSpinButton.setParent(this);
    addChild(&axSpinButton);
}
    
bool AccessibilityRenderObject::isSVGImage() const
{
    return remoteSVGRootElement(Create);
}
    
void AccessibilityRenderObject::detachRemoteSVGRoot()
{
    if (AccessibilitySVGRoot* root = remoteSVGRootElement(Retrieve))
        root->setParent(nullptr);
}

AccessibilitySVGRoot* AccessibilityRenderObject::remoteSVGRootElement(CreationChoice createIfNecessary) const
{
    auto* renderImage = dynamicDowncast<RenderImage>(renderer());
    if (!renderImage)
        return nullptr;

    auto* cachedImage = renderImage->cachedImage();
    if (!cachedImage)
        return nullptr;

    auto* svgImage = dynamicDowncast<SVGImage>(cachedImage->image());
    if (!svgImage)
        return nullptr;

    auto* frameView = svgImage->frameView();
    if (!frameView)
        return nullptr;

    auto* document = frameView->frame().document();
    if (!is<SVGDocument>(document))
        return nullptr;

    auto rootElement = DocumentSVG::rootElement(*document);
    if (!rootElement)
        return nullptr;

    RenderObject* rendererRoot = rootElement->renderer();
    if (!rendererRoot)
        return nullptr;

    // Use the AXObjectCache from this object, not the one from the SVG document above.
    // The SVG document is not connected to the top document of this object, thus it would result in the AX SVG objects to be created in a separate instance of AXObjectCache.
    auto* cache = axObjectCache();
    if (!cache)
        return nullptr;

    auto* rootSVGObject = createIfNecessary == Create ? cache->getOrCreate(*rendererRoot) : cache->get(rendererRoot);
    ASSERT(!createIfNecessary || rootSVGObject);
    return dynamicDowncast<AccessibilitySVGRoot>(rootSVGObject);
}
    
void AccessibilityRenderObject::addRemoteSVGChildren()
{
    RefPtr<AccessibilitySVGRoot> root = remoteSVGRootElement(Create);
    if (!root)
        return;

    // In order to connect the AX hierarchy from the SVG root element from the loaded resource
    // the parent must be set, because there's no other way to get back to who created the image.
    root->setParent(this);
    addChild(root.get());
}

void AccessibilityRenderObject::addCanvasChildren()
{
    // Add the unrendered canvas children as AX nodes, unless we're not using a canvas renderer
    // because JS is disabled for example.
    if (!node() || !node()->hasTagName(canvasTag) || (renderer() && !renderer()->isRenderHTMLCanvas()))
        return;

    // If it's a canvas, it won't have rendered children, but it might have accessible fallback content.
    // Clear m_childrenInitialized because AccessibilityNodeObject::addChildren will expect it to be false.
    ASSERT(!m_children.size());
    m_childrenInitialized = false;
    AccessibilityNodeObject::addChildren();
}

void AccessibilityRenderObject::addAttachmentChildren()
{
    if (!isAttachment())
        return;

    // LocalFrameView's need to be inserted into the AX hierarchy when encountered.
    Widget* widget = widgetForAttachmentView();
    if (!widget || !(widget->isLocalFrameView() || widget->isRemoteFrameView()))
        return;

    addChild(axObjectCache()->getOrCreate(*widget));
}

#if PLATFORM(COCOA)
void AccessibilityRenderObject::updateAttachmentViewParents()
{
    // Only the unignored parent should set the attachment parent, because that's what is reflected in the AX 
    // hierarchy to the client.
    if (accessibilityIsIgnored())
        return;
    
    for (const auto& child : m_children) {
        if (child->isAttachment()) {
            if (auto* liveChild = dynamicDowncast<AccessibilityObject>(child.get()))
                liveChild->overrideAttachmentParent(this);
        }
    }
}
#endif

// Some elements don't have an associated render object, meaning they won't be picked up by a walk of the render tree.
// For example, nodes that are `aria-hidden="false"` and `hidden`, or elements with `display: contents`.
// This function will find and add these elements to the AX tree.
void AccessibilityRenderObject::addNodeOnlyChildren()
{
    Node* node = this->node();
    if (!node)
        return;

    auto nodeHasDisplayContents = [] (Node& node) {
        RefPtr element = dynamicDowncast<Element>(node);
        return element && element->hasDisplayContents();
    };
    // First do a quick run through to determine if we have any interesting nodes (most often we will not).
    // If we do have any interesting nodes, we need to determine where to insert them so they match DOM order as close as possible.
    bool hasNodeOnlyChildren = false;
    for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
        if (child->renderer())
            continue;

        if (nodeHasDisplayContents(*child) || isNodeAriaVisible(*child)) {
            hasNodeOnlyChildren = true;
            break;
        }
    }
    
    if (!hasNodeOnlyChildren)
        return;

    WeakPtr cache = axObjectCache();
    if (!cache)
        return;
    // Iterate through all of the children, including those that may have already been added, and
    // try to insert the nodes in the correct place in the DOM order.
    unsigned insertionIndex = 0;
    for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
        if (child->renderer()) {
            // Find out where the last render sibling is located within m_children.
            AXCoreObject* childObject = axObjectCache()->get(child->renderer());
            if (childObject && childObject->accessibilityIsIgnored()) {
                auto& children = childObject->children();
                if (children.size())
                    childObject = children.last().get();
                else
                    childObject = nullptr;
            }

            if (childObject)
                insertionIndex = m_children.find(childObject) + 1;
            continue;
        }

        if (!nodeHasDisplayContents(*child) && !isNodeAriaVisible(*child))
            continue;

        unsigned previousSize = m_children.size();
        if (insertionIndex > previousSize)
            insertionIndex = previousSize;

        insertChild(cache->getOrCreate(*child), insertionIndex);
        insertionIndex += (m_children.size() - previousSize);
    }
}

#if USE(ATSPI)
RenderObject* AccessibilityRenderObject::markerRenderer() const
{
    if (accessibilityIsIgnored() || !isListItem() || !m_renderer || !m_renderer->isRenderListItem())
        return nullptr;

    return uncheckedDowncast<RenderListItem>(*m_renderer).markerRenderer();
}

void AccessibilityRenderObject::addListItemMarker()
{
    if (auto* marker = markerRenderer())
        insertChild(axObjectCache()->getOrCreate(*marker), 0);
}
#endif

void AccessibilityRenderObject::updateRoleAfterChildrenCreation()
{
    // If a menu does not have valid menuitem children, it should not be exposed as a menu.
    auto role = roleValue();
    if (role == AccessibilityRole::Menu) {
        // Elements marked as menus must have at least one menu item child.
        bool hasMenuItemDescendant = false;
        for (const auto& child : children()) {
            if (child->isMenuItem()) {
                hasMenuItemDescendant = true;
                break;
            }

            // Per the ARIA spec, groups with menuitem children are allowed as children of menus.
            // https://w3c.github.io/aria/#menu
            if (child->isGroup()) {
                for (const auto& grandchild : child->children()) {
                    if (grandchild->isMenuItem()) {
                        hasMenuItemDescendant = true;
                        break;
                    }
                }
            }
        }

        if (!hasMenuItemDescendant)
            m_role = AccessibilityRole::Generic;
    }
    if (role == AccessibilityRole::SVGRoot && !children().size())
        m_role = AccessibilityRole::Image;

    if (role != m_role) {
        if (auto* cache = axObjectCache())
            cache->handleRoleChanged(*this);
    }
}
    
void AccessibilityRenderObject::addChildren()
{
    if (UNLIKELY(!renderer())) {
        AccessibilityNodeObject::addChildren();
        return;
    }
    // If the need to add more children in addition to existing children arises,
    // childrenChanged should have been called, leaving the object with no children.
    ASSERT(!m_childrenInitialized); 
    m_childrenInitialized = true;

    auto clearDirtySubtree = makeScopeExit([&] {
        m_subtreeDirty = false;
    });

    if (!canHaveChildren())
        return;

    auto addChildIfNeeded = [this](AccessibilityObject& object) {
#if USE(ATSPI)
        if (object.renderer()->isRenderListMarker())
            return;
#endif
        auto owners = object.owners();
        if (owners.size() && !owners.contains(this))
            return;

        addChild(&object);
    };

    for (auto& object : AXChildIterator(*this))
        addChildIfNeeded(object);

    addNodeOnlyChildren();
    addAttachmentChildren();
    addImageMapChildren();
    addTextFieldChildren();
    addCanvasChildren();
    addRemoteSVGChildren();
#if USE(ATSPI)
    addListItemMarker();
#endif
#if PLATFORM(COCOA)
    updateAttachmentViewParents();
#endif
    updateOwnedChildren();

    m_subtreeDirty = false;
    updateRoleAfterChildrenCreation();
}

void AccessibilityRenderObject::setAccessibleName(const AtomString& name)
{
    // Setting the accessible name can store the value in the DOM
    if (!m_renderer)
        return;

    Node* node = nullptr;
    // For web areas, set the aria-label on the HTML element.
    if (isWebArea())
        node = m_renderer->document().documentElement();
    else
        node = m_renderer->node();

    if (RefPtr element = dynamicDowncast<Element>(node))
        element->setAttribute(aria_labelAttr, name);
}

bool AccessibilityRenderObject::hasBoldFont() const
{
    if (!m_renderer)
        return false;
    
    return isFontWeightBold(m_renderer->style().fontDescription().weight());
}

bool AccessibilityRenderObject::hasItalicFont() const
{
    if (!m_renderer)
        return false;
    
    return isItalic(m_renderer->style().fontDescription().italic());
}

bool AccessibilityRenderObject::hasPlainText() const
{
    if (!m_renderer)
        return false;

    if (!canHavePlainText())
        return false;

    const RenderStyle& style = m_renderer->style();
    return style.fontDescription().weight() == normalWeightValue()
        && !isItalic(style.fontDescription().italic())
        && style.textDecorationsInEffect().isEmpty();
}

bool AccessibilityRenderObject::hasSameFont(const AXCoreObject& object) const
{
    auto* renderer = object.renderer();
    if (!m_renderer || !renderer)
        return false;
    
    return m_renderer->style().fontDescription().families() == renderer->style().fontDescription().families();
}

#if ENABLE(APPLE_PAY)
bool AccessibilityRenderObject::isApplePayButton() const
{
    if (!m_renderer)
        return false;
    return m_renderer->style().usedAppearance() == StyleAppearance::ApplePayButton;
}

ApplePayButtonType AccessibilityRenderObject::applePayButtonType() const
{
    if (!m_renderer)
        return ApplePayButtonType::Plain;
    return m_renderer->style().applePayButtonType();
}
#endif

bool AccessibilityRenderObject::hasSameFontColor(const AXCoreObject& object) const
{
    auto* renderer = object.renderer();
    if (!m_renderer || !renderer)
        return false;
    
    return m_renderer->style().visitedDependentColor(CSSPropertyColor) == renderer->style().visitedDependentColor(CSSPropertyColor);
}

bool AccessibilityRenderObject::hasSameStyle(const AXCoreObject& object) const
{
    auto* renderer = object.renderer();
    if (!m_renderer || !renderer)
        return false;
    
    return m_renderer->style() == renderer->style();
}

bool AccessibilityRenderObject::hasUnderline() const
{
    if (!m_renderer)
        return false;
    
    return m_renderer->style().textDecorationsInEffect().contains(TextDecorationLine::Underline);
}

String AccessibilityRenderObject::secureFieldValue() const
{
    ASSERT(isSecureField());

    // Look for the RenderText object in the RenderObject tree for this input field.
    RenderObject* renderer = node()->renderer();
    while (renderer && !is<RenderText>(renderer))
        renderer = uncheckedDowncast<RenderElement>(*renderer).firstChild();

    auto* renderText = dynamicDowncast<RenderText>(renderer);
    // Return the text that is actually being rendered in the input field.
    return renderText ? renderText->textWithoutConvertingBackslashToYenSymbol() : String();
}

ScrollableArea* AccessibilityRenderObject::getScrollableAreaIfScrollable() const
{
    // If the parent is a scroll view, then this object isn't really scrollable, the parent ScrollView should handle the scrolling.
    if (auto* parent = parentObject()) {
        if (parent->isScrollView())
            return nullptr;
    }

    auto* box = dynamicDowncast<RenderBox>(renderer());
    if (!box || !box->canBeScrolledAndHasScrollableArea() || !box->layer())
        return nullptr;

    return box->layer()->scrollableArea();
}

void AccessibilityRenderObject::scrollTo(const IntPoint& point) const
{
    auto* box = dynamicDowncast<RenderBox>(renderer());
    if (!box || !box->canBeScrolledAndHasScrollableArea())
        return;

    // FIXME: is point a ScrollOffset or ScrollPosition? Test in RTL overflow.
    ASSERT(box->layer());
    ASSERT(box->layer()->scrollableArea());
    box->layer()->scrollableArea()->scrollToOffset(point);
}

FloatRect AccessibilityRenderObject::frameRect() const
{
    auto* box = dynamicDowncast<RenderBox>(renderer());
    return box ? convertFrameToSpace(box->frameRect(), AccessibilityConversionSpace::Page) : FloatRect();
}

#if ENABLE(MATHML)
bool AccessibilityRenderObject::isIgnoredElementWithinMathTree() const
{
    // We ignore anonymous boxes inserted into RenderMathMLBlocks to honor CSS rules.
    // See https://www.w3.org/TR/css3-box/#block-level0
    return m_renderer && m_renderer->isAnonymous() && m_renderer->parent() && is<RenderMathMLBlock>(m_renderer->parent());
}
#endif
    
} // namespace WebCore
