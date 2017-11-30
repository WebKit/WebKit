//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// State.h: Defines the State class, encapsulating raw GL state

#ifndef LIBANGLE_STATE_H_
#define LIBANGLE_STATE_H_

#include <bitset>
#include <memory>

#include "common/Color.h"
#include "common/angleutils.h"
#include "common/bitset_utils.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Program.h"
#include "libANGLE/ProgramPipeline.h"
#include "libANGLE/RefCountObject.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/Sampler.h"
#include "libANGLE/Texture.h"
#include "libANGLE/TransformFeedback.h"
#include "libANGLE/Version.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/angletypes.h"

namespace gl
{
class Query;
class VertexArray;
class Context;
struct Caps;

class State : public OnAttachmentDirtyReceiver, angle::NonCopyable
{
  public:
    State();
    ~State() override;

    void initialize(const Context *context,
                    bool debug,
                    bool bindGeneratesResource,
                    bool clientArraysEnabled,
                    bool robustResourceInit,
                    bool programBinaryCacheEnabled);
    void reset(const Context *context);

    // State chunk getters
    const RasterizerState &getRasterizerState() const;
    const BlendState &getBlendState() const;
    const DepthStencilState &getDepthStencilState() const;

    // Clear behavior setters & state parameter block generation function
    void setColorClearValue(float red, float green, float blue, float alpha);
    void setDepthClearValue(float depth);
    void setStencilClearValue(int stencil);

    const ColorF &getColorClearValue() const { return mColorClearValue; }
    float getDepthClearValue() const { return mDepthClearValue; }
    int getStencilClearValue() const { return mStencilClearValue; }

    // Write mask manipulation
    void setColorMask(bool red, bool green, bool blue, bool alpha);
    void setDepthMask(bool mask);

    // Discard toggle & query
    bool isRasterizerDiscardEnabled() const;
    void setRasterizerDiscard(bool enabled);

    // Primitive restart
    bool isPrimitiveRestartEnabled() const;
    void setPrimitiveRestart(bool enabled);

    // Face culling state manipulation
    bool isCullFaceEnabled() const;
    void setCullFace(bool enabled);
    void setCullMode(CullFaceMode mode);
    void setFrontFace(GLenum front);

    // Depth test state manipulation
    bool isDepthTestEnabled() const;
    void setDepthTest(bool enabled);
    void setDepthFunc(GLenum depthFunc);
    void setDepthRange(float zNear, float zFar);
    float getNearPlane() const;
    float getFarPlane() const;

    // Blend state manipulation
    bool isBlendEnabled() const;
    void setBlend(bool enabled);
    void setBlendFactors(GLenum sourceRGB, GLenum destRGB, GLenum sourceAlpha, GLenum destAlpha);
    void setBlendColor(float red, float green, float blue, float alpha);
    void setBlendEquation(GLenum rgbEquation, GLenum alphaEquation);
    const ColorF &getBlendColor() const;

    // Stencil state maniupulation
    bool isStencilTestEnabled() const;
    void setStencilTest(bool enabled);
    void setStencilParams(GLenum stencilFunc, GLint stencilRef, GLuint stencilMask);
    void setStencilBackParams(GLenum stencilBackFunc, GLint stencilBackRef, GLuint stencilBackMask);
    void setStencilWritemask(GLuint stencilWritemask);
    void setStencilBackWritemask(GLuint stencilBackWritemask);
    void setStencilOperations(GLenum stencilFail,
                              GLenum stencilPassDepthFail,
                              GLenum stencilPassDepthPass);
    void setStencilBackOperations(GLenum stencilBackFail,
                                  GLenum stencilBackPassDepthFail,
                                  GLenum stencilBackPassDepthPass);
    GLint getStencilRef() const;
    GLint getStencilBackRef() const;

    // Depth bias/polygon offset state manipulation
    bool isPolygonOffsetFillEnabled() const;
    void setPolygonOffsetFill(bool enabled);
    void setPolygonOffsetParams(GLfloat factor, GLfloat units);

    // Multisample coverage state manipulation
    bool isSampleAlphaToCoverageEnabled() const;
    void setSampleAlphaToCoverage(bool enabled);
    bool isSampleCoverageEnabled() const;
    void setSampleCoverage(bool enabled);
    void setSampleCoverageParams(GLclampf value, bool invert);
    GLfloat getSampleCoverageValue() const;
    bool getSampleCoverageInvert() const;

