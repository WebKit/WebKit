/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
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

#include "AXAttributeCacheScope.h"
#include "AXComputedObjectAttributeCache.h"
#include "AXIsolatedObject.h"
#include "AXIsolatedTree.h"
#include "AXListHelpers.h"
#include "AXLocalFrame.h"
#include "AXLogger.h"
#include "AXLoggerBase.h"
#include "AXNotifications.h"
#include "AXObjectCacheInlines.h"
#include "AXRemoteFrame.h"
#include "AXTextMarker.h"
#include "AXTreeStoreInlines.h"
#include "AXUtilities.h"
#include "AccessibilityListBoxOption.h"
#include "AccessibilityMathMLElement.h"
#include "AccessibilityMenuList.h"
#include "AccessibilityMenuListOption.h"
#include "AccessibilityMenuListPopup.h"
#include "AccessibilityObjectInlines.h"
#include "AccessibilityProgressIndicator.h"
#include "AccessibilityRenderObject.h"
#include "AccessibilitySVGObject.h"
#include "AccessibilityScrollView.h"
#include "AccessibilityScrollbar.h"
#include "AccessibilitySlider.h"
#include "AccessibilitySpinButton.h"
#include "AccessibilityTableColumn.h"
#include "AccessibilityTableHeaderContainer.h"
#include "AriaNotifyOptions.h"
#include "CaretRectComputation.h"
#include "ContainerNodeInlines.h"
#include "CustomElementDefaultARIA.h"
#include "DeprecatedGlobalSettings.h"
#include "DocumentPage.h"
#include "Editing.h"
#include "Editor.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ElementRareData.h"
#include "EventNames.h"
#include "FocusController.h"
#include "FrameLoader.h"
#include "HTMLAreaElement.h"
#include "HTMLButtonElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLDetailsElement.h"
#include "HTMLDialogElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLMapElement.h"
#include "HTMLMeterElement.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLProgressElement.h"
#include "HTMLSelectElement.h"
#include "HTMLSummaryElement.h"
#include "HTMLTableElement.h"
#include "HTMLTablePartElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableSectionElement.h"
#include "HTMLTextFormControlElement.h"
#include "HitTestSource.h"
#include "InlineIteratorLogicalOrderTraversal.h"
#include "InlineRunAndOffset.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "MathMLElement.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "Range.h"
#include "RenderAttachment.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLineBreak.h"
#include "RenderListBox.h"
#include "RenderListMarker.h"
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
#include "Settings.h"
#include "ShadowRoot.h"
#include "TextBoundaries.h"
#include "TextControlInnerElements.h"
#include "TextIterator.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <utility>
#include <wtf/DataLog.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/MakeString.h>

#if PLATFORM(COCOA)
#include "AXLiveRegionManager.h"
#include <wtf/spi/darwin/OSVariantSPI.h>
#endif

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXObjectCache);
WTF_MAKE_TZONE_ALLOCATED_IMPL(AXObjectCache);

using namespace HTMLNames;

#if PLATFORM(COCOA)

// Post notifications for secure fields or elements contained in secure fields at a 40hz interval to thwart analysis of typing cadence.
static const Seconds accessibilityPasswordValueChangeNotificationInterval { 25_ms };

static bool isSecureFieldOrContainedBySecureField(AccessibilityObject& object)
{
    return object.isSecureField() || object.isContainedBySecureField();
}

#endif // PLATFORM(COCOA)

static bool rendererNeedsDeferredUpdate(const RenderObject& renderer)
{
    ASSERT(!renderer.beingDestroyed());
    Ref document = renderer.document();
    return renderer.needsLayout() || document->needsStyleRecalc() || document->inRenderTreeUpdate() || (document->view() && document->view()->layoutContext().isInRenderTreeLayout());
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
std::atomic<bool> AXObjectCache::gAccessibilityTextStitchingEnabled = false;
std::atomic<bool> AXObjectCache::gForceInitialFrameCaching = false;
#if PLATFORM(COCOA)
std::atomic<bool> AXObjectCache::gAccessibilityDOMIdentifiersEnabled = false;
std::atomic<bool> AXObjectCache::gShouldRepostNotificationsForTests = false;
#endif

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

static constexpr Seconds updateTreeSnapshotTimerInterval { 100_ms };
#endif

AXObjectCache::AXObjectCache(LocalFrame& localFrame, Document* document)
    : m_document(document)
    , m_frameID(localFrame.frameID())
    , m_notificationPostTimer(*this, &AXObjectCache::notificationPostTimerFired)
#if PLATFORM(COCOA)
    , m_passwordNotificationTimer(*this, &AXObjectCache::passwordNotificationTimerFired)
#endif
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
    if (m_frameID)
        AXLOG(makeString("frameID "_s, m_frameID->loggingString()));
    else
        AXLOG("No frameID.");
#endif
    ASSERT(isMainThread());

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    setAccessibilityLogChannelEnabled(LOG_CHANNEL(Accessibility).state != logChannelStateOff);
#endif

#if ENABLE(AX_THREAD_TEXT_APIS)
    gAccessibilityThreadTextApisEnabled = DeprecatedGlobalSettings::accessibilityThreadTextApisEnabled();
#endif
    gAccessibilityTextStitchingEnabled = DeprecatedGlobalSettings::accessibilityTextStitchingEnabled();

#if PLATFORM(COCOA)
    initializeUserDefaultValues();
#endif

    // If loading completed before the cache was created, loading progress will have been reset to zero.
    // Consider loading progress to be 100% in this case.
    Page* page = localFrame.page();
    if (page) {
        m_loadingProgress = page->progress().estimatedProgress();
        m_pageActivityState = page->activityState();
    }

    if (m_loadingProgress <= 0)
        m_loadingProgress = 1;

#if PLATFORM(COCOA)
    if (RefPtr document = m_document.get()) {
        if (document->settings().isAriaLiveRegionManagementEnabled())
            m_liveRegionManager = makeUnique<AXLiveRegionManager>(*this);
    }
#endif

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

    if (m_frameID) {
        if (auto tree = AXIsolatedTree::treeForFrameID(*m_frameID))
            tree->setPageActivityState({ });
        AXIsolatedTree::removeTreeForFrameID(*m_frameID);
    }
#endif

    AXTreeStore::remove(m_id);
}

String AXObjectCache::debugDescription() const
{
    TextStream stream;
    stream << this;
    return makeString(
        "AXObjectCache "_s,
        stream.release(),
        " { "_s,
        m_document ? m_document->debugDescription() : "null document"_s,
        " }"_s
    );
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
    RefPtr document = this->document();
    for (RefPtr element = document ? ElementTraversal::firstWithin(document->rootNode()) : nullptr; element; element = ElementTraversal::nextIncludingPseudo(*element)) {
        if (isModalElement(*element))
            m_modalElements.append(element.get());
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
        for (RefPtr node = nodeStack.takeLast(); node; node = node->nextSibling()) {
            if (RefPtr axObject = getOrCreate(*node)) {
                if (!axObject->computeIsIgnored())
                    return true;

#if USE(ATSPI)
                // When using ATSPI, an accessibility object with 'StaticText' role is ignored.
                // Its content is exposed by its parent.
                // Treat such elements as having accessible content.
                // FIXME: This may not be sufficient for visibility:hidden or inert (https://bugs.webkit.org/show_bug.cgi?id=280914).
                if (axObject->role() == AccessibilityRole::StaticText && !axObject->isAXHidden())
                    return true;
#endif
            }

            // Don't descend into subtrees for non-visible nodes.
            if (isNodeVisible(node.get()))
                nodeStack.append(node->firstChild());
        }
    }
    return false;
}

void AXObjectCache::updateCurrentModalNode()
{
    auto recomputeModalElement = [&] () -> Element* {
        // There might be multiple modal dialog nodes.
        // We use this function to pick the one we want.
        if (m_modalElements.isEmpty())
            return nullptr;

        RefPtr document = this->document();
        if (!document)
            return nullptr;

        // Pick the document active modal <dialog> element if it exists.
        if (RefPtr activeModalDialog = document->activeModalDialog()) {
            ASSERT(m_modalElements.contains(activeModalDialog.get()));
            return activeModalDialog.unsafeGet();
        }

        SetForScope retrievingCurrentModalNode(m_isRetrievingCurrentModalNode, true);
        // If any of the modal nodes contains the keyboard focus, we want to pick that one.
        // If multiple contain the keyboard focus, we want the deepest.
        // If no modal contains focus, we want to pick the last visible dialog in the DOM.
        RefPtr<Element> focusedElement = document->focusedElement();
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

            bool focusIsInsideElement = focusedElement && focusedElement->isInclusiveDescendantOf(*element);

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

        if (!focusedElement || !foundModalWithFocusInside)
            return nullptr;

        RefPtr object = getOrCreate(modalElementToReturn.get());
        if (!object || object->isAXHidden())
            return nullptr;
        return modalElementToReturn.unsafeGet();
    };

    RefPtr previousModal = m_currentModalElement.get();
    m_currentModalElement = recomputeModalElement();
    if (previousModal.get() != m_currentModalElement.get()) {
        childrenChanged(rootWebArea());
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        // Because the presence of a modal affects every element on the page,
        // regenerate the entire isolated tree with the next cache update.
        m_deferredRegenerateIsolatedTree = true;
#endif
    }
}

