/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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

#include "ApplyStyleCommand.h"
#include "BackForwardCache.h"
#include "BackForwardController.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "CSSValuePool.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DocumentLoader.h"
#include "DocumentTimelinesController.h"
#include "DocumentType.h"
#include "Editing.h"
#include "Editor.h"
#include "EditorClient.h"
#include "ElementInlines.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FloatQuad.h"
#include "FocusController.h"
#include "FrameDestructionObserver.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "HTMLAttachmentElement.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableRowElement.h"
#include "HitTestResult.h"
#include "ImageBuffer.h"
#include "InspectorInstrumentation.h"
#include "JSNode.h"
#include "JSServiceWorkerGlobalScope.h"
#include "JSWindowProxy.h"
#include "LocalDOMWindow.h"
#include "LocalFrameLoaderClient.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "Navigator.h"
#include "NodeList.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "ProcessWarming.h"
#include "RemoteFrame.h"
#include "RenderLayerCompositor.h"
#include "RenderTableCell.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "SVGDocument.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementTypeHelpers.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "ScrollingCoordinator.h"
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
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

#if ENABLE(DATA_DETECTION)
#include "DataDetectionResultsStorage.h"
#endif

#define FRAME_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - Frame::" fmt, this, ##__VA_ARGS__)

namespace WebCore {

using namespace HTMLNames;

#if PLATFORM(IOS_FAMILY)
static const Seconds scrollFrequency { 1000_s / 60. };
#endif

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, frameCounter, ("Frame"));

static inline float parentPageZoomFactor(LocalFrame* frame)
{
    LocalFrame* parent = dynamicDowncast<LocalFrame>(frame->tree().parent());
    if (!parent)
        return 1;
    return parent->pageZoomFactor();
}

static inline float parentTextZoomFactor(LocalFrame* frame)
{
    LocalFrame* parent = dynamicDowncast<LocalFrame>(frame->tree().parent());
    if (!parent)
        return 1;
    return parent->textZoomFactor();
}

static bool isRootFrame(const Frame& frame)
{
    if (auto* parent = frame.tree().parent())
        return is<RemoteFrame>(parent);
    ASSERT(&frame.mainFrame() == &frame);
    return true;
}

LocalFrame::LocalFrame(Page& page, UniqueRef<LocalFrameLoaderClient>&& frameLoaderClient, FrameIdentifier identifier, HTMLFrameOwnerElement* ownerElement, Frame* parent)
    : Frame(page, identifier, FrameType::Local, ownerElement, parent)
    , m_loader(makeUniqueRef<FrameLoader>(*this, WTFMove(frameLoaderClient)))
    , m_script(makeUniqueRef<ScriptController>(*this))
    , m_pageZoomFactor(parentPageZoomFactor(this))
    , m_textZoomFactor(parentTextZoomFactor(this))
    , m_isRootFrame(WebCore::isRootFrame(*this))
    , m_eventHandler(makeUniqueRef<EventHandler>(*this))
{
    ProcessWarming::initializeNames();
    StaticCSSValuePool::init();

    if (auto* localMainFrame = dynamicDowncast<LocalFrame>(mainFrame()); localMainFrame && parent)
        localMainFrame->selfOnlyRef();

#ifndef NDEBUG
    frameCounter.increment();
#endif

    // Pause future ActiveDOMObjects if this frame is being created while the page is in a paused state.
    if (LocalFrame* parent = dynamicDowncast<LocalFrame>(tree().parent()); parent && parent->activeDOMObjectsAndAnimationsSuspended())
        suspendActiveDOMObjectsAndAnimations();

    if (isRootFrame())
        page.addRootFrame(*this);
}

void LocalFrame::init()
{
    checkedLoader()->init();
}

Ref<LocalFrame> LocalFrame::createMainFrame(Page& page, UniqueRef<LocalFrameLoaderClient>&& client, FrameIdentifier identifier)
{
    return adoptRef(*new LocalFrame(page, WTFMove(client), identifier, nullptr, nullptr));
}

