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

FrameTree::FrameTree(AbstractFrame& thisFrame, AbstractFrame* parentFrame)
    : m_thisFrame(static_cast<Frame&>(thisFrame))
    , m_parent(parentFrame)
{
}

FrameTree::~FrameTree()
{
    for (auto* child = firstChild(); child; child = child->tree().nextSibling()) {
        if (auto* localFrame = dynamicDowncast<LocalFrame>(child))
            localFrame->setView(nullptr);
    }
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

AbstractFrame* FrameTree::parent() const
{
    return m_parent.get();
}

void FrameTree::appendChild(AbstractFrame& child)
{
    ASSERT(child.page() == m_thisFrame.page());
    child.tree().m_parent = m_thisFrame;
    WeakPtr<AbstractFrame> oldLast = m_lastChild;
    m_lastChild = child;

    if (oldLast) {
        child.tree().m_previousSibling = oldLast;
        oldLast->tree().m_nextSibling = &child;
    } else
        m_firstChild = &child;

    m_scopedChildCount = invalidCount;

    ASSERT(!m_lastChild->tree().m_nextSibling);
}

void FrameTree::removeChild(AbstractFrame& child)
{
    WeakPtr<AbstractFrame>& newLocationForPrevious = m_lastChild == &child ? m_lastChild : child.tree().m_nextSibling->tree().m_previousSibling;
    RefPtr<AbstractFrame>& newLocationForNext = m_firstChild == &child ? m_firstChild : child.tree().m_previousSibling->tree().m_nextSibling;

    child.tree().m_parent = nullptr;
    newLocationForPrevious = std::exchange(child.tree().m_previousSibling, nullptr);
    newLocationForNext = WTFMove(child.tree().m_nextSibling);

    m_scopedChildCount = invalidCount;
}

AtomString FrameTree::uniqueChildName(const AtomString& requestedName) const
{
    // If the requested name (the frame's "name" attribute) is unique, just use that.
    if (!requestedName.isEmpty() && !child(requestedName) && !isBlankTargetFrameName(requestedName))
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

    return makeAtomString("<!--frame", ++m_frameIDGenerator, "-->");
}

static bool inScope(AbstractFrame& frame, TreeScope& scope)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(frame);
    if (!localFrame)
        return false;
    Document* document = localFrame->document();
    if (!document)
        return false;
    HTMLFrameOwnerElement* owner = document->ownerElement();
    if (!owner)
        return false;
    return &owner->treeScope() == &scope;
}

AbstractFrame* FrameTree::scopedChild(unsigned index, TreeScope* scope) const
{
    if (!scope)
        return nullptr;

    unsigned scopedIndex = 0;
    for (auto* result = firstChild(); result; result = result->tree().nextSibling()) {
        if (inScope(*result, *scope)) {
            if (scopedIndex == index)
                return result;
            scopedIndex++;
        }
    }

    return nullptr;
}

AbstractFrame* FrameTree::scopedChild(const AtomString& name, TreeScope* scope) const
{
    if (!scope)
        return nullptr;

    for (auto* child = firstChild(); child; child = child->tree().nextSibling()) {
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
    for (auto* result = firstChild(); result; result = result->tree().nextSibling()) {
        if (inScope(*result, *scope))
            scopedCount++;
    }

    return scopedCount;
}

AbstractFrame* FrameTree::scopedChild(unsigned index) const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_thisFrame);
    if (!localFrame)
        return nullptr;
    return scopedChild(index, localFrame->document());
}

AbstractFrame* FrameTree::scopedChild(const AtomString& name) const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_thisFrame);
    if (!localFrame)
        return nullptr;
    return scopedChild(name, localFrame->document());
}

unsigned FrameTree::scopedChildCount() const
{
    if (m_scopedChildCount == invalidCount) {
        if (auto* localFrame = dynamicDowncast<LocalFrame>(m_thisFrame))
            m_scopedChildCount = scopedChildCount(localFrame->document());
    }
    return m_scopedChildCount;
}

unsigned FrameTree::childCount() const
{
    unsigned count = 0;
    for (auto* child = firstChild(); child; child = child->tree().nextSibling())
        ++count;
    return count;
}

unsigned FrameTree::descendantCount() const
{
    unsigned count = 0;
    for (auto* child = firstChild(); child; child = child->tree().nextSibling())
        count += 1 + child->tree().descendantCount();
    return count;
}

AbstractFrame* FrameTree::child(unsigned index) const
{
    auto* result = firstChild();
    for (unsigned i = 0; result && i != index; ++i)
        result = result->tree().nextSibling();
    return result;
}

AbstractFrame* FrameTree::child(const AtomString& name) const
{
    for (auto* child = firstChild(); child; child = child->tree().nextSibling()) {
        if (child->tree().uniqueName() == name)
            return child;
    }
    return nullptr;
}

