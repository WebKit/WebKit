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

#include "AXObjectCache.h"

#include "AXImage.h"
#include "AXIsolatedObject.h"
#include "AXIsolatedTree.h"
#include "AXLogger.h"
#include "AXRemoteFrame.h"
#include "AXTextMarker.h"
#include "AccessibilityARIAGridCell.h"
#include "AccessibilityARIAGridRow.h"
#include "AccessibilityARIATable.h"
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
#include "AccessibilitySVGObject.h"
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
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "Editing.h"
#include "Editor.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementRareData.h"
#include "EventNames.h"
#include "FocusController.h"
#include "HTMLAreaElement.h"
#include "HTMLButtonElement.h"
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
#include "HTMLProgressElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTableElement.h"
#include "HTMLTablePartElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableSectionElement.h"
#include "HTMLTextFormControlElement.h"
#include "HitTestSource.h"
#include "InlineRunAndOffset.h"
#include "LocalFrame.h"
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
#include "RenderObjectInlines.h"
#include "RenderProgress.h"
#include "RenderSVGInlineText.h"
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
#include "TypedElementDescendantIteratorInlines.h"
#include <utility>
#include <wtf/DataLog.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/MakeString.h>

#if COMPILER(MSVC)
// See https://msdn.microsoft.com/en-us/library/1wea5zwe.aspx
#pragma warning(disable: 4701)
#endif

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXComputedObjectAttributeCache);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXObjectCache);

using namespace HTMLNames;

#if PLATFORM(COCOA)
// Post value change notifications for password fields or elements contained in password fields at a 40hz interval to thwart analysis of typing cadence
static const Seconds accessibilityPasswordValueChangeNotificationInterval { 25_ms };
#endif

static bool rendererNeedsDeferredUpdate(const RenderObject& renderer)
{
    ASSERT(!renderer.beingDestroyed());
    auto& document = renderer.document();
    return renderer.needsLayout() || document.needsStyleRecalc() || document.inRenderTreeUpdate() || (document.view() && document.view()->layoutContext().isInRenderTreeLayout());
}

static bool nodeRendererIsValid(Node& node)
{
    auto* renderer = node.renderer();
    return renderer && !renderer->beingDestroyed();
}

static bool nodeAndRendererAreValid(Node* node)
{
    return node ? nodeRendererIsValid(*node) : false;
}

AccessibilityObjectInclusion AXComputedObjectAttributeCache::getIgnored(AXID id) const
{
    auto it = m_idMapping.find(id);
    return it != m_idMapping.end() ? it->value.ignored : AccessibilityObjectInclusion::DefaultBehavior;
}

void AXComputedObjectAttributeCache::setIgnored(AXID id, AccessibilityObjectInclusion inclusion)
{
    UncheckedKeyHashMap<AXID, CachedAXObjectAttributes>::iterator it = m_idMapping.find(id);
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
    auto node = highestEditableRoot(position.deepEquivalent(), HasEditableAXRole);
    if (m_replacedText.length())
        cache->postTextReplacementNotification(node.get(), AXTextEditTypeDelete, m_replacedText, type, text, position);
    else
        cache->postTextStateChangeNotification(node.get(), type, text, position);
}

std::atomic<bool> AXObjectCache::gAccessibilityEnabled = false;
bool AXObjectCache::gAccessibilityEnhancedUserInterfaceEnabled = false;
std::atomic<bool> AXObjectCache::gForceDeferredSpellChecking = false;
#if ENABLE(AX_THREAD_TEXT_APIS)
std::atomic<bool> AXObjectCache::gAccessibilityThreadTextApisEnabled = false;
#endif
std::atomic<bool> AXObjectCache::gForceInitialFrameCaching = false;

bool AXObjectCache::accessibilityEnhancedUserInterfaceEnabled()
{
    ASSERT(isMainThread());
    return gAccessibilityEnhancedUserInterfaceEnabled;
}

void AXObjectCache::setEnhancedUserInterfaceAccessibility(bool flag)
{
    ASSERT(isMainThread());
    gAccessibilityEnhancedUserInterfaceEnabled = flag;
#if PLATFORM(MAC)
    if (flag)
        enableAccessibility();
#endif
}

