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

#ifndef FECustomFilter_h
#define FECustomFilter_h

#if ENABLE(CSS_SHADERS) && ENABLE(WEBGL)

#include "CustomFilterOperation.h"
#include "Filter.h"
#include "FilterEffect.h"
#include <wtf/RefPtr.h>

namespace JSC {
class ByteArray;
}

namespace WebCore {

class CachedShader;
class CustomFilterMesh;
class CustomFilterShader;
class Document;
class DrawingBuffer;
class GraphicsContext3D;
class IntSize;
class Texture;

class FECustomFilter : public FilterEffect {
public:
    static PassRefPtr<FECustomFilter> create(Filter*, Document*, const String& vertexShader, const String& fragmentShader,
                   unsigned meshRows, unsigned meshColumns, CustomFilterOperation::MeshBoxType, 
                   CustomFilterOperation::MeshType);

    virtual void platformApplySoftware();
    virtual void dump();

    virtual TextStream& externalRepresentation(TextStream&, int indention) const;

private:
    FECustomFilter(Filter*, Document*, const String& vertexShader, const String& fragmentShader,
                   unsigned meshRows, unsigned meshColumns, CustomFilterOperation::MeshBoxType, 
                   CustomFilterOperation::MeshType);
    
    void initializeContext(const IntSize& contextSize);
    void resizeContext(const IntSize& newContextSize);
    void bindVertexAttribute(int attributeLocation, unsigned size, unsigned& offset);
    void bindProgramAndBuffers(ByteArray* srcPixelArray);
    
    Document* m_document;
    
    RefPtr<GraphicsContext3D> m_context;
    RefPtr<DrawingBuffer> m_drawingBuffer;
    RefPtr<Texture> m_inputTexture;
    RefPtr<CustomFilterShader> m_shader;
    RefPtr<CustomFilterMesh> m_mesh;
    IntSize m_contextSize;
    
    String m_vertexShader;
    String m_fragmentShader;

    unsigned m_meshRows;
    unsigned m_meshColumns;
    CustomFilterOperation::MeshBoxType m_meshBoxType;
    CustomFilterOperation::MeshType m_meshType;
};

} // namespace WebCore

#endif // ENABLE(CSS_SHADERS) && ENABLE(WEBGL)

#endif // FECustomFilter_h
