/* This file is part of the KDE project
 *
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "Frame.h"
#include "FramePrivate.h"

#include "ApplyStyleCommand.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "Cache.h"
#include "CachedCSSStyleSheet.h"
#include "DOMWindow.h"
#include "DocLoader.h"
#include "DocumentType.h"
#include "EditingText.h"
#include "EditorClient.h"
#include "Event.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElement.h"
#include "HTMLGenericFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLViewSourceDocument.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "IconDatabase.h"
#include "IconLoader.h"
#include "ImageDocument.h"
#include "IndentOutdentCommand.h"
#include "MediaFeatureNames.h"
#include "MouseEventWithHitTestResults.h"
#include "NodeList.h"
#include "Page.h"
#include "PlatformScrollBar.h"
#include "PlugInInfoStore.h"
#include "Plugin.h"
#include "PluginDocument.h"
#include "RenderListBox.h"
#include "RenderObject.h"
#include "RenderPart.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "SegmentedString.h"
#include "TextDocument.h"
#include "TextIterator.h"
#include "TextResourceDecoder.h"
#include "TypingCommand.h"
#include "XMLTokenizer.h"
#include "cssstyleselector.h"
#include "htmlediting.h"
#include "kjs_proxy.h"
#include "kjs_window.h"
#include "markup.h"
#include "visible_units.h"
#include "xmlhttprequest.h"
#include <math.h>
#include <sys/types.h>
#include <wtf/Platform.h>

#if PLATFORM(MAC)
#include "FrameMac.h"
#endif

#if !PLATFORM(WIN_OS)
#include <unistd.h>
#endif

#ifdef SVG_SUPPORT
#include "SVGNames.h"
#include "XLinkNames.h"
#include "XMLNames.h"
#include "SVGDocument.h"
#include "SVGDocumentExtensions.h"
#endif

using namespace std;

using KJS::JSLock;
using KJS::JSValue;
using KJS::Location;
using KJS::PausedTimeouts;
using KJS::SavedProperties;
using KJS::SavedBuiltins;
using KJS::UString;
using KJS::Window;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

const double caretBlinkFrequency = 0.5;
const double autoscrollInterval = 0.1;

class UserStyleSheetLoader : public CachedResourceClient {
public:
    UserStyleSheetLoader(Frame* frame, const String& url, DocLoader* docLoader)
        : m_frame(frame)
        , m_cachedSheet(docLoader->requestCSSStyleSheet(url, ""))
    {
        m_cachedSheet->ref(this);
    }
    ~UserStyleSheetLoader()
    {
        m_cachedSheet->deref(this);
    }
private:
    virtual void setCSSStyleSheet(const String& /*URL*/, const String& /*charset*/, const String& sheet)
    {
        m_frame->setUserStyleSheet(sheet);
    }
    Frame* m_frame;
    CachedCSSStyleSheet* m_cachedSheet;
};

#ifndef NDEBUG
struct FrameCounter { 
    static int count; 
    ~FrameCounter() { if (count != 0) fprintf(stderr, "LEAK: %d Frame\n", count); }
};
int FrameCounter::count = 0;
static FrameCounter frameCounter;
#endif

static inline Frame* parentFromOwnerElement(Element* ownerElement)
{
    if (!ownerElement)
        return 0;
    return ownerElement->document()->frame();
}

Frame::Frame(Page* page, Element* ownerElement, PassRefPtr<EditorClient> client) 
    : d(new FramePrivate(page, parentFromOwnerElement(ownerElement), this, ownerElement, client))
{
    AtomicString::init();
    EventNames::init();
    HTMLNames::init();
    QualifiedName::init();
    MediaFeatureNames::init();

#ifdef SVG_SUPPORT
    SVGNames::init();
    XLinkNames::init();
    XMLNames::init();
#endif

    if (!ownerElement)
        page->setMainFrame(this);
    else {
        // FIXME: Frames were originally created with a refcount of 1.
        // Leave this ref call here until we can straighten that out.
        ref();
        page->incrementFrameCount();
    }

#ifndef NDEBUG
    ++FrameCounter::count;
#endif
}

Frame::~Frame()
{
    // FIXME: We should not be doing all this work inside the destructor

    ASSERT(!d->m_lifeSupportTimer.isActive());

#ifndef NDEBUG
    --FrameCounter::count;
#endif

    if (d->m_jscript && d->m_jscript->haveInterpreter())
        if (Window* w = Window::retrieveWindow(this)) {
            w->disconnectFrame();
            // Must clear the window pointer, otherwise we will not
            // garbage-collect collect the window (inside the call to
            // delete d below).
            w = 0;
        }

    disconnectOwnerElement();
    
    if (d->m_domWindow)
        d->m_domWindow->disconnectFrame();
            
    if (d->m_view) {
        d->m_view->hide();
        d->m_view->m_frame = 0;
    }
  
    ASSERT(!d->m_lifeSupportTimer.isActive());

    delete d->m_userStyleSheetLoader;
    delete d;
    d = 0;
}

FrameLoader* Frame::loader() const
{
    return d->m_loader;
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
    if (d->m_doc && view == 0)
        d->m_doc->detach();

    d->m_view = view;
}

bool Frame::javaScriptEnabled() const
{
    return d->m_bJScriptEnabled;
}

KJSProxy *Frame::scriptProxy()
{
    if (!d->m_bJScriptEnabled)
        return 0;

    if (!d->m_jscript)
        d->m_jscript = new KJSProxy(this);

    return d->m_jscript;
}

bool Frame::javaEnabled() const
{
    return d->m_settings->isJavaEnabled();
}

bool Frame::pluginsEnabled() const
{
    return d->m_bPluginsEnabled;
}

Document *Frame::document() const
{
    if (d)
        return d->m_doc.get();
    return 0;
}

void Frame::setDocument(Document* newDoc)
{
    if (d) {
        if (d->m_doc)
            d->m_doc->detach();
        d->m_doc = newDoc;
        if (newDoc)
            newDoc->attach();
    }
}

const Settings *Frame::settings() const
{
  return d->m_settings;
}

void Frame::setUserStyleSheetLocation(const KURL& url)
{
    delete d->m_userStyleSheetLoader;
    d->m_userStyleSheetLoader = 0;
    if (d->m_doc && d->m_doc->docLoader())
        d->m_userStyleSheetLoader = new UserStyleSheetLoader(this, url.url(), d->m_doc->docLoader());
}

void Frame::setUserStyleSheet(const String& styleSheet)
{
    delete d->m_userStyleSheetLoader;
    d->m_userStyleSheetLoader = 0;
    if (d->m_doc)
        d->m_doc->setUserStyleSheet(styleSheet);
}

void Frame::setStandardFont(const String& name)
{
    d->m_settings->setStdFontName(AtomicString(name));
}

void Frame::setFixedFont(const String& name)
{
    d->m_settings->setFixedFontName(AtomicString(name));
}

String Frame::selectedText() const
{
    return plainText(selectionController()->toRange().get());
}

SelectionController* Frame::selectionController() const
{
    return &d->m_selectionController;
}