void AXObjectCache::setForceInitialFrameCaching(bool shouldForce)
{
    gForceInitialFrameCaching = shouldForce;
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
bool AXObjectCache::shouldServeInitialCachedFrame()
{
    return !clientIsInTestMode() || forceInitialFrameCaching();
}

static const Seconds updateTreeSnapshotTimerInterval { 100_ms };
#endif

AXObjectCache::AXObjectCache(Document& document)
    : m_document(document)
    , m_pageID(document.pageID())
    , m_notificationPostTimer(*this, &AXObjectCache::notificationPostTimerFired)
    , m_passwordNotificationPostTimer(*this, &AXObjectCache::passwordNotificationPostTimerFired)
    , m_liveRegionChangedPostTimer(*this, &AXObjectCache::liveRegionChangedNotificationPostTimerFired)
    , m_currentModalElement(nullptr)
    , m_performCacheUpdateTimer(*this, &AXObjectCache::performCacheUpdateTimerFired)
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    , m_buildIsolatedTreeTimer(*this, &AXObjectCache::buildIsolatedTree)
    , m_geometryManager(AXGeometryManager::create(*this))
    , m_selectedTextRangeTimer(*this, &AXObjectCache::selectedTextRangeTimerFired, platformSelectedTextRangeDebounceInterval())
    , m_updateTreeSnapshotTimer(*this, &AXObjectCache::updateTreeSnapshotTimerFired)
#endif
{
    AXTRACE(makeString("AXObjectCache::AXObjectCache 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
#ifndef NDEBUG
    if (m_pageID)
        AXLOG(makeString("pageID "_s, m_pageID->loggingString()));
    else
        AXLOG("No pageID.");
#endif
    ASSERT(isMainThread());

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (auto* frame = document.frame(); frame && frame->isMainFrame())
        gAccessibilityThreadTextApisEnabled = DeprecatedGlobalSettings::accessibilityThreadTextApisEnabled();
#endif

    // If loading completed before the cache was created, loading progress will have been reset to zero.
    // Consider loading progress to be 100% in this case.
    m_loadingProgress = document.page() ? document.page()->progress().estimatedProgress() : 1;
    if (m_loadingProgress <= 0)
        m_loadingProgress = 1;

    if (m_pageID)
        m_pageActivityState = m_document->page()->activityState();
    AXTreeStore::add(m_id, WeakPtr { this });
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
    m_selectedTextRangeTimer.stop();
    m_updateTreeSnapshotTimer.stop();

    if (m_pageID) {
        if (auto tree = AXIsolatedTree::treeForPageID(*m_pageID))
            tree->setPageActivityState({ });
        AXIsolatedTree::removeTreeForPageID(*m_pageID);
    }
#endif

    AXTreeStore::remove(m_id);
}

bool AXObjectCache::isModalElement(Element& element) const
{
    if (hasAnyRole(element, { "dialog"_s, "alertdialog"_s }) && equalLettersIgnoringASCIICase(element.attributeWithDefaultARIA(aria_modalAttr), "true"_s))
        return true;

    RefPtr dialog = dynamicDowncast<HTMLDialogElement>(element);
    return dialog && dialog->isModal();
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
    // has accessible content is as easy as !getOrCreate(element)->unignoredChildren().isEmpty().
    // So don't call this method on anything besides modal elements.
    ASSERT(isModalElement(element));

    // Because computing any object's unignored children is dependent on whether a modal is on the page,
    // we'll need to walk the DOM and find non-ignored AX objects manually.
    Vector<Node*> nodeStack = { element.firstChild() };
    while (!nodeStack.isEmpty()) {
        for (auto* node = nodeStack.takeLast(); node; node = node->nextSibling()) {
            if (auto* axObject = getOrCreate(*node)) {
                if (!axObject->computeIsIgnored())
                    return true;

#if USE(ATSPI)
                // When using ATSPI, an accessibility object with 'StaticText' role is ignored.
                // Its content is exposed by its parent.
                // Treat such elements as having accessible content.
                // FIXME: This may not be sufficient for visibility:hidden or inert (https://bugs.webkit.org/show_bug.cgi?id=280914).
                if (axObject->roleValue() == AccessibilityRole::StaticText && !axObject->isAXHidden())
                    return true;
#endif
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
        // If multiple contain the keyboard focus, we want the deepest.
        // If no modal contains focus, we want to pick the last visible dialog in the DOM.
        RefPtr<Element> focusedElement = document().focusedElement();
        RefPtr<Element> modalElementToReturn;
        bool foundModalWithFocusInside = false;
        for (auto& element : m_modalElements) {
            // Elements in m_modalElementsSet may have become un-modal since we added them, but not yet removed
            // as part of the asynchronous m_deferredModalChangedList handling. Skip these.
            if (!element || !isModalElement(*element))
                continue;

            // To avoid trapping users in an empty modal, skip any non-visible element, or any element without accessible content.
            if (!isNodeVisible(element.get()) || !modalElementHasAccessibleContent(*element))
                continue;

            bool focusIsInsideElement = focusedElement && focusedElement->isDescendantOf(*element);

            // If the modal we found previously is a descendant of this one, prefer the descendant and skip this one.
            if (modalElementToReturn && foundModalWithFocusInside && modalElementToReturn->isDescendantOf(*element))
                continue;

            // If we already found a modal that focus is inside, and this one doesn't have focus inside, skip in favor of the one with focus inside.
            if (modalElementToReturn && foundModalWithFocusInside && !focusIsInsideElement)
                continue;

            modalElementToReturn = element.get();
            if (focusIsInsideElement)
                foundModalWithFocusInside = true;
        }

        bool focusedElementIsOutsideModals = focusedElement && !foundModalWithFocusInside;

        // If there is a focused element, and it's not inside any of the modals, we should
        // consider all modals inactive to allow the user to freely navigate.
        if (focusedElementIsOutsideModals && willRecomputeFocus == WillRecomputeFocus::No)
            return nullptr;

        return modalElementToReturn.get();
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
    RefPtr element = dynamicDowncast<Element>(node);
    if (!element)
        return false;

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return false;

    const auto& style = renderer->style();
    if (style.display() == DisplayType::None)
        return false;

    auto* renderLayer = renderer->enclosingLayer();
    if (style.usedVisibility() != Visibility::Visible && renderLayer && !renderLayer->hasVisibleContent())
        return false;

    // Check whether this object or any of its ancestors has opacity 0.
    // The resulting opacity of a RenderObject is computed as the multiplication
    // of its opacity times the opacities of its ancestors.
    for (auto* renderObject = renderer; renderObject; renderObject = renderObject->parent()) {
        if (!renderObject->style().opacity())
            return false;
    }

    // We also need to consider aria hidden status.
    return !equalLettersIgnoringASCIICase(element->attributeWithDefaultARIA(aria_hiddenAttr), "true"_s) || element->focused();
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

AccessibilityObject* AXObjectCache::focusedImageMapUIElement(HTMLAreaElement& areaElement)
{
    // Find the corresponding accessibility object for the HTMLAreaElement. This should be
    // in the list of children for its corresponding image.
    RefPtr imageElement = areaElement.imageElement();
    if (!imageElement)
        return nullptr;

    AccessibilityObject* axRenderImage = areaElement.protectedDocument()->axObjectCache()->getOrCreate(*imageElement);
    if (!axRenderImage)
        return nullptr;
    
    for (const auto& child : axRenderImage->unignoredChildren()) {
        auto* imageMapLink = dynamicDowncast<AccessibilityImageMapLink>(*child);
        if (imageMapLink && imageMapLink->areaElement() == &areaElement)
            return imageMapLink;
    }
    
    return nullptr;
}

AccessibilityObject* AXObjectCache::focusedObjectForPage(const Page* page)
{
    ASSERT(isMainThread());

    if (!gAccessibilityEnabled)
        return nullptr;

    // get the focused node in the page
    RefPtr focusedOrMainFrame = page->checkedFocusController()->focusedOrMainFrame();
    if (!focusedOrMainFrame)
        return nullptr;
    RefPtr document = focusedOrMainFrame->document();
    if (!document)
        return nullptr;

    document->updateStyleIfNeeded();
    if (RefPtr focusedElement = document->focusedElement())
        return focusedObjectForNode(focusedElement.get());
    return focusedObjectForNode(document.get());
}

AccessibilityObject* AXObjectCache::focusedObjectForNode(Node* focusedNode)
{
    if (auto* area = dynamicDowncast<HTMLAreaElement>(focusedNode))
        return focusedImageMapUIElement(*area);

    auto* focus = getOrCreate(focusedNode);
    if (!focus)
        return nullptr;

    if (focus->shouldFocusActiveDescendant()) {
        if (auto* descendant = focus->activeDescendant())
            return dynamicDowncast<AccessibilityObject>(descendant);
    }

    if (focus->isIgnored())
        return focus->parentObjectUnignored();
    return focus;
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::setIsolatedTreeFocusedObject(AccessibilityObject* focus)
{
    ASSERT(isMainThread());

    if (auto tree = AXIsolatedTree::treeForPageID(m_pageID))
        tree->setFocusedNodeID(focus ? std::optional { focus->objectID() } : std::nullopt);
}
#endif

AccessibilityObject* AXObjectCache::get(Widget& widget) const
{
    auto axID = m_widgetObjectMapping.getOptional(widget);
    return axID ? m_objects.get(*axID) : nullptr;
}

AccessibilityObject* AXObjectCache::get(RenderObject& renderer) const
{
    auto axID = m_renderObjectMapping.getOptional(renderer);
    return axID ? m_objects.get(*axID) : nullptr;
}

AccessibilityObject* AXObjectCache::get(Node& node) const
{
    auto* renderer = node.renderer();
    auto renderID = renderer ? m_renderObjectMapping.getOptional(*renderer) : std::nullopt;
    if (renderID)
        return m_objects.get(*renderID);

    auto nodeID = m_nodeObjectMapping.getOptional(node);
    return nodeID ? m_objects.get(*nodeID) : nullptr;
}

ContainerNode* composedParentIgnoringDocumentFragments(Node& node)
{
    RefPtr ancestor = node.parentInComposedTree();
    while (is<DocumentFragment>(ancestor.get()))
        ancestor = ancestor->parentInComposedTree();
    return ancestor.get();
}

ContainerNode* composedParentIgnoringDocumentFragments(Node* node)
{
    return node ? composedParentIgnoringDocumentFragments(*node) : nullptr;
}

bool hasAccNameAttribute(Element& element)
{
    auto trimmed = [&] (const auto& attribute) {
        const auto& value = element.attributeWithDefaultARIA(attribute);
        if (value.isEmpty())
            return emptyString();
        auto copy = value.string();
        return copy.trim(isASCIIWhitespace);
    };

    // Avoid calculating the actual description here (e.g. resolving aria-labelledby), as it's expensive.
    // The spec is generally permissive in allowing user agents to not ensure complete validity of these attributes.
    // For example, https://w3c.github.io/svg-aam/#include_elements:
    // "It has an ‘aria-labelledby’ attribute or ‘aria-describedby’ attribute containing valid IDREF tokens. User agents MAY include elements with these attributes without checking for validity."
    if (trimmed(aria_labelAttr).length() || trimmed(aria_labelledbyAttr).length() || trimmed(aria_labeledbyAttr).length() || trimmed(aria_descriptionAttr).length() || trimmed(aria_describedbyAttr).length())
        return true;

    return element.attributeWithoutSynchronization(titleAttr).length();
}

static RenderImage* toSimpleImage(RenderObject& renderer)
{
    CheckedPtr renderImage = dynamicDowncast<RenderImage>(renderer);
    if (!renderImage)
        return nullptr;

    // Exclude ImageButtons because they are treated as buttons, not as images.
    RefPtr node = renderer.node();
    if (is<HTMLInputElement>(node))
        return nullptr;

    // ImageMaps are not simple images.
    if (renderImage->imageMap())
        return nullptr;

    if (RefPtr imgElement = dynamicDowncast<HTMLImageElement>(node); imgElement && imgElement->hasAttributeWithoutSynchronization(usemapAttr))
        return nullptr;

#if ENABLE(VIDEO)
    // Exclude video and audio elements.
    if (is<HTMLMediaElement>(node))
        return nullptr;
#endif // ENABLE(VIDEO)

    return renderImage.get();
}

// FIXME: This probably belongs on Element.
bool hasRole(Element& element, StringView role)
{
    auto roleValue = element.attributeWithDefaultARIA(roleAttr);
    if (role.isNull())
        return roleValue.isEmpty();
    if (roleValue.isEmpty())
        return false;

    return SpaceSplitString::spaceSplitStringContainsValue(roleValue, role, SpaceSplitString::ShouldFoldCase::Yes);
}

bool hasAnyRole(Element& element, Vector<StringView>&& roles)
{
    auto roleValue = element.attributeWithDefaultARIA(roleAttr);
    if (roleValue.isEmpty())
        return false;

    for (const auto& role : roles) {
        ASSERT(!role.isEmpty());
        if (SpaceSplitString::spaceSplitStringContainsValue(roleValue, role, SpaceSplitString::ShouldFoldCase::Yes))
            return true;
    }
    return false;
}

bool hasAnyRole(Element* element, Vector<StringView>&& roles)
{
    return element ? hasAnyRole(*element, WTFMove(roles)) : false;
}

bool hasTableRole(Element& element)
{
    return hasAnyRole(element, { "grid"_s, "table"_s, "treegrid"_s });
}

bool hasCellARIARole(Element& element)
{
    return hasAnyRole(element, { "gridcell"_s, "cell"_s, "columnheader"_s, "rowheader"_s });
}

bool hasPresentationRole(Element& element)
{
    return hasAnyRole(element, { "presentation"_s, "none"_s });
}

bool isRowGroup(Element& element)
{
    auto tagName = element.localName();
    return tagName == theadTag || tagName == tbodyTag || tagName == tfootTag || hasRole(element, "rowgroup"_s);
}

bool isRowGroup(Node* node)
{
    auto* element = dynamicDowncast<Element>(node);
    return element && isRowGroup(*element);
}

static bool isAccessibilityList(Element& element)
{
    if (hasAnyRole(element, { "list"_s, "directory"_s }))
        return true;

    // Call it a list if it has no ARIA role and a list tag.
    auto tagName = element.localName();
    return hasRole(element, nullAtom()) && (tagName == ulTag || tagName == olTag || tagName == dlTag || tagName == menuTag);
}

static bool isAccessibilityTree(Element& element)
{
    return hasRole(element, "tree"_s);
}

static bool isAccessibilityTreeItem(Element& element)
{
    return hasRole(element, "treeitem"_s);
}

static bool isAccessibilityTable(Node* node)
{
    return is<HTMLTableElement>(node);
}

static bool isAccessibilityTableRow(Node* node)
{
    return is<HTMLTableRowElement>(node);
}

static bool isAccessibilityTableCell(Node* node)
{
    return is<HTMLTableCellElement>(node);
}

static bool isAccessibilityARIATable(Element& element)
{
    return hasTableRole(element);
}

static bool isAccessibilityARIAGridRow(Element& element)
{
    return hasRole(element, "row"_s);
}

static bool isAccessibilityARIAGridCell(Element& element)
{
    return hasCellARIARole(element);
}

Ref<AccessibilityRenderObject> AXObjectCache::createObjectFromRenderer(RenderObject& renderer)
{
    RefPtr node = renderer.node();
    if (auto* element = dynamicDowncast<Element>(node.get())) {
        if (isAccessibilityList(*element))
            return AccessibilityList::create(generateNewObjectID(), renderer);

        if (isAccessibilityARIATable(*element))
            return AccessibilityARIATable::create(generateNewObjectID(), renderer);
        if (isAccessibilityARIAGridRow(*element))
            return AccessibilityARIAGridRow::create(generateNewObjectID(), renderer);
        if (isAccessibilityARIAGridCell(*element))
            return AccessibilityARIAGridCell::create(generateNewObjectID(), renderer);

        if (isAccessibilityTree(*element))
            return AccessibilityTree::create(generateNewObjectID(), renderer);
        if (isAccessibilityTreeItem(*element))
            return AccessibilityTreeItem::create(generateNewObjectID(), renderer);

        if (is<HTMLLabelElement>(*element) && hasRole(*element, nullAtom()))
            return AccessibilityLabel::create(generateNewObjectID(), renderer);

#if PLATFORM(IOS_FAMILY)
        if (is<HTMLMediaElement>(*element) && hasRole(*element, nullAtom()))
            return AccessibilityMediaObject::create(generateNewObjectID(), renderer);
#endif
    }

    if (renderer.isRenderOrLegacyRenderSVGRoot())
        return AccessibilitySVGRoot::create(generateNewObjectID(), renderer, this);

    if (is<SVGElement>(node) || is<RenderSVGInlineText>(renderer))
        return AccessibilitySVGObject::create(generateNewObjectID(), renderer, this);

    if (auto* renderImage = toSimpleImage(renderer))
        return AXImage::create(generateNewObjectID(), *renderImage);

#if ENABLE(MATHML)
    // The mfenced element creates anonymous RenderMathMLOperators which should be treated
    // as MathML elements and assigned the MathElementRole so that platform logic regarding
    // inclusion and role mapping is not bypassed.
    bool isAnonymousOperator = renderer.isAnonymous() && is<RenderMathMLOperator>(renderer);
    if (isAnonymousOperator || is<MathMLElement>(node))
        return AccessibilityMathMLElement::create(generateNewObjectID(), renderer, isAnonymousOperator);
#endif

    if (is<RenderListBox>(renderer))
        return AccessibilityListBox::create(generateNewObjectID(), renderer);
    if (auto* renderMenuList = dynamicDowncast<RenderMenuList>(renderer))
        return AccessibilityMenuList::create(generateNewObjectID(), *renderMenuList);

    bool isAnonymous = false;
#if USE(ATSPI)
    // This branch is only necessary because ATSPI walks the render tree rather than the DOM to build the accessiblity tree.
    // FIXME: Consider removing this with https://bugs.webkit.org/show_bug.cgi?id=282117.
    isAnonymous = renderer.isAnonymous();
#endif
    // Some websites put display:table on tbody / thead / tfoot, resulting in a RenderTable being generated.
    // We don't want to consider these tables (since they are typically wrapped by an actual <table> element),
    // so only create an AccessibilityTable when !is<HTMLTableSectionElement>.
    if ((is<RenderTable>(renderer) && !isAnonymous && !is<HTMLTableSectionElement>(node.get())) || isAccessibilityTable(node.get()))
        return AccessibilityTable::create(generateNewObjectID(), renderer);
    if ((is<RenderTableRow>(renderer) && !isAnonymous) || isAccessibilityTableRow(node.get()))
        return AccessibilityTableRow::create(generateNewObjectID(), renderer);
    if ((is<RenderTableCell>(renderer) && !isAnonymous) || isAccessibilityTableCell(node.get()))
        return AccessibilityTableCell::create(generateNewObjectID(), renderer);

    // Progress indicator.
    if (is<RenderProgress>(renderer) || is<RenderMeter>(renderer)
        || is<HTMLProgressElement>(node) || is<HTMLMeterElement>(node))
        return AccessibilityProgressIndicator::create(generateNewObjectID(), renderer);

#if ENABLE(ATTACHMENT_ELEMENT)
    if (auto* renderAttachment = dynamicDowncast<RenderAttachment>(renderer))
        return AccessibilityAttachment::create(generateNewObjectID(), *renderAttachment);
#endif

    // input type=range
    if (is<RenderSlider>(renderer))
        return AccessibilitySlider::create(generateNewObjectID(), renderer);

    return AccessibilityRenderObject::create(generateNewObjectID(), renderer);
}

Ref<AccessibilityNodeObject> AXObjectCache::createFromNode(Node& node)
{
    if (auto* element = dynamicDowncast<Element>(node)) {
        if (isAccessibilityList(*element))
            return AccessibilityList::create(generateNewObjectID(), *element);
        if (isAccessibilityTable(element))
            return AccessibilityTable::create(generateNewObjectID(), *element);
        if (isAccessibilityTableRow(element))
            return AccessibilityTableRow::create(generateNewObjectID(), *element);
        if (isAccessibilityTableCell(element))
            return AccessibilityTableCell::create(generateNewObjectID(), *element);
        if (isAccessibilityTree(*element))
            return AccessibilityTree::create(generateNewObjectID(), *element);
        if (isAccessibilityTreeItem(*element))
            return AccessibilityTreeItem::create(generateNewObjectID(), *element);
        if (isAccessibilityARIATable(*element))
            return AccessibilityARIATable::create(generateNewObjectID(), *element);
        if (isAccessibilityARIAGridRow(*element))
            return AccessibilityARIAGridRow::create(generateNewObjectID(), *element);
        if (isAccessibilityARIAGridCell(*element))
            return AccessibilityARIAGridCell::create(generateNewObjectID(), *element);
    }
    return AccessibilityNodeObject::create(generateNewObjectID(), node);
}

void AXObjectCache::cacheAndInitializeWrapper(AccessibilityObject& newObject, DOMObjectVariant domObject)
{
    AXID axID = newObject.objectID();

    WTF::switchOn(domObject,
        [&] (RenderObject* typedValue) {
            m_renderObjectMapping.set(*typedValue, axID);
            if (auto* node = typedValue->node()) {
                // If this node existed in the m_nodeObjectMapping (ie. it is replacing an old object), we should
                // update the object mapping so it is up to date the next time it is replaced.
                auto objectMapIterator = m_nodeObjectMapping.find(*node);
                if (objectMapIterator != m_nodeObjectMapping.end())
                    objectMapIterator->value = axID;
            }
        },
        [&] (Node* typedValue) { m_nodeObjectMapping.set(*typedValue, axID); },
        [&] (Widget* typedValue) { m_widgetObjectMapping.set(*typedValue, axID); },
        [] (auto&) { }
    );

    m_objects.set(axID, newObject);
    newObject.init();
    attachWrapper(newObject);
}

AccessibilityObject* AXObjectCache::getOrCreate(Widget& widget)
{
    if (auto* object = get(widget))
        return object;

    RefPtr<AccessibilityObject> newObject;
    if (auto* scrollView = dynamicDowncast<ScrollView>(widget))
        newObject = AccessibilityScrollView::create(generateNewObjectID(), *scrollView);
    else if (auto* scrollbar = dynamicDowncast<Scrollbar>(widget))
        newObject = AccessibilityScrollbar::create(generateNewObjectID(), *scrollbar);

    // Will crash later if we have two objects for the same widget.
    ASSERT(!get(widget));

    // Ensure we weren't given an unsupported widget type.
    ASSERT(newObject);
    if (!newObject)
        return nullptr;

    cacheAndInitializeWrapper(*newObject, &widget);
    return newObject.get();
}

AccessibilityObject* AXObjectCache::getOrCreate(Node& node, IsPartOfRelation isPartOfRelation)
{
    if (auto* object = get(node))
        return object;

    if (auto* renderer = node.renderer())
        return getOrCreate(*renderer);

    RefPtr composedParent = node.parentElementInComposedTree();
    if (!composedParent)
        return nullptr;

    Ref protectedNode { node };

    auto* optionElement = dynamicDowncast<HTMLOptionElement>(node);
    auto* optGroupElement = dynamicDowncast<HTMLOptGroupElement>(node);
    if (optionElement || optGroupElement) {
        auto* select = optionElement
            ? optionElement->ownerSelectElement()
            : optGroupElement->ownerSelectElement();
        if (!select)
            return nullptr;
        RefPtr<AccessibilityObject> object;
        if (select->usesMenuList()) {
            if (!optionElement || !select->renderer())
                return nullptr;
            object = AccessibilityMenuListOption::create(generateNewObjectID(), *optionElement);
        } else
            object = AccessibilityListBoxOption::create(generateNewObjectID(), downcast<HTMLElement>(node));
        cacheAndInitializeWrapper(*object, &node);
        return object.get();
    }

    bool inCanvasSubtree = lineageOfType<HTMLCanvasElement>(*composedParent).first();
    if (inCanvasSubtree) {
        // Don't include objects that are descendants of user agent shadow trees. For example, in this HTML:
        //     <canvas><input type="text"></canvas>
        // <input type="text"> generates a user agent shadow root to host a <div contenteditable>.
        // We already handle elements that host user agent shadows specially, so don't include their
        // descendants (like this <div>) just because they happen to be within <canvas>.
        if (auto* shadowRoot = composedParent->shadowRoot())
            inCanvasSubtree = !shadowRoot->isUserAgentShadowRoot();
    }
    // If node is the target of a relationship or a descendant of one, create an AX object unconditionally.
    if (isPartOfRelation == IsPartOfRelation::No && !isDescendantOfRelatedNode(node)) {
        bool insideMeterElement = is<HTMLMeterElement>(*composedParent);
        auto* element = dynamicDowncast<Element>(node);
        bool hasDisplayContents = element && element->hasDisplayContents();
        bool isPopover = element && element->hasAttributeWithoutSynchronization(popoverAttr);
        if (!inCanvasSubtree && !insideMeterElement && !hasDisplayContents && !isPopover && !isNodeFocused(node))
            return nullptr;
    }

    // The object may have already been created during relations update.
    if (auto* object = get(node))
        return object;

    // Fallback content is only focusable as long as the canvas is displayed and visible.
    // Update the style before Element::isFocusable() gets called.
    if (inCanvasSubtree)
        node.protectedDocument()->updateStyleIfNeeded();

    RefPtr newObject = createFromNode(node);

    // Will crash later if we have two objects for the same node.
    ASSERT(!get(node));
    cacheAndInitializeWrapper(*newObject, &node);
    // Compute the object's initial ignored status.
    newObject->recomputeIsIgnored();
    // Sometimes asking isIgnored() will cause the newObject to be deallocated, and then
    // it will disappear when this function is finished, leading to a use-after-free.
    if (newObject->isDetached())
        return nullptr;
    return newObject.get();
}

AccessibilityObject* AXObjectCache::getOrCreate(RenderObject& renderer)
{
    if (auto* object = get(renderer))
        return object;

    // Don't create an object for this renderer if it's being destroyed.
    if (renderer.beingDestroyed())
        return nullptr;

    Ref object = createObjectFromRenderer(renderer);

    // Will crash later if we have two objects for the same renderer.
    ASSERT(!get(renderer));

    cacheAndInitializeWrapper(object.get(), &renderer);
    // Compute the object's initial ignored status.
    object->recomputeIsIgnored();
    // Sometimes asking isIgnored() will cause the newObject to be deallocated, and then
    // it will disappear when this function is finished, leading to a use-after-free.
    if (object->isDetached())
        return nullptr;

    return object.ptr();
}

AXCoreObject* AXObjectCache::rootObject()
{
    if (!gAccessibilityEnabled)
        return nullptr;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (isIsolatedTreeEnabled())
        return isolatedTreeRootObject();
#endif

    return getOrCreate(m_document->protectedView().get());
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
RefPtr<AXIsolatedTree> AXObjectCache::getOrCreateIsolatedTree()
{
    AXTRACE(makeString("AXObjectCache::getOrCreateIsolatedTree 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    ASSERT(isMainThread());

    if (!m_pageID)
        return nullptr;

    RefPtr tree = AXIsolatedTree::treeForPageID(m_pageID);
    if (tree)
        return tree;

    // A new isolated tree needs to be created. Initialize the GeometryManager primary screen rect to be ready when needed.
    m_geometryManager->initializePrimaryScreenRect();
    // Schedule a paint to cache the rects for the objects in this new isolated tree.
    scheduleObjectRegionsUpdate(true /* scheduleImmediately */);

    // This method can be called as the result of a client request. Since creating the isolated tree can take long,
    // especially for large documents, for real clients we build a temporary "empty" isolated tree consisting only of the ScrollView and the WebArea objects.
    // Then we schedule building the entire isolated tree on a Timer.
    // For test clients, LayoutTests or XCTests, build the whole isolated tree.
    if (LIKELY(!clientIsInTestMode())) {
        tree = AXIsolatedTree::createEmpty(*this);
        if (!m_buildIsolatedTreeTimer.isActive())
            m_buildIsolatedTreeTimer.startOneShot(0_s);
    } else
        tree = AXIsolatedTree::create(*this);
    setIsolatedTreeRoot(tree->rootNode().get());

    initializeAXThreadIfNeeded();

    return tree;
}

void AXObjectCache::buildIsolatedTree()
{
    m_buildIsolatedTreeTimer.stop();

    if (!m_pageID)
        return;

    auto tree = AXIsolatedTree::create(*this);
    setIsolatedTreeRoot(tree->rootNode().get());

    if (RefPtr webArea = rootWebArea()) {
        postPlatformNotification(*webArea, AXNotification::AXLoadComplete);
        postPlatformNotification(*webArea, AXNotification::AXFocusedUIElementChanged);
    }
}

AXCoreObject* AXObjectCache::isolatedTreeRootObject()
{
    if (auto tree = getOrCreateIsolatedTree())
        return tree->rootNode().get();

    // Should not get here, couldn't create the IsolatedTree.
    ASSERT_NOT_REACHED();
    return nullptr;
}

void AXObjectCache::setIsolatedTreeRoot(AXCoreObject* root)
{
    ASSERT(isMainThread());
    if (RefPtr frame = m_document->frame())
        frame->loader().client().setAXIsolatedTreeRoot(root);
}
#endif

AccessibilityObject* AXObjectCache::rootObjectForFrame(LocalFrame* frame)
{
    if (!gAccessibilityEnabled)
        return nullptr;

    if (!frame)
        return nullptr;
    return getOrCreate(frame->view());
}    

AccessibilityObject* AXObjectCache::create(AccessibilityRole role)
{
    RefPtr<AccessibilityObject> object;
    switch (role) {
    case AccessibilityRole::ImageMapLink:
        object = AccessibilityImageMapLink::create(generateNewObjectID());
        break;
    case AccessibilityRole::Column:
        object = AccessibilityTableColumn::create(generateNewObjectID());
        break;
    case AccessibilityRole::TableHeaderContainer:
        object = AccessibilityTableHeaderContainer::create(generateNewObjectID());
        break;
    case AccessibilityRole::RemoteFrame:
        object = AXRemoteFrame::create(generateNewObjectID());
        break;
    case AccessibilityRole::SliderThumb:
        object = AccessibilitySliderThumb::create(generateNewObjectID());
        break;
    case AccessibilityRole::MenuListPopup:
        object = AccessibilityMenuListPopup::create(generateNewObjectID());
        break;
    case AccessibilityRole::SpinButton:
        object = AccessibilitySpinButton::create(generateNewObjectID(), *this);
        break;
    case AccessibilityRole::SpinButtonPart:
        object = AccessibilitySpinButtonPart::create(generateNewObjectID());
        break;
    default:
        break;
    }

    if (!object)
        return nullptr;

    cacheAndInitializeWrapper(*object);
    return object.get();
}

void AXObjectCache::remove(std::optional<AXID> axID)
{
    AXTRACE(makeString("AXObjectCache::remove 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    AXLOG(makeString("AXID "_s, axID ? axID->loggingString() : ""_s));

    if (!axID)
        return;

    auto object = m_objects.take(*axID);
    if (!object)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (auto tree = AXIsolatedTree::treeForPageID(m_pageID))
        tree->removeNode(*object);
#endif

    removeAllRelations(*axID);
    object->detach(AccessibilityDetachmentType::ElementDestroyed);

    m_idsInUse.remove(*axID);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    m_geometryManager->remove(*axID);
#endif
    ASSERT(m_objects.size() >= m_idsInUse.size());
}

void AXObjectCache::remove(RenderObject* renderer)
{
    AXTRACE(makeString("AXObjectCache::remove RenderObject* 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));

    if (renderer)
        remove(m_renderObjectMapping.takeOptional(*renderer));
}

void AXObjectCache::remove(Node& node)
{
    AXTRACE(makeString("AXObjectCache::remove Node& 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));

    remove(m_nodeObjectMapping.takeOptional(node));
    remove(node.renderer());

    // If we're in the middle of a cache update, don't modify any of these vectors because we are currently
    // iterating over them. They will be cleared at the end of the cache update, so not removing them here is fine.
    if (m_performingDeferredCacheUpdate) {
        AXLOG("Bailing out before removing node from m_deferred* vectors as we are in the middle of a cache update.");
        return;
    }

    // We cannot use RefPtr here as node's m_deletionHasBegun is true.
    if (auto* nodeElement = dynamicDowncast<Element>(node)) {
        m_deferredTextFormControlValue.remove(*nodeElement);
        m_deferredAttributeChange.removeAllMatching([&node] (const auto& entry) {
            return entry.element == &node;
        });
        m_modalElements.removeAllMatching([&nodeElement] (const auto& element) {
            return nodeElement == element.get();
        });
        m_deferredRecomputeIsIgnoredList.remove(*nodeElement);
        m_deferredRecomputeTableIsExposedList.remove(*nodeElement);
        m_deferredSelectedChildredChangedList.remove(*nodeElement);
        m_deferredModalChangedList.remove(*nodeElement);
        m_deferredMenuListChange.remove(*nodeElement);
        m_deferredElementAddedOrRemovedList.remove(*nodeElement);
    }
    m_deferredTextChangedList.remove(node);
}

void AXObjectCache::remove(Widget* view)
{
    if (!view)
        return;
    remove(m_widgetObjectMapping.takeOptional(*view));
    if (auto* scrollView = dynamicDowncast<ScrollView>(*view))
        m_deferredScrollbarUpdateChangeList.remove(*scrollView);
}

AXID AXObjectCache::generateNewObjectID()
{
    AXID axID = AXID::generate();
    while (m_idsInUse.contains(axID))
        axID = AXID::generate();

    m_idsInUse.add(axID);
    return axID;
}

void AXObjectCache::handleTextChanged(AccessibilityObject* object)
{
    AXTRACE(makeString("AXObjectCache::handleTextChanged 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    AXLOG(object);

    if (!object)
        return;

    Ref<AccessibilityObject> protectedObject(*object);

    bool isText = object->isStaticText();
    // If this element supports ARIA live regions, or is part of a region with an ARIA editable role,
    // then notify the AT of changes.
    bool notifiedNonNativeTextControl = false;
    for (RefPtr ancestor = object; ancestor; ancestor = ancestor->parentObject()) {
        if (ancestor->supportsLiveRegion())
            postLiveRegionChangeNotification(*ancestor);

        if (!notifiedNonNativeTextControl && ancestor->isNonNativeTextControl()) {
            postNotification(ancestor.get(), ancestor->protectedDocument().get(), AXValueChanged);
            notifiedNonNativeTextControl = true;
        }

        if (isText) {
            bool dependsOnTextUnderElement = ancestor->dependsOnTextUnderElement();
            auto role = ancestor->roleValue();
            dependsOnTextUnderElement |= role == AccessibilityRole::Label || role == AccessibilityRole::TextField;

            // If the starting object is a static text, its underlying text has changed.
            if (dependsOnTextUnderElement) {
                // Inform this ancestor its textUnderElement-dependent data is now out-of-date.
                postNotification(ancestor.get(), nullptr, AXTextUnderElementChanged);
            }

            // Any objects this ancestor labeled now also need new AccessibilityText.
            auto labeledObjects = ancestor->labelForObjects();
            for (const auto& labeledObject : labeledObjects)
                postNotification(downcast<AccessibilityObject>(labeledObject.get()), nullptr, AXTextChanged);
        }
    }

    postNotification(object, object->protectedDocument().get(), AXTextChanged);
    object->recomputeIsIgnored();
}

void AXObjectCache::onRendererCreated(Element& element)
{
    if (!element.renderer()) {
        ASSERT_NOT_REACHED();
        return;
    }

    // If there is already an AXObject that was created for this element,
    // remove it since there will be a new AXRenderObject created using the renderer.
    if (auto axID = m_nodeObjectMapping.getOptional(element)) {
        // The removal needs to be async because this is called during a RenderTree
        // update and remove(AXID) updates the isolated tree, that in turn calls
        // parentObjectUnignored() on the object being removed, that may result
        // in a call to textUnderElement, that can not be called during a layout.
        m_deferredReplacedObjects.add(*axID);
        if (!m_performCacheUpdateTimer.isActive())
            m_performCacheUpdateTimer.startOneShot(0_s);
    }
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
static bool isClickEvent(const AtomString& eventType)
{
    return eventType == eventNames().clickEvent
        || eventType == eventNames().mousedownEvent
        || eventType == eventNames().mouseupEvent;
}
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

void AXObjectCache::onEventListenerAdded(Node& node, const AtomString& eventType)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isClickEvent(eventType))
        return;

    if (RefPtr tree = AXIsolatedTree::treeForPageID(m_pageID))
        tree->updateNodeProperties(get(node), { AXPropertyName::HasClickHandler });
#else
    UNUSED_PARAM(node);
    UNUSED_PARAM(eventType);
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
}

void AXObjectCache::onEventListenerRemoved(Node& node, const AtomString& eventType)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isClickEvent(eventType))
        return;

    if (RefPtr tree = AXIsolatedTree::treeForPageID(m_pageID))
        tree->updateNodeProperties(get(node), { AXPropertyName::HasClickHandler });
#else
    UNUSED_PARAM(node);
    UNUSED_PARAM(eventType);
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
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

void AXObjectCache::handleAllDeferredChildrenChanged()
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    RefPtr<AXIsolatedTree> tree;
    if (!m_deferredChildrenChangedList.isEmpty())
        tree = AXIsolatedTree::treeForPageID(m_pageID);
#endif

    // Because m_deferredChildrenChangedList can be appended to while we iterate, we have to process
    // this list in two steps (repeatedly until empty) to ensure the isolated tree is updated correctly.
    // Specifically, it's possible for an entry to be appended during AXIsolatedTree::updateChildren (e.g.
    // due to an object re-computing is-ignored to a different value).
    while (!m_deferredChildrenChangedList.isEmpty()) {
        // Perform AXObjectCache::handleChildrenChanged on all objects first, then update the isolated tree
        // afterwards. Doing it in two steps prevents thrashing m_subtreeDirty (i.e. AXObjectCache::handleChildrenChanged
        // setting m_subtreeDirty on some high-in-the-tree object, clearing that during AXIsolatedTree::updateChildren,
        // then having it set again by the next children-changed entry, repeat).
        auto deferredChildrenChangedList = std::exchange(m_deferredChildrenChangedList, { });
        for (auto& object : deferredChildrenChangedList)
            handleChildrenChanged(object);

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        if (tree)
            tree->updateChildrenForObjects(deferredChildrenChangedList);
#endif

#if !PLATFORM(COCOA)
        // Neither the MAC nor IOS_FAMILY ports map AXChildrenChanged to a platform notification.
        for (auto& object : deferredChildrenChangedList)
            postPlatformNotification(object, AXChildrenChanged);
#endif
    }
}

void AXObjectCache::handleChildrenChanged(AccessibilityObject& object)
{
    AXTRACE("AXObjectCache::handleChildrenChanged"_s);
    AXLOG(object);

    // Handle MenuLists and MenuListPopups as special cases.
    if (is<AccessibilityMenuList>(object)) {
        const auto& children = object.unignoredChildren(/* updateChildrenIfNeeded */ false);
        if (children.isEmpty())
            return;

        ASSERT(children.size() == 1);
        handleChildrenChanged(downcast<AccessibilityObject>(*children[0]));
    } else if (auto* menuListPopup = dynamicDowncast<AccessibilityMenuListPopup>(object)) {
        menuListPopup->handleChildrenChanged();
        return;
    } else if (auto* axTable = dynamicDowncast<AccessibilityTable>(object))
        deferRecomputeTableCellSlots(*axTable);
    else if (auto* axRow = dynamicDowncast<AccessibilityTableRow>(object)) {
        if (auto* parentTable = axRow->parentTable())
            deferRecomputeTableCellSlots(*parentTable);
    } else if (auto* scrollView = dynamicDowncast<AccessibilityScrollView>(object)) {
        // When the children of an iframe change, e.g., because its visibility changes,
        // then we need to dirty the web area's subtree since the scroll area doesn't
        // have a node nor renderer, thus, failing the check below and returning early.

        // Only do this when the web area is not the root web area, as this indicates
        // we are in an iframe.
        if (RefPtr webArea = scrollView->webAreaObject(); webArea && webArea != rootWebArea()) {
            webArea->setNeedsToUpdateSubtree();
            webArea->setNeedsToUpdateChildren();
        }
    }

    if (!object.node() && !object.renderer())
        return;

    // Should make the subtree dirty so that everything below will be updated correctly.
    object.setNeedsToUpdateSubtree();

    object.recomputeIsIgnored();

    // Go up the existing ancestors chain and fire the appropriate notifications.
    bool shouldUpdateParent = true;
    bool foundTableCaption = false;
    for (RefPtr parent = &object; parent; parent = parent->parentObject()) {
        if (shouldUpdateParent)
            parent->setNeedsToUpdateChildren();

        // If this object supports ARIA live regions, then notify AT of changes.
        // This notification needs to be sent even when the screen reader has not accessed this live region since the last update.
        // Sometimes this function can be called many times within a short period of time, leading to posting too many AXLiveRegionChanged notifications.
        // To fix this, we use a timer to make sure we only post one notification for the children changes within a pre-defined time interval.
        if (parent->supportsLiveRegion())
            postLiveRegionChangeNotification(*parent);

        // If this object is an ARIA text control, notify that its value changed.
        if (parent->isNonNativeTextControl()) {
            postNotification(parent.get(), parent->protectedDocument().get(), AXValueChanged);

            // Do not let any ancestor of an editable object update its children.
            shouldUpdateParent = false;
        }

        if (parent->isLabel()) {
            // A label's descendant was added or removed. Update its LabelFor relationships.
            handleLabelChanged(parent.get());
        }

        for (const auto& describedObject : parent->descriptionForObjects())
            postNotification(downcast<AccessibilityObject>(describedObject.get()), nullptr, AXExtendedDescriptionChanged);

        if (parent->hasTagName(captionTag))
            foundTableCaption = true;
        else if (foundTableCaption && parent->isTable()) {
            postNotification(parent.get(), nullptr, AXTextChanged);
            foundTableCaption = false;
        }
    }

    // The role of list objects is dependent on their children, so we'll need to re-compute it here.
    if (is<AccessibilityList>(object))
        object.updateRole();
}

void AXObjectCache::handleRecomputeCellSlots(AccessibilityTable& axTable)
{
    axTable.setCellSlotsDirty();
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(axTable, AXCellSlotsChanged);
#endif
}

void AXObjectCache::onRemoteFrameInitialized(AXRemoteFrame& remoteFrame)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(remoteFrame, AXPropertyName::RemoteFramePlatformElement);
#else
    UNUSED_PARAM(remoteFrame);
#endif
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::handleRowspanChanged(AccessibilityTableCell& axCell)
{
    updateIsolatedTree(axCell, AXRowSpanChanged);
}
#endif

#if ENABLE(AX_THREAD_TEXT_APIS)
void AXObjectCache::onTextRunsChanged(const RenderObject& renderer)
{
    postNotification(const_cast<RenderObject*>(&renderer), AXTextRunsChanged);
}
#endif

void AXObjectCache::handleMenuOpened(Element& element)
{
    if (!element.renderer() || !hasRole(element, "menu"_s))
        return;

    postNotification(getOrCreate(element), protectedDocument().ptr(), AXMenuOpened);
}

void AXObjectCache::handleLiveRegionCreated(Element& element)
{
    if (!element.renderer())
        return;

    auto liveRegionStatus = element.attributeWithoutSynchronization(aria_liveAttr);
    if (liveRegionStatus.isEmpty()) {
        const AtomString& ariaRole = element.attributeWithoutSynchronization(roleAttr);
        if (!ariaRole.isEmpty())
            liveRegionStatus = AtomString { AccessibilityObject::defaultLiveRegionStatusForRole(AccessibilityObject::ariaRoleToWebCoreRole(ariaRole)) };
    }

    if (AXCoreObject::liveRegionStatusIsEnabled(liveRegionStatus))
        postNotification(getOrCreate(element), protectedDocument().ptr(), AXLiveRegionCreated);
}

void AXObjectCache::deferElementAddedOrRemoved(Element* element)
{
    if (!element)
        return;

    m_deferredElementAddedOrRemovedList.add(*element);
    if (isModalElement(*element))
        deferModalChange(*element);

    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::deferAddUnconnectedNode(AccessibilityObject& axObject)
{
    m_deferredUnconnectedObjects.add(axObject);

    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}
#endif

void AXObjectCache::childrenChanged(Node* node, Element* changedChild)
{
    childrenChanged(get(node));
    deferElementAddedOrRemoved(changedChild);
}

void AXObjectCache::childrenChanged(RenderObject* renderer, RenderObject* changedChild)
{
    if (!renderer)
        return;

    childrenChanged(get(*renderer));
    if (changedChild)
        deferElementAddedOrRemoved(dynamicDowncast<Element>(changedChild->node()));
}

void AXObjectCache::childrenChanged(AccessibilityObject* object)
{
    if (!object)
        return;
    m_deferredChildrenChangedList.add(*object);

    // Adding or removing rows from a table can cause it to change from layout table to AX data table and vice versa, so queue up recomputation of that for the parent table.
    if (auto* tableElement = dynamicDowncast<HTMLTableElement>(object->element()))
        deferRecomputeTableIsExposed(const_cast<HTMLTableElement*>(tableElement));
    else if (auto* tableSectionElement = dynamicDowncast<HTMLTableSectionElement>(object->element()))
        deferRecomputeTableIsExposed(const_cast<HTMLTableElement*>(tableSectionElement->findParentTable().get()));

    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}

void AXObjectCache::valueChanged(Element& element)
{
    postNotification(&element, AXValueChanged);
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::columnIndexChanged(AccessibilityObject& object)
{
    postNotification(object, AXColumnIndexChanged);
}

void AXObjectCache::rowIndexChanged(AccessibilityObject& object)
{
    postNotification(object, AXRowIndexChanged);
}
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

void AXObjectCache::notificationPostTimerFired()
{
    AXTRACE(makeString("AXObjectCache::notificationPostTimerFired 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    // During LayoutTests, accessibility may be disabled between the time the notifications are queued and the timer fires.
    // Thus check here and return if accessibility is disabled.
    if (!accessibilityEnabled())
        return;

    Ref document = m_document.get();
    m_notificationPostTimer.stop();

    if (!document->hasLivingRenderTree())
        return;

    // In tests, posting notifications has a tendency to immediately queue up other notifications, which can lead to unexpected behavior
    // when the notification list is cleared at the end. Instead copy this list at the start.
    auto notifications = std::exchange(m_notificationsToPost, { });

    // Filter out the notifications that are not going to be posted to platform clients.
    Vector<std::pair<Ref<AccessibilityObject>, AXNotification>> notificationsToPost;
    notificationsToPost.reserveInitialCapacity(notifications.size());
    for (auto& note : notifications) {
        if (note.first->isDetached() || !note.first->axObjectCache())
            continue;

#if ASSERT_ENABLED
        // Make sure none of the render views are in the process of being layed out.
        // Notifications should only be sent after the renderer has finished
        if (auto* renderObject = dynamicDowncast<AccessibilityRenderObject>(note.first.get())) {
            if (auto* renderer = renderObject->renderer())
                ASSERT(!renderer->view().frameView().layoutContext().layoutState());
        }
#endif

        if (note.second == AXMenuOpened) {
            // Only notify if the object is in fact a menu.
            note.first->updateChildrenIfNecessary();
            if (note.first->roleValue() != AccessibilityRole::Menu)
                continue;
        }

        notificationsToPost.append(WTFMove(note));
    }

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(notificationsToPost);
#endif

    for (const auto& note : notificationsToPost)
        postPlatformNotification(note.first, note.second);
}

void AXObjectCache::passwordNotificationPostTimerFired()
{
#if PLATFORM(COCOA)
    m_passwordNotificationPostTimer.stop();

    // In tests, posting notifications has a tendency to immediately queue up other notifications, which can lead to unexpected behavior
    // when the notification list is cleared at the end. Instead copy this list at the start.
    auto notifications = std::exchange(m_passwordNotificationsToPost, { });

    for (auto& notification : notifications)
        postTextStateChangePlatformNotification(notification.ptr(), AXTextEditTypeInsert, " "_s, VisiblePosition());
#endif
}

void AXObjectCache::postNotification(RenderObject* renderer, AXNotification notification, PostTarget postTarget)
{
    if (!renderer)
        return;
    
    stopCachingComputedObjectAttributes();

    // Get an accessibility object that already exists. One should not be created here
    // because a render update may be in progress and creating an AX object can re-trigger a layout
    RefPtr<AccessibilityObject> object = get(*renderer);
    while (!object && renderer) {
        renderer = renderer->parent();
        object = get(renderer);
    }
    
    if (!renderer)
        return;
    
    postNotification(object.get(), renderer->protectedDocument().ptr(), notification, postTarget);
}

void AXObjectCache::postNotification(Node* node, AXNotification notification, PostTarget postTarget)
{
    if (!node)
        return;
    
    stopCachingComputedObjectAttributes();

    // Get an accessibility object that already exists. One should not be created here
    // because a render update may be in progress and creating an AX object can re-trigger a layout
    RefPtr<AccessibilityObject> object = get(*node);
    while (!object && node) {
        node = node->parentNode();
        object = get(node);
    }
    
    if (!node)
        return;
    
    postNotification(object.get(), node->protectedDocument().ptr(), notification, postTarget);
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

    m_notificationsToPost.append(std::make_pair(Ref { *object }, notification));
    if (!m_notificationPostTimer.isActive())
        m_notificationPostTimer.startOneShot(0_s);
}

void AXObjectCache::postNotification(AccessibilityObject& object, AXNotification notification)
{
    AXTRACE(makeString("AXObjectCache::postNotification 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    AXLOG(std::make_pair(Ref { object }, notification));
    ASSERT(isMainThread());

    stopCachingComputedObjectAttributes();

    m_notificationsToPost.append(std::make_pair(Ref { object }, notification));
    if (!m_notificationPostTimer.isActive())
        m_notificationPostTimer.startOneShot(0_s);
}

void AXObjectCache::checkedStateChanged(Element& element)
{
    postNotification(&element, AXCheckedStateChanged);
}

void AXObjectCache::autofillTypeChanged(HTMLInputElement& element)
{
    postNotification(&element, AXNotification::AXAutofillTypeChanged);
}

void AXObjectCache::handleMenuItemSelected(Element* element)
{
    if (!element)
        return;

    if (!hasAnyRole(*element, { "menuitem"_s, "menuitemradio"_s, "menuitemcheckbox"_s }))
        return;

    if (!element->focused() && !equalLettersIgnoringASCIICase(element->attributeWithoutSynchronization(aria_selectedAttr), "true"_s))
        return;

    postNotification(getOrCreate(*element), protectedDocument().ptr(), AXMenuListItemSelected);
}

// FIXME: Consider also handling updating SelectedChildren of TabLists (this should happen for the oldElement, newElement, and/or the parent of a tab)
void AXObjectCache::handleTabPanelSelected(Element* oldElement, Element* newElement)
{
    auto updateTab = [this] (AccessibilityObject* controlPanel, Element& element) {
        if (!controlPanel)
            return;

        auto controllers = controlPanel->controllers();
        for (auto& controller : controllers)
            postNotification(dynamicDowncast<AccessibilityObject>(controller.get()), element.protectedDocument().ptr(), AXSelectedStateChanged);
    };


    RefPtr oldObject = get(oldElement);
    RefPtr<AccessibilityObject> oldFocusedControlledPanel;
    if (oldObject) {
        oldFocusedControlledPanel = Accessibility::findAncestor<AccessibilityObject>(*oldObject, false, [] (auto& ancestor) {
            return ancestor.roleValue() == AccessibilityRole::TabPanel;
        });

        updateTab(oldFocusedControlledPanel.get(), *oldElement);
    }

    RefPtr newObject = get(newElement);
    if (!newObject)
        return;

    RefPtr newFocusedControlledPanel = Accessibility::findAncestor<AccessibilityObject>(*newObject, false, [] (auto& ancestor) {
        return ancestor.roleValue() == AccessibilityRole::TabPanel;
    });

    if (oldFocusedControlledPanel != newFocusedControlledPanel)
        updateTab(newFocusedControlledPanel.get(), *newElement);
}

void AXObjectCache::handleRowCountChanged(AccessibilityObject* axObject, Document* document)
{
    if (!axObject)
        return;

    if (auto* axTable = dynamicDowncast<AccessibilityTable>(axObject))
        axTable->recomputeIsExposable();

    postNotification(axObject, document, AXRowCountChanged);
}

void AXObjectCache::onPageActivityStateChange(OptionSet<ActivityState> newState)
{
    ASSERT(m_pageID);

    m_pageActivityState = newState;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (auto tree = AXIsolatedTree::treeForPageID(m_pageID))
        tree->setPageActivityState(newState);
#endif
}

static bool shouldDeferFocusChange(Element* element)
{
    if (!element)
        return false;

    auto* renderer = element->renderer();
    if (renderer && rendererNeedsDeferredUpdate(*renderer))
        return true;

    // We also want to defer handling focus changes for nodes that haven't yet attached their renderer.
    if (const auto* style = element->existingComputedStyle())
        return !renderer && element->rendererIsNeeded(*style);
    // No existing style, so we can't easily determine whether this element will need a renderer.
    // Resolving style is expensive and we don't want to do it now, so make this decision assuming
    // a renderer just hasn't been attached yet, indicated by it being nullptr.
    return !renderer;
}

void AXObjectCache::onFocusChange(Element* oldElement, Element* newElement)
{
    if (shouldDeferFocusChange(newElement)) {
        if (m_deferredFocusedNodeChange) {
            // If we got a focus change notification but haven't committed a previously deferred focus change:
            if (m_deferredFocusedNodeChange->first == newElement) {
                // Cancel the focus change entirely if the new focused node will be the same as the old one (i.e. there is no effective focus change).
                m_deferredFocusedNodeChange = std::nullopt;
                return;
            }
            // Otherwise, recompute is-ignored for the node that was slated to become the AX focused element.
            // This is important because we may have computed is-ignored for this node after it gained DOM focus,
            // meaning it could be unignored solely because it was DOM focused.
            recomputeIsIgnored(m_deferredFocusedNodeChange->second.get());
            // And now we can update the new deferred focus node to be |newNode|.
            m_deferredFocusedNodeChange->second = newElement;
        } else
            m_deferredFocusedNodeChange = { oldElement, newElement };

        // Don't start the timer if a layout is pending, as the layout will trigger a cache update.
        bool needsLayout = newElement->renderer() && newElement->renderer()->needsLayout();
        if (!needsLayout && !m_performCacheUpdateTimer.isActive())
            m_performCacheUpdateTimer.startOneShot(0_s);
    } else
        handleFocusedUIElementChanged(oldElement, newElement);
}

void AXObjectCache::onInertOrVisibilityChange(RenderElement& renderer)
{
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    RefPtr axObject = get(renderer);
    if (!axObject)
        return
    // Both of these change the is-ignored state of all descendants of `renderer`, so throw away
    // the is-ignored cache.
    stopCachingComputedObjectAttributes();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (RefPtr tree = AXIsolatedTree::treeForPageID(m_pageID))
        tree->updatePropertiesForSelfAndDescendants(*axObject, { AXPropertyName::IsIgnored });
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

#else // !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    childrenChanged(renderer.checkedParent().get(), &renderer);
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
}

void AXObjectCache::onPopoverToggle(const HTMLElement& popover)
{
    RefPtr axPopover = get(const_cast<HTMLElement*>(&popover));
    if (!axPopover)
        return;
    // There may be multiple elements with popovertarget attributes that point at |popover|.
    for (const auto& invoker : axPopover->controllers())
        postNotification(dynamicDowncast<AccessibilityObject>(invoker.get()), protectedDocument().ptr(), AXExpandedChanged);
}

void AXObjectCache::deferMenuListValueChange(Element* element)
{
    if (!element)
        return;

    m_deferredMenuListChange.add(*element);
    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}

void AXObjectCache::deferModalChange(Element& element)
{
    m_deferredModalChangedList.add(element);

    // Notify that parent's children have changed.
    if (auto* axParent = get(element.parentNode()))
        m_deferredChildrenChangedList.add(*axParent);

    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}

void AXObjectCache::handleFocusedUIElementChanged(Element* oldElement, Element* newElement, UpdateModal updateModal)
{
    if (updateModal == UpdateModal::Yes)
        updateCurrentModalNode();
    handleMenuItemSelected(newElement);

    // FIXME: Consider creating a new ancestor flag to only do this work when |oldNode| or |newNode| have a tab panel ancestor (the only time it is necessary)
    handleTabPanelSelected(oldElement, newElement);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    setIsolatedTreeFocusedObject(focusedObjectForNode(newElement));
#endif
    platformHandleFocusedUIElementChanged(oldElement, newElement);
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

void AXObjectCache::onScrollbarFrameRectChange(const Scrollbar& scrollbar)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!m_pageID || !isIsolatedTreeEnabled())
        return;

    if (auto* axScrollbar = get(const_cast<Scrollbar*>(&scrollbar)))
        m_geometryManager->cacheRect(axScrollbar->objectID(), enclosingIntRect(axScrollbar->relativeFrame()));
#else
    UNUSED_PARAM(scrollbar);
#endif
}

void AXObjectCache::onSelectedChanged(Element& element)
{
    if (hasCellARIARole(element))
        postNotification(&element, AXSelectedCellsChanged);
    else if (is<HTMLOptionElement>(element))
        postNotification(&element, AXSelectedStateChanged);
    else if (auto* axObject = getOrCreate(element)) {
        if (auto* ancestor = Accessibility::findAncestor<AccessibilityObject>(*axObject, false, [] (const auto& object) {
            return object.canHaveSelectedChildren();
        })) {
            selectedChildrenChanged(ancestor->node());
            postNotification(axObject, element.protectedDocument().ptr(), AXSelectedStateChanged);
        }
    }

    handleMenuItemSelected(&element);
    handleTabPanelSelected(nullptr, &element);
}

void AXObjectCache::onStyleChange(Element& element, Style::Change change, const RenderStyle* newStyle, const RenderStyle* oldStyle)
{
    if (change == Style::Change::None)
        return;

    if (!element.renderer() && oldStyle && newStyle && oldStyle->usedVisibility() != newStyle->usedVisibility()) {
        // We only need to do this when the given element doesn't have a renderer, as if it did, we would get a normal
        // children-changed event through the render tree.
        childrenChanged(&element);
    }
}

void AXObjectCache::onTextSecurityChanged(HTMLInputElement& inputElement)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    postNotification(get(&inputElement), AXTextSecurityChanged);
#else
    UNUSED_PARAM(inputElement);
#endif
}

void AXObjectCache::onTitleChange(Document& document)
{
    postNotification(get(&document), AXTextChanged);
}

void AXObjectCache::onValidityChange(Element& element)
{
    postNotification(get(&element), AXInvalidStatusChanged);
}

void AXObjectCache::onTextCompositionChange(Node& node, CompositionState compositionState, bool valueChanged, const String& text, size_t position, bool handlingAcceptedCandidate)
{
#if HAVE(INLINE_PREDICTIONS)
    auto* object = getOrCreate(node);
    if (!object)
        return;

#if PLATFORM(IOS_FAMILY)
    if (valueChanged)
        object->setLastPresentedTextPrediction(node, compositionState, text, position, handlingAcceptedCandidate);
#endif

    if (auto* observableObject = object->observableObject())
        object = observableObject;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(object, AXTextCompositionChanged);
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

    if (compositionState == CompositionState::Started)
        postNotification(object, node.protectedDocument().ptr(), AXTextCompositionBegan);

    if (valueChanged)
        postNotification(object, node.protectedDocument().ptr(), AXValueChanged);

    if (compositionState == CompositionState::Ended)
        postNotification(object, node.protectedDocument().ptr(), AXTextCompositionEnded);
#else
    UNUSED_PARAM(node);
    UNUSED_PARAM(compositionState);
    UNUSED_PARAM(valueChanged);
#endif // HAVE(INLINE_PREDICTIONS)

#if !PLATFORM(IOS_FAMILY) || !HAVE(INLINE_PREDICTIONS)
    UNUSED_PARAM(text);
    UNUSED_PARAM(position);
    UNUSED_PARAM(handlingAcceptedCandidate);
#endif
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
static bool isSecureFieldOrContainedBySecureField(AccessibilityObject& object)
{
    return object.isSecureField() || object.isContainedBySecureField();
}
#endif

void AXObjectCache::postTextStateChangeNotification(Node* node, const AXTextStateChangeIntent& intent, const VisibleSelection& selection)
{
    if (!node)
        return;

#if PLATFORM(COCOA) || USE(ATSPI)
    stopCachingComputedObjectAttributes();

    postTextStateChangeNotification(getOrCreate(*node), intent, selection);
#else
    postNotification(node->renderer(), AXSelectedTextChanged, PostTarget::ObservableParent);
    UNUSED_PARAM(intent);
    UNUSED_PARAM(selection);
#endif
}

void AXObjectCache::postTextStateChangeNotification(const Position& position, const AXTextStateChangeIntent& intent, const VisibleSelection& selection)
{
    auto node = position.protectedDeprecatedNode();
    if (!node)
        return;

    stopCachingComputedObjectAttributes();

#if PLATFORM(COCOA) || USE(ATSPI)
    AccessibilityObject* object = getOrCreate(*node);
    if (object && object->isIgnored()) {
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
    postTextStateChangeNotification(node.get(), intent, selection);
#endif
}

void AXObjectCache::postTextStateChangeNotification(AccessibilityObject* object, const AXTextStateChangeIntent& intent, const VisibleSelection& selection)
{
    AXTRACE(makeString("AXObjectCache::postTextStateChangeNotification 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    stopCachingComputedObjectAttributes();

#if PLATFORM(COCOA) || USE(ATSPI)
    if (object) {
#if PLATFORM(COCOA)
        if (isSecureFieldOrContainedBySecureField(*object))
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

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        m_lastDebouncedTextRangeObject = object->objectID();
        if (!m_selectedTextRangeTimer.isActive())
            m_selectedTextRangeTimer.restart();
#endif
    }
#if PLATFORM(MAC)
    onSelectedTextChanged(selection);
#endif
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
        if (enqueuePasswordValueChangeNotification(*object))
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

    AccessibilityObject* object = getOrCreate(*node);
#if PLATFORM(COCOA) || USE(ATSPI)
    if (object) {
        if (enqueuePasswordValueChangeNotification(*object))
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

    AccessibilityObject* object = getOrCreate(textControl);
#if PLATFORM(COCOA) || USE(ATSPI)
    if (object) {
        if (enqueuePasswordValueChangeNotification(*object))
            return;
        object = object->observableObject();
    }

    postTextReplacementPlatformNotificationForTextControl(object, deletedText, insertedText);
#else
    nodeTextChangePlatformNotification(object, textChangeForEditType(AXTextEditTypeDelete), 0, deletedText);
    nodeTextChangePlatformNotification(object, textChangeForEditType(AXTextEditTypeInsert), 0, insertedText);
#endif
}

bool AXObjectCache::enqueuePasswordValueChangeNotification(AccessibilityObject& object)
{
#if PLATFORM(COCOA)
    if (!isSecureFieldOrContainedBySecureField(object))
        return false;

    AccessibilityObject* observableObject = object.observableObject();
    if (!observableObject) {
        ASSERT_NOT_REACHED();
        // return true even though the enqueue didn't happen because this is a password field and caller shouldn't post a notification
        return true;
    }

    m_passwordNotificationsToPost.add(*observableObject);
    if (!m_passwordNotificationPostTimer.isActive())
        m_passwordNotificationPostTimer.startOneShot(accessibilityPasswordValueChangeNotificationInterval);

    return true;
#else
    UNUSED_PARAM(object);
    return false;
#endif
}

void AXObjectCache::frameLoadingEventNotification(LocalFrame* frame, AXLoadingEvent loadingEvent)
{
    if (!frame)
        return;

    // Delegate on the right platform
    frameLoadingEventPlatformNotification(getOrCreate(frame->contentRenderer()), loadingEvent);
}

void AXObjectCache::postLiveRegionChangeNotification(AccessibilityObject& object)
{
    if (m_liveRegionChangedPostTimer.isActive())
        m_liveRegionChangedPostTimer.stop();

    Ref objectRef = object;
    if (!m_liveRegionObjects.contains(objectRef))
        m_liveRegionObjects.add(WTFMove(objectRef));

    m_liveRegionChangedPostTimer.startOneShot(0_s);
}

void AXObjectCache::liveRegionChangedNotificationPostTimerFired()
{
    m_liveRegionChangedPostTimer.stop();

    if (m_liveRegionObjects.isEmpty())
        return;

    for (auto& object : m_liveRegionObjects)
        postNotification(object.ptr(), object->protectedDocument().get(), AXLiveRegionChanged);
    m_liveRegionObjects.clear();
}

static AccessibilityObject* firstFocusableChild(AccessibilityObject& object)
{
    for (auto& child : AXChildIterator(object)) {
        if (child.canSetFocusAttribute())
            return &child;
        if (auto* focusableDescendant = firstFocusableChild(child))
            return focusableDescendant;
    }
    return nullptr;
}

void AXObjectCache::focusCurrentModal()
{
    Ref document = m_document.get();
    if (!document->hasLivingRenderTree())
        return;

    if (!nodeAndRendererAreValid(m_currentModalElement.get()) || !isNodeVisible(m_currentModalElement.get()))
        return;

    // Don't focus the current modal if focus has been requested to be put elsewhere (e.g. via JS).
    if (m_deferredFocusedNodeChange)
        return;
    
    // Don't set focus if we are already focusing onto some element within
    // the dialog.
    if (m_currentModalElement->contains(document->focusedElement()))
        return;

    if (auto* currentModalNodeObject = getOrCreate(*m_currentModalElement)) {
        if (auto* focusable = firstFocusableChild(*currentModalNodeObject))
            focusable->setFocused(true);
    }
}

void AXObjectCache::onScrollbarUpdate(ScrollView& view)
{
    m_deferredScrollbarUpdateChangeList.add(view);
    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}

void AXObjectCache::handleScrollbarUpdate(ScrollView& view)
{
    // We don't want to create a scroll view from this method, only update an existing one.
    if (auto* scrollViewObject = get(&view)) {
        stopCachingComputedObjectAttributes();
        scrollViewObject->updateChildrenIfNecessary();
    }
}

void AXObjectCache::handleAriaExpandedChange(Element& element)
{
    // An aria-expanded change can cause two notifications to be posted:
    // RowCountChanged for the tree or table ancestor of this object, and
    // RowExpanded/Collapsed for this object.
    if (RefPtr object = get(element)) {
        // Find the ancestor that supports RowCountChanged if exists.
        auto* ancestor = Accessibility::findAncestor<AccessibilityObject>(*object, false, [] (auto& candidate) {
            return candidate.supportsRowCountChange();
        });

        // Post that the ancestor's row count changed.
        if (ancestor)
            handleRowCountChanged(ancestor, protectedDocument().ptr());

        // Post that the specific row either collapsed or expanded.
        auto role = object->roleValue();
        if (role == AccessibilityRole::Row || role == AccessibilityRole::TreeItem)
            postNotification(object.get(), protectedDocument().ptr(), object->isExpanded() ? AXRowExpanded : AXRowCollapsed);
        else
            postNotification(object.get(), protectedDocument().ptr(), AXExpandedChanged);
    }
}

void AXObjectCache::handleActiveDescendantChange(Element& element, const AtomString& oldValue, const AtomString& newValue)
{
    AXTRACE("AXObjectCache::handleActiveDescendantChange"_s);

    // Use the element's document instead of the cache's document in case we're inside a frame that's managing focus.
    if (!element.document().frame()->selection().isFocusedAndActive())
        return;

    RefPtr object = getOrCreate(element);
    if (!object)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(*object, AXNotification::AXActiveDescendantChanged);
#endif

    // Notify active descendant changes only for the focused element.
    if (element.document().focusedElement() != &element)
        return;

    RefPtr activeDescendant = dynamicDowncast<AccessibilityObject>(object->activeDescendant());
    if (!activeDescendant) {
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        if (object->shouldFocusActiveDescendant()
            && !oldValue.isEmpty() && newValue.isEmpty()) {
            // The focused object just lost its active descendant, so set the IsolatedTree focused object back to it.
            setIsolatedTreeFocusedObject(object.get());
        }
#else
        UNUSED_PARAM(oldValue);
        UNUSED_PARAM(newValue);
#endif
        return;
    }

    if (object->shouldFocusActiveDescendant()) {
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        setIsolatedTreeFocusedObject(activeDescendant.get());
#endif
        postPlatformNotification(*activeDescendant, AXNotification::AXFocusedUIElementChanged);
    }

    // Handle active-descendant changes when the target allows for it, or the controlled object allows for it.
    RefPtr<AccessibilityObject> target;
    if (object->supportsActiveDescendant())
        target = object;
    else {
        // Check to see if the active descendant is a descendant of an object controlled by this object.
        // In that case, the controlled object will be the target for the notification.
        auto controlledObjects = object->relatedObjects(AXRelationType::ControllerFor);
        if (controlledObjects.size()) {
            target = Accessibility::findAncestor(*activeDescendant, false, [&controlledObjects] (const auto& activeDescendantAncestor) {
                return controlledObjects.contains(&activeDescendantAncestor);
            });
        }
    }

    if (!target)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (target != object)
        updateIsolatedTree(target.get(), AXNotification::AXActiveDescendantChanged);
#endif

    postPlatformNotification(*target, AXNotification::AXActiveDescendantChanged);

    // Table cell active descendant changes should trigger selected cell changes.
    if (target->isTable() && activeDescendant->isExposedTableCell())
        postPlatformNotification(*target, AXSelectedCellsChanged);
}

static bool isTableOrRowRole(const AtomString& attrValue)
{
    return attrValue == "table"_s
        || attrValue == "grid"_s
        || attrValue == "treegrid"_s
        || attrValue == "row"_s;
}

void AXObjectCache::handleRoleChanged(Element& element, const AtomString& oldValue, const AtomString& newValue)
{
    AXTRACE("AXObjectCache::handleRoleChanged"_s);
    AXLOG(makeString("oldValue "_s, oldValue, " new value "_s, newValue));
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
            remove(element);
            childrenChanged(parent);
            return;
        }
    }

    object->updateRole();
}

void AXObjectCache::handleRoleChanged(AccessibilityObject& axObject)
{
    stopCachingComputedObjectAttributes();
    axObject.recomputeIsIgnored();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    postNotification(axObject, AXRoleChanged);
#endif
}

void AXObjectCache::handleRoleDescriptionChanged(Element& element)
{
    auto* object = get(element);
    if (!object)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(object, AXNotification::AXRoleDescriptionChanged);
#endif
}

void AXObjectCache::deferAttributeChangeIfNeeded(Element& element, const QualifiedName& attrName, const AtomString& oldValue, const AtomString& newValue)
{
    AXTRACE(makeString("AXObjectCache::deferAttributeChangeIfNeeded 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));

    if (nodeRendererIsValid(element) && rendererNeedsDeferredUpdate(*element.renderer())) {
        m_deferredAttributeChange.append({ element, attrName, oldValue, newValue });
        if (!m_performCacheUpdateTimer.isActive())
            m_performCacheUpdateTimer.startOneShot(0_s);
        AXLOG(makeString("Deferring handling of attribute "_s, attrName.localName().string(), " for element "_s, element.debugDescription()));
        return;
    }
    Ref protectedElement { element };
    handleAttributeChange(protectedElement.ptr(), attrName, oldValue, newValue);
    if (attrName == idAttr)
        relationsNeedUpdate(true);
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
    return get(*element) || get(element->parentNode());
}

void AXObjectCache::handleAttributeChange(Element* element, const QualifiedName& attrName, const AtomString& oldValue, const AtomString& newValue)
{
    AXTRACE(makeString("AXObjectCache::handleAttributeChange 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    AXLOG(makeString("attribute "_s, attrName.localName(), " for element "_s, element ? element->debugDescription() : "nullptr"_str));
    AXLOG(makeString("old value: "_s, oldValue, " new value: "_s, newValue));

    enum class TableProperty : uint8_t { Exposed = 1 << 0, CellSlots = 1 << 1 };
    auto recomputeParentTableProperties = [this] (Element* element, OptionSet<TableProperty> properties) {
        ASSERT(!properties.isEmpty());

        if (!properties.contains(TableProperty::CellSlots)) {
            // If we're re-computing the exposed state of the table, we only need to do work for non-ARIA tables, allowing us to
            // do a cheap dynamicDowncast check for an HTMLTablePartElement rather than calling AccessibilityTableCell::parentTable().
            // (ARIA tables are inherently always exposed).
            if (auto* tablePartElement = dynamicDowncast<HTMLTablePartElement>(element))
                deferRecomputeTableIsExposed(const_cast<HTMLTableElement*>(tablePartElement->findParentTable().get()));
        } else if (auto* axCell = dynamicDowncast<AccessibilityTableCell>(getOrCreate(element))) {
            if (auto* parentTable = axCell->parentTable()) {
                if (properties.contains(TableProperty::Exposed) && !parentTable->isAriaTable())
                    deferRecomputeTableIsExposed(parentTable->element());
                if (properties.contains(TableProperty::CellSlots))
                    deferRecomputeTableCellSlots(*parentTable);
            }
        }
    };

    if (!shouldProcessAttributeChange(element, attrName))
        return;
    // The remaining code in this method relies on shouldProcessAttributeChange null-checking element.
    ASSERT(element);

    if (relationAttributes().contains(attrName))
        updateRelations(*element, attrName);

    if (attrName == roleAttr)
        handleRoleChanged(*element, oldValue, newValue);
    else if (attrName == altAttr || attrName == titleAttr)
        handleTextChanged(getOrCreate(*element));
    else if (attrName == contenteditableAttr) {
        if (auto* axObject = get(*element))
            axObject->updateRole();
    }
    else if (attrName == disabledAttr)
        postNotification(element, AXDisabledStateChanged);
    else if (attrName == forAttr) {
        if (RefPtr label = dynamicDowncast<HTMLLabelElement>(element)) {
            updateLabelFor(*label);

            if (RefPtr oldControl = element->treeScope().getElementById(oldValue))
                postNotification(oldControl.get(), AXTextChanged);
            if (RefPtr newControl = element->treeScope().getElementById(newValue))
                postNotification(newControl.get(), AXTextChanged);
        }
    } else if (attrName == requiredAttr)
        postNotification(element, AXRequiredStatusChanged);
    else if (attrName == tabindexAttr) {
        if (oldValue.isEmpty() || newValue.isEmpty()) {
            childrenChanged(element->parentNode(), element);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
            postNotification(element, AXFocusableStateChanged);
#endif
        }
    }
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    else if (attrName == langAttr)
        updateIsolatedTree(get(*element), AXLanguageChanged);
    else if (attrName == nameAttr)
        postNotification(get(*element), AXNameChanged);
    else if (attrName == placeholderAttr)
        postNotification(element, AXPlaceholderChanged);
    else if (attrName == hrefAttr || attrName == srcAttr)
        postNotification(element, AXURLChanged);
    else if (attrName == idAttr) {
#if !LOG_DISABLED
        updateIsolatedTree(get(*element), AXIdAttributeChanged);
#endif
    } else if (attrName == accesskeyAttr)
        updateIsolatedTree(get(*element), AXAccessKeyChanged);
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    else if (attrName == openAttr && is<HTMLDialogElement>(*element)) {
        deferModalChange(*element);
        recomputeIsIgnored(element->parentNode());
    } else if (attrName == rowspanAttr) {
        deferRowspanChange(get(*element));
        recomputeParentTableProperties(element, TableProperty::CellSlots);
    } else if (attrName == colspanAttr) {
        postNotification(element, AXColumnSpanChanged);
        recomputeParentTableProperties(element, TableProperty::CellSlots);
    } else if (attrName == popovertargetAttr)
        postNotification(element, AXPopoverTargetChanged);
    else if (attrName == scopeAttr)
        postNotification(element, AXCellScopeChanged);

    if (!attrName.localName().string().startsWith("aria-"_s))
        return;

    if (attrName == aria_activedescendantAttr)
        handleActiveDescendantChange(*element, oldValue, newValue);
    else if (attrName == aria_atomicAttr)
        postNotification(element, AXIsAtomicChanged);
    else if (attrName == aria_busyAttr)
        postNotification(element, AXElementBusyChanged);
    else if (attrName == aria_controlsAttr)
        postNotification(element, AXControlledObjectsChanged);
    else if (attrName == aria_valuenowAttr || attrName == aria_valuetextAttr)
        postNotification(element, AXValueChanged);
    else if (attrName == aria_labelAttr && element->hasTagName(htmlTag)) {
        // When aria-label changes on an <html> element, it's the web area who needs to re-compute its accessibility text.
        handleTextChanged(get(element->protectedDocument().ptr()));
    } else if (attrName == aria_labelAttr || attrName == aria_labeledbyAttr || attrName == aria_labelledbyAttr) {
        RefPtr axObject = get(*element);
        if (!axObject)
            return;

        if (hasAnyRole(*element, { "form"_s, "region"_s })) {
            // https://w3c.github.io/aria/#document-handling_author-errors_roles
            // The computed role of ARIA forms and regions is dependent on whether they have a label.
            if (oldValue.isEmpty() || newValue.isEmpty())
                axObject->updateRole();
        }
        handleTextChanged(axObject.get());
    }
    else if (attrName == aria_checkedAttr)
        checkedStateChanged(*element);
    else if (attrName == aria_colcountAttr) {
        postNotification(element, AXColumnCountChanged);
        deferRecomputeTableIsExposed(dynamicDowncast<HTMLTableElement>(element));
    } else if (attrName == aria_colindexAttr) {
        postNotification(element, AXARIAColumnIndexChanged);
        recomputeParentTableProperties(element, TableProperty::Exposed);
    } else if (attrName == aria_colspanAttr) {
        postNotification(element, AXColumnSpanChanged);
        recomputeParentTableProperties(element, { TableProperty::CellSlots, TableProperty::Exposed });
    }
    else if (attrName == aria_describedbyAttr)
        postNotification(element, AXDescribedByChanged);
    else if (attrName == aria_descriptionAttr)
        postNotification(element, AXExtendedDescriptionChanged);
    else if (attrName == aria_dropeffectAttr)
        postNotification(element, AXDropEffectChanged);
    else if (attrName == aria_flowtoAttr)
        postNotification(element, AXFlowToChanged);
    else if (attrName == aria_grabbedAttr)
        postNotification(element, AXGrabbedStateChanged);
    else if (attrName == aria_keyshortcutsAttr)
        postNotification(element, AXKeyShortcutsChanged);
    else if (attrName == aria_levelAttr)
        postNotification(element, AXLevelChanged);
    else if (attrName == aria_liveAttr)
        postNotification(element, AXLiveRegionStatusChanged);
    else if (attrName == aria_placeholderAttr)
        postNotification(element, AXPlaceholderChanged);
    else if (attrName == aria_rowindexAttr) {
        postNotification(element, AXARIARowIndexChanged);
        recomputeParentTableProperties(element, { TableProperty::CellSlots, TableProperty::Exposed });
    }
    else if (attrName == aria_valuemaxAttr)
        postNotification(element, AXMaximumValueChanged);
    else if (attrName == aria_valueminAttr)
        postNotification(element, AXMinimumValueChanged);
    else if (attrName == aria_multilineAttr) {
        if (auto* axObject = get(*element)) {
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
        onSelectedChanged(*element);
    else if (attrName == aria_setsizeAttr)
        postNotification(element, AXSetSizeChanged);
    else if (attrName == aria_expandedAttr)
        handleAriaExpandedChange(*element);
    else if (attrName == aria_haspopupAttr)
        postNotification(element, AXHasPopupChanged);
    else if (attrName == aria_hiddenAttr) {
        if (RefPtr parent = get(element->parentNode()))
            childrenChanged(parent.get());

        if (m_currentModalElement && m_currentModalElement->isDescendantOf(element)) {
            m_modalNodesInitialized = false;
            deferModalChange(*m_currentModalElement);
        }
    }
    else if (attrName == aria_invalidAttr)
        postNotification(element, AXInvalidStatusChanged);
    else if (attrName == aria_modalAttr) {
        // aria-modal changed, so the element may have become modal or un-modal.
        if (isModalElement(*element))
            m_modalElements.appendIfNotContains(element);
        else
            m_modalElements.removeAll(element);
        deferModalChange(*element);
    }
    else if (attrName == aria_currentAttr)
        postNotification(element, AXCurrentStateChanged);
    else if (attrName == aria_disabledAttr)
        postNotification(element, AXDisabledStateChanged);
    else if (attrName == aria_pressedAttr)
        postNotification(element, AXPressedStateChanged);
    else if (attrName == aria_readonlyAttr)
        postNotification(element, AXReadOnlyStatusChanged);
    else if (attrName == aria_requiredAttr)
        postNotification(element, AXRequiredStatusChanged);
    else if (attrName == aria_roledescriptionAttr)
        handleRoleDescriptionChanged(*element);
    else if (attrName == aria_rowcountAttr)
        handleRowCountChanged(get(*element), element->protectedDocument().ptr());
    else if (attrName == aria_rowspanAttr) {
        deferRowspanChange(get(*element));
        recomputeParentTableProperties(element, { TableProperty::CellSlots, TableProperty::Exposed });
    } else if (attrName == aria_sortAttr)
        postNotification(element, AXSortDirectionChanged);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    else if (attrName == aria_ownsAttr) {
        if (oldValue.isEmpty() || newValue.isEmpty())
            updateIsolatedTree(get(*element), AXPropertyName::SupportsARIAOwns);
    } else if (attrName == aria_braillelabelAttr)
        postNotification(element, AXBrailleLabelChanged);
    else if (attrName == aria_brailleroledescriptionAttr)
        postNotification(element, AXBrailleRoleDescriptionChanged);
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
}

void AXObjectCache::handleLabelChanged(AccessibilityObject* object)
{
    AXTRACE("AXObjectCache::handleLabelChanged"_s);

    if (!object)
        return;

    if (RefPtr label = dynamicDowncast<HTMLLabelElement>(object->element()))
        updateLabelFor(*label);
    else {
        auto labeledObjects = object->labelForObjects();
        for (auto& labeledObject : labeledObjects) {
            updateLabeledBy(RefPtr { labeledObject->element() }.get());
            postNotification(downcast<AccessibilityObject>(labeledObject.get()), protectedDocument().ptr(), AXValueChanged);
        }
    }

    postNotification(object, protectedDocument().ptr(), AXLabelChanged);
}

void AXObjectCache::updateLabelFor(HTMLLabelElement& label)
{
    removeRelation(label, AXRelationType::LabelFor);
    addLabelForRelation(label);
}

void AXObjectCache::updateLabeledBy(Element* element)
{
    if (!element)
        return;

    bool changedRelation = removeRelation(*element, AXRelationType::LabeledBy);
    changedRelation |= addRelation(*element, aria_labelledbyAttr);
    if (changedRelation)
        dirtyIsolatedTreeRelations();
}

void AXObjectCache::dirtyIsolatedTreeRelations()
{
    AXTRACE("AXObjectCache::dirtyIsolatedTreeRelations"_s);

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (auto tree = AXIsolatedTree::treeForPageID(m_pageID))
        tree->relationsNeedUpdate(true);
    startUpdateTreeSnapshotTimer();
#endif
}

void AXObjectCache::recomputeIsIgnored(RenderObject& renderer)
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

Ref<Document> AXObjectCache::protectedDocument() const
{
    return document();
}

VisiblePosition AXObjectCache::visiblePositionForTextMarkerData(const TextMarkerData& textMarkerData)
{
    RefPtr node = nodeForID(textMarkerData.axObjectID());
    if (!node)
        return { };

    if (node->isPseudoElement())
        return { };

    auto visiblePosition = VisiblePosition({ node.get(), textMarkerData.offset, textMarkerData.anchorType }, textMarkerData.affinity);
    auto deepPosition = visiblePosition.deepEquivalent();
    if (deepPosition.isNull())
        return { };

    auto* renderer = deepPosition.deprecatedNode()->renderer();
    if (!renderer)
        return { };

    auto* cache = renderer->document().axObjectCache();
    if (cache && !cache->m_idsInUse.contains(*textMarkerData.axObjectID()))
        return { };

    return visiblePosition;
}

CharacterOffset AXObjectCache::characterOffsetForTextMarkerData(const TextMarkerData& textMarkerData)
{
    if (textMarkerData.ignored)
        return { };

    RefPtr node = nodeForID(textMarkerData.axObjectID());
    if (!node)
        return { };

    CharacterOffset result(node.get(), textMarkerData.characterStart, textMarkerData.characterOffset);
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
    
    // Enable the cache here for isIgnored calls in replacedNodeNeedsCharacter.
    AXAttributeCacheEnabler enableCache(this);

    // When the range has zero length, there might be replaced node or brTag that we need to increment the characterOffset.
    if (iterator.atEnd()) {
        currentNode = range.start.container.ptr();
        lastStartOffset = range.start.offset;
        if (offset > 0 || toNodeEnd) {
            if (AccessibilityObject::replacedNodeNeedsCharacter(*currentNode) || (currentNode->renderer() && currentNode->renderer()->isBR()))
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
            if (AccessibilityObject::replacedNodeNeedsCharacter(*childNode)) {
                cumulativeOffset++;
                currentLength++;
                currentNode = childNode;
                hasReplacedNodeOrBR = true;
            } else
                continue;
        } else {
            // Ignore space, new line, tag node.
            if (currentLength == 1) {
                if (isASCIIWhitespace(iterator.text()[0])) {
                    // If the node has BR tag, we want to set the currentNode to it.
                    Node* childNode = iterator.node();
                    if (childNode && childNode->renderer() && childNode->renderer()->isBR()) {
                        currentNode = childNode;
                        hasReplacedNodeOrBR = true;
                    } else if (auto* shadowHost = currentNode->shadowHost()) {
                        // Since we are entering text controls, we should set the currentNode
                        // to be the shadow host when there's no content.
                        if (elementIsTextControl(*shadowHost) && currentNode->isShadowRoot()) {
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

unsigned AXObjectCache::lengthForRange(const SimpleRange& range)
{
    unsigned length = 0;
    for (TextIterator it(range); !it.atEnd(); it.advance()) {
        // Non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX).
        if (it.text().length())
            length += it.text().length();
        else {
            if (AccessibilityObject::replacedNodeNeedsCharacter(*it.node()))
                ++length;
        }
    }
    return length;
}

SimpleRange AXObjectCache::rangeForNodeContents(Node& node)
{
    if (AccessibilityObject::replacedNodeNeedsCharacter(node)) {
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

static bool isReplacedNodeOrBR(Node& node)
{
    return AccessibilityObject::replacedNodeNeedsCharacter(node) || node.hasTagName(brTag);
}

static bool characterOffsetsInOrder(const CharacterOffset& characterOffset1, const CharacterOffset& characterOffset2)
{
    // FIXME: Should just be able to call treeOrder without accessibility-specific logic.
    // FIXME: Not clear why CharacterOffset needs to exist at all; we have both Position and BoundaryPoint to choose from.

    if (characterOffset1.isNull() || characterOffset2.isNull())
        return false;
    
    if (characterOffset1.node == characterOffset2.node)
        return characterOffset1.offset <= characterOffset2.offset;
    
    RefPtr node1 = characterOffset1.node;
    RefPtr node2 = characterOffset2.node;
    if (!node1->isCharacterDataNode() && !isReplacedNodeOrBR(*node1) && node1->hasChildNodes())
        node1 = node1->traverseToChildAt(characterOffset1.offset);
    if (!node2->isCharacterDataNode() && !isReplacedNodeOrBR(*node2) && node2->hasChildNodes())
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

static std::optional<BoundaryPoint> boundaryPoint(const CharacterOffset& characterOffset)
{
    if (characterOffset.isNull())
        return std::nullopt;

    int offset = characterOffset.startIndex + characterOffset.offset;
    // Guaranteed to be non-null by checking CharacterOffset::isNull.
    RefPtr node = characterOffset.node;
    if (isReplacedNodeOrBR(*node))
        node = resetNodeAndOffsetForReplacedNode(*node, offset, characterOffset.offset);

    if (!node)
        return std::nullopt;
    return { { *node, static_cast<unsigned>(offset) } };
}

static bool setRangeStartOrEndWithCharacterOffset(SimpleRange& range, const CharacterOffset& characterOffset, bool isStart)
{
    auto point = boundaryPoint(characterOffset);
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
    auto start = boundaryPoint(alreadyInOrder ? characterOffset1 : characterOffset2);
    auto end = boundaryPoint(alreadyInOrder ? characterOffset2 : characterOffset1);
    if (!start || !end)
        return std::nullopt;
    return { { *start, *end } };
}

TextMarkerData AXObjectCache::textMarkerDataForCharacterOffset(const CharacterOffset& characterOffset)
{
    if (characterOffset.isNull())
        return { };

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(characterOffset.node.get()); input && input->isSecureField())
        return { *this, { }, true };

    return { *this, characterOffset, false };
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

TextMarkerData AXObjectCache::startOrEndTextMarkerDataForRange(const SimpleRange& range, bool isStart)
{
    auto characterOffset = startOrEndCharacterOffsetForRange(range, isStart);
    if (characterOffset.isNull())
        return { };
    return textMarkerDataForCharacterOffset(characterOffset);
}

CharacterOffset AXObjectCache::characterOffsetForNodeAndOffset(Node& node, int offset, TraverseOption option)
{
    Node* domNode = &node;
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
            charOffset = characterOffsetForNodeAndOffset(Ref { *charOffset.node }, charOffset.offset - offset, TraverseOptionIncludeStart);
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

TextMarkerData AXObjectCache::textMarkerDataForNextCharacterOffset(const CharacterOffset& characterOffset)
{
    if (characterOffset.isNull())
        return { };

    TextMarkerData data;
    auto next = characterOffset;
    auto previous = characterOffset;
    bool shouldContinue;
    do {
        shouldContinue = false;
        next = nextCharacterOffset(next, false);
        if (next.isNull())
            return { };

        if (shouldSkipBoundary(previous, next))
            next = nextCharacterOffset(next, false);
        if (next.isNull() || next.isEqual(previous))
            return { };

        data = textMarkerDataForCharacterOffset(next);

        // We should skip next CharacterOffset if it's visually the same.
        auto range = rangeForUnorderedCharacterOffsets(previous, next);
        if (!range || !lengthForRange(*range))
            shouldContinue = true;
        previous = next;
    } while (data.ignored || shouldContinue);
    return data;
}

AXTextMarker AXObjectCache::nextTextMarker(const AXTextMarker& marker)
{
    ASSERT(m_id == marker.treeID());
    return textMarkerDataForNextCharacterOffset(marker);
}

TextMarkerData AXObjectCache::textMarkerDataForPreviousCharacterOffset(const CharacterOffset& characterOffset)
{
    if (characterOffset.isNull())
        return { };

    TextMarkerData data;
    auto previous = characterOffset;
    auto next = characterOffset;
    bool shouldContinue;
    do {
        shouldContinue = false;
        previous = previousCharacterOffset(previous, false);
        if (previous.isNull() || previous.isEqual(next))
            return { };

        data = textMarkerDataForCharacterOffset(previous);

        // We should skip previous CharacterOffset if it's visually the same.
        auto range = rangeForUnorderedCharacterOffsets(previous, next);
        if (!range || !lengthForRange(*range))
            shouldContinue = true;
        next = previous;
    } while (data.ignored || shouldContinue);
    return data;
}

AXTextMarker AXObjectCache::previousTextMarker(const AXTextMarker& marker)
{
    ASSERT(m_id == marker.treeID());
    return textMarkerDataForPreviousCharacterOffset(marker);
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

CharacterOffset AXObjectCache::characterOffsetFromVisiblePosition(const VisiblePosition& targetVisiblePosition)
{
    if (targetVisiblePosition.isNull())
        return { };

    Position targetDeepPosition = targetVisiblePosition.deepEquivalent();
    // Dereferencing deprecatedNode is safe because VisiblePosition::isNull returns true if the deepEquivalent position has a null node.
    Ref targetNode = *targetDeepPosition.deprecatedNode();
    if (targetNode->isCharacterDataNode())
        return traverseToOffsetInRange(rangeForNodeContents(targetNode.get()), targetDeepPosition.deprecatedEditingOffset(), TraverseOptionValidateOffset);

    RefPtr object = getOrCreate(targetNode.get());
    if (!object)
        return { };

    // Use nextVisiblePosition to calculate how many characters we need to traverse to the current position.
    auto visiblePosition = object->visiblePositionRange().start;
    int characterOffset = 0;
    auto currentPosition = visiblePosition.deepEquivalent();
    auto startPosition = currentPosition;

    while (!currentPosition.isNull() && !targetDeepPosition.equals(currentPosition)) {
        auto previousPosition = visiblePosition.deepEquivalent();
        visiblePosition = VisiblePosition(nextVisuallyDistinctCandidate(visiblePosition.deepEquivalent(), SkipDisplayContents::No), visiblePosition.affinity());
        currentPosition = visiblePosition.deepEquivalent();
        // Sometimes nextVisiblePosition will give the same VisiblePostion,
        // we break here to avoid infinite loop.
        if (currentPosition.equals(previousPosition))
            break;
        // FIXME: Due to a foundational editing bug, it's possible to end up back at the start position, causing
        // an infinite loop. This break condition should be removed after this bug is fixed (tracked by https://bugs.webkit.org/show_bug.cgi?id=276460).
        if (startPosition.equals(currentPosition))
            break;
        characterOffset++;

        // When VisiblePostion moves to next node, it will count the leading line break as
        // 1 offset, which we shouldn't include in CharacterOffset.
        if (currentPosition.deprecatedNode() != previousPosition.deprecatedNode()) {
            if (visiblePosition.characterBefore() == '\n')
                characterOffset--;
        } else if (currentPosition.deprecatedNode()->isCharacterDataNode()) {
            // Sometimes VisiblePosition will move multiple characters, like emoji.
            characterOffset += currentPosition.offsetInContainerNode() - previousPosition.offsetInContainerNode() - 1;
        }
    }
    
    // Sometimes when the node is a replaced node and is ignored in accessibility, we get a wrong CharacterOffset from it.
    CharacterOffset result = traverseToOffsetInRange(rangeForNodeContents(targetNode.get()), characterOffset);
    if (result.remainingOffset > 0 && !result.isNull() && isRendererReplacedElement(result.node->renderer()))
        result.offset += result.remainingOffset;
    return result;
}

AccessibilityObject* AXObjectCache::objectForTextMarkerData(const TextMarkerData& textMarkerData)
{
    if (textMarkerData.ignored)
        return nullptr;

    RefPtr object = m_objects.get(*textMarkerData.axObjectID());
    if (!object)
        return nullptr;

    Node* node = object->node();
    if (!node)
        return nullptr;
    ASSERT(object.get() == getOrCreate(*node));
    return getOrCreate(*node);
}

std::optional<TextMarkerData> AXObjectCache::textMarkerDataForVisiblePosition(const VisiblePosition& visiblePosition)
{
    if (visiblePosition.isNull())
        return std::nullopt;

    Position position = visiblePosition.deepEquivalent();
    RefPtr node = position.anchorNode();
    ASSERT(node);
    if (!node)
        return std::nullopt;

    if (auto* input = dynamicDowncast<HTMLInputElement>(node.get()); input && input->isSecureField())
        return std::nullopt;

    // If the visible position has an anchor type referring to a node other than the anchored node, we should
    // set the text marker data with CharacterOffset so that the offset will correspond to the node.
    auto characterOffset = characterOffsetFromVisiblePosition(visiblePosition);
    if (position.anchorType() == Position::PositionIsAfterAnchor || position.anchorType() == Position::PositionIsAfterChildren)
        return textMarkerDataForCharacterOffset(characterOffset);

    CheckedPtr cache = node->document().axObjectCache();
    if (!cache)
        return std::nullopt;
    return { { *cache, visiblePosition,
        characterOffset.startIndex, characterOffset.offset, false } };
}

CharacterOffset AXObjectCache::nextCharacterOffset(const CharacterOffset& characterOffset, bool ignoreNextNodeStart)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    RefPtr node = characterOffset.node;
    // We don't always move one 'character' at a time since there might be composed characters.
    unsigned nextOffset = Position::uncheckedNextOffset(node.get(), characterOffset.offset);
    CharacterOffset next = characterOffsetForNodeAndOffset(*node, nextOffset);
    
    // To be consistent with VisiblePosition, we should consider the case that current node end to next node start counts 1 offset.
    RefPtr nextNode = next.node;
    if (!ignoreNextNodeStart && !next.isNull() && !isReplacedNodeOrBR(*nextNode) && nextNode != node) {
        if (auto range = rangeForUnorderedCharacterOffsets(characterOffset, next)) {
            auto length = characterCount(*range);
            if (length > nextOffset - characterOffset.offset)
                next = characterOffsetForNodeAndOffset(*nextNode, 0, TraverseOptionIncludeStart);
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
        return characterOffsetForNodeAndOffset(Ref { *characterOffset.node }, 0);
    
    // We don't always move one 'character' a time since there might be composed characters.
    RefPtr characterOffsetNode = characterOffset.node;
    int previousOffset = Position::uncheckedPreviousOffset(characterOffsetNode.get(), characterOffset.offset);
    return characterOffsetForNodeAndOffset(*characterOffsetNode, previousOffset, TraverseOptionIncludeStart);
}

CharacterOffset AXObjectCache::startCharacterOffsetOfWord(const CharacterOffset& characterOffset, WordSide side)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    CharacterOffset c = characterOffset;
    if (side == WordSide::RightWordIfOnBoundary) {
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

CharacterOffset AXObjectCache::endCharacterOffsetOfWord(const CharacterOffset& characterOffset, WordSide side)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    CharacterOffset c = characterOffset;
    if (side == WordSide::LeftWordIfOnBoundary) {
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
    
    return startCharacterOffsetOfWord(previousOffset, WordSide::RightWordIfOnBoundary);
}

CharacterOffset AXObjectCache::nextWordEndCharacterOffset(const CharacterOffset& characterOffset)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    CharacterOffset nextOffset = nextCharacterOffset(characterOffset);
    if (nextOffset.isNull())
        return CharacterOffset();
    
    return endCharacterOffsetOfWord(nextOffset, WordSide::LeftWordIfOnBoundary);
}

std::optional<SimpleRange> AXObjectCache::leftWordRange(const CharacterOffset& characterOffset)
{
    CharacterOffset start = startCharacterOffsetOfWord(characterOffset, WordSide::LeftWordIfOnBoundary);
    CharacterOffset end = endCharacterOffsetOfWord(start);
    return rangeForUnorderedCharacterOffsets(start, end);
}

std::optional<SimpleRange> AXObjectCache::rightWordRange(const CharacterOffset& characterOffset)
{
    CharacterOffset start = startCharacterOffsetOfWord(characterOffset, WordSide::RightWordIfOnBoundary);
    CharacterOffset end = endCharacterOffsetOfWord(start);
    return rangeForUnorderedCharacterOffsets(start, end);
}

static char32_t characterForCharacterOffset(const CharacterOffset& characterOffset)
{
    if (characterOffset.isNull() || !characterOffset.node->isTextNode())
        return 0;
    
    char32_t ch = 0;
    unsigned offset = characterOffset.startIndex + characterOffset.offset;
    if (offset < characterOffset.node->textContent().length()) {
// FIXME: Remove IGNORE_CLANG_WARNINGS macros once one of <rdar://problem/58615489&58615391> is fixed.
IGNORE_CLANG_WARNINGS_BEGIN("conditional-uninitialized")
        U16_NEXT(characterOffset.node->textContent(), offset, characterOffset.node->textContent().length(), ch);
IGNORE_CLANG_WARNINGS_END
    }
    return ch;
}

char32_t AXObjectCache::characterAfter(const CharacterOffset& characterOffset)
{
    return characterForCharacterOffset(nextCharacterOffset(characterOffset));
}

char32_t AXObjectCache::characterBefore(const CharacterOffset& characterOffset)
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

    RefPtr boundary = parentEditingBoundary(RefPtr { characterOffset.node }.get());
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
        return characterOffsetForNodeAndOffset(Ref { *characterOffset.node }, characterOffset.offset + next - prefixLength);
    
    return characterOffset;
}

// FIXME: Share code with the one in VisibleUnits.cpp.
CharacterOffset AXObjectCache::previousBoundary(const CharacterOffset& characterOffset, BoundarySearchFunction searchFunction, NeedsContextAtParagraphStart needsContextAtParagraphStart)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    RefPtr boundary = parentEditingBoundary(RefPtr { characterOffset.node.get() }.get());
    if (!boundary)
        return CharacterOffset();
    
    auto searchRange = rangeForNodeContents(*boundary);
    Vector<UChar, 1024> string;
    unsigned suffixLength = 0;

    if (needsContextAtParagraphStart == NeedsContextAtParagraphStart::Yes && startCharacterOffsetOfParagraph(characterOffset).isEqual(characterOffset)) {
        auto forwardsScanRange = makeRangeSelectingNodeContents(boundary->protectedDocument());
        auto endOfCurrentParagraph = endCharacterOffsetOfParagraph(characterOffset);
        if (!setRangeStartOrEndWithCharacterOffset(forwardsScanRange, characterOffset, true))
            return { };
        if (!setRangeStartOrEndWithCharacterOffset(forwardsScanRange, endOfCurrentParagraph, false))
            return { };
        for (TextIterator forwardsIterator(forwardsScanRange); !forwardsIterator.atEnd(); forwardsIterator.advance())
            append(string, forwardsIterator.text());
        suffixLength = string.size();
    } else if (requiresContextForWordBoundary(characterBefore(characterOffset))) {
        auto forwardsScanRange = makeRangeSelectingNodeContents(boundary->protectedDocument());
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
    // Subsequent *characterOffset.node dereferences are safe because we called CharacterOffset.isNull()
    // at the top of the method.
    if (AccessibilityObject::replacedNodeNeedsCharacter(*characterOffset.node))
        return characterOffsetForNodeAndOffset(*characterOffset.node, 0);
    Node* nextSibling = node.nextSibling();
    if (&node != characterOffset.node.get() && nextSibling && AccessibilityObject::replacedNodeNeedsCharacter(*nextSibling))
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
    
    Ref startNode = *characterOffset.node;
    
    if (isRenderedAsNonInlineTableImageOrHR(startNode.ptr()))
        return startOrEndCharacterOffsetForRange(rangeForNodeContents(startNode), true);
    
    auto startBlock = enclosingBlock(startNode.ptr());
    int offset = characterOffset.startIndex + characterOffset.offset;
    auto highestRoot = highestEditableRoot(firstPositionInOrBeforeNode(startNode.ptr()));
    Position::AnchorType type = Position::PositionIsOffsetInAnchor;
    
    Ref node = *findStartOfParagraph(startNode.ptr(), highestRoot.get(), startBlock.get(), offset, type, boundaryCrossingRule);
    
    if (type == Position::PositionIsOffsetInAnchor)
        return characterOffsetForNodeAndOffset(node, offset, TraverseOptionIncludeStart);
    
    return startOrEndCharacterOffsetForRange(rangeForNodeContents(node), true);
}

CharacterOffset AXObjectCache::endCharacterOffsetOfParagraph(const CharacterOffset& characterOffset, EditingBoundaryCrossingRule boundaryCrossingRule)
{
    if (characterOffset.isNull())
        return CharacterOffset();
    
    RefPtr startNode = characterOffset.node;
    if (isRenderedAsNonInlineTableImageOrHR(startNode.get()))
        return startOrEndCharacterOffsetForRange(rangeForNodeContents(*startNode), false);
    
    auto stayInsideBlock = enclosingBlock(startNode.get());
    int offset = characterOffset.startIndex + characterOffset.offset;
    auto highestRoot = highestEditableRoot(firstPositionInOrBeforeNode(startNode.get()));
    Position::AnchorType type = Position::PositionIsOffsetInAnchor;
    
    Ref node = *findEndOfParagraph(startNode.get(), highestRoot.get(), stayInsideBlock.get(), offset, type, boundaryCrossingRule);
    if (type == Position::PositionIsOffsetInAnchor) {
        if (node->isTextNode()) {
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

    if (auto* renderLineBreak = dynamicDowncast<RenderLineBreak>(renderer); renderLineBreak && InlineIterator::boxFor(*renderLineBreak) != boxAndOffset.box)
        return IntRect();

    return computeLocalCaretRect(*renderer, boxAndOffset);
}

IntRect AXObjectCache::absoluteCaretBoundsForCharacterOffset(const CharacterOffset& characterOffset)
{
    RenderBlock* caretPainter = nullptr;
    
    // First compute a rect local to the renderer at the selection start.
    RenderObject* renderer = nullptr;
    LayoutRect localRect = localCaretRectForCharacterOffset(renderer, characterOffset);
    
    localRect = localCaretRectInRendererForRect(localRect, RefPtr { characterOffset.node }.get(), renderer, caretPainter);
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
    auto range = makeSimpleRange(m_document->caretPositionFromPoint(point, HitTestSource::User));
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

CharacterOffset AXObjectCache::characterOffsetForIndex(int index, const AXCoreObject* object)
{
    if (!object)
        return { };

    auto visiblePosition = object->visiblePositionForIndex(index);
    auto characterOffset = characterOffsetFromVisiblePosition(visiblePosition);
    // In text control, VisiblePosition always gives the before position of a
    // BR node, while CharacterOffset will do the opposite.
    if (object->isTextControl() && characterOffsetNodeIsBR(characterOffset))
        characterOffset.offset = 1;

    auto range = object->simpleRange();
    if (!range)
        return { };

    auto start = startOrEndCharacterOffsetForRange(*range, true, true);
    auto end = startOrEndCharacterOffsetForRange(*range, false, true);
    auto result = start;
    for (int i = 0; i < index; i++) {
        if (result.isEqual(characterOffset)) {
            // Do not include the new line character, always move the offset to the start of next node.
            if ((characterOffset.node->isTextNode() || characterOffsetNodeIsBR(characterOffset))) {
                auto next = nextCharacterOffset(characterOffset, false);
                if (!next.isNull() && !next.offset && rootAXEditableElement(RefPtr { next.node }.get()) == rootAXEditableElement(RefPtr { characterOffset.node }.get()))
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
    const auto* result = node->rootEditableElement();
    const auto* element = dynamicDowncast<Element>(*node);
    if (!element)
        element = node->parentElement();

    for (; element; element = element->parentElement()) {
        if (elementIsTextControl(*element))
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
static void filterMapForRemoval(const UncheckedKeyHashMap<T, U>& list, const Document& document, HashSet<Ref<Node>>& nodesToRemove)
{
    for (auto& entry : list)
        conditionallyAddNodeToFilterList(entry.key, document, nodesToRemove);
}

template<typename T>
static void filterListForRemoval(const ListHashSet<T>& list, const Document& document, HashSet<Ref<Node>>& nodesToRemove)
{
    for (auto& node : list)
        conditionallyAddNodeToFilterList(node.ptr(), document, nodesToRemove);
}

template<typename WeakHashSet>
static void filterWeakHashSetForRemoval(WeakHashSet& weakHashSet, const Document& document, HashSet<Ref<Node>>& nodesToRemove)
{
    weakHashSet.forEach([&] (auto& element) {
        conditionallyAddNodeToFilterList(&element, document, nodesToRemove);
    });
}

template<typename WeakListHashSetType>
static void filterWeakListHashSetForRemoval(WeakListHashSetType& list, const Document& document, HashSet<Ref<Node>>& nodesToRemove)
{
    for (auto& node : list)
        conditionallyAddNodeToFilterList(&node, document, nodesToRemove);
}

template<typename WeakHashMapType>
static void filterWeakHashMapForRemoval(WeakHashMapType& map, const Document& document, HashSet<Ref<Node>>& nodesToRemove)
{
    for (auto elementEntry : map)
        conditionallyAddNodeToFilterList(&elementEntry.key, document, nodesToRemove);
}

void AXObjectCache::prepareForDocumentDestruction(const Document& document)
{
    HashSet<Ref<Node>> nodesToRemove;
    filterWeakListHashSetForRemoval(m_deferredTextChangedList, document, nodesToRemove);
    filterWeakListHashSetForRemoval(m_deferredElementAddedOrRemovedList, document, nodesToRemove);
    filterWeakHashSetForRemoval(m_deferredRecomputeIsIgnoredList, document, nodesToRemove);
    filterWeakHashSetForRemoval(m_deferredRecomputeTableIsExposedList, document, nodesToRemove);
    filterWeakHashSetForRemoval(m_deferredSelectedChildredChangedList, document, nodesToRemove);
    filterWeakHashSetForRemoval(m_deferredModalChangedList, document, nodesToRemove);
    filterWeakHashSetForRemoval(m_deferredMenuListChange, document, nodesToRemove);
    filterWeakHashMapForRemoval(m_deferredTextFormControlValue, document, nodesToRemove);
    m_deferredFocusedNodeChange = std::nullopt;

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
    
bool AXObjectCache::elementIsTextControl(const Element& element)
{
    const auto* axObject = getOrCreate(const_cast<Element&>(element));
    return axObject && axObject->isTextControl();
}

void AXObjectCache::performDeferredCacheUpdate(ForceLayout forceLayout)
{
    AXTRACE(makeString("AXObjectCache::performDeferredCacheUpdate 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    if (m_performingDeferredCacheUpdate) {
        AXLOG("Bailing out due to reentrant call.");
        return;
    }
    SetForScope performingDeferredCacheUpdate(m_performingDeferredCacheUpdate, true);

    Ref document = this->document();

    // It's unexpected for this function to run in the middle of a render tree or style update.
    ASSERT(!Accessibility::inRenderTreeOrStyleUpdate(document));

    if (!document->view())
        return;

    if (document->view()->needsLayout()) {
        // Layout became dirty while waiting to performDeferredCacheUpdate, and we require clean layout
        // to update the accessibility tree correctly in this function.
        if ((m_cacheUpdateDeferredCount >= 3 || forceLayout == ForceLayout::Yes) && !Accessibility::inRenderTreeOrStyleUpdate(document)) {
            // Layout is being thrashed before we get a chance to update, so stop waiting and just force it.
            m_cacheUpdateDeferredCount = 0;
            document->updateLayoutIgnorePendingStylesheets();
        } else {
            // Wait for layout to trigger another async cache update.
            ++m_cacheUpdateDeferredCount;
            return;
        }
    }

    RefPtr<Frame> frame = document->frame();
    if (frame && frame->isMainFrame()) {
        // The layout of subframes must also be clean (assuming we're processing objects from those subframes), so force it here if necessary.
        for (; frame; frame = frame->tree().traverseNext()) {
            auto* localFrame = dynamicDowncast<LocalFrame>(frame.get());
            RefPtr subDocument = localFrame ? localFrame->document() : nullptr;
            if (subDocument && subDocument->view() && subDocument->view()->needsLayout())
                subDocument->updateLayoutIgnorePendingStylesheets();
        }
    }

    bool markedRelationsDirty = false;
    auto markRelationsDirty = [&] () {
        if (!markedRelationsDirty) {
            relationsNeedUpdate(true);
            markedRelationsDirty = true;
        }
    };
    AXLOGDeferredCollection("ReplacedObjectsList"_s, m_deferredReplacedObjects);
    bool anyRelationsDirty = false;
    for (AXID axID : m_deferredReplacedObjects) {
        // If the replaced object was part of any relation, we need to make sure the relations are updated.
        // Relations for this object may have been removed already (via the renderer being destroyed), so
        // we should check if this axID was recently removed so we can dirty relations.
        if (m_relations.contains(axID) || m_recentlyRemovedRelations.contains(axID))
            anyRelationsDirty = true;
        remove(axID);
    }
    m_deferredReplacedObjects.clear();
    if (anyRelationsDirty)
        markRelationsDirty();

    AXLOGDeferredCollection("RecomputeTableIsExposedList"_s, m_deferredRecomputeTableIsExposedList);
    m_deferredRecomputeTableIsExposedList.forEach([this] (auto& tableElement) {
        if (auto* axTable = dynamicDowncast<AccessibilityTable>(get(&tableElement)))
            axTable->recomputeIsExposable();
    });
    m_deferredRecomputeTableIsExposedList.clear();

    AXLOGDeferredCollection("ElementAddedOrRemovedList"_s, m_deferredElementAddedOrRemovedList);
    auto nodeAddedOrRemovedList = copyToVector(m_deferredElementAddedOrRemovedList);
    for (auto& weakNode : nodeAddedOrRemovedList) {
        if (RefPtr node = weakNode.get()) {
            handleMenuOpened(*node);
            handleLiveRegionCreated(*node);

            if (RefPtr label = dynamicDowncast<HTMLLabelElement>(node.get())) {
                // A label was added or removed. Update its LabelFor relationships.
                handleLabelChanged(getOrCreate(*label));
            }
        }
    }
    m_deferredElementAddedOrRemovedList.clear();

    AXLOGDeferredCollection("ChildrenChangedList"_s, m_deferredChildrenChangedList);
    handleAllDeferredChildrenChanged();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    AXLOGDeferredCollection("UnconnectedObjects"_s, m_deferredUnconnectedObjects);
    if (auto tree = AXIsolatedTree::treeForPageID(m_pageID)) {
        m_deferredUnconnectedObjects.forEach([&tree] (auto& object) {
            tree->addUnconnectedNode(object);
        });
        m_deferredUnconnectedObjects.clear();
    }
#endif

    AXLOGDeferredCollection("RecomputeTableCellSlotsList"_s, m_deferredRecomputeTableCellSlotsList);
    m_deferredRecomputeTableCellSlotsList.forEach([this] (auto& axTable) {
        handleRecomputeCellSlots(axTable);
    });
    m_deferredRecomputeTableCellSlotsList.clear();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    AXLOGDeferredCollection("RowspanChangesList"_s, m_deferredRowspanChanges);
    m_deferredRowspanChanges.forEach([this] (auto& axCell) {
        handleRowspanChanged(axCell);
    });
    m_deferredRowspanChanges.clear();
#endif

    AXLOGDeferredCollection("TextChangedList"_s, m_deferredTextChangedList);
    auto textChangedList = copyToVector(m_deferredTextChangedList);
    for (auto& weakNode : textChangedList) {
        if (RefPtr node = weakNode.get())
            handleTextChanged(getOrCreate(*node));
    }
    m_deferredTextChangedList.clear();

    AXLOGDeferredCollection("RecomputeIsIgnoredList"_s, m_deferredRecomputeIsIgnoredList);
    m_deferredRecomputeIsIgnoredList.forEach([this] (auto& element) {
        if (auto* renderer = element.renderer())
            recomputeIsIgnored(*renderer);
    });
    m_deferredRecomputeIsIgnoredList.clear();

    AXLOGDeferredCollection("SelectedChildredChangedList"_s, m_deferredSelectedChildredChangedList);
    m_deferredSelectedChildredChangedList.forEach([this] (auto& selectElement) {
        selectedChildrenChanged(&selectElement);
    });
    m_deferredSelectedChildredChangedList.clear();

    AXLOGDeferredCollection("TextFormControlValue"_s, m_deferredTextFormControlValue);
    for (auto deferredFormControlContext : m_deferredTextFormControlValue) {
        auto& textFormControlElement = downcast<HTMLTextFormControlElement>(deferredFormControlContext.key);
        postTextReplacementNotificationForTextControl(textFormControlElement, deferredFormControlContext.value, textFormControlElement.innerTextValue());
    }
    m_deferredTextFormControlValue.clear();

    AXLOGDeferredCollection("AttributeChange"_s, m_deferredAttributeChange);
    for (const auto& attributeChange : m_deferredAttributeChange) {
        handleAttributeChange(attributeChange.element.get(), attributeChange.attrName, attributeChange.oldValue, attributeChange.newValue);
        if (attributeChange.attrName == idAttr)
            markRelationsDirty();
    }
    m_deferredAttributeChange.clear();

    if (m_deferredFocusedNodeChange) {
        AXLOG(makeString(
            "Processing deferred focused node change. Old node "_s,
            m_deferredFocusedNodeChange->first ? m_deferredFocusedNodeChange->first->debugDescription() : "nullptr"_s,
            ", new node "_s,
            m_deferredFocusedNodeChange->second ? m_deferredFocusedNodeChange->second->debugDescription() : "nullptr"_s
        ));
        // Don't update the modal with this focus change since it may need to be updated again as a result of processing m_deferredModalChangedList below.
        handleFocusedUIElementChanged(m_deferredFocusedNodeChange->first.get(), m_deferredFocusedNodeChange->second.get(), UpdateModal::No);
        // Recompute isIgnored after a focus change in case that altered visibility.
        recomputeIsIgnored(m_deferredFocusedNodeChange->first.get());
        recomputeIsIgnored(m_deferredFocusedNodeChange->second.get());
    }
    bool updatedFocusedElement = m_deferredFocusedNodeChange.has_value();
    // If we changed the focused element, that could affect what modal should be active, so recompute it.
    bool shouldRecomputeModal = updatedFocusedElement;
    RefPtr newFocusElement = m_deferredFocusedNodeChange ? m_deferredFocusedNodeChange->second.get() : nullptr;
    m_deferredFocusedNodeChange = std::nullopt;

    AXLOGDeferredCollection("ModalChangedList"_s, m_deferredModalChangedList);
    for (auto& element : m_deferredModalChangedList) {
        if (!is<HTMLDialogElement>(element) && !hasAnyRole(element, { "dialog"_s, "alertdialog"_s }))
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
        // If we are re-computing the modal, automatically focus into it if the author didn't explicitly change focus, or
        // they did change focus but the focused element has become unfocusable since it was set (e.g. it gained display:none).
        bool shouldFocusIntoModal = !updatedFocusedElement || (newFocusElement && !newFocusElement->isFocusable());

        updateCurrentModalNode(shouldFocusIntoModal ? WillRecomputeFocus::Yes : WillRecomputeFocus::No);
        // "When a modal element is displayed, assistive technologies SHOULD navigate to the element unless focus has explicitly been set elsewhere."
        // `updatedFocusedElement` indicates focus was explicitly set elsewhere, so don't autofocus into the modal.
        // https://w3c.github.io/aria/#aria-modal
        if (shouldFocusIntoModal)
            focusCurrentModal();
    }

    AXLOGDeferredCollection("MenuListChange"_s, m_deferredMenuListChange);
    m_deferredMenuListChange.forEach([this] (auto& element) {
        handleMenuListValueChanged(element);
    });
    m_deferredMenuListChange.clear();

    m_deferredScrollbarUpdateChangeList.forEach([this] (auto& scrollView) {
        handleScrollbarUpdate(scrollView);
    });
    m_deferredScrollbarUpdateChangeList.clear();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (m_deferredRegenerateIsolatedTree) {
        if (auto tree = AXIsolatedTree::treeForPageID(m_pageID)) {
            // Re-generate the subtree rooted at the webarea.
            if (RefPtr webArea = rootWebArea()) {
                AXLOG("Regenerating isolated tree from AXObjectCache::performDeferredCacheUpdate().");
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
                // m_deferredRegenerateIsolatedTree is only set when we change the active modal, which effects the ignored
                // status of every object on the page. With ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE), this means we only have
                // to re-compute is-ignored for every object, rather than re-compute all objects entirely as when this flag is off.
                tree->updatePropertiesForSelfAndDescendants(*webArea, { AXPropertyName::IsIgnored });
#else
                tree->generateSubtree(*webArea);
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

                // In some cases, the ID of the focus after a dialog pops up doesn't match the ID in the last focus change notification, creating a mismatch between the isolated tree cached focused object ID and the actual focused object ID.
                // For this reason, reset the focused object ID.
                if (auto* focus = focusedObjectForPage(document->protectedPage().get()))
                    tree->setFocusedNodeID(focus->objectID());
            }
        }
    }
    m_deferredRegenerateIsolatedTree = false;
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

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

    postPlatformNotification(*object, AXMenuListValueChanged);
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::updateIsolatedTree(AccessibilityObject* object, AXNotification notification)
{
    if (object)
        updateIsolatedTree(*object, notification);
}

void AXObjectCache::updateIsolatedTree(AccessibilityObject& object, AXNotification notification)
{
    updateIsolatedTree({ std::make_pair(Ref { object }, notification) });
}

void AXObjectCache::updateIsolatedTree(const Vector<std::pair<Ref<AccessibilityObject>, AXNotification>>& notifications)
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

    enum class Field : uint8_t {
        Children = 1 << 0,
        DependentProperties = 1 << 1,
        Node = 1 << 2,
    };
    UncheckedKeyHashMap<AXID, OptionSet<Field>> updatedObjects;
    auto updateChildren = [&] (const Ref<AccessibilityObject>& axObject) {
        auto updatedFields = updatedObjects.get(axObject->objectID());
        if (!updatedFields.contains(Field::Children)) {
            updatedFields.add(Field::Children);
            updatedObjects.set(axObject->objectID(), updatedFields);
            tree->queueNodeUpdate(axObject->objectID(), NodeUpdateOptions::childrenUpdate());
        }
    };
    auto updateDependentProperties = [&] (const Ref<AccessibilityObject>& axObject) {
        auto updatedFields = updatedObjects.get(axObject->objectID());
        if (!updatedFields.contains(Field::DependentProperties)) {
            updatedFields.add(Field::DependentProperties);
            updatedObjects.set(axObject->objectID(), updatedFields);
            tree->updateDependentProperties(axObject.get());
        }
    };
    auto updateNode = [&] (const Ref<AccessibilityObject>& axObject) {
        auto updatedFields = updatedObjects.get(axObject->objectID());
        if (!updatedFields.contains(Field::Node)) {
            updatedFields.add(Field::Node);
            updatedObjects.set(axObject->objectID(), updatedFields);
            tree->queueNodeUpdate(axObject->objectID(), NodeUpdateOptions::nodeUpdate());
        }
    };

    for (const auto& notification : notifications) {
        AXLOG(notification);
        if (notification.first->isDetached())
            continue;

        switch (notification.second) {
        case AXAccessKeyChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::AccessKey });
            break;
        case AXAutofillTypeChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::ValueAutofillButtonType });
            break;
        case AXARIAColumnIndexChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::AXColumnIndex });
            break;
        case AXARIARowIndexChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::AXRowIndex });
            break;
        case AXBrailleLabelChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::BrailleLabel });
            break;
        case AXBrailleRoleDescriptionChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::BrailleRoleDescription });
            break;
        case AXCellSlotsChanged:
            ASSERT(notification.first->isTable());
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::CellSlots });
            break;
        case AXCheckedStateChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::IsChecked });
            break;
        case AXCurrentStateChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::CurrentState });
            break;
        case AXColumnCountChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::AXColumnCount });
            break;
        case AXColumnIndexChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXPropertyName::ColumnIndexRange, AXPropertyName::ColumnIndex } });
            break;
        case AXColumnSpanChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::ColumnIndexRange });
            break;
        case AXDisabledStateChanged:
            tree->updatePropertiesForSelfAndDescendants(notification.first.get(), { { AXPropertyName::CanSetFocusAttribute, AXPropertyName::IsEnabled } });
            break;
        case AXExpandedChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::IsExpanded });
            break;
        case AXExtendedDescriptionChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXPropertyName::AccessibilityText, AXPropertyName::ExtendedDescription } });
            break;
        case AXFocusableStateChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::CanSetFocusAttribute });
            break;
        case AXMaximumValueChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXPropertyName::MaxValueForRange, AXPropertyName::ValueForRange } });
            break;
        case AXMenuListItemSelected: {
            RefPtr ancestor = Accessibility::findAncestor<AccessibilityObject>(notification.first.get(), false, [] (const auto& object) {
                return object.isMenu() || object.isMenuBar();
            });
            if (ancestor) {
                tree->queueNodeUpdate(ancestor->objectID(), { AXPropertyName::SelectedChildren });
                tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::IsSelected });
            }
            break;
        }
        case AXMinimumValueChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXPropertyName::MinValueForRange, AXPropertyName::ValueForRange } });
            break;
        case AXNameChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::NameAttribute });
            break;
        case AXOrientationChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::Orientation });
            break;
        case AXPositionInSetChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXPropertyName::PosInSet, AXPropertyName::SupportsPosInSet } });
            break;
        case AXPopoverTargetChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXPropertyName::SupportsExpanded, AXPropertyName::IsExpanded } });
            break;
        case AXSelectedTextChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::SelectedTextRange });
            break;
        case AXSortDirectionChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::SortDirection });
            break;
        case AXIdAttributeChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::IdentifierAttribute });
            break;
        case AXReadOnlyStatusChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::CanSetValueAttribute });
            break;
        case AXRequiredStatusChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::IsRequired });
            break;
        case AXRoleDescriptionChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::RoleDescription });
            break;
        case AXRowIndexChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXPropertyName::RowIndexRange, AXPropertyName::RowIndex } });
            break;
        case AXRowSpanChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::RowIndexRange });
            break;
        case AXCellScopeChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXPropertyName::CellScope, AXPropertyName::IsColumnHeader, AXPropertyName::IsRowHeader } });
            break;
        //  FIXME: Contrary to the name "AXSelectedCellsChanged", this notification can be posted on a cell
        //  who has changed selected state, not just on table or grid who has changed its selected cells.
        case AXSelectedCellsChanged:
        case AXSelectedStateChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::IsSelected });
            break;
        case AXSetSizeChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXPropertyName::SetSize, AXPropertyName::SupportsSetSize } });
            break;
        case AXTextCompositionChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::TextInputMarkedTextMarkerRange });
            break;
        case AXTextUnderElementChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXPropertyName::AccessibilityText, AXPropertyName::Title } });
            if (notification.first->isAccessibilityLabelInstance() || notification.first->roleValue() == AccessibilityRole::TextField)
                tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::StringValue });
            break;
