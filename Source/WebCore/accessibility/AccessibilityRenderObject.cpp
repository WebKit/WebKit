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

#include "AXImageMapHelpers.h"
#include "AXListHelpers.h"
#include "AXLogger.h"
#include "AXLoggerBase.h"
#include "AXNotifications.h"
#include "AXObjectCacheInlines.h"
#include "AXUtilities.h"
#include "AccessibilityMediaHelpers.h"
#include "AccessibilityObjectInlines.h"
#include "AccessibilitySVGObject.h"
#include "AccessibilitySpinButton.h"
#include "CachedImage.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ComplexTextController.h"
#include "ComposedTreeIterator.h"
#include "ContainerNodeInlines.h"
#include "DocumentSVG.h"
#include "DocumentView.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "ElementAncestorIteratorInlines.h"
#include "EventTargetInlines.h"
#include "FloatRect.h"
#include "FocusOptions.h"
#include "FontCascade.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "HTMLAreaElement.h"
#include "HTMLAttachmentElement.h"
#include "HTMLBRElement.h"
#include "HTMLDetailsElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLMapElement.h"
#include "HTMLMediaElement.h"
#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLOptionsCollection.h"
#include "HTMLSelectElement.h"
#include "HTMLSummaryElement.h"
#include "HTMLTableElement.h"
#include "HTMLTextAreaElement.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Image.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorLogicalOrderTraversal.h"
#include "InlineIteratorTextBoxInlines.h"
#include "LegacyRenderSVGRoot.h"
#include "LegacyRenderSVGShape.h"
#include "LineSelection.h"
#include "LocalFrame.h"
#include "LocalizedStrings.h"
#include "NodeList.h"
#include "Page.h"
#include "PathUtilities.h"
#include "PluginViewBase.h"
#include "PositionInlines.h"
#include "ProgressTracker.h"
#include "Range.h"
#include "RenderButton.h"
#include "RenderElementInlines.h"
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
#include "RenderObjectInlines.h"
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
#include "ShadowRootMode.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "Text.h"
#include "TextControlInnerElements.h"
#include "TextIterator.h"
#include "TextRecognitionOptions.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "VisibleUnits.h"
#include "WidthIterator.h"
#include <algorithm>
#include <ranges>
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

static Node* nodeForRenderer(RenderObject& renderer)
{
    if (!renderer.isRenderView()) [[likely]]
        return renderer.node();
    return &renderer.document();
}

AccessibilityRenderObject::AccessibilityRenderObject(AXID axID, RenderObject& renderer, AXObjectCache& cache)
    : AccessibilityNodeObject(axID, nodeForRenderer(renderer), cache)
    , m_renderer(renderer)
{
}

AccessibilityRenderObject::AccessibilityRenderObject(AXID axID, Node& node, AXObjectCache& cache)
    : AccessibilityNodeObject(axID, &node, cache)
{
    // We should only ever create an instance of this class with a node if that node has no renderer (i.e. because of display:contents).
    ASSERT(!node.renderer());
}

AccessibilityRenderObject::~AccessibilityRenderObject()
{
    ASSERT(isDetached());
}

Ref<AccessibilityRenderObject> AccessibilityRenderObject::create(AXID axID, RenderObject& renderer, AXObjectCache& cache)
{
    return adoptRef(*new AccessibilityRenderObject(axID, renderer, cache));
}

Ref<AccessibilityRenderObject> AccessibilityRenderObject::create(AXID axID, Node& node, AXObjectCache& cache)
{
    return adoptRef(*new AccessibilityRenderObject(axID, node, cache));
}

void AccessibilityRenderObject::detachRemoteParts(AccessibilityDetachmentType detachmentType)
{
    AccessibilityNodeObject::detachRemoteParts(detachmentType);

    detachRemoteSVGRoot();
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
    if (renderBlock && (startOfConts = startOfContinuations(*renderBlock)))
        previousSibling = childBeforeConsideringContinuations(startOfConts, renderBlock.get());
    else if (renderBlock && renderBlock->isAnonymousBlock() && firstChildIsInlineContinuation(*renderBlock)) {
        // Case 2: Anonymous block parent of the end of a continuation - skip all the way to before
        // the parent of the start, since everything in between will be linked up via the continuation.
        auto* firstParent = startOfContinuations(*renderBlock->firstChild())->parent();
        ASSERT(firstParent);
        while (firstChildIsInlineContinuation(*firstParent))
            firstParent = startOfContinuations(*firstParent->firstChild())->parent();
        previousSibling = firstParent->previousSibling();
    } else if (RenderObject* ps = m_renderer->previousSibling()) {
        // Case 3: The node has an actual previous sibling
        previousSibling = ps;
    } else if (is<RenderInline>(m_renderer->parent()) && (startOfConts = startOfContinuations(*m_renderer->parent()))) {
        // Case 4: This node has no previous siblings, but its parent is an inline,
        // and is another node's inline continutation. Follow the continuation chain.
        previousSibling = childBeforeConsideringContinuations(startOfConts, m_renderer->parent()->firstChild());
    }

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
    else if (renderBlock && renderBlock->isAnonymousBlock() && lastChildHasContinuation(*renderBlock)) {
        // Case 2: Anonymous block parent of the start of a continuation - skip all the way to
        // after the parent of the end, since everything in between will be linked up via the continuation.
        auto* lastParent = endOfContinuations(*renderBlock->lastChild())->parent();
        ASSERT(lastParent);
        while (lastChildHasContinuation(*lastParent))
            lastParent = endOfContinuations(*lastParent->lastChild())->parent();
        nextSibling = lastParent->nextSibling();
    } else if (RenderObject* ns = m_renderer->nextSibling())
        nextSibling = ns;
    else if (isInlineWithContinuation(*m_renderer)) {
        // Case 4: node is an inline with a continuation. Next sibling is the next sibling of the end
        // of the continuation chain.
        nextSibling = endOfContinuations(*m_renderer)->nextSibling();
    }

    // Case 5: node has no next sibling, and its parent is an inline with a continuation.
    // Case 5.1: After case 4, (the element was inline w/ continuation but had no sibling), then check it's parent.
    if (!nextSibling && m_renderer->parent() && isInlineWithContinuation(*m_renderer->parent())) {
        auto& continuation = *downcast<RenderInline>(*m_renderer->parent()).continuation();

        // Case 5a: continuation is a block - in this case the block itself is the next sibling.
        if (is<RenderBlock>(continuation))
            nextSibling = &continuation;
        else {
            // Case 5b: continuation is an inline - in this case the inline's first child is the next sibling
            nextSibling = firstChildConsideringContinuation(continuation);
        }
    }

    if (!nextSibling)
        return nullptr;

    CheckedPtr cache = axObjectCache();
    if (!cache)
        return nullptr;

    // After case 4, there are chances that nextSibling has the same node as the current renderer,
    // which might lead to looping over the same object repeatedly.
    if (nextSibling->node() && nextSibling->node() == m_renderer->node()) {
        if (RefPtr nextObject = cache->getOrCreate(*nextSibling)) {
            if (nextObject.get() == this) {
                // WebKit accessibility objects use DOM nodes as the "primary key" (i.e. in m_nodeIdMapping).
                // This can cause a bit of trouble for continuations, which result in multiple renderers being associated
                // with the same node. That can cause us to get into this branch — if nextSibling or us is a continuation,
                // we will be different renderers with the same node, and thus `nextObject` will be us.
                //
                // Fallback to walking the DOM in this case to avoid looping infinitely.
                return AccessibilityNodeObject::nextSibling();
            }
            return nextObject->nextSibling();
        }
    }

    RefPtr nextObject = cache->getOrCreate(*nextSibling);
    RefPtr nextAXRenderObject = dynamicDowncast<AccessibilityRenderObject>(nextObject);
    RefPtr nextRenderParent = nextAXRenderObject ? cache->getOrCreate(nextAXRenderObject->renderParentObject()) : nullptr;

    // Make sure the next sibling has the same render parent.
    return !nextRenderParent || nextRenderParent == cache->getOrCreate(renderParentObject()) ? nextObject.unsafeGet() : nullptr;
}

