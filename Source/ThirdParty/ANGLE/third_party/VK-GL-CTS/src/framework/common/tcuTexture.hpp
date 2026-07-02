#ifndef _TCUTEXTURE_HPP
#define _TCUTEXTURE_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Reference Texture Implementation.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuVector.hpp"
#include "rrGenericVector.hpp"
#include "deArrayBuffer.hpp"

#include <vector>
#include <ostream>

namespace tcu
{

/*--------------------------------------------------------------------*//*!
 * \brief Texture format
 *//*--------------------------------------------------------------------*/
class TextureFormat
{
public:
    enum ChannelOrder
    {
        R = 0,
        A,
        I,
        L,
        LA,
        RG,
        RA,
        RGB,
        RGBA,
        ARGB,
        ABGR,
        BGR,
        BGRA,

        sR,
        sRG,
        sRGB,
        sRGBA,
        sBGR,
        sBGRA,

        D,
        S,
        DS,

        CHANNELORDER_LAST
    };

    enum ChannelType
    {
        SNORM_INT8 = 0,
        SNORM_INT16,
        SNORM_INT32,
        UNORM_INT8,
        UNORM_INT16,
        UNORM_INT24,
        UNORM_INT32,
        UNORM_BYTE_44,
        UNORM_SHORT_565,
        UNORM_SHORT_555,
        UNORM_SHORT_4444,
        UNORM_SHORT_5551,
        UNORM_SHORT_1555,
        UNORM_INT_101010,
        SNORM_INT_1010102_REV,
        UNORM_INT_1010102_REV,
        UNSIGNED_BYTE_44,
        UNSIGNED_SHORT_565,
        UNSIGNED_SHORT_4444,
        UNSIGNED_SHORT_5551,
        SIGNED_INT_1010102_REV,
        UNSIGNED_INT_1010102_REV,
        UNSIGNED_INT_11F_11F_10F_REV,
        UNSIGNED_INT_999_E5_REV,
        UNSIGNED_INT_16_8_8,
        UNSIGNED_INT_24_8,
        UNSIGNED_INT_24_8_REV,
        SIGNED_INT8,
        SIGNED_INT16,
        SIGNED_INT32,
        SIGNED_INT64,
        UNSIGNED_INT8,
        UNSIGNED_INT16,
        UNSIGNED_INT24,
        UNSIGNED_INT32,
        UNSIGNED_INT64,
        HALF_FLOAT,
        FLOAT,
        FLOAT64,
        FLOAT_UNSIGNED_INT_24_8_REV,

        UNORM_SHORT_10,
        UNORM_SHORT_12,

        USCALED_INT8,
        USCALED_INT16,
        SSCALED_INT8,
        SSCALED_INT16,
        USCALED_INT_1010102_REV,
        SSCALED_INT_1010102_REV,

        CHANNELTYPE_LAST
    };

    ChannelOrder order;
    ChannelType type;

    TextureFormat(ChannelOrder order_, ChannelType type_) : order(order_), type(type_)
    {
    }

    TextureFormat(void) : order(CHANNELORDER_LAST), type(CHANNELTYPE_LAST)
    {
    }

    int getPixelSize(void) const; //!< Deprecated, use tcu::getPixelSize(fmt)

    bool operator==(const TextureFormat &other) const
    {
        return !(*this != other);
    }
    bool operator!=(const TextureFormat &other) const
    {
        return (order != other.order || type != other.type);
    }
    bool operator<(const TextureFormat &other) const
    {
        if (order < other.order)
            return true;
        if (order > other.order)
            return false;
        return (type < other.type);
    }
} DE_WARN_UNUSED_TYPE;

tcu::Vec4 unpackRGB999E5(uint32_t color);
bool isValid(TextureFormat format);
int getPixelSize(TextureFormat format);
int getNumUsedChannels(TextureFormat::ChannelOrder order);
bool hasAlphaChannel(TextureFormat::ChannelOrder order);
int getChannelSize(TextureFormat::ChannelType type);

/*--------------------------------------------------------------------*//*!
 * \brief Texture swizzle
 *//*--------------------------------------------------------------------*/
struct TextureSwizzle
{
    enum Channel
    {
        // \note CHANNEL_N must equal int N
        CHANNEL_0 = 0,
        CHANNEL_1,
        CHANNEL_2,
        CHANNEL_3,

        CHANNEL_ZERO,
        CHANNEL_ONE,

        CHANNEL_LAST
    };

    Channel components[4];
};

//! get the swizzle used to expand texture data with a given channel order to RGBA form
const TextureSwizzle &getChannelReadSwizzle(TextureFormat::ChannelOrder order);

//! get the swizzle used to narrow RGBA form data to native texture data with a given channel order
const TextureSwizzle &getChannelWriteSwizzle(TextureFormat::ChannelOrder order);

/*--------------------------------------------------------------------*//*!
 * \brief Sampling parameters
 *//*--------------------------------------------------------------------*/
class Sampler
{
public:
    enum WrapMode
    {
        CLAMP_TO_EDGE = 0,  //! Clamp to edge
        CLAMP_TO_BORDER,    //! Use border color at edge
        REPEAT_GL,          //! Repeat with OpenGL semantics
        REPEAT_CL,          //! Repeat with OpenCL semantics
        MIRRORED_REPEAT_GL, //! Mirrored repeat with OpenGL semantics
        MIRRORED_REPEAT_CL, //! Mirrored repeat with OpenCL semantics
        MIRRORED_ONCE,      //! Mirrored once in negative directions

        WRAPMODE_LAST
    };

    enum FilterMode
    {
        NEAREST = 0,
        LINEAR,
        CUBIC,

        NEAREST_MIPMAP_NEAREST,
        NEAREST_MIPMAP_LINEAR,
        LINEAR_MIPMAP_NEAREST,
        LINEAR_MIPMAP_LINEAR,
        CUBIC_MIPMAP_NEAREST,
        CUBIC_MIPMAP_LINEAR,

        FILTERMODE_LAST
    };

    enum ReductionMode
    {
        WEIGHTED_AVERAGE = 0,
        MIN,
        MAX,

        REDUCTIONMODE_LAST
    };

    enum CompareMode
    {
        COMPAREMODE_NONE = 0,
        COMPAREMODE_LESS,
        COMPAREMODE_LESS_OR_EQUAL,
        COMPAREMODE_GREATER,
        COMPAREMODE_GREATER_OR_EQUAL,
        COMPAREMODE_EQUAL,
        COMPAREMODE_NOT_EQUAL,
        COMPAREMODE_ALWAYS,
        COMPAREMODE_NEVER,

        COMPAREMODE_LAST
    };

    enum DepthStencilMode
    {
        MODE_DEPTH = 0,
        MODE_STENCIL,

        MODE_LAST
    };

    // Wrap control
    WrapMode wrapS;
    WrapMode wrapT;
    WrapMode wrapR;

    // Minifcation & magnification
    FilterMode minFilter;
    FilterMode magFilter;

    // min/max filtering reduction
    ReductionMode reductionMode;

    float lodThreshold; // lod <= lodThreshold ? magnified : minified

    // Coordinate normalization
    bool normalizedCoords;

