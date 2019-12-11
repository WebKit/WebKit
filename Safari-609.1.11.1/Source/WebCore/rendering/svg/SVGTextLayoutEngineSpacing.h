/*
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

#pragma once

#include <unicode/uchar.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class FontCascade;
class SVGRenderStyle;
class SVGElement;

// Helper class used by SVGTextLayoutEngine to handle 'kerning' / 'letter-spacing' and 'word-spacing'.
class SVGTextLayoutEngineSpacing {
    WTF_MAKE_NONCOPYABLE(SVGTextLayoutEngineSpacing);
public:
    SVGTextLayoutEngineSpacing(const FontCascade&);

    float calculateCSSKerningAndSpacing(const SVGRenderStyle*, SVGElement* lengthContext, const UChar* currentCharacter);

private:
    const FontCascade& m_font;
    const UChar* m_lastCharacter;
};

} // namespace WebCore
