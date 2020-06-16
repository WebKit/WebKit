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
#include "AccessibilityLabel.h"
#include "AccessibilityListBox.h"
#include "AccessibilitySVGRoot.h"
#include "AccessibilitySpinButton.h"
#include "AccessibilityTable.h"
#include "CachedImage.h"
#include "Editing.h"
#include "Editor.h"
#include "ElementIterator.h"
#include "EventHandler.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "HTMLAreaElement.h"
#include "HTMLAudioElement.h"
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
#include "HTMLParserIdioms.h"
#include "HTMLSelectElement.h"
#include "HTMLSummaryElement.h"
#include "HTMLTableElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLVideoElement.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Image.h"
#include "LocalizedStrings.h"
#include "NodeList.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "RenderButton.h"
#include "RenderFileUploadControl.h"
#include "RenderHTMLCanvas.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLineBreak.h"
#include "RenderListBox.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderMathMLBlock.h"
#include "RenderMenuList.h"
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
#include "SVGImage.h"
#include "SVGSVGElement.h"
#include "Text.h"
#include "TextControlInnerElements.h"
#include "TextIterator.h"
#include "VisibleUnits.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

using namespace HTMLNames;

AccessibilityRenderObject::AccessibilityRenderObject(RenderObject* renderer)
    : AccessibilityNodeObject(renderer->node())
    , m_renderer(makeWeakPtr(renderer))
{
#ifndef NDEBUG
    m_renderer->setHasAXObject(true);
#endif
}

AccessibilityRenderObject::~AccessibilityRenderObject()
{
    ASSERT(isDetached());
}

void AccessibilityRenderObject::init()
{
    AccessibilityNodeObject::init();
}

Ref<AccessibilityRenderObject> AccessibilityRenderObject::create(RenderObject* renderer)
{
    return adoptRef(*new AccessibilityRenderObject(renderer));
}

void AccessibilityRenderObject::detachRemoteParts(AccessibilityDetachmentType detachmentType)
{
    AccessibilityNodeObject::detachRemoteParts(detachmentType);
    
    detachRemoteSVGRoot();
    
#ifndef NDEBUG
    if (m_renderer)
        m_renderer->setHasAXObject(false);
#endif
    m_renderer = nullptr;
}

RenderBoxModelObject* AccessibilityRenderObject::renderBoxModelObject() const
{
    if (!is<RenderBoxModelObject>(renderer()))
        return nullptr;
    return downcast<RenderBoxModelObject>(renderer());
}

void AccessibilityRenderObject::setRenderer(RenderObject* renderer)
{
    m_renderer = makeWeakPtr(renderer);
    setNode(renderer->node());
}

static inline bool isInlineWithContinuation(RenderObject& object)
{
    return is<RenderInline>(object) && downcast<RenderInline>(object).continuation();
}

static inline RenderObject* firstChildInContinuation(RenderInline& renderer)
{
    auto* continuation = renderer.continuation();

    while (continuation) {
        if (is<RenderBlock>(*continuation))
            return continuation;
        if (RenderObject* child = continuation->firstChild())
            return child;
        continuation = downcast<RenderInline>(*continuation).continuation();
    }

    return nullptr;
}

static inline RenderObject* firstChildConsideringContinuation(RenderObject& renderer)
{
    RenderObject* firstChild = renderer.firstChildSlow();

    // We don't want to include the end of a continuation as the firstChild of the
    // anonymous parent, because everything has already been linked up via continuation.
    // CSS first-letter selector is an example of this case.
    if (renderer.isAnonymous() && is<RenderInline>(firstChild) && downcast<RenderInline>(*firstChild).isContinuation())
        firstChild = nullptr;
    
    if (!firstChild && isInlineWithContinuation(renderer))
        firstChild = firstChildInContinuation(downcast<RenderInline>(renderer));

    return firstChild;
}


static inline RenderObject* lastChildConsideringContinuation(RenderObject& renderer)
{
    if (!is<RenderInline>(renderer) && !is<RenderBlock>(renderer))
        return &renderer;

    RenderObject* lastChild = downcast<RenderBoxModelObject>(renderer).lastChild();
    for (auto* current = &downcast<RenderBoxModelObject>(renderer); current; ) {
        if (RenderObject* newLastChild = current->lastChild())
            lastChild = newLastChild;

        current = current->inlineContinuation();
    }

    return lastChild;
}

AccessibilityObject* AccessibilityRenderObject::firstChild() const
{
    if (!m_renderer)
        return nullptr;
    
    RenderObject* firstChild = firstChildConsideringContinuation(*m_renderer);

    // If an object can't have children, then it is using this method to help
    // calculate some internal property (like its description).
    // In this case, it should check the Node level for children in case they're
    // not rendered (like a <meter> element).
    if (!firstChild && !canHaveChildren())
        return AccessibilityNodeObject::firstChild();

    return axObjectCache()->getOrCreate(firstChild);
}

AccessibilityObject* AccessibilityRenderObject::lastChild() const
{
    if (!m_renderer)
        return nullptr;

    RenderObject* lastChild = lastChildConsideringContinuation(*m_renderer);

    if (!lastChild && !canHaveChildren())
        return AccessibilityNodeObject::lastChild();

    return axObjectCache()->getOrCreate(lastChild);
}

static inline RenderInline* startOfContinuations(RenderObject& renderer)
{
    if (!is<RenderElement>(renderer))
        return nullptr;
    auto& renderElement = downcast<RenderElement>(renderer);
    if (is<RenderInline>(renderElement) && renderElement.isContinuation() && is<RenderInline>(renderElement.element()->renderer()))
        return downcast<RenderInline>(renderer.node()->renderer());

    // Blocks with a previous continuation always have a next continuation
    if (is<RenderBlock>(renderElement) && downcast<RenderBlock>(renderElement).inlineContinuation())
        return downcast<RenderInline>(downcast<RenderBlock>(renderElement).inlineContinuation()->element()->renderer());

    return nullptr;
}

static inline RenderObject* endOfContinuations(RenderObject& renderer)
{
    if (!is<RenderInline>(renderer) && !is<RenderBlock>(renderer))
        return &renderer;

    auto* previous = &downcast<RenderBoxModelObject>(renderer);
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
    RenderObject* child = renderer.firstChild();
    return is<RenderInline>(child) && downcast<RenderInline>(*child).isContinuation();
}

AccessibilityObject* AccessibilityRenderObject::previousSibling() const
{
    if (!m_renderer)
        return nullptr;

    RenderObject* previousSibling = nullptr;

    // Case 1: The node is a block and is an inline's continuation. In that case, the inline's
    // last child is our previous sibling (or further back in the continuation chain)
    RenderInline* startOfConts;
    if (is<RenderBox>(*m_renderer) && (startOfConts = startOfContinuations(*m_renderer)))
        previousSibling = childBeforeConsideringContinuations(startOfConts, renderer());

    // Case 2: Anonymous block parent of the end of a continuation - skip all the way to before
    // the parent of the start, since everything in between will be linked up via the continuation.
    else if (m_renderer->isAnonymousBlock() && firstChildIsInlineContinuation(downcast<RenderBlock>(*m_renderer))) {
        RenderBlock& renderBlock = downcast<RenderBlock>(*m_renderer);
        auto* firstParent = startOfContinuations(*renderBlock.firstChild())->parent();
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
    else if (is<RenderInline>(*m_renderer->parent()) && (startOfConts = startOfContinuations(*m_renderer->parent())))
        previousSibling = childBeforeConsideringContinuations(startOfConts, m_renderer->parent()->firstChild());

    if (!previousSibling)
        return nullptr;
    
    return axObjectCache()->getOrCreate(previousSibling);
}

static inline bool lastChildHasContinuation(RenderElement& renderer)
{
    RenderObject* child = renderer.lastChild();
    return child && isInlineWithContinuation(*child);
}

AccessibilityObject* AccessibilityRenderObject::nextSibling() const
{
    if (!m_renderer || is<RenderView>(*m_renderer))
        return nullptr;

    RenderObject* nextSibling = nullptr;

    // Case 1: node is a block and has an inline continuation. Next sibling is the inline continuation's
    // first child.
    RenderInline* inlineContinuation;
    if (is<RenderBlock>(*m_renderer) && (inlineContinuation = downcast<RenderBlock>(*m_renderer).inlineContinuation()))
        nextSibling = firstChildConsideringContinuation(*inlineContinuation);

    // Case 2: Anonymous block parent of the start of a continuation - skip all the way to
    // after the parent of the end, since everything in between will be linked up via the continuation.
    else if (m_renderer->isAnonymousBlock() && lastChildHasContinuation(downcast<RenderBlock>(*m_renderer))) {
        RenderElement* lastParent = endOfContinuations(*downcast<RenderBlock>(*m_renderer).lastChild())->parent();
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
        
        // After case 4, there are chances that nextSibling has the same node as the current renderer,
        // which might lead to adding the same child repeatedly.
        if (nextSibling && nextSibling->node() == m_renderer->node()) {
            if (AccessibilityObject* nextObj = axObjectCache()->getOrCreate(nextSibling))
                return nextObj->nextSibling();
        }
    }

    if (!nextSibling)
        return nullptr;
    
    // Make sure next sibling has the same parent.
    AccessibilityObject* nextObj = axObjectCache()->getOrCreate(nextSibling);
    if (nextObj && nextObj->parentObject() != this->parentObject())
        return nullptr;
    
    return nextObj;
}

static RenderBoxModelObject* nextContinuation(RenderObject& renderer)
{
    if (is<RenderInline>(renderer) && !renderer.isReplaced())
        return downcast<RenderInline>(renderer).continuation();
    if (is<RenderBlock>(renderer))
        return downcast<RenderBlock>(renderer).inlineContinuation();
    return nullptr;
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
    if (isWebArea())
        return cache->get(&m_renderer->view().frameView());

    return cache->get(renderParentObject());
}
    
AccessibilityObject* AccessibilityRenderObject::parentObject() const
{
    if (!m_renderer)
        return nullptr;
    
    if (ariaRoleAttribute() == AccessibilityRole::MenuBar)
        return axObjectCache()->getOrCreate(m_renderer->parent());

    // menuButton and its corresponding menu are DOM siblings, but Accessibility needs them to be parent/child
    if (ariaRoleAttribute() == AccessibilityRole::Menu) {
        AccessibilityObject* parent = menuButtonForMenu();
        if (parent)
            return parent;
    }
    
    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return nullptr;
    
    RenderObject* parentObj = renderParentObject();
    if (parentObj)
        return cache->getOrCreate(parentObj);
    
    // WebArea's parent should be the scroll view containing it.
    if (isWebArea())
        return cache->getOrCreate(&m_renderer->view().frameView());
    
    return nullptr;
}
    
bool AccessibilityRenderObject::isAttachment() const
{
    RenderBoxModelObject* renderer = renderBoxModelObject();
    if (!renderer)
        return false;
    // Widgets are the replaced elements that we represent to AX as attachments
    bool isWidget = renderer->isWidget();

    return isWidget && ariaRoleAttribute() == AccessibilityRole::Unknown;
}

bool AccessibilityRenderObject::isFileUploadButton() const
{
    if (m_renderer && is<HTMLInputElement>(m_renderer->node())) {
        HTMLInputElement& input = downcast<HTMLInputElement>(*m_renderer->node());
        return input.isFileUpload();
    }
    
    return false;
}

bool AccessibilityRenderObject::isOffScreen() const
{
    if (!m_renderer)
        return true;

    IntRect contentRect = snappedIntRect(m_renderer->absoluteClippedOverflowRect());
    // FIXME: unclear if we need LegacyIOSDocumentVisibleRect.
    IntRect viewRect = m_renderer->view().frameView().visibleContentRect(ScrollableArea::LegacyIOSDocumentVisibleRect);
    viewRect.intersect(contentRect);
    return viewRect.isEmpty();
}

Element* AccessibilityRenderObject::anchorElement() const
{
    if (!m_renderer)
        return nullptr;
    
    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return nullptr;
    
    RenderObject* currentRenderer;
    
    // Search up the render tree for a RenderObject with a DOM node.  Defer to an earlier continuation, though.
    for (currentRenderer = renderer(); currentRenderer && !currentRenderer->node(); currentRenderer = currentRenderer->parent()) {
        if (currentRenderer->isAnonymousBlock()) {
            if (RenderObject* continuation = downcast<RenderBlock>(*currentRenderer).continuation())
                return cache->getOrCreate(continuation)->anchorElement();
        }
    }
    
    // bail if none found
    if (!currentRenderer)
        return nullptr;
    
    // search up the DOM tree for an anchor element
    // NOTE: this assumes that any non-image with an anchor is an HTMLAnchorElement
    for (Node* node = currentRenderer->node(); node; node = node->parentNode()) {
        if (is<HTMLAnchorElement>(*node) || (node->renderer() && cache->getOrCreate(node->renderer())->isLink()))
            return downcast<Element>(node);
    }
    
    return nullptr;
}

String AccessibilityRenderObject::helpText() const
{
    if (!m_renderer)
        return String();
    
    const AtomString& ariaHelp = getAttribute(aria_helpAttr);
    if (!ariaHelp.isEmpty())
        return ariaHelp;
    
    String describedBy = ariaDescribedByAttribute();
    if (!describedBy.isEmpty())
        return describedBy;
    
    String description = accessibilityDescription();
    for (RenderObject* ancestor = renderer(); ancestor; ancestor = ancestor->parent()) {
        if (is<HTMLElement>(ancestor->node())) {
            HTMLElement& element = downcast<HTMLElement>(*ancestor->node());
            const AtomString& summary = element.getAttribute(summaryAttr);
            if (!summary.isEmpty())
                return summary;
            
            // The title attribute should be used as help text unless it is already being used as descriptive text.
            const AtomString& title = element.getAttribute(titleAttr);
            if (!title.isEmpty() && description != title)
                return title;
        }
        
        // Only take help text from an ancestor element if its a group or an unknown role. If help was 
        // added to those kinds of elements, it is likely it was meant for a child element.
        if (AccessibilityObject* axObj = axObjectCache()->getOrCreate(ancestor)) {
            if (!axObj->isGroup() && axObj->roleValue() != AccessibilityRole::Unknown)
                break;
        }
    }
    
    return String();
}

String AccessibilityRenderObject::textUnderElement(AccessibilityTextUnderElementMode mode) const
{
    if (!m_renderer)
        return String();

    if (is<RenderFileUploadControl>(*m_renderer))
        return downcast<RenderFileUploadControl>(*m_renderer).buttonValue();
    
    // Reflect when a content author has explicitly marked a line break.
    if (m_renderer->isBR())
        return "\n"_s;

    if (shouldGetTextFromNode(mode))
        return AccessibilityNodeObject::textUnderElement(mode);

    // We use a text iterator for text objects AND for those cases where we are
    // explicitly asking for the full text under a given element.
    if (is<RenderText>(*m_renderer) || mode.childrenInclusion == AccessibilityTextUnderElementMode::TextUnderElementModeIncludeAllChildren) {
        // If possible, use a text iterator to get the text, so that whitespace
        // is handled consistently.
        Document* nodeDocument = nullptr;
        Optional<SimpleRange> textRange;
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
                Node* firstNodeInBlock = firstChildRenderer->node();
                Position startPosition = positionInParentBeforeNode(firstNodeInBlock);
                Position endPosition = positionInParentAfterNode(lastChildRenderer->node());

                nodeDocument = &firstNodeInBlock->document();
                textRange = { { *makeBoundaryPoint(startPosition), *makeBoundaryPoint(endPosition) } };
            }
        }

        if (nodeDocument && textRange) {
            if (Frame* frame = nodeDocument->frame()) {
                // catch stale WebCoreAXObject (see <rdar://problem/3960196>)
                if (frame->document() != nodeDocument)
                    return String();

                // Renders referenced by accessibility objects could get destroyed, if TextIterator ends up triggering
                // style update/layout here. See also AXObjectCache::deferTextChangedIfNeeded().
                ASSERT_WITH_SECURITY_IMPLICATION(!nodeDocument->childNeedsStyleRecalc());
                ASSERT_WITH_SECURITY_IMPLICATION(!nodeDocument->view()->layoutContext().isInRenderTreeLayout());
                return plainText(*textRange, textIteratorBehaviorForTextRange());
            }
        }
    
        // Sometimes text fragments don't have Nodes associated with them (like when
        // CSS content is used to insert text or when a RenderCounter is used.)
        if (is<RenderText>(*m_renderer)) {
            RenderText& renderTextObject = downcast<RenderText>(*m_renderer);
            if (is<RenderTextFragment>(renderTextObject)) {
                RenderTextFragment& renderTextFragment = downcast<RenderTextFragment>(renderTextObject);
                // The alt attribute may be set on a text fragment through CSS, which should be honored.
                const String& altText = renderTextFragment.altText();
                if (!altText.isEmpty())
                    return altText;
                return renderTextFragment.contentString();
            }

            return renderTextObject.text();
        }
    }
    
    return AccessibilityNodeObject::textUnderElement(mode);
}

