/*-------------------------------------------------------------------------
 * drawElements Quality Program Reference Renderer
 * -----------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Reference rasterizer
 *//*--------------------------------------------------------------------*/

#include "rrRasterizer.hpp"
#include "deMath.h"
#include "tcuVectorUtil.hpp"

namespace rr
{

inline int64_t toSubpixelCoord(float v, int bits)
{
    return (int64_t)(v * (float)(1 << bits) + (v < 0.f ? -0.5f : 0.5f));
}

inline int64_t toSubpixelCoord(int32_t v, int bits)
{
    return v << bits;
}

inline int32_t ceilSubpixelToPixelCoord(int64_t coord, int bits, bool fillEdge)
{
    if (coord >= 0)
        return (int32_t)((coord + ((1ll << bits) - (fillEdge ? 0 : 1))) >> bits);
    else
        return (int32_t)((coord + (fillEdge ? 1 : 0)) >> bits);
}

inline int32_t floorSubpixelToPixelCoord(int64_t coord, int bits, bool fillEdge)
{
    if (coord >= 0)
        return (int32_t)((coord - (fillEdge ? 1 : 0)) >> bits);
    else
        return (int32_t)((coord - ((1ll << bits) - (fillEdge ? 0 : 1))) >> bits);
}

static inline void initEdgeCCW(EdgeFunction &edge, const HorizontalFill horizontalFill, const VerticalFill verticalFill,
                               const int64_t x0, const int64_t y0, const int64_t x1, const int64_t y1)
{
    // \note See EdgeFunction documentation for details.

    const int64_t xd = x1 - x0;
    const int64_t yd = y1 - y0;
    bool inclusive   = false; //!< Inclusive in CCW orientation.

    if (yd == 0)
        inclusive = verticalFill == FILL_BOTTOM ? xd >= 0 : xd <= 0;
    else
        inclusive = horizontalFill == FILL_LEFT ? yd <= 0 : yd >= 0;

    edge.a         = (y0 - y1);
    edge.b         = (x1 - x0);
    edge.c         = x0 * y1 - y0 * x1;
    edge.inclusive = inclusive; //!< \todo [pyry] Swap for CW triangles
}

static inline void reverseEdge(EdgeFunction &edge)
{
    edge.a         = -edge.a;
    edge.b         = -edge.b;
    edge.c         = -edge.c;
    edge.inclusive = !edge.inclusive;
}

static inline int64_t evaluateEdge(const EdgeFunction &edge, const int64_t x, const int64_t y)
{
    return edge.a * x + edge.b * y + edge.c;
}

static inline bool isInsideCCW(const EdgeFunction &edge, const int64_t edgeVal)
{
    return edge.inclusive ? (edgeVal >= 0) : (edgeVal > 0);
}

namespace LineRasterUtil
{

struct SubpixelLineSegment
{
    const tcu::Vector<int64_t, 2> m_v0;
    const tcu::Vector<int64_t, 2> m_v1;

    SubpixelLineSegment(const tcu::Vector<int64_t, 2> &v0, const tcu::Vector<int64_t, 2> &v1) : m_v0(v0), m_v1(v1)
    {
    }

