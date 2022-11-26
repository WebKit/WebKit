//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// renderer_utils:
//   Helper methods pertaining to most or all back-ends.
//

#ifndef LIBANGLE_RENDERER_RENDERER_UTILS_H_
#define LIBANGLE_RENDERER_RENDERER_UTILS_H_

#include <cstdint>

#include <limits>
#include <map>

#include "GLSLANG/ShaderLang.h"
#include "common/angleutils.h"
#include "common/utilities.h"
#include "libANGLE/angletypes.h"

namespace angle
{
struct FeatureSetBase;
struct Format;
struct ImageLoadContext;
enum class FormatID;
}  // namespace angle

namespace gl
{
struct FormatType;
struct InternalFormat;
class State;
}  // namespace gl

namespace egl
{
class AttributeMap;
struct DisplayState;
}  // namespace egl

namespace rx
{
class ContextImpl;

// The possible rotations of the surface/draw framebuffer, particularly for the Vulkan back-end on
// Android.
enum class SurfaceRotation
{
    Identity,
    Rotated90Degrees,
    Rotated180Degrees,
    Rotated270Degrees,
    FlippedIdentity,
    FlippedRotated90Degrees,
    FlippedRotated180Degrees,
    FlippedRotated270Degrees,

    InvalidEnum,
    EnumCount = InvalidEnum,
};

bool IsRotatedAspectRatio(SurfaceRotation rotation);

using SpecConstUsageBits = angle::PackedEnumBitSet<sh::vk::SpecConstUsage, uint32_t>;

void RotateRectangle(const SurfaceRotation rotation,
                     const bool flipY,
                     const int framebufferWidth,
                     const int framebufferHeight,
                     const gl::Rectangle &incoming,
                     gl::Rectangle *outgoing);

using MipGenerationFunction = void (*)(size_t sourceWidth,
                                       size_t sourceHeight,
                                       size_t sourceDepth,
                                       const uint8_t *sourceData,
                                       size_t sourceRowPitch,
                                       size_t sourceDepthPitch,
                                       uint8_t *destData,
                                       size_t destRowPitch,
                                       size_t destDepthPitch);

typedef void (*PixelReadFunction)(const uint8_t *source, uint8_t *dest);
typedef void (*PixelWriteFunction)(const uint8_t *source, uint8_t *dest);
typedef void (*FastCopyFunction)(const uint8_t *source,
                                 int srcXAxisPitch,
                                 int srcYAxisPitch,
                                 uint8_t *dest,
                                 int destXAxisPitch,
                                 int destYAxisPitch,
                                 int width,
                                 int height);

class FastCopyFunctionMap
{
  public:
    struct Entry
    {
        angle::FormatID formatID;
        FastCopyFunction func;
    };

    constexpr FastCopyFunctionMap() : FastCopyFunctionMap(nullptr, 0) {}

    constexpr FastCopyFunctionMap(const Entry *data, size_t size) : mSize(size), mData(data) {}

    bool has(angle::FormatID formatID) const;
    FastCopyFunction get(angle::FormatID formatID) const;

  private:
    size_t mSize;
    const Entry *mData;
};

struct PackPixelsParams
{
    PackPixelsParams();
    PackPixelsParams(const gl::Rectangle &area,
                     const angle::Format &destFormat,
                     GLuint outputPitch,
                     bool reverseRowOrderIn,
                     gl::Buffer *packBufferIn,
                     ptrdiff_t offset);

    gl::Rectangle area;
    const angle::Format *destFormat;
    GLuint outputPitch;
    gl::Buffer *packBuffer;
    bool reverseRowOrder;
    ptrdiff_t offset;
    SurfaceRotation rotation;
};

void PackPixels(const PackPixelsParams &params,
                const angle::Format &sourceFormat,
                int inputPitch,
                const uint8_t *source,
                uint8_t *destination);

using InitializeTextureDataFunction = void (*)(size_t width,
                                               size_t height,
                                               size_t depth,
                                               uint8_t *output,
                                               size_t outputRowPitch,
                                               size_t outputDepthPitch);

using LoadImageFunction = void (*)(const angle::ImageLoadContext &context,
                                   size_t width,
                                   size_t height,
                                   size_t depth,
                                   const uint8_t *input,
                                   size_t inputRowPitch,
                                   size_t inputDepthPitch,
                                   uint8_t *output,
                                   size_t outputRowPitch,
                                   size_t outputDepthPitch);

struct LoadImageFunctionInfo
{
    LoadImageFunctionInfo() : loadFunction(nullptr), requiresConversion(false) {}
    LoadImageFunctionInfo(LoadImageFunction loadFunction, bool requiresConversion)
        : loadFunction(loadFunction), requiresConversion(requiresConversion)
    {}

