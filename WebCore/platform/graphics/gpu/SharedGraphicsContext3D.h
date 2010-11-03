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

#ifndef SharedGraphicsContext3D_h
#define SharedGraphicsContext3D_h

#include "GraphicsTypes.h"
#include "ImageSource.h"
#include "Texture.h"

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class AffineTransform;
class Color;
class GraphicsContext3D;
class FloatRect;
class HostWindow;
class IntSize;
class SolidFillShader;
class TexShader;

typedef HashMap<NativeImagePtr, RefPtr<Texture> > TextureHashMap;

class SharedGraphicsContext3D : public RefCounted<SharedGraphicsContext3D> {
public:
    static PassRefPtr<SharedGraphicsContext3D> create(HostWindow*);
    ~SharedGraphicsContext3D();

    // Functions that delegate directly to GraphicsContext3D, with caching
    void makeContextCurrent();
    void bindFramebuffer(unsigned framebuffer);
    void setViewport(const IntSize&);
    void scissor(const FloatRect&);
    void enable(unsigned capacity);
    void disable(unsigned capacity);
    void clearColor(const Color&);
    void clear(unsigned mask);
    void drawArrays(unsigned long mode, long first, long count);
    unsigned long getError();
    void getIntegerv(unsigned long pname, int* value);
    void flush();

    unsigned createFramebuffer();
    unsigned createTexture();

    void deleteFramebuffer(unsigned framebuffer);
    void deleteTexture(unsigned texture);

    void framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, unsigned, long level);
    void texParameteri(unsigned target, unsigned pname, int param);
    int texImage2D(unsigned target, unsigned level, unsigned internalformat, unsigned width, unsigned height, unsigned border, unsigned format, unsigned type, void* pixels);
    int texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset, unsigned width, unsigned height, unsigned format, unsigned type, void* pixels);

    void readPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type, void* data);

    bool paintsIntoCanvasBuffer() const;

    // Shared logic for canvas 2d
    void applyCompositeOperator(CompositeOperator);
    void useQuadVertices();

    void useFillSolidProgram(const AffineTransform&, const Color&);
    void useTextureProgram(const AffineTransform&, const AffineTransform&, float alpha);

    void setActiveTexture(unsigned textureUnit);
    void bindTexture(unsigned target, unsigned texture);

    bool supportsBGRA();

    // Creates a texture associated with the given image.  Is owned by this context's
    // TextureHashMap.
    Texture* createTexture(NativeImagePtr, Texture::Format, int width, int height);
    Texture* getTexture(NativeImagePtr);

    // Multiple SharedGraphicsContext3D can exist in a single process (one per compositing context) and the same
    // NativeImagePtr may be uploaded as a texture into all of them.  This function removes uploaded textures
    // for a given NativeImagePtr in all contexts.
    static void removeTexturesFor(NativeImagePtr);

    // Creates a texture that is not associated with any image.  The caller takes ownership of
    // the texture.
    PassRefPtr<Texture> createTexture(Texture::Format, int width, int height);

    GraphicsContext3D* graphicsContext3D() const { return m_context.get(); }

private:
    SharedGraphicsContext3D(PassRefPtr<GraphicsContext3D>, PassOwnPtr<SolidFillShader>, PassOwnPtr<TexShader>);

    // Used to implement removeTexturesFor(), see the comment above.
    static HashSet<SharedGraphicsContext3D*>* allContexts();
    void removeTextureFor(NativeImagePtr);

    RefPtr<GraphicsContext3D> m_context;

    unsigned m_quadVertices;

    OwnPtr<SolidFillShader> m_solidFillShader;
    OwnPtr<TexShader> m_texShader;

    TextureHashMap m_textures;
};

} // namespace WebCore

#endif // SharedGraphicsContext3D_h
