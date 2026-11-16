/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
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
 * \brief Parametrized, long-running stress case.
 *
 * \todo [2013-06-27 nuutti] Do certain things in a cleaner and less
 *                             confusing way, such as the "redundant buffer
 *                             factor" thing in LongStressCase.
 *//*--------------------------------------------------------------------*/

#include "glsLongStressCase.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVector.hpp"
#include "tcuVectorUtil.hpp"
#include "glsTextureTestUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluTextureUtil.hpp"
#include "tcuStringTemplate.hpp"
#include "gluStrUtil.hpp"
#include "gluShaderProgram.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deString.h"
#include "deSharedPtr.hpp"
#include "deClock.h"

#include "glw.h"

#include <limits>
#include <vector>
#include <iomanip>
#include <map>
#include <iomanip>

using de::Random;
using de::SharedPtr;
using de::toString;
using tcu::ConstPixelBufferAccess;
using tcu::CubeFace;
using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;
using tcu::TestLog;
using tcu::TextureFormat;
using tcu::TextureLevel;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

using std::map;
using std::string;
using std::vector;

namespace deqp
{
namespace gls
{

using glu::TextureTestUtil::TextureType;
using glu::TextureTestUtil::TEXTURETYPE_2D;
using glu::TextureTestUtil::TEXTURETYPE_CUBE;

static const float Mi = (float)(1 << 20);

static const uint32_t bufferUsages[] = {GL_STATIC_DRAW, GL_STREAM_DRAW, GL_DYNAMIC_DRAW,

                                        GL_STATIC_READ, GL_STREAM_READ, GL_DYNAMIC_READ,

                                        GL_STATIC_COPY, GL_STREAM_COPY, GL_DYNAMIC_COPY};

static const uint32_t bufferUsagesGLES2[] = {GL_STATIC_DRAW, GL_DYNAMIC_DRAW, GL_STREAM_DRAW};

static const uint32_t bufferTargets[] = {GL_ARRAY_BUFFER,        GL_ELEMENT_ARRAY_BUFFER,

                                         GL_COPY_READ_BUFFER,    GL_COPY_WRITE_BUFFER,         GL_PIXEL_PACK_BUFFER,
                                         GL_PIXEL_UNPACK_BUFFER, GL_TRANSFORM_FEEDBACK_BUFFER, GL_UNIFORM_BUFFER};

static const uint32_t bufferTargetsGLES2[] = {GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER};

static inline int computePixelStore(const TextureFormat &format)
{
    const int pixelSize = format.getPixelSize();
    if (deIsPowerOfTwo32(pixelSize))
        return de::min(pixelSize, 8);
    else
        return 1;
}

static inline int getNumIterations(const tcu::TestContext &testCtx, const int defaultNumIterations)
{
    const int cmdLineVal = testCtx.getCommandLine().getTestIterationCount();
    return cmdLineVal == 0 ? defaultNumIterations : cmdLineVal;
}

static inline float triangleArea(const Vec2 &a, const Vec2 &b, const Vec2 &c)
{
    const Vec2 ab = b - a;
    const Vec2 ac = c - a;
    return 0.5f * tcu::length(ab.x() * ac.y() - ab.y() * ac.x());
}

static inline string mangleShaderNames(const string &source, const string &manglingSuffix)
{
    map<string, string> m;
    m["NS"] = manglingSuffix;
    return tcu::StringTemplate(source.c_str()).specialize(m);
}

template <typename T, int N>
static inline T randomChoose(Random &rnd, const T (&arr)[N])
{
    return rnd.choose<T>(DE_ARRAY_BEGIN(arr), DE_ARRAY_END(arr));
}

static inline int nextDivisible(const int x, const int div)
{
    DE_ASSERT(x >= 0);
    DE_ASSERT(div >= 1);
    return x == 0 ? 0 : x - 1 + div - (x - 1) % div;
}

static inline string getTimeStr(const uint64_t seconds)
{
    const uint64_t m = seconds / 60;
    const uint64_t h = m / 60;
    const uint64_t d = h / 24;
    std::ostringstream res;

    res << d << "d " << h % 24 << "h " << m % 60 << "m " << seconds % 60 << "s";
    return res.str();
}

static inline string probabilityStr(const float prob)
{
    return prob == 0.0f ? "never" : prob == 1.0f ? "ALWAYS" : de::floatToString(prob * 100.0f, 0) + "%";
}

static inline uint32_t randomBufferTarget(Random &rnd, const bool isGLES3)
{
    return isGLES3 ? randomChoose(rnd, bufferTargets) : randomChoose(rnd, bufferTargetsGLES2);
}

static inline uint32_t randomBufferUsage(Random &rnd, const bool isGLES3)
{
    return isGLES3 ? randomChoose(rnd, bufferUsages) : randomChoose(rnd, bufferUsagesGLES2);
}

static inline uint32_t cubeFaceToGLFace(tcu::CubeFace face)
{
    switch (face)
    {
    case tcu::CUBEFACE_NEGATIVE_X:
        return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
    case tcu::CUBEFACE_POSITIVE_X:
        return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    case tcu::CUBEFACE_NEGATIVE_Y:
        return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
    case tcu::CUBEFACE_POSITIVE_Y:
        return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
    case tcu::CUBEFACE_NEGATIVE_Z:
        return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
    case tcu::CUBEFACE_POSITIVE_Z:
        return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
    default:
        DE_ASSERT(false);
        return GL_NONE;
    }
}

#if defined(DE_DEBUG)
static inline bool isMatchingGLInternalFormat(const uint32_t internalFormat, const TextureFormat &texFormat)
{
    switch (internalFormat)
    {
        // Unsized formats.

    case GL_RGBA:
        return texFormat.order == TextureFormat::RGBA &&
               (texFormat.type == TextureFormat::UNORM_INT8 || texFormat.type == TextureFormat::UNORM_SHORT_4444 ||
                texFormat.type == TextureFormat::UNORM_SHORT_5551);

    case GL_RGB:
        return texFormat.order == TextureFormat::RGB &&
               (texFormat.type == TextureFormat::UNORM_INT8 || texFormat.type == TextureFormat::UNORM_SHORT_565);

    case GL_LUMINANCE_ALPHA:
        return texFormat.order == TextureFormat::LA && texFormat.type == TextureFormat::UNORM_INT8;
    case GL_LUMINANCE:
        return texFormat.order == TextureFormat::L && texFormat.type == TextureFormat::UNORM_INT8;
    case GL_ALPHA:
        return texFormat.order == TextureFormat::A && texFormat.type == TextureFormat::UNORM_INT8;

        // Sized formats.

    default:
        return glu::mapGLInternalFormat(internalFormat) == texFormat;
    }
}
#endif // DE_DEBUG

static inline bool compileShader(const uint32_t shaderGL)
{
    glCompileShader(shaderGL);

    int success = GL_FALSE;
    glGetShaderiv(shaderGL, GL_COMPILE_STATUS, &success);

    return success == GL_TRUE;
}

static inline bool linkProgram(const uint32_t programGL)
{
    glLinkProgram(programGL);

    int success = GL_FALSE;
    glGetProgramiv(programGL, GL_LINK_STATUS, &success);

    return success == GL_TRUE;
}

static inline string getShaderInfoLog(const uint32_t shaderGL)
{
    int infoLogLen = 0;
    vector<char> infoLog;
    glGetShaderiv(shaderGL, GL_INFO_LOG_LENGTH, &infoLogLen);
    infoLog.resize(infoLogLen + 1);
    glGetShaderInfoLog(shaderGL, (int)infoLog.size(), nullptr, &infoLog[0]);
    return &infoLog[0];
}

static inline string getProgramInfoLog(const uint32_t programGL)
{
    int infoLogLen = 0;
    vector<char> infoLog;
    glGetProgramiv(programGL, GL_INFO_LOG_LENGTH, &infoLogLen);
    infoLog.resize(infoLogLen + 1);
    glGetProgramInfoLog(programGL, (int)infoLog.size(), nullptr, &infoLog[0]);
    return &infoLog[0];
}

namespace LongStressCaseInternal
{

// A hacky-ish class for drawing text on screen as GL quads.
class DebugInfoRenderer
{
public:
    DebugInfoRenderer(const glu::RenderContext &ctx);
    ~DebugInfoRenderer(void)
    {
        delete m_prog;
    }

    void drawInfo(uint64_t secondsElapsed, int texMem, int maxTexMem, int bufMem, int maxBufMem, int iterNdx);

private:
    DebugInfoRenderer(const DebugInfoRenderer &);
    DebugInfoRenderer &operator=(const DebugInfoRenderer &);

    void render(void);
    void addTextToBuffer(const string &text, int yOffset);