#if ENABLE(AX_THREAD_TEXT_APIS)
        case AXTextRunsChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::TextRuns });
            break;
#endif
        case AXURLChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXPropertyName::URL, AXPropertyName::InternalLinkElement } });
            break;
        case AXKeyShortcutsChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::KeyShortcuts });
            break;
        case AXVisibilityChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXPropertyName::IsVisible });
            break;
        case AXActiveDescendantChanged:
        case AXRoleChanged:
        case AXControlledObjectsChanged:
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
        case AXSelectedChildrenChanged:
        case AXTextChanged:
        case AXTextSecurityChanged:
        case AXValueChanged:
            updateNode(notification.first);
            break;
        case AXLabelChanged: {
            updateNode(notification.first);
            updateDependentProperties(notification.first);
            break;
        }
        case AXLanguageChanged:
        case AXRowCountChanged:
            updateNode(notification.first);
            FALLTHROUGH;
        case AXRowCollapsed:
        case AXRowExpanded:
            updateChildren(notification.first);
            break;
        default:
            break;
        }
    }
}

void AXObjectCache::updateIsolatedTree(AccessibilityObject* axObject, AXPropertyName property) const
{
    if (axObject)
        updateIsolatedTree(*axObject, property);
}

void AXObjectCache::updateIsolatedTree(AccessibilityObject& axObject, AXPropertyName property) const
{
    if (RefPtr tree = AXIsolatedTree::treeForPageID(m_pageID))
        tree->queueNodeUpdate(axObject.objectID(), { property });
}

