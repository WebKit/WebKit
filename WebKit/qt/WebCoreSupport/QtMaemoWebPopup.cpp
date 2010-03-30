/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
#include "QtMaemoWebPopup.h"

namespace WebCore {

QtMaemoWebPopup::QtMaemoWebPopup()
    : QtAbstractWebPopup()
    , m_popup(0)
{
}

QtMaemoWebPopup::~QtMaemoWebPopup()
{
    if (m_popup)
        m_popup->deleteLater();
}

Maemo5Popup* QtMaemoWebPopup::createSingleSelectionPopup()
{
}

Maemo5Popup* QtMaemoWebPopup::createMultipleSelectionPopup()
{
}

Maemo5Popup* QtMaemoWebPopup::createPopup()
{
    Maemo5Popup* result = multiple() ? createMultipleSelectionPopup() : createSingleSelectionPopup();
    connect(result, SIGNAL(finished(int)), this, SLOT(popupClosed()));
    connect(result, SIGNAL(itemClicked(int)), this, SLOT(itemClicked(int)));
    return result;
}


void QtMaemoWebPopup::show()
{
    if (!pageClient() || m_popup)
        return;

    m_popup = createPopup();
    m_popup->show();
}

void QtMaemoWebPopup::hide()
{
    if (!m_popup)
        return;

    m_popup->accept();
}

void QtMaemoWebPopup::popupClosed()
{
    if (!m_popup)
        return;

    m_popup->deleteLater();
    m_popup = 0;
    popupDidHide();
}

void QtMaemoWebPopup::itemClicked(int idx)
{
    selectItem(idx, true, false);
}

}