    const glu::RenderContext &m_ctx;
    const glu::ShaderProgram *m_prog;
    vector<float> m_posBuf;
    vector<uint16_t> m_ndxBuf;
};

void DebugInfoRenderer::drawInfo(const uint64_t secondsElapsed, const int texMem, const int maxTexMem, const int bufMem,
                                 const int maxBufMem, const int iterNdx)
{
    const uint64_t m = secondsElapsed / 60;
    const uint64_t h = m / 60;
    const uint64_t d = h / 24;

    {
        std::ostringstream text;

        text << std::setw(2) << std::setfill('0') << d << ":" << std::setw(2) << std::setfill('0') << h % 24 << ":"
             << std::setw(2) << std::setfill('0') << m % 60 << ":" << std::setw(2) << std::setfill('0')
             << secondsElapsed % 60;
        addTextToBuffer(text.str(), 0);
        text.str("");

        text << std::fixed << std::setprecision(2) << (float)texMem / Mi << "/" << (float)maxTexMem / Mi;
        addTextToBuffer(text.str(), 1);
        text.str("");

        text << std::fixed << std::setprecision(2) << (float)bufMem / Mi << "/" << (float)maxBufMem / Mi;
        addTextToBuffer(text.str(), 2);
        text.str("");

        text << std::setw(0) << iterNdx;
        addTextToBuffer(text.str(), 3);
    }

    render();
}

DebugInfoRenderer::DebugInfoRenderer(const glu::RenderContext &ctx) : m_ctx(ctx), m_prog(nullptr)
{
    DE_ASSERT(!m_prog);
    m_prog = new glu::ShaderProgram(ctx, glu::makeVtxFragSources("attribute highp vec2 a_pos;\n"
                                                                 "void main (void)\n"
                                                                 "{\n"
                                                                 "    gl_Position = vec4(a_pos, -1.0, 1.0);\n"
                                                                 "}\n",

                                                                 "void main(void)\n"
                                                                 "{\n"
                                                                 "    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
                                                                 "}\n"));
}

void DebugInfoRenderer::addTextToBuffer(const string &text, const int yOffset)
{
    static const char characters[] = "0123456789.:/";
    const int numCharacters        = DE_LENGTH_OF_ARRAY(characters) - 1; // \note -1 for null byte.
    const int charWid              = 6;
    const int charHei              = 6;
    static const string charsStr(characters);

    static const char font[numCharacters * charWid * charHei + 1] = " #### "
                                                                    "   #  "
                                                                    " #### "
                                                                    "##### "
                                                                    "   #  "
                                                                    "######"
                                                                    " #####"
                                                                    "######"
                                                                    " #### "
                                                                    " #### "
                                                                    "      "
                                                                    "  ##  "
                                                                    "     #"
                                                                    "#    #"
                                                                    "  ##  "
                                                                    "#    #"
                                                                    "     #"
                                                                    "  #   "
                                                                    "#     "
                                                                    "#     "
                                                                    "    # "
                                                                    "#    #"
                                                                    "#    #"
                                                                    "      "
                                                                    "  ##  "
                                                                    "    # "
                                                                    "#    #"
                                                                    "   #  "
                                                                    "    # "
                                                                    "  ### "
                                                                    " #  # "
                                                                    " #### "
                                                                    "# ### "
                                                                    "   #  "
                                                                    " #### "
                                                                    " #####"
                                                                    "      "
                                                                    "      "
                                                                    "   #  "
                                                                    "#    #"
                                                                    "   #  "
                                                                    "   #  "
                                                                    "     #"
                                                                    "######"
                                                                    "     #"
                                                                    "##   #"
                                                                    "  #   "
                                                                    "#    #"
                                                                    "     #"
                                                                    "      "
                                                                    "  ##  "
                                                                    "  #   "
                                                                    "#    #"
                                                                    "   #  "
                                                                    "  #   "
                                                                    "#    #"
                                                                    "    # "
                                                                    "#    #"
                                                                    "#    #"
                                                                    " #    "
                                                                    "#    #"
                                                                    "   ## "
                                                                    "  ##  "
                                                                    "  ##  "
                                                                    " #    "
                                                                    " #### "
                                                                    "  ### "
                                                                    "######"
                                                                    " #### "
                                                                    "    # "
                                                                    " #### "
                                                                    " #### "
                                                                    "#     "
                                                                    " #### "
                                                                    "###   "
                                                                    "  ##  "
                                                                    "      "
                                                                    "#     ";

    for (int ndxInText = 0; ndxInText < (int)text.size(); ndxInText++)
    {
        const int ndxInCharset = (int)charsStr.find(text[ndxInText]);
        DE_ASSERT(ndxInCharset < numCharacters);
        const int fontXStart = ndxInCharset * charWid;

        for (int y = 0; y < charHei; y++)
        {
            float ay = -1.0f + (float)(y + 0 + yOffset * (charHei + 2)) * 0.1f / (float)(charHei + 2);
            float by = -1.0f + (float)(y + 1 + yOffset * (charHei + 2)) * 0.1f / (float)(charHei + 2);
            for (int x = 0; x < charWid; x++)
            {
                // \note Text is mirrored in x direction since on most(?) mobile devices the image is mirrored(?).
                float ax = 1.0f - (float)(x + 0 + ndxInText * (charWid + 2)) * 0.1f / (float)(charWid + 2);
                float bx = 1.0f - (float)(x + 1 + ndxInText * (charWid + 2)) * 0.1f / (float)(charWid + 2);

                if (font[y * numCharacters * charWid + fontXStart + x] != ' ')
                {
                    const int vtxNdx = (int)m_posBuf.size() / 2;

                    m_ndxBuf.push_back(uint16_t(vtxNdx + 0));
                    m_ndxBuf.push_back(uint16_t(vtxNdx + 1));
                    m_ndxBuf.push_back(uint16_t(vtxNdx + 2));

                    m_ndxBuf.push_back(uint16_t(vtxNdx + 2));
                    m_ndxBuf.push_back(uint16_t(vtxNdx + 1));
                    m_ndxBuf.push_back(uint16_t(vtxNdx + 3));

                    m_posBuf.push_back(ax);
                    m_posBuf.push_back(ay);

                    m_posBuf.push_back(bx);
                    m_posBuf.push_back(ay);

                    m_posBuf.push_back(ax);
                    m_posBuf.push_back(by);

                    m_posBuf.push_back(bx);
                    m_posBuf.push_back(by);
                }
            }
        }
    }
}

void DebugInfoRenderer::render(void)
{
    const int prog   = m_prog->getProgram();
    const int posloc = glGetAttribLocation(prog, "a_pos");

    glUseProgram(prog);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(posloc);
    glVertexAttribPointer(posloc, 2, GL_FLOAT, 0, 0, &m_posBuf[0]);
    glDrawElements(GL_TRIANGLES, (int)m_ndxBuf.size(), GL_UNSIGNED_SHORT, &m_ndxBuf[0]);
    glDisableVertexAttribArray(posloc);

    m_posBuf.clear();
    m_ndxBuf.clear();
}

/*--------------------------------------------------------------------*//*!
 * \brief Texture object helper class
 *
 * Each Texture owns a GL texture object that is created when the Texture
 * is constructed and deleted when it's destructed. The class provides some
 * convenience interface functions to e.g. upload texture data to the GL.
 *
 * In addition, the class tracks the approximate amount of GL memory likely
 * used by the corresponding GL texture object; get this with
 * getApproxMemUsage(). Also, getApproxMemUsageDiff() returns N-M, where N
 * is the value that getApproxMemUsage() would return after a call to
 * setData() with arguments corresponding to those given to
 * getApproxMemUsageDiff(), and M is the value currently returned by
 * getApproxMemUsage(). This can be used to check if we need to free some
 * other memory before performing the setData() call, in case we have an
 * upper limit on the amount of memory we want to use.
 *//*--------------------------------------------------------------------*/
class Texture
{
public:
    Texture(TextureType type);
    ~Texture(void);

    // Functions that may change the value returned by getApproxMemUsage().
    void setData(const ConstPixelBufferAccess &src, int width, int height, uint32_t internalFormat, bool useMipmap);

    // Functions that don't change the value returned by getApproxMemUsage().
    void setSubData(const ConstPixelBufferAccess &src, int xOff, int yOff, int width, int height) const;
    void toUnit(int unit) const;
    void setFilter(uint32_t min, uint32_t mag) const;
    void setWrap(uint32_t s, uint32_t t) const;

    int getApproxMemUsage(void) const
    {
        return m_dataSizeApprox;
    }
    int getApproxMemUsageDiff(int width, int height, uint32_t internalFormat, bool useMipmap) const;

private:
    Texture(const Texture &);            // Not allowed.
    Texture &operator=(const Texture &); // Not allowed.

    static uint32_t genTexture(void)
    {
        uint32_t tex = 0;
        glGenTextures(1, &tex);
        return tex;
    }

    uint32_t getGLBindTarget(void) const
    {
        DE_ASSERT(m_type == TEXTURETYPE_2D || m_type == TEXTURETYPE_CUBE);
        return m_type == TEXTURETYPE_2D ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP;
    }

    const TextureType m_type;
    const uint32_t m_textureGL;

