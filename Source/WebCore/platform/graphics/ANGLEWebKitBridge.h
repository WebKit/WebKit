/*
 * Copyright (C) 2010, 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#if ENABLE(WEBGL)

#if USE(LIBEPOXY)
// libepoxy headers have to be included before <ANGLE/ShaderLang.h> in order to avoid
// picking up khrplatform.h inclusion that's done in ANGLE.
#include <epoxy/gl.h>
#endif

#include <ANGLE/ShaderLang.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)

#if USE(OPENGL_ES)
#import <OpenGLES/ES2/glext.h>
#elif USE(OPENGL)
#include <OpenGL/gl.h>
#elif USE(ANGLE)
#include <ANGLE/gl2.h>
#else
#error Unsupported configuration
#endif

#elif PLATFORM(WIN)
#include "OpenGLESShims.h"

#elif USE(LIBEPOXY)
// <epoxy/gl.h> already included above.

#elif USE(OPENGL_ES)
#include <GLES2/gl2.h>

#else
#include "OpenGLShims.h"
#endif

// FIXME
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
//

namespace WebCore {

enum ANGLEShaderType {
    SHADER_TYPE_VERTEX = GL_VERTEX_SHADER,
    SHADER_TYPE_FRAGMENT = GL_FRAGMENT_SHADER,
};

enum ANGLEShaderSymbolType {
    SHADER_SYMBOL_TYPE_ATTRIBUTE,
    SHADER_SYMBOL_TYPE_UNIFORM,
    SHADER_SYMBOL_TYPE_VARYING
};

class ANGLEWebKitBridge {
public:

    ANGLEWebKitBridge(ShShaderOutput = SH_GLSL_COMPATIBILITY_OUTPUT, ShShaderSpec = SH_WEBGL_SPEC);
    ~ANGLEWebKitBridge();
    
    const ShBuiltInResources& getResources() { return m_resources; }
    void setResources(const ShBuiltInResources&);
    
    bool compileShaderSource(const char* shaderSource, ANGLEShaderType, String& translatedShaderSource, String& shaderValidationLog, Vector<std::pair<ANGLEShaderSymbolType, sh::ShaderVariable>>& symbols, uint64_t extraCompileOptions = 0);

private:

    void cleanupCompilers();

    bool builtCompilers;
    
    ShHandle m_fragmentCompiler;
    ShHandle m_vertexCompiler;

    ShShaderOutput m_shaderOutput;
    ShShaderSpec m_shaderSpec;

    ShBuiltInResources m_resources;
};

} // namespace WebCore

#endif
