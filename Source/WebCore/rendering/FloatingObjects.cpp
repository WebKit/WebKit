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

#include "config.h"
#include "FloatingObjects.h"

#include "RenderBlock.h"
#include "RenderBox.h"
#include "RenderView.h"

using namespace std;
using namespace WTF;

namespace WebCore {

struct SameSizeAsFloatingObject {
    void* pointers[2];
    LayoutRect rect;
    int paginationStrut;
    uint32_t bitfields : 8;
};

COMPILE_ASSERT(sizeof(FloatingObject) == sizeof(SameSizeAsFloatingObject), FloatingObject_should_stay_small);

template <FloatingObject::Type FloatTypeValue>
class ComputeFloatOffsetAdapter {
public:
    typedef FloatingObjectInterval IntervalType;

    ComputeFloatOffsetAdapter(const RenderBlock* renderer, int lineTop, int lineBottom, LayoutUnit& offset)
        : m_renderer(renderer)
        , m_lineTop(lineTop)
        , m_lineBottom(lineBottom)
        , m_offset(offset)
        , m_outermostFloat(0)
    {
    }

    int lowValue() const { return m_lineTop; }
    int highValue() const { return m_lineBottom; }
    void collectIfNeeded(const IntervalType&);

#if ENABLE(CSS_SHAPES)
    // When computing the offset caused by the floats on a given line, if
    // the outermost float on that line has a shape-outside, the inline
    // content that butts up against that float must be positioned using
    // the contours of the shape, not the margin box of the float.
    const FloatingObject* outermostFloat() const { return m_outermostFloat; }
#endif

    LayoutUnit getHeightRemaining() const;

private:
    bool updateOffsetIfNeeded(const FloatingObject*);