void AXObjectCache::startUpdateTreeSnapshotTimer()
{
    if (!m_updateTreeSnapshotTimer.isActive())
        m_updateTreeSnapshotTimer.startOneShot(updateTreeSnapshotTimerInterval);
}

void AXObjectCache::onPaint(const RenderObject& renderer, IntRect&& paintRect) const
{
    if (!m_pageID)
        return;
    m_geometryManager->cacheRect(m_renderObjectMapping.getOptional(const_cast<RenderObject&>(renderer)), WTFMove(paintRect));
}

void AXObjectCache::onPaint(const Widget& widget, IntRect&& paintRect) const
{
    if (!m_pageID)
        return;
    m_geometryManager->cacheRect(m_widgetObjectMapping.getOptional(const_cast<Widget&>(widget)), WTFMove(paintRect));
}
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

void AXObjectCache::deferRecomputeIsIgnoredIfNeeded(Element* element)
{
    if (!nodeAndRendererAreValid(element))
        return;
    
    SingleThreadWeakRef<RenderObject> renderer = *element->renderer();
    if (rendererNeedsDeferredUpdate(renderer.get())) {
        m_deferredRecomputeIsIgnoredList.add(*element);
        return;
    }
    recomputeIsIgnored(renderer.get());
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

void AXObjectCache::deferRecomputeTableCellSlots(AccessibilityTable& axTable)
{
    m_deferredRecomputeTableCellSlotsList.add(axTable);
    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}

void AXObjectCache::deferRowspanChange(AccessibilityObject* axObject)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    auto* axTableCell = dynamicDowncast<AccessibilityTableCell>(axObject);
    if (!axTableCell)
        return;

    m_deferredRowspanChanges.add(*axTableCell);
    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
#else
    UNUSED_PARAM(axObject);
#endif
}