bool AXObjectCache::isNodeVisible(const Node* node) const
{
    RefPtr element = dynamicDowncast<Element>(node);
    if (!element)
        return false;

    auto* renderer = element->renderer();
    if (!renderer)
        return false;

    const auto& style = renderer->style();
    if (style.display() == DisplayType::None)
        return false;

    auto* renderLayer = renderer->enclosingLayer();
    if (isVisibilityHidden(style) && renderLayer && !renderLayer->hasVisibleContent())
        return false;

    // Check whether this object or any of its ancestors has opacity 0.
    // The resulting opacity of a RenderObject is computed as the multiplication
    // of its opacity times the opacities of its ancestors.
    for (auto* ancestor = renderer; ancestor; ancestor = ancestor->parent()) {
        if (ancestor->style().opacity().isTransparent())
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

    RefPtr axRenderImage = areaElement.protectedDocument()->axObjectCache()->getOrCreate(*imageElement);
    if (!axRenderImage)
        return nullptr;

    for (const auto& child : axRenderImage->unignoredChildren()) {
        if (child->isImageMapLink() && child->node() == &areaElement)
            return dynamicDowncast<AccessibilityObject>(child.get());
    }
    return nullptr;
}

AccessibilityObject* AXObjectCache::focusedObjectForPage(const Page* page)
{
#if ENABLE_ACCESSIBILITY_LOCAL_FRAME
    return focusedObjectForLocalFrame();
#endif

    ASSERT(isMainThread());

    if (!gAccessibilityEnabled)
        return nullptr;

    // get the focused node in the page
    RefPtr focusedOrMainFrame = page->focusController().focusedOrMainFrame();
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

AccessibilityObject* AXObjectCache::focusedObjectForLocalFrame()
{
    ASSERT(isMainThread());

    if (!gAccessibilityEnabled)
        return nullptr;

    RefPtr document = this->document();
    if (!document)
        return nullptr;

#if ENABLE_ACCESSIBILITY_LOCAL_FRAME
    // If there's one AXObjectCache per frame, then we should return null if focus is in a different frame.
    RefPtr page = document->page();
    RefPtr focusedOrMainFrame = page ? page->focusController().focusedOrMainFrame() : nullptr;
    if (!focusedOrMainFrame)
        return nullptr;
    if (focusedOrMainFrame->document() != document.get())
        return nullptr;
#endif

    document->updateStyleIfNeeded();
    if (RefPtr focusedElement = document->focusedElement())
        return focusedObjectForNode(focusedElement.get());
    return focusedObjectForNode(document.get());
}

AccessibilityObject* AXObjectCache::focusedObjectForNode(Node* focusedNode)
{
    if (auto* area = dynamicDowncast<HTMLAreaElement>(focusedNode))
        return focusedImageMapUIElement(*area);

    RefPtr focus = getOrCreate(focusedNode);
    if (!focus)
        return nullptr;

    if (focus->shouldFocusActiveDescendant()) {
        if (RefPtr descendant = focus->activeDescendant())
            return dynamicDowncast<AccessibilityObject>(descendant.get());
    }

    if (focus->isIgnored())
        return focus->parentObjectUnignored();
    return focus.unsafeGet();
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::setIsolatedTreeFocusedObject(AccessibilityObject* focus)
{
    ASSERT(isMainThread());

    if (auto tree = AXIsolatedTree::treeForFrameID(m_frameID))
        tree->setFocusedNodeID(focus ? std::optional { focus->objectID() } : std::nullopt);
}
#endif


Ref<AccessibilityRenderObject> AXObjectCache::createObjectFromRenderer(RenderObject& renderer)
{
    RefPtr node = renderer.node();
    if (RefPtr element = dynamicDowncast<Element>(node)) {
        // Lists shouldn't fallthrough to table components, so explicitly create a render object.
        if (AXListHelpers::isAccessibilityList(*element))
            return AccessibilityRenderObject::create(AXID::generate(), renderer, *this);
    }

    if (renderer.isRenderOrLegacyRenderSVGRoot())
        return AccessibilitySVGObject::create(AXID::generate(), renderer, *this, /* isSVGRoot */ true);

    if (is<SVGElement>(node) || is<RenderSVGInlineText>(renderer))
        return AccessibilitySVGObject::create(AXID::generate(), renderer, *this);

    if (CheckedPtr renderImage = toSimpleImage(renderer))
        return AccessibilityRenderObject::create(AXID::generate(), *renderImage, *this);

#if ENABLE(MATHML)
    // The mfenced element creates anonymous RenderMathMLOperators which should be treated
    // as MathML elements and assigned the MathElementRole so that platform logic regarding
    // inclusion and role mapping is not bypassed.
    bool isAnonymousOperator = renderer.isAnonymous() && is<RenderMathMLOperator>(renderer);
    if (isAnonymousOperator || is<MathMLElement>(node))
        return AccessibilityMathMLElement::create(AXID::generate(), renderer, *this, isAnonymousOperator);
#endif

    if (CheckedPtr renderMenuList = dynamicDowncast<RenderMenuList>(renderer))
        return AccessibilityMenuList::create(AXID::generate(), *renderMenuList, *this);

    // Progress indicator.
    if (is<RenderProgress>(renderer) || is<RenderMeter>(renderer)
        || is<HTMLProgressElement>(node) || is<HTMLMeterElement>(node))
        return AccessibilityProgressIndicator::create(AXID::generate(), renderer, *this);

    // input type=range
    if (is<RenderSlider>(renderer))
        return AccessibilitySlider::create(AXID::generate(), renderer, *this);

    return AccessibilityRenderObject::create(AXID::generate(), renderer, *this);
}

Ref<AccessibilityNodeObject> AXObjectCache::createFromNode(Node& node)
{
    if (RefPtr element = dynamicDowncast<Element>(node)) {
        // Lists shouldn't fallthrough to table components, so explicitly create a render object.
        if (AXListHelpers::isAccessibilityList(*element))
            return AccessibilityRenderObject::create(AXID::generate(), *element, *this);
        if (RefPtr areaElement = dynamicDowncast<HTMLAreaElement>(*element))
            return AccessibilityNodeObject::create(AXID::generate(), areaElement.get(), *this);
        if (is<HTMLProgressElement>(*element) || is<HTMLMeterElement>(*element))
            return AccessibilityProgressIndicator::create(AXID::generate(), *element, *this);
        if (is<SVGElement>(*element))
            return AccessibilitySVGObject::create(AXID::generate(), *element, *this);
    }
    return AccessibilityRenderObject::create(AXID::generate(), node, *this);
}

void AXObjectCache::cacheAndInitializeWrapper(AccessibilityObject& newObject, DOMObjectVariant domObject)
{
    AXID axID = newObject.objectID();

    WTF::switchOn(domObject,
        [&] (RenderObject* typedValue) {
            CheckedPtr node = typedValue->node();
            if (!node)
                m_renderObjectIdMapping.set(*typedValue, axID);
            else {
                m_nodeIdMapping.set(*node, axID);
                m_nodeObjectMapping.set(*node, newObject);
            }
        },
        [&] (Node* typedValue) {
            m_nodeIdMapping.set(*typedValue, axID);
            m_nodeObjectMapping.set(*typedValue, newObject);
        },
        [&] (Widget* typedValue) { m_widgetIdMapping.set(*typedValue, axID); },
        [] (auto&) { }
    );

    m_objects.set(axID, newObject);
    newObject.init();
    attachWrapper(newObject);
}

AccessibilityObject* AXObjectCache::exportedGetOrCreate(Node* node)
{
    return node ? exportedGetOrCreate(*node) : nullptr;
}

AccessibilityObject* AXObjectCache::exportedGetOrCreate(Node& node)
{
    return getOrCreate(node, IsPartOfRelation::No);
}

AccessibilityObject* AXObjectCache::getOrCreate(Widget& widget)
{
    if (RefPtr object = get(widget))
        return object.unsafeGet();

    RefPtr<AccessibilityObject> newObject;
    if (auto* scrollView = dynamicDowncast<ScrollView>(widget))
        newObject = AccessibilityScrollView::create(AXID::generate(), *scrollView, *this);
    else if (auto* scrollbar = dynamicDowncast<Scrollbar>(widget))
        newObject = AccessibilityScrollbar::create(AXID::generate(), *scrollbar, *this);

    // Will crash later if we have two objects for the same widget.
    ASSERT(!get(widget));

    // Ensure we weren't given an unsupported widget type.
    ASSERT(newObject);
    if (!newObject)
        return nullptr;

    cacheAndInitializeWrapper(*newObject, &widget);
    return newObject.unsafeGet();
}

AccessibilityObject* AXObjectCache::getOrCreateSlow(Node& node, IsPartOfRelation isPartOfRelation)
{
    // `get` for this Node should've been attempted before calling this method.
    ASSERT(!get(node));

#if ENABLE_ACCESSIBILITY_LOCAL_FRAME
    ASSERT(&node.document() == document());
#endif

    bool isYouTubeReplacement = false;
    if (CheckedPtr renderer = nodeRendererIsValid(node) ? node.renderer() : nullptr) {
        if (!renderer->isYouTubeReplacement()) [[likely]]
            return getOrCreate(*renderer);
        isYouTubeReplacement = true;
    }

    if (CheckedPtr document = dynamicDowncast<Document>(node)) [[unlikely]]
        return getOrCreate(document->renderView());

    RefPtr composedParent = node.parentElementInComposedTree();
    if (!composedParent)
        return nullptr;

    Ref protectedNode { node };

    RefPtr optionElement = dynamicDowncast<HTMLOptionElement>(node);
    RefPtr optGroupElement = dynamicDowncast<HTMLOptGroupElement>(node);
    if (optionElement || optGroupElement) {
        RefPtr select = optionElement
            ? optionElement->ownerSelectElement()
            : optGroupElement->ownerSelectElement();
        if (!select)
            return nullptr;
        RefPtr<AccessibilityObject> object;
        if (select->usesMenuList()) {
            if (!optionElement || !select->renderer())
                return nullptr;
            object = AccessibilityMenuListOption::create(AXID::generate(), *optionElement, *this);
        } else
            object = AccessibilityListBoxOption::create(AXID::generate(), downcast<HTMLElement>(node), *this);
        cacheAndInitializeWrapper(*object, &node);
        return object.unsafeGet();
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
        RefPtr element = dynamicDowncast<Element>(node);
        bool hasDisplayContents = element && element->hasDisplayContents();
        bool isPopover = element && element->hasAttributeWithoutSynchronization(popoverAttr);
        bool isAreaElement = is<HTMLAreaElement>(element);
        // YouTube embeds specifically create a render hierarchy with two elements that share a renderer.
        // In this instance, we want the <embed> element to associate its node with an AX element, so we need to create one here.
        bool replacementWillCreateRenderer = isYouTubeReplacement;
        if (!inCanvasSubtree && !insideMeterElement && !hasDisplayContents && !isPopover && !isNodeFocused(node) && !isAreaElement && !replacementWillCreateRenderer)
            return nullptr;
    }

    // The object may have already been created during relations update.
    if (RefPtr object = get(node))
        return object.unsafeGet();

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
    return newObject.unsafeGet();
}

AccessibilityObject* AXObjectCache::getOrCreate(RenderObject& renderer)
{
    if (renderer.isYouTubeReplacement()) [[unlikely]]
        return getOrCreate(renderer.node());

    if (RefPtr object = get(renderer))
        return object.unsafeGet();

    // Don't create an object for this renderer if it's being destroyed.
    if (renderer.beingDestroyed())
        return nullptr;

    // We should never create objects that have dirty layout. Doing so can cause
    // incorrect accessibility tree updates and also for renderers to be deleted
    // out from under us, causing memory safety issues (or CheckedPtr crashes if we're lucky).
    AX_BROKEN_ASSERT(!renderer.needsLayout());

    Ref object = createObjectFromRenderer(renderer);

    // Will crash later if we have two objects for the same renderer.
    AX_BROKEN_ASSERT(!get(renderer));

    cacheAndInitializeWrapper(object.get(), &renderer);
    // Compute the object's initial ignored status.
    object->recomputeIsIgnored();
    // Sometimes asking isIgnored() will cause the newObject to be deallocated, and then
    // it will disappear when this function is finished, leading to a use-after-free.
    if (object->isDetached())
        return nullptr;

    return object.unsafePtr();
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
RefPtr<AXIsolatedTree> AXObjectCache::getOrCreateIsolatedTree()
{
    AXTRACE(makeString("AXObjectCache::getOrCreateIsolatedTree 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    ASSERT(isMainThread());

    if (!m_frameID)
        return nullptr;

    RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID);
    if (tree) {
        if (tree->treeID() == treeID())
            return tree;

        // The tree belongs to a different document (navigation occurred). Remove the old tree and create a new one.
        AXIsolatedTree::removeTreeForFrameID(*m_frameID);
        tree = nullptr;
    }

    // A new isolated tree needs to be created. Initialize the GeometryManager primary screen rect to be ready when needed.
    m_geometryManager->initializePrimaryScreenRect();
    // Schedule a paint to cache the rects for the objects in this new isolated tree.
    scheduleObjectRegionsUpdate(true /* scheduleImmediately */);

    // This method can be called as the result of a client request. Since creating the isolated tree can take long,
    // especially for large documents, for real clients we build a temporary "empty" isolated tree consisting only of the ScrollView and the WebArea objects.
    // Then we schedule building the entire isolated tree on a Timer.
    // For test clients, LayoutTests or XCTests, build the whole isolated tree.
    if (!clientIsInTestMode()) [[likely]] {
        tree = AXIsolatedTree::createEmpty(*this);
        if (!m_buildIsolatedTreeTimer.isActive())
            m_buildIsolatedTreeTimer.startOneShot(0_s);
    } else
        tree = AXIsolatedTree::create(*this);

    initializeAXThreadIfNeeded();

    return tree;
}

void AXObjectCache::buildIsolatedTree()
{
    m_buildIsolatedTreeTimer.stop();

    if (!m_frameID)
        return;

    auto tree = AXIsolatedTree::create(*this);
    if (RefPtr webArea = rootWebArea()) {
        postPlatformNotification(*webArea, AXNotification::LoadComplete);
        postPlatformNotification(*webArea, AXNotification::FocusedUIElementChanged);
    }
}

void AXObjectCache::setIsolatedTree(Ref<AXIsolatedTree> tree)
{
    ASSERT(isMainThread());
    if (RefPtr frame = m_document ? m_document->frame() : nullptr)
        frame->loader().client().setIsolatedTree(WTFMove(tree));
}
#endif

AXCoreObject* AXObjectCache::rootObjectForFrame(LocalFrame& frame)
{
    if (!gAccessibilityEnabled)
        return nullptr;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (isIsolatedTreeEnabled()) {
        RefPtr tree = getOrCreateIsolatedTree();
        if (!isMainThread()) {
            tree->applyPendingChanges();
            return tree->rootNode();
        }
    }
#endif

    return getOrCreate(frame.view());
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::buildIsolatedTreeIfNeeded()
{
    if (!gAccessibilityEnabled)
        return;

    if (isIsolatedTreeEnabled())
        getOrCreateIsolatedTree();
}
#endif

AccessibilityObject* AXObjectCache::create(AccessibilityRole role)
{
    RefPtr<AccessibilityObject> object;
    switch (role) {
    case AccessibilityRole::Column:
        object = AccessibilityTableColumn::create(AXID::generate(), *this);
        break;
    case AccessibilityRole::TableHeaderContainer:
        object = AccessibilityTableHeaderContainer::create(AXID::generate(), *this);
        break;
    case AccessibilityRole::RemoteFrame:
        object = AXRemoteFrame::create(AXID::generate(), *this);
        break;
    case AccessibilityRole::LocalFrame:
        object = AXLocalFrame::create(AXID::generate(), *this);
        break;
    case AccessibilityRole::SliderThumb:
        object = AccessibilitySliderThumb::create(AXID::generate(), *this);
        break;
    case AccessibilityRole::MenuListPopup:
        object = AccessibilityMenuListPopup::create(AXID::generate(), *this);
        break;
    case AccessibilityRole::SpinButton:
        object = AccessibilitySpinButton::create(AXID::generate(), *this);
        break;
    case AccessibilityRole::SpinButtonPart:
        object = AccessibilitySpinButtonPart::create(AXID::generate(), *this);
        break;
    default:
        break;
    }

    if (!object)
        return nullptr;

    cacheAndInitializeWrapper(*object);
    return object.unsafeGet();
}

void AXObjectCache::remove(AXID axID)
{
    AXTRACE(makeString("AXObjectCache::remove 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    AXLOG(makeString("AXID "_s, axID.loggingString()));

    RefPtr object = m_objects.take(axID);
    if (!object)
        return;

#if PLATFORM(COCOA)
    if (m_liveRegionManager)
        m_liveRegionManager->unregisterLiveRegion(axID);
#endif

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    unsigned liveRegionsRemoved = m_sortedLiveRegionIDs.removeAll(axID);
    unsigned webAreasRemoved = m_sortedNonRootWebAreaIDs.removeAll(axID);

    if (RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID)) {
        tree->queueNodeRemoval(*object);

        if (liveRegionsRemoved)
            tree->sortedLiveRegionsDidChange(m_sortedLiveRegionIDs);
        else if (webAreasRemoved)
            tree->sortedNonRootWebAreasDidChange(m_sortedNonRootWebAreaIDs);
    }
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

    removeAllRelations(axID);
    object->detach(AccessibilityDetachmentType::ElementDestroyed);

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    m_geometryManager->remove(axID);
#endif
}

void AXObjectCache::remove(RenderObject& renderer)
{
    AXTRACE(makeString("AXObjectCache::remove RenderObject* 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    ASSERT(!renderer.node() || !m_renderObjectIdMapping.contains(renderer));

    if (RefPtr node = renderer.node()) {
        // We only delete objects with nodes when their DOM node is destroyed, not their renderer.
        if (RefPtr existingObject = dynamicDowncast<AccessibilityRenderObject>(get(*node))) {
            if (existingObject->setRendererIfNeeded(nullptr)) {
                // We do need to update the object in response to losing its renderer, though.
                m_deferredRendererChangedList.add(*existingObject);
            }
        }
    } else
        remove(m_renderObjectIdMapping.takeOptional(renderer));
}

void AXObjectCache::remove(Node& node)
{
    AXTRACE(makeString("AXObjectCache::remove Node& 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));

    m_nodeObjectMapping.remove(node);
    remove(m_nodeIdMapping.take(node));

    if (auto* renderer = node.renderer())
        remove(*renderer);

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

void AXObjectCache::remove(Widget& view)
{
    remove(m_widgetIdMapping.takeOptional(view));
    if (auto* scrollView = dynamicDowncast<ScrollView>(view))
        m_deferredScrollbarUpdateChangeList.remove(*scrollView);
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
            postNotification(ancestor.get(), ancestor->protectedDocument().get(), AXNotification::ValueChanged);
            notifiedNonNativeTextControl = true;
        }

        if (isText) {
            bool dependsOnTextUnderElement = ancestor->dependsOnTextUnderElement();
            auto role = ancestor->role();
            dependsOnTextUnderElement |= role == AccessibilityRole::Label || role == AccessibilityRole::TextField;

            // If the starting object is a static text, its underlying text has changed.
            if (dependsOnTextUnderElement) {
                // Inform this ancestor its textUnderElement-dependent data is now out-of-date.
                postNotification(ancestor.get(), nullptr, AXNotification::TextUnderElementChanged);
            }

            // Any objects this ancestor labeled now also need new AccessibilityText.
            auto labeledObjects = ancestor->labelForObjects();
            for (const auto& labeledObject : labeledObjects)
                postNotification(&downcast<AccessibilityObject>(labeledObject.get()), nullptr, AXNotification::TextChanged);
        }
    }

    postNotification(object, object->protectedDocument().get(), AXNotification::TextChanged);
    object->recomputeIsIgnored();
}

void AXObjectCache::onRendererCreated(Node& node)
{
    CheckedPtr renderer = node.renderer();
    if (!renderer) [[unlikely]] {
        ASSERT_NOT_REACHED();
        return;
    }

    if (RefPtr existingObject = dynamicDowncast<AccessibilityRenderObject>(get(node))) {
        if (existingObject->setRendererIfNeeded(renderer.get()))
            m_deferredRendererChangedList.add(*existingObject);
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

void AXObjectCache::onDragElementChanged(Element* oldElement, Element* newElement)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (oldElement == newElement)
        return;

    if (oldElement)
        postNotification(get(*oldElement), AXNotification::GrabbedStateChanged);

    if (newElement)
        postNotification(get(*newElement), AXNotification::GrabbedStateChanged);
#else
    UNUSED_PARAM(oldElement);
    UNUSED_PARAM(newElement);
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
}

void AXObjectCache::onDraggingStarted(Element& dragTarget)
{
    postNotification(&dragTarget, AXNotification::DraggingStarted);
}

void AXObjectCache::onDraggingEnded(Element& dragTarget)
{
    postNotification(&dragTarget, AXNotification::DraggingEnded);
}

void AXObjectCache::onDraggingEnteredDropZone(Element& dragTarget)
{
    postNotification(&dragTarget, AXNotification::DraggingEnteredDropZone);
}

void AXObjectCache::onDraggingExitedDropZone(Element& dragTarget)
{
    postNotification(&dragTarget, AXNotification::DraggingExitedDropZone);
}

void AXObjectCache::onDraggingDropped(Element& dragTarget)
{
    postNotification(&dragTarget, AXNotification::DraggingDropped);
}

void AXObjectCache::onEventListenerAdded(Node& node, const AtomString& eventType)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isClickEvent(eventType))
        return;

    if (RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID)) {
        if (RefPtr object = get(node))
            tree->queueNodeUpdate(object->objectID(), { AXProperty::HasClickHandler });
    }
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

    if (RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID)) {
        if (RefPtr object = get(node))
            tree->queueNodeUpdate(object->objectID(), { AXProperty::HasClickHandler });
    }
#else
    UNUSED_PARAM(node);
    UNUSED_PARAM(eventType);
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
}

void AXObjectCache::updateLoadingProgress(double newProgressValue)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    ASSERT_WITH_MESSAGE(newProgressValue >= 0 && newProgressValue <= 1, "unexpected loading progress value: %f", newProgressValue);
    if (m_frameID) {
        // Sometimes the isolated tree hasn't been created by the time we get loading progress updates,
        // so cache this value in the AXObjectCache too so we can give it to the tree upon creation.
        m_loadingProgress = newProgressValue;
        if (auto tree = AXIsolatedTree::treeForFrameID(*m_frameID))
            tree->updateLoadingProgress(newProgressValue);
    }
#else
    UNUSED_PARAM(newProgressValue);
#endif
}

void AXObjectCache::onTopDocumentLoaded(RenderObject& renderObject)
{
    postNotification(getOrCreate(renderObject), AXNotification::NewDocumentLoadComplete);
}

void AXObjectCache::onNonTopDocumentLoaded(RenderObject& renderObject)
{
    // AXLoadComplete can only be posted on the top document, so if it's a document
    // in an iframe that just finished loading, post AXLayoutComplete instead.
    postNotification(getOrCreate(renderObject), AXNotification::LayoutComplete);
}

void AXObjectCache::onAutocorrectionOccured(Element& element)
{
    postNotification(&element, AXNotification::AutocorrectionOccured);
}

void AXObjectCache::onEditableTextValueChanged(Node& node)
{
    postNotification(&node, AXNotification::ValueChanged, PostTarget::ObservableParent);
}

void AXObjectCache::onDocumentInitialFocus(Node& node)
{
    postNotification(&node, AXNotification::FocusedUIElementChanged);
}

void AXObjectCache::onLayoutComplete(RenderObject& renderObject)
{
    postNotification(&renderObject, AXNotification::LayoutComplete);
}

void AXObjectCache::handleAllDeferredChildrenChanged()
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    RefPtr<AXIsolatedTree> tree;
    if (!m_deferredChildrenChangedList.isEmpty())
        tree = AXIsolatedTree::treeForFrameID(m_frameID);
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
            postPlatformNotification(object, AXNotification::ChildrenChanged);
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
        handleChildrenChanged(downcast<AccessibilityObject>(children[0].get()));
    } else if (auto* menuListPopup = dynamicDowncast<AccessibilityMenuListPopup>(object)) {
        menuListPopup->handleChildrenChanged();
        return;
    } else if (auto* nodeObject = dynamicDowncast<AccessibilityNodeObject>(object); nodeObject && nodeObject->isTable())
        deferRecomputeTableCellSlots(*nodeObject);
    else if (auto* parentTable = dynamicDowncast<AccessibilityNodeObject>(object.parentTableIfExposedTableRow()))
        deferRecomputeTableCellSlots(*parentTable);
    else if (auto* scrollView = dynamicDowncast<AccessibilityScrollView>(object)) {
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
    for (RefPtr parent = object; parent; parent = parent->parentObject()) {
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
            postNotification(parent.get(), parent->protectedDocument().get(), AXNotification::ValueChanged);

            // Do not let any ancestor of an editable object update its children.
            shouldUpdateParent = false;
        }

        if (parent->isLabel()) {
            // A label's descendant was added or removed. Update its LabelFor relationships.
            handleLabelChanged(parent.get());
        }

        for (const auto& describedObject : parent->descriptionForObjects())
            postNotification(&downcast<AccessibilityObject>(describedObject.get()), nullptr, AXNotification::ExtendedDescriptionChanged);

        if (parent->hasElementName(ElementName::HTML_caption))
            foundTableCaption = true;
        else if (foundTableCaption && parent->isTable()) {
            postNotification(parent.get(), nullptr, AXNotification::TextChanged);
            foundTableCaption = false;
        }
    }

    // The role of list objects is dependent on their children, so we'll need to re-compute it here.
    if (object.isAccessibilityList())
        object.updateRole();
}

void AXObjectCache::handleRecomputeCellSlots(AccessibilityNodeObject& axTable)
{
    axTable.setCellSlotsDirty();
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(axTable, AXNotification::CellSlotsChanged);
#endif
}

void AXObjectCache::onRemoteFrameInitialized(AXRemoteFrame& remoteFrame)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(remoteFrame, AXProperty::RemoteFramePlatformElement);
#else
    UNUSED_PARAM(remoteFrame);
#endif
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::handleRowspanChanged(AccessibilityNodeObject& axCell)
{
    updateIsolatedTree(axCell, AXNotification::RowSpanChanged);
}
#endif

const Vector<Vector<AXID>>* AXObjectCache::stitchGroupsOwnedBy(AccessibilityObject& object)
{
    CheckedPtr renderBlockFlow = dynamicDowncast<RenderBlockFlow>(object.renderer());
    if (!renderBlockFlow || renderBlockFlow->beingDestroyed())
        return nullptr;

    const auto& groups = m_stitchGroups.ensure(*renderBlockFlow, [&] {
        return object.stitchGroups();
    }).iterator->value;

    return groups.isEmpty() ? nullptr : &groups;
}

void AXObjectCache::setDirtyStitchGroups(const RenderBlock& renderBlock)
{
    if (std::optional groups = m_stitchGroups.takeOptional(renderBlock)) {
        UNUSED_PARAM(groups);

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        if (std::optional axID = getAXID(const_cast<RenderBlock&>(renderBlock))) {
            if (RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID))
                tree->queueNodeUpdate(*axID, { AXProperty::StitchGroups });
        }
#endif
    }
}

#if ENABLE(AX_THREAD_TEXT_APIS)
void AXObjectCache::onTextRunsChanged(const RenderObject& renderer)
{
    if (is<RenderInline>(renderer) || is<RenderListMarker>(renderer)) {
        // Fast-path exit for common renderers that will never produce text runs.
        return;
    }

    if (std::optional axID = getAXID(const_cast<RenderObject&>(renderer))) {
        if (RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID))
            tree->queueNodeUpdate(*axID, { AXProperty::TextRuns });
    }
}
#endif

void AXObjectCache::handleMenuOpened(Element& element)
{
    if (!element.renderer() || !hasRole(element, "menu"_s)) {
        // FIXME: Doesn't this early-return mean we are going to handle display:contents menus incorrectly?
        return;
    }

    postNotification(getOrCreate(element), protectedDocument().get(), AXNotification::MenuOpened);
}

void AXObjectCache::handleLiveRegionCreated(Element& element)
{
    if (!element.renderer()) {
        // FIXME: Doesn't this early-return mean we are going to handle display:contents live regions incorrectly?
        return;
    }

    auto liveRegionStatus = element.attributeWithoutSynchronization(aria_liveAttr);
    if (liveRegionStatus.isEmpty()) {
        const AtomString& ariaRole = element.attributeWithoutSynchronization(roleAttr);
        if (!ariaRole.isEmpty())
            liveRegionStatus = AtomString { AXCoreObject::defaultLiveRegionStatusForRole(AccessibilityObject::ariaRoleToWebCoreRole(ariaRole)) };
    }

    if (AXCoreObject::liveRegionStatusIsEnabled(liveRegionStatus)) {
        RefPtr axObject = getOrCreate(element);
        if (!axObject)
            return;

#if PLATFORM(COCOA)
        if (m_liveRegionManager) {
            m_liveRegionManager->registerLiveRegion(*axObject, true);
            return;
        }
#endif

#if PLATFORM(MAC)
        deferSortForNewLiveRegion(*axObject);
#endif // PLATFORM(MAC)

#if PLATFORM(COCOA)
        if (!m_liveRegionManager)
            postNotification(axObject.get(), protectedDocument().get(), AXNotification::LiveRegionCreated);
#endif
    }
}

#if PLATFORM(COCOA)
void AXObjectCache::initializeLiveRegionManager()
{
    if (!m_liveRegionManager || m_liveRegionManagerInitialized)
        return;

    m_liveRegionManagerInitialized = true;

    RefPtr current = rootWebArea();
    while ((current = current ? downcast<AccessibilityObject>(current->nextInPreOrder()) : nullptr)) {
        if (current->supportsLiveRegion())
            m_liveRegionManager->registerLiveRegion(*current);
    }
}
#endif

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

AccessibilityObject* AXObjectCache::getIncludingAncestors(RenderObject& renderer) const
{
    for (CheckedPtr current = &renderer; current; current = current->parent()) {
        if (RefPtr object = get(*current))
            return object.unsafeGet();
    }
    return nullptr;
}

void AXObjectCache::childrenChanged(RenderObject& renderer, RenderObject* changedChild)
{
    if (renderer.isAnonymous()) {
        // Don't drop a children-changed event if we can't |get| the given renderer if the renderer is anonymous.
        // Sometimes the only children-changed we get from the render tree for some subtrees is for an anonymous renderer,
        // so if we just drop it, we can have a stale accessibility tree. This problem is specific to anonymous renderers
        // because we walk the DOM when building the accessibility tree, so it's unlikely that we will have created
        // an accessibility object for this renderer (since it only exists in the render tree).
        //
        // Children-changed events associated with a DOM node are safe to drop if |get| fails, since some other
        // element that does have an accessibility object must also get a children-changed event. The same is
        // not always true for anonymous renderers, hence this branch.
        //
        // This behavior comes from a bug on a real webpage that I unfortunately couldn't figure out how to distill
        // into a layout test. The key seems to be anonymous continuation renderers destroyed and recreated as part
        // of a call to Node::insertBefore(). dynamic-inline-continuation.html gets close to reproducing the issue,
        // following many of the same codepaths, but unfortunately will still pass even without this branch.
        childrenChanged(getIncludingAncestors(renderer));
    } else
        childrenChanged(get(renderer));

    if (changedChild)
        deferElementAddedOrRemoved(dynamicDowncast<Element>(changedChild->node()));
}

void AXObjectCache::childrenChanged(AccessibilityObject* object)
{
    if (!object)
        return;
    m_deferredChildrenChangedList.add(*object);

    // Adding or removing rows from a table can cause it to change from layout table to AX data table and vice versa, so queue up recomputation of that for the parent table.
    if (RefPtr tableElement = dynamicDowncast<HTMLTableElement>(object->element()))
        deferRecomputeTableIsExposed(const_cast<HTMLTableElement*>(tableElement.get()));
    else if (RefPtr tableSectionElement = dynamicDowncast<HTMLTableSectionElement>(object->element()))
        deferRecomputeTableIsExposed(const_cast<HTMLTableElement*>(tableSectionElement->findParentTable().get()));

    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}

void AXObjectCache::valueChanged(Element& element)
{
    postNotification(&element, AXNotification::ValueChanged);
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::columnIndexChanged(AccessibilityObject& object)
{
    postNotification(object, AXNotification::ColumnIndexChanged);
}

void AXObjectCache::rowIndexChanged(AccessibilityObject& object)
{
    postNotification(object, AXNotification::RowIndexChanged);
}
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

void AXObjectCache::notificationPostTimerFired()
{
    AXTRACE(makeString("AXObjectCache::notificationPostTimerFired 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    // During LayoutTests, accessibility may be disabled between the time the notifications are queued and the timer fires.
    // Thus check here and return if accessibility is disabled.
    if (!accessibilityEnabled())
        return;

    RefPtr document = m_document.get();
    m_notificationPostTimer.stop();

    if (!document || !document->hasLivingRenderTree())
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

        if (note.second == AXNotification::MenuOpened) {
            // Only notify if the object is in fact a menu.
            note.first->updateChildrenIfNecessary();
            if (note.first->role() != AccessibilityRole::Menu)
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

#if PLATFORM(COCOA)
void AXObjectCache::setShouldRepostNotificationsForTests(bool value)
{
    gShouldRepostNotificationsForTests = value;
}
#endif

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
    RefPtr axNode = node;
    if (!axNode)
        return;

    stopCachingComputedObjectAttributes();

    // Get an accessibility object that already exists. One should not be created here
    // because a render update may be in progress and creating an AX object can re-trigger a layout
    RefPtr object = get(*axNode);
    while (!object && axNode) {
        axNode = axNode->parentNode();
        object = get(axNode.get());
    }

    if (!axNode)
        return;

    postNotification(object.get(), axNode->protectedDocument().ptr(), notification, postTarget);
}

void AXObjectCache::postNotification(AccessibilityObject* object, Document* document, AXNotification notification, PostTarget postTarget)
{
    AXTRACE(makeString("AXObjectCache::postNotification 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    AXLOG(std::make_pair(object, notification));
    ASSERT(isMainThread());

    stopCachingComputedObjectAttributes();

    RefPtr axObject = object;
    if (axObject && postTarget == PostTarget::ObservableParent)
        axObject = axObject->observableObject();

    if (!axObject && document)
        axObject = get(document->renderView());

    if (!axObject)
        return;

#if PLATFORM(COCOA)
    if (notification == AXNotification::ValueChanged
        && enqueuePasswordNotification(*axObject, { }))
        return;
#endif

    m_notificationsToPost.append(std::make_pair(axObject.releaseNonNull(), notification));
    if (!m_notificationPostTimer.isActive())
        m_notificationPostTimer.startOneShot(0_s);
}

void AXObjectCache::postNotification(AccessibilityObject& object, AXNotification notification)
{
    AXTRACE(makeString("AXObjectCache::postNotification 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    AXLOG(std::make_pair(Ref { object }, notification));
    ASSERT(isMainThread());

    stopCachingComputedObjectAttributes();

#if PLATFORM(COCOA)
    if (notification == AXNotification::ValueChanged
        && enqueuePasswordNotification(object, { }))
        return;
#endif

    m_notificationsToPost.append(std::make_pair(Ref { object }, notification));
    if (!m_notificationPostTimer.isActive())
        m_notificationPostTimer.startOneShot(0_s);
}

void AXObjectCache::postARIANotifyNotification(Node& node, const String& announcement, const AriaNotifyOptions& options)
{
    RefPtr object = getOrCreate(node);
    if (!object)
        return;

    auto priority = NotifyPriority::Normal;
    if (options.priority) {
        switch (*options.priority) {
        case AriaNotifyOptions::NotificationPriority::Normal:
            priority = NotifyPriority::Normal;
            break;
        case AriaNotifyOptions::NotificationPriority::High:
            priority = NotifyPriority::High;
            break;
        }
    }

    auto interruptBehavior = InterruptBehavior::None;
    if (options.interrupt) {
        switch (*options.interrupt) {
        case AriaNotifyOptions::NotificationInterrupt::None:
            interruptBehavior = InterruptBehavior::None;
            break;
        case AriaNotifyOptions::NotificationInterrupt::All:
            interruptBehavior = InterruptBehavior::All;
            break;
        case AriaNotifyOptions::NotificationInterrupt::Pending:
            interruptBehavior = InterruptBehavior::Pending;
            break;
        }
    }

    postPlatformARIANotifyNotification(announcement, priority, interruptBehavior, object->languageIncludingAncestors());
}

void AXObjectCache::postLiveRegionNotification(AccessibilityObject& object, LiveRegionStatus status, const AttributedString& announcement)
{
    postPlatformLiveRegionNotification(object, status, announcement);
}

void AXObjectCache::checkedStateChanged(Element& element)
{
    postNotification(&element, AXNotification::CheckedStateChanged);
}

void AXObjectCache::autofillTypeChanged(HTMLInputElement& element)
{
    postNotification(&element, AXNotification::AutofillTypeChanged);
}

void AXObjectCache::handleMenuItemSelected(Element* element)
{
    if (!element)
        return;

    if (!hasAnyRole(*element, { "menuitem"_s, "menuitemradio"_s, "menuitemcheckbox"_s }))
        return;

    if (!element->focused() && !equalLettersIgnoringASCIICase(element->attributeWithoutSynchronization(aria_selectedAttr), "true"_s))
        return;

    postNotification(getOrCreate(*element), protectedDocument().get(), AXNotification::MenuListItemSelected);
}

void AXObjectCache::handleTabPanelSelected(Element* oldElement, Element* newElement)
{
    auto updateTab = [this] (AccessibilityObject* controlPanel, Element& element) {
        if (!controlPanel)
            return;

        auto controllers = controlPanel->controllers();
        for (auto& controller : controllers)
            postNotification(dynamicDowncast<AccessibilityObject>(controller.get()), element.protectedDocument().ptr(), AXNotification::SelectedStateChanged);
    };


    RefPtr oldObject = get(oldElement);
    RefPtr<AccessibilityObject> oldFocusedControlledPanel;
    if (oldObject) {
        oldFocusedControlledPanel = Accessibility::findAncestor<AccessibilityObject>(*oldObject, false, [] (auto& ancestor) {
            return ancestor.role() == AccessibilityRole::TabPanel;
        });

        updateTab(oldFocusedControlledPanel.get(), *oldElement);
    }

    RefPtr newObject = get(newElement);
    if (!newObject)
        return;

    RefPtr newFocusedControlledPanel = Accessibility::findAncestor<AccessibilityObject>(*newObject, false, [] (auto& ancestor) {
        return ancestor.role() == AccessibilityRole::TabPanel;
    });

    if (oldFocusedControlledPanel != newFocusedControlledPanel)
        updateTab(newFocusedControlledPanel.get(), *newElement);
}

void AXObjectCache::handleRowCountChanged(AccessibilityObject* axObject, Document* document)
{
    if (!axObject)
        return;

    axObject->recomputeIsExposableIfNecessary();
    postNotification(axObject, document, AXNotification::RowCountChanged);
}

void AXObjectCache::onPageActivityStateChange(OptionSet<ActivityState> newState)
{
    ASSERT(m_frameID);

    m_pageActivityState = newState;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (auto tree = AXIsolatedTree::treeForFrameID(m_frameID))
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

#if PLATFORM(IOS_FAMILY)
    // Date/DateTimeLocal input fields present popovers in the UI process synchronously.
    // The web process can infer that this will happen via a DOMActivateEvent, but this may be dispatched after the focused change event.
    // Defer the focus until after BaseDateAndTimeInputType::showPicker is called and has set the appropriate state (willPresentDatePopover) on AXObjectCache.
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(element)) {
        if (input->isDateField() || input->isDateTimeLocalField())
            return true;
    }
#endif // PLATFORM(IOS_FAMILY)

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
        return;
    // Both of these change the is-ignored state of all descendants of `renderer`, so throw away
    // the is-ignored cache.
    stopCachingComputedObjectAttributes();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    postNotification(*axObject, AXNotification::InertOrVisibilityChanged);
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

#else // !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    if (CheckedPtr parent = renderer.parent())
        childrenChanged(*parent, &renderer);
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
}

void AXObjectCache::onPopoverToggle(const HTMLElement& popover)
{
    RefPtr axPopover = get(const_cast<HTMLElement*>(&popover));
    if (!axPopover)
        return;
    // There may be multiple elements with popovertarget attributes that point at |popover|.
    for (const auto& invoker : axPopover->controllers())
        postNotification(dynamicDowncast<AccessibilityObject>(invoker.get()), protectedDocument().get(), AXNotification::ExpandedChanged);
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

    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}

void AXObjectCache::handleFocusedUIElementChanged(Element* oldElement, Element* newElement, UpdateModal updateModal)
{
#if PLATFORM(IOS_FAMILY)
    if (willPresentDatePopover()) {
        setWillPresentDatePopover(false);
        if (RefPtr inputElement = dynamicDowncast<HTMLInputElement>(newElement)) {
            if (inputElement->isDateField() || inputElement->isDateTimeLocalField())
                return;
        }
    }
#endif

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
    postNotification(node, AXNotification::SelectedChildrenChanged);
}

void AXObjectCache::selectedChildrenChanged(RenderObject* renderer)
{
    if (renderer)
        selectedChildrenChanged(renderer->node());
}

void AXObjectCache::onScrollbarFrameRectChange(const Scrollbar& scrollbar)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!m_frameID || !isIsolatedTreeEnabled())
        return;

    if (RefPtr axScrollbar = get(const_cast<Scrollbar*>(&scrollbar)))
        std::ignore = m_geometryManager->cacheRectIfNeeded(axScrollbar->objectID(), enclosingIntRect(axScrollbar->relativeFrame()));
#else
    UNUSED_PARAM(scrollbar);
#endif
}

void AXObjectCache::onSelectedOptionChanged(Element& element)
{
    if (hasCellARIARole(element))
        postNotification(&element, AXNotification::SelectedCellsChanged);
    else if (is<HTMLOptionElement>(element))
        postNotification(&element, AXNotification::SelectedStateChanged);
    else if (RefPtr axObject = getOrCreate(element)) {
        if (RefPtr ancestor = Accessibility::findAncestor<AccessibilityObject>(*axObject, false, [] (const auto& object) {
            return object.canHaveSelectedChildren();
        })) {
            selectedChildrenChanged(ancestor->node());
            postNotification(axObject.get(), element.protectedDocument().ptr(), AXNotification::SelectedStateChanged);
        }
    }

    handleMenuItemSelected(&element);
    handleTabPanelSelected(nullptr, &element);
}

void AXObjectCache::onSelectedOptionChanged(RenderObject& menuList, int optionIndex)
{
    if (RefPtr axMenuList = dynamicDowncast<AccessibilityMenuList>(get(menuList)))
        axMenuList->didUpdateActiveOption(optionIndex);
}

void AXObjectCache::onSlottedContentChange(const HTMLSlotElement& slot)
{
    childrenChanged(get(const_cast<HTMLSlotElement&>(slot)));
}

void AXObjectCache::onDetailsSummarySlotChange(const HTMLDetailsElement& details)
{
    for (auto& summary : childrenOfType<HTMLSummaryElement>(const_cast<HTMLDetailsElement&>(details))) {
        if (RefPtr object = get(summary))
            m_deferredRecomputeActiveSummaryList.add(*object);
    }
}

void AXObjectCache::onRadioGroupMembershipChanged(HTMLElement& radio)
{
    if (auto* radioElement = dynamicDowncast<HTMLInputElement>(radio)) {
        for (auto& sibling : radioElement->radioButtonGroup()) {
            if (sibling.ptr() == &radio)
                continue;

            if (auto* axObject = get(sibling.ptr()))
                postNotification(axObject, &sibling->document(), AXNotification::RadioGroupMembershipChanged);
        }
    }
}

static bool isContentVisibilityHidden(const RenderStyle& style)
{
    return style.usedContentVisibility() == ContentVisibility::Hidden;
}

void AXObjectCache::onStyleChange(Element& element, OptionSet<Style::Change> change, const RenderStyle* oldStyle, const RenderStyle* newStyle)
{
    if (!change || !oldStyle || !newStyle)
        return;

    RefPtr object = get(element);
    if (!object)
        return;

    if (element.renderer()) {
        // Unlike changes to `visibility`, changes to `content-visibility` do not provide accessibility
        // children changed notifications via the render tree, so check for that here.
        if (isContentVisibilityHidden(*oldStyle) != isContentVisibilityHidden(*newStyle))
            childrenChanged(object.get());
    } else if (isVisibilityHidden(*oldStyle) != isVisibilityHidden(*newStyle)) {
        // We only need to do this when the given element doesn't have a renderer, as if it did, we would
        // get a children-changed event through the render tree.
        childrenChanged(object.get());
    }

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (oldStyle->insideLink() != newStyle->insideLink())
        postNotification(*object, AXNotification::VisitedStateChanged);

    if (oldStyle->speakAs() != newStyle->speakAs())
        postNotification(*object, AXNotification::SpeakAsChanged);

    if (oldStyle->cursorType() != newStyle->cursorType())
        postNotification(*object, AXNotification::CursorTypeChanged);

    if (oldStyle->pointerEvents() != newStyle->pointerEvents())
        postNotification(*object, AXNotification::PointerEventsChanged);

    // Certain properties (e.g. AXProperty::ShowsCursorOnHover) cannot easily
    // be diffed here so post this general notification for those.
    updateIsolatedTree(*object, AXNotification::StyleChanged);
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
}

HashMap<AXID, LineRange> AXObjectCache::mostRecentlyPaintedText()
{
    HashMap<AXID, LineRange> recentlyPaintedText;
    for (auto renderTextToLineRange : m_mostRecentlyPaintedText) {
        if (RefPtr axObject = getOrCreate(renderTextToLineRange.key))
            recentlyPaintedText.add(axObject->objectID(), renderTextToLineRange.value);
    }
    return recentlyPaintedText;
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::onAccessibilityPaintStarted()
{
    m_mostRecentlyPaintedText.clear();
}

void AXObjectCache::onAccessibilityPaintFinished()
{
    RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID);
    if (!tree)
        return;

    for (auto iterator = m_mostRecentlyPaintedText.begin(); iterator != m_mostRecentlyPaintedText.end(); ++iterator) {
        const auto& renderText = iterator->key;
        if (auto textBox = InlineIterator::firstTextBoxInLogicalOrderFor(renderText).first) {
            // The line index from TextBox::lineIndex is relative to the containing block, which count lines from
            // other renderers. The LineRange struct we have built expects the start and end line indices to be
            // relative to just this renderer, so normalize them by getting the first line index for this renderer.
            size_t firstLineIndexForRenderer = textBox->lineIndex();
            ASSERT(firstLineIndexForRenderer <= iterator->value.startLineIndex);
            ASSERT(firstLineIndexForRenderer <= iterator->value.endLineIndex);
            iterator->value.startLineIndex -= firstLineIndexForRenderer;
            iterator->value.endLineIndex -= firstLineIndexForRenderer;
        }
    }

    tree->markMostRecentlyPaintedTextDirty();
    startUpdateTreeSnapshotTimer();
}

bool AXObjectCache::onFontChange(Element& element, const RenderStyle* oldStyle, const RenderStyle* newStyle)
{
    if (!oldStyle || !newStyle)
        return false;

    RefPtr object = get(element);
    if (!object)
        return false;

    RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID);
    if (!tree)
        return false;

    if (!oldStyle->fontCascadeEqual(*newStyle)) {
        postNotification(*object, AXNotification::FontChanged);
        return true;
    }

    return false;
}

bool AXObjectCache::onTextColorChange(Element& element, const RenderStyle* oldStyle, const RenderStyle* newStyle)
{
    if (!oldStyle || !newStyle)
        return false;

    RefPtr object = get(element);
    if (!object)
        return false;

    RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID);
    if (!tree)
        return false;

    if (oldStyle->visitedDependentColor(CSSPropertyColor) != newStyle->visitedDependentColor(CSSPropertyColor)) {
        postNotification(*object, AXNotification::TextColorChanged);
        return true;
    }

    return false;
}
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

void AXObjectCache::onStyleChange(RenderText& renderText, StyleDifference difference, const RenderStyle* oldStyle, const RenderStyle& newStyle)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!oldStyle)
        return;

    bool speakAsChanged = oldStyle->speakAs() != newStyle.speakAs();
#if !ENABLE(AX_THREAD_TEXT_APIS)
    // In !ENABLE(AX_THREAD_TEXT_APIS), we don't have anything to do if speak-as hasn't changed.
    if (!speakAsChanged) [[likely]]
        return;
#endif // !ENABLE(AX_THREAD_TEXT_APIS)

    bool diffIsEqual = difference == StyleDifference::Equal;
    // When speak-as changes, style difference will be StyleDifference::Equal (so "equal"
    // is not exactly accurate). So if the styles are "equal" and speak-as hasn't changed,
    // we have nothing to do.
    if (diffIsEqual) {
        if (!speakAsChanged) [[likely]]
            return;
    }

    RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID);
    if (!tree)
        return;

    RefPtr object = get(renderText);
    if (!object)
        return;

    if (speakAsChanged) [[unlikely]]
        postNotification(*object, AXNotification::SpeakAsChanged);

    // The following style changes will not have a StyleDifference::Equal, so we can
    // exit early if the diff is equal.
    if (diffIsEqual)
        return;

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (oldStyle->visitedDependentColor(CSSPropertyBackgroundColor) != newStyle.visitedDependentColor(CSSPropertyBackgroundColor))
        tree->queueNodeUpdate(object->objectID(), { AXProperty::BackgroundColor });

    if (oldStyle->verticalAlign() != newStyle.verticalAlign())
        tree->queueNodeUpdate(object->objectID(), { { AXProperty::IsSuperscript, AXProperty::IsSubscript } });

    if (oldStyle->hasTextShadow() != newStyle.hasTextShadow())
        tree->queueNodeUpdate(object->objectID(), { AXProperty::HasTextShadow });

    auto oldDecor = oldStyle->textDecorationLineInEffect();
    auto newDecor = newStyle.textDecorationLineInEffect();
    if (oldDecor.hasUnderline() != newDecor.hasUnderline())
        tree->queueNodeUpdate(object->objectID(), { AXProperty::UnderlineColor });

    if (oldDecor.hasLineThrough() != newDecor.hasLineThrough())
        tree->queueNodeUpdate(object->objectID(), { AXProperty::HasLinethrough });

    if (oldStyle->textDecorationColor() != newStyle.textDecorationColor())
        tree->queueNodeUpdate(object->objectID(), { { AXProperty::LinethroughColor, AXProperty::UnderlineColor } });

    if (oldStyle->pointerEvents() != newStyle.pointerEvents())
        postNotification(*object, AXNotification::PointerEventsChanged);
#endif // ENABLE(AX_THREAD_TEXT_APIS)

#else
    UNUSED_PARAM(renderText);
    UNUSED_PARAM(difference);
    UNUSED_PARAM(oldStyle);
    UNUSED_PARAM(newStyle);
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
}

void AXObjectCache::onTextSecurityChanged(HTMLInputElement& inputElement)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    postNotification(get(&inputElement), AXNotification::TextSecurityChanged);
#else
    UNUSED_PARAM(inputElement);
#endif
}

void AXObjectCache::onTitleChange(Document& document)
{
    postNotification(get(&document), AXNotification::TextChanged);
}

void AXObjectCache::onValidityChange(Element& element)
{
    postNotification(get(&element), AXNotification::InvalidStatusChanged);
}

void AXObjectCache::onTextCompositionChange(Node& node, CompositionState compositionState, bool valueChanged, const String& text, size_t position, bool handlingAcceptedCandidate)
{
#if HAVE(INLINE_PREDICTIONS)
    RefPtr object = getOrCreate(node);
    if (!object)
        return;

#if PLATFORM(IOS_FAMILY)
    if (valueChanged)
        object->setLastPresentedTextPrediction(node, compositionState, text, position, handlingAcceptedCandidate);
#endif

    if (RefPtr observableObject = object->observableObject())
        object = WTFMove(observableObject);

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(object.get(), AXNotification::TextCompositionChanged);
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

    if (compositionState == CompositionState::Started)
        postNotification(object.get(), node.protectedDocument().ptr(), AXNotification::TextCompositionBegan);

    if (valueChanged)
        postNotification(object.get(), node.protectedDocument().ptr(), AXNotification::ValueChanged);

    if (compositionState == CompositionState::Ended)
        postNotification(object.get(), node.protectedDocument().ptr(), AXNotification::TextCompositionEnded);
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
        switch (intent.editType) {
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
        case AXTextEditTypeReplace:
            dataLog("Replace");
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

void AXObjectCache::postTextStateChangeNotification(Node* node, const AXTextStateChangeIntent& intent, const VisibleSelection& selection)
{
    if (!node)
        return;

#if PLATFORM(COCOA) || USE(ATSPI)
    stopCachingComputedObjectAttributes();

    postTextStateChangeNotification(getOrCreate(*node), intent, selection);
#else
    postNotification(node->renderer(), AXNotification::SelectedTextChanged, PostTarget::ObservableParent);
    UNUSED_PARAM(intent);
    UNUSED_PARAM(selection);
#endif
}

void AXObjectCache::postTextStateChangeNotification(const Position& position, const AXTextStateChangeIntent& intent, const VisibleSelection& selection)
{
    RefPtr node = position.deprecatedNode();
    if (!node)
        return;

    stopCachingComputedObjectAttributes();

#if PLATFORM(COCOA) || USE(ATSPI)
    RefPtr object = getOrCreate(*node);
    if (object && object->isIgnored()) {
#if PLATFORM(COCOA)
        if (position.atLastEditingPositionForNode()) {
            if (RefPtr nextSibling = object->nextSiblingUnignored(1))
                object = WTFMove(nextSibling);
        } else if (position.atFirstEditingPositionForNode()) {
            if (RefPtr previousSibling = object->previousSiblingUnignored(1))
                object = WTFMove(previousSibling);
        }
#elif USE(ATSPI)
        // ATSPI doesn't expose text nodes, so we need the parent
        // object which is the one implementing the text interface.
        object = object->parentObjectUnignored();
#endif
    }

    postTextStateChangeNotification(object.get(), intent, selection);
#else
    postTextStateChangeNotification(node.get(), intent, selection);
#endif
}

void AXObjectCache::postTextStateChangeNotification(AccessibilityObject* object, const AXTextStateChangeIntent& intent, const VisibleSelection& selection)
{
    AXTRACE(makeString("AXObjectCache::postTextStateChangeNotification 0x"_s, hex(reinterpret_cast<uintptr_t>(this))));
    stopCachingComputedObjectAttributes();

#if PLATFORM(COCOA) || USE(ATSPI)
    RefPtr axObject = object;
    if (axObject) {
        if (RefPtr observableObject = axObject->observableObject())
            axObject = observableObject;
    }

    if (!axObject)
        axObject = rootWebArea();

    if (axObject) {
        const AXTextStateChangeIntent& newIntent = (intent.type == AXTextStateChangeTypeUnknown || (m_isSynchronizingSelection && m_textSelectionIntent.type != AXTextStateChangeTypeUnknown)) ? m_textSelectionIntent : intent;

#if PLATFORM(COCOA)
        if (enqueuePasswordNotification(*axObject, { newIntent, { }, { }, selection }))
            return;
#endif

        postTextSelectionChangePlatformNotification(axObject.get(), newIntent, selection);
    }

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    onSelectedTextChanged(selection, axObject.get());
#endif
#else // PLATFORM(COCOA) || USE(ATSPI)
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

    RefPtr object = getOrCreate(node);
#if PLATFORM(COCOA) || USE(ATSPI)
    if (object)
        object = object->observableObject();

    if (!object)
        object = rootWebArea();

    if (!object)
        return;

#if PLATFORM(COCOA)
    if (enqueuePasswordNotification(*object, { { type }, { }, text, { position, position } }))
        return;
#endif

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(*object, AXNotification::ValueChanged);
#endif

    postTextStateChangePlatformNotification(object.get(), type, text, position);
#else // PLATFORM(COCOA) || USE(ATSPI)
    nodeTextChangePlatformNotification(object.get(), textChangeForEditType(type), position.deepEquivalent().deprecatedEditingOffset(), text);
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

    RefPtr object = getOrCreate(*node);
#if PLATFORM(COCOA) || USE(ATSPI)
    if (object)
        object = object->observableObject();
    if (!object)
        return;

#if PLATFORM(COCOA)
    if (enqueuePasswordNotification(*object, { { AXTextEditTypeReplace }, deletedText, insertedText, { position, position } }))
        return;
#endif

    postTextReplacementPlatformNotification(object.get(), deletionType, deletedText, insertionType, insertedText, position);
#else // PLATFORM(COCOA) || USE(ATSPI)
    nodeTextChangePlatformNotification(object.get(), textChangeForEditType(deletionType), position.deepEquivalent().deprecatedEditingOffset(), deletedText);
    nodeTextChangePlatformNotification(object.get(), textChangeForEditType(insertionType), position.deepEquivalent().deprecatedEditingOffset(), insertedText);
#endif
}

void AXObjectCache::postTextReplacementNotificationForTextControl(HTMLTextFormControlElement& textControl, const String& deletedText, const String& insertedText)
{
    stopCachingComputedObjectAttributes();

    RefPtr object = getOrCreate(textControl);
#if PLATFORM(COCOA) || USE(ATSPI)
    if (object)
        object = object->observableObject();
    if (!object)
        return;

#if PLATFORM(COCOA)
    if (enqueuePasswordNotification(*object, { { AXTextEditTypeReplace }, deletedText, insertedText, { } }))
        return;
#endif

    postTextReplacementPlatformNotificationForTextControl(object.get(), deletedText, insertedText);
#else // PLATFORM(COCOA) || USE(ATSPI)
    nodeTextChangePlatformNotification(object.get(), textChangeForEditType(AXTextEditTypeDelete), 0, deletedText);
    nodeTextChangePlatformNotification(object.get(), textChangeForEditType(AXTextEditTypeInsert), 0, insertedText);
#endif
}

#if PLATFORM(COCOA)

static AXTextChangeContext secureContext(AccessibilityObject& object, AXTextChangeContext& context)
{
    ASSERT(isSecureFieldOrContainedBySecureField(object));

    // FIXME: Add a better way to retrieve the maskingCharacter for secure fields.
    String value = object.secureFieldValue();
    auto maskingCharacter = value.length() ? value[0] : bullet;

    const auto& secureString = [&maskingCharacter] (String& text) {
        if (text.isEmpty())
            return;

        std::span<char16_t> characters;
        text = String::createUninitialized(text.length(), characters);
        for (unsigned i = 0; i < text.length(); ++i)
            characters[i] = maskingCharacter;
    };

    secureString(context.deletedText);
    secureString(context.insertedText);
    return context;
}

bool AXObjectCache::enqueuePasswordNotification(AccessibilityObject& object, AXTextChangeContext&& context)
{
    if (!isSecureFieldOrContainedBySecureField(object))
        return false;

    RefPtr observableObject = object.observableObject();
    if (!observableObject) {
        ASSERT_NOT_REACHED();
        // Return true even though the enqueue didn't happen because this is a password field and caller shouldn't post a notification unless it is queued.
        return true;
    }

    m_passwordNotifications.append({ *observableObject, secureContext(*observableObject, context) });
    if (!m_passwordNotificationTimer.isActive())
        m_passwordNotificationTimer.startRepeating(accessibilityPasswordValueChangeNotificationInterval);

    return true;
}

void AXObjectCache::passwordNotificationTimerFired()
{
    if (m_passwordNotifications.isEmpty()) {
        m_passwordNotificationTimer.stop();
        return;
    }

    auto notification = m_passwordNotifications.takeFirst();
    auto& context = notification.second;
    switch (context.intent.type) {
    case AXTextStateChangeTypeEdit:
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        updateIsolatedTree(notification.first, AXNotification::ValueChanged);
#endif
        if (context.intent.editType == AXTextEditTypeReplace) {
            postTextReplacementPlatformNotification(notification.first.ptr(),
                AXTextEditTypeDelete, context.deletedText, AXTextEditTypeInsert, context.insertedText, context.selection.start());
        } else {
            postTextStateChangePlatformNotification(notification.first.ptr(),
                context.intent.editType, context.insertedText, context.selection.start());
        }
        break;
    case AXTextStateChangeTypeSelectionMove:
    case AXTextStateChangeTypeSelectionExtend:
    case AXTextStateChangeTypeSelectionBoundary:
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        updateIsolatedTree(notification.first, AXNotification::SelectedTextChanged);
#endif
        postTextSelectionChangePlatformNotification(notification.first.ptr(),
            context.intent, context.selection);
        break;
    case AXTextStateChangeTypeUnknown:
        // No additional context, fallback to a ValueChanged notification.
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        updateIsolatedTree(notification.first, AXNotification::ValueChanged);
#endif
        postPlatformNotification(notification.first, AXNotification::ValueChanged);
        break;
    };
}

#endif // PLATFORM(COCOA)

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::onSelectedTextChanged(const VisiblePositionRange& selection, AccessibilityObject* object)
{
    if (object) {
        m_lastDebouncedTextRangeObject = object->objectID();
        if (!m_selectedTextRangeTimer.isActive())
            m_selectedTextRangeTimer.restart();
    }

    if (RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID)) {
        if (selection.isNull())
            tree->setSelectedTextMarkerRange({ });
        else {
            auto startPosition = selection.start.deepEquivalent();
            auto endPosition = selection.end.deepEquivalent();

            if (startPosition.isNull() || endPosition.isNull())
                tree->setSelectedTextMarkerRange({ });
            else {
                if (RefPtr startObject = get(startPosition.anchorNode()))
                    createIsolatedObjectIfNeeded(*startObject);
                if (RefPtr endObject = get(endPosition.anchorNode()))
                    createIsolatedObjectIfNeeded(*endObject);

                tree->setSelectedTextMarkerRange({ selection });
            }
        }
    }
}
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

void AXObjectCache::frameLoadingEventNotification(LocalFrame* frame, AXLoadingEvent loadingEvent)
{
    if (frame) {
        // We pass the RenderView* (via contentRenderer()) rather than calling getOrCreate and passing
        // that because some platforms don't handle all loading event types, and we don't want to call
        // getOrCreate unnecessarily (because doing so is not always safe, and can do a fair amount of work).
        frameLoadingEventPlatformNotification(frame->contentRenderer(), loadingEvent);
    }
}

void AXObjectCache::postLiveRegionChangeNotification(AccessibilityObject& object)
{
#if PLATFORM(COCOA)
    if (m_liveRegionManager) {
        m_liveRegionManager->handleLiveRegionChange(object);
        return;
    }
#endif

    if (m_liveRegionChangedPostTimer.isActive())
        m_liveRegionChangedPostTimer.stop();

    Ref objectRef = object;
    if (!m_changedLiveRegions.contains(objectRef))
        m_changedLiveRegions.add(WTFMove(objectRef));

    m_liveRegionChangedPostTimer.startOneShot(0_s);
}

void AXObjectCache::liveRegionChangedNotificationPostTimerFired()
{
    m_liveRegionChangedPostTimer.stop();

    if (m_changedLiveRegions.isEmpty())
        return;

    for (auto& object : m_changedLiveRegions)
        postNotification(object.ptr(), object->protectedDocument().get(), AXNotification::LiveRegionChanged);
    m_changedLiveRegions.clear();
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
    if (RefPtr scrollViewObject = get(&view)) {
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
        RefPtr ancestor = Accessibility::findAncestor<AccessibilityObject>(*object, false, [] (auto& candidate) {
            return candidate.supportsRowCountChange();
        });

        // Post that the ancestor's row count changed.
        if (ancestor)
            handleRowCountChanged(ancestor.get(), protectedDocument().get());

        // Post that the specific row either collapsed or expanded.
        auto role = object->role();
        if (role == AccessibilityRole::Row || role == AccessibilityRole::TreeItem)
            postNotification(object.get(), protectedDocument().get(), object->isExpanded() ? AXNotification::RowExpanded : AXNotification::RowCollapsed);
        else
            postNotification(object.get(), protectedDocument().get(), AXNotification::ExpandedChanged);
    }
}

void AXObjectCache::handleActiveDescendantChange(Element& element, const AtomString& oldValue, const AtomString& newValue)
{
    AXTRACE("AXObjectCache::handleActiveDescendantChange"_s);

    // Use the element's document instead of the cache's document in case we're inside a frame that's managing focus.
    RefPtr frame = element.document().frame();
    if (!frame || !frame->selection().isFocusedAndActive())
        return;

    RefPtr object = getOrCreate(element);
    if (!object)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(*object, AXNotification::ActiveDescendantChanged);
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
        postPlatformNotification(*activeDescendant, AXNotification::FocusedUIElementChanged);
    }

    // Handle active-descendant changes when the target allows for it, or the controlled object allows for it.
    RefPtr<AccessibilityObject> target;
    if (object->supportsActiveDescendant())
        target = object;
    else {
        // Check to see if the active descendant is a descendant of an object controlled by this object.
        // In that case, the controlled object will be the target for the notification.
        auto controlledObjects = object->relatedObjects(AXRelation::ControllerFor);
        if (controlledObjects.size()) {
            target = Accessibility::findAncestor(*activeDescendant, false, [&controlledObjects] (const auto& activeDescendantAncestor) {
                return controlledObjects.contains(Ref { activeDescendantAncestor });
            });
        }
    }

    if (!target)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (target != object)
        updateIsolatedTree(target.get(), AXNotification::ActiveDescendantChanged);
#endif

    postPlatformNotification(*target, AXNotification::ActiveDescendantChanged);

    // Table cell active descendant changes should trigger selected cell changes.
    if (target->isTable() && activeDescendant->isExposedTableCell())
        postPlatformNotification(*target, AXNotification::SelectedCellsChanged);
}

void AXObjectCache::handleRoleChanged(Element& element, const AtomString& oldValue, const AtomString& newValue)
{
    AXTRACE("AXObjectCache::handleRoleChanged"_s);
    AXLOG(makeString("oldValue "_s, oldValue, " new value "_s, newValue));
    ASSERT(oldValue != newValue);

    RefPtr object = get(element);
    if (!object)
        return;

    // The class of an AX object created for an Element depends on the role attribute of that Element.
    // Thus when the role changes, remove the existing AX object and force a ChildrenChanged on the parent
    // so that the object is re-created.
    if (oldValue.isEmpty() || oldValue == "row"_s || newValue.isEmpty() || newValue == "row"_s) {
        if (auto* parent = object->parentObject()) {
            remove(element);
            childrenChanged(parent);
            return;
        }
    }

    object->updateRole();
}

void AXObjectCache::handleRoleChanged(AccessibilityObject& axObject, AccessibilityRole oldRole)
{
    stopCachingComputedObjectAttributes();
    axObject.recomputeIsIgnored();

#if PLATFORM(MAC)
    if (axObject.supportsLiveRegion())
        deferSortForNewLiveRegion(axObject);
    else if (AXCoreObject::liveRegionStatusIsEnabled(AtomString { AXCoreObject::defaultLiveRegionStatusForRole(oldRole) }))
        removeLiveRegion(axObject);
#else
    UNUSED_PARAM(oldRole);
#endif // PLATFORM(MAC)

    if (axObject.needsRareData()) {
        // A role change may have caused an object gain the need for rare data, so handle that here.
        axObject.ensureRareData();
    }

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    postNotification(axObject, AXNotification::RoleChanged);
#endif
}

void AXObjectCache::handleARIARoleDescriptionChanged(Element& element)
{
    RefPtr object = get(element);
    if (!object)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(object.get(), AXNotification::ARIARoleDescriptionChanged);
#endif
}

void AXObjectCache::handleInputTypeChanged(Element& element)
{
    RefPtr object = get(element);
    if (!object)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(object.get(), AXNotification::InputTypeChanged);
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

void AXObjectCache::handleReferenceTargetChanged()
{
    relationsNeedUpdate(true);
}

void AXObjectCache::handlePageEditibilityChanged(Document& document)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    postNotification(&document, AXNotification::IsEditableWebAreaChanged);
#else
    UNUSED_PARAM(document);
#endif
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
            // do a cheap dynamicDowncast check for an HTMLTablePartElement rather than calling AccessibilityNodeObject::parentTable().
            // (ARIA tables are inherently always exposed).
            if (auto* tablePartElement = dynamicDowncast<HTMLTablePartElement>(element))
                deferRecomputeTableIsExposed(const_cast<HTMLTableElement*>(tablePartElement->findParentTable().get()));
        } else if (RefPtr object = getOrCreate(element); object && object->isTableCell()) {
            if (RefPtr parentTable = dynamicDowncast<AccessibilityNodeObject>(object->parentTable())) {
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
        if (RefPtr axObject = get(*element))
            axObject->updateRole();
    } else if (attrName == disabledAttr)
        postNotification(element, AXNotification::DisabledStateChanged);
    else if (attrName == forAttr) {
        if (RefPtr label = dynamicDowncast<HTMLLabelElement>(element)) {
            bool updatedLabelFor = updateLabelFor(*label);

            if (updatedLabelFor) {
                if (RefPtr oldControl = element->treeScope().elementByIdResolvingReferenceTarget(oldValue))
                    postNotification(oldControl.get(), AXNotification::TextChanged);
                if (RefPtr newControl = element->treeScope().elementByIdResolvingReferenceTarget(newValue))
                    postNotification(newControl.get(), AXNotification::TextChanged);
            }
        }
    } else if (attrName == requiredAttr)
        postNotification(element, AXNotification::RequiredStatusChanged);
    else if (attrName == tabindexAttr) {
        if (oldValue.isEmpty() || newValue.isEmpty()) {
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
            // When ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE), we don't need to do issue any children-changed events,
            // which is quite expensive — just re-compute is-ignored on this single object.
            if (RefPtr object = get(*element))
                object->recomputeIsIgnored();
#else
            RefPtr parent = element->parentNode();
            if (auto* renderer = parent ? parent->renderer() : nullptr)
                childrenChanged(*renderer);
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
            postNotification(element, AXNotification::FocusableStateChanged);
#endif
        }
    }
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    else if (attrName == draggableAttr)
        postNotification(get(*element), AXNotification::DraggableStateChanged);
    else if (attrName == langAttr)
        updateIsolatedTree(get(*element), AXNotification::LanguageChanged);
    else if (attrName == nameAttr)
        postNotification(get(*element), AXNotification::NameChanged);
    else if (attrName == placeholderAttr)
        postNotification(element, AXNotification::PlaceholderChanged);
    else if (attrName == hrefAttr || attrName == srcAttr)
        postNotification(element, AXNotification::URLChanged);
    else if (attrName == idAttr) {
#if !LOG_DISABLED
        updateIsolatedTree(get(*element), AXNotification::IdAttributeChanged);
#endif
    } else if (attrName == accesskeyAttr)
        updateIsolatedTree(get(*element), AXNotification::AccessKeyChanged);
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    else if (attrName == openAttr) {
        if (is<HTMLDialogElement>(*element)) {
            deferModalChange(*element);
            recomputeIsIgnored(element->parentNode());
        } else if (is<HTMLDetailsElement>(*element)) {
            if (RefPtr object = get(*element)) {
                postNotification(*object, AXNotification::ExpandedChanged);
                childrenChanged(object.get());
                object->recomputeIsIgnoredForDescendants();
            }

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
            for (Ref summary : descendantsOfType<HTMLSummaryElement>(*element))
                updateIsolatedTree(get(WTFMove(summary)), AXNotification::ExpandedChanged);
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        }
    } else if (attrName == rowspanAttr) {
        deferRowspanChange(get(*element));
        recomputeParentTableProperties(element, TableProperty::CellSlots);
    } else if (attrName == colspanAttr) {
        postNotification(element, AXNotification::ColumnSpanChanged);
        recomputeParentTableProperties(element, TableProperty::CellSlots);
    } else if (attrName == commandAttr)
        postNotification(element, AXNotification::CommandChanged);
    else if (attrName == commandforAttr)
        postNotification(element, AXNotification::CommandForChanged);
    else if (attrName == popovertargetAttr)
        postNotification(element, AXNotification::PopoverTargetChanged);
    else if (attrName == scopeAttr)
        postNotification(element, AXNotification::CellScopeChanged);
    else if (attrName == datetimeAttr)
        postNotification(element, AXNotification::DatetimeChanged);
    else if (attrName == abbrAttr)
        postNotification(element, AXNotification::AbbreviationChanged);
    else if (attrName == hiddenAttr)
        postNotification(element, AXNotification::HiddenStateChanged);

    if (!attrName.localName().string().startsWith("aria-"_s))
        return;

    if (attrName == aria_activedescendantAttr)
        handleActiveDescendantChange(*element, oldValue, newValue);
    else if (attrName == aria_atomicAttr)
        postNotification(element, AXNotification::IsAtomicChanged);
    else if (attrName == aria_busyAttr)
        postNotification(element, AXNotification::ElementBusyChanged);
    else if (attrName == aria_controlsAttr)
        postNotification(element, AXNotification::ControlledObjectsChanged);
    else if (attrName == aria_valuenowAttr || attrName == aria_valuetextAttr)
        postNotification(element, AXNotification::ValueChanged);
    else if (attrName == aria_labelAttr && element->elementName() == ElementName::HTML_html) {
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
    } else if (attrName == aria_checkedAttr)
        checkedStateChanged(*element);
    else if (attrName == aria_colcountAttr) {
        postNotification(element, AXNotification::ColumnCountChanged);
        deferRecomputeTableIsExposed(dynamicDowncast<HTMLTableElement>(element));
    } else if (attrName == aria_colindexAttr) {
        postNotification(element, AXNotification::ARIAColumnIndexChanged);
        recomputeParentTableProperties(element, TableProperty::Exposed);
    } else if (attrName == aria_colindextextAttr) {
        postNotification(element, AXNotification::ARIAColumnIndexTextChanged);
    } else if (attrName == aria_colspanAttr) {
        postNotification(element, AXNotification::ColumnSpanChanged);
        recomputeParentTableProperties(element, { TableProperty::CellSlots, TableProperty::Exposed });
    } else if (attrName == aria_describedbyAttr)
        postNotification(element, AXNotification::DescribedByChanged);
    else if (attrName == aria_descriptionAttr)
        postNotification(element, AXNotification::ExtendedDescriptionChanged);
    else if (attrName == aria_dropeffectAttr)
        postNotification(element, AXNotification::DropEffectChanged);
    else if (attrName == aria_flowtoAttr)
        postNotification(element, AXNotification::FlowToChanged);
    else if (attrName == aria_grabbedAttr)
        postNotification(element, AXNotification::GrabbedStateChanged);
    else if (attrName == aria_keyshortcutsAttr)
        postNotification(element, AXNotification::KeyShortcutsChanged);
    else if (attrName == aria_levelAttr)
        postNotification(element, AXNotification::LevelChanged);
    else if (attrName == aria_liveAttr) {
        postNotification(element, AXNotification::LiveRegionStatusChanged);

#if PLATFORM(MAC)
        if (RefPtr object = getOrCreate(element)) {
            if (object->supportsLiveRegion())
                deferSortForNewLiveRegion(object.releaseNonNull());
            else
                removeLiveRegion(*object);
        }
#endif // PLATFORM(MAC)
    } else if (attrName == aria_placeholderAttr)
        postNotification(element, AXNotification::PlaceholderChanged);
    else if (attrName == aria_rowindexAttr) {
        postNotification(element, AXNotification::ARIARowIndexChanged);
        recomputeParentTableProperties(element, { TableProperty::CellSlots, TableProperty::Exposed });
    } else if (attrName == aria_rowindextextAttr)
        postNotification(element, AXNotification::ARIARowIndexTextChanged);
    else if (attrName == aria_valuemaxAttr)
        postNotification(element, AXNotification::MaximumValueChanged);
    else if (attrName == aria_valueminAttr)
        postNotification(element, AXNotification::MinimumValueChanged);
    else if (attrName == aria_multilineAttr) {
        if (RefPtr axObject = get(*element)) {
            // The role of textarea and textfield objects is dependent on whether they can span multiple lines, so recompute it here.
            if (axObject->role() == AccessibilityRole::TextArea || axObject->role() == AccessibilityRole::TextField)
                axObject->updateRole();
        }
    } else if (attrName == aria_multiselectableAttr)
        postNotification(element, AXNotification::MultiSelectableStateChanged);
    else if (attrName == aria_orientationAttr)
        postNotification(element, AXNotification::OrientationChanged);
    else if (attrName == aria_posinsetAttr)
        postNotification(element, AXNotification::PositionInSetChanged);
    else if (attrName == aria_relevantAttr)
        postNotification(element, AXNotification::LiveRegionRelevantChanged);
    else if (attrName == aria_selectedAttr)
        onSelectedOptionChanged(*element);
    else if (attrName == aria_setsizeAttr)
        postNotification(element, AXNotification::SetSizeChanged);
    else if (attrName == aria_expandedAttr)
        handleAriaExpandedChange(*element);
    else if (attrName == aria_haspopupAttr)
        postNotification(element, AXNotification::HasPopupChanged);
    else if (attrName == aria_hiddenAttr) {
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
        if (RefPtr axObject = getOrCreate(*element))
            axObject->recomputeIsIgnoredForDescendants(/* includeSelf */ true);
#else
        if (RefPtr parent = get(element->parentNode()))
            childrenChanged(parent.get());
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

        if (m_currentModalElement && m_currentModalElement->isDescendantOf(element))
            deferModalChange(*m_currentModalElement);
    } else if (attrName == aria_invalidAttr)
        postNotification(element, AXNotification::InvalidStatusChanged);
    else if (attrName == aria_modalAttr) {
        // aria-modal changed, so the element may have become modal or un-modal.
        if (isModalElement(*element))
            m_modalElements.appendIfNotContains(element);
        else
            m_modalElements.removeAll(element);
        deferModalChange(*element);
    } else if (attrName == aria_currentAttr)
        postNotification(element, AXNotification::CurrentStateChanged);
    else if (attrName == aria_disabledAttr)
        postNotification(element, AXNotification::DisabledStateChanged);
    else if (attrName == aria_pressedAttr)
        postNotification(element, AXNotification::PressedStateChanged);
    else if (attrName == aria_readonlyAttr)
        postNotification(element, AXNotification::ReadOnlyStatusChanged);
    else if (attrName == aria_requiredAttr)
        postNotification(element, AXNotification::RequiredStatusChanged);
    else if (attrName == aria_roledescriptionAttr)
        handleARIARoleDescriptionChanged(*element);
    else if (attrName == aria_rowcountAttr)
        handleRowCountChanged(get(*element), element->protectedDocument().ptr());
    else if (attrName == aria_rowspanAttr) {
        deferRowspanChange(get(*element));
        recomputeParentTableProperties(element, { TableProperty::CellSlots, TableProperty::Exposed });
    } else if (attrName == aria_sortAttr)
        postNotification(element, AXNotification::SortDirectionChanged);
    else if (attrName == aria_ownsAttr) {
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        if (oldValue.isEmpty() || newValue.isEmpty())
            updateIsolatedTree(get(*element), AXProperty::SupportsARIAOwns);
#endif
        auto updateStitchGroups = [&] (const AtomString& ariaOwnsValue) {
            // Stitch groups are computed by the containing block-flow, so loop over the owned DOM id's
            // to find the associated element's containing-block flow and mark it dirty.
            SpaceSplitString ids(ariaOwnsValue, SpaceSplitString::ShouldFoldCase::No);
            for (auto& id : ids) {
                if (RefPtr ownsTarget = element->treeScope().elementByIdResolvingReferenceTarget(id)) {
                    CheckedPtr renderer = ownsTarget->renderer();
                    if (auto* containingBlock = renderer ? renderer->containingBlock() : nullptr)
                        setDirtyStitchGroups(*containingBlock);
                }
            }
        };

        // FIXME: aria-owns relationships can also change when an object's ID changes. We should update
        // stitch groups in that case too.
        updateStitchGroups(oldValue);
        updateStitchGroups(newValue);
    }
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    else if (attrName == aria_braillelabelAttr)
        postNotification(element, AXNotification::BrailleLabelChanged);
    else if (attrName == aria_brailleroledescriptionAttr)
        postNotification(element, AXNotification::BrailleRoleDescriptionChanged);
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    else if (attrName == typeAttr)
        handleInputTypeChanged(*element);
}

static bool hasAnyARIALabelling(Element& element)
{
    return element.hasAttributeWithoutSynchronization(aria_labelAttr)
        || element.hasAttributeWithoutSynchronization(aria_labelledbyAttr)
        || element.hasAttributeWithoutSynchronization(aria_labeledbyAttr);
}

void AXObjectCache::handleLabelChanged(AccessibilityObject* object)
{
    AXTRACE("AXObjectCache::handleLabelChanged"_s);

    if (!object)
        return;

    bool updatedLabelFor = false;
    if (RefPtr label = dynamicDowncast<HTMLLabelElement>(object->element()))
        updatedLabelFor = updateLabelFor(*label);

    if (!updatedLabelFor) {
        auto labeledObjects = object->labelForObjects();
        for (auto& labeledObject : labeledObjects) {
            updateLabeledBy(RefPtr { labeledObject->element() }.get());
            postNotification(&downcast<AccessibilityObject>(labeledObject.get()), protectedDocument().get(), AXNotification::ValueChanged);
        }
    }

    postNotification(object, protectedDocument().get(), AXNotification::LabelChanged);
}

bool AXObjectCache::updateLabelFor(HTMLLabelElement& label)
{
    if (RefPtr control = Accessibility::controlForLabelElement(label)) {
        if (hasAnyARIALabelling(*control))
            return false;
    }

    removeRelation(label, AXRelation::LabelFor);
    addLabelForRelation(label);
    return true;
}

void AXObjectCache::updateLabeledBy(Element* element)
{
    if (!element)
        return;

    bool changedRelation = removeRelation(*element, AXRelation::LabeledBy);
    changedRelation |= addRelation(*element, aria_labelledbyAttr);
    if (changedRelation)
        dirtyIsolatedTreeRelations();
}

void AXObjectCache::dirtyIsolatedTreeRelations()
{
    AXTRACE("AXObjectCache::dirtyIsolatedTreeRelations"_s);

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID))
        tree->markRelationsDirty();
    startUpdateTreeSnapshotTimer();