bool AccessibilityRenderObject::shouldGetTextFromNode(AccessibilityTextUnderElementMode mode) const
{
    if (!m_renderer)
        return false;

    // AccessibilityRenderObject::textUnderElement() gets the text of anonymous blocks by using
    // the child nodes to define positions. CSS tables and their anonymous descendants lack
    // children with nodes.
    if (m_renderer->isAnonymous() && m_renderer->isTablePart())
        return mode.childrenInclusion == AccessibilityTextUnderElementMode::TextUnderElementModeIncludeAllChildren;

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
    if (!m_renderer)
        return nullptr;
    if (m_renderer->isRenderView())
        return &m_renderer->document();
    return m_renderer->node();
}    

String AccessibilityRenderObject::stringValue() const
{
    if (!m_renderer)
        return String();

    if (isPasswordField())
        return passwordFieldValue();

    RenderBoxModelObject* cssBox = renderBoxModelObject();

    if (isARIAStaticText()) {
        String staticText = text();
        if (!staticText.length())
            staticText = textUnderElement();
        return staticText;
    }
        
    if (is<RenderText>(*m_renderer))
        return textUnderElement();

    if (is<RenderMenuList>(cssBox)) {
        // RenderMenuList will go straight to the text() of its selected item.
        // This has to be overridden in the case where the selected item has an ARIA label.
        HTMLSelectElement& selectElement = downcast<HTMLSelectElement>(*m_renderer->node());
        int selectedIndex = selectElement.selectedIndex();
        const Vector<HTMLElement*>& listItems = selectElement.listItems();
        if (selectedIndex >= 0 && static_cast<size_t>(selectedIndex) < listItems.size()) {
            const AtomString& overriddenDescription = listItems[selectedIndex]->attributeWithoutSynchronization(aria_labelAttr);
            if (!overriddenDescription.isNull())
                return overriddenDescription;
        }
        return downcast<RenderMenuList>(*m_renderer).text();
    }
    
    if (is<RenderListMarker>(*m_renderer))
        return downcast<RenderListMarker>(*m_renderer).text();
    
    if (isWebArea())
        return String();
    
    if (isTextControl())
        return text();
    
#if PLATFORM(IOS_FAMILY)
    if (isInputTypePopupButton())
        return textUnderElement();
#endif
    
    if (is<RenderFileUploadControl>(*m_renderer))
        return downcast<RenderFileUploadControl>(*m_renderer).fileTextValue();
    
    // FIXME: We might need to implement a value here for more types
    // FIXME: It would be better not to advertise a value at all for the types for which we don't implement one;
    // this would require subclassing or making accessibilityAttributeNames do something other than return a
    // single static array.
    return String();
}

bool AccessibilityRenderObject::canHavePlainText() const
{
    return isARIAStaticText() || is<RenderText>(*m_renderer) || isTextControl();
}

HTMLLabelElement* AccessibilityRenderObject::labelElementContainer() const
{
    if (!m_renderer)
        return nullptr;

    // the control element should not be considered part of the label
    if (isControl())
        return nullptr;
    
    // find if this has a parent that is a label
    for (Node* parentNode = m_renderer->node(); parentNode; parentNode = parentNode->parentNode()) {
        if (is<HTMLLabelElement>(*parentNode))
            return downcast<HTMLLabelElement>(parentNode);
    }
    
    return nullptr;
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
    RenderObject* obj = renderer();
    
    if (!obj)
        return LayoutRect();
    
    if (obj->node()) // If we are a continuation, we want to make sure to use the primary renderer.
        obj = obj->node()->renderer();
    
    // absoluteFocusRingQuads will query the hierarchy below this element, which for large webpages can be very slow.
    // For a web area, which will have the most elements of any element, absoluteQuads should be used.
    // We should also use absoluteQuads for SVG elements, otherwise transforms won't be applied.
    Vector<FloatQuad> quads;
    bool isSVGRoot = false;

    if (obj->isSVGRoot())
        isSVGRoot = true;

    if (is<RenderText>(*obj))
        quads = downcast<RenderText>(*obj).absoluteQuadsClippedToEllipsis();
    else if (isWebArea() || isSVGRoot)
        obj->absoluteQuads(quads);
    else
        obj->absoluteFocusRingQuads(quads);
    
    LayoutRect result = boundingBoxForQuads(obj, quads);

    Document* document = this->document();
    if (document && document->isSVGDocument())
        offsetBoundingBoxForRemoteSVGElement(result);
    
    // The size of the web area should be the content size, not the clipped size.
    if (isWebArea())
        result.setSize(obj->view().frameView().contentsSize());
    
    return result;
}
    
LayoutRect AccessibilityRenderObject::checkboxOrRadioRect() const
{
    if (!m_renderer)
        return LayoutRect();
    
    HTMLLabelElement* label = labelForElement(downcast<Element>(m_renderer->node()));
    if (!label || !label->renderer())
        return boundingBoxRect();
    
    LayoutRect labelRect = axObjectCache()->getOrCreate(label)->elementRect();
    labelRect.unite(boundingBoxRect());
    return labelRect;
}

LayoutRect AccessibilityRenderObject::elementRect() const
{
    // a checkbox or radio button should encompass its label
    if (isCheckboxOrRadio())
        return checkboxOrRadioRect();
    
    return boundingBoxRect();
}
    
bool AccessibilityRenderObject::supportsPath() const
{
    return is<RenderSVGShape>(renderer());
}

Path AccessibilityRenderObject::elementPath() const
{
    if (is<RenderSVGShape>(renderer()) && downcast<RenderSVGShape>(*m_renderer).hasPath()) {
        Path path = downcast<RenderSVGShape>(*m_renderer).path();
        
        // The SVG path is in terms of the parent's bounding box. The path needs to be offset to frame coordinates.
        if (auto svgRoot = ancestorsOfType<RenderSVGRoot>(*m_renderer).first()) {
            LayoutPoint parentOffset = axObjectCache()->getOrCreate(&*svgRoot)->elementRect().location();
            path.transform(AffineTransform().translate(parentOffset.x(), parentOffset.y()));
        }
        
        return path;
    }
    
    return Path();
}

IntPoint AccessibilityRenderObject::linkClickPoint()
{
    ASSERT(isLink());
    /* A link bounding rect can contain points that are not part of the link.
     For instance, a link that starts at the end of a line and finishes at the
     beginning of the next line will have a bounding rect that includes the
     entire two lines. In such a case, the middle point of the bounding rect
     may not belong to the link element and thus may not activate the link.
     Hence, return the middle point of the first character in the link if exists.
     */
    if (RefPtr<Range> range = elementRange()) {
        VisiblePosition start = range->startPosition();
        VisiblePosition end = nextVisiblePosition(start);
        if (start.isNull() || !range->contains(end))
            return AccessibilityObject::clickPoint();

        RefPtr<Range> charRange = makeRange(start, end);
        IntRect rect = boundsForRange(charRange);
        return { rect.x() + rect.width() / 2, rect.y() + rect.height() / 2 };
    }
    return AccessibilityObject::clickPoint();
}

IntPoint AccessibilityRenderObject::clickPoint()
{
    // Headings are usually much wider than their textual content. If the mid point is used, often it can be wrong.
    if (isHeading() && children().size() == 1)
        return children().first()->clickPoint();

    if (isLink())
        return linkClickPoint();

    // use the default position unless this is an editable web area, in which case we use the selection bounds.
    if (!isWebArea() || !canSetValueAttribute())
        return AccessibilityObject::clickPoint();
    
    VisibleSelection visSelection = selection();
    VisiblePositionRange range = VisiblePositionRange(visSelection.visibleStart(), visSelection.visibleEnd());
    IntRect bounds = boundsForVisiblePositionRange(range);
    return { bounds.x() + (bounds.width() / 2), bounds.y() + (bounds.height() / 2) };
}
    
AccessibilityObject* AccessibilityRenderObject::internalLinkElement() const
{
    auto element = anchorElement();

    // Right now, we do not support ARIA links as internal link elements
    if (!is<HTMLAnchorElement>(element))
        return nullptr;
    auto& anchor = downcast<HTMLAnchorElement>(*element);

    auto linkURL = anchor.href();
    auto fragmentIdentifier = linkURL.fragmentIdentifier();
    if (fragmentIdentifier.isEmpty())
        return nullptr;

    // check if URL is the same as current URL
    auto documentURL = m_renderer->document().url();
    if (!equalIgnoringFragmentIdentifier(documentURL, linkURL))
        return nullptr;

    auto linkedNode = m_renderer->document().findAnchor(fragmentIdentifier.toStringWithoutCopying());
    if (!linkedNode)
        return nullptr;

    // The element we find may not be accessible, so find the first accessible object.
    return firstAccessibleObjectFromNode(linkedNode);
}

OptionSet<SpeakAs> AccessibilityRenderObject::speakAsProperty() const
{
    if (!m_renderer)
        return AccessibilityObject::speakAsProperty();
    
    return m_renderer->style().speakAs();
}
    
void AccessibilityRenderObject::addRadioButtonGroupChildren(AXCoreObject* parent, AccessibilityChildrenVector& linkedUIElements) const
{
    for (const auto& child : parent->children()) {
        if (child->roleValue() == AccessibilityRole::RadioButton)
            linkedUIElements.append(child);
        else
            addRadioButtonGroupChildren(child.get(), linkedUIElements);
    }
}
    
void AccessibilityRenderObject::addRadioButtonGroupMembers(AccessibilityChildrenVector& linkedUIElements) const
{
    if (roleValue() != AccessibilityRole::RadioButton)
        return;
    
    Node* node = this->node();
    if (is<HTMLInputElement>(node)) {
        HTMLInputElement& input = downcast<HTMLInputElement>(*node);
        for (auto& radioSibling : input.radioButtonGroup()) {
            if (AccessibilityObject* object = axObjectCache()->getOrCreate(radioSibling.ptr()))
                linkedUIElements.append(object);
        }
    } else {
        // If we didn't find any radio button siblings with the traditional naming, lets search for a radio group role and find its children.
        for (AccessibilityObject* parent = parentObject(); parent; parent = parent->parentObject()) {
            if (parent->roleValue() == AccessibilityRole::RadioGroup)
                addRadioButtonGroupChildren(parent, linkedUIElements);
        }
    }
}
    
// linked ui elements could be all the related radio buttons in a group
// or an internal anchor connection
void AccessibilityRenderObject::linkedUIElements(AccessibilityChildrenVector& linkedUIElements) const
{
    ariaFlowToElements(linkedUIElements);

    if (isLink()) {
        AccessibilityObject* linkedAXElement = internalLinkElement();
        if (linkedAXElement)
            linkedUIElements.append(linkedAXElement);
    }

    if (roleValue() == AccessibilityRole::RadioButton)
        addRadioButtonGroupMembers(linkedUIElements);
}

bool AccessibilityRenderObject::hasTextAlternative() const
{
    // ARIA: section 2A, bullet #3 says if aria-labeledby or aria-label appears, it should
    // override the "label" element association.
    return ariaAccessibilityDescription().length();
}

bool AccessibilityRenderObject::hasPopup() const
{
    // Return true if this has the aria-haspopup attribute, or if it has an ancestor of type link with the aria-haspopup attribute.
    return Accessibility::findAncestor<AccessibilityObject>(*this, true, [this] (const AccessibilityObject& object) {
        return (this == &object) ? !equalLettersIgnoringASCIICase(object.popupValue(), "false")
            : object.isLink() && !equalLettersIgnoringASCIICase(object.popupValue(), "false");
    });
}

