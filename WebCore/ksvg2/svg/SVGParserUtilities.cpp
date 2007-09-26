/* This file is part of the KDE project
   Copyright (C) 2002, 2003 The Karbon Developers
                 2006       Alexander Kellett <lypanov@kde.org>
                 2006, 2007 Rob Buis <buis@kde.org>

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
#include "SVGParserUtilities.h"

#include "FloatConversion.h"
#include "PlatformString.h"
#include <math.h>
#include <wtf/MathExtras.h>

namespace WebCore {

bool parseNumber(const UChar*& ptr, const UChar* end, double& number, bool skip)
{
    int integer, exponent;
    double decimal, frac;
    int sign, expsign;
    const UChar* start = ptr;

    exponent = 0;
    integer = 0;
    frac = 1.0;
    decimal = 0;
    sign = 1;
    expsign = 1;

    // read the sign
    if (ptr < end && *ptr == '+')
        ptr++;
    else if (ptr < end && *ptr == '-') {
        ptr++;
        sign = -1;
    }
    // read the integer part
    while (ptr < end && *ptr >= '0' && *ptr <= '9')
        integer = (integer * 10) + *(ptr++) - '0';

    if (ptr < end && *ptr == '.') { // read the decimals
        ptr++;
        while(ptr < end && *ptr >= '0' && *ptr <= '9')
            decimal += (*(ptr++) - '0') * (frac *= 0.1);
    }

    if (ptr < end && (*ptr == 'e' || *ptr == 'E')) { // read the exponent part
        ptr++;

        // read the sign of the exponent
        if (ptr < end && *ptr == '+')
            ptr++;
        else if (ptr < end && *ptr == '-') {
            ptr++;
            expsign = -1;
        }

        while (ptr < end && *ptr >= '0' && *ptr <= '9') {
            exponent *= 10;
            exponent += *ptr - '0';
            ptr++;
        }
    }

    number = integer + decimal;
    number *= sign * pow(10.0, expsign * exponent);

    if (start == ptr)
        return false;

    if (skip)
        skipOptionalSpacesOrDelimiter(ptr, end);

    return true;
}

bool parseNumberOptionalNumber(const String& s, double& x, double& y)
{
    if (s.isEmpty())
        return false;
    const UChar* cur = s.characters();
    const UChar* end = cur + s.length();

    if (!parseNumber(cur, end, x))
        return false;

    if (cur == end)
        y = x;
    else if (!parseNumber(cur, end, y, false))
        return false;

    return cur == end;
}

bool SVGPolyParser::parsePoints(const String& s) const
{
    if (s.isEmpty())
        return true;
    const UChar* cur = s.characters();
    const UChar* end = cur + s.length();

    skipOptionalSpaces(cur, end);

    bool delimParsed = false;
    int segmentNum = 0;
    while (cur < end) {
        delimParsed = false;
        double xPos = 0;
        if (!parseNumber(cur, end, xPos))
           return false;

        double yPos = 0;
        if (!parseNumber(cur, end, yPos, false))
            return false;

        skipOptionalSpaces(cur, end);

        if (cur < end && *cur == ',') {
            delimParsed = true;
            cur++;
        }
        skipOptionalSpaces(cur, end);

        svgPolyTo(xPos, yPos, segmentNum++);
    }
    return cur == end && !delimParsed;
}

bool SVGPathParser::parseSVG(const String& s, bool process)
{
    if (s.isEmpty())
        return false;

    const UChar* ptr = s.characters();
    const UChar* end = ptr + s.length();

    double contrlx, contrly, curx, cury, subpathx, subpathy, tox, toy, x1, y1, x2, y2, xc, yc;
    double px1, py1, px2, py2, px3, py3;
    bool closed = true;
    
    if (!skipOptionalSpaces(ptr, end)) // skip any leading spaces
        return false;
    
    char command = *(ptr++), lastCommand = ' ';
    if (command != 'm' && command != 'M') // path must start with moveto
        return false;

    subpathx = subpathy = curx = cury = contrlx = contrly = 0.0;
    while (1) {
        skipOptionalSpaces(ptr, end); // skip spaces between command and first coord

        bool relative = false;

        switch(command)
        {
            case 'm':
                relative = true;
            case 'M':
            {
                if (!parseNumber(ptr, end, tox) || !parseNumber(ptr, end, toy))
                    return false;

                if (process) {
                    subpathx = curx = relative ? curx + tox : tox;
                    subpathy = cury = relative ? cury + toy : toy;

                    svgMoveTo(narrowPrecisionToFloat(curx), narrowPrecisionToFloat(cury), closed);
                } else
                    svgMoveTo(narrowPrecisionToFloat(tox), narrowPrecisionToFloat(toy), closed, !relative);
                closed = false;
                break;
            }
            case 'l':
                relative = true;
            case 'L':
            {
                if (!parseNumber(ptr, end, tox) || !parseNumber(ptr, end, toy))
                    return false;

                if (process) {
                    curx = relative ? curx + tox : tox;
                    cury = relative ? cury + toy : toy;

                    svgLineTo(narrowPrecisionToFloat(curx), narrowPrecisionToFloat(cury));
                }
                else
                    svgLineTo(narrowPrecisionToFloat(tox), narrowPrecisionToFloat(toy), !relative);
                break;
            }
            case 'h':
            {
                if (!parseNumber(ptr, end, tox))
                    return false;
                if (process) {
                    curx = curx + tox;
                    svgLineTo(narrowPrecisionToFloat(curx), narrowPrecisionToFloat(cury));
                }
                else
                    svgLineToHorizontal(narrowPrecisionToFloat(tox), false);
                break;
            }
            case 'H':
            {
                if (!parseNumber(ptr, end, tox))
                    return false;
                if (process) {
                    curx = tox;
                    svgLineTo(narrowPrecisionToFloat(curx), narrowPrecisionToFloat(cury));
                }
                else
                    svgLineToHorizontal(narrowPrecisionToFloat(tox));
                break;
            }
            case 'v':
            {
                if (!parseNumber(ptr, end, toy))
                    return false;
                if (process) {
                    cury = cury + toy;
                    svgLineTo(narrowPrecisionToFloat(curx), narrowPrecisionToFloat(cury));
                }
                else
                    svgLineToVertical(narrowPrecisionToFloat(toy), false);
                break;
            }
            case 'V':
            {
                if (!parseNumber(ptr, end, toy))
                    return false;
                if (process) {
                    cury = toy;
                    svgLineTo(narrowPrecisionToFloat(curx), narrowPrecisionToFloat(cury));
                }
                else
                    svgLineToVertical(narrowPrecisionToFloat(toy));
                break;
            }
            case 'z':
            case 'Z':
            {
                // reset curx, cury for next path
                if (process) {
                    curx = subpathx;
                    cury = subpathy;
                }
                closed = true;
                svgClosePath();
                break;
            }
            case 'c':
                relative = true;
            case 'C':
            {
                if (!parseNumber(ptr, end, x1)  || !parseNumber(ptr, end, y1) ||
                    !parseNumber(ptr, end, x2)  || !parseNumber(ptr, end, y2) ||
                    !parseNumber(ptr, end, tox) || !parseNumber(ptr, end, toy))
                    return false;

                if (process) {
                    px1 = relative ? curx + x1 : x1;
                    py1 = relative ? cury + y1 : y1;
                    px2 = relative ? curx + x2 : x2;
                    py2 = relative ? cury + y2 : y2;
                    px3 = relative ? curx + tox : tox;
                    py3 = relative ? cury + toy : toy;

                    svgCurveToCubic(narrowPrecisionToFloat(px1), narrowPrecisionToFloat(py1), narrowPrecisionToFloat(px2), 
                                    narrowPrecisionToFloat(py2), narrowPrecisionToFloat(px3), narrowPrecisionToFloat(py3));

                    contrlx = relative ? curx + x2 : x2;
                    contrly = relative ? cury + y2 : y2;
                    curx = relative ? curx + tox : tox;
                    cury = relative ? cury + toy : toy;
                }
                else
                    svgCurveToCubic(narrowPrecisionToFloat(x1), narrowPrecisionToFloat(y1), narrowPrecisionToFloat(x2),
                                    narrowPrecisionToFloat(y2), narrowPrecisionToFloat(tox), narrowPrecisionToFloat(toy), !relative);

                break;
            }
            case 's':
                relative = true;
            case 'S':
            {
                if (!parseNumber(ptr, end, x2)  || !parseNumber(ptr, end, y2) ||
                    !parseNumber(ptr, end, tox) || !parseNumber(ptr, end, toy))
                    return false;

                if (!(lastCommand == 'c' || lastCommand == 'C' ||
                     lastCommand == 's' || lastCommand == 'S')) {
                    contrlx = curx;
                    contrly = cury;
                }

                if (process) {
                    px1 = 2 * curx - contrlx;
                    py1 = 2 * cury - contrly;
                    px2 = relative ? curx + x2 : x2;
                    py2 = relative ? cury + y2 : y2;
                    px3 = relative ? curx + tox : tox;
                    py3 = relative ? cury + toy : toy;

                    svgCurveToCubic(narrowPrecisionToFloat(px1), narrowPrecisionToFloat(py1), narrowPrecisionToFloat(px2),
                                    narrowPrecisionToFloat(py2), narrowPrecisionToFloat(px3), narrowPrecisionToFloat(py3));

                    contrlx = relative ? curx + x2 : x2;
                    contrly = relative ? cury + y2 : y2;
                    curx = relative ? curx + tox : tox;
                    cury = relative ? cury + toy : toy;
                }
                else
                    svgCurveToCubicSmooth(narrowPrecisionToFloat(x2), narrowPrecisionToFloat(y2), 
                                          narrowPrecisionToFloat(tox), narrowPrecisionToFloat(toy), !relative);
                break;
            }
            case 'q':
                relative = true;
            case 'Q':
            {
                if (!parseNumber(ptr, end, x1)  || !parseNumber(ptr, end, y1) ||
                    !parseNumber(ptr, end, tox) || !parseNumber(ptr, end, toy))
                    return false;

                if (process) {
                    px1 = relative ? (curx + 2 * (x1 + curx)) * (1.0 / 3.0) : (curx + 2 * x1) * (1.0 / 3.0);
                    py1 = relative ? (cury + 2 * (y1 + cury)) * (1.0 / 3.0) : (cury + 2 * y1) * (1.0 / 3.0);
                    px2 = relative ? ((curx + tox) + 2 * (x1 + curx)) * (1.0 / 3.0) : (tox + 2 * x1) * (1.0 / 3.0);
                    py2 = relative ? ((cury + toy) + 2 * (y1 + cury)) * (1.0 / 3.0) : (toy + 2 * y1) * (1.0 / 3.0);
                    px3 = relative ? curx + tox : tox;
                    py3 = relative ? cury + toy : toy;

                    svgCurveToCubic(narrowPrecisionToFloat(px1), narrowPrecisionToFloat(py1), narrowPrecisionToFloat(px2),
                                    narrowPrecisionToFloat(py2), narrowPrecisionToFloat(px3), narrowPrecisionToFloat(py3));

                    contrlx = relative ? curx + x1 : x1;
                    contrly = relative ? cury + y1 : y1;
                    curx = relative ? curx + tox : tox;
                    cury = relative ? cury + toy : toy;
                }
                else
                    svgCurveToQuadratic(narrowPrecisionToFloat(x1), narrowPrecisionToFloat(y1),
                                        narrowPrecisionToFloat(tox), narrowPrecisionToFloat(toy), !relative);
                break;
            }
            case 't':
                relative = true;
            case 'T':
            {
                if (!parseNumber(ptr, end, tox) || !parseNumber(ptr, end, toy))
                    return false;
                if (!(lastCommand == 'q' || lastCommand == 'Q' ||
                     lastCommand == 't' || lastCommand == 'T')) {
                    contrlx = curx;
                    contrly = cury;
                }

                if (process) {
                    xc = 2 * curx - contrlx;
                    yc = 2 * cury - contrly;

                    px1 = relative ? (curx + 2 * xc) * (1.0 / 3.0) : (curx + 2 * xc) * (1.0 / 3.0);
                    py1 = relative ? (cury + 2 * yc) * (1.0 / 3.0) : (cury + 2 * yc) * (1.0 / 3.0);
                    px2 = relative ? ((curx + tox) + 2 * xc) * (1.0 / 3.0) : (tox + 2 * xc) * (1.0 / 3.0);
                    py2 = relative ? ((cury + toy) + 2 * yc) * (1.0 / 3.0) : (toy + 2 * yc) * (1.0 / 3.0);
                    px3 = relative ? curx + tox : tox;
                    py3 = relative ? cury + toy : toy;

                    svgCurveToCubic(narrowPrecisionToFloat(px1), narrowPrecisionToFloat(py1), narrowPrecisionToFloat(px2),
                                    narrowPrecisionToFloat(py2), narrowPrecisionToFloat(px3), narrowPrecisionToFloat(py3));

                    contrlx = xc;
                    contrly = yc;
                    curx = relative ? curx + tox : tox;
                    cury = relative ? cury + toy : toy;
                }
                else
                    svgCurveToQuadraticSmooth(narrowPrecisionToFloat(tox), narrowPrecisionToFloat(toy), !relative);
                break;
            }
            case 'a':
                relative = true;
            case 'A':
            {
                bool largeArc, sweep;
                double angle, rx, ry;
                if (!parseNumber(ptr, end, rx)    || !parseNumber(ptr, end, ry) ||
                    !parseNumber(ptr, end, angle) || !parseNumber(ptr, end, tox))
                    return false;
                largeArc = tox == 1;
                if (!parseNumber(ptr, end, tox))
                    return false;
                sweep = tox == 1;
                if (!parseNumber(ptr, end, tox) || !parseNumber(ptr, end, toy))
                    return false;

                // Spec: radii are nonnegative numbers
                rx = fabs(rx);
                ry = fabs(ry);

                if (process)
                    calculateArc(relative, curx, cury, angle, tox, toy, rx, ry, largeArc, sweep);
                else
                    svgArcTo(narrowPrecisionToFloat(tox), narrowPrecisionToFloat(toy), narrowPrecisionToFloat(rx), narrowPrecisionToFloat(ry),
                             narrowPrecisionToFloat(angle), largeArc, sweep, !relative);
                break;
            }
            default:
                // FIXME: An error should go to the JavaScript console, or the like.
                return false;
        }
        lastCommand = command;

        if (ptr >= end)
            return true;

        // Check for remaining coordinates in the current command.
        if ((*ptr == '+' || *ptr == '-' || (*ptr >= '0' && *ptr <= '9')) &&
            (command != 'z' && command !='a' && command != 'A')) {
            if (command == 'M')
                command = 'L';
            else if (command == 'm')
                command = 'l';
        } else
            command = *(ptr++);

        if (lastCommand != 'C' && lastCommand != 'c' &&
            lastCommand != 'S' && lastCommand != 's' &&
            lastCommand != 'Q' && lastCommand != 'q' &&
            lastCommand != 'T' && lastCommand != 't') {
            contrlx = curx;
            contrly = cury;
        }
    }

    return false;
}

// This works by converting the SVG arc to "simple" beziers.
// For each bezier found a svgToCurve call is done.
// Adapted from Niko's code in kdelibs/kdecore/svgicons.
// Maybe this can serve in some shared lib? (Rob)
void SVGPathParser::calculateArc(bool relative, double& curx, double& cury, double angle, double x, double y, double r1, double r2, bool largeArcFlag, bool sweepFlag)
{
    double sin_th, cos_th;
    double a00, a01, a10, a11;
    double x0, y0, x1, y1, xc, yc;
    double d, sfactor, sfactor_sq;
    double th0, th1, th_arc;
    int i, n_segs;

    sin_th = sin(angle * (piDouble / 180.0));
    cos_th = cos(angle * (piDouble / 180.0));

    double dx;

    if (!relative)
        dx = (curx - x) / 2.0;
    else
        dx = -x / 2.0;

    double dy;
        
    if (!relative)
        dy = (cury - y) / 2.0;
    else
        dy = -y / 2.0;
        
    double _x1 =  cos_th * dx + sin_th * dy;
    double _y1 = -sin_th * dx + cos_th * dy;
    double Pr1 = r1 * r1;
    double Pr2 = r2 * r2;
    double Px = _x1 * _x1;
    double Py = _y1 * _y1;

    // Spec : check if radii are large enough
    double check = Px / Pr1 + Py / Pr2;
    if (check > 1) {
        r1 = r1 * sqrt(check);
        r2 = r2 * sqrt(check);
    }

    a00 = cos_th / r1;
    a01 = sin_th / r1;
    a10 = -sin_th / r2;
    a11 = cos_th / r2;

    x0 = a00 * curx + a01 * cury;
    y0 = a10 * curx + a11 * cury;

    if (!relative)
        x1 = a00 * x + a01 * y;
    else
        x1 = a00 * (curx + x) + a01 * (cury + y);
        
    if (!relative)
        y1 = a10 * x + a11 * y;
    else
        y1 = a10 * (curx + x) + a11 * (cury + y);

    /* (x0, y0) is current point in transformed coordinate space.
       (x1, y1) is new point in transformed coordinate space.

       The arc fits a unit-radius circle in this space.
    */

    d = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0);

    sfactor_sq = 1.0 / d - 0.25;

    if (sfactor_sq < 0)
        sfactor_sq = 0;

    sfactor = sqrt(sfactor_sq);

    if (sweepFlag == largeArcFlag)
        sfactor = -sfactor;

    xc = 0.5 * (x0 + x1) - sfactor * (y1 - y0);
    yc = 0.5 * (y0 + y1) + sfactor * (x1 - x0);

    /* (xc, yc) is center of the circle. */
    th0 = atan2(y0 - yc, x0 - xc);
    th1 = atan2(y1 - yc, x1 - xc);

    th_arc = th1 - th0;
    if (th_arc < 0 && sweepFlag)
        th_arc += 2 * piDouble;
    else if (th_arc > 0 && !sweepFlag)
        th_arc -= 2 * piDouble;

    n_segs = (int) (int) ceil(fabs(th_arc / (piDouble * 0.5 + 0.001)));

    for(i = 0; i < n_segs; i++) {
        double sin_th, cos_th;
        double a00, a01, a10, a11;
        double x1, y1, x2, y2, x3, y3;
        double t;
        double th_half;

        double _th0 = th0 + i * th_arc / n_segs;
        double _th1 = th0 + (i + 1) * th_arc / n_segs;

        sin_th = sin(angle * (piDouble / 180.0));
        cos_th = cos(angle * (piDouble / 180.0));

        /* inverse transform compared with rsvg_path_arc */
        a00 = cos_th * r1;
        a01 = -sin_th * r2;
        a10 = sin_th * r1;
        a11 = cos_th * r2;

        th_half = 0.5 * (_th1 - _th0);
        t = (8.0 / 3.0) * sin(th_half * 0.5) * sin(th_half * 0.5) / sin(th_half);
        x1 = xc + cos(_th0) - t * sin(_th0);
        y1 = yc + sin(_th0) + t * cos(_th0);
        x3 = xc + cos(_th1);
        y3 = yc + sin(_th1);
        x2 = x3 + t * sin(_th1);
        y2 = y3 - t * cos(_th1);

        svgCurveToCubic(narrowPrecisionToFloat(a00 * x1 + a01 * y1), narrowPrecisionToFloat(a10 * x1 + a11 * y1),
                        narrowPrecisionToFloat(a00 * x2 + a01 * y2), narrowPrecisionToFloat(a10 * x2 + a11 * y2),
                        narrowPrecisionToFloat(a00 * x3 + a01 * y3), narrowPrecisionToFloat(a10 * x3 + a11 * y3));
    }

    if (!relative)
        curx = x;
    else
        curx += x;

    if (!relative)
        cury = y;
    else
        cury += y;    
}

void SVGPathParser::svgLineToHorizontal(float, bool)
{
}

void SVGPathParser::svgLineToVertical(float, bool)
{
}

void SVGPathParser::svgCurveToCubicSmooth(float, float, float, float, bool)
{
}

void SVGPathParser::svgCurveToQuadratic(float, float, float, float, bool)
{
}

void SVGPathParser::svgCurveToQuadraticSmooth(float, float, bool)
{
}

void SVGPathParser::svgArcTo(float, float, float, float, float, bool, bool, bool)
{
} 

}

// vim:ts=4:noet
#endif // ENABLE(SVG)
