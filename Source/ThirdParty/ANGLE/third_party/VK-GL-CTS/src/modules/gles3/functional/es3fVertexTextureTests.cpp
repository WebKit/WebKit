/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
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
 * \brief Vertex texture tests.
 *//*--------------------------------------------------------------------*/

#include "es3fVertexTextureTests.hpp"
#include "glsTextureTestUtil.hpp"
#include "gluTexture.hpp"
#include "gluPixelTransfer.hpp"
#include "gluTextureUtil.hpp"
#include "tcuVector.hpp"
#include "tcuMatrix.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deMath.h"
#include "deRandom.hpp"
#include "deString.h"

#include <string>
#include <vector>

#include <limits>

#include "glw.h"

using std::string;
using std::vector;
using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;
using tcu::Mat3;
using tcu::TestLog;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

namespace deqp
{

using namespace gls::TextureTestUtil;
using namespace glu::TextureTestUtil;

using glu::TextureTestUtil::TEXTURETYPE_2D;
using glu::TextureTestUtil::TEXTURETYPE_2D_ARRAY;
using glu::TextureTestUtil::TEXTURETYPE_3D;
using glu::TextureTestUtil::TEXTURETYPE_CUBE;

namespace gles3
{
namespace Functional
{

static const int WIDTH_2D_ARRAY  = 128;
static const int HEIGHT_2D_ARRAY = 128;
static const int LAYERS_2D_ARRAY = 8;

static const int WIDTH_3D  = 64;
static const int HEIGHT_3D = 64;
static const int DEPTH_3D  = 64;

// The 2D case draws four images.
static const int MAX_2D_RENDER_WIDTH  = 128 * 2;
static const int MAX_2D_RENDER_HEIGHT = 128 * 2;

// The cube map case draws four 3-by-2 image groups.
static const int MAX_CUBE_RENDER_WIDTH  = 28 * 2 * 3;
static const int MAX_CUBE_RENDER_HEIGHT = 28 * 2 * 2;

static const int MAX_2D_ARRAY_RENDER_WIDTH  = 128 * 2;
static const int MAX_2D_ARRAY_RENDER_HEIGHT = 128 * 2;

static const int MAX_3D_RENDER_WIDTH  = 128 * 2;
static const int MAX_3D_RENDER_HEIGHT = 128 * 2;

static const int GRID_SIZE_2D       = 127;
static const int GRID_SIZE_CUBE     = 63;
static const int GRID_SIZE_2D_ARRAY = 127;
static const int GRID_SIZE_3D       = 127;

// Helpers for making texture coordinates "safe", i.e. move them further from coordinate bounary.

// Moves x towards the closest K+targetFraction, where K is an integer.
// E.g. moveTowardsFraction(x, 0.5f) moves x away from integer boundaries.
static inline float moveTowardsFraction(float x, float targetFraction)
{
    const float strictness = 0.5f;
    DE_ASSERT(0.0f < strictness && strictness <= 1.0f);
    DE_ASSERT(de::inBounds(targetFraction, 0.0f, 1.0f));
    const float y = x + 0.5f - targetFraction;
    return deFloatFloor(y) + deFloatFrac(y) * (1.0f - strictness) + strictness * 0.5f - 0.5f + targetFraction;
}

static inline float safeCoord(float raw, int scale, float fraction)
{
    const float scaleFloat = (float)scale;
    return moveTowardsFraction(raw * scaleFloat, fraction) / scaleFloat;
}

template <int Size>
static inline tcu::Vector<float, Size> safeCoords(const tcu::Vector<float, Size> &raw,
                                                  const tcu::Vector<int, Size> &scale,
                                                  const tcu::Vector<float, Size> &fraction)
{
    tcu::Vector<float, Size> result;
    for (int i = 0; i < Size; i++)
        result[i] = safeCoord(raw[i], scale[i], fraction[i]);
    return result;
}

static inline Vec2 safe2DTexCoords(const Vec2 &raw, const IVec2 &textureSize)
{
    return safeCoords(raw, textureSize, Vec2(0.5f));
}

static inline Vec3 safe2DArrayTexCoords(const Vec3 &raw, const IVec3 &textureSize)
{
    return safeCoords(raw, textureSize, Vec3(0.5f, 0.5f, 0.0f));
}

static inline Vec3 safe3DTexCoords(const Vec3 &raw, const IVec3 &textureSize)
{
    return safeCoords(raw, textureSize, Vec3(0.5f));
}

namespace
{

struct Rect
{
    Rect(int x_, int y_, int w_, int h_) : x(x_), y(y_), w(w_), h(h_)
    {
    }
    IVec2 pos(void) const
    {
        return IVec2(x, y);
    }
    IVec2 size(void) const
    {
        return IVec2(w, h);
    }

    int x;
    int y;
    int w;
    int h;
};

template <TextureType>
struct TexTypeTcuClass;
template <>
struct TexTypeTcuClass<TEXTURETYPE_2D>
{
    typedef tcu::Texture2D t;
};
template <>
struct TexTypeTcuClass<TEXTURETYPE_CUBE>
{
    typedef tcu::TextureCube t;
};
template <>
struct TexTypeTcuClass<TEXTURETYPE_2D_ARRAY>
{
    typedef tcu::Texture2DArray t;
};
template <>
struct TexTypeTcuClass<TEXTURETYPE_3D>
{
    typedef tcu::Texture3D t;
};

template <TextureType>
struct TexTypeSizeDims;
template <>
struct TexTypeSizeDims<TEXTURETYPE_2D>
{
    enum
    {
        V = 2
    };
};
template <>
struct TexTypeSizeDims<TEXTURETYPE_CUBE>
{
    enum
    {
        V = 2
    };
};
template <>
struct TexTypeSizeDims<TEXTURETYPE_2D_ARRAY>
{
    enum
    {
        V = 3
    };
};
template <>
struct TexTypeSizeDims<TEXTURETYPE_3D>
{
    enum
    {
        V = 3
    };
};

template <TextureType>
struct TexTypeCoordDims;
template <>
struct TexTypeCoordDims<TEXTURETYPE_2D>
{
    enum
    {
        V = 2
    };
};
template <>
struct TexTypeCoordDims<TEXTURETYPE_CUBE>
{
    enum
    {
        V = 3
    };
};
template <>
struct TexTypeCoordDims<TEXTURETYPE_2D_ARRAY>
{
    enum
    {
        V = 3
    };
};
template <>
struct TexTypeCoordDims<TEXTURETYPE_3D>
{
    enum
    {
        V = 3
    };
};

template <TextureType TexType>
struct TexTypeSizeIVec
{
    typedef tcu::Vector<int, TexTypeSizeDims<TexType>::V> t;
};
template <TextureType TexType>
struct TexTypeCoordVec
{
    typedef tcu::Vector<float, TexTypeCoordDims<TexType>::V> t;
};

template <TextureType>
struct TexTypeCoordParams;

template <>
struct TexTypeCoordParams<TEXTURETYPE_2D>
{
    Vec2 scale;
    Vec2 bias;

    TexTypeCoordParams(const Vec2 &scale_, const Vec2 &bias_) : scale(scale_), bias(bias_)
    {
    }
};

template <>
struct TexTypeCoordParams<TEXTURETYPE_CUBE>
{
    Vec2 scale;
    Vec2 bias;
    tcu::CubeFace face;

    TexTypeCoordParams(const Vec2 &scale_, const Vec2 &bias_, tcu::CubeFace face_)
        : scale(scale_)
        , bias(bias_)
        , face(face_)
    {
    }
};

template <>
struct TexTypeCoordParams<TEXTURETYPE_2D_ARRAY>
{
    Mat3 transform;

    TexTypeCoordParams(const Mat3 &transform_) : transform(transform_)
    {
    }
};

template <>
struct TexTypeCoordParams<TEXTURETYPE_3D>
{
    Mat3 transform;

    TexTypeCoordParams(const Mat3 &transform_) : transform(transform_)
    {
    }
};

/*--------------------------------------------------------------------*//*!
 * \brief Quad grid class containing position and texture coordinate data.
 *
 * A quad grid of size S means a grid consisting of S*S quads (S rows and
 * S columns). The quads are rectangles with main axis aligned sides, and
 * each consists of two triangles. Note that although there are only
 * (S+1)*(S+1) distinct vertex positions, there are S*S*4 distinct vertices
 * because we want texture coordinates to be constant across the vertices
 * of a quad (to avoid interpolation issues), and thus each quad needs its
 * own 4 vertices.
 *
 * Pointers returned by get*Ptr() are suitable for gl calls such as
 * glVertexAttribPointer() (for position and tex coord) or glDrawElements()
 * (for indices).
 *//*--------------------------------------------------------------------*/
template <TextureType TexType>
class PosTexCoordQuadGrid
{
private:
    enum
    {
        TEX_COORD_DIMS = TexTypeCoordDims<TexType>::V
    };
    typedef typename TexTypeCoordVec<TexType>::t TexCoordVec;
    typedef typename TexTypeSizeIVec<TexType>::t TexSizeIVec;
    typedef TexTypeCoordParams<TexType> TexCoordParams;

public:
    PosTexCoordQuadGrid(int gridSize, const IVec2 &renderSize, const TexSizeIVec &textureSize,
                        const TexCoordParams &texCoordParams, bool useSafeTexCoords);

    int getSize(void) const
    {
        return m_gridSize;
    }
    Vec4 getQuadLDRU(int col, int row) const; //!< Vec4(leftX, downY, rightX, upY)
    const TexCoordVec &getQuadTexCoord(int col, int row) const;

    int getNumIndices(void) const
    {
        return m_gridSize * m_gridSize * 3 * 2;
    }
    const float *getPositionPtr(void) const
    {
        DE_STATIC_ASSERT(sizeof(Vec2) == 2 * sizeof(float));
        return (float *)&m_positions[0];
    }
    const float *getTexCoordPtr(void) const
    {
        DE_STATIC_ASSERT(sizeof(TexCoordVec) == TEX_COORD_DIMS * (int)sizeof(float));
        return (float *)&m_texCoords[0];
    }
    const uint16_t *getIndexPtr(void) const
    {
        return &m_indices[0];
    }

private:
    void initializeTexCoords(const TexSizeIVec &textureSize, const TexCoordParams &texCoordParams,
                             bool useSafeTexCoords);