void AXObjectCache::deferTextChangedIfNeeded(Node* node)
{
    if (!nodeAndRendererAreValid(node))
        return;

    if (rendererNeedsDeferredUpdate(*node->renderer())) {
        m_deferredTextChangedList.add(*node);
        return;
    }
    handleTextChanged(getOrCreate(*node));
}

void AXObjectCache::deferSelectedChildrenChangedIfNeeded(Element& selectElement)
{
    if (!nodeRendererIsValid(selectElement))
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
    m_deferredTextFormControlValue.add(formControlElement, previousValue);
}

bool isNodeFocused(Node& node)
{
    return is<Element>(node) && uncheckedDowncast<Element>(node).focused();
}

// DOM component of hidden definition.
// https://www.w3.org/TR/wai-aria/#dfn-hidden
bool isDOMHidden(const RenderStyle* style)
{
    return style ? style->display() == DisplayType::None || style->usedVisibility() != Visibility::Visible : true;
}

AccessibilityObject* AXObjectCache::rootWebArea()
{
    auto* root = getOrCreate(m_document->view());
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
        popovertargetAttr,
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
    case AXRelationType::LabeledBy:
        return AXRelationType::LabelFor;
    case AXRelationType::LabelFor:
        return AXRelationType::LabeledBy;
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
    if (attribute == aria_controlsAttr || attribute == popovertargetAttr)
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
        return AXRelationType::LabeledBy;
    if (attribute == aria_ownsAttr)
        return AXRelationType::OwnerFor;
    if (attribute == headersAttr)
        return AXRelationType::Headers;
    return AXRelationType::None;
}

