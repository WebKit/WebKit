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

    const AtomString& specifiedName() const { return m_specifiedName; }
    const AtomString& uniqueName() const { return m_uniqueName; }
    WEBCORE_EXPORT void setSpecifiedName(const AtomString&);
    WEBCORE_EXPORT void clearName();
    WEBCORE_EXPORT Frame* parent() const;

    Frame* nextSibling() const { return m_nextSibling.get(); }
    Frame* previousSibling() const { return m_previousSibling.get(); }
    Frame* firstChild() const { return m_firstChild.get(); }
    Frame* lastChild() const { return m_lastChild.get(); }

    Frame* firstRenderedChild() const;
    Frame* nextRenderedSibling() const;

    WEBCORE_EXPORT bool isDescendantOf(const Frame* ancestor) const;
    
    WEBCORE_EXPORT Frame* traverseNext(const Frame* stayWithin = nullptr) const;
    Frame* traverseNextSkippingChildren(const Frame* stayWithin = nullptr) const;
    // Rendered means being the main frame or having an ownerRenderer. It may not have been parented in the Widget tree yet (see WidgetHierarchyUpdatesSuspensionScope).
    WEBCORE_EXPORT Frame* traverseNextRendered(const Frame* stayWithin = nullptr) const;
    WEBCORE_EXPORT Frame* traverseNext(CanWrap, DidWrap* = nullptr) const;
    WEBCORE_EXPORT Frame* traversePrevious(CanWrap, DidWrap* = nullptr) const;

    Frame* traverseNextInPostOrder(CanWrap) const;

    WEBCORE_EXPORT void appendChild(Frame&);
    void detachFromParent() { m_parent = nullptr; }
    WEBCORE_EXPORT void removeChild(Frame&);

    Frame* child(unsigned index) const;
    Frame* childByUniqueName(const AtomString& name) const;
    WEBCORE_EXPORT Frame* findByUniqueName(const AtomString&, Frame& activeFrame) const;
    WEBCORE_EXPORT Frame* findBySpecifiedName(const AtomString&, Frame& activeFrame) const;
    WEBCORE_EXPORT unsigned childCount() const;
    unsigned descendantCount() const;
    WEBCORE_EXPORT Frame& top() const;
    unsigned depth() const;

    WEBCORE_EXPORT Frame* scopedChild(unsigned index) const;
    WEBCORE_EXPORT Frame* scopedChildByUniqueName(const AtomString&) const;
    Frame* scopedChildBySpecifiedName(const AtomString& name) const;
    unsigned scopedChildCount() const;

    void resetFrameIdentifiers() { m_frameIDGenerator = 0; }

private:
    Frame* deepFirstChild() const;
    Frame* deepLastChild() const;
    Frame* nextAncestorSibling(const Frame* stayWithin) const;

    bool scopedBy(TreeScope*) const;
    Frame* scopedChild(unsigned index, TreeScope*) const;
    Frame* scopedChild(const Function<bool(const FrameTree&)>& isMatch, TreeScope*) const;
    unsigned scopedChildCount(TreeScope*) const;

    Frame* find(const AtomString& name, const Function<const AtomString&(const FrameTree&)>& nameGetter, Frame& activeFrame) const;

    AtomString uniqueChildName(const AtomString& requestedName) const;
    AtomString generateUniqueName() const;

    Frame& m_thisFrame;

    WeakPtr<Frame> m_parent;
    AtomString m_specifiedName; // The actual frame name (may be empty).
    AtomString m_uniqueName;

    RefPtr<Frame> m_nextSibling;
    WeakPtr<Frame> m_previousSibling;
    RefPtr<Frame> m_firstChild;
    WeakPtr<Frame> m_lastChild;
    mutable unsigned m_scopedChildCount { invalidCount };
    mutable uint64_t m_frameIDGenerator { 0 };
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
