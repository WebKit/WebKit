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
#include "FrameLoader.h"
#include "HTMLFrameOwnerElement.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Page.h"
#include "PageGroup.h"
#include <stdarg.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

FrameTree::FrameTree(Frame& thisFrame, Frame* parentFrame)
    : m_thisFrame(thisFrame)
    , m_parent(parentFrame)
{
}

FrameTree::~FrameTree()
{
    for (auto* child = firstChild(); child; child = child->tree().nextSibling())
        child->disconnectView();
}

void FrameTree::setSpecifiedName(const AtomString& specifiedName)
{
    m_specifiedName = specifiedName;
}

void FrameTree::clearName()
{
    m_specifiedName = nullAtom();
}

Frame* FrameTree::parent() const
{
    return m_parent.get();
}

void FrameTree::appendChild(Frame& child)
{
    ASSERT(child.page() == m_thisFrame->page());
    child.tree().m_parent = m_thisFrame.ptr();
    WeakPtr<Frame> oldLast = m_lastChild;
    m_lastChild = child;

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
    WeakPtr<Frame>& newLocationForPrevious = m_lastChild == &child ? m_lastChild : child.tree().m_nextSibling->tree().m_previousSibling;
    RefPtr<Frame>& newLocationForNext = m_firstChild == &child ? m_firstChild : child.tree().m_previousSibling->tree().m_nextSibling;

    child.tree().m_parent = nullptr;
    newLocationForPrevious = std::exchange(child.tree().m_previousSibling, nullptr);
    newLocationForNext = WTFMove(child.tree().m_nextSibling);

    m_scopedChildCount = invalidCount;
}

Ref<Frame> FrameTree::protectedThisFrame() const
{
    return m_thisFrame.get();
}

static bool inScope(Frame& frame, TreeScope& scope)
{
    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
    if (!localFrame)
        return true;
    RefPtr document = localFrame->document();
    if (!document)
        return false;
    RefPtr owner = document->ownerElement();
    return owner && &owner->treeScope() == &scope;
}

Frame* FrameTree::scopedChild(unsigned index, TreeScope* scope) const
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

inline Frame* FrameTree::scopedChild(const Function<bool(const FrameTree&)>& isMatch, TreeScope* scope) const
{
    if (!scope)
        return nullptr;

    for (auto* child = firstChild(); child; child = child->tree().nextSibling()) {
        if (isMatch(child->tree()) && inScope(*child, *scope))
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

Frame* FrameTree::scopedChild(unsigned index) const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_thisFrame.get());
    if (!localFrame)
        return nullptr;
    return scopedChild(index, localFrame->protectedDocument().get());
}

Frame* FrameTree::scopedChildByUniqueName(const AtomString& uniqueName) const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_thisFrame.get());
    if (!localFrame)
        return nullptr;
    return scopedChild([&](auto& frameTree) {
        return frameTree.uniqueName() == uniqueName;
    }, localFrame->protectedDocument().get());
}

Frame* FrameTree::scopedChildBySpecifiedName(const AtomString& specifiedName) const
{
    auto* localFrame = dynamicDowncast<LocalFrame>(m_thisFrame.get());
    if (!localFrame)
        return nullptr;
    return scopedChild([&](auto& frameTree) {
        return frameTree.specifiedName() == specifiedName;
    }, localFrame->protectedDocument().get());
}