static bool validRelation(void* origin, void* target, AXRelationType relationType)
{
    if (!origin || !target || relationType == AXRelationType::None)
        return false;
    return origin != target || relationType == AXRelationType::LabeledBy;
}

static bool validRelation(Element& origin, Element& target, AXRelationType relationType)
{
    if (relationType == AXRelationType::None)
        return false;
    return &origin != &target || relationType == AXRelationType::LabeledBy;
}

bool AXObjectCache::addRelation(Element& origin, Element& target, AXRelationType relationType)
{
    AXTRACE("AXObjectCache::addRelation"_s);
    AXLOG(makeString("origin: "_s, origin.debugDescription(), " target: "_s, target.debugDescription(), " relationType "_s, static_cast<uint8_t>(relationType)));

    if (!validRelation(origin, target, relationType)) {
        ASSERT_NOT_REACHED();
        return false;
    }

    if (relationType == AXRelationType::LabelFor) {
        // Add a LabelFor relation if the target doesn't have an ARIA label which should take precedence.
        if (target.hasAttributeWithoutSynchronization(aria_labelAttr)
            || target.hasAttributeWithoutSynchronization(aria_labelledbyAttr)
            || target.hasAttributeWithoutSynchronization(aria_labeledbyAttr))
            return false;
    }

    return addRelation(RefPtr { getOrCreate(origin, IsPartOfRelation::Yes) }.get(), RefPtr { getOrCreate(target, IsPartOfRelation::Yes) }.get(), relationType);
}

