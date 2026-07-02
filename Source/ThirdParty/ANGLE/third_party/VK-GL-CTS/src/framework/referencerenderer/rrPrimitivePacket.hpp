#ifndef _RRPRIMITIVEPACKET_HPP
#define _RRPRIMITIVEPACKET_HPP
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

#include "rrDefs.hpp"
#include "rrGenericVector.hpp"

#include <vector>

namespace rr
{

struct VertexPacket;
class VertexPacketAllocator;

/*--------------------------------------------------------------------*//*!
 * \brief Geometry packet
 *
 * Geometry packet contains inputs for geometry shading.
 *//*--------------------------------------------------------------------*/
struct PrimitivePacket
{
    int primitiveIDIn;
    const VertexPacket *vertices[6];
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief Geometry emitter
 *
 * Geometry emitter handles outputting of new vertices from geometry shader
 *//*--------------------------------------------------------------------*/
class GeometryEmitter
{
public:
    GeometryEmitter(VertexPacketAllocator &vpalloc, size_t numVertices);

    void EmitVertex(const tcu::Vec4 &position, float pointSize, const GenericVec4 *varyings, int primitiveID);
    void EndPrimitive(void);

    void moveEmittedTo(std::vector<VertexPacket *> &);

private:
    GeometryEmitter(const GeometryEmitter &);
    GeometryEmitter &operator=(const GeometryEmitter &);

    std::vector<VertexPacket *> m_emitted; //!< NULL elements mean primitive end
    VertexPacketAllocator &m_vpalloc;
    size_t m_numEmitted;
    size_t m_maxVertices;

} DE_WARN_UNUSED_TYPE;

} // namespace rr

#endif // _RRPRIMITIVEPACKET_HPP
