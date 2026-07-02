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
 * \brief Reference renderer interface.
 *//*--------------------------------------------------------------------*/

#include "rrRenderer.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuFloat.hpp"
#include "rrPrimitiveAssembler.hpp"
#include "rrFragmentOperations.hpp"
#include "rrRasterizer.hpp"
#include "deMemory.h"

#include <set>
#include <limits>

namespace rr
{
namespace
{

typedef double ClipFloat; // floating point type used in clipping

typedef tcu::Vector<ClipFloat, 4> ClipVec4;

struct RasterizationInternalBuffers
{
    std::vector<FragmentPacket> fragmentPackets;
    std::vector<GenericVec4> shaderOutputs;
    std::vector<GenericVec4> shaderOutputsSrc1;
    std::vector<Fragment> shadedFragments;
    float *fragmentDepthBuffer;
};

uint32_t readIndexArray(const IndexType type, const void *ptr, size_t ndx)
{
    switch (type)
    {
    case INDEXTYPE_UINT8:
        return ((const uint8_t *)ptr)[ndx];

    case INDEXTYPE_UINT16:
    {
        uint16_t retVal;
        deMemcpy(&retVal, (const uint8_t *)ptr + ndx * sizeof(uint16_t), sizeof(uint16_t));

        return retVal;
    }

    case INDEXTYPE_UINT32:
    {
        uint32_t retVal;
        deMemcpy(&retVal, (const uint8_t *)ptr + ndx * sizeof(uint32_t), sizeof(uint32_t));

        return retVal;
    }

    default:
        DE_ASSERT(false);
        return 0;
    }
}

tcu::IVec4 getBufferSize(const rr::MultisampleConstPixelBufferAccess &multisampleBuffer)
{
    return tcu::IVec4(0, 0, multisampleBuffer.raw().getHeight(), multisampleBuffer.raw().getDepth());
}

bool isEmpty(const rr::MultisampleConstPixelBufferAccess &access)
{
    return access.raw().getWidth() == 0 || access.raw().getHeight() == 0 || access.raw().getDepth() == 0;
}

struct DrawContext
{
    int primitiveID;

