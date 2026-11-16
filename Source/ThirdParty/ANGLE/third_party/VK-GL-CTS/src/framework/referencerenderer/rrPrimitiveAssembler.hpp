#ifndef _RRPRIMITIVEASSEMBLER_HPP
#define _RRPRIMITIVEASSEMBLER_HPP
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
 * \brief Primitive assembler
 *//*--------------------------------------------------------------------*/

#include "rrDefs.hpp"
#include "rrVertexPacket.hpp"

namespace rr
{
namespace pa
{

struct Triangle
{
    enum
    {
        NUM_VERTICES = 3
    };

    Triangle(void) : v0(nullptr), v1(nullptr), v2(nullptr), provokingIndex(-1)
    {
    }

    Triangle(VertexPacket *v0_, VertexPacket *v1_, VertexPacket *v2_, int provokingIndex_)
        : v0(v0_)
        , v1(v1_)
        , v2(v2_)
        , provokingIndex(provokingIndex_)
    {
    }

    VertexPacket *getProvokingVertex(void)
    {
        switch (provokingIndex)
        {
        case 0:
            return v0;
        case 1:
            return v1;
        case 2:
            return v2;
        default:
            DE_ASSERT(false);
            return nullptr;
        }
    }

    VertexPacket *v0;
    VertexPacket *v1;
    VertexPacket *v2;

