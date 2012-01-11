/*
 * Copyright 2011 Adobe Systems Incorporated. All Rights Reserved.
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
#include "FECustomFilter.h"

#include "CachedShader.h"
#include "CustomFilterMesh.h"
#include "CustomFilterProgram.h"
#include "CustomFilterShader.h"
#include "Document.h"
#include "DrawingBuffer.h"
#include "FrameView.h"
#include "GraphicsContext3D.h"
#include "ImageData.h"
#include "RenderTreeAsText.h"
#include "StyleCachedShader.h"
#include "TextStream.h"
#include "Texture.h"
#include "TilingData.h"
#include "TransformationMatrix.h"

#include <wtf/ByteArray.h>

namespace WebCore {

static void orthogonalProjectionMatrix(TransformationMatrix& matrix, float left, float right, float bottom, float top)
{
    ASSERT(matrix.isIdentity());
    
    float deltaX = right - left;
    float deltaY = top - bottom;
    if (!deltaX || !deltaY)
        return;
    matrix.setM11(2.0f / deltaX);
    matrix.setM41(-(right + left) / deltaX);
    matrix.setM22(2.0f / deltaY);
    matrix.setM42(-(top + bottom) / deltaY);

    // Use big enough near/far values, so that simple rotations of rather large objects will not
    // get clipped. 10000 should cover most of the screen resolutions.
    const float farValue = 10000;
    const float nearValue = -10000;
    matrix.setM33(-2.0f / (farValue - nearValue));
    matrix.setM43(- (farValue + nearValue) / (farValue - nearValue));
    matrix.setM44(1.0f);
}

FECustomFilter::FECustomFilter(Filter* filter, Document* document, PassRefPtr<CustomFilterProgram> program,
                               unsigned meshRows, unsigned meshColumns, CustomFilterOperation::MeshBoxType meshBoxType,
                               CustomFilterOperation::MeshType meshType)
    : FilterEffect(filter)
    , m_document(document)
    , m_program(program)
    , m_meshRows(meshRows)
    , m_meshColumns(meshColumns)
    , m_meshBoxType(meshBoxType)
    , m_meshType(meshType)
{
}

PassRefPtr<FECustomFilter> FECustomFilter::create(Filter* filter, Document* document, PassRefPtr<CustomFilterProgram> program,
                                           unsigned meshRows, unsigned meshColumns, CustomFilterOperation::MeshBoxType meshBoxType,
                                           CustomFilterOperation::MeshType meshType)
{
    return adoptRef(new FECustomFilter(filter, document, program, meshRows, meshColumns, meshBoxType, meshType));
}

void FECustomFilter::platformApplySoftware()
{
    ByteArray* dstPixelArray = createPremultipliedImageResult();
    if (!dstPixelArray)
        return;

    FilterEffect* in = inputEffect(0);
    IntRect effectDrawingRect = requestedRegionOfInputImageData(in->absolutePaintRect());
    RefPtr<ByteArray> srcPixelArray = in->asPremultipliedImage(effectDrawingRect);
    
    IntSize newContextSize(effectDrawingRect.size());
    bool hadContext = m_context;
    if (!m_context)
        initializeContext(newContextSize);
    
    if (!hadContext || m_contextSize != newContextSize)
        resizeContext(newContextSize);
    
    // Do not draw the filter if the input image cannot fit inside a single GPU texture.
    if (m_inputTexture->tiles().numTiles() != 1)
        return;
    
    m_context->clearColor(0, 0, 0, 0);
    m_context->clear(GraphicsContext3D::COLOR_BUFFER_BIT | GraphicsContext3D::DEPTH_BUFFER_BIT);
    
    bindProgramAndBuffers(srcPixelArray.get());
    
    m_context->drawElements(GraphicsContext3D::TRIANGLES, m_mesh->indicesCount(), GraphicsContext3D::UNSIGNED_SHORT, 0);
    
    m_drawingBuffer->commit();

    RefPtr<ImageData> imageData = m_context->paintRenderingResultsToImageData(m_drawingBuffer.get());
    ByteArray* gpuResult = imageData->data()->data();
    ASSERT(gpuResult->length() == dstPixelArray->length());
    memcpy(dstPixelArray->data(), gpuResult->data(), gpuResult->length());
}

void FECustomFilter::initializeContext(const IntSize& contextSize)
{
    GraphicsContext3D::Attributes attributes;
    attributes.preserveDrawingBuffer = true;
    attributes.premultipliedAlpha = false;
    
    ASSERT(!m_context.get());
    m_context = GraphicsContext3D::create(attributes, m_document->view()->root()->hostWindow(), GraphicsContext3D::RenderOffscreen);
    m_drawingBuffer = DrawingBuffer::create(m_context.get(), contextSize, !attributes.preserveDrawingBuffer);
    
    m_shader = m_program->createShaderWithContext(m_context.get());
    m_mesh = CustomFilterMesh::create(m_context.get(), m_meshColumns, m_meshRows, 
                                      FloatRect(0, 0, 1, 1),
                                      m_meshType);
}

void FECustomFilter::resizeContext(const IntSize& newContextSize)
{
    m_inputTexture = 0;
    m_drawingBuffer->reset(newContextSize);
    m_context->reshape(newContextSize.width(), newContextSize.height());
    m_context->viewport(0, 0, newContextSize.width(), newContextSize.height());
    m_inputTexture = Texture::create(m_context.get(), Texture::RGBA8, newContextSize.width(), newContextSize.height());
    m_contextSize = newContextSize;
}

void FECustomFilter::bindVertexAttribute(int attributeLocation, unsigned size, unsigned& offset)
{
    if (attributeLocation != -1) {
        m_context->vertexAttribPointer(attributeLocation, 4, GraphicsContext3D::FLOAT, false, m_mesh->bytesPerVertex(), offset);
        m_context->enableVertexAttribArray(attributeLocation);
    }
    offset += size * sizeof(float);
}

void FECustomFilter::bindProgramAndBuffers(ByteArray* srcPixelArray)
{
    m_context->useProgram(m_shader->program());
    
    if (m_shader->samplerLocation() != -1) {
        m_context->activeTexture(GraphicsContext3D::TEXTURE0);
        m_context->uniform1i(m_shader->samplerLocation(), 0);
        m_inputTexture->load(srcPixelArray->data());
        m_inputTexture->bindTile(0);
    }
    
    if (m_shader->projectionMatrixLocation() != -1) {
        TransformationMatrix projectionMatrix; 
        orthogonalProjectionMatrix(projectionMatrix, -0.5, 0.5, -0.5, 0.5);
        float glProjectionMatrix[16];
        projectionMatrix.toColumnMajorFloatArray(glProjectionMatrix);
        m_context->uniformMatrix4fv(m_shader->projectionMatrixLocation(), false, &glProjectionMatrix[0], 1);
    }
    
    m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_mesh->verticesBufferObject());
    m_context->bindBuffer(GraphicsContext3D::ELEMENT_ARRAY_BUFFER, m_mesh->elementsBufferObject());

    unsigned offset = 0;
    bindVertexAttribute(m_shader->positionAttribLocation(), 4, offset);
    bindVertexAttribute(m_shader->texAttribLocation(), 2, offset);
    bindVertexAttribute(m_shader->meshAttribLocation(), 2, offset);
    if (m_meshType == CustomFilterOperation::DETACHED)
        bindVertexAttribute(m_shader->triangleAttribLocation(), 3, offset);
}

void FECustomFilter::dump()
{
}

TextStream& FECustomFilter::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feCustomFilter";
    FilterEffect::externalRepresentation(ts);
    ts << "]\n";
    inputEffect(0)->externalRepresentation(ts, indent + 1);
    return ts;
}

} // namespace WebCore

#endif // ENABLE(CSS_SHADERS) && ENABLE(WEBGL)