Editor* Frame::editor() const
{
    return &d->m_editor;
}

CommandByName* Frame::command() const
{
    return &d->m_command;
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
    if (selectionController()->rootEditableElement())
        renderer = selectionController()->rootEditableElement()->shadowAncestorNode()->renderer();

    // If the current selection is in a textfield or textarea, notify the renderer that the selection has changed
    if (renderer && (renderer->isTextArea() || renderer->isTextField()))
        static_cast<RenderTextControl*>(renderer)->selectionChanged(userTriggered);
}

void Frame::invalidateSelection()
{
    selectionController()->setNeedsLayout();
    selectionLayoutChanged();
}

void Frame::setCaretVisible(bool flag)
{
    if (d->m_caretVisible == flag)
        return;
    clearCaretRectIfNeeded();
    if (flag)
        setFocusNodeIfNeeded();
    d->m_caretVisible = flag;
    selectionLayoutChanged();
}


void Frame::clearCaretRectIfNeeded()
{
    if (d->m_caretPaint) {
        d->m_caretPaint = false;
        selectionController()->invalidateCaretRect();
    }        
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

void Frame::setFocusNodeIfNeeded()
{
    if (!document() || selectionController()->isNone() || !d->m_isActive)
        return;

    Node* target = selectionController()->rootEditableElement();
    if (target) {
        RenderObject* renderer = target->renderer();

        // Walk up the render tree to search for a node to focus.
        // Walking up the DOM tree wouldn't work for shadow trees, like those behind the engine-based text fields.
        while (renderer) {
            // We don't want to set focus on a subframe when selecting in a parent frame,
            // so add the !isFrameElement check here. There's probably a better way to make this
            // work in the long term, but this is the safest fix at this time.
            if (target && target->isMouseFocusable() && !isFrameElement(target)) {
                document()->setFocusNode(target);
                return;
            }
            renderer = renderer->parent();
            if (renderer)
                target = renderer->element();
        }
        document()->setFocusNode(0);
    }
}

void Frame::selectionLayoutChanged()
{
    bool caretRectChanged = selectionController()->recomputeCaretRect();

    bool shouldBlink = d->m_caretVisible
        && selectionController()->isCaret() && selectionController()->isContentEditable();

    // If the caret moved, stop the blink timer so we can restart with a
    // black caret in the new location.
    if (caretRectChanged || !shouldBlink)
        d->m_caretBlinkTimer.stop();

    // Start blinking with a black caret. Be sure not to restart if we're
    // already blinking in the right location.
    if (shouldBlink && !d->m_caretBlinkTimer.isActive()) {
        d->m_caretBlinkTimer.startRepeating(caretBlinkFrequency);
        d->m_caretPaint = true;
    }

    if (d->m_doc)
        d->m_doc->updateSelection();
}

void Frame::setXPosForVerticalArrowNavigation(int x)
{
    d->m_xPosForVerticalArrowNavigation = x;
}

int Frame::xPosForVerticalArrowNavigation() const
{
    return d->m_xPosForVerticalArrowNavigation;
}

void Frame::caretBlinkTimerFired(Timer<Frame>*)
{
    ASSERT(d->m_caretVisible);
    ASSERT(selectionController()->isCaret());
    bool caretPaint = d->m_caretPaint;
    if (d->m_bMousePressed && caretPaint)
        return;
    d->m_caretPaint = !caretPaint;
    selectionController()->invalidateCaretRect();
}

void Frame::paintCaret(GraphicsContext* p, const IntRect& rect) const
{
    if (d->m_caretPaint && d->m_caretVisible)
        selectionController()->paintCaret(p, rect);
}

void Frame::paintDragCaret(GraphicsContext* p, const IntRect& rect) const
{
    SelectionController* dragCaretController = d->m_page->dragCaretController();
    assert(dragCaretController->selection().isCaret());
    if (dragCaretController->selection().start().node()->document()->frame() == this)
        dragCaretController->paintCaret(p, rect);
}

int Frame::zoomFactor() const
{
  return d->m_zoomFactor;
}

void Frame::setZoomFactor(int percent)
{  
  if (d->m_zoomFactor == percent)
      return;

  d->m_zoomFactor = percent;

  if (d->m_doc)
      d->m_doc->recalcStyle(Node::Force);

  for (Frame* child = tree()->firstChild(); child; child = child->tree()->nextSibling())
      child->setZoomFactor(d->m_zoomFactor);

  if (d->m_doc && d->m_doc->renderer() && d->m_doc->renderer()->needsLayout())
      view()->layout();
}

void Frame::setJSStatusBarText(const String& text)
{
    d->m_kjsStatusBarText = text;
    setStatusBarText(d->m_kjsStatusBarText);
}

void Frame::setJSDefaultStatusBarText(const String& text)
{
    d->m_kjsDefaultStatusBarText = text;
    setStatusBarText(d->m_kjsDefaultStatusBarText);
}

String Frame::jsStatusBarText() const
{
    return d->m_kjsStatusBarText;
}

String Frame::jsDefaultStatusBarText() const
{
   return d->m_kjsDefaultStatusBarText;
}

void Frame::reparseConfiguration()
{
    if (d->m_doc)
        d->m_doc->docLoader()->setAutoLoadImages(d->m_settings->autoLoadImages());
        
    d->m_bJScriptEnabled = d->m_settings->isJavaScriptEnabled();
    d->m_bJavaEnabled = d->m_settings->isJavaEnabled();
    d->m_bPluginsEnabled = d->m_settings->isPluginsEnabled();

    const KURL& userStyleSheetLocation = d->m_settings->userStyleSheetLocation();
    if (!userStyleSheetLocation.isEmpty())
        setUserStyleSheetLocation(userStyleSheetLocation);
    else
        setUserStyleSheet(String());

    // FIXME: It's not entirely clear why the following is needed.
    // The document automatically does this as required when you set the style sheet.
    // But we had problems when this code was removed. Details are in
    // <http://bugs.webkit.org/show_bug.cgi?id=8079>.
    if (d->m_doc)
        d->m_doc->updateStyleSelector();
}

bool Frame::shouldDragAutoNode(Node *node, const IntPoint& point) const
{
    return false;
}

void Frame::selectClosestWordFromMouseEvent(const PlatformMouseEvent& mouse, Node *innerNode)
{
    Selection newSelection;

    if (innerNode && innerNode->renderer() && mouseDownMayStartSelect() && innerNode->renderer()->shouldSelect()) {
        IntPoint vPoint = view()->windowToContents(mouse.pos());
        VisiblePosition pos(innerNode->renderer()->positionForPoint(vPoint));
        if (pos.isNotNull()) {
            newSelection = Selection(pos);
            newSelection.expandUsingGranularity(WordGranularity);
        }
    }
    
    if (newSelection.isRange()) {
        d->m_selectionGranularity = WordGranularity;
        d->m_beganSelectingText = true;
    }
    
    if (shouldChangeSelection(newSelection))
        selectionController()->setSelection(newSelection);
}

void Frame::handleMousePressEventDoubleClick(const MouseEventWithHitTestResults& event)
{
    if (event.event().button() == LeftButton) {
        if (selectionController()->isRange())
            // A double-click when range is already selected
            // should not change the selection.  So, do not call
            // selectClosestWordFromMouseEvent, but do set
            // m_beganSelectingText to prevent handleMouseReleaseEvent
            // from setting caret selection.
            d->m_beganSelectingText = true;
        else
            selectClosestWordFromMouseEvent(event.event(), event.targetNode());
    }
}

void Frame::handleMousePressEventTripleClick(const MouseEventWithHitTestResults& event)
{
    Node *innerNode = event.targetNode();
    
    if (event.event().button() == LeftButton && innerNode && innerNode->renderer() &&
        mouseDownMayStartSelect() && innerNode->renderer()->shouldSelect()) {
        Selection newSelection;
        IntPoint vPoint = view()->windowToContents(event.event().pos());
        VisiblePosition pos(innerNode->renderer()->positionForPoint(vPoint));
        if (pos.isNotNull()) {
            newSelection = Selection(pos);
            newSelection.expandUsingGranularity(ParagraphGranularity);
        }
        if (newSelection.isRange()) {
            d->m_selectionGranularity = ParagraphGranularity;
            d->m_beganSelectingText = true;
        }
        
        if (shouldChangeSelection(newSelection))
            selectionController()->setSelection(newSelection);
    }
}

void Frame::handleMousePressEventSingleClick(const MouseEventWithHitTestResults& event)
{
    Node *innerNode = event.targetNode();
    
    if (event.event().button() == LeftButton) {
        if (innerNode && innerNode->renderer() &&
            mouseDownMayStartSelect() && innerNode->renderer()->shouldSelect()) {
            
            // Extend the selection if the Shift key is down, unless the click is in a link.
            bool extendSelection = event.event().shiftKey() && !event.isOverLink();

            // Don't restart the selection when the mouse is pressed on an
            // existing selection so we can allow for text dragging.
            IntPoint vPoint = view()->windowToContents(event.event().pos());
            if (!extendSelection && selectionController()->contains(vPoint))
                return;

            VisiblePosition visiblePos(innerNode->renderer()->positionForPoint(vPoint));
            if (visiblePos.isNull())
                visiblePos = VisiblePosition(innerNode, innerNode->caretMinOffset(), DOWNSTREAM);
            Position pos = visiblePos.deepEquivalent();
            
            Selection newSelection = selectionController()->selection();
            if (extendSelection && newSelection.isCaretOrRange()) {
                selectionController()->clearModifyBias();
                
                // See <rdar://problem/3668157> REGRESSION (Mail): shift-click deselects when selection 
                // was created right-to-left
                Position start = newSelection.start();
                Position end = newSelection.end();
                short before = Range::compareBoundaryPoints(pos.node(), pos.offset(), start.node(), start.offset());
                if (before <= 0)
                    newSelection = Selection(pos, end);
                else
                    newSelection = Selection(start, pos);

                if (d->m_selectionGranularity != CharacterGranularity)
                    newSelection.expandUsingGranularity(d->m_selectionGranularity);
                d->m_beganSelectingText = true;
            } else {
                newSelection = Selection(visiblePos);
                d->m_selectionGranularity = CharacterGranularity;
            }
            
            if (shouldChangeSelection(newSelection))
                selectionController()->setSelection(newSelection);
        }
    }
}

void Frame::handleMousePressEvent(const MouseEventWithHitTestResults& event)
{
    Node *innerNode = event.targetNode();

    d->m_mousePressNode = innerNode;
    d->m_dragStartPos = event.event().pos();

    if (event.event().button() == LeftButton || event.event().button() == MiddleButton) {
        d->m_bMousePressed = true;
        d->m_beganSelectingText = false;

        if (event.event().clickCount() == 2) {
            handleMousePressEventDoubleClick(event);
            return;
        }
        if (event.event().clickCount() >= 3) {
            handleMousePressEventTripleClick(event);
            return;
        }
        handleMousePressEventSingleClick(event);
    }
    
   setMouseDownMayStartAutoscroll(mouseDownMayStartSelect() || 
        (d->m_mousePressNode && d->m_mousePressNode->renderer() && d->m_mousePressNode->renderer()->shouldAutoscroll()));
}

void Frame::handleMouseMoveEvent(const MouseEventWithHitTestResults& event)
{
    // Mouse not pressed. Do nothing.
    if (!d->m_bMousePressed)
        return;

    Node *innerNode = event.targetNode();

    if (event.event().button() != 0 || !innerNode || !innerNode->renderer())
        return;

    ASSERT(mouseDownMayStartSelect() || mouseDownMayStartAutoscroll());

    setMouseDownMayStartDrag(false);
    view()->invalidateClick();

     if (mouseDownMayStartAutoscroll()) {            
        // If the selection is contained in a layer that can scroll, that layer should handle the autoscroll
        // Otherwise, let the bridge handle it so the view can scroll itself.
        RenderObject* renderer = innerNode->renderer();
        while (renderer && !renderer->shouldAutoscroll())
            renderer = renderer->parent();
        if (renderer)
            handleAutoscroll(renderer);
    }
    
    if (mouseDownMayStartSelect() && innerNode->renderer()->shouldSelect()) {
        // handle making selection
        IntPoint vPoint = view()->windowToContents(event.event().pos());        
        VisiblePosition pos(innerNode->renderer()->positionForPoint(vPoint));

        updateSelectionForMouseDragOverPosition(pos);
    }

}

void Frame::updateSelectionForMouseDragOverPosition(const VisiblePosition& pos)
{
    // Don't modify the selection if we're not on a node.
    if (pos.isNull())
        return;

    // Restart the selection if this is the first mouse move. This work is usually
    // done in handleMousePressEvent, but not if the mouse press was on an existing selection.
    Selection newSelection = selectionController()->selection();
    selectionController()->clearModifyBias();
    
    if (!d->m_beganSelectingText) {
        d->m_beganSelectingText = true;
        newSelection = Selection(pos);
    }

    newSelection.setExtent(pos);
    if (d->m_selectionGranularity != CharacterGranularity)
        newSelection.expandUsingGranularity(d->m_selectionGranularity);

    if (shouldChangeSelection(newSelection))
        selectionController()->setSelection(newSelection);
}

void Frame::handleMouseReleaseEvent(const MouseEventWithHitTestResults& event)
{
    stopAutoscrollTimer();
    
    // Used to prevent mouseMoveEvent from initiating a drag before
    // the mouse is pressed again.
    d->m_bMousePressed = false;
  
    // Clear the selection if the mouse didn't move after the last mouse press.
    // We do this so when clicking on the selection, the selection goes away.
    // However, if we are editing, place the caret.
    if (mouseDownMayStartSelect() && !d->m_beganSelectingText
            && d->m_dragStartPos == event.event().pos()
            && selectionController()->isRange()) {
        Selection newSelection;
        Node *node = event.targetNode();
        if (node && node->isContentEditable() && node->renderer()) {
            IntPoint vPoint = view()->windowToContents(event.event().pos());
            VisiblePosition pos = node->renderer()->positionForPoint(vPoint);
            newSelection = Selection(pos);
        }
        if (shouldChangeSelection(newSelection))
            selectionController()->setSelection(newSelection);
    }

    notifyRendererOfSelectionChange(true);

    selectionController()->selectFrameElementInParentIfFullySelected();
}

bool Frame::shouldChangeSelection(const Selection& newSelection) const
{
    return shouldChangeSelection(selectionController()->selection(), newSelection, newSelection.affinity(), false);
}

bool Frame::shouldDeleteSelection(const Selection& newSelection) const
{
    return true;
}

bool Frame::shouldBeginEditing(const Range *range) const
{
    return true;
}

bool Frame::shouldEndEditing(const Range *range) const
{
    return true;
}

bool Frame::isContentEditable() const 
{
    if (!d->m_doc)
        return false;
    return d->m_doc->inDesignMode();
}

void Frame::textFieldDidBeginEditing(Element* input)
{
}

void Frame::textFieldDidEndEditing(Element* input)
{
}

void Frame::textDidChangeInTextField(Element* input)
{
}

bool Frame::doTextFieldCommandFromEvent(Element* input, const PlatformKeyboardEvent* evt)
{
    return false;
}

void Frame::textWillBeDeletedInTextField(Element* input)
{
}

void Frame::textDidChangeInTextArea(Element* input)
{
}

static void dispatchEditableContentChangedEvents(const EditCommand& command)
{
     Element* startRoot = command.startingRootEditableElement();
     Element* endRoot = command.endingRootEditableElement();
     ExceptionCode ec;
     if (startRoot)
         startRoot->dispatchEvent(new Event(khtmlEditableContentChangedEvent, false, false), ec, true);
     if (endRoot && endRoot != startRoot)
         endRoot->dispatchEvent(new Event(khtmlEditableContentChangedEvent, false, false), ec, true);
}

void Frame::appliedEditing(PassRefPtr<EditCommand> cmd)
{
    dispatchEditableContentChangedEvents(*cmd);
 
    Selection newSelection(cmd->endingSelection());
    if (shouldChangeSelection(newSelection))
        selectionController()->setSelection(newSelection, false);
    
    // Now set the typing style from the command. Clear it when done.
    // This helps make the case work where you completely delete a piece
    // of styled text and then type a character immediately after.
    // That new character needs to take on the style of the just-deleted text.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (cmd->typingStyle()) {
        setTypingStyle(cmd->typingStyle());
        cmd->setTypingStyle(0);
    }

    // Command will be equal to last edit command only in the case of typing
    if (editor()->lastEditCommand() == cmd)
        assert(cmd->isTypingCommand());
    else {
        // Only register a new undo command if the command passed in is
        // different from the last command
        editor()->setLastEditCommand(cmd.get());
        registerCommandForUndo(cmd);
    }
    respondToChangedContents(newSelection);
    editor()->respondToChangedContents();
}

