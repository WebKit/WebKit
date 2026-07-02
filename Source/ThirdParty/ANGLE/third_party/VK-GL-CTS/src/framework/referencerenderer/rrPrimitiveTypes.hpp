#ifndef _RRPRIMITIVETYPES_HPP
#define _RRPRIMITIVETYPES_HPP
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
 * \brief Primitive types
 *//*--------------------------------------------------------------------*/

#include "rrDefs.hpp"
#include "rrPrimitiveAssembler.hpp"

namespace rr
{

enum PrimitiveType
{
    PRIMITIVETYPE_TRIANGLES = 0,  //!< Separate triangles
    PRIMITIVETYPE_TRIANGLE_STRIP, //!< Triangle strip
    PRIMITIVETYPE_TRIANGLE_FAN,   //!< Triangle fan

    PRIMITIVETYPE_LINES,      //!< Separate lines
    PRIMITIVETYPE_LINE_STRIP, //!< Line strip
    PRIMITIVETYPE_LINE_LOOP,  //!< Line loop

    PRIMITIVETYPE_POINTS, //!< Points

    PRIMITIVETYPE_LINES_ADJACENCY,          //!< Separate lines (adjacency)
    PRIMITIVETYPE_LINE_STRIP_ADJACENCY,     //!< Line strip (adjacency)
    PRIMITIVETYPE_TRIANGLES_ADJACENCY,      //!< Separate triangles (adjacency)
    PRIMITIVETYPE_TRIANGLE_STRIP_ADJACENCY, //!< Triangle strip (adjacency)

    PRIMITIVETYPE_LAST
};

template <PrimitiveType DrawPrimitiveType>
struct PrimitiveTypeTraits
{
};
template <>
struct PrimitiveTypeTraits<PRIMITIVETYPE_TRIANGLES>
{
    typedef pa::Triangle Type;
    typedef pa::Triangle BaseType;
    typedef pa::Triangles Assembler;
};
template <>
struct PrimitiveTypeTraits<PRIMITIVETYPE_TRIANGLE_STRIP>
{
    typedef pa::Triangle Type;
    typedef pa::Triangle BaseType;
    typedef pa::TriangleStrip Assembler;
};
template <>
struct PrimitiveTypeTraits<PRIMITIVETYPE_TRIANGLE_FAN>
{
    typedef pa::Triangle Type;
    typedef pa::Triangle BaseType;
    typedef pa::TriangleFan Assembler;
};
template <>
struct PrimitiveTypeTraits<PRIMITIVETYPE_LINES>
{
    typedef pa::Line Type;
    typedef pa::Line BaseType;
    typedef pa::Lines Assembler;
};
template <>
struct PrimitiveTypeTraits<PRIMITIVETYPE_LINE_STRIP>
{
    typedef pa::Line Type;
    typedef pa::Line BaseType;
    typedef pa::LineStrip Assembler;
};
template <>
struct PrimitiveTypeTraits<PRIMITIVETYPE_LINE_LOOP>
{
    typedef pa::Line Type;
    typedef pa::Line BaseType;
    typedef pa::LineLoop Assembler;
};
template <>
struct PrimitiveTypeTraits<PRIMITIVETYPE_POINTS>
{
    typedef pa::Point Type;
    typedef pa::Point BaseType;
    typedef pa::Points Assembler;
};
template <>
struct PrimitiveTypeTraits<PRIMITIVETYPE_LINES_ADJACENCY>
{
    typedef pa::LineAdjacency Type;
    typedef pa::Line BaseType;
    typedef pa::LinesAdjacency Assembler;
};
template <>
struct PrimitiveTypeTraits<PRIMITIVETYPE_LINE_STRIP_ADJACENCY>
{
    typedef pa::LineAdjacency Type;
    typedef pa::Line BaseType;
    typedef pa::LineStripAdjacency Assembler;
};
template <>
struct PrimitiveTypeTraits<PRIMITIVETYPE_TRIANGLES_ADJACENCY>
{
    typedef pa::TriangleAdjacency Type;
    typedef pa::Triangle BaseType;
    typedef pa::TrianglesAdjacency Assembler;
};
template <>
struct PrimitiveTypeTraits<PRIMITIVETYPE_TRIANGLE_STRIP_ADJACENCY>
{
    typedef pa::TriangleAdjacency Type;
    typedef pa::Triangle BaseType;
    typedef pa::TriangleStripAdjacency Assembler;
};

} // namespace rr

#endif // _RRPRIMITIVETYPES_HPP