static RenderBoxModelObject* nextContinuation(RenderObject& renderer)
{
    if (!renderer.isBlockLevelReplacedOrAtomicInline()) {
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

AccessibilityObject* AccessibilityRenderObject::parentObject() const
{
    if (RefPtr ownerParent = ownerParentObject()) [[unlikely]]
        return ownerParent.unsafeGet();

#if USE(ATSPI)
    // FIXME: Consider removing this ATSPI-only branch with https://bugs.webkit.org/show_bug.cgi?id=282117.
    if (auto* displayContentsParent = this->displayContentsParent())
        return displayContentsParent;
#endif // USE(ATSPI)

    if (!m_renderer)
        return AccessibilityNodeObject::parentObject();

    WeakPtr cache = axObjectCache();
    if (!cache)
        return nullptr;

#if !USE(ATSPI)
    // FIXME: This compiler directive can be removed after https://bugs.webkit.org/show_bug.cgi?id=282117 is fixed.
    RefPtr node = this->node();
    if (RefPtr parentNode = composedParentIgnoringDocumentFragments(node.get())) {
        if (RefPtr parent = cache->getOrCreate(*parentNode))
            return parent.unsafeGet();
    }

    if (CheckedPtr renderElement = dynamicDowncast<RenderElement>(m_renderer.get()); renderElement && renderElement->isBeforeOrAfterContent()) {
        // In AccessibilityRenderObject::addChildren(), we make the generating element add ::before
        // and ::after as children, so we must match that here by making sure ::before and ::after regard the
        // generating element as their parent rather than their "natural" render tree parent. This avoids
        // a parent-child mismatch which can cause issues for ATs.
        if (RefPtr parent = cache->getOrCreate(renderElement->generatingElement()))
            return parent.unsafeGet();
    }
#endif // !USE(ATSPI)

    // Expose markers that are not direct children of a list item too.
    if (m_renderer->isRenderListMarker()) {
        for (auto& listItemAncestor : ancestorsOfType<RenderListItem>(*m_renderer)) {
            RefPtr parent = dynamicDowncast<AccessibilityRenderObject>(axObjectCache()->getOrCreate(listItemAncestor));
            if (parent && parent->markerRenderer() == m_renderer)
                return parent.unsafeGet();
        }
    }

    if (auto* parentObject = renderParentObject())
        return cache->getOrCreate(*parentObject);

    // WebArea's parent should be the scroll view containing it.
    if (isWebArea())
        return cache->getOrCreate(m_renderer->view().frameView());

    return nullptr;
}

#if ENABLE_ACCESSIBILITY_LOCAL_FRAME

AccessibilityObject* AccessibilityRenderObject::crossFrameParentObject() const
{
    return nullptr;
}

AccessibilityObject* AccessibilityRenderObject::crossFrameChildObject() const
{
    return nullptr;
}

#endif // ENABLE_ACCESSIBILITY_LOCAL_FRAME

bool AccessibilityRenderObject::isAttachment() const
{
    RefPtr widget = this->widget();
    // WebKit2 plugins need to be treated differently than attachments, so return false here.
    // Only WebKit1 plugins have an associated platformWidget.
    if (is<PluginViewBase>(widget) && !widget->platformWidget())
        return false;

    auto* renderer = this->renderer();
    // Widgets are the replaced elements that we represent to AX as attachments
    return renderer && renderer->isRenderWidget();
}

#if ENABLE(ATTACHMENT_ELEMENT)
bool AccessibilityRenderObject::isAttachmentElement() const
{
    return is<HTMLAttachmentElement>(node());
}
#endif

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
        return AccessibilityNodeObject::anchorElement();

    WeakPtr cache = axObjectCache();
    if (!cache)
        return nullptr;

    RenderObject* currentRenderer;

    // Search up the render tree for a RenderObject with a DOM node. Defer to an earlier continuation, though.
    for (currentRenderer = renderer(); currentRenderer && !currentRenderer->node(); currentRenderer = currentRenderer->parent()) {
        if (CheckedPtr blockRenderer = dynamicDowncast<RenderBlock>(*currentRenderer); blockRenderer && blockRenderer->isAnonymousBlock()) {
            if (auto* continuation = blockRenderer->continuation())
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

        RefPtr object = cache ? cache->getOrCreate(*node) : nullptr;
        if (object && object->isLink())
            return dynamicDowncast<Element>(*node);
    }

    return nullptr;
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
    WeakPtr renderText = dynamicDowncast<RenderText>(*m_renderer);
    bool includeAllChildren = false;
#if USE(ATSPI)
    // Only ATSPI ever sets IncludeAllChildren.
    includeAllChildren = mode.childrenInclusion == TextUnderElementMode::Children::IncludeAllChildren;
#endif
    if (renderText || includeAllChildren) {
        // If possible, use a text iterator to get the text, so that whitespace
        // is handled consistently.
        RefPtr<Document> nodeDocument;
        std::optional<SimpleRange> textRange;
        if (RefPtr node = m_renderer->node()) {
            nodeDocument = node->document();
            textRange = makeRangeSelectingNodeContents(*node);
        }
#if USE(ATSPI)
        // FIXME: Consider removing this ATSPI-only branch with https://bugs.webkit.org/show_bug.cgi?id=282117.
        else {
            // For anonymous blocks, we work around not having a direct node to create a range from
            // defining one based in the two external positions defining the boundaries of the subtree.
            RenderObject* firstChildRenderer = m_renderer->firstChildSlow();
            RenderObject* lastChildRenderer = m_renderer->lastChildSlow();
            if (firstChildRenderer && firstChildRenderer->node() && lastChildRenderer && lastChildRenderer->node()) {
                // We define the start and end positions for the range as the ones right before and after
                // the first and the last nodes in the DOM tree that is wrapped inside the anonymous block.
                auto& firstNodeInBlock = *firstChildRenderer->node();
                nodeDocument = firstNodeInBlock.document();
                textRange = makeSimpleRange(positionInParentBeforeNode(&firstNodeInBlock), positionInParentAfterNode(lastChildRenderer->node()));
            }
        }
#endif // USE(ATSPI)

        if (nodeDocument && textRange) {
            if (RefPtr frame = nodeDocument->frame()) {
                // catch stale WebCoreAXObject (see <rdar://problem/3960196>)
                if (frame->document() != nodeDocument.get())
                    return { };

                return plainText(*textRange, textIteratorBehaviorForTextRange());
            }
        }

        // Sometimes text fragments don't have Nodes associated with them (like when
        // CSS content is used to insert text or when a RenderCounter is used.)
        if (WeakPtr renderTextFragment = dynamicDowncast<RenderTextFragment>(renderText.get())) {
            // The alt attribute may be set on a text fragment through CSS, which should be honored.
            if (auto& altText = renderTextFragment->altText(); !altText.isNull())
                return altText;
            return renderTextFragment->contentString();
        }
        if (renderText)
            return renderText->text();
    }

    return AccessibilityNodeObject::textUnderElement(WTFMove(mode));
}

bool AccessibilityRenderObject::shouldGetTextFromNode(const TextUnderElementMode& mode) const
{
    if (!m_renderer)
        return true;

#if USE(ATSPI)
    // AccessibilityRenderObject::textUnderElement() gets the text of anonymous blocks by using
    // the child nodes to define positions. CSS tables and their anonymous descendants lack
    // children with nodes.
    if (m_renderer->isAnonymous() && m_renderer->isTablePart())
        return mode.childrenInclusion == TextUnderElementMode::Children::IncludeAllChildren;
#else
    UNUSED_PARAM(mode);
#endif // USE(ATSPI)

    // AccessibilityRenderObject::textUnderElement() calls rangeOfContents() to create the text
    // range. rangeOfContents() does not include CSS-generated content.
    if (CheckedPtr renderElement = dynamicDowncast<RenderElement>(m_renderer.get()); renderElement && renderElement->isBeforeOrAfterContent())
        return true;
    if (RefPtr node = m_renderer->node()) {
        RefPtr firstChild = node->pseudoAwareFirstChild();
        RefPtr lastChild = node->pseudoAwareLastChild();
        if ((firstChild && firstChild->isPseudoElement()) || (lastChild && lastChild->isPseudoElement()))
            return true;
    }

    return false;
}

String AccessibilityRenderObject::stringValue() const
{
#if PLATFORM(IOS_FAMILY)
    if (RefPtr element = mediaElement())
        return localizedMediaTimeDescription(element->currentTime());
#endif

    if (isNativeLabel())
        return isLabelContainingOnlyStaticText() ? textUnderElement() : AccessibilityNodeObject::stringValue();

    if (!m_renderer)
        return AccessibilityNodeObject::stringValue();

    bool isStaticText = this->isStaticText();
    if (isStaticText && node()) {
        // FIXME: Implement text stitching for node-less static text, like CSS generated content.
        // This is relevant here because only AccessibilityNodeObject::stringValue() knows how to
        // stitch text. Ideally, we would shrink this implementation into AccessibilityNodeObject,
        // since that is intended to become the "node and or renderer having" accessibility object class.
        return AccessibilityNodeObject::stringValue();
    }

    if (isStaticText || isTextControl() || isSecureField()) {
        // A combobox is considered a text control, and its value is handled in AXNodeObject.
        if (isComboBox())
            return AccessibilityNodeObject::stringValue();
        return text();
    }

    if (auto* renderMenuList = dynamicDowncast<RenderMenuList>(m_renderer.get())) {
        // RenderMenuList will go straight to the text() of its selected item.
        // This has to be overridden in the case where the selected item has an ARIA label.
        Ref selectElement = renderMenuList->selectElement();
        int selectedIndex = selectElement->selectedIndex();
        const auto& listItems = selectElement->listItems();
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
        return renderListMarker->textWithSuffix();
#else
        return renderListMarker->textWithoutSuffix();
#endif
    }

    if (isWebArea())
        return { };

    if (isDateTime()) {
        if (RefPtr input = dynamicDowncast<HTMLInputElement>(node()))
            return input->visibleValue();

        // As fallback, gather the static text under this.
        String value;
        Accessibility::enumerateUnignoredDescendants(*const_cast<AccessibilityRenderObject*>(this), false, [&value] (const auto& object) {
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
    for (RefPtr<AccessibilityObject> parent = const_cast<AccessibilityRenderObject*>(this); parent; parent = parent->parentObject()) {
        if (parent->isAccessibilitySVGRoot()) {
            rect.moveBy(parent->parentObject()->boundingBoxRect().location());
            break;
        }
    }
}

LayoutRect AccessibilityRenderObject::boundingBoxRect() const
{
    CheckedPtr renderer = this->renderer();
    if (!renderer)
        return AccessibilityNodeObject::boundingBoxRect();

    RefPtr node = renderer->node();
    if (node) // If we are a continuation, we want to make sure to use the primary renderer.
        renderer = node->renderer();

    // absoluteFocusRingQuads will query the hierarchy below this element, which for large webpages can be very slow.
    // For a web area, which will have the most elements of any element, absoluteQuads should be used.
    // We should also use absoluteQuads for SVG elements, otherwise transforms won't be applied.
    Vector<FloatQuad> quads;
    bool isSVGRoot = false;

    if (renderer->isRenderOrLegacyRenderSVGRoot())
        isSVGRoot = true;

    if (auto* renderText = dynamicDowncast<RenderText>(*renderer)) {
        auto stitchState = this->stitchState();
        if (!stitchState.stitchedIntoID || *stitchState.stitchedIntoID != objectID() || stitchState.group.isEmpty())
            quads = renderText->absoluteQuadsClippedToEllipsis();
        else {
            // |this| is a stitching of multiple objects, so we need to combine all of their bounding boxes.

            CheckedPtr cache = axObjectCache();
            RefPtr endNode = !stitchState.group.isEmpty() && cache ? lastNode(stitchState.group, *cache) : nullptr;
            if (endNode && endNode != node) {
                if (std::optional range = makeSimpleRange(positionBeforeNode(node.get()), positionAfterNode(endNode.get())))
                    quads = RenderObject::absoluteTextQuads(*range);
            }
            if (quads.isEmpty())
                quads = renderText->absoluteQuadsClippedToEllipsis();
        }
    }
    else if (isWebArea() || isSVGRoot)
        renderer->absoluteQuads(quads);
    else
        renderer->absoluteFocusRingQuads(quads);

    LayoutRect result = boundingBoxForQuads(renderer.get(), quads);

    RefPtr document = this->document();
    if (document && document->isSVGDocument())
        offsetBoundingBoxForRemoteSVGElement(result);

    // The size of the web area should be the content size, not the clipped size.
    if (isWebArea())
        result.setSize(renderer->view().frameView().contentsSize());

    if (result.isEmpty())
        return nonEmptyAncestorBoundingBox();
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

    if (CheckedPtr renderText = dynamicDowncast<RenderText>(*m_renderer)) {
        Vector<LayoutRect> rects;
        renderText->boundingRects(rects, flooredLayoutPoint(renderText->localToAbsolute()));
        // If only 1 rect, don't compute path since the bounding rect will be good enough.
        if (rects.size() < 2)
            return { };

        // Compute the path only if this is the last part of a line followed by the beginning of the next line.
        auto& style = renderText->style();
        bool rightToLeftText = style.writingMode().isBidiRTL();
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

        auto outlineOffset = Style::evaluate<float>(style.outlineOffset(), Style::ZoomNeeded { });
        float deviceScaleFactor = renderText->document().deviceScaleFactor();
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

AccessibilityObject* AccessibilityRenderObject::titleUIElement() const
{
    if (m_renderer && isFieldset())
        return axObjectCache()->getOrCreate(dynamicDowncast<RenderBlock>(*m_renderer)->findFieldsetLegend(RenderBlock::FieldsetIncludeFloatingOrOutOfFlow));

    if (is<RenderTableCell>(m_renderer.get())) {
        // Try to find if the first cell in this row is a <th>. If it is,
        // then it can act as the title ui element. (This is only in the
        // case when the table is not appearing as an AXTable.)
        if (isExposedTableCell())
            return nullptr;

        // Table cells that are th cannot have title ui elements, since by definition
        // they are title ui elements
        if (WebCore::elementName(checkedNode().get()) == ElementName::HTML_th)
            return nullptr;

        CheckedRef renderCell = downcast<RenderTableCell>(*m_renderer);

        // If this cell is in the first column, there is no need to continue.
        int col = renderCell->col();
        if (!col)
            return nullptr;

        CheckedPtr section = renderCell->section();
        if (!section)
            return nullptr;

        int row = renderCell->rowIndex();
        CheckedPtr headerCell = section->primaryCellAt(row, 0);
        if (!headerCell || headerCell.get() == renderCell.ptr())
            return nullptr;

        RefPtr element = headerCell->element();
        if (!element || element->elementName() != ElementName::HTML_th)
            return nullptr;

        CheckedPtr cache = axObjectCache();
        return cache ? cache->getOrCreate(*headerCell) : nullptr;
    }

    return downcast<AccessibilityObject>(AccessibilityNodeObject::titleUIElement());
}

bool AccessibilityRenderObject::isAllowedChildOfTree() const
{
    // Determine if this is in a tree. If so, we apply special behavior to make it work like an AXOutline.
    RefPtr axObj = parentObject();
    bool isInTree = false;
    bool isTreeItemDescendant = false;
    while (axObj) {
        if (axObj->role() == AccessibilityRole::TreeItem)
            isTreeItemDescendant = true;
        if (axObj->isTree()) {
            isInTree = true;
            break;
        }
        axObj = axObj->parentObject();
    }

    // If the object is in a tree, only tree items should be exposed (and the children of tree items).
    if (isInTree) {
        auto role = this->role();
        if (role != AccessibilityRole::TreeItem && role != AccessibilityRole::StaticText && !isTreeItemDescendant)
            return false;
    }
    return true;
}

AccessibilityObject* AccessibilityRenderObject::containingTree() const
{
    return Accessibility::findAncestor<AccessibilityObject>(*this, false, [] (const auto& ancestor) {
        return ancestor.isTree();
    });
}

static AccessibilityObjectInclusion objectInclusionFromAltText(const String& altText)
{
    // Don't ignore an image that has alt text.
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

    RefPtr ownerElement = renderer->document().ownerElement();
    return ownerElement && hasPresentationRole(*ownerElement);
}

bool AccessibilityRenderObject::computeIsIgnored() const
{
#ifndef NDEBUG
    ASSERT(m_initialized);
#endif

    if (!m_renderer)
        return AccessibilityNodeObject::computeIsIgnored();

    if (isTree())
        return isIgnoredByDefault();

    // Check first if any of the common reasons cause this element to be ignored.
    // Then process other use cases that need to be applied to all the various roles
    // that AccessibilityRenderObjects take on.
    AccessibilityObjectInclusion decision = defaultObjectInclusion();
    if (decision == AccessibilityObjectInclusion::IncludeObject)
        return false;
    if (decision == AccessibilityObjectInclusion::IgnoreObject)
        return true;

    if (role() == AccessibilityRole::Ignored)
        return true;

    // Needs to happen before the presentational role check, since we want to expose table cells if they are in an exposable table (even if within a presentational role).
    if (isTableCell()) {
        // Ignore anonymous table cells as long as they're not in a table (ie. when display:table is used).
        RefPtr parentTable = this->parentTable();
        RefPtr parentElement = parentTable ? parentTable->element() : nullptr;
        bool inTable = parentElement && (parentElement->elementName() == ElementName::HTML_table || hasTableRole(*parentElement));
        if (!inTable && !element())
            return true;

        if (isExposedTableCell())
            return false;
    }

    if (ignoredFromPresentationalRole())
        return true;

    // WebAreas should be ignored if their iframe container is marked as presentational.
    if (webAreaIsPresentational(renderer()))
        return true;

    // An ARIA tree can only have tree items and static text as children.
    if (!isAllowedChildOfTree())
        return true;

    if (isAccessibilityList())
        return false;

    // Allow the platform to decide if the attachment is ignored or not.
    if (isAttachment())
        return accessibilityIgnoreAttachment();

#if ENABLE(ATTACHMENT_ELEMENT)
    if (isAttachmentElement())
        return false;
#endif

#if PLATFORM(COCOA)
    // If this widget has an underlying AX object, don't ignore it.
    if (widget() && widget()->accessibilityObject())
        return false;
#endif

    if (isExposableTable())
        return false;

    // ignore popup menu items because AppKit does
    if (m_renderer && ancestorsOfType<RenderMenuList>(*m_renderer).first())
        return true;

    // https://webkit.org/b/161276 Getting the controlObject might cause the m_renderer to be nullptr.
    if (!m_renderer)
        return true;

    if (m_renderer->isBR())
        return true;

    if (WeakPtr renderText = dynamicDowncast<RenderText>(m_renderer.get())) {
        // Text elements with no rendered text, or only whitespace should not be part of the AX tree.
        if (!renderText->hasRenderedText()) {
            // Layout must be clean to make the right decision here (because hasRenderedText() can return false solely because layout is dirty).
            ASSERT(!renderText->needsLayout() || !renderText->text().length());
            return true;
        }

        if (renderText->text().containsOnly<isASCIIWhitespace>())
            return true;

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
            if (ancestor->role() == AccessibilityRole::TextField)
                return true;

            if (checkForIgnored && !ancestor->isIgnored()) {
                checkForIgnored = false;
                // Static text beneath MenuItems are just reported along with the menu item, so it's ignored on an individual level.
                if (ancestor->isMenuItem())
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

    switch (role()) {
    case AccessibilityRole::Audio:
    case AccessibilityRole::DescriptionListTerm:
    case AccessibilityRole::DescriptionListDetail:
    case AccessibilityRole::Details:
    case AccessibilityRole::DocumentArticle:
    case AccessibilityRole::LandmarkRegion:
    case AccessibilityRole::ListItem:
    case AccessibilityRole::SectionFooter:
    case AccessibilityRole::SectionHeader:
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

        // https://github.com/w3c/aria/pull/2224
        // If the image is a descendant of a <figure>, don't ignore it due to <figure> potentially participating in its accname.
        RefPtr node = m_renderer->node();
        for (RefPtr ancestor = node ? node->parentNode() : nullptr; ancestor; ancestor = ancestor->parentNode()) {
            if (RefPtr element = dynamicDowncast<HTMLElement>(ancestor); element && element->hasTagName(figureTag))
                return false;
        }

        // First check the RenderImage's altText (which can be set through a style sheet, or come from the Element).
        // However, if this is not a native image, fallback to the attribute on the Element.
        // If the image is decorative (i.e. alt=""), it should be ignored even if a title, aria-label, etc. is supplied.
        AccessibilityObjectInclusion altTextInclusion = AccessibilityObjectInclusion::DefaultBehavior;
        WeakPtr image = dynamicDowncast<RenderImage>(*m_renderer);
        if (image)
            altTextInclusion = objectInclusionFromAltText(image->altText());
        else
            altTextInclusion = objectInclusionFromAltText(altTextFromAttributeOrStyle());

        if (altTextInclusion == AccessibilityObjectInclusion::IgnoreObject)
            return true;
        if (altTextInclusion == AccessibilityObjectInclusion::IncludeObject)
            return false;

        // webkit.org/b/173870 - If an image has other alternative text, don't ignore it if alt text is empty.
        // This means we should process title and aria-label first.

        // If an image has an accname, accessibility should be lenient and allow it to appear in the hierarchy (according to WAI-ARIA).
        if (hasAccNameAttribute())
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

    if (role() == AccessibilityRole::HorizontalRule)
        return false;

    // don't ignore labels, because they serve as TitleUIElements
    RefPtr node = m_renderer->node();
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

    // This logic, originally added in:
    // https://github.com/WebKit/WebKit/commit/ddeb923489b58fd890527bf0e432ebe6a477d2ef
    // Results in a lot of useless generics being exposed, which is wasteful. We should remove this.
    WeakPtr blockFlow = dynamicDowncast<RenderBlockFlow>(*m_renderer);
    if (blockFlow && m_renderer->childrenInline() && !canSetFocusAttribute() && !blockFlow->hasBlocksInInlineLayout())
        return !blockFlow->hasLines() && !clickableSelfOrAncestor();

    if (isCanvas()) {
        if (hasElementDescendant()) {
            // Don't ignore canvases with potential fallback content, indicated by
            // having at least one element descendant. If it has no children or its
            // only children are not elements (e.g. just text nodes), it doesn't have
            // fallback content.
            return false;
        }

        if (WeakPtr canvasBox = dynamicDowncast<RenderBox>(*m_renderer)) {
            if (canvasBox->height() <= 1 || canvasBox->width() <= 1)
                return true;
        }
        // Otherwise fall through; use presence of help text, title, or description to decide.
    }

    if (m_renderer->isRenderListMarker()) {
        RefPtr parent = parentObjectUnignored();
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
    if (isGenericFocusableElement() && node->firstChild()) {
        RefPtr shadowRoot = node->containingShadowRoot();
        if (shadowRoot && shadowRoot->mode() == ShadowRootMode::UserAgent && is<HTMLInputElement>(shadowRoot->host())) {
            // We do still want to ignore the user-agent generic div rendered within text inputs.
            return true;
        }
        return false;
    }

    // <span> tags are inline tags and not meant to convey information if they have no other aria
    // information on them. If we don't ignore them, they may emit signals expected to come from
    // their parent. In addition, because included spans are AccessibilityRole::Group objects, and AccessibilityRole::Group
    // objects are often containers with meaningful information, the inclusion of a span can have
    // the side effect of causing the immediate parent accessible to be ignored. This is especially
    // problematic for platforms which have distinct roles for textual block elements.
    auto elementName = WebCore::elementName(node.get());
    if (elementName == ElementName::HTML_span)
        return true;

    // Other non-ignored host language elements
    if (elementName == ElementName::HTML_dfn)
        return false;

    if (isStyleFormatGroup())
        return false;

    switch (downcast<RenderElement>(*m_renderer).style().display()) {
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
    RefPtr controlObject = controlForLabelElement();
    if (controlObject && controlObject->isCheckboxOrRadio() && !controlObject->titleUIElement())
        return true;

    if (isExposedTableRow())
        return isRenderHidden() || ignoredFromPresentationalRole();

    // By default, objects should be ignored so that the AX hierarchy is not
    // filled with unnecessary items.
    return true;
}

int AccessibilityRenderObject::layoutCount() const
{
    auto* view = dynamicDowncast<RenderView>(m_renderer.get());
    return view ? view->frameView().layoutUpdateCount() : 0;
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

    if (isNativeTextControl() && m_renderer) {
        Ref textControl = downcast<RenderTextControl>(*m_renderer).textFormControlElement();
        return textControl->selectedText();
    }

    return doAXStringForRange(documentBasedSelectedTextRange());
}

CharacterRange AccessibilityRenderObject::selectedTextRange() const
{
    ASSERT(isTextControl());

    // Use the text control native range if it's a native object.
    if (isNativeTextControl() && m_renderer) {
        Ref textControl = downcast<RenderTextControl>(*m_renderer).textFormControlElement();
        return { textControl->selectionStart(), textControl->selectionEnd() - textControl->selectionStart() };
    }

    return documentBasedSelectedTextRange();
}

#if ENABLE(AX_THREAD_TEXT_APIS)
AXTextRuns AccessibilityRenderObject::textRuns()
{
    constexpr std::array<uint16_t, 2> lengthOneDomOffsets = { 0, 1 };
    CheckedPtr renderer = this->renderer();
    if (auto* renderLineBreak = dynamicDowncast<RenderLineBreak>(renderer.get())) {
        auto box = InlineIterator::boxFor(*renderLineBreak);

        return AXTextRuns(
            renderLineBreak->containingBlock(),
            { AXTextRun(box ? box->lineIndex() : 0, /* startIndex */ 0, /* endIndex */ 1, { lengthOneDomOffsets }, { 0 }, 0, 0) },
            makeString('\n').isolatedCopy()
        );
    }

    if (is<SVGGraphicsElement>(element())) {
        // Match TextIterator (and other browsers) and don't emit text positions for SVG graphics elements.
        return { };
    }

    if (isReplacedElement()) {
        auto* containingBlock = renderer ? renderer->containingBlock() : nullptr;
        FloatRect rect = localRect();
        uint16_t width = static_cast<uint16_t>(rect.width());
        uint16_t height = static_cast<uint16_t>(rect.height());
        if (!containingBlock)
            return { };

        return AXTextRuns(
            containingBlock,
            { AXTextRun(0, /* startIndex */ 0, /* endIndex */ 1, { lengthOneDomOffsets }, { width }, height, 0) },
            String(span(objectReplacementCharacter))
        );
    }

    WeakPtr renderText = dynamicDowncast<RenderText>(renderer.get());
    if (!renderText)
        return { };

    // FIXME: Need to handle PseudoElementType::FirstLetter. Right now, it will be chopped off from the other
    // other text in the line, and AccessibilityRenderObject::computeIsIgnored ignores the
    // first-letter RenderText, meaning we can't recover it later by combining text across AX objects.

    Vector<AXTextRun> runs;
    // The string of all runs concatenated together.
    StringBuilder fullString;
    // The string representing a single run, which is eventually rolled up into |fullString|.
    StringBuilder lineString;
    Vector<uint16_t> characterWidths;
    float distanceFromBoundsInDirection = 0;
    // Used to round an accumulated floating point value into an uint16, which is how we store character widths.
    float accumulatedDistanceFromStart = 0.0;
    float lineHeight = 0.0;

    bool isHorizontal = fontOrientation() == FontOrientation::Horizontal;
    // Appends text to the current lineString, collapsing whitespace as necessary (similar to how TextIterator::handleTextRun() does).
    auto appendToLineString = [&] (const InlineIterator::TextBoxIterator& textBox) {
        auto text = textBox->originalText();
        if (text.isEmpty())
            return;

        if (textBox->style().textTransform().contains(Style::TextTransformValue::FullSizeKana)) {
            // We don't want to serve transformed kana text to AT since it is a visual affordance.
            // Using the original text from the renderer provides the untransformed string.
            text = textBox->renderer().originalText().substring(textBox->start(), textBox->length());
        }

        bool collapseTabs = textBox->style().collapseWhiteSpace();
        bool collapseNewlines = !textBox->style().preserveNewline();

        auto textRun = textBox->textRun(InlineIterator::TextRunMode::Editing);
        auto lineBox = textBox->lineBox();
        if (!lineBox)
            return;
        lineHeight = LineSelection::logicalRect(*lineBox).height();

        CheckedPtr renderStyle = style();
        if (renderStyle && renderStyle->textAlign() != Style::TextAlign::Left) {
            // To serve the appropriate bounds for text, we need to offset them by a text run's position within its associated RenderText.
            // Computing this requires the following:
            //     1. Get the run's logical offset within the containing block (see note below).
            //     2. Add the containing block's position to get an page-relative position.
            //     3. Subtract the this object's (RenderText) position to get a distance relative to the RenderText.

            // Note: For horizontal text, the contentLogicalLeft property accurately gets us the offset within the containing block.
            // ContentLogicalLeft is wrong for vertical orientations, but xPos (only set in vertical mode) provides that same information accurately.
            float containingBlockOffset = 0;
            if (CheckedPtr containingBlock = renderText->containingBlock())
                containingBlockOffset = isHorizontal ? containingBlock->absoluteBoundingBoxRect().x() : containingBlock->absoluteBoundingBoxRect().y();

            distanceFromBoundsInDirection = isHorizontal ? lineBox->contentLogicalLeft() + containingBlockOffset - elementRect().x() : -textRun.xPos() + containingBlockOffset - elementRect().y();
        }

        // Populate GlyphBuffer with all of the glyphs for the text runs, enabling us to measure character widths.
        const FontCascade& fontCascade = textBox->fontCascade();
        GlyphBuffer glyphBuffer;
        if (fontCascade.codePath(textRun) == FontCascade::CodePath::Complex) {
            ComplexTextController complexTextController(fontCascade, textRun, true, nil);
            complexTextController.advance(text.length(), &glyphBuffer);
        } else {
            WidthIterator simpleIterator(fontCascade, textRun);
            simpleIterator.advance(text.length(), glyphBuffer);
        }

        // Iterate over the glyphs and populate characterWidths. Sometimes multiple characters correspond to just
        // a single glyph so we might need to insert extras zeros into characterWidths for those indexes that
        // don't correspond to a glyph.
        characterWidths.reserveCapacity(text.length());
        unsigned characterIndex = 0;
        for (unsigned glyphIndex = 0; glyphIndex < glyphBuffer.size() && characterIndex < text.length(); glyphIndex++) {
            unsigned newCharacterIndex = glyphBuffer.uncheckedStringOffsetAt(glyphIndex);
            float characterWidth = WebCore::width(glyphBuffer.advanceAt(glyphIndex));
            while (characterIndex + 1 < newCharacterIndex && characterIndex < text.length()) {
                characterWidths.append(0);
                characterIndex++;
            }

            // Round to integer widths, using the fractional part of accumulatedDistanceFromStart to avoid accumulating rounding errors.
            characterWidths.append(static_cast<uint16_t>(characterWidth + fmod(accumulatedDistanceFromStart, 1)));
            accumulatedDistanceFromStart += characterWidth;
            characterIndex = newCharacterIndex;
        }

        if (!collapseTabs && !collapseNewlines) {
            lineString.append(text);
            return;
        }

        lineString.reserveCapacity(lineString.length() + text.length());
        for (unsigned i = 0; i < text.length(); i++) {
            char16_t character = text[i];
            if (character == '\t' && collapseTabs)
                lineString.append(' ');
            else if (character == '\n' && collapseNewlines)
                lineString.append(' ');
            else
                lineString.append(character);
        }
    };

    auto domOffset = [] (unsigned value) -> uint16_t {
        // It shouldn't be possible for any textbox to have more than 65535 characters.
        ASSERT(value <= std::numeric_limits<uint16_t>::max());
        return static_cast<uint16_t>(value);
    };

    Vector<std::array<uint16_t, 2>> textRunDomOffsets;
    auto [textBox, orderCache] = InlineIterator::firstTextBoxInLogicalOrderFor(*renderText);
    size_t currentLineIndex = textBox ? textBox->lineIndex() : 0;

    bool containsOnlyASCII = true;

    auto updateContainsOnlyASCIIFromLineString = [&] {
        if (containsOnlyASCII) {
            StringView view = lineString;
            containsOnlyASCII = view.containsOnlyASCII();
        }
    };

    while (textBox) {
        size_t newLineIndex = textBox->lineIndex();
        uint16_t startDOMOffset = domOffset(textBox->minimumCaretOffset());
        uint16_t endDOMOffset = domOffset(textBox->maximumCaretOffset());
        if (newLineIndex != currentLineIndex) {
            // The start and end (exclusive) character indices of this run.
            unsigned startIndex = fullString.length();
            unsigned endIndex = startIndex + lineString.length();

            updateContainsOnlyASCIIFromLineString();

            if (textRunDomOffsets.size() && startDOMOffset != textRunDomOffsets.last()[1]) {
                // If a space was trimmed in this text run (i.e., there's a gap between the end
                // of the current run's DOM offset and the start of the next), add it back.
                lineString.append(' ');
                ++endIndex;
                characterWidths.append(0);
                // We also need to account for this in the DOM offset itself, as otherwise we'll
                // compute the wrong value when going from rendered-text offset to DOM offset
                // (e.g. via AXTextRuns::domOffset()).
                textRunDomOffsets.last()[1] += 1;
            }
            runs.append({ currentLineIndex, startIndex, endIndex, { std::exchange(textRunDomOffsets, { }) }, std::exchange(characterWidths, { }), lineHeight, distanceFromBoundsInDirection });

            currentLineIndex = newLineIndex;
            // Reset variables used in appendToLineString().
            fullString.append(lineString.toString());
            lineString.clear();
            accumulatedDistanceFromStart = 0.0;
            lineHeight = 0.0;
            distanceFromBoundsInDirection = 0.0;
        }
        appendToLineString(textBox);

        // Within each iteration of this loop, we are looking at the *next* text box to compare to the current.
        // So, we need to set the textRunDomOffsets after the line index comparison, in order to assign the right DOM offsets per text box.
        textBox = InlineIterator::nextTextBoxInLogicalOrder(textBox, orderCache);
        textRunDomOffsets.append({ startDOMOffset, endDOMOffset });
    }

    if (!lineString.isEmpty()) {
        updateContainsOnlyASCIIFromLineString();

        unsigned startIndex = fullString.length();
        unsigned endIndex = startIndex + lineString.length();
        runs.append({ currentLineIndex, startIndex, endIndex, WTFMove(textRunDomOffsets), std::exchange(characterWidths, { }), lineHeight, distanceFromBoundsInDirection });

        fullString.append(lineString.toString());
    }
    return { renderText->containingBlock(), WTFMove(runs), fullString.toString().isolatedCopy(), containsOnlyASCII };
}

AXTextRunLineID AccessibilityRenderObject::listMarkerLineID() const
{
    ASSERT(role() == AccessibilityRole::ListMarker);
    return { renderer() ? renderer()->containingBlock() : nullptr, 0 };
}

String AccessibilityRenderObject::listMarkerText() const
{
    CheckedPtr marker = dynamicDowncast<RenderListMarker>(renderer());
    return marker ? marker->textWithSuffix() : String();
}
#endif // ENABLE(AX_THREAD_TEXT_APIS)

int AccessibilityRenderObject::insertionPointLineNumber() const
{
    ASSERT(isTextControl());

    // Use the text control native range if it's a native object.
    if (isNativeTextControl() && m_renderer) {
        Ref textControl = downcast<RenderTextControl>(*m_renderer).textFormControlElement();
        int start = textControl->selectionStart();
        int end = textControl->selectionEnd();

        // If the selection range is not a collapsed range, we don't know whether the insertion point is the start or the end, thus return -1.
        // FIXME: for non-collapsed selection, determine the insertion point based on the TextFieldSelectionDirection.
        if (start != end)
            return -1;

        return lineForPosition(textControl->visiblePositionForIndex(start));
    }

    RefPtr frame = this->frame();
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

    if (isNativeTextControl() && m_renderer) {
        Ref textControl = downcast<RenderTextControl>(*m_renderer).textFormControlElement();
        FocusOptions focusOptions { .preventScroll = true };
        textControl->focus(focusOptions);
        textControl->setSelectionRange(range.location, range.location + range.length);
    } else if (m_renderer) {
        ASSERT(node());
        Ref node = *this->node();
        auto elementRange = simpleRange();
        auto start = visiblePositionForIndexUsingCharacterIterator(node.get(), range.location);
        if (!contains<ComposedTree>(*elementRange, makeBoundaryPoint(start)))
            start = makeContainerOffsetPosition(elementRange->start);
        auto end = visiblePositionForIndexUsingCharacterIterator(node.get(), range.location + range.length);
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
    if (RefPtr frame = renderer.document().frame()) {
        Ref editor = frame->editor();
        if (element->shouldUseInputMethod()) {
            editor->clearText();
            editor->insertText(string, nullptr);
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

bool AccessibilityRenderObject::press()
{
#if PLATFORM(IOS_FAMILY)
    if (RefPtr mediaElement = this->mediaElement()) {
        // We can safely call the internal togglePlayState method, which doesn't check restrictions,
        // because this method is only called from user interaction.
        return AccessibilityMediaHelpers::press(*mediaElement);
    }
#endif
    return AccessibilityObject::press();
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
    RefPtr renderWidget = dynamicDowncast<RenderWidget>(renderer());
    return renderWidget ? renderWidget->widget() : nullptr;
}

AccessibilityObject* AccessibilityRenderObject::associatedImageObject(HTMLMapElement& map) const
{
    CheckedPtr cache = axObjectCache();
    return cache ? cache->getOrCreate(map.imageElement().get()) : nullptr;
}

AXCoreObject::AccessibilityChildrenVector AccessibilityRenderObject::documentLinks()
{
    if (!m_renderer)
        return { };

    AccessibilityChildrenVector result;
    Ref document = m_renderer->document();
    Ref<HTMLCollection> links = document->links();
    CheckedPtr cache = document->existingAXObjectCache();
    if (!cache)
        return { };

    for (unsigned i = 0; RefPtr current = links->item(i); ++i) {
        if (current->renderer()) {
            RefPtr axObject = cache->getOrCreate(*current);
            ASSERT(axObject);
            if (!axObject->isIgnored() && axObject->isLink())
                result.append(axObject.releaseNonNull());
        } else {
            RefPtr parent = current->parentNode();
            if (RefPtr parentMap = dynamicDowncast<HTMLMapElement>(parent); parentMap && is<HTMLAreaElement>(*current)) {
                RefPtr parentImage = parentMap->imageElement();
                if (RefPtr parentImageAxObject = cache->getOrCreate(parentImage.get())) {
                    for (const auto& child : parentImageAxObject->unignoredChildren()) {
                        if (child->isImageMapLink() && !result.contains(child))
                            result.append(child);
                    }
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
    RefPtr renderWidget = dynamicDowncast<RenderWidget>(*m_renderer);
    return renderWidget ? renderWidget->widget() : nullptr;
}

VisiblePosition AccessibilityRenderObject::visiblePositionForIndex(int index) const
{
    if (m_renderer) {
        if (isNativeTextControl()) {
            Ref textControl = downcast<RenderTextControl>(*m_renderer).textFormControlElement();
            return textControl->visiblePositionForIndex(std::clamp(index, 0, static_cast<int>(textControl->value()->length())));
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
            return downcast<RenderTextControl>(*m_renderer).textFormControlElement().indexForVisiblePosition(position);

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
        if (elementIsTextControl(*ancestor))
            result = ancestor;
        if (ancestor->elementName() == ElementName::HTML_body)
            break;
    }
    return result ? result : rootEditableElement;
}

bool AccessibilityRenderObject::elementIsTextControl(const Element& element) const
{
    auto* cache = axObjectCache();
    RefPtr axObject = cache ? cache->getOrCreate(const_cast<Element&>(element)) : nullptr;
    return axObject && axObject->isTextControl();
}

bool AccessibilityRenderObject::isVisiblePositionRangeInDifferentDocument(const VisiblePositionRange& range) const
{
    if (range.start.isNull() || range.end.isNull())
        return false;

    VisibleSelection newSelection = VisibleSelection(range.start, range.end);
    if (RefPtr newSelectionDocument = newSelection.base().document()) {
        if (RefPtr newSelectionFrame = newSelectionDocument->frame()) {
            RefPtr frame = this->frame();
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
        Ref textControl = uncheckedDowncast<HTMLTextFormControlElement>(*node());
        int start = textControl->indexForVisiblePosition(range.start);
        int end = textControl->indexForVisiblePosition(range.end);

        // For ranges entirely contained in textControl, the start or end position may not be inside textControl.innerTextElement.
        // This would cause that the above indexes will be 0, leading to an incorrect selected range
        // (see HTMLTextFormControlElement::indexForVisiblePosition). This is
        // the case when range is obtained from AXObjectCache::rangeForNodeContents
        // for the HTMLTextFormControlElement.
        // Thus, the following corrects the start and end indexes in such a case..
        if (range.start.deepEquivalent().anchorNode() == range.end.deepEquivalent().anchorNode()
            && range.start.deepEquivalent().anchorNode() == textControl.ptr()) {
            if (auto innerText = textControl->innerTextElement()) {
                auto textControlRange = makeVisiblePositionRange(AXObjectCache::rangeForNodeContents(textControl.get()));
                auto innerRange = makeVisiblePositionRange(AXObjectCache::rangeForNodeContents(*innerText));

                if (range.start.equals(textControlRange.end))
                    start = textControl->value()->length();
                else if (range.start <= innerRange.start)
                    start = 0;

                if (range.end >= innerRange.end
                    || range.end.equals(textControlRange.end))
                    end = textControl->value()->length();
            }
        }

        setTextSelectionIntent(axObjectCache(), start == end ? AXTextStateChangeTypeSelectionMove : AXTextStateChangeTypeSelectionExtend);
        textControl->focus();
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

    int lineStartIndex = indexForVisiblePosition(lineStart);
    int lineEndIndex = indexForVisiblePosition(lineEnd);

    if (isHardLineBreak(lineEnd))
        ++lineEndIndex;

    if (lineStartIndex < 0 || lineEndIndex < 0 || lineEndIndex <= lineStartIndex)
        return { };

    return { static_cast<unsigned>(lineStartIndex), static_cast<unsigned>(lineEndIndex - lineStartIndex) };
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
    RefPtr mapAncestor = ancestorsOfType<HTMLMapElement>(area).first();
    RefPtr associatedImage = mapAncestor ? associatedImageObject(*mapAncestor) : nullptr;
    if (!associatedImage)
        return nullptr;

    for (const auto& child : associatedImage->unignoredChildren()) {
        if (child->elementRect().contains(point))
            return dynamicDowncast<AccessibilityObject>(child.get());
    }

    return nullptr;
}

AccessibilityObject* AccessibilityRenderObject::remoteSVGElementHitTest(const IntPoint& point) const
{
    RefPtr remote = remoteSVGRootElement(CreateIfNecessary::Yes);
    if (!remote)
        return nullptr;

    IntSize offset = point - roundedIntPoint(boundingBoxRect().location());
    return remote->accessibilityHitTest(IntPoint(offset));
}

AccessibilityObject* AccessibilityRenderObject::elementAccessibilityHitTest(const IntPoint& point) const
{
    if (isSVGImage())
        return remoteSVGElementHitTest(point);

    if (role() == AccessibilityRole::ListBox) {
        if (CheckedPtr renderListBox = dynamicDowncast<RenderListBox>(m_renderer.get())) {
            LayoutRect parentRect = boundingBoxRect();

            RefPtr<AccessibilityObject> listBoxOption;
            const auto& children = const_cast<AccessibilityRenderObject*>(this)->unignoredChildren();
            unsigned length = children.size();
            for (unsigned i = 0; i < length; ++i) {
                LayoutRect rect = renderListBox->itemBoundingBoxRect(parentRect.location(), i);
                if (rect.contains(point)) {
                    listBoxOption = downcast<AccessibilityObject>(children[i].get());
                    break;
                }
            }

            if (listBoxOption && !listBoxOption->isIgnored())
                return listBoxOption.unsafeGet();

            CheckedPtr cache = axObjectCache();
            return cache ? cache->getOrCreate(*renderListBox) : nullptr;
        }
    }

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

    // Adjust point for the remoteFrameOffset
    IntPoint adjustedPoint = point;
    adjustedPoint.moveBy(-remoteFrameOffset());

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AccessibilityHitTest };
    HitTestResult hitTestResult { adjustedPoint };

    dynamicDowncast<RenderLayerModelObject>(*m_renderer)->layer()->hitTest(hitType, hitTestResult);
    RefPtr node = hitTestResult.innerNode();
    if (!node)
        return nullptr;

    if (RefPtr area = dynamicDowncast<HTMLAreaElement>(*node))
        return accessibilityImageMapHitTest(*area, point);

    if (RefPtr option = dynamicDowncast<HTMLOptionElement>(*node))
        node = option->ownerSelectElement();

    auto* cache = node ? node->document().axObjectCache() : nullptr;
    RefPtr result = cache ? cache->getOrCreate(*node) : nullptr;
    if (!result)
        return nullptr;

    result->updateChildrenIfNecessary();
    // Allow the element to perform any hit-testing it might need to do to reach non-render children.
    result = result->elementAccessibilityHitTest(point);

    if (result && result->isIgnored()) {
        // If this element is the label of a control, a hit test should return the control.
        RefPtr controlObject = result->controlForLabelElement();
        if (controlObject && !controlObject->titleUIElement())
            return controlObject.unsafeGet();

        result = result->parentObjectUnignored();
    }
    return result.unsafeGet();
}

bool AccessibilityRenderObject::renderObjectIsObservable(RenderObject& renderer) const
{
    // AX clients will listen for AXValueChange on a text control.
    if (is<RenderTextControl>(renderer))
        return true;

    // AX clients will listen for AXSelectedChildrenChanged on listboxes.
    RefPtr node = renderer.node();
    if (!node)
        return false;

    RefPtr element = dynamicDowncast<Element>(*node);
    auto* renderBox = dynamicDowncast<RenderBoxModelObject>(renderer);
    if ((renderBox && renderBox->isRenderListBox()) || (element && hasRole(*element, "listbox"_s)))
        return true;

    // Textboxes should send out notifications.
    return element && (contentEditableAttributeIsEnabled(*element) || hasRole(*element, "textbox"_s));
}

AccessibilityObject* AccessibilityRenderObject::observableObject() const
{
    // This allows the table to be the one who sends notifications about tables.
    if (RefPtr parentTable = parentTableIfExposedTableRow())
        return dynamicDowncast<AccessibilityObject>(parentTable.get());

    // Find the object going up the parent chain that is used in accessibility to monitor certain notifications.
    for (RenderObject* renderer = this->renderer(); renderer && renderer->node(); renderer = renderer->parent()) {
        if (renderObjectIsObservable(*renderer)) {
            if (AXObjectCache* cache = axObjectCache())
                return cache->getOrCreate(*renderer);
        }
    }

    return nullptr;
}

bool AccessibilityRenderObject::shouldIgnoreAttributeRole() const
{
    if (hasTreeItemRole())
        return hasRareData() && !rareData()->isTreeItemValid();

    return m_ariaRole == AccessibilityRole::Document && hasContentEditableAttributeSet();
}

AccessibilityRole AccessibilityRenderObject::determineAccessibilityRole()
{
    // Handle list role determination before checking for a renderer, because we want to use the same codepath in both scenarios.
    if (isAccessibilityList()) {
        if (!m_childrenDirty && childrenInitialized())
            return determineListRoleWithCleanChildren();
        return isDescriptionList() ? AccessibilityRole::DescriptionList : AccessibilityRole::List;
    }

    if (hasTreeItemRole())
        ensureRareData().setIsTreeItemValid(containingTree());

    if (hasTreeRole())
        return isValidTree() ? AccessibilityRole::Tree : AccessibilityRole::Generic;

    if (!m_renderer)
        return AccessibilityNodeObject::determineAccessibilityRole();

    if (m_renderer->isRenderText())
        return AccessibilityRole::StaticText;

#if ENABLE(APPLE_PAY)
    if (isApplePayButton())
        return AccessibilityRole::Button;
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    if (isAttachmentElement())
        return AccessibilityRole::Button;
#endif

    // Sometimes we need to ignore the attribute role. Like if a tree is malformed,
    // we want to ignore the treeitem's attribute role.
    if (m_ariaRole != AccessibilityRole::Unknown && !shouldIgnoreAttributeRole())
        return m_ariaRole;

    if (isExposableTable())
        return AccessibilityRole::Table;

    if (isExposedTableRow())
        return AccessibilityRole::Row;

    RefPtr node = m_renderer->node();
    if (m_renderer->isRenderListItem()) {
        // The details / summary disclosure triangle is implemented using RenderListItem
        // but we want to return `AccessibilityRole::Summary` there, so skip them here.
        RefPtr summary = dynamicDowncast<HTMLSummaryElement>(node);
        if (!summary || !summary->isActiveSummary())
            return AccessibilityRole::ListItem;
    }

    if (m_renderer->isRenderListMarker())
        return AccessibilityRole::ListMarker;
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (m_renderer->isBR())
        return AccessibilityRole::LineBreak;
#endif
    if (RefPtr img = dynamicDowncast<HTMLImageElement>(node); img && img->hasAttributeWithoutSynchronization(usemapAttr))
        return AccessibilityRole::ImageMap;
    if (m_renderer->isImage()) {
        if (is<HTMLInputElement>(node))
            return selfOrAncestorLinkHasPopup() ? AccessibilityRole::PopUpButton : AccessibilityRole::Button;

        if (RefPtr svgRoot = remoteSVGRootElement(CreateIfNecessary::Yes)) {
            if (svgRoot->isRootWithAccessibleContent())
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
    if (m_renderer->isRenderListBox())
        return AccessibilityRole::ListBox;

    if (m_renderer->isRenderOrLegacyRenderSVGRoot())
        return AccessibilityRole::SVGRoot;

    switch (downcast<RenderElement>(*m_renderer).style().display()) {
    case DisplayType::Ruby:
        return AccessibilityRole::RubyInline;
    case DisplayType::RubyAnnotation:
        return AccessibilityRole::RubyText;
    case DisplayType::RubyBlock:
    case DisplayType::RubyBase:
        return AccessibilityRole::Group;
    default:
        break;
    }

#if USE(ATSPI)
    // Non-USE(ATSPI) platforms walk the DOM to build the accessibility tree, and thus never encounter these ignorable
    // anonymous renderers.
    // FIXME: Consider removing this ATSPI-only branch with https://bugs.webkit.org/show_bug.cgi?id=282117.
    if (m_renderer->isAnonymous() && (is<RenderTableCell>(m_renderer.get()) || is<RenderTableRow>(m_renderer.get()) || is<RenderTable>(m_renderer.get())))
        return AccessibilityRole::Ignored;
#endif // USE(ATSPI)

    if (m_renderer->isRenderTableSection())
        return AccessibilityRole::Ignored;

    auto treatStyleFormatGroupAsInline = is<RenderInline>(*m_renderer) ? TreatStyleFormatGroupAsInline::Yes : TreatStyleFormatGroupAsInline::No;
    auto roleFromNode = determineAccessibilityRoleFromNode(treatStyleFormatGroupAsInline);

    // Table cells (by default) return a TextGroup role from determineAccessibilityRoleFromNode.
    // Before returning that role, check if the table cell is within a valid table, so that the a cell role can be returned.
    if (isTableCell()) {
        RefPtr parentTable = this->parentTable();
        if (parentTable && parentTable->isExposableTable())
            return parentTable->hasGridRole() ? AccessibilityRole::GridCell : AccessibilityRole::Cell;
    }

    if (roleFromNode != AccessibilityRole::Unknown)
        return roleFromNode;

#if !PLATFORM(COCOA)
    // This block should be deleted for all platforms, but doing so causes a lot of test failures that need to be sorted out.
    if (CheckedPtr blockRenderer = dynamicDowncast<RenderBlockFlow>(m_renderer.get()))
        return blockRenderer->isAnonymousBlock() ? AccessibilityRole::TextGroup : AccessibilityRole::Generic;
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

std::optional<AXCoreObject::AccessibilityChildrenVector> AccessibilityRenderObject::imageOverlayElements()
{
    AXTRACE("AccessibilityRenderObject::imageOverlayElements"_s);

    if (!m_renderer || !toSimpleImage(*m_renderer))
        return std::nullopt;

    const auto& children = this->unignoredChildren();
    if (children.size())
        return children;

#if ENABLE(IMAGE_ANALYSIS)
    RefPtr page = this->page();
    if (!page)
        return std::nullopt;

    RefPtr element = this->element();
    if (!element)
        return std::nullopt;

    page->chrome().client().requestTextRecognition(*element, { }, [] (RefPtr<Element>&& imageOverlayHost) {
        if (!imageOverlayHost)
            return;

        if (CheckedPtr cache = imageOverlayHost->document().existingAXObjectCache())
            cache->postNotification(imageOverlayHost.get(), AXNotification::ImageOverlayChanged);
    });
#endif

    return std::nullopt;
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
    switch (role()) {
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

    for (RefPtr parent = parentObject(); parent; parent = parent->parentObject()) {
        RefPtr accessibilityRenderer = dynamicDowncast<AccessibilityRenderObject>(*parent);
        if (!accessibilityRenderer)
            continue;

        RefPtr node = dynamicDowncast<Element>(accessibilityRenderer->node());
        if (!node)
            continue;

        // If native tag of the parent element matches an acceptable name, then return
        // based on its presentational status.
        auto& name = node->tagQName();
        if (std::ranges::any_of(parentTags, [&name](auto* possibleName) { return possibleName->get() == name; }))
            return parent->role() == AccessibilityRole::Presentational;
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

    WeakPtr cache = axObjectCache();
    if (!cache)
        return;
    for (Ref area : descendantsOfType<HTMLAreaElement>(*map)) {
        // add an <area> element for this child if it has a link
        if (!area->isLink())
            continue;
        addChild(cache->getOrCreate(area.get()));
    }
}

void AccessibilityRenderObject::addTextFieldChildren()
{
    RefPtr node = dynamicDowncast<HTMLInputElement>(this->node());
    if (!node)
        return;

    RefPtr spinButtonElement = dynamicDowncast<SpinButtonElement>(node->innerSpinButtonElement());
    if (!spinButtonElement)
        return;

    Ref axSpinButton = uncheckedDowncast<AccessibilitySpinButton>(*axObjectCache()->create(AccessibilityRole::SpinButton));
    axSpinButton->setSpinButtonElement(spinButtonElement.get());
    axSpinButton->setParent(this);
    addChild(WTFMove(axSpinButton));
}

bool AccessibilityRenderObject::isSVGImage() const
{
    return remoteSVGRootElement(CreateIfNecessary::Yes);
}

void AccessibilityRenderObject::detachRemoteSVGRoot()
{
    if (RefPtr root = remoteSVGRootElement(CreateIfNecessary::No))
        root->setParent(nullptr);
}

AccessibilitySVGObject* AccessibilityRenderObject::remoteSVGRootElement(CreateIfNecessary createIfNecessary) const
{
    auto* renderImage = dynamicDowncast<RenderImage>(renderer());
    if (!renderImage)
        return nullptr;

    auto* cachedImage = renderImage->cachedImage();
    if (!cachedImage)
        return nullptr;

    RefPtr svgImage = dynamicDowncast<SVGImage>(cachedImage->image());
    if (!svgImage)
        return nullptr;

    RefPtr frameView = svgImage->frameView();
    if (!frameView)
        return nullptr;

    RefPtr document = frameView->frame().document();
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

    RefPtr rootSVGObject = createIfNecessary == CreateIfNecessary::Yes ? cache->getOrCreate(*rendererRoot) : cache->get(rendererRoot);
    ASSERT(createIfNecessary == CreateIfNecessary::No || rootSVGObject);
    return dynamicDowncast<AccessibilitySVGObject>(rootSVGObject).unsafeGet();
}

void AccessibilityRenderObject::addRemoteSVGChildren()
{
    RefPtr<AccessibilitySVGObject> root = remoteSVGRootElement(CreateIfNecessary::Yes);
    if (!root)
        return;

    // FIXME: It's possible for an SVG that is rendered twice to share renderers. We don't want to add this as a child of both parents
    // in this case, as it will create an invalid parent-child relationship in the accessibility tree.
    // If it's parent is a WebArea, that is just the default value given when the object was created, so still add the child in that case.
    RefPtr parent = root->parentObject();
    if (parent && parent->role() != AccessibilityRole::WebArea)
        return;

    // In order to connect the AX hierarchy from the SVG root element from the loaded resource
    // the parent must be set, because there's no other way to get back to who created the image.
    root->setParent(this);
    addChild(*root);
}

void AccessibilityRenderObject::addAttachmentChildren()
{
    if (!isAttachment())
        return;

    // LocalFrameView's need to be inserted into the AX hierarchy when encountered.
    RefPtr widget = widgetForAttachmentView();
    if (!widget || !(widget->isLocalFrameView() || widget->isRemoteFrameView()))
        return;

    if (CheckedPtr cache = axObjectCache())
        addChild(cache->getOrCreate(*widget));
}

#if USE(ATSPI)
// FIXME: Consider removing these ATSPI-only functions with https://bugs.webkit.org/show_bug.cgi?id=282117.
void AccessibilityRenderObject::addCanvasChildren()
{
    // Add the unrendered canvas children as AX nodes, unless we're not using a canvas renderer
    // because JS is disabled for example.
    if (elementName() != ElementName::HTML_canvas || (renderer() && !renderer()->isRenderHTMLCanvas()))
        return;

    // If it's a canvas, it won't have rendered children, but it might have accessible fallback content.
    // Clear m_childrenInitialized because AccessibilityNodeObject::addChildren will expect it to be false.
    ASSERT(!m_children.size());
    m_childrenInitialized = false;
    AccessibilityNodeObject::addChildren();
}

// Some elements don't have an associated render object, meaning they won't be picked up by a walk of the render tree.
// For example, elements with `display: contents`, or aria-hidden=true elements that are focused.
// This function will find and add these elements to the AX tree.
void AccessibilityRenderObject::addNodeOnlyChildren()
{
    RefPtr node = this->node();
    if (!node)
        return;

    auto nodeHasDisplayContents = [] (Node& node) {
        RefPtr element = dynamicDowncast<Element>(node);
        return element && element->hasDisplayContents();
    };
    // First do a quick run through to determine if we have any interesting nodes (most often we will not).
    // If we do have any interesting nodes, we need to determine where to insert them so they match DOM order as close as possible.
    bool hasNodeOnlyChildren = false;
    for (RefPtr child = node->firstChild(); child; child = child->nextSibling()) {
        if (child->renderer())
            continue;

        if (nodeHasDisplayContents(*child) || isNodeFocused(*child)) {
            hasNodeOnlyChildren = true;
            break;
        }
    }

    if (!hasNodeOnlyChildren)
        return;

    WeakPtr cache = axObjectCache();
    if (!cache)
        return;
    // FIXME: This algorithm does not work correctly when ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE) due to use of m_children, as this algorithm is written assuming m_children only every contains unignored objects.
    // Iterate through all of the children, including those that may have already been added, and
    // try to insert the nodes in the correct place in the DOM order.
    unsigned insertionIndex = 0;
    for (RefPtr child = node->firstChild(); child; child = child->nextSibling()) {
        if (child->renderer()) {
            // Find out where the last render sibling is located within m_children.
            RefPtr<AXCoreObject> childObject = cache->get(child->renderer());
            if (childObject && childObject->isIgnored()) {
                const auto& children = childObject->unignoredChildren();
                if (children.size())
                    childObject = children.last().ptr();
                else
                    childObject = nullptr;
            }

            if (childObject) {
                insertionIndex = m_children.findIf([&childObject] (const Ref<AXCoreObject>& child) {
                    return child.ptr() == childObject;
                });
                ++insertionIndex;
            }
            continue;
        }

        if (!nodeHasDisplayContents(*child))
            continue;

        unsigned previousSize = m_children.size();
        if (insertionIndex > previousSize)
            insertionIndex = previousSize;

        insertChild(cache->getOrCreate(*child), insertionIndex);
        insertionIndex += (m_children.size() - previousSize);
    }
}
#endif // USE(ATSPI)

#if PLATFORM(MAC)
void AccessibilityRenderObject::updateAttachmentViewParents()
{
    // updateChildrenIfNeeded == false because this is called right after we've added children, so we know
    // they're clean and don't need updating.
    for (const auto& child : children(/* updateChildrenIfNeeded */ false))
        downcast<AccessibilityObject>(child)->overrideAttachmentParent(this);
}
#endif // PLATFORM(MAC)

RenderObject* AccessibilityRenderObject::markerRenderer() const
{
    CheckedPtr renderListItem = dynamicDowncast<RenderListItem>(m_renderer.get());
    if (!renderListItem || !isListItem() || isIgnored())
        return nullptr;

    return renderListItem->markerRenderer();
}

void AccessibilityRenderObject::updateRoleAfterChildrenCreation()
{
    // If a menu does not have valid menuitem children, it should not be exposed as a menu.
    auto role = this->role();
    if (role == AccessibilityRole::Menu) {
        // Elements marked as menus must have at least one menu item child.
        bool hasMenuItemDescendant = false;
        for (const auto& child : unignoredChildren()) {
            if (child->isMenuItem()) {
                hasMenuItemDescendant = true;
                break;
            }

            // Per the ARIA spec, groups with menuitem children are allowed as children of menus.
            // https://w3c.github.io/aria/#menu
            if (child->isGroup()) {
                for (const auto& grandchild : child->unignoredChildren()) {
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
    if (role == AccessibilityRole::SVGRoot && unignoredChildren().isEmpty())
        m_role = AccessibilityRole::Image;

    if (isAccessibilityList()) {
        updateRole();
        return;
    }

    if (role != m_role) {
        if (auto* cache = axObjectCache())
            cache->handleRoleChanged(*this, role);
    }
}

void AccessibilityRenderObject::addChildren()
{
    if (!renderer()) [[unlikely]] {
        AccessibilityNodeObject::addChildren();
        return;
    }
    // If the need to add more children in addition to existing children arises,
    // childrenChanged should have been called, leaving the object with no children.
    AX_DEBUG_ASSERT(!m_childrenInitialized);
    m_childrenInitialized = true;

    auto scopeExit = makeScopeExit([this, protectedThis = Ref { *this }] {
        m_subtreeDirty = false;
        if (isNativeLabel())
            m_containsOnlyStaticTextDirty = true;
#ifndef NDEBUG
        verifyChildrenIndexInParent();
#endif
    });

#if !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    if (isExposableTable()) {
        // See comment in AccessibilityNodeObject::addChildren() explaing this #if ... branch.
        addTableChildrenAndCellSlots();
        return;
    }
#endif

    auto addChildIfNeeded = [this](AccessibilityObject& object) {
#if USE(ATSPI)
        // FIXME: Consider removing this ATSPI-only branch with https://bugs.webkit.org/show_bug.cgi?id=282117.
        if (object.renderer() && object.renderer()->isRenderListMarker())
            return;
#endif
        addChild(object);
    };

    WeakPtr cache = axObjectCache();
    auto addListItemMarker = [&] () {
        if (CheckedPtr marker = markerRenderer(); marker && cache)
            addChild(cache->getOrCreate(*marker));
    };

    auto addListBoxChildrenIfNecessary = [&](Node& node) -> bool {
        if (role() == AccessibilityRole::ListBox) {
            if (RefPtr selectElement = dynamicDowncast<HTMLSelectElement>(node)) {
                for (const auto& listItem : selectElement->listItems())
                    addChild(cache->getOrCreate(listItem.get()), AccessibilityObject::DescendIfIgnored::No);
                return true;
            }
        }

        return false;
    };

    RefPtr node = dynamicDowncast<ContainerNode>(this->node());

#if !USE(ATSPI)
    // Non-ATSPI platforms walk the DOM to build the accessibility tree.
    // Ideally this would be the case for all platforms, but there are GLib tests that rely on anonymous renderers
    // being part of the accessibility tree.
    RefPtr element = dynamicDowncast<Element>(node);

    // ::before and ::after pseudos should be the first and last children of the element
    // that generates them (rather than being siblings to the generating element).
    if (RefPtr beforePseudo = element ? element->beforePseudoElement() : nullptr) {
        if (RefPtr pseudoObject = cache ? cache->getOrCreate(*beforePseudo) : nullptr)
            addChildIfNeeded(*pseudoObject);
    }

    // If |this| has an associated list marker, it should be the first child (or second if |this| has a ::before pseudo).
    addListItemMarker();

    if (node && !(element && element->isPseudoElement()) && cache) {
        if (addListBoxChildrenIfNecessary(*node))
            return;

        // If we have a DOM node, use the DOM to find accessible children.
        //
        // The ComposedTreeIterator is extremely large by default, and will cause a stack
        // overflow when building the accessibility tree from a deep DOM. Specify that we
        // do not want its internal vector to allocate any space on the stack, in turn
        // ensuring its contents go on the heap. We should consider rewriting our algorithm
        // to build the accessibility tree to be iterative rather than recursive.
        for (Ref child : composedTreeChildren</* InlineContextCapacity */ 0>(*node)) {
            if (RefPtr childObject = cache->getOrCreate(child.get()))
                addChildIfNeeded(*childObject);
        }
    } else {
        if (auto* blockRenderer = dynamicDowncast<RenderBlock>(m_renderer.get()); blockRenderer && blockRenderer->isAnonymousBlock())
            return;
        // If we are a valid anonymous renderer (pseudo-element, list marker), use
        // AXChildIterator to walk the render tree / DOM (we may walk between the
        // two — reference AccessibilityObject::iterator documentation for more information).
        for (Ref object : AXChildIterator(*this))
            addChildIfNeeded(WTFMove(object));
    }

    if (RefPtr afterPseudo = element ? element->afterPseudoElement() : nullptr) {
        if (RefPtr pseudoObject = cache ? cache->getOrCreate(*afterPseudo) : nullptr)
            addChildIfNeeded(*pseudoObject);
    }
#else
    // USE(ATPSI) within this block. Walk the render tree (primarily -- see comments for AccessibilityObject::iterator)
    addListItemMarker();
    // to build the accessibility tree.
    // FIXME: Consider removing this ATSPI-only branch with https://bugs.webkit.org/show_bug.cgi?id=282117.
    if (node && addListBoxChildrenIfNecessary(*node))
        return;

    for (auto& object : AXChildIterator(*this))
        addChildIfNeeded(object);

    addNodeOnlyChildren();
    addCanvasChildren();
#endif // !USE(ATSPI)

    addAttachmentChildren();
    addImageMapChildren();
    addTextFieldChildren();
    addRemoteSVGChildren();
#if PLATFORM(MAC)
    updateAttachmentViewParents();
#endif
    updateOwnedChildrenIfNecessary();

    // Handle aria-colindex for all table rows (whether using owned objects or not).
    if (isExposedTableRow()) {
        if (std::optional colIndex = axColumnIndex()) {
            unsigned index = 0;
            for (const auto& child : unignoredChildren()) {
                if (RefPtr rowChild = dynamicDowncast<AccessibilityNodeObject>(child); rowChild && rowChild->isTableCell())
                    rowChild->setAXColIndexFromRow(*colIndex + index);
                index++;
            }
        }
    }

    updateRoleAfterChildrenCreation();

#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    if (isExposableTable())
        addTableChildrenAndCellSlots();
#endif
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

    return m_renderer->style().fontStyle().isConsideredItalic();
}

bool AccessibilityRenderObject::hasPlainText() const
{
    if (!m_renderer)
        return false;

    if (!canHavePlainText())
        return false;

    const RenderStyle& style = m_renderer->style();
    return style.fontWeight().isNormal()
        && !style.fontStyle().isConsideredItalic()
        && style.textDecorationLineInEffect().isNone();
}

bool AccessibilityRenderObject::hasSameFont(AXCoreObject& object)
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
    if (CheckedPtr renderElement = dynamicDowncast<RenderElement>(renderer()))
        return renderElement->style().applePayButtonType();
    return ApplePayButtonType::Plain;
}
#endif

bool AccessibilityRenderObject::hasSameFontColor(AXCoreObject& object)
{
    auto* renderer = object.renderer();
    if (!m_renderer || !renderer)
        return false;

    return m_renderer->style().visitedDependentColor(CSSPropertyColor) == renderer->style().visitedDependentColor(CSSPropertyColor);
}

bool AccessibilityRenderObject::hasSameStyle(AXCoreObject& object)
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

    return m_renderer->style().textDecorationLineInEffect().hasUnderline();
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
    if (RefPtr parent = parentObject()) {
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

FloatRect AccessibilityRenderObject::localRect() const
{
    CheckedPtr renderer = this->renderer();
    if (CheckedPtr box = dynamicDowncast<RenderBox>(renderer.get()))
        return box ? convertFrameToSpace(box->frameRect(), AccessibilityConversionSpace::Page) : FloatRect();

    CheckedPtr renderText = dynamicDowncast<RenderText>(renderer.get());
    return renderText ? renderText->linesBoundingBox() : FloatRect();
}

#if ENABLE(MATHML)
bool AccessibilityRenderObject::isIgnoredElementWithinMathTree() const
{
    // We ignore anonymous boxes inserted into RenderMathMLBlocks to honor CSS rules.
    // See https://www.w3.org/TR/css3-box/#block-level0
    return m_renderer && m_renderer->isAnonymous() && m_renderer->parent() && is<RenderMathMLBlock>(m_renderer->parent());
}
#endif

#if PLATFORM(IOS_FAMILY)
String AccessibilityRenderObject::interactiveVideoDuration() const
{
    return AccessibilityMediaHelpers::interactiveVideoDuration(mediaElement());
}

void AccessibilityRenderObject::toggleMute()
{
    AccessibilityMediaHelpers::toggleMute(mediaElement());
}

bool AccessibilityRenderObject::isPlaying() const
{
    return AccessibilityMediaHelpers::isPlaying(mediaElement());
}

bool AccessibilityRenderObject::isMuted() const
{
    return AccessibilityMediaHelpers::isMuted(mediaElement());
}

bool AccessibilityRenderObject::isMediaObject() const
{
    return is<HTMLMediaElement>(node());
}

bool AccessibilityRenderObject::isAutoplayEnabled() const
{
    return AccessibilityMediaHelpers::isAutoplayEnabled(mediaElement());
}

void AccessibilityRenderObject::enterFullscreen() const
{
    AccessibilityMediaHelpers::enterFullscreen(videoElement());
}
#endif // PLATFORM(IOS_FAMILY)

} // namespace WebCore
