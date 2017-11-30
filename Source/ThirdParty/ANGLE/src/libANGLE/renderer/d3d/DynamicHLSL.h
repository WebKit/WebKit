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
#include "libANGLE/angletypes.h"
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
class VaryingPacking;
struct VertexAttribute;
}

namespace rx
{
class ProgramD3DMetadata;
class ShaderD3D;

struct PixelShaderOutputVariable
{
    PixelShaderOutputVariable() {}
    PixelShaderOutputVariable(GLenum typeIn,
                              const std::string &nameIn,
                              const std::string &sourceIn,
                              size_t outputIndexIn)
        : type(typeIn), name(nameIn), source(sourceIn), outputIndex(outputIndexIn)
    {
    }

    GLenum type = GL_NONE;
    std::string name;
    std::string source;
    size_t outputIndex = 0;
};

struct BuiltinVarying final : private angle::NonCopyable
{
    BuiltinVarying();

    std::string str() const;
    void enableSystem(const std::string &systemValueSemantic);
    void enable(const std::string &semanticVal, unsigned int indexVal);

    bool enabled;
    std::string semantic;
    unsigned int index;
    bool systemValue;
};

struct BuiltinInfo
{
    BuiltinInfo();
    ~BuiltinInfo();

    BuiltinVarying dxPosition;
    BuiltinVarying glPosition;
    BuiltinVarying glFragCoord;
    BuiltinVarying glPointCoord;
    BuiltinVarying glPointSize;
    BuiltinVarying glViewIDOVR;
    BuiltinVarying glViewportIndex;
    BuiltinVarying glLayer;
};

inline std::string GetVaryingSemantic(int majorShaderModel, bool programUsesPointSize)
{
    // SM3 reserves the TEXCOORD semantic for point sprite texcoords (gl_PointCoord)
    // In D3D11 we manually compute gl_PointCoord in the GS.
    return ((programUsesPointSize && majorShaderModel < 4) ? "COLOR" : "TEXCOORD");
}

class BuiltinVaryingsD3D
{
  public:
    BuiltinVaryingsD3D(const ProgramD3DMetadata &metadata, const gl::VaryingPacking &packing);
    ~BuiltinVaryingsD3D();

    bool usesPointSize() const { return mBuiltinInfo[gl::SHADER_VERTEX].glPointSize.enabled; }

    const BuiltinInfo &operator[](gl::ShaderType shaderType) const
    {
        return mBuiltinInfo[shaderType];
    }
    BuiltinInfo &operator[](gl::ShaderType shaderType) { return mBuiltinInfo[shaderType]; }

  private:
    void updateBuiltins(gl::ShaderType shaderType,
                        const ProgramD3DMetadata &metadata,
                        const gl::VaryingPacking &packing);

    std::array<BuiltinInfo, gl::SHADER_TYPE_MAX> mBuiltinInfo;
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
    void generateShaderLinkHLSL(const gl::Context *context,
                                const gl::ProgramState &programData,
                                const ProgramD3DMetadata &programMetadata,
                                const gl::VaryingPacking &varyingPacking,
                                const BuiltinVaryingsD3D &builtinsD3D,
                                std::string *pixelHLSL,
                                std::string *vertexHLSL) const;
    std::string generateComputeShaderLinkHLSL(const gl::Context *context,
                                              const gl::ProgramState &programData) const;

    std::string generateGeometryShaderPreamble(const gl::VaryingPacking &varyingPacking,
                                               const BuiltinVaryingsD3D &builtinsD3D,
                                               const bool hasANGLEMultiviewEnabled,
                                               const bool selectViewInVS) const;

    std::string generateGeometryShaderHLSL(const gl::Context *context,
                                           gl::PrimitiveType primitiveType,
                                           const gl::ProgramState &programData,
                                           const bool useViewScale,
                                           const bool hasANGLEMultiviewEnabled,
                                           const bool selectViewInVS,
                                           const bool pointSpriteEmulation,
                                           const std::string &preambleString) const;

    void getPixelShaderOutputKey(const gl::ContextState &data,
                                 const gl::ProgramState &programData,
                                 const ProgramD3DMetadata &metadata,
                                 std::vector<PixelShaderOutputVariable> *outPixelShaderKey);

  private:
    RendererD3D *const mRenderer;

    void generateVaryingLinkHLSL(const gl::VaryingPacking &varyingPacking,
                                 const BuiltinInfo &builtins,
                                 bool programUsesPointSize,
                                 std::ostringstream &hlslStream) const;

    static void GenerateAttributeConversionHLSL(gl::VertexFormatType vertexFormatType,
                                                const sh::ShaderVariable &shaderAttrib,
                                                std::ostringstream &outStream);
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_DYNAMICHLSL_H_
