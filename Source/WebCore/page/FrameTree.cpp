/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "Page.h"
#include "PageGroup.h"
#include <stdarg.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

FrameTree::~FrameTree()
{
    for (Frame* child = firstChild(); child; child = child->tree().nextSibling())
        child->setView(nullptr);
}

void FrameTree::setName(const AtomString& name) 
{
    m_name = name;
    if (!parent()) {
        m_uniqueName = name;
        return;
    }
    m_uniqueName = nullAtom(); // Remove our old frame name so it's not considered in uniqueChildName.
    m_uniqueName = parent()->tree().uniqueChildName(name);
}

void FrameTree::clearName()
{
    m_name = nullAtom();
    m_uniqueName = nullAtom();
}

Frame* FrameTree::parent() const 
{ 
    return m_parent;
}

void FrameTree::appendChild(Frame& child)
{
    ASSERT(child.page() == m_thisFrame.page());
    child.tree().m_parent = &m_thisFrame;
    Frame* oldLast = m_lastChild;
    m_lastChild = &child;

    if (oldLast) {
        child.tree().m_previousSibling = oldLast;
        oldLast->tree().m_nextSibling = &child;
    } else
        m_firstChild = &child;

    m_scopedChildCount = invalidCount;

    ASSERT(!m_lastChild->tree().m_nextSibling);
}

void FrameTree::removeChild(Frame& child)
{
    Frame*& newLocationForPrevious = m_lastChild == &child ? m_lastChild : child.tree().m_nextSibling->tree().m_previousSibling;
    RefPtr<Frame>& newLocationForNext = m_firstChild == &child ? m_firstChild : child.tree().m_previousSibling->tree().m_nextSibling;

    child.tree().m_parent = nullptr;
    newLocationForPrevious = std::exchange(child.tree().m_previousSibling, nullptr);
    newLocationForNext = WTFMove(child.tree().m_nextSibling);

    m_scopedChildCount = invalidCount;
}

AtomString FrameTree::uniqueChildName(const AtomString& requestedName) const
{
    // If the requested name (the frame's "name" attribute) is unique, just use that.
    if (!requestedName.isEmpty() && !child(requestedName) && !equalIgnoringASCIICase(requestedName, "_blank"))
        return requestedName;

    // The "name" attribute was not unique or absent. Generate a name based on a counter on the main frame that gets reset
    // on navigation. The name uses HTML comment syntax to avoid collisions with author names.
    return generateUniqueName();
}