    LoadImageFunction loadFunction;
    bool requiresConversion;
};

using LoadFunctionMap = LoadImageFunctionInfo (*)(GLenum);

bool ShouldUseDebugLayers(const egl::AttributeMap &attribs);

void CopyImageCHROMIUM(const uint8_t *sourceData,
                       size_t sourceRowPitch,
                       size_t sourcePixelBytes,
                       size_t sourceDepthPitch,
                       PixelReadFunction pixelReadFunction,
                       uint8_t *destData,
                       size_t destRowPitch,
                       size_t destPixelBytes,
                       size_t destDepthPitch,
                       PixelWriteFunction pixelWriteFunction,
                       GLenum destUnsizedFormat,
                       GLenum destComponentType,
                       size_t width,
                       size_t height,
                       size_t depth,
                       bool unpackFlipY,
                       bool unpackPremultiplyAlpha,
                       bool unpackUnmultiplyAlpha);

// Incomplete textures are 1x1 textures filled with black, used when samplers are incomplete.
// This helper class encapsulates handling incomplete textures. Because the GL back-end
// can take advantage of the driver's incomplete textures, and because clearing multisample
// textures is so difficult, we can keep an instance of this class in the back-end instead
// of moving the logic to the Context front-end.

// This interface allows us to call-back to init a multisample texture.
class MultisampleTextureInitializer
{
  public:
    virtual ~MultisampleTextureInitializer() {}
    virtual angle::Result initializeMultisampleTextureToBlack(const gl::Context *context,
                                                              gl::Texture *glTexture) = 0;
};

class IncompleteTextureSet final : angle::NonCopyable
{
  public:
    IncompleteTextureSet();
    ~IncompleteTextureSet();

    void onDestroy(const gl::Context *context);

    angle::Result getIncompleteTexture(const gl::Context *context,
                                       gl::TextureType type,
                                       gl::SamplerFormat format,
                                       MultisampleTextureInitializer *multisampleInitializer,
                                       gl::Texture **textureOut);

  private:
    using TextureMapWithSamplerFormat = angle::PackedEnumMap<gl::SamplerFormat, gl::TextureMap>;

