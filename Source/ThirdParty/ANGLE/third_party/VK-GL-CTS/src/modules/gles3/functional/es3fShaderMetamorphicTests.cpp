/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
 *
 * Copyright 2017 Hugues Evrard, Imperial College London
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
 * \brief Shader metamorphic tests.
 *//*--------------------------------------------------------------------*/

#include "es3fShaderMetamorphicTests.hpp"
#include "glsShaderRenderCase.hpp"
#include "deUniquePtr.hpp"
#include "deFilePath.hpp"
#include "tcuTestContext.hpp"
#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuImageCompare.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuResource.hpp"
#include "gluPixelTransfer.hpp"
#include "gluDrawUtil.hpp"

#include "glwFunctions.hpp"

using std::vector;
using tcu::TestLog;

namespace deqp
{
namespace gles3
{
namespace Functional
{

static const int MAX_RENDER_WIDTH  = 256;
static const int MAX_RENDER_HEIGHT = 256;

typedef bool (*SanityCheckFunc)(const tcu::ConstPixelBufferAccess &);

/*--------------------------------------------------------------------*//*!
 * \brief ShaderMetamorphicVariant
 *
 * ShaderMetamorphicVariant aims at rendering a recipient shader and a
 * variant shader, and compare whether the resulting images are the
 * approximately the same. It also checks non-deterministic renderings,
 * by rendering each fragment shader a couple of times.
 *//*--------------------------------------------------------------------*/
class ShaderMetamorphicVariant : public TestCase
{
public:
    ShaderMetamorphicVariant(Context &context, const char *name, const std::string &vertexFilename,
                             const std::string &recipientFilename, const std::string &variantFilename,
                             SanityCheckFunc sanityCheck);
    ~ShaderMetamorphicVariant(void);
    IterateResult iterate(void);

private:
    const std::string m_vertexFilename;
    const std::string m_recipientFilename;
    const std::string m_variantFilename;
    SanityCheckFunc m_sanityCheck;

