/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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

#ifndef CustomFilterCompiledProgram_h
#define CustomFilterCompiledProgram_h

#if ENABLE(CSS_SHADERS) && USE(3D_GRAPHICS)

#include "CustomFilterProgramInfo.h"
#include "GraphicsTypes3D.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CustomFilterGlobalContext;
class GraphicsContext3D;

// A specific combination of vertex / fragment shader is only going to be compiled once. The CustomFilterGlobalContext is 
// caching the compiled programs. CustomFilterGlobalContext has a weak reference to the CustomFilterCompiledProgram, so the 
// CustomFilterCompiledProgram destructor needs to notify the CustomFilterGlobalContext to remove the program from the cache.
// FECustomFilter is the reference owner of the CustomFilterCompiledProgram, so a compiled shader is only kept alive as 
// long as there is at least one visible layer that applies the shader.
class CustomFilterCompiledProgram: public RefCounted<CustomFilterCompiledProgram> {
public:
    static PassRefPtr<CustomFilterCompiledProgram> create(CustomFilterGlobalContext* globalContext, const CustomFilterProgramInfo& programInfo)
    {
        return adoptRef(new CustomFilterCompiledProgram(globalContext, programInfo));
    }
    
    ~CustomFilterCompiledProgram();

    const CustomFilterProgramInfo& programInfo() const { return m_programInfo; }
    
    int positionAttribLocation() const { return m_positionAttribLocation; }
    int texAttribLocation() const { return m_texAttribLocation; }
    int meshAttribLocation() const { return m_meshAttribLocation; }
    int triangleAttribLocation() const { return m_triangleAttribLocation; }
    int meshBoxLocation() const { return m_meshBoxLocation; }
    int projectionMatrixLocation() const { return m_projectionMatrixLocation; }
    int tileSizeLocation() const { return m_tileSizeLocation; }
    int meshSizeLocation() const { return m_meshSizeLocation; }
    int samplerLocation() const { return m_samplerLocation; }
    int contentSamplerLocation() const { return m_contentSamplerLocation; }
    int samplerSizeLocation() const { return m_samplerSizeLocation; }

    int uniformLocationByName(const String&);
    
    bool isInitialized() const { return m_isInitialized; }
    
    Platform3DObject program() const { return m_program; }

    // 'detachGlobalContext' is called when the CustomFilterGlobalContext is deleted
    // and there's no need for the callback anymore. 
    // Note that CustomFilterGlobalContext doesn't not keep a strong reference to 
    // the CustomFilterCompiledProgram.
    void detachFromGlobalContext() { m_globalContext = 0; }
private:
    CustomFilterCompiledProgram(CustomFilterGlobalContext*, const CustomFilterProgramInfo&);
    
    Platform3DObject compileShader(GC3Denum shaderType, const String& shaderString);
    Platform3DObject linkProgram(Platform3DObject vertexShader, Platform3DObject fragmentShader);
    void initializeParameterLocations();
    
    static String defaultVertexShaderString();
    static String defaultFragmentShaderString();
    String getDefaultShaderString(GC3Denum shaderType);
    
    CustomFilterGlobalContext* m_globalContext;
    RefPtr<GraphicsContext3D> m_context;
    CustomFilterProgramInfo m_programInfo;
    Platform3DObject m_program;
    
    int m_positionAttribLocation;
    int m_texAttribLocation;
    int m_meshAttribLocation;
    int m_triangleAttribLocation;
    int m_meshBoxLocation;
    int m_projectionMatrixLocation;
    int m_tileSizeLocation;
    int m_meshSizeLocation;
    int m_samplerLocation;
    int m_samplerSizeLocation;
    int m_contentSamplerLocation;
    
    bool m_isInitialized;
};

}

#endif // ENABLE(CSS_SHADERS) && USE(3D_GRAPHICS)

#endif