    // Shadow comparison
    CompareMode compare;
    int compareChannel;

    // Border color.
    // \note It is setter's responsibility to guarantee that the values are representable
    //       in sampled texture's internal format.
    // \note It is setter's responsibility to guarantee that the format is compatible with the
    //       sampled texture's internal format. Otherwise results are undefined.
    rr::GenericVec4 borderColor;

    // Seamless cube map filtering
    bool seamlessCubeMap;

    // Depth stencil mode
    DepthStencilMode depthStencilMode;

    Sampler(WrapMode wrapS_, WrapMode wrapT_, WrapMode wrapR_, FilterMode minFilter_, FilterMode magFilter_,
            float lodThreshold_ = 0.0f, bool normalizedCoords_ = true, CompareMode compare_ = COMPAREMODE_NONE,
            int compareChannel_ = 0, const Vec4 &borderColor_ = Vec4(0.0f, 0.0f, 0.0f, 0.0f),
            bool seamlessCubeMap_ = false, DepthStencilMode depthStencilMode_ = MODE_DEPTH,
            ReductionMode reductionMode_ = WEIGHTED_AVERAGE)
        : wrapS(wrapS_)
        , wrapT(wrapT_)
        , wrapR(wrapR_)
        , minFilter(minFilter_)
        , magFilter(magFilter_)
        , reductionMode(reductionMode_)
        , lodThreshold(lodThreshold_)
        , normalizedCoords(normalizedCoords_)
        , compare(compare_)
        , compareChannel(compareChannel_)
        , borderColor(borderColor_)
        , seamlessCubeMap(seamlessCubeMap_)
        , depthStencilMode(depthStencilMode_)
    {
    }

    Sampler(void)
        : wrapS(WRAPMODE_LAST)
        , wrapT(WRAPMODE_LAST)
        , wrapR(WRAPMODE_LAST)
        , minFilter(FILTERMODE_LAST)
        , magFilter(FILTERMODE_LAST)
        , reductionMode(REDUCTIONMODE_LAST)
        , lodThreshold(0.0f)
        , normalizedCoords(true)
        , compare(COMPAREMODE_NONE)
        , compareChannel(0)
        , borderColor(Vec4(0.0f, 0.0f, 0.0f, 0.0f))
        , seamlessCubeMap(false)
        , depthStencilMode(MODE_DEPTH)
    {
    }
} DE_WARN_UNUSED_TYPE;

// Calculate pitches for pixel data with no padding.
IVec3 calculatePackedPitch(const TextureFormat &format, const IVec3 &size);

bool isSamplerMipmapModeLinear(tcu::Sampler::FilterMode filterMode);

class TextureLevel;

/*--------------------------------------------------------------------*//*!
 * \brief Read-only pixel data access
 *
 * ConstPixelBufferAccess encapsulates pixel data pointer along with
 * format and layout information. It can be used for read-only access
 * to arbitrary pixel buffers.
 *
 * Access objects are like iterators or pointers. They can be passed around
 * as values and are valid as long as the storage doesn't change.
 *//*--------------------------------------------------------------------*/
class ConstPixelBufferAccess
{
public:
    ConstPixelBufferAccess(void);
    ConstPixelBufferAccess(const TextureLevel &level);
    ConstPixelBufferAccess(const TextureFormat &format, int width, int height, int depth, const void *data);
    ConstPixelBufferAccess(const TextureFormat &format, const IVec3 &size, const void *data);
    ConstPixelBufferAccess(const TextureFormat &format, int width, int height, int depth, int rowPitch, int slicePitch,
                           const void *data);
    ConstPixelBufferAccess(const TextureFormat &format, const IVec3 &size, const IVec3 &pitch, const void *data);
    ConstPixelBufferAccess(const TextureFormat &format, const IVec3 &size, const IVec3 &pitch, const IVec3 &divider,
                           const void *data);

    const TextureFormat &getFormat(void) const
    {
        return m_format;
    }
    const IVec3 &getSize(void) const
    {
        return m_size;
    }
    int getWidth(void) const
    {
        return m_size.x();
    }
    int getHeight(void) const
    {
        return m_size.y();
    }
    int getDepth(void) const
    {
        return m_size.z();
    }
    int getPixelPitch(void) const
    {
        return m_pitch.x();
    }
    int getRowPitch(void) const
    {
        return m_pitch.y();
    }
    int getSlicePitch(void) const
    {
        return m_pitch.z();
    }
    const IVec3 &getPitch(void) const
    {
        return m_pitch;
    }
    const IVec3 &getDivider(void) const
    {
        return m_divider;
    }

    const void *getDataPtr(void) const
    {
        return m_data;
    }
    const void *getPixelPtr(int x, int y, int z = 0) const
    {
        return (const uint8_t *)m_data + (x / m_divider.x()) * m_pitch.x() + (y / m_divider.y()) * m_pitch.y() +
               (z / m_divider.z()) * m_pitch.z();
    }

    Vec4 getPixel(int x, int y, int z = 0) const;
    IVec4 getPixelInt(int x, int y, int z = 0) const;
    UVec4 getPixelUint(int x, int y, int z = 0) const
    {
        return getPixelInt(x, y, z).cast<uint32_t>();
    }
    I64Vec4 getPixelInt64(int x, int y, int z = 0) const;
    U64Vec4 getPixelUint64(int x, int y, int z = 0) const
    {
        return getPixelInt64(x, y, z).cast<uint64_t>();
    }
    U64Vec4 getPixelBitsAsUint64(int x, int y, int z = 0) const;

    template <typename T>
    Vector<T, 4> getPixelT(int x, int y, int z = 0) const;

    float getPixDepth(int x, int y, int z = 0) const;
    int getPixStencil(int x, int y, int z = 0) const;

    Vec4 sample1D(const Sampler &sampler, Sampler::FilterMode filter, float s, int level) const;
    Vec4 sample2D(const Sampler &sampler, Sampler::FilterMode filter, float s, float t, int depth) const;
    Vec4 sample3D(const Sampler &sampler, Sampler::FilterMode filter, float s, float t, float r) const;

    Vec4 sample1DOffset(const Sampler &sampler, Sampler::FilterMode filter, float s, const IVec2 &offset) const;
    Vec4 sample2DOffset(const Sampler &sampler, Sampler::FilterMode filter, float s, float t,
                        const IVec3 &offset) const;
    Vec4 sample3DOffset(const Sampler &sampler, Sampler::FilterMode filter, float s, float t, float r,
                        const IVec3 &offset) const;

