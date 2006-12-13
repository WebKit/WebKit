/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "FocusController.h"

#include "AXObjectCache.h"
#include "Document.h"
#include "Editor.h"
#include "Element.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "Page.h"
#include "Range.h"
#include "RenderObject.h"
#include "RenderWidget.h"
#include "SelectionController.h"
#include "Widget.h"
#include <wtf/Platform.h>

namespace WebCore {

using namespace EventNames;

static bool shouldFocus(Node* node)
{
    ASSERT(node);
    
    Node* root = node->rootEditableElement();
    if (node != root)
        return true;
    ASSERT(node->isContentEditable());

    Frame* frame = node->document()->frame();
    if (!frame)
        return true;

    return frame->editor()->shouldBeginEditing(rangeOfContents(node).get());
}

static bool shouldUnfocus(Node* node)
{
    ASSERT(node);

    Node* root = node->rootEditableElement();
    if (node != root)
        return true;
    ASSERT(node->isContentEditable());
        
    Frame* frame = node->document()->frame();
    if (!frame)
        return true;

    return frame->editor()->shouldEndEditing(rangeOfContents(root).get());
}

static void clearSelectionIfNeeded(Node* oldFocusedNode, Node* newFocusedNode)
{
    Frame* frame = oldFocusedNode ? oldFocusedNode->document()->frame() : 0;
    if (!frame)
        return;

    // Clear the selection when changing the focus node to null or to a node that is not 
    // contained by the current selection.
    Node *startContainer = frame->selectionController()->start().node();
    if (!newFocusedNode || (startContainer && startContainer != newFocusedNode && !(startContainer->isDescendantOf(newFocusedNode)) && startContainer->shadowAncestorNode() != newFocusedNode))
        frame->selectionController()->clear();
}

static Widget* widgetForNode(Node* node)
{
    if (!node)
        return 0;
    RenderObject* renderer = node->renderer();
    if (!renderer || !renderer->isWidget())
        return 0;
    return static_cast<RenderWidget*>(renderer)->widget();
}

FocusController::FocusController(Page* page)
    : m_page(page)
{
}

bool FocusController::setFocusedNode(PassRefPtr<Node> newFocusedNode)
{    
    if (m_focusedNode == newFocusedNode)
        return true;

    if (m_focusedNode && !shouldUnfocus(m_focusedNode.get()))
        return false;

    bool focusChangeBlocked = false;
    RefPtr<Node> oldFocusedNode = m_focusedNode;
    m_focusedNode = 0;
    clearSelectionIfNeeded(oldFocusedNode.get(), newFocusedNode.get());

    // Remove focus from previously focused node (if any)
    if (oldFocusedNode && !oldFocusedNode->inDetach()) { 
        if (oldFocusedNode->active())
            oldFocusedNode->setActive(false);

        oldFocusedNode->setFocus(false);
                
        // Dispatch a change event for text fields or textareas that have been edited
        RenderObject *r = static_cast<RenderObject*>(oldFocusedNode->renderer());
        if (r && (r->isTextArea() || r->isTextField()) && r->isEdited()) {
            EventTargetNodeCast(oldFocusedNode.get())->dispatchHTMLEvent(changeEvent, true, false);
            if ((r = static_cast<RenderObject*>(oldFocusedNode.get()->renderer())))
                r->setEdited(false);
        }

        // Dispatch the blur event and let the node do any other blur related activities 
        // (important for text fields)
        EventTargetNodeCast(oldFocusedNode.get())->dispatchBlurEvent();

        if (m_focusedNode) {
            // handler shifted focus
            focusChangeBlocked = true;
            newFocusedNode = 0;
        }
        clearSelectionIfNeeded(oldFocusedNode.get(), newFocusedNode.get());
        EventTargetNodeCast(oldFocusedNode.get())->dispatchUIEvent(DOMFocusOutEvent);
        if (m_focusedNode) {
            // handler shifted focus
            focusChangeBlocked = true;
            newFocusedNode = 0;
        }
        clearSelectionIfNeeded(oldFocusedNode.get(), newFocusedNode.get());
        if ((oldFocusedNode == oldFocusedNode->document()) && oldFocusedNode->hasOneRef())
            return true;

        if (oldFocusedNode == oldFocusedNode->rootEditableElement())
            if (Frame* frame = oldFocusedNode->document()->frame())
                frame->editor()->didEndEditing();
    }

    if (newFocusedNode) {
        if (!shouldFocus(newFocusedNode.get())) {
            // delegate blocks focus change
            focusChangeBlocked = true;
            goto SetFocusedNodeDone;
        }
        // Set focus on the new node
        m_focusedNode = newFocusedNode.get();

        // Dispatch the focus event and let the node do any other focus related activities
        // (important for text fields)
        EventTargetNodeCast(m_focusedNode.get())->dispatchFocusEvent();

        if (m_focusedNode != newFocusedNode) {
            // handler shifted focus
            focusChangeBlocked = true;
            goto SetFocusedNodeDone;
        }
        EventTargetNodeCast(m_focusedNode.get())->dispatchUIEvent(DOMFocusInEvent);
        if (m_focusedNode != newFocusedNode) { 
            // handler shifted focus
            focusChangeBlocked = true;
            goto SetFocusedNodeDone;
        }

        if (Page* page = m_focusedNode->document()->page())
            page->focusController()->setFocusedFrame(m_focusedNode->document()->frame());
        
        m_focusedNode->setFocus();

        if (m_focusedNode.get() == m_focusedNode->rootEditableElement())
            if (Frame* frame = m_focusedNode->document()->frame())
                frame->editor()->didBeginEditing();
        
        // This only matters for <input type=search>, until it becomes an engine-based control.
        Widget* focusedWidget = widgetForNode(m_focusedNode.get());
        if (focusedWidget) {
            // Make sure a widget has the right size before giving it focus.
            // Otherwise, we are testing edge cases of the Widget code.
            // Specifically, in WebCore this does not work well for text fields.
            m_focusedNode->document()->updateLayout();
            // Re-get the widget in case updating the layout changed things.
            focusedWidget = widgetForNode(m_focusedNode.get());
            if (focusedWidget)
                focusedWidget->setFocus();
        }
   }

#if PLATFORM(MAC)
    if (!focusChangeBlocked && m_focusedNode && AXObjectCache::accessibilityEnabled())
        m_focusedNode->document()->axObjectCache()->handleFocusedUIElementChanged();
#endif

SetFocusedNodeDone:
    Document::updateDocumentsRendering();
    return !focusChangeBlocked;
}

void FocusController::focusedNodeDetached(Node* node)
{
    ASSERT(node == m_focusedNode);
    setFocusedNode(0);
}

void FocusController::setFocusedFrame(PassRefPtr<Frame> frame)
{
    if (m_focusedFrame == frame)
        return;

    if (m_focusedFrame) {
        m_focusedFrame->setWindowHasFocus(false);
        m_focusedFrame->setIsActive(false);
    }

    m_focusedFrame = frame;

    if (m_focusedFrame) {
        m_focusedFrame->setWindowHasFocus(true);
        m_focusedFrame->setIsActive(true);
    }
}

Frame* FocusController::focusedOrMainFrame()
{
    Frame* frame = focusedFrame();
    if (!frame)
        setFocusedFrame(m_page->mainFrame());
    ASSERT(focusedFrame());
    return focusedFrame();
}

} // namespace WebCore
