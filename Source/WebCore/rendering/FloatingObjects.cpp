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

#include "RenderBlockFlow.h"
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

FloatingObject::FloatingObject(RenderBox& renderer)
    : m_renderer(renderer)
    , m_originatingLine(nullptr)
    , m_paginationStrut(0)
    , m_shouldPaint(true)
    , m_isDescendant(false)
    , m_isPlaced(false)
#ifndef NDEBUG
    , m_isInPlacedTree(false)
#endif
{
    EFloat type = renderer.style()->floating();
    ASSERT(type != NoFloat);
    if (type == LeftFloat)
        m_type = FloatLeft;
    else if (type == RightFloat)
        m_type = FloatRight;
}

FloatingObject::FloatingObject(RenderBox& renderer, Type type, const LayoutRect& frameRect, bool shouldPaint, bool isDescendant)
    : m_renderer(renderer)
    , m_originatingLine(nullptr)
    , m_frameRect(frameRect)
    , m_paginationStrut(0)
    , m_type(type)
    , m_shouldPaint(shouldPaint)
    , m_isDescendant(isDescendant)
    , m_isPlaced(true)
#ifndef NDEBUG
    , m_isInPlacedTree(false)
#endif
{
}

std::unique_ptr<FloatingObject> FloatingObject::create(RenderBox& renderer)
{
    auto object = std::make_unique<FloatingObject>(renderer);
    object->setShouldPaint(!renderer.hasSelfPaintingLayer()); // If a layer exists, the float will paint itself. Otherwise someone else will.
    object->setIsDescendant(true);
    return object;
}

std::unique_ptr<FloatingObject> FloatingObject::copyToNewContainer(LayoutSize offset, bool shouldPaint, bool isDescendant) const
{
    // FIXME: Use make_unique here, once we can get it to compile on all platforms we support.
    return std::unique_ptr<FloatingObject>(new FloatingObject(renderer(), type(), LayoutRect(frameRect().location() - offset, frameRect().size()), shouldPaint, isDescendant));
}

std::unique_ptr<FloatingObject> FloatingObject::unsafeClone() const
{
    // FIXME: Use make_unique here, once we can get it to compile on all platforms we support.
    std::unique_ptr<FloatingObject> cloneObject(new FloatingObject(renderer(), type(), m_frameRect, m_shouldPaint, m_isDescendant));
    cloneObject->m_originatingLine = m_originatingLine;
    cloneObject->m_paginationStrut = m_paginationStrut;
    cloneObject->m_isPlaced = m_isPlaced;
    return cloneObject;
}

template <FloatingObject::Type FloatTypeValue>
class ComputeFloatOffsetAdapter {
public:
    typedef FloatingObjectInterval IntervalType;

    ComputeFloatOffsetAdapter(const RenderBlockFlow* renderer, int lineTop, int lineBottom, LayoutUnit& offset)
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

    const RenderBlockFlow* m_renderer;
    int m_lineTop;
    int m_lineBottom;
    LayoutUnit& m_offset;
    const FloatingObject* m_outermostFloat;
};

FloatingObjects::FloatingObjects(const RenderBlockFlow* renderer, bool horizontalWritingMode)
    : m_placedFloatsTree(UninitializedTree)
    , m_leftObjectsCount(0)
    , m_rightObjectsCount(0)
    , m_horizontalWritingMode(horizontalWritingMode)
    , m_renderer(renderer)
{
}

FloatingObjects::~FloatingObjects()
{
}

void FloatingObjects::clearLineBoxTreePointers()
{
    // Clear references to originating lines, since the lines are being deleted
    for (auto it = m_set.begin(), end = m_set.end(); it != end; ++it) {
        ASSERT(!((*it)->originatingLine()) || &((*it)->originatingLine()->renderer()) == m_renderer);
        (*it)->setOriginatingLine(0);
    }
}

void FloatingObjects::clear()
{
    m_set.clear();
    m_placedFloatsTree.clear();
    m_leftObjectsCount = 0;
    m_rightObjectsCount = 0;
}