    // Multisample mask state manipulation.
    bool isSampleMaskEnabled() const;
    void setSampleMaskEnabled(bool enabled);
    void setSampleMaskParams(GLuint maskNumber, GLbitfield mask);
    GLbitfield getSampleMaskWord(GLuint maskNumber) const;
    GLuint getMaxSampleMaskWords() const;

    // Multisampling/alpha to one manipulation.
    void setSampleAlphaToOne(bool enabled);
    bool isSampleAlphaToOneEnabled() const;
    void setMultisampling(bool enabled);
    bool isMultisamplingEnabled() const;

    // Scissor test state toggle & query
    bool isScissorTestEnabled() const;
    void setScissorTest(bool enabled);
    void setScissorParams(GLint x, GLint y, GLsizei width, GLsizei height);
    const Rectangle &getScissor() const;

    // Dither state toggle & query
    bool isDitherEnabled() const;
    void setDither(bool enabled);

    // Generic state toggle & query
    void setEnableFeature(GLenum feature, bool enabled);
    bool getEnableFeature(GLenum feature) const;

    // Line width state setter
    void setLineWidth(GLfloat width);
    float getLineWidth() const;

    // Hint setters
    void setGenerateMipmapHint(GLenum hint);
    void setFragmentShaderDerivativeHint(GLenum hint);

    // GL_CHROMIUM_bind_generates_resource
    bool isBindGeneratesResourceEnabled() const;

    // GL_ANGLE_client_arrays
    bool areClientArraysEnabled() const;

    // Viewport state setter/getter
    void setViewportParams(GLint x, GLint y, GLsizei width, GLsizei height);
    const Rectangle &getViewport() const;

    // Texture binding & active texture unit manipulation
    void setActiveSampler(unsigned int active);
    unsigned int getActiveSampler() const;
    void setSamplerTexture(const Context *context, GLenum type, Texture *texture);
    Texture *getTargetTexture(GLenum target) const;
    Texture *getSamplerTexture(unsigned int sampler, GLenum type) const;
    GLuint getSamplerTextureId(unsigned int sampler, GLenum type) const;
    void detachTexture(const Context *context, const TextureMap &zeroTextures, GLuint texture);
    void initializeZeroTextures(const Context *context, const TextureMap &zeroTextures);

    // Sampler object binding manipulation
    void setSamplerBinding(const Context *context, GLuint textureUnit, Sampler *sampler);
    GLuint getSamplerId(GLuint textureUnit) const;
    Sampler *getSampler(GLuint textureUnit) const;
    void detachSampler(const Context *context, GLuint sampler);

    // Renderbuffer binding manipulation
    void setRenderbufferBinding(const Context *context, Renderbuffer *renderbuffer);
    GLuint getRenderbufferId() const;
    Renderbuffer *getCurrentRenderbuffer() const;
    void detachRenderbuffer(const Context *context, GLuint renderbuffer);

    // Framebuffer binding manipulation
    void setReadFramebufferBinding(Framebuffer *framebuffer);
    void setDrawFramebufferBinding(Framebuffer *framebuffer);
    Framebuffer *getTargetFramebuffer(GLenum target) const;
    Framebuffer *getReadFramebuffer() const;
    Framebuffer *getDrawFramebuffer() const;
    bool removeReadFramebufferBinding(GLuint framebuffer);
    bool removeDrawFramebufferBinding(GLuint framebuffer);

    // Vertex array object binding manipulation
    void setVertexArrayBinding(VertexArray *vertexArray);
    GLuint getVertexArrayId() const;
    VertexArray *getVertexArray() const;
    bool removeVertexArrayBinding(GLuint vertexArray);

    // Program binding manipulation
    void setProgram(const Context *context, Program *newProgram);
    Program *getProgram() const;

    // Transform feedback object (not buffer) binding manipulation
    void setTransformFeedbackBinding(const Context *context, TransformFeedback *transformFeedback);
    TransformFeedback *getCurrentTransformFeedback() const;
    bool isTransformFeedbackActiveUnpaused() const;
    bool removeTransformFeedbackBinding(const Context *context, GLuint transformFeedback);

    // Query binding manipulation
    bool isQueryActive(const GLenum type) const;
    bool isQueryActive(Query *query) const;
    void setActiveQuery(const Context *context, GLenum target, Query *query);
    GLuint getActiveQueryId(GLenum target) const;
    Query *getActiveQuery(GLenum target) const;