// FrameTree::find() only returns frames in pages that are related to the active
// page by an opener <-> openee relationship.
static bool isFrameFamiliarWith(AbstractFrame& abstractFrameA, AbstractFrame& abstractFrameB)
{
    if (abstractFrameA.page() == abstractFrameB.page())
        return true;

    auto* frameA = dynamicDowncast<LocalFrame>(abstractFrameA);
    auto* frameB = dynamicDowncast<LocalFrame>(abstractFrameB);
    if (!frameA || !frameB)
        return false;

    auto* frameAOpener = frameA->mainFrame().loader().opener();
    auto* frameBOpener = frameB->mainFrame().loader().opener();
    return (frameAOpener && frameAOpener->page() == frameB->page()) || (frameBOpener && frameBOpener->page() == frameA->page()) || (frameAOpener && frameBOpener && frameAOpener->page() == frameBOpener->page());
}

AbstractFrame* FrameTree::find(const AtomString& name, AbstractFrame& activeFrame) const
{
    if (isSelfTargetFrameName(name))
        return &m_thisFrame;
    
    if (isTopTargetFrameName(name))
        return &top();
    
    if (isParentTargetFrameName(name))
        return parent() ? parent() : &m_thisFrame;

    // Since "_blank" cannot be a frame's name, this check is an optimization, not for correctness.
    if (isBlankTargetFrameName(name))
        return nullptr;

    // Search subtree starting with this frame first.
    for (auto* frame = &m_thisFrame; frame; frame = frame->tree().traverseNext(&m_thisFrame)) {
        if (frame->tree().uniqueName() == name)
            return frame;
    }

    // Then the rest of the tree.
    auto* localFrame = dynamicDowncast<LocalFrame>(m_thisFrame);
    for (AbstractFrame* frame = localFrame ? &localFrame->mainFrame() : nullptr; frame; frame = frame->tree().traverseNext()) {
        if (frame->tree().uniqueName() == name)
            return frame;
    }

    // Search the entire tree of each of the other pages in this namespace.
    Page* page = m_thisFrame.page();
    if (!page)
        return nullptr;
    
    // FIXME: These pages are searched in random order; that doesn't seem good. Maybe use order of creation?
    for (auto& otherPage : page->group().pages()) {
        if (&otherPage == page || otherPage.isClosing())
            continue;
        for (AbstractFrame* frame = &otherPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
            if (frame->tree().uniqueName() == name && isFrameFamiliarWith(activeFrame, *frame))
                return frame;
        }
    }

    return nullptr;
}

bool FrameTree::isDescendantOf(const AbstractFrame* ancestor) const
{
    if (!ancestor)
        return false;

    if (m_thisFrame.page() != ancestor->page())
        return false;

    for (AbstractFrame* frame = &m_thisFrame; frame; frame = frame->tree().parent()) {
        if (frame == ancestor)
            return true;
    }
    return false;
}

