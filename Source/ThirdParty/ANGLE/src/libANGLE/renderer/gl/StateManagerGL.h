//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// StateManagerGL.h: Defines a class for caching applied OpenGL state

#ifndef LIBANGLE_RENDERER_GL_STATEMANAGERGL_H_
#define LIBANGLE_RENDERER_GL_STATEMANAGERGL_H_

#include "common/debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/State.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/gl/ProgramExecutableGL.h"
#include "libANGLE/renderer/gl/functionsgl_typedefs.h"
#include "platform/autogen/FeaturesGL_autogen.h"

#include <array>
#include <map>

namespace gl
{
struct Caps;
class FramebufferState;
class State;
}  // namespace gl

namespace rx
{

class FramebufferGL;
class FunctionsGL;
class TransformFeedbackGL;
class VertexArrayGL;
class QueryGL;

struct ExternalContextVertexAttribute
{
    bool enabled;
    const angle::Format *format;
    GLuint stride;
    GLvoid *pointer;
    GLuint buffer;
    gl::VertexAttribCurrentValueData currentData;
};

// TODO(penghuang): use gl::State?
struct ExternalContextState
{
    GLint packAlignment;
    GLint unpackAlignment;

    GLenum vertexArrayBufferBinding;
    GLenum elementArrayBufferBinding;

    bool depthTest;
    bool cullFace;
    GLenum cullFaceMode;
    std::array<bool, 4> colorMask;
    gl::ColorF colorClear;
    gl::ColorF blendColor;
    GLfloat depthClear;
    GLenum currentProgram;
    GLenum depthFunc;
    bool depthMask;
    GLfloat depthRage[2];
    GLenum frontFace;
    GLfloat lineWidth;
    GLfloat polygonOffsetFactor;
    GLfloat polygonOffsetUnits;
    GLfloat polygonOffsetClamp;
    GLfloat sampleCoverageValue;
    bool sampleCoverageInvert;
    GLenum blendEquationRgb;
    GLenum blendEquationAlpha;
    bool enableBlendEquationAdvancedCoherent;

    bool enableDither;
    GLenum polygonMode;
    bool enablePolygonOffsetPoint;
    bool enablePolygonOffsetLine;
    bool enablePolygonOffsetFill;
    bool enableDepthClamp;
    bool enableSampleAlphaToCoverage;
    bool enableSampleCoverage;
    bool multisampleEnabled;

    bool blendEnabled;
    GLenum blendSrcRgb;
    GLenum blendSrcAlpha;
    GLenum blendDestRgb;
    GLenum blendDestAlpha;
    GLenum activeTexture;
    gl::Rectangle viewport;
    GLenum clipOrigin;
    GLenum clipDepthMode;
    bool scissorTest;
    gl::Rectangle scissorBox;

    struct StencilState
    {
        bool stencilTestEnabled;
        GLenum stencilFrontFunc;
        GLint stencilFrontRef;
        GLenum stencilFrontMask;
        GLenum stencilBackFunc;
        GLint stencilBackRef;
        GLenum stencilBackMask;
        GLint stencilClear;
        GLenum stencilFrontWritemask;
        GLenum stencilBackWritemask;
        GLenum stencilFrontFailOp;
        GLenum stencilFrontZFailOp;
        GLenum stencilFrontZPassOp;
        GLenum stencilBackFailOp;
        GLenum stencilBackZFailOp;
        GLenum stencilBackZPassOp;
    };
    StencilState stencilState;

    GLenum framebufferBinding;

    struct TextureBindings
    {
        GLenum texture2d;
        GLenum textureCubeMap;
        GLenum textureExternalOES;
        // TODO(boliu): TEXTURE_RECTANGLE_ARB
    };
    std::vector<TextureBindings> textureBindings;

    GLenum vertexArrayBinding;

