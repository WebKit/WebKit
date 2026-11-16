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
 * \brief Shader - render state interaction case.
 *//*--------------------------------------------------------------------*/

#include "glsFragOpInteractionCase.hpp"

#include "glsRandomShaderProgram.hpp"
#include "glsFragmentOpUtil.hpp"
#include "glsInteractionTestUtil.hpp"

#include "gluRenderContext.hpp"
#include "gluContextInfo.hpp"

#include "rsgShader.hpp"
#include "rsgProgramGenerator.hpp"
#include "rsgUtils.hpp"

#include "sglrContext.hpp"
#include "sglrReferenceContext.hpp"
#include "sglrGLContext.hpp"
#include "sglrContextUtil.hpp"

#include "tcuRenderTarget.hpp"
#include "tcuImageCompare.hpp"

#include "deRandom.hpp"
#include "deString.h"
#include "deStringUtil.hpp"

#include "glwEnums.hpp"

#include "gluDrawUtil.hpp"

namespace deqp
{
namespace gls
{

using std::string;
using std::vector;
using tcu::IVec2;
using tcu::IVec4;
using tcu::Vec2;
using tcu::Vec4;

using gls::InteractionTestUtil::RenderState;
using gls::InteractionTestUtil::StencilState;

enum
{
    NUM_ITERATIONS             = 5,
    NUM_COMMANDS_PER_ITERATION = 5,
    VIEWPORT_WIDTH             = 64,
    VIEWPORT_HEIGHT            = 64
};

namespace
{

static void computeVertexLayout(const vector<rsg::ShaderInput *> &attributes, int numVertices,
                                vector<glu::VertexArrayBinding> *layout, int *stride)
{
    DE_ASSERT(layout->empty());

    int curOffset = 0;

    for (vector<rsg::ShaderInput *>::const_iterator iter = attributes.begin(); iter != attributes.end(); ++iter)
    {
        const rsg::ShaderInput *attrib = *iter;
        const rsg::Variable *var       = attrib->getVariable();
        const rsg::VariableType &type  = var->getType();
        const int numComps             = type.getNumElements();

        TCU_CHECK_INTERNAL(type.getBaseType() == rsg::VariableType::TYPE_FLOAT &&
                           de::inRange(type.getNumElements(), 1, 4));

        layout->push_back(glu::va::Float(var->getName(), numComps, numVertices, 0 /* computed later */,
                                         (const float *)(uintptr_t)curOffset));

        curOffset += numComps * (int)sizeof(float);
    }

    for (vector<glu::VertexArrayBinding>::iterator vaIter = layout->begin(); vaIter != layout->end(); ++vaIter)
        vaIter->pointer.stride = curOffset;

    *stride = curOffset;
}

class VertexDataStorage
{
public:
    VertexDataStorage(const vector<rsg::ShaderInput *> &attributes, int numVertices);

    int getDataSize(void) const
    {
        return (int)m_data.size();
    }
    void *getBasePtr(void)
    {
        return m_data.empty() ? nullptr : &m_data[0];
    }
    const void *getBasePtr(void) const
    {
        return m_data.empty() ? nullptr : &m_data[0];
    }

    const std::vector<glu::VertexArrayBinding> &getLayout(void) const
    {
        return m_layout;
    }

