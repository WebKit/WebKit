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
#include <wtf/CheckedArithmetic.h>
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
    WEBCORE_EXPORT Frame* LEGACY_NODELETE parent() const;

    Frame* nextSibling() const { return m_nextSibling.get(); }
    Frame* previousSibling() const { return m_previousSibling.get(); }
    Frame* firstChild() const { return m_firstChild.get(); }
    Frame* lastChild() const { return m_lastChild.get(); }

    RefPtr<Frame> firstRenderedChild() const;
    RefPtr<Frame> nextRenderedSibling() const;

    LocalFrame* LEGACY_NODELETE firstLocalDescendant() const;
    LocalFrame* LEGACY_NODELETE nextLocalSibling() const;

    WEBCORE_EXPORT bool LEGACY_NODELETE isDescendantOf(const Frame* ancestor) const;

    bool containsRemoteFrame() const;
    bool containsLocalFrame() const;
    WEBCORE_EXPORT bool hasRemoteFrameAncestor() const;
    
    WEBCORE_EXPORT Frame* LEGACY_NODELETE traverseNext(const Frame* stayWithin = nullptr) const;
    Frame* LEGACY_NODELETE traverseNextSkippingChildren(const Frame* stayWithin = nullptr) const;
    // Rendered means being the main frame or having an ownerRenderer. It may not have been parented in the Widget tree yet (see WidgetHierarchyUpdatesSuspensionScope).
    WEBCORE_EXPORT RefPtr<Frame> traverseNextRendered(const Frame* stayWithin = nullptr) const;
    WEBCORE_EXPORT Frame* LEGACY_NODELETE traverseNext(CanWrap, DidWrap* = nullptr) const;
    WEBCORE_EXPORT Frame* LEGACY_NODELETE traversePrevious(CanWrap, DidWrap* = nullptr) const;

    Frame* LEGACY_NODELETE traverseNextInPostOrder(CanWrap) const;

    WEBCORE_EXPORT void appendChild(Frame&);
    WEBCORE_EXPORT void detachFromParent();
    WEBCORE_EXPORT void removeChild(Frame&);
    WEBCORE_EXPORT void replaceChild(Frame&, Frame&);

    Frame* LEGACY_NODELETE child(unsigned index) const;
    Frame* LEGACY_NODELETE childBySpecifiedName(const AtomString& name) const;
    WEBCORE_EXPORT Frame* LEGACY_NODELETE descendantByFrameID(FrameIdentifier) const;
    WEBCORE_EXPORT RefPtr<Frame> findByUniqueName(const AtomString&, Frame& activeFrame) const;
    WEBCORE_EXPORT RefPtr<Frame> findBySpecifiedName(const AtomString&, Frame& activeFrame) const;
    WEBCORE_EXPORT unsigned LEGACY_NODELETE childCount() const;
    unsigned LEGACY_NODELETE descendantCount() const;
    WEBCORE_EXPORT Frame& LEGACY_NODELETE top() const;
    unsigned LEGACY_NODELETE depth() const;

    bool hasRemoteFrameDescendant() const { return !!m_remoteFrameDescendantCount; }

    WEBCORE_EXPORT RefPtr<Frame> scopedChild(unsigned index) const;
    WEBCORE_EXPORT RefPtr<Frame> scopedChildByUniqueName(const AtomString&) const;
    RefPtr<Frame> scopedChildBySpecifiedName(const AtomString& name) const;
    unsigned scopedChildCount() const;

private:
    Frame* LEGACY_NODELETE deepFirstChild() const;
    Frame* LEGACY_NODELETE deepLastChild() const;
    Frame* LEGACY_NODELETE nextAncestorSibling(const Frame* stayWithin) const;

    unsigned remoteFrameCountIncludingSelf() const;
    void adjustRemoteFrameDescendantCountForSelfAndAncestors(int delta);
#if ASSERT_ENABLED
    unsigned remoteFrameDescendantCountSlow() const;
#endif

    RefPtr<Frame> scopedChild(unsigned index, TreeScope*) const;
    RefPtr<Frame> scopedChild(NOESCAPE const Function<bool(const FrameTree&)>& isMatch, TreeScope*) const;
    unsigned scopedChildCount(TreeScope*) const;

    template<typename F> RefPtr<Frame> find(const AtomString& name, NOESCAPE const F& nameGetter, Frame& activeFrame) const;

    WeakRef<Frame> m_thisFrame;

    WeakPtr<Frame> m_parent;
    AtomString m_specifiedName; // The actual frame name (may be empty).

    RefPtr<Frame> m_nextSibling;
    WeakPtr<Frame> m_previousSibling;
    RefPtr<Frame> m_firstChild;
    WeakPtr<Frame> m_lastChild;
    mutable unsigned m_scopedChildCount { invalidCount };
    Checked<unsigned> m_remoteFrameDescendantCount { 0 };
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
