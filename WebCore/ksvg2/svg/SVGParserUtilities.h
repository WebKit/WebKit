/* This file is part of the KDE project
   Copyright (C) 2002, 2003 The Karbon Developers
                 2006       Rob Buis <buis@kde.org>

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

#ifndef SVGParserUtilities_h
#define SVGParserUtilities_h
#if ENABLE(SVG)

#include <PlatformString.h>

namespace WebCore
{
    bool parseNumber(const UChar*& ptr, const UChar* end, double& number, bool skip = true);
    bool parseNumberOptionalNumber(const String& s, double& h, double& v);

    // SVG allows several different whitespace characters:
    // http://www.w3.org/TR/SVG/paths.html#PathDataBNF
    static inline bool isWhitespace(const UChar& c) {
        return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
    }

    static inline bool skipOptionalSpaces(const UChar*& ptr, const UChar* end)
    {
        while (ptr < end && isWhitespace(*ptr))
            ptr++;
        return ptr < end;
    }

    static inline bool skipOptionalSpacesOrDelimiter(const UChar*& ptr, const UChar *end, UChar delimiter = ',')
    {
        if (ptr < end && !isWhitespace(*ptr) && *ptr != delimiter)
            return false;
        if (skipOptionalSpaces(ptr, end)) {
            if (ptr < end && *ptr == delimiter) {
                ptr++;
                skipOptionalSpaces(ptr, end);
            }
        }
        return ptr < end;
    }

    static inline bool skipString(const UChar*& ptr, const UChar*& end, const UChar* name, int length)
    {
        if (end - ptr < length)
            return false;
        if (memcmp(name, ptr, sizeof(UChar) * length))
            return false;
        ptr += length;
        return true;
    }

    static inline bool skipString(const UChar*& ptr, const UChar*& end, const char* str)
    {
        int length = strlen(str);
        if (end - ptr < length)
            return false;
        for (int i = 0; i < length; ++i)
            if (ptr[i] != str[i])
                return false;
        ptr += length;
        return true;
    }

    class SVGPolyParser
    {
    public:
        virtual ~SVGPolyParser() { }
        bool parsePoints(const String& points) const;

    protected:
        virtual void svgPolyTo(double x1, double y1, int nr) const = 0;
    };

    /**
     * Parser for svg path data, contained in the d attribute.
     *
     * The parser delivers encountered commands and parameters by calling
     * methods that correspond to those commands. Clients have to derive
     * from this class and implement the abstract command methods.
     *
     * There are two operating modes. By default the parser just delivers unaltered
     * svg path data commands and parameters. In the second mode, it will convert all
     * relative coordinates to absolute ones, and convert all curves to cubic beziers.
     */
    class SVGPathParser
    {
    public:
        virtual ~SVGPathParser() { }
        bool parseSVG(const String& d, bool process = false);

    protected:
        virtual void svgMoveTo(float x1, float y1, bool closed, bool abs = true) = 0;
        virtual void svgLineTo(float x1, float y1, bool abs = true) = 0;
        virtual void svgLineToHorizontal(float x, bool abs = true);
        virtual void svgLineToVertical(float y, bool abs = true);
        virtual void svgCurveToCubic(float x1, float y1, float x2, float y2, float x, float y, bool abs = true) = 0;
        virtual void svgCurveToCubicSmooth(float x, float y, float x2, float y2, bool abs = true);
        virtual void svgCurveToQuadratic(float x, float y, float x1, float y1, bool abs = true);
        virtual void svgCurveToQuadraticSmooth(float x, float y, bool abs = true);
        virtual void svgArcTo(float x, float y, float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, bool abs = true);
        virtual void svgClosePath() = 0;
    private:
        void calculateArc(bool relative, double& curx, double& cury, double angle, double x, double y, double r1, double r2, bool largeArcFlag, bool sweepFlag);
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
