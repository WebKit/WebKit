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

namespace WebCore {

bool SVGZoomAndPan::parseZoomAndPan(const UChar*& start, const UChar* end)
{
    static const UChar disable[] = { 'd', 'i', 's', 'a', 'b', 'l', 'e' };
    if (skipString(start, end, disable, WTF_ARRAY_LENGTH(disable))) {
        m_zoomAndPan = SVGZoomAndPanDisable;
        return true;
    }

    static const UChar magnify[] = { 'm', 'a', 'g', 'n', 'i', 'f', 'y' };
    if (skipString(start, end, magnify, WTF_ARRAY_LENGTH(magnify))) {
        m_zoomAndPan = SVGZoomAndPanMagnify;
        return true;
    }

    return false;
}

void SVGZoomAndPan::parseAttribute(const QualifiedName& attributeName, const AtomString& value)
{
    if (attributeName != SVGNames::zoomAndPanAttr)
        return;
    m_zoomAndPan = SVGPropertyTraits<SVGZoomAndPanType>::fromString(value);
}

}