    angle::FixedVector<ExternalContextVertexAttribute, gl::MAX_VERTEX_ATTRIBS>
        defaultVertexArrayAttributes;
};

struct VertexAttributeGL
{
    bool enabled                = false;
    const angle::Format *format = &angle::Format::Get(angle::FormatID::R32G32B32A32_FLOAT);

    const void *pointer   = nullptr;
    GLuint relativeOffset = 0;

    GLuint bindingIndex = 0;
};

struct VertexBindingGL
{
    GLuint stride   = 16;
    GLuint divisor  = 0;
    uintptr_t offset = 0;

    GLuint buffer = 0;
};

struct VertexArrayStateGL
{
    VertexArrayStateGL(size_t maxAttribs, size_t maxBindings);

    GLuint elementArrayBuffer = 0;

    angle::FixedVector<VertexAttributeGL, gl::MAX_VERTEX_ATTRIBS> attributes;
    angle::FixedVector<VertexBindingGL, gl::MAX_VERTEX_ATTRIBS> bindings;
};

struct IndexedBufferBindingGL
{
    size_t offset = 0;
    size_t size   = 0;
    GLuint buffer = 0;
};

struct ImageUnitBindingGL
{
    GLuint texture    = 0;
    GLint level       = 0;
    GLboolean layered = false;
    GLint layer       = 0;
    GLenum access     = GL_READ_ONLY;
    GLenum format     = GL_R32UI;
};

struct ContextStateGL
{
    ContextStateGL(const gl::Caps &caps, const gl::Extensions &extensions);

    GLuint program = 0;

    GLuint vao = 0;
    std::vector<gl::VertexAttribCurrentValueData> vertexAttribCurrentValues;

    angle::PackedEnumMap<gl::BufferBinding, GLuint> buffers = {};
    angle::PackedEnumMap<gl::BufferBinding, std::vector<IndexedBufferBindingGL>> indexedBuffers;

    size_t textureUnitIndex                                                        = 0;
    angle::PackedEnumMap<gl::TextureType, gl::ActiveTextureArray<GLuint>> textures = {};
    gl::ActiveTextureArray<GLuint> samplers                                        = {};

    std::vector<ImageUnitBindingGL> images;

    GLuint transformFeedback = 0;

    GLint unpackAlignment   = 4;
    GLint unpackRowLength   = 0;
    GLint unpackSkipRows    = 0;
    GLint unpackSkipPixels  = 0;
    GLint unpackImageHeight = 0;
    GLint unpackSkipImages  = 0;

    GLint packAlignment  = 4;
    GLint packRowLength  = 0;
    GLint packSkipRows   = 0;
    GLint packSkipPixels = 0;

    std::array<GLuint, angle::FramebufferBinding::FramebufferBindingSingletonMax> framebuffers = {
        0};
    GLuint renderbuffer = 0;

    bool scissorTestEnabled = false;
    gl::Rectangle scissor   = gl::Rectangle(0, 0, 0, 0);
    gl::Rectangle viewport  = gl::Rectangle(0, 0, 0, 0);
    float near              = 0.0f;
    float far               = 1.0f;

    gl::ClipOrigin clipOrigin       = gl::ClipOrigin::LowerLeft;
    gl::ClipDepthMode clipDepthMode = gl::ClipDepthMode::NegativeOneToOne;

    gl::ColorF blendColor = gl::ColorF(0, 0, 0, 0);
    gl::BlendStateExt blendState;
    bool blendAdvancedCoherent = true;

    bool sampleAlphaToCoverageEnabled = false;
    bool sampleCoverageEnabled        = false;
    float sampleCoverageValue         = 1.0f;
    bool sampleCoverageInvert         = false;
    bool sampleMaskEnabled            = false;
    gl::SampleMaskArray<GLbitfield> sampleMaskValues;

