/*
 * Copyright (C) 2006-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Kenneth Rohde Christiansen
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "SystemFontDatabase.h"

#include "WebKitFontFamilyNames.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

SystemFontDatabase& SystemFontDatabase::singleton()
{
    static NeverDestroyed<SystemFontDatabase> database = SystemFontDatabase();
    return database.get();
}

static const float defaultControlFontPixelSize = 13;

auto SystemFontDatabase::platformSystemFontShorthandInfo(FontShorthand fontShorthand) -> SystemFontShorthandInfo
{
    static bool initialized;
    static NONCLIENTMETRICS ncm;

    if (!initialized) {
        initialized = true;
        ncm.cbSize = sizeof(NONCLIENTMETRICS);
        ::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    }

    LOGFONT logFont;
    bool shouldUseDefaultControlFontPixelSize = false;
    switch (fontShorthand) {
    case FontShorthand::Icon:
        ::SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(logFont), &logFont, 0);
        break;
    case FontShorthand::Menu:
        logFont = ncm.lfMenuFont;
        break;
    case FontShorthand::MessageBox:
        logFont = ncm.lfMessageFont;
        break;
    case FontShorthand::StatusBar:
        logFont = ncm.lfStatusFont;
        break;
    case FontShorthand::Caption:
        logFont = ncm.lfCaptionFont;
        break;
    case FontShorthand::SmallCaption:
        logFont = ncm.lfSmCaptionFont;
        break;
    case FontShorthand::WebkitSmallControl:
    case FontShorthand::WebkitMiniControl: // Just map to small.
    case FontShorthand::WebkitControl: // Just map to small.
        shouldUseDefaultControlFontPixelSize = true;
        FALLTHROUGH;
    default: { // Everything else uses the stock GUI font.
        HGDIOBJ hGDI = ::GetStockObject(DEFAULT_GUI_FONT);
        if (!hGDI)
            return { WebKitFontFamilyNames::standardFamily, 16, normalWeightValue() };
        if (::GetObject(hGDI, sizeof(logFont), &logFont) <= 0)
            return { WebKitFontFamilyNames::standardFamily, 16, normalWeightValue() };
    }
    }
    float size = shouldUseDefaultControlFontPixelSize ? defaultControlFontPixelSize : std::abs(logFont.lfHeight);
    auto weight = logFont.lfWeight >= 700 ? boldWeightValue() : normalWeightValue();
    return { logFont.lfFaceName, size, weight };
}

void SystemFontDatabase::platformInvalidate()
{
}

} // namespace WebCore