    std::string fileContents(const std::string &filename);
    void render(const tcu::PixelBufferAccess &img, const std::string &vertexSrc, const std::string &fragmentSrc);
    void checkNondet(const tcu::Surface &refImg, const std::string &vertexSrc, const std::string &fragmentSrc);
};

ShaderMetamorphicVariant::ShaderMetamorphicVariant(Context &context, const char *name,
                                                   const std::string &vertexFilename,
                                                   const std::string &recipientFilename,
                                                   const std::string &variantFilename, SanityCheckFunc sanityCheck)
    : TestCase(context, name, "Test a given variant")
    , m_vertexFilename(vertexFilename)
    , m_recipientFilename(recipientFilename)
    , m_variantFilename(variantFilename)
    , m_sanityCheck(sanityCheck)
{
}

ShaderMetamorphicVariant::~ShaderMetamorphicVariant(void)
{
}

std::string ShaderMetamorphicVariant::fileContents(const std::string &filename)
{
    de::UniquePtr<tcu::Resource> resource(m_testCtx.getArchive().getResource(filename.c_str()));
    int size = resource->getSize();
    std::vector<uint8_t> data;

    data.resize(size + 1);
    resource->read(&data[0], size);
    data[size]           = '\0';
    std::string contents = std::string((const char *)(&data[0]));
    return contents;
}

void ShaderMetamorphicVariant::render(const tcu::PixelBufferAccess &img, const std::string &vertexSrc,
                                      const std::string &fragmentSrc)
{
    TestLog &log             = m_testCtx.getLog();
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // Positions, shared between shaders
    const float positions[] = {
        -1.0f, 1.0f,  // top-left
        -1.0f, -1.0f, // bottom-left
        1.0f,  -1.0f, // bottom-right
        1.0f,  1.0f,  // top-right
    };

    const uint16_t indices[] = {
        0, 1, 2, // bottom-left triangle
        0, 3, 2, // top-right triangle
    };

    glu::VertexArrayBinding posBinding = glu::va::Float("coord2d", 2, 6, 0, &positions[0]);

    const glu::ShaderProgram program(m_context.getRenderContext(), glu::makeVtxFragSources(vertexSrc, fragmentSrc));
    log << program;

    if (!program.isOk())
        throw tcu::TestError("Compile failed");

    // Set uniforms expected in GraphicsFuzz generated programs
    gl.useProgram(program.getProgram());
    // Uniform: injectionSwitch
    int uniformLoc = gl.getUniformLocation(program.getProgram(), "injectionSwitch");
    if (uniformLoc != -1)
        gl.uniform2f(uniformLoc, 0.0f, 1.0f);
    // Uniform: resolution
    uniformLoc = gl.getUniformLocation(program.getProgram(), "resolution");
    if (uniformLoc != -1)
        gl.uniform2f(uniformLoc, glw::GLfloat(img.getWidth()), glw::GLfloat(img.getHeight()));
    // Uniform: mouse
    uniformLoc = gl.getUniformLocation(program.getProgram(), "mouse");
    if (uniformLoc != -1)
        gl.uniform2f(uniformLoc, 0.0f, 0.0f);
    // Uniform: time
    uniformLoc = gl.getUniformLocation(program.getProgram(), "time");
    if (uniformLoc != -1)
        gl.uniform1f(uniformLoc, 0.0f);

    // Render two times to check nondeterministic renderings
    glu::draw(m_context.getRenderContext(), program.getProgram(), 1, &posBinding,
              glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));
    glu::readPixels(m_context.getRenderContext(), 0, 0, img);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Draw");
}

void ShaderMetamorphicVariant::checkNondet(const tcu::Surface &refImg, const std::string &vertexSrc,
                                           const std::string &fragmentSrc)
{
    TestLog &log     = m_testCtx.getLog();
    tcu::Surface img = tcu::Surface(refImg.getWidth(), refImg.getHeight());

    render(img.getAccess(), vertexSrc, fragmentSrc);
    bool same = tcu::pixelThresholdCompare(log, "Result", "Image comparison result", img, refImg, tcu::RGBA(0, 0, 0, 0),
                                           tcu::COMPARE_LOG_RESULT);
    if (!same)
        throw tcu::TestError("Nondeterministic rendering");
}

ShaderMetamorphicVariant::IterateResult ShaderMetamorphicVariant::iterate(void)
{
    TestLog &log = m_testCtx.getLog();
    const tcu::RGBA threshold =
        tcu::RGBA(1, 1, 1, 1) + m_context.getRenderTarget().getPixelFormat().getColorThreshold();
    std::string vertexSrc     = fileContents(m_vertexFilename);
    std::string recipientSrc  = fileContents(m_recipientFilename);
    std::string variantSrc    = fileContents(m_variantFilename);
    const int width           = deMin32(m_context.getRenderTarget().getWidth(), MAX_RENDER_WIDTH);
    const int height          = deMin32(m_context.getRenderTarget().getHeight(), MAX_RENDER_HEIGHT);
    tcu::Surface recipientImg = tcu::Surface(width, height);
    tcu::Surface variantImg   = tcu::Surface(width, height);

    render(recipientImg.getAccess(), vertexSrc, recipientSrc);
    render(variantImg.getAccess(), vertexSrc, variantSrc);

    checkNondet(recipientImg, vertexSrc, recipientSrc);
    checkNondet(variantImg, vertexSrc, variantSrc);

    if (m_sanityCheck != nullptr)
    {
        bool isSane = m_sanityCheck(recipientImg.getAccess());
        if (!isSane)
            throw tcu::TestError("Quick check fails on recipient");
    }

    bool isOk = tcu::pixelThresholdCompare(log, "Result", "Image comparison result", recipientImg, variantImg,
                                           threshold, tcu::COMPARE_LOG_RESULT);

    m_testCtx.setTestResult(isOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                            isOk ? "Pass" : "Image comparison failed");

    return STOP;
}

/*--------------------------------------------------------------------*//*!
 * \brief ShaderMetamorphicShaderset
 *
 * ShaderMetamorphicShaderset gathers a set of ShaderMetamorphicVariant
 * for a similar recipient.
 *//*--------------------------------------------------------------------*/
class ShaderMetamorphicShaderset : public TestCaseGroup
{
public:
    ShaderMetamorphicShaderset(Context &context, const char *name, const std::string &vertexFilename,
                               const std::string &recipientFilename, std::vector<std::string> variantFilenames,
                               SanityCheckFunc sanityCheck);
    ~ShaderMetamorphicShaderset(void);
    virtual void init(void);

private:
    const std::string m_vertexFilename;
    const std::string m_recipientFilename;
    std::vector<std::string> m_variantFilenames;
    SanityCheckFunc m_sanityCheck;