    tcu::Vector<int64_t, 2> direction(void) const
    {
        return m_v1 - m_v0;
    }
};

enum LINE_SIDE
{
    LINE_SIDE_INTERSECT = 0,
    LINE_SIDE_LEFT,
    LINE_SIDE_RIGHT
};

static tcu::Vector<int64_t, 2> toSubpixelVector(const tcu::Vec2 &v, int bits)
{
    return tcu::Vector<int64_t, 2>(toSubpixelCoord(v.x(), bits), toSubpixelCoord(v.y(), bits));
}

static tcu::Vector<int64_t, 2> toSubpixelVector(const tcu::IVec2 &v, int bits)
{
    return tcu::Vector<int64_t, 2>(toSubpixelCoord(v.x(), bits), toSubpixelCoord(v.y(), bits));
}

#if defined(DE_DEBUG)
static bool isTheCenterOfTheFragment(const tcu::Vector<int64_t, 2> &a, int bits)
{
    const uint64_t pixelSize = 1ll << bits;
    const uint64_t halfPixel = 1ll << (bits - 1);
    return ((a.x() & (pixelSize - 1)) == halfPixel && (a.y() & (pixelSize - 1)) == halfPixel);
}

static bool inViewport(const tcu::IVec2 &p, const tcu::IVec4 &viewport)
{
    return p.x() >= viewport.x() && p.y() >= viewport.y() && p.x() < viewport.x() + viewport.z() &&
           p.y() < viewport.y() + viewport.w();
}
#endif // DE_DEBUG

// returns true if vertex is on the left side of the line
static bool vertexOnLeftSideOfLine(const tcu::Vector<int64_t, 2> &p, const SubpixelLineSegment &l)
{
    const tcu::Vector<int64_t, 2> u = l.direction();
    const tcu::Vector<int64_t, 2> v = (p - l.m_v0);
    const int64_t crossProduct      = (u.x() * v.y() - u.y() * v.x());
    return crossProduct < 0;
}

// returns true if vertex is on the right side of the line
static bool vertexOnRightSideOfLine(const tcu::Vector<int64_t, 2> &p, const SubpixelLineSegment &l)
{
    const tcu::Vector<int64_t, 2> u = l.direction();
    const tcu::Vector<int64_t, 2> v = (p - l.m_v0);
    const int64_t crossProduct      = (u.x() * v.y() - u.y() * v.x());
    return crossProduct > 0;
}

// returns true if vertex is on the line
static bool vertexOnLine(const tcu::Vector<int64_t, 2> &p, const SubpixelLineSegment &l)
{
    const tcu::Vector<int64_t, 2> u = l.direction();
    const tcu::Vector<int64_t, 2> v = (p - l.m_v0);
    const int64_t crossProduct      = (u.x() * v.y() - u.y() * v.x());
    return crossProduct == 0; // cross product == 0
}

// returns true if vertex is on the line segment
static bool vertexOnLineSegment(const tcu::Vector<int64_t, 2> &p, const SubpixelLineSegment &l)
{
    if (!vertexOnLine(p, l))
        return false;

    const tcu::Vector<int64_t, 2> v  = l.direction();
    const tcu::Vector<int64_t, 2> u1 = (p - l.m_v0);
    const tcu::Vector<int64_t, 2> u2 = (p - l.m_v1);

    if (v.x() == 0 && v.y() == 0)
        return false;

    return tcu::dot(v, u1) >= 0 && tcu::dot(-v, u2) >= 0; // dot (A->B, A->V) >= 0 and dot (B->A, B->V) >= 0
}

static LINE_SIDE getVertexSide(const tcu::Vector<int64_t, 2> &v, const SubpixelLineSegment &l)
{
    if (vertexOnLeftSideOfLine(v, l))
        return LINE_SIDE_LEFT;
    else if (vertexOnRightSideOfLine(v, l))
        return LINE_SIDE_RIGHT;
    else if (vertexOnLine(v, l))
        return LINE_SIDE_INTERSECT;
    else
    {
        DE_ASSERT(false);
        return LINE_SIDE_INTERSECT;
    }
}

// returns true if angle between line and given cornerExitNormal is in range (-45, 45)
bool lineInCornerAngleRange(const SubpixelLineSegment &line, const tcu::Vector<int64_t, 2> &cornerExitNormal)
{
    // v0 -> v1 has angle difference to cornerExitNormal in range (-45, 45)
    const tcu::Vector<int64_t, 2> v = line.direction();
    const int64_t dotProduct        = dot(v, cornerExitNormal);

    // dotProduct > |v1-v0|*|cornerExitNormal|/sqrt(2)
    if (dotProduct < 0)
        return false;
    return 2 * dotProduct * dotProduct > tcu::lengthSquared(v) * tcu::lengthSquared(cornerExitNormal);
}

// returns true if angle between line and given cornerExitNormal is in range (-135, 135)
bool lineInCornerOutsideAngleRange(const SubpixelLineSegment &line, const tcu::Vector<int64_t, 2> &cornerExitNormal)
{
    // v0 -> v1 has angle difference to cornerExitNormal in range (-135, 135)
    const tcu::Vector<int64_t, 2> v = line.direction();
    const int64_t dotProduct        = dot(v, cornerExitNormal);

    // dotProduct > -|v1-v0|*|cornerExitNormal|/sqrt(2)
    if (dotProduct >= 0)
        return true;
    return 2 * (-dotProduct) * (-dotProduct) < tcu::lengthSquared(v) * tcu::lengthSquared(cornerExitNormal);
}

bool doesLineSegmentExitDiamond(const SubpixelLineSegment &line, const tcu::Vector<int64_t, 2> &diamondCenter, int bits)
{
    DE_ASSERT(isTheCenterOfTheFragment(diamondCenter, bits));

    // Diamond Center is at diamondCenter in subpixel coords

    const int64_t halfPixel = 1ll << (bits - 1);

    // Reject distant diamonds early
    {
        const tcu::Vector<int64_t, 2> u = line.direction();
        const tcu::Vector<int64_t, 2> v = (diamondCenter - line.m_v0);
        const int64_t crossProduct      = (u.x() * v.y() - u.y() * v.x());

        // crossProduct = |p| |l| sin(theta)
        // distanceFromLine = |p| sin(theta)
        // => distanceFromLine = crossProduct / |l|
        //
        // |distanceFromLine| > C
        // => distanceFromLine^2 > C^2
        // => crossProduct^2 / |l|^2 > C^2
        // => crossProduct^2 > |l|^2 * C^2

        const int64_t floorSqrtMaxInt64 = 3037000499LL; //!< floor(sqrt(MAX_INT64))

        const int64_t broadRejectDistance        = 2 * halfPixel;
        const int64_t broadRejectDistanceSquared = broadRejectDistance * broadRejectDistance;
        const bool crossProductOverflows = (crossProduct > floorSqrtMaxInt64 || crossProduct < -floorSqrtMaxInt64);
        const int64_t crossProductSquared =
            (crossProductOverflows) ? (0) : (crossProduct * crossProduct); // avoid overflow
        const int64_t lineLengthSquared = tcu::lengthSquared(u);
        const bool limitValueCouldOverflow =
            ((64 - deClz64(lineLengthSquared)) + (64 - deClz64(broadRejectDistanceSquared))) > 63;
        const int64_t limitValue =
            (limitValueCouldOverflow) ? (0) : (lineLengthSquared * broadRejectDistanceSquared); // avoid overflow

        // only cross overflows
        if (crossProductOverflows && !limitValueCouldOverflow)
            return false;

        // both representable
        if (!crossProductOverflows && !limitValueCouldOverflow)
        {
            if (crossProductSquared > limitValue)
                return false;
        }
    }

    const struct DiamondBound
    {
        tcu::Vector<int64_t, 2> p0;
        tcu::Vector<int64_t, 2> p1;
        bool edgeInclusive; // would a point on the bound be inside of the region
    } bounds[] = {
        {diamondCenter + tcu::Vector<int64_t, 2>(0, -halfPixel), diamondCenter + tcu::Vector<int64_t, 2>(-halfPixel, 0),
         false},
        {diamondCenter + tcu::Vector<int64_t, 2>(-halfPixel, 0), diamondCenter + tcu::Vector<int64_t, 2>(0, halfPixel),
         false},
        {diamondCenter + tcu::Vector<int64_t, 2>(0, halfPixel), diamondCenter + tcu::Vector<int64_t, 2>(halfPixel, 0),
         true},
        {diamondCenter + tcu::Vector<int64_t, 2>(halfPixel, 0), diamondCenter + tcu::Vector<int64_t, 2>(0, -halfPixel),
         true},
    };

    const struct DiamondCorners
    {
        enum CORNER_EDGE_CASE_BEHAVIOR
        {
            CORNER_EDGE_CASE_NONE,              // if the line intersects just a corner, no entering or exiting
            CORNER_EDGE_CASE_HIT,               // if the line intersects just a corner, entering and exit
            CORNER_EDGE_CASE_HIT_FIRST_QUARTER, // if the line intersects just a corner and the line has either endpoint in (+X,-Y) direction (preturbing moves the line inside)
            CORNER_EDGE_CASE_HIT_SECOND_QUARTER // if the line intersects just a corner and the line has either endpoint in (+X,+Y) direction (preturbing moves the line inside)
        };
        enum CORNER_START_CASE_BEHAVIOR
        {
            CORNER_START_CASE_NONE, // the line starting point is outside, no exiting
            CORNER_START_CASE_OUTSIDE, // exit, if line does not intersect the region (preturbing moves the start point inside)
            CORNER_START_CASE_POSITIVE_Y_45, // exit, if line the angle of line vector and X-axis is in range (0, 45] in positive Y side.
            CORNER_START_CASE_NEGATIVE_Y_45 // exit, if line the angle of line vector and X-axis is in range [0, 45] in negative Y side.
        };
        enum CORNER_END_CASE_BEHAVIOR
        {
            CORNER_END_CASE_NONE,      // end is inside, no exiting (preturbing moves the line end inside)
            CORNER_END_CASE_DIRECTION, // exit, if line intersected the region (preturbing moves the line end outside)
            CORNER_END_CASE_DIRECTION_AND_FIRST_QUARTER, // exit, if line intersected the region, or line originates from (+X,-Y) direction (preturbing moves the line end outside)
            CORNER_END_CASE_DIRECTION_AND_SECOND_QUARTER // exit, if line intersected the region, or line originates from (+X,+Y) direction (preturbing moves the line end outside)
        };

        tcu::Vector<int64_t, 2> dp;
        bool pointInclusive; // would a point in this corner intersect with the region
        CORNER_EDGE_CASE_BEHAVIOR
        lineBehavior; // would a line segment going through this corner intersect with the region
        CORNER_START_CASE_BEHAVIOR startBehavior; // how the corner behaves if the start point at the corner
        CORNER_END_CASE_BEHAVIOR endBehavior;     // how the corner behaves if the end point at the corner
    } corners[] = {
        {tcu::Vector<int64_t, 2>(0, -halfPixel), false, DiamondCorners::CORNER_EDGE_CASE_HIT_SECOND_QUARTER,
         DiamondCorners::CORNER_START_CASE_POSITIVE_Y_45, DiamondCorners::CORNER_END_CASE_DIRECTION_AND_SECOND_QUARTER},
        {tcu::Vector<int64_t, 2>(-halfPixel, 0), false, DiamondCorners::CORNER_EDGE_CASE_NONE,
         DiamondCorners::CORNER_START_CASE_NONE, DiamondCorners::CORNER_END_CASE_DIRECTION},
        {tcu::Vector<int64_t, 2>(0, halfPixel), false, DiamondCorners::CORNER_EDGE_CASE_HIT_FIRST_QUARTER,
         DiamondCorners::CORNER_START_CASE_NEGATIVE_Y_45, DiamondCorners::CORNER_END_CASE_DIRECTION_AND_FIRST_QUARTER},
        {tcu::Vector<int64_t, 2>(halfPixel, 0), true, DiamondCorners::CORNER_EDGE_CASE_HIT,
         DiamondCorners::CORNER_START_CASE_OUTSIDE, DiamondCorners::CORNER_END_CASE_NONE},
    };

    // Corner cases at the corners
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(corners); ++ndx)
    {
        const tcu::Vector<int64_t, 2> p = diamondCenter + corners[ndx].dp;
        const bool intersectsAtCorner   = LineRasterUtil::vertexOnLineSegment(p, line);

        if (!intersectsAtCorner)
            continue;

        // line segment body intersects with the corner
        if (p != line.m_v0 && p != line.m_v1)
        {
            if (corners[ndx].lineBehavior == DiamondCorners::CORNER_EDGE_CASE_HIT)
                return true;

            // endpoint in (+X, -Y) (X or Y may be 0) direction <==> x*y <= 0
            if (corners[ndx].lineBehavior == DiamondCorners::CORNER_EDGE_CASE_HIT_FIRST_QUARTER &&
                (line.direction().x() * line.direction().y()) <= 0)
                return true;

            // endpoint in (+X, +Y) (Y > 0) direction <==> x*y > 0
            if (corners[ndx].lineBehavior == DiamondCorners::CORNER_EDGE_CASE_HIT_SECOND_QUARTER &&
                (line.direction().x() * line.direction().y()) > 0)
                return true;
        }

        // line exits the area at the corner
        if (lineInCornerAngleRange(line, corners[ndx].dp))
        {
            const bool startIsInside = corners[ndx].pointInclusive || p != line.m_v0;
            const bool endIsOutside  = !corners[ndx].pointInclusive || p != line.m_v1;

            // starting point is inside the region and end endpoint is outside
            if (startIsInside && endIsOutside)
                return true;
        }

        // line end is at the corner
        if (p == line.m_v1)
        {
            if (corners[ndx].endBehavior == DiamondCorners::CORNER_END_CASE_DIRECTION ||
                corners[ndx].endBehavior == DiamondCorners::CORNER_END_CASE_DIRECTION_AND_FIRST_QUARTER ||
                corners[ndx].endBehavior == DiamondCorners::CORNER_END_CASE_DIRECTION_AND_SECOND_QUARTER)
            {
                // did the line intersect the region
                if (lineInCornerAngleRange(line, corners[ndx].dp))
                    return true;
            }

            // due to the perturbed endpoint, lines at this the angle will cause and enter-exit pair
            if (corners[ndx].endBehavior == DiamondCorners::CORNER_END_CASE_DIRECTION_AND_FIRST_QUARTER &&
                line.direction().x() < 0 && line.direction().y() > 0)
                return true;
            if (corners[ndx].endBehavior == DiamondCorners::CORNER_END_CASE_DIRECTION_AND_SECOND_QUARTER &&
                line.direction().x() > 0 && line.direction().y() > 0)
                return true;
        }

        // line start is at the corner
        if (p == line.m_v0)
        {
            if (corners[ndx].startBehavior == DiamondCorners::CORNER_START_CASE_OUTSIDE)
            {
                // if the line is not going inside, it will exit
                if (lineInCornerOutsideAngleRange(line, corners[ndx].dp))
                    return true;
            }

            // exit, if line the angle between line vector and X-axis is in range (0, 45] in positive Y side.
            if (corners[ndx].startBehavior == DiamondCorners::CORNER_START_CASE_POSITIVE_Y_45 &&
                line.direction().x() > 0 && line.direction().y() > 0 && line.direction().y() <= line.direction().x())
                return true;

            // exit, if line the angle between line vector and X-axis is in range [0, 45] in negative Y side.
            if (corners[ndx].startBehavior == DiamondCorners::CORNER_START_CASE_NEGATIVE_Y_45 &&
                line.direction().x() > 0 && line.direction().y() <= 0 && -line.direction().y() <= line.direction().x())
                return true;
        }
    }

