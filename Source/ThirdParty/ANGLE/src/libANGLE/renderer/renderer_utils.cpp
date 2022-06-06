//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// renderer_utils:
//   Helper methods pertaining to most or all back-ends.
//

#include "libANGLE/renderer/renderer_utils.h"

#include "common/string_utils.h"
#include "common/system_utils.h"
#include "common/utilities.h"
#include "image_util/copyimage.h"
#include "image_util/imageformats.h"
#include "libANGLE/AttributeMap.h"
#include "libANGLE/Context.h"
#include "libANGLE/Context.inl.h"
#include "libANGLE/Display.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/Format.h"
#include "platform/Feature.h"

#include <string.h>
#include <cctype>

namespace angle
{
namespace
{
// For the sake of feature name matching, underscore is ignored, and the names are matched
// case-insensitive.  This allows feature names to be overriden both in snake_case (previously used
// by ANGLE) and camelCase.
bool FeatureNameMatch(const std::string &a, const std::string &b)
{
    size_t ai = 0;
    size_t bi = 0;

    while (ai < a.size() && bi < b.size())
    {
        if (a[ai] == '_')
        {
            ++ai;
        }
        if (b[bi] == '_')
        {
            ++bi;
        }
        if (std::tolower(a[ai++]) != std::tolower(b[bi++]))
        {
            return false;
        }
    }

    return ai == a.size() && bi == b.size();
}

// Search for a feature by name, matching it loosely so that both snake_case and camelCase names are
// matched.
FeatureInfo *FindFeatureByName(FeatureMap *features, const std::string &name)
{
    for (auto iter : *features)
    {
        if (FeatureNameMatch(iter.first, name))
        {
            return iter.second;
        }
    }
    return nullptr;
}
}  // anonymous namespace

// FeatureSetBase implementation
void FeatureSetBase::overrideFeatures(const std::vector<std::string> &featureNames, bool enabled)
{
    for (const std::string &name : featureNames)
    {
        FeatureInfo *feature = FindFeatureByName(&members, name);
        if (feature != nullptr)
        {
            feature->enabled = enabled;
        }
    }
}

void FeatureSetBase::populateFeatureList(FeatureList *features) const
{
    for (FeatureMap::const_iterator it = members.begin(); it != members.end(); it++)
    {
        features->push_back(it->second);
    }
}
}  // namespace angle