AbstractFrame* FrameTree::traverseNext(const AbstractFrame* stayWithin) const
{
    auto* child = firstChild();
    if (child) {
        ASSERT(!stayWithin || child->tree().isDescendantOf(stayWithin));
        return child;
    }

    if (&m_thisFrame == stayWithin)
        return nullptr;

    auto* sibling = nextSibling();
    if (sibling) {
        ASSERT(!stayWithin || sibling->tree().isDescendantOf(stayWithin));
        return sibling;
    }

    auto* frame = &m_thisFrame;
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

AbstractFrame* FrameTree::firstRenderedChild() const
{
    auto* child = firstChild();
    if (!child)
        return nullptr;

    auto* localChild = dynamicDowncast<LocalFrame>(child);
    if (!localChild)
        return nullptr;

    if (localChild->ownerRenderer())
        return child;

    while ((child = child->tree().nextSibling())) {
        auto* localChild = dynamicDowncast<LocalFrame>(child);
        if (!localChild)
            continue;
        if (localChild->ownerRenderer())
            return child;
    }
    
    return nullptr;
}

AbstractFrame* FrameTree::nextRenderedSibling() const
{
    auto* sibling = &m_thisFrame;

    while ((sibling = sibling->tree().nextSibling())) {
        auto* localSibling = dynamicDowncast<LocalFrame>(sibling);
        if (localSibling && localSibling->ownerRenderer())
            return sibling;
    }
    
    return nullptr;
}

AbstractFrame* FrameTree::traverseNextRendered(const AbstractFrame* stayWithin) const
{
    auto* child = firstRenderedChild();
    if (child) {
        ASSERT(!stayWithin || child->tree().isDescendantOf(stayWithin));
        return child;
    }

    if (&m_thisFrame == stayWithin)
        return nullptr;

    auto* sibling = nextRenderedSibling();
    if (sibling) {
        ASSERT(!stayWithin || sibling->tree().isDescendantOf(stayWithin));
        return sibling;
    }

    auto* frame = &m_thisFrame;
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

AbstractFrame* FrameTree::traverseNext(CanWrap canWrap, DidWrap* didWrap) const
{
    if (auto* result = traverseNext())
        return result;

    if (canWrap == CanWrap::Yes) {
        if (didWrap)
            *didWrap = DidWrap::Yes;
        auto* localFrame = dynamicDowncast<LocalFrame>(m_thisFrame);
        if (!localFrame)
            return nullptr;
        return &localFrame->mainFrame();
    }

    return nullptr;
}

AbstractFrame* FrameTree::traversePrevious(CanWrap canWrap, DidWrap* didWrap) const
{
    // FIXME: besides the wrap feature, this is just the traversePreviousNode algorithm

    if (auto* prevSibling = previousSibling())
        return prevSibling->tree().deepLastChild();
    if (auto* parentFrame = parent())
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

AbstractFrame* FrameTree::traverseNextInPostOrder(CanWrap canWrap) const
{
    if (m_nextSibling)
        return m_nextSibling->tree().deepFirstChild();
    if (m_parent)
        return dynamicDowncast<LocalFrame>(m_parent.get());
    if (canWrap == CanWrap::Yes)
        return deepFirstChild();
    return nullptr;
}

AbstractFrame* FrameTree::deepFirstChild() const
{
    auto* result = &m_thisFrame;
    while (auto* next = result->tree().firstChild())
        result = next;
    return result;
}

AbstractFrame* FrameTree::deepLastChild() const
{
    auto* result = &m_thisFrame;
    for (auto* last = lastChild(); last; last = last->tree().lastChild())
        result = last;

    return result;
}

AbstractFrame& FrameTree::top() const
{
    auto* frame = &m_thisFrame;
    for (auto* parent = &m_thisFrame; parent; parent = parent->tree().parent())
        frame = parent;
    return *frame;
}

ASCIILiteral blankTargetFrameName()
{
    return "_blank"_s;
}

// FIXME: Is it important to have this? Can't we just use the empty string everywhere this is used, instead?
ASCIILiteral selfTargetFrameName()
{
    return "_self"_s;
}

bool isBlankTargetFrameName(StringView name)
{
    return equalIgnoringASCIICase(name, "_blank"_s);
}

bool isParentTargetFrameName(StringView name)
{
    return equalIgnoringASCIICase(name, "_parent"_s);
}

bool isSelfTargetFrameName(StringView name)
{
    // FIXME: Some day we should remove _current, which is not part of the HTML specification.
    return name.isEmpty() || equalIgnoringASCIICase(name, "_self"_s) || name == "_current"_s;
}

bool isTopTargetFrameName(StringView name)
{
    return equalIgnoringASCIICase(name, "_top"_s);
}

} // namespace WebCore

#ifndef NDEBUG

static void printIndent(int indent)
{
    for (int i = 0; i < indent; ++i)
        printf("    ");
}

static void printFrames(const WebCore::AbstractFrame& frame, const WebCore::AbstractFrame* targetFrame, int indent)
{
    if (&frame == targetFrame) {
        printf("--> ");
        printIndent(indent - 1);
    } else
        printIndent(indent);

    auto* localFrame = dynamicDowncast<WebCore::Frame>(frame);
    if (!localFrame) {
        printf("RemoteFrame %p\n", &frame);
        return;
    }

    auto* view = localFrame->view();
    printf("Frame %p %dx%d\n", &frame, view ? view->width() : 0, view ? view->height() : 0);
    printIndent(indent);
    printf("  ownerElement=%p\n", localFrame->ownerElement());
    printIndent(indent);
    printf("  frameView=%p (needs layout %d)\n", view, view ? view->needsLayout() : false);
    printIndent(indent);
    printf("  renderView=%p\n", view ? view->renderView() : nullptr);
    printIndent(indent);
    printf("  ownerRenderer=%p\n", localFrame->ownerRenderer());
    printIndent(indent);
    printf("  document=%p (needs style recalc %d)\n", localFrame->document(), localFrame->document() ? localFrame->document()->childNeedsStyleRecalc() : false);
    printIndent(indent);
    printf("  uri=%s\n", localFrame->document()->documentURI().utf8().data());

    for (auto* child = frame.tree().firstChild(); child; child = child->tree().nextSibling())
        printFrames(*child, targetFrame, indent + 1);
}

void showFrameTree(const WebCore::AbstractFrame* frame)
{
    if (!frame) {
        printf("Null input frame\n");
        return;
    }

    printFrames(frame->tree().top(), frame, 0);
}

#endif