Ref<LocalFrame> LocalFrame::createSubframe(Page& page, UniqueRef<LocalFrameLoaderClient>&& client, FrameIdentifier identifier, HTMLFrameOwnerElement& ownerElement)
{
    return adoptRef(*new LocalFrame(page, WTFMove(client), identifier, &ownerElement, ownerElement.document().frame()));
}

Ref<LocalFrame> LocalFrame::createSubframeHostedInAnotherProcess(Page& page, UniqueRef<LocalFrameLoaderClient>&& client, FrameIdentifier identifier, Frame& parent)
{
    return adoptRef(*new LocalFrame(page, WTFMove(client), identifier, nullptr, &parent));
}

LocalFrame::~LocalFrame()
{
    setView(nullptr);

    CheckedRef loader = this->loader();
    if (!loader->isComplete())
        loader->closeURL();

    loader->clear(protectedDocument(), false);
    checkedScript()->updatePlatformScriptObjects();

    // FIXME: We should not be doing all this work inside the destructor

#ifndef NDEBUG
    frameCounter.decrement();
#endif

    disconnectOwnerElement();

    while (auto* destructionObserver = m_destructionObservers.takeAny())
        destructionObserver->frameDestroyed();

    auto* localMainFrame = dynamicDowncast<LocalFrame>(mainFrame());
    if (!isMainFrame() && localMainFrame)
        localMainFrame->selfOnlyDeref();

    if (isRootFrame()) {
        if (auto* page = this->page())
            page->removeRootFrame(*this);
    }
}

bool LocalFrame::isRootFrame() const
{
    return m_isRootFrame;
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
    checkedLoader()->resetMultipleFormSubmissionProtection();
}

CheckedRef<Editor> LocalFrame::checkedEditor()
{
    return editor();
}

CheckedRef<const Editor> LocalFrame::checkedEditor() const
{
    return editor();
}

void LocalFrame::setDocument(RefPtr<Document>&& newDocument)
{
    ASSERT(!newDocument || newDocument->frame() == this);

    if (m_documentIsBeingReplaced)
        return;

    m_documentIsBeingReplaced = true;

    if (isMainFrame()) {
        if (RefPtr page = this->page())
            page->didChangeMainDocument();
        checkedLoader()->client().dispatchDidChangeMainDocument();

        // We want to generate the same unique names whenever a page is loaded to avoid making layout tests
        // flaky and for things like form state restoration to work. To achieve this, we reset our frame
        // identifier generator every time the page is navigated.
        tree().resetFrameIdentifiers();
    }

    if (RefPtr previousDocument = m_doc) {
#if ENABLE(ATTACHMENT_ELEMENT)
        for (Ref attachment : previousDocument->attachmentElementsByIdentifier().values())
            checkedEditor()->didRemoveAttachmentElement(attachment);
#endif

        if (previousDocument->backForwardCacheState() != Document::InBackForwardCache)
            previousDocument->willBeRemovedFromFrame();
    }

    m_doc = newDocument.copyRef();
    ASSERT(!m_doc || m_doc->domWindow());
    ASSERT(!m_doc || m_doc->domWindow()->frame() == this);

    // Don't use m_doc because it can be overwritten and we want to guarantee
    // that the document is not destroyed during this function call.
    if (newDocument)
        newDocument->didBecomeCurrentDocumentInFrame();

#if ENABLE(ATTACHMENT_ELEMENT)
    if (RefPtr document = m_doc) {
        CheckedRef editor = this->editor();
        for (Ref attachment : document->attachmentElementsByIdentifier().values())
            editor->didInsertAttachmentElement(attachment);
    }
#endif

    if (page() && m_doc && isMainFrame() && !loader().stateMachine().isDisplayingInitialEmptyDocument())
        protectedPage()->mainFrameDidChangeToNonInitialEmptyDocument();

    InspectorInstrumentation::frameDocumentUpdated(*this);

#if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)
    m_accessedWindowProxyPropertiesViaOpener = { };
#endif

    m_documentIsBeingReplaced = false;
}

void LocalFrame::frameDetached()
{
    checkedLoader()->frameDetached();
}