    const RenderBlock* m_renderer;
    int m_lineTop;
    int m_lineBottom;
    LayoutUnit& m_offset;
    const FloatingObject* m_outermostFloat;
};

FloatingObjects::FloatingObjects(const RenderBlock* renderer, bool horizontalWritingMode)
    : m_placedFloatsTree(UninitializedTree)
    , m_leftObjectsCount(0)
    , m_rightObjectsCount(0)
    , m_horizontalWritingMode(horizontalWritingMode)
    , m_renderer(renderer)
{
}

FloatingObjects::~FloatingObjects()
{
    // FIXME: m_set should use OwnPtr instead.
    deleteAllValues(m_set);
}

void FloatingObjects::clearLineBoxTreePointers()
{
    // Clear references to originating lines, since the lines are being deleted
    FloatingObjectSetIterator end = m_set.end();
    for (FloatingObjectSetIterator it = m_set.begin(); it != end; ++it) {
        ASSERT(!((*it)->originatingLine()) || &((*it)->originatingLine()->renderer()) == m_renderer);
        (*it)->setOriginatingLine(0);
    }
}

void FloatingObjects::clear()
{
    // FIXME: This should call deleteAllValues, except clearFloats
    // like to play fast and loose with ownership of these pointers.
    // If we move to OwnPtr that will fix this ownership oddness.
    m_set.clear();
    m_placedFloatsTree.clear();
    m_leftObjectsCount = 0;
    m_rightObjectsCount = 0;
}

void FloatingObjects::increaseObjectsCount(FloatingObject::Type type)
{
    if (type == FloatingObject::FloatLeft)
        m_leftObjectsCount++;
    else
        m_rightObjectsCount++;
}

void FloatingObjects::decreaseObjectsCount(FloatingObject::Type type)
{
    if (type == FloatingObject::FloatLeft)
        m_leftObjectsCount--;
    else
        m_rightObjectsCount--;
}

FloatingObjectInterval FloatingObjects::intervalForFloatingObject(FloatingObject* floatingObject)
{
    if (m_horizontalWritingMode)
        return FloatingObjectInterval(floatingObject->frameRect().y(), floatingObject->frameRect().maxY(), floatingObject);
    return FloatingObjectInterval(floatingObject->frameRect().x(), floatingObject->frameRect().maxX(), floatingObject);
}

void FloatingObjects::addPlacedObject(FloatingObject* floatingObject)
{
    ASSERT(!floatingObject->isInPlacedTree());

    floatingObject->setIsPlaced(true);
    if (m_placedFloatsTree.isInitialized())
        m_placedFloatsTree.add(intervalForFloatingObject(floatingObject));

#ifndef NDEBUG
    floatingObject->setIsInPlacedTree(true);
#endif
}

void FloatingObjects::removePlacedObject(FloatingObject* floatingObject)
{
    ASSERT(floatingObject->isPlaced() && floatingObject->isInPlacedTree());

    if (m_placedFloatsTree.isInitialized()) {
        bool removed = m_placedFloatsTree.remove(intervalForFloatingObject(floatingObject));
        ASSERT_UNUSED(removed, removed);
    }

    floatingObject->setIsPlaced(false);
#ifndef NDEBUG
    floatingObject->setIsInPlacedTree(false);
#endif
}

void FloatingObjects::add(FloatingObject* floatingObject)
{
    increaseObjectsCount(floatingObject->type());
    m_set.add(floatingObject);
    if (floatingObject->isPlaced())
        addPlacedObject(floatingObject);
}

void FloatingObjects::remove(FloatingObject* floatingObject)
{
    decreaseObjectsCount(floatingObject->type());
    m_set.remove(floatingObject);
    ASSERT(floatingObject->isPlaced() || !floatingObject->isInPlacedTree());
    if (floatingObject->isPlaced())
        removePlacedObject(floatingObject);
}

void FloatingObjects::computePlacedFloatsTree()
{
    ASSERT(!m_placedFloatsTree.isInitialized());
    if (m_set.isEmpty())
        return;
    m_placedFloatsTree.initIfNeeded(m_renderer->view().intervalArena());
    FloatingObjectSetIterator it = m_set.begin();
    FloatingObjectSetIterator end = m_set.end();
    for (; it != end; ++it) {
        FloatingObject* floatingObject = *it;
        if (floatingObject->isPlaced())
            m_placedFloatsTree.add(intervalForFloatingObject(floatingObject));
    }
}

LayoutUnit FloatingObjects::logicalLeftOffset(LayoutUnit fixedOffset, LayoutUnit logicalTop, LayoutUnit logicalHeight, ShapeOutsideFloatOffsetMode offsetMode, LayoutUnit *heightRemaining)
{
#if !ENABLE(CSS_SHAPES)
    UNUSED_PARAM(offsetMode);
#endif

    LayoutUnit offset = fixedOffset;
    ComputeFloatOffsetAdapter<FloatingObject::FloatLeft> adapter(m_renderer, roundToInt(logicalTop), roundToInt(logicalTop + logicalHeight), offset);
    placedFloatsTree().allOverlapsWithAdapter(adapter);

    if (heightRemaining)
        *heightRemaining = adapter.getHeightRemaining();

#if ENABLE(CSS_SHAPES)
    const FloatingObject* outermostFloat = adapter.outermostFloat();
    if (offsetMode == ShapeOutsideFloatShapeOffset && outermostFloat) {
        if (ShapeOutsideInfo* shapeOutside = outermostFloat->renderer()->shapeOutsideInfo()) {
            shapeOutside->computeSegmentsForContainingBlockLine(logicalTop, outermostFloat->logicalTop(m_horizontalWritingMode), logicalHeight);
            offset += shapeOutside->rightSegmentMarginBoxDelta();
        }
    }
#endif

    return offset;
}

LayoutUnit FloatingObjects::logicalRightOffset(LayoutUnit fixedOffset, LayoutUnit logicalTop, LayoutUnit logicalHeight, ShapeOutsideFloatOffsetMode offsetMode, LayoutUnit *heightRemaining)
{
#if !ENABLE(CSS_SHAPES)
    UNUSED_PARAM(offsetMode);
#endif

    LayoutUnit offset = fixedOffset;
    ComputeFloatOffsetAdapter<FloatingObject::FloatRight> adapter(m_renderer, roundToInt(logicalTop), roundToInt(logicalTop + logicalHeight), offset);
    placedFloatsTree().allOverlapsWithAdapter(adapter);

    if (heightRemaining)
        *heightRemaining = adapter.getHeightRemaining();

#if ENABLE(CSS_SHAPES)
    const FloatingObject* outermostFloat = adapter.outermostFloat();
    if (offsetMode == ShapeOutsideFloatShapeOffset && outermostFloat) {
        if (ShapeOutsideInfo* shapeOutside = outermostFloat->renderer()->shapeOutsideInfo()) {
            shapeOutside->computeSegmentsForContainingBlockLine(logicalTop, outermostFloat->logicalTop(m_horizontalWritingMode), logicalHeight);
            offset += shapeOutside->leftSegmentMarginBoxDelta();
        }
    }
#endif

    return min(fixedOffset, offset);
}

inline static bool rangesIntersect(int floatTop, int floatBottom, int objectTop, int objectBottom)
{
    if (objectTop >= floatBottom || objectBottom < floatTop)
        return false;

    // The top of the object overlaps the float
    if (objectTop >= floatTop)
        return true;

    // The object encloses the float
    if (objectTop < floatTop && objectBottom > floatBottom)
        return true;

    // The bottom of the object overlaps the float
    if (objectBottom > objectTop && objectBottom > floatTop && objectBottom <= floatBottom)
        return true;

    return false;
}

template<>
inline bool ComputeFloatOffsetAdapter<FloatingObject::FloatLeft>::updateOffsetIfNeeded(const FloatingObject* floatingObject)
{
    LayoutUnit logicalRight = floatingObject->logicalRight(m_renderer->isHorizontalWritingMode());
    if (logicalRight > m_offset) {
        m_offset = logicalRight;
        return true;
    }
    return false;
}

template<>
inline bool ComputeFloatOffsetAdapter<FloatingObject::FloatRight>::updateOffsetIfNeeded(const FloatingObject* floatingObject)
{
    LayoutUnit logicalLeft = floatingObject->logicalLeft(m_renderer->isHorizontalWritingMode());
    if (logicalLeft < m_offset) {
        m_offset = logicalLeft;
        return true;
    }
    return false;
}

template <FloatingObject::Type FloatTypeValue>
inline void ComputeFloatOffsetAdapter<FloatTypeValue>::collectIfNeeded(const IntervalType& interval)
{
    const FloatingObject* floatingObject = interval.data();
    if (floatingObject->type() != FloatTypeValue || !rangesIntersect(interval.low(), interval.high(), m_lineTop, m_lineBottom))
        return;

    // All the objects returned from the tree should be already placed.
    ASSERT(floatingObject->isPlaced());
    ASSERT(rangesIntersect(floatingObject->pixelSnappedLogicalTop(m_renderer->isHorizontalWritingMode()), floatingObject->pixelSnappedLogicalBottom(m_renderer->isHorizontalWritingMode()), m_lineTop, m_lineBottom));

    bool floatIsNewExtreme = updateOffsetIfNeeded(floatingObject);
    if (floatIsNewExtreme)
        m_outermostFloat = floatingObject;
}

template <FloatingObject::Type FloatTypeValue>
LayoutUnit ComputeFloatOffsetAdapter<FloatTypeValue>::getHeightRemaining() const
{
    return m_outermostFloat ? m_outermostFloat->logicalBottom(m_renderer->isHorizontalWritingMode()) - m_lineTop : LayoutUnit(1);
}

#ifndef NDEBUG
// These helpers are only used by the PODIntervalTree for debugging purposes.
String ValueToString<int>::string(const int value)
{
    return String::number(value);
}

String ValueToString<FloatingObject*>::string(const FloatingObject* floatingObject)
{
    return String::format("%p (%ix%i %ix%i)", floatingObject, floatingObject->frameRect().x().toInt(), floatingObject->frameRect().y().toInt(), floatingObject->frameRect().maxX().toInt(), floatingObject->frameRect().maxY().toInt());
}
#endif


} // namespace WebCore