    float sample1DCompare(const Sampler &sampler, Sampler::FilterMode filter, float ref, float s,
                          const IVec2 &offset) const;
    float sample2DCompare(const Sampler &sampler, Sampler::FilterMode filter, float ref, float s, float t,
                          const IVec3 &offset) const;

protected:
    TextureFormat m_format;
    IVec3 m_size;
    IVec3 m_pitch; //!< (pixelPitch, rowPitch, slicePitch)
    IVec3 m_divider;
    mutable void *m_data;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief Read-write pixel data access
 *
 * This class extends read-only access object by providing write functionality.
 *
 * \note PixelBufferAccess may not have any data members nor add any
 *         virtual functions. It must be possible to reinterpret_cast<>
 *         PixelBufferAccess to ConstPixelBufferAccess.
 *//*--------------------------------------------------------------------*/
class PixelBufferAccess : public ConstPixelBufferAccess
{
public:
    PixelBufferAccess(void)
    {
    }
    PixelBufferAccess(TextureLevel &level);
    PixelBufferAccess(const TextureFormat &format, int width, int height, int depth, void *data);
    PixelBufferAccess(const TextureFormat &format, const IVec3 &size, void *data);
    PixelBufferAccess(const TextureFormat &format, int width, int height, int depth, int rowPitch, int slicePitch,
                      void *data);
    PixelBufferAccess(const TextureFormat &format, const IVec3 &size, const IVec3 &pitch, void *data);
    PixelBufferAccess(const TextureFormat &format, const IVec3 &size, const IVec3 &pitch, const IVec3 &block,
                      void *data);

    void *getDataPtr(void) const
    {
        return m_data;
    }
    void *getPixelPtr(int x, int y, int z = 0) const
    {
        return (uint8_t *)m_data + (x / m_divider.x()) * m_pitch.x() + (y / m_divider.y()) * m_pitch.y() +
               (z / m_divider.z()) * m_pitch.z();
    }

    void setPixel(const tcu::Vec4 &color, int x, int y, int z = 0) const;
    void setPixel(const tcu::IVec4 &color, int x, int y, int z = 0) const;
    void setPixel(const tcu::UVec4 &color, int x, int y, int z = 0) const
    {
        setPixel(color.cast<int>(), x, y, z);
    }

    void setPixDepth(float depth, int x, int y, int z = 0) const;
    void setPixStencil(int stencil, int x, int y, int z = 0) const;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief Generic pixel data container
 *
 * This container supports all valid TextureFormat combinations and
 * both 2D and 3D textures. To read or manipulate data access object must
 * be queried using getAccess().
 *//*--------------------------------------------------------------------*/
class TextureLevel
{
public:
    TextureLevel(void);
    TextureLevel(const TextureFormat &format);
    TextureLevel(const TextureFormat &format, int width, int height, int depth = 1);
    ~TextureLevel(void);

    const IVec3 &getSize(void) const
    {
        return m_size;
    }
    int getWidth(void) const
    {
        return m_size.x();
    }
    int getHeight(void) const
    {
        return m_size.y();
    }
    int getDepth(void) const
    {
        return m_size.z();
    }
    bool isEmpty(void) const
    {
        return m_size.x() * m_size.y() * m_size.z() == 0;
    }
    const TextureFormat getFormat(void) const
    {
        return m_format;
    }

    void setStorage(const TextureFormat &format, int width, int heigth, int depth = 1);
    void setSize(int width, int height, int depth = 1);

    PixelBufferAccess getAccess(void)
    {
        return isEmpty() ? PixelBufferAccess() :
                           PixelBufferAccess(m_format, m_size, calculatePackedPitch(m_format, m_size), getPtr());
    }
    ConstPixelBufferAccess getAccess(void) const
    {
        return isEmpty() ? ConstPixelBufferAccess() :
                           ConstPixelBufferAccess(m_format, m_size, calculatePackedPitch(m_format, m_size), getPtr());
    }

private:
    void *getPtr(void)
    {
        return m_data.getPtr();
    }
    const void *getPtr(void) const
    {
        return m_data.getPtr();
    }

    TextureFormat m_format;
    IVec3 m_size;
    de::ArrayBuffer<uint8_t> m_data;

    friend class ConstPixelBufferAccess;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief VK_EXT_image_view_min_lod
 *//*--------------------------------------------------------------------*/
enum ImageViewMinLodMode
{
    IMAGEVIEWMINLODMODE_PREFERRED, //!< use image view min lod as-is
    IMAGEVIEWMINLODMODE_ALTERNATIVE, //!< use floor of image view min lod, as in 'Image Level(s) Selection' in VK spec (v 1.3.206)
};

struct ImageViewMinLod
{
    float value;
    ImageViewMinLodMode mode;
};

struct ImageViewMinLodParams
{
    int baseLevel;
    ImageViewMinLod minLod;
    bool intTexCoord;
};

Vec4 sampleLevelArray1D(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float s, int level,
                        float lod);
Vec4 sampleLevelArray2D(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float s, float t,
                        int depth, float lod, bool es2 = false, ImageViewMinLodParams *minLodParams = nullptr);
Vec4 sampleLevelArray3D(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float s, float t,
                        float r, float lod, ImageViewMinLodParams *minLodParams = nullptr);

Vec4 sampleLevelArray1DOffset(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float s,
                              float lod, const IVec2 &offset);
Vec4 sampleLevelArray2DOffset(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float s,
                              float t, float lod, const IVec3 &offset, bool es2 = false,
                              ImageViewMinLodParams *minLodParams = nullptr);
Vec4 sampleLevelArray3DOffset(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float s,
                              float t, float r, float lod, const IVec3 &offset,
                              ImageViewMinLodParams *minLodParams = nullptr);

float sampleLevelArray1DCompare(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float ref,
                                float s, float lod, const IVec2 &offset);
float sampleLevelArray2DCompare(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float ref,
                                float s, float t, float lod, const IVec3 &offset);

Vec4 gatherArray2DOffsets(const ConstPixelBufferAccess &src, const Sampler &sampler, float s, float t, int depth,
                          int componentNdx, const IVec2 (&offsets)[4]);
Vec4 gatherArray2DOffsetsCompare(const ConstPixelBufferAccess &src, const Sampler &sampler, float ref, float s, float t,
                                 int depth, const IVec2 (&offsets)[4]);

enum CubeFace
{
    CUBEFACE_NEGATIVE_X = 0,
    CUBEFACE_POSITIVE_X,
    CUBEFACE_NEGATIVE_Y,
    CUBEFACE_POSITIVE_Y,
    CUBEFACE_NEGATIVE_Z,
    CUBEFACE_POSITIVE_Z,

    CUBEFACE_LAST
};

/*--------------------------------------------------------------------*//*!
 * \brief Coordinates projected onto cube face.
 *//*--------------------------------------------------------------------*/
template <typename T>
struct CubeFaceCoords
{
    CubeFace face;
    T s;
    T t;

    CubeFaceCoords(CubeFace face_, T s_, T t_) : face(face_), s(s_), t(t_)
    {
    }
    CubeFaceCoords(CubeFace face_, const Vector<T, 2> &c) : face(face_), s(c.x()), t(c.y())
    {
    }
} DE_WARN_UNUSED_TYPE;

typedef CubeFaceCoords<float> CubeFaceFloatCoords;
typedef CubeFaceCoords<int> CubeFaceIntCoords;

CubeFace selectCubeFace(const Vec3 &coords);
Vec2 projectToFace(CubeFace face, const Vec3 &coords);
CubeFaceFloatCoords getCubeFaceCoords(const Vec3 &coords);
CubeFaceIntCoords remapCubeEdgeCoords(const CubeFaceIntCoords &coords, int size);

/*--------------------------------------------------------------------*//*!
 * \brief 2D Texture View
 *//*--------------------------------------------------------------------*/
class Texture2DView
{
public:
    Texture2DView(int numLevels, const ConstPixelBufferAccess *levels, bool es2 = false,
                  ImageViewMinLodParams *minLodParams = nullptr);

