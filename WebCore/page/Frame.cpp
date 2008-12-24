/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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
#include "FramePrivate.h"

#include "ApplyStyleCommand.h"
#include "BeforeUnloadEvent.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CachedCSSStyleSheet.h"
#include "DOMWindow.h"
#include "DocLoader.h"
#include "DocumentType.h"
#include "EditingText.h"
#include "EditorClient.h"
#include "EventNames.h"
#include "FocusController.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLDocument.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLFormControlElement.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "HitTestResult.h"
#include "JSDOMWindowShell.h"
#include "Logging.h"
#include "markup.h"
#include "MediaFeatureNames.h"
#include "Navigator.h"
#include "NodeList.h"
#include "Page.h"
#include "RegularExpression.h"
#include "RenderPart.h"
#include "RenderTableCell.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Settings.h"
#include "SystemTime.h"
#include "TextIterator.h"
#include "TextResourceDecoder.h"
#include "XMLNames.h"
#include "ScriptController.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include "visible_units.h"
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>

#if FRAME_LOADS_USER_STYLESHEET
#include "UserStyleSheetLoader.h"
#endif

#if ENABLE(SVG)
#include "SVGDocument.h"
#include "SVGDocumentExtensions.h"
#include "SVGNames.h"
#include "XLinkNames.h"
#endif

#if ENABLE(WML)
#include "WMLNames.h"
#endif

using namespace std;

