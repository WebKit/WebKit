//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramD3D.h: Defines the rx::ProgramD3D class which implements rx::ProgramImpl.

#ifndef LIBANGLE_RENDERER_D3D_PROGRAMD3D_H_
#define LIBANGLE_RENDERER_D3D_PROGRAMD3D_H_

#include <string>
#include <vector>

#include "compiler/translator/hlsl/blocklayoutHLSL.h"
#include "libANGLE/Constants.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/ProgramImpl.h"
#include "libANGLE/renderer/d3d/ProgramExecutableD3D.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/renderer/d3d/ShaderD3D.h"
#include "platform/autogen/FeaturesD3D_autogen.h"

namespace rx
{
class RendererD3D;

class ProgramD3DMetadata final : angle::NonCopyable
{
  public:
    ProgramD3DMetadata(RendererD3D *renderer,
                       const gl::SharedCompiledShaderState &fragmentShader,
                       const gl::ShaderMap<SharedCompiledShaderStateD3D> &attachedShaders,
                       EGLenum clientType,
                       int shaderVersion);
    ~ProgramD3DMetadata();

    int getRendererMajorShaderModel() const;
    bool usesBroadcast(const gl::Version &clientVersion) const;
    bool usesSecondaryColor() const;
    bool usesPointCoord() const;
    bool usesFragCoord() const;
    bool usesPointSize() const;
    bool usesInsertedPointCoordValue() const;
    bool usesViewScale() const;
    bool hasMultiviewEnabled() const;
    bool usesVertexID() const;
    bool usesViewID() const;
    bool canSelectViewInVertexShader() const;
    bool addsPointCoordToVertexShader() const;
    bool usesTransformFeedbackGLPosition() const;
    bool usesSystemValuePointSize() const;
    bool usesMultipleFragmentOuts() const;
    bool usesCustomOutVars() const;
    bool usesSampleMask() const;
    const gl::SharedCompiledShaderState &getFragmentShader() const;
    FragDepthUsage getFragDepthUsage() const;
    uint8_t getClipDistanceArraySize() const;
    uint8_t getCullDistanceArraySize() const;

  private:
    const int mRendererMajorShaderModel;
    const std::string mShaderModelSuffix;
    const bool mUsesInstancedPointSpriteEmulation;
    const bool mUsesViewScale;
    const bool mCanSelectViewInVertexShader;
    gl::SharedCompiledShaderState mFragmentShader;
    const gl::ShaderMap<SharedCompiledShaderStateD3D> &mAttachedShaders;
    const EGLenum mClientType;
    int mShaderVersion;
};

class ProgramD3D : public ProgramImpl
{
  public:
    ProgramD3D(const gl::ProgramState &data, RendererD3D *renderer);
    ~ProgramD3D() override;

    void destroy(const gl::Context *context) override;

    std::unique_ptr<LinkEvent> load(const gl::Context *context,
                                    gl::BinaryInputStream *stream) override;
    void save(const gl::Context *context, gl::BinaryOutputStream *stream) override;
    void setBinaryRetrievableHint(bool retrievable) override;
    void setSeparable(bool separable) override;

    void prepareForLink(const gl::ShaderMap<ShaderImpl *> &shaders) override;
    std::unique_ptr<LinkEvent> link(const gl::Context *context,
                                    const gl::ProgramLinkedResources &resources,
                                    gl::ProgramMergedVaryings &&mergedVaryings) override;
    GLboolean validate(const gl::Caps &caps) override;

    void setUniform1fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform2fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform3fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform4fv(GLint location, GLsizei count, const GLfloat *v) override;
    void setUniform1iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform2iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform3iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform4iv(GLint location, GLsizei count, const GLint *v) override;
    void setUniform1uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniform2uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniform3uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniform4uiv(GLint location, GLsizei count, const GLuint *v) override;
    void setUniformMatrix2fv(GLint location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value) override;
    void setUniformMatrix3fv(GLint location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value) override;
    void setUniformMatrix4fv(GLint location,
                             GLsizei count,
                             GLboolean transpose,
                             const GLfloat *value) override;
    void setUniformMatrix2x3fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix3x2fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix2x4fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix4x2fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix3x4fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;
    void setUniformMatrix4x3fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value) override;

    void getUniformfv(const gl::Context *context, GLint location, GLfloat *params) const override;
    void getUniformiv(const gl::Context *context, GLint location, GLint *params) const override;
    void getUniformuiv(const gl::Context *context, GLint location, GLuint *params) const override;

    unsigned int getSerial() const;

    const gl::ProgramState &getState() const { return mState; }

    const ProgramExecutableD3D *getExecutable() const
    {
        return GetImplAs<ProgramExecutableD3D>(&mState.getExecutable());
    }
    ProgramExecutableD3D *getExecutable()
    {
        return GetImplAs<ProgramExecutableD3D>(&mState.getExecutable());
    }

  private:
    // These forward-declared tasks are used for multi-thread shader compiles.
    class GetExecutableTask;
    class GetVertexExecutableTask;
    class GetPixelExecutableTask;
    class GetGeometryExecutableTask;
    class GetComputeExecutableTask;
    class GraphicsProgramLinkEvent;
    class ComputeProgramLinkEvent;

    class LoadBinaryTask;
    class LoadBinaryLinkEvent;

    template <typename DestT>
    void getUniformInternal(GLint location, DestT *dataOut) const;

    template <typename T>
    void setUniformImpl(D3DUniform *targetUniform,
                        const gl::VariableLocation &locationInfo,
                        GLsizei count,
                        const T *v,
                        uint8_t *targetData,
                        GLenum uniformType);

    template <typename T>
    void setUniformInternal(GLint location, GLsizei count, const T *v, GLenum uniformType);

    template <int cols, int rows>
    void setUniformMatrixfvInternal(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *value);

    std::unique_ptr<LinkEvent> compileProgramExecutables(const gl::Context *context);
    std::unique_ptr<LinkEvent> compileComputeExecutable(const gl::Context *context);

    void reset();

    void linkResources(const gl::ProgramLinkedResources &resources);

    RendererD3D *mRenderer;

    unsigned int mSerial;

    static unsigned int issueSerial();
    static unsigned int mCurrentSerial;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_PROGRAMD3D_H_
