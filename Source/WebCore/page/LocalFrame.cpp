/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Google Inc.
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
 */

#include "config.h"
#include "LocalFrame.h"

#include "AdjustViewSize.h"
#include "AnimationTimelinesController.h"
#include "ApplyStyleCommand.h"
#include "BackForwardCache.h"
#include "BackForwardController.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "CSSValuePool.h"
#include "CachedCSSStyleSheet.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "DocumentLoader.h"
#include "DocumentQuirks.h"
#include "DocumentResourceLoader.h"
#include "DocumentSyncClient.h"
#include "DocumentType.h"
#include "DocumentView.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "ElementInlines.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FloatQuad.h"
#include "FocusController.h"
#include "FrameConsoleClient.h"
#include "FrameDestructionObserver.h"
#include "FrameInspectorController.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "HTMLAttachmentElement.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableRowElement.h"
#include "HitTestResult.h"
#include "ImageBuffer.h"
#include "InspectorInstrumentation.h"
#include "JSDOMWindow.h"
#include "JSNode.h"
#include "JSServiceWorkerGlobalScope.h"
#include "JSWindowProxy.h"
#include "LocalDOMWindow.h"
#include "LocalFrameLoaderClient.h"
#include "LocalFrameView.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "Navigator.h"
#include "NodeList.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "PaymentSession.h"
#include "ProcessWarming.h"
#include "RemoteFrame.h"
#include "RenderLayerCompositor.h"
#include "RenderStyleInlines.h"
#include "RenderTableCell.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidgetInlines.h"
#include "ResourceMonitor.h"
#include "SVGDocument.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementTypeHelpers.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "ScrollingCoordinator.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerGlobalScope.h"
#include "Settings.h"
#include "StyleProperties.h"
#include "StyleScope.h"
#include "TextNodeTraversal.h"
#include "TextResourceDecoder.h"
#include "UserContentController.h"
#include "UserContentURLPattern.h"
#include "UserGestureIndicator.h"
#include "UserScript.h"
#include "UserTypingGestureIndicator.h"
#include "VisibleUnits.h"
#include "markup.h"
#include "runtime_root.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/RegularExpression.h>
#include <wtf/HexNumber.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

#if ENABLE(DATA_DETECTION)
#include "DataDetectionResultsStorage.h"
#endif

#if ENABLE(CONTENT_EXTENSIONS) && USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/LocalFrameAdditions.h>)
#include <WebKitAdditions/LocalFrameAdditions.h>
#endif

#define FRAME_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - Frame::" fmt, this, ##__VA_ARGS__)
#define FRAME_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - Frame::" fmt, this, ##__VA_ARGS__)

namespace WebCore {

using namespace HTMLNames;

#if PLATFORM(IOS_FAMILY)
static const Seconds scrollFrequency { 1000_s / 60. };
#endif

struct OverrideScreenSize {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(OverrideScreenSize);