    int getNumLevels(void) const
    {
        return m_numLevels;
    }
    int getWidth(void) const
    {
        return m_numLevels > 0 ? m_levels[0].getWidth() : 0;
    }
    int getHeight(void) const
    {
        return m_numLevels > 0 ? m_levels[0].getHeight() : 0;
    }
    const ConstPixelBufferAccess &getLevel(int ndx) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, m_numLevels));
        return m_levels[ndx];
    }
    const ConstPixelBufferAccess *getLevels(void) const
    {
        return m_levels;
    }
    bool isES2(void) const
    {
        return m_es2;
    }

    Vec4 sample(const Sampler &sampler, float s, float t, float lod) const;
    Vec4 sampleOffset(const Sampler &sampler, float s, float t, float lod, const IVec2 &offset) const;
    float sampleCompare(const Sampler &sampler, float ref, float s, float t, float lod) const;
    float sampleCompareOffset(const Sampler &sampler, float ref, float s, float t, float lod,
                              const IVec2 &offset) const;

    Vec4 gatherOffsets(const Sampler &sampler, float s, float t, int componentNdx, const IVec2 (&offsets)[4]) const;
    Vec4 gatherOffsetsCompare(const Sampler &sampler, float ref, float s, float t, const IVec2 (&offsets)[4]) const;

    ImageViewMinLodParams *getImageViewMinLodParams(void) const
    {
        return m_minLodParams;
    }

protected:
    int m_numLevels;
    const ConstPixelBufferAccess *m_levels;
    bool m_es2;
    struct ImageViewMinLodParams *m_minLodParams;
} DE_WARN_UNUSED_TYPE;

inline Texture2DView::Texture2DView(int numLevels, const ConstPixelBufferAccess *levels, bool es2,
                                    ImageViewMinLodParams *minLodParams)
    : m_numLevels(numLevels)
    , m_levels(levels)
    , m_es2(es2)
    , m_minLodParams(minLodParams)
{
    DE_ASSERT(m_numLevels >= 0 && ((m_numLevels == 0) == !m_levels));
}

inline Vec4 Texture2DView::sample(const Sampler &sampler, float s, float t, float lod) const
{
    return sampleLevelArray2D(m_levels, m_numLevels, sampler, s, t, 0 /* depth */, lod, m_es2, m_minLodParams);
}

inline Vec4 Texture2DView::sampleOffset(const Sampler &sampler, float s, float t, float lod, const IVec2 &offset) const
{
    return sampleLevelArray2DOffset(m_levels, m_numLevels, sampler, s, t, lod, IVec3(offset.x(), offset.y(), 0));
}

inline float Texture2DView::sampleCompare(const Sampler &sampler, float ref, float s, float t, float lod) const
{
    return sampleLevelArray2DCompare(m_levels, m_numLevels, sampler, ref, s, t, lod, IVec3(0, 0, 0));
}

inline float Texture2DView::sampleCompareOffset(const Sampler &sampler, float ref, float s, float t, float lod,
                                                const IVec2 &offset) const
{
    return sampleLevelArray2DCompare(m_levels, m_numLevels, sampler, ref, s, t, lod, IVec3(offset.x(), offset.y(), 0));
}

inline Vec4 Texture2DView::gatherOffsets(const Sampler &sampler, float s, float t, int componentNdx,
                                         const IVec2 (&offsets)[4]) const
{
    return gatherArray2DOffsets(m_levels[0], sampler, s, t, 0, componentNdx, offsets);
}

inline Vec4 Texture2DView::gatherOffsetsCompare(const Sampler &sampler, float ref, float s, float t,
                                                const IVec2 (&offsets)[4]) const
{
    return gatherArray2DOffsetsCompare(m_levels[0], sampler, ref, s, t, 0, offsets);
}

/*--------------------------------------------------------------------*//*!
 * \brief Base class for textures that have single mip-map pyramid
 *//*--------------------------------------------------------------------*/
class TextureLevelPyramid
{
public:
    TextureLevelPyramid(const TextureFormat &format, int numLevels);
    TextureLevelPyramid(const TextureLevelPyramid &other);
    ~TextureLevelPyramid(void);

    const TextureFormat &getFormat(void) const
    {
        return m_format;
    }
    int getNumLevels(void) const
    {
        return (int)m_access.size();
    }

    bool isLevelEmpty(int levelNdx) const
    {
        DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));
        return m_data[(size_t)levelNdx].empty();
    }
    const ConstPixelBufferAccess &getLevel(int levelNdx) const
    {
        DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));
        return m_access[(size_t)levelNdx];
    }
    const PixelBufferAccess &getLevel(int levelNdx)
    {
        DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));
        return m_access[(size_t)levelNdx];
    }

    const ConstPixelBufferAccess *getLevels(void) const
    {
        return &m_access[0];
    }
    const PixelBufferAccess *getLevels(void)
    {
        return &m_access[0];
    }

    void allocLevel(int levelNdx, int width, int height, int depth);
    void clearLevel(int levelNdx);

    TextureLevelPyramid &operator=(const TextureLevelPyramid &other);

private:
    typedef de::ArrayBuffer<uint8_t> LevelData;

    TextureFormat m_format;
    std::vector<LevelData> m_data;
    std::vector<PixelBufferAccess> m_access;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief 2D Texture reference implementation
 *//*--------------------------------------------------------------------*/
class Texture2D : private TextureLevelPyramid
{
public:
    Texture2D(const TextureFormat &format, int width, int height, bool es2 = false);
    Texture2D(const TextureFormat &format, int width, int height, int mipmaps);
    Texture2D(const Texture2D &other);
    ~Texture2D(void);

    int getWidth(void) const
    {
        return m_width;
    }
    int getHeight(void) const
    {
        return m_height;
    }
    const Texture2DView &getView(void) const
    {
        return m_view;
    }
    bool isYUVTextureUsed(void) const
    {
        return m_yuvTextureUsed;
    }
    void allocLevel(int levelNdx);

    // Sampling
    Vec4 sample(const Sampler &sampler, float s, float t, float lod) const;
    Vec4 sampleOffset(const Sampler &sampler, float s, float t, float lod, const IVec2 &offset) const;
    float sampleCompare(const Sampler &sampler, float ref, float s, float t, float lod) const;
    float sampleCompareOffset(const Sampler &sampler, float ref, float s, float t, float lod,
                              const IVec2 &offset) const;

    Vec4 gatherOffsets(const Sampler &sampler, float s, float t, int componentNdx, const IVec2 (&offsets)[4]) const;
    Vec4 gatherOffsetsCompare(const Sampler &sampler, float ref, float s, float t, const IVec2 (&offsets)[4]) const;

    using TextureLevelPyramid::clearLevel;
    using TextureLevelPyramid::getFormat;
    using TextureLevelPyramid::getLevel;
    using TextureLevelPyramid::getNumLevels;
    using TextureLevelPyramid::isLevelEmpty;

