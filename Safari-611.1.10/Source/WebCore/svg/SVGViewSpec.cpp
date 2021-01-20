/*
 * Copyright (C) 2007, 2010 Rob Buis <buis@kde.org>
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
#include "SVGViewSpec.h"

#include "Document.h"
#include "SVGElement.h"
#include "SVGFitToViewBox.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGTransformList.h"
#include "SVGTransformable.h"
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

SVGViewSpec::SVGViewSpec(SVGElement& contextElement)
    : SVGFitToViewBox(&contextElement, SVGPropertyAccess::ReadOnly)
    , m_contextElement(makeWeakPtr(contextElement))
    , m_transform(SVGTransformList::create(&contextElement, SVGPropertyAccess::ReadOnly))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::transformAttr, &SVGViewSpec::m_transform>();
    });
}

SVGElement* SVGViewSpec::viewTarget() const
{
    if (!m_contextElement)
        return nullptr;
    auto* element = m_contextElement->treeScope().getElementById(m_viewTargetString);
    if (!is<SVGElement>(element))
        return nullptr;
    return downcast<SVGElement>(element);
}

void SVGViewSpec::reset()
{
    m_viewTargetString = emptyString();
    m_transform->clearItems();
    SVGFitToViewBox::reset();
    SVGZoomAndPan::reset();
}

template<typename CharacterType> static constexpr CharacterType svgViewSpec[] = {'s', 'v', 'g', 'V', 'i', 'e', 'w'};
template<typename CharacterType> static constexpr CharacterType viewBoxSpec[] = {'v', 'i', 'e', 'w', 'B', 'o', 'x'};
template<typename CharacterType> static constexpr CharacterType preserveAspectRatioSpec[] = {'p', 'r', 'e', 's', 'e', 'r', 'v', 'e', 'A', 's', 'p', 'e', 'c', 't', 'R', 'a', 't', 'i', 'o'};
template<typename CharacterType> static constexpr CharacterType transformSpec[] = {'t', 'r', 'a', 'n', 's', 'f', 'o', 'r', 'm'};
template<typename CharacterType> static constexpr CharacterType zoomAndPanSpec[] = {'z', 'o', 'o', 'm', 'A', 'n', 'd', 'P', 'a', 'n'};
template<typename CharacterType> static constexpr CharacterType viewTargetSpec[] =  {'v', 'i', 'e', 'w', 'T', 'a', 'r', 'g', 'e', 't'};

bool SVGViewSpec::parseViewSpec(const StringView& string)
{
    return readCharactersForParsing(string, [&](auto buffer) -> bool {
        using CharacterType = typename decltype(buffer)::CharacterType;

        if (buffer.atEnd() || !m_contextElement)
            return false;

        if (!skipCharactersExactly(buffer, svgViewSpec<CharacterType>))
            return false;

        if (!skipExactly(buffer, '('))
            return false;

        while (buffer.hasCharactersRemaining() && *buffer != ')') {
            if (*buffer == 'v') {
                if (skipCharactersExactly(buffer, viewBoxSpec<CharacterType>)) {
                    if (!skipExactly(buffer, '('))
                        return false;
                    auto viewBox = SVGFitToViewBox::parseViewBox(buffer, false);
                    if (!viewBox)
                        return false;
                    setViewBox(WTFMove(*viewBox));
                    if (!skipExactly(buffer, ')'))
                        return false;
                } else if (skipCharactersExactly(buffer, viewTargetSpec<CharacterType>)) {
                    if (!skipExactly(buffer, '('))
                        return false;
                    auto viewTargetStart = buffer.position();
                    skipUntil(buffer, ')');
                    if (buffer.atEnd())
                        return false;
                    m_viewTargetString = String(viewTargetStart, buffer.position() - viewTargetStart);
                    ++buffer;
                } else
                    return false;
            } else if (*buffer == 'z') {
                if (!skipCharactersExactly(buffer, zoomAndPanSpec<CharacterType>))
                    return false;
                if (!skipExactly(buffer, '('))
                    return false;
                auto zoomAndPan = SVGZoomAndPan::parseZoomAndPan(buffer);
                if (!zoomAndPan)
                    return false;
                setZoomAndPan(*zoomAndPan);
                if (!skipExactly(buffer, ')'))
                    return false;
            } else if (*buffer == 'p') {
                if (!skipCharactersExactly(buffer, preserveAspectRatioSpec<CharacterType>))
                    return false;
                if (!skipExactly(buffer, '('))
                    return false;
                SVGPreserveAspectRatioValue preserveAspectRatio;
                if (!preserveAspectRatio.parse(buffer, false))
                    return false;
                setPreserveAspectRatio(preserveAspectRatio);
                if (!skipExactly(buffer, ')'))
                    return false;
            } else if (*buffer == 't') {
                if (!skipCharactersExactly(buffer, transformSpec<CharacterType>))
                    return false;
                if (!skipExactly(buffer, '('))
                    return false;
                m_transform->parse(buffer);
                if (!skipExactly(buffer, ')'))
                    return false;
            } else
                return false;

            skipExactly(buffer, ';');
        }

        if (buffer.atEnd() || *buffer != ')')
            return false;

        return true;
    });
}

}