#endif
}

void AXObjectCache::recomputeIsIgnored(RenderObject& renderer)
{
    if (RefPtr object = get(renderer))
        object->recomputeIsIgnored();
}

void AXObjectCache::recomputeIsIgnored(Node* node)
{
    if (RefPtr object = get(node))
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

RefPtr<Document> AXObjectCache::protectedDocument() const
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
    // Return an empty position if the object associated with the text marker has been destroyed.
    if (!cache || !cache->objectForID(*textMarkerData.axObjectID()))
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
    RefPtr<Node> currentNode;
    bool finished = false;
    int lastStartOffset = 0;

    TextIteratorBehaviors behaviors;
    if (!doNotEnterTextControls)
        behaviors.add(TextIteratorBehavior::EntersTextControls);
    TextIterator iterator(range, behaviors);

    // Enable the cache here for isIgnored calls in replacedNodeNeedsCharacter.
    AXAttributeCacheScope enableCache(this);

    // When the range has zero length, there might be replaced node or brTag that we need to increment the characterOffset.
    if (iterator.atEnd()) {
        currentNode = range.start.container.get();
        lastStartOffset = range.start.offset;
        if (offset > 0 || toNodeEnd) {
            if (AccessibilityObject::replacedNodeNeedsCharacter(*currentNode) || (currentNode->renderer() && currentNode->renderer()->isBR()))
                cumulativeOffset++;
            lastLength = cumulativeOffset;

            // When going backwards, stayWithinRange is false.
            // Here when we don't have any character to move and we are going backwards, we traverse to the previous node.
            if (!lastLength && toNodeEnd && !stayWithinRange) {
                if (RefPtr preNode = previousNode(currentNode.get()))
                    return traverseToOffsetInRange(rangeForNodeContents(*preNode), offset, option);
                return CharacterOffset();
            }
        }
    }

    // Sometimes text contents in a node are split into several iterations, so that iterator.range().startOffset()
    // might not be the total character count. Here we use a previousNode object to keep track of that.
    RefPtr<Node> previousNode;
    for (; !iterator.atEnd(); iterator.advance()) {
        int currentLength = iterator.text().length();
        bool hasReplacedNodeOrBR = false;

        currentNode = iterator.range().start.container;

        // When currentLength == 0, we check if there's any replaced node.
        // If not, we skip the node with no length.
        if (!currentLength) {
            RefPtr childNode = iterator.node();
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
                    RefPtr childNode = iterator.node();
                    if (childNode && childNode->renderer() && childNode->renderer()->isBR()) {
                        currentNode = childNode;
                        hasReplacedNodeOrBR = true;
                    } else if (RefPtr shadowHost = currentNode->shadowHost()) {
                        // Since we are entering text controls, we should set the currentNode
                        // to be the shadow host when there's no content.
                        if (elementIsTextControl(*shadowHost) && currentNode->isShadowRoot()) {
                            currentNode = shadowHost;
                            continue;
                        }
                    } else if (previousNode && previousNode->isTextNode() && previousNode->isDescendantOf(currentNode.get()) && elementName(*currentNode) == ElementName::HTML_p) {
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
    if (toNodeEnd && currentNode->isTextNode() && currentNode.get() == range.end.container.ptr() && static_cast<int>(range.end.offset) < lastStartOffset + offsetInCharacter)
        offsetInCharacter = range.end.offset - lastStartOffset;

    return CharacterOffset(currentNode.get(), lastStartOffset, offsetInCharacter, remaining);
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
    return AccessibilityObject::replacedNodeNeedsCharacter(node) || WebCore::elementName(node) == ElementName::HTML_br;
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

TextMarkerData AXObjectCache::textMarkerDataForCharacterOffset(const CharacterOffset& characterOffset, TextMarkerOrigin origin)
{
    if (characterOffset.isNull())
        return { };

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(characterOffset.node.get()); input && input->isSecureField())
        return { *this, { }, true, origin };

    return { *this, characterOffset, false, origin };
}

CharacterOffset AXObjectCache::startOrEndCharacterOffsetForRange(const SimpleRange& range, bool isStart, bool enterTextControls)
{
    // When getting the end CharacterOffset at node boundary, we don't want to collapse to the previous node.
    if (!isStart && !range.end.offset)
        return characterOffsetForNodeAndOffset(range.end.container, 0, TraverseOptionIncludeStart);

    // If it's end text marker, we want to go to the end of the range, and stay within the range.
    bool stayWithinRange = !isStart;

    Ref endNode = range.end.container;
    if (endNode->isCharacterDataNode() && !isStart)
        return traverseToOffsetInRange(rangeForNodeContents(endNode.get()), range.end.offset, TraverseOptionValidateOffset);

    auto copyRange = range;
    // Change the start of the range, so the character offset starts from node beginning.
    int offset = 0;
    Ref node = copyRange.start.container;
    if (node->isCharacterDataNode()) {
        auto nodeStartOffset = traverseToOffsetInRange(rangeForNodeContents(node.get()), range.start.offset, TraverseOptionValidateOffset);
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
    RefPtr domNode = node;
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
            domNode = previousNode(domNode.get());
            if (domNode)
                charOffset = characterOffsetForNodeAndOffset(*domNode, 0, TraverseOptionToNodeEnd);
            else
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
        domNode = nextNode(domNode.get());
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
    while (!currentPosition.isNull() && !targetDeepPosition.equals(currentPosition)) {
        auto previousPosition = currentPosition;
        // Note that we explicitly _do not_ want currentPosition to be derived from visiblePositon.deepEquivalent(),
        // as creating a VisiblePosition with a Position calls |canonicalPosition| on said Position, which critically
        // can return a previous position, resulting in us looping infinitely. Iterating solely through
        // |nextVisuallyDistinctCandidate|s should guarantee forward progress.
        currentPosition = nextVisuallyDistinctCandidate(currentPosition, SkipDisplayContents::No);
        visiblePosition = VisiblePosition(currentPosition, visiblePosition.affinity());

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

std::optional<TextMarkerData> AXObjectCache::textMarkerDataForVisiblePosition(const VisiblePosition& visiblePosition, TextMarkerOrigin origin)
{
    if (visiblePosition.isNull())
        return std::nullopt;

    Position position = visiblePosition.deepEquivalent();
    RefPtr node = position.anchorNode();
    ASSERT(node);
    if (!node)
        return std::nullopt;

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(node); input && input->isSecureField())
        return std::nullopt;

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (shouldCreateAXThreadCompatibleMarkers()) {
        // We need to convert the DOM offset (which is offset into pre-whitespace-collapse text) into an offset into
        // the rendered, post-whitespace-collapse text.
        unsigned domOffset = position.deprecatedEditingOffset();

        auto createFromRendererAndOffset = [&origin, &visiblePosition] (RenderObject& renderer, unsigned offset) -> std::optional<TextMarkerData> {
            CheckedPtr cache = renderer.document().axObjectCache();
            RefPtr object = cache ? cache->getOrCreate(renderer) : nullptr;
            if (!object)
                return std::nullopt;

            return std::optional(TextMarkerData {
                cache->treeID(),
                object->objectID(),
                offset,
                Position::PositionIsOffsetInAnchor,
                visiblePosition.affinity(),
                0,
                offset,
                object->isIgnored(),
                origin
            });
        };

        if (isRendererReplacedElement(node->renderer()) || is<RenderLineBreak>(node->renderer()))
            return createFromRendererAndOffset(*node->renderer(), domOffset);

        CheckedPtr<const RenderText> renderText = nullptr;

        auto boxAndOffset = visiblePosition.inlineBoxAndOffset();
        if (boxAndOffset.box) {
            renderText = dynamicDowncast<RenderText>(boxAndOffset.box->renderer());
            domOffset = boxAndOffset.offset;
        }

        if (!renderText) {
            renderText = dynamicDowncast<RenderText>(node ? node->renderer() : nullptr);
            if (!renderText)
                return std::nullopt;
        }

        auto [textBox, orderCache] = InlineIterator::firstTextBoxInLogicalOrderFor(*renderText);
        if (!textBox)
            return std::nullopt;

        unsigned differenceBetweenDomAndRenderedOffsets = textBox->minimumCaretOffset();
        unsigned previousEndDomOffset = textBox->maximumCaretOffset();
        size_t previousLineIndex = textBox->lineIndex();

        while (domOffset > textBox->maximumCaretOffset()) {
            textBox = InlineIterator::nextTextBoxInLogicalOrder(textBox, orderCache);
            size_t newLineIndex = textBox->lineIndex();
            unsigned differenceToPrevious = textBox->minimumCaretOffset() - previousEndDomOffset;
            // Just like when building AXTextRuns, we need to consider trimmed spaces between lines. So, if we find
            // a gap between runs, subtract one from that gap to account for a trimmed space.
            unsigned trimmedCharacterAdjustment = newLineIndex != previousLineIndex && differenceToPrevious ? 1 : 0;
            differenceBetweenDomAndRenderedOffsets += differenceToPrevious - trimmedCharacterAdjustment;
            previousEndDomOffset = textBox->maximumCaretOffset();
            previousLineIndex = newLineIndex;
        }
        ASSERT(domOffset >= differenceBetweenDomAndRenderedOffsets);
        unsigned renderedOffset = domOffset - differenceBetweenDomAndRenderedOffsets;
        return createFromRendererAndOffset(const_cast<RenderText&>(*renderText), renderedOffset);
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    // If the visible position has an anchor type referring to a node other than the anchored node, we should
    // set the text marker data with CharacterOffset so that the offset will correspond to the node.
    auto characterOffset = characterOffsetFromVisiblePosition(visiblePosition);
    if (position.anchorType() == Position::PositionIsAfterAnchor || position.anchorType() == Position::PositionIsAfterChildren)
        return textMarkerDataForCharacterOffset(characterOffset);

    CheckedPtr cache = node->document().axObjectCache();
    if (!cache)
        return std::nullopt;
    return { { *cache, visiblePosition,
        characterOffset.startIndex, characterOffset.offset, false, origin } };
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

    return WebCore::elementName(*characterOffset.node) == ElementName::HTML_br;
}

static Node* parentEditingBoundary(Node* node)
{
    if (!node)
        return nullptr;

    RefPtr documentElement = node->document().documentElement();
    if (!documentElement)
        return nullptr;

    RefPtr boundary = node;
    while (boundary != documentElement && boundary->nonShadowBoundaryParentNode() && node->hasEditableStyle() == boundary->parentNode()->hasEditableStyle())
        boundary = boundary->nonShadowBoundaryParentNode();

    return boundary.unsafeGet();
}

CharacterOffset AXObjectCache::nextBoundary(const CharacterOffset& characterOffset, BoundarySearchFunction searchFunction)
{
    if (characterOffset.isNull())
        return { };

    RefPtr boundary = parentEditingBoundary(RefPtr { characterOffset.node }.get());
    if (!boundary)
        return { };

    auto searchRange = rangeForNodeContents(*boundary);

    Vector<char16_t, 1024> string;
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
    Vector<char16_t, 1024> string;
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

    Ref node = (it.atEnd() ? searchRange : it.range()).start.container;

    // SimplifiedBackwardsTextIterator ignores replaced elements.
    // Subsequent *characterOffset.node dereferences are safe because we called CharacterOffset.isNull()
    // at the top of the method.
    if (AccessibilityObject::replacedNodeNeedsCharacter(*characterOffset.node))
        return characterOffsetForNodeAndOffset(*characterOffset.node, 0);
    RefPtr nextSibling = node->nextSibling();
    if (node.ptr() != characterOffset.node.get() && nextSibling && AccessibilityObject::replacedNodeNeedsCharacter(*nextSibling))
        return startOrEndCharacterOffsetForRange(rangeForNodeContents(*nextSibling), false);

    if ((!suffixLength && node->isTextNode() && next <= node->length()) || (node->renderer() && node->renderer()->isBR() && !next)) {
        // The next variable contains a usable index into a text node
        if (node->isTextNode())
            return traverseToOffsetInRange(rangeForNodeContents(node.get()), next, TraverseOptionValidateOffset);
        return characterOffsetForNodeAndOffset(node.get(), next, TraverseOptionIncludeStart);
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
    if (!m_document)
        return { };
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
static void filterMapForRemoval(const HashMap<T, U>& list, const Document& document, HashSet<Ref<Node>>& nodesToRemove)
{
    for (auto& entry : list)
        conditionallyAddNodeToFilterList(entry.key, document, nodesToRemove);
}

template<typename T>
static void filterListForRemoval(const ListHashSet<T>& list, const Document& document, HashSet<Ref<Node>>& nodesToRemove)
{
    for (Ref node : list)
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
    for (Ref node : list)
        conditionallyAddNodeToFilterList(node.ptr(), document, nodesToRemove);
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
    const RefPtr axObject = getOrCreate(const_cast<Element&>(element));
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

    RefPtr document = this->document();
    if (!document)
        return;

    // It's unexpected for this function to run in the middle of a render tree or style update.
    ASSERT(!Accessibility::inRenderTreeOrStyleUpdate(*document));

    if (!document->view())
        return;

    if (needsLayoutOrStyleRecalc(*document)) {
        // Layout became dirty while waiting to performDeferredCacheUpdate, and we require clean layout
        // to update the accessibility tree correctly in this function.
        if ((m_cacheUpdateDeferredCount >= 3 || forceLayout == ForceLayout::Yes) && !Accessibility::inRenderTreeOrStyleUpdate(*document)) {
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
            if (subDocument && needsLayoutOrStyleRecalc(*subDocument))
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

    AXLOGDeferredCollection("RendererChangedList"_s, m_deferredRendererChangedList);
    for (auto& object : m_deferredRendererChangedList) {
        object.updateRole();
        object.recomputeIsIgnored();
    }
    m_deferredRendererChangedList.clear();

    AXLOGDeferredCollection("RecomputeActiveSummaryList"_s, m_deferredRecomputeActiveSummaryList);
    for (auto& object : m_deferredRecomputeActiveSummaryList) {
        object.updateRole();
        object.recomputeIsIgnored();
    }
    m_deferredRecomputeActiveSummaryList.clear();

    AXLOGDeferredCollection("RecomputeTableIsExposedList"_s, m_deferredRecomputeTableIsExposedList);
    m_deferredRecomputeTableIsExposedList.forEach([this] (auto& tableElement) {
        if (RefPtr axObject = get(&tableElement))
            axObject->recomputeIsExposableIfNecessary();
    });
    m_deferredRecomputeTableIsExposedList.clear();

    AXLOGDeferredCollection("ChildrenChangedList"_s, m_deferredChildrenChangedList);
    handleAllDeferredChildrenChanged();

    AXLOGDeferredCollection("ElementAddedOrRemovedList"_s, m_deferredElementAddedOrRemovedList);
    auto elementAddedOrRemovedList = copyToVector(m_deferredElementAddedOrRemovedList);
    for (auto& weakElement : elementAddedOrRemovedList) {
        if (RefPtr element = weakElement.get()) {
            handleMenuOpened(*element);
            handleLiveRegionCreated(*element);

            if (RefPtr label = dynamicDowncast<HTMLLabelElement>(*element)) {
                // A label was added or removed. Update its LabelFor relationships.
                handleLabelChanged(getOrCreate(*label));
            }
        }
    }
    m_deferredElementAddedOrRemovedList.clear();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    AXLOGDeferredCollection("UnconnectedObjects"_s, m_deferredUnconnectedObjects);
    if (auto tree = AXIsolatedTree::treeForFrameID(m_frameID)) {
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
        Ref textFormControlElement = downcast<HTMLTextFormControlElement>(deferredFormControlContext.key);
        postTextReplacementNotificationForTextControl(textFormControlElement.get(), deferredFormControlContext.value, textFormControlElement->innerTextValue());
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
    // If we changed the focused element, that could affect what modal should be active, so recompute it.
    bool shouldRecomputeModal = m_deferredFocusedNodeChange.has_value();
    RefPtr newFocusElement = m_deferredFocusedNodeChange ? m_deferredFocusedNodeChange->second.get() : nullptr;
    m_deferredFocusedNodeChange = std::nullopt;

    AXLOGDeferredCollection("ModalChangedList"_s, m_deferredModalChangedList);
    for (Ref element : m_deferredModalChangedList) {
        if (!is<HTMLDialogElement>(element) && !hasAnyRole(element.get(), { "dialog"_s, "alertdialog"_s }))
            continue;
        shouldRecomputeModal = true;

        if (!m_modalNodesInitialized) [[unlikely]]
            findModalNodes();

        if (isModalElement(element.get())) {
            // Add the newly modified node to the modal nodes set.
            // We will recompute the current valid aria modal node in modalNode() when this node is not visible.
            m_modalElements.append(element.ptr());
        } else {
            m_modalElements.removeAllMatching([&element] (const auto& modalElement) {
                return element.ptr() == modalElement.get();
            });
        }
    }
    m_deferredModalChangedList.clear();

    if (shouldRecomputeModal)
        updateCurrentModalNode();

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
        if (auto tree = AXIsolatedTree::treeForFrameID(m_frameID)) {
            // Re-generate the subtree rooted at the webarea.
            if (RefPtr webArea = rootWebArea()) {
                AXLOG("Regenerating isolated tree from AXObjectCache::performDeferredCacheUpdate().");
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
                // m_deferredRegenerateIsolatedTree is only set when we change the active modal, which effects the ignored
                // status of every object on the page. With ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE), this means we only have
                // to re-compute is-ignored for every object, rather than re-compute all objects entirely as when this flag is off.
                tree->updatePropertiesForSelfAndDescendants(*webArea, { AXProperty::IsIgnored });
#else
                tree->generateSubtree(*webArea);
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

                // In some cases, the ID of the focus after a dialog pops up doesn't match the ID in the last focus change notification, creating a mismatch between the isolated tree cached focused object ID and the actual focused object ID.
                // For this reason, reset the focused object ID.
                if (RefPtr focus = focusedObjectForLocalFrame())
                    tree->setFocusedNodeID(focus->objectID());
            }
        }
    }
    m_deferredRegenerateIsolatedTree = false;
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

#if PLATFORM(COCOA)
    // Initialize the live region manager after the first cache update, so all existing
    // live regions are registered with their initial state before any changes occur.
    if (!m_liveRegionManagerInitialized)
        initializeLiveRegionManager();
#endif

    platformPerformDeferredCacheUpdate();
}

void AXObjectCache::handleMenuListValueChanged(Element& element)
{
    RefPtr<AccessibilityObject> object = get(&element);
    if (!object)
        return;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    updateIsolatedTree(*object, AXNotification::MenuListValueChanged);
#endif

    postPlatformNotification(*object, AXNotification::MenuListValueChanged);
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

    if (!m_frameID) {
        AXLOG("No frameID.");
        return;
    }

    auto tree = AXIsolatedTree::treeForFrameID(*m_frameID);
    if (!tree) {
        AXLOG("No isolated tree for m_frameID");
        return;
    }

    HashSet<AXID> idsWithUpdatedDependentProperties;
    auto updateDependentProperties = [&] (const Ref<AccessibilityObject>& axObject) {
        if (idsWithUpdatedDependentProperties.add(axObject->objectID()).isNewEntry)
            tree->updateDependentProperties(axObject.get());
    };

    for (const auto& notification : notifications) {
        AXLOG(notification);
        if (notification.first->isDetached())
            continue;

        switch (notification.second) {
        case AXNotification::AbbreviationChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::Abbreviation });
            break;
        case AXNotification::AccessKeyChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::AccessKey });
            break;
        case AXNotification::AutofillTypeChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::ValueAutofillButtonType });
            break;
        case AXNotification::ARIAColumnIndexChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::AXColumnIndex });
            break;
        case AXNotification::ARIAColumnIndexTextChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::AXColumnIndexText });
            break;
        case AXNotification::ARIARoleDescriptionChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::ARIARoleDescription });
            break;
        case AXNotification::ARIARowIndexChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::AXRowIndex });
            break;
        case AXNotification::ARIARowIndexTextChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::AXRowIndexText });
            break;
        case AXNotification::BrailleLabelChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::BrailleLabel });
            break;
        case AXNotification::BrailleRoleDescriptionChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::BrailleRoleDescription });
            break;
        case AXNotification::CellSlotsChanged:
            ASSERT(notification.first->isTable());
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::CellSlots });
            break;
        case AXNotification::CheckedStateChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::IsChecked });
            break;
        case AXNotification::CurrentStateChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::CurrentState });
            break;
        case AXNotification::CursorTypeChanged:
            tree->updatePropertiesForSelfAndDescendants(notification.first.get(), { AXProperty::HasCursorPointer });
            break;
        case AXNotification::ColumnCountChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::AXColumnCount });
            break;
        case AXNotification::ColumnIndexChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXProperty::ColumnIndexRange, AXProperty::ColumnIndex } });
            break;
        case AXNotification::ColumnSpanChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::ColumnIndexRange });
            break;
        case AXNotification::CommandChanged:
        case AXNotification::CommandForChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXProperty::SupportsExpanded, AXProperty::IsExpanded } });
            break;
        case AXNotification::DatetimeChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::DatetimeAttributeValue });
            break;
        case AXNotification::DisabledStateChanged:
            tree->updatePropertiesForSelfAndDescendants(notification.first.get(), { { AXProperty::CanSetFocusAttribute, AXProperty::CanSetSelectedAttribute, AXProperty::IsEnabled } });
            break;
        case AXNotification::DraggableStateChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::SupportsDragging });
            break;
        case AXNotification::ExpandedChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::IsExpanded });
            break;
        case AXNotification::ExtendedDescriptionChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXProperty::AccessibilityText, AXProperty::ExtendedDescription } });
            break;
        case AXNotification::FocusableStateChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::CanSetFocusAttribute });
            break;
        case AXNotification::FontChanged:
            tree->updatePropertiesForSelfAndDescendants(notification.first.get(), { AXProperty::Font });
            break;
        case AXNotification::HiddenStateChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::IsHiddenUntilFoundContainer });
            break;
        case AXNotification::InertOrVisibilityChanged:
            tree->updatePropertiesForSelfAndDescendants(notification.first.get(), { AXProperty::IsIgnored });
            break;
        case AXNotification::InputTypeChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::InputType });
            break;
        case AXNotification::IsEditableWebAreaChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::IsEditableWebArea });
            break;
        case AXNotification::LevelChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::ARIALevel });
            break;
        case AXNotification::MaximumValueChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXProperty::MaxValueForRange, AXProperty::ValueForRange } });
            break;
        case AXNotification::MenuListItemSelected: {
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::IsSelected });
            break;
        }
        case AXNotification::MinimumValueChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXProperty::MinValueForRange, AXProperty::ValueForRange } });
            break;
        case AXNotification::NameChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::NameAttribute });
            break;
        case AXNotification::OrientationChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::ExplicitOrientation });
            break;
        case AXNotification::PositionInSetChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXProperty::PosInSet, AXProperty::SupportsPosInSet } });
            break;
        case AXNotification::PointerEventsChanged:
            tree->updatePropertiesForSelfAndDescendants(notification.first.get(), { { AXProperty::HasCursorPointer, AXProperty::HasPointerEventsNone } });
            break;
        case AXNotification::PopoverTargetChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXProperty::SupportsExpanded, AXProperty::IsExpanded } });
            break;
        case AXNotification::SelectedTextChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::SelectedTextRange });
            break;
        case AXNotification::SortDirectionChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::SortDirection });
            break;
        case AXNotification::IdAttributeChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::IdentifierAttribute });
            break;
        case AXNotification::RadioGroupMembershipChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::RadioButtonGroupMembers });
            break;
        case AXNotification::ReadOnlyStatusChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::CanSetValueAttribute });
            break;
        case AXNotification::RequiredStatusChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::IsRequired });
            break;
        case AXNotification::RowIndexChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXProperty::RowIndexRange, AXProperty::RowIndex } });
            break;
        case AXNotification::RowSpanChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::RowIndexRange });
            break;
        case AXNotification::CellScopeChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXProperty::CellScope, AXProperty::IsColumnHeader, AXProperty::IsRowHeader } });
            break;
        //  FIXME: Contrary to the name "AXSelectedCellsChanged", this notification can be posted on a cell
        //  who has changed selected state, not just on table or grid who has changed its selected cells.
        case AXNotification::SelectedCellsChanged:
        case AXNotification::SelectedStateChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::IsSelected });
            break;
        case AXNotification::SetSizeChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXProperty::SetSize, AXProperty::SupportsSetSize } });
            break;
        case AXNotification::SpeakAsChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::SpeakAs });
            break;
        case AXNotification::StyleChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::ShowsCursorOnHover });
            break;
        case AXNotification::TextColorChanged:
            tree->updatePropertiesForSelfAndDescendants(notification.first.get(), { AXProperty::TextColor });
            break;
        case AXNotification::TextCompositionChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::TextInputMarkedTextMarkerRange });
            break;
        case AXNotification::TextUnderElementChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::AccessibilityText });
            if (notification.first->isNativeLabel() || notification.first->role() == AccessibilityRole::TextField)
                tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::StringValue });
            break;
        case AXNotification::URLChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { { AXProperty::URL, AXProperty::InternalLinkElement } });
            break;
        case AXNotification::KeyShortcutsChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::KeyShortcuts });
            break;
        case AXNotification::VisibilityChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::IsVisible });
            break;
        case AXNotification::VisitedStateChanged:
            tree->queueNodeUpdate(notification.first->objectID(), { AXProperty::IsVisited });
            break;
        case AXNotification::ActiveDescendantChanged:
        case AXNotification::RoleChanged:
        case AXNotification::ControlledObjectsChanged:
        case AXNotification::DescribedByChanged:
        case AXNotification::DropEffectChanged:
        case AXNotification::ElementBusyChanged:
        case AXNotification::FlowToChanged:
        case AXNotification::GrabbedStateChanged:
        case AXNotification::HasPopupChanged:
        case AXNotification::InvalidStatusChanged:
        case AXNotification::IsAtomicChanged:
        case AXNotification::LiveRegionStatusChanged:
        case AXNotification::LiveRegionRelevantChanged:
        case AXNotification::PlaceholderChanged:
        case AXNotification::MenuListValueChanged:
        case AXNotification::MultiSelectableStateChanged:
        case AXNotification::PressedStateChanged:
        case AXNotification::TextChanged:
        case AXNotification::TextSecurityChanged:
        case AXNotification::ValueChanged:
            tree->queueNodeUpdate(notification.first->objectID(), NodeUpdateOptions::nodeUpdate());
            break;
        case AXNotification::LabelChanged: {
            tree->queueNodeUpdate(notification.first->objectID(), NodeUpdateOptions::nodeUpdate());
            updateDependentProperties(notification.first);
            break;
        }
        case AXNotification::LanguageChanged:
        case AXNotification::RowCountChanged:
            tree->queueNodeUpdate(notification.first->objectID(), NodeUpdateOptions::nodeUpdate());
            [[fallthrough]];
        case AXNotification::RowCollapsed:
        case AXNotification::RowExpanded:
            tree->queueNodeUpdate(notification.first->objectID(), NodeUpdateOptions::childrenUpdate());
            break;
        default:
            break;
        }
    }
}

