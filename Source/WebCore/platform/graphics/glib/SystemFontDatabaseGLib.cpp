/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "SystemFontDatabase.h"

#include "SystemSettings.h"
#include "WebKitFontFamilyNames.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

SystemFontDatabase& SystemFontDatabase::singleton()
{
    static NeverDestroyed<SystemFontDatabase> database = SystemFontDatabase();
    return database.get();
}

auto SystemFontDatabase::platformSystemFontShorthandInfo(FontShorthand) -> SystemFontShorthandInfo
{
    auto& systemSettings = SystemSettings::singleton();
    auto systemFontFamily = systemSettings.fontFamily();
    return { systemFontFamily ? AtomString(*systemFontFamily) : WebKitFontFamilyNames::standardFamily, systemSettings.fontSize().value_or(16), FontSelectionValue(systemSettings.fontWeight().value_or(400)) };
}

void SystemFontDatabase::platformInvalidate()
{
}

} // namespace WebCore
