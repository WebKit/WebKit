#ifndef _GLSLONGSTRESSCASE_HPP
#define _GLSLONGSTRESSCASE_HPP
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
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuTexture.hpp"
#include "tcuMatrix.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderUtil.hpp"
#include "glsTextureTestUtil.hpp"
#include "deRandom.hpp"
#include "deSharedPtr.hpp"

#include <string>
#include <vector>
#include <map>

namespace deqp
{
namespace gls
{

namespace LongStressCaseInternal
{

template <typename T>
class GLObjectManager;
class Program;
class Buffer;
class Texture;
class DebugInfoRenderer;

} // namespace LongStressCaseInternal

struct VarSpec
{
    union Value
    {
        float f[4 * 4]; // \note Matrices are stored in column major order.
        int i[4];
    };

    std::string name;
    glu::DataType type;
    Value minValue;
    Value maxValue;

    template <typename T>
    VarSpec(const std::string &name_, const T &minValue_, const T &maxValue_) : name(name_)
    {
        set(minValue_, maxValue_);
    }

    template <typename T>
    VarSpec(const std::string &name_, const T &value) : name(name_)
    {
        set(value, value);
    }

    void set(float minValue_, float maxValue_)
    {
        type          = glu::TYPE_FLOAT;
        minValue.f[0] = minValue_;
        maxValue.f[0] = maxValue_;
    }

    template <int ValSize>
    void set(const tcu::Vector<float, ValSize> &minValue_, const tcu::Vector<float, ValSize> &maxValue_)
    {
        type = glu::getDataTypeFloatVec(ValSize);
        vecToArr(minValue_, minValue.f);
        vecToArr(maxValue_, maxValue.f);
    }

    template <int ValRows, int ValCols>
    void set(const tcu::Matrix<float, ValRows, ValCols> &minValue_,
             const tcu::Matrix<float, ValRows, ValCols> &maxValue_)
    {
        type = glu::getDataTypeMatrix(ValCols, ValRows);
        matToArr(minValue_, minValue.f);
        matToArr(maxValue_, maxValue.f);
    }

    void set(int minValue_, int maxValue_)
    {
        type          = glu::TYPE_INT;
        minValue.i[0] = minValue_;
        maxValue.i[0] = maxValue_;
    }

    template <int ValSize>
    void set(const tcu::Vector<int, ValSize> &minValue_, const tcu::Vector<int, ValSize> &maxValue_)
    {
        type = glu::getDataTypeVector(glu::TYPE_INT, ValSize);
        vecToArr(minValue_, minValue.i);
        vecToArr(maxValue_, maxValue.i);
    }

private:
    template <typename T, int SrcSize, int DstSize>
    static inline void vecToArr(const tcu::Vector<T, SrcSize> &src, T (&dst)[DstSize])
    {
        DE_STATIC_ASSERT(DstSize >= SrcSize);
        for (int i = 0; i < SrcSize; i++)
            dst[i] = src[i];
    }

    template <int ValRows, int ValCols, int DstSize>
    static inline void matToArr(const tcu::Matrix<float, ValRows, ValCols> &src, float (&dst)[DstSize])
    {
        DE_STATIC_ASSERT(DstSize >= ValRows * ValCols);
        tcu::Array<float, ValRows *ValCols> data = src.getColumnMajorData();
        for (int i = 0; i < ValRows * ValCols; i++)
            dst[i] = data[i];
    }
};

struct TextureSpec
{
    glu::TextureTestUtil::TextureType textureType;
    uint32_t textureUnit;
    int width;
    int height;
    uint32_t format;
    uint32_t dataType;
    uint32_t internalFormat;
    bool useMipmap;
    uint32_t minFilter;
    uint32_t magFilter;
    uint32_t sWrap;
    uint32_t tWrap;
    tcu::Vec4 minValue;
    tcu::Vec4 maxValue;