void AXObjectCache::updateIsolatedTree(AccessibilityObject* axObject, AXProperty property) const
{
    if (axObject)
        updateIsolatedTree(*axObject, property);
}

void AXObjectCache::updateIsolatedTree(AccessibilityObject& axObject, AXProperty property) const
{
    if (RefPtr tree = AXIsolatedTree::treeForFrameID(m_frameID))
        tree->queueNodeUpdate(axObject.objectID(), { property });
}

void AXObjectCache::startUpdateTreeSnapshotTimer()
{
    if (!m_updateTreeSnapshotTimer.isActive())
        m_updateTreeSnapshotTimer.startOneShot(updateTreeSnapshotTimerInterval);
}

void AXObjectCache::onPaint(const RenderObject& renderer, IntRect&& paintRect) const
{
    if (!m_frameID)
        return;

    if (std::optional axID = getAXID(const_cast<RenderObject&>(renderer))) {
        bool cachedNewRect = m_geometryManager->cacheRectIfNeeded(*axID, WTFMove(paintRect));

        if (cachedNewRect) {
            auto* renderImage = dynamicDowncast<RenderImage>(renderer);
            if (RefPtr imageMap = renderImage ? renderImage->imageMap() : nullptr) {
                // <area> elements have no renderers and thus will never be painted themselves.
                // If the image was repainted in a new location, the associated area elements
                // probably need new rects cached too.
                for (Ref area : descendantsOfType<HTMLAreaElement>(*imageMap)) {
                    if (RefPtr areaObject = get(area.get()))
                        std::ignore = m_geometryManager->cacheRectIfNeeded(areaObject->objectID(), snappedIntRect(LayoutRect(areaObject->relativeFrame())));
                }
            }
        }
    }
}