namespace WebCore {

using namespace HTMLNames;

#ifndef NDEBUG    
static WTF::RefCountedLeakCounter frameCounter("Frame");
#endif

static inline Frame* parentFromOwnerElement(HTMLFrameOwnerElement* ownerElement)
{
    if (!ownerElement)
        return 0;
    return ownerElement->document()->frame();
}

Frame::Frame(Page* page, HTMLFrameOwnerElement* ownerElement, FrameLoaderClient* frameLoaderClient) 
    : d(new FramePrivate(page, parentFromOwnerElement(ownerElement), this, ownerElement, frameLoaderClient))
{
    AtomicString::init();
    HTMLNames::init();
    QualifiedName::init();
    MediaFeatureNames::init();

#if ENABLE(SVG)
    SVGNames::init();
    XLinkNames::init();
#endif

#if ENABLE(WML)
    WMLNames::init();
#endif

    XMLNames::init();

    if (!ownerElement)
        page->setMainFrame(this);
    else {
        page->incrementFrameCount();
        // Make sure we will not end up with two frames referencing the same owner element.
        ASSERT((!(ownerElement->m_contentFrame)) || (ownerElement->m_contentFrame->ownerElement() != ownerElement));        
        ownerElement->m_contentFrame = this;
    }

#ifndef NDEBUG
    frameCounter.increment();
#endif
}

Frame::~Frame()
{
    setView(0);
    loader()->clearRecordedFormValues();
    loader()->cancelAndClear();
    
    // FIXME: We should not be doing all this work inside the destructor

    ASSERT(!d->m_lifeSupportTimer.isActive());

#ifndef NDEBUG
    frameCounter.decrement();
#endif

    if (d->m_script.haveWindowShell())
        d->m_script.windowShell()->disconnectFrame();

    disconnectOwnerElement();
    
    if (d->m_domWindow)
        d->m_domWindow->disconnectFrame();

    HashSet<DOMWindow*>::iterator end = d->m_liveFormerWindows.end();
    for (HashSet<DOMWindow*>::iterator it = d->m_liveFormerWindows.begin(); it != end; ++it)
        (*it)->disconnectFrame();
            
    if (d->m_view) {
        d->m_view->hide();
        d->m_view->clearFrame();
    }

    ASSERT(!d->m_lifeSupportTimer.isActive());

#if FRAME_LOADS_USER_STYLESHEET
    delete d->m_userStyleSheetLoader;
#endif

    delete d;
    d = 0;
}

void Frame::init()
{
    d->m_loader.init();
}

FrameLoader* Frame::loader() const
{
    return &d->m_loader;
}

FrameView* Frame::view() const
{
    return d->m_view.get();
}

void Frame::setView(FrameView* view)
{
    // Detach the document now, so any onUnload handlers get run - if
    // we wait until the view is destroyed, then things won't be
    // hooked up enough for some JavaScript calls to work.
    if (!view && d->m_doc && d->m_doc->attached() && !d->m_doc->inPageCache()) {
        // FIXME: We don't call willRemove here. Why is that OK?
        d->m_doc->detach();
        if (d->m_view)
            d->m_view->unscheduleRelayout();
    }
    eventHandler()->clear();

    d->m_view = view;

    // Only one form submission is allowed per view of a part.
    // Since this part may be getting reused as a result of being
    // pulled from the back/forward cache, reset this flag.
    loader()->resetMultipleFormSubmissionProtection();
}

ScriptController* Frame::script()
{
    return &d->m_script;
}

Document* Frame::document() const
{
    return d->m_doc.get();
}

void Frame::setDocument(PassRefPtr<Document> newDoc)
{
    if (d->m_doc && d->m_doc->attached() && !d->m_doc->inPageCache()) {
        // FIXME: We don't call willRemove here. Why is that OK?
        d->m_doc->detach();
    }

    d->m_doc = newDoc;
    if (d->m_doc && selection()->isFocusedAndActive())
        setUseSecureKeyboardEntry(d->m_doc->useSecureKeyboardEntryWhenActive());
        
    if (d->m_doc && !d->m_doc->attached())
        d->m_doc->attach();

    // Update the cached 'document' property, which is now stale.
    d->m_script.updateDocument();
}

Settings* Frame::settings() const
{
    return d->m_page ? d->m_page->settings() : 0;
}

String Frame::selectedText() const
{
    return plainText(selection()->toRange().get());
}

IntRect Frame::firstRectForRange(Range* range) const
{
    int extraWidthToEndOfLine = 0;
    ExceptionCode ec = 0;
    ASSERT(range->startContainer(ec));
    ASSERT(range->endContainer(ec));

    InlineBox* startInlineBox;
    int startCaretOffset;
    range->startPosition().getInlineBoxAndOffset(DOWNSTREAM, startInlineBox, startCaretOffset);

    RenderObject* startRenderer = range->startContainer(ec)->renderer();
    IntRect startCaretRect = startRenderer->localCaretRect(startInlineBox, startCaretOffset, &extraWidthToEndOfLine);
    if (startCaretRect != IntRect())
        startCaretRect = startRenderer->localToAbsoluteQuad(FloatRect(startCaretRect)).enclosingBoundingBox();

    InlineBox* endInlineBox;
    int endCaretOffset;
    range->endPosition().getInlineBoxAndOffset(UPSTREAM, endInlineBox, endCaretOffset);

    RenderObject* endRenderer = range->endContainer(ec)->renderer();
    IntRect endCaretRect = endRenderer->localCaretRect(endInlineBox, endCaretOffset);
    if (endCaretRect != IntRect())
        endCaretRect = endRenderer->localToAbsoluteQuad(FloatRect(endCaretRect)).enclosingBoundingBox();

    if (startCaretRect.y() == endCaretRect.y()) {
        // start and end are on the same line
        return IntRect(min(startCaretRect.x(), endCaretRect.x()), 
                       startCaretRect.y(), 
                       abs(endCaretRect.x() - startCaretRect.x()),
                       max(startCaretRect.height(), endCaretRect.height()));
    }
    
    // start and end aren't on the same line, so go from start to the end of its line
    return IntRect(startCaretRect.x(), 
                   startCaretRect.y(),
                   startCaretRect.width() + extraWidthToEndOfLine,
                   startCaretRect.height());
}

SelectionController* Frame::selection() const
{
    return &d->m_selectionController;
}

Editor* Frame::editor() const
{
    return &d->m_editor;
}

TextGranularity Frame::selectionGranularity() const
{
    return d->m_selectionGranularity;
}

void Frame::setSelectionGranularity(TextGranularity granularity) const
{
    d->m_selectionGranularity = granularity;
}

SelectionController* Frame::dragCaretController() const
{
    return d->m_page->dragCaretController();
}


AnimationController* Frame::animation() const
{
    return &d->m_animationController;
}

static RegularExpression* createRegExpForLabels(const Vector<String>& labels)
{
    // REVIEW- version of this call in FrameMac.mm caches based on the NSArray ptrs being
    // the same across calls.  We can't do that.

    DEFINE_STATIC_LOCAL(RegularExpression, wordRegExp, ("\\w"));
    String pattern("(");
    unsigned int numLabels = labels.size();
    unsigned int i;
    for (i = 0; i < numLabels; i++) {
        String label = labels[i];

        bool startsWithWordChar = false;
        bool endsWithWordChar = false;
        if (label.length() != 0) {
            startsWithWordChar = wordRegExp.search(label.substring(0, 1)) >= 0;
            endsWithWordChar = wordRegExp.search(label.substring(label.length() - 1, 1)) >= 0;
        }
        
        if (i != 0)
            pattern.append("|");
        // Search for word boundaries only if label starts/ends with "word characters".
        // If we always searched for word boundaries, this wouldn't work for languages
        // such as Japanese.
        if (startsWithWordChar) {
            pattern.append("\\b");
        }
        pattern.append(label);
        if (endsWithWordChar) {
            pattern.append("\\b");
        }
    }
    pattern.append(")");
    return new RegularExpression(pattern, false);
}

String Frame::searchForLabelsAboveCell(RegularExpression* regExp, HTMLTableCellElement* cell)
{
    RenderTableCell* cellRenderer = static_cast<RenderTableCell*>(cell->renderer());

    if (cellRenderer && cellRenderer->isTableCell()) {
        RenderTableCell* cellAboveRenderer = cellRenderer->table()->cellAbove(cellRenderer);

        if (cellAboveRenderer) {
            HTMLTableCellElement* aboveCell =
                static_cast<HTMLTableCellElement*>(cellAboveRenderer->element());

            if (aboveCell) {
                // search within the above cell we found for a match
                for (Node* n = aboveCell->firstChild(); n; n = n->traverseNextNode(aboveCell)) {
                    if (n->isTextNode() && n->renderer() && n->renderer()->style()->visibility() == VISIBLE) {
                        // For each text chunk, run the regexp
                        String nodeString = n->nodeValue();
                        int pos = regExp->searchRev(nodeString);
                        if (pos >= 0)
                            return nodeString.substring(pos, regExp->matchedLength());
                    }
                }
            }
        }
    }
    // Any reason in practice to search all cells in that are above cell?
    return String();
}

String Frame::searchForLabelsBeforeElement(const Vector<String>& labels, Element* element)
{
    OwnPtr<RegularExpression> regExp(createRegExpForLabels(labels));
    // We stop searching after we've seen this many chars
    const unsigned int charsSearchedThreshold = 500;
    // This is the absolute max we search.  We allow a little more slop than
    // charsSearchedThreshold, to make it more likely that we'll search whole nodes.
    const unsigned int maxCharsSearched = 600;
    // If the starting element is within a table, the cell that contains it
    HTMLTableCellElement* startingTableCell = 0;
    bool searchedCellAbove = false;

    // walk backwards in the node tree, until another element, or form, or end of tree
    int unsigned lengthSearched = 0;
    Node* n;
    for (n = element->traversePreviousNode();
         n && lengthSearched < charsSearchedThreshold;
         n = n->traversePreviousNode())
    {
        if (n->hasTagName(formTag)
            || (n->isHTMLElement()
                && static_cast<HTMLElement*>(n)->isGenericFormElement()))
        {
            // We hit another form element or the start of the form - bail out
            break;
        } else if (n->hasTagName(tdTag) && !startingTableCell) {
            startingTableCell = static_cast<HTMLTableCellElement*>(n);
        } else if (n->hasTagName(trTag) && startingTableCell) {
            String result = searchForLabelsAboveCell(regExp.get(), startingTableCell);
            if (!result.isEmpty())
                return result;
            searchedCellAbove = true;
        } else if (n->isTextNode() && n->renderer() && n->renderer()->style()->visibility() == VISIBLE) {
            // For each text chunk, run the regexp
            String nodeString = n->nodeValue();
            // add 100 for slop, to make it more likely that we'll search whole nodes
            if (lengthSearched + nodeString.length() > maxCharsSearched)
                nodeString = nodeString.right(charsSearchedThreshold - lengthSearched);
            int pos = regExp->searchRev(nodeString);
            if (pos >= 0)
                return nodeString.substring(pos, regExp->matchedLength());
            lengthSearched += nodeString.length();
        }
    }

    // If we started in a cell, but bailed because we found the start of the form or the
    // previous element, we still might need to search the row above us for a label.
    if (startingTableCell && !searchedCellAbove) {
         return searchForLabelsAboveCell(regExp.get(), startingTableCell);
    }
    return String();
}

String Frame::matchLabelsAgainstElement(const Vector<String>& labels, Element* element)
{
    String name = element->getAttribute(nameAttr);
    if (name.isEmpty())
        return String();

    // Make numbers and _'s in field names behave like word boundaries, e.g., "address2"
    replace(name, RegularExpression("\\d"), " ");
    name.replace('_', ' ');
    
    OwnPtr<RegularExpression> regExp(createRegExpForLabels(labels));
    // Use the largest match we can find in the whole name string
    int pos;
    int length;
    int bestPos = -1;
    int bestLength = -1;
    int start = 0;
    do {
        pos = regExp->search(name, start);
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
        return name.substring(bestPos, bestLength);
    return String();
}

const Selection& Frame::mark() const
{
    return d->m_mark;
}

void Frame::setMark(const Selection& s)
{
    ASSERT(!s.base().node() || s.base().node()->document() == document());
    ASSERT(!s.extent().node() || s.extent().node()->document() == document());
    ASSERT(!s.start().node() || s.start().node()->document() == document());
    ASSERT(!s.end().node() || s.end().node()->document() == document());

    d->m_mark = s;
}

void Frame::notifyRendererOfSelectionChange(bool userTriggered)
{
    RenderObject* renderer = 0;
    if (selection()->rootEditableElement())
        renderer = selection()->rootEditableElement()->shadowAncestorNode()->renderer();

    // If the current selection is in a textfield or textarea, notify the renderer that the selection has changed
    if (renderer && (renderer->isTextArea() || renderer->isTextField()))
        static_cast<RenderTextControl*>(renderer)->selectionChanged(userTriggered);
}

void Frame::invalidateSelection()
{
    selection()->setNeedsLayout();
    selectionLayoutChanged();
}

void Frame::setCaretVisible(bool flag)
{
    if (d->m_caretVisible == flag)
        return;
    clearCaretRectIfNeeded();
    d->m_caretVisible = flag;
    selectionLayoutChanged();
}

void Frame::clearCaretRectIfNeeded()
{
#if ENABLE(TEXT_CARET)
    if (d->m_caretPaint) {
        d->m_caretPaint = false;
        selection()->invalidateCaretRect();
    }
#endif
}

// Helper function that tells whether a particular node is an element that has an entire
// Frame and FrameView, a <frame>, <iframe>, or <object>.
static bool isFrameElement(const Node *n)
{
    if (!n)
        return false;
    RenderObject *renderer = n->renderer();
    if (!renderer || !renderer->isWidget())
        return false;
    Widget* widget = static_cast<RenderWidget*>(renderer)->widget();
    return widget && widget->isFrameView();
}

void Frame::setFocusedNodeIfNeeded()
{
    if (!document() || selection()->isNone() || !selection()->isFocusedAndActive())
        return;

    Node* target = selection()->rootEditableElement();
    if (target) {
        RenderObject* renderer = target->renderer();

        // Walk up the render tree to search for a node to focus.
        // Walking up the DOM tree wouldn't work for shadow trees, like those behind the engine-based text fields.
        while (renderer) {
            // We don't want to set focus on a subframe when selecting in a parent frame,
            // so add the !isFrameElement check here. There's probably a better way to make this
            // work in the long term, but this is the safest fix at this time.
            if (target && target->isMouseFocusable() && !isFrameElement(target)) {
                page()->focusController()->setFocusedNode(target, this);
                return;
            }
            renderer = renderer->parent();
            if (renderer)
                target = renderer->element();
        }
        document()->setFocusedNode(0);
    }
}

void Frame::selectionLayoutChanged()
{
    bool caretRectChanged = selection()->recomputeCaretRect();

#if ENABLE(TEXT_CARET)
    bool shouldBlink = d->m_caretVisible
        && selection()->isCaret() && selection()->isContentEditable();

    // If the caret moved, stop the blink timer so we can restart with a
    // black caret in the new location.
    if (caretRectChanged || !shouldBlink)
        d->m_caretBlinkTimer.stop();

    // Start blinking with a black caret. Be sure not to restart if we're
    // already blinking in the right location.
    if (shouldBlink && !d->m_caretBlinkTimer.isActive()) {
        if (double blinkInterval = theme()->caretBlinkInterval())
            d->m_caretBlinkTimer.startRepeating(blinkInterval);

        if (!d->m_caretPaint) {
            d->m_caretPaint = true;
            selection()->invalidateCaretRect();
        }
    }
#else
    if (!caretRectChanged)
        return;
#endif

    RenderView* view = contentRenderer();
    if (!view)
        return;

    Selection selection = this->selection()->selection();
        
    if (!selection.isRange())
        view->clearSelection();
    else {
        // Use the rightmost candidate for the start of the selection, and the leftmost candidate for the end of the selection.
        // Example: foo <a>bar</a>.  Imagine that a line wrap occurs after 'foo', and that 'bar' is selected.   If we pass [foo, 3]
        // as the start of the selection, the selection painting code will think that content on the line containing 'foo' is selected
        // and will fill the gap before 'bar'.
        Position startPos = selection.start();
        if (startPos.downstream().isCandidate())
            startPos = startPos.downstream();
        Position endPos = selection.end();
        if (endPos.upstream().isCandidate())
            endPos = endPos.upstream();
        
        // We can get into a state where the selection endpoints map to the same VisiblePosition when a selection is deleted
        // because we don't yet notify the SelectionController of text removal.
        if (startPos.isNotNull() && endPos.isNotNull() && selection.visibleStart() != selection.visibleEnd()) {
            RenderObject *startRenderer = startPos.node()->renderer();
            RenderObject *endRenderer = endPos.node()->renderer();
            view->setSelection(startRenderer, startPos.offset(), endRenderer, endPos.offset());
        }
    }
}

void Frame::caretBlinkTimerFired(Timer<Frame>*)
{
#if ENABLE(TEXT_CARET)
    ASSERT(d->m_caretVisible);
    ASSERT(selection()->isCaret());
    bool caretPaint = d->m_caretPaint;
    if (selection()->isCaretBlinkingSuspended() && caretPaint)
        return;
    d->m_caretPaint = !caretPaint;
    selection()->invalidateCaretRect();
#endif
}

void Frame::paintCaret(GraphicsContext* p, int tx, int ty, const IntRect& clipRect) const
{
#if ENABLE(TEXT_CARET)
    if (d->m_caretPaint && d->m_caretVisible)
        selection()->paintCaret(p, tx, ty, clipRect);
#endif
}

void Frame::paintDragCaret(GraphicsContext* p, int tx, int ty, const IntRect& clipRect) const
{
#if ENABLE(TEXT_CARET)
    SelectionController* dragCaretController = d->m_page->dragCaretController();
    ASSERT(dragCaretController->selection().isCaret());
    if (dragCaretController->selection().start().node()->document()->frame() == this)
        dragCaretController->paintCaret(p, tx, ty, clipRect);
#endif
}

float Frame::zoomFactor() const
{
    return d->m_zoomFactor;
}

bool Frame::isZoomFactorTextOnly() const
{
    return d->m_page->settings()->zoomsTextOnly();
}

bool Frame::shouldApplyTextZoom() const
{
    if (d->m_zoomFactor == 1.0f || !isZoomFactorTextOnly())
        return false;
#if ENABLE(SVG)
    if (d->m_doc && d->m_doc->isSVGDocument())
        return false;
#endif
    return true;
}

bool Frame::shouldApplyPageZoom() const
{
    if (d->m_zoomFactor == 1.0f || isZoomFactorTextOnly())
        return false;
#if ENABLE(SVG)
    if (d->m_doc && d->m_doc->isSVGDocument())
        return false;
#endif
    return true;
}

void Frame::setZoomFactor(float percent, bool isTextOnly)
{  
    if (d->m_zoomFactor == percent && isZoomFactorTextOnly())
        return;

#if ENABLE(SVG)
    // SVG doesn't care if the zoom factor is text only.  It will always apply a 
    // zoom to the whole SVG.
    if (d->m_doc && d->m_doc->isSVGDocument()) {
        if (!static_cast<SVGDocument*>(d->m_doc.get())->zoomAndPanEnabled())
            return;
        d->m_zoomFactor = percent;
        d->m_page->settings()->setZoomsTextOnly(true); // We do this to avoid doing any scaling of CSS pixels, since the SVG has its own notion of zoom.
        if (d->m_doc->renderer())
            d->m_doc->renderer()->repaint();
        return;
    }
#endif

    d->m_zoomFactor = percent;
    d->m_page->settings()->setZoomsTextOnly(isTextOnly);

    if (d->m_doc)
        d->m_doc->recalcStyle(Node::Force);

    for (Frame* child = tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->setZoomFactor(d->m_zoomFactor, isTextOnly);

    if (d->m_doc && d->m_doc->renderer() && d->m_doc->renderer()->needsLayout() && view()->didFirstLayout())
        view()->layout();
}

void Frame::setPrinting(bool printing, float minPageWidth, float maxPageWidth, bool adjustViewSize)
{
    if (!d->m_doc)
        return;

    d->m_doc->setPrinting(printing);
    view()->setMediaType(printing ? "print" : "screen");
    d->m_doc->updateStyleSelector();
    forceLayoutWithPageWidthRange(minPageWidth, maxPageWidth, adjustViewSize);

    for (Frame* child = tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->setPrinting(printing, minPageWidth, maxPageWidth, adjustViewSize);
}

void Frame::setJSStatusBarText(const String& text)
{
    d->m_kjsStatusBarText = text;
    if (d->m_page)
        d->m_page->chrome()->setStatusbarText(this, d->m_kjsStatusBarText);
}

void Frame::setJSDefaultStatusBarText(const String& text)
{
    d->m_kjsDefaultStatusBarText = text;
    if (d->m_page)
        d->m_page->chrome()->setStatusbarText(this, d->m_kjsDefaultStatusBarText);
}

String Frame::jsStatusBarText() const
{
    return d->m_kjsStatusBarText;
}

String Frame::jsDefaultStatusBarText() const
{
   return d->m_kjsDefaultStatusBarText;
}

void Frame::setNeedsReapplyStyles()
{
    if (d->m_needsReapplyStyles)
        return;

    d->m_needsReapplyStyles = true;

    // FrameView's "layout" timer includes reapplyStyles, so despite its
    // name, it's what we want to call here.
    if (view())
        view()->scheduleRelayout();
}

bool Frame::needsReapplyStyles() const
{
    return d->m_needsReapplyStyles;
}

void Frame::reapplyStyles()
{
    d->m_needsReapplyStyles = false;

    // FIXME: This call doesn't really make sense in a method called
    // "reapplyStyles". We should probably eventually move it into its own
    // method.
    if (d->m_doc)
        d->m_doc->docLoader()->setAutoLoadImages(d->m_page && d->m_page->settings()->loadsImagesAutomatically());
        
#if FRAME_LOADS_USER_STYLESHEET
    const KURL userStyleSheetLocation = d->m_page ? d->m_page->settings()->userStyleSheetLocation() : KURL();
    if (!userStyleSheetLocation.isEmpty())
        setUserStyleSheetLocation(userStyleSheetLocation);
    else
        setUserStyleSheet(String());
#endif

    // FIXME: It's not entirely clear why the following is needed.
    // The document automatically does this as required when you set the style sheet.
    // But we had problems when this code was removed. Details are in
    // <http://bugs.webkit.org/show_bug.cgi?id=8079>.
    if (d->m_doc)
        d->m_doc->updateStyleSelector();
}

bool Frame::shouldChangeSelection(const Selection& newSelection) const
{
    return shouldChangeSelection(selection()->selection(), newSelection, newSelection.affinity(), false);
}

bool Frame::shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity affinity, bool stillSelecting) const
{
    return editor()->client()->shouldChangeSelectedRange(oldSelection.toRange().get(), newSelection.toRange().get(),
                                                         affinity, stillSelecting);
}

bool Frame::shouldDeleteSelection(const Selection& selection) const
{
    return editor()->client()->shouldDeleteRange(selection.toRange().get());
}

bool Frame::isContentEditable() const 
{
    if (d->m_editor.clientIsEditable())
        return true;
    if (!d->m_doc)
        return false;
    return d->m_doc->inDesignMode();
}

#if !PLATFORM(MAC)

void Frame::setUseSecureKeyboardEntry(bool)
{
}

#endif

void Frame::updateSecureKeyboardEntryIfActive()
{
    if (selection()->isFocusedAndActive())
        setUseSecureKeyboardEntry(d->m_doc->useSecureKeyboardEntryWhenActive());
}

CSSMutableStyleDeclaration *Frame::typingStyle() const
{
    return d->m_typingStyle.get();
}

void Frame::setTypingStyle(CSSMutableStyleDeclaration *style)
{
    d->m_typingStyle = style;
}

void Frame::clearTypingStyle()
{
    d->m_typingStyle = 0;
}

void Frame::computeAndSetTypingStyle(CSSStyleDeclaration *style, EditAction editingAction)
{
    if (!style || style->length() == 0) {
        clearTypingStyle();
        return;
    }

    // Calculate the current typing style.
    RefPtr<CSSMutableStyleDeclaration> mutableStyle = style->makeMutable();
    if (typingStyle()) {
        typingStyle()->merge(mutableStyle.get());
        mutableStyle = typingStyle();
    }

    RefPtr<CSSValue> unicodeBidi;
    RefPtr<CSSValue> direction;
    if (editingAction == EditActionSetWritingDirection) {
        unicodeBidi = mutableStyle->getPropertyCSSValue(CSSPropertyUnicodeBidi);
        direction = mutableStyle->getPropertyCSSValue(CSSPropertyDirection);
    }

    Node* node = selection()->selection().visibleStart().deepEquivalent().node();
    computedStyle(node)->diff(mutableStyle.get());

    if (editingAction == EditActionSetWritingDirection && unicodeBidi) {
        ASSERT(unicodeBidi->isPrimitiveValue());
        mutableStyle->setProperty(CSSPropertyUnicodeBidi, static_cast<CSSPrimitiveValue*>(unicodeBidi.get())->getIdent());
        if (direction) {
            ASSERT(direction->isPrimitiveValue());
            mutableStyle->setProperty(CSSPropertyDirection, static_cast<CSSPrimitiveValue*>(direction.get())->getIdent());
        }
    }

    // Handle block styles, substracting these from the typing style.
    RefPtr<CSSMutableStyleDeclaration> blockStyle = mutableStyle->copyBlockProperties();
    blockStyle->diff(mutableStyle.get());
    if (document() && blockStyle->length() > 0)
        applyCommand(ApplyStyleCommand::create(document(), blockStyle.get(), editingAction));
    
    // Set the remaining style as the typing style.
    d->m_typingStyle = mutableStyle.release();
}

String Frame::selectionStartStylePropertyValue(int stylePropertyID) const
{
    Node *nodeToRemove;
    RefPtr<CSSStyleDeclaration> selectionStyle = selectionComputedStyle(nodeToRemove);
    if (!selectionStyle)
        return String();

    String value = selectionStyle->getPropertyValue(stylePropertyID);

    if (nodeToRemove) {
        ExceptionCode ec = 0;
        nodeToRemove->remove(ec);
        ASSERT(ec == 0);
    }

    return value;
}

PassRefPtr<CSSComputedStyleDeclaration> Frame::selectionComputedStyle(Node*& nodeToRemove) const
{
    nodeToRemove = 0;

    if (!document())
        return 0;

    if (selection()->isNone())
        return 0;

    RefPtr<Range> range(selection()->toRange());
    Position pos = range->editingStartPosition();

    Element *elem = pos.element();
    if (!elem)
        return 0;
    
    RefPtr<Element> styleElement = elem;
    ExceptionCode ec = 0;

    if (d->m_typingStyle) {
        styleElement = document()->createElementNS(xhtmlNamespaceURI, "span", ec);
        ASSERT(ec == 0);

        styleElement->setAttribute(styleAttr, d->m_typingStyle->cssText().impl(), ec);
        ASSERT(ec == 0);
        
        styleElement->appendChild(document()->createEditingTextNode(""), ec);
        ASSERT(ec == 0);

        if (elem->renderer() && elem->renderer()->canHaveChildren()) {
            elem->appendChild(styleElement, ec);
        } else {
            Node *parent = elem->parent();
            Node *next = elem->nextSibling();

            if (next) {
                parent->insertBefore(styleElement, next, ec);
            } else {
                parent->appendChild(styleElement, ec);
            }
        }
        ASSERT(ec == 0);

        nodeToRemove = styleElement.get();
    }

    return computedStyle(styleElement.release());
}

void Frame::textFieldDidBeginEditing(Element* e)
{
    if (editor()->client())
        editor()->client()->textFieldDidBeginEditing(e);
}

void Frame::textFieldDidEndEditing(Element* e)
{
    if (editor()->client())
        editor()->client()->textFieldDidEndEditing(e);
}

void Frame::textDidChangeInTextField(Element* e)
{
    if (editor()->client())
        editor()->client()->textDidChangeInTextField(e);
}

bool Frame::doTextFieldCommandFromEvent(Element* e, KeyboardEvent* ke)
{
    if (editor()->client())
        return editor()->client()->doTextFieldCommandFromEvent(e, ke);

    return false;
}

void Frame::textWillBeDeletedInTextField(Element* input)
{
    if (editor()->client())
        editor()->client()->textWillBeDeletedInTextField(input);
}

void Frame::textDidChangeInTextArea(Element* e)
{
    if (editor()->client())
        editor()->client()->textDidChangeInTextArea(e);
}

void Frame::applyEditingStyleToBodyElement() const
{
    if (!d->m_doc)
        return;
        
    RefPtr<NodeList> list = d->m_doc->getElementsByTagName("body");
    unsigned len = list->length();
    for (unsigned i = 0; i < len; i++) {
        applyEditingStyleToElement(static_cast<Element*>(list->item(i)));    
    }
}

void Frame::removeEditingStyleFromBodyElement() const
{
    if (!d->m_doc)
        return;
        
    RefPtr<NodeList> list = d->m_doc->getElementsByTagName("body");
    unsigned len = list->length();
    for (unsigned i = 0; i < len; i++) {
        removeEditingStyleFromElement(static_cast<Element*>(list->item(i)));    
    }
}

void Frame::applyEditingStyleToElement(Element* element) const
{
    if (!element)
        return;

    CSSStyleDeclaration* style = element->style();
    ASSERT(style);

    ExceptionCode ec = 0;
    style->setProperty(CSSPropertyWordWrap, "break-word", false, ec);
    ASSERT(ec == 0);
    style->setProperty(CSSPropertyWebkitNbspMode, "space", false, ec);
    ASSERT(ec == 0);
    style->setProperty(CSSPropertyWebkitLineBreak, "after-white-space", false, ec);
    ASSERT(ec == 0);
}

void Frame::removeEditingStyleFromElement(Element*) const
{
}

#ifndef NDEBUG
static HashSet<Frame*>& keepAliveSet()
{
    DEFINE_STATIC_LOCAL(HashSet<Frame*>, staticKeepAliveSet, ());
    return staticKeepAliveSet;
}
#endif

void Frame::keepAlive()
{
    if (d->m_lifeSupportTimer.isActive())
        return;
#ifndef NDEBUG
    keepAliveSet().add(this);
#endif
    ref();
    d->m_lifeSupportTimer.startOneShot(0);
}

#ifndef NDEBUG
void Frame::cancelAllKeepAlive()
{
    HashSet<Frame*>::iterator end = keepAliveSet().end();
    for (HashSet<Frame*>::iterator it = keepAliveSet().begin(); it != end; ++it) {
        Frame* frame = *it;
        frame->d->m_lifeSupportTimer.stop();
        frame->deref();
    }
    keepAliveSet().clear();
}
#endif

void Frame::lifeSupportTimerFired(Timer<Frame>*)
{
#ifndef NDEBUG
    keepAliveSet().remove(this);
#endif
    deref();
}

void Frame::clearDOMWindow()
{
    if (d->m_domWindow) {
        d->m_liveFormerWindows.add(d->m_domWindow.get());
        d->m_domWindow->clear();
    }
    d->m_domWindow = 0;
}

RenderView* Frame::contentRenderer() const
{
    Document* doc = document();
    if (!doc)
        return 0;
    RenderObject* object = doc->renderer();
    if (!object)
        return 0;
    ASSERT(object->isRenderView());
    return static_cast<RenderView*>(object);
}

HTMLFrameOwnerElement* Frame::ownerElement() const
{
    return d->m_ownerElement;
}

RenderPart* Frame::ownerRenderer() const
{
    HTMLFrameOwnerElement* ownerElement = d->m_ownerElement;
    if (!ownerElement)
        return 0;
    RenderObject* object = ownerElement->renderer();
    if (!object)
        return 0;
    // FIXME: If <object> is ever fixed to disassociate itself from frames
    // that it has started but canceled, then this can turn into an ASSERT
    // since d->m_ownerElement would be 0 when the load is canceled.
    // https://bugs.webkit.org/show_bug.cgi?id=18585
    if (!object->isRenderPart())
        return 0;
    return static_cast<RenderPart*>(object);
}

bool Frame::isDisconnected() const
{
    return d->m_isDisconnected;
}

void Frame::setIsDisconnected(bool isDisconnected)
{
    d->m_isDisconnected = isDisconnected;
}

bool Frame::excludeFromTextSearch() const
{
    return d->m_excludeFromTextSearch;
}

void Frame::setExcludeFromTextSearch(bool exclude)
{
    d->m_excludeFromTextSearch = exclude;
}

// returns FloatRect because going through IntRect would truncate any floats
FloatRect Frame::selectionBounds(bool clipToVisibleContent) const
{
    RenderView* root = contentRenderer();
    FrameView* view = d->m_view.get();
    if (!root || !view)
        return IntRect();
    
    IntRect selectionRect = root->selectionBounds(clipToVisibleContent);
    return clipToVisibleContent ? intersection(selectionRect, view->visibleContentRect()) : selectionRect;
}

void Frame::selectionTextRects(Vector<FloatRect>& rects, bool clipToVisibleContent) const
{
    RenderView* root = contentRenderer();
    if (!root)
        return;

    RefPtr<Range> selectedRange = selection()->toRange();

    Vector<IntRect> intRects;
    selectedRange->addLineBoxRects(intRects, true);

    unsigned size = intRects.size();
    FloatRect visibleContentRect = d->m_view->visibleContentRect();
    for (unsigned i = 0; i < size; ++i)
        if (clipToVisibleContent)
            rects.append(intersection(intRects[i], visibleContentRect));
        else
            rects.append(intRects[i]);
}


bool Frame::isFrameSet() const
{
    Document* document = d->m_doc.get();
    if (!document || !document->isHTMLDocument())
        return false;
    Node *body = static_cast<HTMLDocument*>(document)->body();
    return body && body->renderer() && body->hasTagName(framesetTag);
}

// Scans logically forward from "start", including any child frames
static HTMLFormElement *scanForForm(Node *start)
{
    Node *n;
    for (n = start; n; n = n->traverseNextNode()) {
        if (n->hasTagName(formTag))
            return static_cast<HTMLFormElement*>(n);
        else if (n->isHTMLElement() && static_cast<HTMLElement*>(n)->isGenericFormElement())
            return static_cast<HTMLFormControlElement*>(n)->form();
        else if (n->hasTagName(frameTag) || n->hasTagName(iframeTag)) {
            Node *childDoc = static_cast<HTMLFrameElementBase*>(n)->contentDocument();
            if (HTMLFormElement *frameResult = scanForForm(childDoc))
                return frameResult;
        }
    }
    return 0;
}

// We look for either the form containing the current focus, or for one immediately after it
HTMLFormElement *Frame::currentForm() const
{
    // start looking either at the active (first responder) node, or where the selection is
    Node *start = d->m_doc ? d->m_doc->focusedNode() : 0;
    if (!start)
        start = selection()->start().node();
    
    // try walking up the node tree to find a form element
    Node *n;
    for (n = start; n; n = n->parentNode()) {
        if (n->hasTagName(formTag))
            return static_cast<HTMLFormElement*>(n);
        else if (n->isHTMLElement()
                   && static_cast<HTMLElement*>(n)->isGenericFormElement())
            return static_cast<HTMLFormControlElement*>(n)->form();
    }
    
    // try walking forward in the node tree to find a form element
    return start ? scanForForm(start) : 0;
}

// FIXME: should this go in SelectionController?
void Frame::revealSelection(const RenderLayer::ScrollAlignment& alignment) const
{
    IntRect rect;
    
    switch (selection()->state()) {
        case Selection::NONE:
            return;
            
        case Selection::CARET:
            rect = selection()->absoluteCaretBounds();
            break;
            
        case Selection::RANGE:
            rect = enclosingIntRect(selectionBounds(false));
            break;
    }

    Position start = selection()->start();

    ASSERT(start.node());
    if (start.node() && start.node()->renderer()) {
        // FIXME: This code only handles scrolling the startContainer's layer, but
        // the selection rect could intersect more than just that. 
        // See <rdar://problem/4799899>.
        if (RenderLayer *layer = start.node()->renderer()->enclosingLayer())
            layer->scrollRectToVisible(rect, false, alignment, alignment);
    }
}

void Frame::revealCaret(const RenderLayer::ScrollAlignment& alignment) const
{
    if (selection()->isNone())
        return;

    Position extent = selection()->extent();
    if (extent.node() && extent.node()->renderer()) {
        IntRect extentRect = VisiblePosition(extent).absoluteCaretBounds();
        RenderLayer* layer = extent.node()->renderer()->enclosingLayer();
        if (layer)
            layer->scrollRectToVisible(extentRect, false, alignment, alignment);
    }
}

void Frame::adjustPageHeight(float *newBottom, float oldTop, float oldBottom, float bottomLimit)
{
    RenderView* root = contentRenderer();
    if (root) {
        // Use a context with painting disabled.
        GraphicsContext context((PlatformGraphicsContext*)0);
        root->setTruncatedAt((int)floorf(oldBottom));
        IntRect dirtyRect(0, (int)floorf(oldTop), root->docWidth(), (int)ceilf(oldBottom - oldTop));
        root->layer()->paint(&context, dirtyRect);
        *newBottom = root->bestTruncatedAt();
        if (*newBottom == 0)
            *newBottom = oldBottom;
    } else
        *newBottom = oldBottom;
}

Frame* Frame::frameForWidget(const Widget* widget)
{
    ASSERT_ARG(widget, widget);

    if (RenderWidget* renderer = RenderWidget::find(widget))
        if (Node* node = renderer->node())
            return node->document()->frame();

    // Assume all widgets are either a FrameView or owned by a RenderWidget.
    // FIXME: That assumption is not right for scroll bars!
    ASSERT(widget->isFrameView());
    return static_cast<const FrameView*>(widget)->frame();
}

void Frame::forceLayout(bool allowSubtree)
{
    FrameView *v = d->m_view.get();
    if (v) {
        v->layout(allowSubtree);
        // We cannot unschedule a pending relayout, since the force can be called with
        // a tiny rectangle from a drawRect update.  By unscheduling we in effect
        // "validate" and stop the necessary full repaint from occurring.  Basically any basic
        // append/remove DHTML is broken by this call.  For now, I have removed the optimization
        // until we have a better invalidation stategy. -dwh
        //v->unscheduleRelayout();
    }
}

void Frame::forceLayoutWithPageWidthRange(float minPageWidth, float maxPageWidth, bool adjustViewSize)
{
    // Dumping externalRepresentation(m_frame->renderer()).ascii() is a good trick to see
    // the state of things before and after the layout
    RenderView *root = static_cast<RenderView*>(document()->renderer());
    if (root) {
        // This magic is basically copied from khtmlview::print
        int pageW = (int)ceilf(minPageWidth);
        root->setWidth(pageW);
        root->setNeedsLayoutAndPrefWidthsRecalc();
        forceLayout();
        
        // If we don't fit in the minimum page width, we'll lay out again. If we don't fit in the
        // maximum page width, we will lay out to the maximum page width and clip extra content.
        // FIXME: We are assuming a shrink-to-fit printing implementation.  A cropping
        // implementation should not do this!
        int rightmostPos = root->rightmostPosition();
        if (rightmostPos > minPageWidth) {
            pageW = min(rightmostPos, (int)ceilf(maxPageWidth));
            root->setWidth(pageW);
            root->setNeedsLayoutAndPrefWidthsRecalc();
            forceLayout();
        }
    }

    if (adjustViewSize && view())
        view()->adjustViewSize();
}

void Frame::sendResizeEvent()
{
    if (Document* doc = document())
        doc->dispatchWindowEvent(eventNames().resizeEvent, false, false);
}

void Frame::sendScrollEvent()
{
    FrameView* v = d->m_view.get();
    if (!v)
        return;
    v->setWasScrolledByUser(true);
    Document* doc = document();
    if (!doc)
        return;
    doc->dispatchEventForType(eventNames().scrollEvent, true, false);
}

void Frame::clearTimers(FrameView *view, Document *document)
{
    if (view) {
        view->unscheduleRelayout();
        if (view->frame()) {
            if (document && document->renderer() && document->renderer()->hasLayer())
                document->renderer()->layer()->suspendMarquees();
            view->frame()->animation()->suspendAnimations(document);
            view->frame()->eventHandler()->stopAutoscrollTimer();
        }
    }
}

void Frame::clearTimers()
{
    clearTimers(d->m_view.get(), document());
}

RenderStyle *Frame::styleForSelectionStart(Node *&nodeToRemove) const
{
    nodeToRemove = 0;
    
    if (!document())
        return 0;
    if (selection()->isNone())
        return 0;
    
    Position pos = selection()->selection().visibleStart().deepEquivalent();
    if (!pos.isCandidate())
        return 0;
    Node *node = pos.node();
    if (!node)
        return 0;
    
    if (!d->m_typingStyle)
        return node->renderer()->style();
    
    ExceptionCode ec = 0;
    RefPtr<Element> styleElement = document()->createElementNS(xhtmlNamespaceURI, "span", ec);
    ASSERT(ec == 0);
    
    String styleText = d->m_typingStyle->cssText() + " display: inline";
    styleElement->setAttribute(styleAttr, styleText.impl(), ec);
    ASSERT(ec == 0);
    
    styleElement->appendChild(document()->createEditingTextNode(""), ec);
    ASSERT(ec == 0);
    
    node->parentNode()->appendChild(styleElement, ec);
    ASSERT(ec == 0);
    
    nodeToRemove = styleElement.get();    
    return styleElement->renderer() ? styleElement->renderer()->style() : 0;
}

void Frame::setSelectionFromNone()
{
    // Put a caret inside the body if the entire frame is editable (either the 
    // entire WebView is editable or designMode is on for this document).
    Document *doc = document();
    if (!doc || !selection()->isNone() || !isContentEditable())
        return;
        
    Node* node = doc->documentElement();
    while (node && !node->hasTagName(bodyTag))
        node = node->traverseNextNode();
    if (node)
        selection()->setSelection(Selection(Position(node, 0), DOWNSTREAM));
}

bool Frame::inViewSourceMode() const
{
    return d->m_inViewSourceMode;
}

void Frame::setInViewSourceMode(bool mode) const
{
    d->m_inViewSourceMode = mode;
}

UChar Frame::backslashAsCurrencySymbol() const
{
    Document *doc = document();
    if (!doc)
        return '\\';
    TextResourceDecoder *decoder = doc->decoder();
    if (!decoder)
        return '\\';

    return decoder->encoding().backslashAsCurrencySymbol();
}

// Searches from the beginning of the document if nothing is selected.
bool Frame::findString(const String& target, bool forward, bool caseFlag, bool wrapFlag, bool startInSelection)
{
    if (target.isEmpty() || !document())
        return false;
    
    if (excludeFromTextSearch())
        return false;
    
    // Start from an edge of the selection, if there's a selection that's not in shadow content. Which edge
    // is used depends on whether we're searching forward or backward, and whether startInSelection is set.
    RefPtr<Range> searchRange(rangeOfContents(document()));
    Selection selection = this->selection()->selection();

    if (forward)
        setStart(searchRange.get(), startInSelection ? selection.visibleStart() : selection.visibleEnd());
    else
        setEnd(searchRange.get(), startInSelection ? selection.visibleEnd() : selection.visibleStart());

    Node* shadowTreeRoot = selection.shadowTreeRootNode();
    if (shadowTreeRoot) {
        ExceptionCode ec = 0;
        if (forward)
            searchRange->setEnd(shadowTreeRoot, shadowTreeRoot->childNodeCount(), ec);
        else
            searchRange->setStart(shadowTreeRoot, 0, ec);
    }

    RefPtr<Range> resultRange(findPlainText(searchRange.get(), target, forward, caseFlag));
    // If we started in the selection and the found range exactly matches the existing selection, find again.
    // Build a selection with the found range to remove collapsed whitespace.
    // Compare ranges instead of selection objects to ignore the way that the current selection was made.
    if (startInSelection && *Selection(resultRange.get()).toRange() == *selection.toRange()) {
        searchRange = rangeOfContents(document());
        if (forward)
            setStart(searchRange.get(), selection.visibleEnd());
        else
            setEnd(searchRange.get(), selection.visibleStart());

        if (shadowTreeRoot) {
            ExceptionCode ec = 0;
            if (forward)
                searchRange->setEnd(shadowTreeRoot, shadowTreeRoot->childNodeCount(), ec);
            else
                searchRange->setStart(shadowTreeRoot, 0, ec);
        }

        resultRange = findPlainText(searchRange.get(), target, forward, caseFlag);
    }
    
    ExceptionCode exception = 0;

    // If nothing was found in the shadow tree, search in main content following the shadow tree.
    if (resultRange->collapsed(exception) && shadowTreeRoot) {
        searchRange = rangeOfContents(document());
        if (forward)
            searchRange->setStartAfter(shadowTreeRoot->shadowParentNode(), exception);
        else
            searchRange->setEndBefore(shadowTreeRoot->shadowParentNode(), exception);

        resultRange = findPlainText(searchRange.get(), target, forward, caseFlag);
    }
    
    if (!editor()->insideVisibleArea(resultRange.get())) {
        resultRange = editor()->nextVisibleRange(resultRange.get(), target, forward, caseFlag, wrapFlag);
        if (!resultRange)
            return false;
    }

    // If we didn't find anything and we're wrapping, search again in the entire document (this will
    // redundantly re-search the area already searched in some cases).
    if (resultRange->collapsed(exception) && wrapFlag) {
        searchRange = rangeOfContents(document());
        resultRange = findPlainText(searchRange.get(), target, forward, caseFlag);
        // We used to return false here if we ended up with the same range that we started with
        // (e.g., the selection was already the only instance of this text). But we decided that
        // this should be a success case instead, so we'll just fall through in that case.
    }

    if (resultRange->collapsed(exception))
        return false;

    this->selection()->setSelection(Selection(resultRange.get(), DOWNSTREAM));
    revealSelection();
    return true;
}

unsigned Frame::markAllMatchesForText(const String& target, bool caseFlag, unsigned limit)
{
    if (target.isEmpty() || !document())
        return 0;
    
    RefPtr<Range> searchRange(rangeOfContents(document()));
    
    ExceptionCode exception = 0;
    unsigned matchCount = 0;
    do {
        RefPtr<Range> resultRange(findPlainText(searchRange.get(), target, true, caseFlag));
        if (resultRange->collapsed(exception)) {
            if (!resultRange->startContainer()->isInShadowTree())
                break;

            searchRange = rangeOfContents(document());
            searchRange->setStartAfter(resultRange->startContainer()->shadowAncestorNode(), exception);
            continue;
        }
        
        // A non-collapsed result range can in some funky whitespace cases still not
        // advance the range's start position (4509328). Break to avoid infinite loop.
        VisiblePosition newStart = endVisiblePosition(resultRange.get(), DOWNSTREAM);
        if (newStart == startVisiblePosition(searchRange.get(), DOWNSTREAM))
            break;

        // Only treat the result as a match if it is visible
        if (editor()->insideVisibleArea(resultRange.get())) {
            ++matchCount;
            document()->addMarker(resultRange.get(), DocumentMarker::TextMatch);
        }
        
        // Stop looking if we hit the specified limit. A limit of 0 means no limit.
        if (limit > 0 && matchCount >= limit)
            break;
        
        setStart(searchRange.get(), newStart);
        Node* shadowTreeRoot = searchRange->shadowTreeRootNode();
        if (searchRange->collapsed(exception) && shadowTreeRoot)
            searchRange->setEnd(shadowTreeRoot, shadowTreeRoot->childNodeCount(), exception);
    } while (true);
    
    // Do a "fake" paint in order to execute the code that computes the rendered rect for 
    // each text match.
    Document* doc = document();
    if (doc && d->m_view && contentRenderer()) {
        doc->updateLayout(); // Ensure layout is up to date.
        IntRect visibleRect = d->m_view->visibleContentRect();
        if (!visibleRect.isEmpty()) {
            GraphicsContext context((PlatformGraphicsContext*)0);
            context.setPaintingDisabled(true);
            d->m_view->paintContents(&context, visibleRect);
        }
    }
    
    return matchCount;
}

bool Frame::markedTextMatchesAreHighlighted() const
{
    return d->m_highlightTextMatches;
}

void Frame::setMarkedTextMatchesAreHighlighted(bool flag)
{
    if (flag == d->m_highlightTextMatches || !document())
        return;
    
    d->m_highlightTextMatches = flag;
    document()->repaintMarkers(DocumentMarker::TextMatch);
}

FrameTree* Frame::tree() const
{
    return &d->m_treeNode;
}

void Frame::setDOMWindow(DOMWindow* domWindow)
{
    if (d->m_domWindow) {
        d->m_liveFormerWindows.add(d->m_domWindow.get());
        d->m_domWindow->clear();
    }
    d->m_domWindow = domWindow;
}

DOMWindow* Frame::domWindow() const
{
    if (!d->m_domWindow)
        d->m_domWindow = DOMWindow::create(const_cast<Frame*>(this));

    return d->m_domWindow.get();
}

void Frame::clearFormerDOMWindow(DOMWindow* window)
{
    d->m_liveFormerWindows.remove(window);    
}

Page* Frame::page() const
{
    return d->m_page;
}

EventHandler* Frame::eventHandler() const
{
    return &d->m_eventHandler;
}

void Frame::pageDestroyed()
{
    if (Frame* parent = tree()->parent())
        parent->loader()->checkLoadComplete();

    // FIXME: It's unclear as to why this is called more than once, but it is,
    // so page() could be NULL.
    if (page() && page()->focusController()->focusedFrame() == this)
        page()->focusController()->setFocusedFrame(0);

    script()->clearWindowShell();

    // This will stop any JS timers
    if (script()->haveWindowShell())
        script()->windowShell()->disconnectFrame();

    script()->clearScriptObjects();
    script()->updatePlatformScriptObjects();

    d->m_page = 0;
}

void Frame::disconnectOwnerElement()
{
    if (d->m_ownerElement) {
        if (Document* doc = document())
            doc->clearAXObjectCache();
        d->m_ownerElement->m_contentFrame = 0;
        if (d->m_page)
            d->m_page->decrementFrameCount();
    }
    d->m_ownerElement = 0;
}

String Frame::documentTypeString() const
{
    if (Document* doc = document()) {
        if (DocumentType* doctype = doc->doctype())
            return createMarkup(doctype);
    }

    return String();
}

void Frame::focusWindow()
{
    if (!page())
        return;

    // If we're a top level window, bring the window to the front.
    if (!tree()->parent())
        page()->chrome()->focus();

    eventHandler()->focusDocumentView();
}

void Frame::unfocusWindow()
{
    if (!page())
        return;
    
    // If we're a top level window, deactivate the window.
    if (!tree()->parent())
        page()->chrome()->unfocus();
}

bool Frame::shouldClose()
{
    Chrome* chrome = page() ? page()->chrome() : 0;
    if (!chrome || !chrome->canRunBeforeUnloadConfirmPanel())
        return true;

    RefPtr<Document> doc = document();
    if (!doc)
        return true;
    HTMLElement* body = doc->body();
    if (!body)
        return true;

    RefPtr<BeforeUnloadEvent> beforeUnloadEvent = BeforeUnloadEvent::create();
    beforeUnloadEvent->setTarget(doc);
    doc->handleWindowEvent(beforeUnloadEvent.get(), false);

    if (!beforeUnloadEvent->defaultPrevented() && doc)
        doc->defaultEventHandler(beforeUnloadEvent.get());
    if (beforeUnloadEvent->result().isNull())
        return true;

    String text = beforeUnloadEvent->result();
    text.replace('\\', backslashAsCurrencySymbol());

    return chrome->runBeforeUnloadConfirmPanel(text, this);
}

void Frame::scheduleClose()
{
    if (!shouldClose())
        return;

    Chrome* chrome = page() ? page()->chrome() : 0;
    if (chrome)
        chrome->closeWindowSoon();
}

void Frame::respondToChangedSelection(const Selection& oldSelection, bool closeTyping)
{
    if (document()) {
        bool isContinuousSpellCheckingEnabled = editor()->isContinuousSpellCheckingEnabled();
        bool isContinuousGrammarCheckingEnabled = isContinuousSpellCheckingEnabled && editor()->isGrammarCheckingEnabled();
        if (isContinuousSpellCheckingEnabled) {
            Selection newAdjacentWords;
            Selection newSelectedSentence;
            if (selection()->selection().isContentEditable()) {
                VisiblePosition newStart(selection()->selection().visibleStart());
                newAdjacentWords = Selection(startOfWord(newStart, LeftWordIfOnBoundary), endOfWord(newStart, RightWordIfOnBoundary));
                if (isContinuousGrammarCheckingEnabled)
                    newSelectedSentence = Selection(startOfSentence(newStart), endOfSentence(newStart));
            }

            // When typing we check spelling elsewhere, so don't redo it here.
            // If this is a change in selection resulting from a delete operation,
            // oldSelection may no longer be in the document.
            if (closeTyping && oldSelection.isContentEditable() && oldSelection.start().node() && oldSelection.start().node()->inDocument()) {
                VisiblePosition oldStart(oldSelection.visibleStart());
                Selection oldAdjacentWords = Selection(startOfWord(oldStart, LeftWordIfOnBoundary), endOfWord(oldStart, RightWordIfOnBoundary));   
                if (oldAdjacentWords != newAdjacentWords) {
                    editor()->markMisspellings(oldAdjacentWords);
                    if (isContinuousGrammarCheckingEnabled) {
                        Selection oldSelectedSentence = Selection(startOfSentence(oldStart), endOfSentence(oldStart));   
                        if (oldSelectedSentence != newSelectedSentence)
                            editor()->markBadGrammar(oldSelectedSentence);
                    }
                }
            }

            // This only erases markers that are in the first unit (word or sentence) of the selection.
            // Perhaps peculiar, but it matches AppKit.
            if (RefPtr<Range> wordRange = newAdjacentWords.toRange())
                document()->removeMarkers(wordRange.get(), DocumentMarker::Spelling);
            if (RefPtr<Range> sentenceRange = newSelectedSentence.toRange())
                document()->removeMarkers(sentenceRange.get(), DocumentMarker::Grammar);
        }

        // When continuous spell checking is off, existing markers disappear after the selection changes.
        if (!isContinuousSpellCheckingEnabled)
            document()->removeMarkers(DocumentMarker::Spelling);
        if (!isContinuousGrammarCheckingEnabled)
            document()->removeMarkers(DocumentMarker::Grammar);
    }

    editor()->respondToChangedSelection(oldSelection);
}

VisiblePosition Frame::visiblePositionForPoint(const IntPoint& framePoint)
{
    HitTestResult result = eventHandler()->hitTestResultAtPoint(framePoint, true);
    Node* node = result.innerNode();
    if (!node)
        return VisiblePosition();
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();
    VisiblePosition visiblePos = renderer->positionForCoordinates(result.localPoint().x(), result.localPoint().y());
    if (visiblePos.isNull())
        visiblePos = VisiblePosition(Position(node, 0));
    return visiblePos;
}
    
Document* Frame::documentAtPoint(const IntPoint& point)
{  
    if (!view()) 
        return 0;
    
    IntPoint pt = view()->windowToContents(point);
    HitTestResult result = HitTestResult(pt);
    
    if (contentRenderer())
        result = eventHandler()->hitTestResultAtPoint(pt, false);
    return result.innerNode() ? result.innerNode()->document() : 0;
}
    
FramePrivate::FramePrivate(Page* page, Frame* parent, Frame* thisFrame, HTMLFrameOwnerElement* ownerElement,
                           FrameLoaderClient* frameLoaderClient)
    : m_page(page)
    , m_treeNode(thisFrame, parent)
    , m_loader(thisFrame, frameLoaderClient)
    , m_ownerElement(ownerElement)
    , m_script(thisFrame)
    , m_zoomFactor(parent ? parent->d->m_zoomFactor : 1.0f)
    , m_selectionGranularity(CharacterGranularity)
    , m_selectionController(thisFrame)
    , m_caretBlinkTimer(thisFrame, &Frame::caretBlinkTimerFired)
    , m_editor(thisFrame)
    , m_eventHandler(thisFrame)
    , m_animationController(thisFrame)
    , m_lifeSupportTimer(thisFrame, &Frame::lifeSupportTimerFired)
    , m_caretVisible(false)
    , m_caretPaint(true)
    , m_highlightTextMatches(false)
    , m_inViewSourceMode(false)
    , m_needsReapplyStyles(false)
    , m_isDisconnected(false)
    , m_excludeFromTextSearch(false)
#if FRAME_LOADS_USER_STYLESHEET
    , m_userStyleSheetLoader(0)
#endif
{
}

FramePrivate::~FramePrivate()
{
}

} // namespace WebCore