    const int m_gridSize;
    vector<Vec2> m_positions;
    vector<TexCoordVec> m_texCoords;
    vector<uint16_t> m_indices;
};

template <TextureType TexType>
Vec4 PosTexCoordQuadGrid<TexType>::getQuadLDRU(int col, int row) const
{
    int ndx00 = (row * m_gridSize + col) * 4;
    int ndx11 = ndx00 + 3;

    return Vec4(m_positions[ndx00].x(), m_positions[ndx00].y(), m_positions[ndx11].x(), m_positions[ndx11].y());
}

template <TextureType TexType>
const typename TexTypeCoordVec<TexType>::t &PosTexCoordQuadGrid<TexType>::getQuadTexCoord(int col, int row) const
{
    return m_texCoords[(row * m_gridSize + col) * 4];
}

template <TextureType TexType>
PosTexCoordQuadGrid<TexType>::PosTexCoordQuadGrid(int gridSize, const IVec2 &renderSize, const TexSizeIVec &textureSize,
                                                  const TexCoordParams &texCoordParams, bool useSafeTexCoords)
    : m_gridSize(gridSize)
{
    DE_ASSERT(m_gridSize > 0 && m_gridSize * m_gridSize <= (int)std::numeric_limits<uint16_t>::max() + 1);

    const float gridSizeFloat = (float)m_gridSize;

    m_positions.reserve(m_gridSize * m_gridSize * 4);
    m_indices.reserve(m_gridSize * m_gridSize * 3 * 2);

    for (int y = 0; y < m_gridSize; y++)
        for (int x = 0; x < m_gridSize; x++)
        {
            float fx0 = (float)(x + 0) / gridSizeFloat;
            float fx1 = (float)(x + 1) / gridSizeFloat;
            float fy0 = (float)(y + 0) / gridSizeFloat;
            float fy1 = (float)(y + 1) / gridSizeFloat;

            Vec2 quadVertices[4] = {Vec2(fx0, fy0), Vec2(fx1, fy0), Vec2(fx0, fy1), Vec2(fx1, fy1)};

            int firstNdx = (int)m_positions.size();

            for (int i = 0; i < DE_LENGTH_OF_ARRAY(quadVertices); i++)
                m_positions.push_back(safeCoords(quadVertices[i], renderSize, Vec2(0.0f)) * 2.0f - 1.0f);

            m_indices.push_back(uint16_t(firstNdx + 0));
            m_indices.push_back(uint16_t(firstNdx + 1));
            m_indices.push_back(uint16_t(firstNdx + 2));

            m_indices.push_back(uint16_t(firstNdx + 1));
            m_indices.push_back(uint16_t(firstNdx + 3));
            m_indices.push_back(uint16_t(firstNdx + 2));
        }

    m_texCoords.reserve(m_gridSize * m_gridSize * 4);
    initializeTexCoords(textureSize, texCoordParams, useSafeTexCoords);

    DE_ASSERT((int)m_positions.size() == m_gridSize * m_gridSize * 4);
    DE_ASSERT((int)m_indices.size() == m_gridSize * m_gridSize * 3 * 2);
    DE_ASSERT((int)m_texCoords.size() == m_gridSize * m_gridSize * 4);
}

template <>
void PosTexCoordQuadGrid<TEXTURETYPE_2D>::initializeTexCoords(const IVec2 &textureSize,
                                                              const TexCoordParams &texCoordParams,
                                                              bool useSafeTexCoords)
{
    DE_ASSERT(m_texCoords.empty());

    const float gridSizeFloat = (float)m_gridSize;

    for (int y = 0; y < m_gridSize; y++)
        for (int x = 0; x < m_gridSize; x++)
        {
            Vec2 rawCoord =
                Vec2((float)x / gridSizeFloat, (float)y / gridSizeFloat) * texCoordParams.scale + texCoordParams.bias;

            for (int i = 0; i < 4; i++)
                m_texCoords.push_back(useSafeTexCoords ? safe2DTexCoords(rawCoord, textureSize) : rawCoord);
        }
}

template <>
void PosTexCoordQuadGrid<TEXTURETYPE_CUBE>::initializeTexCoords(const IVec2 &textureSize,
                                                                const TexCoordParams &texCoordParams,
                                                                bool useSafeTexCoords)
{
    DE_ASSERT(m_texCoords.empty());

    const float gridSizeFloat = (float)m_gridSize;
    vector<float> texBoundaries;
    computeQuadTexCoordCube(texBoundaries, texCoordParams.face);
    const Vec3 coordA  = Vec3(texBoundaries[0], texBoundaries[1], texBoundaries[2]);
    const Vec3 coordB  = Vec3(texBoundaries[3], texBoundaries[4], texBoundaries[5]);
    const Vec3 coordC  = Vec3(texBoundaries[6], texBoundaries[7], texBoundaries[8]);
    const Vec3 coordAB = coordB - coordA;
    const Vec3 coordAC = coordC - coordA;

    for (int y = 0; y < m_gridSize; y++)
        for (int x = 0; x < m_gridSize; x++)
        {
            const Vec2 rawFaceCoord =
                texCoordParams.scale * Vec2((float)x / gridSizeFloat, (float)y / gridSizeFloat) + texCoordParams.bias;
            const Vec2 safeFaceCoord = useSafeTexCoords ? safe2DTexCoords(rawFaceCoord, textureSize) : rawFaceCoord;
            const Vec3 texCoord      = coordA + coordAC * safeFaceCoord.x() + coordAB * safeFaceCoord.y();

            for (int i = 0; i < 4; i++)
                m_texCoords.push_back(texCoord);
        }
}

template <>
void PosTexCoordQuadGrid<TEXTURETYPE_2D_ARRAY>::initializeTexCoords(const IVec3 &textureSize,
                                                                    const TexCoordParams &texCoordParams,
                                                                    bool useSafeTexCoords)
{
    DE_ASSERT(m_texCoords.empty());

    const float gridSizeFloat = (float)m_gridSize;

    for (int y = 0; y < m_gridSize; y++)
        for (int x = 0; x < m_gridSize; x++)
        {
            const Vec3 rawCoord =
                texCoordParams.transform * Vec3((float)x / gridSizeFloat, (float)y / gridSizeFloat, 1.0f);

            for (int i = 0; i < 4; i++)
                m_texCoords.push_back(useSafeTexCoords ? safe2DArrayTexCoords(rawCoord, textureSize) : rawCoord);
        }
}

template <>
void PosTexCoordQuadGrid<TEXTURETYPE_3D>::initializeTexCoords(const IVec3 &textureSize,
                                                              const TexCoordParams &texCoordParams,
                                                              bool useSafeTexCoords)
{
    DE_ASSERT(m_texCoords.empty());

    const float gridSizeFloat = (float)m_gridSize;

    for (int y = 0; y < m_gridSize; y++)
        for (int x = 0; x < m_gridSize; x++)
        {
            Vec3 rawCoord = texCoordParams.transform * Vec3((float)x / gridSizeFloat, (float)y / gridSizeFloat, 1.0f);

            for (int i = 0; i < 4; i++)
                m_texCoords.push_back(useSafeTexCoords ? safe3DTexCoords(rawCoord, textureSize) : rawCoord);
        }
}

} // namespace

static inline bool isLevelNearest(uint32_t filter)
{
    return filter == GL_NEAREST || filter == GL_NEAREST_MIPMAP_NEAREST || filter == GL_NEAREST_MIPMAP_LINEAR;
}

static inline IVec2 getTextureSize(const glu::Texture2D &tex)
{
    const tcu::Texture2D &ref = tex.getRefTexture();
    return IVec2(ref.getWidth(), ref.getHeight());
}

static inline IVec2 getTextureSize(const glu::TextureCube &tex)
{
    const tcu::TextureCube &ref = tex.getRefTexture();
    return IVec2(ref.getSize(), ref.getSize());
}

static inline IVec3 getTextureSize(const glu::Texture2DArray &tex)
{
    const tcu::Texture2DArray &ref = tex.getRefTexture();
    return IVec3(ref.getWidth(), ref.getHeight(), ref.getNumLayers());
}

static inline IVec3 getTextureSize(const glu::Texture3D &tex)
{
    const tcu::Texture3D &ref = tex.getRefTexture();
    return IVec3(ref.getWidth(), ref.getHeight(), ref.getDepth());
}

template <TextureType TexType>
static void setPixelColors(const vector<Vec4> &quadColors, const Rect &region, const PosTexCoordQuadGrid<TexType> &grid,
                           tcu::Surface &dst)
{
    const int gridSize = grid.getSize();

    for (int y = 0; y < gridSize; y++)
        for (int x = 0; x < gridSize; x++)
        {
            const Vec4 color = quadColors[y * gridSize + x];
            const Vec4 ldru  = grid.getQuadLDRU(x, y) * 0.5f + 0.5f; // [-1, 1] -> [0, 1]
            const int ix0    = deCeilFloatToInt32(ldru.x() * (float)region.w - 0.5f);
            const int ix1    = deCeilFloatToInt32(ldru.z() * (float)region.w - 0.5f);
            const int iy0    = deCeilFloatToInt32(ldru.y() * (float)region.h - 0.5f);
            const int iy1    = deCeilFloatToInt32(ldru.w() * (float)region.h - 0.5f);

            for (int iy = iy0; iy < iy1; iy++)
                for (int ix = ix0; ix < ix1; ix++)
                {
                    DE_ASSERT(deInBounds32(ix + region.x, 0, dst.getWidth()));
                    DE_ASSERT(deInBounds32(iy + region.y, 0, dst.getHeight()));

                    dst.setPixel(ix + region.x, iy + region.y, tcu::RGBA(color));
                }
        }
}

static inline Vec4 sample(const tcu::Texture2D &tex, const Vec2 &coord, float lod, const tcu::Sampler &sam)
{
    return tex.sample(sam, coord.x(), coord.y(), lod);
}
static inline Vec4 sample(const tcu::TextureCube &tex, const Vec3 &coord, float lod, const tcu::Sampler &sam)
{
    return tex.sample(sam, coord.x(), coord.y(), coord.z(), lod);
}
static inline Vec4 sample(const tcu::Texture2DArray &tex, const Vec3 &coord, float lod, const tcu::Sampler &sam)
{
    return tex.sample(sam, coord.x(), coord.y(), coord.z(), lod);
}
static inline Vec4 sample(const tcu::Texture3D &tex, const Vec3 &coord, float lod, const tcu::Sampler &sam)
{
    return tex.sample(sam, coord.x(), coord.y(), coord.z(), lod);
}

template <TextureType TexType>
void computeReference(const typename TexTypeTcuClass<TexType>::t &texture, float lod, const tcu::Sampler &sampler,
                      const PosTexCoordQuadGrid<TexType> &grid, tcu::Surface &dst, const Rect &dstRegion)
{
    const int gridSize = grid.getSize();
    vector<Vec4> quadColors(gridSize * gridSize);

    for (int y = 0; y < gridSize; y++)
        for (int x = 0; x < gridSize; x++)
        {
            const int ndx                                     = y * gridSize + x;
            const typename TexTypeCoordVec<TexType>::t &coord = grid.getQuadTexCoord(x, y);

            quadColors[ndx] = sample(texture, coord, lod, sampler);
        }

    setPixelColors(quadColors, dstRegion, grid, dst);
}

static bool compareImages(const glu::RenderContext &renderCtx, tcu::TestLog &log, const tcu::Surface &ref,
                          const tcu::Surface &res)
{
    DE_ASSERT(renderCtx.getRenderTarget().getNumSamples() == 0);

    const tcu::RGBA threshold =
        renderCtx.getRenderTarget().getPixelFormat().getColorThreshold() + tcu::RGBA(15, 15, 15, 15);
    return tcu::pixelThresholdCompare(log, "Result", "Image compare result", ref, res, threshold,
                                      tcu::COMPARE_LOG_RESULT);
}

class Vertex2DTextureCase : public TestCase
{
public:
    Vertex2DTextureCase(Context &testCtx, const char *name, const char *desc, uint32_t minFilter, uint32_t magFilter,
                        uint32_t wrapS, uint32_t wrapT);
    ~Vertex2DTextureCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    typedef PosTexCoordQuadGrid<TEXTURETYPE_2D> Grid;