    // Program Pipeline binding manipulation
    void setProgramPipelineBinding(const Context *context, ProgramPipeline *pipeline);
    void detachProgramPipeline(const Context *context, GLuint pipeline);

    //// Typed buffer binding point manipulation ////
    void setBufferBinding(const Context *context, BufferBinding target, Buffer *buffer);
    Buffer *getTargetBuffer(BufferBinding target) const;
    void setIndexedBufferBinding(const Context *context,
                                 BufferBinding target,
                                 GLuint index,
                                 Buffer *buffer,
                                 GLintptr offset,
                                 GLsizeiptr size);

    const OffsetBindingPointer<Buffer> &getIndexedUniformBuffer(size_t index) const;
    const OffsetBindingPointer<Buffer> &getIndexedAtomicCounterBuffer(size_t index) const;
    const OffsetBindingPointer<Buffer> &getIndexedShaderStorageBuffer(size_t index) const;

    // Detach a buffer from all bindings
    void detachBuffer(const Context *context, GLuint bufferName);

    // Vertex attrib manipulation
    void setEnableVertexAttribArray(unsigned int attribNum, bool enabled);
    void setElementArrayBuffer(const Context *context, Buffer *buffer);
    void setVertexAttribf(GLuint index, const GLfloat values[4]);
    void setVertexAttribu(GLuint index, const GLuint values[4]);
    void setVertexAttribi(GLuint index, const GLint values[4]);
    void setVertexAttribPointer(const Context *context,
                                unsigned int attribNum,
                                Buffer *boundBuffer,
                                GLint size,
                                GLenum type,
                                bool normalized,
                                bool pureInteger,
                                GLsizei stride,
                                const void *pointer);
    void setVertexAttribDivisor(const Context *context, GLuint index, GLuint divisor);
    const VertexAttribCurrentValueData &getVertexAttribCurrentValue(size_t attribNum) const;
    const std::vector<VertexAttribCurrentValueData> &getVertexAttribCurrentValues() const;
    const void *getVertexAttribPointer(unsigned int attribNum) const;
    void bindVertexBuffer(const Context *context,
                          GLuint bindingIndex,
                          Buffer *boundBuffer,
                          GLintptr offset,
                          GLsizei stride);
    void setVertexAttribFormat(GLuint attribIndex,
                               GLint size,
                               GLenum type,
                               bool normalized,
                               bool pureInteger,
                               GLuint relativeOffset);
    void setVertexAttribBinding(const Context *context, GLuint attribIndex, GLuint bindingIndex);
    void setVertexBindingDivisor(GLuint bindingIndex, GLuint divisor);

    // Pixel pack state manipulation
    void setPackAlignment(GLint alignment);
    GLint getPackAlignment() const;
    void setPackReverseRowOrder(bool reverseRowOrder);
    bool getPackReverseRowOrder() const;
    void setPackRowLength(GLint rowLength);
    GLint getPackRowLength() const;
    void setPackSkipRows(GLint skipRows);
    GLint getPackSkipRows() const;
    void setPackSkipPixels(GLint skipPixels);
    GLint getPackSkipPixels() const;
    const PixelPackState &getPackState() const;
    PixelPackState &getPackState();

    // Pixel unpack state manipulation
    void setUnpackAlignment(GLint alignment);
    GLint getUnpackAlignment() const;
    void setUnpackRowLength(GLint rowLength);
    GLint getUnpackRowLength() const;
    void setUnpackImageHeight(GLint imageHeight);
    GLint getUnpackImageHeight() const;
    void setUnpackSkipImages(GLint skipImages);
    GLint getUnpackSkipImages() const;
    void setUnpackSkipRows(GLint skipRows);
    GLint getUnpackSkipRows() const;
    void setUnpackSkipPixels(GLint skipPixels);
    GLint getUnpackSkipPixels() const;
    const PixelUnpackState &getUnpackState() const;
    PixelUnpackState &getUnpackState();

    // Debug state
    const Debug &getDebug() const;
    Debug &getDebug();

    // CHROMIUM_framebuffer_mixed_samples coverage modulation
    void setCoverageModulation(GLenum components);
    GLenum getCoverageModulation() const;

    // CHROMIUM_path_rendering
    void loadPathRenderingMatrix(GLenum matrixMode, const GLfloat *matrix);
    const GLfloat *getPathRenderingMatrix(GLenum which) const;
    void setPathStencilFunc(GLenum func, GLint ref, GLuint mask);

