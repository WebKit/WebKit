//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DynamicHLSL.h: Interface for link and run-time HLSL generation
//

#ifndef LIBGLESV2_DYNAMIC_HLSL_H_
#define LIBGLESV2_DYNAMIC_HLSL_H_

#include "common/angleutils.h"

namespace rx
{
class Renderer;
}

namespace gl
{

class InfoLog;
class FragmentShader;
class VertexShader;
struct VariableLocation;
struct LinkedVarying;
class VertexAttribute;
struct VertexFormat;
struct ShaderVariable;
struct Attribute;

class DynamicHLSL
{
  public:
    explicit DynamicHLSL(rx::Renderer *const renderer);

    int packVaryings(InfoLog &infoLog, const ShaderVariable *packing[][4], FragmentShader *fragmentShader,
                     VertexShader *vertexShader, const std::vector<std::string>& transformFeedbackVaryings);
    std::string generateInputLayoutHLSL(const VertexFormat inputLayout[], const Attribute shaderAttributes[]) const;
    bool generateShaderLinkHLSL(InfoLog &infoLog, int registers, const ShaderVariable *packing[][4],
                                std::string& pixelHLSL, std::string& vertexHLSL,
                                FragmentShader *fragmentShader, VertexShader *vertexShader,
                                const std::vector<std::string>& transformFeedbackVaryings,
                                std::vector<LinkedVarying> *linkedVaryings,
                                std::map<int, VariableLocation> *programOutputVars) const;

    std::string generateGeometryShaderHLSL(int registers, const ShaderVariable *packing[][4], FragmentShader *fragmentShader, VertexShader *vertexShader) const;

    static const std::string VERTEX_ATTRIBUTE_STUB_STRING;

  private:
    DISALLOW_COPY_AND_ASSIGN(DynamicHLSL);

    rx::Renderer *const mRenderer;

    std::string generateVaryingHLSL(VertexShader *shader, const std::string &varyingSemantic,
                                    std::vector<LinkedVarying> *linkedVaryings) const;
    void defineOutputVariables(FragmentShader *fragmentShader, std::map<int, VariableLocation> *programOutputVars) const;
    std::string generatePointSpriteHLSL(int registers, const ShaderVariable *packing[][4], FragmentShader *fragmentShader, VertexShader *vertexShader) const;

    // Prepend an underscore
    static std::string decorateVariable(const std::string &name);

    std::string generateAttributeConversionHLSL(const VertexFormat &vertexFormat, const ShaderVariable &shaderAttrib) const;
};

// Utility method shared between ProgramBinary and DynamicHLSL
std::string ArrayString(unsigned int i);

}

#endif // LIBGLESV2_DYNAMIC_HLSL_H_