    TextureSpec(const glu::TextureTestUtil::TextureType texType, const uint32_t unit, const int width_,
                const int height_, const uint32_t format_, const uint32_t dataType_, const uint32_t internalFormat_,
                const bool useMipmap_, const uint32_t minFilter_, const uint32_t magFilter_, const uint32_t sWrap_,
                const uint32_t tWrap_, const tcu::Vec4 &minValue_, const tcu::Vec4 &maxValue_)
        : textureType(texType)
        , textureUnit(unit)
        , width(width_)
        , height(height_)
        , format(format_)
        , dataType(dataType_)
        , internalFormat(internalFormat_)
        , useMipmap(useMipmap_)
        , minFilter(minFilter_)
        , magFilter(magFilter_)
        , sWrap(sWrap_)
        , tWrap(tWrap_)
        , minValue(minValue_)
        , maxValue(maxValue_)
    {
    }
};

/*--------------------------------------------------------------------*//*!
 * \brief Struct for a shader program sources and related data
 *
 * A ProgramContext holds a program's vertex and fragment shader sources
 * as well as specifications of its attributes, uniforms, and textures.
 * When given to a StressCase, the string ${NS} is replaced by a magic
 * number that varies between different compilations of the same program;
 * the same replacement is done in attributes' and uniforms' names. This
 * can be used to avoid shader caching by the GL, by e.g. suffixing each
 * attribute, uniform and varying name with ${NS} in the shader source.
 *//*--------------------------------------------------------------------*/
struct ProgramContext
{
    std::string vertexSource;
    std::string fragmentSource;
    std::vector<VarSpec> attributes;
    std::vector<VarSpec> uniforms;

    std::vector<TextureSpec>
        textureSpecs; //!< \note If multiple textures have same unit, one of them is picked randomly.

    std::string
        positionAttrName; //!< \note Position attribute may get a bit more careful handling than just complete random.

    ProgramContext(const char *const vtxShaderSource_, const char *const fragShaderSource_,
                   const char *const positionAttrName_)
        : vertexSource(vtxShaderSource_)
        , fragmentSource(fragShaderSource_)
        , positionAttrName(positionAttrName_)
    {
    }
};

class LongStressCase : public tcu::TestCase
{
public:
    //! Probabilities for actions that may be taken on each iteration. \note The texture and buffer specific actions are randomized per texture or buffer.
    struct FeatureProbabilities
    {
        float rebuildProgram;         //!< Rebuild program, with variable name-mangling.
        float reuploadTexture;        //!< Reupload texture, even if it already exists and has been uploaded.
        float reuploadBuffer;         //!< Reupload buffer, even if it already exists and has been uploaded.
        float reuploadWithTexImage;   //!< Use glTexImage*() when re-uploading texture, not glTexSubImage*().
        float reuploadWithBufferData; //!< Use glBufferData() when re-uploading buffer, not glBufferSubData().
        float deleteTexture;          //!< Delete texture at end of iteration, even if we could re-use it.
        float deleteBuffer;           //!< Delete buffer at end of iteration, even if we could re-use it.
        float
            wastefulTextureMemoryUsage; //!< Don't re-use a texture, and don't delete it until given memory limit is hit.
        float
            wastefulBufferMemoryUsage; //!< Don't re-use a buffer, and don't delete it until given memory limit is hit.
        float
            clientMemoryAttributeData; //!< Use client memory for vertex attribute data when drawing (instead of GL buffers).
        float clientMemoryIndexData; //!< Use client memory for vertex indices when drawing (instead of GL buffers).
        float
            randomBufferUploadTarget; //!< Use a random target when setting buffer data (i.e. not necessarily the one it'll be ultimately bound to).
        float
            randomBufferUsage; //!< Use a random buffer usage parameter with glBufferData(), instead of the ones specified as params for the case.
        float useDrawArrays;            //!< Use glDrawArrays() instead of glDrawElements().
        float separateAttributeBuffers; //!< Give each vertex attribute its own buffer.

        // Named parameter idiom: helpers that can be used when making temporaries, e.g. FeatureProbabilities().pReuploadTexture(1.0f).pReuploadWithTexImage(1.0f)
        FeatureProbabilities &pRebuildProgram(const float prob)
        {
            rebuildProgram = prob;
            return *this;
        }
        FeatureProbabilities &pReuploadTexture(const float prob)
        {
            reuploadTexture = prob;
            return *this;
        }
        FeatureProbabilities &pReuploadBuffer(const float prob)
        {
            reuploadBuffer = prob;
            return *this;
        }
        FeatureProbabilities &pReuploadWithTexImage(const float prob)
        {
            reuploadWithTexImage = prob;
            return *this;
        }
        FeatureProbabilities &pReuploadWithBufferData(const float prob)
        {
            reuploadWithBufferData = prob;
            return *this;
        }
        FeatureProbabilities &pDeleteTexture(const float prob)
        {
            deleteTexture = prob;
            return *this;
        }
        FeatureProbabilities &pDeleteBuffer(const float prob)
        {
            deleteBuffer = prob;
            return *this;
        }
        FeatureProbabilities &pWastefulTextureMemoryUsage(const float prob)
        {
            wastefulTextureMemoryUsage = prob;
            return *this;
        }
        FeatureProbabilities &pWastefulBufferMemoryUsage(const float prob)
        {
            wastefulBufferMemoryUsage = prob;
            return *this;
        }
        FeatureProbabilities &pClientMemoryAttributeData(const float prob)
        {
            clientMemoryAttributeData = prob;
            return *this;
        }
        FeatureProbabilities &pClientMemoryIndexData(const float prob)
        {
            clientMemoryIndexData = prob;
            return *this;
        }
        FeatureProbabilities &pRandomBufferUploadTarget(const float prob)
        {
            randomBufferUploadTarget = prob;
            return *this;
        }
        FeatureProbabilities &pRandomBufferUsage(const float prob)
        {
            randomBufferUsage = prob;
            return *this;
        }
        FeatureProbabilities &pUseDrawArrays(const float prob)
        {
            useDrawArrays = prob;
            return *this;
        }
        FeatureProbabilities &pSeparateAttribBuffers(const float prob)
        {
            separateAttributeBuffers = prob;
            return *this;
        }