    FloatSize size;
};

static inline float parentPageZoomFactor(LocalFrame* frame)
{
    SUPPRESS_UNCOUNTED_LOCAL auto* parent = dynamicDowncast<LocalFrame>(frame->tree().parent());
    if (!parent)
        return 1;
    return parent->pageZoomFactor();
}

static inline float parentTextZoomFactor(LocalFrame* frame)
{
    SUPPRESS_UNCOUNTED_LOCAL auto* parent = dynamicDowncast<LocalFrame>(frame->tree().parent());
    if (!parent)
        return 1;
    return parent->textZoomFactor();
}

static const LocalFrame& rootFrame(const LocalFrame& frame, Frame* parent)
{
    SUPPRESS_UNCOUNTED_LOCAL auto* localParent = dynamicDowncast<LocalFrame>(parent);
    if (localParent)
        return localParent->rootFrame();
    ASSERT(is<RemoteFrame>(parent) || frame.isMainFrame());
    return frame;
}

LocalFrame::LocalFrame(Page& page, ClientCreator&& clientCreator, FrameIdentifier identifier, SandboxFlags sandboxFlags, ReferrerPolicy referrerPolicy, std::optional<ScrollbarMode> scrollingMode, HTMLFrameOwnerElement* ownerElement, Frame* parent, Frame* opener, Ref<FrameTreeSyncData>&& frameTreeSyncData, AddToFrameTree addToFrameTree)
    : Frame(page, identifier, FrameType::Local, ownerElement, parent, opener, WTFMove(frameTreeSyncData), addToFrameTree)
    , m_loader(makeUniqueRefWithoutRefCountedCheck<FrameLoader>(*this, WTFMove(clientCreator)))
    , m_script(makeUniqueRef<ScriptController>(*this))
#if PLATFORM(IOS_FAMILY)
    , m_viewportArguments(makeUniqueRef<ViewportArguments>())
    , m_rangedSelectionBase(makeUniqueRef<VisibleSelection>())
    , m_rangedSelectionInitialExtent(makeUniqueRef<VisibleSelection>())
#endif
    , m_pageZoomFactor(parentPageZoomFactor(this))
    , m_textZoomFactor(parentTextZoomFactor(this))
    , m_rootFrame(WebCore::rootFrame(*this, parent))
    , m_sandboxFlags(sandboxFlags)
    , m_parentFrameOrOpenerReferrerPolicy(referrerPolicy)
    , m_eventHandler(makeUniqueRef<EventHandler>(*this))
    , m_inspectorController(makeUniqueRefWithoutRefCountedCheck<FrameInspectorController>(*this))
    , m_consoleClient(makeUniqueRef<FrameConsoleClient>(*this))
{
    ProcessWarming::initializeNames();
    StaticCSSValuePool::init();

    if (RefPtr localMainFrame = this->localMainFrame(); localMainFrame && parent)
        localMainFrame->selfOnlyRef();

    ASSERT(scrollingMode.has_value() ^ !!ownerElement);
    m_scrollingMode = scrollingMode ? *scrollingMode : ownerElement->scrollingMode();

    // Pause future ActiveDOMObjects if this frame is being created while the page is in a paused state.
    if (RefPtr parent = dynamicDowncast<LocalFrame>(tree().parent()); parent && parent->activeDOMObjectsAndAnimationsSuspended())
        suspendActiveDOMObjectsAndAnimations();

    if (isRootFrame())
        page.addRootFrame(*this);

    ASSERT(isRootFrameIdentifier(frameID()) == isRootFrame());
}

void LocalFrame::init()
{
    loader().init();
}

Ref<LocalFrame> LocalFrame::createMainFrame(Page& page, ClientCreator&& clientCreator, FrameIdentifier identifier, SandboxFlags effectiveSandboxFlags, ReferrerPolicy effectiveReferrerPolicy, Frame* opener, Ref<FrameTreeSyncData>&& frameTreeSyncData)
{
    return adoptRef(*new LocalFrame(page, WTFMove(clientCreator), identifier, effectiveSandboxFlags, effectiveReferrerPolicy, ScrollbarMode::Auto, nullptr, nullptr, opener, WTFMove(frameTreeSyncData)));
}

Ref<LocalFrame> LocalFrame::createSubframe(Page& page, ClientCreator&& clientCreator, FrameIdentifier identifier, SandboxFlags effectiveSandboxFlags, ReferrerPolicy effectiveReferrerPolicy, HTMLFrameOwnerElement& ownerElement, Ref<FrameTreeSyncData>&& frameTreeSyncData)
{
    return adoptRef(*new LocalFrame(page, WTFMove(clientCreator), identifier, effectiveSandboxFlags, effectiveReferrerPolicy, std::nullopt, &ownerElement, ownerElement.document().frame(), nullptr, WTFMove(frameTreeSyncData)));
}

Ref<LocalFrame> LocalFrame::createProvisionalSubframe(Page& page, ClientCreator&& clientCreator, FrameIdentifier identifier, SandboxFlags effectiveSandboxFlags, ReferrerPolicy effectiveReferrerPolicy, ScrollbarMode scrollingMode, Frame& parent, Ref<FrameTreeSyncData>&& frameTreeSyncData)
{
    return adoptRef(*new LocalFrame(page, WTFMove(clientCreator), identifier, effectiveSandboxFlags, effectiveReferrerPolicy, scrollingMode, nullptr, &parent, nullptr, WTFMove(frameTreeSyncData), AddToFrameTree::No));
}

LocalFrame::~LocalFrame()
{
    setView(nullptr);

    m_inspectorController->inspectedFrameDestroyed();

    Ref loader = this->loader();
    if (!loader->isComplete())
        loader->closeURL();

    loader->clear(protectedDocument(), false);
    checkedScript()->updatePlatformScriptObjects();

    // FIXME: We should not be doing all this work inside the destructor

    disconnectOwnerElement();

    while (RefPtr destructionObserver = m_destructionObservers.takeAny())
        destructionObserver->frameDestroyed();

    RefPtr localMainFrame = this->localMainFrame();
    if (!isMainFrame() && localMainFrame)
        localMainFrame->selfOnlyDeref();

    detachFromPage();
}

RefPtr<const LocalFrame> LocalFrame::localMainFrame() const
{
    return dynamicDowncast<const LocalFrame>(mainFrame());
}

RefPtr<LocalFrame> LocalFrame::localMainFrame()
{
    return dynamicDowncast<LocalFrame>(mainFrame());
}

void LocalFrame::addDestructionObserver(FrameDestructionObserver& observer)
{
    m_destructionObservers.add(observer);
}

void LocalFrame::removeDestructionObserver(FrameDestructionObserver& observer)
{
    m_destructionObservers.remove(observer);
}

void LocalFrame::setView(RefPtr<LocalFrameView>&& view)
{
    // We the custom scroll bars as early as possible to prevent m_doc->detach()
    // from messing with the view such that its scroll bars won't be torn down.
    // FIXME: We should revisit this.
    if (RefPtr view = m_view)
        view->prepareForDetach();

    // Prepare for destruction now, so any unload event handlers get run and the LocalDOMWindow is
    // notified. If we wait until the view is destroyed, then things won't be hooked up enough for
    // these calls to work.
    if (!view && m_doc && m_doc->backForwardCacheState() != Document::InBackForwardCache)
        protectedDocument()->willBeRemovedFromFrame();
    
    if (RefPtr view = m_view)
        view->checkedLayoutContext()->unscheduleLayout();
    
    m_eventHandler->clear();

    RELEASE_ASSERT(!m_doc || !m_doc->hasLivingRenderTree());

    m_view = WTFMove(view);
    
    // Only one form submission is allowed per view of a part.
    // Since this part may be getting reused as a result of being
    // pulled from the back/forward cache, reset this flag.
    loader().resetMultipleFormSubmissionProtection();
}

void LocalFrame::setDocument(RefPtr<Document>&& newDocument)
{
    ASSERT(!newDocument || newDocument->frame() == this);

    if (m_documentIsBeingReplaced)
        return;

    m_documentIsBeingReplaced = true;

    if (isMainFrame()) {
        if (RefPtr page = this->page())
            page->didChangeMainDocument(newDocument.get());
        loader().client().dispatchDidChangeMainDocument();
    }

    if (RefPtr previousDocument = m_doc) {
#if ENABLE(ATTACHMENT_ELEMENT)
        for (Ref attachment : previousDocument->attachmentElementsByIdentifier().values())
            protectedEditor()->didRemoveAttachmentElement(attachment);
#endif

        if (previousDocument->backForwardCacheState() != Document::InBackForwardCache)
            previousDocument->willBeRemovedFromFrame();
    }

    m_doc = newDocument.copyRef();
    ASSERT(!m_doc || m_doc->window());
    ASSERT(!m_doc || m_doc->window()->frame() == this);

    // Don't use m_doc because it can be overwritten and we want to guarantee
    // that the document is not destroyed during this function call.
    if (newDocument)
        newDocument->didBecomeCurrentDocumentInFrame();

#if ENABLE(ATTACHMENT_ELEMENT)
    if (RefPtr document = m_doc) {
        Ref editor = this->editor();
        for (Ref attachment : document->attachmentElementsByIdentifier().values())
            editor->didInsertAttachmentElement(attachment);
    }
#endif

    if (RefPtr page = this->page(); page && isMainFrame()) {
        if (m_doc && !loader().stateMachine().isDisplayingInitialEmptyDocument())
            page->mainFrameDidChangeToNonInitialEmptyDocument();
        page->clearAXObjectCache();
    }

    InspectorInstrumentation::frameDocumentUpdated(*this);

#if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)
    m_accessedWindowProxyPropertiesViaOpener = { };
#endif

    m_documentIsBeingReplaced = false;
}

void LocalFrame::frameDetached()
{
    loader().frameDetached();
}

bool LocalFrame::preventsParentFromBeingComplete() const
{
    return !loader().isComplete() && (!ownerElement() || !ownerElement()->isLazyLoadObserverActive());
}

void LocalFrame::changeLocation(FrameLoadRequest&& request)
{
    loader().changeLocation(WTFMove(request));
}

void LocalFrame::loadFrameRequest(FrameLoadRequest&& request, Event* event)
{
    loader().loadFrameRequest(WTFMove(request), event, { });
}

void LocalFrame::didFinishLoadInAnotherProcess()
{
    loader().provisionalLoadFailedInAnotherProcess();
}

void LocalFrame::invalidateContentEventRegionsIfNeeded(InvalidateContentEventRegionsReason reason)
{
    if (!page() || !m_doc || !m_doc->renderView())
        return;

    bool needsUpdateForTouchEventHandlers = false;
    bool needsUpdateForWheelEventHandlers = false;
    bool needsUpdateForTouchActionElements = false;
    bool needsUpdateForEditableElements = false;
    bool needsUpdateForInteractionRegions = false;
#if ENABLE(WHEEL_EVENT_REGIONS)
    needsUpdateForWheelEventHandlers = m_doc->hasWheelEventHandlers() || reason == InvalidateContentEventRegionsReason::EventHandlerChange;
#else
    UNUSED_PARAM(reason);
#endif
#if ENABLE(TOUCH_EVENT_REGIONS)
    needsUpdateForTouchEventHandlers = m_doc->hasTouchEventHandlers() || reason == InvalidateContentEventRegionsReason::EventHandlerChange;
#else
    UNUSED_PARAM(reason);
#endif

#if ENABLE(TOUCH_ACTION_REGIONS)
    // Document::mayHaveElementsWithNonAutoTouchAction never changes from true to false currently.
    needsUpdateForTouchActionElements = m_doc->mayHaveElementsWithNonAutoTouchAction();
#endif
#if ENABLE(EDITABLE_REGION)
    // Document::mayHaveEditableElements never changes from true to false currently.
    needsUpdateForEditableElements = m_doc->mayHaveEditableElements() && page()->shouldBuildEditableRegion();
#endif
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    needsUpdateForInteractionRegions = page()->shouldBuildInteractionRegions();
#endif

    if (!needsUpdateForTouchActionElements && !needsUpdateForEditableElements && !needsUpdateForWheelEventHandlers && !needsUpdateForInteractionRegions && !needsUpdateForTouchEventHandlers)
        return;

    if (!m_doc->renderView()->compositor().viewNeedsToInvalidateEventRegionOfEnclosingCompositingLayerForRepaint())
        return;

    if (RefPtr ownerElement = this->ownerElement())
        ownerElement->protectedDocument()->invalidateEventRegionsForFrame(*ownerElement);
}

#if ENABLE(ORIENTATION_EVENTS)
void LocalFrame::orientationChanged()
{
    Page::forEachDocumentFromMainFrame(*this, [newOrientation = orientation()] (Document& document) {
        document.orientationChanged(newOrientation);
    });
}

IntDegrees LocalFrame::orientation() const
{
    if (RefPtr page = this->page())
        return page->chrome().client().deviceOrientation();
    return 0;
}
#endif // ENABLE(ORIENTATION_EVENTS)

static JSC::Yarr::RegularExpression createRegExpForLabels(const Vector<String>& labels)
{
    // REVIEW- version of this call in FrameMac.mm caches based on the NSArray ptrs being
    // the same across calls.  We can't do that.

    static NeverDestroyed<JSC::Yarr::RegularExpression> wordRegExp("\\w"_s);
    StringBuilder pattern;
    pattern.append('(');
    for (unsigned i = 0, numLabels = labels.size(); i < numLabels; i++) {
        auto& label = labels[i];

        bool startsWithWordCharacter = false;
        bool endsWithWordCharacter = false;
        if (label.length()) {
            StringView labelView { label };
            startsWithWordCharacter = wordRegExp.get().match(labelView.left(1)) >= 0;
            endsWithWordCharacter = wordRegExp.get().match(labelView.right(1)) >= 0;
        }

        // Search for word boundaries only if label starts/ends with "word characters".
        // If we always searched for word boundaries, this wouldn't work for languages such as Japanese.
        pattern.append(i ? "|"_s : ""_s, startsWithWordCharacter ? "\\b"_s : ""_s, label, endsWithWordCharacter ? "\\b"_s : ""_s);
    }
    pattern.append(')');
    return JSC::Yarr::RegularExpression(pattern.toString(), { JSC::Yarr::Flags::IgnoreCase });
}

String LocalFrame::searchForLabelsAboveCell(const JSC::Yarr::RegularExpression& regExp, HTMLTableCellElement* cell, size_t* resultDistanceFromStartOfCell)
{
    if (RefPtr aboveCell = cell->cellAbove()) {
        // search within the above cell we found for a match
        size_t lengthSearched = 0;
        for (RefPtr textNode = TextNodeTraversal::firstWithin(*aboveCell); textNode; textNode = TextNodeTraversal::next(*textNode, aboveCell.get())) {
            if (!textNode->renderer() || textNode->renderer()->style().usedVisibility() != Visibility::Visible)
                continue;
            // For each text chunk, run the regexp
            String nodeString = textNode->data();
            int pos = regExp.searchRev(nodeString);
            if (pos >= 0) {
                if (resultDistanceFromStartOfCell)
                    *resultDistanceFromStartOfCell = lengthSearched;
                return nodeString.substring(pos, regExp.matchedLength());
            }
            lengthSearched += nodeString.length();
        }
    }

    // Any reason in practice to search all cells in that are above cell?
    if (resultDistanceFromStartOfCell)
        *resultDistanceFromStartOfCell = notFound;
    return String();
}

// FIXME: This should take an Element&.
String LocalFrame::searchForLabelsBeforeElement(const Vector<String>& labels, Element* element, size_t* resultDistance, bool* resultIsInCellAbove)
{
    ASSERT(element);
    JSC::Yarr::RegularExpression regExp = createRegExpForLabels(labels);
    // We stop searching after we've seen this many chars
    const unsigned int charsSearchedThreshold = 500;
    // This is the absolute max we search.  We allow a little more slop than
    // charsSearchedThreshold, to make it more likely that we'll search whole nodes.
    const unsigned int maxCharsSearched = 600;
    // If the starting element is within a table, the cell that contains it
    RefPtr<HTMLTableCellElement> startingTableCell;
    bool searchedCellAbove = false;

    if (resultDistance)
        *resultDistance = notFound;
    if (resultIsInCellAbove)
        *resultIsInCellAbove = false;
    
    // walk backwards in the node tree, until another element, or form, or end of tree
    int unsigned lengthSearched = 0;
    RefPtr<Node> n;
    for (n = NodeTraversal::previous(*element); n && lengthSearched < charsSearchedThreshold; n = NodeTraversal::previous(*n)) {
        // We hit another form element or the start of the form - bail out
        if (is<HTMLFormElement>(*n))
            break;

        if (RefPtr element = dynamicDowncast<Element>(*n); element && element->isValidatedFormListedElement())
            break;

        if (n->hasTagName(tdTag) && !startingTableCell)
            startingTableCell = downcast<HTMLTableCellElement>(n);
        else if (is<HTMLTableRowElement>(*n) && startingTableCell) {
            String result = searchForLabelsAboveCell(regExp, startingTableCell.get(), resultDistance);
            if (!result.isEmpty()) {
                if (resultIsInCellAbove)
                    *resultIsInCellAbove = true;
                return result;
            }
            searchedCellAbove = true;
        } else if (CheckedPtr renderText = dynamicDowncast<RenderText>(n->renderer()); renderText && renderText->style().usedVisibility() == Visibility::Visible) {
            // For each text chunk, run the regexp
            String nodeString = n->nodeValue();
            // add 100 for slop, to make it more likely that we'll search whole nodes
            if (lengthSearched + nodeString.length() > maxCharsSearched)
                nodeString = nodeString.right(charsSearchedThreshold - lengthSearched);
            int pos = regExp.searchRev(nodeString);
            if (pos >= 0) {
                if (resultDistance)
                    *resultDistance = lengthSearched;
                return nodeString.substring(pos, regExp.matchedLength());
            }
            lengthSearched += nodeString.length();
        }
    }

    // If we started in a cell, but bailed because we found the start of the form or the
    // previous element, we still might need to search the row above us for a label.
    if (startingTableCell && !searchedCellAbove) {
        String result = searchForLabelsAboveCell(regExp, startingTableCell.get(), resultDistance);
        if (!result.isEmpty()) {
            if (resultIsInCellAbove)
                *resultIsInCellAbove = true;
            return result;
        }
    }
    return String();
}

static String matchLabelsAgainstString(const Vector<String>& labels, const String& stringToMatch)
{
    if (stringToMatch.isEmpty())
        return String();

    String mutableStringToMatch = stringToMatch;

    // Make numbers and _'s in field names behave like word boundaries, e.g., "address2"
    replace(mutableStringToMatch, JSC::Yarr::RegularExpression("\\d"_s), " "_s);
    mutableStringToMatch = makeStringByReplacingAll(mutableStringToMatch, '_', ' ');
    
    JSC::Yarr::RegularExpression regExp = createRegExpForLabels(labels);
    // Use the largest match we can find in the whole string
    int pos;
    int length;
    int bestPos = -1;
    int bestLength = -1;
    int start = 0;
    do {
        pos = regExp.match(mutableStringToMatch, start);
        if (pos != -1) {
            length = regExp.matchedLength();
            if (length >= bestLength) {
                bestPos = pos;
                bestLength = length;
            }
            start = pos + 1;
        }
    } while (pos != -1);
    
    if (bestPos != -1)
        return mutableStringToMatch.substring(bestPos, bestLength);
    return String();
}
    
String LocalFrame::matchLabelsAgainstElement(const Vector<String>& labels, Element* element)
{
    // Match against the name element, then against the id element if no match is found for the name element.
    // See 7538330 for one popular site that benefits from the id element check.
    // FIXME: This code is mirrored in FrameMac.mm. It would be nice to make the Mac code call the platform-agnostic
    // code, which would require converting the NSArray of NSStrings to a Vector of Strings somewhere along the way.
    String resultFromNameAttribute = matchLabelsAgainstString(labels, element->getNameAttribute());
    if (!resultFromNameAttribute.isEmpty())
        return resultFromNameAttribute;
    
    return matchLabelsAgainstString(labels, element->attributeWithoutSynchronization(idAttr));
}

#if PLATFORM(IOS_FAMILY)

void LocalFrame::setSelectionChangeCallbacksDisabled(bool selectionChangeCallbacksDisabled)
{
    m_selectionChangeCallbacksDisabled = selectionChangeCallbacksDisabled;
}

bool LocalFrame::selectionChangeCallbacksDisabled() const
{
    return m_selectionChangeCallbacksDisabled;
}
#endif // PLATFORM(IOS_FAMILY)

bool LocalFrame::requestDOMPasteAccess(DOMPasteAccessCategory pasteAccessCategory)
{
    if (settings().javaScriptCanAccessClipboard() && settings().domPasteAllowed())
        return true;

    if (!m_doc)
        return false;

    if (editor().isPastingFromMenuOrKeyBinding())
        return true;

    if (!settings().domPasteAccessRequestsEnabled())
        return false;

    auto gestureToken = UserGestureIndicator::currentUserGesture();
    if (!gestureToken || !gestureToken->processingUserGesture() || !gestureToken->canRequestDOMPaste())
        return false;

    switch (gestureToken->domPasteAccessPolicy()) {
    case DOMPasteAccessPolicy::Granted:
        return true;
    case DOMPasteAccessPolicy::Denied:
        return false;
    case DOMPasteAccessPolicy::NotRequestedYet: {
        auto* client = editor().client();
        if (!client)
            return false;

        auto response = client->requestDOMPasteAccess(pasteAccessCategory, frameID(), m_doc->originIdentifierForPasteboard());
        gestureToken->didRequestDOMPasteAccess(response);
        switch (response) {
        case DOMPasteAccessResponse::GrantedForCommand:
        case DOMPasteAccessResponse::GrantedForGesture:
            return true;
        case DOMPasteAccessResponse::DeniedForGesture:
            return false;
        }
    }
    }

    ASSERT_NOT_REACHED();
    return false;
}

void LocalFrame::setPrinting(bool printing, FloatSize pageSize, FloatSize originalPageSize, float maximumShrinkRatio, AdjustViewSize shouldAdjustViewSize, NotifyUIProcess notifyUIProcess)
{
    if (!view() || !document())
        return;

    Frame::setPrinting(printing, pageSize, originalPageSize, maximumShrinkRatio, shouldAdjustViewSize, notifyUIProcess);

    RefPtr document = m_doc;
    // In setting printing, we should not validate resources already cached for the document.
    // See https://bugs.webkit.org/show_bug.cgi?id=43704
    ResourceCacheValidationSuppressor validationSuppressor(document->cachedResourceLoader());

    document->setPrinting(printing);
    protectedView()->adjustMediaTypeForPrinting(printing);

    // FIXME: Consider invoking Page::updateRendering or an equivalent.
    document->styleScope().didChangeStyleSheetEnvironment();
    document->evaluateMediaQueriesAndReportChanges();
    if (!view())
        return;

    Ref frameView = *view();
    if (shouldUsePrintingLayout())
        frameView->forceLayoutForPagination(pageSize, originalPageSize, maximumShrinkRatio, shouldAdjustViewSize);
    else {
        frameView->forceLayout();
        if (shouldAdjustViewSize == AdjustViewSize::Yes)
            frameView->adjustViewSize();
    }

    // Subframes of the one we're printing don't lay out to the page size.
    for (RefPtr child = tree().firstChild(); child; child = child->tree().nextSibling())
        child->setPrinting(printing, FloatSize(), FloatSize(), 0, shouldAdjustViewSize);
}

bool LocalFrame::shouldUsePrintingLayout() const
{
    // Only top frame being printed should be fit to page size.
    // Subframes should be constrained by parents only.
    return isPrinting() && (!tree().parent() || !tree().parent()->isPrinting());
}

FloatSize LocalFrame::resizePageRectsKeepingRatio(const FloatSize& originalSize, const FloatSize& expectedSize)
{
    FloatSize resultSize;
    if (!contentRenderer())
        return FloatSize();

    if (contentRenderer()->writingMode().isHorizontal()) {
        ASSERT(std::abs(originalSize.width()) > std::numeric_limits<float>::epsilon());
        float ratio = originalSize.height() / originalSize.width();
        resultSize.setWidth(floorf(expectedSize.width()));
        resultSize.setHeight(floorf(resultSize.width() * ratio));
    } else {
        ASSERT(std::abs(originalSize.height()) > std::numeric_limits<float>::epsilon());
        float ratio = originalSize.width() / originalSize.height();
        resultSize.setHeight(floorf(expectedSize.height()));
        resultSize.setWidth(floorf(resultSize.height() * ratio));
    }
    return resultSize;
}

const UserContentProvider* LocalFrame::userContentProvider() const
{
    RefPtr document = this->document();
    RefPtr documentLoader = document ? document->loader() : nullptr;
    if (RefPtr userContentProvider = documentLoader ? documentLoader->preferences().userContentProvider : nullptr)
        return userContentProvider.unsafeGet();
    if (RefPtr page = this->page())
        return page->protectedUserContentProviderForFrame().unsafePtr();
    return nullptr;
}

UserContentProvider* LocalFrame::userContentProvider()
{
    RefPtr document = this->document();
    RefPtr documentLoader = document ? document->loader() : nullptr;
    if (RefPtr userContentProvider = documentLoader ? documentLoader->preferences().userContentProvider : nullptr)
        return userContentProvider.unsafeGet();
    if (RefPtr page = this->page())
        return page->protectedUserContentProviderForFrame().unsafePtr();
    return nullptr;
}

bool LocalFrame::hasUserContentProvider(const UserContentProvider& provider)
{
    return userContentProvider() == &provider;
}

void LocalFrame::injectUserScripts(UserScriptInjectionTime injectionTime)
{
    if (loader().stateMachine().creatingInitialEmptyDocument() && !settings().shouldInjectUserScriptsInInitialEmptyDocument())
        return;

    RefPtr userContentProvider = this->userContentProvider();
    if (!userContentProvider)
        return;

    userContentProvider->forEachUserScript([this, injectionTime](DOMWrapperWorld& world, const UserScript& script) {
        if (script.injectionTime() == injectionTime)
            injectUserScriptImmediately(world, script);
    });
}

void LocalFrame::injectUserScriptImmediately(DOMWrapperWorld& world, const UserScript& script)
{
    Ref loader = this->loader();

#if ENABLE(APP_BOUND_DOMAINS)
    if (loader->client().shouldEnableInAppBrowserPrivacyProtections()) {
        if (RefPtr document = this->document())
            document->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, "Ignoring user script injection for non-app bound domain."_s);
        FRAME_RELEASE_LOG_ERROR(Loading, "injectUserScriptImmediately: Ignoring user script injection for non app-bound domain");
        return;
    }