    // Does the line intersect boundary at the left == exits the diamond
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(bounds); ++ndx)
    {
        const bool startVertexInside =
            LineRasterUtil::vertexOnLeftSideOfLine(
                line.m_v0, LineRasterUtil::SubpixelLineSegment(bounds[ndx].p0, bounds[ndx].p1)) ||
            (bounds[ndx].edgeInclusive && LineRasterUtil::vertexOnLine(line.m_v0, LineRasterUtil::SubpixelLineSegment(
                                                                                      bounds[ndx].p0, bounds[ndx].p1)));
        const bool endVertexInside =
            LineRasterUtil::vertexOnLeftSideOfLine(
                line.m_v1, LineRasterUtil::SubpixelLineSegment(bounds[ndx].p0, bounds[ndx].p1)) ||
            (bounds[ndx].edgeInclusive && LineRasterUtil::vertexOnLine(line.m_v1, LineRasterUtil::SubpixelLineSegment(
                                                                                      bounds[ndx].p0, bounds[ndx].p1)));

        // start must be on inside this half space (left or at the inclusive boundary)
        if (!startVertexInside)
            continue;

        // end must be outside of this half-space (right or at non-inclusive boundary)
        if (endVertexInside)
            continue;

        // Does the line via v0 and v1 intersect the line segment p0-p1
        // <==> p0 and p1 are the different sides (LEFT, RIGHT) of the v0-v1 line.
        // Corners are not allowed, they are checked already
        LineRasterUtil::LINE_SIDE sideP0 = LineRasterUtil::getVertexSide(bounds[ndx].p0, line);
        LineRasterUtil::LINE_SIDE sideP1 = LineRasterUtil::getVertexSide(bounds[ndx].p1, line);

        if (sideP0 != LineRasterUtil::LINE_SIDE_INTERSECT && sideP1 != LineRasterUtil::LINE_SIDE_INTERSECT &&
            sideP0 != sideP1)
            return true;
    }

    return false;
}

} // namespace LineRasterUtil

TriangleRasterizer::TriangleRasterizer(const tcu::IVec4 &viewport, const int numSamples,
                                       const RasterizationState &state, const int subpixelBits)
    : m_viewport(viewport)
    , m_numSamples(numSamples)
    , m_winding(state.winding)
    , m_horizontalFill(state.horizontalFill)
    , m_verticalFill(state.verticalFill)
    , m_subpixelBits(subpixelBits)
    , m_face(FACETYPE_LAST)
    , m_viewportOrientation(state.viewportOrientation)
{
}

/*--------------------------------------------------------------------*//*!
 * \brief Initialize triangle rasterization
 * \param v0 Screen-space coordinates (x, y, z) and 1/w for vertex 0.
 * \param v1 Screen-space coordinates (x, y, z) and 1/w for vertex 1.
 * \param v2 Screen-space coordinates (x, y, z) and 1/w for vertex 2.
 *//*--------------------------------------------------------------------*/