    Vertex2DTextureCase(const Vertex2DTextureCase &other);
    Vertex2DTextureCase &operator=(const Vertex2DTextureCase &other);

    float calculateLod(const Vec2 &texScale, const Vec2 &dstSize, int textureNdx) const;
    void setupShaderInputs(int textureNdx, float lod, const Grid &grid) const;
    void renderCell(int textureNdx, float lod, const Grid &grid) const;
    void computeReferenceCell(int textureNdx, float lod, const Grid &grid, tcu::Surface &dst,
                              const Rect &dstRegion) const;

    const uint32_t m_minFilter;
    const uint32_t m_magFilter;
    const uint32_t m_wrapS;
    const uint32_t m_wrapT;

    const glu::ShaderProgram *m_program;
    glu::Texture2D *m_textures[2]; // 2 textures, a gradient texture and a grid texture.
};

Vertex2DTextureCase::Vertex2DTextureCase(Context &testCtx, const char *name, const char *desc, uint32_t minFilter,
                                         uint32_t magFilter, uint32_t wrapS, uint32_t wrapT)
    : TestCase(testCtx, tcu::NODETYPE_SELF_VALIDATE, name, desc)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_program(nullptr)
{
    m_textures[0] = nullptr;
    m_textures[1] = nullptr;
}

Vertex2DTextureCase::~Vertex2DTextureCase(void)
{
    Vertex2DTextureCase::deinit();
}

void Vertex2DTextureCase::init(void)
{
    const char *const vertexShader = "#version 300 es\n"
                                     "in highp vec2 a_position;\n"
                                     "in highp vec2 a_texCoord;\n"
                                     "uniform highp sampler2D u_texture;\n"
                                     "uniform highp float u_lod;\n"
                                     "out mediump vec4 v_color;\n"
                                     "\n"
                                     "void main()\n"
                                     "{\n"
                                     "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                                     "    v_color = textureLod(u_texture, a_texCoord, u_lod);\n"
                                     "}\n";

    const char *const fragmentShader = "#version 300 es\n"
                                       "layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
                                       "in mediump vec4 v_color;\n"
                                       "\n"
                                       "void main()\n"
                                       "{\n"
                                       "    dEQP_FragColor = v_color;\n"
                                       "}\n";

    if (m_context.getRenderTarget().getNumSamples() != 0)
        throw tcu::NotSupportedError("MSAA config not supported by this test");

    DE_ASSERT(!m_program);
    m_program =
        new glu::ShaderProgram(m_context.getRenderContext(), glu::makeVtxFragSources(vertexShader, fragmentShader));

    if (!m_program->isOk())
    {
        m_testCtx.getLog() << *m_program;

        GLint maxVertexTextures;
        glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &maxVertexTextures);

        if (maxVertexTextures < 1)
            throw tcu::NotSupportedError("Vertex texture image units not supported", "", __FILE__, __LINE__);
        else
            TCU_FAIL("Failed to compile shader");
    }

    // Make the textures.
    try
    {
        // Compute suitable power-of-two sizes (for mipmaps).
        const int texWidth  = 1 << deLog2Ceil32(MAX_2D_RENDER_WIDTH / 2);
        const int texHeight = 1 << deLog2Ceil32(MAX_2D_RENDER_HEIGHT / 2);

        for (int i = 0; i < 2; i++)
        {
            DE_ASSERT(!m_textures[i]);
            m_textures[i] =
                new glu::Texture2D(m_context.getRenderContext(), GL_RGB, GL_UNSIGNED_BYTE, texWidth, texHeight);
        }

        const bool mipmaps                   = (deIsPowerOfTwo32(texWidth) && deIsPowerOfTwo32(texHeight));
        const int numLevels                  = mipmaps ? deLog2Floor32(de::max(texWidth, texHeight)) + 1 : 1;
        const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(m_textures[0]->getRefTexture().getFormat());
        const Vec4 cBias                     = fmtInfo.valueMin;
        const Vec4 cScale                    = fmtInfo.valueMax - fmtInfo.valueMin;

        // Fill first with gradient texture.
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            const Vec4 gMin = Vec4(-0.5f, -0.5f, -0.5f, 2.0f) * cScale + cBias;
            const Vec4 gMax = Vec4(1.0f, 1.0f, 1.0f, 0.0f) * cScale + cBias;

            m_textures[0]->getRefTexture().allocLevel(levelNdx);
            tcu::fillWithComponentGradients(m_textures[0]->getRefTexture().getLevel(levelNdx), gMin, gMax);
        }

        // Fill second with grid texture.
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            const uint32_t step   = 0x00ffffff / numLevels;
            const uint32_t rgb    = step * levelNdx;
            const uint32_t colorA = 0xff000000 | rgb;
            const uint32_t colorB = 0xff000000 | ~rgb;

            m_textures[1]->getRefTexture().allocLevel(levelNdx);
            tcu::fillWithGrid(m_textures[1]->getRefTexture().getLevel(levelNdx), 4,
                              tcu::RGBA(colorA).toVec() * cScale + cBias, tcu::RGBA(colorB).toVec() * cScale + cBias);
        }

        // Upload.
        for (int i = 0; i < 2; i++)
            m_textures[i]->upload();
    }
    catch (const std::exception &)
    {
        // Clean up to save memory.
        Vertex2DTextureCase::deinit();
        throw;
    }
}

void Vertex2DTextureCase::deinit(void)
{
    for (int i = 0; i < 2; i++)
    {
        delete m_textures[i];
        m_textures[i] = nullptr;
    }

    delete m_program;
    m_program = nullptr;
}

float Vertex2DTextureCase::calculateLod(const Vec2 &texScale, const Vec2 &dstSize, int textureNdx) const
{
    const tcu::Texture2D &refTexture = m_textures[textureNdx]->getRefTexture();
    const Vec2 srcSize               = Vec2((float)refTexture.getWidth(), (float)refTexture.getHeight());
    const Vec2 sizeRatio             = texScale * srcSize / dstSize;

    // \note In this particular case dv/dx and du/dy are zero, simplifying the expression.
    return deFloatLog2(de::max(sizeRatio.x(), sizeRatio.y()));
}

Vertex2DTextureCase::IterateResult Vertex2DTextureCase::iterate(void)
{
    const int viewportWidth  = deMin32(m_context.getRenderTarget().getWidth(), MAX_2D_RENDER_WIDTH);
    const int viewportHeight = deMin32(m_context.getRenderTarget().getHeight(), MAX_2D_RENDER_HEIGHT);

    const int viewportXOffsetMax = m_context.getRenderTarget().getWidth() - viewportWidth;
    const int viewportYOffsetMax = m_context.getRenderTarget().getHeight() - viewportHeight;

    de::Random rnd(deStringHash(getName()));

    const int viewportXOffset = rnd.getInt(0, viewportXOffsetMax);
    const int viewportYOffset = rnd.getInt(0, viewportYOffsetMax);

    glUseProgram(m_program->getProgram());

    // Divide viewport into 4 cells.
    const int leftWidth    = viewportWidth / 2;
    const int rightWidth   = viewportWidth - leftWidth;
    const int bottomHeight = viewportHeight / 2;
    const int topHeight    = viewportHeight - bottomHeight;

    // Clear.
    glClearColor(0.125f, 0.25f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Texture scaling and offsetting vectors.
    const Vec2 texMinScale(+1.8f, +1.8f);
    const Vec2 texMinOffset(-0.3f, -0.2f);
    const Vec2 texMagScale(+0.3f, +0.3f);
    const Vec2 texMagOffset(+0.9f, +0.8f);

    // Surface for the reference image.
    tcu::Surface refImage(viewportWidth, viewportHeight);

    {
        const struct Render
        {
            const Rect region;
            int textureNdx;
            const Vec2 texCoordScale;
            const Vec2 texCoordOffset;
            Render(const Rect &r, int tN, const Vec2 &tS, const Vec2 &tO)
                : region(r)
                , textureNdx(tN)
                , texCoordScale(tS)
                , texCoordOffset(tO)
            {
            }
        } renders[] = {Render(Rect(0, 0, leftWidth, bottomHeight), 0, texMinScale, texMinOffset),
                       Render(Rect(leftWidth, 0, rightWidth, bottomHeight), 0, texMagScale, texMagOffset),
                       Render(Rect(0, bottomHeight, leftWidth, topHeight), 1, texMinScale, texMinOffset),
                       Render(Rect(leftWidth, bottomHeight, rightWidth, topHeight), 1, texMagScale, texMagOffset)};

        for (int renderNdx = 0; renderNdx < DE_LENGTH_OF_ARRAY(renders); renderNdx++)
        {
            const Render &rend = renders[renderNdx];
            const float lod    = calculateLod(rend.texCoordScale, rend.region.size().asFloat(), rend.textureNdx);
            const bool useSafeTexCoords = isLevelNearest(lod > 0.0f ? m_minFilter : m_magFilter);
            const Grid grid(GRID_SIZE_2D, rend.region.size(), getTextureSize(*m_textures[rend.textureNdx]),
                            TexTypeCoordParams<TEXTURETYPE_2D>(rend.texCoordScale, rend.texCoordOffset),
                            useSafeTexCoords);

            glViewport(viewportXOffset + rend.region.x, viewportYOffset + rend.region.y, rend.region.w, rend.region.h);
            renderCell(rend.textureNdx, lod, grid);
            computeReferenceCell(rend.textureNdx, lod, grid, refImage, rend.region);
        }
    }

    // Read back rendered results.
    tcu::Surface resImage(viewportWidth, viewportHeight);
    glu::readPixels(m_context.getRenderContext(), viewportXOffset, viewportYOffset, resImage.getAccess());

    glUseProgram(0);

    // Compare and log.
    {
        const bool isOk = compareImages(m_context.getRenderContext(), m_testCtx.getLog(), refImage, resImage);

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Image comparison failed");
    }

    return STOP;
}

void Vertex2DTextureCase::setupShaderInputs(int textureNdx, float lod, const Grid &grid) const
{
    const uint32_t programID = m_program->getProgram();

    // SETUP ATTRIBUTES.

    {
        const int positionLoc = glGetAttribLocation(programID, "a_position");
        if (positionLoc != -1)
        {
            glEnableVertexAttribArray(positionLoc);
            glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, grid.getPositionPtr());
        }
    }

    {
        const int texCoordLoc = glGetAttribLocation(programID, "a_texCoord");
        if (texCoordLoc != -1)
        {
            glEnableVertexAttribArray(texCoordLoc);
            glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, grid.getTexCoordPtr());
        }
    }

    // SETUP UNIFORMS.

    {
        const int lodLoc = glGetUniformLocation(programID, "u_lod");
        if (lodLoc != -1)
            glUniform1f(lodLoc, lod);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textures[textureNdx]->getGLTexture());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrapT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_magFilter);

    {
        const int texLoc = glGetUniformLocation(programID, "u_texture");
        if (texLoc != -1)
            glUniform1i(texLoc, 0);
    }
}

