/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(3D_CANVAS)

#include "ANGLEWebKitBridge.h"

namespace WebCore {


ANGLEWebKitBridge::ANGLEWebKitBridge() :
    builtCompilers(false)
{
    ShInitialize();
}

ANGLEWebKitBridge::~ANGLEWebKitBridge()
{
    if (builtCompilers) {
        ShDestruct(m_fragmentCompiler);
        ShDestruct(m_vertexCompiler);
    }
}

bool ANGLEWebKitBridge::validateShaderSource(const char* shaderSource, ANGLEShaderType shaderType, String& translatedShaderSource, String& shaderValidationLog)
{
    if (!builtCompilers) {
        m_fragmentCompiler = ShConstructCompiler(EShLangFragment, EShSpecWebGL, &m_resources);
        m_vertexCompiler = ShConstructCompiler(EShLangVertex, EShSpecWebGL, &m_resources);

        builtCompilers = true;
    }
    
    ShHandle compiler;

    if (shaderType == SHADER_TYPE_VERTEX)
        compiler = m_vertexCompiler;
    else
        compiler = m_fragmentCompiler;

    const char* const shaderSourceStrings[] = { shaderSource };

    bool validateSuccess = ShCompile(compiler, shaderSourceStrings, 1, EShOptNone, EDebugOpIntermediate);

    translatedShaderSource = ShGetObjectCode(compiler);
    shaderValidationLog = ShGetInfoLog(compiler);

    return validateSuccess;
}

}

#endif // ENABLE(3D_CANVAS)
