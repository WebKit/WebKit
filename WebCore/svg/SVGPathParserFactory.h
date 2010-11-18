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

#ifndef SVGPathParserFactory_h
#define SVGPathParserFactory_h

#if ENABLE(SVG)
#include "Path.h"
#include "PlatformString.h"
#include "SVGPathConsumer.h"
#include "SVGPathSegList.h"
#include "SVGPathByteStream.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class SVGPathParserFactory {
public:
    static SVGPathParserFactory* self();

    bool buildPathFromString(const String&, Path&);
    bool buildPathFromByteStream(SVGPathByteStream*, Path&);
    bool buildPathFromSVGPathSegList(SVGPathSegList*, Path&);

    bool buildSVGPathSegListFromString(const String&, SVGPathSegList*, PathParsingMode);
    bool buildSVGPathSegListFromByteStream(SVGPathByteStream*, SVGPathSegList*, PathParsingMode);

    bool buildStringFromByteStream(SVGPathByteStream*, String&, PathParsingMode);
    bool buildStringFromSVGPathSegList(SVGPathSegList*, String&, PathParsingMode);

    bool buildSVGPathByteStreamFromString(const String&, OwnPtr<SVGPathByteStream>&, PathParsingMode);

    bool buildAnimatedSVGPathByteStream(SVGPathByteStream*, SVGPathByteStream*, OwnPtr<SVGPathByteStream>&, float);

    bool getSVGPathSegAtLengthFromSVGPathSegList(SVGPathSegList*, float, unsigned long&);

private:
    SVGPathParserFactory();
    ~SVGPathParserFactory();
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGPathParserFactory_h