void Frame::unappliedEditing(PassRefPtr<EditCommand> cmd)
{
    dispatchEditableContentChangedEvents(*cmd);

    Selection newSelection(cmd->startingSelection());
    if (shouldChangeSelection(newSelection))
        selectionController()->setSelection(newSelection, true);
        
    editor()->setLastEditCommand(0);
    registerCommandForRedo(cmd);
    respondToChangedContents(newSelection);
    editor()->respondToChangedContents();
}

void Frame::reappliedEditing(PassRefPtr<EditCommand> cmd)
{
    dispatchEditableContentChangedEvents(*cmd);

    Selection newSelection(cmd->endingSelection());
    if (shouldChangeSelection(newSelection))
        selectionController()->setSelection(newSelection, true);
        
    editor()->setLastEditCommand(0);
    registerCommandForUndo(cmd);
    respondToChangedContents(newSelection);
    editor()->respondToChangedContents();
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

bool Frame::tabsToLinks() const
{
    return true;
}

bool Frame::tabsToAllControls() const
{
    return true;
}

void Frame::copyToPasteboard()
{
    issueCopyCommand();
}

void Frame::cutToPasteboard()
{
    issueCutCommand();
}

void Frame::pasteFromPasteboard()
{
    issuePasteCommand();
}

void Frame::pasteAndMatchStyle()
{
    issuePasteAndMatchStyleCommand();
}

void Frame::transpose()
{
    issueTransposeCommand();
}

void Frame::redo()
{
    issueRedoCommand();
}

void Frame::undo()
{
    issueUndoCommand();
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

    Node *node = selectionController()->selection().visibleStart().deepEquivalent().node();
    CSSComputedStyleDeclaration computedStyle(node);
    computedStyle.diff(mutableStyle.get());
    
    // Handle block styles, substracting these from the typing style.
    RefPtr<CSSMutableStyleDeclaration> blockStyle = mutableStyle->copyBlockProperties();
    blockStyle->diff(mutableStyle.get());
    if (document() && blockStyle->length() > 0)
        applyCommand(new ApplyStyleCommand(document(), blockStyle.get(), editingAction));
    
    // Set the remaining style as the typing style.
    d->m_typingStyle = mutableStyle.release();
}

void Frame::applyStyle(CSSStyleDeclaration *style, EditAction editingAction)
{
    switch (selectionController()->state()) {
        case Selection::NONE:
            // do nothing
            break;
        case Selection::CARET: {
            computeAndSetTypingStyle(style, editingAction);
            break;
        }
        case Selection::RANGE:
            if (document() && style)
                applyCommand(new ApplyStyleCommand(document(), style, editingAction));
            break;
    }
}

void Frame::applyParagraphStyle(CSSStyleDeclaration *style, EditAction editingAction)
{
    switch (selectionController()->state()) {
        case Selection::NONE:
            // do nothing
            break;
        case Selection::CARET:
        case Selection::RANGE:
            if (document() && style)
                applyCommand(new ApplyStyleCommand(document(), style, editingAction, ApplyStyleCommand::ForceBlockProperties));
            break;
    }
}

void Frame::indent()
{
    applyCommand(new IndentOutdentCommand(document(), IndentOutdentCommand::Indent));
}

void Frame::outdent()
{
    applyCommand(new IndentOutdentCommand(document(), IndentOutdentCommand::Outdent));
}

static void updateState(CSSMutableStyleDeclaration *desiredStyle, CSSComputedStyleDeclaration *computedStyle, bool& atStart, Frame::TriState& state)
{
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = desiredStyle->valuesIterator(); it != end; ++it) {
        int propertyID = (*it).id();
        String desiredProperty = desiredStyle->getPropertyValue(propertyID);
        String computedProperty = computedStyle->getPropertyValue(propertyID);
        Frame::TriState propertyState = equalIgnoringCase(desiredProperty, computedProperty)
            ? Frame::trueTriState : Frame::falseTriState;
        if (atStart) {
            state = propertyState;
            atStart = false;
        } else if (state != propertyState) {
            state = Frame::mixedTriState;
            break;
        }
    }
}

