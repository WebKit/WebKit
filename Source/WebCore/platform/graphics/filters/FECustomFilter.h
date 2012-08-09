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

#ifndef FECustomFilter_h
#define FECustomFilter_h

#if ENABLE(CSS_SHADERS) && USE(3D_GRAPHICS)

#include "CustomFilterOperation.h"
#include "Filter.h"
#include "FilterEffect.h"
#include "GraphicsTypes3D.h"
#include <wtf/RefPtr.h>

namespace JSC {
class Uint8ClampedArray;
}

namespace WebCore {

class CachedShader;
class CustomFilterGlobalContext;
class CustomFilterMesh;
class CustomFilterNumberParameter;
class CustomFilterProgram;
class CustomFilterCompiledProgram;
class DrawingBuffer;
class GraphicsContext3D;
class IntSize;
class Texture;

class FECustomFilter : public FilterEffect {
public:
    static PassRefPtr<FECustomFilter> create(Filter*, CustomFilterGlobalContext*, PassRefPtr<CustomFilterProgram>, const CustomFilterParameterList&,
                   unsigned meshRows, unsigned meshColumns, CustomFilterOperation::MeshBoxType, 
                   CustomFilterOperation::MeshType);

    virtual void platformApplySoftware();
    virtual void dump();

    virtual TextStream& externalRepresentation(TextStream&, int indention) const;

private:
    FECustomFilter(Filter*, CustomFilterGlobalContext*, PassRefPtr<CustomFilterProgram>, const CustomFilterParameterList&,
                   unsigned meshRows, unsigned meshColumns, CustomFilterOperation::MeshBoxType, 
                   CustomFilterOperation::MeshType);
    ~FECustomFilter();
    
    bool applyShader();
    void clearShaderResult();
    bool initializeContext();
    void deleteRenderBuffers();
    void resizeContext(const IntSize& newContextSize);
    void bindVertexAttribute(int attributeLocation, unsigned size, unsigned& offset);
    void bindProgramNumberParameters(int uniformLocation, CustomFilterNumberParameter*);
    void bindProgramParameters();
    void bindProgramAndBuffers(Uint8ClampedArray* srcPixelArray);
    
    // No need to keep a reference here. It is owned by the RenderView.
    CustomFilterGlobalContext* m_globalContext;
    
    RefPtr<GraphicsContext3D> m_context;
    RefPtr<Texture> m_inputTexture;
    RefPtr<CustomFilterCompiledProgram> m_compiledProgram;
    RefPtr<CustomFilterMesh> m_mesh;
    IntSize m_contextSize;

    Platform3DObject m_frameBuffer;
    Platform3DObject m_depthBuffer;
    Platform3DObject m_destTexture;

    RefPtr<CustomFilterProgram> m_program;
    CustomFilterParameterList m_parameters;

    unsigned m_meshRows;
    unsigned m_meshColumns;
    CustomFilterOperation::MeshType m_meshType;
};

} // namespace WebCore

#endif // ENABLE(CSS_SHADERS) && USE(3D_GRAPHICS)

#endif // FECustomFilter_h