namespace rx
{

namespace
{
// Both D3D and Vulkan support the same set of standard sample positions for 1, 2, 4, 8, and 16
// samples.  See:
//
// - https://msdn.microsoft.com/en-us/library/windows/desktop/ff476218.aspx
//
// -
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#primsrast-multisampling
using SamplePositionsArray                                     = std::array<float, 32>;
constexpr std::array<SamplePositionsArray, 5> kSamplePositions = {
    {{{0.5f, 0.5f}},
     {{0.75f, 0.75f, 0.25f, 0.25f}},
     {{0.375f, 0.125f, 0.875f, 0.375f, 0.125f, 0.625f, 0.625f, 0.875f}},
     {{0.5625f, 0.3125f, 0.4375f, 0.6875f, 0.8125f, 0.5625f, 0.3125f, 0.1875f, 0.1875f, 0.8125f,
       0.0625f, 0.4375f, 0.6875f, 0.9375f, 0.9375f, 0.0625f}},
     {{0.5625f, 0.5625f, 0.4375f, 0.3125f, 0.3125f, 0.625f,  0.75f,   0.4375f,
       0.1875f, 0.375f,  0.625f,  0.8125f, 0.8125f, 0.6875f, 0.6875f, 0.1875f,
       0.375f,  0.875f,  0.5f,    0.0625f, 0.25f,   0.125f,  0.125f,  0.75f,
       0.0f,    0.5f,    0.9375f, 0.25f,   0.875f,  0.9375f, 0.0625f, 0.0f}}}};

struct IncompleteTextureParameters
{
    GLenum sizedInternalFormat;
    GLenum format;
    GLenum type;
    GLubyte clearColor[4];
};

// Note that for gl::SamplerFormat::Shadow, the clearColor datatype needs to be GLushort and as such
// we will reinterpret GLubyte[4] as GLushort[2].
constexpr angle::PackedEnumMap<gl::SamplerFormat, IncompleteTextureParameters>
    kIncompleteTextureParameters = {
        {gl::SamplerFormat::Float, {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, {0, 0, 0, 255}}},
        {gl::SamplerFormat::Unsigned,
         {GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, {0, 0, 0, 255}}},
        {gl::SamplerFormat::Signed, {GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, {0, 0, 0, 127}}},
        {gl::SamplerFormat::Shadow,
         {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, {0, 0, 0, 0}}}};

void CopyColor(gl::ColorF *color)
{
    // No-op
}

void PremultiplyAlpha(gl::ColorF *color)
{
    color->red *= color->alpha;
    color->green *= color->alpha;
    color->blue *= color->alpha;
}

void UnmultiplyAlpha(gl::ColorF *color)
{
    if (color->alpha != 0.0f)
    {
        float invAlpha = 1.0f / color->alpha;
        color->red *= invAlpha;
        color->green *= invAlpha;
        color->blue *= invAlpha;
    }
}

void ClipChannelsR(gl::ColorF *color)
{
    color->green = 0.0f;
    color->blue  = 0.0f;
    color->alpha = 1.0f;
}

void ClipChannelsRG(gl::ColorF *color)
{
    color->blue  = 0.0f;
    color->alpha = 1.0f;
}

void ClipChannelsRGB(gl::ColorF *color)
{
    color->alpha = 1.0f;
}

void ClipChannelsLuminance(gl::ColorF *color)
{
    color->alpha = 1.0f;
}

void ClipChannelsAlpha(gl::ColorF *color)
{
    color->red   = 0.0f;
    color->green = 0.0f;
    color->blue  = 0.0f;
}

void ClipChannelsNoOp(gl::ColorF *color) {}

void WriteUintColor(const gl::ColorF &color,
                    PixelWriteFunction colorWriteFunction,
                    uint8_t *destPixelData)
{
    gl::ColorUI destColor(
        static_cast<unsigned int>(color.red * 255), static_cast<unsigned int>(color.green * 255),
        static_cast<unsigned int>(color.blue * 255), static_cast<unsigned int>(color.alpha * 255));
    colorWriteFunction(reinterpret_cast<const uint8_t *>(&destColor), destPixelData);
}

void WriteFloatColor(const gl::ColorF &color,
                     PixelWriteFunction colorWriteFunction,
                     uint8_t *destPixelData)
{
    colorWriteFunction(reinterpret_cast<const uint8_t *>(&color), destPixelData);
}

template <int cols, int rows, bool IsColumnMajor>
inline int GetFlattenedIndex(int col, int row)
{
    if (IsColumnMajor)
    {
        return col * rows + row;
    }
    else
    {
        return row * cols + col;
    }
}

template <typename T,
          bool IsSrcColumnMajor,
          int colsSrc,
          int rowsSrc,
          bool IsDstColumnMajor,
          int colsDst,
          int rowsDst>
void ExpandMatrix(T *target, const GLfloat *value)
{
    static_assert(colsSrc <= colsDst && rowsSrc <= rowsDst, "Can only expand!");

    constexpr int kDstFlatSize = colsDst * rowsDst;
    T staging[kDstFlatSize]    = {0};

    for (int r = 0; r < rowsSrc; r++)
    {
        for (int c = 0; c < colsSrc; c++)
        {
            int srcIndex = GetFlattenedIndex<colsSrc, rowsSrc, IsSrcColumnMajor>(c, r);
            int dstIndex = GetFlattenedIndex<colsDst, rowsDst, IsDstColumnMajor>(c, r);

            staging[dstIndex] = static_cast<T>(value[srcIndex]);
        }
    }

    memcpy(target, staging, kDstFlatSize * sizeof(T));
}

template <bool IsSrcColumMajor,
          int colsSrc,
          int rowsSrc,
          bool IsDstColumnMajor,
          int colsDst,
          int rowsDst>
void SetFloatUniformMatrix(unsigned int arrayElementOffset,
                           unsigned int elementCount,
                           GLsizei countIn,
                           const GLfloat *value,
                           uint8_t *targetData)
{
    unsigned int count =
        std::min(elementCount - arrayElementOffset, static_cast<unsigned int>(countIn));

    const unsigned int targetMatrixStride = colsDst * rowsDst;
    GLfloat *target                       = reinterpret_cast<GLfloat *>(
        targetData + arrayElementOffset * sizeof(GLfloat) * targetMatrixStride);

    for (unsigned int i = 0; i < count; i++)
    {
        ExpandMatrix<GLfloat, IsSrcColumMajor, colsSrc, rowsSrc, IsDstColumnMajor, colsDst,
                     rowsDst>(target, value);

        target += targetMatrixStride;
        value += colsSrc * rowsSrc;
    }
}

void SetFloatUniformMatrixFast(unsigned int arrayElementOffset,
                               unsigned int elementCount,
                               GLsizei countIn,
                               size_t matrixSize,
                               const GLfloat *value,
                               uint8_t *targetData)
{
    const unsigned int count =
        std::min(elementCount - arrayElementOffset, static_cast<unsigned int>(countIn));

    const uint8_t *valueData = reinterpret_cast<const uint8_t *>(value);
    targetData               = targetData + arrayElementOffset * matrixSize;

    memcpy(targetData, valueData, matrixSize * count);
}
}  // anonymous namespace

bool IsRotatedAspectRatio(SurfaceRotation rotation)
{
    switch (rotation)
    {
        case SurfaceRotation::Rotated90Degrees:
        case SurfaceRotation::Rotated270Degrees:
        case SurfaceRotation::FlippedRotated90Degrees:
        case SurfaceRotation::FlippedRotated270Degrees:
            return true;
        default:
            return false;
    }
}

void RotateRectangle(const SurfaceRotation rotation,
                     const bool flipY,
                     const int framebufferWidth,
                     const int framebufferHeight,
                     const gl::Rectangle &incoming,
                     gl::Rectangle *outgoing)
{
    // GLES's y-axis points up; Vulkan's points down.
    switch (rotation)
    {
        case SurfaceRotation::Identity:
            // Do not rotate gl_Position (surface matches the device's orientation):
            outgoing->x     = incoming.x;
            outgoing->y     = flipY ? framebufferHeight - incoming.y - incoming.height : incoming.y;
            outgoing->width = incoming.width;
            outgoing->height = incoming.height;
            break;
        case SurfaceRotation::Rotated90Degrees:
            // Rotate gl_Position 90 degrees:
            outgoing->x      = incoming.y;
            outgoing->y      = flipY ? incoming.x : framebufferWidth - incoming.x - incoming.width;
            outgoing->width  = incoming.height;
            outgoing->height = incoming.width;
            break;
        case SurfaceRotation::Rotated180Degrees:
            // Rotate gl_Position 180 degrees:
            outgoing->x     = framebufferWidth - incoming.x - incoming.width;
            outgoing->y     = flipY ? incoming.y : framebufferHeight - incoming.y - incoming.height;
            outgoing->width = incoming.width;
            outgoing->height = incoming.height;
            break;
        case SurfaceRotation::Rotated270Degrees:
            // Rotate gl_Position 270 degrees:
            outgoing->x      = framebufferHeight - incoming.y - incoming.height;
            outgoing->y      = flipY ? framebufferWidth - incoming.x - incoming.width : incoming.x;
            outgoing->width  = incoming.height;
            outgoing->height = incoming.width;
            break;
        default:
            UNREACHABLE();
            break;
    }
}

PackPixelsParams::PackPixelsParams()
    : destFormat(nullptr),
      outputPitch(0),
      packBuffer(nullptr),
      offset(0),
      rotation(SurfaceRotation::Identity)
{}

PackPixelsParams::PackPixelsParams(const gl::Rectangle &areaIn,
                                   const angle::Format &destFormat,
                                   GLuint outputPitchIn,
                                   bool reverseRowOrderIn,
                                   gl::Buffer *packBufferIn,
                                   ptrdiff_t offsetIn)
    : area(areaIn),
      destFormat(&destFormat),
      outputPitch(outputPitchIn),
      packBuffer(packBufferIn),
      reverseRowOrder(reverseRowOrderIn),
      offset(offsetIn),
      rotation(SurfaceRotation::Identity)
{}

void PackPixels(const PackPixelsParams &params,
                const angle::Format &sourceFormat,
                int inputPitchIn,
                const uint8_t *sourceIn,
                uint8_t *destWithoutOffset)
{
    uint8_t *destWithOffset = destWithoutOffset + params.offset;

    const uint8_t *source = sourceIn;
    int inputPitch        = inputPitchIn;
    int destWidth         = params.area.width;
    int destHeight        = params.area.height;
    int xAxisPitch        = 0;
    int yAxisPitch        = 0;
    switch (params.rotation)
    {
        case SurfaceRotation::Identity:
            // The source image is not rotated (i.e. matches the device's orientation), and may or
            // may not be y-flipped.  The image is row-major.  Each source row (one step along the
            // y-axis for each step in the dest y-axis) is inputPitch past the previous row.  Along
            // a row, each source pixel (one step along the x-axis for each step in the dest
            // x-axis) is sourceFormat.pixelBytes past the previous pixel.
            xAxisPitch = sourceFormat.pixelBytes;
            if (params.reverseRowOrder)
            {
                // The source image is y-flipped, which means we start at the last row, and each
                // source row is BEFORE the previous row.
                source += inputPitchIn * (params.area.height - 1);
                inputPitch = -inputPitch;
                yAxisPitch = -inputPitchIn;
            }
            else
            {
                yAxisPitch = inputPitchIn;
            }
            break;
        case SurfaceRotation::Rotated90Degrees:
            // The source image is rotated 90 degrees counter-clockwise.  Y-flip is always applied
            // to rotated images.  The image is column-major.  Each source column (one step along
            // the source x-axis for each step in the dest y-axis) is inputPitch past the previous
            // column.  Along a column, each source pixel (one step along the y-axis for each step
            // in the dest x-axis) is sourceFormat.pixelBytes past the previous pixel.
            xAxisPitch = inputPitchIn;
            yAxisPitch = sourceFormat.pixelBytes;
            destWidth  = params.area.height;
            destHeight = params.area.width;
            break;
        case SurfaceRotation::Rotated180Degrees:
            // The source image is rotated 180 degrees.  Y-flip is always applied to rotated
            // images.  The image is row-major, but upside down.  Each source row (one step along
            // the y-axis for each step in the dest y-axis) is inputPitch after the previous row.
            // Along a row, each source pixel (one step along the x-axis for each step in the dest
            // x-axis) is sourceFormat.pixelBytes BEFORE the previous pixel.
            xAxisPitch = -static_cast<int>(sourceFormat.pixelBytes);
            yAxisPitch = inputPitchIn;
            source += sourceFormat.pixelBytes * (params.area.width - 1);
            break;
        case SurfaceRotation::Rotated270Degrees:
            // The source image is rotated 270 degrees counter-clockwise (or 90 degrees clockwise).
            // Y-flip is always applied to rotated images.  The image is column-major, where each
            // column (one step in the source x-axis for one step in the dest y-axis) is inputPitch
            // BEFORE the previous column.  Along a column, each source pixel (one step along the
            // y-axis for each step in the dest x-axis) is sourceFormat.pixelBytes BEFORE the
            // previous pixel.  The first pixel is at the end of the source.
            xAxisPitch = -inputPitchIn;
            yAxisPitch = -static_cast<int>(sourceFormat.pixelBytes);
            destWidth  = params.area.height;
            destHeight = params.area.width;
            source += inputPitch * (params.area.height - 1) +
                      sourceFormat.pixelBytes * (params.area.width - 1);
            break;
        default:
            UNREACHABLE();
            break;
    }

    if (params.rotation == SurfaceRotation::Identity && sourceFormat == *params.destFormat)
    {
        // Direct copy possible
        for (int y = 0; y < params.area.height; ++y)
        {
            memcpy(destWithOffset + y * params.outputPitch, source + y * inputPitch,
                   params.area.width * sourceFormat.pixelBytes);
        }
        return;
    }

    FastCopyFunction fastCopyFunc = sourceFormat.fastCopyFunctions.get(params.destFormat->id);

    if (fastCopyFunc)
    {
        // Fast copy is possible through some special function
        fastCopyFunc(source, xAxisPitch, yAxisPitch, destWithOffset, params.destFormat->pixelBytes,
                     params.outputPitch, destWidth, destHeight);
        return;
    }

    PixelWriteFunction pixelWriteFunction = params.destFormat->pixelWriteFunction;
    ASSERT(pixelWriteFunction != nullptr);

    // Maximum size of any Color<T> type used.
    uint8_t temp[16];
    static_assert(sizeof(temp) >= sizeof(gl::ColorF) && sizeof(temp) >= sizeof(gl::ColorUI) &&
                      sizeof(temp) >= sizeof(gl::ColorI) &&
                      sizeof(temp) >= sizeof(angle::DepthStencil),
                  "Unexpected size of pixel struct.");

    PixelReadFunction pixelReadFunction = sourceFormat.pixelReadFunction;
    ASSERT(pixelReadFunction != nullptr);

    for (int y = 0; y < destHeight; ++y)
    {
        for (int x = 0; x < destWidth; ++x)
        {
            uint8_t *dest =
                destWithOffset + y * params.outputPitch + x * params.destFormat->pixelBytes;
            const uint8_t *src = source + y * yAxisPitch + x * xAxisPitch;

            // readFunc and writeFunc will be using the same type of color, CopyTexImage
            // will not allow the copy otherwise.
            pixelReadFunction(src, temp);
            pixelWriteFunction(temp, dest);
        }
    }
}

bool FastCopyFunctionMap::has(angle::FormatID formatID) const
{
    return (get(formatID) != nullptr);
}

namespace
{

const FastCopyFunctionMap::Entry *getEntry(const FastCopyFunctionMap::Entry *entry,
                                           size_t numEntries,
                                           angle::FormatID formatID)
{
    const FastCopyFunctionMap::Entry *end = entry + numEntries;
    while (entry != end)
    {
        if (entry->formatID == formatID)
        {
            return entry;
        }
        ++entry;
    }

    return nullptr;
}

}  // namespace

FastCopyFunction FastCopyFunctionMap::get(angle::FormatID formatID) const
{
    const FastCopyFunctionMap::Entry *entry = getEntry(mData, mSize, formatID);
    return entry ? entry->func : nullptr;
}

bool ShouldUseDebugLayers(const egl::AttributeMap &attribs)
{
    EGLAttrib debugSetting =
        attribs.get(EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE, EGL_DONT_CARE);

    // Prefer to enable debug layers when available.
#if defined(ANGLE_ENABLE_ASSERTS)
    return (debugSetting != EGL_FALSE);
#else
    return (debugSetting == EGL_TRUE);
#endif  // defined(ANGLE_ENABLE_ASSERTS)
}

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
                       bool unpackUnmultiplyAlpha)
{
    using ConversionFunction              = void (*)(gl::ColorF *);
    ConversionFunction conversionFunction = CopyColor;
    if (unpackPremultiplyAlpha != unpackUnmultiplyAlpha)
    {
        if (unpackPremultiplyAlpha)
        {
            conversionFunction = PremultiplyAlpha;
        }
        else
        {
            conversionFunction = UnmultiplyAlpha;
        }
    }

    auto clipChannelsFunction = ClipChannelsNoOp;
    switch (destUnsizedFormat)
    {
        case GL_RED:
            clipChannelsFunction = ClipChannelsR;
            break;
        case GL_RG:
            clipChannelsFunction = ClipChannelsRG;
            break;
        case GL_RGB:
            clipChannelsFunction = ClipChannelsRGB;
            break;
        case GL_LUMINANCE:
            clipChannelsFunction = ClipChannelsLuminance;
            break;
        case GL_ALPHA:
            clipChannelsFunction = ClipChannelsAlpha;
            break;
    }

    auto writeFunction = (destComponentType == GL_UNSIGNED_INT) ? WriteUintColor : WriteFloatColor;

    for (size_t z = 0; z < depth; z++)
    {
        for (size_t y = 0; y < height; y++)
        {
            for (size_t x = 0; x < width; x++)
            {
                const uint8_t *sourcePixelData =
                    sourceData + y * sourceRowPitch + x * sourcePixelBytes + z * sourceDepthPitch;

                gl::ColorF sourceColor;
                pixelReadFunction(sourcePixelData, reinterpret_cast<uint8_t *>(&sourceColor));

                conversionFunction(&sourceColor);
                clipChannelsFunction(&sourceColor);

                size_t destY = 0;
                if (unpackFlipY)
                {
                    destY += (height - 1);
                    destY -= y;
                }
                else
                {
                    destY += y;
                }

                uint8_t *destPixelData =
                    destData + destY * destRowPitch + x * destPixelBytes + z * destDepthPitch;
                writeFunction(sourceColor, pixelWriteFunction, destPixelData);
            }
        }
    }
}