    TextureMapWithSamplerFormat mIncompleteTextures;
    gl::Buffer *mIncompleteTextureBufferAttachment;
};

// Helpers to set a matrix uniform value based on GLSL or HLSL semantics.
// The return value indicate if the data was updated or not.
template <int cols, int rows>
struct SetFloatUniformMatrixGLSL
{
    static void Run(unsigned int arrayElementOffset,
                    unsigned int elementCount,
                    GLsizei countIn,
                    GLboolean transpose,
                    const GLfloat *value,
                    uint8_t *targetData);
};

template <int cols, int rows>
struct SetFloatUniformMatrixHLSL
{
    static void Run(unsigned int arrayElementOffset,
                    unsigned int elementCount,
                    GLsizei countIn,
                    GLboolean transpose,
                    const GLfloat *value,
                    uint8_t *targetData);
};

// Helper method to de-tranpose a matrix uniform for an API query.
void GetMatrixUniform(GLenum type, GLfloat *dataOut, const GLfloat *source, bool transpose);

template <typename NonFloatT>
void GetMatrixUniform(GLenum type, NonFloatT *dataOut, const NonFloatT *source, bool transpose);

const angle::Format &GetFormatFromFormatType(GLenum format, GLenum type);

angle::Result ComputeStartVertex(ContextImpl *contextImpl,
                                 const gl::IndexRange &indexRange,
                                 GLint baseVertex,
                                 GLint *firstVertexOut);

angle::Result GetVertexRangeInfo(const gl::Context *context,
                                 GLint firstVertex,
                                 GLsizei vertexOrIndexCount,
                                 gl::DrawElementsType indexTypeOrInvalid,
                                 const void *indices,
                                 GLint baseVertex,
                                 GLint *startVertexOut,
                                 size_t *vertexCountOut);

gl::Rectangle ClipRectToScissor(const gl::State &glState, const gl::Rectangle &rect, bool invertY);

// Helper method to intialize a FeatureSet with overrides from the DisplayState
void ApplyFeatureOverrides(angle::FeatureSetBase *features, const egl::DisplayState &state);

template <typename In>
uint32_t LineLoopRestartIndexCountHelper(GLsizei indexCount, const uint8_t *srcPtr)
{
    constexpr In restartIndex = gl::GetPrimitiveRestartIndexFromType<In>();
    const In *inIndices       = reinterpret_cast<const In *>(srcPtr);
    uint32_t numIndices       = 0;
    // See CopyLineLoopIndicesWithRestart() below for more info on how
    // numIndices is calculated.
    GLsizei loopStartIndex = 0;
    for (GLsizei curIndex = 0; curIndex < indexCount; curIndex++)
    {
        In vertex = inIndices[curIndex];
        if (vertex != restartIndex)
        {
            numIndices++;
        }
        else
        {
            if (curIndex > loopStartIndex)
            {
                numIndices += 2;
            }
            loopStartIndex = curIndex + 1;
        }
    }
    if (indexCount > loopStartIndex)
    {
        numIndices++;
    }
    return numIndices;
}

inline uint32_t GetLineLoopWithRestartIndexCount(gl::DrawElementsType glIndexType,
                                                 GLsizei indexCount,
                                                 const uint8_t *srcPtr)
{
    switch (glIndexType)
    {
        case gl::DrawElementsType::UnsignedByte:
            return LineLoopRestartIndexCountHelper<uint8_t>(indexCount, srcPtr);
        case gl::DrawElementsType::UnsignedShort:
            return LineLoopRestartIndexCountHelper<uint16_t>(indexCount, srcPtr);
        case gl::DrawElementsType::UnsignedInt:
            return LineLoopRestartIndexCountHelper<uint32_t>(indexCount, srcPtr);
        default:
            UNREACHABLE();
            return 0;
    }
}

// Writes the line-strip vertices for a line loop to outPtr,
// where outLimit is calculated as in GetPrimitiveRestartIndexCount.
template <typename In, typename Out>
void CopyLineLoopIndicesWithRestart(GLsizei indexCount, const uint8_t *srcPtr, uint8_t *outPtr)
{
    constexpr In restartIndex     = gl::GetPrimitiveRestartIndexFromType<In>();
    constexpr Out outRestartIndex = gl::GetPrimitiveRestartIndexFromType<Out>();
    const In *inIndices           = reinterpret_cast<const In *>(srcPtr);
    Out *outIndices               = reinterpret_cast<Out *>(outPtr);
    GLsizei loopStartIndex        = 0;
    for (GLsizei curIndex = 0; curIndex < indexCount; curIndex++)
    {
        In vertex = inIndices[curIndex];
        if (vertex != restartIndex)
        {
            *(outIndices++) = static_cast<Out>(vertex);
        }
        else
        {
            if (curIndex > loopStartIndex)
            {
                // Emit an extra vertex only if the loop is not empty.
                *(outIndices++) = inIndices[loopStartIndex];
                // Then restart the strip.
                *(outIndices++) = outRestartIndex;
            }
            loopStartIndex = curIndex + 1;
        }
    }
    if (indexCount > loopStartIndex)
    {
        // Close the last loop if not empty.
        *(outIndices++) = inIndices[loopStartIndex];
    }
}

void GetSamplePosition(GLsizei sampleCount, size_t index, GLfloat *xy);

angle::Result MultiDrawArraysGeneral(ContextImpl *contextImpl,
                                     const gl::Context *context,
                                     gl::PrimitiveMode mode,
                                     const GLint *firsts,
                                     const GLsizei *counts,
                                     GLsizei drawcount);
angle::Result MultiDrawArraysIndirectGeneral(ContextImpl *contextImpl,
                                             const gl::Context *context,
                                             gl::PrimitiveMode mode,
                                             const void *indirect,
                                             GLsizei drawcount,
                                             GLsizei stride);
angle::Result MultiDrawArraysInstancedGeneral(ContextImpl *contextImpl,
                                              const gl::Context *context,
                                              gl::PrimitiveMode mode,
                                              const GLint *firsts,
                                              const GLsizei *counts,
                                              const GLsizei *instanceCounts,
                                              GLsizei drawcount);
angle::Result MultiDrawElementsGeneral(ContextImpl *contextImpl,
                                       const gl::Context *context,
                                       gl::PrimitiveMode mode,
                                       const GLsizei *counts,
                                       gl::DrawElementsType type,
                                       const GLvoid *const *indices,
                                       GLsizei drawcount);
angle::Result MultiDrawElementsIndirectGeneral(ContextImpl *contextImpl,
                                               const gl::Context *context,
                                               gl::PrimitiveMode mode,
                                               gl::DrawElementsType type,
                                               const void *indirect,
                                               GLsizei drawcount,
                                               GLsizei stride);
angle::Result MultiDrawElementsInstancedGeneral(ContextImpl *contextImpl,
                                                const gl::Context *context,
                                                gl::PrimitiveMode mode,
                                                const GLsizei *counts,
                                                gl::DrawElementsType type,
                                                const GLvoid *const *indices,
                                                const GLsizei *instanceCounts,
                                                GLsizei drawcount);
angle::Result MultiDrawArraysInstancedBaseInstanceGeneral(ContextImpl *contextImpl,
                                                          const gl::Context *context,
                                                          gl::PrimitiveMode mode,
                                                          const GLint *firsts,
                                                          const GLsizei *counts,
                                                          const GLsizei *instanceCounts,
                                                          const GLuint *baseInstances,
                                                          GLsizei drawcount);
angle::Result MultiDrawElementsInstancedBaseVertexBaseInstanceGeneral(ContextImpl *contextImpl,
                                                                      const gl::Context *context,
                                                                      gl::PrimitiveMode mode,
                                                                      const GLsizei *counts,
                                                                      gl::DrawElementsType type,
                                                                      const GLvoid *const *indices,
                                                                      const GLsizei *instanceCounts,
                                                                      const GLint *baseVertices,
                                                                      const GLuint *baseInstances,
                                                                      GLsizei drawcount);

// RAII object making sure reset uniforms is called no matter whether there's an error in draw calls
class ResetBaseVertexBaseInstance : angle::NonCopyable
{
  public:
    ResetBaseVertexBaseInstance(gl::Program *programObject,
                                bool resetBaseVertex,
                                bool resetBaseInstance);

