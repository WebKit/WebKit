/*
 * Copyright (C) 2008-2022 Apple Inc. All rights reserved.
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

#if ENABLE(ACCESSIBILITY)

#include "AXObjectCache.h"

#include "AXImage.h"
#include "AXIsolatedObject.h"
#include "AXIsolatedTree.h"
#include "AXLogger.h"
#include "AccessibilityARIAGrid.h"
#include "AccessibilityARIAGridCell.h"
#include "AccessibilityARIAGridRow.h"
#include "AccessibilityAttachment.h"
#include "AccessibilityImageMapLink.h"
#include "AccessibilityLabel.h"
#include "AccessibilityList.h"
#include "AccessibilityListBox.h"
#include "AccessibilityListBoxOption.h"
#include "AccessibilityMathMLElement.h"
#include "AccessibilityMediaObject.h"
#include "AccessibilityMenuList.h"
#include "AccessibilityMenuListOption.h"
#include "AccessibilityMenuListPopup.h"
#include "AccessibilityProgressIndicator.h"
#include "AccessibilityRenderObject.h"
#include "AccessibilitySVGElement.h"
#include "AccessibilitySVGRoot.h"
#include "AccessibilityScrollView.h"
#include "AccessibilityScrollbar.h"
#include "AccessibilitySlider.h"
#include "AccessibilitySpinButton.h"
#include "AccessibilityTable.h"
#include "AccessibilityTableCell.h"
#include "AccessibilityTableColumn.h"
#include "AccessibilityTableHeaderContainer.h"
#include "AccessibilityTableRow.h"
#include "AccessibilityTree.h"
#include "AccessibilityTreeItem.h"
#include "CaretRectComputation.h"
#include "CustomElementDefaultARIA.h"
#include "Document.h"
#include "Editing.h"
#include "Editor.h"
#include "ElementIterator.h"
#include "ElementRareData.h"
#include "FocusController.h"
#include "Frame.h"
#include "HTMLAreaElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLDialogElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLMediaElement.h"
#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLProgressElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTableElement.h"
#include "HTMLTablePartElement.h"
#include "HTMLTableSectionElement.h"
#include "HTMLTextFormControlElement.h"
#include "InlineRunAndOffset.h"
#include "MathMLElement.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "Range.h"
#include "RenderAttachment.h"
#include "RenderImage.h"
#include "RenderLayer.h"
#include "RenderLineBreak.h"
#include "RenderListBox.h"
#include "RenderMathMLOperator.h"
#include "RenderMenuList.h"
#include "RenderMeter.h"
#include "RenderProgress.h"
#include "RenderSlider.h"
#include "RenderTable.h"
#include "RenderTableCell.h"
#include "RenderTableRow.h"
#include "RenderView.h"
#include "SVGElement.h"
#include "ScriptDisallowedScope.h"
#include "ScrollView.h"
#include "ShadowRoot.h"
#include "TextBoundaries.h"
#include "TextControlInnerElements.h"
#include "TextIterator.h"
#include <utility>
#include <wtf/DataLog.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>
#include <wtf/text/AtomString.h>

#if COMPILER(MSVC)
// See https://msdn.microsoft.com/en-us/library/1wea5zwe.aspx
#pragma warning(disable: 4701)
#endif

namespace WebCore {

using namespace HTMLNames;

// Post value change notifications for password fields or elements contained in password fields at a 40hz interval to thwart analysis of typing cadence
static const Seconds accessibilityPasswordValueChangeNotificationInterval { 25_ms };

static bool rendererNeedsDeferredUpdate(const RenderObject& renderer)
{
    ASSERT(!renderer.beingDestroyed());
    auto& document = renderer.document();
    return renderer.needsLayout() || document.needsStyleRecalc() || document.inRenderTreeUpdate() || (document.view() && document.view()->layoutContext().isInRenderTreeLayout());
}

static bool nodeAndRendererAreValid(Node* node)
{
    if (!node)
        return false;
    
    auto* renderer = node->renderer();
    return renderer && !renderer->beingDestroyed();
}
    
AccessibilityObjectInclusion AXComputedObjectAttributeCache::getIgnored(AXID id) const
{
    auto it = m_idMapping.find(id);
    return it != m_idMapping.end() ? it->value.ignored : AccessibilityObjectInclusion::DefaultBehavior;
}

void AXComputedObjectAttributeCache::setIgnored(AXID id, AccessibilityObjectInclusion inclusion)
{
    HashMap<AXID, CachedAXObjectAttributes>::iterator it = m_idMapping.find(id);
    if (it != m_idMapping.end())
        it->value.ignored = inclusion;
    else {
        CachedAXObjectAttributes attributes;
        attributes.ignored = inclusion;
        m_idMapping.set(id, attributes);
    }
}

AccessibilityReplacedText::AccessibilityReplacedText(const VisibleSelection& selection)
{
    if (AXObjectCache::accessibilityEnabled()) {
        m_replacedRange.startIndex.value = indexForVisiblePosition(selection.visibleStart(), m_replacedRange.startIndex.scope);
        if (selection.isRange()) {
            m_replacedText = AccessibilityObject::stringForVisiblePositionRange(selection);
            m_replacedRange.endIndex.value = indexForVisiblePosition(selection.visibleEnd(), m_replacedRange.endIndex.scope);
        } else
            m_replacedRange.endIndex = m_replacedRange.startIndex;
    }
}

void AccessibilityReplacedText::postTextStateChangeNotification(AXObjectCache* cache, AXTextEditType type, const String& text, const VisibleSelection& selection)
{
    if (!cache)
        return;
    if (!AXObjectCache::accessibilityEnabled())
        return;

    VisiblePosition position = selection.start();
    auto* node = highestEditableRoot(position.deepEquivalent(), HasEditableAXRole);
    if (m_replacedText.length())
        cache->postTextReplacementNotification(node, AXTextEditTypeDelete, m_replacedText, type, text, position);
    else
        cache->postTextStateChangeNotification(node, type, text, position);
}

bool AXObjectCache::gAccessibilityEnabled = false;
bool AXObjectCache::gAccessibilityEnhancedUserInterfaceEnabled = false;

void AXObjectCache::enableAccessibility()
{
    ASSERT(isMainThread());
    gAccessibilityEnabled = true;
}

void AXObjectCache::disableAccessibility()
{
    gAccessibilityEnabled = false;
}

void AXObjectCache::setEnhancedUserInterfaceAccessibility(bool flag)
{
    gAccessibilityEnhancedUserInterfaceEnabled = flag;
#if PLATFORM(MAC)
    if (flag)
        enableAccessibility();
#endif
}

AXObjectCache::AXObjectCache(Document& document)
    : m_document(document)
    , m_pageID(document.pageID())
    , m_notificationPostTimer(*this, &AXObjectCache::notificationPostTimerFired)
    , m_passwordNotificationPostTimer(*this, &AXObjectCache::passwordNotificationPostTimerFired)
    , m_liveRegionChangedPostTimer(*this, &AXObjectCache::liveRegionChangedNotificationPostTimerFired)
    , m_currentModalElement(nullptr)
    , m_performCacheUpdateTimer(*this, &AXObjectCache::performCacheUpdateTimerFired)
{
    AXTRACE(makeString("AXObjectCache::AXObjectCache 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
#ifndef NDEBUG
    if (m_pageID)
        AXLOG(makeString("pageID ", m_pageID->loggingString()));
    else
        AXLOG("No pageID.");
#endif
    ASSERT(isMainThread());

    // If loading completed before the cache was created, loading progress will have been reset to zero.
    // Consider loading progress to be 100% in this case.
    double loadingProgress = document.page() ? document.page()->progress().estimatedProgress() : 1;
    if (loadingProgress <= 0)
        loadingProgress = 1;
    m_loadingProgress = loadingProgress;
}

AXObjectCache::~AXObjectCache()
{
    AXTRACE(makeString("AXObjectCache::~AXObjectCache 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    m_notificationPostTimer.stop();
    m_liveRegionChangedPostTimer.stop();
    m_performCacheUpdateTimer.stop();

    for (const auto& object : m_objects.values())
        object->detach(AccessibilityDetachmentType::CacheDestroyed);

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (m_pageID)
        AXIsolatedTree::removeTreeForPageID(*m_pageID);
#endif
}

bool AXObjectCache::isModalElement(Element& element) const
{
    bool hasDialogRole = nodeHasRole(&element, "dialog"_s) || nodeHasRole(&element, "alertdialog"_s);
    AtomString modalValue = element.attributeWithoutSynchronization(aria_modalAttr);
    if (modalValue.isNull()) {
        if (auto* defaultARIA = element.customElementDefaultARIAIfExists())
            modalValue = defaultARIA->valueForAttribute(element, aria_modalAttr);
    }
    bool isAriaModal = equalLettersIgnoringASCIICase(modalValue, "true"_s);
    return (hasDialogRole && isAriaModal) || (is<HTMLDialogElement>(element) && downcast<HTMLDialogElement>(element).isModal());
}

void AXObjectCache::findModalNodes()
{
    // Traverse the DOM tree to look for the aria-modal=true nodes or modal <dialog> elements.
    for (Element* element = ElementTraversal::firstWithin(document().rootNode()); element; element = ElementTraversal::nextIncludingPseudo(*element)) {
        if (isModalElement(*element))
            m_modalElements.append(element);
    }

    m_modalNodesInitialized = true;
}

bool AXObjectCache::modalElementHasAccessibleContent(Element& element)
{
    // Unless you're trying to compute the new modal node, determining whether an element
    // has accessible content is as easy as !getOrCreate(element)->children().isEmpty().
    // So don't call this method on anything besides modal elements.
    ASSERT(isModalElement(element));

    // Because computing any object's children() is dependent on whether a modal is on the page,
    // we'll need to walk the DOM and find non-ignored AX objects manually.
    Vector<Node*> nodeStack = { element.firstChild() };
    while (!nodeStack.isEmpty()) {
        for (auto* node = nodeStack.takeLast(); node; node = node->nextSibling()) {
            if (auto* axObject = getOrCreate(node)) {
                if (!axObject->computeAccessibilityIsIgnored())
                    return true;
            }

            // Don't descend into subtrees for non-visible nodes.
            if (isNodeVisible(node))
                nodeStack.append(node->firstChild());
        }
    }
    return false;
}

void AXObjectCache::updateCurrentModalNode(WillRecomputeFocus willRecomputeFocus)
{
    auto recomputeModalElement = [&] () -> Element* {
        // There might be multiple modal dialog nodes.
        // We use this function to pick the one we want.
        if (m_modalElements.isEmpty())
            return nullptr;

        // Pick the document active modal <dialog> element if it exists.
        if (Element* activeModalDialog = document().activeModalDialog()) {
            ASSERT(m_modalElements.contains(activeModalDialog));
            return activeModalDialog;
        }

        SetForScope retrievingCurrentModalNode(m_isRetrievingCurrentModalNode, true);
        // If any of the modal nodes contains the keyboard focus, we want to pick that one.
        // If not, we want to pick the last visible dialog in the DOM.
        RefPtr<Element> focusedElement = document().focusedElement();
        bool focusedElementIsOutsideModals = focusedElement;
        RefPtr<Element> lastVisible;
        for (auto& element : m_modalElements) {
            // Elements in m_modalElementsSet may have become un-modal since we added them, but not yet removed
            // as part of the asynchronous m_deferredModalChangedList handling. Skip these.
            if (!element || !isModalElement(*element))
                continue;

            // To avoid trapping users in an empty modal, skip any non-visible element, or any element without accessible content.
            if (!isNodeVisible(element.get()) || !modalElementHasAccessibleContent(*element))
                continue;

            lastVisible = element.get();
            if (focusedElement && focusedElement->isDescendantOf(*element)) {
                focusedElementIsOutsideModals = false;
                break;
            }
        }

        // If there is a focused element, and it's not inside any of the modals, we should
        // consider all modals inactive to allow the user to freely navigate.
        if (focusedElementIsOutsideModals && willRecomputeFocus == WillRecomputeFocus::No)
            return nullptr;
        return lastVisible.get();
    };

    auto* previousModal = m_currentModalElement.get();
    m_currentModalElement = recomputeModalElement();
    if (previousModal != m_currentModalElement.get()) {
        childrenChanged(rootWebArea());
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        // Because the presence of a modal affects every element on the page,
        // regenerate the entire isolated tree with the next cache update.
        m_deferredRegenerateIsolatedTree = true;
#endif
    }
}

bool AXObjectCache::isNodeVisible(Node* node) const
{
    if (!is<Element>(node))
        return false;

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return false;

    const auto& style = renderer->style();
    if (style.display() == DisplayType::None)
        return false;

    auto* renderLayer = renderer->enclosingLayer();
    if (style.visibility() != Visibility::Visible && renderLayer && !renderLayer->hasVisibleContent())
        return false;

    // Check whether this object or any of its ancestors has opacity 0.
    // The resulting opacity of a RenderObject is computed as the multiplication
    // of its opacity times the opacities of its ancestors.
    for (auto* renderObject = renderer; renderObject; renderObject = renderObject->parent()) {
        if (!renderObject->style().opacity())
            return false;
    }

    // We also need to consider aria hidden status.
    if (!isNodeAriaVisible(node))
        return false;

    return true;
}

// This function returns the valid aria modal node.
Node* AXObjectCache::modalNode()
{
    if (!m_modalNodesInitialized)
        findModalNodes();

    if (m_modalElements.isEmpty())
        return nullptr;

    // Check the cached current valid aria modal node first.
    // Usually when one dialog sets aria-modal=true, that dialog is the one we want.
    if (isNodeVisible(m_currentModalElement.get()))
        return m_currentModalElement.get();

    // Recompute the valid aria modal node when m_currentModalElement is null or hidden.
    updateCurrentModalNode();
    return m_currentModalElement.get();
}

AccessibilityObject* AXObjectCache::focusedImageMapUIElement(HTMLAreaElement* areaElement)
{
    // Find the corresponding accessibility object for the HTMLAreaElement. This should be
    // in the list of children for its corresponding image.
    if (!areaElement)
        return nullptr;
    
    HTMLImageElement* imageElement = areaElement->imageElement();
    if (!imageElement)
        return nullptr;
    
    AccessibilityObject* axRenderImage = areaElement->document().axObjectCache()->getOrCreate(imageElement);
    if (!axRenderImage)
        return nullptr;
    
    for (const auto& child : axRenderImage->children()) {
        if (!is<AccessibilityImageMapLink>(*child))
            continue;

        if (downcast<AccessibilityImageMapLink>(*child).areaElement() == areaElement)
            return downcast<AccessibilityImageMapLink>(child.get());
    }
    
    return nullptr;
}

AccessibilityObject* AXObjectCache::focusedObjectForPage(const Page* page)
{
    ASSERT(isMainThread());

    if (!gAccessibilityEnabled)
        return nullptr;

    // get the focused node in the page
    Document* document = page->focusController().focusedOrMainFrame().document();
    if (!document)
        return nullptr;

    document->updateStyleIfNeeded();

    Element* focusedElement = document->focusedElement();
    if (is<HTMLAreaElement>(focusedElement))
        return focusedImageMapUIElement(downcast<HTMLAreaElement>(focusedElement));

    auto* focus = getOrCreate(focusedElement ? focusedElement : static_cast<Node*>(document));
    if (!focus)
        return nullptr;

    if (focus->shouldFocusActiveDescendant()) {
        if (auto* descendant = focus->activeDescendant())
            focus = descendant;
    }

    // the HTML element, for example, is focusable but has an AX object that is ignored
    if (focus->accessibilityIsIgnored())
        focus = focus->parentObjectUnignored();

    return focus;
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::setIsolatedTreeFocusedObject(Node* focusedNode)
{
    ASSERT(isMainThread());
    if (!m_pageID)
        return;

    auto* focus = getOrCreate(focusedNode);

    if (auto tree = AXIsolatedTree::treeForPageID(*m_pageID))
        tree->setFocusedNodeID(focus ? focus->objectID() : AXID());
}
#endif

AccessibilityObject* AXObjectCache::get(Widget* widget)
{
    if (!widget)
        return nullptr;
        
    AXID axID = m_widgetObjectMapping.get(widget);
    ASSERT(!axID.isHashTableDeletedValue());
    if (!axID)
        return nullptr;
    
    return m_objects.get(axID);    
}
    
AccessibilityObject* AXObjectCache::get(RenderObject* renderer)
{
    if (!renderer)
        return nullptr;
    
    AXID axID = m_renderObjectMapping.get(renderer);
    ASSERT(!axID.isHashTableDeletedValue());
    if (!axID)
        return nullptr;

    return m_objects.get(axID);    
}

AccessibilityObject* AXObjectCache::get(Node* node)
{
    if (!node)
        return nullptr;

    AXID renderID = node->renderer() ? m_renderObjectMapping.get(node->renderer()) : AXID();
    ASSERT(!renderID.isHashTableDeletedValue());

    AXID nodeID = m_nodeObjectMapping.get(node);
    ASSERT(!nodeID.isHashTableDeletedValue());

    if (node->renderer() && nodeID && !renderID) {
        // This can happen if an AccessibilityNodeObject is created for a node that's not
        // rendered, but later something changes and it gets a renderer (like if it's
        // reparented).
        remove(nodeID);
        return nullptr;
    }

    if (renderID)
        return m_objects.get(renderID);

    if (!nodeID)
        return nullptr;

    return m_objects.get(nodeID);
}

// FIXME: This probably belongs on Node.
bool nodeHasRole(Node* node, StringView role)
{
    if (!node || !is<Element>(node))
        return false;

    auto& element = downcast<Element>(*node);
    AtomString roleValue = element.attributeWithoutSynchronization(roleAttr);
    if (roleValue.isNull()) {
        if (auto* defaultARIA = element.customElementDefaultARIAIfExists())
            roleValue = defaultARIA->valueForAttribute(element, roleAttr);
    }
    if (role.isNull())
        return roleValue.isEmpty();
    if (roleValue.isEmpty())
        return false;

    return SpaceSplitString::spaceSplitStringContainsValue(roleValue, role, SpaceSplitString::ShouldFoldCase::Yes);
}

static bool isSimpleImage(const RenderObject& renderer)
{
    if (!is<RenderImage>(renderer))
        return false;

    // Exclude ImageButtons because they are treated as buttons, not as images.
    auto* node = renderer.node();
    if (is<HTMLInputElement>(node))
        return false;

    // ImageMaps are not simple images.
    if (downcast<RenderImage>(renderer).imageMap()
        || (is<HTMLImageElement>(node) && downcast<HTMLImageElement>(node)->hasAttributeWithoutSynchronization(usemapAttr)))
        return false;

#if ENABLE(VIDEO)
    // Exclude video and audio elements.
    if (is<HTMLMediaElement>(node))
        return false;
#endif // ENABLE(VIDEO)

    return true;
}

static Ref<AccessibilityObject> createFromRenderer(RenderObject* renderer)
{
    // FIXME: How could renderer->node() ever not be an Element?
    Node* node = renderer->node();

    // If the node is aria role="list" or the aria role is empty and its a
    // ul/ol/dl type (it shouldn't be a list if aria says otherwise).
    if (node && ((nodeHasRole(node, "list"_s) || nodeHasRole(node, "directory"_s))
        || (nodeHasRole(node, nullAtom()) && (node->hasTagName(ulTag) || node->hasTagName(olTag) || node->hasTagName(dlTag)))))
        return AccessibilityList::create(renderer);

    // aria tables
    if (nodeHasRole(node, "grid"_s) || nodeHasRole(node, "treegrid"_s) || nodeHasRole(node, "table"_s))
        return AccessibilityARIAGrid::create(renderer);
    if (nodeHasRole(node, "row"_s))
        return AccessibilityARIAGridRow::create(renderer);
    if (nodeHasRole(node, "gridcell"_s) || nodeHasRole(node, "cell"_s) || nodeHasRole(node, "columnheader"_s) || nodeHasRole(node, "rowheader"_s))
        return AccessibilityARIAGridCell::create(renderer);

    // aria tree
    if (nodeHasRole(node, "tree"_s))
        return AccessibilityTree::create(renderer);
    if (nodeHasRole(node, "treeitem"_s))
        return AccessibilityTreeItem::create(renderer);

    if (node && is<HTMLLabelElement>(node) && nodeHasRole(node, nullAtom()))
        return AccessibilityLabel::create(renderer);

#if PLATFORM(IOS_FAMILY)
    if (is<HTMLMediaElement>(node) && nodeHasRole(node, nullAtom()))
        return AccessibilityMediaObject::create(renderer);
#endif

    if (renderer->isSVGRootOrLegacySVGRoot())
        return AccessibilitySVGRoot::create(renderer);
    
    if (is<SVGElement>(node))
        return AccessibilitySVGElement::create(renderer);

    if (isSimpleImage(*renderer))
        return AXImage::create(downcast<RenderImage>(renderer));

#if ENABLE(MATHML)
    // The mfenced element creates anonymous RenderMathMLOperators which should be treated
    // as MathML elements and assigned the MathElementRole so that platform logic regarding
    // inclusion and role mapping is not bypassed.
    bool isAnonymousOperator = renderer->isAnonymous() && is<RenderMathMLOperator>(*renderer);
    if (isAnonymousOperator || is<MathMLElement>(node))
        return AccessibilityMathMLElement::create(renderer, isAnonymousOperator);
#endif

    if (is<RenderListBox>(renderer))
        return AccessibilityListBox::create(renderer);
    if (is<RenderMenuList>(renderer))
        return AccessibilityMenuList::create(downcast<RenderMenuList>(renderer));

    // standard tables
    if (is<RenderTable>(renderer))
        return AccessibilityTable::create(renderer);
    if (is<RenderTableRow>(renderer))
        return AccessibilityTableRow::create(renderer);
    if (is<RenderTableCell>(renderer))
        return AccessibilityTableCell::create(renderer);

    // progress bar
    if (is<RenderProgress>(renderer) || is<HTMLProgressElement>(node))
        return AccessibilityProgressIndicator::create(renderer);

#if ENABLE(ATTACHMENT_ELEMENT)
    if (is<RenderAttachment>(renderer))
        return AccessibilityAttachment::create(downcast<RenderAttachment>(renderer));
#endif

    if (is<RenderMeter>(renderer) || is<HTMLMeterElement>(node))
        return AccessibilityProgressIndicator::create(renderer);

    // input type=range
    if (is<RenderSlider>(renderer))
        return AccessibilitySlider::create(renderer);

    return AccessibilityRenderObject::create(renderer);
}

static Ref<AccessibilityObject> createFromNode(Node* node)
{
    return AccessibilityNodeObject::create(node);
}

void AXObjectCache::cacheAndInitializeWrapper(AccessibilityObject* newObject, DOMObjectVariant domObject)
{
    ASSERT(newObject);
    AXID axID = getAXID(newObject);
    ASSERT(axID.isValid());

    WTF::switchOn(domObject,
        [&axID, this] (RenderObject* typedValue) { m_renderObjectMapping.set(typedValue, axID); },
        [&axID, this] (Node* typedValue) { m_nodeObjectMapping.set(typedValue, axID); },
        [&axID, this] (Widget* typedValue) { m_widgetObjectMapping.set(typedValue, axID); },
        [] (auto&) { }
    );

    m_objects.set(axID, newObject);
    newObject->init();
    attachWrapper(newObject);
}

AccessibilityObject* AXObjectCache::getOrCreate(Widget* widget)
{
    if (!widget)
        return nullptr;

    if (AccessibilityObject* obj = get(widget))
        return obj;
    
    RefPtr<AccessibilityObject> newObj;
    if (is<ScrollView>(*widget))
        newObj = AccessibilityScrollView::create(downcast<ScrollView>(widget));
    else if (is<Scrollbar>(*widget))
        newObj = AccessibilityScrollbar::create(downcast<Scrollbar>(widget));

    // Will crash later if we have two objects for the same widget.
    ASSERT(!get(widget));

    // Ensure we weren't given an unsupported widget type.
    ASSERT(newObj);
    if (!newObj)
        return nullptr;

    cacheAndInitializeWrapper(newObj.get(), widget);
    return newObj.get();
}

AccessibilityObject* AXObjectCache::getOrCreate(Node* node)
{
    if (!node)
        return nullptr;

    if (AccessibilityObject* obj = get(node))
        return obj;

    if (node->renderer())
        return getOrCreate(node->renderer());

    if (!node->parentElement())
        return nullptr;
    
    bool isOptionElement = is<HTMLOptionElement>(*node);
    if (isOptionElement || is<HTMLOptGroupElement>(*node)) {
        auto select = isOptionElement
            ? downcast<HTMLOptionElement>(*node).ownerSelectElement()
            : downcast<HTMLOptGroupElement>(*node).ownerSelectElement();
        if (!select)
            return nullptr;
        RefPtr<AccessibilityObject> object;
        if (select->usesMenuList()) {
            if (!isOptionElement)
                return nullptr;
            if (!select->renderer())
                return nullptr;
            object = AccessibilityMenuListOption::create(downcast<HTMLOptionElement>(*node));
        } else
            object = AccessibilityListBoxOption::create(downcast<HTMLElement>(*node));
        cacheAndInitializeWrapper(object.get(), node);
        return object.get();
    }

    bool inCanvasSubtree = lineageOfType<HTMLCanvasElement>(*node->parentElement()).first();
    bool insideMeterElement = is<HTMLMeterElement>(*node->parentElement());
    bool hasDisplayContents = is<Element>(*node) && downcast<Element>(*node).hasDisplayContents();
    if (!inCanvasSubtree && !insideMeterElement && !hasDisplayContents && !isNodeAriaVisible(node))
        return nullptr;

    Ref protectedNode { *node };

    // Fallback content is only focusable as long as the canvas is displayed and visible.
    // Update the style before Element::isFocusable() gets called.
    if (inCanvasSubtree)
        node->document().updateStyleIfNeeded();

    RefPtr<AccessibilityObject> newObj = createFromNode(node);

    // Will crash later if we have two objects for the same node.
    ASSERT(!get(node));

    cacheAndInitializeWrapper(newObj.get(), node);
    // Compute the object's initial ignored status.
    newObj->recomputeIsIgnored();
    // Sometimes asking accessibilityIsIgnored() will cause the newObject to be deallocated, and then
    // it will disappear when this function is finished, leading to a use-after-free.
    if (newObj->isDetached())
        return nullptr;
    
    return newObj.get();
}

AccessibilityObject* AXObjectCache::getOrCreate(RenderObject* renderer)
{
    if (!renderer)
        return nullptr;

    if (AccessibilityObject* obj = get(renderer))
        return obj;

    // Don't create an object for this renderer if it's being destroyed.
    if (renderer->beingDestroyed())
        return nullptr;
    RefPtr<AccessibilityObject> newObj = createFromRenderer(renderer);

    // Will crash later if we have two objects for the same renderer.
    ASSERT(!get(renderer));

    cacheAndInitializeWrapper(newObj.get(), renderer);
    // Compute the object's initial ignored status.
    newObj->recomputeIsIgnored();
    // Sometimes asking accessibilityIsIgnored() will cause the newObject to be deallocated, and then
    // it will disappear when this function is finished, leading to a use-after-free.
    if (newObj->isDetached())
        return nullptr;
    
    return newObj.get();
}

AXCoreObject* AXObjectCache::rootObject()
{
    if (!gAccessibilityEnabled)
        return nullptr;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (isIsolatedTreeEnabled())
        return isolatedTreeRootObject();
#endif

    return getOrCreate(m_document.view());
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
RefPtr<AXIsolatedTree> AXObjectCache::getOrCreateIsolatedTree() const
{
    RefPtr tree = AXIsolatedTree::treeForPageID(m_pageID);
    if (!tree) {
        tree = Accessibility::retrieveValueFromMainThread<RefPtr<AXIsolatedTree>>([this] () -> RefPtr<AXIsolatedTree> {
            return AXIsolatedTree::create(const_cast<AXObjectCache*>(this));
        });
        AXObjectCache::initializeSecondaryAXThread();
    }

    return tree;
}

AXCoreObject* AXObjectCache::isolatedTreeRootObject()
{
    if (auto tree = getOrCreateIsolatedTree())
        return tree->rootNode().get();

    // Should not get here, couldn't create the IsolatedTree.
    ASSERT_NOT_REACHED();
    return nullptr;
}
#endif

AccessibilityObject* AXObjectCache::rootObjectForFrame(Frame* frame)
{
    if (!gAccessibilityEnabled)
        return nullptr;

    if (!frame)
        return nullptr;
    return getOrCreate(frame->view());
}    
    
AccessibilityObject* AXObjectCache::create(AccessibilityRole role)
{
    RefPtr<AccessibilityObject> obj;

    // will be filled in...
    switch (role) {
    case AccessibilityRole::ImageMapLink:
        obj = AccessibilityImageMapLink::create();
        break;
    case AccessibilityRole::Column:
        obj = AccessibilityTableColumn::create();
        break;            
    case AccessibilityRole::TableHeaderContainer:
        obj = AccessibilityTableHeaderContainer::create();
        break;   
    case AccessibilityRole::SliderThumb:
        obj = AccessibilitySliderThumb::create();
        break;
    case AccessibilityRole::MenuListPopup:
        obj = AccessibilityMenuListPopup::create();
        break;
    case AccessibilityRole::SpinButton:
        obj = AccessibilitySpinButton::create();
        break;
    case AccessibilityRole::SpinButtonPart:
        obj = AccessibilitySpinButtonPart::create();
        break;
    default:
        obj = nullptr;
    }

    if (!obj)
        return nullptr;

    cacheAndInitializeWrapper(obj.get());
    return obj.get();
}

void AXObjectCache::remove(AXID axID)
{
    AXTRACE(makeString("AXObjectCache::remove 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    AXLOG(makeString("AXID ", axID.loggingString()));

    if (!axID)
        return;

    auto object = m_objects.take(axID);
    if (!object)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (auto tree = AXIsolatedTree::treeForPageID(m_pageID))
        tree->removeNode(*object);
#endif

    object->detach(AccessibilityDetachmentType::ElementDestroyed);

    m_idsInUse.remove(axID);
    ASSERT(m_objects.size() >= m_idsInUse.size());
}
    
void AXObjectCache::remove(RenderObject* renderer)
{
    if (!renderer)
        return;
    remove(m_renderObjectMapping.take(renderer));
}

void AXObjectCache::remove(Node& node)
{
    AXTRACE(makeString("AXObjectCache::remove 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));

    removeNodeForUse(node);
    remove(m_nodeObjectMapping.take(&node));
    remove(node.renderer());

    // If we're in the middle of a cache update, don't modify any of these vectors because we are currently
    // iterating over them. They will be cleared at the end of the cache update, so not removing them here is fine.
    if (m_performingDeferredCacheUpdate) {
        AXLOG("Bailing out before removing node from m_deferred* vectors as we are in the middle of a cache update.");
        return;
    }

    if (is<Element>(node)) {
        m_deferredTextFormControlValue.remove(downcast<Element>(&node));
        m_deferredAttributeChange.removeAllMatching([&node] (const auto& entry) {
            return entry.element == &node;
        });
        m_modalElements.removeAllMatching([&node] (const auto& element) {
            return downcast<Element>(&node) == element.get();
        });
        m_deferredRecomputeIsIgnoredList.remove(downcast<Element>(node));
        m_deferredRecomputeTableIsExposedList.remove(downcast<Element>(node));
        m_deferredSelectedChildredChangedList.remove(downcast<Element>(node));
        m_deferredModalChangedList.remove(downcast<Element>(node));
        m_deferredMenuListChange.remove(downcast<Element>(node));
    }
    m_deferredNodeAddedOrRemovedList.remove(&node);
    m_deferredTextChangedList.remove(&node);
    // Remove the entry if the new focused node is being removed.
    m_deferredFocusedNodeChange.removeAllMatching([&node](auto& entry) -> bool {
        return entry.second == &node;
    });
    // Set nullptr to the old focused node if it is being removed.
    std::for_each(m_deferredFocusedNodeChange.begin(), m_deferredFocusedNodeChange.end(), [&node](auto& entry) {
        if (entry.first == &node)
            entry.first = nullptr;
    });
}

void AXObjectCache::remove(Widget* view)
{
    if (!view)
        return;
    remove(m_widgetObjectMapping.take(view));
}
    
    
#if !PLATFORM(WIN)
AXID AXObjectCache::platformGenerateAXID() const
{
    AXID objID;
    do {
        objID = AXID::generate();
    } while (!objID.isValid() || m_idsInUse.contains(objID));
    return objID;
}
#endif

Vector<RefPtr<AXCoreObject>> AXObjectCache::objectsForIDs(const Vector<AXID>& axIDs) const
{
    ASSERT(isMainThread());

    Vector<RefPtr<AXCoreObject>> result;
    result.reserveInitialCapacity(axIDs.size());
    for (auto& axID : axIDs) {
        if (auto* object = objectForID(axID))
            result.uncheckedAppend(object);
    }
    result.shrinkToFit();
    return result;
}

AXID AXObjectCache::getAXID(AccessibilityObject* obj)
{
    // check for already-assigned ID
    AXID objID = obj->objectID();
    if (objID) {
        ASSERT(m_idsInUse.contains(objID));
        return objID;
    }

    objID = platformGenerateAXID();

    m_idsInUse.add(objID);
    obj->setObjectID(objID);
    
    return objID;
}

void AXObjectCache::handleTextChanged(AccessibilityObject* object)
{
    AXTRACE(makeString("AXObjectCache::handleTextChanged 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    AXLOG(object);

    if (!object)
        return;

    Ref<AccessibilityObject> protectedObject(*object);

    // If this element supports ARIA live regions, or is part of a region with an ARIA editable role,
    // then notify the AT of changes.
    bool notifiedNonNativeTextControl = false;
    for (auto* parent = object; parent; parent = parent->parentObject()) {
        if (parent->supportsLiveRegion())
            postLiveRegionChangeNotification(parent);

        if (!notifiedNonNativeTextControl && parent->isNonNativeTextControl()) {
            postNotification(parent, parent->document(), AXValueChanged);
            notifiedNonNativeTextControl = true;
        }
    }

    postNotification(object, object->document(), AXTextChanged);
    object->recomputeIsIgnored();
}

void AXObjectCache::updateCacheAfterNodeIsAttached(Node* node)
{
    // Calling get() will update the AX object if we had an AccessibilityNodeObject but now we need
    // an AccessibilityRenderObject, because it was reparented to a location outside of a canvas.
    get(node);
}

void AXObjectCache::updateLoadingProgress(double newProgressValue)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    ASSERT_WITH_MESSAGE(newProgressValue >= 0 && newProgressValue <= 1, "unexpected loading progress value: %f", newProgressValue);
    if (m_pageID) {
        // Sometimes the isolated tree hasn't been created by the time we get loading progress updates,
        // so cache this value in the AXObjectCache too so we can give it to the tree upon creation.
        m_loadingProgress = newProgressValue;
        if (auto tree = AXIsolatedTree::treeForPageID(*m_pageID))
            tree->updateLoadingProgress(newProgressValue);
    }
#else
    UNUSED_PARAM(newProgressValue);
#endif
}

void AXObjectCache::handleChildrenChanged(AccessibilityObject& object)
{
    // Handle MenuLists and MenuListPopups as special cases.
    if (is<AccessibilityMenuList>(object)) {
        auto& children = object.children(false);
        if (children.isEmpty())
            return;

        ASSERT(children.size() == 1 && is<AccessibilityObject>(*children[0]));
        handleChildrenChanged(downcast<AccessibilityObject>(*children[0]));
    } else if (is<AccessibilityMenuListPopup>(object)) {
        downcast<AccessibilityMenuListPopup>(object).handleChildrenChanged();
        return;
    }

    if (!object.node() && !object.renderer())
        return;

    // Should make the subtree dirty so that everything below will be updated correctly.
    object.setNeedsToUpdateSubtree();

    object.recomputeIsIgnored();

    // Go up the existing ancestors chain and fire the appropriate notifications.
    bool shouldUpdateParent = true;
    for (auto* parent = &object; parent; parent = parent->parentObjectIfExists()) {
        if (shouldUpdateParent)
            parent->setNeedsToUpdateChildren();

        // If this object supports ARIA live regions, then notify AT of changes.
        // This notification needs to be sent even when the screen reader has not accessed this live region since the last update.
        // Sometimes this function can be called many times within a short period of time, leading to posting too many AXLiveRegionChanged notifications.
        // To fix this, we use a timer to make sure we only post one notification for the children changes within a pre-defined time interval.
        if (parent->supportsLiveRegion())
            postLiveRegionChangeNotification(parent);

        // If this object is an ARIA text control, notify that its value changed.
        if (parent->isNonNativeTextControl()) {
            postNotification(parent, parent->document(), AXValueChanged);

            // Do not let any ancestor of an editable object update its children.
            shouldUpdateParent = false;
        }
    }

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(object, AXChildrenChanged);
#endif

    // The role of list objects is dependent on their children, so we'll need to re-compute it here.
    if (is<AccessibilityList>(object))
        object.updateRole();

    postPlatformNotification(&object, AXChildrenChanged);
}

void AXObjectCache::handleMenuOpened(Node* node)
{
    if (!node || !node->renderer() || !nodeHasRole(node, "menu"_s))
        return;
    
    postNotification(getOrCreate(node), &document(), AXMenuOpened);
}
    
void AXObjectCache::handleLiveRegionCreated(Node* node)
{
    if (!is<Element>(node) || !node->renderer())
        return;
    
    Element* element = downcast<Element>(node);
    auto liveRegionStatus = element->attributeWithoutSynchronization(aria_liveAttr);
    if (liveRegionStatus.isEmpty()) {
        const AtomString& ariaRole = element->attributeWithoutSynchronization(roleAttr);
        if (!ariaRole.isEmpty())
            liveRegionStatus = AtomString { AccessibilityObject::defaultLiveRegionStatusForRole(AccessibilityObject::ariaRoleToWebCoreRole(ariaRole)) };
    }
    
    if (AccessibilityObject::liveRegionStatusIsEnabled(liveRegionStatus))
        postNotification(getOrCreate(node), &document(), AXLiveRegionCreated);
}

void AXObjectCache::deferNodeAddedOrRemoved(Node* node)
{
    if (!node)
        return;

    m_deferredNodeAddedOrRemovedList.add(node);

    if (is<Element>(node)) {
        auto* changedElement = downcast<Element>(node);
        if (isModalElement(*changedElement))
            deferModalChange(changedElement);
    }

    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}

void AXObjectCache::childrenChanged(Node* node, Node* changedChild)
{
    childrenChanged(get(node));
    deferNodeAddedOrRemoved(changedChild);
}

void AXObjectCache::childrenChanged(RenderObject* renderer, RenderObject* changedChild)
{
    if (!renderer)
        return;

    childrenChanged(get(renderer));
    if (changedChild)
        deferNodeAddedOrRemoved(changedChild->node());
}

void AXObjectCache::childrenChanged(AccessibilityObject* object)
{
    if (!object)
        return;
    m_deferredChildrenChangedList.add(object);

    // Adding or removing rows from a table can cause it to change from layout table to AX data table and vice versa, so queue up recomputation of that for the parent table.
    if (auto* tableSectionElement = dynamicDowncast<HTMLTableSectionElement>(object->element()))
        deferRecomputeTableIsExposed(const_cast<HTMLTableElement*>(tableSectionElement->findParentTable().get()));

    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}

void AXObjectCache::valueChanged(Element* element)
{
    postNotification(element, AXNotification::AXValueChanged);
}

void AXObjectCache::notificationPostTimerFired()
{
    AXTRACE(makeString("AXObjectCache::notificationPostTimerFired 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    // During LayoutTests, accessibility may be disabled between the time the notifications are queued and the timer fires.
    // Thus check here and return if accessibility is disabled.
    if (!accessibilityEnabled())
        return;

    Ref<Document> protectorForCacheOwner(m_document);
    m_notificationPostTimer.stop();

    if (!m_document.hasLivingRenderTree())
        return;

    // In tests, posting notifications has a tendency to immediately queue up other notifications, which can lead to unexpected behavior
    // when the notification list is cleared at the end. Instead copy this list at the start.
    auto notifications = std::exchange(m_notificationsToPost, { });

    // Filter out the notifications that are not going to be posted to platform clients.
    Vector<std::pair<RefPtr<AccessibilityObject>, AXNotification>> notificationsToPost;
    notificationsToPost.reserveInitialCapacity(notifications.size());
    for (const auto& note : notifications) {
        ASSERT(note.first);
        if (note.first->isDetached() || !note.first->axObjectCache())
            continue;

#ifndef NDEBUG
        // Make sure none of the render views are in the process of being layed out.
        // Notifications should only be sent after the renderer has finished
        if (is<AccessibilityRenderObject>(*note.first)) {
            if (auto* renderer = downcast<AccessibilityRenderObject>(*note.first).renderer())
                ASSERT(!renderer->view().frameView().layoutContext().layoutState());
        }
#endif

        if (note.second == AXMenuOpened) {
            // Only notify if the object is in fact a menu.
            note.first->updateChildrenIfNecessary();
            if (note.first->roleValue() != AccessibilityRole::Menu)
                continue;
        }

        notificationsToPost.uncheckedAppend(note);
    }

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(notificationsToPost);
#endif

    for (const auto& note : notificationsToPost)
        postPlatformNotification(note.first.get(), note.second);
}

void AXObjectCache::passwordNotificationPostTimerFired()
{
#if PLATFORM(COCOA)
    m_passwordNotificationPostTimer.stop();

    // In tests, posting notifications has a tendency to immediately queue up other notifications, which can lead to unexpected behavior
    // when the notification list is cleared at the end. Instead copy this list at the start.
    auto notifications = std::exchange(m_passwordNotificationsToPost, { });

    for (auto& notification : notifications)
        postTextStateChangePlatformNotification(notification.get(), AXTextEditTypeInsert, " "_s, VisiblePosition());
#endif
}

void AXObjectCache::postNotification(RenderObject* renderer, AXNotification notification, PostTarget postTarget)
{
    if (!renderer)
        return;
    
    stopCachingComputedObjectAttributes();

    // Get an accessibility object that already exists. One should not be created here
    // because a render update may be in progress and creating an AX object can re-trigger a layout
    RefPtr<AccessibilityObject> object = get(renderer);
    while (!object && renderer) {
        renderer = renderer->parent();
        object = get(renderer); 
    }
    
    if (!renderer)
        return;
    
    postNotification(object.get(), &renderer->document(), notification, postTarget);
}

void AXObjectCache::postNotification(Node* node, AXNotification notification, PostTarget postTarget)
{
    if (!node)
        return;
    
    stopCachingComputedObjectAttributes();

    // Get an accessibility object that already exists. One should not be created here
    // because a render update may be in progress and creating an AX object can re-trigger a layout
    RefPtr<AccessibilityObject> object = get(node);
    while (!object && node) {
        node = node->parentNode();
        object = get(node);
    }
    
    if (!node)
        return;
    
    postNotification(object.get(), &node->document(), notification, postTarget);
}

void AXObjectCache::postNotification(AccessibilityObject* object, Document* document, AXNotification notification, PostTarget postTarget)
{
    AXTRACE(makeString("AXObjectCache::postNotification 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    AXLOG(std::make_pair(object, notification));
    ASSERT(isMainThread());

    stopCachingComputedObjectAttributes();

    if (object && postTarget == PostTarget::ObservableParent)
        object = object->observableObject();

    if (!object && document)
        object = get(document->renderView());

    if (!object)
        return;

    m_notificationsToPost.append(std::make_pair(object, notification));
    if (!m_notificationPostTimer.isActive())
        m_notificationPostTimer.startOneShot(0_s);
}

void AXObjectCache::checkedStateChanged(Node* node)
{
    postNotification(node, AXObjectCache::AXCheckedStateChanged);
}

void AXObjectCache::autofillTypeChanged(Node* node)
{
    postNotification(node, AXNotification::AXAutofillTypeChanged);
}

void AXObjectCache::handleMenuItemSelected(Node* node)
{
    if (!node)
        return;
    
    if (!nodeHasRole(node, "menuitem"_s) && !nodeHasRole(node, "menuitemradio"_s) && !nodeHasRole(node, "menuitemcheckbox"_s))
        return;
    
    if (!downcast<Element>(*node).focused() && !equalLettersIgnoringASCIICase(downcast<Element>(*node).attributeWithoutSynchronization(aria_selectedAttr), "true"_s))
        return;
    
    postNotification(getOrCreate(node), &document(), AXMenuListItemSelected);
}

void AXObjectCache::handleRowCountChanged(AccessibilityObject* axObject, Document* document)
{
    if (!axObject)
        return;

    if (auto* axTable = dynamicDowncast<AccessibilityTable>(axObject))
        axTable->recomputeIsExposable();

    postNotification(axObject, document, AXRowCountChanged);
}

void AXObjectCache::deferFocusedUIElementChangeIfNeeded(Node* oldNode, Node* newNode)
{
    if (nodeAndRendererAreValid(newNode) && rendererNeedsDeferredUpdate(*newNode->renderer())) {
        m_deferredFocusedNodeChange.append({ oldNode, newNode });
        if (!newNode->renderer()->needsLayout() && !m_performCacheUpdateTimer.isActive())
            m_performCacheUpdateTimer.startOneShot(0_s);
    } else
        handleFocusedUIElementChanged(oldNode, newNode);
}

void AXObjectCache::deferMenuListValueChange(Element* element)
{
    if (!element)
        return;

    m_deferredMenuListChange.add(*element);
    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}

void AXObjectCache::deferModalChange(Element* element)
{
    if (!element)
        return;

    m_deferredModalChangedList.add(*element);

    // Notify that parent's children have changed.
    if (auto* axParent = get(element->parentNode()))
        m_deferredChildrenChangedList.add(axParent);

    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}
    
void AXObjectCache::handleFocusedUIElementChanged(Node* oldNode, Node* newNode, UpdateModal updateModal)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    setIsolatedTreeFocusedObject(newNode);
#endif

    if (updateModal == UpdateModal::Yes)
        updateCurrentModalNode();
    handleMenuItemSelected(newNode);
    platformHandleFocusedUIElementChanged(oldNode, newNode);
}
    
void AXObjectCache::selectedChildrenChanged(Node* node)
{
    postNotification(node, AXSelectedChildrenChanged);
}

void AXObjectCache::selectedChildrenChanged(RenderObject* renderer)
{
    if (renderer)
        selectedChildrenChanged(renderer->node());
}

static bool isARIATableCell(Node* node)
{
    return node && (nodeHasRole(node, "gridcell"_s) || nodeHasRole(node, "cell"_s) || nodeHasRole(node, "columnheader"_s) || nodeHasRole(node, "rowheader"_s));
}

void AXObjectCache::onSelectedChanged(Node* node)
{
    if (isARIATableCell(node))
        postNotification(node, AXSelectedCellChanged);
    else if (is<HTMLOptionElement>(node))
        postNotification(node, AXSelectedStateChanged);
    else if (auto* axObject = getOrCreate(node)) {
        if (auto* ancestor = Accessibility::findAncestor<AccessibilityObject>(*axObject, false, [] (const auto& object) {
            return object.canHaveSelectedChildren();
        }))
            selectedChildrenChanged(ancestor->node());
    }

    handleMenuItemSelected(node);
}

void AXObjectCache::onTitleChange(Document& document)
{
    postNotification(get(&document), nullptr, AXTextChanged);
}

#ifndef NDEBUG
void AXObjectCache::showIntent(const AXTextStateChangeIntent &intent)
{
    switch (intent.type) {
    case AXTextStateChangeTypeUnknown:
        dataLog("Unknown");
        break;
    case AXTextStateChangeTypeEdit:
        dataLog("Edit::");
        break;
    case AXTextStateChangeTypeSelectionMove:
        dataLog("Move::");
        break;
    case AXTextStateChangeTypeSelectionExtend:
        dataLog("Extend::");
        break;
    case AXTextStateChangeTypeSelectionBoundary:
        dataLog("Boundary::");
        break;
    }
    switch (intent.type) {
    case AXTextStateChangeTypeUnknown:
        break;
    case AXTextStateChangeTypeEdit:
        switch (intent.change) {
        case AXTextEditTypeUnknown:
            dataLog("Unknown");
            break;
        case AXTextEditTypeDelete:
            dataLog("Delete");
            break;
        case AXTextEditTypeInsert:
            dataLog("Insert");
            break;
        case AXTextEditTypeDictation:
            dataLog("DictationInsert");
            break;
        case AXTextEditTypeTyping:
            dataLog("TypingInsert");
            break;
        case AXTextEditTypeCut:
            dataLog("Cut");
            break;
        case AXTextEditTypePaste:
            dataLog("Paste");
            break;
        case AXTextEditTypeAttributesChange:
            dataLog("AttributesChange");
            break;
        }
        break;
    case AXTextStateChangeTypeSelectionMove:
    case AXTextStateChangeTypeSelectionExtend:
    case AXTextStateChangeTypeSelectionBoundary:
        switch (intent.selection.direction) {
        case AXTextSelectionDirectionUnknown:
            dataLog("Unknown::");
            break;
        case AXTextSelectionDirectionBeginning:
            dataLog("Beginning::");
            break;
        case AXTextSelectionDirectionEnd:
            dataLog("End::");
            break;
        case AXTextSelectionDirectionPrevious:
            dataLog("Previous::");
            break;
        case AXTextSelectionDirectionNext:
            dataLog("Next::");
            break;
        case AXTextSelectionDirectionDiscontiguous:
            dataLog("Discontiguous::");
            break;
        }
        switch (intent.selection.direction) {
        case AXTextSelectionDirectionUnknown:
        case AXTextSelectionDirectionBeginning:
        case AXTextSelectionDirectionEnd:
        case AXTextSelectionDirectionPrevious:
        case AXTextSelectionDirectionNext:
            switch (intent.selection.granularity) {
            case AXTextSelectionGranularityUnknown:
                dataLog("Unknown");
                break;
            case AXTextSelectionGranularityCharacter:
                dataLog("Character");
                break;
            case AXTextSelectionGranularityWord:
                dataLog("Word");
                break;
            case AXTextSelectionGranularityLine:
                dataLog("Line");
                break;
            case AXTextSelectionGranularitySentence:
                dataLog("Sentence");
                break;
            case AXTextSelectionGranularityParagraph:
                dataLog("Paragraph");
                break;
            case AXTextSelectionGranularityPage:
                dataLog("Page");
                break;
            case AXTextSelectionGranularityDocument:
                dataLog("Document");
                break;
            case AXTextSelectionGranularityAll:
                dataLog("All");
                break;
            }
            break;
        case AXTextSelectionDirectionDiscontiguous:
            break;
        }
        break;
    }
    dataLog("\n");
}
#endif

void AXObjectCache::setTextSelectionIntent(const AXTextStateChangeIntent& intent)
{
    m_textSelectionIntent = intent;
}
    
void AXObjectCache::setIsSynchronizingSelection(bool isSynchronizing)
{
    m_isSynchronizingSelection = isSynchronizing;
}

#if PLATFORM(COCOA)
static bool isPasswordFieldOrContainedByPasswordField(AccessibilityObject* object)
{
    return object && (object->isPasswordField() || object->isContainedByPasswordField());
}
#endif

void AXObjectCache::postTextStateChangeNotification(Node* node, const AXTextStateChangeIntent& intent, const VisibleSelection& selection)
{
    if (!node)
        return;

#if PLATFORM(COCOA) || USE(ATSPI)
    stopCachingComputedObjectAttributes();

    postTextStateChangeNotification(getOrCreate(node), intent, selection);
#else
    postNotification(node->renderer(), AXObjectCache::AXSelectedTextChanged, PostTarget::ObservableParent);
    UNUSED_PARAM(intent);
    UNUSED_PARAM(selection);
#endif
}

void AXObjectCache::postTextStateChangeNotification(const Position& position, const AXTextStateChangeIntent& intent, const VisibleSelection& selection)
{
    Node* node = position.deprecatedNode();
    if (!node)
        return;

    stopCachingComputedObjectAttributes();

#if PLATFORM(COCOA) || USE(ATSPI)
    AccessibilityObject* object = getOrCreate(node);
    if (object && object->accessibilityIsIgnored()) {
#if PLATFORM(COCOA)
        if (position.atLastEditingPositionForNode()) {
            if (AccessibilityObject* nextSibling = object->nextSiblingUnignored(1))
                object = nextSibling;
        } else if (position.atFirstEditingPositionForNode()) {
            if (AccessibilityObject* previousSibling = object->previousSiblingUnignored(1))
                object = previousSibling;
        }
#elif USE(ATSPI)
        // ATSPI doesn't expose text nodes, so we need the parent
        // object which is the one implementing the text interface.
        object = object->parentObjectUnignored();
#endif
    }

    postTextStateChangeNotification(object, intent, selection);
#else
    postTextStateChangeNotification(node, intent, selection);
#endif
}

void AXObjectCache::postTextStateChangeNotification(AccessibilityObject* object, const AXTextStateChangeIntent& intent, const VisibleSelection& selection)
{
    AXTRACE(makeString("AXObjectCache::postTextStateChangeNotification 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    stopCachingComputedObjectAttributes();

#if PLATFORM(COCOA) || USE(ATSPI)
    if (object) {
#if PLATFORM(COCOA)
        if (isPasswordFieldOrContainedByPasswordField(object))
            return;
#endif
        if (auto observableObject = object->observableObject())
            object = observableObject;
    }

    if (!object)
        object = rootWebArea();

    if (object) {
        const AXTextStateChangeIntent& newIntent = (intent.type == AXTextStateChangeTypeUnknown || (m_isSynchronizingSelection && m_textSelectionIntent.type != AXTextStateChangeTypeUnknown)) ? m_textSelectionIntent : intent;
        postTextStateChangePlatformNotification(object, newIntent, selection);
    }
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(intent);
    UNUSED_PARAM(selection);
#endif

    setTextSelectionIntent(AXTextStateChangeIntent());
    setIsSynchronizingSelection(false);
}

void AXObjectCache::postTextStateChangeNotification(Node* node, AXTextEditType type, const String& text, const VisiblePosition& position)
{
    AXTRACE(makeString("AXObjectCache::postTextStateChangeNotification 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    if (!node || type == AXTextEditTypeUnknown)
        return;

    stopCachingComputedObjectAttributes();

    AccessibilityObject* object = getOrCreate(node);
#if PLATFORM(COCOA) || USE(ATSPI)
    if (object) {
        if (enqueuePasswordValueChangeNotification(object))
            return;
        object = object->observableObject();
    }

    if (!object)
        object = rootWebArea();

    if (!object)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(*object, AXValueChanged);
#endif

    postTextStateChangePlatformNotification(object, type, text, position);
#else
    nodeTextChangePlatformNotification(object, textChangeForEditType(type), position.deepEquivalent().deprecatedEditingOffset(), text);
#endif
}

void AXObjectCache::postTextReplacementNotification(Node* node, AXTextEditType deletionType, const String& deletedText, AXTextEditType insertionType, const String& insertedText, const VisiblePosition& position)
{
    if (!node)
        return;
    if (deletionType != AXTextEditTypeDelete)
        return;
    if (!(insertionType == AXTextEditTypeInsert || insertionType == AXTextEditTypeTyping || insertionType == AXTextEditTypeDictation || insertionType == AXTextEditTypePaste))
        return;

    stopCachingComputedObjectAttributes();

    AccessibilityObject* object = getOrCreate(node);
#if PLATFORM(COCOA) || USE(ATSPI)
    if (object) {
        if (enqueuePasswordValueChangeNotification(object))
            return;
        object = object->observableObject();
    }

    postTextReplacementPlatformNotification(object, deletionType, deletedText, insertionType, insertedText, position);
#else
    nodeTextChangePlatformNotification(object, textChangeForEditType(deletionType), position.deepEquivalent().deprecatedEditingOffset(), deletedText);
    nodeTextChangePlatformNotification(object, textChangeForEditType(insertionType), position.deepEquivalent().deprecatedEditingOffset(), insertedText);
#endif
}

void AXObjectCache::postTextReplacementNotificationForTextControl(HTMLTextFormControlElement& textControl, const String& deletedText, const String& insertedText)
{
    stopCachingComputedObjectAttributes();

    AccessibilityObject* object = getOrCreate(&textControl);
#if PLATFORM(COCOA) || USE(ATSPI)
    if (object) {
        if (enqueuePasswordValueChangeNotification(object))
            return;
        object = object->observableObject();
    }

    postTextReplacementPlatformNotificationForTextControl(object, deletedText, insertedText, textControl);
#else
    nodeTextChangePlatformNotification(object, textChangeForEditType(AXTextEditTypeDelete), 0, deletedText);
    nodeTextChangePlatformNotification(object, textChangeForEditType(AXTextEditTypeInsert), 0, insertedText);
#endif
}

bool AXObjectCache::enqueuePasswordValueChangeNotification(AccessibilityObject* object)
{
#if PLATFORM(COCOA)
    if (!isPasswordFieldOrContainedByPasswordField(object))
        return false;

    AccessibilityObject* observableObject = object->observableObject();
    if (!observableObject) {
        ASSERT_NOT_REACHED();
        // return true even though the enqueue didn't happen because this is a password field and caller shouldn't post a notification
        return true;
    }

    m_passwordNotificationsToPost.add(observableObject);
    if (!m_passwordNotificationPostTimer.isActive())
        m_passwordNotificationPostTimer.startOneShot(accessibilityPasswordValueChangeNotificationInterval);

    return true;
#else
    UNUSED_PARAM(object);
    return false;
#endif
}

void AXObjectCache::frameLoadingEventNotification(Frame* frame, AXLoadingEvent loadingEvent)
{
    if (!frame)
        return;

    // Delegate on the right platform
    RenderView* contentRenderer = frame->contentRenderer();
    if (!contentRenderer)
        return;

    AccessibilityObject* obj = getOrCreate(contentRenderer);
    frameLoadingEventPlatformNotification(obj, loadingEvent);
}

void AXObjectCache::postLiveRegionChangeNotification(AccessibilityObject* object)
{
    if (m_liveRegionChangedPostTimer.isActive())
        m_liveRegionChangedPostTimer.stop();

    if (!m_liveRegionObjectsSet.contains(object))
        m_liveRegionObjectsSet.add(object);

    m_liveRegionChangedPostTimer.startOneShot(0_s);
}

void AXObjectCache::liveRegionChangedNotificationPostTimerFired()
{
    m_liveRegionChangedPostTimer.stop();

    if (m_liveRegionObjectsSet.isEmpty())
        return;

    for (auto& object : m_liveRegionObjectsSet)
        postNotification(object.get(), object->document(), AXObjectCache::AXLiveRegionChanged);
    m_liveRegionObjectsSet.clear();
}

static AccessibilityObject* firstFocusableChild(AccessibilityObject* obj)
{
    if (!obj)
        return nullptr;
    
    for (auto* child = obj->firstChild(); child; child = child->nextSibling()) {
        if (child->canSetFocusAttribute())
            return child;
        if (AccessibilityObject* focusable = firstFocusableChild(child))
            return focusable;
    }
    return nullptr;
}

void AXObjectCache::focusCurrentModal()
{
    if (!m_document.hasLivingRenderTree())
        return;

    Ref<Document> protectedDocument(m_document);
    if (!nodeAndRendererAreValid(m_currentModalElement.get()) || !isNodeVisible(m_currentModalElement.get()))
        return;

    // Don't focus the current modal if focus has been requested to be put elsewhere (e.g. via JS).
    if (!m_deferredFocusedNodeChange.isEmpty())
        return;
    
    // Don't set focus if we are already focusing onto some element within
    // the dialog.
    if (m_currentModalElement->contains(document().focusedElement()))
        return;
    
    if (AccessibilityObject* currentModalNodeObject = getOrCreate(m_currentModalElement.get())) {
        if (AccessibilityObject* focusable = firstFocusableChild(currentModalNodeObject))
            focusable->setFocused(true);
    }
}

void AXObjectCache::handleScrollbarUpdate(ScrollView* view)
{
    if (!view)
        return;
    
    // We don't want to create a scroll view from this method, only update an existing one.
    if (AccessibilityObject* scrollViewObject = get(view)) {
        stopCachingComputedObjectAttributes();
        scrollViewObject->updateChildrenIfNecessary();
    }
}

void AXObjectCache::handleAriaExpandedChange(Node* node)
{
    // An aria-expanded change can cause two notifications to be posted:
    // RowCountChanged for the tree or table ancestor of this object, and
    // RowExpanded/Collapsed for this object.
    if (RefPtr object = get(node)) {
        // Find the ancestor that supports RowCountChanged if exists.
        auto* ancestor = Accessibility::findAncestor<AccessibilityObject>(*object, false, [] (auto& candidate) {
            return candidate.supportsRowCountChange();
        });

        // Post that the ancestor's row count changed.
        if (ancestor)
            handleRowCountChanged(ancestor, &document());

        // Post that the specific row either collapsed or expanded.
        auto role = object->roleValue();
        if (role == AccessibilityRole::Row || role == AccessibilityRole::TreeItem)
            postNotification(object.get(), &document(), object->isExpanded() ? AXRowExpanded : AXRowCollapsed);
        else
            postNotification(object.get(), &document(), AXExpandedChanged);
    }
}

void AXObjectCache::handleActiveDescendantChanged(Element& element)
{
    if (!document().frame()->selection().isFocusedAndActive() || document().focusedElement() != &element)
        return;

    auto* object = getOrCreate(&element);
    if (!object)
        return;

    auto* activeDescendant = object->activeDescendant();
    // We want to notify that the combo box has changed its active descendant,
    // but we do not want to change the focus, because focus should remain with the combo box.
    if (activeDescendant && (object->isComboBox() || object->shouldFocusActiveDescendant())) {
        auto target = object;

#if PLATFORM(COCOA)
        // If the combobox's activeDescendant is inside a descendant owned or controlled by the combobox, that descendant should be the target of the notification and not the combobox itself.
        if (object->isComboBox()) {
            if (auto* ownedObject = Accessibility::findRelatedObjectInAncestry(*object, AXRelationType::OwnerFor, *activeDescendant))
                target = ownedObject;
            else if (auto* controlledObject = Accessibility::findRelatedObjectInAncestry(*object, AXRelationType::ControllerFor, *activeDescendant))
                target = controlledObject;
        }
#endif

        postNotification(target, &document(), AXActiveDescendantChanged);
    }
}

static bool isTableOrRowRole(const AtomString& attrValue)
{
    return attrValue == "table"_s
        || attrValue == "grid"_s
        || attrValue == "treegrid"_s
        || attrValue == "row"_s;
}

void AXObjectCache::handleRoleChanged(Element* element, const AtomString& oldValue, const AtomString& newValue)
{
    AXTRACE("AXObjectCache::handleRoleChanged"_s);
    AXLOG(makeString("oldValue ", oldValue, " new value ", newValue));
    ASSERT(oldValue != newValue);

    auto* object = get(element);
    if (!object)
        return;

    // The class of an AX object created for an Element depends on the role attribute of that Element.
    // Thus when the role changes, remove the existing AX object and force a ChildrenChanged on the parent so that the object is re-created.
    // At the moment this is done only for table and row roles. Other roles may be added here if needed.
    if (oldValue.isEmpty() || isTableOrRowRole(oldValue)
        || newValue.isEmpty() || isTableOrRowRole(newValue)) {
        if (auto* parent = object->parentObject()) {
            remove(*element);
            childrenChanged(parent);
            return;
        }
    }

    object->updateRole();
}

void AXObjectCache::handleRoleChanged(AccessibilityObject* axObject)
{
    stopCachingComputedObjectAttributes();
    axObject->recomputeIsIgnored();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(axObject, AXNotification::AXRoleChanged);
#endif
}

void AXObjectCache::handleRoleDescriptionChanged(Element* element)
{
    auto* object = get(element);
    if (!object)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(object, AXNotification::AXRoleDescriptionChanged);
#endif
}

void AXObjectCache::deferAttributeChangeIfNeeded(Element* element, const QualifiedName& attrName, const AtomString& oldValue, const AtomString& newValue)
{
    AXTRACE(makeString("AXObjectCache::deferAttributeChangeIfNeeded 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));

    if (nodeAndRendererAreValid(element) && rendererNeedsDeferredUpdate(*element->renderer())) {
        m_deferredAttributeChange.append({ element, attrName, oldValue, newValue });
        if (!m_performCacheUpdateTimer.isActive())
            m_performCacheUpdateTimer.startOneShot(0_s);
        AXLOG(makeString("Deferring handling of attribute ", attrName.localName().string(), " for element ", element->debugDescription()));
        return;
    }
    handleAttributeChange(element, attrName, oldValue, newValue);
}

bool AXObjectCache::shouldProcessAttributeChange(Element* element, const QualifiedName& attrName)
{
    if (!element)
        return false;

    // aria-modal ends up affecting sub-trees that are being shown/hidden so it's likely that
    // an AT would not have accessed this node yet.
    if (attrName == aria_modalAttr)
        return true;

    // If an AXObject has yet to be created, then there's no need to process attribute changes.
    // Some of these notifications are processed on the parent, so allow that to proceed as well
    return get(element) || get(element->parentNode());
}

void AXObjectCache::handleAttributeChange(Element* element, const QualifiedName& attrName, const AtomString& oldValue, const AtomString& newValue)
{
    AXTRACE(makeString("AXObjectCache::handleAttributeChange 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    AXLOG(makeString("attribute ", attrName.localName().string(), " for element ", element ? element->debugDescription() : String("nullptr"_s)));

    if (!shouldProcessAttributeChange(element, attrName))
        return;

    if (relationAttributes().contains(attrName))
        relationsNeedUpdate(true);

    if (attrName == roleAttr)
        handleRoleChanged(element, oldValue, newValue);
    else if (attrName == altAttr || attrName == titleAttr)
        handleTextChanged(getOrCreate(element));
    else if (attrName == contenteditableAttr) {
        if (auto* axObject = get(element))
            axObject->updateRole();
    }
    else if (attrName == disabledAttr)
        postNotification(element, AXObjectCache::AXDisabledStateChanged);
    else if (attrName == forAttr && is<HTMLLabelElement>(*element))
        labelChanged(element);
    else if (attrName == requiredAttr)
        postNotification(element, AXRequiredStatusChanged);
    else if (attrName == tabindexAttr)
        childrenChanged(element->parentNode(), element);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    else if (attrName == headersAttr)
        updateIsolatedTree(get(element), AXTableHeadersChanged);
    else if (attrName == langAttr)
        updateIsolatedTree(get(element), AXObjectCache::AXLanguageChanged);
    else if (attrName == placeholderAttr)
        postNotification(element, AXPlaceholderChanged);
    else if (attrName == hrefAttr)
        updateIsolatedTree(get(element), AXURLChanged);
    else if (attrName == idAttr) {
        relationsNeedUpdate(true);
#if !LOG_DISABLED
        updateIsolatedTree(get(element), AXIdAttributeChanged);
#endif
    }
#endif
    else if (attrName == openAttr && is<HTMLDialogElement>(*element)) {
        deferModalChange(element);
        recomputeIsIgnored(element->parentNode());
    }

    if (!attrName.localName().string().startsWith("aria-"_s))
        return;

    auto recomputeParentTableExposure = [this] (Element* element) {
        if (auto* tablePartElement = dynamicDowncast<HTMLTablePartElement>(element))
            deferRecomputeTableIsExposed(const_cast<HTMLTableElement*>(tablePartElement->findParentTable().get()));
    };

    if (attrName == aria_activedescendantAttr)
        handleActiveDescendantChanged(*element);
    else if (attrName == aria_atomicAttr)
        postNotification(element, AXIsAtomicChanged);
    else if (attrName == aria_busyAttr)
        postNotification(element, AXObjectCache::AXElementBusyChanged);
    else if (attrName == aria_valuenowAttr || attrName == aria_valuetextAttr)
        postNotification(element, AXObjectCache::AXValueChanged);
    else if (attrName == aria_labelAttr || attrName == aria_labeledbyAttr || attrName == aria_labelledbyAttr)
        handleTextChanged(getOrCreate(element));
    else if (attrName == aria_checkedAttr)
        checkedStateChanged(element);
    else if (attrName == aria_colcountAttr) {
        postNotification(element, AXColumnCountChanged);
        deferRecomputeTableIsExposed(dynamicDowncast<HTMLTableElement>(element));
    } else if (attrName == aria_colindexAttr) {
        postNotification(element, AXColumnIndexChanged);
        recomputeParentTableExposure(element);
    } else if (attrName == aria_colspanAttr) {
        postNotification(element, AXColumnSpanChanged);
        recomputeParentTableExposure(element);
    }
    else if (attrName == aria_describedbyAttr)
        postNotification(element, AXDescribedByChanged);
    else if (attrName == aria_dropeffectAttr)
        postNotification(element, AXDropEffectChanged);
    else if (attrName == aria_flowtoAttr)
        postNotification(element, AXFlowToChanged);
    else if (attrName == aria_grabbedAttr)
        postNotification(element, AXGrabbedStateChanged);
    else if (attrName == aria_levelAttr)
        postNotification(element, AXLevelChanged);
    else if (attrName == aria_liveAttr)
        postNotification(element, AXLiveRegionStatusChanged);
    else if (attrName == aria_placeholderAttr)
        postNotification(element, AXPlaceholderChanged);
    else if (attrName == aria_rowindexAttr) {
        postNotification(element, AXRowIndexChanged);
        recomputeParentTableExposure(element);
    }
    else if (attrName == aria_valuemaxAttr)
        postNotification(element, AXMaximumValueChanged);
    else if (attrName == aria_valueminAttr)
        postNotification(element, AXMinimumValueChanged);
    else if (attrName == aria_multilineAttr) {
        if (auto* axObject = get(element)) {
            // The role of textarea and textfield objects is dependent on whether they can span multiple lines, so recompute it here.
            if (axObject->roleValue() == AccessibilityRole::TextArea || axObject->roleValue() == AccessibilityRole::TextField)
                axObject->updateRole();
        }
    }
    else if (attrName == aria_multiselectableAttr)
        postNotification(element, AXMultiSelectableStateChanged);
    else if (attrName == aria_orientationAttr)
        postNotification(element, AXOrientationChanged);
    else if (attrName == aria_posinsetAttr)
        postNotification(element, AXPositionInSetChanged);
    else if (attrName == aria_relevantAttr)
        postNotification(element, AXLiveRegionRelevantChanged);
    else if (attrName == aria_selectedAttr)
        onSelectedChanged(element);
    else if (attrName == aria_setsizeAttr)
        postNotification(element, AXSetSizeChanged);
    else if (attrName == aria_expandedAttr)
        handleAriaExpandedChange(element);
    else if (attrName == aria_haspopupAttr)
        postNotification(element, AXHasPopupChanged);
    else if (attrName == aria_hiddenAttr) {
        if (auto* parent = get(element->parentNode()))
            handleChildrenChanged(*parent);

        if (m_currentModalElement && m_currentModalElement->isDescendantOf(element)) {
            m_modalNodesInitialized = false;
            deferModalChange(m_currentModalElement.get());
        }
    }
    else if (attrName == aria_invalidAttr)
        postNotification(element, AXObjectCache::AXInvalidStatusChanged);
    else if (attrName == aria_modalAttr)
        deferModalChange(element);
    else if (attrName == aria_currentAttr)
        postNotification(element, AXObjectCache::AXCurrentStateChanged);
    else if (attrName == aria_disabledAttr)
        postNotification(element, AXObjectCache::AXDisabledStateChanged);
    else if (attrName == aria_pressedAttr)
        postNotification(element, AXObjectCache::AXPressedStateChanged);
    else if (attrName == aria_readonlyAttr)
        postNotification(element, AXObjectCache::AXReadOnlyStatusChanged);
    else if (attrName == aria_requiredAttr)
        postNotification(element, AXObjectCache::AXRequiredStatusChanged);
    else if (attrName == aria_roledescriptionAttr)
        handleRoleDescriptionChanged(element);
    else if (attrName == aria_rowcountAttr)
        handleRowCountChanged(get(element), element ? &element->document() : nullptr);
    else if (attrName == aria_rowspanAttr) {
        postNotification(element, AXRowSpanChanged);
        recomputeParentTableExposure(element);
    }
    else if (attrName == aria_sortAttr)
        postNotification(element, AXObjectCache::AXSortDirectionChanged);
}

void AXObjectCache::labelChanged(Element* element)
{
    ASSERT(is<HTMLLabelElement>(*element));
    auto correspondingControl = downcast<HTMLLabelElement>(*element).control();
    deferTextChangedIfNeeded(correspondingControl.get());
}

void AXObjectCache::recomputeIsIgnored(RenderObject* renderer)
{
    if (auto* object = get(renderer))
        object->recomputeIsIgnored();
}

void AXObjectCache::recomputeIsIgnored(Node* node)
{
    if (auto* object = get(node))
        object->recomputeIsIgnored();
}

void AXObjectCache::startCachingComputedObjectAttributesUntilTreeMutates()
{
    if (!m_computedObjectAttributeCache)
        m_computedObjectAttributeCache = makeUnique<AXComputedObjectAttributeCache>();
}

void AXObjectCache::stopCachingComputedObjectAttributes()
{
    m_computedObjectAttributeCache = nullptr;
}

VisiblePosition AXObjectCache::visiblePositionForTextMarkerData(TextMarkerData& textMarkerData)
{
    if (!isNodeInUse(textMarkerData.node)
        || textMarkerData.node->isPseudoElement())
        return { };

    auto visiblePosition = VisiblePosition({ textMarkerData.node, textMarkerData.offset, textMarkerData.anchorType }, textMarkerData.affinity);
    auto deepPosition = visiblePosition.deepEquivalent();
    if (deepPosition.isNull())
        return { };

    auto* renderer = deepPosition.deprecatedNode()->renderer();
    if (!renderer)
        return { };

    auto* cache = renderer->document().axObjectCache();
    if (cache && !cache->m_idsInUse.contains(textMarkerData.axID))
        return { };

    return visiblePosition;
}

CharacterOffset AXObjectCache::characterOffsetForTextMarkerData(TextMarkerData& textMarkerData)
{
    if (!isNodeInUse(textMarkerData.node))
        return CharacterOffset();
    
    if (textMarkerData.ignored)
        return CharacterOffset();
    
    CharacterOffset result = CharacterOffset(textMarkerData.node, textMarkerData.characterStartIndex, textMarkerData.characterOffset);
    // When we are at a line wrap and the VisiblePosition is upstream, it means the text marker is at the end of the previous line.
    // We use the previous CharacterOffset so that it will match the Range.
    if (textMarkerData.affinity == Affinity::Upstream)
        return previousCharacterOffset(result, false);
    return result;
}

CharacterOffset AXObjectCache::traverseToOffsetInRange(const SimpleRange& range, int offset, TraverseOption option, bool stayWithinRange)
{
    bool toNodeEnd = option & TraverseOptionToNodeEnd;
    bool validateOffset = option & TraverseOptionValidateOffset;
    bool doNotEnterTextControls = option & TraverseOptionDoNotEnterTextControls;
    
    int offsetInCharacter = 0;
    int cumulativeOffset = 0;
    int remaining = 0;
    int lastLength = 0;
    Node* currentNode = nullptr;
    bool finished = false;
    int lastStartOffset = 0;
    
    TextIteratorBehaviors behaviors;
    if (!doNotEnterTextControls)
        behaviors.add(TextIteratorBehavior::EntersTextControls);
    TextIterator iterator(range, behaviors);
    
    // When the range has zero length, there might be replaced node or brTag that we need to increment the characterOffset.
    if (iterator.atEnd()) {
        currentNode = range.start.container.ptr();
        lastStartOffset = range.start.offset;
        if (offset > 0 || toNodeEnd) {
            if (AccessibilityObject::replacedNodeNeedsCharacter(currentNode) || (currentNode->renderer() && currentNode->renderer()->isBR()))
                cumulativeOffset++;
            lastLength = cumulativeOffset;
            
            // When going backwards, stayWithinRange is false.
            // Here when we don't have any character to move and we are going backwards, we traverse to the previous node.
            if (!lastLength && toNodeEnd && !stayWithinRange) {
                if (Node* preNode = previousNode(currentNode))
                    return traverseToOffsetInRange(rangeForNodeContents(*preNode), offset, option);
                return CharacterOffset();
            }
        }
    }
    
    // Sometimes text contents in a node are split into several iterations, so that iterator.range().startOffset()
    // might not be the total character count. Here we use a previousNode object to keep track of that.
    Node* previousNode = nullptr;
    for (; !iterator.atEnd(); iterator.advance()) {
        int currentLength = iterator.text().length();
        bool hasReplacedNodeOrBR = false;

        Node& node = iterator.range().start.container;
        currentNode = &node;
        
        // When currentLength == 0, we check if there's any replaced node.
        // If not, we skip the node with no length.
        if (!currentLength) {
            Node* childNode = iterator.node();
            if (AccessibilityObject::replacedNodeNeedsCharacter(childNode)) {
                cumulativeOffset++;
                currentLength++;
                currentNode = childNode;
                hasReplacedNodeOrBR = true;
            } else
                continue;
        } else {
            // Ignore space, new line, tag node.
            if (currentLength == 1) {
                if (isHTMLSpace(iterator.text()[0])) {
                    // If the node has BR tag, we want to set the currentNode to it.
                    Node* childNode = iterator.node();
                    if (childNode && childNode->renderer() && childNode->renderer()->isBR()) {
                        currentNode = childNode;
                        hasReplacedNodeOrBR = true;
                    } else if (auto* shadowHost = currentNode->shadowHost()) {
                        // Since we are entering text controls, we should set the currentNode
                        // to be the shadow host when there's no content.
                        if (nodeIsTextControl(shadowHost) && currentNode->isShadowRoot()) {
                            currentNode = shadowHost;
                            continue;
                        }
                    } else if (previousNode && previousNode->isTextNode() && previousNode->isDescendantOf(currentNode) && currentNode->hasTagName(pTag)) {
                        // TextIterator is emitting an extra newline after the <p> element. We should
                        // ignore that since the extra text node is not in the DOM tree.
                        currentNode = previousNode;
                        continue;
                    } else if (currentNode != previousNode) {
                        // We should set the start offset and length for the current node in case this is the last iteration.
                        lastStartOffset = 1;
                        lastLength = 0;
                        continue;
                    }
                }
            }
            cumulativeOffset += currentLength;
        }

        if (currentNode == previousNode) {
            lastLength += currentLength;
            lastStartOffset = iterator.range().end.offset - lastLength;
        } else {
            lastLength = currentLength;
            lastStartOffset = hasReplacedNodeOrBR ? 0 : iterator.range().start.offset;
        }
        
        // Break early if we have advanced enough characters.
        bool offsetLimitReached = validateOffset ? cumulativeOffset + lastStartOffset >= offset : cumulativeOffset >= offset;
        if (!toNodeEnd && offsetLimitReached) {
            offsetInCharacter = validateOffset ? std::max(offset - lastStartOffset, 0) : offset - (cumulativeOffset - lastLength);
            finished = true;
            break;
        }
        previousNode = currentNode;
    }
    
    if (!finished) {
        offsetInCharacter = lastLength;
        if (!toNodeEnd)
            remaining = offset - cumulativeOffset;
    }
    
    // Sometimes when we are getting the end CharacterOffset of a line range, the TextIterator will emit an extra space at the end
    // and make the character count greater than the Range's end offset.
    if (toNodeEnd && currentNode->isTextNode() && currentNode == range.end.container.ptr() && static_cast<int>(range.end.offset) < lastStartOffset + offsetInCharacter)
        offsetInCharacter = range.end.offset - lastStartOffset;
    
    return CharacterOffset(currentNode, lastStartOffset, offsetInCharacter, remaining);
}

int AXObjectCache::lengthForRange(const std::optional<SimpleRange>& range)
{
    if (!range)
        return -1;
    int length = 0;
    for (TextIterator it(*range); !it.atEnd(); it.advance()) {
        // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
        if (it.text().length())
            length += it.text().length();
        else {
            // locate the node and starting offset for this replaced range
            if (AccessibilityObject::replacedNodeNeedsCharacter(it.node()))
                ++length;
        }
    }
    return length;
}

SimpleRange AXObjectCache::rangeForNodeContents(Node& node)
{
    if (AccessibilityObject::replacedNodeNeedsCharacter(&node)) {
        // For replaced nodes without children, the node itself is included in the range.
        if (auto range = makeRangeSelectingNode(node))
            return *range;
    }
    return makeRangeSelectingNodeContents(node);
}
    
std::optional<SimpleRange> AXObjectCache::rangeMatchesTextNearRange(const SimpleRange& originalRange, const String& matchText)
{
    // Create a large enough range to find the text within it that's being searched for.
    unsigned textLength = matchText.length();
    auto startPosition = VisiblePosition(makeContainerOffsetPosition(originalRange.start));
    for (unsigned k = 0; k < textLength; k++) {
        auto testPosition = startPosition.previous();
        if (testPosition.isNull())
            break;
        startPosition = testPosition;
    }

    auto endPosition = VisiblePosition(makeContainerOffsetPosition(originalRange.end));
    for (unsigned k = 0; k < textLength; k++) {
        auto testPosition = endPosition.next();
        if (testPosition.isNull())
            break;
        endPosition = testPosition;
    }

    auto searchRange = makeSimpleRange(startPosition, endPosition);
    if (!searchRange || searchRange->collapsed())
        return std::nullopt;

    auto targetOffset = characterCount({ searchRange->start, originalRange.start }, TextIteratorBehavior::EmitsCharactersBetweenAllVisiblePositions);
    return findClosestPlainText(*searchRange, matchText, { }, targetOffset);
}

static bool isReplacedNodeOrBR(Node* node)
{
    return node && (AccessibilityObject::replacedNodeNeedsCharacter(node) || node->hasTagName(brTag));
}

static bool characterOffsetsInOrder(const CharacterOffset& characterOffset1, const CharacterOffset& characterOffset2)
{
    // FIXME: Should just be able to call treeOrder without accessibility-specific logic.
    // FIXME: Not clear why CharacterOffset needs to exist at all; we have both Position and BoundaryPoint to choose from.

    if (characterOffset1.isNull() || characterOffset2.isNull())
        return false;
    
    if (characterOffset1.node == characterOffset2.node)
        return characterOffset1.offset <= characterOffset2.offset;
    
    Node* node1 = characterOffset1.node;
    Node* node2 = characterOffset2.node;
    if (!node1->isCharacterDataNode() && !isReplacedNodeOrBR(node1) && node1->hasChildNodes())
        node1 = node1->traverseToChildAt(characterOffset1.offset);
    if (!node2->isCharacterDataNode() && !isReplacedNodeOrBR(node2) && node2->hasChildNodes())
        node2 = node2->traverseToChildAt(characterOffset2.offset);
    if (!node1 || !node2)
        return false;
    
    auto range1 = AXObjectCache::rangeForNodeContents(*node1);
    auto range2 = AXObjectCache::rangeForNodeContents(*node2);
    return is_lteq(treeOrder<ComposedTree>(range1.start, range2.start));
}

static Node* resetNodeAndOffsetForReplacedNode(Node& replacedNode, int& offset, int characterCount)
{
    // Use this function to include the replaced node itself in the range we are creating.
    auto nodeRange = AXObjectCache::rangeForNodeContents(replacedNode);
    bool isInNode = static_cast<unsigned>(characterCount) <= WebCore::characterCount(nodeRange);
    offset = replacedNode.computeNodeIndex() + (isInNode ? 0 : 1);
    return replacedNode.parentNode();
}

static std::optional<BoundaryPoint> boundaryPoint(const CharacterOffset& characterOffset, bool isStart)
{
    if (characterOffset.isNull())
        return std::nullopt;

    int offset = characterOffset.startIndex + characterOffset.offset;
    Node* node = characterOffset.node;
    ASSERT(node);

    bool replacedNodeOrBR = isReplacedNodeOrBR(node);
    // For the non text node that has no children, we should create the range with its parent, otherwise the range would be collapsed.
    // Example: <div contenteditable="true"></div>, we want the range to include the div element.
    bool noChildren = !replacedNodeOrBR && !node->isTextNode() && !node->hasChildNodes();
    int characterCount = noChildren ? (isStart ? 0 : 1) : characterOffset.offset;

    if (replacedNodeOrBR || noChildren)
        node = resetNodeAndOffsetForReplacedNode(*node, offset, characterCount);

    if (!node)
        return std::nullopt;

    return { { *node, static_cast<unsigned>(offset) } };
}

static bool setRangeStartOrEndWithCharacterOffset(SimpleRange& range, const CharacterOffset& characterOffset, bool isStart)
{
    auto point = boundaryPoint(characterOffset, isStart);
    if (!point)
        return false;
    if (isStart)
        range.start = *point;
    else
        range.end = *point;
    return true;
}

std::optional<SimpleRange> AXObjectCache::rangeForUnorderedCharacterOffsets(const CharacterOffset& characterOffset1, const CharacterOffset& characterOffset2)
{
    bool alreadyInOrder = characterOffsetsInOrder(characterOffset1, characterOffset2);
    auto start = boundaryPoint(alreadyInOrder ? characterOffset1 : characterOffset2, true);
    auto end = boundaryPoint(alreadyInOrder ? characterOffset2 : characterOffset1, false);
    if (!start || !end)
        return std::nullopt;
    return { { *start, * end } };
}

void AXObjectCache::setTextMarkerDataWithCharacterOffset(TextMarkerData& textMarkerData, const CharacterOffset& characterOffset)
{
    if (characterOffset.isNull())
        return;
    
    Node* domNode = characterOffset.node;
    if (is<HTMLInputElement>(*domNode) && downcast<HTMLInputElement>(*domNode).isPasswordField()) {
        textMarkerData.ignored = true;
        return;
    }
    
    RefPtr<AccessibilityObject> obj = this->getOrCreate(domNode);
    if (!obj)
        return;
    
    // Convert to visible position.
    VisiblePosition visiblePosition = visiblePositionFromCharacterOffset(characterOffset);
    int vpOffset = 0;
    if (!visiblePosition.isNull()) {
        Position deepPos = visiblePosition.deepEquivalent();
        vpOffset = deepPos.deprecatedEditingOffset();
    }
    
    textMarkerData.axID = obj->objectID();
    textMarkerData.node = domNode;
    textMarkerData.characterOffset = characterOffset.offset;
    textMarkerData.characterStartIndex = characterOffset.startIndex;
    textMarkerData.offset = vpOffset;
    textMarkerData.affinity = visiblePosition.affinity();
    
    this->setNodeInUse(domNode);
}

CharacterOffset AXObjectCache::startOrEndCharacterOffsetForRange(const SimpleRange& range, bool isStart, bool enterTextControls)
{
    // When getting the end CharacterOffset at node boundary, we don't want to collapse to the previous node.
    if (!isStart && !range.end.offset)
        return characterOffsetForNodeAndOffset(range.end.container, 0, TraverseOptionIncludeStart);
    
    // If it's end text marker, we want to go to the end of the range, and stay within the range.
    bool stayWithinRange = !isStart;
    
    Node& endNode = range.end.container;
    if (endNode.isCharacterDataNode() && !isStart)
        return traverseToOffsetInRange(rangeForNodeContents(endNode), range.end.offset, TraverseOptionValidateOffset);
    
    auto copyRange = range;
    // Change the start of the range, so the character offset starts from node beginning.
    int offset = 0;
    auto& node = copyRange.start.container.get();
    if (node.isCharacterDataNode()) {
        auto nodeStartOffset = traverseToOffsetInRange(rangeForNodeContents(node), range.start.offset, TraverseOptionValidateOffset);
        if (isStart)
            return nodeStartOffset;
        copyRange.start.offset = 0;
        offset += nodeStartOffset.offset;
    }

    auto options = isStart ? TraverseOptionDefault : TraverseOptionToNodeEnd;
    if (!enterTextControls)
        options = static_cast<TraverseOption>(options | TraverseOptionDoNotEnterTextControls);
    return traverseToOffsetInRange(copyRange, offset, options, stayWithinRange);
}

void AXObjectCache::startOrEndTextMarkerDataForRange(TextMarkerData& textMarkerData, const SimpleRange& range, bool isStart)
{
    // This memory must be zero'd so instances of TextMarkerData can be tested for byte-equivalence.
    // Warning: This is risky and bad because TextMarkerData is a nontrivial type.
    memset(static_cast<void*>(&textMarkerData), 0, sizeof(TextMarkerData));
    
    CharacterOffset characterOffset = startOrEndCharacterOffsetForRange(range, isStart);
    if (characterOffset.isNull())
        return;
    
    setTextMarkerDataWithCharacterOffset(textMarkerData, characterOffset);
}

CharacterOffset AXObjectCache::characterOffsetForNodeAndOffset(Node& node, int offset, TraverseOption option)
{
    Node* domNode = &node;
    if (!domNode)
        return CharacterOffset();
    
    bool toNodeEnd = option & TraverseOptionToNodeEnd;
    bool includeStart = option & TraverseOptionIncludeStart;
    
    // ignoreStart is used to determine if we should go to previous node or
    // stay in current node when offset is 0.
    if (!toNodeEnd && (offset < 0 || (!offset && !includeStart))) {
        // Set the offset to the amount of characters we need to go backwards.
        offset = - offset;
        CharacterOffset charOffset = CharacterOffset();
        while (offset >= 0 && charOffset.offset <= offset) {
            offset -= charOffset.offset;
            domNode = previousNode(domNode);
            if (domNode) {
                charOffset = characterOffsetForNodeAndOffset(*domNode, 0, TraverseOptionToNodeEnd);
            } else
                return CharacterOffset();
            if (charOffset.offset == offset)
                break;
        }
        if (offset > 0)
            charOffset = characterOffsetForNodeAndOffset(*charOffset.node, charOffset.offset - offset, TraverseOptionIncludeStart);
        return charOffset;
    }
    
    auto range = rangeForNodeContents(*domNode);

    // Traverse the offset amount of characters forward and see if there's remaining offsets.
    // Keep traversing to the next node when there's remaining offsets.
    CharacterOffset characterOffset = traverseToOffsetInRange(range, offset, option);
    while (!characterOffset.isNull() && characterOffset.remaining() && !toNodeEnd) {
        domNode = nextNode(domNode);
        if (!domNode)
            return CharacterOffset();
        range = rangeForNodeContents(*domNode);
        characterOffset = traverseToOffsetInRange(range, characterOffset.remaining(), option);
    }
    
    return characterOffset;
}

void AXObjectCache::textMarkerDataForCharacterOffset(TextMarkerData& textMarkerData, const CharacterOffset& characterOffset)
{
    // This memory must be zero'd so instances of TextMarkerData can be tested for byte-equivalence.
    // Warning: This is risky and bad because TextMarkerData is a nontrivial type.
    memset(static_cast<void*>(&textMarkerData), 0, sizeof(TextMarkerData));

    setTextMarkerDataWithCharacterOffset(textMarkerData, characterOffset);
}

bool AXObjectCache::shouldSkipBoundary(const CharacterOffset& previous, const CharacterOffset& next)
{
    // Match the behavior of VisiblePosition, we should skip the node boundary when there's no visual space or new line character.
    if (previous.isNull() || next.isNull())
        return false;
    
    if (previous.node == next.node)
        return false;
    
    if (next.startIndex > 0 || next.offset > 0)
        return false;
    
    CharacterOffset newLine = startCharacterOffsetOfLine(next);
    if (next.isEqual(newLine))
        return false;
    
    return true;
}
    
void AXObjectCache::textMarkerDataForNextCharacterOffset(TextMarkerData& textMarkerData, const CharacterOffset& characterOffset)
{
    CharacterOffset next = characterOffset;
    CharacterOffset previous = characterOffset;
    bool shouldContinue;
    do {
        shouldContinue = false;
        next = nextCharacterOffset(next, false);
        if (shouldSkipBoundary(previous, next))
            next = nextCharacterOffset(next, false);
        textMarkerDataForCharacterOffset(textMarkerData, next);
        
        // We should skip next CharacterOffset if it's visually the same.
        if (!lengthForRange(rangeForUnorderedCharacterOffsets(previous, next)))
            shouldContinue = true;
        previous = next;
    } while (textMarkerData.ignored || shouldContinue);
}

void AXObjectCache::textMarkerDataForPreviousCharacterOffset(TextMarkerData& textMarkerData, const CharacterOffset& characterOffset)
{
    CharacterOffset previous = characterOffset;
    CharacterOffset next = characterOffset;
    bool shouldContinue;
    do {
        shouldContinue = false;
        previous = previousCharacterOffset(previous, false);
        textMarkerDataForCharacterOffset(textMarkerData, previous);
        
        // We should skip previous CharacterOffset if it's visually the same.
        if (!lengthForRange(rangeForUnorderedCharacterOffsets(previous, next)))
            shouldContinue = true;
        next = previous;
    } while (textMarkerData.ignored || shouldContinue);
}

Node* AXObjectCache::nextNode(Node* node) const
{
    if (!node)
        return nullptr;
    
    return NodeTraversal::nextSkippingChildren(*node);
}

Node* AXObjectCache::previousNode(Node* node) const
{
    if (!node)
        return nullptr;
    
    // First child of body shouldn't have previous node.
    if (node->parentNode() && node->parentNode()->renderer() && node->parentNode()->renderer()->isBody() && !node->previousSibling())
        return nullptr;

    return NodeTraversal::previousSkippingChildren(*node);
}

VisiblePosition AXObjectCache::visiblePositionFromCharacterOffset(const CharacterOffset& characterOffset)
{
    if (characterOffset.isNull())
        return VisiblePosition();
    
    // Create a collapsed range and use that to form a VisiblePosition, so that the case with
    // composed characters will be covered.
    auto range = rangeForUnorderedCharacterOffsets(characterOffset, characterOffset);
    if (!range)
        return { };
    return makeContainerOffsetPosition(range->start);
}

CharacterOffset AXObjectCache::characterOffsetFromVisiblePosition(const VisiblePosition& visiblePos)
{
    if (visiblePos.isNull())
        return CharacterOffset();
    
    Position deepPos = visiblePos.deepEquivalent();
    Node* domNode = deepPos.deprecatedNode();
    ASSERT(domNode);
    
    if (domNode->isCharacterDataNode())
        return traverseToOffsetInRange(rangeForNodeContents(*domNode), deepPos.deprecatedEditingOffset(), TraverseOptionValidateOffset);
    
    RefPtr<AccessibilityObject> obj = this->getOrCreate(domNode);
    if (!obj)
        return CharacterOffset();
    
    // Use nextVisiblePosition to calculate how many characters we need to traverse to the current position.
    VisiblePositionRange visiblePositionRange = obj->visiblePositionRange();
    VisiblePosition visiblePosition = visiblePositionRange.start;
    int characterOffset = 0;
    Position currentPosition = visiblePosition.deepEquivalent();
    
    VisiblePosition previousVisiblePos;
    while (!currentPosition.isNull() && !deepPos.equals(currentPosition)) {
        previousVisiblePos = visiblePosition;
        visiblePosition = visiblePosition.next();
        currentPosition = visiblePosition.deepEquivalent();
        Position previousPosition = previousVisiblePos.deepEquivalent();
        // Sometimes nextVisiblePosition will give the same VisiblePostion,
        // we break here to avoid infinite loop.
        if (currentPosition.equals(previousPosition))
            break;
        characterOffset++;
        
        // When VisiblePostion moves to next node, it will count the leading line break as
        // 1 offset, which we shouldn't include in CharacterOffset.
        if (currentPosition.deprecatedNode() != previousPosition.deprecatedNode()) {
            if (visiblePosition.characterBefore() == '\n')
                characterOffset--;
        } else {
            // Sometimes VisiblePosition will move multiple characters, like emoji.
            if (currentPosition.deprecatedNode()->isCharacterDataNode())
                characterOffset += currentPosition.offsetInContainerNode() - previousPosition.offsetInContainerNode() - 1;
        }
    }
    
    // Sometimes when the node is a replaced node and is ignored in accessibility, we get a wrong CharacterOffset from it.
    CharacterOffset result = traverseToOffsetInRange(rangeForNodeContents(*obj->node()), characterOffset);
    if (result.remainingOffset > 0 && !result.isNull() && isRendererReplacedElement(result.node->renderer()))
        result.offset += result.remainingOffset;
    return result;
}

AccessibilityObject* AXObjectCache::accessibilityObjectForTextMarkerData(TextMarkerData& textMarkerData)
{
    if (!isNodeInUse(textMarkerData.node))
        return nullptr;
    
    Node* domNode = textMarkerData.node;
    return this->getOrCreate(domNode);
}

std::optional<TextMarkerData> AXObjectCache::textMarkerDataForVisiblePosition(const VisiblePosition& visiblePos)
{
    if (visiblePos.isNull())
        return std::nullopt;

    Position deepPos = visiblePos.deepEquivalent();
    Node* domNode = deepPos.anchorNode();
    ASSERT(domNode);
    if (!domNode)
        return std::nullopt;

    if (is<HTMLInputElement>(*domNode) && downcast<HTMLInputElement>(*domNode).isPasswordField())
        return std::nullopt;

    // If the visible position has an anchor type referring to a node other than the anchored node, we should
    // set the text marker data with CharacterOffset so that the offset will correspond to the node.
    CharacterOffset characterOffset = characterOffsetFromVisiblePosition(visiblePos);
    if (deepPos.anchorType() == Position::PositionIsAfterAnchor || deepPos.anchorType() == Position::PositionIsAfterChildren) {
        TextMarkerData textMarkerData;
        textMarkerDataForCharacterOffset(textMarkerData, characterOffset);
        return textMarkerData;
    }

    // find or create an accessibility object for this node
    AXObjectCache* cache = domNode->document().axObjectCache();
    if (!cache)
        return std::nullopt;
    RefPtr<AccessibilityObject> obj = cache->getOrCreate(domNode);

    // This memory must be zero'd so instances of TextMarkerData can be tested for byte-equivalence.
    // Warning: This is risky and bad because TextMarkerData is a nontrivial type.
    TextMarkerData textMarkerData;
    memset(static_cast<void*>(&textMarkerData), 0, sizeof(TextMarkerData));
    
    textMarkerData.axID = obj->objectID();
    textMarkerData.node = domNode;
    textMarkerData.offset = deepPos.deprecatedEditingOffset();
    textMarkerData.anchorType = deepPos.anchorType();
    textMarkerData.affinity = visiblePos.affinity();

    textMarkerData.characterOffset = characterOffset.offset;
    textMarkerData.characterStartIndex = characterOffset.startIndex;

    cache->setNodeInUse(domNode);

    return textMarkerData;
}

// This function exits as a performance optimization to avoid a synchronous layout.
std::optional<TextMarkerData> AXObjectCache::textMarkerDataForFirstPositionInTextControl(HTMLTextFormControlElement& textControl)
{
    if (is<HTMLInputElement>(textControl) && downcast<HTMLInputElement>(textControl).isPasswordField())
        return std::nullopt;

    AXObjectCache* cache = textControl.document().axObjectCache();
    if (!cache)
        return std::nullopt;

    RefPtr<AccessibilityObject> obj = cache->getOrCreate(&textControl);
    if (!obj)
        return std::nullopt;

    // This memory must be zero'd so instances of TextMarkerData can be tested for byte-equivalence.
    // Warning: This is risky and bad because TextMarkerData is a nontrivial type.
    TextMarkerData textMarkerData;
    memset(static_cast<void*>(&textMarkerData), 0, sizeof(TextMarkerData));
    
    textMarkerData.axID = obj->objectID();
    textMarkerData.node = &textControl;

    cache->setNodeInUse(&textControl);

    return textMarkerData;
}

CharacterOffset AXObjectCache::nextCharacterOffset(const CharacterOffset& characterOffset, bool ignoreNextNodeStart)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    // We don't always move one 'character' at a time since there might be composed characters.
    unsigned nextOffset = Position::uncheckedNextOffset(characterOffset.node, characterOffset.offset);
    CharacterOffset next = characterOffsetForNodeAndOffset(*characterOffset.node, nextOffset);
    
    // To be consistent with VisiblePosition, we should consider the case that current node end to next node start counts 1 offset.
    if (!ignoreNextNodeStart && !next.isNull() && !isReplacedNodeOrBR(next.node) && next.node != characterOffset.node) {
        if (auto range = rangeForUnorderedCharacterOffsets(characterOffset, next)) {
            auto length = characterCount(*range);
            if (length > nextOffset - characterOffset.offset)
                next = characterOffsetForNodeAndOffset(*next.node, 0, TraverseOptionIncludeStart);
        }
    }
    
    return next;
}

CharacterOffset AXObjectCache::previousCharacterOffset(const CharacterOffset& characterOffset, bool ignorePreviousNodeEnd)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    // To be consistent with VisiblePosition, we should consider the case that current node start to previous node end counts 1 offset.
    if (!ignorePreviousNodeEnd && !characterOffset.offset)
        return characterOffsetForNodeAndOffset(*characterOffset.node, 0);
    
    // We don't always move one 'character' a time since there might be composed characters.
    int previousOffset = Position::uncheckedPreviousOffset(characterOffset.node, characterOffset.offset);
    return characterOffsetForNodeAndOffset(*characterOffset.node, previousOffset, TraverseOptionIncludeStart);
}

CharacterOffset AXObjectCache::startCharacterOffsetOfWord(const CharacterOffset& characterOffset, EWordSide side)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    CharacterOffset c = characterOffset;
    if (side == RightWordIfOnBoundary) {
        CharacterOffset endOfParagraph = endCharacterOffsetOfParagraph(c);
        if (c.isEqual(endOfParagraph))
            return c;
        
        // We should consider the node boundary that splits words. Otherwise VoiceOver won't see it as space.
        c = nextCharacterOffset(characterOffset, false);
        if (shouldSkipBoundary(characterOffset, c))
            c = nextCharacterOffset(c, false);
        if (c.isNull())
            return characterOffset;
    }
    
    return previousBoundary(c, startWordBoundary);
}

CharacterOffset AXObjectCache::endCharacterOffsetOfWord(const CharacterOffset& characterOffset, EWordSide side)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    CharacterOffset c = characterOffset;
    if (side == LeftWordIfOnBoundary) {
        CharacterOffset startOfParagraph = startCharacterOffsetOfParagraph(c);
        if (c.isEqual(startOfParagraph))
            return c;
        
        c = previousCharacterOffset(characterOffset);
        if (c.isNull())
            return characterOffset;
    } else {
        CharacterOffset endOfParagraph = endCharacterOffsetOfParagraph(characterOffset);
        if (characterOffset.isEqual(endOfParagraph))
            return characterOffset;
    }
    
    return nextBoundary(c, endWordBoundary);
}

CharacterOffset AXObjectCache::previousWordStartCharacterOffset(const CharacterOffset& characterOffset)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    CharacterOffset previousOffset = previousCharacterOffset(characterOffset);
    if (previousOffset.isNull())
        return CharacterOffset();
    
    return startCharacterOffsetOfWord(previousOffset, RightWordIfOnBoundary);
}

CharacterOffset AXObjectCache::nextWordEndCharacterOffset(const CharacterOffset& characterOffset)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    CharacterOffset nextOffset = nextCharacterOffset(characterOffset);
    if (nextOffset.isNull())
        return CharacterOffset();
    
    return endCharacterOffsetOfWord(nextOffset, LeftWordIfOnBoundary);
}

std::optional<SimpleRange> AXObjectCache::leftWordRange(const CharacterOffset& characterOffset)
{
    CharacterOffset start = startCharacterOffsetOfWord(characterOffset, LeftWordIfOnBoundary);
    CharacterOffset end = endCharacterOffsetOfWord(start);
    return rangeForUnorderedCharacterOffsets(start, end);
}

std::optional<SimpleRange> AXObjectCache::rightWordRange(const CharacterOffset& characterOffset)
{
    CharacterOffset start = startCharacterOffsetOfWord(characterOffset, RightWordIfOnBoundary);
    CharacterOffset end = endCharacterOffsetOfWord(start);
    return rangeForUnorderedCharacterOffsets(start, end);
}

static UChar32 characterForCharacterOffset(const CharacterOffset& characterOffset)
{
    if (characterOffset.isNull() || !characterOffset.node->isTextNode())
        return 0;
    
    UChar32 ch = 0;
    unsigned offset = characterOffset.startIndex + characterOffset.offset;
    if (offset < characterOffset.node->textContent().length()) {
// FIXME: Remove IGNORE_CLANG_WARNINGS macros once one of <rdar://problem/58615489&58615391> is fixed.
IGNORE_CLANG_WARNINGS_BEGIN("conditional-uninitialized")
        U16_NEXT(characterOffset.node->textContent(), offset, characterOffset.node->textContent().length(), ch);
IGNORE_CLANG_WARNINGS_END
    }
    return ch;
}

UChar32 AXObjectCache::characterAfter(const CharacterOffset& characterOffset)
{
    return characterForCharacterOffset(nextCharacterOffset(characterOffset));
}

UChar32 AXObjectCache::characterBefore(const CharacterOffset& characterOffset)
{
    return characterForCharacterOffset(characterOffset);
}

static bool characterOffsetNodeIsBR(const CharacterOffset& characterOffset)
{
    if (characterOffset.isNull())
        return false;
    
    return characterOffset.node->hasTagName(brTag);
}
    
static Node* parentEditingBoundary(Node* node)
{
    if (!node)
        return nullptr;
    
    Node* documentElement = node->document().documentElement();
    if (!documentElement)
        return nullptr;
    
    Node* boundary = node;
    while (boundary != documentElement && boundary->nonShadowBoundaryParentNode() && node->hasEditableStyle() == boundary->parentNode()->hasEditableStyle())
        boundary = boundary->nonShadowBoundaryParentNode();
    
    return boundary;
}

CharacterOffset AXObjectCache::nextBoundary(const CharacterOffset& characterOffset, BoundarySearchFunction searchFunction)
{
    if (characterOffset.isNull())
        return { };

    Node* boundary = parentEditingBoundary(characterOffset.node);
    if (!boundary)
        return { };

    auto searchRange = rangeForNodeContents(*boundary);

    Vector<UChar, 1024> string;
    unsigned prefixLength = 0;
    
    if (requiresContextForWordBoundary(characterAfter(characterOffset))) {
        auto backwardsScanRange = makeRangeSelectingNodeContents(boundary->document());
        if (!setRangeStartOrEndWithCharacterOffset(backwardsScanRange, characterOffset, false))
            return { };
        prefixLength = prefixLengthForRange(backwardsScanRange, string);
    }
    
    if (!setRangeStartOrEndWithCharacterOffset(searchRange, characterOffset, true))
        return { };
    CharacterOffset end = startOrEndCharacterOffsetForRange(searchRange, false);
    
    TextIterator it(searchRange, TextIteratorBehavior::EmitsObjectReplacementCharacters);
    unsigned next = forwardSearchForBoundaryWithTextIterator(it, string, prefixLength, searchFunction);
    
    if (it.atEnd() && next == string.size())
        return end;
    
    // We should consider the node boundary that splits words.
    if (searchFunction == endWordBoundary && next - prefixLength == 1)
        return nextCharacterOffset(characterOffset, false);
    
    // The endSentenceBoundary function will include a line break at the end of the sentence.
    if (searchFunction == endSentenceBoundary && string[next - 1] == '\n')
        next--;
    
    if (next > prefixLength)
        return characterOffsetForNodeAndOffset(*characterOffset.node, characterOffset.offset + next - prefixLength);
    
    return characterOffset;
}

// FIXME: Share code with the one in VisibleUnits.cpp.
CharacterOffset AXObjectCache::previousBoundary(const CharacterOffset& characterOffset, BoundarySearchFunction searchFunction, NeedsContextAtParagraphStart needsContextAtParagraphStart)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    Node* boundary = parentEditingBoundary(characterOffset.node);
    if (!boundary)
        return CharacterOffset();
    
    auto searchRange = rangeForNodeContents(*boundary);
    Vector<UChar, 1024> string;
    unsigned suffixLength = 0;

    if (needsContextAtParagraphStart == NeedsContextAtParagraphStart::Yes && startCharacterOffsetOfParagraph(characterOffset).isEqual(characterOffset)) {
        auto forwardsScanRange = makeRangeSelectingNodeContents(boundary->document());
        auto endOfCurrentParagraph = endCharacterOffsetOfParagraph(characterOffset);
        if (!setRangeStartOrEndWithCharacterOffset(forwardsScanRange, characterOffset, true))
            return { };
        if (!setRangeStartOrEndWithCharacterOffset(forwardsScanRange, endOfCurrentParagraph, false))
            return { };
        for (TextIterator forwardsIterator(forwardsScanRange); !forwardsIterator.atEnd(); forwardsIterator.advance())
            append(string, forwardsIterator.text());
        suffixLength = string.size();
    } else if (requiresContextForWordBoundary(characterBefore(characterOffset))) {
        auto forwardsScanRange = makeRangeSelectingNodeContents(boundary->document());
        auto afterBoundary = makeBoundaryPointAfterNode(*boundary);
        if (!afterBoundary)
            return { };
        forwardsScanRange.start = *afterBoundary;
        if (!setRangeStartOrEndWithCharacterOffset(forwardsScanRange, characterOffset, true))
            return { };
        suffixLength = suffixLengthForRange(forwardsScanRange, string);
    }
    
    if (!setRangeStartOrEndWithCharacterOffset(searchRange, characterOffset, false))
        return { };
    CharacterOffset start = startOrEndCharacterOffsetForRange(searchRange, true);
    
    SimplifiedBackwardsTextIterator it(searchRange);
    unsigned next = backwardSearchForBoundaryWithTextIterator(it, string, suffixLength, searchFunction);
    
    if (!next)
        return it.atEnd() ? start : characterOffset;
    
    auto& node = (it.atEnd() ? searchRange : it.range()).start.container.get();

    // SimplifiedBackwardsTextIterator ignores replaced elements.
    if (AccessibilityObject::replacedNodeNeedsCharacter(characterOffset.node))
        return characterOffsetForNodeAndOffset(*characterOffset.node, 0);
    Node* nextSibling = node.nextSibling();
    if (&node != characterOffset.node && AccessibilityObject::replacedNodeNeedsCharacter(nextSibling))
        return startOrEndCharacterOffsetForRange(rangeForNodeContents(*nextSibling), false);

    if ((!suffixLength && node.isTextNode() && next <= node.length()) || (node.renderer() && node.renderer()->isBR() && !next)) {
        // The next variable contains a usable index into a text node
        if (node.isTextNode())
            return traverseToOffsetInRange(rangeForNodeContents(node), next, TraverseOptionValidateOffset);
        return characterOffsetForNodeAndOffset(node, next, TraverseOptionIncludeStart);
    }
    
    int characterCount = characterOffset.offset;
    if (next < string.size() - suffixLength)
        characterCount -= string.size() - suffixLength - next;
    // We don't want to go to the previous node if the node is at the start of a new line.
    if (characterCount < 0 && (characterOffsetNodeIsBR(characterOffset) || string[string.size() - suffixLength - 1] == '\n'))
        characterCount = 0;
    return characterOffsetForNodeAndOffset(*characterOffset.node, characterCount, TraverseOptionIncludeStart);
}

CharacterOffset AXObjectCache::startCharacterOffsetOfParagraph(const CharacterOffset& characterOffset, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    auto& startNode = *characterOffset.node;
    
    if (isRenderedAsNonInlineTableImageOrHR(&startNode))
        return startOrEndCharacterOffsetForRange(rangeForNodeContents(startNode), true);
    
    auto* startBlock = enclosingBlock(&startNode);
    int offset = characterOffset.startIndex + characterOffset.offset;
    auto* highestRoot = highestEditableRoot(firstPositionInOrBeforeNode(&startNode));
    Position::AnchorType type = Position::PositionIsOffsetInAnchor;
    
    auto& node = *findStartOfParagraph(&startNode, highestRoot, startBlock, offset, type, boundaryCrossingRule);
    
    if (type == Position::PositionIsOffsetInAnchor)
        return characterOffsetForNodeAndOffset(node, offset, TraverseOptionIncludeStart);
    
    return startOrEndCharacterOffsetForRange(rangeForNodeContents(node), true);
}

CharacterOffset AXObjectCache::endCharacterOffsetOfParagraph(const CharacterOffset& characterOffset, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    Node* startNode = characterOffset.node;
    if (isRenderedAsNonInlineTableImageOrHR(startNode))
        return startOrEndCharacterOffsetForRange(rangeForNodeContents(*startNode), false);
    
    Node* stayInsideBlock = enclosingBlock(startNode);
    int offset = characterOffset.startIndex + characterOffset.offset;
    Node* highestRoot = highestEditableRoot(firstPositionInOrBeforeNode(startNode));
    Position::AnchorType type = Position::PositionIsOffsetInAnchor;
    
    auto& node = *findEndOfParagraph(startNode, highestRoot, stayInsideBlock, offset, type, boundaryCrossingRule);
    if (type == Position::PositionIsOffsetInAnchor) {
        if (node.isTextNode()) {
            CharacterOffset startOffset = startOrEndCharacterOffsetForRange(rangeForNodeContents(node), true);
            offset -= startOffset.startIndex;
        }
        return characterOffsetForNodeAndOffset(node, offset, TraverseOptionIncludeStart);
    }
    
    return startOrEndCharacterOffsetForRange(rangeForNodeContents(node), false);
}

std::optional<SimpleRange> AXObjectCache::paragraphForCharacterOffset(const CharacterOffset& characterOffset)
{
    CharacterOffset start = startCharacterOffsetOfParagraph(characterOffset);
    CharacterOffset end = endCharacterOffsetOfParagraph(start);
    return rangeForUnorderedCharacterOffsets(start, end);
}

CharacterOffset AXObjectCache::nextParagraphEndCharacterOffset(const CharacterOffset& characterOffset)
{
    // make sure we move off of a paragraph end
    CharacterOffset next = nextCharacterOffset(characterOffset);
    
    // We should skip the following BR node.
    if (characterOffsetNodeIsBR(next) && !characterOffsetNodeIsBR(characterOffset))
        next = nextCharacterOffset(next);
    
    return endCharacterOffsetOfParagraph(next);
}

CharacterOffset AXObjectCache::previousParagraphStartCharacterOffset(const CharacterOffset& characterOffset)
{
    // make sure we move off of a paragraph start
    CharacterOffset previous = previousCharacterOffset(characterOffset);
    
    // We should skip the preceding BR node.
    if (characterOffsetNodeIsBR(previous) && !characterOffsetNodeIsBR(characterOffset))
        previous = previousCharacterOffset(previous);
    
    return startCharacterOffsetOfParagraph(previous);
}

CharacterOffset AXObjectCache::startCharacterOffsetOfSentence(const CharacterOffset& characterOffset)
{
    return previousBoundary(characterOffset, startSentenceBoundary, NeedsContextAtParagraphStart::Yes);
}

CharacterOffset AXObjectCache::endCharacterOffsetOfSentence(const CharacterOffset& characterOffset)
{
    return nextBoundary(characterOffset, endSentenceBoundary);
}

std::optional<SimpleRange> AXObjectCache::sentenceForCharacterOffset(const CharacterOffset& characterOffset)
{
    CharacterOffset start = startCharacterOffsetOfSentence(characterOffset);
    CharacterOffset end = endCharacterOffsetOfSentence(start);
    return rangeForUnorderedCharacterOffsets(start, end);
}

CharacterOffset AXObjectCache::nextSentenceEndCharacterOffset(const CharacterOffset& characterOffset)
{
    // Make sure we move off of a sentence end.
    return endCharacterOffsetOfSentence(nextCharacterOffset(characterOffset));
}

CharacterOffset AXObjectCache::previousSentenceStartCharacterOffset(const CharacterOffset& characterOffset)
{
    // Make sure we move off of a sentence start.
    CharacterOffset previous = previousCharacterOffset(characterOffset);
    
    // We should skip the preceding BR node.
    if (characterOffsetNodeIsBR(previous) && !characterOffsetNodeIsBR(characterOffset))
        previous = previousCharacterOffset(previous);
    
    return startCharacterOffsetOfSentence(previous);
}

LayoutRect AXObjectCache::localCaretRectForCharacterOffset(RenderObject*& renderer, const CharacterOffset& characterOffset)
{
    if (characterOffset.isNull()) {
        renderer = nullptr;
        return IntRect();
    }
    
    renderer = characterOffset.node->renderer();
    if (!renderer)
        return LayoutRect();
    
    // Use a collapsed range to get the position.
    auto range = rangeForUnorderedCharacterOffsets(characterOffset, characterOffset);
    if (!range)
        return IntRect();

    auto boxAndOffset = makeContainerOffsetPosition(range->start).inlineBoxAndOffset(Affinity::Downstream);
    if (boxAndOffset.box)
        renderer = const_cast<RenderObject*>(&boxAndOffset.box->renderer());

    if (is<RenderLineBreak>(renderer) && InlineIterator::boxFor(downcast<RenderLineBreak>(*renderer)) != boxAndOffset.box)
        return IntRect();

    return computeLocalCaretRect(*renderer, boxAndOffset);
}

IntRect AXObjectCache::absoluteCaretBoundsForCharacterOffset(const CharacterOffset& characterOffset)
{
    RenderBlock* caretPainter = nullptr;
    
    // First compute a rect local to the renderer at the selection start.
    RenderObject* renderer = nullptr;
    LayoutRect localRect = localCaretRectForCharacterOffset(renderer, characterOffset);
    
    localRect = localCaretRectInRendererForRect(localRect, characterOffset.node, renderer, caretPainter);
    return absoluteBoundsForLocalCaretRect(caretPainter, localRect);
}

CharacterOffset AXObjectCache::characterOffsetForPoint(const IntPoint& point, AXCoreObject* object)
{
    if (!object)
        return { };
    auto range = makeSimpleRange(object->visiblePositionForPoint(point));
    if (!range)
        return { };
    return startOrEndCharacterOffsetForRange(*range, true);
}

CharacterOffset AXObjectCache::characterOffsetForPoint(const IntPoint& point)
{
    auto range = makeSimpleRange(m_document.caretPositionFromPoint(point));
    if (!range)
        return { };
    return startOrEndCharacterOffsetForRange(*range, true);
}

CharacterOffset AXObjectCache::characterOffsetForBounds(const IntRect& rect, bool first)
{
    if (rect.isEmpty())
        return CharacterOffset();
    
    IntPoint corner = first ? rect.minXMinYCorner() : rect.maxXMaxYCorner();
    CharacterOffset characterOffset = characterOffsetForPoint(corner);
    
    if (rect.contains(absoluteCaretBoundsForCharacterOffset(characterOffset).center()))
        return characterOffset;
    
    // If the initial position is located outside the bounds adjust it incrementally as needed.
    CharacterOffset nextCharOffset = nextCharacterOffset(characterOffset, false);
    CharacterOffset previousCharOffset = previousCharacterOffset(characterOffset, false);
    while (!nextCharOffset.isNull() || !previousCharOffset.isNull()) {
        if (rect.contains(absoluteCaretBoundsForCharacterOffset(nextCharOffset).center()))
            return nextCharOffset;
        if (rect.contains(absoluteCaretBoundsForCharacterOffset(previousCharOffset).center()))
            return previousCharOffset;
        
        nextCharOffset = nextCharacterOffset(nextCharOffset, false);
        previousCharOffset = previousCharacterOffset(previousCharOffset, false);
    }
    
    return CharacterOffset();
}

// FIXME: Remove VisiblePosition code after implementing this using CharacterOffset.
CharacterOffset AXObjectCache::endCharacterOffsetOfLine(const CharacterOffset& characterOffset)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    VisiblePosition vp = visiblePositionFromCharacterOffset(characterOffset);
    VisiblePosition endLine = endOfLine(vp);
    
    return characterOffsetFromVisiblePosition(endLine);
}

CharacterOffset AXObjectCache::startCharacterOffsetOfLine(const CharacterOffset& characterOffset)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    VisiblePosition vp = visiblePositionFromCharacterOffset(characterOffset);
    VisiblePosition startLine = startOfLine(vp);
    
    return characterOffsetFromVisiblePosition(startLine);
}

CharacterOffset AXObjectCache::characterOffsetForIndex(int index, const AXCoreObject* obj)
{
    if (!obj)
        return CharacterOffset();
    
    VisiblePosition vp = obj->visiblePositionForIndex(index);
    CharacterOffset validate = characterOffsetFromVisiblePosition(vp);
    // In text control, VisiblePosition always gives the before position of a
    // BR node, while CharacterOffset will do the opposite.
    if (obj->isTextControl() && characterOffsetNodeIsBR(validate))
        validate.offset = 1;

    auto liveRange = obj->elementRange();
    if (!liveRange)
        return { };

    auto range = SimpleRange { *liveRange };
    CharacterOffset start = startOrEndCharacterOffsetForRange(range, true, true);
    CharacterOffset end = startOrEndCharacterOffsetForRange(range, false, true);
    CharacterOffset result = start;
    for (int i = 0; i < index; i++) {
        if (result.isEqual(validate)) {
            // Do not include the new line character, always move the offset to the start of next node.
            if ((validate.node->isTextNode() || characterOffsetNodeIsBR(validate))) {
                CharacterOffset next = nextCharacterOffset(validate, false);
                if (!next.isNull() && !next.offset && rootAXEditableElement(next.node) == rootAXEditableElement(validate.node))
                    result = next;
            }
            break;
        }
        
        result = nextCharacterOffset(result, false);
        if (result.isEqual(end))
            break;
    }
    return result;
}

const Element* AXObjectCache::rootAXEditableElement(const Node* node)
{
    const Element* result = node->rootEditableElement();
    const Element* element = is<Element>(*node) ? downcast<Element>(node) : node->parentElement();

    for (; element; element = element->parentElement()) {
        if (nodeIsTextControl(element))
            result = element;
    }

    return result;
}

static void conditionallyAddNodeToFilterList(Node* node, const Document& document, HashSet<Ref<Node>>& nodesToRemove)
{
    if (node && (!node->isConnected() || &node->document() == &document))
        nodesToRemove.add(*node);
}
    
template<typename T>
static void filterVectorPairForRemoval(const Vector<std::pair<T, T>>& list, const Document& document, HashSet<Ref<Node>>& nodesToRemove)
{
    for (auto& entry : list) {
        conditionallyAddNodeToFilterList(entry.first, document, nodesToRemove);
        conditionallyAddNodeToFilterList(entry.second, document, nodesToRemove);
    }
}
    
template<typename T, typename U>
static void filterMapForRemoval(const HashMap<T, U>& list, const Document& document, HashSet<Ref<Node>>& nodesToRemove)
{
    for (auto& entry : list)
        conditionallyAddNodeToFilterList(entry.key, document, nodesToRemove);
}

template<typename T>
static void filterListForRemoval(const ListHashSet<T>& list, const Document& document, HashSet<Ref<Node>>& nodesToRemove)
{
    for (auto* node : list)
        conditionallyAddNodeToFilterList(node, document, nodesToRemove);
}

template<typename WeakHashSet>
static void filterWeakHashSetForRemoval(WeakHashSet& weakHashSet, const Document& document, HashSet<Ref<Node>>& nodesToRemove)
{
    weakHashSet.forEach([&] (auto& element) {
        conditionallyAddNodeToFilterList(&element, document, nodesToRemove);
    });
}

void AXObjectCache::prepareForDocumentDestruction(const Document& document)
{
    HashSet<Ref<Node>> nodesToRemove;
    filterListForRemoval(m_textMarkerNodes, document, nodesToRemove);
    filterListForRemoval(m_deferredTextChangedList, document, nodesToRemove);
    filterListForRemoval(m_deferredNodeAddedOrRemovedList, document, nodesToRemove);
    filterWeakHashSetForRemoval(m_deferredRecomputeIsIgnoredList, document, nodesToRemove);
    filterWeakHashSetForRemoval(m_deferredRecomputeTableIsExposedList, document, nodesToRemove);
    filterWeakHashSetForRemoval(m_deferredSelectedChildredChangedList, document, nodesToRemove);
    filterWeakHashSetForRemoval(m_deferredModalChangedList, document, nodesToRemove);
    filterWeakHashSetForRemoval(m_deferredMenuListChange, document, nodesToRemove);
    filterMapForRemoval(m_deferredTextFormControlValue, document, nodesToRemove);
    filterVectorPairForRemoval(m_deferredFocusedNodeChange, document, nodesToRemove);

    for (const auto& entry : m_deferredAttributeChange) {
        if (entry.element && (!entry.element->isConnected() || &entry.element->document() == &document))
            nodesToRemove.add(*entry.element);
    }

    for (const auto& element : m_modalElements) {
        if (element && (!element->isConnected() || &element->document() == &document))
            nodesToRemove.add(*element);
    }

    for (auto& node : nodesToRemove)
        remove(node);
}
    
bool AXObjectCache::nodeIsTextControl(const Node* node)
{
    if (!node)
        return false;

    const AccessibilityObject* axObject = getOrCreate(const_cast<Node*>(node));
    return axObject && axObject->isTextControl();
}

void AXObjectCache::performCacheUpdateTimerFired()
{
    // If there's a pending layout, let the layout trigger the AX update.
    if (!document().view() || document().view()->needsLayout())
        return;

    performDeferredCacheUpdate();
}

void AXObjectCache::processDeferredChildrenChangedList()
{
    AXTRACE(makeString("AXObjectCache::processDeferredChildrenChangedList 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    AXLOG(makeString("ChildrenChangedList size ", m_deferredChildrenChangedList.size()));
    for (auto& child : m_deferredChildrenChangedList)
        handleChildrenChanged(*child);
    m_deferredChildrenChangedList.clear();
}

void AXObjectCache::performDeferredCacheUpdate()
{
    AXTRACE(makeString("AXObjectCache::performDeferredCacheUpdate 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    if (m_performingDeferredCacheUpdate) {
        AXLOG("Bailing out due to reentrant call.");
        return;
    }
    SetForScope performingDeferredCacheUpdate(m_performingDeferredCacheUpdate, true);

    AXLOG(makeString("RecomputeTableIsExposedList size ", m_deferredRecomputeTableIsExposedList.computeSize()));
    m_deferredRecomputeTableIsExposedList.forEach([this] (auto& tableElement) {
        if (auto* axTable = dynamicDowncast<AccessibilityTable>(get(&tableElement)))
            axTable->recomputeIsExposable();
    });
    m_deferredRecomputeTableIsExposedList.clear();

    AXLOG(makeString("NodeAddedOrRemovedList size ", m_deferredNodeAddedOrRemovedList.size()));
    for (auto* nodeChild : m_deferredNodeAddedOrRemovedList) {
        handleMenuOpened(nodeChild);
        handleLiveRegionCreated(nodeChild);
    }
    m_deferredNodeAddedOrRemovedList.clear();

    processDeferredChildrenChangedList();

    AXLOG(makeString("TextChangedList size ", m_deferredTextChangedList.size()));
    for (auto* node : m_deferredTextChangedList)
        handleTextChanged(getOrCreate(node));
    m_deferredTextChangedList.clear();

    AXLOG(makeString("RecomputeIsIgnoredList size ", m_deferredRecomputeIsIgnoredList.computeSize()));
    m_deferredRecomputeIsIgnoredList.forEach([this] (auto& element) {
        if (auto* renderer = element.renderer())
            recomputeIsIgnored(renderer);
    });
    m_deferredRecomputeIsIgnoredList.clear();

    AXLOG(makeString("SelectedChildredChangedList size ", m_deferredSelectedChildredChangedList.computeSize()));
    m_deferredSelectedChildredChangedList.forEach([this] (auto& selectElement) {
        selectedChildrenChanged(&selectElement);
    });
    m_deferredSelectedChildredChangedList.clear();

    AXLOG(makeString("TextFormControlValue size ", m_deferredTextFormControlValue.size()));
    for (auto& deferredFormControlContext : m_deferredTextFormControlValue) {
        auto& textFormControlElement = downcast<HTMLTextFormControlElement>(*deferredFormControlContext.key);
        postTextReplacementNotificationForTextControl(textFormControlElement, deferredFormControlContext.value, textFormControlElement.innerTextValue());
    }
    m_deferredTextFormControlValue.clear();

    AXLOG(makeString("AttributeChange size ", m_deferredAttributeChange.size()));
    for (const auto& attributeChange : m_deferredAttributeChange)
        handleAttributeChange(attributeChange.element, attributeChange.attrName, attributeChange.oldValue, attributeChange.newValue);
    m_deferredAttributeChange.clear();

    AXLOG(makeString("FocusedNodeChange size ", m_deferredFocusedNodeChange.size()));
    for (auto& deferredFocusedChangeContext : m_deferredFocusedNodeChange) {
        // Don't recompute the active modal for each individal focus change, as that could cause a lot of expensive tree rebuilding. Instead, we do it once below.
        handleFocusedUIElementChanged(deferredFocusedChangeContext.first, deferredFocusedChangeContext.second, UpdateModal::No);
        // Recompute isIgnored after a focus change in case that altered visibility.
        recomputeIsIgnored(deferredFocusedChangeContext.first);
        recomputeIsIgnored(deferredFocusedChangeContext.second);
    }
    bool updatedFocusedElement = !m_deferredFocusedNodeChange.isEmpty();
    // If we changed the focused element, that could affect what modal should be active, so recompute it.
    bool shouldRecomputeModal = updatedFocusedElement;
    m_deferredFocusedNodeChange.clear();

    AXLOG(makeString("ModalChangedList size ", m_deferredModalChangedList.computeSize()));
    for (auto& element : m_deferredModalChangedList) {
        if (!is<HTMLDialogElement>(element) && !nodeHasRole(&element, "dialog"_s) && !nodeHasRole(&element, "alertdialog"_s))
            continue;

        shouldRecomputeModal = true;
        if (!m_modalNodesInitialized)
            findModalNodes();

        if (isModalElement(element)) {
            // Add the newly modified node to the modal nodes set.
            // We will recompute the current valid aria modal node in modalNode() when this node is not visible.
            m_modalElements.append(&element);
        } else {
            m_modalElements.removeAllMatching([&element] (const auto& modalElement) {
                return &element == modalElement.get();
            });
        }
    }
    m_deferredModalChangedList.clear();

    if (shouldRecomputeModal) {
        updateCurrentModalNode(updatedFocusedElement ? WillRecomputeFocus::No : WillRecomputeFocus::Yes);
        // "When a modal element is displayed, assistive technologies SHOULD navigate to the element unless focus has explicitly been set elsewhere."
        // `updatedFocusedElement` indicates focus was explicitly set elsewhere, so don't autofocus into the modal.
        // https://w3c.github.io/aria/#aria-modal
        if (!updatedFocusedElement)
            focusCurrentModal();
    }

    AXLOG(makeString("MenuListChange size ", m_deferredMenuListChange.computeSize()));
    m_deferredMenuListChange.forEach([this] (auto& element) {
        handleMenuListValueChanged(element);
    });
    m_deferredMenuListChange.clear();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (m_deferredRegenerateIsolatedTree) {
        if (auto tree = AXIsolatedTree::treeForPageID(m_pageID)) {
            if (auto* webArea = rootWebArea()) {
                AXLOG("Regenerating isolated tree from AXObjectCache::performDeferredCacheUpdate().");
                tree->generateSubtree(*webArea);
            }
        }
    }
    m_deferredRegenerateIsolatedTree = false;
#endif

    platformPerformDeferredCacheUpdate();
}

void AXObjectCache::handleMenuListValueChanged(Element& element)
{
    RefPtr<AccessibilityObject> object = get(&element);
    if (!object)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(*object, AXMenuListValueChanged);
#endif

    postPlatformNotification(object.get(), AXMenuListValueChanged);
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::updateIsolatedTree(AccessibilityObject* object, AXNotification notification)
{
    if (object)
        updateIsolatedTree(*object, notification);
}

void AXObjectCache::updateIsolatedTree(AccessibilityObject& object, AXNotification notification)
{
    updateIsolatedTree({ std::make_pair(&object, notification) });
}

void AXObjectCache::updateIsolatedTree(const Vector<std::pair<RefPtr<AccessibilityObject>, AXNotification>>& notifications)
{
    AXTRACE(makeString("AXObjectCache::updateIsolatedTree 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));

    if (!m_pageID) {
        AXLOG("No pageID.");
        return;
    }

    auto tree = AXIsolatedTree::treeForPageID(*m_pageID);
    if (!tree) {
        AXLOG("No isolated tree for m_pageID");
        return;
    }

    struct UpdatedFields {
        bool children { false };
        bool node { false };
    };
    HashMap<AXID, UpdatedFields> updatedObjects;
    auto updateNode = [&] (RefPtr<AXCoreObject> axObject) {
        auto updatedFields = updatedObjects.get(axObject->objectID());
        if (!updatedFields.node) {
            updatedObjects.set(axObject->objectID(), UpdatedFields { updatedFields.children, true });
            tree->updateNode(*axObject);
        }
    };

    for (const auto& notification : notifications) {
        AXLOG(notification);
        if (!notification.first || notification.first->isDetached())
            continue;

        switch (notification.second) {
        case AXAutofillTypeChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::ValueAutofillButtonType);
            break;
        case AXCheckedStateChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::IsChecked);
            break;
        case AXCurrentStateChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::CurrentState);
            break;
        case AXColumnCountChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::AXColumnCount);
            break;
        case AXColumnIndexChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::AXColumnIndex);
            break;
        case AXDisabledStateChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::CanSetFocusAttribute);
            tree->updateNodeProperty(*notification.first, AXPropertyName::IsEnabled);
            break;
        case AXExpandedChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::IsExpanded);
            break;
        case AXMaximumValueChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::MaxValueForRange);
            tree->updateNodeProperty(*notification.first, AXPropertyName::ValueForRange);
            break;
        case AXMinimumValueChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::MinValueForRange);
            tree->updateNodeProperty(*notification.first, AXPropertyName::ValueForRange);
            break;
        case AXOrientationChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::Orientation);
            break;
        case AXPositionInSetChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::PosInSet);
            tree->updateNodeProperty(*notification.first, AXPropertyName::SupportsPosInSet);
            break;
        case AXSortDirectionChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::SortDirection);
            break;
        case AXIdAttributeChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::IdentifierAttribute);
            break;
        case AXReadOnlyStatusChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::CanSetValueAttribute);
            tree->updateNodeProperty(*notification.first, AXPropertyName::ReadOnlyValue);
            break;
        case AXRequiredStatusChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::IsRequired);
            break;
        case AXRoleDescriptionChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::RoleDescription);
            break;
        case AXRowIndexChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::AXRowIndex);
            break;
        case AXSelectedCellChanged:
        case AXSelectedStateChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::IsSelected);
            break;
        case AXSetSizeChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::SetSize);
            tree->updateNodeProperty(*notification.first, AXPropertyName::SupportsSetSize);
            break;
        case AXTableHeadersChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::ColumnHeaders);
            break;
        case AXURLChanged:
            tree->updateNodeProperty(*notification.first, AXPropertyName::URL);
            break;
        case AXActiveDescendantChanged:
        case AXRoleChanged:
        case AXColumnSpanChanged:
        case AXDescribedByChanged:
        case AXDropEffectChanged:
        case AXElementBusyChanged:
        case AXFlowToChanged:
        case AXGrabbedStateChanged:
        case AXHasPopupChanged:
        case AXInvalidStatusChanged:
        case AXIsAtomicChanged:
        case AXLevelChanged:
        case AXLiveRegionStatusChanged:
        case AXLiveRegionRelevantChanged:
        case AXPlaceholderChanged:
        case AXMenuListValueChanged:
        case AXMultiSelectableStateChanged:
        case AXPressedStateChanged:
        case AXRowSpanChanged:
        case AXSelectedChildrenChanged:
        case AXTextChanged:
        case AXValueChanged:
            updateNode(notification.first);
            break;
        case AXLanguageChanged:
        case AXRowCountChanged:
            updateNode(notification.first);
            FALLTHROUGH;
        case AXChildrenChanged:
        case AXRowCollapsed:
        case AXRowExpanded: {
            auto updatedFields = updatedObjects.get(notification.first->objectID());
            if (!updatedFields.children) {
                updatedObjects.set(notification.first->objectID(), UpdatedFields { true, updatedFields.node });
                tree->updateChildren(*notification.first);
            }
            break;
        }
        default:
            break;
        }
    }
}
#endif

void AXObjectCache::deferRecomputeIsIgnoredIfNeeded(Element* element)
{
    if (!nodeAndRendererAreValid(element))
        return;
    
    if (rendererNeedsDeferredUpdate(*element->renderer())) {
        m_deferredRecomputeIsIgnoredList.add(*element);
        return;
    }
    recomputeIsIgnored(element->renderer());
}

void AXObjectCache::deferRecomputeIsIgnored(Element* element)
{
    if (!nodeAndRendererAreValid(element))
        return;

    m_deferredRecomputeIsIgnoredList.add(*element);
}

void AXObjectCache::deferRecomputeTableIsExposed(Element* element)
{
    auto* tableElement = dynamicDowncast<HTMLTableElement>(element);
    if (!tableElement)
        return;

    m_deferredRecomputeTableIsExposedList.add(*tableElement);
    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}

void AXObjectCache::deferTextChangedIfNeeded(Node* node)
{
    if (!nodeAndRendererAreValid(node))
        return;

    if (rendererNeedsDeferredUpdate(*node->renderer())) {
        m_deferredTextChangedList.add(node);
        return;
    }
    handleTextChanged(getOrCreate(node));
}

void AXObjectCache::deferSelectedChildrenChangedIfNeeded(Element& selectElement)
{
    if (!nodeAndRendererAreValid(&selectElement))
        return;

    if (rendererNeedsDeferredUpdate(*selectElement.renderer())) {
        m_deferredSelectedChildredChangedList.add(selectElement);
        if (!m_performCacheUpdateTimer.isActive())
            m_performCacheUpdateTimer.startOneShot(0_s);
        return;
    }
    selectedChildrenChanged(&selectElement);
}

void AXObjectCache::deferTextReplacementNotificationForTextControl(HTMLTextFormControlElement& formControlElement, const String& previousValue)
{
    auto* renderer = formControlElement.renderer();
    if (!renderer)
        return;
    m_deferredTextFormControlValue.add(&formControlElement, previousValue);
}

bool isNodeAriaVisible(Node* node)
{
    if (!node)
        return false;

    // If an element is focused, it should not be hidden.
    if (is<Element>(*node) && downcast<Element>(*node).focused())
        return true;

    // ARIA Node visibility is controlled by aria-hidden
    //  1) if aria-hidden=true, the whole subtree is hidden
    //  2) if aria-hidden=false, and the object is rendered, there's no effect
    //  3) if aria-hidden=false, and the object is NOT rendered, then it must have
    //     aria-hidden=false on each parent until it gets to a rendered object
    //  3b) a text node inherits a parents aria-hidden value
    bool requiresAriaHiddenFalse = !node->renderer();
    bool ariaHiddenFalsePresent = false;
    for (Node* testNode = node; testNode; testNode = testNode->parentNode()) {
        if (is<Element>(*testNode)) {
            const AtomString& ariaHiddenValue = downcast<Element>(*testNode).attributeWithoutSynchronization(aria_hiddenAttr);
            if (equalLettersIgnoringASCIICase(ariaHiddenValue, "true"_s))
                return false;

            // We should break early when it gets to the body.
            if (testNode->hasTagName(bodyTag))
                break;

            bool ariaHiddenFalse = equalLettersIgnoringASCIICase(ariaHiddenValue, "false"_s);
            if (!testNode->renderer() && !ariaHiddenFalse)
                return false;
            if (!ariaHiddenFalsePresent && ariaHiddenFalse)
                ariaHiddenFalsePresent = true;
        }
    }
    
    return !requiresAriaHiddenFalse || ariaHiddenFalsePresent;
}

AccessibilityObject* AXObjectCache::rootWebArea()
{
    auto* root = getOrCreate(m_document.view());
    if (!root || !root->isScrollView())
        return nullptr;
    return root->webAreaObject();
}

AXTreeData AXObjectCache::treeData()
{
    ASSERT(isMainThread());

    AXTreeData data;
    TextStream stream(TextStream::LineMode::MultipleLine);

    stream << "\nAXObjectTree:\n";
    if (auto* root = get(document().view())) {
        constexpr OptionSet<AXStreamOptions> options = { AXStreamOptions::ObjectID, AXStreamOptions::ParentID, AXStreamOptions::Role, AXStreamOptions::IdentifierAttribute, AXStreamOptions::OuterHTML };
        streamSubtree(stream, root, options);
    } else
        stream << "No root!";
    data.liveTree = stream.release();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (isIsolatedTreeEnabled()) {
        stream << "\nAXIsolatedTree:\n";
        if (auto tree = getOrCreateIsolatedTree()) {
            constexpr OptionSet<AXStreamOptions> options = { AXStreamOptions::ObjectID, AXStreamOptions::ParentID };
            streamSubtree(stream, tree->rootNode(), options);
        } else
            stream << "No isolated tree!";
        data.isolatedTree = stream.release();
    }
#endif

    return data;
}

Vector<QualifiedName>& AXObjectCache::relationAttributes()
{
    static NeverDestroyed<Vector<QualifiedName>> relationAttributes = Vector<QualifiedName> {
        aria_activedescendantAttr,
        aria_controlsAttr,
        aria_describedbyAttr,
        aria_detailsAttr,
        aria_errormessageAttr,
        aria_flowtoAttr,
        aria_labelledbyAttr,
        aria_labeledbyAttr,
        aria_ownsAttr,
        headersAttr,
    };
    return relationAttributes;
}

AXRelationType AXObjectCache::symmetricRelation(AXRelationType relationType)
{
    switch (relationType) {
    case AXRelationType::ActiveDescendant:
        return AXRelationType::ActiveDescendantOf;
    case AXRelationType::ActiveDescendantOf:
        return AXRelationType::ActiveDescendant;
    case AXRelationType::ControlledBy:
        return AXRelationType::ControllerFor;
    case AXRelationType::ControllerFor:
        return AXRelationType::ControlledBy;
    case AXRelationType::DescribedBy:
        return AXRelationType::DescriptionFor;
    case AXRelationType::DescriptionFor:
        return AXRelationType::DescribedBy;
    case AXRelationType::Details:
        return AXRelationType::DetailsFor;
    case AXRelationType::DetailsFor:
        return AXRelationType::Details;
    case AXRelationType::ErrorMessage:
        return AXRelationType::ErrorMessageFor;
    case AXRelationType::ErrorMessageFor:
        return AXRelationType::ErrorMessage;
    case AXRelationType::FlowsFrom:
        return AXRelationType::FlowsTo;
    case AXRelationType::FlowsTo:
        return AXRelationType::FlowsFrom;
    case AXRelationType::Headers:
        return AXRelationType::HeaderFor;
    case AXRelationType::HeaderFor:
        return AXRelationType::Headers;
    case AXRelationType::LabelledBy:
        return AXRelationType::LabelFor;
    case AXRelationType::LabelFor:
        return AXRelationType::LabelledBy;
    case AXRelationType::OwnedBy:
        return AXRelationType::OwnerFor;
    case AXRelationType::OwnerFor:
        return AXRelationType::OwnedBy;
    case AXRelationType::None:
        return AXRelationType::None;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

AXRelationType AXObjectCache::attributeToRelationType(const QualifiedName& attribute)
{
    if (attribute == aria_activedescendantAttr)
        return AXRelationType::ActiveDescendant;
    if (attribute == aria_controlsAttr)
        return AXRelationType::ControllerFor;
    if (attribute == aria_describedbyAttr)
        return AXRelationType::DescribedBy;
    if (attribute == aria_detailsAttr)
        return AXRelationType::Details;
    if (attribute == aria_errormessageAttr)
        return AXRelationType::ErrorMessage;
    if (attribute == aria_flowtoAttr)
        return AXRelationType::FlowsTo;
    if (attribute == aria_labelledbyAttr || attribute == aria_labeledbyAttr)
        return AXRelationType::LabelledBy;
    if (attribute == aria_ownsAttr)
        return AXRelationType::OwnerFor;
    if (attribute == headersAttr)
        return AXRelationType::Headers;
    return AXRelationType::None;
}

void AXObjectCache::addRelation(Element* origin, Element* target, AXRelationType relationType)
{
    if (!origin || !target || origin == target || relationType == AXRelationType::None) {
        ASSERT_NOT_REACHED();
        return;
    }
    addRelation(getOrCreate(origin), getOrCreate(target), relationType);
}

void AXObjectCache::addRelation(AccessibilityObject* origin, AccessibilityObject* target, AXRelationType relationType, AddingSymmetricRelation addingSymmetricRelation)
{
    if (!origin || !target || origin == target || relationType == AXRelationType::None)
        return;

    auto relationsIterator = m_relations.find(origin->objectID());
    if (relationsIterator == m_relations.end()) {
        // No relations for this object, add the first one.
        m_relations.add(origin->objectID(), AXRelations { { static_cast<uint8_t>(relationType), { target->objectID() } } });
    } else if (auto targetsIterator = relationsIterator->value.find(static_cast<uint8_t>(relationType)); targetsIterator == relationsIterator->value.end()) {
        // No relation of this type for this object, add the first one.
        relationsIterator->value.add(static_cast<uint8_t>(relationType), Vector<AXID> { target->objectID() });
    } else {
        // There are already relations of this type for the object. Add the new relation.
        if (relationType == AXRelationType::ActiveDescendant
            || relationType == AXRelationType::OwnedBy) {
            // There should be only one active descendant and only one owner. Enforce that by removing any existing targets.
            targetsIterator->value.clear();
        }
        targetsIterator->value.append(target->objectID());
    }
    m_relationTargets.add(target->objectID());

    if (addingSymmetricRelation == AddingSymmetricRelation::No) {
        if (auto symmetric = symmetricRelation(relationType); symmetric != AXRelationType::None)
            addRelation(target, origin, symmetric, AddingSymmetricRelation::Yes);
    }
}

void AXObjectCache::updateRelationsIfNeeded()
{
    AXTRACE(makeString("AXObjectCache::updateRelationsIfNeeded 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));

    if (!m_relationsNeedUpdate)
        return;
    relationsNeedUpdate(false);
    AXLOG("Updating relations.");
    m_relations.clear();
    m_relationTargets.clear();
    updateRelationsForTree(m_document.rootNode());
}

void AXObjectCache::updateRelationsForTree(ContainerNode& rootNode)
{
    ASSERT(!rootNode.parentNode());
    struct RelationOrigin {
        RefPtr<Element> originElement;
        AtomString targetID;
        AXRelationType relationType;
    };

    struct RelationTarget {
        RefPtr<Element> targetElement;
        AtomString targetID;
    };

    Vector<RelationOrigin> origins;
    Vector<RelationTarget> targets;
    for (auto& element : descendantsOfType<Element>(rootNode)) {
        if (RefPtr shadowRoot = element.shadowRoot(); shadowRoot && shadowRoot->mode() != ShadowRootMode::UserAgent)
            updateRelationsForTree(*shadowRoot);
        // Collect all possible origins, i.e., elements with non-empty relation attributes.
        for (const auto& attribute : relationAttributes()) {
            if (m_document.settings().ariaReflectionForElementReferencesEnabled()) {
                if (Element::isElementReflectionAttribute(attribute)) {
                    if (auto reflectedElement = element.getElementAttribute(attribute)) {
                        addRelation(&element, reflectedElement, attributeToRelationType(attribute));
                        continue;
                    }
                } else if (Element::isElementsArrayReflectionAttribute(attribute)) {
                    if (auto reflectedElements = element.getElementsArrayAttribute(attribute)) {
                        for (auto reflectedElement : reflectedElements.value())
                            addRelation(&element, reflectedElement.get(), attributeToRelationType(attribute));
                        continue;
                    }
                }
            }

            auto& idsString = element.attributeWithoutSynchronization(attribute);
            if (idsString.isNull()) {
                if (auto* defaultARIA = element.customElementDefaultARIAIfExists()) {
                    for (auto& targetElement : defaultARIA->elementsForAttribute(element, attribute))
                        addRelation(&element, targetElement.get(), attributeToRelationType(attribute));
                }
                continue;
            }
            SpaceSplitString ids(idsString, SpaceSplitString::ShouldFoldCase::No);
            for (size_t i = 0; i < ids.size(); ++i)
                origins.append({ &element, ids[i], attributeToRelationType(attribute) });
        }

        // Collect all possible targets, i.e., elements with a non-empty id attribute.
        auto elementID = element.attributeWithoutSynchronization(idAttr);
        if (!elementID.isEmpty())
            targets.append({ &element, elementID });
    }

    for (const auto& origin : origins) {
        for (const auto& target : targets) {
            if (origin.originElement == target.targetElement) {
                // Relationship should be between different elements.
                continue;
            }

            if (origin.targetID == target.targetID)
                addRelation(origin.originElement.get(), target.targetElement.get(), origin.relationType);
        }
    }
}

void AXObjectCache::relationsNeedUpdate(bool needUpdate)
{
    m_relationsNeedUpdate = needUpdate;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (m_relationsNeedUpdate) {
        if (auto tree = AXIsolatedTree::treeForPageID(m_pageID))
            tree->relationsNeedUpdate(true);
    }
#endif
}

HashMap<AXID, AXRelations> AXObjectCache::relations()
{
    updateRelationsIfNeeded();
    return m_relations;
}

const HashSet<AXID>& AXObjectCache::relationTargetIDs()
{
    updateRelationsIfNeeded();
    return m_relationTargets;
}

std::optional<Vector<AXID>> AXObjectCache::relatedObjectIDsFor(const AXCoreObject& object, AXRelationType relationType)
{
    updateRelationsIfNeeded();
    auto relationsIterator = m_relations.find(object.objectID());
    if (relationsIterator == m_relations.end())
        return std::nullopt;

    auto targetsIterator = relationsIterator->value.find(static_cast<uint8_t>(relationType));
    if (targetsIterator == relationsIterator->value.end())
        return std::nullopt;
    return targetsIterator->value;
}

AXAttributeCacheEnabler::AXAttributeCacheEnabler(AXObjectCache* cache)
    : m_cache(cache)
{
    if (m_cache)
        m_cache->startCachingComputedObjectAttributesUntilTreeMutates();
}
    
AXAttributeCacheEnabler::~AXAttributeCacheEnabler()
{
    if (m_cache)
        m_cache->stopCachingComputedObjectAttributes();
}

#if !PLATFORM(COCOA) && !USE(ATSPI)
AXTextChange AXObjectCache::textChangeForEditType(AXTextEditType type)
{
    switch (type) {
    case AXTextEditTypeCut:
    case AXTextEditTypeDelete:
        return AXTextDeleted;
    case AXTextEditTypeInsert:
    case AXTextEditTypeDictation:
    case AXTextEditTypeTyping:
    case AXTextEditTypePaste:
        return AXTextInserted;
    case AXTextEditTypeAttributesChange:
        return AXTextAttributesChanged;
    case AXTextEditTypeUnknown:
        break;
    }
    ASSERT_NOT_REACHED();
    return AXTextInserted;
}
#endif
    
} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY)