    Texture2D &operator=(const Texture2D &other);
    //whether this is a yuv format texture tests
    bool m_yuvTextureUsed;
    operator Texture2DView(void) const
    {
        return m_view;
    }

private:
    int m_width;
    int m_height;
    Texture2DView m_view;
} DE_WARN_UNUSED_TYPE;

inline Vec4 Texture2D::sample(const Sampler &sampler, float s, float t, float lod) const
{
    return m_view.sample(sampler, s, t, lod);
}

inline Vec4 Texture2D::sampleOffset(const Sampler &sampler, float s, float t, float lod, const IVec2 &offset) const
{
    return m_view.sampleOffset(sampler, s, t, lod, offset);
}

inline float Texture2D::sampleCompare(const Sampler &sampler, float ref, float s, float t, float lod) const
{
    return m_view.sampleCompare(sampler, ref, s, t, lod);
}

inline float Texture2D::sampleCompareOffset(const Sampler &sampler, float ref, float s, float t, float lod,
                                            const IVec2 &offset) const
{
    return m_view.sampleCompareOffset(sampler, ref, s, t, lod, offset);
}

inline Vec4 Texture2D::gatherOffsets(const Sampler &sampler, float s, float t, int componentNdx,
                                     const IVec2 (&offsets)[4]) const
{
    return m_view.gatherOffsets(sampler, s, t, componentNdx, offsets);
}

inline Vec4 Texture2D::gatherOffsetsCompare(const Sampler &sampler, float ref, float s, float t,
                                            const IVec2 (&offsets)[4]) const
{
    return m_view.gatherOffsetsCompare(sampler, ref, s, t, offsets);
}

/*--------------------------------------------------------------------*//*!
 * \brief Cube Map Texture View
 *//*--------------------------------------------------------------------*/
class TextureCubeView
{
public:
    TextureCubeView(void);
    TextureCubeView(int numLevels, const ConstPixelBufferAccess *const (&levels)[CUBEFACE_LAST], bool es2 = false,
                    ImageViewMinLodParams *minLodParams = nullptr);

    int getNumLevels(void) const
    {
        return m_numLevels;
    }
    bool isES2(void) const
    {
        return m_es2;
    }
    int getSize(void) const
    {
        return m_numLevels > 0 ? m_levels[0][0].getWidth() : 0;
    }
    const ConstPixelBufferAccess &getLevelFace(int ndx, CubeFace face) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, m_numLevels));
        return m_levels[face][ndx];
    }
    const ConstPixelBufferAccess *getFaceLevels(CubeFace face) const
    {
        return m_levels[face];
    }

    Vec4 sample(const Sampler &sampler, float s, float t, float p, float lod) const;
    float sampleCompare(const Sampler &sampler, float ref, float s, float t, float r, float lod) const;

    Vec4 gather(const Sampler &sampler, float s, float t, float r, int componentNdx) const;
    Vec4 gatherCompare(const Sampler &sampler, float ref, float s, float t, float r) const;

    ImageViewMinLodParams *getImageViewMinLodParams(void) const
    {
        return m_minLodParams;
    }

protected:
    int m_numLevels;
    const ConstPixelBufferAccess *m_levels[CUBEFACE_LAST];
    bool m_es2;
    ImageViewMinLodParams *m_minLodParams;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief Cube Map Texture reference implementation
 *//*--------------------------------------------------------------------*/
class TextureCube
{
public:
    TextureCube(const TextureFormat &format, int size, bool es2 = false);
    TextureCube(const TextureCube &other);
    ~TextureCube(void);

    const TextureFormat &getFormat(void) const
    {
        return m_format;
    }
    int getSize(void) const
    {
        return m_size;
    }
    const TextureCubeView &getView(void) const
    {
        return m_view;
    }

    int getNumLevels(void) const
    {
        return (int)m_access[0].size();
    }
    const ConstPixelBufferAccess &getLevelFace(int ndx, CubeFace face) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, getNumLevels()));
        return m_access[face][(size_t)ndx];
    }
    const PixelBufferAccess &getLevelFace(int ndx, CubeFace face)
    {
        DE_ASSERT(de::inBounds(ndx, 0, getNumLevels()));
        return m_access[face][(size_t)ndx];
    }

    void allocLevel(CubeFace face, int levelNdx);
    void clearLevel(CubeFace face, int levelNdx);
    bool isLevelEmpty(CubeFace face, int levelNdx) const
    {
        DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));
        return m_data[face][(size_t)levelNdx].empty();
    }

    Vec4 sample(const Sampler &sampler, float s, float t, float p, float lod) const;
    float sampleCompare(const Sampler &sampler, float ref, float s, float t, float r, float lod) const;

    Vec4 gather(const Sampler &sampler, float s, float t, float r, int componentNdx) const;
    Vec4 gatherCompare(const Sampler &sampler, float ref, float s, float t, float r) const;

    TextureCube &operator=(const TextureCube &other);

    operator TextureCubeView(void) const
    {
        return m_view;
    }

private:
    typedef de::ArrayBuffer<uint8_t> LevelData;

    TextureFormat m_format;
    int m_size;
    std::vector<LevelData> m_data[CUBEFACE_LAST];
    std::vector<PixelBufferAccess> m_access[CUBEFACE_LAST];
    TextureCubeView m_view;
} DE_WARN_UNUSED_TYPE;

inline Vec4 TextureCube::sample(const Sampler &sampler, float s, float t, float p, float lod) const
{
    return m_view.sample(sampler, s, t, p, lod);
}

inline float TextureCube::sampleCompare(const Sampler &sampler, float ref, float s, float t, float r, float lod) const
{
    return m_view.sampleCompare(sampler, ref, s, t, r, lod);
}

inline Vec4 TextureCube::gather(const Sampler &sampler, float s, float t, float r, int componentNdx) const
{
    return m_view.gather(sampler, s, t, r, componentNdx);
}

inline Vec4 TextureCube::gatherCompare(const Sampler &sampler, float ref, float s, float t, float r) const
{
    return m_view.gatherCompare(sampler, ref, s, t, r);
}

/*--------------------------------------------------------------------*//*!
 * \brief 1D Texture View
 *//*--------------------------------------------------------------------*/
class Texture1DView
{
public:
    Texture1DView(int numLevels, const ConstPixelBufferAccess *levels, bool es2, ImageViewMinLodParams *minLodParams);

    int getNumLevels(void) const
    {
        return m_numLevels;
    }
    int getWidth(void) const
    {
        return m_numLevels > 0 ? m_levels[0].getWidth() : 0;
    }
    const ConstPixelBufferAccess &getLevel(int ndx) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, m_numLevels));
        return m_levels[ndx];
    }
    const ConstPixelBufferAccess *getLevels(void) const
    {
        return m_levels;
    }
    bool isES2(void) const
    {
        return false;
    }

    Vec4 sample(const Sampler &sampler, float s, float lod) const;
    Vec4 sampleOffset(const Sampler &sampler, float s, float lod, int32_t offset) const;
    float sampleCompare(const Sampler &sampler, float ref, float s, float lod) const;
    float sampleCompareOffset(const Sampler &sampler, float ref, float s, float lod, int32_t offset) const;

    ImageViewMinLodParams *getImageViewMinLodParams(void) const
    {
        return nullptr;
    }