// IncompleteTextureSet implementation.
IncompleteTextureSet::IncompleteTextureSet() : mIncompleteTextureBufferAttachment(nullptr) {}

IncompleteTextureSet::~IncompleteTextureSet() {}

void IncompleteTextureSet::onDestroy(const gl::Context *context)
{
    // Clear incomplete textures.
    for (auto &incompleteTextures : mIncompleteTextures)
    {
        for (auto &incompleteTexture : incompleteTextures)
        {
            if (incompleteTexture.get() != nullptr)
            {
                incompleteTexture->onDestroy(context);
                incompleteTexture.set(context, nullptr);
            }
        }
    }
    if (mIncompleteTextureBufferAttachment != nullptr)
    {
        mIncompleteTextureBufferAttachment->onDestroy(context);
        mIncompleteTextureBufferAttachment = nullptr;
    }
}

angle::Result IncompleteTextureSet::getIncompleteTexture(
    const gl::Context *context,
    gl::TextureType type,
    gl::SamplerFormat format,
    MultisampleTextureInitializer *multisampleInitializer,
    gl::Texture **textureOut)
{
    *textureOut = mIncompleteTextures[format][type].get();
    if (*textureOut != nullptr)
    {
        return angle::Result::Continue;
    }

    ContextImpl *implFactory = context->getImplementation();

    gl::Extents colorSize(1, 1, 1);
    gl::PixelUnpackState unpack;
    unpack.alignment = 1;
    gl::Box area(0, 0, 0, 1, 1, 1);
    const IncompleteTextureParameters &incompleteTextureParam =
        kIncompleteTextureParameters[format];

    // Cube map arrays are expected to have layer counts that are multiples of 6
    constexpr int kCubeMapArraySize = 6;
    if (type == gl::TextureType::CubeMapArray)
    {
        // From the GLES 3.2 spec:
        //   8.18. IMMUTABLE-FORMAT TEXTURE IMAGES
        //   TexStorage3D Errors
        //   An INVALID_OPERATION error is generated if any of the following conditions hold:
        //     * target is TEXTURE_CUBE_MAP_ARRAY and depth is not a multiple of 6
        // Since ANGLE treats incomplete textures as immutable, respect that here.
        colorSize.depth = kCubeMapArraySize;
        area.depth      = kCubeMapArraySize;
    }

    // If a texture is external use a 2D texture for the incomplete texture
    gl::TextureType createType = (type == gl::TextureType::External) ? gl::TextureType::_2D : type;

    gl::Texture *tex =
        new gl::Texture(implFactory, {std::numeric_limits<GLuint>::max()}, createType);
    angle::UniqueObjectPointer<gl::Texture, gl::Context> t(tex, context);

    // This is a bit of a kludge but is necessary to consume the error.
    gl::Context *mutableContext = const_cast<gl::Context *>(context);

    if (createType == gl::TextureType::Buffer)
    {
        constexpr uint32_t kBufferInitData = 0;
        mIncompleteTextureBufferAttachment =
            new gl::Buffer(implFactory, {std::numeric_limits<GLuint>::max()});
        ANGLE_TRY(mIncompleteTextureBufferAttachment->bufferData(
            mutableContext, gl::BufferBinding::Texture, &kBufferInitData, sizeof(kBufferInitData),
            gl::BufferUsage::StaticDraw));
    }
    else if (createType == gl::TextureType::_2DMultisample)
    {
        ANGLE_TRY(t->setStorageMultisample(mutableContext, createType, 1,
                                           incompleteTextureParam.sizedInternalFormat, colorSize,
                                           true));
    }
    else
    {
        ANGLE_TRY(t->setStorage(mutableContext, createType, 1,
                                incompleteTextureParam.sizedInternalFormat, colorSize));
    }

    if (type == gl::TextureType::CubeMap)
    {
        for (gl::TextureTarget face : gl::AllCubeFaceTextureTargets())
        {
            ANGLE_TRY(t->setSubImage(mutableContext, unpack, nullptr, face, 0, area,
                                     incompleteTextureParam.format, incompleteTextureParam.type,
                                     incompleteTextureParam.clearColor));
        }
    }
    else if (type == gl::TextureType::CubeMapArray)
    {
        // We need to provide enough pixel data to fill the array of six faces
        GLubyte incompleteCubeArrayPixels[kCubeMapArraySize][4];
        for (int i = 0; i < kCubeMapArraySize; ++i)
        {
            incompleteCubeArrayPixels[i][0] = incompleteTextureParam.clearColor[0];
            incompleteCubeArrayPixels[i][1] = incompleteTextureParam.clearColor[1];
            incompleteCubeArrayPixels[i][2] = incompleteTextureParam.clearColor[2];
            incompleteCubeArrayPixels[i][3] = incompleteTextureParam.clearColor[3];
        }

        ANGLE_TRY(t->setSubImage(mutableContext, unpack, nullptr,
                                 gl::NonCubeTextureTypeToTarget(createType), 0, area,
                                 incompleteTextureParam.format, incompleteTextureParam.type,
                                 *incompleteCubeArrayPixels));
    }
    else if (type == gl::TextureType::_2DMultisample)
    {
        // Call a specialized clear function to init a multisample texture.
        ANGLE_TRY(multisampleInitializer->initializeMultisampleTextureToBlack(context, t.get()));
    }
    else if (type == gl::TextureType::Buffer)
    {
        ANGLE_TRY(t->setBuffer(context, mIncompleteTextureBufferAttachment,
                               incompleteTextureParam.sizedInternalFormat));
    }
    else
    {
        ANGLE_TRY(t->setSubImage(mutableContext, unpack, nullptr,
                                 gl::NonCubeTextureTypeToTarget(createType), 0, area,
                                 incompleteTextureParam.format, incompleteTextureParam.type,
                                 incompleteTextureParam.clearColor));
    }

    if (format == gl::SamplerFormat::Shadow)
    {
        // To avoid the undefined spec behavior for shadow samplers with a depth texture, we set the
        // compare mode to GL_COMPARE_REF_TO_TEXTURE
        ASSERT(!t->hasObservers());
        t->setCompareMode(context, GL_COMPARE_REF_TO_TEXTURE);
    }

    ANGLE_TRY(t->syncState(context, gl::Command::Other));

    mIncompleteTextures[format][type].set(context, t.release());
    *textureOut = mIncompleteTextures[format][type].get();
    return angle::Result::Continue;
}

