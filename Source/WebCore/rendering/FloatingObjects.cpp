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

#include "config.h"
#include "FloatingObjects.h"

#include "FloatingContext.h"

#include "PODIntervalTree.h"
#include "PlacedFloats.h"
#include "RenderBlockFlowInlines.h"
#include "RenderBlockInlines.h"
#include "RenderBox.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderView.h"
#include <wtf/HexNumber.h>
#include <wtf/TZoneMallocInlines.h>


namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FloatingObject);
WTF_MAKE_TZONE_ALLOCATED_IMPL(FloatingObjects);


struct SameSizeAsFloatingObject {
    SingleThreadWeakPtr<RenderBox> renderer;
    LayoutRect rect;
    LayoutSize size;
    uint32_t bitfields : 8;
};

static_assert(sizeof(FloatingObject) == sizeof(SameSizeAsFloatingObject), "FloatingObject should stay small");
#if !ASSERT_ENABLED && ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
static_assert(sizeof(SingleThreadWeakPtr<RenderBox>) == sizeof(void*), "WeakPtr should be same size as raw pointer");
#endif

FloatingObject::FloatingObject(RenderBox& renderer)
    : m_renderer(renderer)
{
    UsedFloat type = renderer.usedStyle().floating();
    ASSERT(type != UsedFloat::None);
    if (type == UsedFloat::Left)
        m_type = FloatLeft;
    else if (type == UsedFloat::Right)
        m_type = FloatRight;
}

FloatingObject::FloatingObject(RenderBox& renderer, Type type, const LayoutRect& frameRect, const LayoutSize& marginOffset, bool isDescendant)
    : m_renderer(renderer)
    , m_frameRect(frameRect)
    , m_marginOffset(marginOffset)
    , m_type(type)
    , m_isDescendant(isDescendant)
    , m_isPlaced(true)
{
}

std::unique_ptr<FloatingObject> FloatingObject::create(RenderBox& renderer)
{
    auto object = makeUnique<FloatingObject>(renderer);
    object->setIsDescendant(true);
    return object;
}

std::unique_ptr<FloatingObject> FloatingObject::copyToNewContainer(LayoutSize offset, bool isDescendant) const
{
    ASSERT(renderer());
    return makeUnique<FloatingObject>(*renderer(), type(), LayoutRect(frameRect().location() - offset, frameRect().size()), marginOffset(), isDescendant);
}

std::unique_ptr<FloatingObject> FloatingObject::cloneForNewParent() const
{
    ASSERT(renderer());
    auto cloneObject = makeUnique<FloatingObject>(*renderer(), type(), m_frameRect, m_marginOffset, m_isDescendant);
    cloneObject->m_isPlaced = m_isPlaced;
    return cloneObject;
}

#if ENABLE(TREE_DEBUGGING)

TextStream& operator<<(TextStream& stream, const FloatingObject& object)
{
    stream << "(" << &object << ") renderer (";
    object.hasRenderer() ? stream  << object.renderer() << ")" : stream << "destroyed)";

    if (object.isPlaced())
        stream << " " << object.frameRect();
    else
        stream << " (not placed yet)";
    return stream << " isDescendant " << object.isDescendant();
}

#endif

NODELETE inline static bool rangesIntersect(LayoutUnit floatTop, LayoutUnit floatBottom, LayoutUnit objectTop, LayoutUnit objectBottom)
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
        : m_renderer(renderer)
        , m_lineTop(lineTop)
        , m_lineBottom(lineBottom)
        , m_offset(offset)
        , m_outermostFloat(0)
    {
    }

    virtual ~ComputeFloatOffsetAdapter() = default;

    NODELETE LayoutUnit lowValue() const { return m_lineTop; }
    NODELETE LayoutUnit highValue() const { return m_lineBottom; }
    void collectIfNeeded(const IntervalType&);

    NODELETE LayoutUnit offset() const { return m_offset; }

protected:
    virtual bool updateOffsetIfNeeded(const FloatingObject&) = 0;

    SingleThreadWeakPtr<const RenderBlockFlow> m_renderer;
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

    NODELETE LayoutUnit heightRemaining() const;

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

    NODELETE LayoutUnit lowValue() const { return LayoutUnit { this->m_lineTop.floor() }; }
    void collectIfNeeded(const FloatingObjectInterval&);

