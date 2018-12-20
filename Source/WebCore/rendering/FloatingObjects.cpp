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


namespace WebCore {
using namespace WTF;

struct SameSizeAsFloatingObject {
    void* pointers[2];
    LayoutRect rect;
    int paginationStrut;
    LayoutSize size;
    uint32_t bitfields : 8;
};

COMPILE_ASSERT(sizeof(FloatingObject) == sizeof(SameSizeAsFloatingObject), FloatingObject_should_stay_small);

FloatingObject::FloatingObject(RenderBox& renderer)
    : m_renderer(makeWeakPtr(renderer))
    , m_shouldPaint(true)
    , m_isDescendant(false)
    , m_isPlaced(false)
#ifndef NDEBUG
    , m_isInPlacedTree(false)
#endif
{
    Float type = renderer.style().floating();
    ASSERT(type != Float::No);
    if (type == Float::Left)
        m_type = FloatLeft;
    else if (type == Float::Right)
        m_type = FloatRight;
}

FloatingObject::FloatingObject(RenderBox& renderer, Type type, const LayoutRect& frameRect, const LayoutSize& marginOffset, bool shouldPaint, bool isDescendant)
    : m_renderer(makeWeakPtr(renderer))
    , m_frameRect(frameRect)
    , m_marginOffset(marginOffset)
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
    return std::make_unique<FloatingObject>(renderer(), type(), LayoutRect(frameRect().location() - offset, frameRect().size()), marginOffset(), shouldPaint, isDescendant);
}

std::unique_ptr<FloatingObject> FloatingObject::cloneForNewParent() const
{
    auto cloneObject = std::make_unique<FloatingObject>(renderer(), type(), m_frameRect, m_marginOffset, m_shouldPaint, m_isDescendant);
    cloneObject->m_paginationStrut = m_paginationStrut;
    cloneObject->m_isPlaced = m_isPlaced;
    return cloneObject;
}

LayoutSize FloatingObject::translationOffsetToAncestor() const
{
    return locationOffsetOfBorderBox() - renderer().locationOffset();
}

inline static bool rangesIntersect(LayoutUnit floatTop, LayoutUnit floatBottom, LayoutUnit objectTop, LayoutUnit objectBottom)
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

template <FloatingObject::Type FloatTypeValue>
class ComputeFloatOffsetAdapter {
public:
    typedef FloatingObjectInterval IntervalType;

    ComputeFloatOffsetAdapter(const RenderBlockFlow& renderer, LayoutUnit lineTop, LayoutUnit lineBottom, LayoutUnit offset)
        : m_renderer(makeWeakPtr(renderer))
        , m_lineTop(lineTop)
        , m_lineBottom(lineBottom)
        , m_offset(offset)
        , m_outermostFloat(0)
    {
    }

    virtual ~ComputeFloatOffsetAdapter() = default;

    LayoutUnit lowValue() const { return m_lineTop; }
    LayoutUnit highValue() const { return m_lineBottom; }
    void collectIfNeeded(const IntervalType&);

    LayoutUnit offset() const { return m_offset; }

protected:
    virtual bool updateOffsetIfNeeded(const FloatingObject&) = 0;

    WeakPtr<const RenderBlockFlow> m_renderer;
    LayoutUnit m_lineTop;
    LayoutUnit m_lineBottom;
    LayoutUnit m_offset;
    const FloatingObject* m_outermostFloat;
};

template <FloatingObject::Type FloatTypeValue>
class ComputeFloatOffsetForFloatLayoutAdapter : public ComputeFloatOffsetAdapter<FloatTypeValue> {
public:
    ComputeFloatOffsetForFloatLayoutAdapter(const RenderBlockFlow& renderer, LayoutUnit lineTop, LayoutUnit lineBottom, LayoutUnit offset)
        : ComputeFloatOffsetAdapter<FloatTypeValue>(renderer, lineTop, lineBottom, offset)
    {
    }

    virtual ~ComputeFloatOffsetForFloatLayoutAdapter() = default;

    LayoutUnit heightRemaining() const;

protected:
    bool updateOffsetIfNeeded(const FloatingObject&) final;
};

template <FloatingObject::Type FloatTypeValue>
class ComputeFloatOffsetForLineLayoutAdapter : public ComputeFloatOffsetAdapter<FloatTypeValue> {
public:
    ComputeFloatOffsetForLineLayoutAdapter(const RenderBlockFlow& renderer, LayoutUnit lineTop, LayoutUnit lineBottom, LayoutUnit offset)
        : ComputeFloatOffsetAdapter<FloatTypeValue>(renderer, lineTop, lineBottom, offset)
    {
    }