    int provokingIndex;
} DE_WARN_UNUSED_TYPE;

struct Triangles
{
    template <typename Iterator>
    static void exec(Iterator outputIterator, VertexPacket *const *vertices, size_t numVertices,
                     rr::ProvokingVertex provokingConvention)
    {
        const int provokingOffset = (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (0) : (2);

        for (size_t ndx = 0; ndx + 2 < numVertices; ndx += 3)
            *(outputIterator++) = Triangle(vertices[ndx], vertices[ndx + 1], vertices[ndx + 2], provokingOffset);
    }

    static size_t getPrimitiveCount(size_t vertices)
    {
        return vertices / 3;
    }
} DE_WARN_UNUSED_TYPE;

struct TriangleStrip
{
    template <typename Iterator>
    static void exec(Iterator outputIterator, VertexPacket *const *vertices, size_t numVertices,
                     rr::ProvokingVertex provokingConvention)
    {
        if (numVertices < 3)
        {
        }
        else
        {
            VertexPacket *vert0 = vertices[0];
            VertexPacket *vert1 = vertices[1];
            size_t ndx          = 2;

            for (;;)
            {
                {
                    if (ndx >= numVertices)
                        break;

                    *(outputIterator++) = Triangle(vert0, vert1, vertices[ndx],
                                                   (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (0) : (2));
                    vert0               = vertices[ndx];

                    ndx++;
                }

                {
                    if (ndx >= numVertices)
                        break;

                    *(outputIterator++) = Triangle(vert0, vert1, vertices[ndx],
                                                   (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (1) : (2));
                    vert1               = vertices[ndx];

                    ndx++;
                }
            }
        }
    }

    static size_t getPrimitiveCount(size_t vertices)
    {
        return (vertices < 3) ? (0) : (vertices - 2);
    }
} DE_WARN_UNUSED_TYPE;

struct TriangleFan
{
    template <typename Iterator>
    static void exec(Iterator outputIterator, VertexPacket *const *vertices, size_t numVertices,
                     rr::ProvokingVertex provokingConvention)
    {
        if (numVertices == 0)
        {
        }
        else
        {
            const int provokingOffset = (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (1) : (2);
            VertexPacket *const first = vertices[0];

            for (size_t ndx = 1; ndx + 1 < numVertices; ++ndx)
                *(outputIterator++) = Triangle(first, vertices[ndx], vertices[ndx + 1], provokingOffset);
        }
    }

    static size_t getPrimitiveCount(size_t vertices)
    {
        return (vertices < 3) ? (0) : (vertices - 2);
    }
} DE_WARN_UNUSED_TYPE;

struct Line
{
    enum
    {
        NUM_VERTICES = 2
    };

    Line(void) : v0(nullptr), v1(nullptr), provokingIndex(-1)
    {
    }

    Line(VertexPacket *v0_, VertexPacket *v1_, int provokingIndex_) : v0(v0_), v1(v1_), provokingIndex(provokingIndex_)
    {
    }

    VertexPacket *getProvokingVertex(void)
    {
        switch (provokingIndex)
        {
        case 0:
            return v0;
        case 1:
            return v1;
        default:
            DE_ASSERT(false);
            return nullptr;
        }
    }

    VertexPacket *v0;
    VertexPacket *v1;

    int provokingIndex;
} DE_WARN_UNUSED_TYPE;

struct Lines
{
    template <typename Iterator>
    static void exec(Iterator outputIterator, VertexPacket *const *vertices, size_t numVertices,
                     rr::ProvokingVertex provokingConvention)
    {
        const int provokingOffset = (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (0) : (1);

        for (size_t ndx = 0; ndx + 1 < numVertices; ndx += 2)
            *(outputIterator++) = Line(vertices[ndx], vertices[ndx + 1], provokingOffset);
    }

    static size_t getPrimitiveCount(size_t vertices)
    {
        return vertices / 2;
    }
} DE_WARN_UNUSED_TYPE;

struct LineStrip
{
    template <typename Iterator>
    static void exec(Iterator outputIterator, VertexPacket *const *vertices, size_t numVertices,
                     rr::ProvokingVertex provokingConvention)
    {
        if (numVertices == 0)
        {
        }
        else
        {
            VertexPacket *prev = vertices[0];

            for (size_t ndx = 1; ndx < numVertices; ++ndx)
            {
                *(outputIterator++) =
                    Line(prev, vertices[ndx], (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (0) : (1));
                prev = vertices[ndx];
            }
        }
    }

    static size_t getPrimitiveCount(size_t vertices)
    {
        return (vertices < 2) ? (0) : (vertices - 1);
    }
} DE_WARN_UNUSED_TYPE;

struct LineLoop
{
    template <typename Iterator>
    static void exec(Iterator outputIterator, VertexPacket *const *vertices, size_t numVertices,
                     rr::ProvokingVertex provokingConvention)
    {
        if (numVertices < 2)
        {
        }
        else
        {
            VertexPacket *prev = vertices[0];

            for (size_t ndx = 1; ndx < numVertices; ++ndx)
            {
                *(outputIterator++) =
                    Line(prev, vertices[ndx], (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (0) : (1));
                prev = vertices[ndx];
            }

            *(outputIterator++) =
                Line(prev, vertices[0], (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (0) : (1));
        }
    }

    static size_t getPrimitiveCount(size_t vertices)
    {
        return (vertices < 2) ? (0) : (vertices);
    }
} DE_WARN_UNUSED_TYPE;

struct Point
{
    enum
    {
        NUM_VERTICES = 1
    };

    Point(void) : v0(nullptr)
    {
    }

    Point(VertexPacket *v0_) : v0(v0_)
    {
    }

    VertexPacket *v0;
} DE_WARN_UNUSED_TYPE;

struct Points
{
    template <typename Iterator>
    static void exec(Iterator outputIterator, VertexPacket *const *vertices, size_t numVertices,
                     rr::ProvokingVertex provokingConvention)
    {
        DE_UNREF(provokingConvention);

        for (size_t ndx = 0; ndx < numVertices; ++ndx)
            *(outputIterator++) = Point(vertices[ndx]);
    }

    static size_t getPrimitiveCount(size_t vertices)
    {
        return (vertices);
    }
} DE_WARN_UNUSED_TYPE;

struct LineAdjacency
{
    enum
    {
        NUM_VERTICES = 4
    };

    LineAdjacency(void) : v0(nullptr), v1(nullptr), v2(nullptr), v3(nullptr), provokingIndex(-1)
    {
    }

    LineAdjacency(VertexPacket *v0_, VertexPacket *v1_, VertexPacket *v2_, VertexPacket *v3_, int provokingIndex_)
        : v0(v0_)
        , v1(v1_)
        , v2(v2_)
        , v3(v3_)
        , provokingIndex(provokingIndex_)
    {
    }

    VertexPacket *getProvokingVertex(void)
    {
        switch (provokingIndex)
        {
        case 1:
            return v1;
        case 2:
            return v2;
        default:
            DE_ASSERT(false);
            return nullptr;
        }
    }

    VertexPacket *v0;
    VertexPacket *v1;
    VertexPacket *v2;
    VertexPacket *v3;

    int provokingIndex;
} DE_WARN_UNUSED_TYPE;

struct LinesAdjacency
{
    template <typename Iterator>
    static void exec(Iterator outputIterator, VertexPacket *const *vertices, size_t numVertices,
                     rr::ProvokingVertex provokingConvention)
    {
        const int provokingOffset = (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (1) : (2);

        for (size_t ndx = 0; ndx + 3 < numVertices; ndx += 4)
            *(outputIterator++) =
                LineAdjacency(vertices[ndx], vertices[ndx + 1], vertices[ndx + 2], vertices[ndx + 3], provokingOffset);
    }

    static size_t getPrimitiveCount(size_t vertices)
    {
        return vertices / 4;
    }
} DE_WARN_UNUSED_TYPE;

struct LineStripAdjacency
{
    template <typename Iterator>
    static void exec(Iterator outputIterator, VertexPacket *const *vertices, size_t numVertices,
                     rr::ProvokingVertex provokingConvention)
    {
        const int provokingOffset = (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (1) : (2);

        for (size_t ndx = 0; ndx + 3 < numVertices; ++ndx)
            *(outputIterator++) =
                LineAdjacency(vertices[ndx], vertices[ndx + 1], vertices[ndx + 2], vertices[ndx + 3], provokingOffset);
    }

    static size_t getPrimitiveCount(size_t vertices)
    {
        return (vertices < 4) ? (0) : (vertices - 3);
    }
} DE_WARN_UNUSED_TYPE;

struct TriangleAdjacency
{
    enum
    {
        NUM_VERTICES = 6
    };

    TriangleAdjacency(void)
        : v0(nullptr)
        , v1(nullptr)
        , v2(nullptr)
        , v3(nullptr)
        , v4(nullptr)
        , v5(nullptr)
        , provokingIndex(-1)
    {
    }

    TriangleAdjacency(VertexPacket *v0_, VertexPacket *v1_, VertexPacket *v2_, VertexPacket *v3_, VertexPacket *v4_,
                      VertexPacket *v5_, int provokingIndex_)
        : v0(v0_)
        , v1(v1_)
        , v2(v2_)
        , v3(v3_)
        , v4(v4_)
        , v5(v5_)
        , provokingIndex(provokingIndex_)
    {
    }

    VertexPacket *getProvokingVertex(void)
    {
        switch (provokingIndex)
        {
        case 0:
            return v0;
        case 2:
            return v2;
        case 4:
            return v4;
        default:
            DE_ASSERT(false);
            return nullptr;
        }
    }

    VertexPacket *v0;
    VertexPacket *v1; //!< adjacent
    VertexPacket *v2;
    VertexPacket *v3; //!< adjacent
    VertexPacket *v4;
    VertexPacket *v5; //!< adjacent

    int provokingIndex;
} DE_WARN_UNUSED_TYPE;

struct TrianglesAdjacency
{
    template <typename Iterator>
    static void exec(Iterator outputIterator, VertexPacket *const *vertices, size_t numVertices,
                     rr::ProvokingVertex provokingConvention)
    {
        const int provokingOffset = (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (0) : (4);

        for (size_t ndx = 0; ndx + 5 < numVertices; ndx += 6)
            *(outputIterator++) =
                TriangleAdjacency(vertices[ndx], vertices[ndx + 1], vertices[ndx + 2], vertices[ndx + 3],
                                  vertices[ndx + 4], vertices[ndx + 5], provokingOffset);
    }

    static size_t getPrimitiveCount(size_t vertices)
    {
        return vertices / 6;
    }
} DE_WARN_UNUSED_TYPE;

struct TriangleStripAdjacency
{
    template <typename Iterator>
    static void exec(Iterator outputIterator, VertexPacket *const *vertices, size_t numVertices,
                     rr::ProvokingVertex provokingConvention)
    {
        if (numVertices < 6)
        {
        }
        else if (numVertices < 8)
        {
            *(outputIterator++) =
                TriangleAdjacency(vertices[0], vertices[1], vertices[2], vertices[5], vertices[4], vertices[3],
                                  (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (0) : (4));
        }
        else
        {
            const size_t primitiveCount = getPrimitiveCount(numVertices);
            size_t i;

            // first
            *(outputIterator++) =
                TriangleAdjacency(vertices[0], vertices[1], vertices[2], vertices[6], vertices[4], vertices[3],
                                  (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (0) : (4));

            // middle
            for (i = 1; i + 1 < primitiveCount; ++i)
            {
                // odd
                if (i % 2 == 1)
                {
                    *(outputIterator++) =
                        TriangleAdjacency(vertices[2 * i + 2], vertices[2 * i - 2], vertices[2 * i + 0],
                                          vertices[2 * i + 3], vertices[2 * i + 4], vertices[2 * i + 6],
                                          (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (2) : (4));
                }
                // even
                else
                {
                    *(outputIterator++) =
                        TriangleAdjacency(vertices[2 * i + 0], vertices[2 * i - 2], vertices[2 * i + 2],
                                          vertices[2 * i + 6], vertices[2 * i + 4], vertices[2 * i + 3],
                                          (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (0) : (4));
                }
            }

            // last

            // odd
            if (i % 2 == 1)
                *(outputIterator++) = TriangleAdjacency(vertices[2 * i + 2], vertices[2 * i - 2], vertices[2 * i + 0],
                                                        vertices[2 * i + 3], vertices[2 * i + 4], vertices[2 * i + 5],
                                                        (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (2) : (4));
            // even
            else
                *(outputIterator++) = TriangleAdjacency(vertices[2 * i + 0], vertices[2 * i - 2], vertices[2 * i + 2],
                                                        vertices[2 * i + 5], vertices[2 * i + 4], vertices[2 * i + 3],
                                                        (provokingConvention == rr::PROVOKINGVERTEX_FIRST) ? (0) : (4));
        }
    }

    static size_t getPrimitiveCount(size_t vertices)
    {
        return (vertices < 6) ? 0 : ((vertices - 4) / 2);
    }
} DE_WARN_UNUSED_TYPE;

} // namespace pa
} // namespace rr

#endif // _RRPRIMITIVEASSEMBLER_HPP