AtomString FrameTree::generateUniqueName() const
{
    auto& top = this->top();
    if (&top.tree() != this)
        return top.tree().generateUniqueName();

    return makeString("<!--frame", ++m_frameIDGenerator, "-->");
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

inline Frame* FrameTree::scopedChild(const AtomString& name, TreeScope* scope) const
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

Frame* FrameTree::scopedChild(const AtomString& name) const
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

Frame* FrameTree::child(const AtomString& name) const
{
    for (Frame* child = firstChild(); child; child = child->tree().nextSibling())
        if (child->tree().uniqueName() == name)
            return child;
    return nullptr;
}

// FrameTree::find() only returns frames in pages that are related to the active
// page by an opener <-> openee relationship.
static bool isFrameFamiliarWith(Frame& frameA, Frame& frameB)
{
    if (frameA.page() == frameB.page())
        return true;

    auto* frameAOpener = frameA.mainFrame().loader().opener();
    auto* frameBOpener = frameB.mainFrame().loader().opener();
    return (frameAOpener && frameAOpener->page() == frameB.page()) || (frameBOpener && frameBOpener->page() == frameA.page()) || (frameAOpener && frameBOpener && frameAOpener->page() == frameBOpener->page());
}

Frame* FrameTree::find(const AtomString& name, Frame& activeFrame) const
{
    // FIXME: _current is not part of the HTML specification.
    if (equalIgnoringASCIICase(name, "_self") || name == "_current" || name.isEmpty())
        return &m_thisFrame;
    
    if (equalIgnoringASCIICase(name, "_top"))
        return &top();
    
    if (equalIgnoringASCIICase(name, "_parent"))
        return parent() ? parent() : &m_thisFrame;

    // Since "_blank" should never be any frame's name, the following is only an optimization.
    if (equalIgnoringASCIICase(name, "_blank"))
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
        for (auto* frame = &otherPage->mainFrame(); frame; frame = frame->tree().traverseNext()) {
            if (frame->tree().uniqueName() == name && isFrameFamiliarWith(activeFrame, *frame))
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

Frame* FrameTree::firstRenderedChild() const
{
    Frame* child = firstChild();
    if (!child)
        return nullptr;
    
    if (child->ownerRenderer())
        return child;

    while ((child = child->tree().nextSibling())) {
        if (child->ownerRenderer())
            return child;
    }
    
    return nullptr;
}

Frame* FrameTree::nextRenderedSibling() const
{
    Frame* sibling = &m_thisFrame;

    while ((sibling = sibling->tree().nextSibling())) {
        if (sibling->ownerRenderer())
            return sibling;
    }
    
    return nullptr;
}

Frame* FrameTree::traverseNextRendered(const Frame* stayWithin) const
{
    Frame* child = firstRenderedChild();
    if (child) {
        ASSERT(!stayWithin || child->tree().isDescendantOf(stayWithin));
        return child;
    }

    if (&m_thisFrame == stayWithin)
        return nullptr;

    Frame* sibling = nextRenderedSibling();
    if (sibling) {
        ASSERT(!stayWithin || sibling->tree().isDescendantOf(stayWithin));
        return sibling;
    }

    Frame* frame = &m_thisFrame;
    while (!sibling && (!stayWithin || frame->tree().parent() != stayWithin)) {
        frame = frame->tree().parent();
        if (!frame)
            return nullptr;
        sibling = frame->tree().nextRenderedSibling();
    }

    if (frame) {
        ASSERT(!stayWithin || !sibling || sibling->tree().isDescendantOf(stayWithin));
        return sibling;
    }

    return nullptr;
}

Frame* FrameTree::traverseNext(CanWrap canWrap, DidWrap* didWrap) const
{
    if (Frame* result = traverseNext())
        return result;

    if (canWrap == CanWrap::Yes) {
        if (didWrap)
            *didWrap = DidWrap::Yes;
        return &m_thisFrame.mainFrame();
    }

    return nullptr;
}

Frame* FrameTree::traversePrevious(CanWrap canWrap, DidWrap* didWrap) const
{
    // FIXME: besides the wrap feature, this is just the traversePreviousNode algorithm

    if (Frame* prevSibling = previousSibling())
        return prevSibling->tree().deepLastChild();
    if (Frame* parentFrame = parent())
        return parentFrame;
    
    // no siblings, no parent, self==top
    if (canWrap == CanWrap::Yes) {
        if (didWrap)
            *didWrap = DidWrap::Yes;
        return deepLastChild();
    }

    // top view is always the last one in this ordering, so prev is nil without wrap
    return nullptr;
}

Frame* FrameTree::traverseNextInPostOrder(CanWrap canWrap) const
{
    if (m_nextSibling)
        return m_nextSibling->tree().deepFirstChild();
    if (m_parent)
        return m_parent;
    if (canWrap == CanWrap::Yes)
        return deepFirstChild();
    return nullptr;
}

Frame* FrameTree::deepFirstChild() const
{
    Frame* result = &m_thisFrame;
    while (auto* next = result->tree().firstChild())
        result = next;
    return result;
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
    printf("  frameView=%p (needs layout %d)\n", view, view ? view->needsLayout() : false);
    printIndent(indent);
    printf("  renderView=%p\n", view ? view->renderView() : nullptr);
    printIndent(indent);
    printf("  ownerRenderer=%p\n", frame.ownerRenderer());
    printIndent(indent);
    printf("  document=%p (needs style recalc %d)\n", frame.document(), frame.document() ? frame.document()->childNeedsStyleRecalc() : false);
    printIndent(indent);
    printf("  uri=%s\n", frame.document()->documentURI().utf8().data());

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
