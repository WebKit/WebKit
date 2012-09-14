/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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

#if ENABLE(CSS_SHADERS) && USE(3D_GRAPHICS)
#include "FECustomFilter.h"

#include "CustomFilterArrayParameter.h"
#include "CustomFilterCompiledProgram.h"
#include "CustomFilterGlobalContext.h"
#include "CustomFilterMesh.h"
#include "CustomFilterNumberParameter.h"
#include "CustomFilterParameter.h"
#include "CustomFilterProgram.h"
#include "CustomFilterTransformParameter.h"
#include "CustomFilterValidatedProgram.h"
#include "DrawingBuffer.h"
#include "GraphicsContext3D.h"
#include "ImageData.h"
#include "NotImplemented.h"
#include "RenderTreeAsText.h"
#include "TextStream.h"
#include "Texture.h"
#include "TilingData.h"
#include "TransformationMatrix.h"

#include <wtf/Uint8ClampedArray.h>

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

FECustomFilter::FECustomFilter(Filter* filter, CustomFilterGlobalContext* customFilterGlobalContext, PassRefPtr<CustomFilterValidatedProgram> validatedProgram, const CustomFilterParameterList& parameters,
                               unsigned meshRows, unsigned meshColumns, CustomFilterOperation::MeshBoxType,
                               CustomFilterOperation::MeshType meshType)
    : FilterEffect(filter)
    , m_globalContext(customFilterGlobalContext)
    , m_validatedProgram(validatedProgram)
    , m_compiledProgram(0) // Don't compile the program unless we need to paint.
    , m_inputTexture(0)
    , m_frameBuffer(0)
    , m_depthBuffer(0)
    , m_destTexture(0)
    , m_parameters(parameters)
    , m_meshRows(meshRows)
    , m_meshColumns(meshColumns)
    , m_meshType(meshType)
{
    // An FECustomFilter shouldn't have been created unless the program passed validation.
    ASSERT(m_validatedProgram->isInitialized());
}

PassRefPtr<FECustomFilter> FECustomFilter::create(Filter* filter, CustomFilterGlobalContext* customFilterGlobalContext, PassRefPtr<CustomFilterValidatedProgram> validatedProgram, const CustomFilterParameterList& parameters,
                                           unsigned meshRows, unsigned meshColumns, CustomFilterOperation::MeshBoxType meshBoxType,
                                           CustomFilterOperation::MeshType meshType)
{
    return adoptRef(new FECustomFilter(filter, customFilterGlobalContext, validatedProgram, parameters, meshRows, meshColumns, meshBoxType, meshType));
}

FECustomFilter::~FECustomFilter()
{
    deleteRenderBuffers();
}

void FECustomFilter::deleteRenderBuffers()
{
    if (!m_context)
        return;
    m_context->makeContextCurrent();
    if (m_inputTexture) {
        m_context->deleteTexture(m_inputTexture);
        m_inputTexture = 0;
    }
    if (m_frameBuffer) {
        // Make sure to unbind any framebuffer from the context first, otherwise
        // some platforms might refuse to bind the same buffer id again.
        m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, 0);
        m_context->deleteFramebuffer(m_frameBuffer);
        m_frameBuffer = 0;
    }
    if (m_depthBuffer) {
        m_context->deleteRenderbuffer(m_depthBuffer);
        m_depthBuffer = 0;
    }
    if (m_destTexture) {
        m_context->deleteTexture(m_destTexture);
        m_destTexture = 0;
    }
}

void FECustomFilter::platformApplySoftware()
{
    if (!applyShader())
        clearShaderResult();
}

void FECustomFilter::clearShaderResult()
{
    clearResult();
    Uint8ClampedArray* dstPixelArray = createUnmultipliedImageResult();
    if (!dstPixelArray)
        return;

    FilterEffect* in = inputEffect(0);
    setIsAlphaImage(in->isAlphaImage());
    IntRect effectDrawingRect = requestedRegionOfInputImageData(in->absolutePaintRect());
    in->copyUnmultipliedImage(dstPixelArray, effectDrawingRect);
}

void FECustomFilter::drawFilterMesh(Platform3DObject inputTexture)
{
    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_frameBuffer);
    m_context->viewport(0, 0, m_contextSize.width(), m_contextSize.height());
    
    m_context->clearColor(0, 0, 0, 0);
    m_context->clear(GraphicsContext3D::COLOR_BUFFER_BIT | GraphicsContext3D::DEPTH_BUFFER_BIT);
    
    bindProgramAndBuffers(inputTexture);
    m_context->drawElements(GraphicsContext3D::TRIANGLES, m_mesh->indicesCount(), GraphicsContext3D::UNSIGNED_SHORT, 0);
    unbindVertexAttributes();
}