bool LocalFrame::preventsParentFromBeingComplete() const
{
    return !checkedLoader()->isComplete() && (!ownerElement() || !ownerElement()->isLazyLoadObserverActive());
}

void LocalFrame::changeLocation(FrameLoadRequest&& request)
{
    checkedLoader()->changeLocation(WTFMove(request));
}

void LocalFrame::broadcastFrameRemovalToOtherProcesses()
{
    checkedLoader()->client().broadcastFrameRemovalToOtherProcesses();
}

void LocalFrame::didFinishLoadInAnotherProcess()
{
    checkedLoader()->provisionalLoadFailedInAnotherProcess();
}

void LocalFrame::invalidateContentEventRegionsIfNeeded(InvalidateContentEventRegionsReason reason)
{
    if (!page() || !m_doc || !m_doc->renderView())
        return;

    bool needsUpdateForWheelEventHandlers = false;
    bool needsUpdateForTouchActionElements = false;
    bool needsUpdateForEditableElements = false;
    bool needsUpdateForInteractionRegions = false;
#if ENABLE(WHEEL_EVENT_REGIONS)
    needsUpdateForWheelEventHandlers = m_doc->hasWheelEventHandlers() || reason == InvalidateContentEventRegionsReason::EventHandlerChange;
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
    if (!needsUpdateForTouchActionElements && !needsUpdateForEditableElements && !needsUpdateForWheelEventHandlers && !needsUpdateForInteractionRegions)
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
        pattern.append(i ? "|" : "", startsWithWordCharacter ? "\\b" : "", label, endsWithWordCharacter ? "\\b" : "");
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
            if (!textNode->renderer() || textNode->renderer()->style().visibility() != Visibility::Visible)
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
        } else if (n->isTextNode() && n->renderer() && n->renderer()->style().visibility() == Visibility::Visible) {
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

        auto response = client->requestDOMPasteAccess(pasteAccessCategory, m_doc->originIdentifierForPasteboard());
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

void LocalFrame::setPrinting(bool printing, const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkRatio, AdjustViewSizeOrNot shouldAdjustViewSize)
{
    if (!view() || !document())
        return;

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
        if (shouldAdjustViewSize == AdjustViewSize)
            frameView->adjustViewSize();
    }

    // Subframes of the one we're printing don't lay out to the page size.
    for (RefPtr child = tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (RefPtr localFrame = dynamicDowncast<LocalFrame>(child.get()))
            localFrame->setPrinting(printing, FloatSize(), FloatSize(), 0, shouldAdjustViewSize);
    }
}

bool LocalFrame::shouldUsePrintingLayout() const
{
    // Only top frame being printed should be fit to page size.
    // Subframes should be constrained by parents only.
    auto* parent = dynamicDowncast<LocalFrame>(tree().parent());
    return m_doc->printing() && (!parent || !parent->m_doc->printing());
}

