/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "SVGZoomAndPan.h"

#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

template<typename CharacterType> static constexpr CharacterType disable[] = { 'd', 'i', 's', 'a', 'b', 'l', 'e' };
template<typename CharacterType> static constexpr CharacterType magnify[] = { 'm', 'a', 'g', 'n', 'i', 'f', 'y' };

template<typename CharacterType> static Optional<SVGZoomAndPanType> parseZoomAndPanGeneric(StringParsingBuffer<CharacterType>& buffer)
{
    if (skipCharactersExactly(buffer, disable<CharacterType>))
        return SVGZoomAndPanDisable;

    if (skipCharactersExactly(buffer, magnify<CharacterType>))
        return SVGZoomAndPanMagnify;

    return WTF::nullopt;
}

Optional<SVGZoomAndPanType> SVGZoomAndPan::parseZoomAndPan(StringParsingBuffer<LChar>& buffer)
{
    return parseZoomAndPanGeneric(buffer);
}

Optional<SVGZoomAndPanType> SVGZoomAndPan::parseZoomAndPan(StringParsingBuffer<UChar>& buffer)
{
    return parseZoomAndPanGeneric(buffer);
}

void SVGZoomAndPan::parseAttribute(const QualifiedName& attributeName, const AtomString& value)
{
    if (attributeName != SVGNames::zoomAndPanAttr)
        return;
    m_zoomAndPan = SVGPropertyTraits<SVGZoomAndPanType>::fromString(value);
}

}