    GLenum getPathStencilFunc() const;
    GLint getPathStencilRef() const;
    GLuint getPathStencilMask() const;

    // GL_EXT_sRGB_write_control
    void setFramebufferSRGB(bool sRGB);
    bool getFramebufferSRGB() const;

    // State query functions
    void getBooleanv(GLenum pname, GLboolean *params);
    void getFloatv(GLenum pname, GLfloat *params);
    void getIntegerv(const Context *context, GLenum pname, GLint *params);
    void getPointerv(GLenum pname, void **params) const;
    void getIntegeri_v(GLenum target, GLuint index, GLint *data);
    void getInteger64i_v(GLenum target, GLuint index, GLint64 *data);
    void getBooleani_v(GLenum target, GLuint index, GLboolean *data);

    bool hasMappedBuffer(BufferBinding target) const;
    bool isRobustResourceInitEnabled() const { return mRobustResourceInit; }

    // Sets the dirty bit for the program executable.
    void onProgramExecutableChange(Program *program);

    enum DirtyBitType
    {
        DIRTY_BIT_SCISSOR_TEST_ENABLED,
        DIRTY_BIT_SCISSOR,
        DIRTY_BIT_VIEWPORT,
        DIRTY_BIT_DEPTH_RANGE,
        DIRTY_BIT_BLEND_ENABLED,
        DIRTY_BIT_BLEND_COLOR,
        DIRTY_BIT_BLEND_FUNCS,
        DIRTY_BIT_BLEND_EQUATIONS,
        DIRTY_BIT_COLOR_MASK,
        DIRTY_BIT_SAMPLE_ALPHA_TO_COVERAGE_ENABLED,
        DIRTY_BIT_SAMPLE_COVERAGE_ENABLED,
        DIRTY_BIT_SAMPLE_COVERAGE,
        DIRTY_BIT_SAMPLE_MASK_ENABLED,
        DIRTY_BIT_SAMPLE_MASK,
        DIRTY_BIT_DEPTH_TEST_ENABLED,
        DIRTY_BIT_DEPTH_FUNC,
        DIRTY_BIT_DEPTH_MASK,
        DIRTY_BIT_STENCIL_TEST_ENABLED,
        DIRTY_BIT_STENCIL_FUNCS_FRONT,
        DIRTY_BIT_STENCIL_FUNCS_BACK,
        DIRTY_BIT_STENCIL_OPS_FRONT,
        DIRTY_BIT_STENCIL_OPS_BACK,
        DIRTY_BIT_STENCIL_WRITEMASK_FRONT,
        DIRTY_BIT_STENCIL_WRITEMASK_BACK,
        DIRTY_BIT_CULL_FACE_ENABLED,
        DIRTY_BIT_CULL_FACE,
        DIRTY_BIT_FRONT_FACE,
        DIRTY_BIT_POLYGON_OFFSET_FILL_ENABLED,
        DIRTY_BIT_POLYGON_OFFSET,
        DIRTY_BIT_RASTERIZER_DISCARD_ENABLED,
        DIRTY_BIT_LINE_WIDTH,
        DIRTY_BIT_PRIMITIVE_RESTART_ENABLED,
        DIRTY_BIT_CLEAR_COLOR,
        DIRTY_BIT_CLEAR_DEPTH,
        DIRTY_BIT_CLEAR_STENCIL,
        DIRTY_BIT_UNPACK_STATE,
        DIRTY_BIT_UNPACK_BUFFER_BINDING,
        DIRTY_BIT_PACK_STATE,
        DIRTY_BIT_PACK_BUFFER_BINDING,
        DIRTY_BIT_DITHER_ENABLED,
        DIRTY_BIT_GENERATE_MIPMAP_HINT,
        DIRTY_BIT_SHADER_DERIVATIVE_HINT,
        DIRTY_BIT_READ_FRAMEBUFFER_BINDING,
        DIRTY_BIT_DRAW_FRAMEBUFFER_BINDING,
        DIRTY_BIT_RENDERBUFFER_BINDING,
        DIRTY_BIT_VERTEX_ARRAY_BINDING,
        DIRTY_BIT_DRAW_INDIRECT_BUFFER_BINDING,
        DIRTY_BIT_PROGRAM_BINDING,
        DIRTY_BIT_PROGRAM_EXECUTABLE,
        // TODO(jmadill): Fine-grained dirty bits for each texture/sampler.
        DIRTY_BIT_TEXTURE_BINDINGS,
        DIRTY_BIT_SAMPLER_BINDINGS,
        DIRTY_BIT_MULTISAMPLING,
        DIRTY_BIT_SAMPLE_ALPHA_TO_ONE,
        DIRTY_BIT_COVERAGE_MODULATION,         // CHROMIUM_framebuffer_mixed_samples
        DIRTY_BIT_PATH_RENDERING_MATRIX_MV,    // CHROMIUM_path_rendering path model view matrix
        DIRTY_BIT_PATH_RENDERING_MATRIX_PROJ,  // CHROMIUM_path_rendering path projection matrix
        DIRTY_BIT_PATH_RENDERING_STENCIL_STATE,
        DIRTY_BIT_FRAMEBUFFER_SRGB,  // GL_EXT_sRGB_write_control
        DIRTY_BIT_CURRENT_VALUES,
        DIRTY_BIT_INVALID,
        DIRTY_BIT_MAX = DIRTY_BIT_INVALID,
    };

