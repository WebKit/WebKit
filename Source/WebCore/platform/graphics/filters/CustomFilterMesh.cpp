/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(CSS_SHADERS) && ENABLE(WEBGL)
#include "CustomFilterMesh.h"
#include "GraphicsContext3D.h"

namespace WebCore {
    
#ifndef NDEBUG
// Use "call 'WebCore::s_dumpCustomFilterMeshBuffers' = 1" in GDB to activate printing of the mesh buffers.
static bool s_dumpCustomFilterMeshBuffers = false;
#endif

class MeshGenerator {
public:
    // Lines and columns are the values passed in CSS. The result is vertex mesh that has 'rows' numbers of rows
    // and 'columns' number of columns with a total of 'rows + 1' * 'columns + 1' vertices.
    // MeshBox is the filtered area calculated defined using the border-box, padding-box, content-box or filter-box 
    // attributes. A value of (0, 0, 1, 1) will cover the entire output surface.
    MeshGenerator(unsigned columns, unsigned rows, const FloatRect& meshBox, CustomFilterOperation::MeshType meshType)
        : m_meshType(meshType)
        , m_points(columns + 1, rows + 1)
        , m_tiles(columns, rows)
        , m_tileSizeInPixels(meshBox.width() / m_tiles.width(), meshBox.height() / m_tiles.height())
        , m_tileSizeInDeviceSpace(1.0f / m_tiles.width(), 1.0f / m_tiles.height())
        , m_meshBox(meshBox)
    {
        // Build the two buffers needed to draw triangles:
        // * m_vertices has a number of float attributes that will be passed to the vertex shader
        // for each computed vertex. This number is calculated in floatsPerVertex() based on the meshType.
        // * m_indices is a buffer that will have 3 indices per triangle. Each index will point inside
        // the m_vertices buffer.
        m_vertices.reserveCapacity(verticesCount() * floatsPerVertex());
        m_indices.reserveCapacity(indicesCount());
        
        // Based on the meshType there can be two types of meshes.
        // * attached: each triangle uses vertices from the neighbor triangles. This is useful to save some GPU memory
        // when there's no need to explode the tiles.
        // * detached: each triangle has its own vertices. This means each triangle can be moved independently and a vec3
        // attribute is passed, so that each vertex can be uniquely identified.
        if (m_meshType == CustomFilterOperation::ATTACHED)
            generateAttachedMesh();
        else
            generateDetachedMesh();
        
#ifndef NDEBUG
        if (s_dumpCustomFilterMeshBuffers)
            dumpBuffers();
#endif
    }

    const Vector<float>& vertices() const { return m_vertices; }
    const Vector<uint16_t>& indices() const { return m_indices; }

    const IntSize& points() const { return m_points; }
    unsigned pointsCount() const { return m_points.width() * m_points.height(); }
    
    const IntSize& tiles() const { return m_tiles; }
    unsigned tilesCount() const { return m_tiles.width() * m_tiles.height(); }
    
    unsigned indicesCount() const
    {
        const unsigned trianglesPerTile = 2;
        const unsigned indicesPerTriangle = 3;
        return tilesCount() * trianglesPerTile * indicesPerTriangle;
    }
    
    unsigned floatsPerVertex() const
    {
        static const unsigned AttachedMeshVertexSize = 4 + // vec4 a_position
                                                       2 + // vec2 a_texCoord
                                                       2; // vec2 a_meshCoord

        static const unsigned DetachedMeshVertexSize = AttachedMeshVertexSize +
                                                       3; // vec3 a_triangleCoord

        return m_meshType == CustomFilterOperation::ATTACHED ? AttachedMeshVertexSize : DetachedMeshVertexSize;
    }
    
    unsigned verticesCount() const
    {
        return m_meshType == CustomFilterOperation::ATTACHED ? pointsCount() : indicesCount();
    }

private:
    typedef void (MeshGenerator::*AddTriangleVertexFunction)(int quadX, int quadY, int triangleX, int triangleY, int triangle);
    
    template <AddTriangleVertexFunction addTriangleVertex>
    void addTile(int quadX, int quadY)
    {
        ((*this).*(addTriangleVertex))(quadX, quadY, 0, 0, 1);
        ((*this).*(addTriangleVertex))(quadX, quadY, 1, 0, 2);
        ((*this).*(addTriangleVertex))(quadX, quadY, 1, 1, 3);
        ((*this).*(addTriangleVertex))(quadX, quadY, 0, 0, 4);
        ((*this).*(addTriangleVertex))(quadX, quadY, 1, 1, 5);
        ((*this).*(addTriangleVertex))(quadX, quadY, 0, 1, 6);
    }
    
    void addAttachedMeshIndex(int quadX, int quadY, int triangleX, int triangleY, int triangle)
    {
        UNUSED_PARAM(triangle);
        m_indices.append((quadY + triangleY) * m_points.width() + (quadX + triangleX));
    }
    
    void generateAttachedMesh()
    {
        for (int j = 0; j < m_points.height(); ++j) {
            for (int i = 0; i < m_points.width(); ++i)
                addAttachedMeshVertexAttributes(i, j);
        }
        
        for (int j = 0; j < m_tiles.height(); ++j) {
            for (int i = 0; i < m_tiles.width(); ++i)
                addTile<&MeshGenerator::addAttachedMeshIndex>(i, j);
        }
    }
    