protected:
    bool updateOffsetIfNeeded(const FloatingObject&) final;
};

static Layout::PlacedFloats::Item placedFloatItem(const RenderBlockFlow& block, const FloatingObject& floatingObject)
{
    CheckedRef floatBox = *floatingObject.renderer();
    auto isHorizontalWritingMode = block.isHorizontalWritingMode();
    auto isLeftToRight = block.writingMode().isLogicalLeftInlineStart();

    auto marginBoxLogicalLeft = block.logicalLeftForFloat(floatingObject);
    auto marginBoxLogicalWidth = block.logicalWidthForFloat(floatingObject);
    auto marginBoxLogicalHeight = block.logicalHeightForFloat(floatingObject);
    auto borderBoxLogicalWidth = block.logicalWidthForChild(floatBox);
    auto borderBoxLogicalHeight = block.logicalHeightForChild(floatBox);

    auto marginOffset = floatingObject.marginOffset();
    auto marginOnLogicalLeft = isHorizontalWritingMode ? marginOffset.width() : marginOffset.height();
    auto marginOnLogicalRight = marginBoxLogicalWidth - marginOnLogicalLeft - borderBoxLogicalWidth;
    auto marginStart = isLeftToRight ? marginOnLogicalLeft : marginOnLogicalRight;
    auto marginEnd = isLeftToRight ? marginOnLogicalRight : marginOnLogicalLeft;
    auto marginBefore = isHorizontalWritingMode ? marginOffset.height() : marginOffset.width();

    auto marginBoxInlineStart = isLeftToRight ? marginBoxLogicalLeft : block.logicalWidth() - (marginBoxLogicalLeft + marginBoxLogicalWidth);
    auto borderBoxTopLeft = LayoutPoint { marginBoxInlineStart + marginStart, block.logicalTopForFloat(floatingObject) + marginBefore };

    auto boxGeometry = Layout::BoxGeometry { };
    boxGeometry.setTopLeft(borderBoxTopLeft);
    boxGeometry.setContentBoxWidth(borderBoxLogicalWidth);
    boxGeometry.setContentBoxHeight(borderBoxLogicalHeight);
    boxGeometry.setBorder({ });
    boxGeometry.setPadding({ });
    boxGeometry.setHorizontalMargin({ marginStart, marginEnd });
    boxGeometry.setVerticalMargin({ marginBefore, marginBoxLogicalHeight - marginBefore - borderBoxLogicalHeight });

    auto position = (floatBox->usedStyle().floating() == UsedFloat::Left) == isLeftToRight ? Layout::PlacedFloats::Item::Position::Start : Layout::PlacedFloats::Item::Position::End;
    auto marginBoxRect = Layout::Rect { block.logicalTopForFloat(floatingObject), marginBoxInlineStart, marginBoxLogicalWidth, marginBoxLogicalHeight };
    return { floatBox->layoutBox(), position, marginBoxRect, boxGeometry, borderBoxTopLeft };
}

FloatingObjects::FloatingObjects(const RenderBlockFlow& renderer)
    : m_horizontalWritingMode(renderer.isHorizontalWritingMode())
    , m_renderer(renderer)
{
}

FloatingObjects::~FloatingObjects() = default;

void FloatingObjects::clear()
{
    m_set.clear();
    m_placedFloatsTree = nullptr;
    m_floatingContext = nullptr;
    m_placedFloats = nullptr;
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
    // FIXME: The endpoints of the floating object interval shouldn't need to be
    // floored. See <https://webkit.org/b/125831> for more details.
    if (m_horizontalWritingMode)
        return FloatingObjectInterval(floatingObject->frameRect().y().floor(), floatingObject->frameRect().maxY().floor(), floatingObject);
    return FloatingObjectInterval(floatingObject->frameRect().x().floor(), floatingObject->frameRect().maxX().floor(), floatingObject);
}

void FloatingObjects::addPlacedObject(FloatingObject* floatingObject)
{
    ASSERT(!floatingObject->isInPlacedTree());

    floatingObject->setIsPlaced(true);
    if (m_placedFloatsTree) {
        m_placedFloatsTree->add(intervalForFloatingObject(floatingObject));
#if ASSERT_ENABLED
        floatingObject->setIsInPlacedTree(true);
#endif
    }
    if (m_placedFloats && floatingObject->renderer()) {
        m_placedFloats->add(placedFloatItem(renderer(), *floatingObject));
        // FloatingContext::constraints() skips the floats it found ending above its last position, and a float inserted among them would be skipped too.
        if (m_floatingContext && &m_placedFloats->list().last().layoutBox() != &floatingObject->renderer()->layoutBox())
            m_floatingContext = nullptr;
    }
}

