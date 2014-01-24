/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
#include "Frame.h"

#include "AnimationController.h"
#include "ApplyStyleCommand.h"
#include "BackForwardController.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMWindow.h"
#include "DocumentType.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FloatQuad.h"
#include "FocusController.h"
#include "FrameDestructionObserver.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "HTMLDocument.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "HitTestResult.h"
#include "ImageBuffer.h"
#include "InspectorInstrumentation.h"
#include "JSDOMWindowShell.h"
#include "Logging.h"
#include "MainFrame.h"
#include "MathMLNames.h"
#include "MediaFeatureNames.h"
#include "Navigator.h"
#include "NodeList.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "PageCache.h"
#include "PageGroup.h"
#include "RenderTableCell.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGNames.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "StyleProperties.h"
#include "TextIterator.h"
#include "TextNodeTraversal.h"
#include "TextResourceDecoder.h"
#include "UserContentController.h"
#include "UserContentURLPattern.h"
#include "UserTypingGestureIndicator.h"
#include "VisibleUnits.h"
#include "WebKitFontFamilyNames.h"
#include "XLinkNames.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include "htmlediting.h"
#include "markup.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include <bindings/ScriptValue.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>
#include <yarr/RegularExpression.h>

#if USE(ACCELERATED_COMPOSITING)
#include "RenderLayerCompositor.h"
#endif

#if ENABLE(SVG)
#include "SVGDocument.h"
#include "SVGDocumentExtensions.h"
#endif

#if USE(TILED_BACKING_STORE)
#include "TiledBackingStore.h"
#endif

#if PLATFORM(IOS)
#include "WKContentObservation.h"
#endif

namespace WebCore {

using namespace HTMLNames;

#if PLATFORM(IOS)
const unsigned scrollFrequency = 1000 / 60;
#endif

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, frameCounter, ("Frame"));

static inline Frame* parentFromOwnerElement(HTMLFrameOwnerElement* ownerElement)
{
    if (!ownerElement)
        return 0;
    return ownerElement->document().frame();
}

static inline float parentPageZoomFactor(Frame* frame)
{
    Frame* parent = frame->tree().parent();
    if (!parent)
        return 1;
    return parent->pageZoomFactor();
}

static inline float parentTextZoomFactor(Frame* frame)
{
    Frame* parent = frame->tree().parent();
    if (!parent)
        return 1;
    return parent->textZoomFactor();
}

Frame::Frame(Page& page, HTMLFrameOwnerElement* ownerElement, FrameLoaderClient& frameLoaderClient)
    : m_mainFrame(ownerElement ? page.mainFrame() : static_cast<MainFrame&>(*this))
    , m_page(&page)
    , m_settings(&page.settings())
    , m_treeNode(*this, parentFromOwnerElement(ownerElement))
    , m_loader(*this, frameLoaderClient)
    , m_navigationScheduler(*this)
    , m_ownerElement(ownerElement)
    , m_script(std::make_unique<ScriptController>(*this))
    , m_editor(Editor::create(*this))
    , m_selection(adoptPtr(new FrameSelection(this)))
    , m_eventHandler(adoptPtr(new EventHandler(*this)))
    , m_animationController(std::make_unique<AnimationController>(*this))
#if PLATFORM(IOS)
    , m_overflowAutoScrollTimer(this, &Frame::overflowAutoScrollTimerFired)
    , m_selectionChangeCallbacksDisabled(false)
    , m_timersPausedCount(0)
#endif
    , m_pageZoomFactor(parentPageZoomFactor(this))
    , m_textZoomFactor(parentTextZoomFactor(this))
#if ENABLE(ORIENTATION_EVENTS)
    , m_orientation(0)
#endif
    , m_inViewSourceMode(false)
    , m_activeDOMObjectsAndAnimationsSuspendedCount(0)
{
    AtomicString::init();
    HTMLNames::init();
    QualifiedName::init();
    MediaFeatureNames::init();
    SVGNames::init();
    XLinkNames::init();
    MathMLNames::init();
    XMLNSNames::init();
    XMLNames::init();
    WebKitFontFamilyNames::init();

    if (!ownerElement) {
#if USE(TILED_BACKING_STORE)
        // Top level frame only for now.
        setTiledBackingStoreEnabled(settings().tiledBackingStoreEnabled());
#endif
    } else {
        m_mainFrame.selfOnlyRef();
        page.incrementSubframeCount();
        ownerElement->setContentFrame(this);
    }

#ifndef NDEBUG
    frameCounter.increment();
#endif

    // FIXME: We should reconcile the iOS and OpenSource code below.
    Frame* parent = parentFromOwnerElement(ownerElement);
#if PLATFORM(IOS)
    // Pause future timers if this frame is created when page is in pending state.
    if (parent && parent->timersPaused())
        setTimersPaused(true);
#else
    // Pause future ActiveDOMObjects if this frame is being created while the page is in a paused state.
    if (parent && parent->activeDOMObjectsAndAnimationsSuspended())
        suspendActiveDOMObjectsAndAnimations();
#endif
}