    int getNumEntries(void) const
    {
        return (int)m_layout.size();
    }
    const glu::VertexArrayBinding &getLayoutEntry(int ndx) const
    {
        return m_layout[ndx];
    }

private:
    std::vector<uint8_t> m_data;
    std::vector<glu::VertexArrayBinding> m_layout;
};

VertexDataStorage::VertexDataStorage(const vector<rsg::ShaderInput *> &attributes, int numVertices)
{
    int stride = 0;
    computeVertexLayout(attributes, numVertices, &m_layout, &stride);
    m_data.resize(stride * numVertices);
}

static inline glu::VertexArrayBinding getEntryWithPointer(const VertexDataStorage &data, int ndx)
{
    const glu::VertexArrayBinding &entry = data.getLayoutEntry(ndx);
    return glu::VertexArrayBinding(
        entry.binding,
        glu::VertexArrayPointer(entry.pointer.componentType, entry.pointer.convert, entry.pointer.numComponents,
                                entry.pointer.numElements, entry.pointer.stride,
                                (const void *)((uintptr_t)entry.pointer.data + (uintptr_t)data.getBasePtr())));
}

template <int Size>
static void setVertex(const glu::VertexArrayPointer &pointer, int vertexNdx, const tcu::Vector<float, Size> &value)
{
    // \todo [2013-12-14 pyry] Implement other modes.
    DE_ASSERT(pointer.componentType == glu::VTX_COMP_FLOAT && pointer.convert == glu::VTX_COMP_CONVERT_NONE);
    DE_ASSERT(pointer.numComponents == Size);
    DE_ASSERT(de::inBounds(vertexNdx, 0, pointer.numElements));

    float *dst = (float *)((uint8_t *)pointer.data + pointer.stride * vertexNdx);

    for (int ndx = 0; ndx < Size; ndx++)
        dst[ndx] = value[ndx];
}

template <int Size>
static tcu::Vector<float, Size> interpolateRange(const rsg::ConstValueRangeAccess &range,
                                                 const tcu::Vector<float, Size> &t)
{
    tcu::Vector<float, Size> result;

    for (int ndx = 0; ndx < Size; ndx++)
        result[ndx] = range.getMin().component(ndx).asFloat() * (1.0f - t[ndx]) +
                      range.getMax().component(ndx).asFloat() * t[ndx];

    return result;
}

struct Quad
{
    tcu::IVec2 posA;
    tcu::IVec2 posB;
};

struct RenderCommand
{
    Quad quad;
    float depth;
    RenderState state;