#define ANGLE_INSTANTIATE_SET_UNIFORM_MATRIX_FUNC(api, cols, rows) \
    template void SetFloatUniformMatrix##api<cols, rows>::Run(     \
        unsigned int, unsigned int, GLsizei, GLboolean, const GLfloat *, uint8_t *)

ANGLE_INSTANTIATE_SET_UNIFORM_MATRIX_FUNC(GLSL, 2, 2);
ANGLE_INSTANTIATE_SET_UNIFORM_MATRIX_FUNC(GLSL, 3, 3);
ANGLE_INSTANTIATE_SET_UNIFORM_MATRIX_FUNC(GLSL, 2, 3);
ANGLE_INSTANTIATE_SET_UNIFORM_MATRIX_FUNC(GLSL, 3, 2);
ANGLE_INSTANTIATE_SET_UNIFORM_MATRIX_FUNC(GLSL, 4, 2);
ANGLE_INSTANTIATE_SET_UNIFORM_MATRIX_FUNC(GLSL, 4, 3);

ANGLE_INSTANTIATE_SET_UNIFORM_MATRIX_FUNC(HLSL, 2, 2);
ANGLE_INSTANTIATE_SET_UNIFORM_MATRIX_FUNC(HLSL, 3, 3);
ANGLE_INSTANTIATE_SET_UNIFORM_MATRIX_FUNC(HLSL, 2, 3);
ANGLE_INSTANTIATE_SET_UNIFORM_MATRIX_FUNC(HLSL, 3, 2);
ANGLE_INSTANTIATE_SET_UNIFORM_MATRIX_FUNC(HLSL, 2, 4);
ANGLE_INSTANTIATE_SET_UNIFORM_MATRIX_FUNC(HLSL, 3, 4);

#undef ANGLE_INSTANTIATE_SET_UNIFORM_MATRIX_FUNC

#define ANGLE_SPECIALIZATION_ROWS_SET_UNIFORM_MATRIX_FUNC(api, cols, rows)                      \
    template void SetFloatUniformMatrix##api<cols, 4>::Run(unsigned int, unsigned int, GLsizei, \
                                                           GLboolean, const GLfloat *, uint8_t *)

template <int cols>
struct SetFloatUniformMatrixGLSL<cols, 4>
{
    static void Run(unsigned int arrayElementOffset,
                    unsigned int elementCount,
                    GLsizei countIn,
                    GLboolean transpose,
                    const GLfloat *value,
                    uint8_t *targetData);
};

ANGLE_SPECIALIZATION_ROWS_SET_UNIFORM_MATRIX_FUNC(GLSL, 2, 4);
ANGLE_SPECIALIZATION_ROWS_SET_UNIFORM_MATRIX_FUNC(GLSL, 3, 4);
ANGLE_SPECIALIZATION_ROWS_SET_UNIFORM_MATRIX_FUNC(GLSL, 4, 4);

#undef ANGLE_SPECIALIZATION_ROWS_SET_UNIFORM_MATRIX_FUNC

#define ANGLE_SPECIALIZATION_COLS_SET_UNIFORM_MATRIX_FUNC(api, cols, rows)                      \
    template void SetFloatUniformMatrix##api<4, rows>::Run(unsigned int, unsigned int, GLsizei, \
                                                           GLboolean, const GLfloat *, uint8_t *)

template <int rows>
struct SetFloatUniformMatrixHLSL<4, rows>
{
    static void Run(unsigned int arrayElementOffset,
                    unsigned int elementCount,
                    GLsizei countIn,
                    GLboolean transpose,
                    const GLfloat *value,
                    uint8_t *targetData);
};

ANGLE_SPECIALIZATION_COLS_SET_UNIFORM_MATRIX_FUNC(HLSL, 4, 2);
ANGLE_SPECIALIZATION_COLS_SET_UNIFORM_MATRIX_FUNC(HLSL, 4, 3);
ANGLE_SPECIALIZATION_COLS_SET_UNIFORM_MATRIX_FUNC(HLSL, 4, 4);

#undef ANGLE_SPECIALIZATION_COLS_SET_UNIFORM_MATRIX_FUNC

