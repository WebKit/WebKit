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

#include "config.h"

#if ENABLE(GRAPHICS_CONTEXT_3D)

#include "ANGLEWebKitBridge.h"
#include "Logging.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

// FIXME: This is awful. Get rid of ANGLEWebKitBridge completely and call the libANGLE API directly to validate shaders.

static void appendSymbol(const sh::ShaderVariable& variable, ANGLEShaderSymbolType symbolType, Vector<std::pair<ANGLEShaderSymbolType, sh::ShaderVariable>>& symbols, const std::string& name, const std::string& mappedName)
{
    LOG(WebGL, "Map shader symbol %s -> %s\n", name.c_str(), mappedName.c_str());
    
    sh::ShaderVariable variableToAppend = variable;
    variableToAppend.name = name;
    variableToAppend.mappedName = mappedName;
    symbols.append(std::make_pair(symbolType, variableToAppend));
    
    if (variable.isArray()) {
        for (unsigned i = 0; i < std::max(1u, variable.arraySizes.back()); i++) {
            std::string arrayBrackets = "[" + std::to_string(i) + "]";
            std::string arrayName = name + arrayBrackets;
            std::string arrayMappedName = mappedName + arrayBrackets;
            LOG(WebGL, "Map shader symbol %s -> %s\n", arrayName.c_str(), arrayMappedName.c_str());
            variableToAppend.name = arrayName;
            variableToAppend.mappedName = arrayMappedName;
            symbols.append(std::make_pair(symbolType, variableToAppend));
        }
    }
}

static void getStructInfo(const sh::ShaderVariable& field, ANGLEShaderSymbolType symbolType, Vector<std::pair<ANGLEShaderSymbolType, sh::ShaderVariable>>& symbols, const std::string& namePrefix, const std::string& mappedNamePrefix)
{
    std::string name = namePrefix + '.' + field.name;
    std::string mappedName = mappedNamePrefix + '.' + field.mappedName;
    
    if (field.isStruct()) {
        for (const auto& subfield : field.fields) {
            // ANGLE restricts the depth of structs, which prevents stack overflow errors in this recursion.
            getStructInfo(subfield, symbolType, symbols, name, mappedName);
        }
    } else
        appendSymbol(field, symbolType, symbols, name, mappedName);
}

static void getSymbolInfo(const sh::ShaderVariable& variable, ANGLEShaderSymbolType symbolType, Vector<std::pair<ANGLEShaderSymbolType, sh::ShaderVariable>>& symbols)
{
    if (variable.isStruct()) {
        if (variable.isArray()) {
            for (unsigned i = 0; i < std::max(1u, variable.arraySizes.back()); i++) {
                std::string arrayBrackets = "[" + std::to_string(i) + "]";
                std::string arrayName = variable.name + arrayBrackets;
                std::string arrayMappedName = variable.mappedName + arrayBrackets;
                for (const auto& field : variable.fields)
                    getStructInfo(field, symbolType, symbols, arrayName, arrayMappedName);
            }
        } else {
            for (const auto& field : variable.fields)
                getStructInfo(field, symbolType, symbols, variable.name, variable.mappedName);
        }
    } else
        appendSymbol(variable, symbolType, symbols, variable.name, variable.mappedName);
}

static bool getSymbolInfo(ShHandle compiler, ANGLEShaderSymbolType symbolType, Vector<std::pair<ANGLEShaderSymbolType, sh::ShaderVariable>>& symbols)
{
    switch (symbolType) {
    case SHADER_SYMBOL_TYPE_UNIFORM: {
        auto uniforms = sh::GetUniforms(compiler);
        if (!uniforms)
            return false;
        for (const auto& uniform : *uniforms)
            getSymbolInfo(uniform, symbolType, symbols);
        break;
    }
    case SHADER_SYMBOL_TYPE_VARYING: {
        auto varyings = sh::GetVaryings(compiler);
        if (!varyings)
            return false;
        for (const auto& varying : *varyings)
            getSymbolInfo(varying, symbolType, symbols);
        break;
    }
    case SHADER_SYMBOL_TYPE_ATTRIBUTE: {
        auto attributes = sh::GetAttributes(compiler);
        if (!attributes)
            return false;
        for (const auto& attribute : *attributes)
            getSymbolInfo(attribute, symbolType, symbols);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}

ANGLEWebKitBridge::ANGLEWebKitBridge(ShShaderOutput shaderOutput, ShShaderSpec shaderSpec)
    : builtCompilers(false)
    , m_fragmentCompiler(0)
    , m_vertexCompiler(0)
    , m_shaderOutput(shaderOutput)
    , m_shaderSpec(shaderSpec)
{
    // This is a no-op if it's already initialized.
    sh::Initialize();
}

ANGLEWebKitBridge::~ANGLEWebKitBridge()
{
    cleanupCompilers();
}

void ANGLEWebKitBridge::cleanupCompilers()
{
    if (m_fragmentCompiler)
        sh::Destruct(m_fragmentCompiler);
    m_fragmentCompiler = nullptr;
    if (m_vertexCompiler)
        sh::Destruct(m_vertexCompiler);
    m_vertexCompiler = nullptr;

    builtCompilers = false;
}
    
void ANGLEWebKitBridge::setResources(const ShBuiltInResources& resources)
{
    // Resources are (possibly) changing - cleanup compilers if we had them already
    cleanupCompilers();
    
    m_resources = resources;
}

bool ANGLEWebKitBridge::compileShaderSource(const char* shaderSource, ANGLEShaderType shaderType, String& translatedShaderSource, String& shaderValidationLog, Vector<std::pair<ANGLEShaderSymbolType, sh::ShaderVariable>>& symbols, uint64_t extraCompileOptions)
{
    if (!builtCompilers) {
        m_fragmentCompiler = sh::ConstructCompiler(GL_FRAGMENT_SHADER, m_shaderSpec, m_shaderOutput, &m_resources);
        m_vertexCompiler = sh::ConstructCompiler(GL_VERTEX_SHADER, m_shaderSpec, m_shaderOutput, &m_resources);
        if (!m_fragmentCompiler || !m_vertexCompiler) {
            cleanupCompilers();
            return false;
        }

        builtCompilers = true;
    }
    
    ShHandle compiler;

    if (shaderType == SHADER_TYPE_VERTEX)
        compiler = m_vertexCompiler;
    else
        compiler = m_fragmentCompiler;

    const char* const shaderSourceStrings[] = { shaderSource };

    bool validateSuccess = sh::Compile(compiler, shaderSourceStrings, 1, SH_OBJECT_CODE | SH_VARIABLES | extraCompileOptions);
    if (!validateSuccess) {
        const std::string& log = sh::GetInfoLog(compiler);
        if (log.length())
            shaderValidationLog = log.c_str();
        return false;
    }

    const std::string& objectCode = sh::GetObjectCode(compiler);
    if (objectCode.length())
        translatedShaderSource = objectCode.c_str();
    
    if (!getSymbolInfo(compiler, SHADER_SYMBOL_TYPE_ATTRIBUTE, symbols))
        return false;
    if (!getSymbolInfo(compiler, SHADER_SYMBOL_TYPE_UNIFORM, symbols))
        return false;
    if (!getSymbolInfo(compiler, SHADER_SYMBOL_TYPE_VARYING, symbols))
        return false;

    return true;
}

}

#endif // ENABLE(GRAPHICS_CONTEXT_3D)
