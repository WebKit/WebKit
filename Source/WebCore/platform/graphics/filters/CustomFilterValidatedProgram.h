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

#ifndef CustomFilterValidatedProgram_h
#define CustomFilterValidatedProgram_h

#if ENABLE(CSS_SHADERS)

#include "CustomFilterCompiledProgram.h"
#include "CustomFilterProgramInfo.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

// PlatformCompiledProgram defines a type that is compatible with the framework used to implement accelerated compositing on a particular platform.
#if PLATFORM(BLACKBERRY)
namespace WebCore {
class LayerCompiledProgram;
}
typedef WebCore::LayerCompiledProgram PlatformCompiledProgram;
#endif

namespace WebCore {

class ANGLEWebKitBridge;
class CustomFilterCompiledProgram;
class CustomFilterGlobalContext;

//
// A unique combination of vertex shader and fragment shader is only validated and compiled once.
// All shaders are validated through ANGLE in CustomFilterValidatedProgram before being compiled by the GraphicsContext3D in CustomFilterCompiledProgram.
// For shaders that use the CSS mix function, CustomFilterValidatedProgram adds shader code to perform DOM texture access, blending, and compositing.
//
// The CustomFilterGlobalContext caches the validated programs.
// CustomFilterValidatedProgram owns a CustomFilterCompiledProgram if validation and compilation succeeds.
// Thus, compiled programs are cached via their validated program owners.
//
// CustomFilterGlobalContext has a weak reference to the CustomFilterValidatedProgram.
// Thus, the CustomFilterValidatedProgram destructor needs to notify the CustomFilterGlobalContext to remove the program from the cache.
// FECustomFilter is the reference owner of the CustomFilterValidatedProgram.
// Thus, a validated and compiled shader is only kept alive as long as there is at least one visible layer that applies the shader.
//
class CustomFilterValidatedProgram : public RefCounted<CustomFilterValidatedProgram> {
public:
    static PassRefPtr<CustomFilterValidatedProgram> create(CustomFilterGlobalContext* globalContext, const CustomFilterProgramInfo& programInfo)
    {
        return adoptRef(new CustomFilterValidatedProgram(globalContext, programInfo));
    }

    ~CustomFilterValidatedProgram();

    const CustomFilterProgramInfo& programInfo() const { return m_programInfo; }
    PassRefPtr<CustomFilterCompiledProgram> compiledProgram();

#if PLATFORM(BLACKBERRY)
    PlatformCompiledProgram* platformCompiledProgram();
#endif

    bool isInitialized() const { return m_isInitialized; }

    // 'detachFromGlobalContext' is called when the CustomFilterGlobalContext is deleted, and there's no need for the callback anymore. 
    // Note that CustomFilterGlobalContext does not keep a strong reference to the CustomFilterValidatedProgram.
    void detachFromGlobalContext() { m_globalContext = 0; }
private:
    CustomFilterValidatedProgram(CustomFilterGlobalContext*, const CustomFilterProgramInfo&);

    void platformInit();
    void platformDestroy();

    static String defaultVertexShaderString();
    static String defaultFragmentShaderString();

    static String blendFunctionString(BlendMode);
    static String compositeFunctionString(CompositeOperator);

    void rewriteMixVertexShader();
    void rewriteMixFragmentShader();

    CustomFilterGlobalContext* m_globalContext;
    CustomFilterProgramInfo m_programInfo;

    String m_validatedVertexShader;
    String m_validatedFragmentShader;

    RefPtr<CustomFilterCompiledProgram> m_compiledProgram;
#if PLATFORM(BLACKBERRY)
    PlatformCompiledProgram* m_platformCompiledProgram;
#endif

    bool m_isInitialized;
};

}

#endif // ENABLE(CSS_SHADERS)

#endif