template <int cols>
void SetFloatUniformMatrixGLSL<cols, 4>::Run(unsigned int arrayElementOffset,
                                             unsigned int elementCount,
                                             GLsizei countIn,
                                             GLboolean transpose,
                                             const GLfloat *value,
                                             uint8_t *targetData)
{
    const bool isSrcColumnMajor = !transpose;
    if (isSrcColumnMajor)
    {
        // Both src and dst matrixs are has same layout,
        // a single memcpy updates all the matrices
        constexpr size_t srcMatrixSize = sizeof(GLfloat) * cols * 4;
        SetFloatUniformMatrixFast(arrayElementOffset, elementCount, countIn, srcMatrixSize, value,
                                  targetData);
    }
    else
    {
        // fallback to general cases
        SetFloatUniformMatrix<false, cols, 4, true, cols, 4>(arrayElementOffset, elementCount,
                                                             countIn, value, targetData);
    }
}

template <int cols, int rows>
void SetFloatUniformMatrixGLSL<cols, rows>::Run(unsigned int arrayElementOffset,
                                                unsigned int elementCount,
                                                GLsizei countIn,
                                                GLboolean transpose,
                                                const GLfloat *value,
                                                uint8_t *targetData)
{
    const bool isSrcColumnMajor = !transpose;
    // GLSL expects matrix uniforms to be column-major, and each column is padded to 4 rows.
    if (isSrcColumnMajor)
    {
        SetFloatUniformMatrix<true, cols, rows, true, cols, 4>(arrayElementOffset, elementCount,
                                                               countIn, value, targetData);
    }
    else
    {
        SetFloatUniformMatrix<false, cols, rows, true, cols, 4>(arrayElementOffset, elementCount,
                                                                countIn, value, targetData);
    }
}

template <int rows>
void SetFloatUniformMatrixHLSL<4, rows>::Run(unsigned int arrayElementOffset,
                                             unsigned int elementCount,
                                             GLsizei countIn,
                                             GLboolean transpose,
                                             const GLfloat *value,
                                             uint8_t *targetData)
{
    const bool isSrcColumnMajor = !transpose;
    if (!isSrcColumnMajor)
    {
        // Both src and dst matrixs are has same layout,
        // a single memcpy updates all the matrices
        constexpr size_t srcMatrixSize = sizeof(GLfloat) * 4 * rows;
        SetFloatUniformMatrixFast(arrayElementOffset, elementCount, countIn, srcMatrixSize, value,
                                  targetData);
    }
    else
    {
        // fallback to general cases
        SetFloatUniformMatrix<true, 4, rows, false, 4, rows>(arrayElementOffset, elementCount,
                                                             countIn, value, targetData);
    }
}

template <int cols, int rows>
void SetFloatUniformMatrixHLSL<cols, rows>::Run(unsigned int arrayElementOffset,
                                                unsigned int elementCount,
                                                GLsizei countIn,
                                                GLboolean transpose,
                                                const GLfloat *value,
                                                uint8_t *targetData)
{
    const bool isSrcColumnMajor = !transpose;
    // Internally store matrices as row-major to accomodate HLSL matrix indexing.  Each row is
    // padded to 4 columns.
    if (!isSrcColumnMajor)
    {
        SetFloatUniformMatrix<false, cols, rows, false, 4, rows>(arrayElementOffset, elementCount,
                                                                 countIn, value, targetData);
    }
    else
    {
        SetFloatUniformMatrix<true, cols, rows, false, 4, rows>(arrayElementOffset, elementCount,
                                                                countIn, value, targetData);
    }
}

template void GetMatrixUniform<GLint>(GLenum, GLint *, const GLint *, bool);
template void GetMatrixUniform<GLuint>(GLenum, GLuint *, const GLuint *, bool);

void GetMatrixUniform(GLenum type, GLfloat *dataOut, const GLfloat *source, bool transpose)
{
    int columns = gl::VariableColumnCount(type);
    int rows    = gl::VariableRowCount(type);
    for (GLint col = 0; col < columns; ++col)
    {
        for (GLint row = 0; row < rows; ++row)
        {
            GLfloat *outptr = dataOut + ((col * rows) + row);
            const GLfloat *inptr =
                transpose ? source + ((row * 4) + col) : source + ((col * 4) + row);
            *outptr = *inptr;
        }
    }
}

template <typename NonFloatT>
void GetMatrixUniform(GLenum type, NonFloatT *dataOut, const NonFloatT *source, bool transpose)
{
    UNREACHABLE();
}

const angle::Format &GetFormatFromFormatType(GLenum format, GLenum type)
{
    GLenum sizedInternalFormat    = gl::GetInternalFormatInfo(format, type).sizedInternalFormat;
    angle::FormatID angleFormatID = angle::Format::InternalFormatToID(sizedInternalFormat);
    return angle::Format::Get(angleFormatID);
}

angle::Result ComputeStartVertex(ContextImpl *contextImpl,
                                 const gl::IndexRange &indexRange,
                                 GLint baseVertex,
                                 GLint *firstVertexOut)
{
    // The entire index range should be within the limits of a 32-bit uint because the largest
    // GL index type is GL_UNSIGNED_INT.
    ASSERT(indexRange.start <= std::numeric_limits<uint32_t>::max() &&
           indexRange.end <= std::numeric_limits<uint32_t>::max());

    // The base vertex is only used in DrawElementsIndirect. Given the assertion above and the
    // type of mBaseVertex (GLint), adding them both as 64-bit ints is safe.
    int64_t startVertexInt64 =
        static_cast<int64_t>(baseVertex) + static_cast<int64_t>(indexRange.start);

    // OpenGL ES 3.2 spec section 10.5: "Behavior of DrawElementsOneInstance is undefined if the
    // vertex ID is negative for any element"
    ANGLE_CHECK_GL_MATH(contextImpl, startVertexInt64 >= 0);

    // OpenGL ES 3.2 spec section 10.5: "If the vertex ID is larger than the maximum value
    // representable by type, it should behave as if the calculation were upconverted to 32-bit
    // unsigned integers(with wrapping on overflow conditions)." ANGLE does not fully handle
    // these rules, an overflow error is returned if the start vertex cannot be stored in a
    // 32-bit signed integer.
    ANGLE_CHECK_GL_MATH(contextImpl, startVertexInt64 <= std::numeric_limits<GLint>::max());

    *firstVertexOut = static_cast<GLint>(startVertexInt64);
    return angle::Result::Continue;
}

angle::Result GetVertexRangeInfo(const gl::Context *context,
                                 GLint firstVertex,
                                 GLsizei vertexOrIndexCount,
                                 gl::DrawElementsType indexTypeOrInvalid,
                                 const void *indices,
                                 GLint baseVertex,
                                 GLint *startVertexOut,
                                 size_t *vertexCountOut)
{
    if (indexTypeOrInvalid != gl::DrawElementsType::InvalidEnum)
    {
        gl::IndexRange indexRange;
        ANGLE_TRY(context->getState().getVertexArray()->getIndexRange(
            context, indexTypeOrInvalid, vertexOrIndexCount, indices, &indexRange));
        ANGLE_TRY(ComputeStartVertex(context->getImplementation(), indexRange, baseVertex,
                                     startVertexOut));
        *vertexCountOut = indexRange.vertexCount();
    }
    else
    {
        *startVertexOut = firstVertex;
        *vertexCountOut = vertexOrIndexCount;
    }
    return angle::Result::Continue;
}

gl::Rectangle ClipRectToScissor(const gl::State &glState, const gl::Rectangle &rect, bool invertY)
{
    // If the scissor test isn't enabled, assume it has infinite size.  Its intersection with the
    // rect would be the rect itself.
    //
    // Note that on Vulkan, returning this (as opposed to a fixed max-int-sized rect) could lead to
    // unnecessary pipeline creations if two otherwise identical pipelines are used on framebuffers
    // with different sizes.  If such usage is observed in an application, we should investigate
    // possible optimizations.
    if (!glState.isScissorTestEnabled())
    {
        return rect;
    }

    gl::Rectangle clippedRect;
    if (!gl::ClipRectangle(glState.getScissor(), rect, &clippedRect))
    {
        return gl::Rectangle();
    }

    if (invertY)
    {
        clippedRect.y = rect.height - clippedRect.y - clippedRect.height;
    }

    return clippedRect;
}

