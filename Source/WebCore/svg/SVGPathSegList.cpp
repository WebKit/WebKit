/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "SVGPathSegList.h"

#include "SVGAnimatedPathSegListPropertyTearOff.h"
#include "SVGPathElement.h"
#include "SVGPathSegWithContext.h"

namespace WebCore {

ExceptionOr<void> SVGPathSegList::clear()
{
    ASSERT(m_values);
    if (m_values->isEmpty())
        return { };
    return Base::clearValues();
}

ExceptionOr<RefPtr<SVGPathSeg>> SVGPathSegList::getItem(unsigned index)
{
    return Base::getItemValues(index);
}

ExceptionOr<RefPtr<SVGPathSeg>> SVGPathSegList::replaceItem(Ref<SVGPathSeg>&& newItem, unsigned index)
{
    if (index < m_values->size())
        m_values->clearItemContextAndRole(index);
    return Base::replaceItemValues(WTFMove(newItem), index);
}

ExceptionOr<RefPtr<SVGPathSeg>> SVGPathSegList::removeItem(unsigned index)
{
    if (index < m_values->size())
        m_values->clearItemContextAndRole(index);
    auto result = Base::removeItemValues(index);
    if (result.hasException())
        return result;
    return result.releaseReturnValue();
}

SVGPathElement* SVGPathSegList::contextElement() const
{
    SVGElement* contextElement = m_animatedProperty->contextElement();
    ASSERT(contextElement);
    return downcast<SVGPathElement>(contextElement);
}

bool SVGPathSegList::processIncomingListItemValue(const ListItemType& newItem, unsigned* indexToModify)
{
    SVGPathSegWithContext* newItemWithContext = static_cast<SVGPathSegWithContext*>(newItem.get());
    RefPtr<SVGAnimatedProperty> animatedPropertyOfItem = newItemWithContext->animatedProperty();

    // Alter the role, after calling animatedProperty(), as that may influence the returned animated property.
    newItemWithContext->setContextAndRole(contextElement(), m_pathSegRole);

    if (!animatedPropertyOfItem)
        return true;

    // newItem belongs to a SVGPathElement, but its associated SVGAnimatedProperty is not an animated list tear off.
    // (for example: "pathElement.pathSegList.appendItem(pathElement.createSVGPathSegClosepath())")
    if (!animatedPropertyOfItem->isAnimatedListTearOff())
        return true;

    // Spec: If newItem is already in a list, it is removed from its previous list before it is inserted into this list.
    // 'newItem' is already living in another list. If it's not our list, synchronize the other lists wrappers after the removal.
    bool livesInOtherList = animatedPropertyOfItem != m_animatedProperty;
    RefPtr<SVGAnimatedPathSegListPropertyTearOff> propertyTearOff = static_pointer_cast<SVGAnimatedPathSegListPropertyTearOff>(animatedPropertyOfItem);
    int indexToRemove = propertyTearOff->findItem(newItem.get());

    // If newItem does not exist in the propertyTearOff baseVal() list, it has to be
    // in its animVal() list and it has to be animating.
    if (indexToRemove == -1) {
        ASSERT(propertyTearOff->isAnimating());
        ASSERT(propertyTearOff->animVal()->findItem(newItem.get()) != -1);
        return false;
    }

    // Do not remove newItem if already in this list at the target index.
    if (!livesInOtherList && indexToModify && static_cast<unsigned>(indexToRemove) == *indexToModify)
        return false;

    propertyTearOff->removeItemFromList(indexToRemove, livesInOtherList);

    if (!indexToModify)
        return true;

    // If the item lived in our list, adjust the insertion index.
    if (!livesInOtherList) {
        unsigned& index = *indexToModify;
        // Spec: If the item is already in this list, note that the index of the item to (replace|insert before) is before the removal of the item.
        if (static_cast<unsigned>(indexToRemove) < index)
            --index;
    }

    return true;
}

}