    void addDetachedMeshVertexAndIndex(int quadX, int quadY, int triangleX, int triangleY, int triangle)
    {
        addDetachedMeshVertexAttributes(quadX, quadY, triangleX, triangleY, triangle);
        m_indices.append(m_indices.size());
    }
    
    void generateDetachedMesh()
    {
        for (int j = 0; j < m_tiles.height(); ++j) {
            for (int i = 0; i < m_tiles.width(); ++i)
                addTile<&MeshGenerator::addDetachedMeshVertexAndIndex>(i, j);
        }
    }
    
    void addPositionAttribute(int quadX, int quadY)
    {
        // vec4 a_position
        m_vertices.append(m_tileSizeInPixels.width() * quadX - 0.5f + m_meshBox.x());
        m_vertices.append(m_tileSizeInPixels.height() * quadY - 0.5f + m_meshBox.y());
        m_vertices.append(0.0f); // z
        m_vertices.append(1.0f);
    }

    void addTexCoordAttribute(int quadX, int quadY)
    {
        // vec2 a_texCoord
        m_vertices.append(m_tileSizeInPixels.width() * quadX + m_meshBox.x());
        m_vertices.append(m_tileSizeInPixels.height() * quadY + m_meshBox.y());
    }

    void addMeshCoordAttribute(int quadX, int quadY)
    {
        // vec2 a_meshCoord
        m_vertices.append(m_tileSizeInDeviceSpace.width() * quadX);
        m_vertices.append(m_tileSizeInDeviceSpace.height() * quadY);
    }

    void addTriangleCoordAttribute(int quadX, int quadY, int triangle)
    {
        // vec3 a_triangleCoord
        m_vertices.append(quadX);
        m_vertices.append(quadY);
        m_vertices.append(triangle);
    }

    void addAttachedMeshVertexAttributes(int quadX, int quadY)
    {
        addPositionAttribute(quadX, quadY);
        addTexCoordAttribute(quadX, quadY);
        addMeshCoordAttribute(quadX, quadY);
    }

    void addDetachedMeshVertexAttributes(int quadX, int quadY, int triangleX, int triangleY, int triangle)
    {
        addAttachedMeshVertexAttributes(quadX + triangleX, quadY + triangleY);
        addTriangleCoordAttribute(quadX, quadY, triangle);
    }
    
#ifndef NDEBUG
    void dumpBuffers() const
    {
        printf("Mesh buffers: Points.width(): %d, Points.height(): %d meshBox: %f, %f, %f, %f, type: %s\n", 
               m_points.width(), m_points.height(), m_meshBox.x(), m_meshBox.y(), m_meshBox.width(), m_meshBox.height(), 
               (m_meshType == CustomFilterOperation::ATTACHED) ? "Attached" : "Detached");
        printf("---Vertex:\n\t");
        for (unsigned i = 0; i < m_vertices.size(); ++i) {
            printf("%f ", m_vertices.at(i));
            if (!((i + 1) % floatsPerVertex()))
                printf("\n\t");
        }
        printf("\n---Indices: ");
        for (unsigned i = 0; i < m_indices.size(); ++i)
            printf("%d ", m_indices.at(i));
        printf("\n");
    }
#endif

private:
    Vector<float> m_vertices;
    Vector<uint16_t> m_indices;
    
    CustomFilterOperation::MeshType m_meshType;
    IntSize m_points;
    IntSize m_tiles;
    FloatSize m_tileSizeInPixels;
    FloatSize m_tileSizeInDeviceSpace;
    FloatRect m_meshBox;
};

CustomFilterMesh::CustomFilterMesh(GraphicsContext3D* context, unsigned columns, unsigned rows, 
                                   const FloatRect& meshBox, CustomFilterOperation::MeshType meshType)
    : m_context(context)
    , m_verticesBufferObject(0)
    , m_elementsBufferObject(0)
    , m_meshBox(meshBox)
    , m_meshType(meshType)
{
    MeshGenerator generator(columns, rows, meshBox, meshType);
    m_indicesCount = generator.indicesCount();
    m_bytesPerVertex = generator.floatsPerVertex() * sizeof(float);    

    m_verticesBufferObject = m_context->createBuffer();
    m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_verticesBufferObject);
    m_context->bufferData(GraphicsContext3D::ARRAY_BUFFER, generator.vertices().size() * sizeof(float), generator.vertices().data(), GraphicsContext3D::STATIC_DRAW);
    
    m_elementsBufferObject = m_context->createBuffer();
    m_context->bindBuffer(GraphicsContext3D::ELEMENT_ARRAY_BUFFER, m_elementsBufferObject);
    m_context->bufferData(GraphicsContext3D::ELEMENT_ARRAY_BUFFER, generator.indices().size() * sizeof(uint16_t), generator.indices().data(), GraphicsContext3D::STATIC_DRAW);
}

CustomFilterMesh::~CustomFilterMesh()
{
    m_context->deleteBuffer(m_verticesBufferObject);
    m_context->deleteBuffer(m_elementsBufferObject);
}

} // namespace WebCore

#endif // ENABLE(CSS_SHADERS) && ENABLE(WEBGL)