    int m_numMipLevels;
    uint32_t m_internalFormat;
    int m_dataSizeApprox;
};

Texture::Texture(const TextureType type)
    : m_type(type)
    , m_textureGL(genTexture())
    , m_numMipLevels(0)
    , m_internalFormat(0)
    , m_dataSizeApprox(0)
{
}

Texture::~Texture(void)
{
    glDeleteTextures(1, &m_textureGL);
}

int Texture::getApproxMemUsageDiff(const int width, const int height, const uint32_t internalFormat,
                                   const bool useMipmap) const
{
    const int numLevels     = useMipmap ? deLog2Floor32(de::max(width, height)) + 1 : 1;
    const int pixelSize     = internalFormat == GL_RGBA  ? 4 :
                              internalFormat == GL_RGB   ? 3 :
                              internalFormat == GL_ALPHA ? 1 :
                                                           glu::mapGLInternalFormat(internalFormat).getPixelSize();
    int memUsageApproxAfter = 0;

    for (int level = 0; level < numLevels; level++)
        memUsageApproxAfter +=
            de::max(1, width >> level) * de::max(1, height >> level) * pixelSize * (m_type == TEXTURETYPE_CUBE ? 6 : 1);

    return memUsageApproxAfter - getApproxMemUsage();
}

void Texture::setData(const ConstPixelBufferAccess &src, const int width, const int height,
                      const uint32_t internalFormat, const bool useMipmap)
{
    DE_ASSERT(m_type != TEXTURETYPE_CUBE || width == height);
    DE_ASSERT(!useMipmap || (deIsPowerOfTwo32(width) && deIsPowerOfTwo32(height)));

    const TextureFormat &format        = src.getFormat();
    const glu::TransferFormat transfer = glu::getTransferFormat(format);

    m_numMipLevels = useMipmap ? deLog2Floor32(de::max(width, height)) + 1 : 1;

    m_internalFormat = internalFormat;
    m_dataSizeApprox = width * height * format.getPixelSize() * (m_type == TEXTURETYPE_CUBE ? 6 : 1);

    DE_ASSERT(src.getRowPitch() == format.getPixelSize() * src.getWidth());
    DE_ASSERT(isMatchingGLInternalFormat(internalFormat, format));
    DE_ASSERT(width <= src.getWidth() && height <= src.getHeight());

    glPixelStorei(GL_UNPACK_ALIGNMENT, computePixelStore(format));

    if (m_type == TEXTURETYPE_2D)
    {
        m_dataSizeApprox = 0;

        glBindTexture(GL_TEXTURE_2D, m_textureGL);
        for (int level = 0; level < m_numMipLevels; level++)
        {
            const int levelWid = de::max(1, width >> level);
            const int levelHei = de::max(1, height >> level);
            m_dataSizeApprox += levelWid * levelHei * format.getPixelSize();
            glTexImage2D(GL_TEXTURE_2D, level, internalFormat, levelWid, levelHei, 0, transfer.format,
                         transfer.dataType, src.getDataPtr());
        }
    }
    else if (m_type == TEXTURETYPE_CUBE)
    {
        m_dataSizeApprox = 0;

        glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureGL);
        for (int level = 0; level < m_numMipLevels; level++)
        {
            const int levelWid = de::max(1, width >> level);
            const int levelHei = de::max(1, height >> level);
            m_dataSizeApprox += 6 * levelWid * levelHei * format.getPixelSize();
            for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
                glTexImage2D(cubeFaceToGLFace((CubeFace)face), level, internalFormat, levelWid, levelHei, 0,
                             transfer.format, transfer.dataType, src.getDataPtr());
        }
    }
    else
        DE_ASSERT(false);
}

void Texture::setSubData(const ConstPixelBufferAccess &src, const int xOff, const int yOff, const int width,
                         const int height) const
{
    const TextureFormat &format        = src.getFormat();
    const glu::TransferFormat transfer = glu::getTransferFormat(format);

    DE_ASSERT(src.getRowPitch() == format.getPixelSize() * src.getWidth());
    DE_ASSERT(isMatchingGLInternalFormat(m_internalFormat, format));
    DE_ASSERT(width <= src.getWidth() && height <= src.getHeight());

    glPixelStorei(GL_UNPACK_ALIGNMENT, computePixelStore(format));

    if (m_type == TEXTURETYPE_2D)
    {
        glBindTexture(GL_TEXTURE_2D, m_textureGL);
        for (int level = 0; level < m_numMipLevels; level++)
            glTexSubImage2D(GL_TEXTURE_2D, level, xOff >> level, yOff >> level, de::max(1, width >> level),
                            de::max(1, height >> level), transfer.format, transfer.dataType, src.getDataPtr());
    }
    else if (m_type == TEXTURETYPE_CUBE)
    {
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureGL);
        for (int level = 0; level < m_numMipLevels; level++)
        {
            for (int face = 0; face < tcu::CUBEFACE_LAST; face++)
                glTexSubImage2D(cubeFaceToGLFace((CubeFace)face), level, xOff >> level, yOff >> level,
                                de::max(1, width >> level), de::max(1, height >> level), transfer.format,
                                transfer.dataType, src.getDataPtr());
        }
    }
    else
        DE_ASSERT(false);
}

void Texture::setFilter(const uint32_t min, const uint32_t mag) const
{
    glBindTexture(getGLBindTarget(), m_textureGL);
    glTexParameteri(getGLBindTarget(), GL_TEXTURE_MIN_FILTER, min);
    glTexParameteri(getGLBindTarget(), GL_TEXTURE_MAG_FILTER, mag);
}

void Texture::setWrap(const uint32_t s, const uint32_t t) const
{
    glBindTexture(getGLBindTarget(), m_textureGL);
    glTexParameteri(getGLBindTarget(), GL_TEXTURE_WRAP_S, s);
    glTexParameteri(getGLBindTarget(), GL_TEXTURE_WRAP_T, t);
}

void Texture::toUnit(const int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(getGLBindTarget(), m_textureGL);
}

/*--------------------------------------------------------------------*//*!
 * \brief Buffer object helper class
 *
 * Each Buffer owns a GL buffer object that is created when the Buffer
 * is constructed and deleted when it's destructed. The class provides some
 * convenience interface functions to e.g. upload buffer data to the GL.
 *
 * In addition, the class tracks the approximate amount of GL memory,
 * similarly to the Texture class (see above). The getApproxMemUsageDiff()
 * is also analoguous.
 *//*--------------------------------------------------------------------*/
class Buffer
{
public:
    Buffer(void);
    ~Buffer(void);

    // Functions that may change the value returned by getApproxMemUsage().
    template <typename T>
    void setData(const vector<T> &src, const uint32_t target, const uint32_t usage)
    {
        setData(&src[0], (int)(src.size() * sizeof(T)), target, usage);
    }
    void setData(const void *src, int size, uint32_t target, uint32_t usage);

    // Functions that don't change the value returned by getApproxMemUsage().
    template <typename T>
    void setSubData(const vector<T> &src, const int offsetElems, const int numElems, const uint32_t target)
    {
        setSubData(&src[offsetElems], offsetElems * (int)sizeof(T), numElems * (int)sizeof(T), target);
    }
    void setSubData(const void *src, int offsetBytes, int sizeBytes, uint32_t target) const;
    void bind(const uint32_t target) const
    {
        glBindBuffer(target, m_bufferGL);
    }

    int getApproxMemUsage(void) const
    {
        return m_dataSizeApprox;
    }
    template <typename T>
    int getApproxMemUsageDiff(const vector<T> &src) const
    {
        return getApproxMemUsageDiff((int)(src.size() * sizeof(T)));
    }
    int getApproxMemUsageDiff(const int sizeBytes) const
    {
        return sizeBytes - getApproxMemUsage();
    }

private:
    Buffer(const Buffer &);            // Not allowed.
    Buffer &operator=(const Buffer &); // Not allowed.

    static uint32_t genBuffer(void)
    {
        uint32_t buf = 0;
        glGenBuffers(1, &buf);
        return buf;
    }

    const uint32_t m_bufferGL;
    int m_dataSizeApprox;
};

Buffer::Buffer(void) : m_bufferGL(genBuffer()), m_dataSizeApprox(0)
{
}

Buffer::~Buffer(void)
{
    glDeleteBuffers(1, &m_bufferGL);
}

void Buffer::setData(const void *const src, const int size, const uint32_t target, const uint32_t usage)
{
    bind(target);
    glBufferData(target, size, src, usage);
    glBindBuffer(target, 0);

    m_dataSizeApprox = size;
}

void Buffer::setSubData(const void *const src, const int offsetBytes, const int sizeBytes, const uint32_t target) const
{
    bind(target);
    glBufferSubData(target, offsetBytes, sizeBytes, src);
    glBindBuffer(target, 0);
}

class Program
{
public:
    Program(void);
    ~Program(void);