protected:
    int m_numLevels;
    const ConstPixelBufferAccess *m_levels;
} DE_WARN_UNUSED_TYPE;

inline Texture1DView::Texture1DView(int numLevels, const ConstPixelBufferAccess *levels,
                                    bool es2 DE_UNUSED_ATTR                            = false,
                                    ImageViewMinLodParams *minLodParams DE_UNUSED_ATTR = nullptr)
    : m_numLevels(numLevels)
    , m_levels(levels)
{
    DE_ASSERT(m_numLevels >= 0 && ((m_numLevels == 0) == !m_levels));
}

inline Vec4 Texture1DView::sample(const Sampler &sampler, float s, float lod) const
{
    return sampleLevelArray1D(m_levels, m_numLevels, sampler, s, 0 /* depth */, lod);
}

inline Vec4 Texture1DView::sampleOffset(const Sampler &sampler, float s, float lod, int32_t offset) const
{
    return sampleLevelArray1DOffset(m_levels, m_numLevels, sampler, s, lod, IVec2(offset, 0));
}

inline float Texture1DView::sampleCompare(const Sampler &sampler, float ref, float s, float lod) const
{
    return sampleLevelArray1DCompare(m_levels, m_numLevels, sampler, ref, s, lod, IVec2(0, 0));
}

inline float Texture1DView::sampleCompareOffset(const Sampler &sampler, float ref, float s, float lod,
                                                int32_t offset) const
{
    return sampleLevelArray1DCompare(m_levels, m_numLevels, sampler, ref, s, lod, IVec2(offset, 0));
}

/*--------------------------------------------------------------------*//*!
 * \brief 1D Texture reference implementation
 *//*--------------------------------------------------------------------*/
class Texture1D : private TextureLevelPyramid
{
public:
    Texture1D(const TextureFormat &format, int width);
    Texture1D(const Texture1D &other);
    ~Texture1D(void);

    int getWidth(void) const
    {
        return m_width;
    }
    const Texture1DView &getView(void) const
    {
        return m_view;
    }

    void allocLevel(int levelNdx);

    // Sampling
    Vec4 sample(const Sampler &sampler, float s, float lod) const;
    Vec4 sampleOffset(const Sampler &sampler, float s, float lod, int32_t offset) const;
    float sampleCompare(const Sampler &sampler, float ref, float s, float lod) const;
    float sampleCompareOffset(const Sampler &sampler, float ref, float s, float lod, int32_t offset) const;

    using TextureLevelPyramid::clearLevel;
    using TextureLevelPyramid::getFormat;
    using TextureLevelPyramid::getLevel;
    using TextureLevelPyramid::getNumLevels;
    using TextureLevelPyramid::isLevelEmpty;

    Texture1D &operator=(const Texture1D &other);

    operator Texture1DView(void) const
    {
        return m_view;
    }

private:
    int m_width;
    Texture1DView m_view;
} DE_WARN_UNUSED_TYPE;

inline Vec4 Texture1D::sample(const Sampler &sampler, float s, float lod) const
{
    return m_view.sample(sampler, s, lod);
}

inline Vec4 Texture1D::sampleOffset(const Sampler &sampler, float s, float lod, int32_t offset) const
{
    return m_view.sampleOffset(sampler, s, lod, offset);
}

inline float Texture1D::sampleCompare(const Sampler &sampler, float ref, float s, float lod) const
{
    return m_view.sampleCompare(sampler, ref, s, lod);
}

inline float Texture1D::sampleCompareOffset(const Sampler &sampler, float ref, float s, float lod, int32_t offset) const
{
    return m_view.sampleCompareOffset(sampler, ref, s, lod, offset);
}

/*--------------------------------------------------------------------*//*!
 * \brief 1D Array Texture View
 *//*--------------------------------------------------------------------*/
class Texture1DArrayView
{
public:
    Texture1DArrayView(int numLevels, const ConstPixelBufferAccess *levels, bool es2 = false,
                       ImageViewMinLodParams *minLodParams = nullptr);

    int getWidth(void) const
    {
        return m_numLevels > 0 ? m_levels[0].getWidth() : 0;
    }
    int getNumLayers(void) const
    {
        return m_numLevels > 0 ? m_levels[0].getHeight() : 0;
    }
    int getNumLevels(void) const
    {
        return m_numLevels;
    }
    const ConstPixelBufferAccess &getLevel(int ndx) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, m_numLevels));
        return m_levels[ndx];
    }
    const ConstPixelBufferAccess *getLevels(void) const
    {
        return m_levels;
    }
    bool isES2(void) const
    {
        return false;
    }

    Vec4 sample(const Sampler &sampler, float s, float t, float lod) const;
    Vec4 sampleOffset(const Sampler &sampler, float s, float t, float lod, int32_t offset) const;
    float sampleCompare(const Sampler &sampler, float ref, float s, float t, float lod) const;
    float sampleCompareOffset(const Sampler &sampler, float ref, float s, float t, float lod, int32_t offset) const;

    ImageViewMinLodParams *getImageViewMinLodParams(void) const
    {
        return nullptr;
    }

protected:
    int selectLayer(float r) const;

    int m_numLevels;
    const ConstPixelBufferAccess *m_levels;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief 1D Array Texture reference implementation
 *//*--------------------------------------------------------------------*/
class Texture1DArray : private TextureLevelPyramid
{
public:
    Texture1DArray(const TextureFormat &format, int width, int numLayers);
    Texture1DArray(const Texture1DArray &other);
    ~Texture1DArray(void);

    int getWidth(void) const
    {
        return m_width;
    }
    int getNumLayers(void) const
    {
        return m_numLayers;
    }

    void allocLevel(int levelNdx);

    using TextureLevelPyramid::clearLevel;
    using TextureLevelPyramid::getFormat;
    using TextureLevelPyramid::getLevel;
    using TextureLevelPyramid::getNumLevels;
    using TextureLevelPyramid::isLevelEmpty;

    Vec4 sample(const Sampler &sampler, float s, float t, float lod) const;
    Vec4 sampleOffset(const Sampler &sampler, float s, float t, float lod, int32_t offset) const;
    float sampleCompare(const Sampler &sampler, float ref, float s, float t, float lod) const;
    float sampleCompareOffset(const Sampler &sampler, float ref, float s, float t, float lod, int32_t offset) const;

    Texture1DArray &operator=(const Texture1DArray &other);

    operator Texture1DArrayView(void) const
    {
        return m_view;
    }

private:
    int m_width;
    int m_numLayers;
    Texture1DArrayView m_view;
} DE_WARN_UNUSED_TYPE;

inline Vec4 Texture1DArray::sample(const Sampler &sampler, float s, float t, float lod) const
{
    return m_view.sample(sampler, s, t, lod);
}

inline Vec4 Texture1DArray::sampleOffset(const Sampler &sampler, float s, float t, float lod, int32_t offset) const
{
    return m_view.sampleOffset(sampler, s, t, lod, offset);
}

