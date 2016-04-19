
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererD3D.h: Defines a back-end specific class for the DirectX renderer.

#ifndef LIBANGLE_RENDERER_D3D_RENDERERD3D_H_
#define LIBANGLE_RENDERER_D3D_RENDERERD3D_H_

#include "common/debug.h"
#include "common/MemoryBuffer.h"
#include "libANGLE/Data.h"
#include "libANGLE/Device.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/Renderer.h"
#include "libANGLE/renderer/d3d/VertexDataManager.h"
#include "libANGLE/renderer/d3d/formatutilsD3D.h"
#include "libANGLE/renderer/d3d/WorkaroundsD3D.h"
#include "libANGLE/renderer/d3d/d3d11/NativeWindow.h"

//FIXME(jmadill): std::array is currently prohibited by Chromium style guide
#include <array>

namespace egl
{
class ConfigSet;
}

namespace gl
{
class DebugAnnotator;
class InfoLog;
class Texture;
struct LinkedVarying;
}

namespace rx
{
struct D3DUniform;
struct D3DVarying;
class DeviceD3D;
class EGLImageD3D;
class ImageD3D;
class IndexBuffer;
class ProgramD3D;
class RenderTargetD3D;
class ShaderExecutableD3D;
class SwapChainD3D;
class TextureStorage;
class UniformStorageD3D;
class VertexBuffer;

enum ShaderType
{
    SHADER_VERTEX,
    SHADER_PIXEL,
    SHADER_GEOMETRY,
    SHADER_TYPE_MAX
};

struct DeviceIdentifier
{
    UINT VendorId;
    UINT DeviceId;
    UINT SubSysId;
    UINT Revision;
    UINT FeatureLevel;
};

enum RendererClass
{
    RENDERER_D3D11,
    RENDERER_D3D9
};

// Useful for unit testing
class BufferFactoryD3D
{
  public:
    BufferFactoryD3D() {}
    virtual ~BufferFactoryD3D() {}

    virtual VertexBuffer *createVertexBuffer() = 0;
    virtual IndexBuffer *createIndexBuffer() = 0;

    // TODO(jmadill): add VertexFormatCaps
    virtual VertexConversionType getVertexConversionType(gl::VertexFormatType vertexFormatType) const = 0;
    virtual GLenum getVertexComponentType(gl::VertexFormatType vertexFormatType) const = 0;
    virtual gl::ErrorOrResult<unsigned int> getVertexSpaceRequired(
        const gl::VertexAttribute &attrib,
        GLsizei count,
        GLsizei instances) const = 0;
};

using AttribIndexArray = std::array<int, gl::MAX_VERTEX_ATTRIBS>;

class RendererD3D : public Renderer, public BufferFactoryD3D
{
  public:
    explicit RendererD3D(egl::Display *display);
    virtual ~RendererD3D();

    virtual egl::Error initialize() = 0;

    virtual egl::ConfigSet generateConfigs() const = 0;
    virtual void generateDisplayExtensions(egl::DisplayExtensions *outExtensions) const = 0;

    gl::Error drawArrays(const gl::Data &data, GLenum mode, GLint first, GLsizei count) override;
    gl::Error drawArraysInstanced(const gl::Data &data,
                                  GLenum mode,
                                  GLint first,
                                  GLsizei count,
                                  GLsizei instanceCount) override;

    gl::Error drawElements(const gl::Data &data,
                           GLenum mode,
                           GLsizei count,
                           GLenum type,
                           const GLvoid *indices,
                           const gl::IndexRange &indexRange) override;
    gl::Error drawElementsInstanced(const gl::Data &data,
                                    GLenum mode,
                                    GLsizei count,
                                    GLenum type,
                                    const GLvoid *indices,
                                    GLsizei instances,
                                    const gl::IndexRange &indexRange) override;
    gl::Error drawRangeElements(const gl::Data &data,
                                GLenum mode,
                                GLuint start,
                                GLuint end,
                                GLsizei count,
                                GLenum type,
                                const GLvoid *indices,
                                const gl::IndexRange &indexRange) override;

    bool isDeviceLost() const override;
    std::string getVendorString() const override;

    SamplerImpl *createSampler() override;

    virtual int getMinorShaderModel() const = 0;
    virtual std::string getShaderModelSuffix() const = 0;

    // Direct3D Specific methods
    virtual DeviceIdentifier getAdapterIdentifier() const = 0;

    virtual SwapChainD3D *createSwapChain(NativeWindow nativeWindow,
                                          HANDLE shareHandle,
                                          GLenum backBufferFormat,
                                          GLenum depthBufferFormat,
                                          EGLint orientation) = 0;