void LogFeatureStatus(const angle::FeatureSetBase &features,
                      const std::vector<std::string> &featureNames,
                      bool enabled)
{
    for (const std::string &name : featureNames)
    {
        if (features.getFeatures().find(name) != features.getFeatures().end())
        {
            INFO() << "Feature: " << name << (enabled ? " enabled" : " disabled");
        }
    }
}

void ApplyFeatureOverrides(angle::FeatureSetBase *features, const egl::DisplayState &state)
{
    features->overrideFeatures(state.featureOverridesEnabled, true);
    features->overrideFeatures(state.featureOverridesDisabled, false);

    // Override with environment as well.
    constexpr char kAngleFeatureOverridesEnabledEnvName[]  = "ANGLE_FEATURE_OVERRIDES_ENABLED";
    constexpr char kAngleFeatureOverridesDisabledEnvName[] = "ANGLE_FEATURE_OVERRIDES_DISABLED";
    constexpr char kAngleFeatureOverridesEnabledPropertyName[] =
        "debug.angle.feature_overrides_enabled";
    constexpr char kAngleFeatureOverridesDisabledPropertyName[] =
        "debug.angle.feature_overrides_disabled";
    std::vector<std::string> overridesEnabled =
        angle::GetCachedStringsFromEnvironmentVarOrAndroidProperty(
            kAngleFeatureOverridesEnabledEnvName, kAngleFeatureOverridesEnabledPropertyName, ":");
    std::vector<std::string> overridesDisabled =
        angle::GetCachedStringsFromEnvironmentVarOrAndroidProperty(
            kAngleFeatureOverridesDisabledEnvName, kAngleFeatureOverridesDisabledPropertyName, ":");

    features->overrideFeatures(overridesEnabled, true);
    LogFeatureStatus(*features, overridesEnabled, true);

    features->overrideFeatures(overridesDisabled, false);
    LogFeatureStatus(*features, overridesDisabled, false);
}

void GetSamplePosition(GLsizei sampleCount, size_t index, GLfloat *xy)
{
    ASSERT(gl::isPow2(sampleCount));
    if (sampleCount > 16)
    {
        // Vulkan (and D3D11) doesn't have standard sample positions for 32 and 64 samples (and no
        // drivers are known to support that many samples)
        xy[0] = 0.5f;
        xy[1] = 0.5f;
    }
    else
    {
        size_t indexKey = static_cast<size_t>(gl::log2(sampleCount));
        ASSERT(indexKey < kSamplePositions.size() &&
               (2 * index + 1) < kSamplePositions[indexKey].size());

        xy[0] = kSamplePositions[indexKey][2 * index];
        xy[1] = kSamplePositions[indexKey][2 * index + 1];
    }
}

// These macros are to avoid code too much duplication for variations of multi draw types
#define DRAW_ARRAYS__ contextImpl->drawArrays(context, mode, firsts[drawID], counts[drawID])
#define DRAW_ARRAYS_INSTANCED_                                                      \
    contextImpl->drawArraysInstanced(context, mode, firsts[drawID], counts[drawID], \
                                     instanceCounts[drawID])
#define DRAW_ELEMENTS__ \
    contextImpl->drawElements(context, mode, counts[drawID], type, indices[drawID])
#define DRAW_ELEMENTS_INSTANCED_                                                             \
    contextImpl->drawElementsInstanced(context, mode, counts[drawID], type, indices[drawID], \
                                       instanceCounts[drawID])
#define DRAW_ARRAYS_INSTANCED_BASE_INSTANCE                                                     \
    contextImpl->drawArraysInstancedBaseInstance(context, mode, firsts[drawID], counts[drawID], \
                                                 instanceCounts[drawID], baseInstances[drawID])
#define DRAW_ELEMENTS_INSTANCED_BASE_VERTEX_BASE_INSTANCE                             \
    contextImpl->drawElementsInstancedBaseVertexBaseInstance(                         \
        context, mode, counts[drawID], type, indices[drawID], instanceCounts[drawID], \
        baseVertices[drawID], baseInstances[drawID])
#define DRAW_CALL(drawType, instanced, bvbi) DRAW_##drawType##instanced##bvbi

#define MULTI_DRAW_BLOCK(drawType, instanced, bvbi, hasDrawID, hasBaseVertex, hasBaseInstance) \
    for (GLsizei drawID = 0; drawID < drawcount; ++drawID)                                     \
    {                                                                                          \
        if (ANGLE_NOOP_DRAW(instanced))                                                        \
        {                                                                                      \
            ANGLE_TRY(contextImpl->handleNoopDrawEvent());                                     \
            continue;                                                                          \
        }                                                                                      \
        ANGLE_SET_DRAW_ID_UNIFORM(hasDrawID)(drawID);                                          \
        ANGLE_SET_BASE_VERTEX_UNIFORM(hasBaseVertex)(baseVertices[drawID]);                    \
        ANGLE_SET_BASE_INSTANCE_UNIFORM(hasBaseInstance)(baseInstances[drawID]);               \
        ANGLE_TRY(DRAW_CALL(drawType, instanced, bvbi));                                       \
        ANGLE_MARK_TRANSFORM_FEEDBACK_USAGE(instanced);                                        \
        gl::MarkShaderStorageUsage(context);                                                   \
    }

angle::Result MultiDrawArraysGeneral(ContextImpl *contextImpl,
                                     const gl::Context *context,
                                     gl::PrimitiveMode mode,
                                     const GLint *firsts,
                                     const GLsizei *counts,
                                     GLsizei drawcount)
{
    gl::Program *programObject = context->getState().getLinkedProgram(context);
    const bool hasDrawID       = programObject && programObject->hasDrawIDUniform();
    if (hasDrawID)
    {
        MULTI_DRAW_BLOCK(ARRAYS, _, _, 1, 0, 0)
    }
    else
    {
        MULTI_DRAW_BLOCK(ARRAYS, _, _, 0, 0, 0)
    }

    return angle::Result::Continue;
}

angle::Result MultiDrawArraysIndirectGeneral(ContextImpl *contextImpl,
                                             const gl::Context *context,
                                             gl::PrimitiveMode mode,
                                             const void *indirect,
                                             GLsizei drawcount,
                                             GLsizei stride)
{
    const GLubyte *indirectPtr = static_cast<const GLubyte *>(indirect);

    for (auto count = 0; count < drawcount; count++)
    {
        ANGLE_TRY(contextImpl->drawArraysIndirect(
            context, mode, reinterpret_cast<const gl::DrawArraysIndirectCommand *>(indirectPtr)));
        if (stride == 0)
        {
            indirectPtr += sizeof(gl::DrawArraysIndirectCommand);
        }
        else
        {
            indirectPtr += stride;
        }
    }

    return angle::Result::Continue;
}

angle::Result MultiDrawArraysInstancedGeneral(ContextImpl *contextImpl,
                                              const gl::Context *context,
                                              gl::PrimitiveMode mode,
                                              const GLint *firsts,
                                              const GLsizei *counts,
                                              const GLsizei *instanceCounts,
                                              GLsizei drawcount)
{
    gl::Program *programObject = context->getState().getLinkedProgram(context);
    const bool hasDrawID       = programObject && programObject->hasDrawIDUniform();
    if (hasDrawID)
    {
        MULTI_DRAW_BLOCK(ARRAYS, _INSTANCED, _, 1, 0, 0)
    }
    else
    {
        MULTI_DRAW_BLOCK(ARRAYS, _INSTANCED, _, 0, 0, 0)
    }

    return angle::Result::Continue;
}