    ~ResetBaseVertexBaseInstance();

  private:
    gl::Program *mProgramObject;
    bool mResetBaseVertex;
    bool mResetBaseInstance;
};

angle::FormatID ConvertToSRGB(angle::FormatID formatID);
angle::FormatID ConvertToLinear(angle::FormatID formatID);
bool IsOverridableLinearFormat(angle::FormatID formatID);

enum class PipelineType
{
    Graphics = 0,
    Compute  = 1,

    InvalidEnum = 2,
    EnumCount   = 2,
};
}  // namespace rx

// MultiDraw macro patterns
// These macros are to avoid too much code duplication as we don't want to have if detect for
// hasDrawID/BaseVertex/BaseInstance inside for loop in a multiDrawANGLE call Part of these are put
// in the header as we want to share with specialized context impl on some platforms for multidraw
#define ANGLE_SET_DRAW_ID_UNIFORM_0(drawID) \
    {}
#define ANGLE_SET_DRAW_ID_UNIFORM_1(drawID) programObject->setDrawIDUniform(drawID)
#define ANGLE_SET_DRAW_ID_UNIFORM(cond) ANGLE_SET_DRAW_ID_UNIFORM_##cond

#define ANGLE_SET_BASE_VERTEX_UNIFORM_0(baseVertex) \
    {}
#define ANGLE_SET_BASE_VERTEX_UNIFORM_1(baseVertex) programObject->setBaseVertexUniform(baseVertex);
#define ANGLE_SET_BASE_VERTEX_UNIFORM(cond) ANGLE_SET_BASE_VERTEX_UNIFORM_##cond

#define ANGLE_SET_BASE_INSTANCE_UNIFORM_0(baseInstance) \
    {}
#define ANGLE_SET_BASE_INSTANCE_UNIFORM_1(baseInstance) \
    programObject->setBaseInstanceUniform(baseInstance)
#define ANGLE_SET_BASE_INSTANCE_UNIFORM(cond) ANGLE_SET_BASE_INSTANCE_UNIFORM_##cond

#define ANGLE_NOOP_DRAW_ context->noopDraw(mode, counts[drawID])
#define ANGLE_NOOP_DRAW_INSTANCED \
    context->noopDrawInstanced(mode, counts[drawID], instanceCounts[drawID])
#define ANGLE_NOOP_DRAW(_instanced) ANGLE_NOOP_DRAW##_instanced

#define ANGLE_MARK_TRANSFORM_FEEDBACK_USAGE_ \
    gl::MarkTransformFeedbackBufferUsage(context, counts[drawID], 1)
#define ANGLE_MARK_TRANSFORM_FEEDBACK_USAGE_INSTANCED \
    gl::MarkTransformFeedbackBufferUsage(context, counts[drawID], instanceCounts[drawID])
#define ANGLE_MARK_TRANSFORM_FEEDBACK_USAGE(instanced) \
    ANGLE_MARK_TRANSFORM_FEEDBACK_USAGE##instanced

// Helper macro that casts to a bitfield type then verifies no bits were dropped.
#define SetBitField(lhs, rhs)                                                         \
    do                                                                                \
    {                                                                                 \
        auto ANGLE_LOCAL_VAR = rhs;                                                   \
        lhs = static_cast<typename std::decay<decltype(lhs)>::type>(ANGLE_LOCAL_VAR); \
        ASSERT(static_cast<decltype(ANGLE_LOCAL_VAR)>(lhs) == ANGLE_LOCAL_VAR);       \
    } while (0)

#endif  // LIBANGLE_RENDERER_RENDERER_UTILS_H_
