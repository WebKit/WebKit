/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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

#if HAVE(ACCESSIBILITY)

#include "AXObjectCache.h"

#include "AXIsolatedTree.h"
#include "AXIsolatedTreeNode.h"
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
#include "AccessibilityMediaControls.h"
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
#include "Document.h"
#include "Editing.h"
#include "Editor.h"
#include "ElementIterator.h"
#include "FocusController.h"
#include "Frame.h"
#include "HTMLAreaElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLTextFormControlElement.h"
#include "InlineElementBox.h"
#include "MathMLElement.h"
#include "Page.h"
#include "RenderAttachment.h"
#include "RenderLineBreak.h"
#include "RenderListBox.h"
#include "RenderMathMLOperator.h"
#include "RenderMenuList.h"
#include "RenderMeter.h"
#include "RenderProgress.h"
#include "RenderSVGRoot.h"
#include "RenderSlider.h"
#include "RenderTable.h"
#include "RenderTableCell.h"
#include "RenderTableRow.h"
#include "RenderView.h"
#include "SVGElement.h"
#include "ScriptDisallowedScope.h"
#include "ScrollView.h"
#include "TextBoundaries.h"
#include "TextControlInnerElements.h"
#include "TextIterator.h"
#include <wtf/DataLog.h>
#include <wtf/SetForScope.h>

#if ENABLE(VIDEO)
#include "MediaControlElements.h"
#endif

#if COMPILER(MSVC)
// See https://msdn.microsoft.com/en-us/library/1wea5zwe.aspx
#pragma warning(disable: 4701)
#endif