bool FECustomFilter::prepareForDrawing(CustomFilterDrawType filterDrawType)
{
    if (!m_context && !initializeContext())
        return false;
    m_context->makeContextCurrent();

    // If the shader had compiler errors we cannot draw anything.
    if (!m_compiledProgram->isInitialized())
        return false;
    
    // Only allocate a texture if the program needs one and the caller doesn't allocate one by itself.
    if ((programNeedsInputTexture() && (filterDrawType == NEEDS_INPUT_TEXTURE) && !ensureInputTexture())
        || !ensureFrameBuffer())
        return false;
    
    return true;
}

bool FECustomFilter::programNeedsInputTexture() const
{
    ASSERT(m_compiledProgram.get());
    return m_compiledProgram->samplerLocation() != -1;
}

bool FECustomFilter::applyShader()
{
    Uint8ClampedArray* dstPixelArray = createUnmultipliedImageResult();
    if (!dstPixelArray)
        return false;

    if (!prepareForDrawing())
        return false;

    FilterEffect* in = inputEffect(0);
    IntRect effectDrawingRect = requestedRegionOfInputImageData(in->absolutePaintRect());
    IntSize newContextSize(effectDrawingRect.size());
    if (!resizeContextIfNeeded(newContextSize))
        return false;

    bool needsInputTexture = programNeedsInputTexture();
    if (needsInputTexture) {
        RefPtr<Uint8ClampedArray> srcPixelArray = in->asUnmultipliedImage(effectDrawingRect);
        uploadInputTexture(srcPixelArray.get());
    }
    drawFilterMesh(needsInputTexture ? m_inputTexture : 0);

    ASSERT(static_cast<size_t>(newContextSize.width() * newContextSize.height() * 4) == dstPixelArray->length());
    m_context->readPixels(0, 0, newContextSize.width(), newContextSize.height(), GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, dstPixelArray->data());

    return true;
}

bool FECustomFilter::initializeContext()
{
    ASSERT(!m_context.get());
    m_context = m_globalContext->context();
    if (!m_context)
        return false;
    m_context->makeContextCurrent();
    m_compiledProgram = m_validatedProgram->compiledProgram();

    // FIXME: Sharing the mesh would just save the time needed to upload it to the GPU, so I assume we could
    // benchmark that for performance.
    // https://bugs.webkit.org/show_bug.cgi?id=88429
    m_mesh = CustomFilterMesh::create(m_context.get(), m_meshColumns, m_meshRows, 
                                      FloatRect(0, 0, 1, 1),
                                      m_meshType);

    return true;
}

bool FECustomFilter::ensureInputTexture()
{
    if (!m_inputTexture)
        m_inputTexture = m_context->createTexture();
    return m_inputTexture;
}

void FECustomFilter::uploadInputTexture(Uint8ClampedArray* srcPixelArray)
{
    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_inputTexture);
    m_context->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, m_contextSize.width(), m_contextSize.height(), 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, srcPixelArray->data());
}

bool FECustomFilter::ensureFrameBuffer()
{
    if (!m_frameBuffer)
        m_frameBuffer = m_context->createFramebuffer();
    if (!m_depthBuffer)
        m_depthBuffer = m_context->createRenderbuffer();
    if (!m_destTexture)
        m_destTexture = m_context->createTexture();
    return m_frameBuffer && m_depthBuffer && m_destTexture;
}

bool FECustomFilter::resizeContextIfNeeded(const IntSize& newContextSize)
{
    if (newContextSize.isEmpty())
        return false;
    if (m_contextSize == newContextSize)
        return true;

    int maxTextureSize = 0;
    m_context->getIntegerv(GraphicsContext3D::MAX_TEXTURE_SIZE, &maxTextureSize);
    if (newContextSize.height() > maxTextureSize || newContextSize.width() > maxTextureSize)
        return false;

    return resizeContext(newContextSize);
}

bool FECustomFilter::resizeContext(const IntSize& newContextSize)
{
    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_frameBuffer);
    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_destTexture);
    // We are going to clear the output buffer anyway, so we can safely initialize the destination texture with garbage data.