    ShaderMetamorphicShaderset(const ShaderMetamorphicShaderset &);            // Not allowed!
    ShaderMetamorphicShaderset &operator=(const ShaderMetamorphicShaderset &); // Not allowed!
};

ShaderMetamorphicShaderset::ShaderMetamorphicShaderset(Context &context, const char *name,
                                                       const std::string &vertexFilename,
                                                       const std::string &recipientFilename,
                                                       std::vector<std::string> variantFilenames,
                                                       SanityCheckFunc sanityCheck)
    : TestCaseGroup(context, name, "Metamorphic Shader Set")
    , m_vertexFilename(vertexFilename)
    , m_recipientFilename(recipientFilename)
    , m_variantFilenames(variantFilenames)
    , m_sanityCheck(sanityCheck)
{
}

ShaderMetamorphicShaderset::~ShaderMetamorphicShaderset(void)
{
}

void ShaderMetamorphicShaderset::init(void)
{
    for (size_t variantNdx = 0; variantNdx < m_variantFilenames.size(); variantNdx++)
    {
        std::string variantName = de::FilePath(m_variantFilenames[variantNdx]).getBaseName();
        // Remove extension
        size_t pos  = variantName.find_last_of(".");
        variantName = variantName.substr(0, pos);

        addChild(new ShaderMetamorphicVariant(m_context, variantName.c_str(), m_vertexFilename, m_recipientFilename,
                                              m_variantFilenames[variantNdx], m_sanityCheck));
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief SanityPixel
 *
 * A place holder to store info on reference pixel for quick checking.
 *//*--------------------------------------------------------------------*/
class SanityPixel
{
public:
    float m_xRelative;
    float m_yRelative;
    tcu::Vec4 m_RGBA;

    SanityPixel(float xRelative, float yRelative, tcu::Vec4 RGBA);
};

SanityPixel::SanityPixel(float xRelative, float yRelative, tcu::Vec4 RGBA)
    : m_xRelative(xRelative)
    , m_yRelative(yRelative)
    , m_RGBA(RGBA)
{
}

static bool sanityComparePixels(const tcu::ConstPixelBufferAccess &img, std::vector<SanityPixel> sanityPixels)
{
    const int depth           = 0;
    const tcu::Vec4 threshold = tcu::Vec4(0.01f, 0.01f, 0.01f, 0.01f);

    for (uint32_t i = 0; i < sanityPixels.size(); i++)
    {
        SanityPixel sanPix = sanityPixels[i];
        int x              = (int)((float)img.getWidth() * sanPix.m_xRelative);
        int y              = (int)((float)img.getHeight() * sanPix.m_yRelative);
        tcu::Vec4 RGBA     = img.getPixel(x, y, depth);
        tcu::Vec4 diff     = abs(RGBA - sanPix.m_RGBA);
        for (int j = 0; j < 4; j++)
            if (diff[j] >= threshold[j])
                return false;
    }
    return true;
}

static bool sanityCheck_synthetic(const tcu::ConstPixelBufferAccess &img)
{
    std::vector<SanityPixel> sanityPixels;
    bool isOK;

    sanityPixels.push_back(SanityPixel(0.5f, 0.5f, tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f)));

    isOK = sanityComparePixels(img, sanityPixels);
    return isOK;
}

static bool sanityCheck_bubblesort_flag(const tcu::ConstPixelBufferAccess &img)
{
    std::vector<SanityPixel> sanityPixels;
    bool isOK;

    sanityPixels.push_back(SanityPixel(0.25f, 0.25f, tcu::Vec4(0.1f, 0.6f, 1.0f, 1.0f)));
    sanityPixels.push_back(SanityPixel(0.25f, 0.75f, tcu::Vec4(1.0f, 0.5f, 0.1f, 1.0f)));
    sanityPixels.push_back(SanityPixel(0.75f, 0.25f, tcu::Vec4(0.6f, 1.0f, 0.1f, 1.0f)));
    sanityPixels.push_back(SanityPixel(0.75f, 0.75f, tcu::Vec4(0.5f, 0.1f, 1.0f, 1.0f)));

    isOK = sanityComparePixels(img, sanityPixels);
    return isOK;
}

/*--------------------------------------------------------------------*//*!
 * \brief ShaderMetamorphicTests
 *
 * ShaderMetamorphicTests regroups metamorphic shadersets.
 *//*--------------------------------------------------------------------*/
ShaderMetamorphicTests::ShaderMetamorphicTests(Context &context)
    : TestCaseGroup(context, "metamorphic", "Shader Metamorphic Tests")
{
}

ShaderMetamorphicTests::~ShaderMetamorphicTests(void)
{
}

void ShaderMetamorphicTests::init(void)
{
    std::vector<std::string> fragNames;
    std::string vertexFilename = "graphicsfuzz/vertexShader.glsl";

    // synthetic
    fragNames.clear();
    fragNames.push_back("graphicsfuzz/synthetic/variant_1.frag");
    fragNames.push_back("graphicsfuzz/synthetic/variant_2.frag");
    fragNames.push_back("graphicsfuzz/synthetic/variant_3.frag");
    fragNames.push_back("graphicsfuzz/synthetic/variant_4.frag");
    addChild(new ShaderMetamorphicShaderset(m_context, "synthetic", vertexFilename,
                                            "graphicsfuzz/synthetic/recipient.frag", fragNames, sanityCheck_synthetic));

    // bubblesort_flag
    fragNames.clear();
    fragNames.push_back("graphicsfuzz/bubblesort_flag/variant_1.frag");
    fragNames.push_back("graphicsfuzz/bubblesort_flag/variant_2.frag");
    addChild(new ShaderMetamorphicShaderset(m_context, "bubblesort_flag", vertexFilename,
                                            "graphicsfuzz/bubblesort_flag/recipient.frag", fragNames,
                                            sanityCheck_bubblesort_flag));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
