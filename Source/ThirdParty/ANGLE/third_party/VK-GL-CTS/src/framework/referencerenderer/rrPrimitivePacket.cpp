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
 * \brief Primitive packet
 *//*--------------------------------------------------------------------*/

#include "rrPrimitivePacket.hpp"

#include "rrVertexPacket.hpp"

namespace rr
{

GeometryEmitter::GeometryEmitter(VertexPacketAllocator &vpalloc, size_t numVertices)
    : m_vpalloc(vpalloc)
    , m_numEmitted(0)
    , m_maxVertices(numVertices)
{
}

void GeometryEmitter::EmitVertex(const tcu::Vec4 &position, float pointSize, const GenericVec4 *varyings,
                                 int primitiveID)
{
    VertexPacket *packet;

    if (++m_numEmitted > m_maxVertices)
    {
        DE_FATAL("Undefined results, too many vertices emitted.");
        return;
    }

    packet = m_vpalloc.alloc();

    packet->position    = position;
    packet->pointSize   = pointSize;
    packet->primitiveID = primitiveID;

    for (size_t ndx = 0; ndx < m_vpalloc.getNumVertexOutputs(); ++ndx)
        packet->outputs[ndx] = varyings[ndx];

    m_emitted.push_back(packet);
}

void GeometryEmitter::EndPrimitive(void)
{
    m_numEmitted = 0;
    m_emitted.push_back(nullptr);
}

void GeometryEmitter::moveEmittedTo(std::vector<VertexPacket *> &output)
{
    m_emitted.swap(output);
    m_emitted.clear();
}

} // namespace rr