    RenderCommand(void) : depth(0.0f)
    {
    }
};

static Quad getRandomQuad(de::Random &rnd, int targetW, int targetH)
{
    // \note In viewport coordinates.
    // \todo [2012-12-18 pyry] Out-of-bounds values.
    const int maxOutOfBounds = 0;
    const float minSize      = 0.5f;

    const int minW = deCeilFloatToInt32(minSize * (float)targetW);
    const int minH = deCeilFloatToInt32(minSize * (float)targetH);
    const int maxW = targetW + 2 * maxOutOfBounds;
    const int maxH = targetH + 2 * maxOutOfBounds;

    const int width  = rnd.getInt(minW, maxW);
    const int height = rnd.getInt(minH, maxH);
    const int x      = rnd.getInt(-maxOutOfBounds, targetW + maxOutOfBounds - width);
    const int y      = rnd.getInt(-maxOutOfBounds, targetH + maxOutOfBounds - height);

    const bool flipX = rnd.getBool();
    const bool flipY = rnd.getBool();

    Quad quad;

    quad.posA = tcu::IVec2(flipX ? (x + width - 1) : x, flipY ? (y + height - 1) : y);
    quad.posB = tcu::IVec2(flipX ? x : (x + width - 1), flipY ? y : (y + height - 1));

    return quad;
}

static float getRandomDepth(de::Random &rnd)
{
    // \note Not using depth 1.0 since clearing with 1.0 and rendering with 1.0 may not be same value.
    static const float depthValues[] = {0.0f, 0.2f, 0.4f, 0.5f, 0.51f, 0.6f, 0.8f, 0.95f};
    return rnd.choose<float>(DE_ARRAY_BEGIN(depthValues), DE_ARRAY_END(depthValues));
}

static void computeRandomRenderCommand(de::Random &rnd, RenderCommand &command, glu::ApiType apiType, int targetW,
                                       int targetH)
{
    command.quad  = getRandomQuad(rnd, targetW, targetH);
    command.depth = getRandomDepth(rnd);
    gls::InteractionTestUtil::computeRandomRenderState(rnd, command.state, apiType, targetW, targetH);
}

static void setRenderState(sglr::Context &ctx, const RenderState &state)
{
    if (state.scissorTestEnabled)
    {
        ctx.enable(GL_SCISSOR_TEST);
        ctx.scissor(state.scissorRectangle.left, state.scissorRectangle.bottom, state.scissorRectangle.width,
                    state.scissorRectangle.height);
    }
    else
        ctx.disable(GL_SCISSOR_TEST);

    if (state.stencilTestEnabled)
    {
        ctx.enable(GL_STENCIL_TEST);

        for (int face = 0; face < rr::FACETYPE_LAST; face++)
        {
            uint32_t glFace             = face == rr::FACETYPE_BACK ? GL_BACK : GL_FRONT;
            const StencilState &sParams = state.stencil[face];

            ctx.stencilFuncSeparate(glFace, sParams.function, sParams.reference, sParams.compareMask);
            ctx.stencilOpSeparate(glFace, sParams.stencilFailOp, sParams.depthFailOp, sParams.depthPassOp);
            ctx.stencilMaskSeparate(glFace, sParams.writeMask);
        }
    }
    else
        ctx.disable(GL_STENCIL_TEST);

    if (state.depthTestEnabled)
    {
        ctx.enable(GL_DEPTH_TEST);
        ctx.depthFunc(state.depthFunc);
        ctx.depthMask(state.depthWriteMask ? GL_TRUE : GL_FALSE);
    }
    else
        ctx.disable(GL_DEPTH_TEST);

    if (state.blendEnabled)
    {
        ctx.enable(GL_BLEND);
        ctx.blendEquationSeparate(state.blendRGBState.equation, state.blendAState.equation);
        ctx.blendFuncSeparate(state.blendRGBState.srcFunc, state.blendRGBState.dstFunc, state.blendAState.srcFunc,
                              state.blendAState.dstFunc);
        ctx.blendColor(state.blendColor.x(), state.blendColor.y(), state.blendColor.z(), state.blendColor.w());
    }
    else
        ctx.disable(GL_BLEND);

    if (state.ditherEnabled)
        ctx.enable(GL_DITHER);
    else
        ctx.disable(GL_DITHER);

    ctx.colorMask(state.colorMask[0] ? GL_TRUE : GL_FALSE, state.colorMask[1] ? GL_TRUE : GL_FALSE,
                  state.colorMask[2] ? GL_TRUE : GL_FALSE, state.colorMask[3] ? GL_TRUE : GL_FALSE);
}

static void renderQuad(sglr::Context &ctx, const glu::VertexArrayPointer &posPtr, const Quad &quad, const float depth)
{
    const uint16_t indices[] = {0, 1, 2, 2, 1, 3};

    const bool flipX    = quad.posB.x() < quad.posA.x();
    const bool flipY    = quad.posB.y() < quad.posA.y();
    const int viewportX = de::min(quad.posA.x(), quad.posB.x());
    const int viewportY = de::min(quad.posA.y(), quad.posB.y());
    const int viewportW = de::abs(quad.posA.x() - quad.posB.x()) + 1;
    const int viewportH = de::abs(quad.posA.y() - quad.posB.y()) + 1;

    const Vec2 pA(flipX ? 1.0f : -1.0f, flipY ? 1.0f : -1.0f);
    const Vec2 pB(flipX ? -1.0f : 1.0f, flipY ? -1.0f : 1.0f);

    setVertex(posPtr, 0, Vec4(pA.x(), pA.y(), depth, 1.0f));
    setVertex(posPtr, 1, Vec4(pB.x(), pA.y(), depth, 1.0f));
    setVertex(posPtr, 2, Vec4(pA.x(), pB.y(), depth, 1.0f));
    setVertex(posPtr, 3, Vec4(pB.x(), pB.y(), depth, 1.0f));

    ctx.viewport(viewportX, viewportY, viewportW, viewportH);
    ctx.drawElements(GL_TRIANGLES, DE_LENGTH_OF_ARRAY(indices), GL_UNSIGNED_SHORT, &indices[0]);
}

static void render(sglr::Context &ctx, const glu::VertexArrayPointer &posPtr, const RenderCommand &cmd)
{
    setRenderState(ctx, cmd.state);
    renderQuad(ctx, posPtr, cmd.quad, cmd.depth);
}

static void setupAttributes(sglr::Context &ctx, const VertexDataStorage &vertexData, uint32_t program)
{
    for (int attribNdx = 0; attribNdx < vertexData.getNumEntries(); ++attribNdx)
    {
        const glu::VertexArrayBinding bindingPtr = getEntryWithPointer(vertexData, attribNdx);
        const int attribLoc                      = bindingPtr.binding.type == glu::BindingPoint::BPTYPE_NAME ?
                                                       ctx.getAttribLocation(program, bindingPtr.binding.name.c_str()) :
                                                       bindingPtr.binding.location;

        DE_ASSERT(bindingPtr.pointer.componentType == glu::VTX_COMP_FLOAT);

        if (attribLoc >= 0)
        {
            ctx.enableVertexAttribArray(attribLoc);
            ctx.vertexAttribPointer(attribLoc, bindingPtr.pointer.numComponents, GL_FLOAT, GL_FALSE,
                                    bindingPtr.pointer.stride, bindingPtr.pointer.data);
        }
    }
}

void setUniformValue(sglr::Context &ctx, int location, rsg::ConstValueAccess value)
{
    DE_STATIC_ASSERT(sizeof(rsg::Scalar) == sizeof(float));
    DE_STATIC_ASSERT(sizeof(rsg::Scalar) == sizeof(int));

    switch (value.getType().getBaseType())
    {
    case rsg::VariableType::TYPE_FLOAT:
        switch (value.getType().getNumElements())
        {
        case 1:
            ctx.uniform1fv(location, 1, (float *)value.value().getValuePtr());
            break;
        case 2:
            ctx.uniform2fv(location, 1, (float *)value.value().getValuePtr());
            break;
        case 3:
            ctx.uniform3fv(location, 1, (float *)value.value().getValuePtr());
            break;
        case 4:
            ctx.uniform4fv(location, 1, (float *)value.value().getValuePtr());
            break;
        default:
            TCU_FAIL("Unsupported type");
        }
        break;

    case rsg::VariableType::TYPE_INT:
    case rsg::VariableType::TYPE_BOOL:
    case rsg::VariableType::TYPE_SAMPLER_2D:
    case rsg::VariableType::TYPE_SAMPLER_CUBE:
        switch (value.getType().getNumElements())
        {
        case 1:
            ctx.uniform1iv(location, 1, (int *)value.value().getValuePtr());
            break;
        case 2:
            ctx.uniform2iv(location, 1, (int *)value.value().getValuePtr());
            break;
        case 3:
            ctx.uniform3iv(location, 1, (int *)value.value().getValuePtr());
            break;
        case 4:
            ctx.uniform4iv(location, 1, (int *)value.value().getValuePtr());
            break;
        default:
            TCU_FAIL("Unsupported type");
        }
        break;

    default:
        throw tcu::InternalError("Unsupported type", "", __FILE__, __LINE__);
    }
}

static int findShaderInputIndex(const vector<rsg::ShaderInput *> &vars, const char *name)
{
    for (int ndx = 0; ndx < (int)vars.size(); ++ndx)
    {
        if (strcmp(vars[ndx]->getVariable()->getName(), name) == 0)
            return ndx;
    }

    throw tcu::InternalError(string(name) + " not found in shader inputs");
}

static float getWellBehavingChannelColor(float v, int numBits)
{
    DE_ASSERT(de::inRange(numBits, 0, 32));

    // clear color may not be accurately representable in the target format. If the clear color is
    // on a representable value mapping range border, it could be rounded differently by the GL and in
    // SGLR adding an unexpected error source. However, selecting an accurately representable background
    // color would effectively disable dithering. To allow dithering and to prevent undefined rounding
    // direction from affecting results, round accurate color to target color format with 8 sub-units
    // (3 bits). If the selected sub-unit value is 3 or 4 (bordering 0.5), replace it with 2 and 5,
    // respectively.

    if (numBits == 0 || v <= 0.0f || v >= 1.0f)
    {
        // already accurately representable
        return v;
    }
    else
    {
        const uint64_t numSubBits      = 3;
        const uint64_t subUnitBorderLo = (1u << (numSubBits - 1u)) - 1u;
        const uint64_t subUnitBorderHi = 1u << (numSubBits - 1u);
        const uint64_t maxFixedValue   = (1u << (numBits + numSubBits)) - 1u;
        const uint64_t fixedValue      = deRoundFloatToInt64(v * (float)maxFixedValue);

        const uint64_t units    = fixedValue >> numSubBits;
        const uint64_t subUnits = fixedValue & ((1u << numSubBits) - 1u);

        const uint64_t tweakedSubUnits = (subUnits == subUnitBorderLo) ? (subUnitBorderLo - 1) :
                                         (subUnits == subUnitBorderHi) ? (subUnitBorderHi + 1) :
                                                                         (subUnits);
        const uint64_t tweakedValue    = (units << numSubBits) | (tweakedSubUnits);

        return float(tweakedValue) / float(maxFixedValue);
    }
}

static tcu::Vec4 getWellBehavingColor(const tcu::Vec4 &accurateColor, const tcu::PixelFormat &format)
{
    return tcu::Vec4(getWellBehavingChannelColor(accurateColor[0], format.redBits),
                     getWellBehavingChannelColor(accurateColor[1], format.greenBits),
                     getWellBehavingChannelColor(accurateColor[2], format.blueBits),
                     getWellBehavingChannelColor(accurateColor[3], format.alphaBits));
}

} // namespace

struct FragOpInteractionCase::ReferenceContext
{
    const sglr::ReferenceContextLimits limits;
    sglr::ReferenceContextBuffers buffers;
    sglr::ReferenceContext context;