inline float Texture1DArray::sampleCompare(const Sampler &sampler, float ref, float s, float t, float lod) const
{
    return m_view.sampleCompare(sampler, ref, s, t, lod);
}

inline float Texture1DArray::sampleCompareOffset(const Sampler &sampler, float ref, float s, float t, float lod,
                                                 int32_t offset) const
{
    return m_view.sampleCompareOffset(sampler, ref, s, t, lod, offset);
}

/*--------------------------------------------------------------------*//*!
 * \brief 2D Array Texture View
 *//*--------------------------------------------------------------------*/
class Texture2DArrayView
{
public:
    Texture2DArrayView(int numLevels, const ConstPixelBufferAccess *levels, bool es2 = false,
                       ImageViewMinLodParams *minLodParams = nullptr);

    int getWidth(void) const
    {
        return m_numLevels > 0 ? m_levels[0].getWidth() : 0;
    }
    int getHeight(void) const
    {
        return m_numLevels > 0 ? m_levels[0].getHeight() : 0;
    }
    int getNumLayers(void) const
    {
        return m_numLevels > 0 ? m_levels[0].getDepth() : 0;
    }
    int getNumLevels(void) const
    {
        return m_numLevels;
    }
    const ConstPixelBufferAccess &getLevel(int ndx) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, m_numLevels));
        return m_levels[ndx];
    }
    const ConstPixelBufferAccess *getLevels(void) const
    {
        return m_levels;
    }
    bool isES2(void) const
    {
        return false;
    }

    Vec4 sample(const Sampler &sampler, float s, float t, float r, float lod) const;
    Vec4 sampleOffset(const Sampler &sampler, float s, float t, float r, float lod, const IVec2 &offset) const;
    float sampleCompare(const Sampler &sampler, float ref, float s, float t, float r, float lod) const;
    float sampleCompareOffset(const Sampler &sampler, float ref, float s, float t, float r, float lod,
                              const IVec2 &offset) const;

    Vec4 gatherOffsets(const Sampler &sampler, float s, float t, float r, int componentNdx,
                       const IVec2 (&offsets)[4]) const;
    Vec4 gatherOffsetsCompare(const Sampler &sampler, float ref, float s, float t, float r,
                              const IVec2 (&offsets)[4]) const;

    ImageViewMinLodParams *getImageViewMinLodParams(void) const
    {
        return nullptr;
    }

protected:
    int selectLayer(float r) const;

    int m_numLevels;
    const ConstPixelBufferAccess *m_levels;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief 2D Array Texture reference implementation
 *//*--------------------------------------------------------------------*/
class Texture2DArray : private TextureLevelPyramid
{
public:
    Texture2DArray(const TextureFormat &format, int width, int height, int numLayers);
    Texture2DArray(const Texture2DArray &other);
    ~Texture2DArray(void);

    int getWidth(void) const
    {
        return m_width;
    }
    int getHeight(void) const
    {
        return m_height;
    }
    int getNumLayers(void) const
    {
        return m_numLayers;
    }

    void allocLevel(int levelNdx);

    using TextureLevelPyramid::clearLevel;
    using TextureLevelPyramid::getFormat;
    using TextureLevelPyramid::getLevel;
    using TextureLevelPyramid::getNumLevels;
    using TextureLevelPyramid::isLevelEmpty;

    Vec4 sample(const Sampler &sampler, float s, float t, float r, float lod) const;
    Vec4 sampleOffset(const Sampler &sampler, float s, float t, float r, float lod, const IVec2 &offset) const;
    float sampleCompare(const Sampler &sampler, float ref, float s, float t, float r, float lod) const;
    float sampleCompareOffset(const Sampler &sampler, float ref, float s, float t, float r, float lod,
                              const IVec2 &offset) const;

    Vec4 gatherOffsets(const Sampler &sampler, float s, float t, float r, int componentNdx,
                       const IVec2 (&offsets)[4]) const;
    Vec4 gatherOffsetsCompare(const Sampler &sampler, float ref, float s, float t, float r,
                              const IVec2 (&offsets)[4]) const;

    Texture2DArray &operator=(const Texture2DArray &other);

    operator Texture2DArrayView(void) const
    {
        return m_view;
    }

private:
    int m_width;
    int m_height;
    int m_numLayers;
    Texture2DArrayView m_view;
} DE_WARN_UNUSED_TYPE;

inline Vec4 Texture2DArray::sample(const Sampler &sampler, float s, float t, float r, float lod) const
{
    return m_view.sample(sampler, s, t, r, lod);
}

inline Vec4 Texture2DArray::sampleOffset(const Sampler &sampler, float s, float t, float r, float lod,
                                         const IVec2 &offset) const
{
    return m_view.sampleOffset(sampler, s, t, r, lod, offset);
}

inline float Texture2DArray::sampleCompare(const Sampler &sampler, float ref, float s, float t, float r,
                                           float lod) const
{
    return m_view.sampleCompare(sampler, ref, s, t, r, lod);
}

inline float Texture2DArray::sampleCompareOffset(const Sampler &sampler, float ref, float s, float t, float r,
                                                 float lod, const IVec2 &offset) const
{
    return m_view.sampleCompareOffset(sampler, ref, s, t, r, lod, offset);
}

inline Vec4 Texture2DArray::gatherOffsets(const Sampler &sampler, float s, float t, float r, int componentNdx,
                                          const IVec2 (&offsets)[4]) const
{
    return m_view.gatherOffsets(sampler, s, t, r, componentNdx, offsets);
}

inline Vec4 Texture2DArray::gatherOffsetsCompare(const Sampler &sampler, float ref, float s, float t, float r,
                                                 const IVec2 (&offsets)[4]) const
{
    return m_view.gatherOffsetsCompare(sampler, ref, s, t, r, offsets);
}

/*--------------------------------------------------------------------*//*!
 * \brief 3D Texture View
 *//*--------------------------------------------------------------------*/
class Texture3DView
{
public:
    Texture3DView(int numLevels, const ConstPixelBufferAccess *levels, bool es2 = false,
                  ImageViewMinLodParams *minLodParams = nullptr);

    int getWidth(void) const
    {
        return m_numLevels > 0 ? m_levels[0].getWidth() : 0;
    }
    int getHeight(void) const
    {
        return m_numLevels > 0 ? m_levels[0].getHeight() : 0;
    }
    int getDepth(void) const
    {
        return m_numLevels > 0 ? m_levels[0].getDepth() : 0;
    }
    int getNumLevels(void) const
    {
        return m_numLevels;
    }
    const ConstPixelBufferAccess &getLevel(int ndx) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, m_numLevels));
        return m_levels[ndx];
    }
    const ConstPixelBufferAccess *getLevels(void) const
    {
        return m_levels;
    }
    bool isES2(void) const
    {
        return m_es2;
    }

    Vec4 sample(const Sampler &sampler, float s, float t, float r, float lod) const;
    Vec4 sampleOffset(const Sampler &sampler, float s, float t, float r, float lod, const IVec3 &offset) const;

    ImageViewMinLodParams *getImageViewMinLodParams(void) const
    {
        return m_minLodParams;
    }

