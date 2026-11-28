/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/LayoutRect.h>
#include <wtf/ListHashSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

namespace Layout {
class FloatingContext;
class PlacedFloats;
}

class RenderBlockFlow;
class RenderBox;
class FloatingObjects;

// Note that FloatingObjectType uses bits so you can use FloatLeftRight as a mask to query for both left and right.
enum FloatingObjectType : uint8_t { FloatLeft = 1, FloatRight = 2, FloatLeftRight = 3 };

class FloatingObject {
    WTF_MAKE_TZONE_ALLOCATED(FloatingObject);
    WTF_MAKE_NONCOPYABLE(FloatingObject);
public:
    using Type = FloatingObjectType;
    static constexpr FloatingObjectType FloatLeft = WebCore::FloatLeft;
    static constexpr FloatingObjectType FloatRight = WebCore::FloatRight;
    static constexpr FloatingObjectType FloatLeftRight = WebCore::FloatLeftRight;

    static std::unique_ptr<FloatingObject> create(RenderBox&);
    std::unique_ptr<FloatingObject> copyToNewContainer(LayoutSize, bool isDescendant = false) const;
    std::unique_ptr<FloatingObject> cloneForNewParent() const;

    explicit FloatingObject(RenderBox&);
    FloatingObject(RenderBox&, Type, const LayoutRect&, const LayoutSize&, bool isDescendant);

    Type type() const { return static_cast<Type>(m_type); }
    RenderBox* renderer() const { ASSERT(m_renderer); return m_renderer.get(); }
#if ENABLE(TREE_DEBUGGING)
    bool hasRenderer() const { return !!m_renderer; }
#endif

    LayoutUnit x() const { return m_frameRect.x(); }
    LayoutUnit maxX() const { return m_frameRect.maxX(); }
    LayoutUnit y() const { return m_frameRect.y(); }
    LayoutUnit maxY() const { return m_frameRect.maxY(); }
    LayoutUnit width() const { return m_frameRect.width(); }
    LayoutUnit height() const { return m_frameRect.height(); }

    void setX(LayoutUnit x) { m_frameRect.setX(x); }
    void setY(LayoutUnit y) { m_frameRect.setY(y); }
    void setWidth(LayoutUnit width) { m_frameRect.setWidth(width); }
    void setHeight(LayoutUnit height) { m_frameRect.setHeight(height); }

    void setMarginOffset(LayoutSize offset) { m_marginOffset = offset; }

    const LayoutRect& frameRect() const LIFETIME_BOUND { return m_frameRect; }
    void setFrameRect(const LayoutRect& frameRect) { m_frameRect = frameRect; }

    bool isDescendant() const { return m_isDescendant; }
    void setIsDescendant(bool isDescendant) { m_isDescendant = isDescendant; }

    LayoutSize locationOffsetOfBorderBox() const
    {
        return LayoutSize(m_frameRect.location().x() + m_marginOffset.width(), m_frameRect.location().y() + m_marginOffset.height());
    }
    LayoutSize marginOffset() const { return m_marginOffset; }

private:
    friend FloatingObjects;

    SingleThreadWeakPtr<RenderBox> m_renderer;
    LayoutRect m_frameRect;
    LayoutSize m_marginOffset;
    unsigned m_type : 2; // Type (left or right aligned)
    unsigned m_isDescendant : 1 { false };
};

// FIXME: This could be simplified if we made it inherit from PtrHash<std::unique_ptr<FloatingObject>> and
// changed PtrHashBase to have all of its hash and equal functions bottleneck through single functions (as
// is done here). That would allow us to only override those master hash and equal functions.
struct FloatingObjectHashFunctions {
    using T = std::unique_ptr<FloatingObject>;
    using PtrType = FloatingObject*;

    static unsigned hash(const FloatingObject* key) { return PtrHash<RenderBox*>::hash(key->renderer()); }
    static bool equal(const FloatingObject* a, const FloatingObject* b) { return a->renderer() == b->renderer(); }
    static const bool safeToCompareToEmptyOrDeleted = true;