    void setSources(const string &vertSource, const string &fragSource);
    void build(TestLog &log);
    void use(void) const
    {
        DE_ASSERT(m_isBuilt);
        glUseProgram(m_programGL);
    }
    void setRandomUniforms(const vector<VarSpec> &uniforms, const string &shaderNameManglingSuffix, Random &rnd) const;
    void setAttribute(const Buffer &attrBuf, int attrBufOffset, const VarSpec &attrSpec,
                      const string &shaderNameManglingSuffix) const;
    void setAttributeClientMem(const void *attrData, const VarSpec &attrSpec,
                               const string &shaderNameManglingSuffix) const;
    void disableAttributeArray(const VarSpec &attrSpec, const string &shaderNameManglingSuffix) const;

private:
    Program(const Program &);            // Not allowed.
    Program &operator=(const Program &); // Not allowed.

    string m_vertSource;
    string m_fragSource;

    const uint32_t m_vertShaderGL;
    const uint32_t m_fragShaderGL;
    const uint32_t m_programGL;
    bool m_hasSources;
    bool m_isBuilt;
};

Program::Program(void)
    : m_vertShaderGL(glCreateShader(GL_VERTEX_SHADER))
    , m_fragShaderGL(glCreateShader(GL_FRAGMENT_SHADER))
    , m_programGL(glCreateProgram())
    , m_hasSources(false)
    , m_isBuilt(false)
{
    glAttachShader(m_programGL, m_vertShaderGL);
    glAttachShader(m_programGL, m_fragShaderGL);
}

Program::~Program(void)
{
    glDeleteShader(m_vertShaderGL);
    glDeleteShader(m_fragShaderGL);
    glDeleteProgram(m_programGL);
}

void Program::setSources(const string &vertSource, const string &fragSource)
{
    const char *const vertSourceCstr = vertSource.c_str();
    const char *const fragSourceCstr = fragSource.c_str();

    m_vertSource = vertSource;
    m_fragSource = fragSource;

    // \note In GLES2 api the source parameter type lacks one const.
    glShaderSource(m_vertShaderGL, 1, (const char **)&vertSourceCstr, nullptr);
    glShaderSource(m_fragShaderGL, 1, (const char **)&fragSourceCstr, nullptr);

    m_hasSources = true;
}

void Program::build(TestLog &log)
{
    DE_ASSERT(m_hasSources);

    const bool vertCompileOk = compileShader(m_vertShaderGL);
    const bool fragCompileOk = compileShader(m_fragShaderGL);
    const bool attemptLink   = vertCompileOk && fragCompileOk;
    const bool linkOk        = attemptLink && linkProgram(m_programGL);

    if (!(vertCompileOk && fragCompileOk && linkOk))
    {
        log << TestLog::ShaderProgram(linkOk, attemptLink ? getProgramInfoLog(m_programGL) : string(""))
            << TestLog::Shader(QP_SHADER_TYPE_VERTEX, m_vertSource, vertCompileOk, getShaderInfoLog(m_vertShaderGL))
            << TestLog::Shader(QP_SHADER_TYPE_FRAGMENT, m_fragSource, fragCompileOk, getShaderInfoLog(m_fragShaderGL))
            << TestLog::EndShaderProgram;

        throw tcu::TestError("Program build failed");
    }

    m_isBuilt = true;
}

void Program::setRandomUniforms(const vector<VarSpec> &uniforms, const string &shaderNameManglingSuffix,
                                Random &rnd) const
{
    use();

    for (int unifNdx = 0; unifNdx < (int)uniforms.size(); unifNdx++)
    {
        const VarSpec &spec      = uniforms[unifNdx];
        const int typeScalarSize = glu::getDataTypeScalarSize(spec.type);
        const int location =
            glGetUniformLocation(m_programGL, mangleShaderNames(spec.name, shaderNameManglingSuffix).c_str());
        if (location < 0)
            continue;

        if (glu::isDataTypeFloatOrVec(spec.type))
        {
            float val[4];
            for (int i = 0; i < typeScalarSize; i++)
                val[i] = rnd.getFloat(spec.minValue.f[i], spec.maxValue.f[i]);

            switch (spec.type)
            {
            case glu::TYPE_FLOAT:
                glUniform1f(location, val[0]);
                break;
            case glu::TYPE_FLOAT_VEC2:
                glUniform2f(location, val[0], val[1]);
                break;
            case glu::TYPE_FLOAT_VEC3:
                glUniform3f(location, val[0], val[1], val[2]);
                break;
            case glu::TYPE_FLOAT_VEC4:
                glUniform4f(location, val[0], val[1], val[2], val[3]);
                break;
            default:
                DE_ASSERT(false);
            }
        }
        else if (glu::isDataTypeMatrix(spec.type))
        {
            float val[4 * 4];
            for (int i = 0; i < typeScalarSize; i++)
                val[i] = rnd.getFloat(spec.minValue.f[i], spec.maxValue.f[i]);

            switch (spec.type)
            {
            case glu::TYPE_FLOAT_MAT2:
                glUniformMatrix2fv(location, 1, GL_FALSE, &val[0]);
                break;
            case glu::TYPE_FLOAT_MAT3:
                glUniformMatrix3fv(location, 1, GL_FALSE, &val[0]);
                break;
            case glu::TYPE_FLOAT_MAT4:
                glUniformMatrix4fv(location, 1, GL_FALSE, &val[0]);
                break;
            case glu::TYPE_FLOAT_MAT2X3:
                glUniformMatrix2x3fv(location, 1, GL_FALSE, &val[0]);
                break;
            case glu::TYPE_FLOAT_MAT2X4:
                glUniformMatrix2x4fv(location, 1, GL_FALSE, &val[0]);
                break;
            case glu::TYPE_FLOAT_MAT3X2:
                glUniformMatrix3x2fv(location, 1, GL_FALSE, &val[0]);
                break;
            case glu::TYPE_FLOAT_MAT3X4:
                glUniformMatrix3x4fv(location, 1, GL_FALSE, &val[0]);
                break;
            case glu::TYPE_FLOAT_MAT4X2:
                glUniformMatrix4x2fv(location, 1, GL_FALSE, &val[0]);
                break;
            case glu::TYPE_FLOAT_MAT4X3:
                glUniformMatrix4x3fv(location, 1, GL_FALSE, &val[0]);
                break;
            default:
                DE_ASSERT(false);
            }
        }
        else if (glu::isDataTypeIntOrIVec(spec.type))
        {
            int val[4];
            for (int i = 0; i < typeScalarSize; i++)
                val[i] = rnd.getInt(spec.minValue.i[i], spec.maxValue.i[i]);

            switch (spec.type)
            {
            case glu::TYPE_INT:
                glUniform1i(location, val[0]);
                break;
            case glu::TYPE_INT_VEC2:
                glUniform2i(location, val[0], val[1]);
                break;
            case glu::TYPE_INT_VEC3:
                glUniform3i(location, val[0], val[1], val[2]);
                break;
            case glu::TYPE_INT_VEC4:
                glUniform4i(location, val[0], val[1], val[2], val[3]);
                break;
            default:
                DE_ASSERT(false);
            }
        }
        else if (glu::isDataTypeUintOrUVec(spec.type))
        {
            uint32_t val[4];
            for (int i = 0; i < typeScalarSize; i++)
            {
                DE_ASSERT(spec.minValue.i[i] >= 0 && spec.maxValue.i[i] >= 0);
                val[i] = (uint32_t)rnd.getInt(spec.minValue.i[i], spec.maxValue.i[i]);
            }

            switch (spec.type)
            {
            case glu::TYPE_UINT:
                glUniform1ui(location, val[0]);
                break;
            case glu::TYPE_UINT_VEC2:
                glUniform2ui(location, val[0], val[1]);
                break;
            case glu::TYPE_UINT_VEC3:
                glUniform3ui(location, val[0], val[1], val[2]);
                break;
            case glu::TYPE_UINT_VEC4:
                glUniform4ui(location, val[0], val[1], val[2], val[3]);
                break;
            default:
                DE_ASSERT(false);
            }
        }
        else
            DE_ASSERT(false);
    }
}

void Program::setAttribute(const Buffer &attrBuf, const int attrBufOffset, const VarSpec &attrSpec,
                           const string &shaderNameManglingSuffix) const
{
    const int attrLoc =
        glGetAttribLocation(m_programGL, mangleShaderNames(attrSpec.name, shaderNameManglingSuffix).c_str());

    glEnableVertexAttribArray(attrLoc);
    attrBuf.bind(GL_ARRAY_BUFFER);

    if (glu::isDataTypeFloatOrVec(attrSpec.type))
        glVertexAttribPointer(attrLoc, glu::getDataTypeScalarSize(attrSpec.type), GL_FLOAT, GL_FALSE, 0,
                              (GLvoid *)(intptr_t)attrBufOffset);
    else
        DE_ASSERT(false);
}

void Program::setAttributeClientMem(const void *const attrData, const VarSpec &attrSpec,
                                    const string &shaderNameManglingSuffix) const
{
    const int attrLoc =
        glGetAttribLocation(m_programGL, mangleShaderNames(attrSpec.name, shaderNameManglingSuffix).c_str());

    glEnableVertexAttribArray(attrLoc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (glu::isDataTypeFloatOrVec(attrSpec.type))
        glVertexAttribPointer(attrLoc, glu::getDataTypeScalarSize(attrSpec.type), GL_FLOAT, GL_FALSE, 0, attrData);
    else
        DE_ASSERT(false);
}

void Program::disableAttributeArray(const VarSpec &attrSpec, const string &shaderNameManglingSuffix) const
{
    const int attrLoc =
        glGetAttribLocation(m_programGL, mangleShaderNames(attrSpec.name, shaderNameManglingSuffix).c_str());

    glDisableVertexAttribArray(attrLoc);
}

/*--------------------------------------------------------------------*//*!
 * \brief Container class for managing GL objects
 *
 * GLObjectManager can be used for objects of class Program, Buffer or
 * Texture. In the manager, each such object is associated with a name that
 * is used to access it.
 *
 * In addition to the making, getting and removing functions, the manager
 * supports marking objects as "garbage", meaning they're not yet
 * destroyed, but can be later destroyed with removeRandomGarbage(). The
 * idea is that if we want to stress test with high memory usage, we can
 * continuously move objects to garbage after using them, and when a memory
 * limit is reached, we can call removeGarbageUntilUnder(limit, rnd). This
 * way we can approximately keep our memory usage at just under the wanted
 * limit.
 *
 * The manager also supports querying the approximate amount of GL memory
 * used by its objects.
 *
 * \note The memory usage related functions are not currently supported
 *         for Program objects.
 *//*--------------------------------------------------------------------*/
template <typename T>
class GLObjectManager
{
public:
    void make(const string &name)
    {
        DE_ASSERT(!has(name));
        m_objects[name] = SharedPtr<T>(new T);
    }
    void make(const string &name, gls::TextureType texType)
    {
        DE_ASSERT(!has(name));
        m_objects[name] = SharedPtr<T>(new T(texType));
    }
    bool has(const string &name) const
    {
        return m_objects.find(name) != m_objects.end();
    }
    const T &get(const string &name) const;
    T &get(const string &name)
    {
        return const_cast<T &>(((const GLObjectManager<T> *)this)->get(name));
    }
    void remove(const string &name)
    {
        const int removed = (int)m_objects.erase(name);
        DE_ASSERT(removed);
        DE_UNREF(removed);
    }
    int computeApproxMemUsage(void) const;
    void markAsGarbage(const string &name);
    int removeRandomGarbage(Random &rnd);
    void removeGarbageUntilUnder(int limit, Random &rnd);

private:
    static const char *objTypeName(void);

    map<string, SharedPtr<T>> m_objects;
    vector<SharedPtr<T>> m_garbageObjects;
};

template <>
const char *GLObjectManager<Buffer>::objTypeName(void)
{
    return "buffer";
}
template <>
const char *GLObjectManager<Texture>::objTypeName(void)
{
    return "texture";
}
template <>
const char *GLObjectManager<Program>::objTypeName(void)
{
    return "program";
}

template <typename T>
const T &GLObjectManager<T>::get(const string &name) const
{
    const typename map<string, SharedPtr<T>>::const_iterator it = m_objects.find(name);
    DE_ASSERT(it != m_objects.end());
    return *it->second;
}

template <typename T>
int GLObjectManager<T>::computeApproxMemUsage(void) const
{
    int result = 0;

    for (typename map<string, SharedPtr<T>>::const_iterator it = m_objects.begin(); it != m_objects.end(); ++it)
        result += it->second->getApproxMemUsage();

    for (typename vector<SharedPtr<T>>::const_iterator it = m_garbageObjects.begin(); it != m_garbageObjects.end();
         ++it)
        result += (*it)->getApproxMemUsage();

    return result;
}

template <typename T>
void GLObjectManager<T>::markAsGarbage(const string &name)
{
    const typename map<string, SharedPtr<T>>::iterator it = m_objects.find(name);
    DE_ASSERT(it != m_objects.end());
    m_garbageObjects.push_back(it->second);
    m_objects.erase(it);
}

template <typename T>
int GLObjectManager<T>::removeRandomGarbage(Random &rnd)
{
    if (m_garbageObjects.empty())
        return -1;

    const int removeNdx   = rnd.getInt(0, (int)m_garbageObjects.size() - 1);
    const int memoryFreed = m_garbageObjects[removeNdx]->getApproxMemUsage();
    m_garbageObjects.erase(m_garbageObjects.begin() + removeNdx);
    return memoryFreed;
}

template <typename T>
void GLObjectManager<T>::removeGarbageUntilUnder(const int limit, Random &rnd)
{
    int memUsage = computeApproxMemUsage();

    while (memUsage > limit)
    {
        const int memReleased = removeRandomGarbage(rnd);
        if (memReleased < 0)
            throw tcu::InternalError(string("") + "Given " + objTypeName() +
                                     " memory usage limit exceeded, and no unneeded " + objTypeName() +
                                     " resources available to release");
        memUsage -= memReleased;
        DE_ASSERT(memUsage == computeApproxMemUsage());
    }
}

} // namespace LongStressCaseInternal

using namespace LongStressCaseInternal;

static int generateRandomAttribData(vector<uint8_t> &attrDataBuf, int &dataSizeBytesDst, const VarSpec &attrSpec,
                                    const int numVertices, Random &rnd)
{
    const bool isFloat      = glu::isDataTypeFloatOrVec(attrSpec.type);
    const int numComponents = glu::getDataTypeScalarSize(attrSpec.type);
    const int componentSize = (int)(isFloat ? sizeof(GLfloat) : sizeof(GLint));
    const int offsetInBuf   = nextDivisible((int)attrDataBuf.size(), componentSize); // Round up for alignment.

    DE_STATIC_ASSERT(sizeof(GLint) == sizeof(int));
    DE_STATIC_ASSERT(sizeof(GLfloat) == sizeof(float));

    dataSizeBytesDst = numComponents * componentSize * numVertices;

    attrDataBuf.resize(offsetInBuf + dataSizeBytesDst);

    if (isFloat)
    {
        float *const data = (float *)&attrDataBuf[offsetInBuf];

        for (int vtxNdx = 0; vtxNdx < numVertices; vtxNdx++)
            for (int compNdx = 0; compNdx < numComponents; compNdx++)
                data[vtxNdx * numComponents + compNdx] =
                    rnd.getFloat(attrSpec.minValue.f[compNdx], attrSpec.maxValue.f[compNdx]);
    }
    else
    {
        DE_ASSERT(glu::isDataTypeIntOrIVec(attrSpec.type));

        int *const data = (int *)&attrDataBuf[offsetInBuf];

        for (int vtxNdx = 0; vtxNdx < numVertices; vtxNdx++)
            for (int compNdx = 0; compNdx < numComponents; compNdx++)
                data[vtxNdx * numComponents + compNdx] =
                    rnd.getInt(attrSpec.minValue.i[compNdx], attrSpec.maxValue.i[compNdx]);
    }

    return offsetInBuf;
}

static int generateRandomPositionAttribData(vector<uint8_t> &attrDataBuf, int &dataSizeBytesDst,
                                            const VarSpec &attrSpec, const int numVertices, Random &rnd)
{
    DE_ASSERT(glu::isDataTypeFloatOrVec(attrSpec.type));

    const int numComponents = glu::getDataTypeScalarSize(attrSpec.type);
    DE_ASSERT(numComponents >= 2);
    const int offsetInBuf = generateRandomAttribData(attrDataBuf, dataSizeBytesDst, attrSpec, numVertices, rnd);

    if (numComponents > 2)
    {
        float *const data = (float *)&attrDataBuf[offsetInBuf];

        for (int vtxNdx = 0; vtxNdx < numVertices; vtxNdx++)
            data[vtxNdx * numComponents + 2] = -1.0f;

        for (int triNdx = 0; triNdx < numVertices - 2; triNdx++)
        {
            float *const vtxAComps = &data[(triNdx + 0) * numComponents];
            float *const vtxBComps = &data[(triNdx + 1) * numComponents];
            float *const vtxCComps = &data[(triNdx + 2) * numComponents];

            const float triArea = triangleArea(Vec2(vtxAComps[0], vtxAComps[1]), Vec2(vtxBComps[0], vtxBComps[1]),
                                               Vec2(vtxCComps[0], vtxCComps[1]));
            const float t       = triArea / (triArea + 1.0f);
            const float z       = (1.0f - t) * attrSpec.minValue.f[2] + t * attrSpec.maxValue.f[2];

            vtxAComps[2] = de::max(vtxAComps[2], z);
            vtxBComps[2] = de::max(vtxBComps[2], z);
            vtxCComps[2] = de::max(vtxCComps[2], z);
        }
    }

    return offsetInBuf;
}

static void generateAttribs(vector<uint8_t> &attrDataBuf, vector<int> &attrDataOffsets, vector<int> &attrDataSizes,
                            const vector<VarSpec> &attrSpecs, const string &posAttrName, const int numVertices,
                            Random &rnd)
{
    attrDataBuf.clear();
    attrDataOffsets.clear();
    attrDataSizes.resize(attrSpecs.size());

    for (int i = 0; i < (int)attrSpecs.size(); i++)
    {
        if (attrSpecs[i].name == posAttrName)
            attrDataOffsets.push_back(
                generateRandomPositionAttribData(attrDataBuf, attrDataSizes[i], attrSpecs[i], numVertices, rnd));
        else
            attrDataOffsets.push_back(
                generateRandomAttribData(attrDataBuf, attrDataSizes[i], attrSpecs[i], numVertices, rnd));
    }
}

LongStressCase::LongStressCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *const name,
                               const char *const desc, const int maxTexMemoryUsageBytes,
                               const int maxBufMemoryUsageBytes, const int numDrawCallsPerIteration,
                               const int numTrianglesPerDrawCall, const vector<ProgramContext> &programContexts,
                               const FeatureProbabilities &probabilities, const uint32_t indexBufferUsage,
                               const uint32_t attrBufferUsage, const int redundantBufferFactor,
                               const bool showDebugInfo)
    : tcu::TestCase(testCtx, name, desc)
    , m_renderCtx(renderCtx)
    , m_maxTexMemoryUsageBytes(maxTexMemoryUsageBytes)
    , m_maxBufMemoryUsageBytes(maxBufMemoryUsageBytes)
    , m_numDrawCallsPerIteration(numDrawCallsPerIteration)
    , m_numTrianglesPerDrawCall(numTrianglesPerDrawCall)
    , m_numVerticesPerDrawCall(numTrianglesPerDrawCall + 2) // \note Triangle strips are used.
    , m_programContexts(programContexts)
    , m_probabilities(probabilities)
    , m_indexBufferUsage(indexBufferUsage)
    , m_attrBufferUsage(attrBufferUsage)
    , m_redundantBufferFactor(redundantBufferFactor)
    , m_showDebugInfo(showDebugInfo)
    , m_numIterations(getNumIterations(testCtx, 5))
    , m_isGLES3(contextSupports(renderCtx.getType(), glu::ApiType::es(3, 0)))
    , m_currentIteration(0)
    , m_startTimeSeconds((uint64_t)-1)
    , m_lastLogTime((uint64_t)-1)
    , m_lastLogIteration(0)
    , m_currentLogEntryNdx(0)
    , m_rnd(deStringHash(getName()) ^ testCtx.getCommandLine().getBaseSeed())
    , m_programs(nullptr)
    , m_buffers(nullptr)
    , m_textures(nullptr)
    , m_debugInfoRenderer(nullptr)
{
    DE_ASSERT(m_numVerticesPerDrawCall <=
              (int)std::numeric_limits<uint16_t>::max() + 1); // \note Vertices are referred to with 16-bit indices.
    DE_ASSERT(m_redundantBufferFactor > 0);
}