angle::Result MultiDrawElementsGeneral(ContextImpl *contextImpl,
                                       const gl::Context *context,
                                       gl::PrimitiveMode mode,
                                       const GLsizei *counts,
                                       gl::DrawElementsType type,
                                       const GLvoid *const *indices,
                                       GLsizei drawcount)
{
    gl::Program *programObject = context->getState().getLinkedProgram(context);
    const bool hasDrawID       = programObject && programObject->hasDrawIDUniform();
    if (hasDrawID)
    {
        MULTI_DRAW_BLOCK(ELEMENTS, _, _, 1, 0, 0)
    }
    else
    {
        MULTI_DRAW_BLOCK(ELEMENTS, _, _, 0, 0, 0)
    }

    return angle::Result::Continue;
}

angle::Result MultiDrawElementsIndirectGeneral(ContextImpl *contextImpl,
                                               const gl::Context *context,
                                               gl::PrimitiveMode mode,
                                               gl::DrawElementsType type,
                                               const void *indirect,
                                               GLsizei drawcount,
                                               GLsizei stride)
{
    const GLubyte *indirectPtr = static_cast<const GLubyte *>(indirect);

    for (auto count = 0; count < drawcount; count++)
    {
        ANGLE_TRY(contextImpl->drawElementsIndirect(
            context, mode, type,
            reinterpret_cast<const gl::DrawElementsIndirectCommand *>(indirectPtr)));
        if (stride == 0)
        {
            indirectPtr += sizeof(gl::DrawElementsIndirectCommand);
        }
        else
        {
            indirectPtr += stride;
        }
    }

    return angle::Result::Continue;
}

angle::Result MultiDrawElementsInstancedGeneral(ContextImpl *contextImpl,
                                                const gl::Context *context,
                                                gl::PrimitiveMode mode,
                                                const GLsizei *counts,
                                                gl::DrawElementsType type,
                                                const GLvoid *const *indices,
                                                const GLsizei *instanceCounts,
                                                GLsizei drawcount)
{
    gl::Program *programObject = context->getState().getLinkedProgram(context);
    const bool hasDrawID       = programObject && programObject->hasDrawIDUniform();
    if (hasDrawID)
    {
        MULTI_DRAW_BLOCK(ELEMENTS, _INSTANCED, _, 1, 0, 0)
    }
    else
    {
        MULTI_DRAW_BLOCK(ELEMENTS, _INSTANCED, _, 0, 0, 0)
    }

    return angle::Result::Continue;
}

angle::Result MultiDrawArraysInstancedBaseInstanceGeneral(ContextImpl *contextImpl,
                                                          const gl::Context *context,
                                                          gl::PrimitiveMode mode,
                                                          const GLint *firsts,
                                                          const GLsizei *counts,
                                                          const GLsizei *instanceCounts,
                                                          const GLuint *baseInstances,
                                                          GLsizei drawcount)
{
    gl::Program *programObject = context->getState().getLinkedProgram(context);
    const bool hasDrawID       = programObject && programObject->hasDrawIDUniform();
    const bool hasBaseInstance = programObject && programObject->hasBaseInstanceUniform();
    ResetBaseVertexBaseInstance resetUniforms(programObject, false, hasBaseInstance);

    if (hasDrawID && hasBaseInstance)
    {
        MULTI_DRAW_BLOCK(ARRAYS, _INSTANCED, _BASE_INSTANCE, 1, 0, 1)
    }
    else if (hasDrawID)
    {
        MULTI_DRAW_BLOCK(ARRAYS, _INSTANCED, _BASE_INSTANCE, 1, 0, 0)
    }
    else if (hasBaseInstance)
    {
        MULTI_DRAW_BLOCK(ARRAYS, _INSTANCED, _BASE_INSTANCE, 0, 0, 1)
    }
    else
    {
        MULTI_DRAW_BLOCK(ARRAYS, _INSTANCED, _BASE_INSTANCE, 0, 0, 0)
    }

    return angle::Result::Continue;
}

angle::Result MultiDrawElementsInstancedBaseVertexBaseInstanceGeneral(ContextImpl *contextImpl,
                                                                      const gl::Context *context,
                                                                      gl::PrimitiveMode mode,
                                                                      const GLsizei *counts,
                                                                      gl::DrawElementsType type,
                                                                      const GLvoid *const *indices,
                                                                      const GLsizei *instanceCounts,
                                                                      const GLint *baseVertices,
                                                                      const GLuint *baseInstances,
                                                                      GLsizei drawcount)
{
    gl::Program *programObject = context->getState().getLinkedProgram(context);
    const bool hasDrawID       = programObject && programObject->hasDrawIDUniform();
    const bool hasBaseVertex   = programObject && programObject->hasBaseVertexUniform();
    const bool hasBaseInstance = programObject && programObject->hasBaseInstanceUniform();
    ResetBaseVertexBaseInstance resetUniforms(programObject, hasBaseVertex, hasBaseInstance);

    if (hasDrawID)
    {
        if (hasBaseVertex)
        {
            if (hasBaseInstance)
            {
                MULTI_DRAW_BLOCK(ELEMENTS, _INSTANCED, _BASE_VERTEX_BASE_INSTANCE, 1, 1, 1)
            }
            else
            {
                MULTI_DRAW_BLOCK(ELEMENTS, _INSTANCED, _BASE_VERTEX_BASE_INSTANCE, 1, 1, 0)
            }
        }
        else
        {
            if (hasBaseInstance)
            {
                MULTI_DRAW_BLOCK(ELEMENTS, _INSTANCED, _BASE_VERTEX_BASE_INSTANCE, 1, 0, 1)
            }
            else
            {
                MULTI_DRAW_BLOCK(ELEMENTS, _INSTANCED, _BASE_VERTEX_BASE_INSTANCE, 1, 0, 0)
            }
        }
    }
    else
    {
        if (hasBaseVertex)
        {
            if (hasBaseInstance)
            {
                MULTI_DRAW_BLOCK(ELEMENTS, _INSTANCED, _BASE_VERTEX_BASE_INSTANCE, 0, 1, 1)
            }
            else
            {
                MULTI_DRAW_BLOCK(ELEMENTS, _INSTANCED, _BASE_VERTEX_BASE_INSTANCE, 0, 1, 0)
            }
        }
        else
        {
            if (hasBaseInstance)
            {
                MULTI_DRAW_BLOCK(ELEMENTS, _INSTANCED, _BASE_VERTEX_BASE_INSTANCE, 0, 0, 1)
            }
            else
            {
                MULTI_DRAW_BLOCK(ELEMENTS, _INSTANCED, _BASE_VERTEX_BASE_INSTANCE, 0, 0, 0)
            }
        }
    }

    return angle::Result::Continue;
}

ResetBaseVertexBaseInstance::ResetBaseVertexBaseInstance(gl::Program *programObject,
                                                         bool resetBaseVertex,
                                                         bool resetBaseInstance)
    : mProgramObject(programObject),
      mResetBaseVertex(resetBaseVertex),
      mResetBaseInstance(resetBaseInstance)
{}

ResetBaseVertexBaseInstance::~ResetBaseVertexBaseInstance()
{
    if (mProgramObject)
    {
        // Reset emulated uniforms to zero to avoid affecting other draw calls
        if (mResetBaseVertex)
        {
            mProgramObject->setBaseVertexUniform(0);
        }

        if (mResetBaseInstance)
        {
            mProgramObject->setBaseInstanceUniform(0);
        }
    }
}