    bool depthTestEnabled                     = false;
    GLenum depthFunc                          = GL_LESS;
    bool depthMask                            = true;
    bool stencilTestEnabled                   = false;
    GLenum stencilFrontFunc                   = GL_ALWAYS;
    GLint stencilFrontRef                     = 0;
    GLuint stencilFrontValueMask              = static_cast<GLuint>(-1);
    GLenum stencilFrontStencilFailOp          = GL_KEEP;
    GLenum stencilFrontStencilPassDepthFailOp = GL_KEEP;
    GLenum stencilFrontStencilPassDepthPassOp = GL_KEEP;
    GLuint stencilFrontWritemask              = static_cast<GLuint>(-1);
    GLenum stencilBackFunc                    = GL_ALWAYS;
    GLint stencilBackRef                      = 0;
    GLuint stencilBackValueMask               = static_cast<GLuint>(-1);
    GLenum stencilBackStencilFailOp           = GL_KEEP;
    GLenum stencilBackStencilPassDepthFailOp  = GL_KEEP;
    GLenum stencilBackStencilPassDepthPassOp  = GL_KEEP;
    GLuint stencilBackWritemask               = static_cast<GLuint>(-1);

    bool cullFaceEnabled           = false;
    gl::CullFaceMode cullFace      = gl::CullFaceMode::Back;
    GLenum frontFace               = GL_CCW;
    gl::PolygonMode polygonMode    = gl::PolygonMode::Fill;
    bool polygonOffsetPointEnabled = false;
    bool polygonOffsetLineEnabled  = false;
    bool polygonOffsetFillEnabled  = false;
    GLfloat polygonOffsetFactor    = 0.0f;
    GLfloat polygonOffsetUnits     = 0.0f;
    GLfloat polygonOffsetClamp     = 0.0f;
    bool depthClampEnabled         = false;
    bool rasterizerDiscardEnabled  = false;
    float lineWidth                = 1.0f;

    bool primitiveRestartEnabled = false;
    GLuint primitiveRestartIndex = 0;

    gl::ColorF clearColor = gl::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
    float clearDepth      = 1.0f;
    GLint clearStencil    = 0;

    bool framebufferSRGBEnabled = false;

    bool ditherEnabled                 = true;
    bool textureCubemapSeamlessEnabled = false;

    bool multisamplingEnabled    = true;
    bool sampleAlphaToOneEnabled = false;

    GLenum coverageModulation = GL_NONE;

    GLenum provokingVertex = GL_LAST_VERTEX_CONVENTION;

    gl::ClipDistanceEnableBits enabledClipDistances;

    bool logicOpEnabled          = false;
    gl::LogicalOperation logicOp = gl::LogicalOperation::Copy;
};

class StateManagerGL final : angle::NonCopyable
{
  public:
    StateManagerGL(const FunctionsGL *functions,
                   const gl::Caps &rendererCaps,
                   const gl::Extensions &extensions,
                   const angle::FeaturesGL &features);
    ~StateManagerGL();

    void deleteProgram(GLuint program);
    void deleteVertexArray(GLuint vao);
    void deleteTexture(GLuint texture);
    void deleteSampler(GLuint sampler);
    void deleteBuffer(GLuint buffer);
    void deleteFramebuffer(GLuint fbo);
    void deleteRenderbuffer(GLuint rbo);
    void deleteTransformFeedback(GLuint transformFeedback);

    void useProgram(GLuint program);
    void forceUseProgram(GLuint program);
    void bindVertexArray(GLuint vao, VertexArrayStateGL *vaoState);
    void bindBuffer(gl::BufferBinding target, GLuint buffer);
    void bindBufferBase(gl::BufferBinding target, size_t index, GLuint buffer);
    void bindBufferRange(gl::BufferBinding target,
                         size_t index,
                         GLuint buffer,
                         size_t offset,
                         size_t size);
    void activeTexture(size_t unit);
    void bindTexture(gl::TextureType type, GLuint texture);
    void bindSampler(size_t unit, GLuint sampler);
    void bindImageTexture(size_t unit,
                          GLuint texture,
                          GLint level,
                          GLboolean layered,
                          GLint layer,
                          GLenum access,
                          GLenum format);
    void bindFramebuffer(GLenum type, GLuint framebuffer);
    void bindRenderbuffer(GLenum type, GLuint renderbuffer);
    void bindTransformFeedback(GLenum type, GLuint transformFeedback);
    void onTransformFeedbackStateChange();
    void beginQuery(gl::QueryType type, QueryGL *queryObject, GLuint queryId);
    void endQuery(gl::QueryType type, QueryGL *queryObject, GLuint queryId);

