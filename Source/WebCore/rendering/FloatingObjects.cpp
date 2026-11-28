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
    return makeUnique<FloatingObject>(*renderer(), type(), m_frameRect, m_marginOffset, m_isDescendant);
}

#if ENABLE(TREE_DEBUGGING)

TextStream& operator<<(TextStream& stream, const FloatingObject& object)
{
    stream << "(" << &object << ") renderer (";
    object.hasRenderer() ? stream  << object.renderer() << ")" : stream << "destroyed)";

    return stream << " " << object.frameRect() << " isDescendant " << object.isDescendant();
}

#endif

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

void FloatingObjects::addPlacedObject(FloatingObject* floatingObject)
{
    if (m_placedFloats && floatingObject->renderer()) {
        m_placedFloats->add(placedFloatItem(renderer(), *floatingObject));
        // FloatingContext::constraints() skips the floats it found ending above its last position, and a float inserted among them would be skipped too.
        if (m_floatingContext && &m_placedFloats->list().last().layoutBox() != &floatingObject->renderer()->layoutBox())
            m_floatingContext = nullptr;
    }
}

void FloatingObjects::removePlacedObject(FloatingObject* floatingObject)
{
    if (m_placedFloats && floatingObject->renderer()) {
        m_placedFloats->remove(floatingObject->renderer()->layoutBox());
        m_floatingContext = nullptr;
    }
}

void FloatingObjects::place(RenderBox& floatBox, const LayoutRect& frameRect, LayoutSize marginOffset)
{
    auto it = m_set.find<FloatingObjectHashTranslator>(floatBox);
    if (it == m_set.end()) {
        auto floatingObject = FloatingObject::create(floatBox);
        increaseObjectsCount(floatingObject->type());
        it = m_set.add(WTF::move(floatingObject)).iterator;
    }
    (*it)->setFrameRect(frameRect);
    (*it)->setMarginOffset(marginOffset);
    // Inline layout may have put the float anywhere in the list (see PlacedFloats::add()), where FloatingContext::constraints() may skip it.
    m_floatingContext = nullptr;
}

FloatingObject* FloatingObjects::add(std::unique_ptr<FloatingObject> floatingObject)
{
    increaseObjectsCount(floatingObject->type());
    addPlacedObject(floatingObject.get());
    return m_set.add(WTF::move(floatingObject)).iterator->get();
}

void FloatingObjects::remove(FloatingObject* floatingObject)
{
    ASSERT((m_set.contains(floatingObject)));
    decreaseObjectsCount(floatingObject->type());
    removePlacedObject(floatingObject);
    m_set.remove(floatingObject);
}

std::unique_ptr<Layout::PlacedFloats> FloatingObjects::makePlacedFloats() const
{
    auto placedFloats = makeUnique<Layout::PlacedFloats>(renderer().layoutBox());
    for (auto& floatingObject : m_set) {
        if (floatingObject->renderer())
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

OffsetForPositioningFloat FloatingObjects::offsetForPositioningFloat(FloatingObject::Type type, LayoutUnit fixedOffset, LayoutUnit logicalTop)
{
    auto& block = renderer();
    // The list is in the block's inline direction (see placedFloatItem()).
    auto isLeftToRight = block.writingMode().isLogicalLeftInlineStart();
    auto blockLogicalWidth = block.logicalWidth();
    auto isHorizontalWritingMode = block.isHorizontalWritingMode();
    auto isLeftFloat = type == FloatingObject::FloatLeft;
    auto result = OffsetForPositioningFloat { fixedOffset, { } };
    for (auto& floatItem : placedFloats().list()) {
        if ((floatItem.isStartPositioned() == isLeftToRight) != isLeftFloat)
            continue;
        // As with FloatingObject::height(), which is physical, a float that takes no room vertically doesn't count.
        auto marginBoxSize = floatItem.marginBoxSize();
        if (!(isHorizontalWritingMode ? marginBoxSize.height() : marginBoxSize.width()))
            continue;
        // FIXME: The float's block axis edges shouldn't need to be floored. See <https://webkit.org/b/125831> for more details.
        if (logicalTop < floatItem.marginBoxTop().value().floor() || logicalTop >= floatItem.marginBoxBottom().value().floor())
            continue;
        auto edge = [&] {
            if (isLeftFloat)
                return isLeftToRight ? floatItem.marginBoxRight().value() : blockLogicalWidth - floatItem.marginBoxLeft().value();
            return isLeftToRight ? floatItem.marginBoxLeft().value() : blockLogicalWidth - floatItem.marginBoxRight().value();
        }();
        if (isLeftFloat ? edge <= result.offset : edge >= result.offset)
            continue;
        result = { edge, floatItem.marginBoxBottom().value() - logicalTop };
    }
    return result;
}

void FloatingObjects::shiftFloatsBy(LayoutUnit blockShift)
{
    auto shiftX = (m_horizontalWritingMode) ? 0_lu : -blockShift;
    auto shiftY = (m_horizontalWritingMode) ? blockShift : 0_lu;

    for (auto& floatBox : m_set) {
        if (!floatBox->renderer())
            continue;

        removePlacedObject(floatBox.get());

        floatBox->m_frameRect.move(shiftX, shiftY);
        // Only a float this container lays out moves with this container's content. Anything else in the list is a copy of an entry in another box's list.
        if (floatBox->renderer()->containingBlock() == &renderer())
            floatBox->renderer()->move(shiftX, shiftY);

        addPlacedObject(floatBox.get());
    }
}

} // namespace WebCore