bool AccessibilityRenderObject::supportsDropping() const 
{
    return determineDropEffects().size();
}

bool AccessibilityRenderObject::supportsDragging() const
{
    const AtomString& grabbed = getAttribute(aria_grabbedAttr);
    return equalLettersIgnoringASCIICase(grabbed, "true") || equalLettersIgnoringASCIICase(grabbed, "false") || hasAttribute(draggableAttr);
}

bool AccessibilityRenderObject::isGrabbed()
{
#if ENABLE(DRAG_SUPPORT)
    if (mainFrame() && mainFrame()->eventHandler().draggingElement() == element())
        return true;
#endif

    return elementAttributeValue(aria_grabbedAttr);
}

Vector<String> AccessibilityRenderObject::determineDropEffects() const
{
    // Order is aria-dropeffect, dropzone, webkitdropzone
    const AtomString& dropEffects = getAttribute(aria_dropeffectAttr);
    if (!dropEffects.isEmpty()) {
        String dropEffectsString = dropEffects.string();
        dropEffectsString.replace('\n', ' ');
        return dropEffectsString.split(' ');
    }
    
    auto dropzone = getAttribute(dropzoneAttr);
    if (!dropzone.isEmpty())
        return Vector<String> { dropzone };
    
    auto webkitdropzone = getAttribute(webkitdropzoneAttr);
    if (!webkitdropzone.isEmpty())
        return Vector<String> { webkitdropzone };
    
    return { };
}
    