FloatSize LocalFrame::resizePageRectsKeepingRatio(const FloatSize& originalSize, const FloatSize& expectedSize)
{
    FloatSize resultSize;
    if (!contentRenderer())
        return FloatSize();

    if (contentRenderer()->style().isHorizontalWritingMode()) {
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

void LocalFrame::injectUserScripts(UserScriptInjectionTime injectionTime)
{
    if (!page())
        return;

    if (loader().stateMachine().creatingInitialEmptyDocument() && !settings().shouldInjectUserScriptsInInitialEmptyDocument())
        return;

    RefPtr page = this->page();
    bool pageWasNotified = page->hasBeenNotifiedToInjectUserScripts();
    page->protectedUserContentProvider()->forEachUserScript([this, protectedThis = Ref { *this }, injectionTime, pageWasNotified] (DOMWrapperWorld& world, const UserScript& script) {
        if (script.injectionTime() == injectionTime) {
            if (script.waitForNotificationBeforeInjecting() == WaitForNotificationBeforeInjecting::Yes && !pageWasNotified)
                addUserScriptAwaitingNotification(world, script);
            else
                injectUserScriptImmediately(world, script);
        }
    });
}

void LocalFrame::injectUserScriptImmediately(DOMWrapperWorld& world, const UserScript& script)
{
    CheckedRef loader = this->loader();
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
    if (script.injectedFrames() == UserContentInjectedFrames::InjectInTopFrameOnly && !isMainFrame())
        return;
    if (!UserContentURLPattern::matchesPatterns(document->url(), script.allowlist(), script.blocklist()))
        return;

    document->setAsRunningUserScripts();
    loader->client().willInjectUserScript(world);
    checkedScript()->evaluateInWorldIgnoringException(ScriptSourceCode(script.source(), JSC::SourceTaintedOrigin::Untainted, URL(script.url())), world);
}

void LocalFrame::addUserScriptAwaitingNotification(DOMWrapperWorld& world, const UserScript& script)
{
    m_userScriptsAwaitingNotification.append({ world, makeUniqueRef<UserScript>(script) });
}

void LocalFrame::injectUserScriptsAwaitingNotification()
{
    for (const auto& [world, script] : std::exchange(m_userScriptsAwaitingNotification, { }))
        injectUserScriptImmediately(world, script.get());
}

RenderView* LocalFrame::contentRenderer() const
{
    return document() ? document()->renderView() : nullptr;
}

LocalFrame* LocalFrame::frameForWidget(const Widget& widget)
{
    if (auto* renderer = RenderWidget::find(widget))
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
    view->protectedFrame()->checkedEventHandler()->stopAutoscrollTimer();
}

void LocalFrame::clearTimers()
{
    clearTimers(protectedView().get(), protectedDocument().get());
}

CheckedRef<const FrameLoader> LocalFrame::checkedLoader() const
{
    return m_loader.get();
}

CheckedRef<FrameLoader> LocalFrame::checkedLoader()
{
    return m_loader.get();
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
        parent->checkedLoader()->checkLoadComplete();

    for (auto& observer : m_destructionObservers)
        observer.willDetachPage();

    // FIXME: It's unclear as to why this is called more than once, but it is,
    // so page() could be NULL.
    if (RefPtrAllowingPartiallyDestroyed<Page> page = this->page()) {
        CheckedRef focusController = page->focusController();
        if (focusController->focusedFrame() == this)
            focusController->setFocusedFrame(nullptr);
    }


    if (page() && page()->scrollingCoordinator() && m_view)
        page()->protectedScrollingCoordinator()->willDestroyScrollableArea(*protectedView());

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
    VisiblePosition visiblePos = renderer->positionForPoint(result.localPoint(), nullptr);
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
        result = checkedEventHandler()->hitTestResultAtPoint(pt, hitType);
    }
    return result.innerNode() ? &result.innerNode()->document() : 0;
}

std::optional<SimpleRange> LocalFrame::rangeForPoint(const IntPoint& framePoint)
{
    auto position = visiblePositionForPoint(framePoint);

    auto containerText = position.deepEquivalent().containerText();
    if (!containerText || !containerText->renderer() || containerText->renderer()->style().effectiveUserSelect() == UserSelect::None)
        return std::nullopt;

    if (auto previousCharacterRange = makeSimpleRange(position.previous(), position)) {
        if (checkedEditor()->firstRectForRange(*previousCharacterRange).contains(framePoint))
            return *previousCharacterRange;
    }

    if (auto nextCharacterRange = makeSimpleRange(position, position.next())) {
        if (checkedEditor()->firstRectForRange(*nextCharacterRange).contains(framePoint))
            return *nextCharacterRange;
    }

    return std::nullopt;
}

void LocalFrame::createView(const IntSize& viewportSize, const std::optional<Color>& backgroundColor,
    const IntSize& fixedLayoutSize, const IntRect& fixedVisibleContentRect,
    bool useFixedLayout, ScrollbarMode horizontalScrollbarMode, bool horizontalLock,
    ScrollbarMode verticalScrollbarMode, bool verticalLock)
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
#if USE(COORDINATED_GRAPHICS)
        frameView->setFixedVisibleContentRect(fixedVisibleContentRect);
#else
        UNUSED_PARAM(fixedVisibleContentRect);
#endif
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

    if (RefPtr owner = ownerElement())
        protectedView()->setCanHaveScrollbars(owner->scrollingMode() != ScrollbarMode::AlwaysOff);
}

