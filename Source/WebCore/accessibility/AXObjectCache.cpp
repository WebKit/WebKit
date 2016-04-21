/*
 * Copyright (C) 2008, 2009, 2010, 2015 Apple Inc. All rights reserved.
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

#include "AccessibilityARIAGrid.h"
#include "AccessibilityARIAGridCell.h"
#include "AccessibilityARIAGridRow.h"
#include "AccessibilityAttachment.h"
#include "AccessibilityImageMapLink.h"
#include "AccessibilityList.h"
#include "AccessibilityListBox.h"
#include "AccessibilityListBoxOption.h"
#include "AccessibilityMediaControls.h"
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
#include "Page.h"
#include "RenderAttachment.h"
#include "RenderListBox.h"
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
#include "ScrollView.h"
#include "TextBoundaries.h"
#include "TextIterator.h"
#include "htmlediting.h"
#include <wtf/DataLog.h>

#if ENABLE(VIDEO)
#include "MediaControlElements.h"
#endif

namespace WebCore {

using namespace HTMLNames;

// Post value change notifications for password fields or elements contained in password fields at a 40hz interval to thwart analysis of typing cadence
static double AccessibilityPasswordValueChangeNotificationInterval = 0.025;
static double AccessibilityLiveRegionChangedNotificationInterval = 0.020;

AccessibilityObjectInclusion AXComputedObjectAttributeCache::getIgnored(AXID id) const
{
    HashMap<AXID, CachedAXObjectAttributes>::const_iterator it = m_idMapping.find(id);
    return it != m_idMapping.end() ? it->value.ignored : DefaultBehavior;
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
    if (AXObjectCache::accessibilityEnabled() && selection.isRange()) {
        m_replacedText = AccessibilityObject::stringForVisiblePositionRange(selection);
        m_replacedRange.startIndex.value = indexForVisiblePosition(selection.start(), m_replacedRange.startIndex.scope);
        m_replacedRange.endIndex.value = indexForVisiblePosition(selection.end(), m_replacedRange.endIndex.scope);
    }
}

void AccessibilityReplacedText::postTextStateChangeNotification(AXObjectCache* cache, AXTextEditType type, const String& text, const VisibleSelection& selection)
{
    if (!cache)
        return;
    if (!AXObjectCache::accessibilityEnabled())
        return;

    VisiblePosition position = selection.start();
    Node* node = highestEditableRoot(position.deepEquivalent(), HasEditableAXRole);
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
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    if (flag)
        enableAccessibility();
#endif
#endif
}

AXObjectCache::AXObjectCache(Document& document)
    : m_document(document)
    , m_notificationPostTimer(*this, &AXObjectCache::notificationPostTimerFired)
    , m_passwordNotificationPostTimer(*this, &AXObjectCache::passwordNotificationPostTimerFired)
    , m_liveRegionChangedPostTimer(*this, &AXObjectCache::liveRegionChangedNotificationPostTimerFired)
    , m_currentAriaModalNode(nullptr)
{
    findAriaModalNodes();
}

AXObjectCache::~AXObjectCache()
{
    m_notificationPostTimer.stop();
    m_liveRegionChangedPostTimer.stop();

    for (const auto& object : m_objects.values()) {
        detachWrapper(object.get(), CacheDestroyed);
        object->detach(CacheDestroyed);
        removeAXID(object.get());
    }
}

void AXObjectCache::findAriaModalNodes()
{
    // Traverse the DOM tree to look for the aria-modal=true nodes.
    for (Element* element = ElementTraversal::firstWithin(document().rootNode()); element; element = ElementTraversal::nextIncludingPseudo(*element)) {
        
        // Must have dialog or alertdialog role
        if (!nodeHasRole(element, "dialog") && !nodeHasRole(element, "alertdialog"))
            continue;
        if (!equalLettersIgnoringASCIICase(element->fastGetAttribute(aria_modalAttr), "true"))
            continue;
        
        m_ariaModalNodesSet.add(element);
    }
    
    // Set the current valid aria-modal node if possible.
    updateCurrentAriaModalNode();
}

void AXObjectCache::updateCurrentAriaModalNode()
{
    // There might be multiple nodes with aria-modal=true set.
    // We use this function to pick the one we want.
    m_currentAriaModalNode = nullptr;
    if (m_ariaModalNodesSet.isEmpty())
        return;
    
    // We only care about the nodes which are visible.
    ListHashSet<RefPtr<Node>> visibleNodes;
    for (auto& object : m_ariaModalNodesSet) {
        if (isNodeVisible(object))
            visibleNodes.add(object);
    }
    
    if (visibleNodes.isEmpty())
        return;
    
    // If any of the node are keyboard focused, we want to pick that.
    Node* focusedNode = document().focusedElement();
    for (auto& object : visibleNodes) {
        if (focusedNode != nullptr && focusedNode->isDescendantOf(object.get())) {
            m_currentAriaModalNode = object.get();
            break;
        }
    }
    
    // If none of the nodes are focused, we want to pick the last dialog in the DOM.
    if (!m_currentAriaModalNode)
        m_currentAriaModalNode = visibleNodes.last().get();
}

bool AXObjectCache::isNodeVisible(Node* node) const
{
    if (!is<Element>(node))
        return false;
    
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return false;
    const RenderStyle& style = renderer->style();
    if (style.display() == NONE || style.visibility() != VISIBLE)
        return false;
    
    // We also need to consider aria hidden status.
    if (!isNodeAriaVisible(node))
        return false;
    
    return true;
}

Node* AXObjectCache::ariaModalNode()
{
    // This function returns the valid aria modal node.
    if (m_ariaModalNodesSet.isEmpty())
        return nullptr;
    
    // Check the current valid aria modal node first.
    // Usually when one dialog sets aria-modal=true, that dialog is the one we want.
    if (isNodeVisible(m_currentAriaModalNode))
        return m_currentAriaModalNode;
    
    // Recompute the valid aria modal node when m_currentAriaModalNode is null or hidden.
    updateCurrentAriaModalNode();
    return isNodeVisible(m_currentAriaModalNode) ? m_currentAriaModalNode : nullptr;
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
// FIXME: This should take a const char*, but one caller passes nullAtom.
bool nodeHasRole(Node* node, const String& role)
{
    if (!node || !is<Element>(node))
        return false;

    auto& roleValue = downcast<Element>(*node).fastGetAttribute(roleAttr);
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
                      || (nodeHasRole(node, nullAtom) && (node->hasTagName(ulTag) || node->hasTagName(olTag) || node->hasTagName(dlTag)))))
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

#if ENABLE(VIDEO)
    // media controls
    if (node && node->isMediaControlElement())
        return AccessibilityMediaControl::create(renderer);
#endif

    if (is<RenderSVGRoot>(*renderer))
        return AccessibilitySVGRoot::create(renderer);
    
    if (is<SVGElement>(node))
        return AccessibilitySVGElement::create(renderer);

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
    case ListBoxOptionRole:
        obj = AccessibilityListBoxOption::create();
        break;
    case ImageMapLinkRole:
        obj = AccessibilityImageMapLink::create();
        break;
    case ColumnRole:
        obj = AccessibilityTableColumn::create();
        break;            
    case TableHeaderContainerRole:
        obj = AccessibilityTableHeaderContainer::create();
        break;   
    case SliderThumbRole:
        obj = AccessibilitySliderThumb::create();
        break;
    case MenuListPopupRole:
        obj = AccessibilityMenuListPopup::create();
        break;
    case MenuListOptionRole:
        obj = AccessibilityMenuListOption::create();
        break;
    case SpinButtonRole:
        obj = AccessibilitySpinButton::create();
        break;
    case SpinButtonPartRole:
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
    
    // first fetch object to operate some cleanup functions on it 
    AccessibilityObject* obj = m_objects.get(axID);
    if (!obj)
        return;
    
    detachWrapper(obj, ElementDestroyed);
    obj->detach(ElementDestroyed, this);
    removeAXID(obj);
    
    // finally remove the object
    if (!m_objects.take(axID))
        return;
    
    ASSERT(m_objects.size() >= m_idsInUse.size());    
}
    
void AXObjectCache::remove(RenderObject* renderer)
{
    if (!renderer)
        return;
    
    AXID axID = m_renderObjectMapping.get(renderer);
    remove(axID);
    m_renderObjectMapping.remove(renderer);
}

void AXObjectCache::remove(Node* node)
{
    if (!node)
        return;

    removeNodeForUse(node);

    // This is all safe even if we didn't have a mapping.
    AXID axID = m_nodeObjectMapping.get(node);
    remove(axID);
    m_nodeObjectMapping.remove(node);

    // Cleanup for aria modal nodes.
    if (m_currentAriaModalNode == node)
        m_currentAriaModalNode = nullptr;
    if (m_ariaModalNodesSet.contains(node))
        m_ariaModalNodesSet.remove(node);
    
    if (node->renderer()) {
        remove(node->renderer());
        return;
    }
}

void AXObjectCache::remove(Widget* view)
{
    if (!view)
        return;
        
    AXID axID = m_widgetObjectMapping.get(view);
    remove(axID);
    m_widgetObjectMapping.remove(view);
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

void AXObjectCache::removeAXID(AccessibilityObject* object)
{
    if (!object)
        return;
    
    AXID objID = object->axObjectID();
    if (!objID)
        return;
    ASSERT(!HashTraits<AXID>::isDeletedValue(objID));
    ASSERT(m_idsInUse.contains(objID));
    object->setAXObjectID(0);
    m_idsInUse.remove(objID);
}

void AXObjectCache::textChanged(Node* node)
{
    textChanged(getOrCreate(node));
}

void AXObjectCache::textChanged(RenderObject* renderer)
{
    textChanged(getOrCreate(renderer));
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
    String liveRegionStatus = element->fastGetAttribute(aria_liveAttr);
    if (liveRegionStatus.isEmpty()) {
        const AtomicString& ariaRole = element->fastGetAttribute(roleAttr);
        if (!ariaRole.isEmpty())
            liveRegionStatus = AccessibilityObject::defaultLiveRegionStatusForRole(AccessibilityObject::ariaRoleToWebCoreRole(ariaRole));
    }
    
    if (AccessibilityObject::liveRegionStatusIsEnabled(liveRegionStatus))
        postNotification(getOrCreate(node), &document(), AXLiveRegionCreated);
}
    
void AXObjectCache::childrenChanged(Node* node, Node* newChild)
{
    if (newChild) {
        handleMenuOpened(newChild);
        handleLiveRegionCreated(newChild);
    }
    
    childrenChanged(get(node));
}

void AXObjectCache::childrenChanged(RenderObject* renderer, RenderObject* newChild)
{
    if (!renderer)
        return;
    
    if (newChild) {
        handleMenuOpened(newChild->node());
        handleLiveRegionCreated(newChild->node());
    }
    
    childrenChanged(get(renderer));
}

void AXObjectCache::childrenChanged(AccessibilityObject* obj)
{
    if (!obj)
        return;

    obj->childrenChanged();
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
                ASSERT(!renderer->view().layoutState());
        }
#endif

        AXNotification notification = note.second;
        
        // Ensure that this menu really is a menu. We do this check here so that we don't have to create
        // the axChildren when the menu is marked as opening.
        if (notification == AXMenuOpened) {
            obj->updateChildrenIfNecessary();
            if (obj->roleValue() != MenuRole)
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
            m_notificationPostTimer.startOneShot(0);
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
    
    if (!downcast<Element>(*node).focused() && !equalLettersIgnoringASCIICase(downcast<Element>(*node).fastGetAttribute(aria_selectedAttr), "true"))
        return;
    
    postNotification(getOrCreate(node), &document(), AXMenuListItemSelected);
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
        m_passwordNotificationPostTimer.startOneShot(AccessibilityPasswordValueChangeNotificationInterval);

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

    m_liveRegionChangedPostTimer.startOneShot(AccessibilityLiveRegionChangedNotificationInterval);
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
    if (AccessibilityObject* obj = getOrCreate(node))
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

    if (AccessibilityObject* obj = getOrCreate(node)) {
        obj->updateAccessibilityRole();
        obj->notifyIfIgnoredValueChanged();
    }
}

void AXObjectCache::handleAttributeChanged(const QualifiedName& attrName, Element* element)
{
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
        handleAriaModalChange(element);
    else
        postNotification(element, AXObjectCache::AXAriaAttributeChanged);
}

void AXObjectCache::handleAriaModalChange(Node* node)
{
    if (!is<Element>(node))
        return;
    
    if (!nodeHasRole(node, "dialog") && !nodeHasRole(node, "alertdialog"))
        return;
    
    stopCachingComputedObjectAttributes();
    if (equalLettersIgnoringASCIICase(downcast<Element>(*node).fastGetAttribute(aria_modalAttr), "true")) {
        // Add the newly modified node to the modal nodes set, and set it to be the current valid aria modal node.
        // We will recompute the current valid aria modal node in ariaModalNode() when this node is not visible.
        m_ariaModalNodesSet.add(node);
        m_currentAriaModalNode = node;
    } else {
        // Remove the node from the modal nodes set. There might be other visible modal nodes, so we recompute here.
        m_ariaModalNodesSet.remove(node);
        updateCurrentAriaModalNode();
    }
    startCachingComputedObjectAttributesUntilTreeMutates();
}

void AXObjectCache::labelChanged(Element* element)
{
    ASSERT(is<HTMLLabelElement>(*element));
    HTMLElement* correspondingControl = downcast<HTMLLabelElement>(*element).control();
    textChanged(correspondingControl);
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
    if (!cache->isIDinUse(textMarkerData.axID))
        return VisiblePosition();

    return visiblePos;
}

CharacterOffset AXObjectCache::characterOffsetForTextMarkerData(TextMarkerData& textMarkerData)
{
    if (!isNodeInUse(textMarkerData.node))
        return CharacterOffset();
    
    if (textMarkerData.ignored)
        return CharacterOffset();
    
    return CharacterOffset(textMarkerData.node, textMarkerData.characterStartIndex, textMarkerData.characterOffset);
}

CharacterOffset AXObjectCache::traverseToOffsetInRange(RefPtr<Range>range, int offset, TraverseOption option, bool stayWithinRange)
{
    if (!range)
        return CharacterOffset();
    
    bool toNodeEnd = option & TraverseOptionToNodeEnd;
    
    int offsetInCharacter = 0;
    int offsetSoFar = 0;
    int remaining = 0;
    int lastLength = 0;
    Node* currentNode = nullptr;
    bool finished = false;
    int lastStartOffset = 0;
    
    TextIterator iterator(range.get());
    
    // When the range has zero length, there might be replaced node or brTag that we need to increment the characterOffset.
    if (iterator.atEnd()) {
        currentNode = &range->startContainer();
        lastStartOffset = range->startOffset();
        if (offset > 0 || toNodeEnd) {
            if (AccessibilityObject::replacedNodeNeedsCharacter(currentNode) || (currentNode->renderer() && currentNode->renderer()->isBR()))
                offsetSoFar++;
            lastLength = offsetSoFar;
            
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
                offsetSoFar++;
                currentLength++;
                currentNode = childNode;
                hasReplacedNodeOrBR = true;
            } else
                continue;
        } else {
            // Ignore space, new line, tag node.
            if (currentLength == 1) {
                if (isSpaceOrNewline(iterator.text()[0])) {
                    // If the node has BR tag, we want to set the currentNode to it.
                    int subOffset = iterator.range()->startOffset();
                    Node* childNode = node.traverseToChildAt(subOffset);
                    if (childNode && childNode->renderer() && childNode->renderer()->isBR()) {
                        currentNode = childNode;
                        hasReplacedNodeOrBR = true;
                    } else if (currentNode != previousNode) {
                        // We should set the start offset and length for the current node in case this is the last iteration.
                        lastStartOffset = 1;
                        lastLength = 0;
                        continue;
                    }
                }
            }
            offsetSoFar += currentLength;
        }

        if (currentNode == previousNode)
            lastLength += currentLength;
        else {
            lastLength = currentLength;
            lastStartOffset = hasReplacedNodeOrBR ? 0 : iterator.range()->startOffset();
        }
        
        // Break early if we have advanced enough characters.
        if (!toNodeEnd && offsetSoFar >= offset) {
            offsetInCharacter = offset - (offsetSoFar - lastLength);
            finished = true;
            break;
        }
        previousNode = currentNode;
    }
    
    if (!finished) {
        offsetInCharacter = lastLength;
        if (!toNodeEnd)
            remaining = offset - offsetSoFar;
    }
    
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
    RefPtr<Range> range = Range::create(*document);
    ExceptionCode ec = 0;
    range->selectNodeContents(node, ec);
    return ec ? nullptr : range;
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
    if (!node1->offsetInCharacters() && !isReplacedNodeOrBR(node1))
        node1 = node1->traverseToChildAt(characterOffset1.offset);
    if (!node2->offsetInCharacters() && !isReplacedNodeOrBR(node2))
        node2 = node2->traverseToChildAt(characterOffset2.offset);
    
    if (!node1 || !node2)
        return false;
    
    RefPtr<Range> range1 = AXObjectCache::rangeForNodeContents(node1);
    RefPtr<Range> range2 = AXObjectCache::rangeForNodeContents(node2);
    return range1->compareBoundaryPoints(Range::START_TO_START, range2.get(), IGNORE_EXCEPTION) <= 0;
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

static void setRangeStartOrEndWithCharacterOffset(RefPtr<Range> range, const CharacterOffset& characterOffset, bool isStart, ExceptionCode& ec)
{
    if (!range) {
        ec = RangeError;
        return;
    }
    if (characterOffset.isNull()) {
        ec = TypeError;
        return;
    }
    
    int offset = characterOffset.startIndex + characterOffset.offset;
    Node* node = characterOffset.node;
    
    bool replacedNodeOrBR = isReplacedNodeOrBR(node);
    // For the non text node that has no children, we should create the range with its parent, otherwise the range would be collapsed.
    // Example: <div contenteditable="true"></div>, we want the range to include the div element.
    bool noChildren = !replacedNodeOrBR && !node->isTextNode() && !node->hasChildNodes();
    int characterCount = noChildren ? (isStart ? 0 : 1) : characterOffset.offset;
    
    if (replacedNodeOrBR || noChildren)
        node = resetNodeAndOffsetForReplacedNode(node, offset, characterCount);
    
    if (isStart)
        range->setStart(node, offset, ec);
    else
        range->setEnd(node, offset, ec);
}

RefPtr<Range> AXObjectCache::rangeForUnorderedCharacterOffsets(const CharacterOffset& characterOffset1, const CharacterOffset& characterOffset2)
{
    if (characterOffset1.isNull() || characterOffset2.isNull())
        return nullptr;
    
    bool alreadyInOrder = characterOffsetsInOrder(characterOffset1, characterOffset2);
    CharacterOffset startCharacterOffset = alreadyInOrder ? characterOffset1 : characterOffset2;
    CharacterOffset endCharacterOffset = alreadyInOrder ? characterOffset2 : characterOffset1;
    
    RefPtr<Range> result = Range::create(m_document);
    ExceptionCode ec = 0;
    setRangeStartOrEndWithCharacterOffset(result, startCharacterOffset, true, ec);
    setRangeStartOrEndWithCharacterOffset(result, endCharacterOffset, false, ec);
    if (ec)
        return nullptr;
    
    return result;
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

CharacterOffset AXObjectCache::startOrEndCharacterOffsetForRange(RefPtr<Range> range, bool isStart)
{
    if (!range)
        return CharacterOffset();
    
    // When getting the end CharacterOffset at node boundary, we don't want to collapse to the previous node.
    if (!isStart && !range->endOffset())
        return characterOffsetForNodeAndOffset(range->endContainer(), 0, TraverseOptionIncludeStart);
    
    // If it's end text marker, we want to go to the end of the range, and stay within the range.
    bool stayWithinRange = !isStart;
    
    RefPtr<Range> copyRange = range;
    // Change the start of the range, so the character offset starts from node beginning.
    int offset = 0;
    Node* node = &copyRange->startContainer();
    if (node->offsetInCharacters()) {
        copyRange = Range::create(range->ownerDocument(), &range->startContainer(), range->startOffset(), &range->endContainer(), range->endOffset());
        CharacterOffset nodeStartOffset = traverseToOffsetInRange(rangeForNodeContents(node), 0);
        offset = std::max(copyRange->startOffset() - nodeStartOffset.startIndex, 0);
        copyRange->setStart(node, nodeStartOffset.startIndex);
    }
    
    return traverseToOffsetInRange(copyRange, offset, isStart ? TraverseOptionDefault : TraverseOptionToNodeEnd, stayWithinRange);
}

void AXObjectCache::startOrEndTextMarkerDataForRange(TextMarkerData& textMarkerData, RefPtr<Range> range, bool isStart)
{
    memset(&textMarkerData, 0, sizeof(TextMarkerData));
    
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
    memset(&textMarkerData, 0, sizeof(TextMarkerData));
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
    do {
        next = nextCharacterOffset(next, false);
        if (shouldSkipBoundary(previous, next))
            next = nextCharacterOffset(next, false);
        textMarkerDataForCharacterOffset(textMarkerData, next);
        previous = next;
    } while (textMarkerData.ignored);
}

void AXObjectCache::textMarkerDataForPreviousCharacterOffset(TextMarkerData& textMarkerData, const CharacterOffset& characterOffset)
{
    CharacterOffset previous = characterOffset;
    do {
        previous = previousCharacterOffset(previous, false);
        textMarkerDataForCharacterOffset(textMarkerData, previous);
    } while (textMarkerData.ignored);
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
    
    RefPtr<AccessibilityObject> obj = this->getOrCreate(characterOffset.node);
    if (!obj)
        return VisiblePosition();
    
    // nextVisiblePosition means advancing one character. Use this to calculate the character offset.
    VisiblePositionRange vpRange = obj->visiblePositionRange();
    VisiblePosition start = vpRange.start;
    
    // Sometimes vpRange.start will be the previous node's end position and VisiblePosition will count the leading line break as 1 offset.
    int characterCount = start.deepEquivalent().deprecatedNode() == characterOffset.node ? characterOffset.offset : characterOffset.offset + characterOffset.startIndex;
    VisiblePosition result = start;
    for (int i = 0; i < characterCount; i++)
        result = obj->nextVisiblePosition(result);
    
    return result;
}

CharacterOffset AXObjectCache::characterOffsetFromVisiblePosition(const VisiblePosition& visiblePos)
{
    if (visiblePos.isNull())
        return CharacterOffset();
    
    Position deepPos = visiblePos.deepEquivalent();
    Node* domNode = deepPos.deprecatedNode();
    ASSERT(domNode);
    
    RefPtr<AccessibilityObject> obj = this->getOrCreate(domNode);
    if (!obj)
        return CharacterOffset();
    
    // Use nextVisiblePosition to calculate how many characters we need to traverse to the current position.
    VisiblePositionRange vpRange = obj->visiblePositionRange();
    VisiblePosition vp = vpRange.start;
    int characterOffset = 0;
    Position vpDeepPos = vp.deepEquivalent();
    
    VisiblePosition previousVisiblePos;
    while (!vpDeepPos.isNull() && !deepPos.equals(vpDeepPos)) {
        previousVisiblePos = vp;
        vp = obj->nextVisiblePosition(vp);
        vpDeepPos = vp.deepEquivalent();
        // Sometimes nextVisiblePosition will give the same VisiblePostion,
        // we break here to avoid infinite loop.
        if (vpDeepPos.equals(previousVisiblePos.deepEquivalent()))
            break;
        characterOffset++;
        
        // When VisiblePostion moves to next node, it will count the leading line break as
        // 1 offset, which we shouldn't include in CharacterOffset.
        if (vpDeepPos.deprecatedNode() != previousVisiblePos.deepEquivalent().deprecatedNode()) {
            if (vp.characterBefore() == '\n')
                characterOffset--;
        }
    }
    
    return traverseToOffsetInRange(rangeForNodeContents(obj->node()), characterOffset);
}

AccessibilityObject* AXObjectCache::accessibilityObjectForTextMarkerData(TextMarkerData& textMarkerData)
{
    if (!isNodeInUse(textMarkerData.node))
        return nullptr;
    
    Node* domNode = textMarkerData.node;
    return this->getOrCreate(domNode);
}

void AXObjectCache::textMarkerDataForVisiblePosition(TextMarkerData& textMarkerData, const VisiblePosition& visiblePos)
{
    // This memory must be bzero'd so instances of TextMarkerData can be tested for byte-equivalence.
    // This also allows callers to check for failure by looking at textMarkerData upon return.
    memset(&textMarkerData, 0, sizeof(TextMarkerData));
    
    if (visiblePos.isNull())
        return;
    
    Position deepPos = visiblePos.deepEquivalent();
    Node* domNode = deepPos.deprecatedNode();
    ASSERT(domNode);
    if (!domNode)
        return;
    
    if (is<HTMLInputElement>(*domNode) && downcast<HTMLInputElement>(*domNode).isPasswordField())
        return;
    
    // If the visible position has an anchor type referring to a node other than the anchored node, we should
    // set the text marker data with CharacterOffset so that the offset will correspond to the node.
    CharacterOffset characterOffset = characterOffsetFromVisiblePosition(visiblePos);
    if (deepPos.anchorType() == Position::PositionIsAfterAnchor || deepPos.anchorType() == Position::PositionIsAfterChildren) {
        textMarkerDataForCharacterOffset(textMarkerData, characterOffset);
        return;
    }
    
    // find or create an accessibility object for this node
    AXObjectCache* cache = domNode->document().axObjectCache();
    RefPtr<AccessibilityObject> obj = cache->getOrCreate(domNode);
    
    textMarkerData.axID = obj.get()->axObjectID();
    textMarkerData.node = domNode;
    textMarkerData.offset = deepPos.deprecatedEditingOffset();
    textMarkerData.affinity = visiblePos.affinity();
    
    textMarkerData.characterOffset = characterOffset.offset;
    textMarkerData.characterStartIndex = characterOffset.startIndex;
    
    cache->setNodeInUse(domNode);
}

CharacterOffset AXObjectCache::nextCharacterOffset(const CharacterOffset& characterOffset, bool ignoreNextNodeStart)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    CharacterOffset next = characterOffsetForNodeAndOffset(*characterOffset.node, characterOffset.offset + 1);
    
    // To be consistent with VisiblePosition, we should consider the case that current node end to next node start counts 1 offset.
    bool isReplacedOrBR = isReplacedNodeOrBR(characterOffset.node) || isReplacedNodeOrBR(next.node);
    if (!ignoreNextNodeStart && !next.isNull() && !isReplacedOrBR && next.node != characterOffset.node)
        next = characterOffsetForNodeAndOffset(*next.node, 0, TraverseOptionIncludeStart);
    
    return next;
}

CharacterOffset AXObjectCache::previousCharacterOffset(const CharacterOffset& characterOffset, bool ignorePreviousNodeEnd)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    // To be consistent with VisiblePosition, we should consider the case that current node start to previous node end counts 1 offset.
    if (!ignorePreviousNodeEnd && !characterOffset.offset)
        return characterOffsetForNodeAndOffset(*characterOffset.node, 0);
    
    return characterOffsetForNodeAndOffset(*characterOffset.node, characterOffset.offset - 1, TraverseOptionIncludeStart);
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
    CharacterOffset start = startCharacterOffsetOfWord(characterOffset);
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
        return CharacterOffset();
    
    Node* boundary = parentEditingBoundary(characterOffset.node);
    if (!boundary)
        return CharacterOffset();
    
    RefPtr<Range> searchRange = rangeForNodeContents(boundary);
    Vector<UChar, 1024> string;
    unsigned prefixLength = 0;
    
    ExceptionCode ec = 0;
    if (requiresContextForWordBoundary(characterAfter(characterOffset))) {
        RefPtr<Range> backwardsScanRange(boundary->document().createRange());
        setRangeStartOrEndWithCharacterOffset(backwardsScanRange, characterOffset, false, ec);
        prefixLength = prefixLengthForRange(backwardsScanRange, string);
    }
    
    setRangeStartOrEndWithCharacterOffset(searchRange, characterOffset, true, ec);
    CharacterOffset end = startOrEndCharacterOffsetForRange(searchRange, false);
    
    ASSERT(!ec);
    if (ec)
        return CharacterOffset();
    
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
    
    ExceptionCode ec = 0;
    if (requiresContextForWordBoundary(characterBefore(characterOffset))) {
        RefPtr<Range> forwardsScanRange(boundary->document().createRange());
        forwardsScanRange->setEndAfter(boundary, ec);
        setRangeStartOrEndWithCharacterOffset(forwardsScanRange, characterOffset, true, ec);
        suffixLength = suffixLengthForRange(forwardsScanRange, string);
    }
    
    setRangeStartOrEndWithCharacterOffset(searchRange, characterOffset, false, ec);
    CharacterOffset start = startOrEndCharacterOffsetForRange(searchRange, true);
    
    ASSERT(!ec);
    if (ec)
        return CharacterOffset();
    
    SimplifiedBackwardsTextIterator it(*searchRange);
    unsigned next = backwardSearchForBoundaryWithTextIterator(it, string, suffixLength, searchFunction);
    
    if (!next)
        return it.atEnd() ? start : characterOffset;
    
    Node& node = it.atEnd() ? searchRange->startContainer() : it.range()->startContainer();
    if ((node.isTextNode() && static_cast<int>(next) <= node.maxCharacterOffset()) || (node.renderer() && node.renderer()->isBR() && !next)) {
        // The next variable contains a usable index into a text node
        if (&node == characterOffset.node)
            next -= characterOffset.startIndex;
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
    
    Node* startNode = characterOffset.node;
    
    if (isRenderedAsNonInlineTableImageOrHR(startNode))
        return startOrEndCharacterOffsetForRange(rangeForNodeContents(startNode), true);
    
    Node* startBlock = enclosingBlock(startNode);
    int offset = characterOffset.startIndex + characterOffset.offset;
    Position p(startNode, offset, Position::PositionIsOffsetInAnchor);
    Node* highestRoot = highestEditableRoot(p);
    Position::AnchorType type = Position::PositionIsOffsetInAnchor;
    
    Node* node = findStartOfParagraph(startNode, highestRoot, startBlock, offset, type, boundaryCrossingRule);
    
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
    Position p(startNode, offset, Position::PositionIsOffsetInAnchor);
    Node* highestRoot = highestEditableRoot(p);
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
    Position startPosition = range->startPosition();
    startPosition.getInlineBoxAndOffset(DOWNSTREAM, inlineBox, caretOffset);
    
    if (inlineBox)
        renderer = &inlineBox->renderer();
    
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
    
    // Since this would only work on rendered nodes, using VisiblePosition to create a collapsed
    // range should be fine.
    VisiblePosition vp = obj->visiblePositionForIndex(index);
    RefPtr<Range> range = makeRange(vp, vp);
    
    return startOrEndCharacterOffsetForRange(range, true);
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

void AXObjectCache::clearTextMarkerNodesInUse(Document* document)
{
    if (!document)
        return;
    
    // Check each node to see if it's inside the document being deleted, of if it no longer belongs to a document.
    HashSet<Node*> nodesToDelete;
    for (const auto& node : m_textMarkerNodes) {
        if (!node->inDocument() || &(node)->document() == document)
            nodesToDelete.add(node);
    }
    
    for (const auto& node : nodesToDelete)
        m_textMarkerNodes.remove(node);
}
    
bool AXObjectCache::nodeIsTextControl(const Node* node)
{
    if (!node)
        return false;

    const AccessibilityObject* axObject = getOrCreate(const_cast<Node*>(node));
    return axObject && axObject->isTextControl();
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
            const AtomicString& ariaHiddenValue = downcast<Element>(*testNode).fastGetAttribute(aria_hiddenAttr);
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