void FloatingObjects::removePlacedObject(FloatingObject* floatingObject)
{
    ASSERT(floatingObject->isPlaced());

    if (m_placedFloatsTree) {
        auto removed = m_placedFloatsTree->remove(intervalForFloatingObject(floatingObject));
        ASSERT_UNUSED(removed, removed);
#if ASSERT_ENABLED
        floatingObject->setIsInPlacedTree(false);
#endif
    }
    if (m_placedFloats && floatingObject->renderer()) {
        m_placedFloats->remove(floatingObject->renderer()->layoutBox());
        m_floatingContext = nullptr;
    }
    floatingObject->setIsPlaced(false);
}

void FloatingObjects::place(FloatingObject& floatingObject, const LayoutRect& frameRect, LayoutSize marginOffset)
{
    // The float may already be in m_placedFloatsTree, indexed on the rect it had then. Take it out before
    // changing the rect, so the entry is keyed on the rect the float actually has.
    if (floatingObject.isPlaced() && m_placedFloatsTree) {
        auto removed = m_placedFloatsTree->remove(intervalForFloatingObject(&floatingObject));
        ASSERT_UNUSED(removed, removed);
#if ASSERT_ENABLED
        floatingObject.setIsInPlacedTree(false);
#endif
    }

    floatingObject.setFrameRect(frameRect);
    floatingObject.setMarginOffset(marginOffset);
    floatingObject.setIsPlaced(true);
    // Inline layout may have put the float anywhere in the list (see PlacedFloats::add()), where FloatingContext::constraints() may skip it.
    m_floatingContext = nullptr;
    if (m_placedFloatsTree) {
        m_placedFloatsTree->add(intervalForFloatingObject(&floatingObject));
#if ASSERT_ENABLED
        floatingObject.setIsInPlacedTree(true);
#endif
    }
}

FloatingObject* FloatingObjects::add(std::unique_ptr<FloatingObject> floatingObject)
{
    increaseObjectsCount(floatingObject->type());
    if (floatingObject->isPlaced())
        addPlacedObject(floatingObject.get());
    return m_set.add(WTF::move(floatingObject)).iterator->get();
}

void FloatingObjects::remove(FloatingObject* floatingObject)
{
    ASSERT((m_set.contains(floatingObject)));
    decreaseObjectsCount(floatingObject->type());
    ASSERT(floatingObject->isPlaced() || !floatingObject->isInPlacedTree());
    if (floatingObject->isPlaced())
        removePlacedObject(floatingObject);
    m_set.remove(floatingObject);
}

