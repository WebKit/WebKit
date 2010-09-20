/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "SharedGraphicsContext3D.h"

#include "AffineTransform.h"
#include "Color.h"
#include "FloatRect.h"
#include "GraphicsContext3D.h"
#include "GraphicsTypes.h"
#include "IntSize.h"
#include "SolidFillShader.h"
#include "TexShader.h"

#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// static
PassRefPtr<SharedGraphicsContext3D> SharedGraphicsContext3D::create(PassOwnPtr<GraphicsContext3D> context)
{
    return adoptRef(new SharedGraphicsContext3D(context));
}

SharedGraphicsContext3D::SharedGraphicsContext3D(PassOwnPtr<GraphicsContext3D> context)
    : m_context(context)
    , m_quadVertices(0)
    , m_solidFillShader(SolidFillShader::create(m_context.get()))
    , m_texShader(TexShader::create(m_context.get()))
{
    allContexts()->add(this);
}

SharedGraphicsContext3D::~SharedGraphicsContext3D()
{
    m_context->deleteBuffer(m_quadVertices);
    allContexts()->remove(this);
}

void SharedGraphicsContext3D::makeContextCurrent()
{
    m_context->makeContextCurrent();
}

void SharedGraphicsContext3D::scissor(const FloatRect& rect)
{
    m_context->scissor(rect.x(), rect.y(), rect.width(), rect.height());
}

void SharedGraphicsContext3D::enable(unsigned capacity)
{
    m_context->enable(capacity);
}

void SharedGraphicsContext3D::disable(unsigned capacity)
{
    m_context->disable(capacity);
}

void SharedGraphicsContext3D::clearColor(const Color& color)
{
    float rgba[4];
    color.getRGBA(rgba[0], rgba[1], rgba[2], rgba[3]);
    m_context->clearColor(rgba[0], rgba[1], rgba[2], rgba[3]);
}

void SharedGraphicsContext3D::clear(unsigned mask)
{
    m_context->clear(mask);
}

void SharedGraphicsContext3D::drawArrays(unsigned long mode, long first, long count)
{
    m_context->drawArrays(mode, first, count);
}

unsigned long SharedGraphicsContext3D::getError()
{
    return m_context->getError();
}

void SharedGraphicsContext3D::getIntegerv(unsigned long pname, int* value)
{
    m_context->getIntegerv(pname, value);
}

void SharedGraphicsContext3D::flush()
{
    m_context->flush();
}

unsigned SharedGraphicsContext3D::createFramebuffer()
{
    return m_context->createFramebuffer();
}

unsigned SharedGraphicsContext3D::createTexture()
{
    return m_context->createTexture();
}

void SharedGraphicsContext3D::deleteFramebuffer(unsigned framebuffer)
{
    m_context->deleteFramebuffer(framebuffer);
}

void SharedGraphicsContext3D::deleteTexture(unsigned texture)
{
    m_context->deleteTexture(texture);
}

void SharedGraphicsContext3D::framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, unsigned texture, long level)
{
    m_context->framebufferTexture2D(target, attachment, textarget, texture, level);
}

void SharedGraphicsContext3D::texParameteri(unsigned target, unsigned pname, int param)
{
    m_context->texParameteri(target, pname, param);
}

int SharedGraphicsContext3D::texImage2D(unsigned target, unsigned level, unsigned internalformat, unsigned width, unsigned height, unsigned border, unsigned format, unsigned type, void* pixels)
{
    return m_context->texImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

int SharedGraphicsContext3D::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset, unsigned width, unsigned height, unsigned format, unsigned type, void* pixels)
{
    return m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void SharedGraphicsContext3D::readPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type, void* data)
{
    m_context->readPixels(x, y, width, height, format, type, data);
}

bool SharedGraphicsContext3D::supportsBGRA()
{
    return m_context->supportsBGRA();
}

bool SharedGraphicsContext3D::supportsCopyTextureToParentTextureCHROMIUM()

{
    return m_context->supportsCopyTextureToParentTextureCHROMIUM();
}

void SharedGraphicsContext3D::copyTextureToParentTextureCHROMIUM(unsigned texture, unsigned parentTexture)
{
    return m_context->copyTextureToParentTextureCHROMIUM(texture, parentTexture);
}

Texture* SharedGraphicsContext3D::createTexture(NativeImagePtr ptr, Texture::Format format, int width, int height)
{
    RefPtr<Texture> texture = m_textures.get(ptr);
    if (texture)
        return texture.get();

    texture = Texture::create(m_context.get(), format, width, height);
    Texture* t = texture.get();
    m_textures.set(ptr, texture);
    return t;
}

