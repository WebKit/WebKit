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

class AbstractFrame;
class Frame;
class TreeScope;

class FrameTree {
    WTF_MAKE_NONCOPYABLE(FrameTree);
public:
    static constexpr unsigned invalidCount = static_cast<unsigned>(-1);

    FrameTree(AbstractFrame& thisFrame, AbstractFrame* parentFrame);

    ~FrameTree();

    const AtomString& name() const { return m_name; }
    const AtomString& uniqueName() const { return m_uniqueName; }
    WEBCORE_EXPORT void setName(const AtomString&);
    WEBCORE_EXPORT void clearName();
    WEBCORE_EXPORT AbstractFrame* parent() const;

    AbstractFrame* nextSibling() const { return m_nextSibling.get(); }
    AbstractFrame* previousSibling() const { return m_previousSibling.get(); }
    AbstractFrame* firstChild() const { return m_firstChild.get(); }
    AbstractFrame* lastChild() const { return m_lastChild.get(); }

    AbstractFrame* firstRenderedChild() const;
    AbstractFrame* nextRenderedSibling() const;

    WEBCORE_EXPORT bool isDescendantOf(const AbstractFrame* ancestor) const;
    
    WEBCORE_EXPORT AbstractFrame* traverseNext(const AbstractFrame* stayWithin = nullptr) const;
    // Rendered means being the main frame or having an ownerRenderer. It may not have been parented in the Widget tree yet (see WidgetHierarchyUpdatesSuspensionScope).
    WEBCORE_EXPORT AbstractFrame* traverseNextRendered(const AbstractFrame* stayWithin = nullptr) const;
    WEBCORE_EXPORT AbstractFrame* traverseNext(CanWrap, DidWrap* = nullptr) const;
    WEBCORE_EXPORT AbstractFrame* traversePrevious(CanWrap, DidWrap* = nullptr) const;

    AbstractFrame* traverseNextInPostOrder(CanWrap) const;

    WEBCORE_EXPORT void appendChild(AbstractFrame&);
    void detachFromParent() { m_parent = nullptr; }
    void removeChild(AbstractFrame&);

    AbstractFrame* child(unsigned index) const;
    AbstractFrame* child(const AtomString& name) const;
    WEBCORE_EXPORT AbstractFrame* find(const AtomString& name, AbstractFrame& activeFrame) const;
    WEBCORE_EXPORT unsigned childCount() const;
    unsigned descendantCount() const;
    WEBCORE_EXPORT AbstractFrame& top() const;
    unsigned depth() const;

    WEBCORE_EXPORT AbstractFrame* scopedChild(unsigned index) const;
    WEBCORE_EXPORT AbstractFrame* scopedChild(const AtomString& name) const;
    unsigned scopedChildCount() const;

    void resetFrameIdentifiers() { m_frameIDGenerator = 0; }

private:
    AbstractFrame* deepFirstChild() const;
    AbstractFrame* deepLastChild() const;

    bool scopedBy(TreeScope*) const;
    AbstractFrame* scopedChild(unsigned index, TreeScope*) const;
    AbstractFrame* scopedChild(const AtomString& name, TreeScope*) const;
    unsigned scopedChildCount(TreeScope*) const;

    AtomString uniqueChildName(const AtomString& requestedName) const;
    AtomString generateUniqueName() const;

    AbstractFrame& m_thisFrame;

    WeakPtr<AbstractFrame> m_parent;
    AtomString m_name; // The actual frame name (may be empty).
    AtomString m_uniqueName;

    RefPtr<AbstractFrame> m_nextSibling;
    WeakPtr<AbstractFrame> m_previousSibling;
    RefPtr<AbstractFrame> m_firstChild;
    WeakPtr<AbstractFrame> m_lastChild;
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
WEBCORE_EXPORT void showFrameTree(const WebCore::AbstractFrame*);
#endif
