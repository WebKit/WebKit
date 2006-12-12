// -*- c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.
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
#include "Page.h"

#include "AXObjectCache.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ContextMenuClient.h"
#include "ContextMenuController.h"
#include "Document.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Element.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "RenderWidget.h"
#include "SelectionController.h"
#include "StringHash.h"
#include "Widget.h"
#include <kjs/collector.h>
#include <kjs/JSLock.h>
#include <wtf/HashMap.h>

using namespace KJS;

namespace WebCore {

static HashSet<Page*>* allPages;
static HashMap<String, HashSet<Page*>*>* frameNamespaces;

using namespace EventNames;

static bool shouldAcquireEditingFocus(Node* node)
{
    ASSERT(node);
    ASSERT(node->isContentEditable());

    Node* root = node->rootEditableElement();
    if (!root || !root->document()->frame())
        return false;

    return root->document()->frame()->editor()->shouldBeginEditing(rangeOfContents(root).get());
}

static bool shouldRelinquishEditingFocus(Node* node)
{
    ASSERT(node);
    ASSERT(node->isContentEditable());

    Node* root = node->rootEditableElement();
    if (!root || !root->document()->frame())
        return false;

    return root->document()->frame()->editor()->shouldEndEditing(rangeOfContents(root).get());
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

Page::Page(ChromeClient* chromeClient, ContextMenuClient* contextMenuClient, EditorClient* editorClient)
    : m_dragCaretController(new SelectionController(0, true))
    , m_chrome(new Chrome(this, chromeClient))
    , m_contextMenuController(new ContextMenuController(this, contextMenuClient))
    , m_editorClient(editorClient)
    , m_frameCount(0)
    , m_defersLoading(false)
{
    if (!allPages) {
        allPages = new HashSet<Page*>;
        setFocusRingColorChangeFunction(setNeedsReapplyStyles);
    }

    ASSERT(!allPages->contains(this));
    allPages->add(this);
}

Page::~Page()
{
    m_mainFrame->setView(0);
    setGroupName(String());
    allPages->remove(this);
    
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->pageDestroyed();
    
    if (allPages->isEmpty()) {
        Frame::endAllLifeSupport();
#ifndef NDEBUG
        // Force garbage collection, to release all Nodes before exiting.
        m_mainFrame = 0;
#endif
    }
    
    m_editorClient->pageDestroyed();
}

void Page::setMainFrame(PassRefPtr<Frame> mainFrame)
{
    ASSERT(!m_mainFrame); // Should only be called during initialization
    m_mainFrame = mainFrame;
}

void Page::setGroupName(const String& name)
{
    if (frameNamespaces && !m_groupName.isEmpty()) {
        HashSet<Page*>* oldNamespace = frameNamespaces->get(m_groupName);
        if (oldNamespace) {
            oldNamespace->remove(this);
            if (oldNamespace->isEmpty()) {
                frameNamespaces->remove(m_groupName);
                delete oldNamespace;
            }
        }
    }
    m_groupName = name;
    if (!name.isEmpty()) {
        if (!frameNamespaces)
            frameNamespaces = new HashMap<String, HashSet<Page*>*>;
        HashSet<Page*>* newNamespace = frameNamespaces->get(name);
        if (!newNamespace) {
            newNamespace = new HashSet<Page*>;
            frameNamespaces->add(name, newNamespace);
        }
        newNamespace->add(this);
    }
}

bool Page::setFocusedNode(PassRefPtr<Node> newFocusedNode)
{    
    if (m_focusedNode == newFocusedNode)
        return true;

    if (m_focusedNode && m_focusedNode == m_focusedNode->rootEditableElement() && !shouldRelinquishEditingFocus(m_focusedNode.get()))
        return false;

    bool focusChangeBlocked = false;
    RefPtr<Node> oldFocusedNode = m_focusedNode;
    m_focusedNode = 0;
    clearSelectionIfNeeded(oldFocusedNode.get(), newFocusedNode.get());

    // Remove focus from the existing focus node (if any)
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

        // Dispatch the blur event and let the node do any other blur related activities (important for text fields)
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
            oldFocusedNode->document()->frame()->editor()->didEndEditing();
    }

    if (newFocusedNode) {
        if (newFocusedNode == newFocusedNode->rootEditableElement() && !shouldAcquireEditingFocus(newFocusedNode.get())) {
            // delegate blocks focus change
            focusChangeBlocked = true;
            goto SetFocusedNodeDone;
        }
        // Set focus on the new node
        m_focusedNode = newFocusedNode.get();

        // Dispatch the focus event and let the node do any other focus related activities (important for text fields)
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
        m_focusedNode->setFocus();
        
        if (m_focusedNode.get() == m_focusedNode->rootEditableElement())
            m_focusedNode->document()->frame()->editor()->didBeginEditing();
        
        // This only matters for searchfield
        if (m_focusedNode->document()->view()) {
            Widget* focusWidget = widgetForNode(m_focusedNode.get());
            if (focusWidget) {
                // Make sure a widget has the right size before giving it focus.
                // Otherwise, we are testing edge cases of the Widget code.
                // Specifically, in WebCore this does not work well for text fields.
                m_focusedNode->document()->updateLayout();
                // Re-get the widget in case updating the layout changed things.
                focusWidget = widgetForNode(m_focusedNode.get());
            }
            if (focusWidget)
                focusWidget->setFocus();
            else
                m_focusedNode->document()->view()->setFocus();
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

const HashSet<Page*>* Page::frameNamespace() const
{
    return (frameNamespaces && !m_groupName.isEmpty()) ? frameNamespaces->get(m_groupName) : 0;
}

const HashSet<Page*>* Page::frameNamespace(const String& groupName)
{
    return (frameNamespaces && !groupName.isEmpty()) ? frameNamespaces->get(groupName) : 0;
}

void Page::setNeedsReapplyStyles()
{
    if (!allPages)
        return;
    HashSet<Page*>::iterator end = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != end; ++it)
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext())
            frame->setNeedsReapplyStyles();
}

void Page::setNeedsReapplyStylesForSettingsChange(Settings* settings)
{
    if (!allPages)
        return;
    HashSet<Page*>::iterator end = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != end; ++it)
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext())
            if (frame->settings() == settings)
                frame->setNeedsReapplyStyles();
}

void Page::setDefersLoading(bool defers)
{
    if (defers == m_defersLoading)
        return;

    m_defersLoading = defers;
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->loader()->setDefersLoading(defers);
}

} // namespace WebCore
