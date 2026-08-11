#ifndef _RRRENDERER_HPP
#define _RRRENDERER_HPP
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

#include "rrDefs.hpp"
#include "rrShaders.hpp"
#include "rrRenderState.hpp"
#include "rrPrimitiveTypes.hpp"
#include "rrMultisamplePixelBufferAccess.hpp"
#include "tcuTexture.hpp"

namespace rr
{

class RenderTarget
{
public:
    enum
    {
        MAX_COLOR_BUFFERS = 4
    };

    RenderTarget(const MultisamplePixelBufferAccess &colorMultisampleBuffer,
                 const MultisamplePixelBufferAccess &depthMultisampleBuffer   = MultisamplePixelBufferAccess(),
                 const MultisamplePixelBufferAccess &stencilMultisampleBuffer = MultisamplePixelBufferAccess());

    int getNumSamples(void) const;

    const MultisamplePixelBufferAccess &getColorBuffer(int ndx) const
    {
        DE_ASSERT(de::inRange(ndx, 0, m_numColorBuffers));
        return m_colorBuffers[ndx];
    }
    int getNumColorBuffers(void) const
    {
        return m_numColorBuffers;
    }
    const MultisamplePixelBufferAccess &getStencilBuffer(void) const
    {
        return m_stencilBuffer;
    }
    const MultisamplePixelBufferAccess &getDepthBuffer(void) const
    {
        return m_depthBuffer;
    }

private:
    MultisamplePixelBufferAccess m_colorBuffers[MAX_COLOR_BUFFERS];
    const int m_numColorBuffers;
    const MultisamplePixelBufferAccess m_depthBuffer;
    const MultisamplePixelBufferAccess m_stencilBuffer;
} DE_WARN_UNUSED_TYPE;

struct Program
{
    Program(const VertexShader *vertexShader_, const FragmentShader *fragmentShader_,
            const GeometryShader *geometryShader_ = nullptr)
        : vertexShader(vertexShader_)
        , fragmentShader(fragmentShader_)
        , geometryShader(geometryShader_)
    {
    }

    const VertexShader *vertexShader;
    const FragmentShader *fragmentShader;
    const GeometryShader *geometryShader;
} DE_WARN_UNUSED_TYPE;

struct DrawIndices
{
    DrawIndices(const uint32_t *, int baseVertex = 0);
    DrawIndices(const uint16_t *, int baseVertex = 0);
    DrawIndices(const uint8_t *, int baseVertex = 0);
    DrawIndices(const void *ptr, IndexType type, int baseVertex = 0);

    const void *const indices;
    const IndexType indexType;
    const int baseVertex;
} DE_WARN_UNUSED_TYPE;

class PrimitiveList
{
public:
    PrimitiveList(PrimitiveType primitiveType, int numElements,
                  const int firstElement); // !< primitive list for drawArrays-like call
    PrimitiveList(PrimitiveType primitiveType, int numElements,
                  const DrawIndices &indices); // !< primitive list for drawElements-like call

    size_t getIndex(size_t elementNdx) const;
    bool isRestartIndex(size_t elementNdx, uint32_t restartIndex) const;

    inline size_t getNumElements(void) const
    {
        return m_numElements;
    }
    inline PrimitiveType getPrimitiveType(void) const
    {
        return m_primitiveType;
    }
    inline IndexType getIndexType(void) const
    {
        return m_indexType;
    }

private:
    const PrimitiveType m_primitiveType;
    const size_t m_numElements;
    const void *const
        m_indices; // !< if indices is NULL, indices is interpreted as [first (== baseVertex) + 0, first + 1, first + 2, ...]
    const IndexType m_indexType;
    const int m_baseVertex;
};

class DrawCommand
{
public:
    DrawCommand(const RenderState &state_, const RenderTarget &renderTarget_, const Program &program_,
                int numVertexAttribs_, const VertexAttrib *vertexAttribs_, const PrimitiveList &primitives_)
        : state(state_)
        , renderTarget(renderTarget_)
        , program(program_)
        , numVertexAttribs(numVertexAttribs_)
        , vertexAttribs(vertexAttribs_)
        , primitives(primitives_)
    {
    }

    const RenderState &state;
    const RenderTarget &renderTarget;
    const Program &program;

    const int numVertexAttribs;
    const VertexAttrib *const vertexAttribs;

    const PrimitiveList &primitives;
} DE_WARN_UNUSED_TYPE;

class Renderer
{
public:
    Renderer(void);
    ~Renderer(void);

    void draw(const DrawCommand &command) const;
    void drawInstanced(const DrawCommand &command, int numInstances) const;
} DE_WARN_UNUSED_TYPE;

} // namespace rr

#endif // _RRRENDERER_HPP