void AXObjectCache::onPaint(const Widget& widget, IntRect&& paintRect) const
{
    if (!m_frameID)
        return;
    if (std::optional axID = m_widgetIdMapping.getOptional(const_cast<Widget&>(widget)))
        std::ignore = m_geometryManager->cacheRectIfNeeded(*axID, WTFMove(paintRect));
}

void AXObjectCache::onPaint(const RenderText& renderText, size_t lineIndex)
{
    ASSERT(isMainThread());

    if (!m_frameID)
        return;

    auto ensureResult = m_mostRecentlyPaintedText.ensure(renderText, [&] {
        return LineRange(lineIndex, lineIndex);
    });
    if (!ensureResult.isNewEntry) {
        auto iterator = ensureResult.iterator;
        // The goal is for the LineRange we cache to store the first painted line, and last painted line.
        // So check the |lineIndex| we were given and adjust the cached values if necessary.
        if (iterator->value.startLineIndex > lineIndex)
            iterator->value.startLineIndex = lineIndex;

        if (iterator->value.endLineIndex < lineIndex)
            iterator->value.endLineIndex = lineIndex;
    }
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
    RefPtr tableElement = dynamicDowncast<HTMLTableElement>(element);
    if (!tableElement)
        return;

    m_deferredRecomputeTableIsExposedList.add(*tableElement);
    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}

