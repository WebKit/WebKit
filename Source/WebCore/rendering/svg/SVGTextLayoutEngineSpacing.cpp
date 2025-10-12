/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "SVGTextLayoutEngineSpacing.h"

#include "FontCascade.h"
#include "SVGFontElement.h"
#include "SVGFontFaceElement.h"
#include "SVGLengthContext.h"

namespace WebCore {

SVGTextLayoutEngineSpacing::SVGTextLayoutEngineSpacing(const FontCascade& font)
    : m_font(font)
    , m_lastCharacter(0)
{
}

float SVGTextLayoutEngineSpacing::calculateCSSSpacing(char16_t currentCharacter)
{
    float spacing = m_font->letterSpacing();

    if (m_font->wordSpacing() && FontCascade::treatAsSpace(currentCharacter) && !FontCascade::treatAsSpace(m_lastCharacter))
        spacing += m_font->wordSpacing();

    m_lastCharacter = currentCharacter;
    return spacing;
}

}