// Renders one sub-image with given parameters.
void Vertex2DTextureCase::renderCell(int textureNdx, float lod, const Grid &grid) const
{
    setupShaderInputs(textureNdx, lod, grid);
    glDrawElements(GL_TRIANGLES, grid.getNumIndices(), GL_UNSIGNED_SHORT, grid.getIndexPtr());
}

void Vertex2DTextureCase::computeReferenceCell(int textureNdx, float lod, const Grid &grid, tcu::Surface &dst,
                                               const Rect &dstRegion) const
{
    computeReference(m_textures[textureNdx]->getRefTexture(), lod,
                     glu::mapGLSampler(m_wrapS, m_wrapT, m_minFilter, m_magFilter), grid, dst, dstRegion);
}

class VertexCubeTextureCase : public TestCase
{
public:
    VertexCubeTextureCase(Context &testCtx, const char *name, const char *desc, uint32_t minFilter, uint32_t magFilter,
                          uint32_t wrapS, uint32_t wrapT);
    ~VertexCubeTextureCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    typedef PosTexCoordQuadGrid<TEXTURETYPE_CUBE> Grid;

    VertexCubeTextureCase(const VertexCubeTextureCase &other);
    VertexCubeTextureCase &operator=(const VertexCubeTextureCase &other);

    float calculateLod(const Vec2 &texScale, const Vec2 &dstSize, int textureNdx) const;
    void setupShaderInputs(int textureNdx, float lod, const Grid &grid) const;
    void renderCell(int textureNdx, float lod, const Grid &grid) const;
    void computeReferenceCell(int textureNdx, float lod, const Grid &grid, tcu::Surface &dst,
                              const Rect &dstRegion) const;

    const uint32_t m_minFilter;
    const uint32_t m_magFilter;
    const uint32_t m_wrapS;
    const uint32_t m_wrapT;

    const glu::ShaderProgram *m_program;
    glu::TextureCube *m_textures[2]; // 2 textures, a gradient texture and a grid texture.
};

VertexCubeTextureCase::VertexCubeTextureCase(Context &testCtx, const char *name, const char *desc, uint32_t minFilter,
                                             uint32_t magFilter, uint32_t wrapS, uint32_t wrapT)
    : TestCase(testCtx, tcu::NODETYPE_SELF_VALIDATE, name, desc)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_program(nullptr)
{
    m_textures[0] = nullptr;
    m_textures[1] = nullptr;
}

VertexCubeTextureCase::~VertexCubeTextureCase(void)
{
    VertexCubeTextureCase::deinit();
}

void VertexCubeTextureCase::init(void)
{
    const char *const vertexShader = "#version 300 es\n"
                                     "in highp vec2 a_position;\n"
                                     "in highp vec3 a_texCoord;\n"
                                     "uniform highp samplerCube u_texture;\n"
                                     "uniform highp float u_lod;\n"
                                     "out mediump vec4 v_color;\n"
                                     "\n"
                                     "void main()\n"
                                     "{\n"
                                     "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                                     "    v_color = textureLod(u_texture, a_texCoord, u_lod);\n"
                                     "}\n";

    const char *const fragmentShader = "#version 300 es\n"
                                       "layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
                                       "in mediump vec4 v_color;\n"
                                       "\n"
                                       "void main()\n"
                                       "{\n"
                                       "    dEQP_FragColor = v_color;\n"
                                       "}\n";

    if (m_context.getRenderTarget().getNumSamples() != 0)
        throw tcu::NotSupportedError("MSAA config not supported by this test");

    DE_ASSERT(!m_program);
    m_program =
        new glu::ShaderProgram(m_context.getRenderContext(), glu::makeVtxFragSources(vertexShader, fragmentShader));

    if (!m_program->isOk())
    {
        m_testCtx.getLog() << *m_program;

        GLint maxVertexTextures;
        glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &maxVertexTextures);

        if (maxVertexTextures < 1)
            throw tcu::NotSupportedError("Vertex texture image units not supported", "", __FILE__, __LINE__);
        else
            TCU_FAIL("Failed to compile shader");
    }

    // Make the textures.
    try
    {
        // Compute suitable power-of-two sizes (for mipmaps).
        const int texWidth  = 1 << deLog2Ceil32(MAX_CUBE_RENDER_WIDTH / 3 / 2);
        const int texHeight = 1 << deLog2Ceil32(MAX_CUBE_RENDER_HEIGHT / 2 / 2);

        DE_ASSERT(texWidth == texHeight);
        DE_UNREF(texHeight);

        for (int i = 0; i < 2; i++)
        {
            DE_ASSERT(!m_textures[i]);
            m_textures[i] = new glu::TextureCube(m_context.getRenderContext(), GL_RGB, GL_UNSIGNED_BYTE, texWidth);
        }

        const bool mipmaps                   = deIsPowerOfTwo32(texWidth) != false;
        const int numLevels                  = mipmaps ? deLog2Floor32(texWidth) + 1 : 1;
        const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(m_textures[0]->getRefTexture().getFormat());
        const Vec4 cBias                     = fmtInfo.valueMin;
        const Vec4 cScale                    = fmtInfo.valueMax - fmtInfo.valueMin;

        // Fill first with gradient texture.
        static const Vec4 gradients[tcu::CUBEFACE_LAST][2] = {
            {Vec4(-1.0f, -1.0f, -1.0f, 2.0f), Vec4(1.0f, 1.0f, 1.0f, 0.0f)}, // negative x
            {Vec4(0.0f, -1.0f, -1.0f, 2.0f), Vec4(1.0f, 1.0f, 1.0f, 0.0f)},  // positive x
            {Vec4(-1.0f, 0.0f, -1.0f, 2.0f), Vec4(1.0f, 1.0f, 1.0f, 0.0f)},  // negative y
            {Vec4(-1.0f, -1.0f, 0.0f, 2.0f), Vec4(1.0f, 1.0f, 1.0f, 0.0f)},  // positive y
            {Vec4(-1.0f, -1.0f, -1.0f, 0.0f), Vec4(1.0f, 1.0f, 1.0f, 1.0f)}, // negative z
            {Vec4(0.0f, 0.0f, 0.0f, 2.0f), Vec4(1.0f, 1.0f, 1.0f, 0.0f)}     // positive z
        };
        for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
        {
            for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
            {
                m_textures[0]->getRefTexture().allocLevel((tcu::CubeFace)face, levelNdx);
                tcu::fillWithComponentGradients(
                    m_textures[0]->getRefTexture().getLevelFace(levelNdx, (tcu::CubeFace)face),
                    gradients[face][0] * cScale + cBias, gradients[face][1] * cScale + cBias);
            }
        }

        // Fill second with grid texture.
        for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
        {
            for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
            {
                const uint32_t step   = 0x00ffffff / (numLevels * tcu::CUBEFACE_LAST);
                const uint32_t rgb    = step * levelNdx * face;
                const uint32_t colorA = 0xff000000 | rgb;
                const uint32_t colorB = 0xff000000 | ~rgb;

                m_textures[1]->getRefTexture().allocLevel((tcu::CubeFace)face, levelNdx);
                tcu::fillWithGrid(m_textures[1]->getRefTexture().getLevelFace(levelNdx, (tcu::CubeFace)face), 4,
                                  tcu::RGBA(colorA).toVec() * cScale + cBias,
                                  tcu::RGBA(colorB).toVec() * cScale + cBias);
            }
        }

        // Upload.
        for (int i = 0; i < 2; i++)
            m_textures[i]->upload();
    }
    catch (const std::exception &)
    {
        // Clean up to save memory.
        VertexCubeTextureCase::deinit();
        throw;
    }
}

void VertexCubeTextureCase::deinit(void)
{
    for (int i = 0; i < 2; i++)
    {
        delete m_textures[i];
        m_textures[i] = nullptr;
    }

    delete m_program;
    m_program = nullptr;
}

float VertexCubeTextureCase::calculateLod(const Vec2 &texScale, const Vec2 &dstSize, int textureNdx) const
{
    const tcu::TextureCube &refTexture = m_textures[textureNdx]->getRefTexture();
    const Vec2 srcSize                 = Vec2((float)refTexture.getSize(), (float)refTexture.getSize());
    const Vec2 sizeRatio               = texScale * srcSize / dstSize;

    // \note In this particular case, dv/dx and du/dy are zero, simplifying the expression.
    return deFloatLog2(de::max(sizeRatio.x(), sizeRatio.y()));
}