void TriangleRasterizer::init(const tcu::Vec4 &v0, const tcu::Vec4 &v1, const tcu::Vec4 &v2)
{
    m_v0 = v0;
    m_v1 = v1;
    m_v2 = v2;

    // Positions in fixed-point coordinates.
    const int64_t x0 = toSubpixelCoord(v0.x(), m_subpixelBits);
    const int64_t y0 = toSubpixelCoord(v0.y(), m_subpixelBits);
    const int64_t x1 = toSubpixelCoord(v1.x(), m_subpixelBits);
    const int64_t y1 = toSubpixelCoord(v1.y(), m_subpixelBits);
    const int64_t x2 = toSubpixelCoord(v2.x(), m_subpixelBits);
    const int64_t y2 = toSubpixelCoord(v2.y(), m_subpixelBits);

    // Initialize edge functions.
    if (m_winding == WINDING_CCW)
    {
        initEdgeCCW(m_edge01, m_horizontalFill, m_verticalFill, x0, y0, x1, y1);
        initEdgeCCW(m_edge12, m_horizontalFill, m_verticalFill, x1, y1, x2, y2);
        initEdgeCCW(m_edge20, m_horizontalFill, m_verticalFill, x2, y2, x0, y0);
    }
    else
    {
        // Reverse edges
        initEdgeCCW(m_edge01, m_horizontalFill, m_verticalFill, x1, y1, x0, y0);
        initEdgeCCW(m_edge12, m_horizontalFill, m_verticalFill, x2, y2, x1, y1);
        initEdgeCCW(m_edge20, m_horizontalFill, m_verticalFill, x0, y0, x2, y2);
    }

    // Determine face.
    const int64_t s         = evaluateEdge(m_edge01, x2, y2);
    const bool positiveArea = (m_winding == WINDING_CCW) ? (s > 0) : (s < 0);

    if (m_viewportOrientation == VIEWPORTORIENTATION_UPPER_LEFT)
        m_face = positiveArea ? FACETYPE_BACK : FACETYPE_FRONT;
    else
        m_face = positiveArea ? FACETYPE_FRONT : FACETYPE_BACK;

    if (!positiveArea)
    {
        // Reverse edges so that we can use CCW area tests & interpolation
        reverseEdge(m_edge01);
        reverseEdge(m_edge12);
        reverseEdge(m_edge20);
    }

    // Bounding box
    const int64_t xMin = de::min(de::min(x0, x1), x2);
    const int64_t xMax = de::max(de::max(x0, x1), x2);
    const int64_t yMin = de::min(de::min(y0, y1), y2);
    const int64_t yMax = de::max(de::max(y0, y1), y2);

    m_bboxMin.x() = floorSubpixelToPixelCoord(xMin, m_subpixelBits, m_horizontalFill == FILL_LEFT);
    m_bboxMin.y() = floorSubpixelToPixelCoord(yMin, m_subpixelBits, m_verticalFill == FILL_BOTTOM);
    m_bboxMax.x() = ceilSubpixelToPixelCoord(xMax, m_subpixelBits, m_horizontalFill == FILL_RIGHT);
    m_bboxMax.y() = ceilSubpixelToPixelCoord(yMax, m_subpixelBits, m_verticalFill == FILL_TOP);

    // Clamp to viewport
    const int wX0 = m_viewport.x();
    const int wY0 = m_viewport.y();
    const int wX1 = wX0 + m_viewport.z() - 1;
    const int wY1 = wY0 + m_viewport.w() - 1;

    m_bboxMin.x() = de::clamp(m_bboxMin.x(), wX0, wX1);
    m_bboxMin.y() = de::clamp(m_bboxMin.y(), wY0, wY1);
    m_bboxMax.x() = de::clamp(m_bboxMax.x(), wX0, wX1);
    m_bboxMax.y() = de::clamp(m_bboxMax.y(), wY0, wY1);

    m_curPos = m_bboxMin;
}

void TriangleRasterizer::rasterizeSingleSample(FragmentPacket *const fragmentPackets, float *const depthValues,
                                               const int maxFragmentPackets, int &numPacketsRasterized)
{
    DE_ASSERT(maxFragmentPackets > 0);

    const uint64_t halfPixel = 1ll << (m_subpixelBits - 1);
    int packetNdx            = 0;

    // For depth interpolation; given barycentrics A, B, C = (1 - A - B)
    // we can reformulate the usual z = z0*A + z1*B + z2*C into more
    // stable equation z = A*(z0 - z2) + B*(z1 - z2) + z2.
    const float za = m_v0.z() - m_v2.z();
    const float zb = m_v1.z() - m_v2.z();
    const float zc = m_v2.z();

    while (m_curPos.y() <= m_bboxMax.y() && packetNdx < maxFragmentPackets)
    {
        const int x0 = m_curPos.x();
        const int y0 = m_curPos.y();

        // Subpixel coords
        const int64_t sx0 = toSubpixelCoord(x0, m_subpixelBits) + halfPixel;
        const int64_t sx1 = toSubpixelCoord(x0 + 1, m_subpixelBits) + halfPixel;
        const int64_t sy0 = toSubpixelCoord(y0, m_subpixelBits) + halfPixel;
        const int64_t sy1 = toSubpixelCoord(y0 + 1, m_subpixelBits) + halfPixel;

        const int64_t sx[4] = {sx0, sx1, sx0, sx1};
        const int64_t sy[4] = {sy0, sy0, sy1, sy1};

        // Viewport test
        const bool outX1 = x0 + 1 == m_viewport.x() + m_viewport.z();
        const bool outY1 = y0 + 1 == m_viewport.y() + m_viewport.w();

        DE_ASSERT(x0 < m_viewport.x() + m_viewport.z());
        DE_ASSERT(y0 < m_viewport.y() + m_viewport.w());

        // Edge values
        tcu::Vector<int64_t, 4> e01;
        tcu::Vector<int64_t, 4> e12;
        tcu::Vector<int64_t, 4> e20;

        // Coverage
        uint64_t coverage = 0;

        // Evaluate edge values
        for (int i = 0; i < 4; i++)
        {
            e01[i] = evaluateEdge(m_edge01, sx[i], sy[i]);
            e12[i] = evaluateEdge(m_edge12, sx[i], sy[i]);
            e20[i] = evaluateEdge(m_edge20, sx[i], sy[i]);
        }

        // Compute coverage mask
        coverage = setCoverageValue(coverage, 1, 0, 0, 0,
                                    isInsideCCW(m_edge01, e01[0]) && isInsideCCW(m_edge12, e12[0]) &&
                                        isInsideCCW(m_edge20, e20[0]));
        coverage = setCoverageValue(coverage, 1, 1, 0, 0,
                                    !outX1 && isInsideCCW(m_edge01, e01[1]) && isInsideCCW(m_edge12, e12[1]) &&
                                        isInsideCCW(m_edge20, e20[1]));
        coverage = setCoverageValue(coverage, 1, 0, 1, 0,
                                    !outY1 && isInsideCCW(m_edge01, e01[2]) && isInsideCCW(m_edge12, e12[2]) &&
                                        isInsideCCW(m_edge20, e20[2]));
        coverage = setCoverageValue(coverage, 1, 1, 1, 0,
                                    !outX1 && !outY1 && isInsideCCW(m_edge01, e01[3]) &&
                                        isInsideCCW(m_edge12, e12[3]) && isInsideCCW(m_edge20, e20[3]));

        // Advance to next location
        m_curPos.x() += 2;
        if (m_curPos.x() > m_bboxMax.x())
        {
            m_curPos.y() += 2;
            m_curPos.x() = m_bboxMin.x();
        }

        if (coverage == 0)
            continue; // Discard.

        // Floating-point edge values for barycentrics etc.
        const tcu::Vec4 e01f = e01.asFloat();
        const tcu::Vec4 e12f = e12.asFloat();
        const tcu::Vec4 e20f = e20.asFloat();

        // Compute depth values.
        if (depthValues)
        {
            const tcu::Vec4 edgeSum = e01f + e12f + e20f;
            const tcu::Vec4 z0      = e12f / edgeSum;
            const tcu::Vec4 z1      = e20f / edgeSum;

            depthValues[packetNdx * 4 + 0] = z0[0] * za + z1[0] * zb + zc;
            depthValues[packetNdx * 4 + 1] = z0[1] * za + z1[1] * zb + zc;
            depthValues[packetNdx * 4 + 2] = z0[2] * za + z1[2] * zb + zc;
            depthValues[packetNdx * 4 + 3] = z0[3] * za + z1[3] * zb + zc;
        }

        // Compute barycentrics and write out fragment packet
        {
            FragmentPacket &packet = fragmentPackets[packetNdx];

            const tcu::Vec4 b0   = e12f * m_v0.w();
            const tcu::Vec4 b1   = e20f * m_v1.w();
            const tcu::Vec4 b2   = e01f * m_v2.w();
            const tcu::Vec4 bSum = b0 + b1 + b2;

            packet.position       = tcu::IVec2(x0, y0);
            packet.coverage       = coverage;
            packet.barycentric[0] = b0 / bSum;
            packet.barycentric[1] = b1 / bSum;
            packet.barycentric[2] = 1.0f - packet.barycentric[0] - packet.barycentric[1];

            packetNdx += 1;
        }
    }

    DE_ASSERT(packetNdx <= maxFragmentPackets);
    numPacketsRasterized = packetNdx;
}