    void setAttributeCurrentData(size_t index, const gl::VertexAttribCurrentValueData &data);

    void setScissorTestEnabled(bool enabled);
    void setScissor(const gl::Rectangle &scissor);

    void setViewport(const gl::Rectangle &viewport);
    void setDepthRange(float near, float far);
    void setClipControl(gl::ClipOrigin origin, gl::ClipDepthMode depth);

    void setBlendEnabled(bool enabled);
    void setBlendEnabledIndexed(const gl::DrawBufferMask blendEnabledMask);
    void setBlendColor(const gl::ColorF &blendColor);
    void setBlendFuncs(const gl::BlendStateExt &blendStateExt);
    void setBlendEquations(const gl::BlendStateExt &blendStateExt);
    void setBlendAdvancedCoherent(bool enabled);
    void setColorMask(bool red, bool green, bool blue, bool alpha);
    void setSampleAlphaToCoverageEnabled(bool enabled);
    void setSampleCoverageEnabled(bool enabled);
    void setSampleCoverage(float value, bool invert);
    void forceSetSampleCoverage(float value, bool invert);
    void setSampleMaskEnabled(bool enabled);
    void setSampleMaski(GLuint maskNumber, GLbitfield mask);

    void setDepthTestEnabled(bool enabled);
    void setDepthFunc(GLenum depthFunc);
    void setDepthMask(bool mask);
    void setStencilTestEnabled(bool enabled);
    void setStencilFrontWritemask(GLuint mask);
    void setStencilBackWritemask(GLuint mask);
    void setStencilFrontFuncs(GLenum func, GLint ref, GLuint mask);
    void setStencilBackFuncs(GLenum func, GLint ref, GLuint mask);
    void setStencilFrontOps(GLenum sfail, GLenum dpfail, GLenum dppass);
    void setStencilBackOps(GLenum sfail, GLenum dpfail, GLenum dppass);

    void setCullFaceEnabled(bool enabled);
    void setCullFace(gl::CullFaceMode cullFace);
    void setFrontFace(GLenum frontFace);
    void setPolygonMode(gl::PolygonMode mode);
    void setPolygonOffsetPointEnabled(bool enabled);
    void setPolygonOffsetLineEnabled(bool enabled);
    void setPolygonOffsetFillEnabled(bool enabled);
    void setPolygonOffset(float factor, float units, float clamp);
    void setDepthClampEnabled(bool enabled);
    void setRasterizerDiscardEnabled(bool enabled);
    void setLineWidth(float width);

    angle::Result setPrimitiveRestartEnabled(const gl::Context *context, bool enabled);
    angle::Result setPrimitiveRestartIndex(const gl::Context *context, GLuint index);

    void setClearColor(const gl::ColorF &clearColor);
    void setClearDepth(float clearDepth);
    void setClearStencil(GLint clearStencil);

    angle::Result setPixelUnpackState(const gl::Context *context,
                                      const gl::PixelUnpackState &unpack);
    angle::Result setPixelUnpackBuffer(const gl::Context *context, const gl::Buffer *pixelBuffer);
    angle::Result setPixelPackState(const gl::Context *context, const gl::PixelPackState &pack);
    angle::Result setPixelPackBuffer(const gl::Context *context, const gl::Buffer *pixelBuffer);

    void setFramebufferSRGBEnabled(const gl::Context *context, bool enabled);
    void setFramebufferSRGBEnabledForFramebuffer(const gl::Context *context,
                                                 bool enabled,
                                                 const FramebufferGL *framebuffer);
    void setColorMaskForFramebuffer(const gl::BlendStateExt &blendStateExt,
                                    const bool disableAlpha);