Frame::TriState Frame::selectionHasStyle(CSSStyleDeclaration *style) const
{
    bool atStart = true;
    TriState state = falseTriState;

    RefPtr<CSSMutableStyleDeclaration> mutableStyle = style->makeMutable();

    if (!selectionController()->isRange()) {
        Node* nodeToRemove;
        RefPtr<CSSComputedStyleDeclaration> selectionStyle = selectionComputedStyle(nodeToRemove);
        if (!selectionStyle)
            return falseTriState;
        updateState(mutableStyle.get(), selectionStyle.get(), atStart, state);
        if (nodeToRemove) {
            ExceptionCode ec = 0;
            nodeToRemove->remove(ec);
            assert(ec == 0);
        }
    } else {
        for (Node* node = selectionController()->start().node(); node; node = node->traverseNextNode()) {
            RefPtr<CSSComputedStyleDeclaration> computedStyle = new CSSComputedStyleDeclaration(node);
            if (computedStyle)
                updateState(mutableStyle.get(), computedStyle.get(), atStart, state);
            if (state == mixedTriState)
                break;
            if (node == selectionController()->end().node())
                break;
        }
    }

    return state;
}

bool Frame::selectionStartHasStyle(CSSStyleDeclaration *style) const
{
    Node* nodeToRemove;
    RefPtr<CSSStyleDeclaration> selectionStyle = selectionComputedStyle(nodeToRemove);
    if (!selectionStyle)
        return false;

    RefPtr<CSSMutableStyleDeclaration> mutableStyle = style->makeMutable();

    bool match = true;
    DeprecatedValueListConstIterator<CSSProperty> end;
    for (DeprecatedValueListConstIterator<CSSProperty> it = mutableStyle->valuesIterator(); it != end; ++it) {
        int propertyID = (*it).id();
        if (!equalIgnoringCase(mutableStyle->getPropertyValue(propertyID), selectionStyle->getPropertyValue(propertyID))) {
            match = false;
            break;
        }
    }

    if (nodeToRemove) {
        ExceptionCode ec = 0;
        nodeToRemove->remove(ec);
        assert(ec == 0);
    }

    return match;
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
        assert(ec == 0);
    }

    return value;
}