LocalDOMWindow* LocalFrame::window() const
{
    return document() ? document()->domWindow() : nullptr;
}

DOMWindow* LocalFrame::virtualWindow() const
{
    return window();
}

void LocalFrame::disconnectView()
{
    setView(nullptr);
}

void LocalFrame::setOpener(Frame* opener)
{
    loader().setOpener(opener);
}

const Frame* LocalFrame::opener() const
{
    return loader().opener();
}

Frame* LocalFrame::opener()
{
    return loader().opener();
}

FrameView* LocalFrame::virtualView() const
{
    return m_view.get();
}

FrameLoaderClient& LocalFrame::loaderClient()
{
    return loader().client();
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

    checkedEditor()->dismissCorrectionPanelAsIgnored();

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

    // Main frame is scaled with respect to he container but inner frames are not scaled with respect to the main frame.
    if (!page || !isMainFrame())
        return 1;

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
    if (!m_overrideScreenSize.isEmpty())
        return m_overrideScreenSize;

    auto defaultSize = screenRect(view()).size();
    RefPtr document = this->document();
    if (!document)
        return defaultSize;

    RefPtr loader = document->loader();
    if (!loader || !loader->fingerprintingProtectionsEnabled())
        return defaultSize;

    if (RefPtr page = this->page())
        return page->chrome().client().screenSizeForFingerprintingProtections(*this, defaultSize);

    return defaultSize;
}

void LocalFrame::setOverrideScreenSize(FloatSize&& screenSize)
{
    if (m_overrideScreenSize == screenSize)
        return;

    m_overrideScreenSize = WTFMove(screenSize);
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

    if (auto document = this->document())
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
    if (auto* window = JSC::jsDynamicCast<JSLocalDOMWindow*>(globalObjectObj))
        return window->wrapped().frame();
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
    JSC::VM& vm = globalObject->vm();

    if (auto* window = JSLocalDOMWindow::toWrapped(vm, value))
        return window->frame();

    auto* jsNode = JSC::jsDynamicCast<JSNode*>(value);
    if (!jsNode)
        return nullptr;

    RefPtr frameOwner = dynamicDowncast<HTMLFrameOwnerElement>(jsNode->wrapped());
    return frameOwner ? dynamicDowncast<LocalFrame>(frameOwner->contentFrame()) : nullptr;
}

CheckedRef<EventHandler> LocalFrame::checkedEventHandler()
{
    return m_eventHandler.get();
}

CheckedRef<const EventHandler> LocalFrame::checkedEventHandler() const
{
    return m_eventHandler.get();
}

void LocalFrame::documentURLDidChange(const URL& url)
{
    if (RefPtr page = this->page(); page && isMainFrame()) {
        page->setMainFrameURL(url);
        checkedLoader()->client().broadcastMainFrameURLChangeToOtherProcesses(url);
    }
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

    if (RefPtr window = m_doc->domWindow())
        window->willDetachDocumentFromFrame();

    protectedDocument()->detachFromFrame();
}

CheckedRef<FrameSelection> LocalFrame::checkedSelection() const
{
    return document()->selection();
}

#if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)

void LocalFrame::didAccessWindowProxyPropertyViaOpener(WindowProxyProperty property)
{
    if (m_accessedWindowProxyPropertiesViaOpener.contains(property))
        return;
    m_accessedWindowProxyPropertiesViaOpener.add(property);

    RefPtr parentWindow { opener() ? opener()->window() : nullptr };
    if (!parentWindow)
        return;

    auto parentOrigin = SecurityOriginData::fromURL(parentWindow->location().url());
    if (parentOrigin.isNull() || parentOrigin.isOpaque())
        return;

    checkedLoader()->client().didAccessWindowProxyPropertyViaOpener(WTFMove(parentOrigin), property);
}

#endif

} // namespace WebCore

#undef FRAME_RELEASE_LOG_ERROR
