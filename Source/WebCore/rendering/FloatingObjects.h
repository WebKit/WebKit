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

#ifndef FloatingObjects_h
#define FloatingObjects_h

#include "PODIntervalTree.h"
#include "RootInlineBox.h"
#include <wtf/ListHashSet.h>

namespace WebCore {

class RenderBlock;
class RenderBox;

enum ShapeOutsideFloatOffsetMode { ShapeOutsideFloatShapeOffset, ShapeOutsideFloatMarginBoxOffset };

class FloatingObject {
    WTF_MAKE_NONCOPYABLE(FloatingObject); WTF_MAKE_FAST_ALLOCATED;
public:
    // Note that Type uses bits so you can use FloatLeftRight as a mask to query for both left and right.
    enum Type { FloatLeft = 1, FloatRight = 2, FloatLeftRight = 3 };

    FloatingObject(EFloat type)
        : m_renderer(0)
        , m_originatingLine(0)
        , m_paginationStrut(0)
        , m_shouldPaint(true)
        , m_isDescendant(false)
        , m_isPlaced(false)
#ifndef NDEBUG
        , m_isInPlacedTree(false)
#endif
    {
        ASSERT(type != NoFloat);
        if (type == LeftFloat)
            m_type = FloatLeft;
        else if (type == RightFloat)
            m_type = FloatRight;
    }

    FloatingObject(Type type, const LayoutRect& frameRect)
        : m_renderer(0)
        , m_originatingLine(0)
        , m_frameRect(frameRect)
        , m_paginationStrut(0)
        , m_type(type)
        , m_shouldPaint(true)
        , m_isDescendant(false)
        , m_isPlaced(true)
#ifndef NDEBUG
        , m_isInPlacedTree(false)
#endif
    {
    }

    FloatingObject* clone() const
    {
        FloatingObject* cloneObject = new FloatingObject(type(), m_frameRect);
        cloneObject->m_renderer = m_renderer;
        cloneObject->m_originatingLine = m_originatingLine;
        cloneObject->m_paginationStrut = m_paginationStrut;
        cloneObject->m_shouldPaint = m_shouldPaint;
        cloneObject->m_isDescendant = m_isDescendant;
        cloneObject->m_isPlaced = m_isPlaced;
        return cloneObject;
    }

    Type type() const { return static_cast<Type>(m_type); }
    RenderBox* renderer() const { return m_renderer; }

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

    const LayoutRect& frameRect() const { ASSERT(isPlaced()); return m_frameRect; }
    void setFrameRect(const LayoutRect& frameRect) { ASSERT(!isInPlacedTree()); m_frameRect = frameRect; }

    int paginationStrut() const { return m_paginationStrut; }
    void setPaginationStrut(int strut) { m_paginationStrut = strut; }

#ifndef NDEBUG
    bool isInPlacedTree() const { return m_isInPlacedTree; }
    void setIsInPlacedTree(bool value) { m_isInPlacedTree = value; }
#endif

    bool shouldPaint() const { return m_shouldPaint; }
    void setShouldPaint(bool shouldPaint) { m_shouldPaint = shouldPaint; }
    bool isDescendant() const { return m_isDescendant; }
    void setIsDescendant(bool isDescendant) { m_isDescendant = isDescendant; }

    // FIXME: Callers of these methods are dangerous and should be whitelisted explicitly or removed.
    void setRenderer(RenderBox* renderer) { m_renderer = renderer; }
    RootInlineBox* originatingLine() const { return m_originatingLine; }
    void setOriginatingLine(RootInlineBox* line) { m_originatingLine = line; }

    LayoutUnit logicalTop(bool isHorizontalWritingMode) const { return isHorizontalWritingMode ? y() : x(); }
    LayoutUnit logicalBottom(bool isHorizontalWritingMode) const { return isHorizontalWritingMode ? maxY() : maxX(); }
    LayoutUnit logicalLeft(bool isHorizontalWritingMode) const { return isHorizontalWritingMode ? x() : y(); }
    LayoutUnit logicalRight(bool isHorizontalWritingMode) const { return isHorizontalWritingMode ? maxX() : maxY(); }
    LayoutUnit logicalWidth(bool isHorizontalWritingMode) const { return isHorizontalWritingMode ? width() : height(); }

    int pixelSnappedLogicalTop(bool isHorizontalWritingMode) const { return isHorizontalWritingMode ? frameRect().pixelSnappedY() : frameRect().pixelSnappedX(); }
    int pixelSnappedLogicalBottom(bool isHorizontalWritingMode) const { return isHorizontalWritingMode ? frameRect().pixelSnappedMaxY() : frameRect().pixelSnappedMaxX(); }
    int pixelSnappedLogicalLeft(bool isHorizontalWritingMode) const { return isHorizontalWritingMode ? frameRect().pixelSnappedX() : frameRect().pixelSnappedY(); }
    int pixelSnappedLogicalRight(bool isHorizontalWritingMode) const { return isHorizontalWritingMode ? frameRect().pixelSnappedMaxX() : frameRect().pixelSnappedMaxY(); }

