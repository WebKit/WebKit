//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
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
#include "libANGLE/renderer/d3d/DynamicImage2DHLSL.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"

namespace sh
{
struct ShaderVariable;
}  // namespace sh

namespace gl
{
class InfoLog;
struct VariableLocation;
class VaryingPacking;
struct VertexAttribute;
}  // namespace gl

namespace rx
{
class ProgramD3DMetadata;
class ShaderD3D;

// This class needs to match OutputHLSL::decorate
class DecorateVariable final : angle::NonCopyable
{
  public:
    explicit DecorateVariable(const std::string &str) : mName(str) {}
    const std::string &getName() const { return mName; }

  private:
    const std::string &mName;
};

inline std::ostream &operator<<(std::ostream &o, const DecorateVariable &dv)
{
    if (dv.getName().compare(0, 3, "gl_") != 0)
    {
        o << "_";
    }
    o << dv.getName();
    return o;
}

struct PixelShaderOutputVariable
{
    PixelShaderOutputVariable() {}
    PixelShaderOutputVariable(GLenum typeIn,
                              const std::string &nameIn,
                              const std::string &sourceIn,
                              size_t outputLocationIn,
                              size_t outputIndexIn)
        : type(typeIn),
          name(nameIn),
          source(sourceIn),
          outputLocation(outputLocationIn),
          outputIndex(outputIndexIn)
    {}

    GLenum type = GL_NONE;
    std::string name;
    std::string source;
    size_t outputLocation = 0;
    size_t outputIndex    = 0;
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

    bool usesPointSize() const { return mBuiltinInfo[gl::ShaderType::Vertex].glPointSize.enabled; }

    const BuiltinInfo &operator[](gl::ShaderType shaderType) const
    {
        return mBuiltinInfo[shaderType];
    }
    BuiltinInfo &operator[](gl::ShaderType shaderType) { return mBuiltinInfo[shaderType]; }

  private:
    void updateBuiltins(gl::ShaderType shaderType,
                        const ProgramD3DMetadata &metadata,
                        const gl::VaryingPacking &packing);

    gl::ShaderMap<BuiltinInfo> mBuiltinInfo;
};

class DynamicHLSL : angle::NonCopyable
{
  public:
    explicit DynamicHLSL(RendererD3D *const renderer);

    std::string generateVertexShaderForInputLayout(
        const std::string &sourceShader,
        const gl::InputLayout &inputLayout,
        const std::vector<sh::ShaderVariable> &shaderAttributes) const;
    std::string generatePixelShaderForOutputSignature(
        const std::string &sourceShader,
        const std::vector<PixelShaderOutputVariable> &outputVariables,
        bool usesFragDepth,
        const std::vector<GLenum> &outputLayout) const;
    std::string generateComputeShaderForImage2DBindSignature(
        const d3d::Context *context,
        ProgramD3D &programD3D,
        const gl::ProgramState &programData,
        std::vector<sh::ShaderVariable> &image2DUniforms,
        const gl::ImageUnitTextureTypeMap &image2DBindLayout) const;
    void generateShaderLinkHLSL(const gl::Caps &caps,
                                const gl::ProgramState &programData,
                                const ProgramD3DMetadata &programMetadata,
                                const gl::VaryingPacking &varyingPacking,
                                const BuiltinVaryingsD3D &builtinsD3D,
                                gl::ShaderMap<std::string> *shaderHLSL) const;

    std::string generateGeometryShaderPreamble(const gl::VaryingPacking &varyingPacking,
                                               const BuiltinVaryingsD3D &builtinsD3D,
                                               const bool hasANGLEMultiviewEnabled,
                                               const bool selectViewInVS) const;

    std::string generateGeometryShaderHLSL(const gl::Caps &caps,
                                           gl::PrimitiveMode primitiveType,
                                           const gl::ProgramState &programData,
                                           const bool useViewScale,
                                           const bool hasANGLEMultiviewEnabled,
                                           const bool selectViewInVS,
                                           const bool pointSpriteEmulation,
                                           const std::string &preambleString) const;

    void getPixelShaderOutputKey(const gl::State &data,
                                 const gl::ProgramState &programData,
                                 const ProgramD3DMetadata &metadata,
                                 std::vector<PixelShaderOutputVariable> *outPixelShaderKey);

  private:
    RendererD3D *const mRenderer;

    void generateVaryingLinkHLSL(const gl::VaryingPacking &varyingPacking,
                                 const BuiltinInfo &builtins,
                                 bool programUsesPointSize,
                                 std::ostringstream &hlslStream) const;

    static void GenerateAttributeConversionHLSL(angle::FormatID vertexFormatID,
                                                const sh::ShaderVariable &shaderAttrib,
                                                std::ostringstream &outStream);
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_DYNAMICHLSL_H_