PassRefPtr<Frame> Frame::create(Page* page, HTMLFrameOwnerElement* ownerElement, FrameLoaderClient* client)
{
    ASSERT(page);
    ASSERT(client);
    return adoptRef(new Frame(*page, ownerElement, *client));
}

Frame::~Frame()
{
    setView(0);
    loader().cancelAndClear();

    // FIXME: We should not be doing all this work inside the destructor

#ifndef NDEBUG
    frameCounter.decrement();
#endif

    disconnectOwnerElement();

    for (auto& observer : m_destructionObservers)
        observer->frameDestroyed();

    if (!isMainFrame())
        m_mainFrame.selfOnlyDeref();
}

void Frame::addDestructionObserver(FrameDestructionObserver* observer)
{
    m_destructionObservers.add(observer);
}

void Frame::removeDestructionObserver(FrameDestructionObserver* observer)
{
    m_destructionObservers.remove(observer);
}

void Frame::setView(PassRefPtr<FrameView> view)
{
    // We the custom scroll bars as early as possible to prevent m_doc->detach()
    // from messing with the view such that its scroll bars won't be torn down.
    // FIXME: We should revisit this.
    if (m_view)
        m_view->prepareForDetach();

    // Prepare for destruction now, so any unload event handlers get run and the DOMWindow is
    // notified. If we wait until the view is destroyed, then things won't be hooked up enough for
    // these calls to work.
    if (!view && m_doc && m_doc->hasLivingRenderTree() && !m_doc->inPageCache())
        m_doc->prepareForDestruction();
    
    if (m_view)
        m_view->unscheduleRelayout();
    
    eventHandler().clear();

    m_view = view;

    // Only one form submission is allowed per view of a part.
    // Since this part may be getting reused as a result of being
    // pulled from the back/forward cache, reset this flag.
    loader().resetMultipleFormSubmissionProtection();
    
#if USE(TILED_BACKING_STORE)
    if (m_view && tiledBackingStore())
        m_view->setPaintsEntireContents(true);
#endif
}

void Frame::setDocument(PassRefPtr<Document> newDocument)
{
    ASSERT(!newDocument || newDocument->frame() == this);

    if (m_doc && m_doc->hasLivingRenderTree() && !m_doc->inPageCache())
        m_doc->prepareForDestruction();

    m_doc = newDocument.get();
    ASSERT(!m_doc || m_doc->domWindow());
    ASSERT(!m_doc || m_doc->domWindow()->frame() == this);

    // Don't use m_doc because it can be overwritten and we want to guarantee
    // that the document is not destroyed during this function call.
    if (newDocument)
        newDocument->didBecomeCurrentDocumentInFrame();
}

#if ENABLE(ORIENTATION_EVENTS)
void Frame::sendOrientationChangeEvent(int orientation)
{
    m_orientation = orientation;
    if (Document* doc = document())
        doc->dispatchWindowEvent(Event::create(eventNames().orientationchangeEvent, false, false));
}
#endif // ENABLE(ORIENTATION_EVENTS)

static PassOwnPtr<JSC::Yarr::RegularExpression> createRegExpForLabels(const Vector<String>& labels)
{
    // REVIEW- version of this call in FrameMac.mm caches based on the NSArray ptrs being
    // the same across calls.  We can't do that.

    DEFINE_STATIC_LOCAL(JSC::Yarr::RegularExpression, wordRegExp, ("\\w", TextCaseSensitive));
    String pattern("(");
    unsigned int numLabels = labels.size();
    unsigned int i;
    for (i = 0; i < numLabels; i++) {
        String label = labels[i];

        bool startsWithWordChar = false;
        bool endsWithWordChar = false;
        if (label.length()) {
            startsWithWordChar = wordRegExp.match(label.substring(0, 1)) >= 0;
            endsWithWordChar = wordRegExp.match(label.substring(label.length() - 1, 1)) >= 0;
        }

        if (i)
            pattern.append("|");
        // Search for word boundaries only if label starts/ends with "word characters".
        // If we always searched for word boundaries, this wouldn't work for languages
        // such as Japanese.
        if (startsWithWordChar)
            pattern.append("\\b");
        pattern.append(label);
        if (endsWithWordChar)
            pattern.append("\\b");
    }
    pattern.append(")");
    return adoptPtr(new JSC::Yarr::RegularExpression(pattern, TextCaseInsensitive));
}