    static_assert(DIRTY_BIT_MAX <= 64, "State dirty bits must be capped at 64");

    // TODO(jmadill): Consider storing dirty objects in a list instead of by binding.
    enum DirtyObjectType
    {
        DIRTY_OBJECT_READ_FRAMEBUFFER,
        DIRTY_OBJECT_DRAW_FRAMEBUFFER,
        DIRTY_OBJECT_VERTEX_ARRAY,
        // Use a very coarse bit for any program or texture change.
        // TODO(jmadill): Fine-grained dirty bits for each texture/sampler.
        DIRTY_OBJECT_PROGRAM_TEXTURES,
        DIRTY_OBJECT_UNKNOWN,
        DIRTY_OBJECT_MAX = DIRTY_OBJECT_UNKNOWN,
    };

    typedef angle::BitSet<DIRTY_BIT_MAX> DirtyBits;
    const DirtyBits &getDirtyBits() const { return mDirtyBits; }
    void clearDirtyBits() { mDirtyBits.reset(); }
    void clearDirtyBits(const DirtyBits &bitset) { mDirtyBits &= ~bitset; }
    void setAllDirtyBits() { mDirtyBits.set(); }

    typedef angle::BitSet<DIRTY_OBJECT_MAX> DirtyObjects;
    void clearDirtyObjects() { mDirtyObjects.reset(); }
    void setAllDirtyObjects() { mDirtyObjects.set(); }
    void syncDirtyObjects(const Context *context);
    void syncDirtyObjects(const Context *context, const DirtyObjects &bitset);
    void syncDirtyObject(const Context *context, GLenum target);
    void setObjectDirty(GLenum target);

    // This actually clears the current value dirty bits.
    // TODO(jmadill): Pass mutable dirty bits into Impl.
    AttributesMask getAndResetDirtyCurrentValues() const;

    void setImageUnit(const Context *context,
                      GLuint unit,
                      Texture *texture,
                      GLint level,
                      GLboolean layered,
                      GLint layer,
                      GLenum access,
                      GLenum format);

    const ImageUnit &getImageUnit(GLuint unit) const;
    const std::vector<Texture *> &getCompleteTextureCache() const { return mCompleteTextureCache; }

    // Handle a dirty texture event.
    void signal(size_t textureIndex, InitState initState) override;

    Error clearUnclearedActiveTextures(const Context *context);

  private:
    void syncProgramTextures(const Context *context);

    // Cached values from Context's caps
    GLuint mMaxDrawBuffers;
    GLuint mMaxCombinedTextureImageUnits;

    ColorF mColorClearValue;
    GLfloat mDepthClearValue;
    int mStencilClearValue;

    RasterizerState mRasterizer;
    bool mScissorTest;
    Rectangle mScissor;

    BlendState mBlend;
    ColorF mBlendColor;
    bool mSampleCoverage;
    GLfloat mSampleCoverageValue;
    bool mSampleCoverageInvert;
    bool mSampleMask;
    GLuint mMaxSampleMaskWords;
    std::array<GLbitfield, MAX_SAMPLE_MASK_WORDS> mSampleMaskValues;

    DepthStencilState mDepthStencil;
    GLint mStencilRef;
    GLint mStencilBackRef;

    GLfloat mLineWidth;