    loader->client().notifyPageOfAppBoundBehavior();
#endif

    RefPtr document = this->document();
    if (!document)
        return;

    RefPtr page = document->page();
    if (!page)
        return;

    if (script.injectedFrames() == UserContentInjectedFrames::InjectInTopFrameOnly && !isMainFrame())
        return;

    auto url = document->url();

    if (RefPtr parentDocument = document->parentDocument()) {
        switch (script.matchParentFrame()) {
        case UserContentMatchParentFrame::ForOpaqueOrigins:
            if (url.protocolIsAbout() || url.protocolIsBlob() || url.protocolIsData())
                url = parentDocument->url();
            break;

        case UserContentMatchParentFrame::ForAboutBlank:
            if (url.isAboutBlank())
                url = parentDocument->url();
            break;

        case UserContentMatchParentFrame::Never:
            break;
        }
    }

    if (!UserContentURLPattern::matchesPatterns(url, script.allowlist(), script.blocklist()))
        return;

    page->setHasInjectedUserScript();
    loader->client().willInjectUserScript(world);

    WTFBeginSignpost(this, UserScript, "injectUserScript: %" PRIVATE_LOG_STRING " (%u bytes, top frame only %d, doc start %d)", script.debugDescription().ascii().data(), script.source().length(), script.injectedFrames() == UserContentInjectedFrames::InjectInTopFrameOnly, script.injectionTime() == UserScriptInjectionTime::DocumentStart);
    checkedScript()->evaluateInWorldIgnoringException(ScriptSourceCode(script.source(), JSC::SourceTaintedOrigin::Untainted, URL(script.url())), world);
    WTFEndSignpost(this, UserScript);
}