    virtual gl::Error generateSwizzle(gl::Texture *texture) = 0;
    virtual gl::Error setSamplerState(gl::SamplerType type, int index, gl::Texture *texture, const gl::SamplerState &sampler) = 0;
    virtual gl::Error setTexture(gl::SamplerType type, int index, gl::Texture *texture) = 0;

    virtual gl::Error setUniformBuffers(const gl::Data &data,
                                        const std::vector<GLint> &vertexUniformBuffers,
                                        const std::vector<GLint> &fragmentUniformBuffers) = 0;

    virtual gl::Error updateState(const gl::Data &data, GLenum drawMode) = 0;

    virtual gl::Error applyRenderTarget(const gl::Framebuffer *frameBuffer) = 0;
    virtual gl::Error applyUniforms(const ProgramD3D &programD3D,
                                    GLenum drawMode,
                                    const std::vector<D3DUniform *> &uniformArray) = 0;
    virtual bool applyPrimitiveType(GLenum primitiveType, GLsizei elementCount, bool usesPointSize) = 0;
    virtual gl::Error applyVertexBuffer(const gl::State &state,
                                        GLenum mode,
                                        GLint first,
                                        GLsizei count,
                                        GLsizei instances,
                                        TranslatedIndexData *indexInfo) = 0;
    virtual gl::Error applyIndexBuffer(const gl::Data &data,
                                       const GLvoid *indices,
                                       GLsizei count,
                                       GLenum mode,
                                       GLenum type,
                                       TranslatedIndexData *indexInfo) = 0;
    virtual gl::Error applyTransformFeedbackBuffers(const gl::State &state) = 0;

    virtual unsigned int getReservedVertexUniformVectors() const = 0;
    virtual unsigned int getReservedFragmentUniformVectors() const = 0;
    virtual unsigned int getReservedVertexUniformBuffers() const = 0;
    virtual unsigned int getReservedFragmentUniformBuffers() const = 0;

    virtual int getMajorShaderModel() const = 0;

    const WorkaroundsD3D &getWorkarounds() const;

    // Pixel operations
    virtual gl::Error copyImage2D(const gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                                 const gl::Offset &destOffset, TextureStorage *storage, GLint level) = 0;
    virtual gl::Error copyImageCube(const gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                                    const gl::Offset &destOffset, TextureStorage *storage, GLenum target, GLint level) = 0;
    virtual gl::Error copyImage3D(const gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                                  const gl::Offset &destOffset, TextureStorage *storage, GLint level) = 0;
    virtual gl::Error copyImage2DArray(const gl::Framebuffer *framebuffer, const gl::Rectangle &sourceRect, GLenum destFormat,
                                       const gl::Offset &destOffset, TextureStorage *storage, GLint level) = 0;

    // RenderTarget creation
    virtual gl::Error createRenderTarget(int width, int height, GLenum format, GLsizei samples, RenderTargetD3D **outRT) = 0;
    virtual gl::Error createRenderTargetCopy(RenderTargetD3D *source, RenderTargetD3D **outRT) = 0;

    // Shader operations
    virtual gl::Error loadExecutable(const void *function,
                                     size_t length,
                                     ShaderType type,
                                     const std::vector<D3DVarying> &streamOutVaryings,
                                     bool separatedOutputBuffers,
                                     ShaderExecutableD3D **outExecutable) = 0;
    virtual gl::Error compileToExecutable(gl::InfoLog &infoLog,
                                          const std::string &shaderHLSL,
                                          ShaderType type,
                                          const std::vector<D3DVarying> &streamOutVaryings,
                                          bool separatedOutputBuffers,
                                          const D3DCompilerWorkarounds &workarounds,
                                          ShaderExecutableD3D **outExectuable) = 0;
    virtual UniformStorageD3D *createUniformStorage(size_t storageSize) = 0;

    // Image operations
    virtual ImageD3D *createImage() = 0;
    virtual gl::Error generateMipmap(ImageD3D *dest, ImageD3D *source) = 0;
    virtual gl::Error generateMipmapsUsingD3D(TextureStorage *storage,
                                              const gl::TextureState &textureState) = 0;
    virtual TextureStorage *createTextureStorage2D(SwapChainD3D *swapChain) = 0;
    virtual TextureStorage *createTextureStorageEGLImage(EGLImageD3D *eglImage) = 0;
    virtual TextureStorage *createTextureStorage2D(GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, int levels, bool hintLevelZeroOnly) = 0;
    virtual TextureStorage *createTextureStorageCube(GLenum internalformat, bool renderTarget, int size, int levels, bool hintLevelZeroOnly) = 0;
    virtual TextureStorage *createTextureStorage3D(GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, GLsizei depth, int levels) = 0;
    virtual TextureStorage *createTextureStorage2DArray(GLenum internalformat, bool renderTarget, GLsizei width, GLsizei height, GLsizei depth, int levels) = 0;