VertexCubeTextureCase::IterateResult VertexCubeTextureCase::iterate(void)
{
    const int viewportWidth  = deMin32(m_context.getRenderTarget().getWidth(), MAX_CUBE_RENDER_WIDTH);
    const int viewportHeight = deMin32(m_context.getRenderTarget().getHeight(), MAX_CUBE_RENDER_HEIGHT);

    const int viewportXOffsetMax = m_context.getRenderTarget().getWidth() - viewportWidth;
    const int viewportYOffsetMax = m_context.getRenderTarget().getHeight() - viewportHeight;

    de::Random rnd(deStringHash(getName()));

    const int viewportXOffset = rnd.getInt(0, viewportXOffsetMax);
    const int viewportYOffset = rnd.getInt(0, viewportYOffsetMax);

    glUseProgram(m_program->getProgram());

    // Divide viewport into 4 areas.
    const int leftWidth    = viewportWidth / 2;
    const int rightWidth   = viewportWidth - leftWidth;
    const int bottomHeight = viewportHeight / 2;
    const int topHeight    = viewportHeight - bottomHeight;

    // Clear.
    glClearColor(0.125f, 0.25f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Texture scaling and offsetting vectors.
    const Vec2 texMinScale(1.0f, 1.0f);
    const Vec2 texMinOffset(0.0f, 0.0f);
    const Vec2 texMagScale(0.3f, 0.3f);
    const Vec2 texMagOffset(0.5f, 0.3f);

    // Surface for the reference image.
    tcu::Surface refImage(viewportWidth, viewportHeight);

    // Each of the four areas is divided into 6 cells.
    const int defCellWidth  = viewportWidth / 2 / 3;
    const int defCellHeight = viewportHeight / 2 / 2;

    for (int i = 0; i < tcu::CUBEFACE_LAST; i++)
    {
        const int cellOffsetX      = defCellWidth * (i % 3);
        const int cellOffsetY      = defCellHeight * (i / 3);
        const bool isRightmostCell = i == 2 || i == 5;
        const bool isTopCell       = i >= 3;
        const int leftCellWidth    = isRightmostCell ? leftWidth - cellOffsetX : defCellWidth;
        const int rightCellWidth   = isRightmostCell ? rightWidth - cellOffsetX : defCellWidth;
        const int bottomCellHeight = isTopCell ? bottomHeight - cellOffsetY : defCellHeight;
        const int topCellHeight    = isTopCell ? topHeight - cellOffsetY : defCellHeight;

        const struct Render
        {
            const Rect region;
            int textureNdx;
            const Vec2 texCoordScale;
            const Vec2 texCoordOffset;
            Render(const Rect &r, int tN, const Vec2 &tS, const Vec2 &tO)
                : region(r)
                , textureNdx(tN)
                , texCoordScale(tS)
                , texCoordOffset(tO)
            {
            }
        } renders[] = {Render(Rect(cellOffsetX + 0, cellOffsetY + 0, leftCellWidth, bottomCellHeight), 0, texMinScale,
                              texMinOffset),
                       Render(Rect(cellOffsetX + leftWidth, cellOffsetY + 0, rightCellWidth, bottomCellHeight), 0,
                              texMagScale, texMagOffset),
                       Render(Rect(cellOffsetX + 0, cellOffsetY + bottomHeight, leftCellWidth, topCellHeight), 1,
                              texMinScale, texMinOffset),
                       Render(Rect(cellOffsetX + leftWidth, cellOffsetY + bottomHeight, rightCellWidth, topCellHeight),
                              1, texMagScale, texMagOffset)};

        for (int renderNdx = 0; renderNdx < DE_LENGTH_OF_ARRAY(renders); renderNdx++)
        {
            const Render &rend = renders[renderNdx];
            const float lod    = calculateLod(rend.texCoordScale, rend.region.size().asFloat(), rend.textureNdx);
            const bool useSafeTexCoords = isLevelNearest(lod > 0.0f ? m_minFilter : m_magFilter);
            const Grid grid(
                GRID_SIZE_CUBE, rend.region.size(), getTextureSize(*m_textures[rend.textureNdx]),
                TexTypeCoordParams<TEXTURETYPE_CUBE>(rend.texCoordScale, rend.texCoordOffset, (tcu::CubeFace)i),
                useSafeTexCoords);

            glViewport(viewportXOffset + rend.region.x, viewportYOffset + rend.region.y, rend.region.w, rend.region.h);
            renderCell(rend.textureNdx, lod, grid);
            computeReferenceCell(rend.textureNdx, lod, grid, refImage, rend.region);
        }
    }

    // Read back rendered results.
    tcu::Surface resImage(viewportWidth, viewportHeight);
    glu::readPixels(m_context.getRenderContext(), viewportXOffset, viewportYOffset, resImage.getAccess());

    glUseProgram(0);

    // Compare and log.
    {
        const bool isOk = compareImages(m_context.getRenderContext(), m_testCtx.getLog(), refImage, resImage);

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Image comparison failed");
    }

    return STOP;
}

void VertexCubeTextureCase::setupShaderInputs(int textureNdx, float lod, const Grid &grid) const
{
    const uint32_t programID = m_program->getProgram();

    // SETUP ATTRIBUTES.

    {
        const int positionLoc = glGetAttribLocation(programID, "a_position");
        if (positionLoc != -1)
        {
            glEnableVertexAttribArray(positionLoc);
            glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, grid.getPositionPtr());
        }
    }

    {
        const int texCoordLoc = glGetAttribLocation(programID, "a_texCoord");
        if (texCoordLoc != -1)
        {
            glEnableVertexAttribArray(texCoordLoc);
            glVertexAttribPointer(texCoordLoc, 3, GL_FLOAT, GL_FALSE, 0, grid.getTexCoordPtr());
        }
    }

    // SETUP UNIFORMS.

    {
        const int lodLoc = glGetUniformLocation(programID, "u_lod");
        if (lodLoc != -1)
            glUniform1f(lodLoc, lod);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textures[textureNdx]->getGLTexture());
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, m_wrapS);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, m_wrapT);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, m_minFilter);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, m_magFilter);

    {
        const int texLoc = glGetUniformLocation(programID, "u_texture");
        if (texLoc != -1)
            glUniform1i(texLoc, 0);
    }
}

// Renders one cube face with given parameters.
void VertexCubeTextureCase::renderCell(int textureNdx, float lod, const Grid &grid) const
{
    setupShaderInputs(textureNdx, lod, grid);
    glDrawElements(GL_TRIANGLES, grid.getNumIndices(), GL_UNSIGNED_SHORT, grid.getIndexPtr());
}

// Computes reference for one cube face with given parameters.
void VertexCubeTextureCase::computeReferenceCell(int textureNdx, float lod, const Grid &grid, tcu::Surface &dst,
                                                 const Rect &dstRegion) const
{
    tcu::Sampler sampler    = glu::mapGLSampler(m_wrapS, m_wrapT, m_minFilter, m_magFilter);
    sampler.seamlessCubeMap = true;
    computeReference(m_textures[textureNdx]->getRefTexture(), lod, sampler, grid, dst, dstRegion);
}

class Vertex2DArrayTextureCase : public TestCase
{
public:
    Vertex2DArrayTextureCase(Context &testCtx, const char *name, const char *desc, uint32_t minFilter,
                             uint32_t magFilter, uint32_t wrapS, uint32_t wrapT);
    ~Vertex2DArrayTextureCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    typedef PosTexCoordQuadGrid<TEXTURETYPE_2D_ARRAY> Grid;

    Vertex2DArrayTextureCase(const Vertex2DArrayTextureCase &other);
    Vertex2DArrayTextureCase &operator=(const Vertex2DArrayTextureCase &other);

    float calculateLod(const Mat3 &transf, const Vec2 &dstSize, int textureNdx) const;
    void setupShaderInputs(int textureNdx, float lod, const Grid &grid) const;
    void renderCell(int textureNdx, float lod, const Grid &grid) const;
    void computeReferenceCell(int textureNdx, float lod, const Grid &grid, tcu::Surface &dst,
                              const Rect &dstRegion) const;

    const uint32_t m_minFilter;
    const uint32_t m_magFilter;
    const uint32_t m_wrapS;
    const uint32_t m_wrapT;

    const glu::ShaderProgram *m_program;
    glu::Texture2DArray *m_textures[2]; // 2 textures, a gradient texture and a grid texture.
};

Vertex2DArrayTextureCase::Vertex2DArrayTextureCase(Context &testCtx, const char *name, const char *desc,
                                                   uint32_t minFilter, uint32_t magFilter, uint32_t wrapS,
                                                   uint32_t wrapT)
    : TestCase(testCtx, tcu::NODETYPE_SELF_VALIDATE, name, desc)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_program(nullptr)
{
    m_textures[0] = nullptr;
    m_textures[1] = nullptr;
}

Vertex2DArrayTextureCase::~Vertex2DArrayTextureCase(void)
{
    Vertex2DArrayTextureCase::deinit();
}

