/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#include "PODIntervalTree.h"
#include "RootInlineBox.h"
#include <wtf/ListHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class RenderBlockFlow;
class RenderBox;

class FloatingObject {
    WTF_MAKE_NONCOPYABLE(FloatingObject); WTF_MAKE_FAST_ALLOCATED;
public:
    // Note that Type uses bits so you can use FloatLeftRight as a mask to query for both left and right.
    enum Type { FloatLeft = 1, FloatRight = 2, FloatLeftRight = 3 };

    static std::unique_ptr<FloatingObject> create(RenderBox&);
    std::unique_ptr<FloatingObject> copyToNewContainer(LayoutSize, bool shouldPaint = false, bool isDescendant = false) const;
    std::unique_ptr<FloatingObject> cloneForNewParent() const;

    explicit FloatingObject(RenderBox&);
    FloatingObject(RenderBox&, Type, const LayoutRect&, const LayoutSize&, bool shouldPaint, bool isDescendant);

    Type type() const { return static_cast<Type>(m_type); }
    RenderBox& renderer() const { return *m_renderer; }

    bool isPlaced() const { return m_isPlaced; }
    void setIsPlaced(bool placed = true) { m_isPlaced = placed; }

    LayoutUnit x() const { ASSERT(isPlaced()); return m_frameRect.x(); }
    LayoutUnit maxX() const { ASSERT(isPlaced()); return m_frameRect.maxX(); }
    LayoutUnit y() const { ASSERT(isPlaced()); return m_frameRect.y(); }
    LayoutUnit maxY() const { ASSERT(isPlaced()); return m_frameRect.maxY(); }
    LayoutUnit width() const { return m_frameRect.width(); }
    LayoutUnit height() const { return m_frameRect.height(); }

    void setX(LayoutUnit x) { ASSERT(!isInPlacedTree()); m_frameRect.setX(x); }
    void setY(LayoutUnit y) { ASSERT(!isInPlacedTree()); m_frameRect.setY(y); }
    void setWidth(LayoutUnit width) { ASSERT(!isInPlacedTree()); m_frameRect.setWidth(width); }
    void setHeight(LayoutUnit height) { ASSERT(!isInPlacedTree()); m_frameRect.setHeight(height); }

    void setMarginOffset(LayoutSize offset) { ASSERT(!isInPlacedTree()); m_marginOffset = offset; }

    const LayoutRect& frameRect() const { ASSERT(isPlaced()); return m_frameRect; }
    void setFrameRect(const LayoutRect& frameRect) { ASSERT(!isInPlacedTree()); m_frameRect = frameRect; }

    LayoutUnit paginationStrut() const { return m_paginationStrut; }
    void setPaginationStrut(LayoutUnit strut) { m_paginationStrut = strut; }

#ifndef NDEBUG
    bool isInPlacedTree() const { return m_isInPlacedTree; }
    void setIsInPlacedTree(bool value) { m_isInPlacedTree = value; }
#endif

    bool shouldPaint() const { return m_shouldPaint; }
    void setShouldPaint(bool shouldPaint) { m_shouldPaint = shouldPaint; }
    bool isDescendant() const { return m_isDescendant; }
    void setIsDescendant(bool isDescendant) { m_isDescendant = isDescendant; }

    // FIXME: Callers of these methods are dangerous and should be whitelisted explicitly or removed.
    RootInlineBox* originatingLine() const { return m_originatingLine.get(); }
    void clearOriginatingLine() { m_originatingLine = nullptr; }
    void setOriginatingLine(RootInlineBox& line) { m_originatingLine = makeWeakPtr(line); }

    LayoutSize locationOffsetOfBorderBox() const
    {
        ASSERT(isPlaced());
        return LayoutSize(m_frameRect.location().x() + m_marginOffset.width(), m_frameRect.location().y() + m_marginOffset.height());
    }
    LayoutSize marginOffset() const { ASSERT(isPlaced()); return m_marginOffset; }
    LayoutSize translationOffsetToAncestor() const;

private:
    WeakPtr<RenderBox> m_renderer;
    WeakPtr<RootInlineBox> m_originatingLine;
    LayoutRect m_frameRect;
    LayoutUnit m_paginationStrut;
    LayoutSize m_marginOffset;

    unsigned m_type : 2; // Type (left or right aligned)
    unsigned m_shouldPaint : 1;
    unsigned m_isDescendant : 1;
    unsigned m_isPlaced : 1;
#ifndef NDEBUG
    unsigned m_isInPlacedTree : 1;
#endif
};