    DrawContext(void) : primitiveID(0)
    {
    }
};

/*--------------------------------------------------------------------*//*!
 * \brief Calculates intersection of two rects given as (left, bottom, width, height)
 *//*--------------------------------------------------------------------*/
tcu::IVec4 rectIntersection(const tcu::IVec4 &a, const tcu::IVec4 &b)
{
    const tcu::IVec2 pos    = tcu::IVec2(de::max(a.x(), b.x()), de::max(a.y(), b.y()));
    const tcu::IVec2 endPos = tcu::IVec2(de::min(a.x() + a.z(), b.x() + b.z()), de::min(a.y() + a.w(), b.y() + b.w()));

    return tcu::IVec4(pos.x(), pos.y(), endPos.x() - pos.x(), endPos.y() - pos.y());
}

void convertPrimitiveToBaseType(std::vector<pa::Triangle> &output, std::vector<pa::Triangle> &input)
{
    std::swap(output, input);
}

void convertPrimitiveToBaseType(std::vector<pa::Line> &output, std::vector<pa::Line> &input)
{
    std::swap(output, input);
}

void convertPrimitiveToBaseType(std::vector<pa::Point> &output, std::vector<pa::Point> &input)
{
    std::swap(output, input);
}

void convertPrimitiveToBaseType(std::vector<pa::Line> &output, std::vector<pa::LineAdjacency> &input)
{
    output.resize(input.size());
    for (size_t i = 0; i < input.size(); ++i)
    {
        const int adjacentProvokingVertex  = input[i].provokingIndex;
        const int baseProvokingVertexIndex = adjacentProvokingVertex - 1;
        output[i]                          = pa::Line(input[i].v1, input[i].v2, baseProvokingVertexIndex);
    }
}

void convertPrimitiveToBaseType(std::vector<pa::Triangle> &output, std::vector<pa::TriangleAdjacency> &input)
{
    output.resize(input.size());
    for (size_t i = 0; i < input.size(); ++i)
    {
        const int adjacentProvokingVertex  = input[i].provokingIndex;
        const int baseProvokingVertexIndex = adjacentProvokingVertex / 2;
        output[i] = pa::Triangle(input[i].v0, input[i].v2, input[i].v4, baseProvokingVertexIndex);
    }
}

namespace cliputil
{

/*--------------------------------------------------------------------*//*!
 * \brief Get clipped portion of the second endpoint
 *
 * Calculate the intersection of line segment v0-v1 and a given plane. Line
 * segment is defined by a pair of one-dimensional homogeneous coordinates.
 *
 *//*--------------------------------------------------------------------*/
ClipFloat getSegmentVolumeEdgeClip(const ClipFloat v0, const ClipFloat w0, const ClipFloat v1, const ClipFloat w1,
                                   const ClipFloat plane)
{
    // The +epsilon avoids division by zero without causing a meaningful change in the calculation.
    // Fixes divide by zero in builds when using the gcc toolset.
    return (plane * w0 - v0) / ((v1 - v0) - plane * (w1 - w0) + std::numeric_limits<ClipFloat>::epsilon());
}

/*--------------------------------------------------------------------*//*!
 * \brief Get clipped portion of the endpoint
 *
 * How much (in [0-1] range) of a line segment v0-v1 would be clipped
 * of the v0 end of the line segment by clipping.
 *//*--------------------------------------------------------------------*/
ClipFloat getLineEndpointClipping(const ClipVec4 &v0, const ClipVec4 &v1)
{
    const ClipFloat clipVolumeSize = (ClipFloat)1.0;

    if (v0.z() > v0.w())
    {
        // Clip +Z
        return getSegmentVolumeEdgeClip(v0.z(), v0.w(), v1.z(), v1.w(), clipVolumeSize);
    }
    else if (v0.z() < -v0.w())
    {
        // Clip -Z
        return getSegmentVolumeEdgeClip(v0.z(), v0.w(), v1.z(), v1.w(), -clipVolumeSize);
    }
    else
    {
        // no clipping
        return (ClipFloat)0.0;
    }
}

ClipVec4 vec4ToClipVec4(const tcu::Vec4 &v)
{
    return ClipVec4((ClipFloat)v.x(), (ClipFloat)v.y(), (ClipFloat)v.z(), (ClipFloat)v.w());
}

tcu::Vec4 clipVec4ToVec4(const ClipVec4 &v)
{
    return tcu::Vec4((float)v.x(), (float)v.y(), (float)v.z(), (float)v.w());
}

class ClipVolumePlane
{
public:
    virtual ~ClipVolumePlane()
    {
    }
    virtual bool pointInClipVolume(const ClipVec4 &p) const                                 = 0;
    virtual ClipFloat clipLineSegmentEnd(const ClipVec4 &v0, const ClipVec4 &v1) const      = 0;
    virtual ClipVec4 getLineIntersectionPoint(const ClipVec4 &v0, const ClipVec4 &v1) const = 0;
};

template <int Sign, int CompNdx>
class ComponentPlane : public ClipVolumePlane
{
    DE_STATIC_ASSERT(Sign == +1 || Sign == -1);

public:
    bool pointInClipVolume(const ClipVec4 &p) const;
    ClipFloat clipLineSegmentEnd(const ClipVec4 &v0, const ClipVec4 &v1) const;
    ClipVec4 getLineIntersectionPoint(const ClipVec4 &v0, const ClipVec4 &v1) const;
};

template <int Sign, int CompNdx>
bool ComponentPlane<Sign, CompNdx>::pointInClipVolume(const ClipVec4 &p) const
{
    const ClipFloat clipVolumeSize = (ClipFloat)1.0;

    return (ClipFloat)(Sign * p[CompNdx]) <= clipVolumeSize * p.w();
}

template <int Sign, int CompNdx>
ClipFloat ComponentPlane<Sign, CompNdx>::clipLineSegmentEnd(const ClipVec4 &v0, const ClipVec4 &v1) const
{
    const ClipFloat clipVolumeSize = (ClipFloat)1.0;

    return getSegmentVolumeEdgeClip(v0[CompNdx], v0.w(), v1[CompNdx], v1.w(), (ClipFloat)Sign * clipVolumeSize);
}

template <int Sign, int CompNdx>
ClipVec4 ComponentPlane<Sign, CompNdx>::getLineIntersectionPoint(const ClipVec4 &v0, const ClipVec4 &v1) const
{
    // A point on line might be far away, causing clipping ratio (clipLineSegmentEnd) to become extremely close to 1.0
    // even if the another point is not on the plane. Prevent clipping ratio from saturating by using points on line
    // that are (nearly) on this and (nearly) on the opposite plane.

    const ClipVec4 clippedV0  = tcu::mix(v0, v1, ComponentPlane<+1, CompNdx>().clipLineSegmentEnd(v0, v1));
    const ClipVec4 clippedV1  = tcu::mix(v0, v1, ComponentPlane<-1, CompNdx>().clipLineSegmentEnd(v0, v1));
    const ClipFloat clipRatio = clipLineSegmentEnd(clippedV0, clippedV1);

    // Find intersection point of line from v0 to v1 and the current plane. Avoid ratios near 1.0
    if (clipRatio <= (ClipFloat)0.5)
        return tcu::mix(clippedV0, clippedV1, clipRatio);
    else
    {
        const ClipFloat complementClipRatio = clipLineSegmentEnd(clippedV1, clippedV0);
        return tcu::mix(clippedV1, clippedV0, complementClipRatio);
    }
}

struct TriangleVertex
{
    ClipVec4 position;
    ClipFloat weight[3]; //!< barycentrics
};

struct SubTriangle
{
    TriangleVertex vertices[3];
};

void clipTriangleOneVertex(std::vector<TriangleVertex> &clippedEdges, const ClipVolumePlane &plane,
                           const TriangleVertex &clipped, const TriangleVertex &v1, const TriangleVertex &v2)
{
    const ClipFloat degenerateLimit = (ClipFloat)1.0;

    // calc clip pos
    TriangleVertex mid1;
    TriangleVertex mid2;
    bool outputDegenerate = false;

    {
        const TriangleVertex &inside  = v1;
        const TriangleVertex &outside = clipped;
        TriangleVertex &middle        = mid1;

        const ClipFloat hitDist = plane.clipLineSegmentEnd(inside.position, outside.position);

        if (hitDist >= degenerateLimit)
        {
            // do not generate degenerate triangles
            outputDegenerate = true;
        }
        else
        {
            const ClipVec4 approximatedClipPoint = tcu::mix(inside.position, outside.position, hitDist);
            const ClipVec4 anotherPointOnLine    = (hitDist > (ClipFloat)0.5) ? (inside.position) : (outside.position);

            middle.position  = plane.getLineIntersectionPoint(approximatedClipPoint, anotherPointOnLine);
            middle.weight[0] = tcu::mix(inside.weight[0], outside.weight[0], hitDist);
            middle.weight[1] = tcu::mix(inside.weight[1], outside.weight[1], hitDist);
            middle.weight[2] = tcu::mix(inside.weight[2], outside.weight[2], hitDist);
        }
    }

    {
        const TriangleVertex &inside  = v2;
        const TriangleVertex &outside = clipped;
        TriangleVertex &middle        = mid2;

        const ClipFloat hitDist = plane.clipLineSegmentEnd(inside.position, outside.position);

        if (hitDist >= degenerateLimit)
        {
            // do not generate degenerate triangles
            outputDegenerate = true;
        }
        else
        {
            const ClipVec4 approximatedClipPoint = tcu::mix(inside.position, outside.position, hitDist);
            const ClipVec4 anotherPointOnLine    = (hitDist > (ClipFloat)0.5) ? (inside.position) : (outside.position);

            middle.position  = plane.getLineIntersectionPoint(approximatedClipPoint, anotherPointOnLine);
            middle.weight[0] = tcu::mix(inside.weight[0], outside.weight[0], hitDist);
            middle.weight[1] = tcu::mix(inside.weight[1], outside.weight[1], hitDist);
            middle.weight[2] = tcu::mix(inside.weight[2], outside.weight[2], hitDist);
        }
    }

    if (!outputDegenerate)
    {
        // gen quad (v1) -> mid1 -> mid2 -> (v2)
        clippedEdges.push_back(v1);
        clippedEdges.push_back(mid1);
        clippedEdges.push_back(mid2);
        clippedEdges.push_back(v2);
    }
    else
    {
        // don't modify
        clippedEdges.push_back(v1);
        clippedEdges.push_back(clipped);
        clippedEdges.push_back(v2);
    }
}

void clipTriangleTwoVertices(std::vector<TriangleVertex> &clippedEdges, const ClipVolumePlane &plane,
                             const TriangleVertex &v0, const TriangleVertex &clipped1, const TriangleVertex &clipped2)
{
    const ClipFloat unclippableLimit = (ClipFloat)1.0;

    // calc clip pos
    TriangleVertex mid1;
    TriangleVertex mid2;
    bool unclippableVertex1 = false;
    bool unclippableVertex2 = false;

    {
        const TriangleVertex &inside  = v0;
        const TriangleVertex &outside = clipped1;
        TriangleVertex &middle        = mid1;

        const ClipFloat hitDist = plane.clipLineSegmentEnd(inside.position, outside.position);

        if (hitDist >= unclippableLimit)
        {
            // this edge cannot be clipped because the edge is really close to the volume boundary
            unclippableVertex1 = true;
        }
        else
        {
            const ClipVec4 approximatedClipPoint = tcu::mix(inside.position, outside.position, hitDist);
            const ClipVec4 anotherPointOnLine    = (hitDist > (ClipFloat)0.5) ? (inside.position) : (outside.position);

            middle.position  = plane.getLineIntersectionPoint(approximatedClipPoint, anotherPointOnLine);
            middle.weight[0] = tcu::mix(inside.weight[0], outside.weight[0], hitDist);
            middle.weight[1] = tcu::mix(inside.weight[1], outside.weight[1], hitDist);
            middle.weight[2] = tcu::mix(inside.weight[2], outside.weight[2], hitDist);
        }
    }

    {
        const TriangleVertex &inside  = v0;
        const TriangleVertex &outside = clipped2;
        TriangleVertex &middle        = mid2;

        const ClipFloat hitDist = plane.clipLineSegmentEnd(inside.position, outside.position);

        if (hitDist >= unclippableLimit)
        {
            // this edge cannot be clipped because the edge is really close to the volume boundary
            unclippableVertex2 = true;
        }
        else
        {
            const ClipVec4 approximatedClipPoint = tcu::mix(inside.position, outside.position, hitDist);
            const ClipVec4 anotherPointOnLine    = (hitDist > (ClipFloat)0.5) ? (inside.position) : (outside.position);

            middle.position  = plane.getLineIntersectionPoint(approximatedClipPoint, anotherPointOnLine);
            middle.weight[0] = tcu::mix(inside.weight[0], outside.weight[0], hitDist);
            middle.weight[1] = tcu::mix(inside.weight[1], outside.weight[1], hitDist);
            middle.weight[2] = tcu::mix(inside.weight[2], outside.weight[2], hitDist);
        }
    }

    if (!unclippableVertex1 && !unclippableVertex2)
    {
        // gen triangle (v0) -> mid1 -> mid2
        clippedEdges.push_back(v0);
        clippedEdges.push_back(mid1);
        clippedEdges.push_back(mid2);
    }
    else if (!unclippableVertex1 && unclippableVertex2)
    {
        // clip just vertex 1
        clippedEdges.push_back(v0);
        clippedEdges.push_back(mid1);
        clippedEdges.push_back(clipped2);
    }
    else if (unclippableVertex1 && !unclippableVertex2)
    {
        // clip just vertex 2
        clippedEdges.push_back(v0);
        clippedEdges.push_back(clipped1);
        clippedEdges.push_back(mid2);
    }
    else
    {
        // don't modify
        clippedEdges.push_back(v0);
        clippedEdges.push_back(clipped1);
        clippedEdges.push_back(clipped2);
    }
}

void clipTriangleToPlane(std::vector<TriangleVertex> &clippedEdges, const TriangleVertex *vertices,
                         const ClipVolumePlane &plane)
{
    const bool v0Clipped = !plane.pointInClipVolume(vertices[0].position);
    const bool v1Clipped = !plane.pointInClipVolume(vertices[1].position);
    const bool v2Clipped = !plane.pointInClipVolume(vertices[2].position);
    const int clipCount  = ((v0Clipped) ? (1) : (0)) + ((v1Clipped) ? (1) : (0)) + ((v2Clipped) ? (1) : (0));

    if (clipCount == 0)
    {
        // pass
        clippedEdges.insert(clippedEdges.begin(), vertices, vertices + 3);
    }
    else if (clipCount == 1)
    {
        // clip one vertex
        if (v0Clipped)
            clipTriangleOneVertex(clippedEdges, plane, vertices[0], vertices[1], vertices[2]);
        else if (v1Clipped)
            clipTriangleOneVertex(clippedEdges, plane, vertices[1], vertices[2], vertices[0]);
        else
            clipTriangleOneVertex(clippedEdges, plane, vertices[2], vertices[0], vertices[1]);
    }
    else if (clipCount == 2)
    {
        // clip two vertices
        if (!v0Clipped)
            clipTriangleTwoVertices(clippedEdges, plane, vertices[0], vertices[1], vertices[2]);
        else if (!v1Clipped)
            clipTriangleTwoVertices(clippedEdges, plane, vertices[1], vertices[2], vertices[0]);
        else
            clipTriangleTwoVertices(clippedEdges, plane, vertices[2], vertices[0], vertices[1]);
    }
    else if (clipCount == 3)
    {
        // discard
    }
    else
    {
        DE_ASSERT(false);
    }
}

} // namespace cliputil

tcu::Vec2 to2DCartesian(const tcu::Vec4 &p)
{
    return tcu::Vec2(p.x(), p.y()) / p.w();
}

float cross2D(const tcu::Vec2 &a, const tcu::Vec2 &b)
{
    return tcu::cross(tcu::Vec3(a.x(), a.y(), 0.0f), tcu::Vec3(b.x(), b.y(), 0.0f)).z();
}

void flatshadePrimitiveVertices(pa::Triangle &target, size_t outputNdx)
{
    const rr::GenericVec4 flatValue = target.getProvokingVertex()->outputs[outputNdx];
    target.v0->outputs[outputNdx]   = flatValue;
    target.v1->outputs[outputNdx]   = flatValue;
    target.v2->outputs[outputNdx]   = flatValue;
}

void flatshadePrimitiveVertices(pa::Line &target, size_t outputNdx)
{
    const rr::GenericVec4 flatValue = target.getProvokingVertex()->outputs[outputNdx];
    target.v0->outputs[outputNdx]   = flatValue;
    target.v1->outputs[outputNdx]   = flatValue;
}

void flatshadePrimitiveVertices(pa::Point &target, size_t outputNdx)
{
    DE_UNREF(target);
    DE_UNREF(outputNdx);
}

template <typename ContainerType>
void flatshadeVertices(const Program &program, ContainerType &list)
{
    // flatshade
    const std::vector<rr::VertexVaryingInfo> &fragInputs =
        (program.geometryShader) ? (program.geometryShader->getOutputs()) : (program.vertexShader->getOutputs());

    for (size_t inputNdx = 0; inputNdx < fragInputs.size(); ++inputNdx)
        if (fragInputs[inputNdx].flatshade)
            for (typename ContainerType::iterator it = list.begin(); it != list.end(); ++it)
                flatshadePrimitiveVertices(*it, inputNdx);
}

/*--------------------------------------------------------------------*//*!
 * Clip triangles to the clip volume.
 *//*--------------------------------------------------------------------*/
void clipPrimitives(std::vector<pa::Triangle> &list, const Program &program, bool clipWithZPlanes,
                    VertexPacketAllocator &vpalloc)
{
    using namespace cliputil;

    cliputil::ComponentPlane<+1, 0> clipPosX;
    cliputil::ComponentPlane<-1, 0> clipNegX;
    cliputil::ComponentPlane<+1, 1> clipPosY;
    cliputil::ComponentPlane<-1, 1> clipNegY;
    cliputil::ComponentPlane<+1, 2> clipPosZ;
    cliputil::ComponentPlane<-1, 2> clipNegZ;

    const std::vector<rr::VertexVaryingInfo> &fragInputs =
        (program.geometryShader) ? (program.geometryShader->getOutputs()) : (program.vertexShader->getOutputs());
    const ClipVolumePlane *planes[] = {&clipPosX, &clipNegX, &clipPosY, &clipNegY, &clipPosZ, &clipNegZ};
    const int numPlanes             = (clipWithZPlanes) ? (6) : (4);

    std::vector<pa::Triangle> outputTriangles;

    for (int inputTriangleNdx = 0; inputTriangleNdx < (int)list.size(); ++inputTriangleNdx)
    {
        bool clippedByPlane[6];

        // Needs clipping?
        {
            bool discardPrimitive  = false;
            bool fullyInClipVolume = true;

            for (int planeNdx = 0; planeNdx < numPlanes; ++planeNdx)
            {
                const ClipVolumePlane *plane = planes[planeNdx];
                const bool v0InsidePlane =
                    plane->pointInClipVolume(vec4ToClipVec4(list[inputTriangleNdx].v0->position));
                const bool v1InsidePlane =
                    plane->pointInClipVolume(vec4ToClipVec4(list[inputTriangleNdx].v1->position));
                const bool v2InsidePlane =
                    plane->pointInClipVolume(vec4ToClipVec4(list[inputTriangleNdx].v2->position));

                // Fully outside
                if (!v0InsidePlane && !v1InsidePlane && !v2InsidePlane)
                {
                    discardPrimitive = true;
                    break;
                }
                // Partially outside
                else if (!v0InsidePlane || !v1InsidePlane || !v2InsidePlane)
                {
                    clippedByPlane[planeNdx] = true;
                    fullyInClipVolume        = false;
                }
                // Fully inside
                else
                    clippedByPlane[planeNdx] = false;
            }

            if (discardPrimitive)
                continue;

            if (fullyInClipVolume)
            {
                outputTriangles.push_back(list[inputTriangleNdx]);
                continue;
            }
        }

        // Clip
        {
            std::vector<SubTriangle> subTriangles(1);
            SubTriangle &initialTri = subTriangles[0];

            initialTri.vertices[0].position  = vec4ToClipVec4(list[inputTriangleNdx].v0->position);
            initialTri.vertices[0].weight[0] = (ClipFloat)1.0;
            initialTri.vertices[0].weight[1] = (ClipFloat)0.0;
            initialTri.vertices[0].weight[2] = (ClipFloat)0.0;

            initialTri.vertices[1].position  = vec4ToClipVec4(list[inputTriangleNdx].v1->position);
            initialTri.vertices[1].weight[0] = (ClipFloat)0.0;
            initialTri.vertices[1].weight[1] = (ClipFloat)1.0;
            initialTri.vertices[1].weight[2] = (ClipFloat)0.0;

            initialTri.vertices[2].position  = vec4ToClipVec4(list[inputTriangleNdx].v2->position);
            initialTri.vertices[2].weight[0] = (ClipFloat)0.0;
            initialTri.vertices[2].weight[1] = (ClipFloat)0.0;
            initialTri.vertices[2].weight[2] = (ClipFloat)1.0;

            // Clip all subtriangles to all relevant planes
            for (int planeNdx = 0; planeNdx < numPlanes; ++planeNdx)
            {
                std::vector<SubTriangle> nextPhaseSubTriangles;

                if (!clippedByPlane[planeNdx])
                    continue;

                for (int subTriangleNdx = 0; subTriangleNdx < (int)subTriangles.size(); ++subTriangleNdx)
                {
                    std::vector<TriangleVertex> convexPrimitive;

                    // Clip triangle and form a convex n-gon ( n c {3, 4} )
                    clipTriangleToPlane(convexPrimitive, subTriangles[subTriangleNdx].vertices, *planes[planeNdx]);

                    // Subtriangle completely discarded
                    if (convexPrimitive.empty())
                        continue;

                    DE_ASSERT(convexPrimitive.size() == 3 || convexPrimitive.size() == 4);

                    //Triangulate planar convex n-gon
                    {
                        TriangleVertex &v0 = convexPrimitive[0];

                        for (int subsubTriangleNdx = 1; subsubTriangleNdx + 1 < (int)convexPrimitive.size();
                             ++subsubTriangleNdx)
                        {
                            const float degenerateEpsilon = 1.0e-6f;
                            const TriangleVertex &v1      = convexPrimitive[subsubTriangleNdx];
                            const TriangleVertex &v2      = convexPrimitive[subsubTriangleNdx + 1];
                            const float visibleArea       = de::abs(cross2D(to2DCartesian(clipVec4ToVec4(v1.position)) -
                                                                                to2DCartesian(clipVec4ToVec4(v0.position)),
                                                                            to2DCartesian(clipVec4ToVec4(v2.position)) -
                                                                                to2DCartesian(clipVec4ToVec4(v0.position))));

                            // has surface area (is not a degenerate)
                            if (visibleArea >= degenerateEpsilon)
                            {
                                SubTriangle subsubTriangle;

                                subsubTriangle.vertices[0] = v0;
                                subsubTriangle.vertices[1] = v1;
                                subsubTriangle.vertices[2] = v2;

                                nextPhaseSubTriangles.push_back(subsubTriangle);
                            }
                        }
                    }
                }

                subTriangles.swap(nextPhaseSubTriangles);
            }

            // Rebuild pa::Triangles from subtriangles
            for (int subTriangleNdx = 0; subTriangleNdx < (int)subTriangles.size(); ++subTriangleNdx)
            {
                VertexPacket *p0 = vpalloc.alloc();
                VertexPacket *p1 = vpalloc.alloc();
                VertexPacket *p2 = vpalloc.alloc();
                pa::Triangle ngonFragment(p0, p1, p2, -1);

                p0->position = clipVec4ToVec4(subTriangles[subTriangleNdx].vertices[0].position);
                p1->position = clipVec4ToVec4(subTriangles[subTriangleNdx].vertices[1].position);
                p2->position = clipVec4ToVec4(subTriangles[subTriangleNdx].vertices[2].position);

                for (size_t outputNdx = 0; outputNdx < fragInputs.size(); ++outputNdx)
                {
                    if (fragInputs[outputNdx].type == GENERICVECTYPE_FLOAT)
                    {
                        const tcu::Vec4 out0 = list[inputTriangleNdx].v0->outputs[outputNdx].get<float>();
                        const tcu::Vec4 out1 = list[inputTriangleNdx].v1->outputs[outputNdx].get<float>();
                        const tcu::Vec4 out2 = list[inputTriangleNdx].v2->outputs[outputNdx].get<float>();

                        p0->outputs[outputNdx] = (float)subTriangles[subTriangleNdx].vertices[0].weight[0] * out0 +
                                                 (float)subTriangles[subTriangleNdx].vertices[0].weight[1] * out1 +
                                                 (float)subTriangles[subTriangleNdx].vertices[0].weight[2] * out2;

                        p1->outputs[outputNdx] = (float)subTriangles[subTriangleNdx].vertices[1].weight[0] * out0 +
                                                 (float)subTriangles[subTriangleNdx].vertices[1].weight[1] * out1 +
                                                 (float)subTriangles[subTriangleNdx].vertices[1].weight[2] * out2;

                        p2->outputs[outputNdx] = (float)subTriangles[subTriangleNdx].vertices[2].weight[0] * out0 +
                                                 (float)subTriangles[subTriangleNdx].vertices[2].weight[1] * out1 +
                                                 (float)subTriangles[subTriangleNdx].vertices[2].weight[2] * out2;
                    }
                    else
                    {
                        // only floats are interpolated, all others must be flatshaded then
                        p0->outputs[outputNdx] = list[inputTriangleNdx].getProvokingVertex()->outputs[outputNdx];
                        p1->outputs[outputNdx] = list[inputTriangleNdx].getProvokingVertex()->outputs[outputNdx];
                        p2->outputs[outputNdx] = list[inputTriangleNdx].getProvokingVertex()->outputs[outputNdx];
                    }
                }

                outputTriangles.push_back(ngonFragment);
            }
        }
    }

    // output result
    list.swap(outputTriangles);
}

/*--------------------------------------------------------------------*//*!
 * Clip lines to the near and far clip planes.
 *
 * Clipping to other planes is a by-product of the viewport test  (i.e.
 * rasterization area selection).
 *//*--------------------------------------------------------------------*/
void clipPrimitives(std::vector<pa::Line> &list, const Program &program, bool clipWithZPlanes,
                    VertexPacketAllocator &vpalloc)
{
    DE_UNREF(vpalloc);

    using namespace cliputil;

    // Lines are clipped only by the far and the near planes here. Line clipping by other planes done in the rasterization phase

    const std::vector<rr::VertexVaryingInfo> &fragInputs =
        (program.geometryShader) ? (program.geometryShader->getOutputs()) : (program.vertexShader->getOutputs());
    std::vector<pa::Line> visibleLines;

    // Z-clipping disabled, don't do anything
    if (!clipWithZPlanes)
        return;

    for (size_t ndx = 0; ndx < list.size(); ++ndx)
    {
        pa::Line &l = list[ndx];

        // Totally discarded?
        if ((l.v0->position.z() < -l.v0->position.w() && l.v1->position.z() < -l.v1->position.w()) ||
            (l.v0->position.z() > l.v0->position.w() && l.v1->position.z() > l.v1->position.w()))
            continue; // discard

        // Something is visible

        const ClipVec4 p0  = vec4ToClipVec4(l.v0->position);
        const ClipVec4 p1  = vec4ToClipVec4(l.v1->position);
        const ClipFloat t0 = getLineEndpointClipping(p0, p1);
        const ClipFloat t1 = getLineEndpointClipping(p1, p0);

        // Not clipped at all?
        if (t0 == (ClipFloat)0.0 && t1 == (ClipFloat)0.0)
        {
            visibleLines.push_back(pa::Line(l.v0, l.v1, -1));
        }
        else
        {
            // Clip position
            l.v0->position = clipVec4ToVec4(tcu::mix(p0, p1, t0));
            l.v1->position = clipVec4ToVec4(tcu::mix(p1, p0, t1));

            // Clip attributes
            for (size_t outputNdx = 0; outputNdx < fragInputs.size(); ++outputNdx)
            {
                // only floats are clipped, other types are flatshaded
                if (fragInputs[outputNdx].type == GENERICVECTYPE_FLOAT)
                {
                    const tcu::Vec4 a0 = l.v0->outputs[outputNdx].get<float>();
                    const tcu::Vec4 a1 = l.v1->outputs[outputNdx].get<float>();

                    l.v0->outputs[outputNdx] = tcu::mix(a0, a1, (float)t0);
                    l.v1->outputs[outputNdx] = tcu::mix(a1, a0, (float)t1);
                }
            }

            visibleLines.push_back(pa::Line(l.v0, l.v1, -1));
        }
    }

    // return visible in list
    std::swap(visibleLines, list);
}

/*--------------------------------------------------------------------*//*!
 * Discard points not within clip volume. Clipping is a by-product
 * of the viewport test.
 *//*--------------------------------------------------------------------*/
void clipPrimitives(std::vector<pa::Point> &list, const Program &program, bool clipWithZPlanes,
                    VertexPacketAllocator &vpalloc)
{
    DE_UNREF(vpalloc);
    DE_UNREF(program);

    std::vector<pa::Point> visiblePoints;

    // Z-clipping disabled, don't do anything
    if (!clipWithZPlanes)
        return;

    for (size_t ndx = 0; ndx < list.size(); ++ndx)
    {
        pa::Point &p = list[ndx];

        // points are discarded if Z is not in range. (Wide) point clipping is done in the rasterization phase
        if (de::inRange(p.v0->position.z(), -p.v0->position.w(), p.v0->position.w()))
            visiblePoints.push_back(pa::Point(p.v0));
    }

    // return visible in list
    std::swap(visiblePoints, list);
}

void transformVertexClipCoordsToWindowCoords(const RenderState &state, VertexPacket &packet)
{
    // To normalized device coords
    {
        packet.position =
            tcu::Vec4(packet.position.x() / packet.position.w(), packet.position.y() / packet.position.w(),
                      packet.position.z() / packet.position.w(), 1.0f / packet.position.w());
    }

    // To window coords
    {
        const WindowRectangle &viewport = state.viewport.rect;
        const float halfW               = (float)(viewport.width) / 2.0f;
        const float halfH               = (float)(viewport.height) / 2.0f;
        const float oX                  = (float)viewport.left + halfW;
        const float oY                  = (float)viewport.bottom + halfH;
        const float zn                  = state.viewport.zn;
        const float zf                  = state.viewport.zf;

        packet.position = tcu::Vec4(packet.position.x() * halfW + oX, packet.position.y() * halfH + oY,
                                    packet.position.z() * (zf - zn) / 2.0f + (zn + zf) / 2.0f, packet.position.w());
    }
}

void transformPrimitiveClipCoordsToWindowCoords(const RenderState &state, pa::Triangle &target)
{
    transformVertexClipCoordsToWindowCoords(state, *target.v0);
    transformVertexClipCoordsToWindowCoords(state, *target.v1);
    transformVertexClipCoordsToWindowCoords(state, *target.v2);
}

void transformPrimitiveClipCoordsToWindowCoords(const RenderState &state, pa::Line &target)
{
    transformVertexClipCoordsToWindowCoords(state, *target.v0);
    transformVertexClipCoordsToWindowCoords(state, *target.v1);
}

void transformPrimitiveClipCoordsToWindowCoords(const RenderState &state, pa::Point &target)
{
    transformVertexClipCoordsToWindowCoords(state, *target.v0);
}

template <typename ContainerType>
void transformClipCoordsToWindowCoords(const RenderState &state, ContainerType &list)
{
    for (typename ContainerType::iterator it = list.begin(); it != list.end(); ++it)
        transformPrimitiveClipCoordsToWindowCoords(state, *it);
}

void makeSharedVerticeDistinct(VertexPacket *&packet, std::set<VertexPacket *, std::less<void *>> &vertices,
                               VertexPacketAllocator &vpalloc)
{
    // distinct
    if (vertices.find(packet) == vertices.end())
    {
        vertices.insert(packet);
    }
    else
    {
        VertexPacket *newPacket = vpalloc.alloc();

        // copy packet output values
        newPacket->position    = packet->position;
        newPacket->pointSize   = packet->pointSize;
        newPacket->primitiveID = packet->primitiveID;

        for (size_t outputNdx = 0; outputNdx < vpalloc.getNumVertexOutputs(); ++outputNdx)
            newPacket->outputs[outputNdx] = packet->outputs[outputNdx];

        // no need to insert new packet to "vertices" as newPacket is unique
        packet = newPacket;
    }
}

void makeSharedVerticesDistinct(pa::Triangle &target, std::set<VertexPacket *, std::less<void *>> &vertices,
                                VertexPacketAllocator &vpalloc)
{
    makeSharedVerticeDistinct(target.v0, vertices, vpalloc);
    makeSharedVerticeDistinct(target.v1, vertices, vpalloc);
    makeSharedVerticeDistinct(target.v2, vertices, vpalloc);
}

void makeSharedVerticesDistinct(pa::Line &target, std::set<VertexPacket *, std::less<void *>> &vertices,
                                VertexPacketAllocator &vpalloc)
{
    makeSharedVerticeDistinct(target.v0, vertices, vpalloc);
    makeSharedVerticeDistinct(target.v1, vertices, vpalloc);
}

void makeSharedVerticesDistinct(pa::Point &target, std::set<VertexPacket *, std::less<void *>> &vertices,
                                VertexPacketAllocator &vpalloc)
{
    makeSharedVerticeDistinct(target.v0, vertices, vpalloc);
}

template <typename ContainerType>
void makeSharedVerticesDistinct(ContainerType &list, VertexPacketAllocator &vpalloc)
{
    std::set<VertexPacket *, std::less<void *>> vertices;

    for (typename ContainerType::iterator it = list.begin(); it != list.end(); ++it)
        makeSharedVerticesDistinct(*it, vertices, vpalloc);
}

void generatePrimitiveIDs(pa::Triangle &target, int id)
{
    target.v0->primitiveID = id;
    target.v1->primitiveID = id;
    target.v2->primitiveID = id;
}

void generatePrimitiveIDs(pa::Line &target, int id)
{
    target.v0->primitiveID = id;
    target.v1->primitiveID = id;
}

void generatePrimitiveIDs(pa::Point &target, int id)
{
    target.v0->primitiveID = id;
}

template <typename ContainerType>
void generatePrimitiveIDs(ContainerType &list, DrawContext &drawContext)
{
    for (typename ContainerType::iterator it = list.begin(); it != list.end(); ++it)
        generatePrimitiveIDs(*it, drawContext.primitiveID++);
}

static float findTriangleVertexDepthSlope(const tcu::Vec4 &p, const tcu::Vec4 &v0, const tcu::Vec4 &v1)
{
    // screen space
    const tcu::Vec3 ssp  = p.swizzle(0, 1, 2);
    const tcu::Vec3 ssv0 = v0.swizzle(0, 1, 2);
    const tcu::Vec3 ssv1 = v1.swizzle(0, 1, 2);

    // dx & dy

    const tcu::Vec3 a   = ssv0.swizzle(0, 1, 2) - ssp.swizzle(0, 1, 2);
    const tcu::Vec3 b   = ssv1.swizzle(0, 1, 2) - ssp.swizzle(0, 1, 2);
    const float epsilon = 0.0001f;
    const float det     = (a.x() * b.y() - b.x() * a.y());

    // degenerate triangle, it won't generate any fragments anyway. Return value doesn't matter
    if (de::abs(det) < epsilon)
        return 0.0f;

    const tcu::Vec2 dxDir = tcu::Vec2(b.y(), -a.y()) / det;
    const tcu::Vec2 dyDir = tcu::Vec2(-b.x(), a.x()) / det;

    const float dzdx = dxDir.x() * a.z() + dxDir.y() * b.z();
    const float dzdy = dyDir.x() * a.z() + dyDir.y() * b.z();

    // approximate using max(|dz/dx|, |dz/dy|)
    return de::max(de::abs(dzdx), de::abs(dzdy));
}

static float findPrimitiveMaximumDepthSlope(const pa::Triangle &triangle)
{
    const float d1 = findTriangleVertexDepthSlope(triangle.v0->position, triangle.v1->position, triangle.v2->position);
    const float d2 = findTriangleVertexDepthSlope(triangle.v1->position, triangle.v2->position, triangle.v0->position);
    const float d3 = findTriangleVertexDepthSlope(triangle.v2->position, triangle.v0->position, triangle.v1->position);

    return de::max(d1, de::max(d2, d3));
}

static float getFloatingPointMinimumResolvableDifference(float maxZValue, tcu::TextureFormat::ChannelType type)
{
    if (type == tcu::TextureFormat::FLOAT)
    {
        // 32f
        const int maxExponent = tcu::Float32(maxZValue).exponent();
        return tcu::Float32::construct(+1, maxExponent - 23, 1 << 23).asFloat();
    }

    // unexpected format
    DE_ASSERT(false);
    return 0.0f;
}

static float getFixedPointMinimumResolvableDifference(int numBits)
{
    return tcu::Float32::construct(+1, -numBits, 1 << 23).asFloat();
}

static float findPrimitiveMinimumResolvableDifference(const pa::Triangle &triangle,
                                                      const rr::MultisampleConstPixelBufferAccess &depthAccess)
{
    const float maxZvalue =
        de::max(de::max(triangle.v0->position.z(), triangle.v1->position.z()), triangle.v2->position.z());
    const tcu::TextureFormat format              = depthAccess.raw().getFormat();
    const tcu::TextureFormat::ChannelOrder order = format.order;

    if (order == tcu::TextureFormat::D)
    {
        // depth only
        const tcu::TextureFormat::ChannelType channelType = format.type;
        const tcu::TextureChannelClass channelClass       = tcu::getTextureChannelClass(channelType);
        const int numBits                                 = tcu::getTextureFormatBitDepth(format).x();

        if (channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
            return getFloatingPointMinimumResolvableDifference(maxZvalue, channelType);
        else
            // \note channelClass might be CLASS_LAST but that's ok
            return getFixedPointMinimumResolvableDifference(numBits);
    }
    else if (order == tcu::TextureFormat::DS)
    {
        // depth stencil, special cases for possible combined formats
        if (format.type == tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV)
            return getFloatingPointMinimumResolvableDifference(maxZvalue, tcu::TextureFormat::FLOAT);
        else if (format.type == tcu::TextureFormat::UNSIGNED_INT_24_8)
            return getFixedPointMinimumResolvableDifference(24);
    }

    // unexpected format
    DE_ASSERT(false);
    return 0.0f;
}

void writeFragmentPackets(const RenderState &state, const RenderTarget &renderTarget, const Program &program,
                          const FragmentPacket *fragmentPackets, int numRasterizedPackets, rr::FaceType facetype,
                          const std::vector<rr::GenericVec4> &fragmentOutputArray,
                          const std::vector<rr::GenericVec4> &fragmentOutputArraySrc1, const float *depthValues,
                          std::vector<Fragment> &fragmentBuffer)
{
    const int numSamples    = renderTarget.getNumSamples();
    const size_t numOutputs = program.fragmentShader->getOutputs().size();
    FragmentProcessor fragProcessor;

    DE_ASSERT(fragmentOutputArray.size() >= (size_t)numRasterizedPackets * 4 * numOutputs);
    DE_ASSERT(fragmentBuffer.size() >= (size_t)numRasterizedPackets * 4);

    // Translate fragments but do not set the value yet
    {
        int fragCount = 0;
        for (int packetNdx = 0; packetNdx < numRasterizedPackets; ++packetNdx)
            for (int fragNdx = 0; fragNdx < 4; fragNdx++)
            {
                const FragmentPacket &packet = fragmentPackets[packetNdx];
                const int xo                 = fragNdx % 2;
                const int yo                 = fragNdx / 2;

                if (getCoverageAnyFragmentSampleLive(packet.coverage, numSamples, xo, yo))
                {
                    Fragment &fragment = fragmentBuffer[fragCount++];

                    fragment.pixelCoord = packet.position + tcu::IVec2(xo, yo);
                    fragment.coverage =
                        (uint32_t)((packet.coverage & getCoverageFragmentSampleBits(numSamples, xo, yo)) >>
                                   getCoverageOffset(numSamples, xo, yo));
                    fragment.sampleDepths =
                        (depthValues) ? (&depthValues[(packetNdx * 4 + yo * 2 + xo) * numSamples]) : (nullptr);
                }
            }
    }

    // Set per output output values
    {
        rr::FragmentOperationState noStencilDepthWriteState(state.fragOps);
        noStencilDepthWriteState.depthMask                      = false;
        noStencilDepthWriteState.stencilStates[facetype].sFail  = STENCILOP_KEEP;
        noStencilDepthWriteState.stencilStates[facetype].dpFail = STENCILOP_KEEP;
        noStencilDepthWriteState.stencilStates[facetype].dpPass = STENCILOP_KEEP;

        int fragCount = 0;
        for (size_t outputNdx = 0; outputNdx < numOutputs; ++outputNdx)
        {
            // Only the last output-pass has default state, other passes have stencil & depth writemask=0
            const rr::FragmentOperationState &fragOpsState =
                (outputNdx == numOutputs - 1) ? (state.fragOps) : (noStencilDepthWriteState);

            for (int packetNdx = 0; packetNdx < numRasterizedPackets; ++packetNdx)
                for (int fragNdx = 0; fragNdx < 4; fragNdx++)
                {
                    const FragmentPacket &packet = fragmentPackets[packetNdx];
                    const int xo                 = fragNdx % 2;
                    const int yo                 = fragNdx / 2;

                    // Add only fragments that have live samples to shaded fragments queue.
                    if (getCoverageAnyFragmentSampleLive(packet.coverage, numSamples, xo, yo))
                    {
                        Fragment &fragment = fragmentBuffer[fragCount++];
                        fragment.value     = fragmentOutputArray[(packetNdx * 4 + fragNdx) * numOutputs + outputNdx];
                        fragment.value1 = fragmentOutputArraySrc1[(packetNdx * 4 + fragNdx) * numOutputs + outputNdx];
                    }
                }

            // Execute per-fragment ops and write
            fragProcessor.render(renderTarget.getColorBuffer((int)outputNdx), renderTarget.getDepthBuffer(),
                                 renderTarget.getStencilBuffer(), &fragmentBuffer[0], fragCount, facetype,
                                 fragOpsState);
        }
    }
}

void rasterizePrimitive(const RenderState &state, const RenderTarget &renderTarget, const Program &program,
                        const pa::Triangle &triangle, const tcu::IVec4 &renderTargetRect,
                        RasterizationInternalBuffers &buffers)
{
    const int numSamples      = renderTarget.getNumSamples();
    const float depthClampMin = de::min(state.viewport.zn, state.viewport.zf);
    const float depthClampMax = de::max(state.viewport.zn, state.viewport.zf);
    TriangleRasterizer rasterizer(renderTargetRect, numSamples, state.rasterization, state.subpixelBits);
    float depthOffset = 0.0f;

    rasterizer.init(triangle.v0->position, triangle.v1->position, triangle.v2->position);

    // Culling
    const FaceType visibleFace = rasterizer.getVisibleFace();
    if ((state.cullMode == CULLMODE_FRONT && visibleFace == FACETYPE_FRONT) ||
        (state.cullMode == CULLMODE_BACK && visibleFace == FACETYPE_BACK) || state.cullMode == CULLMODE_FRONT_AND_BACK)
        return;

    // Shading context
    FragmentShadingContext shadingContext(
        triangle.v0->outputs, triangle.v1->outputs, triangle.v2->outputs, &buffers.shaderOutputs[0],
        &buffers.shaderOutputsSrc1[0], buffers.fragmentDepthBuffer, triangle.v2->primitiveID,
        (int)program.fragmentShader->getOutputs().size(), numSamples, rasterizer.getVisibleFace());

    // Polygon offset
    if (buffers.fragmentDepthBuffer && state.fragOps.polygonOffsetEnabled)
    {
        const float maximumDepthSlope = findPrimitiveMaximumDepthSlope(triangle);
        const float minimumResolvableDifference =
            findPrimitiveMinimumResolvableDifference(triangle, renderTarget.getDepthBuffer());

        depthOffset = maximumDepthSlope * state.fragOps.polygonOffsetFactor +
                      minimumResolvableDifference * state.fragOps.polygonOffsetUnits;
    }

    // Execute rasterize - shade - write loop
    for (;;)
    {
        const int maxFragmentPackets = (int)buffers.fragmentPackets.size();
        int numRasterizedPackets     = 0;

        // Rasterize

        rasterizer.rasterize(&buffers.fragmentPackets[0], buffers.fragmentDepthBuffer, maxFragmentPackets,
                             numRasterizedPackets);

        // numRasterizedPackets is guaranteed to be greater than zero for shadeFragments()

        if (!numRasterizedPackets)
            break; // Rasterization finished.

        // Polygon offset
        if (buffers.fragmentDepthBuffer && state.fragOps.polygonOffsetEnabled)
            for (int sampleNdx = 0; sampleNdx < numRasterizedPackets * 4 * numSamples; ++sampleNdx)
                buffers.fragmentDepthBuffer[sampleNdx] =
                    de::clamp(buffers.fragmentDepthBuffer[sampleNdx] + depthOffset, 0.0f, 1.0f);

        // Shade

        program.fragmentShader->shadeFragments(&buffers.fragmentPackets[0], numRasterizedPackets, shadingContext);

        // Depth clamp
        if (buffers.fragmentDepthBuffer && state.fragOps.depthClampEnabled)
            for (int sampleNdx = 0; sampleNdx < numRasterizedPackets * 4 * numSamples; ++sampleNdx)
                buffers.fragmentDepthBuffer[sampleNdx] =
                    de::clamp(buffers.fragmentDepthBuffer[sampleNdx], depthClampMin, depthClampMax);

        // Handle fragment shader outputs

        writeFragmentPackets(state, renderTarget, program, &buffers.fragmentPackets[0], numRasterizedPackets,
                             visibleFace, buffers.shaderOutputs, buffers.shaderOutputsSrc1, buffers.fragmentDepthBuffer,
                             buffers.shadedFragments);
    }
}

void rasterizePrimitive(const RenderState &state, const RenderTarget &renderTarget, const Program &program,
                        const pa::Line &line, const tcu::IVec4 &renderTargetRect, RasterizationInternalBuffers &buffers)
{
    const int numSamples      = renderTarget.getNumSamples();
    const float depthClampMin = de::min(state.viewport.zn, state.viewport.zf);
    const float depthClampMax = de::max(state.viewport.zn, state.viewport.zf);
    const bool msaa           = numSamples > 1;
    FragmentShadingContext shadingContext(line.v0->outputs, line.v1->outputs, nullptr, &buffers.shaderOutputs[0],
                                          &buffers.shaderOutputsSrc1[0], buffers.fragmentDepthBuffer,
                                          line.v1->primitiveID, (int)program.fragmentShader->getOutputs().size(),
                                          numSamples, FACETYPE_FRONT);
    SingleSampleLineRasterizer aliasedRasterizer(renderTargetRect, state.subpixelBits);
    MultiSampleLineRasterizer msaaRasterizer(numSamples, renderTargetRect, state.subpixelBits);

    // Initialize rasterization.
    if (msaa)
        msaaRasterizer.init(line.v0->position, line.v1->position, state.line.lineWidth);
    else
        aliasedRasterizer.init(line.v0->position, line.v1->position, state.line.lineWidth, 1, 0xFFFF);

    for (;;)
    {
        const int maxFragmentPackets = (int)buffers.fragmentPackets.size();
        int numRasterizedPackets     = 0;

        // Rasterize

        if (msaa)
            msaaRasterizer.rasterize(&buffers.fragmentPackets[0], buffers.fragmentDepthBuffer, maxFragmentPackets,
                                     numRasterizedPackets);
        else
            aliasedRasterizer.rasterize(&buffers.fragmentPackets[0], buffers.fragmentDepthBuffer, maxFragmentPackets,
                                        numRasterizedPackets);

        // numRasterizedPackets is guaranteed to be greater than zero for shadeFragments()

        if (!numRasterizedPackets)
            break; // Rasterization finished.

        // Shade

        program.fragmentShader->shadeFragments(&buffers.fragmentPackets[0], numRasterizedPackets, shadingContext);

        // Depth clamp
        if (buffers.fragmentDepthBuffer && state.fragOps.depthClampEnabled)
            for (int sampleNdx = 0; sampleNdx < numRasterizedPackets * 4 * numSamples; ++sampleNdx)
                buffers.fragmentDepthBuffer[sampleNdx] =
                    de::clamp(buffers.fragmentDepthBuffer[sampleNdx], depthClampMin, depthClampMax);

        // Handle fragment shader outputs

        writeFragmentPackets(state, renderTarget, program, &buffers.fragmentPackets[0], numRasterizedPackets,
                             rr::FACETYPE_FRONT, buffers.shaderOutputs, buffers.shaderOutputsSrc1,
                             buffers.fragmentDepthBuffer, buffers.shadedFragments);
    }
}

void rasterizePrimitive(const RenderState &state, const RenderTarget &renderTarget, const Program &program,
                        const pa::Point &point, const tcu::IVec4 &renderTargetRect,
                        RasterizationInternalBuffers &buffers)
{
    const int numSamples      = renderTarget.getNumSamples();
    const float depthClampMin = de::min(state.viewport.zn, state.viewport.zf);
    const float depthClampMax = de::max(state.viewport.zn, state.viewport.zf);
    TriangleRasterizer rasterizer1(renderTargetRect, numSamples, state.rasterization, state.subpixelBits);
    TriangleRasterizer rasterizer2(renderTargetRect, numSamples, state.rasterization, state.subpixelBits);

    // draw point as two triangles
    const float offset = point.v0->pointSize / 2.0f;
    const tcu::Vec4 w0 = tcu::Vec4(point.v0->position.x() + offset, point.v0->position.y() + offset,
                                   point.v0->position.z(), point.v0->position.w());
    const tcu::Vec4 w1 = tcu::Vec4(point.v0->position.x() - offset, point.v0->position.y() + offset,
                                   point.v0->position.z(), point.v0->position.w());
    const tcu::Vec4 w2 = tcu::Vec4(point.v0->position.x() - offset, point.v0->position.y() - offset,
                                   point.v0->position.z(), point.v0->position.w());
    const tcu::Vec4 w3 = tcu::Vec4(point.v0->position.x() + offset, point.v0->position.y() - offset,
                                   point.v0->position.z(), point.v0->position.w());

    rasterizer1.init(w0, w1, w2);
    rasterizer2.init(w0, w2, w3);

    // Shading context
    FragmentShadingContext shadingContext(point.v0->outputs, nullptr, nullptr, &buffers.shaderOutputs[0],
                                          &buffers.shaderOutputsSrc1[0], buffers.fragmentDepthBuffer,
                                          point.v0->primitiveID, (int)program.fragmentShader->getOutputs().size(),
                                          numSamples, FACETYPE_FRONT);

    // Execute rasterize - shade - write loop
    for (;;)
    {
        const int maxFragmentPackets = (int)buffers.fragmentPackets.size();
        int numRasterizedPackets     = 0;

        // Rasterize both triangles

        rasterizer1.rasterize(&buffers.fragmentPackets[0], buffers.fragmentDepthBuffer, maxFragmentPackets,
                              numRasterizedPackets);
        if (numRasterizedPackets != maxFragmentPackets)
        {
            float *const depthBufferAppendPointer =
                (buffers.fragmentDepthBuffer) ? (buffers.fragmentDepthBuffer + numRasterizedPackets * numSamples * 4) :
                                                (nullptr);
            int numRasterizedPackets2 = 0;

            rasterizer2.rasterize(&buffers.fragmentPackets[numRasterizedPackets], depthBufferAppendPointer,
                                  maxFragmentPackets - numRasterizedPackets, numRasterizedPackets2);

            numRasterizedPackets += numRasterizedPackets2;
        }

        // numRasterizedPackets is guaranteed to be greater than zero for shadeFragments()

        if (!numRasterizedPackets)
            break; // Rasterization finished.

        // Shade

        program.fragmentShader->shadeFragments(&buffers.fragmentPackets[0], numRasterizedPackets, shadingContext);

        // Depth clamp
        if (buffers.fragmentDepthBuffer && state.fragOps.depthClampEnabled)
            for (int sampleNdx = 0; sampleNdx < numRasterizedPackets * 4 * numSamples; ++sampleNdx)
                buffers.fragmentDepthBuffer[sampleNdx] =
                    de::clamp(buffers.fragmentDepthBuffer[sampleNdx], depthClampMin, depthClampMax);

        // Handle fragment shader outputs

        writeFragmentPackets(state, renderTarget, program, &buffers.fragmentPackets[0], numRasterizedPackets,
                             rr::FACETYPE_FRONT, buffers.shaderOutputs, buffers.shaderOutputsSrc1,
                             buffers.fragmentDepthBuffer, buffers.shadedFragments);
    }
}

template <typename ContainerType>
void rasterize(const RenderState &state, const RenderTarget &renderTarget, const Program &program,
               const ContainerType &list)
{
    const int numSamples            = renderTarget.getNumSamples();
    const int numFragmentOutputs    = (int)program.fragmentShader->getOutputs().size();
    const size_t maxFragmentPackets = 128;

    const tcu::IVec4 viewportRect     = tcu::IVec4(state.viewport.rect.left, state.viewport.rect.bottom,
                                                   state.viewport.rect.width, state.viewport.rect.height);
    const tcu::IVec4 bufferRect       = getBufferSize(renderTarget.getColorBuffer(0));
    const tcu::IVec4 renderTargetRect = rectIntersection(viewportRect, bufferRect);

    // shared buffers for all primitives
    std::vector<FragmentPacket> fragmentPackets(maxFragmentPackets);
    std::vector<GenericVec4> shaderOutputs(maxFragmentPackets * 4 * numFragmentOutputs);
    std::vector<GenericVec4> shaderOutputsSrc1(maxFragmentPackets * 4 * numFragmentOutputs);
    std::vector<Fragment> shadedFragments(maxFragmentPackets * 4);
    std::vector<float> depthValues(0);
    float *depthBufferPointer = nullptr;

    RasterizationInternalBuffers buffers;

    // calculate depth only if we have a depth buffer
    if (!isEmpty(renderTarget.getDepthBuffer()))
    {
        depthValues.resize(maxFragmentPackets * 4 * numSamples);
        depthBufferPointer = &depthValues[0];
    }

    // set buffers
    buffers.fragmentPackets.swap(fragmentPackets);
    buffers.shaderOutputs.swap(shaderOutputs);
    buffers.shaderOutputsSrc1.swap(shaderOutputsSrc1);
    buffers.shadedFragments.swap(shadedFragments);
    buffers.fragmentDepthBuffer = depthBufferPointer;

    // rasterize
    for (typename ContainerType::const_iterator it = list.begin(); it != list.end(); ++it)
        rasterizePrimitive(state, renderTarget, program, *it, renderTargetRect, buffers);
}

/*--------------------------------------------------------------------*//*!
 * Draws transformed triangles, lines or points to render target
 *//*--------------------------------------------------------------------*/
template <typename ContainerType>
void drawBasicPrimitives(const RenderState &state, const RenderTarget &renderTarget, const Program &program,
                         ContainerType &primList, VertexPacketAllocator &vpalloc)
{
    const bool clipZ = !state.fragOps.depthClampEnabled;

    // Transform feedback

    // Flatshading
    flatshadeVertices(program, primList);

    // Clipping
    // \todo [jarkko] is creating & swapping std::vectors really a good solution?
    clipPrimitives(primList, program, clipZ, vpalloc);

    // Transform vertices to window coords
    transformClipCoordsToWindowCoords(state, primList);

    // Rasterize and paint
    rasterize(state, renderTarget, program, primList);
}

void copyVertexPacketPointers(const VertexPacket **dst, const pa::Point &in)
{
    dst[0] = in.v0;
}

void copyVertexPacketPointers(const VertexPacket **dst, const pa::Line &in)
{
    dst[0] = in.v0;
    dst[1] = in.v1;
}

void copyVertexPacketPointers(const VertexPacket **dst, const pa::Triangle &in)
{
    dst[0] = in.v0;
    dst[1] = in.v1;
    dst[2] = in.v2;
}

void copyVertexPacketPointers(const VertexPacket **dst, const pa::LineAdjacency &in)
{
    dst[0] = in.v0;
    dst[1] = in.v1;
    dst[2] = in.v2;
    dst[3] = in.v3;
}

void copyVertexPacketPointers(const VertexPacket **dst, const pa::TriangleAdjacency &in)
{
    dst[0] = in.v0;
    dst[1] = in.v1;
    dst[2] = in.v2;
    dst[3] = in.v3;
    dst[4] = in.v4;
    dst[5] = in.v5;
}

template <PrimitiveType DrawPrimitiveType> // \note DrawPrimitiveType  can only be Points, line_strip, or triangle_strip
void drawGeometryShaderOutputAsPrimitives(const RenderState &state, const RenderTarget &renderTarget,
                                          const Program &program, VertexPacket *const *vertices, size_t numVertices,
                                          VertexPacketAllocator &vpalloc)
{
    // Run primitive assembly for generated stream

    const size_t assemblerPrimitiveCount =
        PrimitiveTypeTraits<DrawPrimitiveType>::Assembler::getPrimitiveCount(numVertices);
    std::vector<typename PrimitiveTypeTraits<DrawPrimitiveType>::BaseType> inputPrimitives(assemblerPrimitiveCount);

    PrimitiveTypeTraits<DrawPrimitiveType>::Assembler::exec(
        inputPrimitives.begin(), vertices, numVertices,
        state
            .provokingVertexConvention); // \note input Primitives are baseType_t => only basic primitives (non adjacency) will compile

    // Make shared vertices distinct

    makeSharedVerticesDistinct(inputPrimitives, vpalloc);

    // Draw assembled primitives

    drawBasicPrimitives(state, renderTarget, program, inputPrimitives, vpalloc);
}

template <PrimitiveType DrawPrimitiveType>
void drawWithGeometryShader(const RenderState &state, const RenderTarget &renderTarget, const Program &program,
                            std::vector<typename PrimitiveTypeTraits<DrawPrimitiveType>::Type> &input,
                            DrawContext &drawContext)
{
    // Vertices outputted by geometry shader may have different number of output variables than the original, create new memory allocator
    VertexPacketAllocator vpalloc(program.geometryShader->getOutputs().size());

    // Run geometry shader for all primitives
    GeometryEmitter emitter(vpalloc, program.geometryShader->getNumVerticesOut());
    std::vector<PrimitivePacket> primitives(input.size());
    const int numInvocations = (int)program.geometryShader->getNumInvocations();
    const int verticesIn     = PrimitiveTypeTraits<DrawPrimitiveType>::Type::NUM_VERTICES;

    for (size_t primitiveNdx = 0; primitiveNdx < input.size(); ++primitiveNdx)
    {
        primitives[primitiveNdx].primitiveIDIn = drawContext.primitiveID++;
        copyVertexPacketPointers(primitives[primitiveNdx].vertices, input[primitiveNdx]);
    }

    if (primitives.empty())
        return;

    for (int invocationNdx = 0; invocationNdx < numInvocations; ++invocationNdx)
    {
        // Shading invocation

        program.geometryShader->shadePrimitives(emitter, verticesIn, &primitives[0], (int)primitives.size(),
                                                invocationNdx);

        // Find primitives in the emitted vertices

        std::vector<VertexPacket *> emitted;
        emitter.moveEmittedTo(emitted);

        for (size_t primitiveBegin = 0; primitiveBegin < emitted.size();)
        {
            size_t primitiveEnd;

            // Find primitive begin
            if (!emitted[primitiveBegin])
            {
                ++primitiveBegin;
                continue;
            }

            // Find primitive end

            primitiveEnd = primitiveBegin + 1;
            for (; (primitiveEnd < emitted.size()) && emitted[primitiveEnd]; ++primitiveEnd)
                ; // find primitive end

            // Draw range [begin, end)

            switch (program.geometryShader->getOutputType())
            {
            case rr::GEOMETRYSHADEROUTPUTTYPE_POINTS:
                drawGeometryShaderOutputAsPrimitives<PRIMITIVETYPE_POINTS>(
                    state, renderTarget, program, &emitted[primitiveBegin], primitiveEnd - primitiveBegin, vpalloc);
                break;
            case rr::GEOMETRYSHADEROUTPUTTYPE_LINE_STRIP:
                drawGeometryShaderOutputAsPrimitives<PRIMITIVETYPE_LINE_STRIP>(
                    state, renderTarget, program, &emitted[primitiveBegin], primitiveEnd - primitiveBegin, vpalloc);
                break;
            case rr::GEOMETRYSHADEROUTPUTTYPE_TRIANGLE_STRIP:
                drawGeometryShaderOutputAsPrimitives<PRIMITIVETYPE_TRIANGLE_STRIP>(
                    state, renderTarget, program, &emitted[primitiveBegin], primitiveEnd - primitiveBegin, vpalloc);
                break;
            default:
                DE_ASSERT(false);
            }

            // Next primitive
            primitiveBegin = primitiveEnd + 1;
        }
    }
}

/*--------------------------------------------------------------------*//*!
 * Assembles, tesselates, runs geometry shader and draws primitives of any type from vertex list.
 *//*--------------------------------------------------------------------*/
template <PrimitiveType DrawPrimitiveType>
void drawAsPrimitives(const RenderState &state, const RenderTarget &renderTarget, const Program &program,
                      VertexPacket *const *vertices, int numVertices, DrawContext &drawContext,
                      VertexPacketAllocator &vpalloc)
{
    // Assemble primitives (deconstruct stips & loops)
    const size_t assemblerPrimitiveCount =
        PrimitiveTypeTraits<DrawPrimitiveType>::Assembler::getPrimitiveCount(numVertices);
    std::vector<typename PrimitiveTypeTraits<DrawPrimitiveType>::Type> inputPrimitives(assemblerPrimitiveCount);

    PrimitiveTypeTraits<DrawPrimitiveType>::Assembler::exec(inputPrimitives.begin(), vertices, (size_t)numVertices,
                                                            state.provokingVertexConvention);

    // Tesselate
    //if (state.tesselation)
    // primList = state.tesselation.exec(primList);

    // Geometry shader
    if (program.geometryShader)
    {
        // If there is an active geometry shader, it will convert any primitive type to basic types
        drawWithGeometryShader<DrawPrimitiveType>(state, renderTarget, program, inputPrimitives, drawContext);
    }
    else
    {
        std::vector<typename PrimitiveTypeTraits<DrawPrimitiveType>::BaseType> basePrimitives;

        // convert types from X_adjacency to X
        convertPrimitiveToBaseType(basePrimitives, inputPrimitives);

        // Make shared vertices distinct. Needed for that the translation to screen space happens only once per vertex, and for flatshading
        makeSharedVerticesDistinct(basePrimitives, vpalloc);

        // A primitive ID will be generated even if no geometry shader is active
        generatePrimitiveIDs(basePrimitives, drawContext);

        // Draw as a basic type
        drawBasicPrimitives(state, renderTarget, program, basePrimitives, vpalloc);
    }
}

bool isValidCommand(const DrawCommand &command, int numInstances)
{
    // numInstances should be valid
    if (numInstances < 0)
        return false;

    // Shaders should have the same varyings
    if (command.program.geometryShader)
    {
        if (command.program.vertexShader->getOutputs() != command.program.geometryShader->getInputs())
            return false;

        if (command.program.geometryShader->getOutputs() != command.program.fragmentShader->getInputs())
            return false;
    }
    else
    {
        if (command.program.vertexShader->getOutputs() != command.program.fragmentShader->getInputs())
            return false;
    }

    // Shader input/output types are set
    for (size_t varyingNdx = 0; varyingNdx < command.program.vertexShader->getInputs().size(); ++varyingNdx)
        if (command.program.vertexShader->getInputs()[varyingNdx].type != GENERICVECTYPE_FLOAT &&
            command.program.vertexShader->getInputs()[varyingNdx].type != GENERICVECTYPE_INT32 &&
            command.program.vertexShader->getInputs()[varyingNdx].type != GENERICVECTYPE_UINT32)
            return false;
    for (size_t varyingNdx = 0; varyingNdx < command.program.vertexShader->getOutputs().size(); ++varyingNdx)
        if (command.program.vertexShader->getOutputs()[varyingNdx].type != GENERICVECTYPE_FLOAT &&
            command.program.vertexShader->getOutputs()[varyingNdx].type != GENERICVECTYPE_INT32 &&
            command.program.vertexShader->getOutputs()[varyingNdx].type != GENERICVECTYPE_UINT32)
            return false;

    for (size_t varyingNdx = 0; varyingNdx < command.program.fragmentShader->getInputs().size(); ++varyingNdx)
        if (command.program.fragmentShader->getInputs()[varyingNdx].type != GENERICVECTYPE_FLOAT &&
            command.program.fragmentShader->getInputs()[varyingNdx].type != GENERICVECTYPE_INT32 &&
            command.program.fragmentShader->getInputs()[varyingNdx].type != GENERICVECTYPE_UINT32)
            return false;
    for (size_t varyingNdx = 0; varyingNdx < command.program.fragmentShader->getOutputs().size(); ++varyingNdx)
        if (command.program.fragmentShader->getOutputs()[varyingNdx].type != GENERICVECTYPE_FLOAT &&
            command.program.fragmentShader->getOutputs()[varyingNdx].type != GENERICVECTYPE_INT32 &&
            command.program.fragmentShader->getOutputs()[varyingNdx].type != GENERICVECTYPE_UINT32)
            return false;

    if (command.program.geometryShader)
    {
        for (size_t varyingNdx = 0; varyingNdx < command.program.geometryShader->getInputs().size(); ++varyingNdx)
            if (command.program.geometryShader->getInputs()[varyingNdx].type != GENERICVECTYPE_FLOAT &&
                command.program.geometryShader->getInputs()[varyingNdx].type != GENERICVECTYPE_INT32 &&
                command.program.geometryShader->getInputs()[varyingNdx].type != GENERICVECTYPE_UINT32)
                return false;
        for (size_t varyingNdx = 0; varyingNdx < command.program.geometryShader->getOutputs().size(); ++varyingNdx)
            if (command.program.geometryShader->getOutputs()[varyingNdx].type != GENERICVECTYPE_FLOAT &&
                command.program.geometryShader->getOutputs()[varyingNdx].type != GENERICVECTYPE_INT32 &&
                command.program.geometryShader->getOutputs()[varyingNdx].type != GENERICVECTYPE_UINT32)
                return false;
    }

    // Enough vertex inputs?
    if ((size_t)command.numVertexAttribs < command.program.vertexShader->getInputs().size())
        return false;

    // There is a fragment output sink for each output?
    if ((size_t)command.renderTarget.getNumColorBuffers() < command.program.fragmentShader->getOutputs().size())
        return false;

    // All destination buffers should have same number of samples and same size
    for (int outputNdx = 0; outputNdx < command.renderTarget.getNumColorBuffers(); ++outputNdx)
    {
        if (getBufferSize(command.renderTarget.getColorBuffer(0)) !=
            getBufferSize(command.renderTarget.getColorBuffer(outputNdx)))
            return false;

        if (command.renderTarget.getNumSamples() != command.renderTarget.getColorBuffer(outputNdx).getNumSamples())
            return false;
    }

    // All destination buffers should have same basic type as matching fragment output
    for (size_t varyingNdx = 0; varyingNdx < command.program.fragmentShader->getOutputs().size(); ++varyingNdx)
    {
        const tcu::TextureChannelClass colorbufferClass =
            tcu::getTextureChannelClass(command.renderTarget.getColorBuffer((int)varyingNdx).raw().getFormat().type);
        const GenericVecType colorType =
            (colorbufferClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER) ?
                (rr::GENERICVECTYPE_INT32) :
                ((colorbufferClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER) ? (rr::GENERICVECTYPE_UINT32) :
                                                                                   (rr::GENERICVECTYPE_FLOAT));

        if (command.program.fragmentShader->getOutputs()[varyingNdx].type != colorType)
            return false;
    }

    // Integer values are flatshaded
    for (size_t outputNdx = 0; outputNdx < command.program.vertexShader->getOutputs().size(); ++outputNdx)
    {
        if (!command.program.vertexShader->getOutputs()[outputNdx].flatshade &&
            (command.program.vertexShader->getOutputs()[outputNdx].type == GENERICVECTYPE_INT32 ||
             command.program.vertexShader->getOutputs()[outputNdx].type == GENERICVECTYPE_UINT32))
            return false;
    }
    if (command.program.geometryShader)
        for (size_t outputNdx = 0; outputNdx < command.program.geometryShader->getOutputs().size(); ++outputNdx)
        {
            if (!command.program.geometryShader->getOutputs()[outputNdx].flatshade &&
                (command.program.geometryShader->getOutputs()[outputNdx].type == GENERICVECTYPE_INT32 ||
                 command.program.geometryShader->getOutputs()[outputNdx].type == GENERICVECTYPE_UINT32))
                return false;
        }

    // Draw primitive is valid for geometry shader
    if (command.program.geometryShader)
    {
        if (command.program.geometryShader->getInputType() == rr::GEOMETRYSHADERINPUTTYPE_POINTS &&
            command.primitives.getPrimitiveType() != PRIMITIVETYPE_POINTS)
            return false;

        if (command.program.geometryShader->getInputType() == rr::GEOMETRYSHADERINPUTTYPE_LINES &&
            (command.primitives.getPrimitiveType() != PRIMITIVETYPE_LINES &&
             command.primitives.getPrimitiveType() != PRIMITIVETYPE_LINE_STRIP &&
             command.primitives.getPrimitiveType() != PRIMITIVETYPE_LINE_LOOP))
            return false;

        if (command.program.geometryShader->getInputType() == rr::GEOMETRYSHADERINPUTTYPE_TRIANGLES &&
            (command.primitives.getPrimitiveType() != PRIMITIVETYPE_TRIANGLES &&
             command.primitives.getPrimitiveType() != PRIMITIVETYPE_TRIANGLE_STRIP &&
             command.primitives.getPrimitiveType() != PRIMITIVETYPE_TRIANGLE_FAN))
            return false;

        if (command.program.geometryShader->getInputType() == rr::GEOMETRYSHADERINPUTTYPE_LINES_ADJACENCY &&
            (command.primitives.getPrimitiveType() != PRIMITIVETYPE_LINES_ADJACENCY &&
             command.primitives.getPrimitiveType() != PRIMITIVETYPE_LINE_STRIP_ADJACENCY))
            return false;

        if (command.program.geometryShader->getInputType() == rr::GEOMETRYSHADERINPUTTYPE_TRIANGLES_ADJACENCY &&
            (command.primitives.getPrimitiveType() != PRIMITIVETYPE_TRIANGLES_ADJACENCY &&
             command.primitives.getPrimitiveType() != PRIMITIVETYPE_TRIANGLE_STRIP_ADJACENCY))
            return false;
    }

    return true;
}

} // namespace

RenderTarget::RenderTarget(const MultisamplePixelBufferAccess &colorMultisampleBuffer,
                           const MultisamplePixelBufferAccess &depthMultisampleBuffer,
                           const MultisamplePixelBufferAccess &stencilMultisampleBuffer)
    : m_numColorBuffers(1)
    , m_depthBuffer(MultisamplePixelBufferAccess::fromMultisampleAccess(
          tcu::getEffectiveDepthStencilAccess(depthMultisampleBuffer.raw(), tcu::Sampler::MODE_DEPTH)))
    , m_stencilBuffer(MultisamplePixelBufferAccess::fromMultisampleAccess(
          tcu::getEffectiveDepthStencilAccess(stencilMultisampleBuffer.raw(), tcu::Sampler::MODE_STENCIL)))
{
    m_colorBuffers[0] = colorMultisampleBuffer;
}

int RenderTarget::getNumSamples(void) const
{
    DE_ASSERT(m_numColorBuffers > 0);
    return m_colorBuffers[0].getNumSamples();
}

DrawIndices::DrawIndices(const uint32_t *ptr, int baseVertex_)
    : indices(ptr)
    , indexType(INDEXTYPE_UINT32)
    , baseVertex(baseVertex_)
{
}

DrawIndices::DrawIndices(const uint16_t *ptr, int baseVertex_)
    : indices(ptr)
    , indexType(INDEXTYPE_UINT16)
    , baseVertex(baseVertex_)
{
}

DrawIndices::DrawIndices(const uint8_t *ptr, int baseVertex_)
    : indices(ptr)
    , indexType(INDEXTYPE_UINT8)
    , baseVertex(baseVertex_)
{
}

DrawIndices::DrawIndices(const void *ptr, IndexType type, int baseVertex_)
    : indices(ptr)
    , indexType(type)
    , baseVertex(baseVertex_)
{
}

PrimitiveList::PrimitiveList(PrimitiveType primitiveType, int numElements, const int firstElement)
    : m_primitiveType(primitiveType)
    , m_numElements(numElements)
    , m_indices(nullptr)
    , m_indexType(INDEXTYPE_LAST)
    , m_baseVertex(firstElement)
{
    DE_ASSERT(numElements >= 0 && "Invalid numElements");
    DE_ASSERT(firstElement >= 0 && "Invalid firstElement");
}

PrimitiveList::PrimitiveList(PrimitiveType primitiveType, int numElements, const DrawIndices &indices)
    : m_primitiveType(primitiveType)
    , m_numElements((size_t)numElements)
    , m_indices(indices.indices)
    , m_indexType(indices.indexType)
    , m_baseVertex(indices.baseVertex)
{
    DE_ASSERT(numElements >= 0 && "Invalid numElements");
}

size_t PrimitiveList::getIndex(size_t elementNdx) const
{
    // indices == nullptr interpreted as command.indices = [first (=baseVertex) + 0, first + 1, first + 2...]
    if (m_indices)
    {
        int index = m_baseVertex + (int)readIndexArray(m_indexType, m_indices, elementNdx);
        DE_ASSERT(index >= 0); // do not access indices < 0

        return (size_t)index;
    }
    else
        return (size_t)(m_baseVertex) + elementNdx;
}

bool PrimitiveList::isRestartIndex(size_t elementNdx, uint32_t restartIndex) const
{
    // implicit index or explicit index (without base vertex) equals restart
    if (m_indices)
        return readIndexArray(m_indexType, m_indices, elementNdx) == restartIndex;
    else
        return elementNdx == (size_t)restartIndex;
}

Renderer::Renderer(void)
{
}

Renderer::~Renderer(void)
{
}

void Renderer::draw(const DrawCommand &command) const
{
    drawInstanced(command, 1);
}

void Renderer::drawInstanced(const DrawCommand &command, int numInstances) const
{
    // Do not run bad commands
    {
        const bool validCommand = isValidCommand(command, numInstances);
        if (!validCommand)
        {
            DE_ASSERT(false);
            return;
        }
    }

    // Do not draw if nothing to draw
    {
        if (command.primitives.getNumElements() == 0 || numInstances == 0)
            return;
    }

    // Prepare transformation

    const size_t numVaryings = command.program.vertexShader->getOutputs().size();
    VertexPacketAllocator vpalloc(numVaryings);
    std::vector<VertexPacket *> vertexPackets = vpalloc.allocArray(command.primitives.getNumElements());
    DrawContext drawContext;

    for (int instanceID = 0; instanceID < numInstances; ++instanceID)
    {
        // Each instance has its own primitives
        drawContext.primitiveID = 0;

        for (size_t elementNdx = 0; elementNdx < command.primitives.getNumElements(); ++elementNdx)
        {
            int numVertexPackets = 0;

            // collect primitive vertices until restart

            while (elementNdx < command.primitives.getNumElements() &&
                   !(command.state.restart.enabled &&
                     command.primitives.isRestartIndex(elementNdx, command.state.restart.restartIndex)))
            {
                // input
                vertexPackets[numVertexPackets]->instanceNdx = instanceID;
                vertexPackets[numVertexPackets]->vertexNdx   = (int)command.primitives.getIndex(elementNdx);

                // output
                vertexPackets[numVertexPackets]->pointSize =
                    command.state.point.pointSize; // default value from the current state
                vertexPackets[numVertexPackets]->position = tcu::Vec4(0, 0, 0, 0); // no undefined values

                ++numVertexPackets;
                ++elementNdx;
            }

            // Duplicated restart shade
            if (numVertexPackets == 0)
                continue;

            // \todo Vertex cache?

            // Transform vertices

            command.program.vertexShader->shadeVertices(command.vertexAttribs, &vertexPackets[0], numVertexPackets);

            // Draw primitives

            switch (command.primitives.getPrimitiveType())
            {
            case PRIMITIVETYPE_TRIANGLES:
            {
                drawAsPrimitives<PRIMITIVETYPE_TRIANGLES>(command.state, command.renderTarget, command.program,
                                                          &vertexPackets[0], numVertexPackets, drawContext, vpalloc);
                break;
            }
            case PRIMITIVETYPE_TRIANGLE_STRIP:
            {
                drawAsPrimitives<PRIMITIVETYPE_TRIANGLE_STRIP>(command.state, command.renderTarget, command.program,
                                                               &vertexPackets[0], numVertexPackets, drawContext,
                                                               vpalloc);
                break;
            }
            case PRIMITIVETYPE_TRIANGLE_FAN:
            {
                drawAsPrimitives<PRIMITIVETYPE_TRIANGLE_FAN>(command.state, command.renderTarget, command.program,
                                                             &vertexPackets[0], numVertexPackets, drawContext, vpalloc);
                break;
            }
            case PRIMITIVETYPE_LINES:
            {
                drawAsPrimitives<PRIMITIVETYPE_LINES>(command.state, command.renderTarget, command.program,
                                                      &vertexPackets[0], numVertexPackets, drawContext, vpalloc);
                break;
            }
            case PRIMITIVETYPE_LINE_STRIP:
            {
                drawAsPrimitives<PRIMITIVETYPE_LINE_STRIP>(command.state, command.renderTarget, command.program,
                                                           &vertexPackets[0], numVertexPackets, drawContext, vpalloc);
                break;
            }
            case PRIMITIVETYPE_LINE_LOOP:
            {
                drawAsPrimitives<PRIMITIVETYPE_LINE_LOOP>(command.state, command.renderTarget, command.program,
                                                          &vertexPackets[0], numVertexPackets, drawContext, vpalloc);
                break;
            }
            case PRIMITIVETYPE_POINTS:
            {
                drawAsPrimitives<PRIMITIVETYPE_POINTS>(command.state, command.renderTarget, command.program,
                                                       &vertexPackets[0], numVertexPackets, drawContext, vpalloc);
                break;
            }
            case PRIMITIVETYPE_LINES_ADJACENCY:
            {
                drawAsPrimitives<PRIMITIVETYPE_LINES_ADJACENCY>(command.state, command.renderTarget, command.program,
                                                                &vertexPackets[0], numVertexPackets, drawContext,
                                                                vpalloc);
                break;
            }
            case PRIMITIVETYPE_LINE_STRIP_ADJACENCY:
            {
                drawAsPrimitives<PRIMITIVETYPE_LINE_STRIP_ADJACENCY>(command.state, command.renderTarget,
                                                                     command.program, &vertexPackets[0],
                                                                     numVertexPackets, drawContext, vpalloc);
                break;
            }
            case PRIMITIVETYPE_TRIANGLES_ADJACENCY:
            {
                drawAsPrimitives<PRIMITIVETYPE_TRIANGLES_ADJACENCY>(command.state, command.renderTarget,
                                                                    command.program, &vertexPackets[0],
                                                                    numVertexPackets, drawContext, vpalloc);
                break;
            }
            case PRIMITIVETYPE_TRIANGLE_STRIP_ADJACENCY:
            {
                drawAsPrimitives<PRIMITIVETYPE_TRIANGLE_STRIP_ADJACENCY>(command.state, command.renderTarget,
                                                                         command.program, &vertexPackets[0],
                                                                         numVertexPackets, drawContext, vpalloc);
                break;
            }
            default:
                DE_ASSERT(false);
            }
        }
    }
}

} // namespace rr