RenderView* LocalFrame::contentRenderer() const
{
    return document() ? document()->renderView() : nullptr;
}

CheckedPtr<RenderView> LocalFrame::checkedContentRenderer() const
{
    return contentRenderer();
}

LocalFrame* LocalFrame::frameForWidget(const Widget& widget)
{
    SUPPRESS_UNCOUNTED_LOCAL auto* renderer = RenderWidget::find(widget);
    if (renderer)
        return renderer->frameOwnerElement().document().frame();

    // Assume all widgets are either a FrameView or owned by a RenderWidget.
    // FIXME: That assumption is not right for scroll bars!
    return &downcast<LocalFrameView>(widget).frame();
}

void LocalFrame::clearTimers(LocalFrameView *view, Document *document)
{
    if (!view)
        return;
    view->checkedLayoutContext()->unscheduleLayout();
    if (CheckedPtr timelines = document->timelinesController())
        timelines->suspendAnimations();
    view->protectedFrame()->eventHandler().stopAutoscrollTimer();
}

void LocalFrame::clearTimers()
{
    clearTimers(protectedView().get(), protectedDocument().get());
}

CheckedRef<ScriptController> LocalFrame::checkedScript()
{
    return m_script.get();
}

CheckedRef<const ScriptController> LocalFrame::checkedScript() const
{
    return m_script.get();
}