    static unsigned hash(const T& key) { return hash(WTF::getPtr(key)); }
    static bool equal(const T& a, const T& b) { return equal(WTF::getPtr(a), WTF::getPtr(b)); }
    static bool equal(const FloatingObject* a, const T& b) { return equal(a, WTF::getPtr(b)); }
    static bool equal(const T& a, const FloatingObject* b) { return equal(WTF::getPtr(a), b); }
};
struct FloatingObjectHashTranslator {
    static unsigned hash(const RenderBox& key) { return PtrHash<const RenderBox*>::hash(&key); }
    static bool equal(const std::unique_ptr<FloatingObject>& a, const RenderBox& b) { return a->renderer() == &b; }
};

typedef ListHashSet<std::unique_ptr<FloatingObject>, FloatingObjectHashFunctions> FloatingObjectSet;

// Where the floats on one side leave room for a new float at a position.
struct OffsetForPositioningFloat {
    LayoutUnit offset;
    // How far down the float that sets the offset reaches, if a float does.
    std::optional<LayoutUnit> heightRemaining;
};

// FIXME: This is really the same thing as FloatingObjectSet.
// Change clients to use that set directly, and replace the moveAllToFloatInfoMap function with a takeSet function.
class FloatingObjects {
    WTF_MAKE_TZONE_ALLOCATED(FloatingObjects);
    WTF_MAKE_NONCOPYABLE(FloatingObjects);
public:
    explicit FloatingObjects(const RenderBlockFlow&);
    ~FloatingObjects();

    void clear();
    FloatingObject* add(std::unique_ptr<FloatingObject>);
    void remove(FloatingObject*);
    // For a float inline layout placed, which put it in placedFloats() already.
    void place(RenderBox&, const LayoutRect& frameRect, LayoutSize marginOffset);
    void setHorizontalWritingMode(bool b = true) { m_horizontalWritingMode = b; }

    bool hasLeftObjects() const { return m_leftObjectsCount > 0; }
    bool hasRightObjects() const { return m_rightObjectsCount > 0; }
    const FloatingObjectSet& set() const LIFETIME_BOUND { return m_set; }
    Layout::PlacedFloats& placedFloats() LIFETIME_BOUND;
    const Layout::PlacedFloats* keptPlacedFloats() const LIFETIME_BOUND { return m_placedFloats.get(); }
    std::unique_ptr<Layout::PlacedFloats> makePlacedFloats() const;
    // Built on placedFloats() and dropped with it, so that FloatingContext::constraints() continues from its last position during the child layout.
    const Layout::FloatingContext& floatingContext() LIFETIME_BOUND;
    // A float leaving the tree is taken out of the list, except when the whole render tree is destroyed (see PlacedFloats::Item),
    // or in skipped content, which drops the whole list instead (see RenderBlockFlow::markAllDescendantsWithFloatsForLayout()).
    void clearPlacedFloats();
    // The FloatingContext takes the block's width, which may change with the next layout.
    void clearFloatingContext();

    OffsetForPositioningFloat offsetForPositioningFloat(FloatingObject::Type, LayoutUnit fixedOffset, LayoutUnit logicalTop);

    void shiftFloatsBy(LayoutUnit blockShift);

private:
    const RenderBlockFlow& renderer() const { ASSERT(m_renderer); return *m_renderer; }
    void addPlacedObject(FloatingObject*);
    void removePlacedObject(FloatingObject*);
    NODELETE void increaseObjectsCount(FloatingObjectType);
    NODELETE void decreaseObjectsCount(FloatingObjectType);

    FloatingObjectSet m_set;
    std::unique_ptr<Layout::PlacedFloats> m_placedFloats;
    std::unique_ptr<Layout::FloatingContext> m_floatingContext;
    unsigned m_leftObjectsCount { 0 };
    unsigned m_rightObjectsCount { 0 };
    bool m_horizontalWritingMode { false };
    SingleThreadWeakPtr<const RenderBlockFlow> m_renderer;
};

#if ENABLE(TREE_DEBUGGING)
TextStream& operator<<(TextStream&, const FloatingObject&);
#endif

} // namespace WebCore
