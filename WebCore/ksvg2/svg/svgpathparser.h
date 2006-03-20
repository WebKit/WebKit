/* This file is part of the KDE project
   Copyright (C) 2002, 2003 The Karbon Developers

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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef __SVGPATHPARSER_H__
#define __SVGPATHPARSER_H__
#if SVG_SUPPORT

class DeprecatedString;

namespace WebCore
{
    const char *parseCoord(const char *ptr, double &number);

    class SVGPolyParser
    {
    public:
        virtual ~SVGPolyParser() { }
        void parsePoints( const DeprecatedString &points ) const;

    protected:
        virtual void svgPolyTo( double x1, double y1, int nr ) const = 0;
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
        void parseSVG( const DeprecatedString &d, bool process = false );

    protected:
        virtual void svgMoveTo( double x1, double y1, bool closed, bool abs = true ) = 0;
        virtual void svgLineTo( double x1, double y1, bool abs = true ) = 0;
        virtual void svgLineToHorizontal( double x, bool abs = true );
        virtual void svgLineToVertical( double y, bool abs = true );
        virtual void svgCurveToCubic( double x1, double y1, double x2, double y2, double x, double y, bool abs = true ) = 0;
        virtual void svgCurveToCubicSmooth( double x, double y, double x2, double y2, bool abs = true );
        virtual void svgCurveToQuadratic( double x, double y, double x1, double y1, bool abs = true );
        virtual void svgCurveToQuadraticSmooth( double x, double y, bool abs = true );
        virtual void svgArcTo( double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag, bool abs = true );
        virtual void svgClosePath() = 0;
    private:
        void calculateArc( bool relative, double &curx, double &cury, double angle, double x, double y, double r1, double r2, bool largeArcFlag, bool sweepFlag );
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