    // Buffer-to-texture and Texture-to-buffer copies
    virtual bool supportsFastCopyBufferToTexture(GLenum internalFormat) const = 0;
    virtual gl::Error fastCopyBufferToTexture(const gl::PixelUnpackState &unpack, unsigned int offset, RenderTargetD3D *destRenderTarget,
                                              GLenum destinationFormat, GLenum sourcePixelsType, const gl::Box &destArea) = 0;

    // Device lost
    void notifyDeviceLost() override;
    virtual bool resetDevice() = 0;
    virtual RendererClass getRendererClass() const = 0;
    virtual void *getD3DDevice() = 0;

    gl::Error getScratchMemoryBuffer(size_t requestedSize, MemoryBuffer **bufferOut);

    // EXT_debug_marker
    void insertEventMarker(GLsizei length, const char *marker) override;
    void pushGroupMarker(GLsizei length, const char *marker) override;
    void popGroupMarker() override;

    void setGPUDisjoint();

    GLint getGPUDisjoint() override;
    GLint64 getTimestamp() override;

    void onMakeCurrent(const gl::Data &data) override;

    // In D3D11, faster than calling setTexture a jillion times
    virtual gl::Error clearTextures(gl::SamplerType samplerType, size_t rangeStart, size_t rangeEnd) = 0;

    virtual egl::Error getEGLDevice(DeviceImpl **device) = 0;

    bool presentPathFastEnabled() const { return mPresentPathFastEnabled; }

    // Stream creation
    virtual StreamImpl *createStream(const egl::AttributeMap &attribs) = 0;

  protected:
    virtual bool getLUID(LUID *adapterLuid) const = 0;
    virtual gl::Error applyShadersImpl(const gl::Data &data, GLenum drawMode) = 0;

    void cleanup();

    virtual void createAnnotator() = 0;

    static unsigned int GetBlendSampleMask(const gl::Data &data, int samples);
    // dirtyPointer is a special value that will make the comparison with any valid pointer fail and force the renderer to re-apply the state.

    egl::Display *mDisplay;
    bool mDeviceLost;

    void initializeDebugAnnotator();
    gl::DebugAnnotator *mAnnotator;

    bool mPresentPathFastEnabled;

  private:
    gl::Error genericDrawArrays(const gl::Data &data,
                                GLenum mode,
                                GLint first,
                                GLsizei count,
                                GLsizei instances);

    gl::Error genericDrawElements(const gl::Data &data,
                                  GLenum mode,
                                  GLsizei count,
                                  GLenum type,
                                  const GLvoid *indices,
                                  GLsizei instances,
                                  const gl::IndexRange &indexRange);

    virtual gl::Error drawArraysImpl(const gl::Data &data,
                                     GLenum mode,
                                     GLint startVertex,
                                     GLsizei count,
                                     GLsizei instances) = 0;
    virtual gl::Error drawElementsImpl(const gl::Data &data,
                                       const TranslatedIndexData &indexInfo,
                                       GLenum mode,
                                       GLsizei count,
                                       GLenum type,
                                       const GLvoid *indices,
                                       GLsizei instances) = 0;

    //FIXME(jmadill): std::array is currently prohibited by Chromium style guide
    typedef std::array<gl::Texture*, gl::IMPLEMENTATION_MAX_FRAMEBUFFER_ATTACHMENTS> FramebufferTextureArray;

    gl::Error generateSwizzles(const gl::Data &data, gl::SamplerType type);
    gl::Error generateSwizzles(const gl::Data &data);

    gl::Error applyState(const gl::Data &data, GLenum drawMode);
    gl::Error applyShaders(const gl::Data &data, GLenum drawMode);
    gl::Error applyTextures(const gl::Data &data, gl::SamplerType shaderType,
                            const FramebufferTextureArray &framebufferTextures, size_t framebufferTextureCount);
    gl::Error applyTextures(const gl::Data &data);

    bool skipDraw(const gl::Data &data, GLenum drawMode);
    gl::Error markTransformFeedbackUsage(const gl::Data &data);

    size_t getBoundFramebufferTextures(const gl::Data &data, FramebufferTextureArray *outTextureArray);
    gl::Texture *getIncompleteTexture(GLenum type);

    gl::DebugAnnotator *getAnnotator();

    virtual WorkaroundsD3D generateWorkarounds() const = 0;

    gl::TextureMap mIncompleteTextures;
    MemoryBuffer mScratchMemoryBuffer;
    unsigned int mScratchMemoryBufferResetCounter;

    mutable bool mWorkaroundsInitialized;
    mutable WorkaroundsD3D mWorkarounds;

    bool mDisjoint;
};

}

#endif // LIBANGLE_RENDERER_D3D_RENDERERD3D_H_