#if PLATFORM(CHROMIUM)
    // FIXME: GraphicsContext3D::texImage2DDirect is not implemented on Chromium.
    m_context->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, newContextSize.width(), newContextSize.height(), 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, 0);
#else
    m_context->texImage2DDirect(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, newContextSize.width(), newContextSize.height(), 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, 0);
#endif
    m_context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, m_destTexture, 0);
 
    m_context->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, m_depthBuffer);
    m_context->renderbufferStorage(GraphicsContext3D::RENDERBUFFER, GraphicsContext3D::DEPTH_COMPONENT16, newContextSize.width(), newContextSize.height());
    m_context->framebufferRenderbuffer(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::DEPTH_ATTACHMENT, GraphicsContext3D::RENDERBUFFER, m_depthBuffer);

    if (m_context->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER) != GraphicsContext3D::FRAMEBUFFER_COMPLETE)
        return false;

    m_context->bindRenderbuffer(GraphicsContext3D::RENDERBUFFER, 0);
    
    m_contextSize = newContextSize;
    return true;
}

void FECustomFilter::bindVertexAttribute(int attributeLocation, unsigned size, unsigned offset)
{
    if (attributeLocation != -1) {
        m_context->vertexAttribPointer(attributeLocation, size, GraphicsContext3D::FLOAT, false, m_mesh->bytesPerVertex(), offset);
        m_context->enableVertexAttribArray(attributeLocation);
    }
}

void FECustomFilter::unbindVertexAttribute(int attributeLocation)
{
    if (attributeLocation != -1)
        m_context->disableVertexAttribArray(attributeLocation);
}

void FECustomFilter::bindProgramArrayParameters(int uniformLocation, CustomFilterArrayParameter* arrayParameter)
{
    unsigned parameterSize = arrayParameter->size();
    Vector<GC3Dfloat> floatVector;

    for (unsigned i = 0; i < parameterSize; ++i)
        floatVector.append(arrayParameter->valueAt(i));

    m_context->uniform1fv(uniformLocation, parameterSize, floatVector.data());
}