bool AccessibilityRenderObject::exposesTitleUIElement() const
{
    if (!isControl() && !isFigureElement())
        return false;

    // If this control is ignored (because it's invisible), 
    // then the label needs to be exposed so it can be visible to accessibility.
    if (accessibilityIsIgnored())
        return true;
    
    // When controls have their own descriptions, the title element should be ignored.
    if (hasTextAlternative())
        return false;
    
    // When <label> element has aria-label or aria-labelledby on it, we shouldn't expose it as the
    // titleUIElement, otherwise its inner text will be announced by a screenreader.
    if (isLabelable()) {
        if (HTMLLabelElement* label = labelForElement(downcast<Element>(node()))) {
            if (!label->attributeWithoutSynchronization(aria_labelAttr).isEmpty())
                return false;
            if (AccessibilityObject* labelObject = axObjectCache()->getOrCreate(label)) {
                if (!labelObject->ariaLabeledByAttribute().isEmpty())
                    return false;
                // To simplify instances where the labeling element includes widget descendants
                // which it does not label.
                if (is<AccessibilityLabel>(*labelObject)
                    && downcast<AccessibilityLabel>(*labelObject).containsUnrelatedControls())
                    return false;
            }
        }
    }
    
    return true;
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

void AccessibilityRenderObject::titleElementText(Vector<AccessibilityText>& textOrder) const
{
#if ENABLE(APPLE_PAY)
    if (isApplePayButton()) {
        textOrder.append(AccessibilityText(applePayButtonDescription(), AccessibilityTextSource::Alternative));
        return;
    }
#endif

    AccessibilityNodeObject::titleElementText(textOrder);
}
    
AccessibilityObject* AccessibilityRenderObject::titleUIElement() const
{
    if (!m_renderer)
        return nullptr;
    
    // if isFieldset is true, the renderer is guaranteed to be a RenderFieldset
    if (isFieldset())
        return axObjectCache()->getOrCreate(downcast<RenderBlock>(*m_renderer).findFieldsetLegend(RenderBlock::FieldsetIncludeFloatingOrOutOfFlow));
    
    if (isFigureElement())
        return captionForFigure();
    
    Node* node = m_renderer->node();
    if (!is<Element>(node))
        return nullptr;
    HTMLLabelElement* label = labelForElement(downcast<Element>(node));
    if (label && label->renderer())
        return axObjectCache()->getOrCreate(label);

    return nullptr;
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
    if (!altText.isAllSpecialCharacters<isHTMLSpace>())
        return AccessibilityObjectInclusion::IncludeObject;

    // The informal standard is to ignore images with zero-length alt strings:
    // https://www.w3.org/WAI/tutorials/images/decorative/.
    if (!altText.isNull())
        return AccessibilityObjectInclusion::IgnoreObject;

    return AccessibilityObjectInclusion::DefaultBehavior;
}

AccessibilityObjectInclusion AccessibilityRenderObject::defaultObjectInclusion() const
{
    // The following cases can apply to any element that's a subclass of AccessibilityRenderObject.
    
    if (!m_renderer)
        return AccessibilityObjectInclusion::IgnoreObject;

    if (m_renderer->style().visibility() != Visibility::Visible) {
        // aria-hidden is meant to override visibility as the determinant in AX hierarchy inclusion.
        if (equalLettersIgnoringASCIICase(getAttribute(aria_hiddenAttr), "false"))
            return AccessibilityObjectInclusion::DefaultBehavior;

        return AccessibilityObjectInclusion::IgnoreObject;
    }

    return AccessibilityObject::defaultObjectInclusion();
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
    AXTRACE("AccessibilityRenderObject::computeAccessibilityIsIgnored");
#ifndef NDEBUG
    ASSERT(m_initialized);
#endif

    if (!m_renderer)
        return true;
    
    // Check first if any of the common reasons cause this element to be ignored.
    // Then process other use cases that need to be applied to all the various roles
    // that AccessibilityRenderObjects take on.
    AccessibilityObjectInclusion decision = defaultObjectInclusion();
    if (decision == AccessibilityObjectInclusion::IncludeObject)
        return false;
    if (decision == AccessibilityObjectInclusion::IgnoreObject)
        return true;
    
    // If this element is within a parent that cannot have children, it should not be exposed.
    if (isDescendantOfBarrenParent())
        return true;    
    
    if (roleValue() == AccessibilityRole::Ignored)
        return true;
    
    if (roleValue() == AccessibilityRole::Presentational || inheritsPresentationalRole())
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
    
    // ignore popup menu items because AppKit does
    if (m_renderer && ancestorsOfType<RenderMenuList>(*m_renderer).first())
        return true;

    // https://webkit.org/b/161276 Getting the controlObject might cause the m_renderer to be nullptr.
    if (!m_renderer)
        return true;

    if (m_renderer->isBR())
        return true;

    if (is<RenderText>(*m_renderer)) {
        // static text beneath MenuItems and MenuButtons are just reported along with the menu item, so it's ignored on an individual level
        AXCoreObject* parent = parentObjectUnignored();
        if (parent && (parent->isMenuItem() || parent->ariaRoleAttribute() == AccessibilityRole::MenuButton))
            return true;
        auto& renderText = downcast<RenderText>(*m_renderer);
        if (!renderText.hasRenderedText())
            return true;

        if (renderText.parent()->isFirstLetter())
            return true;

        // static text beneath TextControls is reported along with the text control text so it's ignored.
        for (AccessibilityObject* parent = parentObject(); parent; parent = parent->parentObject()) { 
            if (parent->roleValue() == AccessibilityRole::TextField)
                return true;
        }
        
        // Walking up the parent chain might reset the m_renderer.
        if (!m_renderer)
            return true;
        
        // The alt attribute may be set on a text fragment through CSS, which should be honored.
        if (is<RenderTextFragment>(renderText)) {
            AccessibilityObjectInclusion altTextInclusion = objectInclusionFromAltText(downcast<RenderTextFragment>(renderText).altText());
            if (altTextInclusion == AccessibilityObjectInclusion::IgnoreObject)
                return true;
            if (altTextInclusion == AccessibilityObjectInclusion::IncludeObject)
                return false;
        }

        // text elements that are just empty whitespace should not be returned
        return renderText.text().isAllSpecialCharacters<isHTMLSpace>();
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
    
    if (isFigureElement())
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

        // First check the RenderImage's altText (which can be set through a style sheet, or come from the Element).
        // However, if this is not a native image, fallback to the attribute on the Element.
        AccessibilityObjectInclusion altTextInclusion = AccessibilityObjectInclusion::DefaultBehavior;
        bool isRenderImage = is<RenderImage>(renderer());
        if (isRenderImage)
            altTextInclusion = objectInclusionFromAltText(downcast<RenderImage>(*m_renderer).altText());
        else
            altTextInclusion = objectInclusionFromAltText(getAttribute(altAttr).string());

        if (altTextInclusion == AccessibilityObjectInclusion::IgnoreObject)
            return true;
        if (altTextInclusion == AccessibilityObjectInclusion::IncludeObject)
            return false;

        // If an image has the title or label attributes, accessibility should be lenient and allow it to appear in the hierarchy (according to WAI-ARIA).
        if (!getAttribute(titleAttr).isEmpty() || !getAttribute(aria_labelAttr).isEmpty())
            return false;

        if (isRenderImage) {
            // check for one-dimensional image
            RenderImage& image = downcast<RenderImage>(*m_renderer);
            if (image.height() <= 1 || image.width() <= 1)
                return true;

            // check whether rendered image was stretched from one-dimensional file image
            if (image.cachedImage()) {
                LayoutSize imageSize = image.cachedImage()->imageSizeForRenderer(&image, image.view().zoomFactor());
                return imageSize.height() <= 1 || imageSize.width() <= 1;
            }
        }
        return false;
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
    
    if (is<RenderBlockFlow>(*m_renderer) && m_renderer->childrenInline() && !canSetFocusAttribute())
        return !downcast<RenderBlockFlow>(*m_renderer).hasLines() && !mouseButtonListener();
    
    if (isCanvas()) {
        if (canvasHasFallbackContent())
            return false;

        if (is<RenderBox>(*m_renderer)) {
            auto& canvasBox = downcast<RenderBox>(*m_renderer);
            if (canvasBox.height() <= 1 || canvasBox.width() <= 1)
                return true;
        }
        // Otherwise fall through; use presence of help text, title, or description to decide.
    }

    if (m_renderer->isListMarker()) {
        AXCoreObject* parent = parentObjectUnignored();
        return parent && !parent->isListItem();
    }

    if (isWebArea())
        return false;
    
#if ENABLE(METER_ELEMENT)
    // The render tree of meter includes a RenderBlock (meter) and a RenderMeter (div).
    // We expose the latter and thus should ignore the former. However, if the author
    // includes a title attribute on the element, hasAttributesRequiredForInclusion()
    // will return true, potentially resulting in a redundant accessible object.
    if (is<HTMLMeterElement>(node))
        return true;
#endif

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
    
    // Make sure that ruby containers are not ignored.
    if (m_renderer->isRubyRun() || m_renderer->isRubyBlock() || m_renderer->isRubyInline())
        return false;

    // Find out if this element is inside of a label element.
    // If so, it may be ignored because it's the label for a checkbox or radio button.
    AccessibilityObject* controlObject = correspondingControlForLabelElement();
    if (controlObject && !controlObject->exposesTitleUIElement() && controlObject->isCheckboxOrRadio())
        return true;

    // By default, objects should be ignored so that the AX hierarchy is not
    // filled with unnecessary items.
    return true;
}

bool AccessibilityRenderObject::isLoaded() const
{
    return m_renderer ? !m_renderer->document().parser() : false;
}

double AccessibilityRenderObject::estimatedLoadingProgress() const
{
    if (!m_renderer)
        return 0;
    
    if (isLoaded())
        return 1.0;
    
    return m_renderer->page().progress().estimatedProgress();
}
    
int AccessibilityRenderObject::layoutCount() const
{
    if (!m_renderer || !is<RenderView>(*m_renderer))
        return 0;
    return downcast<RenderView>(*m_renderer).frameView().layoutContext().layoutCount();
}

String AccessibilityRenderObject::text() const
{
    if (isPasswordField())
        return passwordFieldValue();

    return AccessibilityNodeObject::text();
}
    
int AccessibilityRenderObject::textLength() const
{
    ASSERT(isTextControl());
    
    if (isPasswordField())
        return passwordFieldValue().length();

    return text().length();
}

PlainTextRange AccessibilityRenderObject::documentBasedSelectedTextRange() const
{
    Node* node = m_renderer->node();
    if (!node)
        return PlainTextRange();

    VisibleSelection visibleSelection = selection();
    auto selectionRange = visibleSelection.toNormalizedRange();
    if (!selectionRange)
        return PlainTextRange();

    // FIXME: The reason this does the correct thing when the selection is in the
    // shadow tree of an input element is that we get an exception below, and we
    // choose to interpret all exceptions as "does not intersect". Seems likely
    // that does not handle all cases correctly.
    auto intersectsResult = createLiveRange(*selectionRange)->intersectsNode(*node);
    if (!intersectsResult.hasException() && !intersectsResult.releaseReturnValue())
        return PlainTextRange();

    int start = indexForVisiblePosition(visibleSelection.start());
    int end = indexForVisiblePosition(visibleSelection.end());
    return PlainTextRange(start, end - start);
}

String AccessibilityRenderObject::selectedText() const
{
    ASSERT(isTextControl());
    
    if (isPasswordField())
        return String(); // need to return something distinct from empty string
    
    if (isNativeTextControl()) {
        HTMLTextFormControlElement& textControl = downcast<RenderTextControl>(*m_renderer).textFormControlElement();
        return textControl.selectedText();
    }
    
    return doAXStringForRange(documentBasedSelectedTextRange());
}

String AccessibilityRenderObject::accessKey() const
{
    if (!m_renderer)
        return String();

    Node* node = m_renderer->node();
    if (!is<Element>(node))
        return String();

    return downcast<Element>(*node).attributeWithoutSynchronization(accesskeyAttr);
}

VisibleSelection AccessibilityRenderObject::selection() const
{
    return m_renderer->frame().selection().selection();
}

PlainTextRange AccessibilityRenderObject::selectedTextRange() const
{
    ASSERT(isTextControl());
    
    if (isPasswordField())
        return PlainTextRange();
    
    AccessibilityRole ariaRole = ariaRoleAttribute();
    // Use the text control native range if it's a native object and it has no ARIA role (or has a text based ARIA role).
    if (isNativeTextControl() && (ariaRole == AccessibilityRole::Unknown || isARIATextControl())) {
        HTMLTextFormControlElement& textControl = downcast<RenderTextControl>(*m_renderer).textFormControlElement();
        return PlainTextRange(textControl.selectionStart(), textControl.selectionEnd() - textControl.selectionStart());
    }
    
    return documentBasedSelectedTextRange();
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

void AccessibilityRenderObject::setSelectedTextRange(const PlainTextRange& range)
{
    setTextSelectionIntent(axObjectCache(), range.length ? AXTextStateChangeTypeSelectionExtend : AXTextStateChangeTypeSelectionMove);

    if (isNativeTextControl()) {
        HTMLTextFormControlElement& textControl = downcast<RenderTextControl>(*m_renderer).textFormControlElement();
        textControl.setSelectionRange(range.start, range.start + range.length);
    } else {
        auto node = this->node();
        ASSERT(node);
        VisiblePosition start = visiblePositionForIndexUsingCharacterIterator(*node, range.start);
        VisiblePosition end = visiblePositionForIndexUsingCharacterIterator(*node, range.start + range.length);
        m_renderer->frame().selection().setSelection(VisibleSelection(start, end), FrameSelection::defaultSetSelectionOptions(UserTriggered));
    }
    
    clearTextSelectionIntent(axObjectCache());
}

URL AccessibilityRenderObject::url() const
{
    auto* node = this->node();
    if (isLink() && is<HTMLAnchorElement>(node)) {
        if (HTMLAnchorElement* anchor = downcast<HTMLAnchorElement>(anchorElement()))
            return anchor->href();
    }

    if (m_renderer && isWebArea())
        return m_renderer->document().url();

    if (isImage() && is<HTMLImageElement>(node))
        return downcast<HTMLImageElement>(node)->src();

    if (isInputImage() && is<HTMLInputElement>(node))
        return downcast<HTMLInputElement>(node)->src();

    return URL();
}

bool AccessibilityRenderObject::isUnvisited() const
{
    if (!m_renderer)
        return true;

    // FIXME: Is it a privacy violation to expose unvisited information to accessibility APIs?
    return m_renderer->style().isLink() && m_renderer->style().insideLink() == InsideLink::InsideUnvisited;
}

bool AccessibilityRenderObject::isVisited() const
{
    if (!m_renderer)
        return false;

    // FIXME: Is it a privacy violation to expose visited information to accessibility APIs?
    return m_renderer->style().isLink() && m_renderer->style().insideLink() == InsideLink::InsideVisited;
}

void AccessibilityRenderObject::setElementAttributeValue(const QualifiedName& attributeName, bool value)
{
    if (!m_renderer)
        return;
    
    Node* node = m_renderer->node();
    if (!is<Element>(node))
        return;
    
    downcast<Element>(*node).setAttribute(attributeName, (value) ? "true" : "false");
}
    
bool AccessibilityRenderObject::elementAttributeValue(const QualifiedName& attributeName) const
{
    if (!m_renderer)
        return false;
    
    return equalLettersIgnoringASCIICase(getAttribute(attributeName), "true");
}
    
bool AccessibilityRenderObject::isSelected() const
{
    if (!m_renderer)
        return false;
    
    if (!m_renderer->node())
        return false;
    
    if (equalLettersIgnoringASCIICase(getAttribute(aria_selectedAttr), "true"))
        return true;    
    
    if (isTabItem() && isTabItemSelected())
        return true;

    // Menu items are considered selectable by assistive technologies
    if (isMenuItem())
        return isFocused() || parentObjectUnignored()->activeDescendant() == this;

    return false;
}

bool AccessibilityRenderObject::isTabItemSelected() const
{
    if (!isTabItem() || !m_renderer)
        return false;
    
    Node* node = m_renderer->node();
    if (!node || !node->isElementNode())
        return false;
    
    // The ARIA spec says a tab item can also be selected if it is aria-labeled by a tabpanel
    // that has keyboard focus inside of it, or if a tabpanel in its aria-controls list has KB
    // focus inside of it.
    AccessibilityObject* focusedElement = static_cast<AccessibilityObject*>(focusedUIElement());
    if (!focusedElement)
        return false;
    
    Vector<Element*> elements;
    elementsFromAttribute(elements, aria_controlsAttr);
    
    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return false;
    
    for (const auto& element : elements) {
        AccessibilityObject* tabPanel = cache->getOrCreate(element);

        // A tab item should only control tab panels.
        if (!tabPanel || tabPanel->roleValue() != AccessibilityRole::TabPanel)
            continue;
        
        AccessibilityObject* checkFocusElement = focusedElement;
        // Check if the focused element is a descendant of the element controlled by the tab item.
        while (checkFocusElement) {
            if (tabPanel == checkFocusElement)
                return true;
            checkFocusElement = checkFocusElement->parentObject();
        }
    }
    
    return false;
}
    
bool AccessibilityRenderObject::isFocused() const
{
    if (!m_renderer)
        return false;
    
    Document& document = m_renderer->document();

    Element* focusedElement = document.focusedElement();
    if (!focusedElement)
        return false;
    
    // A web area is represented by the Document node in the DOM tree, which isn't focusable.
    // Check instead if the frame's selection controller is focused
    if (focusedElement == m_renderer->node()
        || (roleValue() == AccessibilityRole::WebArea && document.frame()->selection().isFocusedAndActive()))
        return true;
    
    return false;
}

void AccessibilityRenderObject::setFocused(bool on)
{
    if (!canSetFocusAttribute())
        return;
    
    Document* document = this->document();
    Node* node = this->node();

    if (!on || !is<Element>(node)) {
        document->setFocusedElement(nullptr);
        return;
    }

    // When a node is told to set focus, that can cause it to be deallocated, which means that doing
    // anything else inside this object will crash. To fix this, we added a RefPtr to protect this object
    // long enough for duration.
    RefPtr<AccessibilityObject> protectedThis(this);
    
    // If this node is already the currently focused node, then calling focus() won't do anything.
    // That is a problem when focus is removed from the webpage to chrome, and then returns.
    // In these cases, we need to do what keyboard and mouse focus do, which is reset focus first.
    if (document->focusedElement() == node)
        document->setFocusedElement(nullptr);

    // If we return from setFocusedElement and our element has been removed from a tree, axObjectCache() may be null.
    if (AXObjectCache* cache = axObjectCache()) {
        cache->setIsSynchronizingSelection(true);
        downcast<Element>(*node).focus();
        cache->setIsSynchronizingSelection(false);
    }
}

void AccessibilityRenderObject::setSelectedRows(AccessibilityChildrenVector& selectedRows)
{
    // Setting selected only makes sense in trees and tables (and tree-tables).
    AccessibilityRole role = roleValue();
    if (role != AccessibilityRole::Tree && role != AccessibilityRole::TreeGrid && role != AccessibilityRole::Table && role != AccessibilityRole::Grid)
        return;
    
    bool isMulti = isMultiSelectable();
    unsigned count = selectedRows.size();
    if (count > 1 && !isMulti)
        count = 1;
    
    for (const auto& selectedRow : selectedRows)
        selectedRow->setSelected(true);
}
    
void AccessibilityRenderObject::setValue(const String& string)
{
    if (!m_renderer || !is<Element>(m_renderer->node()))
        return;
    
    Element& element = downcast<Element>(*m_renderer->node());
    RenderObject& renderer = *m_renderer;
    
    // We should use the editor's insertText to mimic typing into the field.
    // Also only do this when the field is in editing mode.
    if (Frame* frame = renderer.document().frame()) {
        Editor& editor = frame->editor();
        if (element.shouldUseInputMethod()) {
            editor.clearText();
            editor.insertText(string, nullptr);
            return;
        }
    }
    // FIXME: Do we want to do anything here for ARIA textboxes?
    if (renderer.isTextField() && is<HTMLInputElement>(element))
        downcast<HTMLInputElement>(element).setValue(string);
    else if (renderer.isTextArea() && is<HTMLTextAreaElement>(element))
        downcast<HTMLTextAreaElement>(element).setValue(string);
}

bool AccessibilityRenderObject::supportsARIAOwns() const
{
    if (!m_renderer)
        return false;
    const AtomString& ariaOwns = getAttribute(aria_ownsAttr);

    return !ariaOwns.isEmpty();
}
    
RenderView* AccessibilityRenderObject::topRenderer() const
{
    Document* topDoc = topDocument();
    if (!topDoc)
        return nullptr;
    
    return topDoc->renderView();
}

Document* AccessibilityRenderObject::document() const
{
    if (!m_renderer)
        return nullptr;
    return &m_renderer->document();
}

Widget* AccessibilityRenderObject::widget() const
{
    if (!m_renderer || !is<RenderWidget>(*m_renderer))
        return nullptr;
    return downcast<RenderWidget>(*m_renderer).widget();
}

AccessibilityObject* AccessibilityRenderObject::accessibilityParentForImageMap(HTMLMapElement* map) const
{
    // find an image that is using this map
    if (!map)
        return nullptr;

    HTMLImageElement* imageElement = map->imageElement();
    if (!imageElement)
        return nullptr;
    
    if (AXObjectCache* cache = axObjectCache())
        return cache->getOrCreate(imageElement);
    
    return nullptr;
}

AXCoreObject::AccessibilityChildrenVector AccessibilityRenderObject::documentLinks()
{
    AccessibilityChildrenVector result;
    Document& document = m_renderer->document();
    Ref<HTMLCollection> links = document.links();
    for (unsigned i = 0; auto* current = links->item(i); ++i) {
        if (auto* renderer = current->renderer()) {
            RefPtr<AccessibilityObject> axObject = document.axObjectCache()->getOrCreate(renderer);
            ASSERT(axObject);
            if (!axObject->accessibilityIsIgnored() && axObject->isLink())
                result.append(axObject);
        } else {
            auto* parent = current->parentNode();
            if (is<HTMLAreaElement>(*current) && is<HTMLMapElement>(parent)) {
                auto& areaObject = downcast<AccessibilityImageMapLink>(*axObjectCache()->getOrCreate(AccessibilityRole::ImageMapLink));
                HTMLMapElement& map = downcast<HTMLMapElement>(*parent);
                areaObject.setHTMLAreaElement(downcast<HTMLAreaElement>(current));
                areaObject.setHTMLMapElement(&map);
                areaObject.setParent(accessibilityParentForImageMap(&map));

                result.append(&areaObject);
            }
        }
    }

    return result;
}

FrameView* AccessibilityRenderObject::documentFrameView() const 
{ 
    if (!m_renderer)
        return nullptr;

    // this is the RenderObject's Document's Frame's FrameView 
    return &m_renderer->view().frameView();
}

Widget* AccessibilityRenderObject::widgetForAttachmentView() const
{
    if (!isAttachment())
        return nullptr;
    return downcast<RenderWidget>(*m_renderer).widget();
}

// This function is like a cross-platform version of - (WebCoreTextMarkerRange*)textMarkerRange. It returns
// a Range that we can convert to a WebCoreTextMarkerRange in the Obj-C file
VisiblePositionRange AccessibilityRenderObject::visiblePositionRange() const
{
    if (!m_renderer)
        return VisiblePositionRange();
    
    // construct VisiblePositions for start and end
    Node* node = m_renderer->node();
    if (!node)
        return VisiblePositionRange();

    VisiblePosition startPos = firstPositionInOrBeforeNode(node);
    VisiblePosition endPos = lastPositionInOrAfterNode(node);

    // the VisiblePositions are equal for nodes like buttons, so adjust for that
    // FIXME: Really?  [button, 0] and [button, 1] are distinct (before and after the button)
    // I expect this code is only hit for things like empty divs?  In which case I don't think
    // the behavior is correct here -- eseidel
    if (startPos == endPos) {
        endPos = endPos.next();
        if (endPos.isNull())
            endPos = startPos;
    }

    return VisiblePositionRange(startPos, endPos);
}

VisiblePositionRange AccessibilityRenderObject::visiblePositionRangeForLine(unsigned lineCount) const
{
    if (!lineCount || !m_renderer)
        return VisiblePositionRange();
    
    // iterate over the lines
    // FIXME: this is wrong when lineNumber is lineCount+1,  because nextLinePosition takes you to the
    // last offset of the last line
    VisiblePosition visiblePos = m_renderer->view().positionForPoint(IntPoint(), nullptr);
    VisiblePosition savedVisiblePos;
    while (--lineCount) {
        savedVisiblePos = visiblePos;
        visiblePos = nextLinePosition(visiblePos, 0);
        if (visiblePos.isNull()
            || visiblePos == savedVisiblePos
            || visiblePos.equals(savedVisiblePos))
            return VisiblePositionRange();
    }
    
    // make a caret selection for the marker position, then extend it to the line
    // NOTE: ignores results of sel.modify because it returns false when
    // starting at an empty line.  The resulting selection in that case
    // will be a caret at visiblePos.
    FrameSelection selection;
    selection.setSelection(VisibleSelection(visiblePos));
    selection.modify(FrameSelection::AlterationExtend, SelectionDirection::Right, TextGranularity::LineBoundary);
    
    return VisiblePositionRange(selection.selection().visibleStart(), selection.selection().visibleEnd());
}
    
VisiblePosition AccessibilityRenderObject::visiblePositionForIndex(int index) const
{
    if (!m_renderer)
        return VisiblePosition();

    if (isNativeTextControl())
        return downcast<RenderTextControl>(*m_renderer).textFormControlElement().visiblePositionForIndex(index);

    if (!allowsTextRanges() && !is<RenderText>(*m_renderer))
        return VisiblePosition();
    
    Node* node = m_renderer->node();
    if (!node)
        return VisiblePosition();

    return visiblePositionForIndexUsingCharacterIterator(*node, index);
}
    
int AccessibilityRenderObject::indexForVisiblePosition(const VisiblePosition& position) const
{
    if (isNativeTextControl())
        return downcast<RenderTextControl>(*m_renderer).textFormControlElement().indexForVisiblePosition(position);

    if (!isTextControl())
        return 0;
    
    Node* node = m_renderer->node();
    if (!node)
        return 0;

    Position indexPosition = position.deepEquivalent();
    if (indexPosition.isNull() || highestEditableRoot(indexPosition, HasEditableAXRole) != node)
        return 0;

#if USE(ATK)
    // We need to consider replaced elements for GTK, as they will be
    // presented with the 'object replacement character' (0xFFFC).
    bool forSelectionPreservation = true;
#else
    bool forSelectionPreservation = false;
#endif

    return WebCore::indexForVisiblePosition(*node, position, forSelectionPreservation);
}

Element* AccessibilityRenderObject::rootEditableElementForPosition(const Position& position) const
{
    // Find the root editable or pseudo-editable (i.e. having an editable ARIA role) element.
    Element* result = nullptr;
    
    Element* rootEditableElement = position.rootEditableElement();

    for (Element* e = position.element(); e && e != rootEditableElement; e = e->parentElement()) {
        if (nodeIsTextControl(e))
            result = e;
        if (e->hasTagName(bodyTag))
            break;
    }

    if (result)
        return result;

    return rootEditableElement;
}

bool AccessibilityRenderObject::nodeIsTextControl(const Node* node) const
{
    if (!node)
        return false;

    if (AXObjectCache* cache = axObjectCache()) {
        if (AccessibilityObject* axObjectForNode = cache->getOrCreate(const_cast<Node*>(node)))
            return axObjectForNode->isTextControl();
    }

    return false;
}

static IntRect boundsForRects(const LayoutRect& rect1, const LayoutRect& rect2, const SimpleRange& dataRange)
{
    LayoutRect ourRect = rect1;
    ourRect.unite(rect2);

    // If the rectangle spans lines and contains multiple text characters, use the range's bounding box intead.
    if (rect1.maxY() != rect2.maxY()) {
        LayoutRect boundingBox = createLiveRange(dataRange)->absoluteBoundingBox();
        if (characterCount(dataRange) > 1 && !boundingBox.isEmpty())
            ourRect = boundingBox;
    }

    return snappedIntRect(ourRect);
}

IntRect AccessibilityRenderObject::boundsForVisiblePositionRange(const VisiblePositionRange& visiblePositionRange) const
{
    if (visiblePositionRange.isNull())
        return IntRect();
    
    // Create a mutable VisiblePositionRange.
    VisiblePositionRange range(visiblePositionRange);
    LayoutRect rect1 = range.start.absoluteCaretBounds();
    LayoutRect rect2 = range.end.absoluteCaretBounds();
    
    // readjust for position at the edge of a line.  This is to exclude line rect that doesn't need to be accounted in the range bounds
    if (rect2.y() != rect1.y()) {
        VisiblePosition endOfFirstLine = endOfLine(range.start);
        if (range.start == endOfFirstLine) {
            range.start.setAffinity(DOWNSTREAM);
            rect1 = range.start.absoluteCaretBounds();
        }
        if (range.end == endOfFirstLine) {
            range.end.setAffinity(UPSTREAM);
            rect2 = range.end.absoluteCaretBounds();
        }
    }
    
    return boundsForRects(rect1, rect2, *makeRange(range.start, range.end));
}

IntRect AccessibilityRenderObject::boundsForRange(const RefPtr<Range> range) const
{
    if (!range)
        return IntRect();
    
    AXObjectCache* cache = this->axObjectCache();
    if (!cache)
        return IntRect();
    
    CharacterOffset start = cache->startOrEndCharacterOffsetForRange(range, true);
    CharacterOffset end = cache->startOrEndCharacterOffsetForRange(range, false);
    
    LayoutRect rect1 = cache->absoluteCaretBoundsForCharacterOffset(start);
    LayoutRect rect2 = cache->absoluteCaretBoundsForCharacterOffset(end);
    
    // readjust for position at the edge of a line. This is to exclude line rect that doesn't need to be accounted in the range bounds.
    if (rect2.y() != rect1.y()) {
        CharacterOffset endOfFirstLine = cache->endCharacterOffsetOfLine(start);
        if (start.isEqual(endOfFirstLine)) {
            start = cache->nextCharacterOffset(start, false);
            rect1 = cache->absoluteCaretBoundsForCharacterOffset(start);
        }
        if (end.isEqual(endOfFirstLine)) {
            end = cache->previousCharacterOffset(end, false);
            rect2 = cache->absoluteCaretBoundsForCharacterOffset(end);
        }
    }
    
    return boundsForRects(rect1, rect2, *range);
}

bool AccessibilityRenderObject::isVisiblePositionRangeInDifferentDocument(const VisiblePositionRange& range) const
{
    if (range.start.isNull() || range.end.isNull())
        return false;
    
    VisibleSelection newSelection = VisibleSelection(range.start, range.end);
    if (Document* newSelectionDocument = newSelection.base().document()) {
        if (RefPtr<Frame> newSelectionFrame = newSelectionDocument->frame()) {
            Frame* frame = this->frame();
            if (!frame || (newSelectionFrame != frame && newSelectionDocument != frame->document()))
                return true;
        }
    }
    
    return false;
}
    
void AccessibilityRenderObject::setSelectedVisiblePositionRange(const VisiblePositionRange& range) const
{
    if (range.start.isNull() || range.end.isNull())
        return;
    
    // In WebKit1, when the top web area sets the selection to be an input element in an iframe, the caret will disappear.
    // FrameSelection::setSelectionWithoutUpdatingAppearance is setting the selection on the new frame in this case, and causing this behavior.
    if (isWebArea() && parentObject() && parentObject()->isAttachment()) {
        if (isVisiblePositionRangeInDifferentDocument(range))
            return;
    }

    // make selection and tell the document to use it. if it's zero length, then move to that position
    if (range.start == range.end) {
        setTextSelectionIntent(axObjectCache(), AXTextStateChangeTypeSelectionMove);
        m_renderer->frame().selection().moveTo(range.start, UserTriggered);
        clearTextSelectionIntent(axObjectCache());
    }
    else {
        setTextSelectionIntent(axObjectCache(), AXTextStateChangeTypeSelectionExtend);
        VisibleSelection newSelection = VisibleSelection(range.start, range.end);
        m_renderer->frame().selection().setSelection(newSelection, FrameSelection::defaultSetSelectionOptions());
        clearTextSelectionIntent(axObjectCache());
    }
}

VisiblePosition AccessibilityRenderObject::visiblePositionForPoint(const IntPoint& point) const
{
    if (!m_renderer)
        return VisiblePosition();

    // convert absolute point to view coordinates
    RenderView* renderView = topRenderer();
    if (!renderView)
        return VisiblePosition();

#if PLATFORM(COCOA)
    FrameView* frameView = &renderView->frameView();
#endif

    Node* innerNode = nullptr;
    
    // Locate the node containing the point
    // FIXME: Remove this loop and instead add HitTestRequest::AllowVisibleChildFrameContentOnly to the hit test request type.
    LayoutPoint pointResult;
    while (1) {
        LayoutPoint pointToUse;
#if PLATFORM(MAC)
        pointToUse = frameView->screenToContents(point);
#else
        pointToUse = point;
#endif
        constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::ReadOnly, HitTestRequest::Active };
        HitTestResult result { pointToUse };
        renderView->document().hitTest(hitType, result);
        innerNode = result.innerNode();
        if (!innerNode)
            return VisiblePosition();
        
        RenderObject* renderer = innerNode->renderer();
        if (!renderer)
            return VisiblePosition();
        
        pointResult = result.localPoint();

        // done if hit something other than a widget
        if (!is<RenderWidget>(*renderer))
            break;

        // descend into widget (FRAME, IFRAME, OBJECT...)
        Widget* widget = downcast<RenderWidget>(*renderer).widget();
        if (!is<FrameView>(widget))
            break;
        Frame& frame = downcast<FrameView>(*widget).frame();
        renderView = frame.document()->renderView();
#if PLATFORM(COCOA)
        frameView = downcast<FrameView>(widget);
#endif
    }
    
    return innerNode->renderer()->positionForPoint(pointResult, nullptr);
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
    position.setAffinity(DOWNSTREAM);
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

void AccessibilityRenderObject::lineBreaks(Vector<int>& lineBreaks) const
{
    if (!isTextControl())
        return;

    VisiblePosition visiblePos = visiblePositionForIndex(0);
    VisiblePosition savedVisiblePos = visiblePos;
    visiblePos = nextLinePosition(visiblePos, 0);
    while (!visiblePos.isNull() && visiblePos != savedVisiblePos) {
        lineBreaks.append(indexForVisiblePosition(visiblePos));
        savedVisiblePos = visiblePos;
        visiblePos = nextLinePosition(visiblePos, 0);
    }
}

// Given a line number, the range of characters of the text associated with this accessibility
// object that contains the line number.
PlainTextRange AccessibilityRenderObject::doAXRangeForLine(unsigned lineNumber) const
{
    if (!isTextControl())
        return PlainTextRange();
    
    // iterate to the specified line
    VisiblePosition visiblePos = visiblePositionForIndex(0);
    VisiblePosition savedVisiblePos;
    for (unsigned lineCount = lineNumber; lineCount; lineCount -= 1) {
        savedVisiblePos = visiblePos;
        visiblePos = nextLinePosition(visiblePos, 0);
        if (visiblePos.isNull() || visiblePos == savedVisiblePos)
            return PlainTextRange();
    }

    // Get the end of the line based on the starting position.
    VisiblePosition endPosition = endOfLine(visiblePos);

    int index1 = indexForVisiblePosition(visiblePos);
    int index2 = indexForVisiblePosition(endPosition);
    
    // add one to the end index for a line break not caused by soft line wrap (to match AppKit)
    if (endPosition.affinity() == DOWNSTREAM && endPosition.next().isNotNull())
        index2 += 1;
    
    // return nil rather than an zero-length range (to match AppKit)
    if (index1 == index2)
        return PlainTextRange();
    
    return PlainTextRange(index1, index2 - index1);
}

// The composed character range in the text associated with this accessibility object that
// is specified by the given index value. This parameterized attribute returns the complete
// range of characters (including surrogate pairs of multi-byte glyphs) at the given index.
PlainTextRange AccessibilityRenderObject::doAXRangeForIndex(unsigned index) const
{
    if (!isTextControl())
        return PlainTextRange();
    
    String elementText = text();
    if (!elementText.length() || index > elementText.length() - 1)
        return PlainTextRange();
    
    return PlainTextRange(index, 1);
}

// A substring of the text associated with this accessibility object that is
// specified by the given character range.
String AccessibilityRenderObject::doAXStringForRange(const PlainTextRange& range) const
{
    if (!range.length)
        return String();
    
    if (!isTextControl())
        return String();
    
    String elementText = isPasswordField() ? passwordFieldValue() : text();
    return elementText.substring(range.start, range.length);
}

// The bounding rectangle of the text associated with this accessibility object that is
// specified by the given range. This is the bounding rectangle a sighted user would see
// on the display screen, in pixels.
IntRect AccessibilityRenderObject::doAXBoundsForRange(const PlainTextRange& range) const
{
    if (allowsTextRanges())
        return boundsForVisiblePositionRange(visiblePositionRangeForRange(range));
    return IntRect();
}

IntRect AccessibilityRenderObject::doAXBoundsForRangeUsingCharacterOffset(const PlainTextRange& range) const
{
    if (allowsTextRanges())
        return boundsForRange(rangeForPlainTextRange(range));
    return IntRect();
}

AXCoreObject* AccessibilityRenderObject::accessibilityImageMapHitTest(HTMLAreaElement* area, const IntPoint& point) const
{
    if (!area)
        return nullptr;

    auto* mapParent = ancestorsOfType<HTMLMapElement>(*area).first();
    if (!mapParent)
        return nullptr;

    auto* parent = accessibilityParentForImageMap(mapParent);
    if (!parent)
        return nullptr;

    for (const auto& child : parent->children()) {
        if (child->elementRect().contains(point))
            return child.get();
    }

    return nullptr;
}

AXCoreObject* AccessibilityRenderObject::remoteSVGElementHitTest(const IntPoint& point) const
{
    AccessibilityObject* remote = remoteSVGRootElement(Create);
    if (!remote)
        return nullptr;
    
    IntSize offset = point - roundedIntPoint(boundingBoxRect().location());
    return remote->accessibilityHitTest(IntPoint(offset));
}

AXCoreObject* AccessibilityRenderObject::elementAccessibilityHitTest(const IntPoint& point) const
{
    if (isSVGImage())
        return remoteSVGElementHitTest(point);
    
    return AccessibilityObject::elementAccessibilityHitTest(point);
}
    
static bool shouldUseShadowHostForHitTesting(Node* shadowHost)
{
    // We need to allow automation of mouse events on video tags.
    return shadowHost && !shadowHost->hasTagName(videoTag);
}

AXCoreObject* AccessibilityRenderObject::accessibilityHitTest(const IntPoint& point) const
{
    if (!m_renderer || !m_renderer->hasLayer())
        return nullptr;
    
    m_renderer->document().updateLayout();

    if (!m_renderer || !m_renderer->hasLayer())
        return nullptr;

    RenderLayer* layer = downcast<RenderBox>(*m_renderer).layer();
     
    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::ReadOnly, HitTestRequest::Active, HitTestRequest::AccessibilityHitTest };
    HitTestResult hitTestResult { point };
    layer->hitTest(hitType, hitTestResult);
    Node* node = hitTestResult.innerNode();
    if (!node)
        return nullptr;
    Node* shadowAncestorNode = node->shadowHost();
    if (shouldUseShadowHostForHitTesting(shadowAncestorNode))
        node = shadowAncestorNode;
    ASSERT(node);

    if (is<HTMLAreaElement>(*node))
        return accessibilityImageMapHitTest(downcast<HTMLAreaElement>(node), point);
    
    if (is<HTMLOptionElement>(*node))
        node = downcast<HTMLOptionElement>(*node).ownerSelectElement();
    
    RenderObject* obj = node->renderer();
    if (!obj)
        return nullptr;
    
    AXCoreObject* result = obj->document().axObjectCache()->getOrCreate(obj);
    result->updateChildrenIfNecessary();

    // Allow the element to perform any hit-testing it might need to do to reach non-render children.
    result = static_cast<AccessibilityObject*>(result->elementAccessibilityHitTest(point));
    
    if (result && result->accessibilityIsIgnored()) {
        // If this element is the label of a control, a hit test should return the control.
        AXCoreObject* controlObject = result->correspondingControlForLabelElement();
        if (controlObject && !controlObject->exposesTitleUIElement())
            return controlObject;

        result = result->parentObjectUnignored();
    }

    return result;
}

bool AccessibilityRenderObject::shouldNotifyActiveDescendant() const
{
#if USE(ATK)
    // According to the Core AAM spec, ATK expects object:state-changed:focused notifications
    // whenever the active descendant changes.
    return true;
#endif
    // We want to notify that the combo box has changed its active descendant,
    // but we do not want to change the focus, because focus should remain with the combo box.
    if (isComboBox())
        return true;
    
    return shouldFocusActiveDescendant();
}

bool AccessibilityRenderObject::shouldFocusActiveDescendant() const
{
    switch (ariaRoleAttribute()) {
    case AccessibilityRole::ApplicationGroup:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::Menu:
    case AccessibilityRole::MenuBar:
    case AccessibilityRole::RadioGroup:
    case AccessibilityRole::Row:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::Meter:
    case AccessibilityRole::ProgressIndicator:
    case AccessibilityRole::Toolbar:
    case AccessibilityRole::Outline:
    case AccessibilityRole::Tree:
    case AccessibilityRole::Grid:
    /* FIXME: replace these with actual roles when they are added to AccessibilityRole
    composite
    alert
    alertdialog
    status
    timer
    */
        return true;
    default:
        return false;
    }
}

AccessibilityObject* AccessibilityRenderObject::activeDescendant() const
{
    if (!m_renderer)
        return nullptr;
    
    const AtomString& activeDescendantAttrStr = getAttribute(aria_activedescendantAttr);
    if (activeDescendantAttrStr.isNull() || activeDescendantAttrStr.isEmpty())
        return nullptr;
    Element* element = this->element();
    if (!element)
        return nullptr;
    
    Element* target = element->treeScope().getElementById(activeDescendantAttrStr);
    if (!target)
        return nullptr;
    
    if (AXObjectCache* cache = axObjectCache()) {
        AccessibilityObject* obj = cache->getOrCreate(target);
        if (obj && obj->isAccessibilityRenderObject())
            // an activedescendant is only useful if it has a renderer, because that's what's needed to post the notification
            return obj;
    }
    
    return nullptr;
}

void AccessibilityRenderObject::handleAriaExpandedChanged()
{
    // This object might be deleted under the call to the parentObject() method.
    auto protectedThis = makeRef(*this);
    
    // Find if a parent of this object should handle aria-expanded changes.
    AccessibilityObject* containerParent = this->parentObject();
    while (containerParent) {
        bool foundParent = false;
        
        switch (containerParent->roleValue()) {
        case AccessibilityRole::Tree:
        case AccessibilityRole::TreeGrid:
        case AccessibilityRole::Grid:
        case AccessibilityRole::Table:
        case AccessibilityRole::Browser:
            foundParent = true;
            break;
        default:
            break;
        }
        
        if (foundParent)
            break;
        
        containerParent = containerParent->parentObject();
    }
    
    // Post that the row count changed.
    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return;
    
    if (containerParent)
        cache->postNotification(containerParent, document(), AXObjectCache::AXRowCountChanged);

    // Post that the specific row either collapsed or expanded.
    if (roleValue() == AccessibilityRole::Row || roleValue() == AccessibilityRole::TreeItem)
        cache->postNotification(this, document(), isExpanded() ? AXObjectCache::AXRowExpanded : AXObjectCache::AXRowCollapsed);
    else
        cache->postNotification(this, document(), AXObjectCache::AXExpandedChanged);
}
    
RenderObject* AccessibilityRenderObject::targetElementForActiveDescendant(const QualifiedName& attributeName, AccessibilityObject* activeDescendant) const
{
    AccessibilityObject::AccessibilityChildrenVector elements;
    ariaElementsFromAttribute(elements, attributeName);
    for (const auto& element : elements) {
        if (activeDescendant->isDescendantOfObject(element.get()))
            return element->renderer();
    }

    return nullptr;
}

void AccessibilityRenderObject::handleActiveDescendantChanged()
{
    Element* element = downcast<Element>(renderer()->node());
    if (!element)
        return;
    if (!renderer()->frame().selection().isFocusedAndActive() || renderer()->document().focusedElement() != element)
        return;

    auto* activeDescendant = this->activeDescendant();
    if (activeDescendant && shouldNotifyActiveDescendant()) {
        auto* targetRenderer = renderer();
        
#if PLATFORM(COCOA)
        // If the combobox's activeDescendant is inside another object, the target element should be that parent.
        if (isComboBox()) {
            if (auto* ariaOwner = targetElementForActiveDescendant(aria_ownsAttr, activeDescendant))
                targetRenderer = ariaOwner;
            else if (auto* ariaController = targetElementForActiveDescendant(aria_controlsAttr, activeDescendant))
                targetRenderer = ariaController;
        }
#endif
    
        renderer()->document().axObjectCache()->postNotification(targetRenderer, AXObjectCache::AXActiveDescendantChanged);
    }
}

AccessibilityObject* AccessibilityRenderObject::correspondingControlForLabelElement() const
{
    HTMLLabelElement* labelElement = labelElementContainer();
    if (!labelElement)
        return nullptr;
    
    auto correspondingControl = labelElement->control();
    if (!correspondingControl)
        return nullptr;

    // Make sure the corresponding control isn't a descendant of this label that's in the middle of being destroyed.
    if (correspondingControl->renderer() && !correspondingControl->renderer()->parent())
        return nullptr;
    
    return axObjectCache()->getOrCreate(correspondingControl.get());
}

AccessibilityObject* AccessibilityRenderObject::correspondingLabelForControlElement() const
{
    if (!m_renderer)
        return nullptr;

    // ARIA: section 2A, bullet #3 says if aria-labeledby or aria-label appears, it should
    // override the "label" element association.
    if (hasTextAlternative())
        return nullptr;

    Node* node = m_renderer->node();
    if (is<HTMLElement>(node)) {
        if (HTMLLabelElement* label = labelForElement(downcast<HTMLElement>(node)))
            return axObjectCache()->getOrCreate(label);
    }

    return nullptr;
}

bool AccessibilityRenderObject::renderObjectIsObservable(RenderObject& renderer) const
{
    // AX clients will listen for AXValueChange on a text control.
    if (is<RenderTextControl>(renderer))
        return true;
    
    // AX clients will listen for AXSelectedChildrenChanged on listboxes.
    Node* node = renderer.node();
    if (!node)
        return false;
    
    if (nodeHasRole(node, "listbox") || (is<RenderBoxModelObject>(renderer) && downcast<RenderBoxModelObject>(renderer).isListBox()))
        return true;

    // Textboxes should send out notifications.
    if (nodeHasRole(node, "textbox") || (is<Element>(*node) && contentEditableAttributeIsEnabled(downcast<Element>(node))))
        return true;
    
    return false;
}
    
AccessibilityObject* AccessibilityRenderObject::observableObject() const
{
    // Find the object going up the parent chain that is used in accessibility to monitor certain notifications.
    for (RenderObject* renderer = this->renderer(); renderer && renderer->node(); renderer = renderer->parent()) {
        if (renderObjectIsObservable(*renderer)) {
            if (AXObjectCache* cache = axObjectCache())
                return cache->getOrCreate(renderer);
        }
    }
    
    return nullptr;
}

bool AccessibilityRenderObject::isDescendantOfElementType(const HashSet<QualifiedName>& tagNames) const
{
    for (auto& ancestor : ancestorsOfType<RenderElement>(*m_renderer)) {
        if (ancestor.element() && tagNames.contains(ancestor.element()->tagQName()))
            return true;
    }
    return false;
}

bool AccessibilityRenderObject::isDescendantOfElementType(const QualifiedName& tagName) const
{
    for (auto& ancestor : ancestorsOfType<RenderElement>(*m_renderer)) {
        if (ancestor.element() && ancestor.element()->hasTagName(tagName))
            return true;
    }
    return false;
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

AccessibilityRole AccessibilityRenderObject::determineAccessibilityRole()
{
    AXTRACE("AccessibilityRenderObject::determineAccessibilityRole");
    if (!m_renderer)
        return AccessibilityRole::Unknown;

#if ENABLE(APPLE_PAY)
    if (isApplePayButton())
        return AccessibilityRole::Button;
#endif

    // Sometimes we need to ignore the attribute role. Like if a tree is malformed,
    // we want to ignore the treeitem's attribute role.
    if ((m_ariaRole = determineAriaRoleAttribute()) != AccessibilityRole::Unknown && !shouldIgnoreAttributeRole())
        return m_ariaRole;

    Node* node = m_renderer->node();
    RenderBoxModelObject* cssBox = renderBoxModelObject();

    if (node && node->isLink())
        return AccessibilityRole::WebCoreLink;
    if (node && is<HTMLImageElement>(*node) && downcast<HTMLImageElement>(*node).hasAttributeWithoutSynchronization(usemapAttr))
        return AccessibilityRole::ImageMap;
    if ((cssBox && cssBox->isListItem()) || (node && node->hasTagName(liTag)))
        return AccessibilityRole::ListItem;
    if (m_renderer->isListMarker())
        return AccessibilityRole::ListMarker;
    if (node && node->hasTagName(buttonTag))
        return buttonRoleType();
    if (node && node->hasTagName(legendTag))
        return AccessibilityRole::Legend;
    if (m_renderer->isText())
        return AccessibilityRole::StaticText;
    if (cssBox && cssBox->isImage()) {
        if (is<HTMLInputElement>(node))
            return hasPopup() ? AccessibilityRole::PopUpButton : AccessibilityRole::Button;
        if (isSVGImage())
            return AccessibilityRole::SVGRoot;
        return AccessibilityRole::Image;
    }
    
    if (node && node->hasTagName(canvasTag))
        return AccessibilityRole::Canvas;

    if (cssBox && cssBox->isRenderView())
        return AccessibilityRole::WebArea;
    
    if (cssBox && cssBox->isTextField()) {
        if (is<HTMLInputElement>(node))
            return downcast<HTMLInputElement>(*node).isSearchField() ? AccessibilityRole::SearchField : AccessibilityRole::TextField;
    }
    
    if (cssBox && cssBox->isTextArea())
        return AccessibilityRole::TextArea;

    if (is<HTMLInputElement>(node)) {
        HTMLInputElement& input = downcast<HTMLInputElement>(*node);
        if (input.isCheckbox())
            return AccessibilityRole::CheckBox;
        if (input.isRadioButton())
            return AccessibilityRole::RadioButton;
        if (input.isTextButton())
            return buttonRoleType();
        // On iOS, the date field and time field are popup buttons. On other platforms they are text fields.
#if PLATFORM(IOS_FAMILY)
        if (input.isDateField() || input.isTimeField())
            return AccessibilityRole::PopUpButton;
#endif
#if ENABLE(INPUT_TYPE_COLOR)
        if (input.isColorControl())
            return AccessibilityRole::ColorWell;
#endif
    }
    
    if (hasContentEditableAttributeSet())
        return AccessibilityRole::TextArea;
    
    if (isFileUploadButton())
        return AccessibilityRole::Button;
    
    if (cssBox && cssBox->isMenuList())
        return AccessibilityRole::PopUpButton;
    
    if (headingLevel())
        return AccessibilityRole::Heading;
    
    if (m_renderer->isSVGRoot())
        return AccessibilityRole::SVGRoot;
    
    if (isStyleFormatGroup()) {
        if (node->hasTagName(delTag))
            return AccessibilityRole::Deletion;
        if (node->hasTagName(insTag))
            return AccessibilityRole::Insertion;
        if (node->hasTagName(subTag))
            return AccessibilityRole::Subscript;
        if (node->hasTagName(supTag))
            return AccessibilityRole::Superscript;
        return is<RenderInline>(*m_renderer) ? AccessibilityRole::Inline : AccessibilityRole::TextGroup;
    }
    
    if (node && node->hasTagName(ddTag))
        return AccessibilityRole::DescriptionListDetail;
    
    if (node && node->hasTagName(dtTag))
        return AccessibilityRole::DescriptionListTerm;

    if (node && node->hasTagName(dlTag))
        return AccessibilityRole::DescriptionList;

    if (node && node->hasTagName(fieldsetTag))
        return AccessibilityRole::Group;

    if (node && node->hasTagName(figureTag))
        return AccessibilityRole::Figure;

    // Check for Ruby elements
    if (m_renderer->isRubyText())
        return AccessibilityRole::RubyText;
    if (m_renderer->isRubyBase())
        return AccessibilityRole::RubyBase;
    if (m_renderer->isRubyRun())
        return AccessibilityRole::RubyRun;
    if (m_renderer->isRubyBlock())
        return AccessibilityRole::RubyBlock;
    if (m_renderer->isRubyInline())
        return AccessibilityRole::RubyInline;
    
    // This return value is what will be used if AccessibilityTableCell determines
    // the cell should not be treated as a cell (e.g. because it is a layout table.
    if (is<RenderTableCell>(renderer()))
        return AccessibilityRole::TextGroup;

    // Table sections should be ignored.
    if (m_renderer->isTableSection())
        return AccessibilityRole::Ignored;

    if (m_renderer->isHR())
        return AccessibilityRole::HorizontalRule;

    if (node && node->hasTagName(pTag))
        return AccessibilityRole::Paragraph;

    if (is<HTMLLabelElement>(node))
        return AccessibilityRole::Label;

    if (node && node->hasTagName(dfnTag))
        return AccessibilityRole::Definition;

    if (node && node->hasTagName(divTag))
        return AccessibilityRole::Div;

    if (is<HTMLFormElement>(node))
        return AccessibilityRole::Form;

    if (node && node->hasTagName(articleTag))
        return AccessibilityRole::DocumentArticle;

    if (node && node->hasTagName(mainTag))
        return AccessibilityRole::LandmarkMain;

    if (node && node->hasTagName(navTag))
        return AccessibilityRole::LandmarkNavigation;

    if (node && node->hasTagName(asideTag))
        return AccessibilityRole::LandmarkComplementary;

    // The default role attribute value for the section element, region, became a landmark in ARIA 1.1.
    // The HTML AAM spec says it is "strongly recommended" that ATs only convey and provide navigation
    // for section elements which have names.
    if (node && node->hasTagName(sectionTag))
        return hasAttribute(aria_labelAttr) || hasAttribute(aria_labelledbyAttr) ? AccessibilityRole::LandmarkRegion : AccessibilityRole::TextGroup;

    if (node && node->hasTagName(addressTag))
        return AccessibilityRole::Group;

    if (node && node->hasTagName(blockquoteTag))
        return AccessibilityRole::Blockquote;

    if (node && node->hasTagName(captionTag))
        return AccessibilityRole::Caption;
    
    if (node && node->hasTagName(markTag))
        return AccessibilityRole::Mark;

    if (node && node->hasTagName(preTag))
        return AccessibilityRole::Pre;

    if (is<HTMLDetailsElement>(node))
        return AccessibilityRole::Details;
    if (is<HTMLSummaryElement>(node))
        return AccessibilityRole::Summary;
    
    // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
    // Output elements should be mapped to status role.
    if (isOutput())
        return AccessibilityRole::ApplicationStatus;

#if ENABLE(VIDEO)
    if (is<HTMLVideoElement>(node))
        return AccessibilityRole::Video;
    if (is<HTMLAudioElement>(node))
        return AccessibilityRole::Audio;
#endif
    
    // The HTML element should not be exposed as an element. That's what the RenderView element does.
    if (node && node->hasTagName(htmlTag))
        return AccessibilityRole::Ignored;

    // There should only be one banner/contentInfo per page. If header/footer are being used within an article or section
    // then it should not be exposed as whole page's banner/contentInfo
    if (node && node->hasTagName(headerTag) && !isDescendantOfElementType({ articleTag, sectionTag }))
        return AccessibilityRole::LandmarkBanner;

    // http://webkit.org/b/190138 Footers should become contentInfo's if scoped to body (and consequently become a landmark).
    // It should remain a footer if scoped to main, sectioning elements (article, section) or root sectioning element (blockquote, details, dialog, fieldset, figure, td).
    if (node && node->hasTagName(footerTag)) {
        if (!isDescendantOfElementType({ articleTag, sectionTag, mainTag, blockquoteTag, detailsTag, fieldsetTag, figureTag, tdTag }))
            return AccessibilityRole::LandmarkContentInfo;
        return AccessibilityRole::Footer;
    }
    
    // menu tags with toolbar type should have Toolbar role.
    if (node && node->hasTagName(menuTag) && equalLettersIgnoringASCIICase(getAttribute(typeAttr), "toolbar"))
        return AccessibilityRole::Toolbar;
    
    if (node && node->hasTagName(timeTag))
        return AccessibilityRole::Time;
    
    // If the element does not have role, but it has ARIA attributes, or accepts tab focus, accessibility should fallback to exposing it as a group.
    if (supportsARIAAttributes() || canSetFocusAttribute())
        return AccessibilityRole::Group;

    if (m_renderer->isRenderBlockFlow())
        return m_renderer->isAnonymousBlock() ? AccessibilityRole::TextGroup : AccessibilityRole::Group;
    
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

AccessibilityOrientation AccessibilityRenderObject::orientation() const
{
    const AtomString& ariaOrientation = getAttribute(aria_orientationAttr);
    if (equalLettersIgnoringASCIICase(ariaOrientation, "horizontal"))
        return AccessibilityOrientation::Horizontal;
    if (equalLettersIgnoringASCIICase(ariaOrientation, "vertical"))
        return AccessibilityOrientation::Vertical;
    if (equalLettersIgnoringASCIICase(ariaOrientation, "undefined"))
        return AccessibilityOrientation::Undefined;

    // In ARIA 1.1, the implicit value of aria-orientation changed from horizontal
    // to undefined on all roles that don't have their own role-specific values. In
    // addition, the implicit value of combobox became undefined.
    if (isComboBox() || isRadioGroup() || isTreeGrid())
        return AccessibilityOrientation::Undefined;

    if (isScrollbar() || isListBox() || isMenu() || isTree())
        return AccessibilityOrientation::Vertical;
    
    if (isMenuBar() || isSplitter() || isTabList() || isToolbar() || isSlider())
        return AccessibilityOrientation::Horizontal;
    
    return AccessibilityObject::orientation();
}

bool AccessibilityRenderObject::inheritsPresentationalRole() const
{
    // ARIA states if an item can get focus, it should not be presentational.
    if (canSetFocusAttribute())
        return false;
    
    // ARIA spec says that when a parent object is presentational, and it has required child elements,
    // those child elements are also presentational. For example, <li> becomes presentational from <ul>.
    // http://www.w3.org/WAI/PF/aria/complete#presentation

    const Vector<const HTMLQualifiedName*>* parentTags;
    switch (roleValue()) {
    case AccessibilityRole::ListItem:
    case AccessibilityRole::ListMarker: {
        static const auto listItemParents = makeNeverDestroyed(Vector<const HTMLQualifiedName*> { &dlTag.get(), &olTag.get(), &ulTag.get() });
        parentTags = &listItemParents.get();
        break;
    }
    case AccessibilityRole::GridCell:
    case AccessibilityRole::Cell: {
        static const auto tableCellParents = makeNeverDestroyed(Vector<const HTMLQualifiedName*> { &tableTag.get() });
        parentTags = &tableCellParents.get();
        break;
    }
    default:
        // Not all elements need to do the following check, only ones that are required children.
        return false;
    }

    for (auto* parent = parentObject(); parent; parent = parent->parentObject()) {
        if (!is<AccessibilityRenderObject>(*parent))
            continue;

        Node* node = downcast<AccessibilityRenderObject>(*parent).node();
        if (!is<Element>(node))
            continue;

        // If native tag of the parent element matches an acceptable name, then return
        // based on its presentational status.
        auto& name = downcast<Element>(*node).tagQName();
        if (std::any_of(parentTags->begin(), parentTags->end(), [&name] (auto* possibleName) { return *possibleName == name; }))
            return parent->roleValue() == AccessibilityRole::Presentational;
    }

    return false;
}
    
bool AccessibilityRenderObject::isPresentationalChildOfAriaRole() const
{
    // Walk the parent chain looking for a parent that has presentational children
    AccessibilityObject* parent;
    for (parent = parentObject(); parent && !parent->ariaRoleHasPresentationalChildren(); parent = parent->parentObject())
    { }
    
    return parent;
}
    
bool AccessibilityRenderObject::ariaRoleHasPresentationalChildren() const
{
    switch (m_ariaRole) {
    case AccessibilityRole::Button:
    case AccessibilityRole::Slider:
    case AccessibilityRole::Image:
    case AccessibilityRole::ProgressIndicator:
    case AccessibilityRole::SpinButton:
    // case SeparatorRole:
        return true;
    default:
        return false;
    }
}

bool AccessibilityRenderObject::canSetExpandedAttribute() const
{
    if (roleValue() == AccessibilityRole::Details)
        return true;
    
    // An object can be expanded if it aria-expanded is true or false.
    const AtomString& expanded = getAttribute(aria_expandedAttr);
    if (equalLettersIgnoringASCIICase(expanded, "true") || equalLettersIgnoringASCIICase(expanded, "false"))
        return true;
    return false;
}

bool AccessibilityRenderObject::canSetTextRangeAttributes() const
{
    return isTextControl();
}

void AccessibilityRenderObject::textChanged()
{
    // If this element supports ARIA live regions, or is part of a region with an ARIA editable role,
    // then notify the AT of changes.
    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return;
    
    for (RenderObject* renderParent = renderer(); renderParent; renderParent = renderParent->parent()) {
        AccessibilityObject* parent = cache->get(renderParent);
        if (!parent)
            continue;
        
        if (parent->supportsLiveRegion())
            cache->postLiveRegionChangeNotification(parent);

        if (parent->isNonNativeTextControl())
            cache->postNotification(renderParent, AXObjectCache::AXValueChanged);
    }
}

void AccessibilityRenderObject::clearChildren()
{
    AccessibilityObject::clearChildren();
    m_childrenDirty = false;
}

void AccessibilityRenderObject::addImageMapChildren()
{
    RenderBoxModelObject* cssBox = renderBoxModelObject();
    if (!is<RenderImage>(cssBox))
        return;
    
    HTMLMapElement* map = downcast<RenderImage>(*cssBox).imageMap();
    if (!map)
        return;

    for (auto& area : descendantsOfType<HTMLAreaElement>(*map)) {
        // add an <area> element for this child if it has a link
        if (!area.isLink())
            continue;
        auto& areaObject = downcast<AccessibilityImageMapLink>(*axObjectCache()->getOrCreate(AccessibilityRole::ImageMapLink));
        areaObject.setHTMLAreaElement(&area);
        areaObject.setHTMLMapElement(map);
        areaObject.setParent(this);
        if (!areaObject.accessibilityIsIgnored())
            m_children.append(&areaObject);
        else
            axObjectCache()->remove(areaObject.objectID());
    }
}

void AccessibilityRenderObject::updateChildrenIfNecessary()
{
    if (needsToUpdateChildren())
        clearChildren();
    
    AccessibilityObject::updateChildrenIfNecessary();
}
    
void AccessibilityRenderObject::addTextFieldChildren()
{
    Node* node = this->node();
    if (!is<HTMLInputElement>(node))
        return;
    
    HTMLInputElement& input = downcast<HTMLInputElement>(*node);
    if (HTMLElement* autoFillElement = input.autoFillButtonElement()) {
        if (AccessibilityObject* axAutoFill = axObjectCache()->getOrCreate(autoFillElement))
            m_children.append(axAutoFill);
    }
    
    HTMLElement* spinButtonElement = input.innerSpinButtonElement();
    if (!is<SpinButtonElement>(spinButtonElement))
        return;

    auto& axSpinButton = downcast<AccessibilitySpinButton>(*axObjectCache()->getOrCreate(AccessibilityRole::SpinButton));
    axSpinButton.setSpinButtonElement(downcast<SpinButtonElement>(spinButtonElement));
    axSpinButton.setParent(this);
    m_children.append(&axSpinButton);
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
    if (!is<RenderImage>(renderer()))
        return nullptr;
    
    CachedImage* cachedImage = downcast<RenderImage>(*m_renderer).cachedImage();
    if (!cachedImage)
        return nullptr;
    
    Image* image = cachedImage->image();
    if (!is<SVGImage>(image))
        return nullptr;
    
    FrameView* frameView = downcast<SVGImage>(*image).frameView();
    if (!frameView)
        return nullptr;
    Frame& frame = frameView->frame();
    
    Document* document = frame.document();
    if (!is<SVGDocument>(document))
        return nullptr;
    
    auto rootElement = SVGDocument::rootElement(*document);
    if (!rootElement)
        return nullptr;
    RenderObject* rendererRoot = rootElement->renderer();
    if (!rendererRoot)
        return nullptr;
    
    AXObjectCache* cache = frame.document()->axObjectCache();
    if (!cache)
        return nullptr;
    AccessibilityObject* rootSVGObject = createIfNecessary == Create ? cache->getOrCreate(rendererRoot) : cache->get(rendererRoot);

    // In order to connect the AX hierarchy from the SVG root element from the loaded resource
    // the parent must be set, because there's no other way to get back to who created the image.
    ASSERT(!createIfNecessary || rootSVGObject);
    if (!is<AccessibilitySVGRoot>(rootSVGObject))
        return nullptr;
    
    return downcast<AccessibilitySVGRoot>(rootSVGObject);
}
    
void AccessibilityRenderObject::addRemoteSVGChildren()
{
    AccessibilitySVGRoot* root = remoteSVGRootElement(Create);
    if (!root)
        return;
    
    root->setParent(this);
    
    if (root->accessibilityIsIgnored()) {
        for (const auto& child : root->children())
            m_children.append(child);
    } else
        m_children.append(root);
}

void AccessibilityRenderObject::addCanvasChildren()
{
    // Add the unrendered canvas children as AX nodes, unless we're not using a canvas renderer
    // because JS is disabled for example.
    if (!node() || !node()->hasTagName(canvasTag) || (renderer() && !renderer()->isCanvas()))
        return;

    // If it's a canvas, it won't have rendered children, but it might have accessible fallback content.
    // Clear m_haveChildren because AccessibilityNodeObject::addChildren will expect it to be false.
    ASSERT(!m_children.size());
    m_haveChildren = false;
    AccessibilityNodeObject::addChildren();
}

void AccessibilityRenderObject::addAttachmentChildren()
{
    if (!isAttachment())
        return;

    // FrameView's need to be inserted into the AX hierarchy when encountered.
    Widget* widget = widgetForAttachmentView();
    if (!widget || !widget->isFrameView())
        return;
    
    addChild(axObjectCache()->getOrCreate(widget));
}

#if PLATFORM(COCOA)
void AccessibilityRenderObject::updateAttachmentViewParents()
{
    // Only the unignored parent should set the attachment parent, because that's what is reflected in the AX 
    // hierarchy to the client.
    if (accessibilityIsIgnored())
        return;
    
    for (const auto& child : m_children) {
        if (child->isAttachment())
            child->overrideAttachmentParent(this);
    }
}
#endif

// Hidden children are those that are not rendered or visible, but are specifically marked as aria-hidden=false,
// meaning that they should be exposed to the AX hierarchy.
void AccessibilityRenderObject::addHiddenChildren()
{
    Node* node = this->node();
    if (!node)
        return;
    
    // First do a quick run through to determine if we have any hidden nodes (most often we will not).
    // If we do have hidden nodes, we need to determine where to insert them so they match DOM order as close as possible.
    bool shouldInsertHiddenNodes = false;
    for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
        if (!child->renderer() && isNodeAriaVisible(child)) {
            shouldInsertHiddenNodes = true;
            break;
        }
    }
    
    if (!shouldInsertHiddenNodes)
        return;
    
    // Iterate through all of the children, including those that may have already been added, and
    // try to insert hidden nodes in the correct place in the DOM order.
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

        if (!isNodeAriaVisible(child))
            continue;
        
        unsigned previousSize = m_children.size();
        if (insertionIndex > previousSize)
            insertionIndex = previousSize;
        
        insertChild(axObjectCache()->getOrCreate(child), insertionIndex);
        insertionIndex += (m_children.size() - previousSize);
    }
}
    
void AccessibilityRenderObject::updateRoleAfterChildrenCreation()
{
    AXTRACE("AccessibilityRenderObject::updateRoleAfterChildrenCreation");
    // If a menu does not have valid menuitem children, it should not be exposed as a menu.
    auto role = roleValue();
    if (role == AccessibilityRole::Menu) {
        // Elements marked as menus must have at least one menu item child.
        size_t menuItemCount = 0;
        for (const auto& child : children()) {
            if (child->isMenuItem()) {
                menuItemCount++;
                break;
            }
        }

        if (!menuItemCount)
            m_role = AccessibilityRole::Group;
    }
    if (role == AccessibilityRole::SVGRoot && !hasChildren())
        m_role = AccessibilityRole::Image;
}
    
void AccessibilityRenderObject::addChildren()
{
    // If the need to add more children in addition to existing children arises, 
    // childrenChanged should have been called, leaving the object with no children.
    ASSERT(!m_haveChildren); 

    m_haveChildren = true;
    
    if (!canHaveChildren())
        return;
    
    for (RefPtr<AccessibilityObject> obj = firstChild(); obj; obj = obj->nextSibling())
        addChild(obj.get());
    
    m_subtreeDirty = false;
    
    addHiddenChildren();
    addAttachmentChildren();
    addImageMapChildren();
    addTextFieldChildren();
    addCanvasChildren();
    addRemoteSVGChildren();
    
#if PLATFORM(COCOA)
    updateAttachmentViewParents();
#endif
    
    updateRoleAfterChildrenCreation();
}

bool AccessibilityRenderObject::canHaveChildren() const
{
    if (!m_renderer)
        return false;

    return AccessibilityNodeObject::canHaveChildren();
}

const String AccessibilityRenderObject::liveRegionStatus() const
{
    const AtomString& liveRegionStatus = getAttribute(aria_liveAttr);
    // These roles have implicit live region status.
    if (liveRegionStatus.isEmpty())
        return defaultLiveRegionStatusForRole(roleValue());

    return liveRegionStatus;
}

const String AccessibilityRenderObject::liveRegionRelevant() const
{
    static MainThreadNeverDestroyed<const AtomString> defaultLiveRegionRelevant("additions text", AtomString::ConstructFromLiteral);
    const AtomString& relevant = getAttribute(aria_relevantAttr);

    // Default aria-relevant = "additions text".
    if (relevant.isEmpty())
        return "additions text";
    
    return relevant;
}

bool AccessibilityRenderObject::liveRegionAtomic() const
{
    const AtomString& atomic = getAttribute(aria_atomicAttr);
    if (equalLettersIgnoringASCIICase(atomic, "true"))
        return true;
    if (equalLettersIgnoringASCIICase(atomic, "false"))
        return false;

    // WAI-ARIA "alert" and "status" roles have an implicit aria-atomic value of true.
    switch (roleValue()) {
    case AccessibilityRole::ApplicationAlert:
    case AccessibilityRole::ApplicationStatus:
        return true;
    default:
        return false;
    }
}

bool AccessibilityRenderObject::isBusy() const
{
    return elementAttributeValue(aria_busyAttr);
}

bool AccessibilityRenderObject::canHaveSelectedChildren() const
{
    switch (roleValue()) {
    // These roles are containers whose children support aria-selected:
    case AccessibilityRole::Grid:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::TabList:
    case AccessibilityRole::Tree:
    case AccessibilityRole::TreeGrid:
    case AccessibilityRole::List:
    // These roles are containers whose children are treated as selected by assistive
    // technologies. We can get the "selected" item via aria-activedescendant or the
    // focused element.
    case AccessibilityRole::Menu:
    case AccessibilityRole::MenuBar:
        return true;
    default:
        return false;
    }
}
    
void AccessibilityRenderObject::ariaSelectedRows(AccessibilityChildrenVector& result)
{
    // Determine which rows are selected.
    bool isMulti = isMultiSelectable();

    // Prefer active descendant over aria-selected.
    AccessibilityObject* activeDesc = activeDescendant();
    if (activeDesc && (activeDesc->isTreeItem() || activeDesc->isTableRow())) {
        result.append(activeDesc);    
        if (!isMulti)
            return;
    }

    // Get all the rows.
    auto rowsIteration = [&](const auto& rows) {
        for (auto& row : rows) {
            if (row->isSelected() || row->isActiveDescendantOfFocusedContainer()) {
                result.append(row);
                if (!isMulti)
                    break;
            }
        }
    };
    if (isTree()) {
        AccessibilityChildrenVector allRows;
        ariaTreeRows(allRows);
        rowsIteration(allRows);
    } else if (is<AccessibilityTable>(*this)) {
        auto& thisTable = downcast<AccessibilityTable>(*this);
        if (thisTable.isExposable() && thisTable.supportsSelectedRows())
            rowsIteration(thisTable.rows());
    }
}
    
void AccessibilityRenderObject::ariaListboxSelectedChildren(AccessibilityChildrenVector& result)
{
    bool isMulti = isMultiSelectable();

    for (const auto& child : children()) {
        // Every child should have aria-role option, and if so, check for selected attribute/state.
        if (child->ariaRoleAttribute() == AccessibilityRole::ListBoxOption && (child->isSelected() || child->isActiveDescendantOfFocusedContainer())) {
            result.append(child);
            if (!isMulti)
                return;
        }
    }
}

void AccessibilityRenderObject::selectedChildren(AccessibilityChildrenVector& result)
{
    ASSERT(result.isEmpty());

    if (!canHaveSelectedChildren())
        return;

    switch (roleValue()) {
    case AccessibilityRole::ListBox:
        // native list boxes would be AccessibilityListBoxes, so only check for aria list boxes
        ariaListboxSelectedChildren(result);
        return;
    case AccessibilityRole::Grid:
    case AccessibilityRole::Tree:
    case AccessibilityRole::TreeGrid:
        ariaSelectedRows(result);
        return;
    case AccessibilityRole::TabList:
        if (AXCoreObject* selectedTab = selectedTabItem())
            result.append(selectedTab);
        return;
    case AccessibilityRole::List:
        if (auto* selectedListItemChild = selectedListItem())
            result.append(selectedListItemChild);
        return;
    case AccessibilityRole::Menu:
    case AccessibilityRole::MenuBar:
        if (AccessibilityObject* descendant = activeDescendant()) {
            result.append(descendant);
            return;
        }
        if (AccessibilityObject* focusedElement = static_cast<AccessibilityObject*>(focusedUIElement())) {
            result.append(focusedElement);
            return;
        }
        return;
    default:
        ASSERT_NOT_REACHED();
    }
}

void AccessibilityRenderObject::ariaListboxVisibleChildren(AccessibilityChildrenVector& result)      
{
    if (!hasChildren())
        addChildren();
    
    for (const auto& child : children()) {
        if (child->isOffScreen())
            result.append(child);
    }
}

void AccessibilityRenderObject::visibleChildren(AccessibilityChildrenVector& result)
{
    ASSERT(result.isEmpty());

    // Only listboxes are asked for their visible children.
    // Native list boxes would be AccessibilityListBoxes, so only check for aria list boxes.
    if (ariaRoleAttribute() != AccessibilityRole::ListBox)
        return;
    return ariaListboxVisibleChildren(result);
}
 
void AccessibilityRenderObject::tabChildren(AccessibilityChildrenVector& result)
{
    if (roleValue() != AccessibilityRole::TabList)
        return;

    for (const auto& child : children()) {
        if (child->isTabItem())
            result.append(child);
    }
}
    
String AccessibilityRenderObject::actionVerb() const
{
#if !PLATFORM(IOS_FAMILY)
    // FIXME: Need to add verbs for select elements.
    static NeverDestroyed<const String> buttonAction(AXButtonActionVerb());
    static NeverDestroyed<const String> textFieldAction(AXTextFieldActionVerb());
    static NeverDestroyed<const String> radioButtonAction(AXRadioButtonActionVerb());
    static NeverDestroyed<const String> checkedCheckBoxAction(AXUncheckedCheckBoxActionVerb());
    static NeverDestroyed<const String> uncheckedCheckBoxAction(AXUncheckedCheckBoxActionVerb());
    static NeverDestroyed<const String> linkAction(AXLinkActionVerb());

    switch (roleValue()) {
    case AccessibilityRole::Button:
    case AccessibilityRole::ToggleButton:
        return buttonAction;
    case AccessibilityRole::TextField:
    case AccessibilityRole::TextArea:
        return textFieldAction;
    case AccessibilityRole::RadioButton:
        return radioButtonAction;
    case AccessibilityRole::CheckBox:
        return isChecked() ? checkedCheckBoxAction : uncheckedCheckBoxAction;
    case AccessibilityRole::Link:
    case AccessibilityRole::WebCoreLink:
        return linkAction;
    default:
        return nullAtom();
    }
#else
    return nullAtom();
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

    if (is<Element>(node))
        downcast<Element>(*node).setAttribute(aria_labelAttr, name);
}
    
static bool isLinkable(const AccessibilityRenderObject& object)
{
    if (!object.renderer())
        return false;

    // See https://wiki.mozilla.org/Accessibility/AT-Windows-API for the elements
    // Mozilla considers linkable.
    return object.isLink() || object.isImage() || object.renderer()->isText();
}

String AccessibilityRenderObject::stringValueForMSAA() const
{
    if (isLinkable(*this)) {
        Element* anchor = anchorElement();
        if (is<HTMLAnchorElement>(anchor))
            return downcast<HTMLAnchorElement>(*anchor).href().string();
    }

    return stringValue();
}

bool AccessibilityRenderObject::isLinked() const
{
    if (!isLinkable(*this))
        return false;

    Element* anchor = anchorElement();
    if (!is<HTMLAnchorElement>(anchor))
        return false;

    return !downcast<HTMLAnchorElement>(*anchor).href().isEmpty();
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
    return m_renderer->style().appearance() == ApplePayButtonPart;
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
    
    return m_renderer->style().textDecorationsInEffect().contains(TextDecoration::Underline);
}

String AccessibilityRenderObject::nameForMSAA() const
{
    if (m_renderer && m_renderer->isText())
        return textUnderElement();

    return title();
}

static bool shouldReturnTagNameAsRoleForMSAA(const Element& element)
{
    return element.hasTagName(abbrTag) || element.hasTagName(acronymTag)
        || element.hasTagName(blockquoteTag) || element.hasTagName(ddTag)
        || element.hasTagName(dlTag) || element.hasTagName(dtTag)
        || element.hasTagName(formTag) || element.hasTagName(frameTag)
        || element.hasTagName(h1Tag) || element.hasTagName(h2Tag)
        || element.hasTagName(h3Tag) || element.hasTagName(h4Tag)
        || element.hasTagName(h5Tag) || element.hasTagName(h6Tag)
        || element.hasTagName(iframeTag) || element.hasTagName(qTag)
        || element.hasTagName(tbodyTag) || element.hasTagName(tfootTag)
        || element.hasTagName(theadTag);
}

String AccessibilityRenderObject::stringRoleForMSAA() const
{
    if (!m_renderer)
        return String();

    Node* node = m_renderer->node();
    if (!is<Element>(node))
        return String();

    Element& element = downcast<Element>(*node);
    if (!shouldReturnTagNameAsRoleForMSAA(element))
        return String();

    return element.tagName();
}

String AccessibilityRenderObject::positionalDescriptionForMSAA() const
{
    // See "positional descriptions",
    // https://wiki.mozilla.org/Accessibility/AT-Windows-API
    if (isHeading())
        return makeString('L', headingLevel());

    // FIXME: Add positional descriptions for other elements.
    return String();
}

String AccessibilityRenderObject::descriptionForMSAA() const
{
    String description = positionalDescriptionForMSAA();
    if (!description.isEmpty())
        return description;

    description = accessibilityDescription();
    if (!description.isEmpty()) {
        // From the Mozilla MSAA implementation:
        // "Signal to screen readers that this description is speakable and is not
        // a formatted positional information description. Don't localize the
        // 'Description: ' part of this string, it will be parsed out by assistive
        // technologies."
        return "Description: " + description;
    }

    return String();
}

static AccessibilityRole msaaRoleForRenderer(const RenderObject* renderer)
{
    if (!renderer)
        return AccessibilityRole::Unknown;

    if (is<RenderText>(*renderer))
        return AccessibilityRole::EditableText;

    if (is<RenderListItem>(*renderer))
        return AccessibilityRole::ListItem;

    return AccessibilityRole::Unknown;
}

AccessibilityRole AccessibilityRenderObject::roleValueForMSAA() const
{
    if (m_roleForMSAA != AccessibilityRole::Unknown)
        return m_roleForMSAA;

    m_roleForMSAA = msaaRoleForRenderer(renderer());

    if (m_roleForMSAA == AccessibilityRole::Unknown)
        m_roleForMSAA = roleValue();

    return m_roleForMSAA;
}

String AccessibilityRenderObject::passwordFieldValue() const
{
    ASSERT(isPasswordField());

    // Look for the RenderText object in the RenderObject tree for this input field.
    RenderObject* renderer = node()->renderer();
    while (renderer && !is<RenderText>(renderer))
        renderer = downcast<RenderElement>(*renderer).firstChild();

    if (!is<RenderText>(renderer))
        return String();

    // Return the text that is actually being rendered in the input field.
    return downcast<RenderText>(*renderer).textWithoutConvertingBackslashToYenSymbol();
}

ScrollableArea* AccessibilityRenderObject::getScrollableAreaIfScrollable() const
{
    // If the parent is a scroll view, then this object isn't really scrollable, the parent ScrollView should handle the scrolling.
    if (auto* parent = parentObject()) {
        if (parent->isScrollView())
            return nullptr;
    }

    if (!is<RenderBox>(renderer()))
        return nullptr;

    auto& box = downcast<RenderBox>(*m_renderer);
    if (!box.canBeScrolledAndHasScrollableArea())
        return nullptr;

    return box.layer();
}

void AccessibilityRenderObject::scrollTo(const IntPoint& point) const
{
    if (!is<RenderBox>(renderer()))
        return;

    auto& box = downcast<RenderBox>(*m_renderer);
    if (!box.canBeScrolledAndHasScrollableArea())
        return;

    // FIXME: is point a ScrollOffset or ScrollPosition? Test in RTL overflow.
    box.layer()->scrollToOffset(point);
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