    virtual ~ComputeFloatOffsetForLineLayoutAdapter() = default;

protected:
    bool updateOffsetIfNeeded(const FloatingObject&) final;
};

class FindNextFloatLogicalBottomAdapter {
public:
    typedef FloatingObjectInterval IntervalType;

    FindNextFloatLogicalBottomAdapter(const RenderBlockFlow& renderer, LayoutUnit belowLogicalHeight)
        : m_renderer(makeWeakPtr(renderer))
        , m_belowLogicalHeight(belowLogicalHeight)
    {
    }

    LayoutUnit lowValue() const { return m_belowLogicalHeight; }
    LayoutUnit highValue() const { return LayoutUnit::max(); }
    void collectIfNeeded(const IntervalType&);

    LayoutUnit nextLogicalBottom() const { return m_nextLogicalBottom.valueOr(0); }
    LayoutUnit nextShapeLogicalBottom() const { return m_nextShapeLogicalBottom.valueOr(nextLogicalBottom()); }

private:
    WeakPtr<const RenderBlockFlow> m_renderer;
    LayoutUnit m_belowLogicalHeight;
    Optional<LayoutUnit> m_nextLogicalBottom;
    Optional<LayoutUnit> m_nextShapeLogicalBottom;
};

inline void FindNextFloatLogicalBottomAdapter::collectIfNeeded(const IntervalType& interval)
{
    const auto& floatingObject = *interval.data();
    if (!rangesIntersect(interval.low(), interval.high(), m_belowLogicalHeight, LayoutUnit::max()))
        return;

    // All the objects returned from the tree should be already placed.
    ASSERT(floatingObject.isPlaced());
    ASSERT(rangesIntersect(m_renderer->logicalTopForFloat(floatingObject), m_renderer->logicalBottomForFloat(floatingObject), m_belowLogicalHeight, LayoutUnit::max()));

    LayoutUnit floatBottom = m_renderer->logicalBottomForFloat(floatingObject);
    if (m_nextLogicalBottom && m_nextLogicalBottom.value() < floatBottom)
        return;

    if (ShapeOutsideInfo* shapeOutside = floatingObject.renderer().shapeOutsideInfo()) {
        LayoutUnit shapeBottom = m_renderer->logicalTopForFloat(floatingObject) + m_renderer->marginBeforeForChild(floatingObject.renderer()) + shapeOutside->shapeLogicalBottom();
        // Use the shapeBottom unless it extends outside of the margin box, in which case it is clipped.
        m_nextShapeLogicalBottom = std::min(shapeBottom, floatBottom);
    } else
        m_nextShapeLogicalBottom = floatBottom;
    m_nextLogicalBottom = floatBottom;
}

LayoutUnit FloatingObjects::findNextFloatLogicalBottomBelow(LayoutUnit logicalHeight)
{
    FindNextFloatLogicalBottomAdapter adapter(renderer(), logicalHeight);
    if (const FloatingObjectTree* placedFloatsTree = this->placedFloatsTree())
        placedFloatsTree->allOverlapsWithAdapter(adapter);

    return adapter.nextShapeLogicalBottom();
}

LayoutUnit FloatingObjects::findNextFloatLogicalBottomBelowForBlock(LayoutUnit logicalHeight)
{
    FindNextFloatLogicalBottomAdapter adapter(renderer(), logicalHeight);
    if (const FloatingObjectTree* placedFloatsTree = this->placedFloatsTree())
        placedFloatsTree->allOverlapsWithAdapter(adapter);

    return adapter.nextLogicalBottom();
}

FloatingObjects::FloatingObjects(const RenderBlockFlow& renderer)
    : m_leftObjectsCount(0)
    , m_rightObjectsCount(0)
    , m_horizontalWritingMode(renderer.isHorizontalWritingMode())
    , m_renderer(makeWeakPtr(renderer))
{
}

FloatingObjects::~FloatingObjects() = default;

void FloatingObjects::clearLineBoxTreePointers()
{
    // Clear references to originating lines, since the lines are being deleted
    for (auto it = m_set.begin(), end = m_set.end(); it != end; ++it) {
        ASSERT(!((*it)->originatingLine()) || &((*it)->originatingLine()->renderer()) == &renderer());
        (*it)->clearOriginatingLine();
    }
}