    void setDitherEnabled(bool enabled);

    void setMultisamplingStateEnabled(bool enabled);
    void setSampleAlphaToOneStateEnabled(bool enabled);

    void setCoverageModulation(GLenum components);

    void setProvokingVertex(GLenum mode);

    void setClipDistancesEnable(const gl::ClipDistanceEnableBits &enables);

    void setLogicOpEnabled(bool enabled);
    void setLogicOp(gl::LogicalOperation opcode);

    void pauseTransformFeedback();
    angle::Result pauseAllQueries(const gl::Context *context);
    angle::Result pauseQuery(const gl::Context *context, gl::QueryType type);
    angle::Result resumeAllQueries(const gl::Context *context);
    angle::Result resumeQuery(const gl::Context *context, gl::QueryType type);
    angle::Result onMakeCurrent(const gl::Context *context);

    angle::Result syncState(const gl::Context *context,
                            const gl::state::DirtyBits &glDirtyBits,
                            const gl::state::DirtyBits &bitMask,
                            const gl::state::ExtendedDirtyBits &extendedDirtyBits,
                            const gl::state::ExtendedDirtyBits &extendedBitMask);

    ANGLE_INLINE void updateMultiviewBaseViewLayerIndexUniform(
        const gl::ProgramExecutable *executable,
        const gl::FramebufferState &drawFramebufferState) const
    {
        if (mIsMultiviewEnabled && executable && executable->usesMultiview())
        {
            updateMultiviewBaseViewLayerIndexUniformImpl(executable, drawFramebufferState);
        }
    }

    ANGLE_INLINE void updateEmulatedClipOriginUniform(const gl::ProgramExecutable *executable,
                                                      const gl::ClipOrigin origin) const
    {
        ASSERT(executable);
        GetImplAs<ProgramExecutableGL>(executable)->updateEmulatedClipOrigin(origin);
    }

    GLuint getProgramID() const { return mState.program; }
    GLuint getVertexArrayID() const { return mState.vao; }
    GLuint getFramebufferID(angle::FramebufferBinding binding) const
    {
        return mState.framebuffers[binding];
    }
    GLuint getBufferID(gl::BufferBinding binding) const { return mState.buffers[binding]; }

    bool getHasSeparateFramebufferBindings() const { return mHasSeparateFramebufferBindings; }

    GLuint getDefaultVAO() const;
    VertexArrayStateGL *getDefaultVAOState();
    void setDefaultVAOStateDirty();

    void validateState() const;

    void syncFromNativeContext(const gl::Extensions &extensions, ExternalContextState *state);
    void restoreNativeContext(const gl::Extensions &extensions, const ExternalContextState *state);

  private:
    void forceBindVertexArray(GLuint vao, VertexArrayStateGL *vaoState);

    void setTextureCubemapSeamlessEnabled(bool enabled);

    void setClipControlWithEmulatedClipOrigin(const gl::ProgramExecutable *executable,
                                              GLenum frontFace,
                                              gl::ClipOrigin origin,
                                              gl::ClipDepthMode depth);

    angle::Result propagateProgramToVAO(const gl::Context *context,
                                        const gl::ProgramExecutable *executable,
                                        VertexArrayGL *vao);

    void updateProgramTextureBindings(const gl::Context *context);
    void updateProgramStorageBufferBindings(const gl::Context *context);
    void updateProgramUniformBufferBindings(const gl::Context *context);
    void updateProgramAtomicCounterBufferBindings(const gl::Context *context);
    void updateProgramImageBindings(const gl::Context *context);

    void updateDispatchIndirectBufferBinding(const gl::Context *context);
    void updateDrawIndirectBufferBinding(const gl::Context *context);

    void setBufferBindingDirty(gl::BufferBinding binding);

    template <typename T>
    void get(GLenum name, T *value);