// FIXME: This could be simplified if we made it inherit from PtrHash<std::unique_ptr<FloatingObject>> and
// changed PtrHashBase to have all of its hash and equal functions bottleneck through single functions (as
// is done here). That would allow us to only override those master hash and equal functions.
struct FloatingObjectHashFunctions {
    typedef std::unique_ptr<FloatingObject> T;
    typedef typename WTF::GetPtrHelper<T>::PtrType PtrType;

    static unsigned hash(PtrType key) { return PtrHash<RenderBox*>::hash(&key->renderer()); }
    static bool equal(PtrType a, PtrType b) { return &a->renderer() == &b->renderer(); }
    static const bool safeToCompareToEmptyOrDeleted = true;

    static unsigned hash(const T& key) { return hash(WTF::getPtr(key)); }
    static bool equal(const T& a, const T& b) { return equal(WTF::getPtr(a), WTF::getPtr(b)); }
    static bool equal(PtrType a, const T& b) { return equal(a, WTF::getPtr(b)); }
    static bool equal(const T& a, PtrType b) { return equal(WTF::getPtr(a), b); }
};
struct FloatingObjectHashTranslator {
    static unsigned hash(const RenderBox& key) { return PtrHash<const RenderBox*>::hash(&key); }
    static bool equal(const std::unique_ptr<FloatingObject>& a, const RenderBox& b) { return &a->renderer() == &b; }
};

typedef ListHashSet<std::unique_ptr<FloatingObject>, FloatingObjectHashFunctions> FloatingObjectSet;

typedef PODInterval<LayoutUnit, FloatingObject*> FloatingObjectInterval;
typedef PODIntervalTree<LayoutUnit, FloatingObject*> FloatingObjectTree;

// FIXME: This is really the same thing as FloatingObjectSet.
// Change clients to use that set directly, and replace the moveAllToFloatInfoMap function with a takeSet function.
typedef HashMap<RenderBox*, std::unique_ptr<FloatingObject>> RendererToFloatInfoMap;

class FloatingObjects {
    WTF_MAKE_NONCOPYABLE(FloatingObjects); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit FloatingObjects(const RenderBlockFlow&);
    ~FloatingObjects();

    void clear();
    void moveAllToFloatInfoMap(RendererToFloatInfoMap&);
    FloatingObject* add(std::unique_ptr<FloatingObject>);
    void remove(FloatingObject*);
    void addPlacedObject(FloatingObject*);
    void removePlacedObject(FloatingObject*);
    void setHorizontalWritingMode(bool b = true) { m_horizontalWritingMode = b; }

    bool hasLeftObjects() const { return m_leftObjectsCount > 0; }
    bool hasRightObjects() const { return m_rightObjectsCount > 0; }
    const FloatingObjectSet& set() const { return m_set; }
    void clearLineBoxTreePointers();

    LayoutUnit logicalLeftOffset(LayoutUnit fixedOffset, LayoutUnit logicalTop, LayoutUnit logicalHeight);
    LayoutUnit logicalRightOffset(LayoutUnit fixedOffset, LayoutUnit logicalTop, LayoutUnit logicalHeight);

    LayoutUnit logicalLeftOffsetForPositioningFloat(LayoutUnit fixedOffset, LayoutUnit logicalTop, LayoutUnit* heightRemaining);
    LayoutUnit logicalRightOffsetForPositioningFloat(LayoutUnit fixedOffset, LayoutUnit logicalTop, LayoutUnit* heightRemaining);

    LayoutUnit findNextFloatLogicalBottomBelow(LayoutUnit logicalHeight);
    LayoutUnit findNextFloatLogicalBottomBelowForBlock(LayoutUnit logicalHeight);

private:
    const RenderBlockFlow& renderer() const { return *m_renderer; }
    void computePlacedFloatsTree();
    const FloatingObjectTree* placedFloatsTree();
    void increaseObjectsCount(FloatingObject::Type);
    void decreaseObjectsCount(FloatingObject::Type);
    FloatingObjectInterval intervalForFloatingObject(FloatingObject*);

    FloatingObjectSet m_set;
    std::unique_ptr<FloatingObjectTree> m_placedFloatsTree;
    unsigned m_leftObjectsCount;
    unsigned m_rightObjectsCount;
    bool m_horizontalWritingMode;
    WeakPtr<const RenderBlockFlow> m_renderer;
};

} // namespace WebCore

#ifndef NDEBUG
namespace WTF {

// This helper is used by PODIntervalTree for debugging purposes.
template<> struct ValueToString<WebCore::FloatingObject*> {
    static String string(const WebCore::FloatingObject* floatingObject)
    {
        return String::format("%p (%ix%i %ix%i)", floatingObject, floatingObject->frameRect().x().toInt(), floatingObject->frameRect().y().toInt(), floatingObject->frameRect().maxX().toInt(), floatingObject->frameRect().maxY().toInt());
    }
};

} // namespace WTF
#endif