unsigned FrameTree::scopedChildCount() const
{
    if (m_scopedChildCount == invalidCount) {
        if (auto* localFrame = dynamicDowncast<LocalFrame>(m_thisFrame.get()))
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

Frame* FrameTree::child(unsigned index) const
{
    auto* result = firstChild();
    for (unsigned i = 0; result && i != index; ++i)
        result = result->tree().nextSibling();
    return result;
}

Frame* FrameTree::childByFrameID(FrameIdentifier frameID) const
{
    for (auto* child = firstChild(); child; child = child->tree().nextSibling()) {
        if (child->frameID() == frameID)
            return child;
    }
    return nullptr;
}

Frame* FrameTree::childBySpecifiedName(const AtomString& name) const
{
    for (auto* child = firstChild(); child; child = child->tree().nextSibling()) {
        if (child->tree().specifiedName() == name)
            return child;
    }
    return nullptr;
}

// FrameTree::find() only returns frames in pages that are related to the active
// page by an opener <-> openee relationship.
static bool isFrameFamiliarWith(Frame& frameA, Frame& frameB)
{
    if (frameA.page() == frameB.page())
        return true;

    auto* frameAOpener = frameA.mainFrame().opener();
    auto* frameBOpener = frameB.mainFrame().opener();
    return (frameAOpener && frameAOpener->page() == frameB.page())
        || (frameBOpener && frameBOpener->page() == frameA.page())
        || (frameAOpener && frameBOpener && frameAOpener->page() == frameBOpener->page());
}

template<typename F>
inline Frame* FrameTree::find(const AtomString& name, F&& nameGetter, Frame& activeFrame) const
{
    if (isSelfTargetFrameName(name))
        return m_thisFrame.ptr();

    if (isTopTargetFrameName(name))
        return &top();

    if (isParentTargetFrameName(name))
        return parent() ? parent() : m_thisFrame.ptr();

    // Since "_blank" cannot be a frame's name, this check is an optimization, not for correctness.
    if (isBlankTargetFrameName(name))
        return nullptr;

    // Search subtree starting with this frame first.
    Ref thisFrame = m_thisFrame.get();
    for (auto* frame = thisFrame.ptr(); frame; frame = frame->tree().traverseNext(thisFrame.ptr())) {
        if (nameGetter(frame->tree()) == name)
            return frame;
    }

    // Then the rest of the tree.
    for (Frame* frame = &thisFrame->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (nameGetter(frame->tree()) == name)
            return frame;
    }

    // Search the entire tree of each of the other pages in this namespace.
    RefPtr page = thisFrame->page();
    if (!page)
        return nullptr;

    // FIXME: These pages are searched in random order; that doesn't seem good. Maybe use order of creation?
    for (auto& otherPage : page->group().pages()) {
        if (&otherPage == page || otherPage.isClosing())
            continue;
        for (Frame* frame = &otherPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
            if (nameGetter(frame->tree()) == name && isFrameFamiliarWith(activeFrame, *frame))
                return frame;
        }
    }

    return nullptr;
}

Frame* FrameTree::findByUniqueName(const AtomString& uniqueName, Frame& activeFrame) const
{
    return find(uniqueName, [&](auto& frameTree) -> AtomString {
        return frameTree.uniqueName();
    }, activeFrame);
}

Frame* FrameTree::findBySpecifiedName(const AtomString& specifiedName, Frame& activeFrame) const
{
    return find(specifiedName, [&](auto& frameTree) -> const AtomString& {
        return frameTree.specifiedName();
    }, activeFrame);
}

bool FrameTree::isDescendantOf(const Frame* ancestor) const
{
    if (!ancestor)
        return false;

    if (m_thisFrame->page() != ancestor->page())
        return false;

    for (Frame* frame = m_thisFrame.ptr(); frame; frame = frame->tree().parent()) {
        if (frame == ancestor)
            return true;
    }
    return false;
}

Frame* FrameTree::traverseNext(const Frame* stayWithin) const
{
    auto* child = firstChild();
    if (child) {
        ASSERT(!stayWithin || child->tree().isDescendantOf(stayWithin));
        return child;
    }

    if (m_thisFrame.ptr() == stayWithin)
        return nullptr;

    auto* sibling = nextSibling();
    if (sibling) {
        ASSERT(!stayWithin || sibling->tree().isDescendantOf(stayWithin));
        return sibling;
    }

    auto* frame = m_thisFrame.ptr();
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

Frame* FrameTree::traverseNextSkippingChildren(const Frame* stayWithin) const
{
    if (m_thisFrame.ptr() == stayWithin)
        return nullptr;
    if (auto* sibling = nextSibling())
        return sibling;
    return nextAncestorSibling(stayWithin);
}

Frame* FrameTree::nextAncestorSibling(const Frame* stayWithin) const
{
    ASSERT(!nextSibling());
    ASSERT(m_thisFrame.ptr() != stayWithin);
    for (auto* ancestor = parent(); ancestor; ancestor = ancestor->tree().parent()) {
        if (ancestor == stayWithin)
            return nullptr;
        if (auto ancestorSibling = ancestor->tree().nextSibling())
            return ancestorSibling;
    }
    return nullptr;
}

Frame* FrameTree::firstRenderedChild() const
{
    auto* child = firstChild();
    if (!child)
        return nullptr;

    if (auto* localChild = dynamicDowncast<LocalFrame>(child); localChild && localChild->ownerRenderer())
        return child;

    while ((child = child->tree().nextSibling())) {
        if (auto* localChild = dynamicDowncast<LocalFrame>(child); localChild && localChild->ownerRenderer())
            return child;
    }

    return nullptr;
}

Frame* FrameTree::nextRenderedSibling() const
{
    auto* sibling = m_thisFrame.ptr();

    while ((sibling = sibling->tree().nextSibling())) {
        if (auto* localSibling = dynamicDowncast<LocalFrame>(sibling); localSibling && localSibling->ownerRenderer())
            return sibling;
    }
    
    return nullptr;
}

Frame* FrameTree::traverseNextRendered(const Frame* stayWithin) const
{
    auto* child = firstRenderedChild();
    if (child) {
        ASSERT(!stayWithin || child->tree().isDescendantOf(stayWithin));
        return child;
    }

    if (m_thisFrame.ptr() == stayWithin)
        return nullptr;

    auto* sibling = nextRenderedSibling();
    if (sibling) {
        ASSERT(!stayWithin || sibling->tree().isDescendantOf(stayWithin));
        return sibling;
    }

    auto* frame = m_thisFrame.ptr();
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
    if (auto* result = traverseNext())
        return result;

    if (canWrap == CanWrap::Yes) {
        if (didWrap)
            *didWrap = DidWrap::Yes;
        return &m_thisFrame->mainFrame();
    }

    return nullptr;
}

Frame* FrameTree::traversePrevious(CanWrap canWrap, DidWrap* didWrap) const
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

Frame* FrameTree::traverseNextInPostOrder(CanWrap canWrap) const
{
    if (m_nextSibling)
        return m_nextSibling->tree().deepFirstChild();
    if (m_parent)
        return m_parent.get();
    if (canWrap == CanWrap::Yes)
        return deepFirstChild();
    return nullptr;
}

Frame* FrameTree::deepFirstChild() const
{
    auto* result = m_thisFrame.ptr();
    while (auto* next = result->tree().firstChild())
        result = next;
    return result;
}

Frame* FrameTree::deepLastChild() const
{
    auto* result = m_thisFrame.ptr();
    for (auto* last = lastChild(); last; last = last->tree().lastChild())
        result = last;

    return result;
}

Frame& FrameTree::top() const
{
    ASSERT(m_thisFrame->isMainFrame() || m_thisFrame->tree().parent());
    return m_thisFrame->mainFrame();
}

Ref<Frame> FrameTree::protectedTop() const
{
    return top();
}

unsigned FrameTree::depth() const
{
    unsigned depth = 0;
    for (auto* parent = m_thisFrame.ptr(); parent; parent = parent->tree().parent())
        depth++;
    return depth;
}

AtomString FrameTree::uniqueName() const
{
    if (!parent())
        return m_specifiedName;

    auto frameIndex { 0u };
    for (RefPtr frame = top().tree().firstChild(); frame; frame = frame->tree().traverseNext()) {
        bool frameMatch = frame->frameID() == m_thisFrame->frameID();
        auto frameName = frame->tree().specifiedName();
        if (frameName.isEmpty() || isBlankTargetFrameName(frameName) || frame->tree().childBySpecifiedName(frameName)) {
            frameIndex++;
            if (frameMatch)
                return makeAtomString("<!--frame"_s, frameIndex, "-->"_s);
        }
        if (frameMatch)
            return frameName;
    }
    return nullAtom();
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

static void printFrames(const WebCore::Frame& frame, const WebCore::Frame* targetFrame, int indent)
{
    if (&frame == targetFrame) {
        printf("--> ");
        printIndent(indent - 1);
    } else
        printIndent(indent);

    auto* localFrame = dynamicDowncast<WebCore::LocalFrame>(frame);
    if (!localFrame)
        printf("RemoteFrame %p\n", &frame);
    else {
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
    }
    for (auto* child = frame.tree().firstChild(); child; child = child->tree().nextSibling())
        printFrames(*child, targetFrame, indent + 1);
}

void showFrameTree(const WebCore::Frame* frame)
{
    if (!frame) {
        printf("Null input frame\n");
        return;
    }

    printFrames(frame->tree().protectedTop(), frame, 0);
}

#endif