// Sample positions - ordered as (x, y) list.
static const float s_samplePts2[] = {0.3f, 0.3f, 0.7f, 0.7f};

static const float s_samplePts4[] = {0.25f, 0.25f, 0.75f, 0.25f, 0.25f, 0.75f, 0.75f, 0.75f};

static const float s_samplePts8[] = {7.f / 16.f,  9.f / 16.f,  9.f / 16.f, 13.f / 16.f, 11.f / 16.f, 3.f / 16.f,
                                     13.f / 16.f, 11.f / 16.f, 1.f / 16.f, 7.f / 16.f,  5.f / 16.f,  1.f / 16.f,
                                     15.f / 16.f, 5.f / 16.f,  3.f / 16.f, 15.f / 16.f};

static const float s_samplePts16[] = {1.f / 8.f, 1.f / 8.f, 3.f / 8.f, 1.f / 8.f, 5.f / 8.f, 1.f / 8.f, 7.f / 8.f,
                                      1.f / 8.f, 1.f / 8.f, 3.f / 8.f, 3.f / 8.f, 3.f / 8.f, 5.f / 8.f, 3.f / 8.f,
                                      7.f / 8.f, 3.f / 8.f, 1.f / 8.f, 5.f / 8.f, 3.f / 8.f, 5.f / 8.f, 5.f / 8.f,
                                      5.f / 8.f, 7.f / 8.f, 5.f / 8.f, 1.f / 8.f, 7.f / 8.f, 3.f / 8.f, 7.f / 8.f,
                                      5.f / 8.f, 7.f / 8.f, 7.f / 8.f, 7.f / 8.f};

template <int NumSamples>
void TriangleRasterizer::rasterizeMultiSample(FragmentPacket *const fragmentPackets, float *const depthValues,
                                              const int maxFragmentPackets, int &numPacketsRasterized)
{
    DE_ASSERT(maxFragmentPackets > 0);

    // Big enough to hold maximum multisample count
    int64_t samplePos[DE_LENGTH_OF_ARRAY(s_samplePts16)];
    const float *samplePts   = nullptr;
    const uint64_t halfPixel = 1ll << (m_subpixelBits - 1);
    int packetNdx            = 0;

    // For depth interpolation, see rasterizeSingleSample
    const float za = m_v0.z() - m_v2.z();
    const float zb = m_v1.z() - m_v2.z();
    const float zc = m_v2.z();

    switch (NumSamples)
    {
    case 2:
        samplePts = s_samplePts2;
        break;
    case 4:
        samplePts = s_samplePts4;
        break;
    case 8:
        samplePts = s_samplePts8;
        break;
    case 16:
        samplePts = s_samplePts16;
        break;
    default:
        DE_ASSERT(false);
    }

    for (int c = 0; c < NumSamples * 2; ++c)
        samplePos[c] = toSubpixelCoord(samplePts[c], m_subpixelBits);

    while (m_curPos.y() <= m_bboxMax.y() && packetNdx < maxFragmentPackets)
    {
        const int x0 = m_curPos.x();
        const int y0 = m_curPos.y();

        // Base subpixel coords
        const int64_t sx0 = toSubpixelCoord(x0, m_subpixelBits);
        const int64_t sx1 = toSubpixelCoord(x0 + 1, m_subpixelBits);
        const int64_t sy0 = toSubpixelCoord(y0, m_subpixelBits);
        const int64_t sy1 = toSubpixelCoord(y0 + 1, m_subpixelBits);

        const int64_t sx[4] = {sx0, sx1, sx0, sx1};
        const int64_t sy[4] = {sy0, sy0, sy1, sy1};

        // Viewport test
        const bool outX1 = x0 + 1 == m_viewport.x() + m_viewport.z();
        const bool outY1 = y0 + 1 == m_viewport.y() + m_viewport.w();

        DE_ASSERT(x0 < m_viewport.x() + m_viewport.z());
        DE_ASSERT(y0 < m_viewport.y() + m_viewport.w());

        // Edge values
        tcu::Vector<int64_t, 4> e01[NumSamples];
        tcu::Vector<int64_t, 4> e12[NumSamples];
        tcu::Vector<int64_t, 4> e20[NumSamples];

        // Coverage
        uint64_t coverage = 0;

        // Evaluate edge values at sample positions
        for (int sampleNdx = 0; sampleNdx < NumSamples; sampleNdx++)
        {
            const int64_t ox = samplePos[sampleNdx * 2 + 0];
            const int64_t oy = samplePos[sampleNdx * 2 + 1];

            for (int fragNdx = 0; fragNdx < 4; fragNdx++)
            {
                e01[sampleNdx][fragNdx] = evaluateEdge(m_edge01, sx[fragNdx] + ox, sy[fragNdx] + oy);
                e12[sampleNdx][fragNdx] = evaluateEdge(m_edge12, sx[fragNdx] + ox, sy[fragNdx] + oy);
                e20[sampleNdx][fragNdx] = evaluateEdge(m_edge20, sx[fragNdx] + ox, sy[fragNdx] + oy);
            }
        }

        // Compute coverage mask
        for (int sampleNdx = 0; sampleNdx < NumSamples; sampleNdx++)
        {
            coverage =
                setCoverageValue(coverage, NumSamples, 0, 0, sampleNdx,
                                 isInsideCCW(m_edge01, e01[sampleNdx][0]) && isInsideCCW(m_edge12, e12[sampleNdx][0]) &&
                                     isInsideCCW(m_edge20, e20[sampleNdx][0]));
            coverage = setCoverageValue(coverage, NumSamples, 1, 0, sampleNdx,
                                        !outX1 && isInsideCCW(m_edge01, e01[sampleNdx][1]) &&
                                            isInsideCCW(m_edge12, e12[sampleNdx][1]) &&
                                            isInsideCCW(m_edge20, e20[sampleNdx][1]));
            coverage = setCoverageValue(coverage, NumSamples, 0, 1, sampleNdx,
                                        !outY1 && isInsideCCW(m_edge01, e01[sampleNdx][2]) &&
                                            isInsideCCW(m_edge12, e12[sampleNdx][2]) &&
                                            isInsideCCW(m_edge20, e20[sampleNdx][2]));
            coverage = setCoverageValue(coverage, NumSamples, 1, 1, sampleNdx,
                                        !outX1 && !outY1 && isInsideCCW(m_edge01, e01[sampleNdx][3]) &&
                                            isInsideCCW(m_edge12, e12[sampleNdx][3]) &&
                                            isInsideCCW(m_edge20, e20[sampleNdx][3]));
        }

        // Advance to next location
        m_curPos.x() += 2;
        if (m_curPos.x() > m_bboxMax.x())
        {
            m_curPos.y() += 2;
            m_curPos.x() = m_bboxMin.x();
        }

        if (coverage == 0)
            continue; // Discard.

        // Compute depth values.
        if (depthValues)
        {
            for (int sampleNdx = 0; sampleNdx < NumSamples; sampleNdx++)
            {
                // Floating-point edge values at sample coordinates.
                const tcu::Vec4 &e01f = e01[sampleNdx].asFloat();
                const tcu::Vec4 &e12f = e12[sampleNdx].asFloat();
                const tcu::Vec4 &e20f = e20[sampleNdx].asFloat();

                const tcu::Vec4 edgeSum = e01f + e12f + e20f;
                const tcu::Vec4 z0      = e12f / edgeSum;
                const tcu::Vec4 z1      = e20f / edgeSum;

                depthValues[(packetNdx * 4 + 0) * NumSamples + sampleNdx] = z0[0] * za + z1[0] * zb + zc;
                depthValues[(packetNdx * 4 + 1) * NumSamples + sampleNdx] = z0[1] * za + z1[1] * zb + zc;
                depthValues[(packetNdx * 4 + 2) * NumSamples + sampleNdx] = z0[2] * za + z1[2] * zb + zc;
                depthValues[(packetNdx * 4 + 3) * NumSamples + sampleNdx] = z0[3] * za + z1[3] * zb + zc;
            }
        }

        // Compute barycentrics and write out fragment packet
        {
            FragmentPacket &packet = fragmentPackets[packetNdx];

            // Floating-point edge values at pixel center.
            tcu::Vec4 e01f;
            tcu::Vec4 e12f;
            tcu::Vec4 e20f;

            for (int i = 0; i < 4; i++)
            {
                e01f[i] = float(evaluateEdge(m_edge01, sx[i] + halfPixel, sy[i] + halfPixel));
                e12f[i] = float(evaluateEdge(m_edge12, sx[i] + halfPixel, sy[i] + halfPixel));
                e20f[i] = float(evaluateEdge(m_edge20, sx[i] + halfPixel, sy[i] + halfPixel));
            }

            // Barycentrics & scale.
            const tcu::Vec4 b0   = e12f * m_v0.w();
            const tcu::Vec4 b1   = e20f * m_v1.w();
            const tcu::Vec4 b2   = e01f * m_v2.w();
            const tcu::Vec4 bSum = b0 + b1 + b2;

            packet.position       = tcu::IVec2(x0, y0);
            packet.coverage       = coverage;
            packet.barycentric[0] = b0 / bSum;
            packet.barycentric[1] = b1 / bSum;
            packet.barycentric[2] = 1.0f - packet.barycentric[0] - packet.barycentric[1];

            packetNdx += 1;
        }
    }

    DE_ASSERT(packetNdx <= maxFragmentPackets);
    numPacketsRasterized = packetNdx;
}