void Vertex2DArrayTextureCase::init(void)
{
    const char *const vertexShaderSource = "#version 300 es\n"
                                           "in highp vec2 a_position;\n"
                                           "in highp vec3 a_texCoord;\n"
                                           "uniform highp sampler2DArray u_texture;\n"
                                           "uniform highp float u_lod;\n"
                                           "out mediump vec4 v_color;\n"
                                           "\n"
                                           "void main()\n"
                                           "{\n"
                                           "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                                           "    v_color = textureLod(u_texture, a_texCoord, u_lod);\n"
                                           "}\n";

    const char *const fragmentShaderSource = "#version 300 es\n"
                                             "layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
                                             "in mediump vec4 v_color;\n"
                                             "\n"
                                             "void main()\n"
                                             "{\n"
                                             "    dEQP_FragColor = v_color;\n"
                                             "}\n";

    if (m_context.getRenderTarget().getNumSamples() != 0)
        throw tcu::NotSupportedError("MSAA config not supported by this test");

    // Create shader.

    DE_ASSERT(!m_program);
    m_program = new glu::ShaderProgram(m_context.getRenderContext(),
                                       glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));

    if (!m_program->isOk())
    {
        m_testCtx.getLog() << *m_program;

        GLint maxVertexTextures;
        glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &maxVertexTextures);

        if (maxVertexTextures < 1)
            throw tcu::NotSupportedError("Vertex texture image units not supported", "", __FILE__, __LINE__);
        else
            TCU_FAIL("Failed to compile shader");
    }

    // Make the textures.

    try
    {
        const int texWidth  = WIDTH_2D_ARRAY;
        const int texHeight = HEIGHT_2D_ARRAY;
        const int texLayers = LAYERS_2D_ARRAY;

        for (int i = 0; i < 2; i++)
        {
            DE_ASSERT(!m_textures[i]);
            m_textures[i] = new glu::Texture2DArray(m_context.getRenderContext(), GL_RGB, GL_UNSIGNED_BYTE, texWidth,
                                                    texHeight, texLayers);
        }

        const int numLevels                  = deLog2Floor32(de::max(texWidth, texHeight)) + 1;
        const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(m_textures[0]->getRefTexture().getFormat());
        const Vec4 cBias                     = fmtInfo.valueMin;
        const Vec4 cScale                    = fmtInfo.valueMax - fmtInfo.valueMin;

        // Fill first with gradient texture.
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            const Vec4 gMin = Vec4(-0.5f, -0.5f, -0.5f, 2.0f) * cScale + cBias;
            const Vec4 gMax = Vec4(1.0f, 1.0f, 1.0f, 0.0f) * cScale + cBias;

            m_textures[0]->getRefTexture().allocLevel(levelNdx);
            tcu::fillWithComponentGradients(m_textures[0]->getRefTexture().getLevel(levelNdx), gMin, gMax);
        }

        // Fill second with grid texture.
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            const uint32_t step   = 0x00ffffff / numLevels;
            const uint32_t rgb    = step * levelNdx;
            const uint32_t colorA = 0xff000000 | rgb;
            const uint32_t colorB = 0xff000000 | ~rgb;

            m_textures[1]->getRefTexture().allocLevel(levelNdx);
            tcu::fillWithGrid(m_textures[1]->getRefTexture().getLevel(levelNdx), 4,
                              tcu::RGBA(colorA).toVec() * cScale + cBias, tcu::RGBA(colorB).toVec() * cScale + cBias);
        }

        // Upload.
        for (int i = 0; i < 2; i++)
            m_textures[i]->upload();
    }
    catch (const std::exception &)
    {
        // Clean up to save memory.
        Vertex2DArrayTextureCase::deinit();
        throw;
    }
}

void Vertex2DArrayTextureCase::deinit(void)
{
    for (int i = 0; i < 2; i++)
    {
        delete m_textures[i];
        m_textures[i] = nullptr;
    }

    delete m_program;
    m_program = nullptr;
}

float Vertex2DArrayTextureCase::calculateLod(const Mat3 &transf, const Vec2 &dstSize, int textureNdx) const
{
    const tcu::Texture2DArray &refTexture = m_textures[textureNdx]->getRefTexture();
    const int texWidth                    = refTexture.getWidth();
    const int texHeight                   = refTexture.getHeight();

    // Calculate transformed coordinates of three screen corners.
    const Vec2 trans00 = (transf * Vec3(0.0f, 0.0f, 1.0f)).xy();
    const Vec2 trans01 = (transf * Vec3(0.0f, 1.0f, 1.0f)).xy();
    const Vec2 trans10 = (transf * Vec3(1.0f, 0.0f, 1.0f)).xy();

    // Derivates.
    const float dudx = (trans10.x() - trans00.x()) * (float)texWidth / dstSize.x();
    const float dudy = (trans01.x() - trans00.x()) * (float)texWidth / dstSize.y();
    const float dvdx = (trans10.y() - trans00.y()) * (float)texHeight / dstSize.x();
    const float dvdy = (trans01.y() - trans00.y()) * (float)texHeight / dstSize.y();

    return deFloatLog2(deFloatSqrt(de::max(dudx * dudx + dvdx * dvdx, dudy * dudy + dvdy * dvdy)));
}

Vertex2DArrayTextureCase::IterateResult Vertex2DArrayTextureCase::iterate(void)
{
    const int viewportWidth  = deMin32(m_context.getRenderTarget().getWidth(), MAX_2D_ARRAY_RENDER_WIDTH);
    const int viewportHeight = deMin32(m_context.getRenderTarget().getHeight(), MAX_2D_ARRAY_RENDER_HEIGHT);

    const int viewportXOffsetMax = m_context.getRenderTarget().getWidth() - viewportWidth;
    const int viewportYOffsetMax = m_context.getRenderTarget().getHeight() - viewportHeight;

    de::Random rnd(deStringHash(getName()));

    const int viewportXOffset = rnd.getInt(0, viewportXOffsetMax);
    const int viewportYOffset = rnd.getInt(0, viewportYOffsetMax);

    glUseProgram(m_program->getProgram());

    // Divide viewport into 4 cells.
    const int leftWidth    = viewportWidth / 2;
    const int rightWidth   = viewportWidth - leftWidth;
    const int bottomHeight = viewportHeight / 2;
    const int topHeight    = viewportHeight - bottomHeight;

    // Clear.
    glClearColor(0.125f, 0.25f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Shear by layer count to get all layers visible.
    static const float layerShearTransfData[] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, (float)LAYERS_2D_ARRAY,
                                                 0.0f, 0.0f};

    // Minification and magnification transformations.
    static const float texMinTransfData[] = {2.1f, 0.0f, -0.3f, 0.0f, 2.1f, -0.3f, 0.0f, 0.0f, 1.0f};
    static const float texMagTransfData[] = {0.4f, 0.0f, 0.8f, 0.0f, 0.4f, 0.8f, 0.0f, 0.0f, 1.0f};

    // Transformation matrices for minification and magnification.
    const Mat3 texMinTransf = Mat3(layerShearTransfData) * Mat3(texMinTransfData);
    const Mat3 texMagTransf = Mat3(layerShearTransfData) * Mat3(texMagTransfData);

    // Surface for the reference image.
    tcu::Surface refImage(viewportWidth, viewportHeight);

    {
        const struct Render
        {
            const Rect region;
            int textureNdx;
            const Mat3 texTransform;
            Render(const Rect &r, int tN, const Mat3 &tT) : region(r), textureNdx(tN), texTransform(tT)
            {
            }
        } renders[] = {Render(Rect(0, 0, leftWidth, bottomHeight), 0, texMinTransf),
                       Render(Rect(leftWidth, 0, rightWidth, bottomHeight), 0, texMagTransf),
                       Render(Rect(0, bottomHeight, leftWidth, topHeight), 1, texMinTransf),
                       Render(Rect(leftWidth, bottomHeight, rightWidth, topHeight), 1, texMagTransf)};

        for (int renderNdx = 0; renderNdx < DE_LENGTH_OF_ARRAY(renders); renderNdx++)
        {
            const Render &rend = renders[renderNdx];
            const float lod    = calculateLod(rend.texTransform, rend.region.size().asFloat(), rend.textureNdx);
            const bool useSafeTexCoords = isLevelNearest(lod > 0.0f ? m_minFilter : m_magFilter);
            const Grid grid(GRID_SIZE_2D_ARRAY, rend.region.size(), getTextureSize(*m_textures[rend.textureNdx]),
                            TexTypeCoordParams<TEXTURETYPE_2D_ARRAY>(rend.texTransform), useSafeTexCoords);

            glViewport(viewportXOffset + rend.region.x, viewportYOffset + rend.region.y, rend.region.w, rend.region.h);
            renderCell(rend.textureNdx, lod, grid);
            computeReferenceCell(rend.textureNdx, lod, grid, refImage, rend.region);
        }
    }

    // Read back rendered results.
    tcu::Surface resImage(viewportWidth, viewportHeight);
    glu::readPixels(m_context.getRenderContext(), viewportXOffset, viewportYOffset, resImage.getAccess());

    glUseProgram(0);

    // Compare and log.
    {
        const bool isOk = compareImages(m_context.getRenderContext(), m_testCtx.getLog(), refImage, resImage);

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Image comparison failed");
    }

    return STOP;
}

void Vertex2DArrayTextureCase::setupShaderInputs(int textureNdx, float lod, const Grid &grid) const
{
    const uint32_t programID = m_program->getProgram();

    // SETUP ATTRIBUTES.

    {
        const int positionLoc = glGetAttribLocation(programID, "a_position");
        if (positionLoc != -1)
        {
            glEnableVertexAttribArray(positionLoc);
            glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, grid.getPositionPtr());
        }
    }

    {
        const int texCoordLoc = glGetAttribLocation(programID, "a_texCoord");
        if (texCoordLoc != -1)
        {
            glEnableVertexAttribArray(texCoordLoc);
            glVertexAttribPointer(texCoordLoc, 3, GL_FLOAT, GL_FALSE, 0, grid.getTexCoordPtr());
        }
    }

    // SETUP UNIFORMS.

    {
        const int lodLoc = glGetUniformLocation(programID, "u_lod");
        if (lodLoc != -1)
            glUniform1f(lodLoc, lod);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_textures[textureNdx]->getGLTexture());
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, m_wrapS);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, m_wrapT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, m_minFilter);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, m_magFilter);

    {
        const int texLoc = glGetUniformLocation(programID, "u_texture");
        if (texLoc != -1)
            glUniform1i(texLoc, 0);
    }
}

// Renders one sub-image with given parameters.
void Vertex2DArrayTextureCase::renderCell(int textureNdx, float lod, const Grid &grid) const
{
    setupShaderInputs(textureNdx, lod, grid);
    glDrawElements(GL_TRIANGLES, grid.getNumIndices(), GL_UNSIGNED_SHORT, grid.getIndexPtr());
}

// Computes reference for one sub-image with given parameters.
void Vertex2DArrayTextureCase::computeReferenceCell(int textureNdx, float lod, const Grid &grid, tcu::Surface &dst,
                                                    const Rect &dstRegion) const
{
    computeReference(m_textures[textureNdx]->getRefTexture(), lod,
                     glu::mapGLSampler(m_wrapS, m_wrapT, m_minFilter, m_magFilter), grid, dst, dstRegion);
}

class Vertex3DTextureCase : public TestCase
{
public:
    Vertex3DTextureCase(Context &testCtx, const char *name, const char *desc, uint32_t minFilter, uint32_t magFilter,
                        uint32_t wrapS, uint32_t wrapT, uint32_t wrapR);
    ~Vertex3DTextureCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    typedef PosTexCoordQuadGrid<TEXTURETYPE_3D> Grid;

    Vertex3DTextureCase(const Vertex3DTextureCase &other);
    Vertex3DTextureCase &operator=(const Vertex3DTextureCase &other);

    float calculateLod(const Mat3 &transf, const Vec2 &dstSize, int textureNdx) const;
    void setupShaderInputs(int textureNdx, float lod, const Grid &grid) const;
    void renderCell(int textureNdx, float lod, const Grid &grid) const;
    void computeReferenceCell(int textureNdx, float lod, const Grid &grid, tcu::Surface &dst,
                              const Rect &dstRegion) const;

