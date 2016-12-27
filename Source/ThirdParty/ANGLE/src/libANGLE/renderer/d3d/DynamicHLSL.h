//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DynamicHLSL.h: Interface for link and run-time HLSL generation
//

#ifndef LIBANGLE_RENDERER_D3D_DYNAMICHLSL_H_
#define LIBANGLE_RENDERER_D3D_DYNAMICHLSL_H_

#include <map>
#include <vector>

#include "angle_gl.h"
#include "common/angleutils.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Program.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"

namespace sh
{
struct Attribute;
struct ShaderVariable;
}

namespace gl
{
class InfoLog;
struct VariableLocation;
struct VertexAttribute;
}

namespace rx
{
struct PackedVarying;
class ProgramD3DMetadata;
class ShaderD3D;
class VaryingPacking;

struct PixelShaderOutputVariable
{
    GLenum type;
    std::string name;
    std::string source;
    size_t outputIndex;
};

class DynamicHLSL : angle::NonCopyable
{
  public:
    explicit DynamicHLSL(RendererD3D *const renderer);

    std::string generateVertexShaderForInputLayout(
        const std::string &sourceShader,
        const gl::InputLayout &inputLayout,
        const std::vector<sh::Attribute> &shaderAttributes) const;
    std::string generatePixelShaderForOutputSignature(
        const std::string &sourceShader,
        const std::vector<PixelShaderOutputVariable> &outputVariables,
        bool usesFragDepth,
        const std::vector<GLenum> &outputLayout) const;
    bool generateShaderLinkHLSL(const gl::ContextState &data,
                                const gl::ProgramState &programData,
                                const ProgramD3DMetadata &programMetadata,
                                const VaryingPacking &varyingPacking,
                                std::string *pixelHLSL,
                                std::string *vertexHLSL) const;

    std::string generateGeometryShaderPreamble(const VaryingPacking &varyingPacking) const;

    std::string generateGeometryShaderHLSL(gl::PrimitiveType primitiveType,
                                           const gl::ContextState &data,
                                           const gl::ProgramState &programData,
                                           const bool useViewScale,
                                           const std::string &preambleString) const;

    void getPixelShaderOutputKey(const gl::ContextState &data,
                                 const gl::ProgramState &programData,
                                 const ProgramD3DMetadata &metadata,
                                 std::vector<PixelShaderOutputVariable> *outPixelShaderKey);

  private:
    RendererD3D *const mRenderer;

    void generateVaryingLinkHLSL(ShaderType shaderType,
                                 const VaryingPacking &varyingPacking,
                                 std::stringstream &linkStream) const;
    void generateVaryingHLSL(const VaryingPacking &varyingPacking,
                             std::stringstream &hlslStream) const;

    // Prepend an underscore
    static std::string decorateVariable(const std::string &name);

    std::string generateAttributeConversionHLSL(gl::VertexFormatType vertexFormatType,
                                                const sh::ShaderVariable &shaderAttrib) const;
};

std::string GetVaryingSemantic(int majorShaderModel, bool programUsesPointSize);
}

#endif  // LIBANGLE_RENDERER_D3D_DYNAMICHLSL_H_
