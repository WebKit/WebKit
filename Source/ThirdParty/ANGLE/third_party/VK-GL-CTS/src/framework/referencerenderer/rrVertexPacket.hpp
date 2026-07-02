#ifndef _RRVERTEXPACKET_HPP
#define _RRVERTEXPACKET_HPP
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
 * \brief Vertex packet and Vertex packet allocator
 *//*--------------------------------------------------------------------*/

#include "rrDefs.hpp"
#include "rrGenericVector.hpp"
#include "tcuVector.hpp"

#include <vector>

namespace rr
{

class VertexPacketAllocator;

/*--------------------------------------------------------------------*//*!
 * \brief Vertex packet
 *
 * Vertex packet contains inputs and outputs for vertex shading.
 *
 * Inputs consist of per-vertex vertex and instance indices. Attribute
 * list that can be accessed using those indices is provided as separate
 * pointer for VS.
 *
 * Outputs include position, optional point size, and list of generic
 * outputs that shader can write to. Number of VS outputs is specified
 * in ProgramInfo.
 *
 * VertexPacket instance must be created by VertexPacketAllocator as
 * outputs must be allocated memory after the instance.
 *//*--------------------------------------------------------------------*/
struct VertexPacket
{
    // Inputs.
    int instanceNdx; //!< Instance index.
    int vertexNdx;   //!< Vertex index.

    // Outputs.
    tcu::Vec4 position; //!< Transformed position - must be written always.
    float pointSize;    //!< Point size, required when rendering points.
    int primitiveID;    //!< Geometry shader output

    GenericVec4 outputs
        [1]; //!< Generic vertex shader outputs - passed to subsequent shader stages. Array length is the number of outputs.
    // --- DO NOT ADD ANY MEMBER VARIABLES AFTER OUTPUTS, OUTPUTS IS VARIABLE-SIZED --- //

private:
    // Allow creation and destruction only for Allocator
    VertexPacket(void);
    VertexPacket(const VertexPacket &); // disabled, non-copyable
    ~VertexPacket(void);

    // Assignment cannot work without knowing the output array length => prevent assignment
    VertexPacket &operator=(const VertexPacket &); // disabled, non-copyable

    friend class VertexPacketAllocator;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief Vertex packet allocator
 *
 * Allocates vertex packets.
 *
 * Vertex packet must have enough space allocated for its outputs.
 *
 * All memory allocated for vertex packets is released when VertexPacketAllocator
 * is destroyed. Allocated vertex packets should not be accessed after
 * allocator is destroyed.
 *
 * alloc and allocArray will throw bad_alloc if allocation fails.
 *//*--------------------------------------------------------------------*/
class VertexPacketAllocator
{
public:
    VertexPacketAllocator(const size_t numberOfVertexOutputs);
    ~VertexPacketAllocator(void);

    std::vector<VertexPacket *> allocArray(size_t count); // throws bad_alloc
    VertexPacket *alloc(void);                            // throws bad_alloc

    inline size_t getNumVertexOutputs(void) const
    {
        return m_numberOfVertexOutputs;
    }

private:
    VertexPacketAllocator(const VertexPacketAllocator &);            // disabled, non-copyable
    VertexPacketAllocator &operator=(const VertexPacketAllocator &); // disabled, non-copyable

    const size_t m_numberOfVertexOutputs;
    std::vector<int8_t *> m_allocations;
    std::vector<VertexPacket *> m_singleAllocPool;
} DE_WARN_UNUSED_TYPE;

} // namespace rr

#endif // _RRVERTEXPACKET_HPP
