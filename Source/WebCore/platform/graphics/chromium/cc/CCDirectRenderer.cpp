/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "CCDirectRenderer.h"

#include "CCMathUtil.h"
#include <public/WebTransformationMatrix.h>

using WebKit::WebTransformationMatrix;

static WebTransformationMatrix orthoProjectionMatrix(float left, float right, float bottom, float top)
{
    // Use the standard formula to map the clipping frustum to the cube from
    // [-1, -1, -1] to [1, 1, 1].
    float deltaX = right - left;
    float deltaY = top - bottom;
    WebTransformationMatrix proj;
    if (!deltaX || !deltaY)
        return proj;
    proj.setM11(2.0f / deltaX);
    proj.setM41(-(right + left) / deltaX);
    proj.setM22(2.0f / deltaY);
    proj.setM42(-(top + bottom) / deltaY);

    // Z component of vertices is always set to zero as we don't use the depth buffer
    // while drawing.
    proj.setM33(0);

    return proj;
}

static WebTransformationMatrix windowMatrix(int x, int y, int width, int height)
{
    WebTransformationMatrix canvas;

    // Map to window position and scale up to pixel coordinates.
    canvas.translate3d(x, y, 0);
    canvas.scale3d(width, height, 0);

    // Map from ([-1, -1] to [1, 1]) -> ([0, 0] to [1, 1])
    canvas.translate3d(0.5, 0.5, 0.5);
    canvas.scale3d(0.5, 0.5, 0.5);

    return canvas;
}

