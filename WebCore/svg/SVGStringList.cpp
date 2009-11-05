/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGStringList.h"

#include "SVGParserUtilities.h"

namespace WebCore {

SVGStringList::SVGStringList(const QualifiedName& attributeName)
    : SVGList<String>(attributeName)
{
}

SVGStringList::~SVGStringList()
{
}

void SVGStringList::reset(const String& str)
{
    ExceptionCode ec = 0;

    parse(str, ' ');
    if (numberOfItems() == 0)
        appendItem(String(""), ec); // Create empty string...
}

void SVGStringList::parse(const String& data, UChar delimiter)
{
    // TODO : more error checking/reporting
    ExceptionCode ec = 0;
    clear(ec);

    const UChar* ptr = data.characters();
    const UChar* end = ptr + data.length();
    while (ptr < end) {
        const UChar* start = ptr;
        while (ptr < end && *ptr != delimiter && !isWhitespace(*ptr))
            ptr++;
        if (ptr == start)
            break;
        appendItem(String(start, ptr - start), ec);
        skipOptionalSpacesOrDelimiter(ptr, end, delimiter);
    }
}

}

#endif // ENABLE(SVG)