void LocalFrame::willDetachPage()
{
    if (RefPtr parent = dynamicDowncast<LocalFrame>(tree().parent()))
        parent->loader().checkLoadComplete();

    for (Ref observer : m_destructionObservers)
        observer->willDetachPage();

    // FIXME: It's unclear as to why this is called more than once, but it is,
    // so page() could be NULL.
    if (RefPtr<Page> page = this->page()) {
        CheckedRef focusController = page->focusController();
        if (focusController->focusedFrame() == this)
            focusController->setFocusedFrame(nullptr);
    }

    CheckedRef script = this->script();
    script->clearScriptObjects();
    script->updatePlatformScriptObjects();

    // We promise that the Frame is always connected to a Page while the render tree is live.
    //
    // The render tree can be torn down in a few different ways, but the two important ones are:
    //
    // - When calling Frame::setView() with a null FrameView*. This is always done before calling
    //   Frame::willDetachPage (this function.) Hence the assertion below.
    //
    // - When adding a document to the back/forward cache, the tree is torn down before instantiating
    //   the CachedPage+CachedFrame object tree.
    ASSERT(!document() || !document()->renderView());
}

String LocalFrame::displayStringModifiedByEncoding(const String& str) const
{
    return document() ? document()->displayStringModifiedByEncoding(str) : str;
}

VisiblePosition LocalFrame::visiblePositionForPoint(const IntPoint& framePoint) const
{
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowVisibleChildFrameContentOnly };
    HitTestResult result = eventHandler().hitTestResultAtPoint(framePoint, hitType);
    RefPtr node = result.innerNonSharedNode();
    if (!node)
        return VisiblePosition();
    CheckedPtr renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();
    auto visiblePos = renderer->visiblePositionForPoint(result.localPoint(), HitTestSource::User);
    if (visiblePos.isNull())
        visiblePos = firstPositionInOrBeforeNode(node.get());
    return visiblePos;
}

Document* LocalFrame::documentAtPoint(const IntPoint& point)
{
    if (!view())
        return nullptr;

    IntPoint pt = protectedView()->windowToContents(point);
    HitTestResult result = HitTestResult(pt);

    if (contentRenderer()) {
        constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowChildFrameContent };
        result = eventHandler().hitTestResultAtPoint(pt, hitType);
    }
    return result.innerNode() ? &result.innerNode()->document() : 0;
}

std::optional<SimpleRange> LocalFrame::rangeForPoint(const IntPoint& framePoint)
{
    auto position = visiblePositionForPoint(framePoint);

    SUPPRESS_UNCOUNTED_LOCAL auto containerText = position.deepEquivalent().containerText();
    if (!containerText || !containerText->renderer() || containerText->renderer()->style().usedUserSelect() == UserSelect::None)
        return std::nullopt;

    if (auto previousCharacterRange = makeSimpleRange(position.previous(), position)) {
        if (protectedEditor()->firstRectForRange(*previousCharacterRange).contains(framePoint))
            return *previousCharacterRange;
    }

    if (auto nextCharacterRange = makeSimpleRange(position, position.next())) {
        if (protectedEditor()->firstRectForRange(*nextCharacterRange).contains(framePoint))
            return *nextCharacterRange;
    }

    return std::nullopt;
}

void LocalFrame::createView(const IntSize& viewportSize, const std::optional<Color>& backgroundColor, const IntSize& fixedLayoutSize, bool useFixedLayout, ScrollbarMode horizontalScrollbarMode, bool horizontalLock, ScrollbarMode verticalScrollbarMode, bool verticalLock)
{
    ASSERT(page());

    bool isRootFrame = this->isRootFrame();

    if (isRootFrame && view())
        protectedView()->setParentVisible(false);

    setView(nullptr);

    RefPtr<LocalFrameView> frameView;
    if (isRootFrame) {
        frameView = LocalFrameView::create(*this, viewportSize);
        frameView->setFixedLayoutSize(fixedLayoutSize);
        frameView->setUseFixedLayout(useFixedLayout);
    } else
        frameView = LocalFrameView::create(*this);

    frameView->setScrollbarModes(horizontalScrollbarMode, verticalScrollbarMode, horizontalLock, verticalLock);

    setView(frameView.copyRef());

    frameView->updateBackgroundRecursively(backgroundColor);

    if (isRootFrame)
        frameView->setParentVisible(true);

    if (CheckedPtr ownerRenderer = this->ownerRenderer())
        ownerRenderer->setWidget(frameView);

    protectedView()->setCanHaveScrollbars(scrollingMode() != ScrollbarMode::AlwaysOff);
}

LocalDOMWindow* LocalFrame::window() const
{
    return document() ? document()->window() : nullptr;
}

RefPtr<LocalDOMWindow> LocalFrame::protectedWindow() const
{
    return window();
}

DOMWindow* LocalFrame::virtualWindow() const
{
    return window();
}

void LocalFrame::reinitializeDocumentSecurityContext()
{
    if (RefPtr document = this->document())
        document->initSecurityContext();
}

void LocalFrame::disconnectView()
{
    setView(nullptr);
}

FrameView* LocalFrame::virtualView() const
{
    return m_view.get();
}

FrameLoaderClient& LocalFrame::loaderClient()
{
    return loader().client();
}

void LocalFrame::documentURLForConsoleLog(CompletionHandler<void(const URL&)>&& completionHandler)
{
    RefPtr document = this->document();
    if (!document)
        return completionHandler({ });
    completionHandler(document->url());
}

String LocalFrame::trackedRepaintRectsAsText() const
{
    if (!m_view)
        return String();
    return protectedView()->trackedRepaintRectsAsText();
}

void LocalFrame::setPageZoomFactor(float factor)
{
    setPageAndTextZoomFactors(factor, m_textZoomFactor);
}

void LocalFrame::setTextZoomFactor(float factor)
{
    setPageAndTextZoomFactors(m_pageZoomFactor, factor);
}