    const uint32_t m_minFilter;
    const uint32_t m_magFilter;
    const uint32_t m_wrapS;
    const uint32_t m_wrapT;
    const uint32_t m_wrapR;

    const glu::ShaderProgram *m_program;
    glu::Texture3D *m_textures[2]; // 2 textures, a gradient texture and a grid texture.
};

Vertex3DTextureCase::Vertex3DTextureCase(Context &testCtx, const char *name, const char *desc, uint32_t minFilter,
                                         uint32_t magFilter, uint32_t wrapS, uint32_t wrapT, uint32_t wrapR)
    : TestCase(testCtx, tcu::NODETYPE_SELF_VALIDATE, name, desc)
    , m_minFilter(minFilter)
    , m_magFilter(magFilter)
    , m_wrapS(wrapS)
    , m_wrapT(wrapT)
    , m_wrapR(wrapR)
    , m_program(nullptr)
{
    m_textures[0] = nullptr;
    m_textures[1] = nullptr;
}

Vertex3DTextureCase::~Vertex3DTextureCase(void)
{
    Vertex3DTextureCase::deinit();
}

void Vertex3DTextureCase::init(void)
{
    const char *const vertexShaderSource = "#version 300 es\n"
                                           "in highp vec2 a_position;\n"
                                           "in highp vec3 a_texCoord;\n"
                                           "uniform highp sampler3D u_texture;\n"
                                           "uniform highp float u_lod;\n"
                                           "out mediump vec4 v_color;\n"
                                           "\n"
                                           "void main()\n"
                                           "{\n"
                                           "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                                           "    v_color = textureLod(u_texture, a_texCoord, u_lod);\n"
                                           "}\n";

    const char *const fragmentShaderSource = "#version 300 es\n"
                                             "layout(location = 0) out mediump vec4 dEQP_FragColor;\n"
                                             "in mediump vec4 v_color;\n"
                                             "\n"
                                             "void main()\n"
                                             "{\n"
                                             "    dEQP_FragColor = v_color;\n"
                                             "}\n";

    if (m_context.getRenderTarget().getNumSamples() != 0)
        throw tcu::NotSupportedError("MSAA config not supported by this test");

    // Create shader.

    DE_ASSERT(!m_program);
    m_program = new glu::ShaderProgram(m_context.getRenderContext(),
                                       glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));

    if (!m_program->isOk())
    {
        m_testCtx.getLog() << *m_program;

        GLint maxVertexTextures;
        glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &maxVertexTextures);

        if (maxVertexTextures < 1)
            throw tcu::NotSupportedError("Vertex texture image units not supported", "", __FILE__, __LINE__);
        else
            TCU_FAIL("Failed to compile shader");
    }

    // Make the textures.

    try
    {
        const int texWidth  = WIDTH_3D;
        const int texHeight = HEIGHT_3D;
        const int texDepth  = DEPTH_3D;

        for (int i = 0; i < 2; i++)
        {
            DE_ASSERT(!m_textures[i]);
            m_textures[i] = new glu::Texture3D(m_context.getRenderContext(), GL_RGB, GL_UNSIGNED_BYTE, texWidth,
                                               texHeight, texDepth);
        }

        const int numLevels                  = deLog2Floor32(de::max(de::max(texWidth, texHeight), texDepth)) + 1;
        const tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(m_textures[0]->getRefTexture().getFormat());
        const Vec4 cBias                     = fmtInfo.valueMin;
        const Vec4 cScale                    = fmtInfo.valueMax - fmtInfo.valueMin;

        // Fill first with gradient texture.
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            const Vec4 gMin = Vec4(-0.5f, -0.5f, -0.5f, 2.0f) * cScale + cBias;
            const Vec4 gMax = Vec4(1.0f, 1.0f, 1.0f, 0.0f) * cScale + cBias;

            m_textures[0]->getRefTexture().allocLevel(levelNdx);
            tcu::fillWithComponentGradients(m_textures[0]->getRefTexture().getLevel(levelNdx), gMin, gMax);
        }

        // Fill second with grid texture.
        for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
        {
            const uint32_t step   = 0x00ffffff / numLevels;
            const uint32_t rgb    = step * levelNdx;
            const uint32_t colorA = 0xff000000 | rgb;
            const uint32_t colorB = 0xff000000 | ~rgb;

            m_textures[1]->getRefTexture().allocLevel(levelNdx);
            tcu::fillWithGrid(m_textures[1]->getRefTexture().getLevel(levelNdx), 4,
                              tcu::RGBA(colorA).toVec() * cScale + cBias, tcu::RGBA(colorB).toVec() * cScale + cBias);
        }

        // Upload.
        for (int i = 0; i < 2; i++)
            m_textures[i]->upload();
    }
    catch (const std::exception &)
    {
        // Clean up to save memory.
        Vertex3DTextureCase::deinit();
        throw;
    }
}

void Vertex3DTextureCase::deinit(void)
{
    for (int i = 0; i < 2; i++)
    {
        delete m_textures[i];
        m_textures[i] = nullptr;
    }

    delete m_program;
    m_program = nullptr;
}

float Vertex3DTextureCase::calculateLod(const Mat3 &transf, const Vec2 &dstSize, int textureNdx) const
{
    const tcu::Texture3D &refTexture = m_textures[textureNdx]->getRefTexture();
    const int srcWidth               = refTexture.getWidth();
    const int srcHeight              = refTexture.getHeight();
    const int srcDepth               = refTexture.getDepth();

    // Calculate transformed coordinates of three screen corners.
    const Vec3 trans00 = transf * Vec3(0.0f, 0.0f, 1.0f);
    const Vec3 trans01 = transf * Vec3(0.0f, 1.0f, 1.0f);
    const Vec3 trans10 = transf * Vec3(1.0f, 0.0f, 1.0f);

    // Derivates.
    const float dudx = (trans10.x() - trans00.x()) * (float)srcWidth / dstSize.x();
    const float dudy = (trans01.x() - trans00.x()) * (float)srcWidth / dstSize.y();
    const float dvdx = (trans10.y() - trans00.y()) * (float)srcHeight / dstSize.x();
    const float dvdy = (trans01.y() - trans00.y()) * (float)srcHeight / dstSize.y();
    const float dwdx = (trans10.z() - trans00.z()) * (float)srcDepth / dstSize.x();
    const float dwdy = (trans01.z() - trans00.z()) * (float)srcDepth / dstSize.y();

    return deFloatLog2(
        deFloatSqrt(de::max(dudx * dudx + dvdx * dvdx + dwdx * dwdx, dudy * dudy + dvdy * dvdy + dwdy * dwdy)));
}

Vertex3DTextureCase::IterateResult Vertex3DTextureCase::iterate(void)
{
    const int viewportWidth  = deMin32(m_context.getRenderTarget().getWidth(), MAX_3D_RENDER_WIDTH);
    const int viewportHeight = deMin32(m_context.getRenderTarget().getHeight(), MAX_3D_RENDER_HEIGHT);

    const int viewportXOffsetMax = m_context.getRenderTarget().getWidth() - viewportWidth;
    const int viewportYOffsetMax = m_context.getRenderTarget().getHeight() - viewportHeight;

    de::Random rnd(deStringHash(getName()));

    const int viewportXOffset = rnd.getInt(0, viewportXOffsetMax);
    const int viewportYOffset = rnd.getInt(0, viewportYOffsetMax);

    glUseProgram(m_program->getProgram());

    // Divide viewport into 4 cells.
    const int leftWidth    = viewportWidth / 2;
    const int rightWidth   = viewportWidth - leftWidth;
    const int bottomHeight = viewportHeight / 2;
    const int topHeight    = viewportHeight - bottomHeight;

    // Clear.
    glClearColor(0.125f, 0.25f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Shear to get all slices visible.
    static const float depthShearTransfData[] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f};

    // Minification and magnification transformations.
    static const float texMinTransfData[] = {2.2f, 0.0f, -0.3f, 0.0f, 2.2f, -0.3f, 0.0f, 0.0f, 1.0f};
    static const float texMagTransfData[] = {0.4f, 0.0f, 0.8f, 0.0f, 0.4f, 0.8f, 0.0f, 0.0f, 1.0f};

    // Transformation matrices for minification and magnification.
    const Mat3 texMinTransf = Mat3(depthShearTransfData) * Mat3(texMinTransfData);
    const Mat3 texMagTransf = Mat3(depthShearTransfData) * Mat3(texMagTransfData);

    // Surface for the reference image.
    tcu::Surface refImage(viewportWidth, viewportHeight);

    {
        const struct Render
        {
            const Rect region;
            int textureNdx;
            const Mat3 texTransform;
            Render(const Rect &r, int tN, const Mat3 &tT) : region(r), textureNdx(tN), texTransform(tT)
            {
            }
        } renders[] = {Render(Rect(0, 0, leftWidth, bottomHeight), 0, texMinTransf),
                       Render(Rect(leftWidth, 0, rightWidth, bottomHeight), 0, texMagTransf),
                       Render(Rect(0, bottomHeight, leftWidth, topHeight), 1, texMinTransf),
                       Render(Rect(leftWidth, bottomHeight, rightWidth, topHeight), 1, texMagTransf)};

        for (int renderNdx = 0; renderNdx < DE_LENGTH_OF_ARRAY(renders); renderNdx++)
        {
            const Render &rend = renders[renderNdx];
            const float lod    = calculateLod(rend.texTransform, rend.region.size().asFloat(), rend.textureNdx);
            const bool useSafeTexCoords = isLevelNearest(lod > 0.0f ? m_minFilter : m_magFilter);
            const Grid grid(GRID_SIZE_3D, rend.region.size(), getTextureSize(*m_textures[rend.textureNdx]),
                            TexTypeCoordParams<TEXTURETYPE_3D>(rend.texTransform), useSafeTexCoords);

            glViewport(viewportXOffset + rend.region.x, viewportYOffset + rend.region.y, rend.region.w, rend.region.h);
            renderCell(rend.textureNdx, lod, grid);
            computeReferenceCell(rend.textureNdx, lod, grid, refImage, rend.region);
        }
    }

    // Read back rendered results.
    tcu::Surface resImage(viewportWidth, viewportHeight);
    glu::readPixels(m_context.getRenderContext(), viewportXOffset, viewportYOffset, resImage.getAccess());

    glUseProgram(0);

    // Compare and log.
    {
        const bool isOk = compareImages(m_context.getRenderContext(), m_testCtx.getLog(), refImage, resImage);

        m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                isOk ? "Pass" : "Image comparison failed");
    }

    return STOP;
}