angle::FormatID ConvertToSRGB(angle::FormatID formatID)
{
    switch (formatID)
    {
        case angle::FormatID::R8_UNORM:
            return angle::FormatID::R8_UNORM_SRGB;
        case angle::FormatID::R8G8_UNORM:
            return angle::FormatID::R8G8_UNORM_SRGB;
        case angle::FormatID::R8G8B8_UNORM:
            return angle::FormatID::R8G8B8_UNORM_SRGB;
        case angle::FormatID::R8G8B8A8_UNORM:
            return angle::FormatID::R8G8B8A8_UNORM_SRGB;
        case angle::FormatID::B8G8R8A8_UNORM:
            return angle::FormatID::B8G8R8A8_UNORM_SRGB;
        case angle::FormatID::BC1_RGB_UNORM_BLOCK:
            return angle::FormatID::BC1_RGB_UNORM_SRGB_BLOCK;
        case angle::FormatID::BC1_RGBA_UNORM_BLOCK:
            return angle::FormatID::BC1_RGBA_UNORM_SRGB_BLOCK;
        case angle::FormatID::BC2_RGBA_UNORM_BLOCK:
            return angle::FormatID::BC2_RGBA_UNORM_SRGB_BLOCK;
        case angle::FormatID::BC3_RGBA_UNORM_BLOCK:
            return angle::FormatID::BC3_RGBA_UNORM_SRGB_BLOCK;
        case angle::FormatID::BC7_RGBA_UNORM_BLOCK:
            return angle::FormatID::BC7_RGBA_UNORM_SRGB_BLOCK;
        case angle::FormatID::ETC2_R8G8B8_UNORM_BLOCK:
            return angle::FormatID::ETC2_R8G8B8_SRGB_BLOCK;
        case angle::FormatID::ETC2_R8G8B8A1_UNORM_BLOCK:
            return angle::FormatID::ETC2_R8G8B8A1_SRGB_BLOCK;
        case angle::FormatID::ETC2_R8G8B8A8_UNORM_BLOCK:
            return angle::FormatID::ETC2_R8G8B8A8_SRGB_BLOCK;
        case angle::FormatID::ASTC_4x4_UNORM_BLOCK:
            return angle::FormatID::ASTC_4x4_SRGB_BLOCK;
        case angle::FormatID::ASTC_5x4_UNORM_BLOCK:
            return angle::FormatID::ASTC_5x4_SRGB_BLOCK;
        case angle::FormatID::ASTC_5x5_UNORM_BLOCK:
            return angle::FormatID::ASTC_5x5_SRGB_BLOCK;
        case angle::FormatID::ASTC_6x5_UNORM_BLOCK:
            return angle::FormatID::ASTC_6x5_SRGB_BLOCK;
        case angle::FormatID::ASTC_6x6_UNORM_BLOCK:
            return angle::FormatID::ASTC_6x6_SRGB_BLOCK;
        case angle::FormatID::ASTC_8x5_UNORM_BLOCK:
            return angle::FormatID::ASTC_8x5_SRGB_BLOCK;
        case angle::FormatID::ASTC_8x6_UNORM_BLOCK:
            return angle::FormatID::ASTC_8x6_SRGB_BLOCK;
        case angle::FormatID::ASTC_8x8_UNORM_BLOCK:
            return angle::FormatID::ASTC_8x8_SRGB_BLOCK;
        case angle::FormatID::ASTC_10x5_UNORM_BLOCK:
            return angle::FormatID::ASTC_10x5_SRGB_BLOCK;
        case angle::FormatID::ASTC_10x6_UNORM_BLOCK:
            return angle::FormatID::ASTC_10x6_SRGB_BLOCK;
        case angle::FormatID::ASTC_10x8_UNORM_BLOCK:
            return angle::FormatID::ASTC_10x8_SRGB_BLOCK;
        case angle::FormatID::ASTC_10x10_UNORM_BLOCK:
            return angle::FormatID::ASTC_10x10_SRGB_BLOCK;
        case angle::FormatID::ASTC_12x10_UNORM_BLOCK:
            return angle::FormatID::ASTC_12x10_SRGB_BLOCK;
        case angle::FormatID::ASTC_12x12_UNORM_BLOCK:
            return angle::FormatID::ASTC_12x12_SRGB_BLOCK;
        default:
            return angle::FormatID::NONE;
    }
}

angle::FormatID ConvertToLinear(angle::FormatID formatID)
{
    switch (formatID)
    {
        case angle::FormatID::R8_UNORM_SRGB:
            return angle::FormatID::R8_UNORM;
        case angle::FormatID::R8G8_UNORM_SRGB:
            return angle::FormatID::R8G8_UNORM;
        case angle::FormatID::R8G8B8_UNORM_SRGB:
            return angle::FormatID::R8G8B8_UNORM;
        case angle::FormatID::R8G8B8A8_UNORM_SRGB:
            return angle::FormatID::R8G8B8A8_UNORM;
        case angle::FormatID::B8G8R8A8_UNORM_SRGB:
            return angle::FormatID::B8G8R8A8_UNORM;
        case angle::FormatID::BC1_RGB_UNORM_SRGB_BLOCK:
            return angle::FormatID::BC1_RGB_UNORM_BLOCK;
        case angle::FormatID::BC1_RGBA_UNORM_SRGB_BLOCK:
            return angle::FormatID::BC1_RGBA_UNORM_BLOCK;
        case angle::FormatID::BC2_RGBA_UNORM_SRGB_BLOCK:
            return angle::FormatID::BC2_RGBA_UNORM_BLOCK;
        case angle::FormatID::BC3_RGBA_UNORM_SRGB_BLOCK:
            return angle::FormatID::BC3_RGBA_UNORM_BLOCK;
        case angle::FormatID::BC7_RGBA_UNORM_SRGB_BLOCK:
            return angle::FormatID::BC7_RGBA_UNORM_BLOCK;
        case angle::FormatID::ETC2_R8G8B8_SRGB_BLOCK:
            return angle::FormatID::ETC2_R8G8B8_UNORM_BLOCK;
        case angle::FormatID::ETC2_R8G8B8A1_SRGB_BLOCK:
            return angle::FormatID::ETC2_R8G8B8A1_UNORM_BLOCK;
        case angle::FormatID::ETC2_R8G8B8A8_SRGB_BLOCK:
            return angle::FormatID::ETC2_R8G8B8A8_UNORM_BLOCK;
        case angle::FormatID::ASTC_4x4_SRGB_BLOCK:
            return angle::FormatID::ASTC_4x4_UNORM_BLOCK;
        case angle::FormatID::ASTC_5x4_SRGB_BLOCK:
            return angle::FormatID::ASTC_5x4_UNORM_BLOCK;
        case angle::FormatID::ASTC_5x5_SRGB_BLOCK:
            return angle::FormatID::ASTC_5x5_UNORM_BLOCK;
        case angle::FormatID::ASTC_6x5_SRGB_BLOCK:
            return angle::FormatID::ASTC_6x5_UNORM_BLOCK;
        case angle::FormatID::ASTC_6x6_SRGB_BLOCK:
            return angle::FormatID::ASTC_6x6_UNORM_BLOCK;
        case angle::FormatID::ASTC_8x5_SRGB_BLOCK:
            return angle::FormatID::ASTC_8x5_UNORM_BLOCK;
        case angle::FormatID::ASTC_8x6_SRGB_BLOCK:
            return angle::FormatID::ASTC_8x6_UNORM_BLOCK;
        case angle::FormatID::ASTC_8x8_SRGB_BLOCK:
            return angle::FormatID::ASTC_8x8_UNORM_BLOCK;
        case angle::FormatID::ASTC_10x5_SRGB_BLOCK:
            return angle::FormatID::ASTC_10x5_UNORM_BLOCK;
        case angle::FormatID::ASTC_10x6_SRGB_BLOCK:
            return angle::FormatID::ASTC_10x6_UNORM_BLOCK;
        case angle::FormatID::ASTC_10x8_SRGB_BLOCK:
            return angle::FormatID::ASTC_10x8_UNORM_BLOCK;
        case angle::FormatID::ASTC_10x10_SRGB_BLOCK:
            return angle::FormatID::ASTC_10x10_UNORM_BLOCK;
        case angle::FormatID::ASTC_12x10_SRGB_BLOCK:
            return angle::FormatID::ASTC_12x10_UNORM_BLOCK;
        case angle::FormatID::ASTC_12x12_SRGB_BLOCK:
            return angle::FormatID::ASTC_12x12_UNORM_BLOCK;
        default:
            return angle::FormatID::NONE;
    }
}

bool IsOverridableLinearFormat(angle::FormatID formatID)
{
    return ConvertToSRGB(formatID) != angle::FormatID::NONE;
}
}  // namespace rx
