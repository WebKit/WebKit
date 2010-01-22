/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
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
 *
 */
#include "config.h"
#include "QtAbstractWebPopup.h"

#include "PopupMenuClient.h"


namespace WebCore {

QtAbstractWebPopup::QtAbstractWebPopup()
    : m_popupClient(0)
    , m_pageClient(0)
    , m_currentIndex(-1)
{
}

QtAbstractWebPopup::~QtAbstractWebPopup()
{
}

void QtAbstractWebPopup::popupDidHide()
{
    Q_ASSERT(m_popupClient);
    m_popupClient->popupDidHide();
}

void QtAbstractWebPopup::valueChanged(int index)
{
    Q_ASSERT(m_popupClient);
    m_popupClient->valueChanged(index);
}

QtAbstractWebPopup::ItemType QtAbstractWebPopup::itemType(int idx) const
{
    if (m_popupClient->itemIsSeparator(idx))
        return Separator;
    if (m_popupClient->itemIsLabel(idx))
        return Group;
    return Option;
}

} // namespace WebCore
