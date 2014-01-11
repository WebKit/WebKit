/*
 * Copyright (C) 2006, 2013 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "FrameTree.h"

#include "Document.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "MainFrame.h"
#include "Page.h"
#include "PageGroup.h"
#include <stdarg.h>
#include <wtf/StringExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

FrameTree::~FrameTree()
{
    for (Frame* child = firstChild(); child; child = child->tree().nextSibling())
        child->setView(0);
}

void FrameTree::setName(const AtomicString& name) 
{
    m_name = name;
    if (!parent()) {
        m_uniqueName = name;
        return;
    }
    m_uniqueName = AtomicString(); // Remove our old frame name so it's not considered in uniqueChildName.
    m_uniqueName = parent()->tree().uniqueChildName(name);
}

void FrameTree::clearName()
{
    m_name = AtomicString();
    m_uniqueName = AtomicString();
}

Frame* FrameTree::parent() const 
{ 
    return m_parent;
}

bool FrameTree::transferChild(PassRefPtr<Frame> child)
{
    Frame* oldParent = child->tree().parent();
    if (oldParent == &m_thisFrame)
        return false; // |child| is already a child of m_thisFrame.

    if (oldParent)
        oldParent->tree().removeChild(child.get());

    ASSERT(child->page() == m_thisFrame.page());
    child->tree().m_parent = &m_thisFrame;

    // We need to ensure that the child still has a unique frame name with respect to its new parent.
    child->tree().setName(child->tree().m_name);

    actuallyAppendChild(child); // Note, on return |child| is null.
    return true;
}

void FrameTree::appendChild(PassRefPtr<Frame> child)
{
    ASSERT(child->page() == m_thisFrame.page());
    child->tree().m_parent = &m_thisFrame;
    actuallyAppendChild(child); // Note, on return |child| is null.
}

void FrameTree::actuallyAppendChild(PassRefPtr<Frame> child)
{
    ASSERT(child->tree().m_parent == &m_thisFrame);
    Frame* oldLast = m_lastChild;
    m_lastChild = child.get();

    if (oldLast) {
        child->tree().m_previousSibling = oldLast;
        oldLast->tree().m_nextSibling = child;
    } else
        m_firstChild = child;

    m_scopedChildCount = invalidCount;

    ASSERT(!m_lastChild->tree().m_nextSibling);
}

void FrameTree::removeChild(Frame* child)
{
    child->tree().m_parent = 0;

    // Slightly tricky way to prevent deleting the child until we are done with it, w/o
    // extra refs. These swaps leave the child in a circular list by itself. Clearing its
    // previous and next will then finally deref it.

    RefPtr<Frame>& newLocationForNext = m_firstChild == child ? m_firstChild : child->tree().m_previousSibling->tree().m_nextSibling;
    Frame*& newLocationForPrevious = m_lastChild == child ? m_lastChild : child->tree().m_nextSibling->tree().m_previousSibling;
    swap(newLocationForNext, child->tree().m_nextSibling);
    // For some inexplicable reason, the following line does not compile without the explicit std:: namespace
    std::swap(newLocationForPrevious, child->tree().m_previousSibling);

    child->tree().m_previousSibling = 0;
    child->tree().m_nextSibling = 0;

    m_scopedChildCount = invalidCount;
}

AtomicString FrameTree::uniqueChildName(const AtomicString& requestedName) const
{
    if (!requestedName.isEmpty() && !child(requestedName) && requestedName != "_blank")
        return requestedName;

    // Create a repeatable name for a child about to be added to us. The name must be
    // unique within the frame tree. The string we generate includes a "path" of names
    // from the root frame down to us. For this path to be unique, each set of siblings must
    // contribute a unique name to the path, which can't collide with any HTML-assigned names.
    // We generate this path component by index in the child list along with an unlikely
    // frame name that can't be set in HTML because it collides with comment syntax.

    const char framePathPrefix[] = "<!--framePath ";
    const int framePathPrefixLength = 14;
    const int framePathSuffixLength = 3;

    // Find the nearest parent that has a frame with a path in it.
    Vector<Frame*, 16> chain;
    Frame* frame;
    for (frame = &m_thisFrame; frame; frame = frame->tree().parent()) {
        if (frame->tree().uniqueName().startsWith(framePathPrefix))
            break;
        chain.append(frame);
    }
    StringBuilder name;
    name.append(framePathPrefix);
    if (frame) {
        name.append(frame->tree().uniqueName().string().substring(framePathPrefixLength,
            frame->tree().uniqueName().length() - framePathPrefixLength - framePathSuffixLength));
    }
    for (int i = chain.size() - 1; i >= 0; --i) {
        frame = chain[i];
        name.append('/');
        name.append(frame->tree().uniqueName());
    }

    name.appendLiteral("/<!--frame");
    name.appendNumber(childCount());
    name.appendLiteral("-->-->");

    return name.toAtomicString();
}

static bool inScope(Frame& frame, TreeScope& scope)
{
    Document* document = frame.document();
    if (!document)
        return false;
    HTMLFrameOwnerElement* owner = document->ownerElement();
    if (!owner)
        return false;
    return &owner->treeScope() == &scope;
}

inline Frame* FrameTree::scopedChild(unsigned index, TreeScope* scope) const
{
    if (!scope)
        return nullptr;

    unsigned scopedIndex = 0;
    for (Frame* result = firstChild(); result; result = result->tree().nextSibling()) {
        if (inScope(*result, *scope)) {
            if (scopedIndex == index)
                return result;
            scopedIndex++;
        }
    }

    return nullptr;
}

inline Frame* FrameTree::scopedChild(const AtomicString& name, TreeScope* scope) const
{
    if (!scope)
        return nullptr;

    for (Frame* child = firstChild(); child; child = child->tree().nextSibling()) {
        if (child->tree().uniqueName() == name && inScope(*child, *scope))
            return child;
    }
    return nullptr;
}

inline unsigned FrameTree::scopedChildCount(TreeScope* scope) const
{
    if (!scope)
        return 0;

    unsigned scopedCount = 0;
    for (Frame* result = firstChild(); result; result = result->tree().nextSibling()) {
        if (inScope(*result, *scope))
            scopedCount++;
    }

    return scopedCount;
}

Frame* FrameTree::scopedChild(unsigned index) const
{
    return scopedChild(index, m_thisFrame.document());
}

Frame* FrameTree::scopedChild(const AtomicString& name) const
{
    return scopedChild(name, m_thisFrame.document());
}

unsigned FrameTree::scopedChildCount() const
{
    if (m_scopedChildCount == invalidCount)
        m_scopedChildCount = scopedChildCount(m_thisFrame.document());
    return m_scopedChildCount;
}

unsigned FrameTree::childCount() const
{
    unsigned count = 0;
    for (Frame* result = firstChild(); result; result = result->tree().nextSibling())
        ++count;
    return count;
}

Frame* FrameTree::child(unsigned index) const
{
    Frame* result = firstChild();
    for (unsigned i = 0; result && i != index; ++i)
        result = result->tree().nextSibling();
    return result;
}

Frame* FrameTree::child(const AtomicString& name) const
{
    for (Frame* child = firstChild(); child; child = child->tree().nextSibling())
        if (child->tree().uniqueName() == name)
            return child;
    return nullptr;
}

Frame* FrameTree::find(const AtomicString& name) const
{
    if (name == "_self" || name == "_current" || name.isEmpty())
        return &m_thisFrame;
    
    if (name == "_top")
        return &top();
    
    if (name == "_parent")
        return parent() ? parent() : &m_thisFrame;

    // Since "_blank" should never be any frame's name, the following is only an optimization.
    if (name == "_blank")
        return nullptr;

    // Search subtree starting with this frame first.
    for (Frame* frame = &m_thisFrame; frame; frame = frame->tree().traverseNext(&m_thisFrame)) {
        if (frame->tree().uniqueName() == name)
            return frame;
    }

    // Then the rest of the tree.
    for (Frame* frame = &m_thisFrame.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->tree().uniqueName() == name)
            return frame;
    }

    // Search the entire tree of each of the other pages in this namespace.
    // FIXME: Is random order OK?
    Page* page = m_thisFrame.page();
    if (!page)
        return nullptr;
    
    for (auto* otherPage : page->group().pages()) {
        if (otherPage == page)
            continue;
        for (Frame* frame = &otherPage->mainFrame(); frame; frame = frame->tree().traverseNext()) {
            if (frame->tree().uniqueName() == name)
                return frame;
        }
    }

    return nullptr;
}

bool FrameTree::isDescendantOf(const Frame* ancestor) const
{
    if (!ancestor)
        return false;

    if (m_thisFrame.page() != ancestor->page())
        return false;

    for (Frame* frame = &m_thisFrame; frame; frame = frame->tree().parent())
        if (frame == ancestor)
            return true;
    return false;
}

Frame* FrameTree::traverseNext(const Frame* stayWithin) const
{
    Frame* child = firstChild();
    if (child) {
        ASSERT(!stayWithin || child->tree().isDescendantOf(stayWithin));
        return child;
    }

    if (&m_thisFrame == stayWithin)
        return nullptr;

    Frame* sibling = nextSibling();
    if (sibling) {
        ASSERT(!stayWithin || sibling->tree().isDescendantOf(stayWithin));
        return sibling;
    }

    Frame* frame = &m_thisFrame;
    while (!sibling && (!stayWithin || frame->tree().parent() != stayWithin)) {
        frame = frame->tree().parent();
        if (!frame)
            return nullptr;
        sibling = frame->tree().nextSibling();
    }

    if (frame) {
        ASSERT(!stayWithin || !sibling || sibling->tree().isDescendantOf(stayWithin));
        return sibling;
    }

    return nullptr;
}

Frame* FrameTree::traverseNextWithWrap(bool wrap) const
{
    if (Frame* result = traverseNext())
        return result;

    if (wrap)
        return &m_thisFrame.mainFrame();

    return nullptr;
}

Frame* FrameTree::traversePreviousWithWrap(bool wrap) const
{
    // FIXME: besides the wrap feature, this is just the traversePreviousNode algorithm

    if (Frame* prevSibling = previousSibling())
        return prevSibling->tree().deepLastChild();
    if (Frame* parentFrame = parent())
        return parentFrame;
    
    // no siblings, no parent, self==top
    if (wrap)
        return deepLastChild();

    // top view is always the last one in this ordering, so prev is nil without wrap
    return nullptr;
}

Frame* FrameTree::deepLastChild() const
{
    Frame* result = &m_thisFrame;
    for (Frame* last = lastChild(); last; last = last->tree().lastChild())
        result = last;

    return result;
}

Frame& FrameTree::top() const
{
    Frame* frame = &m_thisFrame;
    for (Frame* parent = &m_thisFrame; parent; parent = parent->tree().parent())
        frame = parent;
    return *frame;
}

} // namespace WebCore

#ifndef NDEBUG

static void printIndent(int indent)
{
    for (int i = 0; i < indent; ++i)
        printf("    ");
}

static void printFrames(const WebCore::Frame& frame, const WebCore::Frame* targetFrame, int indent)
{
    if (&frame == targetFrame) {
        printf("--> ");
        printIndent(indent - 1);
    } else
        printIndent(indent);

    WebCore::FrameView* view = frame.view();
    printf("Frame %p %dx%d\n", &frame, view ? view->width() : 0, view ? view->height() : 0);
    printIndent(indent);
    printf("  ownerElement=%p\n", frame.ownerElement());
    printIndent(indent);
    printf("  frameView=%p\n", view);
    printIndent(indent);
    printf("  document=%p\n", frame.document());
    printIndent(indent);
    printf("  uri=%s\n\n", frame.document()->documentURI().utf8().data());

    for (WebCore::Frame* child = frame.tree().firstChild(); child; child = child->tree().nextSibling())
        printFrames(*child, targetFrame, indent + 1);
}

void showFrameTree(const WebCore::Frame* frame)
{
    if (!frame) {
        printf("Null input frame\n");
        return;
    }

    printFrames(frame->tree().top(), frame, 0);
}

#endif