void Vertex3DTextureCase::setupShaderInputs(int textureNdx, float lod, const Grid &grid) const
{
    const uint32_t programID = m_program->getProgram();

    // SETUP ATTRIBUTES.

    {
        const int positionLoc = glGetAttribLocation(programID, "a_position");
        if (positionLoc != -1)
        {
            glEnableVertexAttribArray(positionLoc);
            glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, grid.getPositionPtr());
        }
    }

    {
        const int texCoordLoc = glGetAttribLocation(programID, "a_texCoord");
        if (texCoordLoc != -1)
        {
            glEnableVertexAttribArray(texCoordLoc);
            glVertexAttribPointer(texCoordLoc, 3, GL_FLOAT, GL_FALSE, 0, grid.getTexCoordPtr());
        }
    }

    // SETUP UNIFORMS.

    {
        const int lodLoc = glGetUniformLocation(programID, "u_lod");
        if (lodLoc != -1)
            glUniform1f(lodLoc, lod);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, m_textures[textureNdx]->getGLTexture());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, m_wrapS);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, m_wrapT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, m_wrapR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, m_minFilter);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, m_magFilter);

    {
        const int texLoc = glGetUniformLocation(programID, "u_texture");
        if (texLoc != -1)
            glUniform1i(texLoc, 0);
    }
}

// Renders one sub-image with given parameters.
void Vertex3DTextureCase::renderCell(int textureNdx, float lod, const Grid &grid) const
{
    setupShaderInputs(textureNdx, lod, grid);
    glDrawElements(GL_TRIANGLES, grid.getNumIndices(), GL_UNSIGNED_SHORT, grid.getIndexPtr());
}

// Computes reference for one sub-image with given parameters.
void Vertex3DTextureCase::computeReferenceCell(int textureNdx, float lod, const Grid &grid, tcu::Surface &dst,
                                               const Rect &dstRegion) const
{
    computeReference(m_textures[textureNdx]->getRefTexture(), lod,
                     glu::mapGLSampler(m_wrapS, m_wrapT, m_wrapR, m_minFilter, m_magFilter), grid, dst, dstRegion);
}

VertexTextureTests::VertexTextureTests(Context &context) : TestCaseGroup(context, "vertex", "Vertex Texture Tests")
{
}

VertexTextureTests::~VertexTextureTests(void)
{
}

void VertexTextureTests::init(void)
{
    // 2D and cube map groups, and their filtering and wrap sub-groups.
    TestCaseGroup *const group2D      = new TestCaseGroup(m_context, "2d", "2D Vertex Texture Tests");
    TestCaseGroup *const groupCube    = new TestCaseGroup(m_context, "cube", "Cube Map Vertex Texture Tests");
    TestCaseGroup *const group2DArray = new TestCaseGroup(m_context, "2d_array", "2D Array Vertex Texture Tests");
    TestCaseGroup *const group3D      = new TestCaseGroup(m_context, "3d", "3D Vertex Texture Tests");
    TestCaseGroup *const filteringGroup2D =
        new TestCaseGroup(m_context, "filtering", "2D Vertex Texture Filtering Tests");
    TestCaseGroup *const wrapGroup2D = new TestCaseGroup(m_context, "wrap", "2D Vertex Texture Wrap Tests");
    TestCaseGroup *const filteringGroupCube =
        new TestCaseGroup(m_context, "filtering", "Cube Map Vertex Texture Filtering Tests");
    TestCaseGroup *const wrapGroupCube = new TestCaseGroup(m_context, "wrap", "Cube Map Vertex Texture Wrap Tests");
    TestCaseGroup *const filteringGroup2DArray =
        new TestCaseGroup(m_context, "filtering", "2D Array Vertex Texture Filtering Tests");
    TestCaseGroup *const wrapGroup2DArray = new TestCaseGroup(m_context, "wrap", "2D Array Vertex Texture Wrap Tests");
    TestCaseGroup *const filteringGroup3D =
        new TestCaseGroup(m_context, "filtering", "3D Vertex Texture Filtering Tests");
    TestCaseGroup *const wrapGroup3D = new TestCaseGroup(m_context, "wrap", "3D Vertex Texture Wrap Tests");

    group2D->addChild(filteringGroup2D);
    group2D->addChild(wrapGroup2D);
    groupCube->addChild(filteringGroupCube);
    groupCube->addChild(wrapGroupCube);
    group2DArray->addChild(filteringGroup2DArray);
    group2DArray->addChild(wrapGroup2DArray);
    group3D->addChild(filteringGroup3D);
    group3D->addChild(wrapGroup3D);

    addChild(group2D);
    addChild(groupCube);
    addChild(group2DArray);
    addChild(group3D);

    static const struct
    {
        const char *name;
        GLenum mode;
    } wrapModes[] = {{"clamp", GL_CLAMP_TO_EDGE}, {"repeat", GL_REPEAT}, {"mirror", GL_MIRRORED_REPEAT}};

    static const struct
    {
        const char *name;
        GLenum mode;
    } minFilterModes[] = {{"nearest", GL_NEAREST},
                          {"linear", GL_LINEAR},
                          {"nearest_mipmap_nearest", GL_NEAREST_MIPMAP_NEAREST},
                          {"linear_mipmap_nearest", GL_LINEAR_MIPMAP_NEAREST},
                          {"nearest_mipmap_linear", GL_NEAREST_MIPMAP_LINEAR},
                          {"linear_mipmap_linear", GL_LINEAR_MIPMAP_LINEAR}};

    static const struct
    {
        const char *name;
        GLenum mode;
    } magFilterModes[] = {{"nearest", GL_NEAREST}, {"linear", GL_LINEAR}};

#define FOR_EACH(ITERATOR, ARRAY, BODY)                                      \
    for (int ITERATOR = 0; ITERATOR < DE_LENGTH_OF_ARRAY(ARRAY); ITERATOR++) \
    BODY

    // 2D cases.

    FOR_EACH(minFilter, minFilterModes,
             FOR_EACH(magFilter, magFilterModes, FOR_EACH(wrapMode, wrapModes, {
                          const string name = string("") + minFilterModes[minFilter].name + "_" +
                                              magFilterModes[magFilter].name + "_" + wrapModes[wrapMode].name;

                          filteringGroup2D->addChild(new Vertex2DTextureCase(
                              m_context, name.c_str(), "", minFilterModes[minFilter].mode,
                              magFilterModes[magFilter].mode, wrapModes[wrapMode].mode, wrapModes[wrapMode].mode));
                      })))

    FOR_EACH(wrapSMode, wrapModes, FOR_EACH(wrapTMode, wrapModes, {
                 const string name = string("") + wrapModes[wrapSMode].name + "_" + wrapModes[wrapTMode].name;

                 wrapGroup2D->addChild(new Vertex2DTextureCase(m_context, name.c_str(), "", GL_LINEAR_MIPMAP_LINEAR,
                                                               GL_LINEAR, wrapModes[wrapSMode].mode,
                                                               wrapModes[wrapTMode].mode));
             }))

    // Cube map cases.

    FOR_EACH(minFilter, minFilterModes,
             FOR_EACH(magFilter, magFilterModes, FOR_EACH(wrapMode, wrapModes, {
                          const string name = string("") + minFilterModes[minFilter].name + "_" +
                                              magFilterModes[magFilter].name + "_" + wrapModes[wrapMode].name;

                          filteringGroupCube->addChild(new VertexCubeTextureCase(
                              m_context, name.c_str(), "", minFilterModes[minFilter].mode,
                              magFilterModes[magFilter].mode, wrapModes[wrapMode].mode, wrapModes[wrapMode].mode));
                      })))

    FOR_EACH(wrapSMode, wrapModes, FOR_EACH(wrapTMode, wrapModes, {
                 const string name = string("") + wrapModes[wrapSMode].name + "_" + wrapModes[wrapTMode].name;

                 wrapGroupCube->addChild(new VertexCubeTextureCase(m_context, name.c_str(), "", GL_LINEAR_MIPMAP_LINEAR,
                                                                   GL_LINEAR, wrapModes[wrapSMode].mode,
                                                                   wrapModes[wrapTMode].mode));
             }))

    // 2D array cases.

    FOR_EACH(minFilter, minFilterModes,
             FOR_EACH(magFilter, magFilterModes, FOR_EACH(wrapMode, wrapModes, {
                          const string name = string("") + minFilterModes[minFilter].name + "_" +
                                              magFilterModes[magFilter].name + "_" + wrapModes[wrapMode].name;

                          filteringGroup2DArray->addChild(new Vertex2DArrayTextureCase(
                              m_context, name.c_str(), "", minFilterModes[minFilter].mode,
                              magFilterModes[magFilter].mode, wrapModes[wrapMode].mode, wrapModes[wrapMode].mode));
                      })))

    FOR_EACH(wrapSMode, wrapModes, FOR_EACH(wrapTMode, wrapModes, {
                 const string name = string("") + wrapModes[wrapSMode].name + "_" + wrapModes[wrapTMode].name;

                 wrapGroup2DArray->addChild(
                     new Vertex2DArrayTextureCase(m_context, name.c_str(), "", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR,
                                                  wrapModes[wrapSMode].mode, wrapModes[wrapTMode].mode));
             }))

    // 3D cases.

    FOR_EACH(minFilter, minFilterModes, FOR_EACH(magFilter, magFilterModes, FOR_EACH(wrapMode, wrapModes, {
                                                     const string name = string("") + minFilterModes[minFilter].name +
                                                                         "_" + magFilterModes[magFilter].name + "_" +
                                                                         wrapModes[wrapMode].name;

                                                     filteringGroup3D->addChild(new Vertex3DTextureCase(
                                                         m_context, name.c_str(), "", minFilterModes[minFilter].mode,
                                                         magFilterModes[magFilter].mode, wrapModes[wrapMode].mode,
                                                         wrapModes[wrapMode].mode, wrapModes[wrapMode].mode));
                                                 })))

    FOR_EACH(wrapSMode, wrapModes,
             FOR_EACH(wrapTMode, wrapModes, FOR_EACH(wrapRMode, wrapModes, {
                          const string name = string("") + wrapModes[wrapSMode].name + "_" + wrapModes[wrapTMode].name +
                                              "_" + wrapModes[wrapRMode].name;

                          wrapGroup3D->addChild(new Vertex3DTextureCase(
                              m_context, name.c_str(), "", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR,
                              wrapModes[wrapSMode].mode, wrapModes[wrapTMode].mode, wrapModes[wrapRMode].mode));
                      })))
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