void AXObjectCache::deferRecomputeTableCellSlots(AccessibilityNodeObject& axTable)
{
    m_deferredRecomputeTableCellSlotsList.add(axTable);
    if (!m_performCacheUpdateTimer.isActive())
        m_performCacheUpdateTimer.startOneShot(0_s);
}

void AXObjectCache::deferRowspanChange(AccessibilityObject* axObject)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    RefPtr nodeObject = dynamicDowncast<AccessibilityNodeObject>(axObject);
    if (!nodeObject || !nodeObject->isTableCell())
        return;

    m_deferredRowspanChanges.add(*nodeObject);
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

bool isVisibilityHidden(const RenderStyle& style)
{
    return style.usedVisibility() != Visibility::Visible || isContentVisibilityHidden(style);
}

// DOM component of hidden definition.
// https://www.w3.org/TR/wai-aria/#dfn-hidden
bool isRenderHidden(const RenderStyle& style)
{
    return style.display() == DisplayType::None || isVisibilityHidden(style);
}

bool isRenderHidden(const RenderStyle* style)
{
    return style ? isRenderHidden(*style) : true;
}

AccessibilityObject* AXObjectCache::rootWebArea()
{
    if (!m_document)
        return nullptr;
    RefPtr root = getOrCreate(m_document->view());
    if (!root || !root->isScrollView())
        return nullptr;
    return root->webAreaObject();
}