    ReferenceContext(glu::RenderContext &renderCtx, int width, int height)
        : limits(renderCtx)
        , buffers(renderCtx.getRenderTarget().getPixelFormat(), renderCtx.getRenderTarget().getDepthBits(),
                  renderCtx.getRenderTarget().getStencilBits(), width, height)
        , context(limits, buffers.getColorbuffer(), buffers.getDepthbuffer(), buffers.getStencilbuffer())
    {
    }
};

FragOpInteractionCase::FragOpInteractionCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                             const glu::ContextInfo &ctxInfo, const char *name,
                                             const rsg::ProgramParameters &params)
    : TestCase(testCtx, name, "")
    , m_renderCtx(renderCtx)
    , m_ctxInfo(ctxInfo)
    , m_params(params)
    , m_vertexShader(rsg::Shader::TYPE_VERTEX)
    , m_fragmentShader(rsg::Shader::TYPE_FRAGMENT)
    , m_program(nullptr)
    , m_glCtx(nullptr)
    , m_referenceCtx(nullptr)
    , m_glProgram(0)
    , m_refProgram(0)
    , m_iterNdx(0)
{
}

FragOpInteractionCase::~FragOpInteractionCase(void)
{
    FragOpInteractionCase::deinit();
}

void FragOpInteractionCase::init(void)
{
    de::Random rnd(m_params.seed ^ 0x232faac);
    const int viewportW = de::min<int>(m_renderCtx.getRenderTarget().getWidth(), VIEWPORT_WIDTH);
    const int viewportH = de::min<int>(m_renderCtx.getRenderTarget().getHeight(), VIEWPORT_HEIGHT);
    const int viewportX = rnd.getInt(0, m_renderCtx.getRenderTarget().getWidth() - viewportW);
    const int viewportY = rnd.getInt(0, m_renderCtx.getRenderTarget().getHeight() - viewportH);

    rsg::ProgramGenerator generator;

    generator.generate(m_params, m_vertexShader, m_fragmentShader);
    rsg::computeUnifiedUniforms(m_vertexShader, m_fragmentShader, m_unifiedUniforms);

    try
    {
        DE_ASSERT(!m_program);
        m_program = new gls::RandomShaderProgram(m_vertexShader, m_fragmentShader, (int)m_unifiedUniforms.size(),
                                                 m_unifiedUniforms.empty() ? nullptr : &m_unifiedUniforms[0]);

        DE_ASSERT(!m_referenceCtx);
        m_referenceCtx = new ReferenceContext(m_renderCtx, viewportW, viewportH);

        DE_ASSERT(!m_glCtx);
        m_glCtx = new sglr::GLContext(m_renderCtx, m_testCtx.getLog(),
                                      sglr::GLCONTEXT_LOG_CALLS | sglr::GLCONTEXT_LOG_PROGRAMS,
                                      IVec4(viewportX, viewportY, viewportW, viewportH));

        m_refProgram = m_referenceCtx->context.createProgram(m_program);
        m_glProgram  = m_glCtx->createProgram(m_program);

        m_viewportSize = tcu::IVec2(viewportW, viewportH);
        m_iterNdx      = 0;
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    }
    catch (...)
    {
        // Save some memory by cleaning up stuff.
        FragOpInteractionCase::deinit();
        throw;
    }
}

void FragOpInteractionCase::deinit(void)
{
    delete m_referenceCtx;
    m_referenceCtx = nullptr;

    delete m_glCtx;
    m_glCtx = nullptr;

    delete m_program;
    m_program = nullptr;
}

FragOpInteractionCase::IterateResult FragOpInteractionCase::iterate(void)
{
    de::Random rnd(m_params.seed ^ deInt32Hash(m_iterNdx));
    const tcu::ScopedLogSection section(m_testCtx.getLog(), string("Iter") + de::toString(m_iterNdx),
                                        string("Iteration ") + de::toString(m_iterNdx));

    const int positionNdx = findShaderInputIndex(m_vertexShader.getInputs(), "dEQP_Position");

    const int numVertices = 4;
    VertexDataStorage vertexData(m_vertexShader.getInputs(), numVertices);
    std::vector<rsg::VariableValue> uniformValues;
    std::vector<RenderCommand> renderCmds(NUM_COMMANDS_PER_ITERATION);

    tcu::Surface rendered(m_viewportSize.x(), m_viewportSize.y());
    tcu::Surface reference(m_viewportSize.x(), m_viewportSize.y());

    const tcu::Vec4 vtxInterpFactors[] = {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.5f, 0.5f),
                                          tcu::Vec4(0.0f, 1.0f, 0.5f, 0.5f), tcu::Vec4(1.0f, 1.0f, 1.0f, 0.0f)};

