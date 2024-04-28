/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "AccessibilityObjectAtspi.h"

#if USE(ATSPI)

#include "AccessibilityAtspi.h"
#include "AccessibilityObject.h" // NOLINT: check-webkit-style has problems with files that do not have primary header

namespace WebCore {

AccessibilityObjectAtspi* AccessibilityObjectAtspi::activeDescendant() const
{
    if (!m_coreObject)
        return nullptr;

    if (auto* activeDescendant = m_coreObject->activeDescendant())
        return activeDescendant->wrapper();

    return nullptr;
}

void AccessibilityObjectAtspi::activeDescendantChanged()
{
    AccessibilityAtspi::singleton().activeDescendantChanged(*this);
}

} // namespace WebCore

#endif // USE(ATSPI)