void FECustomFilter::bindProgramNumberParameters(int uniformLocation, CustomFilterNumberParameter* numberParameter)
{
    switch (numberParameter->size()) {
    case 1:
        m_context->uniform1f(uniformLocation, numberParameter->valueAt(0));
        break;
    case 2:
        m_context->uniform2f(uniformLocation, numberParameter->valueAt(0), numberParameter->valueAt(1));
        break;
    case 3:
        m_context->uniform3f(uniformLocation, numberParameter->valueAt(0), numberParameter->valueAt(1), numberParameter->valueAt(2));
        break;
    case 4:
        m_context->uniform4f(uniformLocation, numberParameter->valueAt(0), numberParameter->valueAt(1), numberParameter->valueAt(2), numberParameter->valueAt(3));
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void FECustomFilter::bindProgramTransformParameter(int uniformLocation, CustomFilterTransformParameter* transformParameter)
{
    TransformationMatrix matrix;
    if (m_contextSize.width() && m_contextSize.height()) {
        // The viewport is a box with the size of 1 unit, so we are scalling up here to make sure that translations happen using real pixel
        // units. At the end we scale back down in order to map it back to the original box. Note that transforms come in reverse order, because it is 
        // supposed to multiply to the left of the coordinates of the vertices.
        // Note that the origin (0, 0) of the viewport is in the middle of the context, so there's no need to change the origin of the transform
        // in order to rotate around the middle of mesh.
        matrix.scale3d(1.0 / m_contextSize.width(), 1.0 / m_contextSize.height(), 1);
        transformParameter->applyTransform(matrix, m_contextSize);
        matrix.scale3d(m_contextSize.width(), m_contextSize.height(), 1);
    }
    float glMatrix[16];
    matrix.toColumnMajorFloatArray(glMatrix);
    m_context->uniformMatrix4fv(uniformLocation, 1, false, &glMatrix[0]);
}

void FECustomFilter::bindProgramParameters()
{
    // FIXME: Find a way to reset uniforms that are not specified in CSS. This is needed to avoid using values
    // set by other previous rendered filters.
    // https://bugs.webkit.org/show_bug.cgi?id=76440
    
    size_t parametersSize = m_parameters.size();
    for (size_t i = 0; i < parametersSize; ++i) {
        CustomFilterParameter* parameter = m_parameters.at(i).get();
        int uniformLocation = m_compiledProgram->uniformLocationByName(parameter->name());
        if (uniformLocation == -1)
            continue;
        switch (parameter->parameterType()) {
        case CustomFilterParameter::ARRAY:
            bindProgramArrayParameters(uniformLocation, static_cast<CustomFilterArrayParameter*>(parameter));
            break;
        case CustomFilterParameter::NUMBER:
            bindProgramNumberParameters(uniformLocation, static_cast<CustomFilterNumberParameter*>(parameter));
            break;
        case CustomFilterParameter::TRANSFORM:
            bindProgramTransformParameter(uniformLocation, static_cast<CustomFilterTransformParameter*>(parameter));
            break;
        }
    }
}

void FECustomFilter::bindProgramAndBuffers(Platform3DObject inputTexture)
{
    ASSERT(m_compiledProgram->isInitialized());

    m_context->useProgram(m_compiledProgram->program());
    
    if (programNeedsInputTexture()) {
        // We should be binding the DOM element texture sampler only if the author is using the CSS mix function.
        ASSERT(m_validatedProgram->programInfo().programType() == PROGRAM_TYPE_BLENDS_ELEMENT_TEXTURE);
        ASSERT(m_compiledProgram->samplerLocation() != -1);

        m_context->activeTexture(GraphicsContext3D::TEXTURE0);
        m_context->uniform1i(m_compiledProgram->samplerLocation(), 0);
        m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, inputTexture);
        m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
        m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
        m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
        m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
    }
    
    if (m_compiledProgram->projectionMatrixLocation() != -1) {
        TransformationMatrix projectionMatrix; 
        orthogonalProjectionMatrix(projectionMatrix, -0.5, 0.5, -0.5, 0.5);
        float glProjectionMatrix[16];
        projectionMatrix.toColumnMajorFloatArray(glProjectionMatrix);
        m_context->uniformMatrix4fv(m_compiledProgram->projectionMatrixLocation(), 1, false, &glProjectionMatrix[0]);
    }
    
    m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_mesh->verticesBufferObject());
    m_context->bindBuffer(GraphicsContext3D::ELEMENT_ARRAY_BUFFER, m_mesh->elementsBufferObject());

    // FIXME: Ideally, these should be public members of CustomFilterMesh.
    // https://bugs.webkit.org/show_bug.cgi?id=94755
    static const unsigned PositionAttribSize = 4;
    static const unsigned TexAttribSize = 2;
    static const unsigned MeshAttribSize = 2;
    static const unsigned TriangleAttribSize = 3;

    static const unsigned PositionAttribOffset = 0;
    static const unsigned TexAttribOffset = PositionAttribOffset + PositionAttribSize * sizeof(float);
    static const unsigned MeshAttribOffset = TexAttribOffset + TexAttribSize * sizeof(float);
    static const unsigned TriangleAttribOffset = MeshAttribOffset + MeshAttribSize * sizeof(float);

    bindVertexAttribute(m_compiledProgram->positionAttribLocation(), PositionAttribSize, PositionAttribOffset);
    bindVertexAttribute(m_compiledProgram->texAttribLocation(), TexAttribSize, TexAttribOffset);
    // FIXME: Get rid of the internal tex coord attribute "css_a_texCoord". 
    // https://bugs.webkit.org/show_bug.cgi?id=94358
    bindVertexAttribute(m_compiledProgram->internalTexCoordAttribLocation(), TexAttribSize, TexAttribOffset);
    bindVertexAttribute(m_compiledProgram->meshAttribLocation(), MeshAttribSize, MeshAttribOffset);
    if (m_meshType == CustomFilterOperation::DETACHED)
        bindVertexAttribute(m_compiledProgram->triangleAttribLocation(), TriangleAttribSize, TriangleAttribOffset);
    
    bindProgramParameters();
}

void FECustomFilter::unbindVertexAttributes()
{
    unbindVertexAttribute(m_compiledProgram->positionAttribLocation());
    unbindVertexAttribute(m_compiledProgram->texAttribLocation());
    unbindVertexAttribute(m_compiledProgram->internalTexCoordAttribLocation());
    unbindVertexAttribute(m_compiledProgram->meshAttribLocation());
    if (m_meshType == CustomFilterOperation::DETACHED)
        unbindVertexAttribute(m_compiledProgram->triangleAttribLocation());
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

#endif // ENABLE(CSS_SHADERS) && USE(3D_GRAPHICS)