Texture* SharedGraphicsContext3D::getTexture(NativeImagePtr ptr)
{
    RefPtr<Texture> texture = m_textures.get(ptr);
    return texture ? texture.get() : 0;
}

void SharedGraphicsContext3D::removeTextureFor(NativeImagePtr ptr)
{
    TextureHashMap::iterator it = m_textures.find(ptr);
    if (it != m_textures.end())
        m_textures.remove(it);
}

// static
void SharedGraphicsContext3D::removeTexturesFor(NativeImagePtr ptr)
{
    for (HashSet<SharedGraphicsContext3D*>::iterator it = allContexts()->begin(); it != allContexts()->end(); ++it)
        (*it)->removeTextureFor(ptr);
}

// static
HashSet<SharedGraphicsContext3D*>* SharedGraphicsContext3D::allContexts()
{
    static OwnPtr<HashSet<SharedGraphicsContext3D*> > set(new HashSet<SharedGraphicsContext3D*>);
    return set.get();
}


PassRefPtr<Texture> SharedGraphicsContext3D::createTexture(Texture::Format format, int width, int height)
{
    return Texture::create(m_context.get(), format, width, height);
}

void SharedGraphicsContext3D::applyCompositeOperator(CompositeOperator op)
{
    switch (op) {
    case CompositeClear:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ZERO, GraphicsContext3D::ZERO);
        break;
    case CompositeCopy:
        m_context->disable(GraphicsContext3D::BLEND);
        break;
    case CompositeSourceOver:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE, GraphicsContext3D::ONE_MINUS_SRC_ALPHA);
        break;
    case CompositeSourceIn:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::DST_ALPHA, GraphicsContext3D::ZERO);
        break;
    case CompositeSourceOut:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE_MINUS_DST_ALPHA, GraphicsContext3D::ZERO);
        break;
    case CompositeSourceAtop:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::DST_ALPHA, GraphicsContext3D::ONE_MINUS_SRC_ALPHA);
        break;
    case CompositeDestinationOver:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE_MINUS_DST_ALPHA, GraphicsContext3D::ONE);
        break;
    case CompositeDestinationIn:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ZERO, GraphicsContext3D::SRC_ALPHA);
        break;
    case CompositeDestinationOut:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ZERO, GraphicsContext3D::ONE_MINUS_SRC_ALPHA);
        break;
    case CompositeDestinationAtop:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE_MINUS_DST_ALPHA, GraphicsContext3D::SRC_ALPHA);
        break;
    case CompositeXOR:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE_MINUS_DST_ALPHA, GraphicsContext3D::ONE_MINUS_SRC_ALPHA);
        break;
    case CompositePlusDarker:
    case CompositeHighlight:
        // unsupported
        m_context->disable(GraphicsContext3D::BLEND);
        break;
    case CompositePlusLighter:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE, GraphicsContext3D::ONE);
        break;
    }
}

void SharedGraphicsContext3D::useQuadVertices()
{
    if (!m_quadVertices) {
        float vertices[] = { 0.0f, 0.0f, 1.0f,
                             1.0f, 0.0f, 1.0f,
                             0.0f, 1.0f, 1.0f,
                             1.0f, 1.0f, 1.0f };
        m_quadVertices = m_context->createBuffer();
        m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_quadVertices);
        m_context->bufferData(GraphicsContext3D::ARRAY_BUFFER, sizeof(vertices), vertices, GraphicsContext3D::STATIC_DRAW);
    } else {
        m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_quadVertices);
    }
}

void SharedGraphicsContext3D::setActiveTexture(unsigned textureUnit)
{
    m_context->activeTexture(textureUnit);
}

void SharedGraphicsContext3D::bindTexture(unsigned target, unsigned texture)
{
    m_context->bindTexture(target, texture);
}

void SharedGraphicsContext3D::useFillSolidProgram(const AffineTransform& transform, const Color& color)
{
    m_solidFillShader->use(transform, color);
}

void SharedGraphicsContext3D::useTextureProgram(const AffineTransform& transform, const AffineTransform& texTransform, float alpha)
{
    m_texShader->use(transform, texTransform, 0, alpha);
}

void SharedGraphicsContext3D::bindFramebuffer(unsigned framebuffer)
{
    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, framebuffer);
}

void SharedGraphicsContext3D::setViewport(const IntSize& size)
{
    m_context->viewport(0, 0, size.width(), size.height());
}

bool SharedGraphicsContext3D::paintsIntoCanvasBuffer() const
{
    return m_context->paintsIntoCanvasBuffer();
}

} // namespace WebCore