LongStressCase::~LongStressCase(void)
{
    LongStressCase::deinit();
}

void LongStressCase::init(void)
{
    // Generate unused texture data for each texture spec in m_programContexts.

    DE_ASSERT(!m_programContexts.empty());
    DE_ASSERT(m_programResources.empty());
    m_programResources.resize(m_programContexts.size());

    for (int progCtxNdx = 0; progCtxNdx < (int)m_programContexts.size(); progCtxNdx++)
    {
        const ProgramContext &progCtx = m_programContexts[progCtxNdx];
        ProgramResources &progRes     = m_programResources[progCtxNdx];

        for (int texSpecNdx = 0; texSpecNdx < (int)progCtx.textureSpecs.size(); texSpecNdx++)
        {
            const TextureSpec &spec    = progCtx.textureSpecs[texSpecNdx];
            const TextureFormat format = glu::mapGLTransferFormat(spec.format, spec.dataType);

            // If texture data with the same format has already been generated, re-use that (don't care much about contents).

            SharedPtr<TextureLevel> unusedTex;

            for (int prevProgCtxNdx = 0; prevProgCtxNdx < (int)m_programResources.size(); prevProgCtxNdx++)
            {
                const vector<SharedPtr<TextureLevel>> &prevProgCtxTextures =
                    m_programResources[prevProgCtxNdx].unusedTextures;

                for (int texNdx = 0; texNdx < (int)prevProgCtxTextures.size(); texNdx++)
                {
                    if (prevProgCtxTextures[texNdx]->getFormat() == format)
                    {
                        unusedTex = prevProgCtxTextures[texNdx];
                        break;
                    }
                }
            }

            if (!unusedTex)
                unusedTex = SharedPtr<TextureLevel>(new TextureLevel(format));

            if (unusedTex->getWidth() < spec.width || unusedTex->getHeight() < spec.height)
            {
                unusedTex->setSize(spec.width, spec.height);
                tcu::fillWithComponentGradients(unusedTex->getAccess(), spec.minValue, spec.maxValue);
            }

            progRes.unusedTextures.push_back(unusedTex);
        }
    }

    m_vertexIndices.clear();
    for (int i = 0; i < m_numVerticesPerDrawCall; i++)
        m_vertexIndices.push_back((uint16_t)i);
    m_rnd.shuffle(m_vertexIndices.begin(), m_vertexIndices.end());

    DE_ASSERT(!m_programs && !m_buffers && !m_textures);
    m_programs = new GLObjectManager<Program>;
    m_buffers  = new GLObjectManager<Buffer>;
    m_textures = new GLObjectManager<Texture>;

    m_currentIteration = 0;

    {
        TestLog &log = m_testCtx.getLog();

        log << TestLog::Message
            << "Number of iterations: " << (m_numIterations > 0 ? toString(m_numIterations) : "infinite")
            << TestLog::EndMessage << TestLog::Message
            << "Number of draw calls per iteration: " << m_numDrawCallsPerIteration << TestLog::EndMessage
            << TestLog::Message << "Number of triangles per draw call: " << m_numTrianglesPerDrawCall
            << TestLog::EndMessage << TestLog::Message << "Using triangle strips" << TestLog::EndMessage
            << TestLog::Message
            << "Approximate texture memory usage limit: " << de::floatToString((float)m_maxTexMemoryUsageBytes / Mi, 2)
            << " MiB" << TestLog::EndMessage << TestLog::Message
            << "Approximate buffer memory usage limit: " << de::floatToString((float)m_maxBufMemoryUsageBytes / Mi, 2)
            << " MiB" << TestLog::EndMessage << TestLog::Message
            << "Default vertex attribute data buffer usage parameter: " << glu::getUsageName(m_attrBufferUsage)
            << TestLog::EndMessage << TestLog::Message
            << "Default vertex index data buffer usage parameter: " << glu::getUsageName(m_indexBufferUsage)
            << TestLog::EndMessage

            << TestLog::Section("ProbabilityParams", "Per-iteration probability parameters") << TestLog::Message
            << "Program re-build: " << probabilityStr(m_probabilities.rebuildProgram) << TestLog::EndMessage
            << TestLog::Message << "Texture re-upload: " << probabilityStr(m_probabilities.reuploadTexture)
            << TestLog::EndMessage << TestLog::Message
            << "Buffer re-upload: " << probabilityStr(m_probabilities.reuploadBuffer) << TestLog::EndMessage
            << TestLog::Message << "Use glTexImage* instead of glTexSubImage* when uploading texture: "
            << probabilityStr(m_probabilities.reuploadWithTexImage) << TestLog::EndMessage << TestLog::Message
            << "Use glBufferData* instead of glBufferSubData* when uploading buffer: "
            << probabilityStr(m_probabilities.reuploadWithBufferData) << TestLog::EndMessage << TestLog::Message
            << "Delete texture after using it, even if could re-use it: "
            << probabilityStr(m_probabilities.deleteTexture) << TestLog::EndMessage << TestLog::Message
            << "Delete buffer after using it, even if could re-use it: " << probabilityStr(m_probabilities.deleteBuffer)
            << TestLog::EndMessage << TestLog::Message
            << "Don't re-use texture, and only delete if memory limit is hit: "
            << probabilityStr(m_probabilities.wastefulTextureMemoryUsage) << TestLog::EndMessage << TestLog::Message
            << "Don't re-use buffer, and only delete if memory limit is hit: "
            << probabilityStr(m_probabilities.wastefulBufferMemoryUsage) << TestLog::EndMessage << TestLog::Message
            << "Use client memory (instead of GL buffers) for vertex attribute data: "
            << probabilityStr(m_probabilities.clientMemoryAttributeData) << TestLog::EndMessage << TestLog::Message
            << "Use client memory (instead of GL buffers) for vertex index data: "
            << probabilityStr(m_probabilities.clientMemoryIndexData) << TestLog::EndMessage << TestLog::Message
            << "Use random target parameter when uploading buffer data: "
            << probabilityStr(m_probabilities.randomBufferUploadTarget) << TestLog::EndMessage << TestLog::Message
            << "Use random usage parameter when uploading buffer data: "
            << probabilityStr(m_probabilities.randomBufferUsage) << TestLog::EndMessage << TestLog::Message
            << "Use glDrawArrays instead of glDrawElements: " << probabilityStr(m_probabilities.useDrawArrays)
            << TestLog::EndMessage << TestLog::Message
            << "Use separate buffers for each attribute, instead of one array for all: "
            << probabilityStr(m_probabilities.separateAttributeBuffers) << TestLog::EndMessage << TestLog::EndSection
            << TestLog::Message << "Using " << m_programContexts.size() << " program(s)" << TestLog::EndMessage;

        bool anyProgramsFailed = false;
        for (int progCtxNdx = 0; progCtxNdx < (int)m_programContexts.size(); progCtxNdx++)
        {
            const ProgramContext &progCtx = m_programContexts[progCtxNdx];
            glu::ShaderProgram prog(m_renderCtx,
                                    glu::makeVtxFragSources(mangleShaderNames(progCtx.vertexSource, ""),
                                                            mangleShaderNames(progCtx.fragmentSource, "")));
            log << TestLog::Section("ShaderProgram" + toString(progCtxNdx), "Shader program " + toString(progCtxNdx))
                << prog << TestLog::EndSection;
            if (!prog.isOk())
                anyProgramsFailed = true;
        }

        if (anyProgramsFailed)
            throw tcu::TestError("One or more shader programs failed to compile");
    }

    DE_ASSERT(!m_debugInfoRenderer);
    if (m_showDebugInfo)
        m_debugInfoRenderer = new DebugInfoRenderer(m_renderCtx);
}