    GLenum mGenerateMipmapHint;
    GLenum mFragmentShaderDerivativeHint;

    bool mBindGeneratesResource;
    bool mClientArraysEnabled;

    Rectangle mViewport;
    float mNearZ;
    float mFarZ;

    Framebuffer *mReadFramebuffer;
    Framebuffer *mDrawFramebuffer;
    BindingPointer<Renderbuffer> mRenderbuffer;
    Program *mProgram;
    BindingPointer<ProgramPipeline> mProgramPipeline;

    typedef std::vector<VertexAttribCurrentValueData> VertexAttribVector;
    VertexAttribVector mVertexAttribCurrentValues;  // From glVertexAttrib
    VertexArray *mVertexArray;

    // Texture and sampler bindings
    size_t mActiveSampler;  // Active texture unit selector - GL_TEXTURE0

    typedef std::vector<BindingPointer<Texture>> TextureBindingVector;
    typedef std::map<GLenum, TextureBindingVector> TextureBindingMap;
    TextureBindingMap mSamplerTextures;

    // Texture Completeness Caching
    // ----------------------------
    // The texture completeness cache uses dirty bits to avoid having to scan the list
    // of textures each draw call. This gl::State class implements OnAttachmentDirtyReceiver,
    // and keeps an array of bindings to the Texture class. When the Textures are marked dirty,
    // they send messages to the State class (and any Framebuffers they're attached to) via the
    // State::signal method (see above). Internally this then invalidates the completeness cache.
    //
    // Note this requires that we also invalidate the completeness cache manually on events like
    // re-binding textures/samplers or a change in the program. For more information see the
    // signal_utils.h header and the design doc linked there.

    // A cache of complete textures. nullptr indicates unbound or incomplete.
    // Don't use BindingPointer because this cache is only valid within a draw call.
    // Also stores a notification channel to the texture itself to handle texture change events.
    std::vector<Texture *> mCompleteTextureCache;
    std::vector<OnAttachmentDirtyBinding> mCompleteTextureBindings;
    InitState mCachedTexturesInitState;
    using ActiveTextureMask = angle::BitSet<IMPLEMENTATION_MAX_ACTIVE_TEXTURES>;
    ActiveTextureMask mActiveTexturesMask;

    typedef std::vector<BindingPointer<Sampler>> SamplerBindingVector;
    SamplerBindingVector mSamplers;

    typedef std::vector<ImageUnit> ImageUnitVector;
    ImageUnitVector mImageUnits;

    typedef std::map<GLenum, BindingPointer<Query>> ActiveQueryMap;
    ActiveQueryMap mActiveQueries;

    // Stores the currently bound buffer for each binding point. It has entries for the element
    // array buffer and the transform feedback buffer but these should not be used. Instead these
    // bind points are respectively owned by current the vertex array object and the current
    // transform feedback object.
    using BoundBufferMap = angle::PackedEnumMap<BufferBinding, BindingPointer<Buffer>>;
    BoundBufferMap mBoundBuffers;

    using BufferVector = std::vector<OffsetBindingPointer<Buffer>>;
    BufferVector mUniformBuffers;
    BufferVector mAtomicCounterBuffers;
    BufferVector mShaderStorageBuffers;

    BindingPointer<TransformFeedback> mTransformFeedback;

    BindingPointer<Buffer> mPixelUnpackBuffer;
    PixelUnpackState mUnpack;
    BindingPointer<Buffer> mPixelPackBuffer;
    PixelPackState mPack;

    bool mPrimitiveRestart;

    Debug mDebug;

    bool mMultiSampling;
    bool mSampleAlphaToOne;

    GLenum mCoverageModulation;

    // CHROMIUM_path_rendering
    GLfloat mPathMatrixMV[16];
    GLfloat mPathMatrixProj[16];
    GLenum mPathStencilFunc;
    GLint mPathStencilRef;
    GLuint mPathStencilMask;

    // GL_EXT_sRGB_write_control
    bool mFramebufferSRGB;

    // GL_ANGLE_robust_resource_intialization
    bool mRobustResourceInit;

    // GL_ANGLE_program_cache_control
    bool mProgramBinaryCacheEnabled;

    DirtyBits mDirtyBits;
    DirtyObjects mDirtyObjects;
    mutable AttributesMask mDirtyCurrentValues;
};

}  // namespace gl

#endif  // LIBANGLE_STATE_H_