static bool canHaveRelations(Element& element)
{
    return !(element.hasTagName(metaTag) || element.hasTagName(headTag) || element.hasTagName(scriptTag) || element.hasTagName(htmlTag) || element.hasTagName(styleTag));
}

static bool relationCausesCycle(AccessibilityObject* origin, AccessibilityObject* target, AXRelationType relationType)
{
    // Validate that we're not creating an aria-owns cycle.
    if (relationType == AXRelationType::OwnerFor) {
        for (auto* verifyOrigin = origin; verifyOrigin; verifyOrigin = verifyOrigin->parentObject()) {
            if (verifyOrigin == target)
                return true;
        }
    } else if (relationType == AXRelationType::OwnedBy) {
        for (auto* verifyTarget = target; verifyTarget; verifyTarget = verifyTarget->parentObject()) {
            if (verifyTarget == origin)
                return true;
        }
    }

    return false;
}

bool AXObjectCache::addRelation(AccessibilityObject* origin, AccessibilityObject* target, AXRelationType relationType, AddSymmetricRelation addSymmetricRelation)
{
    AXTRACE("AXObjectCache::addRelation"_s);
    AXLOG(origin);
    AXLOG(target);
    AXLOG(relationType);

    if (!validRelation(origin, target, relationType))
        return false;

    if (relationCausesCycle(origin, target, relationType))
        return false;

    if (relationType == AXRelationType::OwnerFor) {
        // Before adding the new OwnerFor relationship, that alters the AX parent-child hierarchy, notify the current target-s parent that one child is being removed.
        // FIXME: This kicks off children-changed notifications every time relations are dirtied and cleaned,
        // even if this specific relationship existed before, incurring unnecessary work.
        RefPtr targetParent = target->parentObject();
        if (targetParent && targetParent.get() != origin)
            childrenChanged(targetParent.get());
    }

    AXID originID = origin->objectID();
    AXID targetID = target->objectID();
    auto relationsIterator = m_relations.find(originID);
    if (relationsIterator == m_relations.end()) {
        // No relations for this object, add the first one.
        m_relations.add(originID, AXRelations { { enumToUnderlyingType(relationType), { targetID } } });
    } else if (auto targetsIterator = relationsIterator->value.find(enumToUnderlyingType(relationType)); targetsIterator == relationsIterator->value.end()) {
        // No relation of this type for this object, add the first one.
        relationsIterator->value.add(enumToUnderlyingType(relationType), ListHashSet { targetID });
    } else {
        // There are already relations of this type for the object. Add the new relation.
        if (relationType == AXRelationType::ActiveDescendant
            || relationType == AXRelationType::OwnedBy) {
            // There should be only one active descendant and only one owner. Enforce that by removing any existing targets.
            targetsIterator->value.clear();
        }
        targetsIterator->value.add(targetID);
    }
    m_relationTargets.add(targetID);

    if (relationType == AXRelationType::OwnerFor) {
        // First find and clear the old owner.
        for (auto oldOwnerIterator = m_relations.begin(); oldOwnerIterator != m_relations.end(); ++oldOwnerIterator) {
            if (oldOwnerIterator->key == originID)
                continue;

            removeRelationByID(oldOwnerIterator->key, targetID, AXRelationType::OwnerFor);
        }

        childrenChanged(origin);
    } else if (relationType == AXRelationType::OwnedBy) {
        if (auto* parentObject = origin->parentObjectUnignored())
            childrenChanged(parentObject);
    }

    if (addSymmetricRelation == AddSymmetricRelation::Yes
        && m_objects.contains(originID) && m_objects.contains(targetID)) {
        // If the IDs are still in the m_objects map, the objects should be still alive.
        if (auto symmetric = symmetricRelation(relationType); symmetric != AXRelationType::None)
            addRelation(target, origin, symmetric, AddSymmetricRelation::No);

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        if (auto tree = AXIsolatedTree::treeForPageID(m_pageID)) {
            if (origin && origin->isIgnored())
                deferAddUnconnectedNode(*origin);
            if (target && target->isIgnored())
                deferAddUnconnectedNode(*target);
        }
#endif
    }

    return true;
}