String Frame::searchForLabelsAboveCell(JSC::Yarr::RegularExpression* regExp, HTMLTableCellElement* cell, size_t* resultDistanceFromStartOfCell)
{
    HTMLTableCellElement* aboveCell = cell->cellAbove();
    if (aboveCell) {
        // search within the above cell we found for a match
        size_t lengthSearched = 0;    
        for (Text* textNode = TextNodeTraversal::firstWithin(aboveCell); textNode; textNode = TextNodeTraversal::next(textNode, aboveCell)) {
            if (!textNode->renderer() || textNode->renderer()->style().visibility() != VISIBLE)
                continue;
            // For each text chunk, run the regexp
            String nodeString = textNode->data();
            int pos = regExp->searchRev(nodeString);
            if (pos >= 0) {
                if (resultDistanceFromStartOfCell)
                    *resultDistanceFromStartOfCell = lengthSearched;
                return nodeString.substring(pos, regExp->matchedLength());
            }
            lengthSearched += nodeString.length();
        }
    }

    // Any reason in practice to search all cells in that are above cell?
    if (resultDistanceFromStartOfCell)
        *resultDistanceFromStartOfCell = notFound;
    return String();
}

String Frame::searchForLabelsBeforeElement(const Vector<String>& labels, Element* element, size_t* resultDistance, bool* resultIsInCellAbove)
{
    OwnPtr<JSC::Yarr::RegularExpression> regExp(createRegExpForLabels(labels));
    // We stop searching after we've seen this many chars
    const unsigned int charsSearchedThreshold = 500;
    // This is the absolute max we search.  We allow a little more slop than
    // charsSearchedThreshold, to make it more likely that we'll search whole nodes.
    const unsigned int maxCharsSearched = 600;
    // If the starting element is within a table, the cell that contains it
    HTMLTableCellElement* startingTableCell = 0;
    bool searchedCellAbove = false;

    if (resultDistance)
        *resultDistance = notFound;
    if (resultIsInCellAbove)
        *resultIsInCellAbove = false;
    
    // walk backwards in the node tree, until another element, or form, or end of tree
    int unsigned lengthSearched = 0;
    Node* n;
    for (n = NodeTraversal::previous(element); n && lengthSearched < charsSearchedThreshold; n = NodeTraversal::previous(n)) {
        // We hit another form element or the start of the form - bail out
        if (isHTMLFormElement(n) || (n->isHTMLElement() && toElement(n)->isFormControlElement()))
            break;

        if (n->hasTagName(tdTag) && !startingTableCell) {
            startingTableCell = toHTMLTableCellElement(n);
        } else if (n->hasTagName(trTag) && startingTableCell) {
            String result = searchForLabelsAboveCell(regExp.get(), startingTableCell, resultDistance);
            if (!result.isEmpty()) {
                if (resultIsInCellAbove)
                    *resultIsInCellAbove = true;
                return result;
            }
            searchedCellAbove = true;
        } else if (n->isTextNode() && n->renderer() && n->renderer()->style().visibility() == VISIBLE) {
            // For each text chunk, run the regexp
            String nodeString = n->nodeValue();
            // add 100 for slop, to make it more likely that we'll search whole nodes
            if (lengthSearched + nodeString.length() > maxCharsSearched)
                nodeString = nodeString.right(charsSearchedThreshold - lengthSearched);
            int pos = regExp->searchRev(nodeString);
            if (pos >= 0) {
                if (resultDistance)
                    *resultDistance = lengthSearched;
                return nodeString.substring(pos, regExp->matchedLength());
            }
            lengthSearched += nodeString.length();
        }
    }

    // If we started in a cell, but bailed because we found the start of the form or the
    // previous element, we still might need to search the row above us for a label.
    if (startingTableCell && !searchedCellAbove) {
         String result = searchForLabelsAboveCell(regExp.get(), startingTableCell, resultDistance);
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
    replace(mutableStringToMatch, JSC::Yarr::RegularExpression("\\d", TextCaseSensitive), " ");
    mutableStringToMatch.replace('_', ' ');
    
    OwnPtr<JSC::Yarr::RegularExpression> regExp(createRegExpForLabels(labels));
    // Use the largest match we can find in the whole string
    int pos;
    int length;
    int bestPos = -1;
    int bestLength = -1;
    int start = 0;
    do {
        pos = regExp->match(mutableStringToMatch, start);
        if (pos != -1) {
            length = regExp->matchedLength();
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
    
String Frame::matchLabelsAgainstElement(const Vector<String>& labels, Element* element)
{
    // Match against the name element, then against the id element if no match is found for the name element.
    // See 7538330 for one popular site that benefits from the id element check.
    // FIXME: This code is mirrored in FrameMac.mm. It would be nice to make the Mac code call the platform-agnostic
    // code, which would require converting the NSArray of NSStrings to a Vector of Strings somewhere along the way.
    String resultFromNameAttribute = matchLabelsAgainstString(labels, element->getNameAttribute());
    if (!resultFromNameAttribute.isEmpty())
        return resultFromNameAttribute;
    
    return matchLabelsAgainstString(labels, element->getAttribute(idAttr));
}

#if PLATFORM(IOS)
void Frame::scrollOverflowLayer(RenderLayer* layer, const IntRect& visibleRect, const IntRect& exposeRect)
{
    if (!layer)
        return;

    RenderBox* box = layer->renderBox();
    if (!box)
        return;

    if (visibleRect.intersects(exposeRect))
        return;

    int x = layer->scrollXOffset();
    int exposeLeft = exposeRect.x();
    int exposeRight = exposeLeft + exposeRect.width();
    int clientWidth = box->clientWidth();
    if (exposeLeft <= 0)
        x = std::max(0, x + exposeLeft - clientWidth / 2);
    else if (exposeRight >= clientWidth)
        x = std::min(box->scrollWidth() - clientWidth, x + clientWidth / 2);

    int y = layer->scrollYOffset();
    int exposeTop = exposeRect.y();
    int exposeBottom = exposeTop + exposeRect.height();
    int clientHeight = box->clientHeight();
    if (exposeTop <= 0)
        y = std::max(0, y + exposeTop - clientHeight / 2);
    else if (exposeBottom >= clientHeight)
        y = std::min(box->scrollHeight() - clientHeight, y + clientHeight / 2);

    layer->scrollToOffset(IntSize(x, y));
    selection().setCaretRectNeedsUpdate();
    selection().updateAppearance();
}

void Frame::overflowAutoScrollTimerFired(Timer<Frame>*)
{
    if (!eventHandler().mousePressed() || checkOverflowScroll(PerformOverflowScroll) == OverflowScrollNone) {
        if (m_overflowAutoScrollTimer.isActive())
            m_overflowAutoScrollTimer.stop();
    }
}

void Frame::startOverflowAutoScroll(const IntPoint& mousePosition)
{
    m_overflowAutoScrollPos = mousePosition;

    if (m_overflowAutoScrollTimer.isActive())
        return;

    if (checkOverflowScroll(DoNotPerformOverflowScroll) == OverflowScrollNone)
        return;

    m_overflowAutoScrollTimer.startRepeating(scrollFrequency);
    m_overflowAutoScrollDelta = 3;
}

int Frame::checkOverflowScroll(OverflowScrollAction action)
{
    Position extent = selection().selection().extent();
    if (extent.isNull())
        return OverflowScrollNone;

    RenderObject* renderer = extent.deprecatedNode()->renderer();
    if (!renderer)
        return OverflowScrollNone;

    FrameView* view = this->view();
    if (!view)
        return OverflowScrollNone;

    RenderBlock* containingBlock = renderer->containingBlock();
    if (!containingBlock || !containingBlock->hasOverflowClip())
        return OverflowScrollNone;
    RenderLayer* layer = containingBlock->layer();
    ASSERT(layer);

    IntRect visibleRect = IntRect(view->scrollX(), view->scrollY(), view->visibleWidth(), view->visibleHeight());
    IntPoint position = m_overflowAutoScrollPos;
    if (visibleRect.contains(position.x(), position.y()))
        return OverflowScrollNone;

    int scrollType = 0;
    int deltaX = 0;
    int deltaY = 0;
    IntPoint selectionPosition;

    // This constant will make the selection draw a little bit beyond the edge of the visible area.
    // This prevents a visual glitch, in that you can fail to select a portion of a character that
    // is being rendered right at the edge of the visible rectangle.
    // FIXME: This probably needs improvement, and may need to take the font size into account.
    static const int scrollBoundsAdjustment = 3;

    // FIXME: Make a small buffer at the end of a visible rectangle so that autoscrolling works 
    // even if the visible extends to the limits of the screen.
    if (position.x() < visibleRect.x()) {
        scrollType |= OverflowScrollLeft;
        if (action == PerformOverflowScroll) {
            deltaX -= static_cast<int>(m_overflowAutoScrollDelta);
            selectionPosition.setX(view->scrollX() - scrollBoundsAdjustment);
        }
    } else if (position.x() > visibleRect.maxX()) {
        scrollType |= OverflowScrollRight;
        if (action == PerformOverflowScroll) {
            deltaX += static_cast<int>(m_overflowAutoScrollDelta);
            selectionPosition.setX(view->scrollX() + view->visibleWidth() + scrollBoundsAdjustment);
        }
    }

    if (position.y() < visibleRect.y()) {
        scrollType |= OverflowScrollUp;
        if (action == PerformOverflowScroll) {
            deltaY -= static_cast<int>(m_overflowAutoScrollDelta);
            selectionPosition.setY(view->scrollY() - scrollBoundsAdjustment);
        }
    } else if (position.y() > visibleRect.maxY()) {
        scrollType |= OverflowScrollDown;
        if (action == PerformOverflowScroll) {
            deltaY += static_cast<int>(m_overflowAutoScrollDelta);
            selectionPosition.setY(view->scrollY() + view->visibleHeight() + scrollBoundsAdjustment);
        }
    }

    if (action == PerformOverflowScroll && (deltaX || deltaY)) {
        layer->scrollToOffset(IntSize(layer->scrollXOffset() + deltaX, layer->scrollYOffset() + deltaY));

        // Handle making selection.
        VisiblePosition visiblePosition(renderer->positionForPoint(selectionPosition));
        if (visiblePosition.isNotNull()) {
            VisibleSelection visibleSelection = selection().selection();
            visibleSelection.setExtent(visiblePosition);
            if (selection().granularity() != CharacterGranularity)
                visibleSelection.expandUsingGranularity(selection().granularity());
            if (selection().shouldChangeSelection(visibleSelection))
                selection().setSelection(visibleSelection);
        }

        m_overflowAutoScrollDelta *= 1.02f; // Accelerate the scroll
    }
    return scrollType;
}

void Frame::setSelectionChangeCallbacksDisabled(bool selectionChangeCallbacksDisabled)
{
    m_selectionChangeCallbacksDisabled = selectionChangeCallbacksDisabled;
}

bool Frame::selectionChangeCallbacksDisabled() const
{
    return m_selectionChangeCallbacksDisabled;
}
#endif // PLATFORM(IOS)

void Frame::setPrinting(bool printing, const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkRatio, AdjustViewSizeOrNot shouldAdjustViewSize)
{
    // In setting printing, we should not validate resources already cached for the document.
    // See https://bugs.webkit.org/show_bug.cgi?id=43704
    ResourceCacheValidationSuppressor validationSuppressor(m_doc->cachedResourceLoader());

    m_doc->setPrinting(printing);
    view()->adjustMediaTypeForPrinting(printing);

    m_doc->styleResolverChanged(RecalcStyleImmediately);
    if (shouldUsePrintingLayout()) {
        view()->forceLayoutForPagination(pageSize, originalPageSize, maximumShrinkRatio, shouldAdjustViewSize);
    } else {
        view()->forceLayout();
        if (shouldAdjustViewSize == AdjustViewSize)
            view()->adjustViewSize();
    }

    // Subframes of the one we're printing don't lay out to the page size.
    for (RefPtr<Frame> child = tree().firstChild(); child; child = child->tree().nextSibling())
        child->setPrinting(printing, FloatSize(), FloatSize(), 0, shouldAdjustViewSize);
}

bool Frame::shouldUsePrintingLayout() const
{
    // Only top frame being printed should be fit to page size.
    // Subframes should be constrained by parents only.
    return m_doc->printing() && (!tree().parent() || !tree().parent()->m_doc->printing());
}

FloatSize Frame::resizePageRectsKeepingRatio(const FloatSize& originalSize, const FloatSize& expectedSize)
{
    FloatSize resultSize;
    if (!contentRenderer())
        return FloatSize();

    if (contentRenderer()->style().isHorizontalWritingMode()) {
        ASSERT(fabs(originalSize.width()) > std::numeric_limits<float>::epsilon());
        float ratio = originalSize.height() / originalSize.width();
        resultSize.setWidth(floorf(expectedSize.width()));
        resultSize.setHeight(floorf(resultSize.width() * ratio));
    } else {
        ASSERT(fabs(originalSize.height()) > std::numeric_limits<float>::epsilon());
        float ratio = originalSize.width() / originalSize.height();
        resultSize.setHeight(floorf(expectedSize.height()));
        resultSize.setWidth(floorf(resultSize.height() * ratio));
    }
    return resultSize;
}

void Frame::injectUserScripts(UserScriptInjectionTime injectionTime)
{
    if (!m_page)
        return;

    if (loader().stateMachine()->creatingInitialEmptyDocument() && !settings().shouldInjectUserScriptsInInitialEmptyDocument())
        return;

    const auto* userContentController = m_page->userContentController();
    if (!userContentController)
        return;

    // Walk the hashtable. Inject by world.
    const UserScriptMap* userScripts = userContentController->userScripts();
    if (!userScripts)
        return;

    for (const auto& worldAndUserScript : *userScripts)
        injectUserScriptsForWorld(*worldAndUserScript.key, *worldAndUserScript.value, injectionTime);
}

void Frame::injectUserScriptsForWorld(DOMWrapperWorld& world, const UserScriptVector& userScripts, UserScriptInjectionTime injectionTime)
{
    if (userScripts.isEmpty())
        return;

    Document* doc = document();
    if (!doc)
        return;

    Vector<ScriptSourceCode> sourceCode;
    unsigned count = userScripts.size();
    for (unsigned i = 0; i < count; ++i) {
        UserScript* script = userScripts[i].get();
        if (script->injectedFrames() == InjectInTopFrameOnly && ownerElement())
            continue;

        if (script->injectionTime() == injectionTime && UserContentURLPattern::matchesPatterns(doc->url(), script->whitelist(), script->blacklist()))
            m_script->evaluateInWorld(ScriptSourceCode(script->source(), script->url()), world);
    }
}

RenderView* Frame::contentRenderer() const
{
    return document() ? document()->renderView() : 0;
}

RenderWidget* Frame::ownerRenderer() const
{
    HTMLFrameOwnerElement* ownerElement = m_ownerElement;
    if (!ownerElement)
        return 0;
    auto object = ownerElement->renderer();
    if (!object)
        return 0;
    // FIXME: If <object> is ever fixed to disassociate itself from frames
    // that it has started but canceled, then this can turn into an ASSERT
    // since m_ownerElement would be 0 when the load is canceled.
    // https://bugs.webkit.org/show_bug.cgi?id=18585
    if (!object->isWidget())
        return 0;
    return toRenderWidget(object);
}

Frame* Frame::frameForWidget(const Widget* widget)
{
    ASSERT_ARG(widget, widget);

    if (RenderWidget* renderer = RenderWidget::find(widget))
        return renderer->frameOwnerElement().document().frame();

    // Assume all widgets are either a FrameView or owned by a RenderWidget.
    // FIXME: That assumption is not right for scroll bars!
    ASSERT_WITH_SECURITY_IMPLICATION(widget->isFrameView());
    return &toFrameView(widget)->frame();
}

void Frame::clearTimers(FrameView *view, Document *document)
{
    if (view) {
        view->unscheduleRelayout();
        view->frame().animation().suspendAnimationsForDocument(document);
        view->frame().eventHandler().stopAutoscrollTimer();
    }
}

void Frame::clearTimers()
{
    clearTimers(m_view.get(), document());
}

void Frame::willDetachPage()
{
    if (Frame* parent = tree().parent())
        parent->loader().checkLoadComplete();

    for (auto& observer : m_destructionObservers)
        observer->willDetachPage();

    // FIXME: It's unclear as to why this is called more than once, but it is,
    // so page() could be NULL.
    if (page() && page()->focusController().focusedFrame() == this)
        page()->focusController().setFocusedFrame(0);

    if (page() && page()->scrollingCoordinator() && m_view)
        page()->scrollingCoordinator()->willDestroyScrollableArea(m_view.get());

#if PLATFORM(IOS)
    if (WebThreadCountOfObservedContentModifiers() > 0 && m_page)
        m_page->chrome().client().clearContentChangeObservers(this);
#endif

    script().clearScriptObjects();
    script().updatePlatformScriptObjects();
}

void Frame::disconnectOwnerElement()
{
    if (m_ownerElement) {
        // We use the ownerElement's document to retrieve the cache, because the contentDocument for this
        // frame is already detached (and can't access the top level AX cache).
        // However, we pass in the current document to clearTextMarkerNodesInUse so we can identify the
        // nodes inside this document that need to be removed from the cache.
        
        // We don't clear the AXObjectCache here because we don't want to clear the top level cache
        // when a sub-frame is removed.
#if HAVE(ACCESSIBILITY)
        if (AXObjectCache* cache = m_ownerElement->document().existingAXObjectCache())
            cache->clearTextMarkerNodesInUse(document());
#endif
        
        m_ownerElement->clearContentFrame();
        if (m_page)
            m_page->decrementSubframeCount();
    }
    m_ownerElement = nullptr;
}

String Frame::displayStringModifiedByEncoding(const String& str) const
{
    return document() ? document()->displayStringModifiedByEncoding(str) : str;
}

VisiblePosition Frame::visiblePositionForPoint(const IntPoint& framePoint)
{
    HitTestResult result = eventHandler().hitTestResultAtPoint(framePoint, HitTestRequest::ReadOnly | HitTestRequest::Active);
    Node* node = result.innerNonSharedNode();
    if (!node)
        return VisiblePosition();
    auto renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();
    VisiblePosition visiblePos = renderer->positionForPoint(result.localPoint());
    if (visiblePos.isNull())
        visiblePos = firstPositionInOrBeforeNode(node);
    return visiblePos;
}

Document* Frame::documentAtPoint(const IntPoint& point)
{
    if (!view())
        return 0;

    IntPoint pt = view()->windowToContents(point);
    HitTestResult result = HitTestResult(pt);

    if (contentRenderer())
        result = eventHandler().hitTestResultAtPoint(pt);
    return result.innerNode() ? &result.innerNode()->document() : 0;
}

PassRefPtr<Range> Frame::rangeForPoint(const IntPoint& framePoint)
{
    VisiblePosition position = visiblePositionForPoint(framePoint);
    if (position.isNull())
        return 0;

    VisiblePosition previous = position.previous();
    if (previous.isNotNull()) {
        RefPtr<Range> previousCharacterRange = makeRange(previous, position);
        LayoutRect rect = editor().firstRectForRange(previousCharacterRange.get());
        if (rect.contains(framePoint))
            return previousCharacterRange.release();
    }

    VisiblePosition next = position.next();
    if (RefPtr<Range> nextCharacterRange = makeRange(position, next)) {
        LayoutRect rect = editor().firstRectForRange(nextCharacterRange.get());
        if (rect.contains(framePoint))
            return nextCharacterRange.release();
    }

    return 0;
}

void Frame::createView(const IntSize& viewportSize, const Color& backgroundColor, bool transparent,
    const IntSize& fixedLayoutSize, const IntRect& fixedVisibleContentRect,
    bool useFixedLayout, ScrollbarMode horizontalScrollbarMode, bool horizontalLock,
    ScrollbarMode verticalScrollbarMode, bool verticalLock)
{
    ASSERT(this);
    ASSERT(m_page);

    bool isMainFrame = this->isMainFrame();

    if (isMainFrame && view())
        view()->setParentVisible(false);

    setView(0);

    RefPtr<FrameView> frameView;
    if (isMainFrame) {
        frameView = FrameView::create(*this, viewportSize);
        frameView->setFixedLayoutSize(fixedLayoutSize);
#if USE(TILED_BACKING_STORE)
        frameView->setFixedVisibleContentRect(fixedVisibleContentRect);
#else
        UNUSED_PARAM(fixedVisibleContentRect);
#endif
        frameView->setUseFixedLayout(useFixedLayout);
    } else
        frameView = FrameView::create(*this);

    frameView->setScrollbarModes(horizontalScrollbarMode, verticalScrollbarMode, horizontalLock, verticalLock);

    setView(frameView);

    if (backgroundColor.isValid())
        frameView->updateBackgroundRecursively(backgroundColor, transparent);

    if (isMainFrame)
        frameView->setParentVisible(true);

    if (ownerRenderer())
        ownerRenderer()->setWidget(frameView);

    if (HTMLFrameOwnerElement* owner = ownerElement())
        view()->setCanHaveScrollbars(owner->scrollingMode() != ScrollbarAlwaysOff);
}

#if USE(TILED_BACKING_STORE)
void Frame::setTiledBackingStoreEnabled(bool enabled)
{
    if (!enabled) {
        m_tiledBackingStore.clear();
        return;
    }
    if (m_tiledBackingStore)
        return;
    m_tiledBackingStore = adoptPtr(new TiledBackingStore(this));
    m_tiledBackingStore->setCommitTileUpdatesOnIdleEventLoop(true);
    if (m_view)
        m_view->setPaintsEntireContents(true);
}

void Frame::tiledBackingStorePaintBegin()
{
    if (!m_view)
        return;
    m_view->updateLayoutAndStyleIfNeededRecursive();
    m_view->flushDeferredRepaints();
}

void Frame::tiledBackingStorePaint(GraphicsContext* context, const IntRect& rect)
{
    if (!m_view)
        return;
    m_view->paintContents(context, rect);
}

void Frame::tiledBackingStorePaintEnd(const Vector<IntRect>& paintedArea)
{
    if (!m_page || !m_view)
        return;
    unsigned size = paintedArea.size();
    // Request repaint from the system
    for (unsigned n = 0; n < size; ++n)
        m_page->chrome().invalidateContentsAndRootView(m_view->contentsToRootView(paintedArea[n]), false);
}

IntRect Frame::tiledBackingStoreContentsRect()
{
    if (!m_view)
        return IntRect();
    return IntRect(IntPoint(), m_view->contentsSize());
}

IntRect Frame::tiledBackingStoreVisibleRect()
{
    if (!m_page)
        return IntRect();
    return m_page->chrome().client().visibleRectForTiledBackingStore();
}

Color Frame::tiledBackingStoreBackgroundColor() const
{
    if (!m_view)
        return Color();
    return m_view->baseBackgroundColor();
}
#endif

String Frame::layerTreeAsText(LayerTreeFlags flags) const
{
#if USE(ACCELERATED_COMPOSITING)
    document()->updateLayout();

    if (!contentRenderer())
        return String();

    return contentRenderer()->compositor().layerTreeAsText(flags);
#else
    UNUSED_PARAM(flags);
    return String();
#endif
}

String Frame::trackedRepaintRectsAsText() const
{
    if (!m_view)
        return String();
    return m_view->trackedRepaintRectsAsText();
}

void Frame::setPageZoomFactor(float factor)
{
    setPageAndTextZoomFactors(factor, m_textZoomFactor);
}

void Frame::setTextZoomFactor(float factor)
{
    setPageAndTextZoomFactors(m_pageZoomFactor, factor);
}

void Frame::setPageAndTextZoomFactors(float pageZoomFactor, float textZoomFactor)
{
    if (m_pageZoomFactor == pageZoomFactor && m_textZoomFactor == textZoomFactor)
        return;

    Page* page = this->page();
    if (!page)
        return;

    Document* document = this->document();
    if (!document)
        return;

    m_editor->dismissCorrectionPanelAsIgnored();

#if ENABLE(SVG)
    // Respect SVGs zoomAndPan="disabled" property in standalone SVG documents.
    // FIXME: How to handle compound documents + zoomAndPan="disabled"? Needs SVG WG clarification.
    if (document->isSVGDocument()) {
        if (!toSVGDocument(document)->zoomAndPanEnabled())
            return;
    }
#endif

    if (m_pageZoomFactor != pageZoomFactor) {
        if (FrameView* view = this->view()) {
            // Update the scroll position when doing a full page zoom, so the content stays in relatively the same position.
            LayoutPoint scrollPosition = view->scrollPosition();
            float percentDifference = (pageZoomFactor / m_pageZoomFactor);
            view->setScrollPosition(IntPoint(scrollPosition.x() * percentDifference, scrollPosition.y() * percentDifference));
        }
    }

    m_pageZoomFactor = pageZoomFactor;
    m_textZoomFactor = textZoomFactor;

    document->recalcStyle(Style::Force);

    for (RefPtr<Frame> child = tree().firstChild(); child; child = child->tree().nextSibling())
        child->setPageAndTextZoomFactors(m_pageZoomFactor, m_textZoomFactor);

    if (FrameView* view = this->view()) {
        if (document->renderView() && document->renderView()->needsLayout() && view->didFirstLayout())
            view->layout();
    }

    if (isMainFrame())
        pageCache()->markPagesForFullStyleRecalc(page);
}

float Frame::frameScaleFactor() const
{
    Page* page = this->page();

    // Main frame is scaled with respect to he container but inner frames are not scaled with respect to the main frame.
    if (!page || &page->mainFrame() != this || settings().delegatesPageScaling())
        return 1;

    return page->pageScaleFactor();
}

void Frame::suspendActiveDOMObjectsAndAnimations()
{
    bool wasSuspended = activeDOMObjectsAndAnimationsSuspended();

    m_activeDOMObjectsAndAnimationsSuspendedCount++;

    if (wasSuspended)
        return;

    // FIXME: Suspend/resume calls will not match if the frame is navigated, and gets a new document.
    if (document()) {
        document()->suspendScriptedAnimationControllerCallbacks();
        animation().suspendAnimationsForDocument(document());
        document()->suspendActiveDOMObjects(ActiveDOMObject::PageWillBeSuspended);
    }
}

void Frame::resumeActiveDOMObjectsAndAnimations()
{
    ASSERT(activeDOMObjectsAndAnimationsSuspended());

    m_activeDOMObjectsAndAnimationsSuspendedCount--;

    if (activeDOMObjectsAndAnimationsSuspended())
        return;

    if (document()) {
        document()->resumeActiveDOMObjects(ActiveDOMObject::PageWillBeSuspended);
        animation().resumeAnimationsForDocument(document());
        document()->resumeScriptedAnimationControllerCallbacks();
    }
}

#if USE(ACCELERATED_COMPOSITING)
void Frame::deviceOrPageScaleFactorChanged()
{
    for (RefPtr<Frame> child = tree().firstChild(); child; child = child->tree().nextSibling())
        child->deviceOrPageScaleFactorChanged();

    if (RenderView* root = contentRenderer())
        root->compositor().deviceOrPageScaleFactorChanged();
}
#endif

bool Frame::isURLAllowed(const URL& url) const
{
    // We allow one level of self-reference because some sites depend on that,
    // but we don't allow more than one.
    if (m_page->subframeCount() >= Page::maxNumberOfFrames)
        return false;
    bool foundSelfReference = false;
    for (const Frame* frame = this; frame; frame = frame->tree().parent()) {
        if (equalIgnoringFragmentIdentifier(frame->document()->url(), url)) {
            if (foundSelfReference)
                return false;
            foundSelfReference = true;
        }
    }
    return true;
}

} // namespace WebCore