void TriangleRasterizer::rasterize(FragmentPacket *const fragmentPackets, float *const depthValues,
                                   const int maxFragmentPackets, int &numPacketsRasterized)
{
    DE_ASSERT(maxFragmentPackets > 0);

    switch (m_numSamples)
    {
    case 1:
        rasterizeSingleSample(fragmentPackets, depthValues, maxFragmentPackets, numPacketsRasterized);
        break;
    case 2:
        rasterizeMultiSample<2>(fragmentPackets, depthValues, maxFragmentPackets, numPacketsRasterized);
        break;
    case 4:
        rasterizeMultiSample<4>(fragmentPackets, depthValues, maxFragmentPackets, numPacketsRasterized);
        break;
    case 8:
        rasterizeMultiSample<8>(fragmentPackets, depthValues, maxFragmentPackets, numPacketsRasterized);
        break;
    case 16:
        rasterizeMultiSample<16>(fragmentPackets, depthValues, maxFragmentPackets, numPacketsRasterized);
        break;
    default:
        DE_ASSERT(false);
    }
}

SingleSampleLineRasterizer::SingleSampleLineRasterizer(const tcu::IVec4 &viewport, const int subpixelBits)
    : m_viewport(viewport)
    , m_subpixelBits(subpixelBits)
    , m_curRowFragment(0)
    , m_lineWidth(0.0f)
    , m_stippleCounter(0)
{
}

SingleSampleLineRasterizer::~SingleSampleLineRasterizer(void)
{
}

void SingleSampleLineRasterizer::init(const tcu::Vec4 &v0, const tcu::Vec4 &v1, float lineWidth, uint32_t stippleFactor,
                                      uint16_t stipplePattern)
{
    const bool isXMajor = de::abs((v1 - v0).x()) >= de::abs((v1 - v0).y());

    // Bounding box \note: with wide lines, the line is actually moved as in the spec
    const int32_t lineWidthPixels = (lineWidth > 1.0f) ? (int32_t)floor(lineWidth + 0.5f) : 1;

    const tcu::Vector<int64_t, 2> widthOffset =
        (isXMajor ? tcu::Vector<int64_t, 2>(0, -1) : tcu::Vector<int64_t, 2>(-1, 0)) *
        (toSubpixelCoord(lineWidthPixels - 1, m_subpixelBits) / 2);

    const int64_t x0 = toSubpixelCoord(v0.x(), m_subpixelBits) + widthOffset.x();
    const int64_t y0 = toSubpixelCoord(v0.y(), m_subpixelBits) + widthOffset.y();
    const int64_t x1 = toSubpixelCoord(v1.x(), m_subpixelBits) + widthOffset.x();
    const int64_t y1 = toSubpixelCoord(v1.y(), m_subpixelBits) + widthOffset.y();

    // line endpoints might be perturbed, add some margin
    const int64_t xMin = de::min(x0, x1) - toSubpixelCoord(1, m_subpixelBits);
    const int64_t xMax = de::max(x0, x1) + toSubpixelCoord(1, m_subpixelBits);
    const int64_t yMin = de::min(y0, y1) - toSubpixelCoord(1, m_subpixelBits);
    const int64_t yMax = de::max(y0, y1) + toSubpixelCoord(1, m_subpixelBits);

    // Remove invisible area

    if (isXMajor)
    {
        // clamp to viewport in major direction
        m_bboxMin.x() = de::clamp(floorSubpixelToPixelCoord(xMin, m_subpixelBits, true), m_viewport.x(),
                                  m_viewport.x() + m_viewport.z() - 1);
        m_bboxMax.x() = de::clamp(ceilSubpixelToPixelCoord(xMax, m_subpixelBits, true), m_viewport.x(),
                                  m_viewport.x() + m_viewport.z() - 1);

        // clamp to padded viewport in minor direction (wide lines might bleed over viewport in minor direction)
        m_bboxMin.y() = de::clamp(floorSubpixelToPixelCoord(yMin, m_subpixelBits, true),
                                  m_viewport.y() - lineWidthPixels, m_viewport.y() + m_viewport.w() - 1);
        m_bboxMax.y() = de::clamp(ceilSubpixelToPixelCoord(yMax, m_subpixelBits, true),
                                  m_viewport.y() - lineWidthPixels, m_viewport.y() + m_viewport.w() - 1);
    }
    else
    {
        // clamp to viewport in major direction
        m_bboxMin.y() = de::clamp(floorSubpixelToPixelCoord(yMin, m_subpixelBits, true), m_viewport.y(),
                                  m_viewport.y() + m_viewport.w() - 1);
        m_bboxMax.y() = de::clamp(ceilSubpixelToPixelCoord(yMax, m_subpixelBits, true), m_viewport.y(),
                                  m_viewport.y() + m_viewport.w() - 1);

        // clamp to padded viewport in minor direction (wide lines might bleed over viewport in minor direction)
        m_bboxMin.x() = de::clamp(floorSubpixelToPixelCoord(xMin, m_subpixelBits, true),
                                  m_viewport.x() - lineWidthPixels, m_viewport.x() + m_viewport.z() - 1);
        m_bboxMax.x() = de::clamp(ceilSubpixelToPixelCoord(xMax, m_subpixelBits, true),
                                  m_viewport.x() - lineWidthPixels, m_viewport.x() + m_viewport.z() - 1);
    }

    m_lineWidth = lineWidth;

    m_v0 = v0;
    m_v1 = v1;

    // Choose direction of traversal and whether to start at bbox min or max. Direction matters
    // for the stipple counter.
    int xDelta = (m_v1 - m_v0).x() > 0 ? 1 : -1;
    int yDelta = (m_v1 - m_v0).y() > 0 ? 1 : -1;

    m_curPos.x() = xDelta > 0 ? m_bboxMin.x() : m_bboxMax.x();
    m_curPos.y() = yDelta > 0 ? m_bboxMin.y() : m_bboxMax.y();

    m_curRowFragment = 0;
    m_stippleFactor  = stippleFactor;
    m_stipplePattern = stipplePattern;
}