AXTreeData AXObjectCache::treeData(std::optional<OptionSet<AXStreamOptions>> additionalOptions)
{
    ASSERT(isMainThread());

    AXTreeData data;
    TextStream stream(TextStream::LineMode::MultipleLine);

    stream << "\nAXObjectTree:\n";
    RefPtr document = this->document();
    if (RefPtr root = document ? get(document->view()) : nullptr) {
        OptionSet<AXStreamOptions> options = { AXStreamOptions::ObjectID, AXStreamOptions::ParentID, AXStreamOptions::Role };
        if (additionalOptions)
            options |= additionalOptions.value();
        streamSubtree(stream, *root, options);
    } else
        stream << "No root!";
    data.liveTree = stream.release();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (isIsolatedTreeEnabled()) {
        stream << "\nAXIsolatedTree:\n";
        RefPtr tree = getOrCreateIsolatedTree();
        if (RefPtr root = tree ? tree->rootNode() : nullptr) {
            OptionSet<AXStreamOptions> options = { AXStreamOptions::ObjectID, AXStreamOptions::ParentID, AXStreamOptions::Role };
            if (additionalOptions)
                options |= additionalOptions.value();
            streamSubtree(stream, root.releaseNonNull(), options);
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
        commandforAttr,
        headersAttr,
        popovertargetAttr,
    };
    return relationAttributes;
}

AXRelation AXObjectCache::symmetricRelation(AXRelation relation)
{
    switch (relation) {
    case AXRelation::ActiveDescendant:
        return AXRelation::ActiveDescendantOf;
    case AXRelation::ActiveDescendantOf:
        return AXRelation::ActiveDescendant;
    case AXRelation::ControlledBy:
        return AXRelation::ControllerFor;
    case AXRelation::ControllerFor:
        return AXRelation::ControlledBy;
    case AXRelation::DescribedBy:
        return AXRelation::DescriptionFor;
    case AXRelation::DescriptionFor:
        return AXRelation::DescribedBy;
    case AXRelation::Details:
        return AXRelation::DetailsFor;
    case AXRelation::DetailsFor:
        return AXRelation::Details;
    case AXRelation::ErrorMessage:
        return AXRelation::ErrorMessageFor;
    case AXRelation::ErrorMessageFor:
        return AXRelation::ErrorMessage;
    case AXRelation::FlowsFrom:
        return AXRelation::FlowsTo;
    case AXRelation::FlowsTo:
        return AXRelation::FlowsFrom;
    case AXRelation::Headers:
        return AXRelation::HeaderFor;
    case AXRelation::HeaderFor:
        return AXRelation::Headers;
    case AXRelation::LabeledBy:
        return AXRelation::LabelFor;
    case AXRelation::LabelFor:
        return AXRelation::LabeledBy;
    case AXRelation::OwnedBy:
        return AXRelation::OwnerFor;
    case AXRelation::OwnerFor:
        return AXRelation::OwnedBy;
    case AXRelation::None:
        return AXRelation::None;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

AXRelation AXObjectCache::attributeToRelationType(const QualifiedName& attribute)
{
    if (attribute == aria_activedescendantAttr)
        return AXRelation::ActiveDescendant;
    if (attribute == aria_controlsAttr || attribute == commandforAttr || attribute == popovertargetAttr)
        return AXRelation::ControllerFor;
    if (attribute == aria_describedbyAttr)
        return AXRelation::DescribedBy;
    if (attribute == aria_detailsAttr)
        return AXRelation::Details;
    if (attribute == aria_errormessageAttr)
        return AXRelation::ErrorMessage;
    if (attribute == aria_flowtoAttr)
        return AXRelation::FlowsTo;
    if (attribute == aria_labelledbyAttr || attribute == aria_labeledbyAttr)
        return AXRelation::LabeledBy;
    if (attribute == aria_ownsAttr)
        return AXRelation::OwnerFor;
    if (attribute == headersAttr)
        return AXRelation::Headers;
    return AXRelation::None;
}

static bool validRelation(void* origin, void* target, AXRelation relation)
{
    if (!origin || !target || relation == AXRelation::None)
        return false;
    return origin != target || relation == AXRelation::LabeledBy;
}

static bool validRelation(Element& origin, Element& target, AXRelation relation)
{
    if (relation == AXRelation::None)
        return false;
    return &origin != &target || relation == AXRelation::LabeledBy;
}

bool AXObjectCache::addRelation(Element& origin, Element& target, AXRelation relation)
{
    AXTRACE("AXObjectCache::addRelation"_s);
    AXLOG(makeString("origin: "_s, origin.debugDescription(), " target: "_s, target.debugDescription(), " relation "_s, static_cast<uint8_t>(relation)));

    if (!validRelation(origin, target, relation)) {
        ASSERT_NOT_REACHED();
        return false;
    }

    if (relation == AXRelation::LabelFor) {
        // Add a LabelFor relation if the target doesn't have an ARIA label which should take precedence.
        if (hasAnyARIALabelling(target))
            return false;
    }

    return addRelation(RefPtr { getOrCreate(origin, IsPartOfRelation::Yes) }.get(), RefPtr { getOrCreate(target, IsPartOfRelation::Yes) }.get(), relation);
}

static bool canHaveRelations(Element& element)
{
    auto elementName = element.elementName();
    return !(elementName == ElementName::HTML_meta || elementName == ElementName::HTML_head || elementName == ElementName::HTML_script || elementName == ElementName::HTML_html || elementName == ElementName::HTML_style);
}

static bool relationCausesCycle(AccessibilityObject* origin, AccessibilityObject* target, AXRelation relation)
{
    // Validate that we're not creating an aria-owns cycle.
    if (relation == AXRelation::OwnerFor) {
        for (auto* verifyOrigin = origin; verifyOrigin; verifyOrigin = verifyOrigin->parentObject()) {
            if (verifyOrigin == target)
                return true;
        }
    } else if (relation == AXRelation::OwnedBy) {
        for (auto* verifyTarget = target; verifyTarget; verifyTarget = verifyTarget->parentObject()) {
            if (verifyTarget == origin)
                return true;
        }
    }

    return false;
}

bool AXObjectCache::addRelation(AccessibilityObject* origin, AccessibilityObject* target, AXRelation relation, AddSymmetricRelation addSymmetricRelation)
{
    AXTRACE("AXObjectCache::addRelation"_s);
    AXLOG(origin);
    AXLOG(target);
    AXLOG(relation);

    if (!validRelation(origin, target, relation))
        return false;

    if (relationCausesCycle(origin, target, relation))
        return false;

    if (relation == AXRelation::OwnerFor) {
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
        m_relations.add(originID, AXRelations { { enumToUnderlyingType(relation), { targetID } } });
    } else if (auto targetsIterator = relationsIterator->value.find(enumToUnderlyingType(relation)); targetsIterator == relationsIterator->value.end()) {
        // No relation of this type for this object, add the first one.
        relationsIterator->value.add(enumToUnderlyingType(relation), ListHashSet { targetID });
    } else {
        // There are already relations of this type for the object. Add the new relation.
        if (relation == AXRelation::ActiveDescendant
            || relation == AXRelation::OwnedBy) {
            // There should be only one active descendant and only one owner. Enforce that by removing any existing targets.
            targetsIterator->value.clear();
        }
        targetsIterator->value.add(targetID);
    }
    m_relationTargets.add(targetID);

    if (relation == AXRelation::OwnerFor) {
        // First find and clear the old owner.
        for (auto oldOwnerIterator = m_relations.begin(); oldOwnerIterator != m_relations.end(); ++oldOwnerIterator) {
            if (oldOwnerIterator->key == originID)
                continue;

            removeRelationByID(oldOwnerIterator->key, targetID, AXRelation::OwnerFor);
        }

        childrenChanged(origin);
    } else if (relation == AXRelation::OwnedBy) {
        if (RefPtr parentObject = origin->parentObjectUnignored())
            childrenChanged(parentObject.get());
    }

    if (addSymmetricRelation == AddSymmetricRelation::Yes
        && m_objects.contains(originID) && m_objects.contains(targetID)) {
        // If the IDs are still in the m_objects map, the objects should be still alive.
        if (auto symmetric = symmetricRelation(relation); symmetric != AXRelation::None)
            addRelation(target, origin, symmetric, AddSymmetricRelation::No);

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        if (auto tree = AXIsolatedTree::treeForFrameID(m_frameID)) {
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

    for (auto relation : it->value.keys()) {
        auto symmetric = symmetricRelation(static_cast<AXRelation>(relation));
        if (symmetric == AXRelation::None)
            continue;

        auto targetIDs = it->value.get(static_cast<uint8_t>(relation));
        for (AXID targetID : targetIDs)
            removeRelationByID(targetID, axID, symmetric);
    }

    m_relations.remove(it);
    dirtyIsolatedTreeRelations();
}

bool AXObjectCache::removeRelation(Element& origin, AXRelation relation)
{
    AXTRACE(makeString("AXObjectCache::removeRelations for "_s, origin.debugDescription()));
    AXLOG(relation);

    RefPtr object = get(&origin);
    if (!object)
        return false;

    auto relationsIterator = m_relations.find(object->objectID());
    if (relationsIterator == m_relations.end())
        return false;

    auto targetIDs = relationsIterator->value.take(enumToUnderlyingType(relation));
    bool removedRelation = !targetIDs.isEmpty();

    auto symmetric = symmetricRelation(relation);
    if (symmetric != AXRelation::None) {
        for (AXID targetID : targetIDs)
            removeRelationByID(targetID, object->objectID(), symmetric);
    }

    if (removedRelation && relation == AXRelation::OwnerFor) {
        childrenChanged(object.get());
        // We also need to notify the object's natural parent it has changed children.
        RefPtr node = object->node();
        if (RefPtr parentNode = node ? composedParentIgnoringDocumentFragments(*node) : nullptr)
            childrenChanged(get(*parentNode));
        else if (CheckedPtr renderer = object->renderer())
            childrenChanged(get(renderer->parent()));
    }

    return removedRelation;
}

void AXObjectCache::removeRelationByID(AXID originID, AXID targetID, AXRelation relation)
{
    AXTRACE("AXObjectCache::removeRelationByID"_s);
    AXLOG(makeString("originID "_s, originID.loggingString(), " targetID "_s, targetID.loggingString()));
    AXLOG(relation);

    auto relationsIterator = m_relations.find(originID);
    if (relationsIterator == m_relations.end())
        return;

    auto targetsIterator = relationsIterator->value.find(enumToUnderlyingType(relation));
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
    if (m_document)
        updateRelationsForTree(m_document->rootNode());
}

void AXObjectCache::updateRelationsForTree(ContainerNode& rootNode)
{
    ASSERT(!rootNode.parentNode());
    for (Ref element : descendantsOfType<Element>(rootNode)) {
        if (!canHaveRelations(element.get()))
            continue;

        if (RefPtr shadowRoot = element->shadowRoot(); shadowRoot && shadowRoot->mode() != ShadowRootMode::UserAgent)
            updateRelationsForTree(*shadowRoot);
        if (RefPtr frameOwnerElement = dynamicDowncast<HTMLFrameOwnerElement>(element)) {
            if (RefPtr document = frameOwnerElement->contentDocument())
                updateRelationsForTree(*document);
        }

        for (const auto& attribute : relationAttributes())
            addRelation(element.get(), attribute);

        // In addition to ARIA specified relations, there may be other relevant relations.
        // For instance, LabelFor in HTMLLabelElements.
        addLabelForRelation(element.get());
    }
}

bool AXObjectCache::addRelation(Element& origin, const QualifiedName& attribute)
{
    if (attribute == aria_labeledbyAttr && origin.hasAttribute(aria_labelledbyAttr)) {
        // The attribute name with British spelling should override the one with American spelling.
        return false;
    }

    bool addedRelation = false;
    auto relation = attributeToRelationType(attribute);
    if (!m_document)
        return false;
    if (Element::isElementReflectionAttribute(Ref { m_document->settings() }, attribute)) {
        if (auto reflectedElement = origin.elementForAttributeInternal(attribute))
            return addRelation(origin, *reflectedElement, relation);
    } else if (Element::isElementsArrayReflectionAttribute(attribute)) {
        if (auto reflectedElements = origin.elementsArrayForAttributeInternal(attribute)) {
            for (auto reflectedElement : reflectedElements.value()) {
                if (addRelation(origin, reflectedElement, relation))
                    addedRelation = true;
            }
            return addedRelation;
        }
    }

    auto& value = origin.attributeWithoutSynchronization(attribute);
    if (value.isNull()) {
        if (auto* defaultARIA = origin.customElementDefaultARIAIfExists()) {
            for (auto& target : defaultARIA->elementsForAttribute(origin, attribute)) {
                if (addRelation(origin, target, relation))
                    addedRelation = true;
            }
        }
        return addedRelation;
    }

    SpaceSplitString ids(value, SpaceSplitString::ShouldFoldCase::No);
    for (auto& id : ids) {
        RefPtr target = origin.treeScope().elementByIdResolvingReferenceTarget(id);
        if (!target || target == &origin)
            continue;

        if (addRelation(origin, *target, relation))
            addedRelation = true;
    }

    return addedRelation;
}

void AXObjectCache::addLabelForRelation(Element& origin)
{
    bool addedRelation = false;

    // LabelFor relations are established for <label for=...>.
    if (RefPtr label = dynamicDowncast<HTMLLabelElement>(origin)) {
        if (RefPtr control = Accessibility::controlForLabelElement(*label))
            addedRelation = addRelation(origin, *control, AXRelation::LabelFor);
    }

    if (addedRelation)
        dirtyIsolatedTreeRelations();
}

void AXObjectCache::updateRelations(Element& origin, const QualifiedName& attribute)
{
    if (!canHaveRelations(origin))
        return;

    auto relation = attributeToRelationType(attribute);
    if (relation == AXRelation::None) {
        ASSERT_NOT_REACHED();
        return;
    }

    // `commandfor` is only valid for button elements.
    if (attribute == commandforAttr) [[unlikely]] {
        if (!is<HTMLButtonElement>(origin))
            return;
    }

    // `popovertarget` is only valid for input and button elements.
    if (attribute == popovertargetAttr) [[unlikely]] {
        if (!is<HTMLInputElement>(origin) && !is<HTMLButtonElement>(origin))
            return;
    }

    bool changedRelation = removeRelation(origin, relation);
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

bool AXObjectCache::isDescendantOfRelatedNode(Node& node)
{
    auto& targetIDs = relationTargetIDs();
    for (RefPtr parent = node.parentNode(); parent; parent = parent->parentNode()) {
        RefPtr object = get(*parent);
        if (object && (m_relations.contains(object->objectID()) || targetIDs.contains(object->objectID())))
            return true;
    }
    return false;
}

std::optional<ListHashSet<AXID>> AXObjectCache::relatedObjectIDsFor(const AXCoreObject& object, AXRelation relation, UpdateRelations updateRelations)
{
    if (updateRelations == UpdateRelations::Yes)
        updateRelationsIfNeeded();

    auto relationsIterator = m_relations.find(object.objectID());
    if (relationsIterator == m_relations.end())
        return std::nullopt;

    auto targetsIterator = relationsIterator->value.find(enumToUnderlyingType(relation));
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

#if !PLATFORM(COCOA) && !USE(ATSPI)
AXTextChange AXObjectCache::textChangeForEditType(AXTextEditType type)
{
    switch (type) {
    case AXTextEditTypeCut:
    case AXTextEditTypeDelete:
        return AXTextChange::Deleted;
    case AXTextEditTypeInsert:
    case AXTextEditTypeDictation:
    case AXTextEditTypeTyping:
    case AXTextEditTypePaste:
        return AXTextChange::Inserted;
    case AXTextEditTypeReplace:
        return AXTextChange::Replaced;
    case AXTextEditTypeAttributesChange:
        return AXTextChange::AttributesChanged;
    case AXTextEditTypeUnknown:
        ASSERT_NOT_REACHED();
        return AXTextChange::Inserted;
    }
}
#endif

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXObjectCache::selectedTextRangeTimerFired()
{
    if (!accessibilityEnabled())
        return;

    m_selectedTextRangeTimer.stop();

    if (m_lastDebouncedTextRangeObject) {
        for (RefPtr axObject = objectForID(*m_lastDebouncedTextRangeObject); axObject; axObject = axObject->parentObject()) {
            if (axObject->isTextControl())
                postNotification(*axObject, AXNotification::SelectedTextChanged);
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
    if (auto tree = AXIsolatedTree::treeForFrameID(m_frameID))
        tree->processQueuedNodeUpdates();
}
#endif

void AXObjectCache::onWidgetVisibilityChanged(RenderWidget& widget)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    postNotification(get(widget), AXNotification::VisibilityChanged);
#else
    UNUSED_PARAM(widget);
#endif
}

#if PLATFORM(MAC)
bool AXObjectCache::isAppleInternalInstall()
{
    static bool isInternal = os_variant_allows_internal_security_policies("com.apple.Accessibility");
    return isInternal;
}
#endif // PLATFORM(COCOA)

} // namespace WebCore