    template <size_t n, typename T>
    void get(GLenum name, std::array<T, n> *values);

    void syncSamplersState(const gl::Context *context);
    void syncTransformFeedbackState(const gl::Context *context);
    void syncProgramState(const gl::Context *context);

    void updateEmulatedClipDistanceState(const gl::ProgramExecutable *executable,
                                         const gl::ClipDistanceEnableBits enables) const;

    void updateMultiviewBaseViewLayerIndexUniformImpl(
        const gl::ProgramExecutable *executable,
        const gl::FramebufferState &drawFramebufferState) const;

    void syncBlendFromNativeContext(const gl::Extensions &extensions, ExternalContextState *state);
    void restoreBlendNativeContext(const gl::Extensions &extensions,
                                   const ExternalContextState *state);

    void syncFramebufferFromNativeContext(const gl::Extensions &extensions,
                                          ExternalContextState *state);
    void restoreFramebufferNativeContext(const gl::Extensions &extensions,
                                         const ExternalContextState *state);

    void syncPixelPackUnpackFromNativeContext(const gl::Extensions &extensions,
                                              ExternalContextState *state);
    void restorePixelPackUnpackNativeContext(const gl::Extensions &extensions,
                                             const ExternalContextState *state);

    void syncStencilFromNativeContext(const gl::Extensions &extensions,
                                      ExternalContextState *state);
    void restoreStencilNativeContext(const gl::Extensions &extensions,
                                     const ExternalContextState *state);

    void syncBufferBindingsFromNativeContext(const gl::Extensions &extensions,
                                             ExternalContextState *state);
    void restoreBufferBindingsNativeContext(const gl::Extensions &extensions,
                                            const ExternalContextState *state);

    void syncTextureUnitsFromNativeContext(const gl::Extensions &extensions,
                                           ExternalContextState *state);
    void restoreTextureUnitsNativeContext(const gl::Extensions &extensions,
                                          const ExternalContextState *state);

    void syncVertexArraysFromNativeContext(const gl::Extensions &extensions,
                                           ExternalContextState *state);
    void restoreVertexArraysNativeContext(const gl::Extensions &extensions,
                                          const ExternalContextState *state);

    const FunctionsGL *mFunctions;
    const angle::FeaturesGL &mFeatures;

    ContextStateGL mState;

    const bool mSupportsVertexArrayObjects;

    GLuint mDefaultVAO = 0;
    // The current state of the default VAO is owned by StateManagerGL. It may be shared between
    // multiple VertexArrayGL objects if the native driver does not support vertex array objects.
    // When this object is shared, StateManagerGL forces VertexArrayGL to resynchronize itself every
    // time a new vertex array is bound.
    VertexArrayStateGL mDefaultVAOState;

    // The state of the currently bound vertex array object so StateManagerGL can know about the
    // current element array buffer.
    VertexArrayStateGL *mVAOState = nullptr;

    TransformFeedbackGL *mCurrentTransformFeedback;

    // Queries that are currently running on the driver
    angle::PackedEnumMap<gl::QueryType, QueryGL *> mQueries;

    // Queries that are temporarily in the paused state so that their results will not be affected
    // by other operations
    angle::PackedEnumMap<gl::QueryType, QueryGL *> mTemporaryPausedQueries;

    gl::ContextID mPrevDrawContext;

    GLuint mPlaceholderFbo;
    GLuint mPlaceholderRbo;

    const bool mIndependentBlendStates;

    bool mSampleCoverageEverChanged;

    const bool mFramebufferSRGBAvailable;
    const bool mHasSeparateFramebufferBindings;

    const bool mIsMultiviewEnabled;

    const size_t mMaxClipDistances;

    gl::state::DirtyBits mLocalDirtyBits;
    gl::state::ExtendedDirtyBits mLocalExtendedDirtyBits;
    gl::AttributesMask mLocalDirtyCurrentValues;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_STATEMANAGERGL_H_