void LongStressCase::deinit(void)
{
    m_programResources.clear();

    delete m_programs;
    m_programs = nullptr;

    delete m_buffers;
    m_buffers = nullptr;

    delete m_textures;
    m_textures = nullptr;

    delete m_debugInfoRenderer;
    m_debugInfoRenderer = nullptr;
}

LongStressCase::IterateResult LongStressCase::iterate(void)
{
    TestLog &log                            = m_testCtx.getLog();
    const int renderWidth                   = m_renderCtx.getRenderTarget().getWidth();
    const int renderHeight                  = m_renderCtx.getRenderTarget().getHeight();
    const bool useClientMemoryIndexData     = m_rnd.getFloat() < m_probabilities.clientMemoryIndexData;
    const bool useDrawArrays                = m_rnd.getFloat() < m_probabilities.useDrawArrays;
    const bool separateAttributeBuffers     = m_rnd.getFloat() < m_probabilities.separateAttributeBuffers;
    const int progContextNdx                = m_rnd.getInt(0, (int)m_programContexts.size() - 1);
    const ProgramContext &programContext    = m_programContexts[progContextNdx];
    ProgramResources &programResources      = m_programResources[progContextNdx];
    const string programName                = "prog" + toString(progContextNdx);
    const string textureNamePrefix          = "tex" + toString(progContextNdx) + "_";
    const string unitedAttrBufferNamePrefix = "attrBuf" + toString(progContextNdx) + "_";
    const string indexBufferName            = "indexBuf" + toString(progContextNdx);
    const string separateAttrBufNamePrefix  = "attrBuf" + toString(progContextNdx) + "_";

    if (m_currentIteration == 0)
        m_lastLogTime = m_startTimeSeconds = deGetTime();

    // Make or re-compile programs.
    {
        const bool hadProgram = m_programs->has(programName);

        if (!hadProgram)
            m_programs->make(programName);

        Program &prog = m_programs->get(programName);

        if (!hadProgram || m_rnd.getFloat() < m_probabilities.rebuildProgram)
        {
            programResources.shaderNameManglingSuffix =
                toString((uint16_t)deUint64Hash((uint64_t)m_currentIteration ^ deGetTime()));

            prog.setSources(
                mangleShaderNames(programContext.vertexSource, programResources.shaderNameManglingSuffix),
                mangleShaderNames(programContext.fragmentSource, programResources.shaderNameManglingSuffix));

            prog.build(log);
        }

        prog.use();
    }

    Program &program = m_programs->get(programName);

    // Make or re-upload textures.

    for (int texNdx = 0; texNdx < (int)programContext.textureSpecs.size(); texNdx++)
    {
        const string texName    = textureNamePrefix + toString(texNdx);
        const bool hadTexture   = m_textures->has(texName);
        const TextureSpec &spec = programContext.textureSpecs[texNdx];

        if (!hadTexture)
            m_textures->make(texName, spec.textureType);

        if (!hadTexture || m_rnd.getFloat() < m_probabilities.reuploadTexture)
        {
            Texture &texture = m_textures->get(texName);

            m_textures->removeGarbageUntilUnder(
                m_maxTexMemoryUsageBytes -
                    texture.getApproxMemUsageDiff(spec.width, spec.height, spec.internalFormat, spec.useMipmap),
                m_rnd);

            if (!hadTexture || m_rnd.getFloat() < m_probabilities.reuploadWithTexImage)
                texture.setData(programResources.unusedTextures[texNdx]->getAccess(), spec.width, spec.height,
                                spec.internalFormat, spec.useMipmap);
            else
                texture.setSubData(programResources.unusedTextures[texNdx]->getAccess(), 0, 0, spec.width, spec.height);

            texture.toUnit(0);
            texture.setWrap(spec.sWrap, spec.tWrap);
            texture.setFilter(spec.minFilter, spec.magFilter);
        }
    }

    // Bind textures to units, in random order (because when multiple texture specs have same unit, we want to pick one randomly).

    {
        vector<int> texSpecIndices(programContext.textureSpecs.size());
        for (int i = 0; i < (int)texSpecIndices.size(); i++)
            texSpecIndices[i] = i;
        m_rnd.shuffle(texSpecIndices.begin(), texSpecIndices.end());
        for (int i = 0; i < (int)texSpecIndices.size(); i++)
            m_textures->get(textureNamePrefix + toString(texSpecIndices[i]))
                .toUnit(programContext.textureSpecs[i].textureUnit);
    }

    // Make or re-upload index buffer.

    if (!useDrawArrays)
    {
        m_rnd.shuffle(m_vertexIndices.begin(), m_vertexIndices.end());

        if (!useClientMemoryIndexData)
        {
            const bool hadIndexBuffer = m_buffers->has(indexBufferName);

            if (!hadIndexBuffer)
                m_buffers->make(indexBufferName);

            Buffer &indexBuf = m_buffers->get(indexBufferName);

            if (!hadIndexBuffer || m_rnd.getFloat() < m_probabilities.reuploadBuffer)
            {
                m_buffers->removeGarbageUntilUnder(
                    m_maxBufMemoryUsageBytes - indexBuf.getApproxMemUsageDiff(m_vertexIndices), m_rnd);
                const uint32_t target = m_rnd.getFloat() < m_probabilities.randomBufferUploadTarget ?
                                            randomBufferTarget(m_rnd, m_isGLES3) :
                                            GL_ELEMENT_ARRAY_BUFFER;

                if (!hadIndexBuffer || m_rnd.getFloat() < m_probabilities.reuploadWithBufferData)
                    indexBuf.setData(m_vertexIndices, target,
                                     m_rnd.getFloat() < m_probabilities.randomBufferUsage ?
                                         randomBufferUsage(m_rnd, m_isGLES3) :
                                         m_indexBufferUsage);
                else
                    indexBuf.setSubData(m_vertexIndices, 0, m_numVerticesPerDrawCall, target);
            }
        }
    }

    // Set vertex attributes. If not using client-memory data, make or re-upload attribute buffers.

    generateAttribs(programResources.attrDataBuf, programResources.attrDataOffsets, programResources.attrDataSizes,
                    programContext.attributes, programContext.positionAttrName, m_numVerticesPerDrawCall, m_rnd);

    if (!(m_rnd.getFloat() < m_probabilities.clientMemoryAttributeData))
    {
        if (separateAttributeBuffers)
        {
            for (int attrNdx = 0; attrNdx < (int)programContext.attributes.size(); attrNdx++)
            {
                const int usedRedundantBufferNdx = m_rnd.getInt(0, m_redundantBufferFactor - 1);

                for (int redundantBufferNdx = 0; redundantBufferNdx < m_redundantBufferFactor; redundantBufferNdx++)
                {
                    const string curAttrBufName =
                        separateAttrBufNamePrefix + toString(attrNdx) + "_" + toString(redundantBufferNdx);
                    const bool hadCurAttrBuffer = m_buffers->has(curAttrBufName);

                    if (!hadCurAttrBuffer)
                        m_buffers->make(curAttrBufName);

                    Buffer &curAttrBuf = m_buffers->get(curAttrBufName);

                    if (!hadCurAttrBuffer || m_rnd.getFloat() < m_probabilities.reuploadBuffer)
                    {
                        m_buffers->removeGarbageUntilUnder(
                            m_maxBufMemoryUsageBytes -
                                curAttrBuf.getApproxMemUsageDiff(programResources.attrDataSizes[attrNdx]),
                            m_rnd);
                        const uint32_t target = m_rnd.getFloat() < m_probabilities.randomBufferUploadTarget ?
                                                    randomBufferTarget(m_rnd, m_isGLES3) :
                                                    GL_ARRAY_BUFFER;

                        if (!hadCurAttrBuffer || m_rnd.getFloat() < m_probabilities.reuploadWithBufferData)
                            curAttrBuf.setData(&programResources.attrDataBuf[programResources.attrDataOffsets[attrNdx]],
                                               programResources.attrDataSizes[attrNdx], target,
                                               m_rnd.getFloat() < m_probabilities.randomBufferUsage ?
                                                   randomBufferUsage(m_rnd, m_isGLES3) :
                                                   m_attrBufferUsage);
                        else
                            curAttrBuf.setSubData(
                                &programResources.attrDataBuf[programResources.attrDataOffsets[attrNdx]], 0,
                                programResources.attrDataSizes[attrNdx], target);
                    }

                    if (redundantBufferNdx == usedRedundantBufferNdx)
                        program.setAttribute(curAttrBuf, 0, programContext.attributes[attrNdx],
                                             programResources.shaderNameManglingSuffix);
                }
            }
        }
        else
        {
            const int usedRedundantBufferNdx = m_rnd.getInt(0, m_redundantBufferFactor - 1);

            for (int redundantBufferNdx = 0; redundantBufferNdx < m_redundantBufferFactor; redundantBufferNdx++)
            {
                const string attrBufName = unitedAttrBufferNamePrefix + toString(redundantBufferNdx);
                const bool hadAttrBuffer = m_buffers->has(attrBufName);

                if (!hadAttrBuffer)
                    m_buffers->make(attrBufName);

                Buffer &attrBuf = m_buffers->get(attrBufName);

                if (!hadAttrBuffer || m_rnd.getFloat() < m_probabilities.reuploadBuffer)
                {
                    m_buffers->removeGarbageUntilUnder(
                        m_maxBufMemoryUsageBytes - attrBuf.getApproxMemUsageDiff(programResources.attrDataBuf), m_rnd);
                    const uint32_t target = m_rnd.getFloat() < m_probabilities.randomBufferUploadTarget ?
                                                randomBufferTarget(m_rnd, m_isGLES3) :
                                                GL_ARRAY_BUFFER;

                    if (!hadAttrBuffer || m_rnd.getFloat() < m_probabilities.reuploadWithBufferData)
                        attrBuf.setData(programResources.attrDataBuf, target,
                                        m_rnd.getFloat() < m_probabilities.randomBufferUsage ?
                                            randomBufferUsage(m_rnd, m_isGLES3) :
                                            m_attrBufferUsage);
                    else
                        attrBuf.setSubData(programResources.attrDataBuf, 0, (int)programResources.attrDataBuf.size(),
                                           target);
                }

                if (redundantBufferNdx == usedRedundantBufferNdx)
                {
                    for (int i = 0; i < (int)programContext.attributes.size(); i++)
                        program.setAttribute(attrBuf, programResources.attrDataOffsets[i], programContext.attributes[i],
                                             programResources.shaderNameManglingSuffix);
                }
            }
        }
    }
    else
    {
        for (int i = 0; i < (int)programContext.attributes.size(); i++)
            program.setAttributeClientMem(&programResources.attrDataBuf[programResources.attrDataOffsets[i]],
                                          programContext.attributes[i], programResources.shaderNameManglingSuffix);
    }

    // Draw.

    glViewport(0, 0, renderWidth, renderHeight);

    glClearDepthf(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    for (int i = 0; i < m_numDrawCallsPerIteration; i++)
    {
        program.use();
        program.setRandomUniforms(programContext.uniforms, programResources.shaderNameManglingSuffix, m_rnd);

        if (useDrawArrays)
            glDrawArrays(GL_TRIANGLE_STRIP, 0, m_numVerticesPerDrawCall);
        else
        {
            if (useClientMemoryIndexData)
            {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                glDrawElements(GL_TRIANGLE_STRIP, m_numVerticesPerDrawCall, GL_UNSIGNED_SHORT, &m_vertexIndices[0]);
            }
            else
            {
                m_buffers->get(indexBufferName).bind(GL_ELEMENT_ARRAY_BUFFER);
                glDrawElements(GL_TRIANGLE_STRIP, m_numVerticesPerDrawCall, GL_UNSIGNED_SHORT, nullptr);
            }
        }
    }

    for (int i = 0; i < (int)programContext.attributes.size(); i++)
        program.disableAttributeArray(programContext.attributes[i], programResources.shaderNameManglingSuffix);

    if (m_showDebugInfo)
        m_debugInfoRenderer->drawInfo(deGetTime() - m_startTimeSeconds, m_textures->computeApproxMemUsage(),
                                      m_maxTexMemoryUsageBytes, m_buffers->computeApproxMemUsage(),
                                      m_maxBufMemoryUsageBytes, m_currentIteration);

    if (m_currentIteration > 0)
    {
        // Log if a certain amount of time has passed since last log entry (or if this is the last iteration).

        const uint64_t loggingIntervalSeconds = 10;
        const uint64_t time                   = deGetTime();
        const uint64_t timeDiff               = time - m_lastLogTime;
        const int iterDiff                    = m_currentIteration - m_lastLogIteration;

        if (timeDiff >= loggingIntervalSeconds || m_currentIteration == m_numIterations - 1)
        {
            log << TestLog::Section("LogEntry" + toString(m_currentLogEntryNdx),
                                    "Log entry " + toString(m_currentLogEntryNdx))
                << TestLog::Message << "Time elapsed: " << getTimeStr(time - m_startTimeSeconds) << TestLog::EndMessage
                << TestLog::Message << "Frame number: " << m_currentIteration << TestLog::EndMessage << TestLog::Message
                << "Time since last log entry: " << timeDiff << "s" << TestLog::EndMessage << TestLog::Message
                << "Frames since last log entry: " << iterDiff << TestLog::EndMessage << TestLog::Message
                << "Average frame time since last log entry: "
                << de::floatToString((float)timeDiff / (float)iterDiff, 2) << "s" << TestLog::EndMessage
                << TestLog::Message << "Approximate texture memory usage: "
                << de::floatToString((float)m_textures->computeApproxMemUsage() / Mi, 2) << " MiB / "
                << de::floatToString((float)m_maxTexMemoryUsageBytes / Mi, 2) << " MiB" << TestLog::EndMessage
                << TestLog::Message << "Approximate buffer memory usage: "
                << de::floatToString((float)m_buffers->computeApproxMemUsage() / Mi, 2) << " MiB / "
                << de::floatToString((float)m_maxBufMemoryUsageBytes / Mi, 2) << " MiB" << TestLog::EndMessage
                << TestLog::EndSection;

            m_lastLogTime      = time;
            m_lastLogIteration = m_currentIteration;
            m_currentLogEntryNdx++;
        }
    }

    // Possibly remove or set-as-garbage some objects, depending on given probabilities.

    for (int texNdx = 0; texNdx < (int)programContext.textureSpecs.size(); texNdx++)
    {
        const string texName = textureNamePrefix + toString(texNdx);
        if (m_rnd.getFloat() < m_probabilities.deleteTexture)
            m_textures->remove(texName);
        else if (m_rnd.getFloat() < m_probabilities.wastefulTextureMemoryUsage)
            m_textures->markAsGarbage(texName);
    }

    if (m_buffers->has(indexBufferName))
    {
        if (m_rnd.getFloat() < m_probabilities.deleteBuffer)
            m_buffers->remove(indexBufferName);
        else if (m_rnd.getFloat() < m_probabilities.wastefulBufferMemoryUsage)
            m_buffers->markAsGarbage(indexBufferName);
    }

    if (separateAttributeBuffers)
    {
        for (int attrNdx = 0; attrNdx < (int)programContext.attributes.size(); attrNdx++)
        {
            const string curAttrBufNamePrefix = separateAttrBufNamePrefix + toString(attrNdx) + "_";

            if (m_buffers->has(curAttrBufNamePrefix + "0"))
            {
                if (m_rnd.getFloat() < m_probabilities.deleteBuffer)
                {
                    for (int i = 0; i < m_redundantBufferFactor; i++)
                        m_buffers->remove(curAttrBufNamePrefix + toString(i));
                }
                else if (m_rnd.getFloat() < m_probabilities.wastefulBufferMemoryUsage)
                {
                    for (int i = 0; i < m_redundantBufferFactor; i++)
                        m_buffers->markAsGarbage(curAttrBufNamePrefix + toString(i));
                }
            }
        }
    }
    else
    {
        if (m_buffers->has(unitedAttrBufferNamePrefix + "0"))
        {
            if (m_rnd.getFloat() < m_probabilities.deleteBuffer)
            {
                for (int i = 0; i < m_redundantBufferFactor; i++)
                    m_buffers->remove(unitedAttrBufferNamePrefix + toString(i));
            }
            else if (m_rnd.getFloat() < m_probabilities.wastefulBufferMemoryUsage)
            {
                for (int i = 0; i < m_redundantBufferFactor; i++)
                    m_buffers->markAsGarbage(unitedAttrBufferNamePrefix + toString(i));
            }
        }
    }

    GLU_CHECK_MSG("End of LongStressCase::iterate()");

    m_currentIteration++;
    if (m_currentIteration == m_numIterations)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Passed");
        return STOP;
    }
    else
        return CONTINUE;
}

} // namespace gls
} // namespace deqp