    rsg::computeUniformValues(rnd, uniformValues, m_unifiedUniforms);

    for (int attribNdx = 0; attribNdx < (int)m_vertexShader.getInputs().size(); ++attribNdx)
    {
        if (attribNdx == positionNdx)
            continue;

        const rsg::ShaderInput *shaderIn            = m_vertexShader.getInputs()[attribNdx];
        const rsg::VariableType &varType            = shaderIn->getVariable()->getType();
        const rsg::ConstValueRangeAccess valueRange = shaderIn->getValueRange();
        const int numComponents                     = varType.getNumElements();
        const glu::VertexArrayBinding layoutEntry   = getEntryWithPointer(vertexData, attribNdx);

        DE_ASSERT(varType.getBaseType() == rsg::VariableType::TYPE_FLOAT);

        for (int vtxNdx = 0; vtxNdx < 4; vtxNdx++)
        {
            const int fNdx     = (attribNdx + vtxNdx + m_iterNdx) % DE_LENGTH_OF_ARRAY(vtxInterpFactors);
            const tcu::Vec4 &f = vtxInterpFactors[fNdx];

            switch (numComponents)
            {
            case 1:
                setVertex(layoutEntry.pointer, vtxNdx, interpolateRange(valueRange, f.toWidth<1>()));
                break;
            case 2:
                setVertex(layoutEntry.pointer, vtxNdx, interpolateRange(valueRange, f.toWidth<2>()));
                break;
            case 3:
                setVertex(layoutEntry.pointer, vtxNdx, interpolateRange(valueRange, f.toWidth<3>()));
                break;
            case 4:
                setVertex(layoutEntry.pointer, vtxNdx, interpolateRange(valueRange, f.toWidth<4>()));
                break;
            default:
                DE_ASSERT(false);
            }
        }
    }