CSSComputedStyleDeclaration *Frame::selectionComputedStyle(Node *&nodeToRemove) const
{
    nodeToRemove = 0;

    if (!document())
        return 0;

    if (selectionController()->isNone())
        return 0;

    RefPtr<Range> range(selectionController()->toRange());
    Position pos = range->editingStartPosition();

    Element *elem = pos.element();
    if (!elem)
        return 0;
    
    RefPtr<Element> styleElement = elem;
    ExceptionCode ec = 0;

    if (d->m_typingStyle) {
        styleElement = document()->createElementNS(xhtmlNamespaceURI, "span", ec);
        assert(ec == 0);

        styleElement->setAttribute(styleAttr, d->m_typingStyle->cssText().impl(), ec);
        assert(ec == 0);
        
        styleElement->appendChild(document()->createEditingTextNode(""), ec);
        assert(ec == 0);

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
        assert(ec == 0);

        nodeToRemove = styleElement.get();
    }

    return new CSSComputedStyleDeclaration(styleElement);
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
    style->setProperty(CSS_PROP_WORD_WRAP, "break-word", false, ec);
    ASSERT(ec == 0);
    style->setProperty(CSS_PROP__WEBKIT_NBSP_MODE, "space", false, ec);
    ASSERT(ec == 0);
    style->setProperty(CSS_PROP__WEBKIT_LINE_BREAK, "after-white-space", false, ec);
    ASSERT(ec == 0);
}

void Frame::removeEditingStyleFromElement(Element*) const
{
}

bool Frame::isCharacterSmartReplaceExempt(UChar, bool)
{
    // no smart replace
    return true;
}

#ifndef NDEBUG
static HashSet<Frame*> lifeSupportSet;
#endif

void Frame::endAllLifeSupport()
{
#ifndef NDEBUG
    HashSet<Frame*> lifeSupportCopy = lifeSupportSet;
    HashSet<Frame*>::iterator end = lifeSupportCopy.end();
    for (HashSet<Frame*>::iterator it = lifeSupportCopy.begin(); it != end; ++it)
        (*it)->endLifeSupport();
#endif
}

void Frame::keepAlive()
{
    if (d->m_lifeSupportTimer.isActive())
        return;
    ref();
#ifndef NDEBUG
    lifeSupportSet.add(this);
#endif
    d->m_lifeSupportTimer.startOneShot(0);
}

void Frame::endLifeSupport()
{
    if (!d->m_lifeSupportTimer.isActive())
        return;
    d->m_lifeSupportTimer.stop();
#ifndef NDEBUG
    lifeSupportSet.remove(this);
#endif
    deref();
}

void Frame::lifeSupportTimerFired(Timer<Frame>*)
{
#ifndef NDEBUG
    lifeSupportSet.remove(this);
#endif
    deref();
}

bool Frame::mouseDownMayStartAutoscroll() const
{
    return d->m_mouseDownMayStartAutoscroll;
}

void Frame::setMouseDownMayStartAutoscroll(bool b)
{
    d->m_mouseDownMayStartAutoscroll = b;
}

bool Frame::mouseDownMayStartDrag() const
{
    return d->m_mouseDownMayStartDrag;
}

void Frame::setMouseDownMayStartDrag(bool b)
{
    d->m_mouseDownMayStartDrag = b;
}

void Frame::setSettings(Settings *settings)
{
    d->m_settings = settings;
}

RenderObject *Frame::renderer() const
{
    Document *doc = document();
    return doc ? doc->renderer() : 0;
}

Element* Frame::ownerElement()
{
    return d->m_ownerElement;
}

RenderPart* Frame::ownerRenderer()
{
    Element* ownerElement = d->m_ownerElement;
    if (!ownerElement)
        return 0;
    return static_cast<RenderPart*>(ownerElement->renderer());
}

IntRect Frame::selectionRect() const
{
    RenderView *root = static_cast<RenderView*>(renderer());
    if (!root)
        return IntRect();

    return root->selectionRect();
}

