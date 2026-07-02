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

#include "rrVertexPacket.hpp"

namespace rr
{

VertexPacket::VertexPacket(void)
{
}

VertexPacket::~VertexPacket(void)
{
}

VertexPacketAllocator::VertexPacketAllocator(const size_t numberOfVertexOutputs)
    : m_numberOfVertexOutputs(numberOfVertexOutputs)
{
}

VertexPacketAllocator::~VertexPacketAllocator(void)
{
    for (size_t i = 0; i < m_allocations.size(); ++i)
        delete[] m_allocations[i];
    m_allocations.clear();
}

std::vector<VertexPacket *> VertexPacketAllocator::allocArray(size_t count)
{
    if (!count)
        return std::vector<VertexPacket *>();

    const size_t extraVaryings = (m_numberOfVertexOutputs == 0) ? (0) : (m_numberOfVertexOutputs - 1);
    const size_t packetSize    = sizeof(VertexPacket) + extraVaryings * sizeof(GenericVec4);

    std::vector<VertexPacket *> retVal;
    int8_t *ptr = new int8_t[packetSize * count]; // throws bad_alloc => ok

    // *.push_back might throw bad_alloc
    try
    {
        // run ctors
        for (size_t i = 0; i < count; ++i)
            retVal.push_back(new (ptr + i * packetSize) VertexPacket()); // throws bad_alloc

        m_allocations.push_back(ptr); // throws bad_alloc
    }
    catch (std::bad_alloc &)
    {
        delete[] ptr;
        throw;
    }

    return retVal;
}

VertexPacket *VertexPacketAllocator::alloc(void)
{
    const size_t poolSize = 8;

    if (m_singleAllocPool.empty())
        m_singleAllocPool = allocArray(poolSize);

    VertexPacket *packet = *--m_singleAllocPool.end();
    m_singleAllocPool.pop_back();
    return packet;
}

} // namespace rr