void LocalFrame::setPageAndTextZoomFactors(float pageZoomFactor, float textZoomFactor)
{
    if (m_pageZoomFactor == pageZoomFactor && m_textZoomFactor == textZoomFactor)
        return;

    RefPtr page = this->page();
    if (!page)
        return;

    RefPtr document = this->document();
    if (!document)
        return;

    protectedEditor()->dismissCorrectionPanelAsIgnored();

    // Respect SVGs zoomAndPan="disabled" property in standalone SVG documents.
    // FIXME: How to handle compound documents + zoomAndPan="disabled"? Needs SVG WG clarification.
    if (RefPtr svgDocument = dynamicDowncast<SVGDocument>(*document); svgDocument && !svgDocument->zoomAndPanEnabled())
        return;

    std::optional<ScrollPosition> scrollPositionAfterZoomed;
    if (m_pageZoomFactor != pageZoomFactor) {
        // Compute the scroll position with scale after zooming to stay the same position in the content.
        if (RefPtr view = this->view()) {
            scrollPositionAfterZoomed = view->scrollPosition();
            scrollPositionAfterZoomed->scale(pageZoomFactor / m_pageZoomFactor);
        }
    }
    m_pageZoomFactor = pageZoomFactor;
    m_textZoomFactor = textZoomFactor;

    document->resolveStyle(Document::ResolveStyleType::Rebuild);

    for (RefPtr child = tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (RefPtr localFrame = dynamicDowncast<LocalFrame>(child.get()))
            localFrame->setPageAndTextZoomFactors(m_pageZoomFactor, m_textZoomFactor);
    }

    if (RefPtr view = this->view()) {
        if (document->renderView() && document->renderView()->needsLayout() && view->didFirstLayout())
            view->checkedLayoutContext()->layout();

        // Scrolling to the calculated position must be done after the layout.
        if (scrollPositionAfterZoomed)
            view->setScrollPosition(scrollPositionAfterZoomed.value());
    }
}

float LocalFrame::frameScaleFactor() const
{
    RefPtr page = this->page();

    if (!page)
        return 1;

    // https://github.com/w3c/csswg-drafts/issues/9644
    // Check if this frame's owner element (iframe) has CSS zoom applied.
    if (!isMainFrame()) {
        auto rootZoom = rootFrame().pageZoomFactor();
        if (RefPtr ownerElement = this->ownerElement()) {
            if (auto* ownerRenderer = ownerElement->renderer())
                return ownerRenderer->style().usedZoom() / rootZoom;
        }
        return rootZoom;
    }

    // Main frame is scaled with respect to the container.
    if (page->delegatesScaling())
        return 1;

    return page->pageScaleFactor();
}

void LocalFrame::suspendActiveDOMObjectsAndAnimations()
{
    bool wasSuspended = activeDOMObjectsAndAnimationsSuspended();

    m_activeDOMObjectsAndAnimationsSuspendedCount++;

    if (wasSuspended)
        return;

    // FIXME: Suspend/resume calls will not match if the frame is navigated, and gets a new document.
    clearTimers(); // Suspends animations and pending relayouts.
    if (RefPtr document = m_doc)
        document->suspendScheduledTasks(ReasonForSuspension::PageWillBeSuspended);
}

void LocalFrame::resumeActiveDOMObjectsAndAnimations()
{
    if (!activeDOMObjectsAndAnimationsSuspended())
        return;

    m_activeDOMObjectsAndAnimationsSuspendedCount--;

    if (activeDOMObjectsAndAnimationsSuspended())
        return;

    if (!m_doc)
        return;

    Ref document = *m_doc;
    // FIXME: Suspend/resume calls will not match if the frame is navigated, and gets a new document.
    document->resumeScheduledTasks(ReasonForSuspension::PageWillBeSuspended);

    // Frame::clearTimers() suspended animations and pending relayouts.

    if (CheckedPtr timelines = document->timelinesController())
        timelines->resumeAnimations();
    if (RefPtr view = m_view)
        view->checkedLayoutContext()->scheduleLayout();
}

void LocalFrame::deviceOrPageScaleFactorChanged()
{
    for (RefPtr child = tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (RefPtr localFrame = dynamicDowncast<LocalFrame>(child.get()))
            localFrame->deviceOrPageScaleFactorChanged();
    }

    if (CheckedPtr root = contentRenderer())
        root->compositor().deviceOrPageScaleFactorChanged();
}

void LocalFrame::dropChildren()
{
    ASSERT(isMainFrame());
    while (RefPtr child = tree().firstChild())
        tree().removeChild(*child);
}

FloatSize LocalFrame::screenSize() const
{
    if (m_overrideScreenSize)
        return m_overrideScreenSize->size;

    auto defaultSize = screenRect(protectedView().get()).size();
    RefPtr document = this->document();
    if (!document)
        return defaultSize;

    RefPtr page = this->page();
    if (!page)
        return defaultSize;

    if (page->shouldApplyScreenFingerprintingProtections(*document))
        return page->chrome().client().screenSizeForFingerprintingProtections(*this, defaultSize);

    return defaultSize;
}

void LocalFrame::setOverrideScreenSize(FloatSize&& screenSize)
{
    if (m_overrideScreenSize && m_overrideScreenSize->size == screenSize)
        return;

    m_overrideScreenSize = makeUnique<OverrideScreenSize>(OverrideScreenSize { WTFMove(screenSize) });
    if (RefPtr document = this->document())
        document->updateViewportArguments();
}

void LocalFrame::selfOnlyRef()
{
    ASSERT(isMainFrame());
    if (m_selfOnlyRefCount++)
        return;

    ref();
}

void LocalFrame::selfOnlyDeref()
{
    ASSERT(isMainFrame());
    ASSERT(m_selfOnlyRefCount);
    if (--m_selfOnlyRefCount)
        return;

    if (hasOneRef())
        dropChildren();

    deref();
}

String LocalFrame::debugDescription() const
{
    StringBuilder builder;

    builder.append("Frame 0x"_s, hex(reinterpret_cast<uintptr_t>(this), Lowercase));
    if (isMainFrame())
        builder.append(" (main frame)"_s);

    if (RefPtr document = this->document())
        builder.append(' ', document->documentURI());
    
    return builder.toString();
}

TextStream& operator<<(TextStream& ts, const LocalFrame& frame)
{
    ts << frame.debugDescription();
    return ts;
}

void LocalFrame::resetScript()
{
    ASSERT(windowProxy().frame() == this);
    windowProxy().detachFromFrame();
    resetWindowProxy();
    m_script = makeUniqueRef<ScriptController>(*this);
}

LocalFrame* LocalFrame::fromJSContext(JSContextRef context)
{
    JSC::JSGlobalObject* globalObjectObj = toJS(context);
    if (auto* window = JSC::jsDynamicCast<JSDOMWindow*>(globalObjectObj))
        return dynamicDowncast<LocalFrame>(window->wrapped().frame());
    if (auto* serviceWorkerGlobalScope = JSC::jsDynamicCast<JSServiceWorkerGlobalScope*>(globalObjectObj))
        return serviceWorkerGlobalScope->wrapped().serviceWorkerPage() ? dynamicDowncast<LocalFrame>(serviceWorkerGlobalScope->wrapped().serviceWorkerPage()->mainFrame()) : nullptr;
    return nullptr;
}

LocalFrame* LocalFrame::contentFrameFromWindowOrFrameElement(JSContextRef context, JSValueRef valueRef)
{
    ASSERT(context);
    ASSERT(valueRef);

    JSC::JSGlobalObject* globalObject = toJS(context);
    JSC::JSValue value = toJS(globalObject, valueRef);

    if (RefPtr window = JSDOMWindow::toWrapped(globalObject->vm(), value))
        return dynamicDowncast<LocalFrame>(window->frame());

    auto* jsNode = JSC::jsDynamicCast<JSNode*>(value);
    if (!jsNode)
        return nullptr;

    RefPtr frameOwner = dynamicDowncast<HTMLFrameOwnerElement>(jsNode->wrapped());
    return frameOwner ? dynamicDowncast<LocalFrame>(frameOwner->contentFrame()) : nullptr;
}