void SingleSampleLineRasterizer::rasterize(FragmentPacket *const fragmentPackets, float *const depthValues,
                                           const int maxFragmentPackets, int &numPacketsRasterized)
{
    DE_ASSERT(maxFragmentPackets > 0);

    const int64_t halfPixel         = 1ll << (m_subpixelBits - 1);
    const int32_t lineWidth         = (m_lineWidth > 1.0f) ? deFloorFloatToInt32(m_lineWidth + 0.5f) : 1;
    const bool isXMajor             = de::abs((m_v1 - m_v0).x()) >= de::abs((m_v1 - m_v0).y());
    const tcu::IVec2 minorDirection = (isXMajor) ? (tcu::IVec2(0, 1)) : (tcu::IVec2(1, 0));
    const int minViewportLimit      = (isXMajor) ? (m_viewport.y()) : (m_viewport.x());
    const int maxViewportLimit = (isXMajor) ? (m_viewport.y() + m_viewport.w()) : (m_viewport.x() + m_viewport.z());
    const tcu::Vector<int64_t, 2> widthOffset =
        -minorDirection.cast<int64_t>() * (toSubpixelCoord(lineWidth - 1, m_subpixelBits) / 2);
    const tcu::Vector<int64_t, 2> pa = LineRasterUtil::toSubpixelVector(m_v0.xy(), m_subpixelBits) + widthOffset;
    const tcu::Vector<int64_t, 2> pb = LineRasterUtil::toSubpixelVector(m_v1.xy(), m_subpixelBits) + widthOffset;
    const LineRasterUtil::SubpixelLineSegment line = LineRasterUtil::SubpixelLineSegment(pa, pb);

    int packetNdx = 0;
    int xDelta    = (m_v1 - m_v0).x() > 0 ? 1 : -1;
    int yDelta    = (m_v1 - m_v0).y() > 0 ? 1 : -1;

    while (m_curPos.y() <= m_bboxMax.y() && m_curPos.y() >= m_bboxMin.y() && packetNdx < maxFragmentPackets)
    {
        const tcu::Vector<int64_t, 2> diamondPosition =
            LineRasterUtil::toSubpixelVector(m_curPos, m_subpixelBits) + tcu::Vector<int64_t, 2>(halfPixel, halfPixel);

        // Should current fragment be drawn? == does the segment exit this diamond?
        if (LineRasterUtil::doesLineSegmentExitDiamond(line, diamondPosition, m_subpixelBits))
        {
            const tcu::Vector<int64_t, 2> pr = diamondPosition;
            const float t =
                tcu::dot((pr - pa).asFloat(), (pb - pa).asFloat()) / tcu::lengthSquared(pb.asFloat() - pa.asFloat());

            // Rasterize on only fragments that are would end up in the viewport (i.e. visible)
            const int fragmentLocation = (isXMajor) ? (m_curPos.y()) : (m_curPos.x());
            const int rowFragBegin     = de::max(0, minViewportLimit - fragmentLocation);
            const int rowFragEnd       = de::min(maxViewportLimit - fragmentLocation, lineWidth);

            int stippleBit   = (m_stippleCounter / m_stippleFactor) % 16;
            bool stipplePass = (m_stipplePattern & (1 << stippleBit)) != 0;
            m_stippleCounter++;

            if (stipplePass)
            {
                // Wide lines require multiple fragments.
                for (; rowFragBegin + m_curRowFragment < rowFragEnd; m_curRowFragment++)
                {
                    const int replicationId      = rowFragBegin + m_curRowFragment;
                    const tcu::IVec2 fragmentPos = m_curPos + minorDirection * replicationId;

                    // We only rasterize visible area
                    DE_ASSERT(LineRasterUtil::inViewport(fragmentPos, m_viewport));

                    // Compute depth values.
                    if (depthValues)
                    {
                        const float za = m_v0.z();
                        const float zb = m_v1.z();

                        depthValues[packetNdx * 4 + 0] = (1 - t) * za + t * zb;
                        depthValues[packetNdx * 4 + 1] = 0;
                        depthValues[packetNdx * 4 + 2] = 0;
                        depthValues[packetNdx * 4 + 3] = 0;
                    }

                    {
                        // output this fragment
                        // \note In order to make consistent output with multisampled line rasterization, output "barycentric" coordinates
                        FragmentPacket &packet = fragmentPackets[packetNdx];

                        const tcu::Vec4 b0    = tcu::Vec4(1 - t);
                        const tcu::Vec4 b1    = tcu::Vec4(t);
                        const tcu::Vec4 ooSum = 1.0f / (b0 + b1);

                        packet.position       = fragmentPos;
                        packet.coverage       = getCoverageBit(1, 0, 0, 0);
                        packet.barycentric[0] = b0 * ooSum;
                        packet.barycentric[1] = b1 * ooSum;
                        packet.barycentric[2] = tcu::Vec4(0.0f);

                        packetNdx += 1;
                    }

                    if (packetNdx == maxFragmentPackets)
                    {
                        m_curRowFragment++; // don't redraw this fragment again next time
                        m_stippleCounter--; // reuse same stipple counter next time
                        numPacketsRasterized = packetNdx;
                        return;
                    }
                }

                m_curRowFragment = 0;
            }
        }

        m_curPos.x() += xDelta;
        if (m_curPos.x() > m_bboxMax.x() || m_curPos.x() < m_bboxMin.x())
        {
            m_curPos.y() += yDelta;
            m_curPos.x() = xDelta > 0 ? m_bboxMin.x() : m_bboxMax.x();
        }
    }

    DE_ASSERT(packetNdx <= maxFragmentPackets);
    numPacketsRasterized = packetNdx;
}