void FloatingObjects::clear()
{
    m_set.clear();
    m_placedFloatsTree = nullptr;
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
        map.add(&renderer, WTFMove(*it));
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
    // FIXME The endpoints of the floating object interval shouldn't need to be
    // floored. See http://wkb.ug/125831 for more details.
    if (m_horizontalWritingMode)
        return FloatingObjectInterval(floatingObject->frameRect().y().floor(), floatingObject->frameRect().maxY().floor(), floatingObject);
    return FloatingObjectInterval(floatingObject->frameRect().x().floor(), floatingObject->frameRect().maxX().floor(), floatingObject);
}

void FloatingObjects::addPlacedObject(FloatingObject* floatingObject)
{
    ASSERT(!floatingObject->isInPlacedTree());

    floatingObject->setIsPlaced(true);
    if (m_placedFloatsTree)
        m_placedFloatsTree->add(intervalForFloatingObject(floatingObject));

#ifndef NDEBUG
    floatingObject->setIsInPlacedTree(true);
#endif
}

void FloatingObjects::removePlacedObject(FloatingObject* floatingObject)
{
    ASSERT(floatingObject->isPlaced() && floatingObject->isInPlacedTree());

    if (m_placedFloatsTree) {
        bool removed = m_placedFloatsTree->remove(intervalForFloatingObject(floatingObject));
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
    return m_set.add(WTFMove(floatingObject)).iterator->get();
}

void FloatingObjects::remove(FloatingObject* floatingObject)
{
    ASSERT((m_set.contains(floatingObject)));
    decreaseObjectsCount(floatingObject->type());
    ASSERT(floatingObject->isPlaced() || !floatingObject->isInPlacedTree());
    if (floatingObject->isPlaced())
        removePlacedObject(floatingObject);
    ASSERT(!floatingObject->originatingLine());
    m_set.remove(floatingObject);
}

void FloatingObjects::computePlacedFloatsTree()
{
    ASSERT(!m_placedFloatsTree);
    if (m_set.isEmpty())
        return;

    m_placedFloatsTree = std::make_unique<FloatingObjectTree>();
    for (auto it = m_set.begin(), end = m_set.end(); it != end; ++it) {
        FloatingObject* floatingObject = it->get();
        if (floatingObject->isPlaced())
            m_placedFloatsTree->add(intervalForFloatingObject(floatingObject));
    }
}

inline const FloatingObjectTree* FloatingObjects::placedFloatsTree()
{
    if (!m_placedFloatsTree)
        computePlacedFloatsTree();
    return m_placedFloatsTree.get();
}

LayoutUnit FloatingObjects::logicalLeftOffsetForPositioningFloat(LayoutUnit fixedOffset, LayoutUnit logicalTop, LayoutUnit *heightRemaining)
{
    ComputeFloatOffsetForFloatLayoutAdapter<FloatingObject::FloatLeft> adapter(renderer(), logicalTop, logicalTop, fixedOffset);
    if (const FloatingObjectTree* placedFloatsTree = this->placedFloatsTree())
        placedFloatsTree->allOverlapsWithAdapter(adapter);

    if (heightRemaining)
        *heightRemaining = adapter.heightRemaining();

    return adapter.offset();
}

LayoutUnit FloatingObjects::logicalRightOffsetForPositioningFloat(LayoutUnit fixedOffset, LayoutUnit logicalTop, LayoutUnit *heightRemaining)
{
    ComputeFloatOffsetForFloatLayoutAdapter<FloatingObject::FloatRight> adapter(renderer(), logicalTop, logicalTop, fixedOffset);
    if (const FloatingObjectTree* placedFloatsTree = this->placedFloatsTree())
        placedFloatsTree->allOverlapsWithAdapter(adapter);

    if (heightRemaining)
        *heightRemaining = adapter.heightRemaining();

    return std::min(fixedOffset, adapter.offset());
}

LayoutUnit FloatingObjects::logicalLeftOffset(LayoutUnit fixedOffset, LayoutUnit logicalTop, LayoutUnit logicalHeight)
{
    ComputeFloatOffsetForLineLayoutAdapter<FloatingObject::FloatLeft> adapter(renderer(), logicalTop, logicalTop + logicalHeight, fixedOffset);
    if (const FloatingObjectTree* placedFloatsTree = this->placedFloatsTree())
        placedFloatsTree->allOverlapsWithAdapter(adapter);

    return adapter.offset();
}

LayoutUnit FloatingObjects::logicalRightOffset(LayoutUnit fixedOffset, LayoutUnit logicalTop, LayoutUnit logicalHeight)
{
    ComputeFloatOffsetForLineLayoutAdapter<FloatingObject::FloatRight> adapter(renderer(), logicalTop, logicalTop + logicalHeight, fixedOffset);
    if (const FloatingObjectTree* placedFloatsTree = this->placedFloatsTree())
        placedFloatsTree->allOverlapsWithAdapter(adapter);

    return std::min(fixedOffset, adapter.offset());
}

template<>
inline bool ComputeFloatOffsetForFloatLayoutAdapter<FloatingObject::FloatLeft>::updateOffsetIfNeeded(const FloatingObject& floatingObject)
{
    LayoutUnit logicalRight = m_renderer->logicalRightForFloat(floatingObject);
    if (logicalRight > m_offset) {
        m_offset = logicalRight;
        return true;
    }
    return false;
}

template<>
inline bool ComputeFloatOffsetForFloatLayoutAdapter<FloatingObject::FloatRight>::updateOffsetIfNeeded(const FloatingObject& floatingObject)
{
    LayoutUnit logicalLeft = m_renderer->logicalLeftForFloat(floatingObject);
    if (logicalLeft < m_offset) {
        m_offset = logicalLeft;
        return true;
    }
    return false;
}

template <FloatingObject::Type FloatTypeValue>
LayoutUnit ComputeFloatOffsetForFloatLayoutAdapter<FloatTypeValue>::heightRemaining() const
{
    return this->m_outermostFloat ? this->m_renderer->logicalBottomForFloat(*this->m_outermostFloat) - this->m_lineTop : 1_lu;
}

template <FloatingObject::Type FloatTypeValue>
inline void ComputeFloatOffsetAdapter<FloatTypeValue>::collectIfNeeded(const IntervalType& interval)
{
    const auto& floatingObject = *interval.data();
    if (floatingObject.type() != FloatTypeValue || !rangesIntersect(interval.low(), interval.high(), m_lineTop, m_lineBottom))
        return;

    // All the objects returned from the tree should be already placed.
    ASSERT(floatingObject.isPlaced());
    ASSERT(rangesIntersect(m_renderer->logicalTopForFloat(floatingObject), m_renderer->logicalBottomForFloat(floatingObject), m_lineTop, m_lineBottom));

    bool floatIsNewExtreme = updateOffsetIfNeeded(floatingObject);
    if (floatIsNewExtreme)
        m_outermostFloat = &floatingObject;
}

template<>
inline bool ComputeFloatOffsetForLineLayoutAdapter<FloatingObject::FloatLeft>::updateOffsetIfNeeded(const FloatingObject& floatingObject)
{
    LayoutUnit logicalRight = m_renderer->logicalRightForFloat(floatingObject);
    if (ShapeOutsideInfo* shapeOutside = floatingObject.renderer().shapeOutsideInfo()) {
        ShapeOutsideDeltas shapeDeltas = shapeOutside->computeDeltasForContainingBlockLine(*m_renderer, floatingObject, m_lineTop, m_lineBottom - m_lineTop);
        if (!shapeDeltas.lineOverlapsShape())
            return false;

        logicalRight += shapeDeltas.rightMarginBoxDelta();
    }
    if (logicalRight > m_offset) {
        m_offset = logicalRight;
        return true;
    }

    return false;
}

template<>
inline bool ComputeFloatOffsetForLineLayoutAdapter<FloatingObject::FloatRight>::updateOffsetIfNeeded(const FloatingObject& floatingObject)
{
    LayoutUnit logicalLeft = m_renderer->logicalLeftForFloat(floatingObject);
    if (ShapeOutsideInfo* shapeOutside = floatingObject.renderer().shapeOutsideInfo()) {
        ShapeOutsideDeltas shapeDeltas = shapeOutside->computeDeltasForContainingBlockLine(*m_renderer, floatingObject, m_lineTop, m_lineBottom - m_lineTop);
        if (!shapeDeltas.lineOverlapsShape())
            return false;

        logicalLeft += shapeDeltas.leftMarginBoxDelta();
    }
    if (logicalLeft < m_offset) {
        m_offset = logicalLeft;
        return true;
    }

    return false;
}

} // namespace WebCore