// returns FloatRect because going through IntRect would truncate any floats
FloatRect Frame::visibleSelectionRect() const
{
    if (!d->m_view)
        return FloatRect();
    
    return intersection(selectionRect(), d->m_view->visibleContentRect());
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
            return static_cast<HTMLGenericFormElement*>(n)->form();
        else if (n->hasTagName(frameTag) || n->hasTagName(iframeTag)) {
            Node *childDoc = static_cast<HTMLFrameElement*>(n)->contentDocument();
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
    Node *start = d->m_doc ? d->m_doc->focusNode() : 0;
    if (!start)
        start = selectionController()->start().node();
    
    // try walking up the node tree to find a form element
    Node *n;
    for (n = start; n; n = n->parentNode()) {
        if (n->hasTagName(formTag))
            return static_cast<HTMLFormElement*>(n);
        else if (n->isHTMLElement()
                   && static_cast<HTMLElement*>(n)->isGenericFormElement())
            return static_cast<HTMLGenericFormElement*>(n)->form();
    }
    
    // try walking forward in the node tree to find a form element
    return start ? scanForForm(start) : 0;
}

// FIXME: should this go in SelectionController?
void Frame::revealSelection(const RenderLayer::ScrollAlignment& alignment) const
{
    IntRect rect;
    
    switch (selectionController()->state()) {
        case Selection::NONE:
            return;
            
        case Selection::CARET:
            rect = selectionController()->caretRect();
            break;
            
        case Selection::RANGE:
            rect = selectionRect();
            break;
    }

    Position start = selectionController()->start();
    Position end = selectionController()->end();

    ASSERT(start.node());
    if (start.node() && start.node()->renderer()) {
        RenderLayer *layer = start.node()->renderer()->enclosingLayer();
        if (layer) {
            ASSERT(!end.node() || !end.node()->renderer() 
                   || (end.node()->renderer()->enclosingLayer() == layer));
            layer->scrollRectToVisible(rect, alignment, alignment);
        }
    }
}

void Frame::revealCaret(const RenderLayer::ScrollAlignment& alignment) const
{
    if (selectionController()->isNone())
        return;

    Position extent = selectionController()->extent();
    if (extent.node() && extent.node()->renderer()) {
        IntRect extentRect = VisiblePosition(extent).caretRect();
        RenderLayer* layer = extent.node()->renderer()->enclosingLayer();
        if (layer)
            layer->scrollRectToVisible(extentRect, alignment, alignment);
    }
}

// FIXME: should this be here?
bool Frame::scrollOverflow(ScrollDirection direction, ScrollGranularity granularity)
{
    if (!document()) {
        return false;
    }
    
    Node *node = document()->focusNode();
    if (node == 0) {
        node = d->m_mousePressNode.get();
    }
    
    if (node != 0) {
        RenderObject *r = node->renderer();
        if (r != 0 && !r->isListBox()) {
            return r->scroll(direction, granularity);
        }
    }
    
    return false;
}

void Frame::handleAutoscroll(RenderObject* renderer)
{
    if (d->m_autoscrollTimer.isActive())
        return;
    setAutoscrollRenderer(renderer);
    startAutoscrollTimer();
}

void Frame::autoscrollTimerFired(Timer<Frame>*)
{
    if (!d->m_bMousePressed){
        stopAutoscrollTimer();
        return;
    }
    if (RenderObject* r = autoscrollRenderer())
        r->autoscroll();
}

RenderObject* Frame::autoscrollRenderer() const
{
    return d->m_autoscrollRenderer;
}

void Frame::setAutoscrollRenderer(RenderObject* renderer)
{
    d->m_autoscrollRenderer = renderer;
}

HitTestResult Frame::hitTestResultAtPoint(const IntPoint& point, bool allowShadowContent)
{
    HitTestResult result(point);
    if (!renderer())
        return result;
    renderer()->layer()->hitTest(HitTestRequest(true, true), result);

    IntPoint widgetPoint(point);
    while (true) {
        Node* n = result.innerNode();
        if (!n || !n->renderer() || !n->renderer()->isWidget())
            break;
        Widget* widget = static_cast<RenderWidget*>(n->renderer())->widget();
        if (!widget || !widget->isFrameView())
            break;
        Frame* frame = static_cast<HTMLFrameElement*>(n)->contentFrame();
        if (!frame || !frame->renderer())
            break;
        int absX, absY;
        n->renderer()->absolutePosition(absX, absY, true);
        FrameView* view = static_cast<FrameView*>(widget);
        widgetPoint.move(view->contentsX() - absX, view->contentsY() - absY);
        HitTestResult widgetHitTestResult(widgetPoint);
        frame->renderer()->layer()->hitTest(HitTestRequest(true, true), widgetHitTestResult);
        result = widgetHitTestResult;
    }

    if (!allowShadowContent) {
        Node* node = result.innerNode();
        if (node)
            node = node->shadowAncestorNode();
        result.setInnerNode(node);
        node = result.innerNonSharedNode();
        if (node)
            node = node->shadowAncestorNode();
        result.setInnerNonSharedNode(node); 
    }

    return result;
}


void Frame::startAutoscrollTimer()
{
    d->m_autoscrollTimer.startRepeating(autoscrollInterval);
}

void Frame::stopAutoscrollTimer(bool rendererIsBeingDestroyed)
{
    if (!rendererIsBeingDestroyed && autoscrollRenderer())
        autoscrollRenderer()->stopAutoscroll();
    setAutoscrollRenderer(0);
    d->m_autoscrollTimer.stop();
}

// FIXME: why is this here instead of on the FrameView?
void Frame::paint(GraphicsContext* p, const IntRect& rect)
{
#ifndef NDEBUG
    bool fillWithRed;
    if (!document() || document()->printing())
        fillWithRed = false; // Printing, don't fill with red (can't remember why).
    else if (document()->ownerElement())
        fillWithRed = false; // Subframe, don't fill with red.
    else if (view() && view()->isTransparent())
        fillWithRed = false; // Transparent, don't fill with red.
    else if (d->m_paintRestriction == PaintRestrictionSelectionOnly || d->m_paintRestriction == PaintRestrictionSelectionOnlyWhiteText)
        fillWithRed = false; // Selections are transparent, don't fill with red.
    else if (d->m_elementToDraw)
        fillWithRed = false; // Element images are transparent, don't fill with red.
    else
        fillWithRed = true;
    
    if (fillWithRed)
        p->fillRect(rect, Color(0xFF, 0, 0));
#endif
    
    if (renderer()) {
        // d->m_elementToDraw is used to draw only one element
        RenderObject *eltRenderer = d->m_elementToDraw ? d->m_elementToDraw->renderer() : 0;
        if (d->m_paintRestriction == PaintRestrictionNone)
            renderer()->document()->invalidateRenderedRectsForMarkersInRect(rect);
        renderer()->layer()->paint(p, rect, d->m_paintRestriction, eltRenderer);

#if PLATFORM(MAC)
        // Regions may have changed as a result of the visibility/z-index of element changing.
        if (renderer()->document()->dashboardRegionsDirty())
            renderer()->view()->frameView()->updateDashboardRegions();
#endif
    } else
        LOG_ERROR("called Frame::paint with nil renderer");
}

#if PLATFORM(CG)

void Frame::adjustPageHeight(float *newBottom, float oldTop, float oldBottom, float bottomLimit)
{
    RenderView *root = static_cast<RenderView*>(document()->renderer());
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

#endif

Frame *Frame::frameForWidget(const Widget *widget)
{
    ASSERT_ARG(widget, widget);
    
    Node *node = nodeForWidget(widget);
    if (node)
        return frameForNode(node);
    
    // Assume all widgets are either form controls, or FrameViews.
    ASSERT(widget->isFrameView());
    return static_cast<const FrameView*>(widget)->frame();
}

Frame *Frame::frameForNode(Node *node)
{
    ASSERT_ARG(node, node);
    return node->document()->frame();
}

Node* Frame::nodeForWidget(const Widget* widget)
{
    ASSERT_ARG(widget, widget);
    WidgetClient* client = widget->client();
    if (!client)
        return 0;
    return client->element(const_cast<Widget*>(widget));
}

void Frame::clearDocumentFocus(Widget *widget)
{
    Node *node = nodeForWidget(widget);
    ASSERT(node);
    node->document()->setFocusNode(0);
}

void Frame::forceLayout()
{
    FrameView *v = d->m_view.get();
    if (v) {
        v->layout(false);
        // We cannot unschedule a pending relayout, since the force can be called with
        // a tiny rectangle from a drawRect update.  By unscheduling we in effect
        // "validate" and stop the necessary full repaint from occurring.  Basically any basic
        // append/remove DHTML is broken by this call.  For now, I have removed the optimization
        // until we have a better invalidation stategy. -dwh
        //v->unscheduleRelayout();
    }
}

void Frame::forceLayoutWithPageWidthRange(float minPageWidth, float maxPageWidth)
{
    // Dumping externalRepresentation(m_frame->renderer()).ascii() is a good trick to see
    // the state of things before and after the layout
    RenderView *root = static_cast<RenderView*>(document()->renderer());
    if (root) {
        // This magic is basically copied from khtmlview::print
        int pageW = (int)ceilf(minPageWidth);
        root->setWidth(pageW);
        root->setNeedsLayoutAndMinMaxRecalc();
        forceLayout();
        
        // If we don't fit in the minimum page width, we'll lay out again. If we don't fit in the
        // maximum page width, we will lay out to the maximum page width and clip extra content.
        // FIXME: We are assuming a shrink-to-fit printing implementation.  A cropping
        // implementation should not do this!
        int rightmostPos = root->rightmostPosition();
        if (rightmostPos > minPageWidth) {
            pageW = min(rightmostPos, (int)ceilf(maxPageWidth));
            root->setWidth(pageW);
            root->setNeedsLayoutAndMinMaxRecalc();
            forceLayout();
        }
    }
}

void Frame::sendResizeEvent()
{
    if (Document* doc = document())
        doc->dispatchWindowEvent(EventNames::resizeEvent, false, false);
}

void Frame::sendScrollEvent()
{
    FrameView *v = d->m_view.get();
    if (v) {
        Document *doc = document();
        if (!doc)
            return;
        doc->dispatchHTMLEvent(scrollEvent, true, false);
    }
}

bool Frame::canMouseDownStartSelect(Node* node)
{
    if (!node || !node->renderer())
        return true;
    
    // Check to see if -webkit-user-select has been set to none
    if (!node->renderer()->canSelect())
        return false;
    
    // Some controls and images can't start a select on a mouse down.
    for (RenderObject* curr = node->renderer(); curr; curr = curr->parent()) {
        if (curr->style()->userSelect() == SELECT_IGNORE)
            return false;
    }
    
    return true;
}

void Frame::clearTimers(FrameView *view)
{
    if (view) {
        view->unscheduleRelayout();
        if (view->frame()) {
            Document* document = view->frame()->document();
            if (document && document->renderer() && document->renderer()->layer())
                document->renderer()->layer()->suspendMarquees();
        }
    }
}

void Frame::clearTimers()
{
    clearTimers(d->m_view.get());
}

RenderStyle *Frame::styleForSelectionStart(Node *&nodeToRemove) const
{
    nodeToRemove = 0;
    
    if (!document())
        return 0;
    if (selectionController()->isNone())
        return 0;
    
    Position pos = selectionController()->selection().visibleStart().deepEquivalent();
    if (!pos.inRenderedContent())
        return 0;
    Node *node = pos.node();
    if (!node)
        return 0;
    
    if (!d->m_typingStyle)
        return node->renderer()->style();
    
    ExceptionCode ec = 0;
    RefPtr<Element> styleElement = document()->createElementNS(xhtmlNamespaceURI, "span", ec);
    ASSERT(ec == 0);
    
    styleElement->setAttribute(styleAttr, d->m_typingStyle->cssText().impl(), ec);
    ASSERT(ec == 0);
    
    styleElement->appendChild(document()->createEditingTextNode(""), ec);
    ASSERT(ec == 0);
    
    node->parentNode()->appendChild(styleElement, ec);
    ASSERT(ec == 0);
    
    nodeToRemove = styleElement.get();    
    return styleElement->renderer()->style();
}

void Frame::setSelectionFromNone()
{
    // Put a caret inside the body if the entire frame is editable (either the 
    // entire WebView is editable or designMode is on for this document).
    Document *doc = document();
    if (!doc || !selectionController()->isNone() || !isContentEditable())
        return;
        
    Node* node = doc->documentElement();
    while (node && !node->hasTagName(bodyTag))
        node = node->traverseNextNode();
    if (node)
        selectionController()->setSelection(Selection(Position(node, 0), DOWNSTREAM));
}

bool Frame::isActive() const
{
    return d->m_isActive;
}

void Frame::setIsActive(bool flag)
{
    if (d->m_isActive == flag)
        return;
    
    d->m_isActive = flag;

    // This method does the job of updating the view based on whether the view is "active".
    // This involves three kinds of drawing updates:

    // 1. The background color used to draw behind selected content (active | inactive color)
    if (d->m_view)
        d->m_view->updateContents(enclosingIntRect(visibleSelectionRect()));

    // 2. Caret blinking (blinks | does not blink)
    if (flag)
        setSelectionFromNone();
    setCaretVisible(flag);
    
    // 3. The drawing of a focus ring around links in web pages.
    Document *doc = document();
    if (doc) {
        Node *node = doc->focusNode();
        if (node) {
            node->setChanged();
            if (node->renderer() && node->renderer()->style()->hasAppearance())
                theme()->stateChanged(node->renderer(), FocusState);
        }
    }
    
    // 4. Changing the tint of controls from clear to aqua/graphite and vice versa.  We
    // do a "fake" paint.  When the theme gets a paint call, it can then do an invalidate.  This is only
    // done if the theme supports control tinting.
    if (doc && d->m_view && theme()->supportsControlTints() && renderer()) {
        doc->updateLayout(); // Ensure layout is up to date.
        IntRect visibleRect(enclosingIntRect(d->m_view->visibleContentRect()));
        GraphicsContext context((PlatformGraphicsContext*)0);
        context.setUpdatingControlTints(true);
        paint(&context, visibleRect);
    }
   
    // 5. Enable or disable secure keyboard entry
    if ((flag && !isSecureKeyboardEntry() && doc && doc->focusNode() && doc->focusNode()->hasTagName(inputTag) && 
            static_cast<HTMLInputElement*>(doc->focusNode())->inputType() == HTMLInputElement::PASSWORD) ||
        (!flag && isSecureKeyboardEntry()))
            setSecureKeyboardEntry(flag);
}

void Frame::setWindowHasFocus(bool flag)
{
    if (d->m_windowHasFocus == flag)
        return;
    d->m_windowHasFocus = flag;
    
    if (Document *doc = document())
        doc->dispatchWindowEvent(flag ? focusEvent : blurEvent, false, false);
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

bool Frame::markedTextUsesUnderlines() const
{
    return d->m_markedTextUsesUnderlines;
}

const Vector<MarkedTextUnderline>& Frame::markedTextUnderlines() const
{
    return d->m_markedTextUnderlines;
}

// Searches from the beginning of the document if nothing is selected.
bool Frame::findString(const String& target, bool forward, bool caseFlag, bool wrapFlag)
{
    if (target.isEmpty())
        return false;
    
    // Initially search from the start (if forward) or end (if backward) of the selection, and search to edge of document.
    RefPtr<Range> searchRange(rangeOfContents(document()));
    Selection selection(selectionController()->selection());
    if (!selection.isNone()) {
        if (forward)
            setStart(searchRange.get(), selection.visibleStart());
        else
            setEnd(searchRange.get(), selection.visibleEnd());
    }
    RefPtr<Range> resultRange(findPlainText(searchRange.get(), target, forward, caseFlag));
    // If the found range is already selected, find again.
    // Build a selection with the found range to remove collapsed whitespace.
    // Compare ranges instead of selection objects to ignore the way that the current selection was made.
    if (!selection.isNone() && *Selection(resultRange.get()).toRange() == *selection.toRange()) {
        searchRange = rangeOfContents(document());
        if (forward)
            setStart(searchRange.get(), selection.visibleEnd());
        else
            setEnd(searchRange.get(), selection.visibleStart());
        resultRange = findPlainText(searchRange.get(), target, forward, caseFlag);
    }
    
    int exception = 0;
    
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

    selectionController()->setSelection(Selection(resultRange.get(), DOWNSTREAM));
    revealSelection();
    return true;
}

unsigned Frame::markAllMatchesForText(const String& target, bool caseFlag, unsigned limit)
{
    if (target.isEmpty())
        return 0;
    
    RefPtr<Range> searchRange(rangeOfContents(document()));
    
    int exception = 0;
    unsigned matchCount = 0;
    do {
        RefPtr<Range> resultRange(findPlainText(searchRange.get(), target, true, caseFlag));
        if (resultRange->collapsed(exception))
            break;
        
        // A non-collapsed result range can in some funky whitespace cases still not
        // advance the range's start position (4509328). Break to avoid infinite loop.
        VisiblePosition newStart = endVisiblePosition(resultRange.get(), DOWNSTREAM);
        if (newStart == startVisiblePosition(searchRange.get(), DOWNSTREAM))
            break;

        ++matchCount;
        
        document()->addMarker(resultRange.get(), DocumentMarker::TextMatch);        
        
        // Stop looking if we hit the specified limit. A limit of 0 means no limit.
        if (limit > 0 && matchCount >= limit)
            break;
        
        setStart(searchRange.get(), newStart);
    } while (true);
    
    // Do a "fake" paint in order to execute the code that computes the rendered rect for 
    // each text match.
    Document* doc = document();
    if (doc && d->m_view && renderer()) {
        doc->updateLayout(); // Ensure layout is up to date.
        IntRect visibleRect(enclosingIntRect(d->m_view->visibleContentRect()));
        GraphicsContext context((PlatformGraphicsContext*)0);
        context.setPaintingDisabled(true);
        paint(&context, visibleRect);
    }
    
    return matchCount;
}

bool Frame::markedTextMatchesAreHighlighted() const
{
    return d->m_highlightTextMatches;
}

void Frame::setMarkedTextMatchesAreHighlighted(bool flag)
{
    if (flag == d->m_highlightTextMatches)
        return;
    
    d->m_highlightTextMatches = flag;
    document()->repaintMarkers(DocumentMarker::TextMatch);
}

void Frame::prepareForUserAction()
{
    // Reset the multiple form submission protection code.
    // We'll let you submit the same form twice if you do two separate user actions.
    loader()->resetMultipleFormSubmissionProtection();
}

Node *Frame::mousePressNode()
{
    return d->m_mousePressNode.get();
}

FrameTree* Frame::tree() const
{
    return &d->m_treeNode;
}

DOMWindow* Frame::domWindow() const
{
    if (!d->m_domWindow)
        d->m_domWindow = new DOMWindow(const_cast<Frame*>(this));

    return d->m_domWindow.get();
}

Page* Frame::page() const
{
    return d->m_page;
}

void Frame::pageDestroyed()
{
    d->m_page = 0;

    // This will stop any JS timers
    if (d->m_jscript && d->m_jscript->haveInterpreter())
        if (Window* w = Window::retrieveWindow(this))
            w->disconnectFrame();
}

void Frame::setStatusBarText(const String&)
{
}

void Frame::disconnectOwnerElement()
{
    if (d->m_ownerElement && d->m_page)
        d->m_page->decrementFrameCount();
        
    d->m_ownerElement = 0;
}

String Frame::documentTypeString() const
{
    if (Document *doc = document())
        if (DocumentType *doctype = doc->realDocType())
            return doctype->toString();

    return String();
}

bool Frame::prohibitsScrolling() const
{
    return d->m_prohibitsScrolling;
}

void Frame::setProhibitsScrolling(const bool prohibit)
{
    d->m_prohibitsScrolling = prohibit;
}

FramePrivate::FramePrivate(Page* page, Frame* parent, Frame* thisFrame, Element* ownerElement, PassRefPtr<EditorClient> client)
    : m_page(page)
    , m_treeNode(thisFrame, parent)
    , m_ownerElement(ownerElement)
    , m_jscript(0)
    , m_bJScriptEnabled(true)
    , m_bJavaEnabled(true)
    , m_bPluginsEnabled(true)
    , m_settings(0)
    , m_zoomFactor(parent ? parent->d->m_zoomFactor : 100)
    , m_bMousePressed(false)
    , m_beganSelectingText(false)
    , m_selectionController(thisFrame)
    , m_caretBlinkTimer(thisFrame, &Frame::caretBlinkTimerFired)
    , m_editor(thisFrame, client)
    , m_command(thisFrame)
    , m_caretVisible(false)
    , m_caretPaint(true)
    , m_isActive(false)
    , m_lifeSupportTimer(thisFrame, &Frame::lifeSupportTimerFired)
    , m_loader(new FrameLoader(thisFrame))
    , m_userStyleSheetLoader(0)
    , m_autoscrollTimer(thisFrame, &Frame::autoscrollTimerFired)
    , m_autoscrollRenderer(0)
    , m_mouseDownMayStartAutoscroll(false)
    , m_mouseDownMayStartDrag(false)
    , m_paintRestriction(PaintRestrictionNone)
    , m_markedTextUsesUnderlines(false)
    , m_highlightTextMatches(false)
    , m_windowHasFocus(false)
    , m_inViewSourceMode(false)
    , frameCount(0)
    , m_prohibitsScrolling(false)
{
}

FramePrivate::~FramePrivate()
{
    delete m_jscript;
    delete m_loader;
}

} // namespace WebCore
