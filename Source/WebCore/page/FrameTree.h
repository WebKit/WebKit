/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/FrameIdentifier.h>
#include <wtf/Forward.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

enum class CanWrap : bool { No, Yes };
enum class DidWrap : bool { No, Yes };

class Frame;
class LocalFrame;
class TreeScope;

class FrameTree {
    WTF_MAKE_NONCOPYABLE(FrameTree);
public:
    static constexpr unsigned invalidCount = static_cast<unsigned>(-1);

    FrameTree(Frame& thisFrame, Frame* parentFrame);

    ~FrameTree();

    const AtomString& specifiedName() const LIFETIME_BOUND { return m_specifiedName; }
    WEBCORE_EXPORT AtomString uniqueName() const;
    WEBCORE_EXPORT void setSpecifiedName(const AtomString&);
    WEBCORE_EXPORT void clearName();
    WEBCORE_EXPORT Frame* NODELETE parent() const;

    Frame* nextSibling() const { return m_nextSibling.get(); }
    Frame* previousSibling() const { return m_previousSibling.get(); }
    Frame* firstChild() const { return m_firstChild.get(); }
    Frame* lastChild() const { return m_lastChild.get(); }

    RefPtr<Frame> firstRenderedChild() const;
    RefPtr<Frame> nextRenderedSibling() const;

    LocalFrame* NODELETE firstLocalDescendant() const;
    LocalFrame* NODELETE nextLocalSibling() const;

    WEBCORE_EXPORT bool NODELETE isDescendantOf(const Frame* ancestor) const;
    
    WEBCORE_EXPORT Frame* NODELETE traverseNext(const Frame* stayWithin = nullptr) const;
    Frame* NODELETE traverseNextSkippingChildren(const Frame* stayWithin = nullptr) const;
    // Rendered means being the main frame or having an ownerRenderer. It may not have been parented in the Widget tree yet (see WidgetHierarchyUpdatesSuspensionScope).
    WEBCORE_EXPORT RefPtr<Frame> traverseNextRendered(const Frame* stayWithin = nullptr) const;
    WEBCORE_EXPORT Frame* NODELETE traverseNext(CanWrap, DidWrap* = nullptr) const;
    WEBCORE_EXPORT Frame* NODELETE traversePrevious(CanWrap, DidWrap* = nullptr) const;

    Frame* NODELETE traverseNextInPostOrder(CanWrap) const;

    WEBCORE_EXPORT void appendChild(Frame&);
    void detachFromParent() { m_parent = nullptr; }
    WEBCORE_EXPORT void removeChild(Frame&);
    WEBCORE_EXPORT void replaceChild(Frame&, Frame&);

    Frame* NODELETE child(unsigned index) const;
    Frame* NODELETE childBySpecifiedName(const AtomString& name) const;
    WEBCORE_EXPORT Frame* NODELETE descendantByFrameID(FrameIdentifier) const;
    WEBCORE_EXPORT RefPtr<Frame> findByUniqueName(const AtomString&, Frame& activeFrame) const;
    WEBCORE_EXPORT RefPtr<Frame> findBySpecifiedName(const AtomString&, Frame& activeFrame) const;
    WEBCORE_EXPORT unsigned NODELETE childCount() const;
    unsigned NODELETE descendantCount() const;
    WEBCORE_EXPORT Frame& NODELETE top() const;
    unsigned NODELETE depth() const;

    WEBCORE_EXPORT RefPtr<Frame> scopedChild(unsigned index) const;
    WEBCORE_EXPORT RefPtr<Frame> scopedChildByUniqueName(const AtomString&) const;
    RefPtr<Frame> scopedChildBySpecifiedName(const AtomString& name) const;
    unsigned scopedChildCount() const;

private:
    Frame* NODELETE deepFirstChild() const;
    Frame* NODELETE deepLastChild() const;
    Frame* NODELETE nextAncestorSibling(const Frame* stayWithin) const;

    RefPtr<Frame> scopedChild(unsigned index, TreeScope*) const;
    RefPtr<Frame> scopedChild(NOESCAPE const Function<bool(const FrameTree&)>& isMatch, TreeScope*) const;
    unsigned scopedChildCount(TreeScope*) const;

    template<typename F> RefPtr<Frame> find(const AtomString& name, F&& nameGetter, Frame& activeFrame) const;

    WeakRef<Frame> m_thisFrame;

    WeakPtr<Frame> m_parent;
    AtomString m_specifiedName; // The actual frame name (may be empty).

    RefPtr<Frame> m_nextSibling;
    WeakPtr<Frame> m_previousSibling;
    RefPtr<Frame> m_firstChild;
    WeakPtr<Frame> m_lastChild;
    mutable unsigned m_scopedChildCount { invalidCount };
};

ASCIILiteral blankTargetFrameName();
ASCIILiteral selfTargetFrameName();

bool isBlankTargetFrameName(StringView);
bool isParentTargetFrameName(StringView);
bool isSelfTargetFrameName(StringView);
bool isTopTargetFrameName(StringView);

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from the debugger.
WEBCORE_EXPORT void showFrameTree(const WebCore::Frame*);
#endif