    void setLogicalTop(LayoutUnit logicalTop, bool isHorizontalWritingMode)
    {
        if (isHorizontalWritingMode)
            setY(logicalTop);
        else
            setX(logicalTop);
    }
    void setLogicalLeft(LayoutUnit logicalLeft, bool isHorizontalWritingMode)
    {
        if (isHorizontalWritingMode)
            setX(logicalLeft);
        else
            setY(logicalLeft);
    }
    void setLogicalHeight(LayoutUnit logicalHeight, bool isHorizontalWritingMode)
    {
        if (isHorizontalWritingMode)
            setHeight(logicalHeight);
        else
            setWidth(logicalHeight);
    }
    void setLogicalWidth(LayoutUnit logicalWidth, bool isHorizontalWritingMode)
    {
        if (isHorizontalWritingMode)
            setWidth(logicalWidth);
        else
            setHeight(logicalWidth);
    }

private:
    RenderBox* m_renderer;
    RootInlineBox* m_originatingLine;
    LayoutRect m_frameRect;
    int m_paginationStrut; // FIXME: This should be a LayoutUnit, since it's a vertical offset.

    unsigned m_type : 2; // Type (left or right aligned)
    unsigned m_shouldPaint : 1;
    unsigned m_isDescendant : 1;
    unsigned m_isPlaced : 1;
#ifndef NDEBUG
    unsigned m_isInPlacedTree : 1;
#endif
};

struct FloatingObjectHashFunctions {
    static unsigned hash(FloatingObject* key) { return DefaultHash<RenderBox*>::Hash::hash(key->renderer()); }
    static bool equal(FloatingObject* a, FloatingObject* b) { return a->renderer() == b->renderer(); }
    static const bool safeToCompareToEmptyOrDeleted = true;
};
struct FloatingObjectHashTranslator {
    static unsigned hash(RenderBox* key) { return DefaultHash<RenderBox*>::Hash::hash(key); }
    static bool equal(FloatingObject* a, RenderBox* b) { return a->renderer() == b; }
};
typedef ListHashSet<FloatingObject*, 4, FloatingObjectHashFunctions> FloatingObjectSet;
typedef FloatingObjectSet::const_iterator FloatingObjectSetIterator;
typedef PODInterval<int, FloatingObject*> FloatingObjectInterval;
typedef PODIntervalTree<int, FloatingObject*> FloatingObjectTree;
typedef PODFreeListArena<PODRedBlackTree<FloatingObjectInterval>::Node> IntervalArena;


class FloatingObjects {
    WTF_MAKE_NONCOPYABLE(FloatingObjects); WTF_MAKE_FAST_ALLOCATED;
public:
    FloatingObjects(const RenderBlock*, bool horizontalWritingMode);
    ~FloatingObjects();

    void clear();
    void add(FloatingObject*);
    void remove(FloatingObject*);
    void addPlacedObject(FloatingObject*);
    void removePlacedObject(FloatingObject*);
    void setHorizontalWritingMode(bool b = true) { m_horizontalWritingMode = b; }

    bool hasLeftObjects() const { return m_leftObjectsCount > 0; }
    bool hasRightObjects() const { return m_rightObjectsCount > 0; }
    const FloatingObjectSet& set() const { return m_set; }
    void clearLineBoxTreePointers();
    LayoutUnit logicalLeftOffset(LayoutUnit fixedOffset, LayoutUnit logicalTop, LayoutUnit logicalHeight, ShapeOutsideFloatOffsetMode = ShapeOutsideFloatShapeOffset, LayoutUnit* heightRemaining = 0);
    LayoutUnit logicalRightOffset(LayoutUnit fixedOffset, LayoutUnit logicalTop, LayoutUnit logicalHeight, ShapeOutsideFloatOffsetMode = ShapeOutsideFloatShapeOffset, LayoutUnit* heightRemaining = 0);
private:
    void computePlacedFloatsTree();
    const FloatingObjectTree& placedFloatsTree()
    {
        if (!m_placedFloatsTree.isInitialized())
            computePlacedFloatsTree();
        return m_placedFloatsTree;
    }
    void increaseObjectsCount(FloatingObject::Type);
    void decreaseObjectsCount(FloatingObject::Type);
    FloatingObjectInterval intervalForFloatingObject(FloatingObject*);

    FloatingObjectSet m_set;
    FloatingObjectTree m_placedFloatsTree;
    unsigned m_leftObjectsCount;
    unsigned m_rightObjectsCount;
    bool m_horizontalWritingMode;
    const RenderBlock* m_renderer;
};

#ifndef NDEBUG
// These structures are used by PODIntervalTree for debugging purposes.
template <> struct ValueToString<int> {
    static String string(const int value);
};
template<> struct ValueToString<FloatingObject*> {
    static String string(const FloatingObject*);
};
#endif

} // namespace WebCore

#endif // FloatingObjects_h