        FeatureProbabilities(void)
            : rebuildProgram(0.0f)
            , reuploadTexture(0.0f)
            , reuploadBuffer(0.0f)
            , reuploadWithTexImage(0.0f)
            , reuploadWithBufferData(0.0f)
            , deleteTexture(0.0f)
            , deleteBuffer(0.0f)
            , wastefulTextureMemoryUsage(0.0f)
            , wastefulBufferMemoryUsage(0.0f)
            , clientMemoryAttributeData(0.0f)
            , clientMemoryIndexData(0.0f)
            , randomBufferUploadTarget(0.0f)
            , randomBufferUsage(0.0f)
            , useDrawArrays(0.0f)
            , separateAttributeBuffers(0.0f)
        {
        }
    };

    LongStressCase(tcu::TestContext &testCtx, const glu::RenderContext &renderCtx, const char *name, const char *desc,
                   int maxTexMemoryUsageBytes, //!< Approximate upper bound on GL texture memory usage.
                   int maxBufMemoryUsageBytes, //!< Approximate upper bound on GL buffer memory usage.
                   int numDrawCallsPerIteration, int numTrianglesPerDrawCall,
                   const std::vector<ProgramContext> &programContexts, const FeatureProbabilities &probabilities,
                   uint32_t indexBufferUsage, uint32_t attrBufferUsage, int redundantBufferFactor = 1,
                   bool showDebugInfo = false);

    ~LongStressCase(void);

    void init(void);
    void deinit(void);

    IterateResult iterate(void);

private:
    LongStressCase(const LongStressCase &);
    LongStressCase &operator=(const LongStressCase &);

    const glu::RenderContext &m_renderCtx;
    const int m_maxTexMemoryUsageBytes;
    const int m_maxBufMemoryUsageBytes;
    const int m_numDrawCallsPerIteration;
    const int m_numTrianglesPerDrawCall;
    const int m_numVerticesPerDrawCall;
    const std::vector<ProgramContext> m_programContexts;
    const FeatureProbabilities m_probabilities;
    const uint32_t m_indexBufferUsage;
    const uint32_t m_attrBufferUsage;
    const int
        m_redundantBufferFactor; //!< By what factor we allocate redundant buffers. Default is 1, i.e. no redundancy.
    const bool m_showDebugInfo;

    const int m_numIterations;
    const bool m_isGLES3;

    int m_currentIteration;
    uint64_t m_startTimeSeconds; //!< Set at beginning of first iteration.
    uint64_t m_lastLogTime;
    int m_lastLogIteration;
    int m_currentLogEntryNdx;

    de::Random m_rnd;
    LongStressCaseInternal::GLObjectManager<LongStressCaseInternal::Program> *m_programs;
    LongStressCaseInternal::GLObjectManager<LongStressCaseInternal::Buffer> *m_buffers;
    LongStressCaseInternal::GLObjectManager<LongStressCaseInternal::Texture> *m_textures;
    std::vector<uint16_t> m_vertexIndices;

    struct ProgramResources
    {
        std::vector<uint8_t> attrDataBuf;
        std::vector<int> attrDataOffsets;
        std::vector<int> attrDataSizes;
        std::vector<de::SharedPtr<tcu::TextureLevel>> unusedTextures;
        std::string shaderNameManglingSuffix;
    };

    std::vector<ProgramResources> m_programResources;

    LongStressCaseInternal::DebugInfoRenderer *m_debugInfoRenderer;
};

} // namespace gls
} // namespace deqp

#endif // _GLSLONGSTRESSCASE_HPP