namespace WebCore {

using namespace HTMLNames;

const AXID InvalidAXID = 0;

// Post value change notifications for password fields or elements contained in password fields at a 40hz interval to thwart analysis of typing cadence
static const Seconds accessibilityPasswordValueChangeNotificationInterval { 25_ms };
static const Seconds accessibilityLiveRegionChangedNotificationInterval { 20_ms };
static const Seconds accessibilityFocusModalNodeNotificationInterval { 50_ms };
    
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
        m_replacedRange.startIndex.value = indexForVisiblePosition(selection.start(), m_replacedRange.startIndex.scope);
        if (selection.isRange()) {
            m_replacedText = AccessibilityObject::stringForVisiblePositionRange(selection);
            m_replacedRange.endIndex.value = indexForVisiblePosition(selection.end(), m_replacedRange.endIndex.scope);
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
    , m_notificationPostTimer(*this, &AXObjectCache::notificationPostTimerFired)
    , m_passwordNotificationPostTimer(*this, &AXObjectCache::passwordNotificationPostTimerFired)
    , m_liveRegionChangedPostTimer(*this, &AXObjectCache::liveRegionChangedNotificationPostTimerFired)
    , m_focusModalNodeTimer(*this, &AXObjectCache::focusModalNodeTimerFired)
    , m_currentModalNode(nullptr)
    , m_performCacheUpdateTimer(*this, &AXObjectCache::performCacheUpdateTimerFired)
{
    findModalNodes();
}

AXObjectCache::~AXObjectCache()
{
    m_notificationPostTimer.stop();
    m_liveRegionChangedPostTimer.stop();
    m_focusModalNodeTimer.stop();
    m_performCacheUpdateTimer.stop();

    for (const auto& object : m_objects.values()) {
        detachWrapper(object.get(), AccessibilityDetachmentType::CacheDestroyed);
        object->detach(AccessibilityDetachmentType::CacheDestroyed);
        object->setAXObjectID(0);
    }
}

void AXObjectCache::findModalNodes()
{
    // Traverse the DOM tree to look for the aria-modal=true nodes.
    for (Element* element = ElementTraversal::firstWithin(document().rootNode()); element; element = ElementTraversal::nextIncludingPseudo(*element)) {
        
        // Must have dialog or alertdialog role
        if (!nodeHasRole(element, "dialog") && !nodeHasRole(element, "alertdialog"))
            continue;
        if (!equalLettersIgnoringASCIICase(element->attributeWithoutSynchronization(aria_modalAttr), "true"))
            continue;
        
        m_modalNodesSet.add(element);
    }
    
    // Set the current valid aria-modal node if possible.
    updateCurrentModalNode();
}

void AXObjectCache::updateCurrentModalNode()
{
    // There might be multiple nodes with aria-modal=true set.
    // We use this function to pick the one we want.
    m_currentModalNode = nullptr;
    if (m_modalNodesSet.isEmpty())
        return;
    
    // We only care about the nodes which are visible.
    ListHashSet<RefPtr<Node>> visibleNodes;
    for (auto& object : m_modalNodesSet) {
        if (isNodeVisible(object))
            visibleNodes.add(object);
    }
    
    if (visibleNodes.isEmpty())
        return;
    
    // If any of the node are keyboard focused, we want to pick that.
    Node* focusedNode = document().focusedElement();
    for (auto& object : visibleNodes) {
        if (focusedNode != nullptr && focusedNode->isDescendantOf(object.get())) {
            m_currentModalNode = object.get();
            break;
        }
    }
    
    // If none of the nodes are focused, we want to pick the last dialog in the DOM.
    if (!m_currentModalNode)
        m_currentModalNode = visibleNodes.last().get();
}

bool AXObjectCache::isNodeVisible(Node* node) const
{
    if (!is<Element>(node))
        return false;
    
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return false;
    const RenderStyle& style = renderer->style();
    if (style.display() == DisplayType::None || style.visibility() != Visibility::Visible)
        return false;
    
    // We also need to consider aria hidden status.
    if (!isNodeAriaVisible(node))
        return false;
    
    return true;
}

Node* AXObjectCache::modalNode()
{
    // This function returns the valid aria modal node.
    if (m_modalNodesSet.isEmpty())
        return nullptr;
    
    // Check the current valid aria modal node first.
    // Usually when one dialog sets aria-modal=true, that dialog is the one we want.
    if (isNodeVisible(m_currentModalNode))
        return m_currentModalNode;
    
    // Recompute the valid aria modal node when m_currentModalNode is null or hidden.
    updateCurrentModalNode();
    return isNodeVisible(m_currentModalNode) ? m_currentModalNode : nullptr;
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
            return child.get();
    }    
    
    return nullptr;
}
    
AccessibilityObject* AXObjectCache::focusedUIElementForPage(const Page* page)
{
    if (!gAccessibilityEnabled)
        return nullptr;

    // get the focused node in the page
    Document* focusedDocument = page->focusController().focusedOrMainFrame().document();
    Element* focusedElement = focusedDocument->focusedElement();
    if (is<HTMLAreaElement>(focusedElement))
        return focusedImageMapUIElement(downcast<HTMLAreaElement>(focusedElement));

    AccessibilityObject* obj = focusedDocument->axObjectCache()->getOrCreate(focusedElement ? static_cast<Node*>(focusedElement) : focusedDocument);
    if (!obj)
        return nullptr;

    if (obj->shouldFocusActiveDescendant()) {
        if (AccessibilityObject* descendant = obj->activeDescendant())
            obj = descendant;
    }

    // the HTML element, for example, is focusable but has an AX object that is ignored
    if (obj->accessibilityIsIgnored())
        obj = obj->parentObjectUnignored();

    return obj;
}

AccessibilityObject* AXObjectCache::get(Widget* widget)
{
    if (!widget)
        return nullptr;
        
    AXID axID = m_widgetObjectMapping.get(widget);
    ASSERT(!HashTraits<AXID>::isDeletedValue(axID));
    if (!axID)
        return nullptr;
    
    return m_objects.get(axID);    
}
    
AccessibilityObject* AXObjectCache::get(RenderObject* renderer)
{
    if (!renderer)
        return nullptr;
    
    AXID axID = m_renderObjectMapping.get(renderer);
    ASSERT(!HashTraits<AXID>::isDeletedValue(axID));
    if (!axID)
        return nullptr;

    return m_objects.get(axID);    
}

AccessibilityObject* AXObjectCache::get(Node* node)
{
    if (!node)
        return nullptr;

    AXID renderID = node->renderer() ? m_renderObjectMapping.get(node->renderer()) : 0;
    ASSERT(!HashTraits<AXID>::isDeletedValue(renderID));

    AXID nodeID = m_nodeObjectMapping.get(node);
    ASSERT(!HashTraits<AXID>::isDeletedValue(nodeID));

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
// FIXME: This should take a const char*, but one caller passes nullAtom().
bool nodeHasRole(Node* node, const String& role)
{
    if (!node || !is<Element>(node))
        return false;

    auto& roleValue = downcast<Element>(*node).attributeWithoutSynchronization(roleAttr);
    if (role.isNull())
        return roleValue.isEmpty();
    if (roleValue.isEmpty())
        return false;

    return SpaceSplitString(roleValue, true).contains(role);
}

static Ref<AccessibilityObject> createFromRenderer(RenderObject* renderer)
{
    // FIXME: How could renderer->node() ever not be an Element?
    Node* node = renderer->node();

    // If the node is aria role="list" or the aria role is empty and its a
    // ul/ol/dl type (it shouldn't be a list if aria says otherwise).
    if (node && ((nodeHasRole(node, "list") || nodeHasRole(node, "directory"))
                      || (nodeHasRole(node, nullAtom()) && (node->hasTagName(ulTag) || node->hasTagName(olTag) || node->hasTagName(dlTag)))))
        return AccessibilityList::create(renderer);

    // aria tables
    if (nodeHasRole(node, "grid") || nodeHasRole(node, "treegrid") || nodeHasRole(node, "table"))
        return AccessibilityARIAGrid::create(renderer);
    if (nodeHasRole(node, "row"))
        return AccessibilityARIAGridRow::create(renderer);
    if (nodeHasRole(node, "gridcell") || nodeHasRole(node, "cell") || nodeHasRole(node, "columnheader") || nodeHasRole(node, "rowheader"))
        return AccessibilityARIAGridCell::create(renderer);

    // aria tree
    if (nodeHasRole(node, "tree"))
        return AccessibilityTree::create(renderer);
    if (nodeHasRole(node, "treeitem"))
        return AccessibilityTreeItem::create(renderer);

    if (node && is<HTMLLabelElement>(node) && nodeHasRole(node, nullAtom()))
        return AccessibilityLabel::create(renderer);

#if PLATFORM(IOS_FAMILY)
    if (is<HTMLMediaElement>(node) && nodeHasRole(node, nullAtom()))
        return AccessibilityMediaObject::create(renderer);
#endif

#if ENABLE(VIDEO)
    // media controls
    if (node && node->isMediaControlElement())
        return AccessibilityMediaControl::create(renderer);
#endif

    if (is<RenderSVGRoot>(*renderer))
        return AccessibilitySVGRoot::create(renderer);
    
    if (is<SVGElement>(node))
        return AccessibilitySVGElement::create(renderer);

#if ENABLE(MATHML)
    // The mfenced element creates anonymous RenderMathMLOperators which should be treated
    // as MathML elements and assigned the MathElementRole so that platform logic regarding
    // inclusion and role mapping is not bypassed.
    bool isAnonymousOperator = renderer->isAnonymous() && is<RenderMathMLOperator>(*renderer);
    if (isAnonymousOperator || is<MathMLElement>(node))
        return AccessibilityMathMLElement::create(renderer, isAnonymousOperator);
#endif

    if (is<RenderBoxModelObject>(*renderer)) {
        RenderBoxModelObject& cssBox = downcast<RenderBoxModelObject>(*renderer);
        if (is<RenderListBox>(cssBox))
            return AccessibilityListBox::create(&downcast<RenderListBox>(cssBox));
        if (is<RenderMenuList>(cssBox))
            return AccessibilityMenuList::create(&downcast<RenderMenuList>(cssBox));

        // standard tables
        if (is<RenderTable>(cssBox))
            return AccessibilityTable::create(&downcast<RenderTable>(cssBox));
        if (is<RenderTableRow>(cssBox))
            return AccessibilityTableRow::create(&downcast<RenderTableRow>(cssBox));
        if (is<RenderTableCell>(cssBox))
            return AccessibilityTableCell::create(&downcast<RenderTableCell>(cssBox));

        // progress bar
        if (is<RenderProgress>(cssBox))
            return AccessibilityProgressIndicator::create(&downcast<RenderProgress>(cssBox));

#if ENABLE(ATTACHMENT_ELEMENT)
        if (is<RenderAttachment>(cssBox))
            return AccessibilityAttachment::create(&downcast<RenderAttachment>(cssBox));
#endif
#if ENABLE(METER_ELEMENT)
        if (is<RenderMeter>(cssBox))
            return AccessibilityProgressIndicator::create(&downcast<RenderMeter>(cssBox));
#endif

        // input type=range
        if (is<RenderSlider>(cssBox))
            return AccessibilitySlider::create(&downcast<RenderSlider>(cssBox));
    }

    return AccessibilityRenderObject::create(renderer);
}

static Ref<AccessibilityObject> createFromNode(Node* node)
{
    return AccessibilityNodeObject::create(node);
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

    // Catch the case if an (unsupported) widget type is used. Only FrameView and ScrollBar are supported now.
    ASSERT(newObj);
    if (!newObj)
        return nullptr;

    getAXID(newObj.get());
    
    m_widgetObjectMapping.set(widget, newObj->axObjectID());
    m_objects.set(newObj->axObjectID(), newObj);    
    newObj->init();
    attachWrapper(newObj.get());
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
    
    // It's only allowed to create an AccessibilityObject from a Node if it's in a canvas subtree.
    // Or if it's a hidden element, but we still want to expose it because of other ARIA attributes.
    bool inCanvasSubtree = lineageOfType<HTMLCanvasElement>(*node->parentElement()).first();
    bool isHidden = isNodeAriaVisible(node);

    bool insideMeterElement = false;
#if ENABLE(METER_ELEMENT)
    insideMeterElement = is<HTMLMeterElement>(*node->parentElement());
#endif
    
    if (!inCanvasSubtree && !isHidden && !insideMeterElement)
        return nullptr;

    // Fallback content is only focusable as long as the canvas is displayed and visible.
    // Update the style before Element::isFocusable() gets called.
    if (inCanvasSubtree)
        node->document().updateStyleIfNeeded();

    RefPtr<AccessibilityObject> newObj = createFromNode(node);

    // Will crash later if we have two objects for the same node.
    ASSERT(!get(node));

    getAXID(newObj.get());

    m_nodeObjectMapping.set(node, newObj->axObjectID());
    m_objects.set(newObj->axObjectID(), newObj);
    newObj->init();
    attachWrapper(newObj.get());
    newObj->setLastKnownIsIgnoredValue(newObj->accessibilityIsIgnored());
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

    RefPtr<AccessibilityObject> newObj = createFromRenderer(renderer);

    // Will crash later if we have two objects for the same renderer.
    ASSERT(!get(renderer));

    getAXID(newObj.get());

    m_renderObjectMapping.set(renderer, newObj->axObjectID());
    m_objects.set(newObj->axObjectID(), newObj);
    newObj->init();
    attachWrapper(newObj.get());
    newObj->setLastKnownIsIgnoredValue(newObj->accessibilityIsIgnored());
    // Sometimes asking accessibilityIsIgnored() will cause the newObject to be deallocated, and then
    // it will disappear when this function is finished, leading to a use-after-free.
    if (newObj->isDetached())
        return nullptr;
    
    return newObj.get();
}
    
AccessibilityObject* AXObjectCache::rootObject()
{
    if (!gAccessibilityEnabled)
        return nullptr;

    return getOrCreate(m_document.view());
}

AccessibilityObject* AXObjectCache::rootObjectForFrame(Frame* frame)
{
    if (!gAccessibilityEnabled)
        return nullptr;

    if (!frame)
        return nullptr;
    return getOrCreate(frame->view());
}    
    
AccessibilityObject* AXObjectCache::getOrCreate(AccessibilityRole role)
{
    RefPtr<AccessibilityObject> obj = nullptr;
    
    // will be filled in...
    switch (role) {
    case AccessibilityRole::ListBoxOption:
        obj = AccessibilityListBoxOption::create();
        break;
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
    case AccessibilityRole::MenuListOption:
        obj = AccessibilityMenuListOption::create();
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
    
    if (obj)
        getAXID(obj.get());
    else
        return nullptr;

    m_objects.set(obj->axObjectID(), obj);    
    obj->init();
    attachWrapper(obj.get());
    return obj.get();
}

void AXObjectCache::remove(AXID axID)
{
    if (!axID)
        return;

    auto object = m_objects.take(axID);
    if (!object)
        return;

    detachWrapper(object.get(), AccessibilityDetachmentType::ElementDestroyed);
    object->detach(AccessibilityDetachmentType::ElementDestroyed, this);
    object->setAXObjectID(0);

    m_idsInUse.remove(axID);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (auto pageID = m_document.pageID())
        AXIsolatedTree::treeForPageID(*pageID)->removeNode(axID);
#endif

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
    if (is<Element>(node)) {
        m_deferredRecomputeIsIgnoredList.remove(downcast<Element>(&node));
        m_deferredSelectedChildredChangedList.remove(downcast<Element>(&node));
        m_deferredChildrenChangedNodeList.remove(&node);
        m_deferredTextFormControlValue.remove(downcast<Element>(&node));
        m_deferredAttributeChange.remove(downcast<Element>(&node));
    }
    m_deferredTextChangedList.remove(&node);
    // Remove the entry if the new focused node is being removed.
    m_deferredFocusedNodeChange.removeAllMatching([&node](auto& entry) -> bool {
        return entry.second == &node;
    });
    removeNodeForUse(node);

    remove(m_nodeObjectMapping.take(&node));

    if (m_currentModalNode == &node)
        m_currentModalNode = nullptr;
    m_modalNodesSet.remove(&node);

    remove(node.renderer());
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
    static AXID lastUsedID = 0;

    // Generate a new ID.
    AXID objID = lastUsedID;
    do {
        ++objID;
    } while (!objID || HashTraits<AXID>::isDeletedValue(objID) || m_idsInUse.contains(objID));

    lastUsedID = objID;

    return objID;
}
#endif

AXID AXObjectCache::getAXID(AccessibilityObject* obj)
{
    // check for already-assigned ID
    AXID objID = obj->axObjectID();
    if (objID) {
        ASSERT(m_idsInUse.contains(objID));
        return objID;
    }

    objID = platformGenerateAXID();

    m_idsInUse.add(objID);
    obj->setAXObjectID(objID);
    
    return objID;
}

void AXObjectCache::textChanged(Node* node)
{
    textChanged(getOrCreate(node));
}

void AXObjectCache::textChanged(AccessibilityObject* obj)
{
    if (!obj)
        return;

    bool parentAlreadyExists = obj->parentObjectIfExists();
    obj->textChanged();
    postNotification(obj, obj->document(), AXObjectCache::AXTextChanged);
    if (parentAlreadyExists)
        obj->notifyIfIgnoredValueChanged();
}

void AXObjectCache::updateCacheAfterNodeIsAttached(Node* node)
{
    // Calling get() will update the AX object if we had an AccessibilityNodeObject but now we need
    // an AccessibilityRenderObject, because it was reparented to a location outside of a canvas.
    get(node);
}

void AXObjectCache::handleMenuOpened(Node* node)
{
    if (!node || !node->renderer() || !nodeHasRole(node, "menu"))
        return;
    
    postNotification(getOrCreate(node), &document(), AXMenuOpened);
}
    
void AXObjectCache::handleLiveRegionCreated(Node* node)
{
    if (!is<Element>(node) || !node->renderer())
        return;
    
    Element* element = downcast<Element>(node);
    String liveRegionStatus = element->attributeWithoutSynchronization(aria_liveAttr);
    if (liveRegionStatus.isEmpty()) {
        const AtomicString& ariaRole = element->attributeWithoutSynchronization(roleAttr);
        if (!ariaRole.isEmpty())
            liveRegionStatus = AccessibilityObject::defaultLiveRegionStatusForRole(AccessibilityObject::ariaRoleToWebCoreRole(ariaRole));
    }
    
    if (AccessibilityObject::liveRegionStatusIsEnabled(liveRegionStatus))
        postNotification(getOrCreate(node), &document(), AXLiveRegionCreated);
}
    
void AXObjectCache::childrenChanged(Node* node, Node* newChild)
{
    if (newChild)
        m_deferredChildrenChangedNodeList.add(newChild);

    childrenChanged(get(node));
}

void AXObjectCache::childrenChanged(RenderObject* renderer, RenderObject* newChild)
{
    if (!renderer)
        return;

    if (newChild && newChild->node())
        m_deferredChildrenChangedNodeList.add(newChild->node());

    childrenChanged(get(renderer));
}

void AXObjectCache::childrenChanged(AccessibilityObject* obj)
{
    if (!obj)
        return;

    m_deferredChildredChangedList.add(obj);
}
    
void AXObjectCache::notificationPostTimerFired()
{
    Ref<Document> protectorForCacheOwner(m_document);
    m_notificationPostTimer.stop();
    
    // In tests, posting notifications has a tendency to immediately queue up other notifications, which can lead to unexpected behavior
    // when the notification list is cleared at the end. Instead copy this list at the start.
    auto notifications = WTFMove(m_notificationsToPost);
    
    for (const auto& note : notifications) {
        AccessibilityObject* obj = note.first.get();
        if (!obj->axObjectID())
            continue;

        if (!obj->axObjectCache())
            continue;
        
#ifndef NDEBUG
        // Make sure none of the render views are in the process of being layed out.
        // Notifications should only be sent after the renderer has finished
        if (is<AccessibilityRenderObject>(*obj)) {
            if (auto* renderer = downcast<AccessibilityRenderObject>(*obj).renderer())
                ASSERT(!renderer->view().frameView().layoutContext().layoutState());
        }
#endif

        AXNotification notification = note.second;
        
        // Ensure that this menu really is a menu. We do this check here so that we don't have to create
        // the axChildren when the menu is marked as opening.
        if (notification == AXMenuOpened) {
            obj->updateChildrenIfNecessary();
            if (obj->roleValue() != AccessibilityRole::Menu)
                continue;
        }
        
        postPlatformNotification(obj, notification);

        if (notification == AXChildrenChanged && obj->parentObjectIfExists() && obj->lastKnownIsIgnoredValue() != obj->accessibilityIsIgnored())
            childrenChanged(obj->parentObject());
    }
}

void AXObjectCache::passwordNotificationPostTimerFired()
{
#if PLATFORM(COCOA)
    m_passwordNotificationPostTimer.stop();

    // In tests, posting notifications has a tendency to immediately queue up other notifications, which can lead to unexpected behavior
    // when the notification list is cleared at the end. Instead copy this list at the start.
    auto notifications = WTFMove(m_passwordNotificationsToPost);

    for (auto& notification : notifications)
        postTextStateChangePlatformNotification(notification.get(), AXTextEditTypeInsert, " ", VisiblePosition());
#endif
}
    
void AXObjectCache::postNotification(RenderObject* renderer, AXNotification notification, PostTarget postTarget, PostType postType)
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
    
    postNotification(object.get(), &renderer->document(), notification, postTarget, postType);
}

void AXObjectCache::postNotification(Node* node, AXNotification notification, PostTarget postTarget, PostType postType)
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
    
    postNotification(object.get(), &node->document(), notification, postTarget, postType);
}

void AXObjectCache::postNotification(AccessibilityObject* object, Document* document, AXNotification notification, PostTarget postTarget, PostType postType)
{
    stopCachingComputedObjectAttributes();

    if (object && postTarget == TargetObservableParent)
        object = object->observableObject();

    if (!object && document)
        object = get(document->renderView());

    if (!object)
        return;

    if (postType == PostAsynchronously) {
        m_notificationsToPost.append(std::make_pair(object, notification));
        if (!m_notificationPostTimer.isActive())
            m_notificationPostTimer.startOneShot(0_s);
    } else
        postPlatformNotification(object, notification);
}

void AXObjectCache::checkedStateChanged(Node* node)
{
    postNotification(node, AXObjectCache::AXCheckedStateChanged);
}

void AXObjectCache::handleMenuItemSelected(Node* node)
{
    if (!node)
        return;
    
    if (!nodeHasRole(node, "menuitem") && !nodeHasRole(node, "menuitemradio") && !nodeHasRole(node, "menuitemcheckbox"))
        return;
    
    if (!downcast<Element>(*node).focused() && !equalLettersIgnoringASCIICase(downcast<Element>(*node).attributeWithoutSynchronization(aria_selectedAttr), "true"))
        return;
    
    postNotification(getOrCreate(node), &document(), AXMenuListItemSelected);
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
    
void AXObjectCache::handleFocusedUIElementChanged(Node* oldNode, Node* newNode)
{
    handleMenuItemSelected(newNode);
    platformHandleFocusedUIElementChanged(oldNode, newNode);
}
    
void AXObjectCache::selectedChildrenChanged(Node* node)
{
    handleMenuItemSelected(node);
    
    // postTarget is TargetObservableParent so that you can pass in any child of an element and it will go up the parent tree
    // to find the container which should send out the notification.
    postNotification(node, AXSelectedChildrenChanged, TargetObservableParent);
}

void AXObjectCache::selectedChildrenChanged(RenderObject* renderer)
{
    if (renderer)
        handleMenuItemSelected(renderer->node());

    // postTarget is TargetObservableParent so that you can pass in any child of an element and it will go up the parent tree
    // to find the container which should send out the notification.
    postNotification(renderer, AXSelectedChildrenChanged, TargetObservableParent);
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

static bool isPasswordFieldOrContainedByPasswordField(AccessibilityObject* object)
{
    return object && (object->isPasswordField() || object->isContainedByPasswordField());
}

void AXObjectCache::postTextStateChangeNotification(Node* node, const AXTextStateChangeIntent& intent, const VisibleSelection& selection)
{
    if (!node)
        return;

#if PLATFORM(COCOA)
    stopCachingComputedObjectAttributes();

    postTextStateChangeNotification(getOrCreate(node), intent, selection);
#else
    postNotification(node->renderer(), AXObjectCache::AXSelectedTextChanged, TargetObservableParent);
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

#if PLATFORM(COCOA)
    AccessibilityObject* object = getOrCreate(node);
    if (object && object->accessibilityIsIgnored()) {
        if (position.atLastEditingPositionForNode()) {
            if (AccessibilityObject* nextSibling = object->nextSiblingUnignored(1))
                object = nextSibling;
        } else if (position.atFirstEditingPositionForNode()) {
            if (AccessibilityObject* previousSibling = object->previousSiblingUnignored(1))
                object = previousSibling;
        }
    }

    postTextStateChangeNotification(object, intent, selection);
#else
    postTextStateChangeNotification(node, intent, selection);
#endif
}

void AXObjectCache::postTextStateChangeNotification(AccessibilityObject* object, const AXTextStateChangeIntent& intent, const VisibleSelection& selection)
{
    stopCachingComputedObjectAttributes();

#if PLATFORM(COCOA)
    if (object) {
        if (isPasswordFieldOrContainedByPasswordField(object))
            return;

        if (auto observableObject = object->observableObject())
            object = observableObject;
    }

    const AXTextStateChangeIntent& newIntent = (intent.type == AXTextStateChangeTypeUnknown || (m_isSynchronizingSelection && m_textSelectionIntent.type != AXTextStateChangeTypeUnknown)) ? m_textSelectionIntent : intent;
    postTextStateChangePlatformNotification(object, newIntent, selection);
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
    if (!node)
        return;
    if (type == AXTextEditTypeUnknown)
        return;

    stopCachingComputedObjectAttributes();

    AccessibilityObject* object = getOrCreate(node);
#if PLATFORM(COCOA)
    if (object) {
        if (enqueuePasswordValueChangeNotification(object))
            return;
        object = object->observableObject();
    }

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
#if PLATFORM(COCOA)
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
#if PLATFORM(COCOA)
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

    m_liveRegionChangedPostTimer.startOneShot(accessibilityLiveRegionChangedNotificationInterval);
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

void AXObjectCache::focusModalNode()
{
    if (m_focusModalNodeTimer.isActive())
        m_focusModalNodeTimer.stop();
    
    m_focusModalNodeTimer.startOneShot(accessibilityFocusModalNodeNotificationInterval);
}

void AXObjectCache::focusModalNodeTimerFired()
{
    if (!m_currentModalNode)
        return;
    
    // Don't set focus if we are already focusing onto some element within
    // the dialog.
    if (m_currentModalNode->contains(document().focusedElement()))
        return;
    
    if (AccessibilityObject* currentModalNodeObject = getOrCreate(m_currentModalNode)) {
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
    if (AccessibilityObject* obj = get(node))
        obj->handleAriaExpandedChanged();
}
    
void AXObjectCache::handleActiveDescendantChanged(Node* node)
{
    if (AccessibilityObject* obj = getOrCreate(node))
        obj->handleActiveDescendantChanged();
}

void AXObjectCache::handleAriaRoleChanged(Node* node)
{
    stopCachingComputedObjectAttributes();

    // Don't make an AX object unless it's needed
    if (AccessibilityObject* obj = get(node)) {
        obj->updateAccessibilityRole();
        obj->notifyIfIgnoredValueChanged();
    }
}

void AXObjectCache::deferAttributeChangeIfNeeded(const QualifiedName& attrName, Element* element)
{
    if (nodeAndRendererAreValid(element) && rendererNeedsDeferredUpdate(*element->renderer()))
        m_deferredAttributeChange.add(element, attrName);
    else
        handleAttributeChange(attrName, element);
}
    
bool AXObjectCache::shouldProcessAttributeChange(const QualifiedName& attrName, Element* element)
{
    if (!element)
        return false;
    
    // aria-modal ends up affecting sub-trees that are being shown/hidden so it's likely that
    // an AT would not have accessed this node yet.
    if (attrName == aria_modalAttr)
        return true;
    
    // If an AXObject has yet to be created, then there's no need to process attribute changes.
    // Some of these notifications are processed on the parent, so allow that to proceed as well
    if (get(element) || get(element->parentNode()))
        return true;

    return false;
}
    
void AXObjectCache::handleAttributeChange(const QualifiedName& attrName, Element* element)
{
    if (!shouldProcessAttributeChange(attrName, element))
        return;
    
    if (attrName == roleAttr)
        handleAriaRoleChanged(element);
    else if (attrName == altAttr || attrName == titleAttr)
        textChanged(element);
    else if (attrName == forAttr && is<HTMLLabelElement>(*element))
        labelChanged(element);

    if (!attrName.localName().string().startsWith("aria-"))
        return;

    if (attrName == aria_activedescendantAttr)
        handleActiveDescendantChanged(element);
    else if (attrName == aria_busyAttr)
        postNotification(element, AXObjectCache::AXElementBusyChanged);
    else if (attrName == aria_valuenowAttr || attrName == aria_valuetextAttr)
        postNotification(element, AXObjectCache::AXValueChanged);
    else if (attrName == aria_labelAttr || attrName == aria_labeledbyAttr || attrName == aria_labelledbyAttr)
        textChanged(element);
    else if (attrName == aria_checkedAttr)
        checkedStateChanged(element);
    else if (attrName == aria_selectedAttr)
        selectedChildrenChanged(element);
    else if (attrName == aria_expandedAttr)
        handleAriaExpandedChange(element);
    else if (attrName == aria_hiddenAttr)
        childrenChanged(element->parentNode(), element);
    else if (attrName == aria_invalidAttr)
        postNotification(element, AXObjectCache::AXInvalidStatusChanged);
    else if (attrName == aria_modalAttr)
        handleModalChange(element);
    else if (attrName == aria_currentAttr)
        postNotification(element, AXObjectCache::AXCurrentChanged);
    else if (attrName == aria_disabledAttr)
        postNotification(element, AXObjectCache::AXDisabledStateChanged);
    else if (attrName == aria_pressedAttr)
        postNotification(element, AXObjectCache::AXPressedStateChanged);
    else if (attrName == aria_readonlyAttr)
        postNotification(element, AXObjectCache::AXReadOnlyStatusChanged);
    else if (attrName == aria_requiredAttr)
        postNotification(element, AXObjectCache::AXRequiredStatusChanged);
    else
        postNotification(element, AXObjectCache::AXAriaAttributeChanged);
}

void AXObjectCache::handleModalChange(Node* node)
{
    if (!is<Element>(node))
        return;
    
    if (!nodeHasRole(node, "dialog") && !nodeHasRole(node, "alertdialog"))
        return;
    
    stopCachingComputedObjectAttributes();
    if (equalLettersIgnoringASCIICase(downcast<Element>(*node).attributeWithoutSynchronization(aria_modalAttr), "true")) {
        // Add the newly modified node to the modal nodes set, and set it to be the current valid aria modal node.
        // We will recompute the current valid aria modal node in modalNode() when this node is not visible.
        m_modalNodesSet.add(node);
        m_currentModalNode = node;
    } else {
        // Remove the node from the modal nodes set. There might be other visible modal nodes, so we recompute here.
        m_modalNodesSet.remove(node);
        updateCurrentModalNode();
    }
    if (m_currentModalNode)
        focusModalNode();
    
    startCachingComputedObjectAttributesUntilTreeMutates();
}

void AXObjectCache::labelChanged(Element* element)
{
    ASSERT(is<HTMLLabelElement>(*element));
    auto correspondingControl = downcast<HTMLLabelElement>(*element).control();
    deferTextChangedIfNeeded(correspondingControl.get());
}

void AXObjectCache::recomputeIsIgnored(RenderObject* renderer)
{
    if (AccessibilityObject* obj = get(renderer))
        obj->notifyIfIgnoredValueChanged();
}

void AXObjectCache::startCachingComputedObjectAttributesUntilTreeMutates()
{
    if (!m_computedObjectAttributeCache)
        m_computedObjectAttributeCache = std::make_unique<AXComputedObjectAttributeCache>();
}

void AXObjectCache::stopCachingComputedObjectAttributes()
{
    m_computedObjectAttributeCache = nullptr;
}

VisiblePosition AXObjectCache::visiblePositionForTextMarkerData(TextMarkerData& textMarkerData)
{
    if (!isNodeInUse(textMarkerData.node))
        return VisiblePosition();
    
    // FIXME: Accessability should make it clear these are DOM-compliant offsets or store Position objects.
    VisiblePosition visiblePos = VisiblePosition(createLegacyEditingPosition(textMarkerData.node, textMarkerData.offset), textMarkerData.affinity);
    Position deepPos = visiblePos.deepEquivalent();
    if (deepPos.isNull())
        return VisiblePosition();
    
    RenderObject* renderer = deepPos.deprecatedNode()->renderer();
    if (!renderer)
        return VisiblePosition();
    
    AXObjectCache* cache = renderer->document().axObjectCache();
    if (cache && !cache->m_idsInUse.contains(textMarkerData.axID))
        return VisiblePosition();

    return visiblePos;
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
    if (textMarkerData.affinity == UPSTREAM)
        return previousCharacterOffset(result, false);
    return result;
}

CharacterOffset AXObjectCache::traverseToOffsetInRange(RefPtr<Range>range, int offset, TraverseOption option, bool stayWithinRange)
{
    if (!range)
        return CharacterOffset();
    
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
    
    TextIterator iterator(range.get(), doNotEnterTextControls ? TextIteratorDefaultBehavior : TextIteratorEntersTextControls);
    
    // When the range has zero length, there might be replaced node or brTag that we need to increment the characterOffset.
    if (iterator.atEnd()) {
        currentNode = &range->startContainer();
        lastStartOffset = range->startOffset();
        if (offset > 0 || toNodeEnd) {
            if (AccessibilityObject::replacedNodeNeedsCharacter(currentNode) || (currentNode->renderer() && currentNode->renderer()->isBR()))
                cumulativeOffset++;
            lastLength = cumulativeOffset;
            
            // When going backwards, stayWithinRange is false.
            // Here when we don't have any character to move and we are going backwards, we traverse to the previous node.
            if (!lastLength && toNodeEnd && !stayWithinRange) {
                if (Node* preNode = previousNode(currentNode))
                    return traverseToOffsetInRange(rangeForNodeContents(preNode), offset, option);
                return CharacterOffset();
            }
        }
    }
    
    // Sometimes text contents in a node are splitted into several iterations, so that iterator.range()->startOffset()
    // might not be the correct character count. Here we use a previousNode object to keep track of that.
    Node* previousNode = nullptr;
    for (; !iterator.atEnd(); iterator.advance()) {
        int currentLength = iterator.text().length();
        bool hasReplacedNodeOrBR = false;
        
        Node& node = iterator.range()->startContainer();
        currentNode = &node;
        
        // When currentLength == 0, we check if there's any replaced node.
        // If not, we skip the node with no length.
        if (!currentLength) {
            int subOffset = iterator.range()->startOffset();
            Node* childNode = node.traverseToChildAt(subOffset);
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
                    int subOffset = iterator.range()->startOffset();
                    Node* childNode = node.traverseToChildAt(subOffset);
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
            lastStartOffset = iterator.range()->endOffset() - lastLength;
        }
        else {
            lastLength = currentLength;
            lastStartOffset = hasReplacedNodeOrBR ? 0 : iterator.range()->startOffset();
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
    if (toNodeEnd && currentNode->isTextNode() && currentNode == &range->endContainer() && static_cast<int>(range->endOffset()) < lastStartOffset + offsetInCharacter)
        offsetInCharacter = range->endOffset() - lastStartOffset;
    
    return CharacterOffset(currentNode, lastStartOffset, offsetInCharacter, remaining);
}

int AXObjectCache::lengthForRange(Range* range)
{
    if (!range)
        return -1;
    
    int length = 0;
    for (TextIterator it(range); !it.atEnd(); it.advance()) {
        // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
        if (it.text().length())
            length += it.text().length();
        else {
            // locate the node and starting offset for this replaced range
            Node& node = it.range()->startContainer();
            int offset = it.range()->startOffset();
            if (AccessibilityObject::replacedNodeNeedsCharacter(node.traverseToChildAt(offset)))
                ++length;
        }
    }
        
    return length;
}

RefPtr<Range> AXObjectCache::rangeForNodeContents(Node* node)
{
    if (!node)
        return nullptr;
    Document* document = &node->document();
    if (!document)
        return nullptr;
    auto range = Range::create(*document);
    if (AccessibilityObject::replacedNodeNeedsCharacter(node)) {
        // For replaced nodes without children, the node itself is included in the range.
        if (range->selectNode(*node).hasException())
            return nullptr;
    } else {
        if (range->selectNodeContents(*node).hasException())
            return nullptr;
    }
    return WTFMove(range);
}
    
RefPtr<Range> AXObjectCache::rangeMatchesTextNearRange(RefPtr<Range> originalRange, const String& matchText)
{
    if (!originalRange)
        return nullptr;
    
    // Create a large enough range for searching the text within.
    unsigned textLength = matchText.length();
    auto startPosition = visiblePositionForPositionWithOffset(originalRange->startPosition(), -textLength);
    auto endPosition = visiblePositionForPositionWithOffset(originalRange->startPosition(), 2 * textLength);
    
    if (startPosition.isNull())
        startPosition = firstPositionInOrBeforeNode(&originalRange->startContainer());
    if (endPosition.isNull())
        endPosition = lastPositionInOrAfterNode(&originalRange->endContainer());
    
    auto searchRange = Range::create(m_document, startPosition, endPosition);
    if (searchRange->collapsed())
        return nullptr;
    
    auto range = Range::create(m_document, startPosition, originalRange->startPosition());
    unsigned targetOffset = TextIterator::rangeLength(range.ptr(), true);
    return findClosestPlainText(searchRange.get(), matchText, { }, targetOffset);
}

static bool isReplacedNodeOrBR(Node* node)
{
    return node && (AccessibilityObject::replacedNodeNeedsCharacter(node) || node->hasTagName(brTag));
}

static bool characterOffsetsInOrder(const CharacterOffset& characterOffset1, const CharacterOffset& characterOffset2)
{
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
    
    RefPtr<Range> range1 = AXObjectCache::rangeForNodeContents(node1);
    RefPtr<Range> range2 = AXObjectCache::rangeForNodeContents(node2);

    if (!range2)
        return true;
    if (!range1)
        return false;
    auto result = range1->compareBoundaryPoints(Range::START_TO_START, *range2);
    if (result.hasException())
        return true;
    return result.releaseReturnValue() <= 0;
}

static Node* resetNodeAndOffsetForReplacedNode(Node* replacedNode, int& offset, int characterCount)
{
    // Use this function to include the replaced node itself in the range we are creating.
    if (!replacedNode)
        return nullptr;
    
    RefPtr<Range> nodeRange = AXObjectCache::rangeForNodeContents(replacedNode);
    int nodeLength = TextIterator::rangeLength(nodeRange.get());
    offset = characterCount <= nodeLength ? replacedNode->computeNodeIndex() : replacedNode->computeNodeIndex() + 1;
    return replacedNode->parentNode();
}

static bool setRangeStartOrEndWithCharacterOffset(Range& range, const CharacterOffset& characterOffset, bool isStart)
{
    if (characterOffset.isNull())
        return false;
    
    int offset = characterOffset.startIndex + characterOffset.offset;
    Node* node = characterOffset.node;
    ASSERT(node);
    
    bool replacedNodeOrBR = isReplacedNodeOrBR(node);
    // For the non text node that has no children, we should create the range with its parent, otherwise the range would be collapsed.
    // Example: <div contenteditable="true"></div>, we want the range to include the div element.
    bool noChildren = !replacedNodeOrBR && !node->isTextNode() && !node->hasChildNodes();
    int characterCount = noChildren ? (isStart ? 0 : 1) : characterOffset.offset;
    
    if (replacedNodeOrBR || noChildren)
        node = resetNodeAndOffsetForReplacedNode(node, offset, characterCount);
    
    if (!node)
        return false;

    if (isStart) {
        if (range.setStart(*node, offset).hasException())
            return false;
    } else {
        if (range.setEnd(*node, offset).hasException())
            return false;
    }

    return true;
}

RefPtr<Range> AXObjectCache::rangeForUnorderedCharacterOffsets(const CharacterOffset& characterOffset1, const CharacterOffset& characterOffset2)
{
    if (characterOffset1.isNull() || characterOffset2.isNull())
        return nullptr;
    
    bool alreadyInOrder = characterOffsetsInOrder(characterOffset1, characterOffset2);
    CharacterOffset startCharacterOffset = alreadyInOrder ? characterOffset1 : characterOffset2;
    CharacterOffset endCharacterOffset = alreadyInOrder ? characterOffset2 : characterOffset1;
    
    auto result = Range::create(m_document);
    if (!setRangeStartOrEndWithCharacterOffset(result, startCharacterOffset, true))
        return nullptr;
    if (!setRangeStartOrEndWithCharacterOffset(result, endCharacterOffset, false))
        return nullptr;
    return WTFMove(result);
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
    
    textMarkerData.axID = obj.get()->axObjectID();
    textMarkerData.node = domNode;
    textMarkerData.characterOffset = characterOffset.offset;
    textMarkerData.characterStartIndex = characterOffset.startIndex;
    textMarkerData.offset = vpOffset;
    textMarkerData.affinity = visiblePosition.affinity();
    
    this->setNodeInUse(domNode);
}

CharacterOffset AXObjectCache::startOrEndCharacterOffsetForRange(RefPtr<Range> range, bool isStart, bool enterTextControls)
{
    if (!range)
        return CharacterOffset();
    
    // When getting the end CharacterOffset at node boundary, we don't want to collapse to the previous node.
    if (!isStart && !range->endOffset())
        return characterOffsetForNodeAndOffset(range->endContainer(), 0, TraverseOptionIncludeStart);
    
    // If it's end text marker, we want to go to the end of the range, and stay within the range.
    bool stayWithinRange = !isStart;
    
    Node& endNode = range->endContainer();
    if (endNode.isCharacterDataNode() && !isStart)
        return traverseToOffsetInRange(rangeForNodeContents(&endNode), range->endOffset(), TraverseOptionValidateOffset);
    
    Ref<Range> copyRange = *range;
    // Change the start of the range, so the character offset starts from node beginning.
    int offset = 0;
    Node& node = copyRange->startContainer();
    if (node.isCharacterDataNode()) {
        CharacterOffset nodeStartOffset = traverseToOffsetInRange(rangeForNodeContents(&node), range->startOffset(), TraverseOptionValidateOffset);
        if (isStart)
            return nodeStartOffset;
        copyRange = Range::create(range->ownerDocument(), &range->startContainer(), 0, &range->endContainer(), range->endOffset());
        offset += nodeStartOffset.offset;
    }
    
    TraverseOption options = isStart ? TraverseOptionDefault : TraverseOptionToNodeEnd;
    if (!enterTextControls)
        options = static_cast<TraverseOption>(options | TraverseOptionDoNotEnterTextControls);
    return traverseToOffsetInRange(WTFMove(copyRange), offset, options, stayWithinRange);
}

void AXObjectCache::startOrEndTextMarkerDataForRange(TextMarkerData& textMarkerData, RefPtr<Range> range, bool isStart)
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
    
    RefPtr<Range> range = rangeForNodeContents(domNode);

    // Traverse the offset amount of characters forward and see if there's remaining offsets.
    // Keep traversing to the next node when there's remaining offsets.
    CharacterOffset characterOffset = traverseToOffsetInRange(range, offset, option);
    while (!characterOffset.isNull() && characterOffset.remaining() && !toNodeEnd) {
        domNode = nextNode(domNode);
        if (!domNode)
            return CharacterOffset();
        range = rangeForNodeContents(domNode);
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
        
        // We should skip next CharactetOffset if it's visually the same.
        if (!lengthForRange(rangeForUnorderedCharacterOffsets(previous, next).get()))
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
        
        // We should skip previous CharactetOffset if it's visually the same.
        if (!lengthForRange(rangeForUnorderedCharacterOffsets(previous, next).get()))
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
    return range ? VisiblePosition(range->startPosition()) : VisiblePosition();
}

CharacterOffset AXObjectCache::characterOffsetFromVisiblePosition(const VisiblePosition& visiblePos)
{
    if (visiblePos.isNull())
        return CharacterOffset();
    
    Position deepPos = visiblePos.deepEquivalent();
    Node* domNode = deepPos.deprecatedNode();
    ASSERT(domNode);
    
    if (domNode->isCharacterDataNode())
        return traverseToOffsetInRange(rangeForNodeContents(domNode), deepPos.deprecatedEditingOffset(), TraverseOptionValidateOffset);
    
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
        visiblePosition = obj->nextVisiblePosition(visiblePosition);
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
    CharacterOffset result = traverseToOffsetInRange(rangeForNodeContents(obj->node()), characterOffset);
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

Optional<TextMarkerData> AXObjectCache::textMarkerDataForVisiblePosition(const VisiblePosition& visiblePos)
{
    if (visiblePos.isNull())
        return WTF::nullopt;

    Position deepPos = visiblePos.deepEquivalent();
    Node* domNode = deepPos.deprecatedNode();
    ASSERT(domNode);
    if (!domNode)
        return WTF::nullopt;

    if (is<HTMLInputElement>(*domNode) && downcast<HTMLInputElement>(*domNode).isPasswordField())
        return WTF::nullopt;

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
        return WTF::nullopt;
    RefPtr<AccessibilityObject> obj = cache->getOrCreate(domNode);

    // This memory must be zero'd so instances of TextMarkerData can be tested for byte-equivalence.
    // Warning: This is risky and bad because TextMarkerData is a nontrivial type.
    TextMarkerData textMarkerData;
    memset(static_cast<void*>(&textMarkerData), 0, sizeof(TextMarkerData));
    
    textMarkerData.axID = obj.get()->axObjectID();
    textMarkerData.node = domNode;
    textMarkerData.offset = deepPos.deprecatedEditingOffset();
    textMarkerData.affinity = visiblePos.affinity();

    textMarkerData.characterOffset = characterOffset.offset;
    textMarkerData.characterStartIndex = characterOffset.startIndex;

    cache->setNodeInUse(domNode);

    return textMarkerData;
}

// This function exits as a performance optimization to avoid a synchronous layout.
Optional<TextMarkerData> AXObjectCache::textMarkerDataForFirstPositionInTextControl(HTMLTextFormControlElement& textControl)
{
    if (is<HTMLInputElement>(textControl) && downcast<HTMLInputElement>(textControl).isPasswordField())
        return WTF::nullopt;

    AXObjectCache* cache = textControl.document().axObjectCache();
    if (!cache)
        return WTF::nullopt;

    RefPtr<AccessibilityObject> obj = cache->getOrCreate(&textControl);
    if (!obj)
        return WTF::nullopt;

    // This memory must be zero'd so instances of TextMarkerData can be tested for byte-equivalence.
    // Warning: This is risky and bad because TextMarkerData is a nontrivial type.
    TextMarkerData textMarkerData;
    memset(static_cast<void*>(&textMarkerData), 0, sizeof(TextMarkerData));
    
    textMarkerData.axID = obj.get()->axObjectID();
    textMarkerData.node = &textControl;

    cache->setNodeInUse(&textControl);

    return textMarkerData;
}

CharacterOffset AXObjectCache::nextCharacterOffset(const CharacterOffset& characterOffset, bool ignoreNextNodeStart)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    // We don't always move one 'character' at a time since there might be composed characters.
    int nextOffset = Position::uncheckedNextOffset(characterOffset.node, characterOffset.offset);
    CharacterOffset next = characterOffsetForNodeAndOffset(*characterOffset.node, nextOffset);
    
    // To be consistent with VisiblePosition, we should consider the case that current node end to next node start counts 1 offset.
    if (!ignoreNextNodeStart && !next.isNull() && !isReplacedNodeOrBR(next.node) && next.node != characterOffset.node) {
        int length = TextIterator::rangeLength(rangeForUnorderedCharacterOffsets(characterOffset, next).get());
        if (length > nextOffset - characterOffset.offset)
            next = characterOffsetForNodeAndOffset(*next.node, 0, TraverseOptionIncludeStart);
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

RefPtr<Range> AXObjectCache::leftWordRange(const CharacterOffset& characterOffset)
{
    CharacterOffset start = startCharacterOffsetOfWord(characterOffset, LeftWordIfOnBoundary);
    CharacterOffset end = endCharacterOffsetOfWord(start);
    return rangeForUnorderedCharacterOffsets(start, end);
}

RefPtr<Range> AXObjectCache::rightWordRange(const CharacterOffset& characterOffset)
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
    if (offset < characterOffset.node->textContent().length())
        U16_NEXT(characterOffset.node->textContent(), offset, characterOffset.node->textContent().length(), ch);
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

    RefPtr<Range> searchRange = rangeForNodeContents(boundary);
    if (!searchRange)
        return { };

    Vector<UChar, 1024> string;
    unsigned prefixLength = 0;
    
    if (requiresContextForWordBoundary(characterAfter(characterOffset))) {
        auto backwardsScanRange = boundary->document().createRange();
        if (!setRangeStartOrEndWithCharacterOffset(backwardsScanRange, characterOffset, false))
            return { };
        prefixLength = prefixLengthForRange(backwardsScanRange, string);
    }
    
    if (!setRangeStartOrEndWithCharacterOffset(*searchRange, characterOffset, true))
        return { };
    CharacterOffset end = startOrEndCharacterOffsetForRange(searchRange, false);
    
    TextIterator it(searchRange.get(), TextIteratorEmitsObjectReplacementCharacters);
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

CharacterOffset AXObjectCache::previousBoundary(const CharacterOffset& characterOffset, BoundarySearchFunction searchFunction)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    Node* boundary = parentEditingBoundary(characterOffset.node);
    if (!boundary)
        return CharacterOffset();
    
    RefPtr<Range> searchRange = rangeForNodeContents(boundary);
    Vector<UChar, 1024> string;
    unsigned suffixLength = 0;
    
    if (requiresContextForWordBoundary(characterBefore(characterOffset))) {
        auto forwardsScanRange = boundary->document().createRange();
        if (forwardsScanRange->setEndAfter(*boundary).hasException())
            return { };
        if (!setRangeStartOrEndWithCharacterOffset(forwardsScanRange, characterOffset, true))
            return { };
        suffixLength = suffixLengthForRange(forwardsScanRange, string);
    }
    
    if (!setRangeStartOrEndWithCharacterOffset(*searchRange, characterOffset, false))
        return { };
    CharacterOffset start = startOrEndCharacterOffsetForRange(searchRange, true);
    
    SimplifiedBackwardsTextIterator it(*searchRange);
    unsigned next = backwardSearchForBoundaryWithTextIterator(it, string, suffixLength, searchFunction);
    
    if (!next)
        return it.atEnd() ? start : characterOffset;
    
    Node& node = it.atEnd() ? searchRange->startContainer() : it.range()->startContainer();
    
    // SimplifiedBackwardsTextIterator ignores replaced elements.
    if (AccessibilityObject::replacedNodeNeedsCharacter(characterOffset.node))
        return characterOffsetForNodeAndOffset(*characterOffset.node, 0);
    Node* nextSibling = node.nextSibling();
    if (&node != characterOffset.node && AccessibilityObject::replacedNodeNeedsCharacter(nextSibling))
        return startOrEndCharacterOffsetForRange(rangeForNodeContents(nextSibling), false);
    
    if ((node.isTextNode() && static_cast<int>(next) <= node.maxCharacterOffset()) || (node.renderer() && node.renderer()->isBR() && !next)) {
        // The next variable contains a usable index into a text node
        if (node.isTextNode())
            return traverseToOffsetInRange(rangeForNodeContents(&node), next, TraverseOptionValidateOffset);
        return characterOffsetForNodeAndOffset(node, next, TraverseOptionIncludeStart);
    }
    
    int characterCount = characterOffset.offset - (string.size() - suffixLength - next);
    // We don't want to go to the previous node if the node is at the start of a new line.
    if (characterCount < 0 && (characterOffsetNodeIsBR(characterOffset) || string[string.size() - suffixLength - 1] == '\n'))
        characterCount = 0;
    return characterOffsetForNodeAndOffset(*characterOffset.node, characterCount, TraverseOptionIncludeStart);
}

CharacterOffset AXObjectCache::startCharacterOffsetOfParagraph(const CharacterOffset& characterOffset, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    auto* startNode = characterOffset.node;
    
    if (isRenderedAsNonInlineTableImageOrHR(startNode))
        return startOrEndCharacterOffsetForRange(rangeForNodeContents(startNode), true);
    
    auto* startBlock = enclosingBlock(startNode);
    int offset = characterOffset.startIndex + characterOffset.offset;
    auto* highestRoot = highestEditableRoot(firstPositionInOrBeforeNode(startNode));
    Position::AnchorType type = Position::PositionIsOffsetInAnchor;
    
    auto* node = findStartOfParagraph(startNode, highestRoot, startBlock, offset, type, boundaryCrossingRule);
    
    if (type == Position::PositionIsOffsetInAnchor)
        return characterOffsetForNodeAndOffset(*node, offset, TraverseOptionIncludeStart);
    
    return startOrEndCharacterOffsetForRange(rangeForNodeContents(node), true);
}

CharacterOffset AXObjectCache::endCharacterOffsetOfParagraph(const CharacterOffset& characterOffset, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    Node* startNode = characterOffset.node;
    if (isRenderedAsNonInlineTableImageOrHR(startNode))
        return startOrEndCharacterOffsetForRange(rangeForNodeContents(startNode), false);
    
    Node* stayInsideBlock = enclosingBlock(startNode);
    int offset = characterOffset.startIndex + characterOffset.offset;
    Node* highestRoot = highestEditableRoot(firstPositionInOrBeforeNode(startNode));
    Position::AnchorType type = Position::PositionIsOffsetInAnchor;
    
    Node* node = findEndOfParagraph(startNode, highestRoot, stayInsideBlock, offset, type, boundaryCrossingRule);
    if (type == Position::PositionIsOffsetInAnchor) {
        if (node->isTextNode()) {
            CharacterOffset startOffset = startOrEndCharacterOffsetForRange(rangeForNodeContents(node), true);
            offset -= startOffset.startIndex;
        }
        return characterOffsetForNodeAndOffset(*node, offset, TraverseOptionIncludeStart);
    }
    
    return startOrEndCharacterOffsetForRange(rangeForNodeContents(node), false);
}

RefPtr<Range> AXObjectCache::paragraphForCharacterOffset(const CharacterOffset& characterOffset)
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
    return previousBoundary(characterOffset, startSentenceBoundary);
}

CharacterOffset AXObjectCache::endCharacterOffsetOfSentence(const CharacterOffset& characterOffset)
{
    return nextBoundary(characterOffset, endSentenceBoundary);
}

RefPtr<Range> AXObjectCache::sentenceForCharacterOffset(const CharacterOffset& characterOffset)
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
    
    Node* node = characterOffset.node;
    
    renderer = node->renderer();
    if (!renderer)
        return LayoutRect();
    
    InlineBox* inlineBox = nullptr;
    int caretOffset;
    // Use a collapsed range to get the position.
    RefPtr<Range> range = rangeForUnorderedCharacterOffsets(characterOffset, characterOffset);
    if (!range)
        return IntRect();
    
    Position startPosition = range->startPosition();
    startPosition.getInlineBoxAndOffset(DOWNSTREAM, inlineBox, caretOffset);
    
    if (inlineBox)
        renderer = &inlineBox->renderer();
    
    if (is<RenderLineBreak>(renderer) && downcast<RenderLineBreak>(renderer)->inlineBoxWrapper() != inlineBox)
        return IntRect();
    
    return renderer->localCaretRect(inlineBox, caretOffset);
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

CharacterOffset AXObjectCache::characterOffsetForPoint(const IntPoint &point, AccessibilityObject* obj)
{
    if (!obj)
        return CharacterOffset();
    
    VisiblePosition vp = obj->visiblePositionForPoint(point);
    RefPtr<Range> range = makeRange(vp, vp);
    return startOrEndCharacterOffsetForRange(range, true);
}

CharacterOffset AXObjectCache::characterOffsetForPoint(const IntPoint &point)
{
    RefPtr<Range> caretRange = m_document.caretRangeFromPoint(LayoutPoint(point));
    return startOrEndCharacterOffsetForRange(caretRange, true);
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

CharacterOffset AXObjectCache::characterOffsetForIndex(int index, const AccessibilityObject* obj)
{
    if (!obj)
        return CharacterOffset();
    
    VisiblePosition vp = obj->visiblePositionForIndex(index);
    CharacterOffset validate = characterOffsetFromVisiblePosition(vp);
    // In text control, VisiblePosition always gives the before position of a
    // BR node, while CharacterOffset will do the opposite.
    if (obj->isTextControl() && characterOffsetNodeIsBR(validate))
        validate.offset = 1;
    
    RefPtr<Range> range = obj->elementRange();
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

int AXObjectCache::indexForCharacterOffset(const CharacterOffset& characterOffset, AccessibilityObject* obj)
{
    // Create a collapsed range so that we can get the VisiblePosition from it.
    RefPtr<Range> range = rangeForUnorderedCharacterOffsets(characterOffset, characterOffset);
    if (!range)
        return 0;
    VisiblePosition vp = range->startPosition();
    return obj->indexForVisiblePosition(vp);
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

static void conditionallyAddNodeToFilterList(Node* node, const Document& document, HashSet<Node*>& nodesToRemove)
{
    if (node && (!node->isConnected() || &node->document() == &document))
        nodesToRemove.add(node);
}
    
template<typename T>
static void filterVectorPairForRemoval(const Vector<std::pair<T, T>>& list, const Document& document, HashSet<Node*>& nodesToRemove)
{
    for (auto& entry : list) {
        conditionallyAddNodeToFilterList(entry.first, document, nodesToRemove);
        conditionallyAddNodeToFilterList(entry.second, document, nodesToRemove);
    }
}
    
template<typename T, typename U>
static void filterMapForRemoval(const HashMap<T, U>& list, const Document& document, HashSet<Node*>& nodesToRemove)
{
    for (auto& entry : list)
        conditionallyAddNodeToFilterList(entry.key, document, nodesToRemove);
}

template<typename T>
static void filterListForRemoval(const ListHashSet<T>& list, const Document& document, HashSet<Node*>& nodesToRemove)
{
    for (auto* node : list)
        conditionallyAddNodeToFilterList(node, document, nodesToRemove);
}

void AXObjectCache::prepareForDocumentDestruction(const Document& document)
{
    HashSet<Node*> nodesToRemove;
    filterListForRemoval(m_textMarkerNodes, document, nodesToRemove);
    filterListForRemoval(m_modalNodesSet, document, nodesToRemove);
    filterListForRemoval(m_deferredRecomputeIsIgnoredList, document, nodesToRemove);
    filterListForRemoval(m_deferredTextChangedList, document, nodesToRemove);
    filterListForRemoval(m_deferredSelectedChildredChangedList, document, nodesToRemove);
    filterListForRemoval(m_deferredChildrenChangedNodeList, document, nodesToRemove);
    filterMapForRemoval(m_deferredTextFormControlValue, document, nodesToRemove);
    filterMapForRemoval(m_deferredAttributeChange, document, nodesToRemove);
    filterVectorPairForRemoval(m_deferredFocusedNodeChange, document, nodesToRemove);

    for (auto* node : nodesToRemove)
        remove(*node);
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
    
void AXObjectCache::performDeferredCacheUpdate()
{
    if (m_performingDeferredCacheUpdate)
        return;

    SetForScope<bool> performingDeferredCacheUpdate(m_performingDeferredCacheUpdate, true);

    for (auto* nodeChild : m_deferredChildrenChangedNodeList) {
        handleMenuOpened(nodeChild);
        handleLiveRegionCreated(nodeChild);
    }
    m_deferredChildrenChangedNodeList.clear();

    for (auto& child : m_deferredChildredChangedList)
        child->childrenChanged();
    m_deferredChildredChangedList.clear();

    for (auto* node : m_deferredTextChangedList)
        textChanged(node);
    m_deferredTextChangedList.clear();

    for (auto* element : m_deferredRecomputeIsIgnoredList) {
        if (auto* renderer = element->renderer())
            recomputeIsIgnored(renderer);
    }
    m_deferredRecomputeIsIgnoredList.clear();
    
    for (auto* selectElement : m_deferredSelectedChildredChangedList)
        selectedChildrenChanged(selectElement);
    m_deferredSelectedChildredChangedList.clear();

    for (auto& deferredFormControlContext : m_deferredTextFormControlValue) {
        auto& textFormControlElement = downcast<HTMLTextFormControlElement>(*deferredFormControlContext.key);
        postTextReplacementNotificationForTextControl(textFormControlElement, deferredFormControlContext.value, textFormControlElement.innerTextValue());
    }
    m_deferredTextFormControlValue.clear();

    for (auto& deferredAttributeChangeContext : m_deferredAttributeChange)
        handleAttributeChange(deferredAttributeChangeContext.value, deferredAttributeChangeContext.key);
    m_deferredAttributeChange.clear();
    
    for (auto& deferredFocusedChangeContext : m_deferredFocusedNodeChange)
        handleFocusedUIElementChanged(deferredFocusedChangeContext.first, deferredFocusedChangeContext.second);
    m_deferredFocusedNodeChange.clear();
}
    
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
Ref<AXIsolatedTreeNode> AXObjectCache::createIsolatedAccessibilityTreeHierarchy(AccessibilityObject& object, AXID parentID, AXIsolatedTree& tree, Vector<Ref<AXIsolatedTreeNode>>& nodeChanges)
{
    auto isolatedTreeNode = AXIsolatedTreeNode::create(object);
    nodeChanges.append(isolatedTreeNode.copyRef());

    isolatedTreeNode->setParent(parentID);
    associateIsolatedTreeNode(object, isolatedTreeNode, tree.treeIdentifier());

    for (auto child : object.children()) {
        auto staticChild = createIsolatedAccessibilityTreeHierarchy(*child, isolatedTreeNode->identifier(), tree, nodeChanges);
        isolatedTreeNode->appendChild(staticChild->identifier());
    }

    return isolatedTreeNode;
}
    
Ref<AXIsolatedTree> AXObjectCache::generateIsolatedAccessibilityTree()
{
    RELEASE_ASSERT(isMainThread());

    auto tree = AXIsolatedTree::treeForPageID(*m_document.pageID());
    if (!tree)
        tree = AXIsolatedTree::createTreeForPageID(*m_document.pageID());
    
    Vector<Ref<AXIsolatedTreeNode>> nodeChanges;
    auto root = createIsolatedAccessibilityTreeHierarchy(*rootObject(), InvalidAXID, *tree, nodeChanges);
    root->setIsRootNode(true);
    tree->appendNodeChanges(nodeChanges);

    return makeRef(*tree);
}
#endif
    
void AXObjectCache::deferRecomputeIsIgnoredIfNeeded(Element* element)
{
    if (!nodeAndRendererAreValid(element))
        return;
    
    if (rendererNeedsDeferredUpdate(*element->renderer())) {
        m_deferredRecomputeIsIgnoredList.add(element);
        return;
    }
    recomputeIsIgnored(element->renderer());
}

void AXObjectCache::deferRecomputeIsIgnored(Element* element)
{
    if (!nodeAndRendererAreValid(element))
        return;

    m_deferredRecomputeIsIgnoredList.add(element);
}

void AXObjectCache::deferTextChangedIfNeeded(Node* node)
{
    if (!nodeAndRendererAreValid(node))
        return;

    if (rendererNeedsDeferredUpdate(*node->renderer())) {
        m_deferredTextChangedList.add(node);
        return;
    }
    textChanged(node);
}

void AXObjectCache::deferSelectedChildrenChangedIfNeeded(Element& selectElement)
{
    if (!nodeAndRendererAreValid(&selectElement))
        return;

    if (rendererNeedsDeferredUpdate(*selectElement.renderer())) {
        m_deferredSelectedChildredChangedList.add(&selectElement);
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
            const AtomicString& ariaHiddenValue = downcast<Element>(*testNode).attributeWithoutSynchronization(aria_hiddenAttr);
            if (equalLettersIgnoringASCIICase(ariaHiddenValue, "true"))
                return false;
            
            bool ariaHiddenFalse = equalLettersIgnoringASCIICase(ariaHiddenValue, "false");
            if (!testNode->renderer() && !ariaHiddenFalse)
                return false;
            if (!ariaHiddenFalsePresent && ariaHiddenFalse)
                ariaHiddenFalsePresent = true;
            // We should break early when it gets to a rendered object.
            if (testNode->renderer())
                break;
        }
    }
    
    return !requiresAriaHiddenFalse || ariaHiddenFalsePresent;
}

AccessibilityObject* AXObjectCache::rootWebArea()
{
    AccessibilityObject* rootObject = this->rootObject();
    if (!rootObject || !rootObject->isAccessibilityScrollView())
        return nullptr;
    return downcast<AccessibilityScrollView>(*rootObject).webAreaObject();
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

#if !PLATFORM(COCOA)
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

#endif // HAVE(ACCESSIBILITY)