void FloatingObjects::computePlacedFloatsTree()
{
    ASSERT(!m_placedFloatsTree);
    if (m_set.isEmpty())
        return;

    m_placedFloatsTree = makeUnique<FloatingObjectTree>();
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

std::unique_ptr<Layout::PlacedFloats> FloatingObjects::makePlacedFloats() const
{
    auto placedFloats = makeUnique<Layout::PlacedFloats>(renderer().layoutBox());
    for (auto& floatingObject : m_set) {
        if (floatingObject->isPlaced() && floatingObject->renderer())
            placedFloats->add(placedFloatItem(renderer(), *floatingObject));
    }
    return placedFloats;
}

Layout::PlacedFloats& FloatingObjects::placedFloats()
{
    if (!m_placedFloats)
        m_placedFloats = makePlacedFloats();
    return *m_placedFloats;
}

const Layout::FloatingContext& FloatingObjects::floatingContext()
{
    if (!m_floatingContext)
        m_floatingContext = makeUnique<Layout::FloatingContext>(renderer().layoutBox(), placedFloats(), Layout::Point<Layout::ParentFormattingContextRoot> { }, renderer().logicalWidth());
    return *m_floatingContext;
}

void FloatingObjects::clearFloatingContext()
{
    m_floatingContext = nullptr;
}

void FloatingObjects::clearPlacedFloats()
{
    m_floatingContext = nullptr;
    m_placedFloats = nullptr;
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

void FloatingObjects::shiftFloatsBy(LayoutUnit blockShift)
{
    auto shiftX = (m_horizontalWritingMode) ? 0_lu : -blockShift;
    auto shiftY = (m_horizontalWritingMode) ? blockShift : 0_lu;

    for (auto& floatBox : m_set) {
        if (!floatBox->renderer())
            continue;

        auto isPlaced = floatBox->isPlaced();
        if (isPlaced)
            removePlacedObject(floatBox.get());

        floatBox->m_frameRect.move(shiftX, shiftY);
        // Only a float this container lays out moves with this container's content. Anything else in the list is a copy of an entry in another box's list.
        if (floatBox->renderer()->containingBlock() == &renderer())
            floatBox->renderer()->move(shiftX, shiftY);

        if (isPlaced)
            addPlacedObject(floatBox.get());
    }
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
    if (floatingObject.type() != FloatTypeValue || !floatingObject.height() || !rangesIntersect(interval.low(), interval.high(), m_lineTop, m_lineBottom))
        return;

    // All the objects returned from the tree should be already placed.
    ASSERT(floatingObject.isPlaced());
    // FIXME: Remove floor(). See <https://webkit.org/b/125831>.
    ASSERT(rangesIntersect(m_renderer->logicalTopForFloat(floatingObject).floor(), m_renderer->logicalBottomForFloat(floatingObject).floor(), m_lineTop, m_lineBottom));

    bool floatIsNewExtreme = updateOffsetIfNeeded(floatingObject);
    if (floatIsNewExtreme)
        m_outermostFloat = &floatingObject;
}

template <FloatingObject::Type FloatTypeValue>
inline void ComputeFloatOffsetForLineLayoutAdapter<FloatTypeValue>::collectIfNeeded(const FloatingObjectInterval& interval)
{
    const auto& floatingObject = *interval.data();
    if (floatingObject.type() != FloatTypeValue || !rangesIntersect(this->m_renderer->logicalTopForFloat(floatingObject), this->m_renderer->logicalBottomForFloat(floatingObject), this->m_lineTop, this->m_lineBottom))
        return;

    // All the objects returned from the tree should be already placed.
    ASSERT(floatingObject.isPlaced());

    if (updateOffsetIfNeeded(floatingObject))
        this->m_outermostFloat = &floatingObject;
}

// There is nothing to overlap in an empty margin box, so a float with one takes no room (see FloatingContext's floatContainsLine()).
// FIXME: The frame rect is not always the margin box: its block size also covers the pagination strut and the initial-letter
// sink (see RenderBlockFlow::positionNewFloats() and adjustInitialLetterPosition()).
static bool hasEmptyMarginBox(const RenderBlockFlow& block, const FloatingObject& floatingObject)
{
    return block.logicalWidthForFloat(floatingObject) <= 0 || block.logicalHeightForFloat(floatingObject) <= 0;
}

template<>
inline bool ComputeFloatOffsetForLineLayoutAdapter<FloatingObject::FloatLeft>::updateOffsetIfNeeded(const FloatingObject& floatingObject)
{
    if (!floatingObject.renderer() || hasEmptyMarginBox(*m_renderer, floatingObject))
        return false;

    LayoutUnit logicalRight = m_renderer->logicalRightForFloat(floatingObject);
    if (ShapeOutsideInfo* shapeOutside = floatingObject.renderer()->shapeOutsideInfo()) {
        ShapeOutsideDeltas shapeDeltas = shapeOutside->computeDeltasForContainingBlockLine(*m_renderer, floatingObject, m_lineTop, m_lineBottom - m_lineTop);
        if (!shapeDeltas.isValid() || !shapeDeltas.lineOverlapsShape())
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
    if (!floatingObject.renderer() || hasEmptyMarginBox(*m_renderer, floatingObject))
        return false;

    LayoutUnit logicalLeft = m_renderer->logicalLeftForFloat(floatingObject);
    if (ShapeOutsideInfo* shapeOutside = floatingObject.renderer()->shapeOutsideInfo()) {
        ShapeOutsideDeltas shapeDeltas = shapeOutside->computeDeltasForContainingBlockLine(*m_renderer, floatingObject, m_lineTop, m_lineBottom - m_lineTop);
        if (!shapeDeltas.isValid() || !shapeDeltas.lineOverlapsShape())
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