MultiSampleLineRasterizer::MultiSampleLineRasterizer(const int numSamples, const tcu::IVec4 &viewport,
                                                     const int subpixelBits)
    : m_numSamples(numSamples)
    , m_triangleRasterizer0(viewport, m_numSamples, RasterizationState(), subpixelBits)
    , m_triangleRasterizer1(viewport, m_numSamples, RasterizationState(), subpixelBits)
{
}

MultiSampleLineRasterizer::~MultiSampleLineRasterizer()
{
}

void MultiSampleLineRasterizer::init(const tcu::Vec4 &v0, const tcu::Vec4 &v1, float lineWidth)
{
    // allow creation of single sampled rasterizer objects but do not allow using them
    DE_ASSERT(m_numSamples > 1);

    const tcu::Vec2 lineVec = tcu::Vec2(tcu::Vec4(v1).xy()) - tcu::Vec2(tcu::Vec4(v0).xy());
    const tcu::Vec2 normal2 = tcu::normalize(tcu::Vec2(-lineVec[1], lineVec[0]));
    const tcu::Vec4 normal4 = tcu::Vec4(normal2.x(), normal2.y(), 0, 0);
    const float offset      = lineWidth / 2.0f;

    const tcu::Vec4 p0 = v0 + normal4 * offset;
    const tcu::Vec4 p1 = v0 - normal4 * offset;
    const tcu::Vec4 p2 = v1 - normal4 * offset;
    const tcu::Vec4 p3 = v1 + normal4 * offset;

    // Edge 0 -> 1 is always along the line and edge 1 -> 2 is in 90 degree angle to the line
    m_triangleRasterizer0.init(p0, p3, p2);
    m_triangleRasterizer1.init(p2, p1, p0);
}

void MultiSampleLineRasterizer::rasterize(FragmentPacket *const fragmentPackets, float *const depthValues,
                                          const int maxFragmentPackets, int &numPacketsRasterized)
{
    DE_ASSERT(maxFragmentPackets > 0);

    m_triangleRasterizer0.rasterize(fragmentPackets, depthValues, maxFragmentPackets, numPacketsRasterized);

    // Remove 3rd barycentric value and rebalance. Lines do not have non-zero barycentric at index 2
    for (int packNdx = 0; packNdx < numPacketsRasterized; ++packNdx)
        for (int fragNdx = 0; fragNdx < 4; fragNdx++)
        {
            float removedValue                               = fragmentPackets[packNdx].barycentric[2][fragNdx];
            fragmentPackets[packNdx].barycentric[2][fragNdx] = 0.0f;
            fragmentPackets[packNdx].barycentric[1][fragNdx] += removedValue;
        }

    // rasterizer 0 filled the whole buffer?
    if (numPacketsRasterized == maxFragmentPackets)
        return;

    {
        FragmentPacket *const nextFragmentPackets = fragmentPackets + numPacketsRasterized;
        float *nextDepthValues    = (depthValues) ? (depthValues + 4 * numPacketsRasterized * m_numSamples) : (nullptr);
        int numPacketsRasterized2 = 0;

        m_triangleRasterizer1.rasterize(nextFragmentPackets, nextDepthValues, maxFragmentPackets - numPacketsRasterized,
                                        numPacketsRasterized2);

        numPacketsRasterized += numPacketsRasterized2;

        // Fix swapped barycentrics in the second triangle
        for (int packNdx = 0; packNdx < numPacketsRasterized2; ++packNdx)
            for (int fragNdx = 0; fragNdx < 4; fragNdx++)
            {
                float removedValue = nextFragmentPackets[packNdx].barycentric[2][fragNdx];
                nextFragmentPackets[packNdx].barycentric[2][fragNdx] = 0.0f;
                nextFragmentPackets[packNdx].barycentric[1][fragNdx] += removedValue;

                // edge has reversed direction
                std::swap(nextFragmentPackets[packNdx].barycentric[0][fragNdx],
                          nextFragmentPackets[packNdx].barycentric[1][fragNdx]);
            }
    }
}

LineExitDiamondGenerator::LineExitDiamondGenerator(const int subpixelBits) : m_subpixelBits(subpixelBits)
{
}

LineExitDiamondGenerator::~LineExitDiamondGenerator(void)
{
}

void LineExitDiamondGenerator::init(const tcu::Vec4 &v0, const tcu::Vec4 &v1)
{
    const int64_t x0 = toSubpixelCoord(v0.x(), m_subpixelBits);
    const int64_t y0 = toSubpixelCoord(v0.y(), m_subpixelBits);
    const int64_t x1 = toSubpixelCoord(v1.x(), m_subpixelBits);
    const int64_t y1 = toSubpixelCoord(v1.y(), m_subpixelBits);

    // line endpoints might be perturbed, add some margin
    const int64_t xMin = de::min(x0, x1) - toSubpixelCoord(1, m_subpixelBits);
    const int64_t xMax = de::max(x0, x1) + toSubpixelCoord(1, m_subpixelBits);
    const int64_t yMin = de::min(y0, y1) - toSubpixelCoord(1, m_subpixelBits);
    const int64_t yMax = de::max(y0, y1) + toSubpixelCoord(1, m_subpixelBits);

    m_bboxMin.x() = floorSubpixelToPixelCoord(xMin, m_subpixelBits, true);
    m_bboxMin.y() = floorSubpixelToPixelCoord(yMin, m_subpixelBits, true);
    m_bboxMax.x() = ceilSubpixelToPixelCoord(xMax, m_subpixelBits, true);
    m_bboxMax.y() = ceilSubpixelToPixelCoord(yMax, m_subpixelBits, true);

    m_v0 = v0;
    m_v1 = v1;

    m_curPos = m_bboxMin;
}

void LineExitDiamondGenerator::rasterize(LineExitDiamond *const lineDiamonds, const int maxDiamonds, int &numWritten)
{
    DE_ASSERT(maxDiamonds > 0);

    const int64_t halfPixel                        = 1ll << (m_subpixelBits - 1);
    const tcu::Vector<int64_t, 2> pa               = LineRasterUtil::toSubpixelVector(m_v0.xy(), m_subpixelBits);
    const tcu::Vector<int64_t, 2> pb               = LineRasterUtil::toSubpixelVector(m_v1.xy(), m_subpixelBits);
    const LineRasterUtil::SubpixelLineSegment line = LineRasterUtil::SubpixelLineSegment(pa, pb);

    int diamondNdx = 0;

    while (m_curPos.y() <= m_bboxMax.y() && diamondNdx < maxDiamonds)
    {
        const tcu::Vector<int64_t, 2> diamondPosition =
            LineRasterUtil::toSubpixelVector(m_curPos, m_subpixelBits) + tcu::Vector<int64_t, 2>(halfPixel, halfPixel);

        if (LineRasterUtil::doesLineSegmentExitDiamond(line, diamondPosition, m_subpixelBits))
        {
            LineExitDiamond &packet = lineDiamonds[diamondNdx];
            packet.position         = m_curPos;
            ++diamondNdx;
        }

        ++m_curPos.x();
        if (m_curPos.x() > m_bboxMax.x())
        {
            ++m_curPos.y();
            m_curPos.x() = m_bboxMin.x();
        }
    }

    DE_ASSERT(diamondNdx <= maxDiamonds);
    numWritten = diamondNdx;
}

} // namespace rr