namespace WebCore {
//
// static
FloatRect CCDirectRenderer::quadVertexRect()
{
    return FloatRect(-0.5, -0.5, 1, 1);
}

// static
void CCDirectRenderer::quadRectTransform(WebKit::WebTransformationMatrix* quadRectTransform, const WebKit::WebTransformationMatrix& quadTransform, const FloatRect& quadRect)
{
    *quadRectTransform = quadTransform;
    quadRectTransform->translate(0.5 * quadRect.width() + quadRect.x(), 0.5 * quadRect.height() + quadRect.y());
    quadRectTransform->scaleNonUniform(quadRect.width(), quadRect.height());
}

// static
void CCDirectRenderer::initializeMatrices(DrawingFrame& frame, const IntRect& drawRect, bool flipY)
{
    if (flipY)
        frame.projectionMatrix = orthoProjectionMatrix(drawRect.x(), drawRect.maxX(), drawRect.maxY(), drawRect.y());
    else
        frame.projectionMatrix = orthoProjectionMatrix(drawRect.x(), drawRect.maxX(), drawRect.y(), drawRect.maxY());
    frame.windowMatrix = windowMatrix(0, 0, drawRect.width(), drawRect.height());
    frame.flippedY = flipY;
}

// static
IntRect CCDirectRenderer::moveScissorToWindowSpace(const DrawingFrame& frame, FloatRect scissorRect)
{
    IntRect scissorRectInCanvasSpace = enclosingIntRect(scissorRect);
    // The scissor coordinates must be supplied in viewport space so we need to offset
    // by the relative position of the top left corner of the current render pass.
    IntRect framebufferOutputRect = frame.currentRenderPass->outputRect();
    scissorRectInCanvasSpace.setX(scissorRectInCanvasSpace.x() - framebufferOutputRect.x());
    if (frame.flippedY && !frame.currentTexture)
        scissorRectInCanvasSpace.setY(framebufferOutputRect.height() - (scissorRectInCanvasSpace.maxY() - framebufferOutputRect.y()));
    else
        scissorRectInCanvasSpace.setY(scissorRectInCanvasSpace.y() - framebufferOutputRect.y());
    return scissorRectInCanvasSpace;
}

void CCDirectRenderer::decideRenderPassAllocationsForFrame(const CCRenderPassList& renderPassesInDrawOrder)
{
    HashMap<int, const CCRenderPass*> renderPassesInFrame;
    for (size_t i = 0; i < renderPassesInDrawOrder.size(); ++i)
        renderPassesInFrame.set(renderPassesInDrawOrder[i]->id(), renderPassesInDrawOrder[i]);

    Vector<int> passesToDelete;
    HashMap<int, OwnPtr<CachedTexture> >::const_iterator passIterator;
    for (passIterator = m_renderPassTextures.begin(); passIterator != m_renderPassTextures.end(); ++passIterator) {
        const CCRenderPass* renderPassInFrame = renderPassesInFrame.get(passIterator->first);
        if (!renderPassInFrame) {
            passesToDelete.append(passIterator->first);
            continue;
        }

        const IntSize& requiredSize = renderPassTextureSize(renderPassInFrame);
        GC3Denum requiredFormat = renderPassTextureFormat(renderPassInFrame);
        CachedTexture* texture = passIterator->second.get();
        ASSERT(texture);

        if (texture->id() && (texture->size() != requiredSize || texture->format() != requiredFormat))
            texture->free();
    }

    // Delete RenderPass textures from the previous frame that will not be used again.
    for (size_t i = 0; i < passesToDelete.size(); ++i)
        m_renderPassTextures.remove(passesToDelete[i]);

    for (size_t i = 0; i < renderPassesInDrawOrder.size(); ++i) {
        if (!m_renderPassTextures.contains(renderPassesInDrawOrder[i]->id())) {
            OwnPtr<CachedTexture> texture = CachedTexture::create(m_resourceProvider);
            m_renderPassTextures.set(renderPassesInDrawOrder[i]->id(), texture.release());
        }
    }
}

void CCDirectRenderer::drawFrame(const CCRenderPassList& renderPassesInDrawOrder, const CCRenderPassIdHashMap& renderPassesById)
{
    const CCRenderPass* rootRenderPass = renderPassesInDrawOrder.last();
    ASSERT(rootRenderPass);

    DrawingFrame frame;
    frame.renderPassesById = &renderPassesById;
    frame.rootRenderPass = rootRenderPass;
    frame.rootDamageRect = capabilities().usingPartialSwap ? rootRenderPass->damageRect() : rootRenderPass->outputRect();
    frame.rootDamageRect.intersect(IntRect(IntPoint::zero(), viewportSize()));

    beginDrawingFrame(frame);
    for (size_t i = 0; i < renderPassesInDrawOrder.size(); ++i)
        drawRenderPass(frame, renderPassesInDrawOrder[i]);
    finishDrawingFrame(frame);
}

void CCDirectRenderer::drawRenderPass(DrawingFrame& frame, const CCRenderPass* renderPass)
{
    if (!useRenderPass(frame, renderPass))
        return;

    frame.scissorRectInRenderPassSpace = frame.currentRenderPass->outputRect();
    if (frame.rootDamageRect != frame.rootRenderPass->outputRect()) {
        WebTransformationMatrix inverseTransformToRoot = frame.currentRenderPass->transformToRootTarget().inverse();
        frame.scissorRectInRenderPassSpace.intersect(CCMathUtil::projectClippedRect(inverseTransformToRoot, frame.rootDamageRect));
    }

    enableScissorTestRect(moveScissorToWindowSpace(frame, frame.scissorRectInRenderPassSpace));
    clearFramebuffer(frame);

    const CCQuadList& quadList = renderPass->quadList();
    for (CCQuadList::constBackToFrontIterator it = quadList.backToFrontBegin(); it != quadList.backToFrontEnd(); ++it) {
        FloatRect quadScissorRect = frame.scissorRectInRenderPassSpace;
        quadScissorRect.intersect(it->get()->clippedRectInTarget());
        if (!quadScissorRect.isEmpty()) {
            enableScissorTestRect(moveScissorToWindowSpace(frame, quadScissorRect));
            drawQuad(frame, it->get());
        }
    }

    CachedTexture* texture = m_renderPassTextures.get(renderPass->id());
    if (texture)
        texture->setIsComplete(!renderPass->hasOcclusionFromOutsideTargetSurface());
}

bool CCDirectRenderer::useRenderPass(DrawingFrame& frame, const CCRenderPass* renderPass)
{
    frame.currentRenderPass = renderPass;
    frame.currentTexture = 0;

    if (renderPass == frame.rootRenderPass) {
        bindFramebufferToOutputSurface(frame);
        initializeMatrices(frame, renderPass->outputRect(), true);
        setDrawViewportSize(renderPass->outputRect().size());
        return true;
    }

    CachedTexture* texture = m_renderPassTextures.get(renderPass->id());
    ASSERT(texture);
    if (!texture->id() && !texture->allocate(CCRenderer::ImplPool, renderPassTextureSize(renderPass), renderPassTextureFormat(renderPass), CCResourceProvider::TextureUsageFramebuffer))
        return false;

    return bindFramebufferToTexture(frame, texture, renderPass->outputRect());
}

bool CCDirectRenderer::haveCachedResourcesForRenderPassId(int id) const
{
    CachedTexture* texture = m_renderPassTextures.get(id);
    return texture && texture->id() && texture->isComplete();
}

// static
IntSize CCDirectRenderer::renderPassTextureSize(const CCRenderPass* pass)
{
    return pass->outputRect().size();
}

// static
GC3Denum CCDirectRenderer::renderPassTextureFormat(const CCRenderPass*)
{
    return GraphicsContext3D::RGBA;
}

}