void LocalFrame::documentURLOrOriginDidChange()
{
    if (!isMainFrame())
        return;

    RefPtr page = this->page();
    RefPtr document = this->document();
    if (page && document)
        page->setMainFrameURLAndOrigin(document->url(), document->protectedSecurityOrigin());
}

void LocalFrame::dispatchLoadEventToParent()
{
    if (is<RemoteFrame>(tree().parent()))
        loader().client().dispatchLoadEventToOwnerElementInAnotherProcess();
    else if (RefPtr owner = ownerElement())
        owner->dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

#if ENABLE(DATA_DETECTION)

DataDetectionResultsStorage& LocalFrame::dataDetectionResults()
{
    if (!m_dataDetectionResults)
        m_dataDetectionResults = makeUnique<DataDetectionResultsStorage>();
    return *m_dataDetectionResults;
}

#endif

void LocalFrame::frameWasDisconnectedFromOwner() const
{
    if (!m_doc)
        return;

    if (RefPtr window = m_doc->window())
        window->willDetachDocumentFromFrame();

    protectedDocument()->detachFromFrame();
}

void LocalFrame::storageAccessExceptionReceivedForDomain(const RegistrableDomain& domain)
{
    if (!m_storageAccessExceptionDomains)
        m_storageAccessExceptionDomains = makeUnique<HashSet<RegistrableDomain>>();
    m_storageAccessExceptionDomains->add(domain);
}

bool LocalFrame::requestSkipUserActivationCheckForStorageAccess(const RegistrableDomain& domain)
{
    if (!m_storageAccessExceptionDomains)
        return false;

    auto iter = m_storageAccessExceptionDomains->find(domain);
    if (iter == m_storageAccessExceptionDomains->end())
        return false;

    // We only allow the domain to skip check once.
    m_storageAccessExceptionDomains->remove(iter);
    return true;
}

#if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)

void LocalFrame::didAccessWindowProxyPropertyViaOpener(WindowProxyProperty property)
{
    // FIXME: until we support restricted openers, report all property accesses as "other" to reduce
    // the number of events logged.
    property = WindowProxyProperty::Other;

    if (m_accessedWindowProxyPropertiesViaOpener.contains(property))
        return;

    auto origin = SecurityOriginData::fromFrame(this);
    if (origin.isNull() || origin.isOpaque())
        return;

    if (!opener() || !opener()->page())
        return;

    auto openerMainFrameOrigin = opener()->page()->mainFrameOrigin().data();
    if (openerMainFrameOrigin.isNull() || openerMainFrameOrigin.isOpaque())
        return;

    auto site = RegistrableDomain(origin);
    auto openerMainFrameSite = RegistrableDomain(openerMainFrameOrigin);
    if (site == openerMainFrameSite)
        return;

    m_accessedWindowProxyPropertiesViaOpener.add(property);
    loader().client().didAccessWindowProxyPropertyViaOpener(WTFMove(openerMainFrameOrigin), property);
}

#endif

String LocalFrame::customUserAgent() const
{
    if (RefPtr documentLoader = loader().activeDocumentLoader())
        return documentLoader->customUserAgent();
    return { };
}

String LocalFrame::customUserAgentAsSiteSpecificQuirks() const
{
    if (RefPtr documentLoader = loader().activeDocumentLoader())
        return documentLoader->customUserAgentAsSiteSpecificQuirks();
    return { };
}

String LocalFrame::customNavigatorPlatform() const
{
    if (RefPtr documentLoader = loader().activeDocumentLoader())
        return documentLoader->customNavigatorPlatform();
    return { };
}

OptionSet<AdvancedPrivacyProtections> LocalFrame::advancedPrivacyProtections() const
{
    if (auto* documentLoader = loader().activeDocumentLoader())
        return documentLoader->advancedPrivacyProtections();
    return { };
}

AutoplayPolicy LocalFrame::autoplayPolicy() const
{
    if (RefPtr documentLoader = loader().activeDocumentLoader())
        return documentLoader->autoplayPolicy();
    return AutoplayPolicy::Default;
}

SandboxFlags LocalFrame::effectiveSandboxFlags() const
{
    auto effectiveSandboxFlags = m_sandboxFlags;
    if (RefPtr document = this->document())
        effectiveSandboxFlags.add(document->sandboxFlags());
    return effectiveSandboxFlags;
}

void LocalFrame::updateSandboxFlags(SandboxFlags flags, NotifyUIProcess notifyUIProcess)
{
    Frame::updateSandboxFlags(flags, notifyUIProcess);
    m_sandboxFlags = flags;
}

ReferrerPolicy LocalFrame::effectiveReferrerPolicy() const
{
    // Policy will be the one passed in from parent frame or opener.
    // Otherwise, use default.
    if (m_parentFrameOrOpenerReferrerPolicy != ReferrerPolicy::EmptyString)
        return m_parentFrameOrOpenerReferrerPolicy;
    return ReferrerPolicy::Default;
}

void LocalFrame::updateReferrerPolicy(ReferrerPolicy referrerPolicy)
{
    m_parentFrameOrOpenerReferrerPolicy = referrerPolicy;
    m_doc->setReferrerPolicy(referrerPolicy);
}

void LocalFrame::updateScrollingMode()
{
    if (!ownerElement())
        return;
    m_scrollingMode = ownerElement()->scrollingMode();
    if (RefPtr view = this->view())
        view->setCanHaveScrollbars(m_scrollingMode != ScrollbarMode::AlwaysOff);
}

void LocalFrame::setScrollingMode(ScrollbarMode scrollingMode)
{
    m_scrollingMode = scrollingMode;
    if (RefPtr view = this->view())
        view->setCanHaveScrollbars(m_scrollingMode != ScrollbarMode::AlwaysOff);
}

void LocalFrame::reportMixedContentViolation(bool blocked, const URL& target) const
{
    RefPtr document = this->document();
    if (!document)
        return;

    auto isUpgradingLocalhostDisabled = !document->settings().iPAddressAndLocalhostMixedContentUpgradeTestingEnabled() && shouldTreatAsPotentiallyTrustworthy(target);
    ASCIILiteral errorString = [&] {
        if (blocked)
            return "blocked and must"_s;
        if (isUpgradingLocalhostDisabled)
            return "not upgraded to HTTPS and must be served from the local host."_s;
        return "automatically upgraded and should"_s;
    }();

    auto message = makeString((!blocked ? ""_s : "[blocked] "_s), "The page at "_s, document->url().stringCenterEllipsizedToLength(), " requested insecure content from "_s, target.stringCenterEllipsizedToLength(), ". This content was "_s, errorString, !isUpgradingLocalhostDisabled ? " be served over HTTPS.\n"_s : "\n"_s);

    document->addConsoleMessage(MessageSource::Security, MessageLevel::Warning, message);
}

#if ENABLE(CONTENT_EXTENSIONS)

static String generateResourceMonitorErrorHTML(OptionSet<ColorScheme> colorScheme)
{
#if PLATFORM(COCOA) && HAVE(CUSTOM_IFRAME_UNLOADING_HTML)
#if HAVE(CUSTOM_IFRAME_UNLOADING_HTML_WITH_COLOR_SCHEME)
    return generateResourceMonitorErrorHTMLForCocoa(colorScheme);
#else
    UNUSED_PARAM(colorScheme);
    return generateResourceMonitorErrorHTMLForCocoa();
#endif
#else
    constexpr auto lightAndDarkColorScheme = ":root { color-scheme: light dark } "_s;
    constexpr auto darkOnlyColorScheme = ":root { color-scheme: only dark } "_s;
    constexpr auto lightStyle = "p { color: black } "_s;
    constexpr auto darkStyle = "p { color: white } "_s;
    constexpr auto empty = ""_s;

    bool needDarkStyle = colorScheme.contains(ColorScheme::Dark);
    bool needLightStyle = !needDarkStyle || colorScheme.contains(ColorScheme::Light);
    bool conditionalStyle = needDarkStyle && needLightStyle;

    const auto& colorSchemeStyle = conditionalStyle ? lightAndDarkColorScheme : needDarkStyle ? darkOnlyColorScheme : empty;
    const auto& darkStyleOpen = conditionalStyle ? "@media (prefers-color-scheme: dark) { "_s : empty;
    const auto& darkStyleClose = conditionalStyle ? "} "_s : empty;

    return makeString(
        "<style> body { background-color: gray }"_s,
        colorSchemeStyle,
        lightStyle,
        darkStyleOpen,
        (needDarkStyle ? darkStyle : empty),
        darkStyleClose,
        "</style><p>"_s,
        WEB_UI_STRING("This frame is hidden for using too many system resources.", "Description HTML for frame unloaded by ResourceMonitor"),
        "</p>"_s
    );
#endif
}

static DiagnosticLoggingClient::ValueDictionary valueDictionaryForResult(bool unloaded)
{
    DiagnosticLoggingClient::ValueDictionary dictionary;
    dictionary.set(DiagnosticLoggingKeys::unloadCountKey(), (unloaded ? 1 : 0));
    dictionary.set(DiagnosticLoggingKeys::unloadPreventedByThrottlerCountKey(), unloaded ? 0 : 1);
    dictionary.set(DiagnosticLoggingKeys::unloadPreventedByStickyActivationCountKey(), 0);
    return dictionary;
}

void LocalFrame::showResourceMonitoringError()
{
    RefPtr iframeElement = dynamicDowncast<HTMLIFrameElement>(ownerElement());
    RefPtr document = this->document();
    if (!iframeElement || !document)
        return;

    URL url;
    URL mainFrameURL;
    if (document)
        url = document->url();
    if (RefPtr page = this->page()) {
        mainFrameURL = page->mainFrameURL();
        page->diagnosticLoggingClient().logDiagnosticMessageWithValueDictionary(DiagnosticLoggingKeys::iframeResourceMonitoringKey(), "IFrame ResourceMonitoring Unloaded"_s, valueDictionaryForResult(true), ShouldSample::No);
    }

    FRAME_RELEASE_LOG(ResourceMonitoring, "Detected excessive network usage in frame at %" SENSITIVE_LOG_STRING " and main frame at %" SENSITIVE_LOG_STRING ": unloading", url.isValid() ? url.string().utf8().data() : "invalid", mainFrameURL.isValid() ? mainFrameURL.string().utf8().data() : "invalid");

    document->addConsoleMessage(MessageSource::ContentBlocker, MessageLevel::Error, makeString("Frame was unloaded because its network usage exceeded the limit: "_s, ResourceMonitorChecker::singleton().networkUsageThreshold(), " bytes, url="_s, url.string()));

    for (RefPtr<Frame> frame = this; frame; frame = frame->tree().traverseNext()) {
        if (RefPtr localFrame = dynamicDowncast<LocalFrame>(frame)) {
            if (RefPtr window = localFrame->window())
                window->removeAllEventListeners();
        }
    }

    OptionSet<ColorScheme> colorScheme { ColorScheme::Light };

#if ENABLE(DARK_MODE_CSS)
    if (CheckedPtr style = iframeElement->existingComputedStyle())
        colorScheme = document->resolvedColorScheme(style.get());
#endif

    iframeElement->setSrcdoc(generateResourceMonitorErrorHTML(colorScheme), SubstituteData::SessionHistoryVisibility::Hidden);
}

void LocalFrame::reportResourceMonitoringWarning()
{
    URL url;
    URL mainFrameURL;
    if (RefPtr document = this->document())
        url = document->url();
    if (RefPtr page = this->page()) {
        mainFrameURL = page->mainFrameURL();
        page->diagnosticLoggingClient().logDiagnosticMessageWithValueDictionary(DiagnosticLoggingKeys::iframeResourceMonitoringKey(), "IFrame ResourceMonitoring Throttled"_s, valueDictionaryForResult(false), ShouldSample::No);
    }

    FRAME_RELEASE_LOG(ResourceMonitoring, "Detected excessive network usage in frame at %" SENSITIVE_LOG_STRING " and main frame at %" SENSITIVE_LOG_STRING ": not unloading due to global limits", url.isValid() ? url.string().utf8().data() : "invalid", mainFrameURL.isValid() ? mainFrameURL.string().utf8().data() : "invalid");

    if (RefPtr document = this->document())
        document->addConsoleMessage(MessageSource::ContentBlocker, MessageLevel::Warning, "Frame's network usage exceeded the limit."_s);
}

#endif

static String generateFrameMemoryMonitorErrorHTML(OptionSet<ColorScheme> colorScheme)
{
    constexpr auto lightAndDarkColorScheme = ":root { color-scheme: light dark } "_s;
    constexpr auto darkOnlyColorScheme = ":root { color-scheme: only dark } "_s;
    constexpr auto lightStyle = "p { color: black } "_s;
    constexpr auto darkStyle = "p { color: white } "_s;
    constexpr auto empty = ""_s;

    bool needDarkStyle = colorScheme.contains(ColorScheme::Dark);
    bool needLightStyle = !needDarkStyle || colorScheme.contains(ColorScheme::Light);
    bool conditionalStyle = needDarkStyle && needLightStyle;

    const auto& colorSchemeStyle = conditionalStyle ? lightAndDarkColorScheme : needDarkStyle ? darkOnlyColorScheme : empty;
    const auto& darkStyleOpen = conditionalStyle ? "@media (prefers-color-scheme: dark) { "_s : empty;
    const auto& darkStyleClose = conditionalStyle ? "} "_s : empty;

    return makeString(
        "<style> body { background-color: gray }"_s,
        colorSchemeStyle,
        lightStyle,
        darkStyleOpen,
        (needDarkStyle ? darkStyle : empty),
        darkStyleClose,
        "</style><p>"_s,
        WEB_UI_STRING("This frame is hidden for using too many system resources.", "Description HTML for frame unloaded by ResourceMonitor"),
        "</p>"_s
    );
}

void LocalFrame::showMemoryMonitorError()
{
    RefPtr iframeElement = dynamicDowncast<HTMLIFrameElement>(ownerElement());
    RefPtr document = this->document();
    if (!iframeElement || !document)
        return;

    for (RefPtr<Frame> frame = this; frame; frame = frame->tree().traverseNext()) {
        if (RefPtr localFrame = dynamicDowncast<LocalFrame>(frame)) {
            if (RefPtr window = localFrame->window())
                window->removeAllEventListeners();
        }
    }

    OptionSet<ColorScheme> colorScheme { ColorScheme::Light };

#if ENABLE(DARK_MODE_CSS)
    if (CheckedPtr style = iframeElement->existingComputedStyle())
        colorScheme = document->resolvedColorScheme(style.get());
#endif

    iframeElement->setSrcdoc(generateFrameMemoryMonitorErrorHTML(colorScheme), SubstituteData::SessionHistoryVisibility::Hidden);
}

bool LocalFrame::frameCanCreatePaymentSession() const
{
#if ENABLE(APPLE_PAY)
    if (auto* documentLoader = loader().activeDocumentLoader())
        return PaymentSession::isSecureForSession(documentLoader->response().url(), documentLoader->response().certificateInfo());
    return false;
#else
    return false;
#endif
}

RefPtr<SecurityOrigin> LocalFrame::frameDocumentSecurityOrigin() const
{
    if (RefPtr document = this->document())
        return &document->securityOrigin();

    return nullptr;
}

Ref<FrameInspectorController> LocalFrame::protectedInspectorController() const
{
    return m_inspectorController.get();
}

String LocalFrame::frameURLProtocol() const
{
    if (RefPtr document = this->document())
        return document->url().protocol().toString();

    return ""_s;
}

} // namespace WebCore

#undef FRAME_RELEASE_LOG_ERROR