void FloatingObjects::moveAllToFloatInfoMap(RendererToFloatInfoMap& map)
{
    for (auto it = m_set.begin(), end = m_set.end(); it != end; ++it) {
        auto& renderer = it->get()->renderer();
        // FIXME: The only reason it is safe to move these out of the set is that
        // we are about to clear it. Otherwise it would break the hash table invariant.
        // A clean way to do this would be to add a takeAll function to HashSet.
        map.add(&renderer, std::move(*it));
    }
    clear();
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

FloatingObject* FloatingObjects::add(std::unique_ptr<FloatingObject> floatingObject)
{
    increaseObjectsCount(floatingObject->type());
    if (floatingObject->isPlaced())
        addPlacedObject(floatingObject.get());
    return m_set.add(std::move(floatingObject)).iterator->get();
}

void FloatingObjects::remove(FloatingObject* floatingObject)
{
    ASSERT((m_set.contains<FloatingObject&, FloatingObjectHashTranslator>(*floatingObject)));
    decreaseObjectsCount(floatingObject->type());
    ASSERT(floatingObject->isPlaced() || !floatingObject->isInPlacedTree());
    if (floatingObject->isPlaced())
        removePlacedObject(floatingObject);
    ASSERT(!floatingObject->originatingLine());
    auto it = m_set.find<FloatingObject&, FloatingObjectHashTranslator>(*floatingObject);
    if (it == m_set.end())
        return;
    m_set.remove(it);
}

void FloatingObjects::computePlacedFloatsTree()
{
    ASSERT(!m_placedFloatsTree.isInitialized());
    if (m_set.isEmpty())
        return;
    m_placedFloatsTree.initIfNeeded(m_renderer->view().intervalArena());
    for (auto it = m_set.begin(), end = m_set.end(); it != end; ++it) {
        FloatingObject* floatingObject = it->get();
        if (floatingObject->isPlaced())
            m_placedFloatsTree.add(intervalForFloatingObject(floatingObject));
    }
}

inline const FloatingObjectTree& FloatingObjects::placedFloatsTree()
{
    if (!m_placedFloatsTree.isInitialized())
        computePlacedFloatsTree();
    return m_placedFloatsTree;
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
        if (ShapeOutsideInfo* shapeOutside = outermostFloat->renderer().shapeOutsideInfo()) {
            shapeOutside->updateDeltasForContainingBlockLine(m_renderer, outermostFloat, logicalTop, logicalHeight);
            offset += shapeOutside->rightMarginBoxDelta();
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
        if (ShapeOutsideInfo* shapeOutside = outermostFloat->renderer().shapeOutsideInfo()) {
            shapeOutside->updateDeltasForContainingBlockLine(m_renderer, outermostFloat, logicalTop, logicalHeight);
            offset += shapeOutside->leftMarginBoxDelta();
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
    LayoutUnit logicalRight = m_renderer->logicalRightForFloat(floatingObject);
    if (logicalRight > m_offset) {
        m_offset = logicalRight;
        return true;
    }
    return false;
}

template<>
inline bool ComputeFloatOffsetAdapter<FloatingObject::FloatRight>::updateOffsetIfNeeded(const FloatingObject* floatingObject)
{
    LayoutUnit logicalLeft = m_renderer->logicalLeftForFloat(floatingObject);
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
    ASSERT(rangesIntersect(m_renderer->pixelSnappedLogicalTopForFloat(floatingObject), m_renderer->pixelSnappedLogicalBottomForFloat(floatingObject), m_lineTop, m_lineBottom));

    bool floatIsNewExtreme = updateOffsetIfNeeded(floatingObject);
    if (floatIsNewExtreme)
        m_outermostFloat = floatingObject;
}

template <FloatingObject::Type FloatTypeValue>
LayoutUnit ComputeFloatOffsetAdapter<FloatTypeValue>::getHeightRemaining() const
{
    return m_outermostFloat ? m_renderer->logicalBottomForFloat(m_outermostFloat) - m_lineTop : LayoutUnit(1);
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