void AXObjectCache::removeAllRelations(AXID axID)
{
    AXTRACE(makeString("AXObjectCache::removeRelations for axID "_s, axID.loggingString()));

    auto it = m_relations.find(axID);
    if (it == m_relations.end())
        return;

    m_recentlyRemovedRelations.add(axID, it->value);

    for (auto relationType : it->value.keys()) {
        auto symmetric = symmetricRelation(static_cast<AXRelationType>(relationType));
        if (symmetric == AXRelationType::None)
            continue;

        auto targetIDs = it->value.get(static_cast<uint8_t>(relationType));
        for (AXID targetID : targetIDs)
            removeRelationByID(targetID, axID, symmetric);
    }

    m_relations.remove(it);
    dirtyIsolatedTreeRelations();
}

bool AXObjectCache::removeRelation(Element& origin, AXRelationType relationType)
{
    AXTRACE(makeString("AXObjectCache::removeRelations for "_s, origin.debugDescription()));
    AXLOG(relationType);

    auto* object = get(&origin);
    if (!object)
        return false;

    auto relationsIterator = m_relations.find(object->objectID());
    if (relationsIterator == m_relations.end())
        return false;

    auto targetIDs = relationsIterator->value.take(enumToUnderlyingType(relationType));
    bool removedRelation = !targetIDs.isEmpty();

    auto symmetric = symmetricRelation(relationType);
    if (symmetric != AXRelationType::None) {
        for (AXID targetID : targetIDs)
            removeRelationByID(targetID, object->objectID(), symmetric);
    }

    if (removedRelation && relationType == AXRelationType::OwnerFor)
        childrenChanged(object);

    return removedRelation;
}

void AXObjectCache::removeRelationByID(AXID originID, AXID targetID, AXRelationType relationType)
{
    AXTRACE("AXObjectCache::removeRelationByID"_s);
    AXLOG(makeString("originID "_s, originID.loggingString(), " targetID "_s, targetID.loggingString()));
    AXLOG(relationType);

    auto relationsIterator = m_relations.find(originID);
    if (relationsIterator == m_relations.end())
        return;

    auto targetsIterator = relationsIterator->value.find(enumToUnderlyingType(relationType));
    if (targetsIterator == relationsIterator->value.end())
        return;
    targetsIterator->value.remove(targetID);
}

void AXObjectCache::updateRelationsIfNeeded()
{
    if (!m_relationsNeedUpdate)
        return;
    relationsNeedUpdate(false);
    m_relations.clear();
    m_recentlyRemovedRelations.clear();
    m_relationTargets.clear();
    updateRelationsForTree(m_document->rootNode());
}

void AXObjectCache::updateRelationsForTree(ContainerNode& rootNode)
{
    ASSERT(!rootNode.parentNode());
    for (auto& element : descendantsOfType<Element>(rootNode)) {
        if (!canHaveRelations(element))
            continue;

        if (RefPtr shadowRoot = element.shadowRoot(); shadowRoot && shadowRoot->mode() != ShadowRootMode::UserAgent)
            updateRelationsForTree(*shadowRoot);
        if (RefPtr frameOwnerElement = dynamicDowncast<HTMLFrameOwnerElement>(element)) {
            if (RefPtr document = frameOwnerElement->contentDocument())
                updateRelationsForTree(*document);
        }

        for (const auto& attribute : relationAttributes())
            addRelation(element, attribute);

        // In addition to ARIA specified relations, there may be other relevant relations.
        // For instance, LabelFor in HTMLLabelElements.
        addLabelForRelation(element);
    }
}

bool AXObjectCache::addRelation(Element& origin, const QualifiedName& attribute)
{
    if (attribute == aria_labeledbyAttr && origin.hasAttribute(aria_labelledbyAttr)) {
        // The attribute name with British spelling should override the one with American spelling.
        return false;
    }

    bool addedRelation = false;
    auto relationType = attributeToRelationType(attribute);
    if (Element::isElementReflectionAttribute(Ref { m_document->settings() }, attribute)) {
        if (auto reflectedElement = origin.getElementAttribute(attribute))
            return addRelation(origin, *reflectedElement, relationType);
    } else if (Element::isElementsArrayReflectionAttribute(attribute)) {
        if (auto reflectedElements = origin.getElementsArrayAttribute(attribute)) {
            for (auto reflectedElement : reflectedElements.value()) {
                if (addRelation(origin, reflectedElement, relationType))
                    addedRelation = true;
            }
            return addedRelation;
        }
    }

    auto& value = origin.attributeWithoutSynchronization(attribute);
    if (value.isNull()) {
        if (auto* defaultARIA = origin.customElementDefaultARIAIfExists()) {
            for (auto& target : defaultARIA->elementsForAttribute(origin, attribute)) {
                if (addRelation(origin, target, relationType))
                    addedRelation = true;
            }
        }
        return addedRelation;
    }

    SpaceSplitString ids(value, SpaceSplitString::ShouldFoldCase::No);
    for (auto& id : ids) {
        RefPtr target = origin.treeScope().getElementById(id);
        if (!target || target == &origin)
            continue;

        if (addRelation(origin, *target, relationType))
            addedRelation = true;
    }

    return addedRelation;
}

void AXObjectCache::addLabelForRelation(Element& origin)
{
    bool addedRelation = false;

    // LabelFor relations are established for <label for=...> and for <figcaption> elements.
    if (RefPtr label = dynamicDowncast<HTMLLabelElement>(origin)) {
        if (RefPtr control = Accessibility::controlForLabelElement(*label))
            addedRelation = addRelation(origin, *control, AXRelationType::LabelFor);
    } else if (origin.hasTagName(figcaptionTag)) {
        RefPtr parent = origin.parentNode();
        if (parent && parent->hasTagName(figureTag))
            addedRelation = addRelation(RefPtr { getOrCreate(origin) }.get(), RefPtr { getOrCreate(*parent) }.get(), AXRelationType::LabelFor);
    }

    if (addedRelation)
        dirtyIsolatedTreeRelations();
}

void AXObjectCache::updateRelations(Element& origin, const QualifiedName& attribute)
{
    if (!canHaveRelations(origin))
        return;

    auto relationType = attributeToRelationType(attribute);
    if (relationType == AXRelationType::None) {
        ASSERT_NOT_REACHED();
        return;
    }

    // `popovertarget` is only valid for input and button elements.
    if (UNLIKELY(attribute == popovertargetAttr) && !is<HTMLInputElement>(origin) && !is<HTMLButtonElement>(origin))
        return;

    bool changedRelation = removeRelation(origin, relationType);
    changedRelation |= addRelation(origin, attribute);
    if (changedRelation)
        dirtyIsolatedTreeRelations();
}

void AXObjectCache::relationsNeedUpdate(bool needUpdate)
{
    m_relationsNeedUpdate = needUpdate;
    if (m_relationsNeedUpdate)
        dirtyIsolatedTreeRelations();
}

UncheckedKeyHashMap<AXID, AXRelations> AXObjectCache::relations()
{
    updateRelationsIfNeeded();
    return m_relations;
}

const HashSet<AXID>& AXObjectCache::relationTargetIDs()
{
    updateRelationsIfNeeded();
    return m_relationTargets;
}

bool AXObjectCache::isDescendantOfRelatedNode(Node& node)
{
    auto& targetIDs = relationTargetIDs();
    for (auto* parent = node.parentNode(); parent; parent = parent->parentNode()) {
        auto* object = get(*parent);
        if (object && (m_relations.contains(object->objectID()) || targetIDs.contains(object->objectID())))
            return true;
    }
    return false;
}

std::optional<ListHashSet<AXID>> AXObjectCache::relatedObjectIDsFor(const AXCoreObject& object, AXRelationType relationType, UpdateRelations updateRelations)
{
    if (updateRelations == UpdateRelations::Yes)
        updateRelationsIfNeeded();

    auto relationsIterator = m_relations.find(object.objectID());
    if (relationsIterator == m_relations.end())
        return std::nullopt;

    auto targetsIterator = relationsIterator->value.find(enumToUnderlyingType(relationType));
    if (targetsIterator == relationsIterator->value.end())
        return std::nullopt;
    return targetsIterator->value;
}

#if PLATFORM(COCOA)
void AXObjectCache::announce(const String& message)
{
    postPlatformAnnouncementNotification(message);
}
#else
void AXObjectCache::announce(const String&)
{
    // FIXME: implement in other platforms.
}
#endif

AXAttributeCacheEnabler::AXAttributeCacheEnabler(AXObjectCache* cache)
    : m_cache(cache)
{
    if (m_cache) {
        if (m_cache->computedObjectAttributeCache())
            m_wasAlreadyCaching = true;
        else
            m_cache->startCachingComputedObjectAttributesUntilTreeMutates();
    }
}
    
AXAttributeCacheEnabler::~AXAttributeCacheEnabler()
{
    if (m_cache && !m_wasAlreadyCaching)
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

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::selectedTextRangeTimerFired()
{
    if (!accessibilityEnabled())
        return;

    m_selectedTextRangeTimer.stop();

    if (m_lastDebouncedTextRangeObject) {
        for (auto* axObject = objectForID(*m_lastDebouncedTextRangeObject); axObject; axObject = axObject->parentObject()) {
            if (axObject->isTextControl())
                postNotification(*axObject, AXSelectedTextChanged);
        }
    }

    m_lastDebouncedTextRangeObject = std::nullopt;
}

void AXObjectCache::updateTreeSnapshotTimerFired()
{
    m_updateTreeSnapshotTimer.stop();
    processQueuedIsolatedNodeUpdates();
}

void AXObjectCache::processQueuedIsolatedNodeUpdates()
{
    if (auto tree = AXIsolatedTree::treeForPageID(m_pageID))
        tree->processQueuedNodeUpdates();
}
#endif

void AXObjectCache::onWidgetVisibilityChanged(RenderWidget& widget)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    postNotification(get(widget), AXVisibilityChanged);
#else
    UNUSED_PARAM(widget);
#endif
}

} // namespace WebCore
