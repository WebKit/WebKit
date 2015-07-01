//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererGL.h: Defines the class interface for RendererGL.

#ifndef LIBANGLE_RENDERER_GL_RENDERERGL_H_
#define LIBANGLE_RENDERER_GL_RENDERERGL_H_

#include "libANGLE/renderer/Renderer.h"

namespace rx
{
class FunctionsGL;
class StateManagerGL;

class RendererGL : public Renderer
{
  public:
    RendererGL(const FunctionsGL *functions);
    ~RendererGL() override;

    gl::Error flush() override;
    gl::Error finish() override;

    gl::Error drawArrays(const gl::Data &data, GLenum mode,
                         GLint first, GLsizei count, GLsizei instances) override;
    gl::Error drawElements(const gl::Data &data, GLenum mode, GLsizei count, GLenum type,
                           const GLvoid *indices, GLsizei instances,
                           const RangeUI &indexRange) override;

    // Shader creation
    CompilerImpl *createCompiler(const gl::Data &data) override;
    ShaderImpl *createShader(GLenum type) override;
    ProgramImpl *createProgram() override;

    // Framebuffer creation
    FramebufferImpl *createDefaultFramebuffer(const gl::Framebuffer::Data &data) override;
    FramebufferImpl *createFramebuffer(const gl::Framebuffer::Data &data) override;

    // Texture creation
    TextureImpl *createTexture(GLenum target) override;

    // Renderbuffer creation
    RenderbufferImpl *createRenderbuffer() override;

    // Buffer creation
    BufferImpl *createBuffer() override;

    // Vertex Array creation
    VertexArrayImpl *createVertexArray() override;

    // Query and Fence creation
    QueryImpl *createQuery(GLenum type) override;
    FenceNVImpl *createFenceNV() override;
    FenceSyncImpl *createFenceSync() override;

    // Transform Feedback creation
    TransformFeedbackImpl *createTransformFeedback() override;

    // lost device
    void notifyDeviceLost() override;
    bool isDeviceLost() const override;
    bool testDeviceLost() override;
    bool testDeviceResettable() override;

    VendorID getVendorId() const override;
    std::string getVendorString() const override;
    std::string getRendererDescription() const override;

  private:
    void generateCaps(gl::Caps *outCaps, gl::TextureCapsMap* outTextureCaps, gl::Extensions *outExtensions) const override;
    Workarounds generateWorkarounds() const override;

    const FunctionsGL *mFunctions;
    StateManagerGL *mStateManager;
};

}

#endif // LIBANGLE_RENDERER_GL_RENDERERGL_H_