protected:
    int m_numLevels;
    const ConstPixelBufferAccess *m_levels;
    bool m_es2;
    ImageViewMinLodParams *m_minLodParams;

} DE_WARN_UNUSED_TYPE;

inline Vec4 Texture3DView::sample(const Sampler &sampler, float s, float t, float r, float lod) const
{
    return sampleLevelArray3D(m_levels, m_numLevels, sampler, s, t, r, lod, m_minLodParams);
}

inline Vec4 Texture3DView::sampleOffset(const Sampler &sampler, float s, float t, float r, float lod,
                                        const IVec3 &offset) const
{
    return sampleLevelArray3DOffset(m_levels, m_numLevels, sampler, s, t, r, lod, offset, m_minLodParams);
}

/*--------------------------------------------------------------------*//*!
 * \brief 3D Texture reference implementation
 *//*--------------------------------------------------------------------*/
class Texture3D : private TextureLevelPyramid
{
public:
    Texture3D(const TextureFormat &format, int width, int height, int depth);
    Texture3D(const Texture3D &other);
    ~Texture3D(void);

    int getWidth(void) const
    {
        return m_width;
    }
    int getHeight(void) const
    {
        return m_height;
    }
    int getDepth(void) const
    {
        return m_depth;
    }

    void allocLevel(int levelNdx);

    using TextureLevelPyramid::clearLevel;
    using TextureLevelPyramid::getFormat;
    using TextureLevelPyramid::getLevel;
    using TextureLevelPyramid::getNumLevels;
    using TextureLevelPyramid::isLevelEmpty;

    Vec4 sample(const Sampler &sampler, float s, float t, float r, float lod) const;
    Vec4 sampleOffset(const Sampler &sampler, float s, float t, float r, float lod, const IVec3 &offset) const;

    Texture3D &operator=(const Texture3D &other);

    operator Texture3DView(void) const
    {
        return m_view;
    }

private:
    int m_width;
    int m_height;
    int m_depth;
    Texture3DView m_view;
} DE_WARN_UNUSED_TYPE;

inline Vec4 Texture3D::sample(const Sampler &sampler, float s, float t, float r, float lod) const
{
    return m_view.sample(sampler, s, t, r, lod);
}

inline Vec4 Texture3D::sampleOffset(const Sampler &sampler, float s, float t, float r, float lod,
                                    const IVec3 &offset) const
{
    return m_view.sampleOffset(sampler, s, t, r, lod, offset);
}

/*--------------------------------------------------------------------*//*!
 * \brief Cube Map Array Texture View
 *//*--------------------------------------------------------------------*/
class TextureCubeArrayView
{
public:
    TextureCubeArrayView(int numLevels, const ConstPixelBufferAccess *levels, bool es2 = false,
                         ImageViewMinLodParams *minLodParams = nullptr);

    int getSize(void) const
    {
        return m_numLevels > 0 ? m_levels[0].getWidth() : 0;
    }
    int getDepth(void) const
    {
        return m_numLevels > 0 ? m_levels[0].getDepth() : 0;
    }
    int getNumLayers(void) const
    {
        return getDepth() / 6;
    }
    int getNumLevels(void) const
    {
        return m_numLevels;
    }
    const ConstPixelBufferAccess &getLevel(int ndx) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, m_numLevels));
        return m_levels[ndx];
    }
    const ConstPixelBufferAccess *getLevels(void) const
    {
        return m_levels;
    }
    bool isES2(void) const
    {
        return false;
    }

    Vec4 sample(const Sampler &sampler, float s, float t, float r, float q, float lod) const;
    Vec4 sampleOffset(const Sampler &sampler, float s, float t, float r, float q, float lod, const IVec2 &offset) const;
    float sampleCompare(const Sampler &sampler, float ref, float s, float t, float r, float q, float lod) const;
    float sampleCompareOffset(const Sampler &sampler, float ref, float s, float t, float r, float q, float lod,
                              const IVec2 &offset) const;

    ImageViewMinLodParams *getImageViewMinLodParams(void) const
    {
        return nullptr;
    }

protected:
    int selectLayer(float q) const;

    int m_numLevels;
    const ConstPixelBufferAccess *m_levels;
} DE_WARN_UNUSED_TYPE;

/*--------------------------------------------------------------------*//*!
 * \brief Cube Map Array Texture reference implementation
 *//*--------------------------------------------------------------------*/
class TextureCubeArray : private TextureLevelPyramid
{
public:
    TextureCubeArray(const TextureFormat &format, int size, int depth);
    TextureCubeArray(const TextureCubeArray &other);
    ~TextureCubeArray(void);

    int getSize(void) const
    {
        return m_size;
    }
    int getDepth(void) const
    {
        return m_depth;
    }

    void allocLevel(int levelNdx);

    using TextureLevelPyramid::clearLevel;
    using TextureLevelPyramid::getFormat;
    using TextureLevelPyramid::getLevel;
    using TextureLevelPyramid::getNumLevels;
    using TextureLevelPyramid::isLevelEmpty;

    Vec4 sample(const Sampler &sampler, float s, float t, float r, float q, float lod) const;
    Vec4 sampleOffset(const Sampler &sampler, float s, float t, float r, float q, float lod, const IVec2 &offset) const;
    float sampleCompare(const Sampler &sampler, float ref, float s, float t, float r, float q, float lod) const;
    float sampleCompareOffset(const Sampler &sampler, float ref, float s, float t, float r, float q, float lod,
                              const IVec2 &offset) const;

    TextureCubeArray &operator=(const TextureCubeArray &other);

    operator TextureCubeArrayView(void) const
    {
        return m_view;
    }

private:
    int m_size;
    int m_depth;
    TextureCubeArrayView m_view;
} DE_WARN_UNUSED_TYPE;

inline Vec4 TextureCubeArray::sample(const Sampler &sampler, float s, float t, float r, float q, float lod) const
{
    return m_view.sample(sampler, s, t, r, q, lod);
}

inline Vec4 TextureCubeArray::sampleOffset(const Sampler &sampler, float s, float t, float r, float q, float lod,
                                           const IVec2 &offset) const
{
    return m_view.sampleOffset(sampler, s, t, r, q, lod, offset);
}

inline float TextureCubeArray::sampleCompare(const Sampler &sampler, float ref, float s, float t, float r, float q,
                                             float lod) const
{
    return m_view.sampleCompare(sampler, ref, s, t, r, q, lod);
}

inline float TextureCubeArray::sampleCompareOffset(const Sampler &sampler, float ref, float s, float t, float r,
                                                   float q, float lod, const IVec2 &offset) const
{
    return m_view.sampleCompareOffset(sampler, ref, s, t, r, q, lod, offset);
}

// Stream operators.
std::ostream &operator<<(std::ostream &str, TextureFormat::ChannelOrder order);
std::ostream &operator<<(std::ostream &str, TextureFormat::ChannelType type);
std::ostream &operator<<(std::ostream &str, TextureFormat format);
std::ostream &operator<<(std::ostream &str, CubeFace face);
std::ostream &operator<<(std::ostream &str, const ConstPixelBufferAccess &access);

} // namespace tcu

#endif // _TCUTEXTURE_HPP