    for (vector<RenderCommand>::iterator cmdIter = renderCmds.begin(); cmdIter != renderCmds.end(); ++cmdIter)
        computeRandomRenderCommand(rnd, *cmdIter, m_renderCtx.getType().getAPI(), m_viewportSize.x(),
                                   m_viewportSize.y());

    // Workaround for inaccurate barycentric/depth computation in current reference renderer:
    // Small bias is added to the draw call depths, in increasing order, to avoid accuracy issues in depth comparison.
    for (int cmdNdx = 0; cmdNdx < (int)renderCmds.size(); cmdNdx++)
        renderCmds[cmdNdx].depth += 0.0231725f * float(cmdNdx);

    {
        const glu::VertexArrayPointer posPtr = getEntryWithPointer(vertexData, positionNdx).pointer;

        sglr::Context *const contexts[]  = {m_glCtx, &m_referenceCtx->context};
        const uint32_t programs[]        = {m_glProgram, m_refProgram};
        tcu::PixelBufferAccess readDst[] = {rendered.getAccess(), reference.getAccess()};

        const tcu::Vec4 accurateClearColor = tcu::Vec4(0.0f, 0.25f, 0.5f, 1.0f);
        const tcu::Vec4 clearColor =
            getWellBehavingColor(accurateClearColor, m_renderCtx.getRenderTarget().getPixelFormat());

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(contexts); ndx++)
        {
            sglr::Context &ctx     = *contexts[ndx];
            const uint32_t program = programs[ndx];

            setupAttributes(ctx, vertexData, program);

            ctx.disable(GL_SCISSOR_TEST);
            ctx.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            ctx.depthMask(GL_TRUE);
            ctx.stencilMask(~0u);
            ctx.clearColor(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());
            ctx.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

            ctx.useProgram(program);

            for (vector<rsg::VariableValue>::const_iterator uniformIter = uniformValues.begin();
                 uniformIter != uniformValues.end(); ++uniformIter)
                setUniformValue(ctx, ctx.getUniformLocation(program, uniformIter->getVariable()->getName()),
                                uniformIter->getValue());

            for (vector<RenderCommand>::const_iterator cmdIter = renderCmds.begin(); cmdIter != renderCmds.end();
                 ++cmdIter)
                render(ctx, posPtr, *cmdIter);

            GLU_EXPECT_NO_ERROR(ctx.getError(), "Rendering failed");

            ctx.readPixels(0, 0, m_viewportSize.x(), m_viewportSize.y(), GL_RGBA, GL_UNSIGNED_BYTE,
                           readDst[ndx].getDataPtr());
        }
    }

    {
        const tcu::RGBA threshold =
            m_renderCtx.getRenderTarget().getPixelFormat().getColorThreshold() + tcu::RGBA(3, 3, 3, 3);
        const bool compareOk =
            tcu::bilinearCompare(m_testCtx.getLog(), "CompareResult", "Image comparison result", reference.getAccess(),
                                 rendered.getAccess(), threshold, tcu::COMPARE_LOG_RESULT);

        if (!compareOk)
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
            return STOP;
        }
    }

    m_iterNdx += 1;
    return (m_iterNdx < NUM_ITERATIONS) ? CONTINUE : STOP;
}

} // namespace gls
} // namespace deqp
