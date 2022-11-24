//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// GLES1Renderer.cpp: Implements the GLES1Renderer renderer.

#include "libANGLE/GLES1Renderer.h"

#include <string.h>
#include <iterator>
#include <sstream>
#include <vector>

#include "common/hash_utils.h"
#include "libANGLE/Context.h"
#include "libANGLE/Context.inl.h"
#include "libANGLE/Program.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/Shader.h"
#include "libANGLE/State.h"
#include "libANGLE/renderer/ContextImpl.h"

namespace
{
#include "libANGLE/GLES1Shaders.inc"

uint32_t GetLogicOpUniform(const gl::FramebufferAttachment *color, gl::LogicalOperation logicOp)
{
    const uint32_t red   = color->getRedSize();
    const uint32_t green = color->getGreenSize();
    const uint32_t blue  = color->getBlueSize();
    const uint32_t alpha = color->getAlphaSize();

    ASSERT(red <= 8 && green <= 8 && blue <= 8 && alpha <= 8);

    return red | green << 4 | blue << 8 | alpha << 12 | static_cast<uint32_t>(logicOp) << 16;
}
}  // anonymous namespace

namespace gl
{
GLES1ShaderState::GLES1ShaderState()  = default;
GLES1ShaderState::~GLES1ShaderState() = default;
GLES1ShaderState::GLES1ShaderState(const GLES1ShaderState &other)
{
    memcpy(this, &other, sizeof(GLES1ShaderState));
}

bool operator==(const GLES1ShaderState &a, const GLES1ShaderState &b)
{
    return memcmp(&a, &b, sizeof(GLES1ShaderState)) == 0;
}
bool operator!=(const GLES1ShaderState &a, const GLES1ShaderState &b)
{
    return !(a == b);
}

size_t GLES1ShaderState::hash() const
{
    return angle::ComputeGenericHash(*this);
}

GLES1Renderer::GLES1Renderer() : mRendererProgramInitialized(false) {}

void GLES1Renderer::onDestroy(Context *context, State *state)
{
    if (mRendererProgramInitialized)
    {
        (void)state->setProgram(context, 0);

        for (const auto &iter : mUberShaderState)
        {
            const GLES1UberShaderState &UberShaderState = iter.second;
            mShaderPrograms->deleteProgram(context, {UberShaderState.programState.program});
        }
        mShaderPrograms->release(context);
        mShaderPrograms             = nullptr;
        mRendererProgramInitialized = false;
    }
}

GLES1Renderer::~GLES1Renderer() = default;

angle::Result GLES1Renderer::prepareForDraw(PrimitiveMode mode, Context *context, State *glState)
{
    GLES1State &gles1State = glState->gles1();

    GLES1ShaderState::BoolTexArray &tex2DEnables   = mShaderState.tex2DEnables;
    GLES1ShaderState::BoolTexArray &texCubeEnables = mShaderState.texCubeEnables;
    GLES1ShaderState::IntTexArray &tex2DFormats    = mShaderState.tex2DFormats;

    for (int i = 0; i < kTexUnitCount; i++)
    {
        // GL_OES_cube_map allows only one of TEXTURE_2D / TEXTURE_CUBE_MAP
        // to be enabled per unit, thankfully. From the extension text:
        //
        //  --  Section 3.8.10 "Texture Application"
        //
        //      Replace the beginning sentences of the first paragraph (page 138)
        //      with:
        //
        //      "Texturing is enabled or disabled using the generic Enable
        //      and Disable commands, respectively, with the symbolic constants
        //      TEXTURE_2D or TEXTURE_CUBE_MAP_OES to enable the two-dimensional or cube
        //      map texturing respectively.  If the cube map texture and the two-
        //      dimensional texture are enabled, then cube map texturing is used.  If
        //      texturing is disabled, a rasterized fragment is passed on unaltered to the
        //      next stage of the GL (although its texture coordinates may be discarded).
        //      Otherwise, a texture value is found according to the parameter values of
        //      the currently bound texture image of the appropriate dimensionality.

        texCubeEnables[i] = gles1State.isTextureTargetEnabled(i, TextureType::CubeMap);
        tex2DEnables[i] =
            !texCubeEnables[i] && gles1State.isTextureTargetEnabled(i, TextureType::_2D);

        Texture *curr2DTexture = glState->getSamplerTexture(i, TextureType::_2D);
        if (curr2DTexture)
        {
            tex2DFormats[i] = gl::GetUnsizedFormat(
                curr2DTexture->getFormat(TextureTarget::_2D, 0).info->internalFormat);

            // Handle GL_BGRA the same way we do GL_RGBA
            if (tex2DFormats[i] == GL_BGRA_EXT)
                tex2DFormats[i] = GL_RGBA;
        }

        Texture *currCubeTexture = glState->getSamplerTexture(i, TextureType::CubeMap);

        // > If texturing is enabled for a texture unit at the time a primitive is rasterized, if
        // > TEXTURE MIN FILTER is one that requires a mipmap, and if the texture image bound to the
        // > enabled texture target is not complete, then it is as if texture mapping were disabled
        // > for that texture unit.
        if (tex2DEnables[i] && curr2DTexture && IsMipmapFiltered(curr2DTexture->getMinFilter()))
        {
            tex2DEnables[i] = curr2DTexture->isMipmapComplete();
        }
        if (texCubeEnables[i] && currCubeTexture &&
            IsMipmapFiltered(currCubeTexture->getMinFilter()))
        {
            texCubeEnables[i] = curr2DTexture->isMipmapComplete();
        }
    }

    GLES1ShaderState::IntTexArray &texEnvModes          = mShaderState.texEnvModes;
    GLES1ShaderState::IntTexArray &texCombineRgbs       = mShaderState.texCombineRgbs;
    GLES1ShaderState::IntTexArray &texCombineAlphas     = mShaderState.texCombineAlphas;
    GLES1ShaderState::IntTexArray &texCombineSrc0Rgbs   = mShaderState.texCombineSrc0Rgbs;
    GLES1ShaderState::IntTexArray &texCombineSrc0Alphas = mShaderState.texCombineSrc0Alphas;
    GLES1ShaderState::IntTexArray &texCombineSrc1Rgbs   = mShaderState.texCombineSrc1Rgbs;
    GLES1ShaderState::IntTexArray &texCombineSrc1Alphas = mShaderState.texCombineSrc1Alphas;
    GLES1ShaderState::IntTexArray &texCombineSrc2Rgbs   = mShaderState.texCombineSrc2Rgbs;
    GLES1ShaderState::IntTexArray &texCombineSrc2Alphas = mShaderState.texCombineSrc2Alphas;
    GLES1ShaderState::IntTexArray &texCombineOp0Rgbs    = mShaderState.texCombineOp0Rgbs;
    GLES1ShaderState::IntTexArray &texCombineOp0Alphas  = mShaderState.texCombineOp0Alphas;
    GLES1ShaderState::IntTexArray &texCombineOp1Rgbs    = mShaderState.texCombineOp1Rgbs;
    GLES1ShaderState::IntTexArray &texCombineOp1Alphas  = mShaderState.texCombineOp1Alphas;
    GLES1ShaderState::IntTexArray &texCombineOp2Rgbs    = mShaderState.texCombineOp2Rgbs;
    GLES1ShaderState::IntTexArray &texCombineOp2Alphas  = mShaderState.texCombineOp2Alphas;

    if (gles1State.isDirty(GLES1State::DIRTY_GLES1_TEXTURE_ENVIRONMENT))
    {
        for (int i = 0; i < kTexUnitCount; i++)
        {
            const auto &env         = gles1State.textureEnvironment(i);
            texEnvModes[i]          = ToGLenum(env.mode);
            texCombineRgbs[i]       = ToGLenum(env.combineRgb);
            texCombineAlphas[i]     = ToGLenum(env.combineAlpha);
            texCombineSrc0Rgbs[i]   = ToGLenum(env.src0Rgb);
            texCombineSrc0Alphas[i] = ToGLenum(env.src0Alpha);
            texCombineSrc1Rgbs[i]   = ToGLenum(env.src1Rgb);
            texCombineSrc1Alphas[i] = ToGLenum(env.src1Alpha);
            texCombineSrc2Rgbs[i]   = ToGLenum(env.src2Rgb);
            texCombineSrc2Alphas[i] = ToGLenum(env.src2Alpha);
            texCombineOp0Rgbs[i]    = ToGLenum(env.op0Rgb);
            texCombineOp0Alphas[i]  = ToGLenum(env.op0Alpha);
            texCombineOp1Rgbs[i]    = ToGLenum(env.op1Rgb);
            texCombineOp1Alphas[i]  = ToGLenum(env.op1Alpha);
            texCombineOp2Rgbs[i]    = ToGLenum(env.op2Rgb);
            texCombineOp2Alphas[i]  = ToGLenum(env.op2Alpha);
        }
    }

    bool enableClipPlanes                                  = false;
    GLES1ShaderState::BoolClipPlaneArray &clipPlaneEnables = mShaderState.clipPlaneEnables;
    for (int i = 0; i < kClipPlaneCount; i++)
    {
        clipPlaneEnables[i] = glState->getEnableFeature(GL_CLIP_PLANE0 + i);
        enableClipPlanes    = enableClipPlanes || clipPlaneEnables[i];
    }

    mShaderState.mGLES1StateEnabled[GLES1StateEnables::ClipPlanes]  = enableClipPlanes;
    mShaderState.mGLES1StateEnabled[GLES1StateEnables::DrawTexture] = mDrawTextureEnabled;
    mShaderState.mGLES1StateEnabled[GLES1StateEnables::PointRasterization] =
        mode == PrimitiveMode::Points;
    mShaderState.mGLES1StateEnabled[GLES1StateEnables::ShadeModelFlat] =
        gles1State.mShadeModel == ShadingModel::Flat;
    mShaderState.mGLES1StateEnabled[GLES1StateEnables::AlphaTest] =
        glState->getEnableFeature(GL_ALPHA_TEST);
    mShaderState.mGLES1StateEnabled[GLES1StateEnables::Lighting] =
        glState->getEnableFeature(GL_LIGHTING);
    mShaderState.mGLES1StateEnabled[GLES1StateEnables::RescaleNormal] =
        glState->getEnableFeature(GL_RESCALE_NORMAL);
    mShaderState.mGLES1StateEnabled[GLES1StateEnables::Normalize] =
        glState->getEnableFeature(GL_NORMALIZE);
    mShaderState.mGLES1StateEnabled[GLES1StateEnables::Fog] = glState->getEnableFeature(GL_FOG);
    mShaderState.mGLES1StateEnabled[GLES1StateEnables::PointSprite] =
        glState->getEnableFeature(GL_POINT_SPRITE_OES);
    mShaderState.mGLES1StateEnabled[GLES1StateEnables::ColorMaterial] =
        glState->getEnableFeature(GL_COLOR_MATERIAL);

    // TODO (lfy@google.com): Implement two-sided lighting model (lightModel.twoSided)
    mShaderState.mGLES1StateEnabled[GLES1StateEnables::LightModelTwoSided] = false;

    GLES1ShaderState::BoolTexArray &pointSpriteCoordReplaces =
        mShaderState.pointSpriteCoordReplaces;
    for (int i = 0; i < kTexUnitCount; i++)
    {
        const auto &env             = gles1State.textureEnvironment(i);
        pointSpriteCoordReplaces[i] = env.pointSpriteCoordReplace;
    }

    GLES1ShaderState::BoolLightArray &lightEnables = mShaderState.lightEnables;
    for (int i = 0; i < kLightCount; i++)
    {
        const auto &light = gles1State.mLights[i];
        lightEnables[i]   = light.enabled;
    }

    mShaderState.alphaTestFunc = gles1State.mAlphaTestFunc;
    mShaderState.fogMode       = gles1State.fogParameters().mode;

    const bool hasLogicOpANGLE     = context->getExtensions().logicOpANGLE;
    const bool hasFramebufferFetch = context->getExtensions().shaderFramebufferFetchEXT ||
                                     context->getExtensions().shaderFramebufferFetchNonCoherentEXT;

    if (!hasLogicOpANGLE && hasFramebufferFetch)
    {
        mShaderState.mGLES1StateEnabled[GLES1StateEnables::LogicOpThroughFramebufferFetch] =
            gles1State.mLogicOpEnabled;
    }

    // All the states set before this spot affect ubershader creation

    ANGLE_TRY(initializeRendererProgram(context, glState));

    GLES1UberShaderState UberShaderState = getUberShaderState();

    const GLES1ProgramState &programState = UberShaderState.programState;
    GLES1UniformBuffers &uniformBuffers   = UberShaderState.uniformBuffers;

    Program *programObject = getProgram(programState.program);

    // If anything is dirty in gles1 or the common parts of gles1/2, just redo these parts
    // completely for now.

    // Feature enables

    // Texture unit enables and format info
    std::array<Vec4Uniform, kTexUnitCount> texCropRects;
    Vec4Uniform *cropRectBuffer = texCropRects.data();
    for (int i = 0; i < kTexUnitCount; i++)
    {
        Texture *curr2DTexture = glState->getSamplerTexture(i, TextureType::_2D);
        if (curr2DTexture)
        {
            const gl::Rectangle &cropRect = curr2DTexture->getCrop();

            GLfloat textureWidth =
                static_cast<GLfloat>(curr2DTexture->getWidth(TextureTarget::_2D, 0));
            GLfloat textureHeight =
                static_cast<GLfloat>(curr2DTexture->getHeight(TextureTarget::_2D, 0));

            if (textureWidth > 0.0f && textureHeight > 0.0f)
            {
                cropRectBuffer[i][0] = cropRect.x / textureWidth;
                cropRectBuffer[i][1] = cropRect.y / textureHeight;
                cropRectBuffer[i][2] = cropRect.width / textureWidth;
                cropRectBuffer[i][3] = cropRect.height / textureHeight;
            }
        }
    }
    setUniform4fv(programObject, programState.drawTextureNormalizedCropRectLoc, kTexUnitCount,
                  reinterpret_cast<GLfloat *>(cropRectBuffer));

    if (gles1State.isDirty(GLES1State::DIRTY_GLES1_LOGIC_OP) && hasLogicOpANGLE)
    {
        context->setLogicOpEnabled(gles1State.mLogicOpEnabled);
        context->setLogicOp(gles1State.mLogicOp);
    }
    else if (hasFramebufferFetch)
    {
        const Framebuffer *drawFramebuffer           = glState->getDrawFramebuffer();
        const FramebufferAttachment *colorAttachment = drawFramebuffer->getColorAttachment(0);

        if (gles1State.mLogicOpEnabled)
        {
            if (gles1State.isDirty(GLES1State::DIRTY_GLES1_LOGIC_OP))
            {
                // Set up uniform value for logic op
                setUniform1ui(programObject, programState.logicOpLoc,
                              GetLogicOpUniform(colorAttachment, gles1State.mLogicOp));
            }

            // Issue a framebuffer fetch barrier if non-coherent
            if (!context->getExtensions().shaderFramebufferFetchEXT)
            {
                context->framebufferFetchBarrier();
            }
        }
    }

    // Client state / current vector enables
    if (gles1State.isDirty(GLES1State::DIRTY_GLES1_CLIENT_STATE_ENABLE) ||
        gles1State.isDirty(GLES1State::DIRTY_GLES1_CURRENT_VECTOR))
    {
        if (!gles1State.isClientStateEnabled(ClientVertexArrayType::Normal))
        {
            const angle::Vector3 normal = gles1State.getCurrentNormal();
            context->vertexAttrib3f(kNormalAttribIndex, normal.x(), normal.y(), normal.z());
        }

        if (!gles1State.isClientStateEnabled(ClientVertexArrayType::Color))
        {
            const ColorF color = gles1State.getCurrentColor();
            context->vertexAttrib4f(kColorAttribIndex, color.red, color.green, color.blue,
                                    color.alpha);
        }

        if (!gles1State.isClientStateEnabled(ClientVertexArrayType::PointSize))
        {
            GLfloat pointSize = gles1State.mPointParameters.pointSize;
            context->vertexAttrib1f(kPointSizeAttribIndex, pointSize);
        }

        for (int i = 0; i < kTexUnitCount; i++)
        {
            if (!gles1State.mTexCoordArrayEnabled[i])
            {
                const TextureCoordF texcoord = gles1State.getCurrentTextureCoords(i);
                context->vertexAttrib4f(kTextureCoordAttribIndexBase + i, texcoord.s, texcoord.t,
                                        texcoord.r, texcoord.q);
            }
        }
    }

    // Matrices
    if (gles1State.isDirty(GLES1State::DIRTY_GLES1_MATRICES))
    {
        angle::Mat4 proj = gles1State.mProjectionMatrices.back();
        setUniformMatrix4fv(programObject, programState.projMatrixLoc, 1, GL_FALSE, proj.data());

        angle::Mat4 modelview = gles1State.mModelviewMatrices.back();
        setUniformMatrix4fv(programObject, programState.modelviewMatrixLoc, 1, GL_FALSE,
                            modelview.data());

        angle::Mat4 modelviewInvTr = modelview.transpose().inverse();
        setUniformMatrix4fv(programObject, programState.modelviewInvTrLoc, 1, GL_FALSE,
                            modelviewInvTr.data());

        Mat4Uniform *textureMatrixBuffer = uniformBuffers.textureMatrices.data();

        for (int i = 0; i < kTexUnitCount; i++)
        {
            angle::Mat4 textureMatrix = gles1State.mTextureMatrices[i].back();
            memcpy(textureMatrixBuffer + i, textureMatrix.data(), sizeof(Mat4Uniform));
        }

        setUniformMatrix4fv(programObject, programState.textureMatrixLoc, kTexUnitCount, GL_FALSE,
                            reinterpret_cast<float *>(uniformBuffers.textureMatrices.data()));
    }

    if (gles1State.isDirty(GLES1State::DIRTY_GLES1_TEXTURE_ENVIRONMENT))
    {
        for (int i = 0; i < kTexUnitCount; i++)
        {
            const auto &env = gles1State.textureEnvironment(i);

            uniformBuffers.texEnvColors[i][0] = env.color.red;
            uniformBuffers.texEnvColors[i][1] = env.color.green;
            uniformBuffers.texEnvColors[i][2] = env.color.blue;
            uniformBuffers.texEnvColors[i][3] = env.color.alpha;

            uniformBuffers.texEnvRgbScales[i]   = env.rgbScale;
            uniformBuffers.texEnvAlphaScales[i] = env.alphaScale;
        }

        setUniform4fv(programObject, programState.textureEnvColorLoc, kTexUnitCount,
                      reinterpret_cast<float *>(uniformBuffers.texEnvColors.data()));
        setUniform1fv(programObject, programState.rgbScaleLoc, kTexUnitCount,
                      uniformBuffers.texEnvRgbScales.data());
        setUniform1fv(programObject, programState.alphaScaleLoc, kTexUnitCount,
                      uniformBuffers.texEnvAlphaScales.data());
    }

    // Alpha test
    if (gles1State.isDirty(GLES1State::DIRTY_GLES1_ALPHA_TEST))
    {
        setUniform1f(programObject, programState.alphaTestRefLoc, gles1State.mAlphaTestRef);
    }

    // Shading, materials, and lighting
    if (gles1State.isDirty(GLES1State::DIRTY_GLES1_MATERIAL))
    {
        const auto &material = gles1State.mMaterial;

        setUniform4fv(programObject, programState.materialAmbientLoc, 1, material.ambient.data());
        setUniform4fv(programObject, programState.materialDiffuseLoc, 1, material.diffuse.data());
        setUniform4fv(programObject, programState.materialSpecularLoc, 1, material.specular.data());
        setUniform4fv(programObject, programState.materialEmissiveLoc, 1, material.emissive.data());
        setUniform1f(programObject, programState.materialSpecularExponentLoc,
                     material.specularExponent);
    }

    if (gles1State.isDirty(GLES1State::DIRTY_GLES1_LIGHTS))
    {
        const auto &lightModel = gles1State.mLightModel;

        setUniform4fv(programObject, programState.lightModelSceneAmbientLoc, 1,
                      lightModel.color.data());

        for (int i = 0; i < kLightCount; i++)
        {
            const auto &light = gles1State.mLights[i];
            memcpy(uniformBuffers.lightAmbients.data() + i, light.ambient.data(),
                   sizeof(Vec4Uniform));
            memcpy(uniformBuffers.lightDiffuses.data() + i, light.diffuse.data(),
                   sizeof(Vec4Uniform));
            memcpy(uniformBuffers.lightSpeculars.data() + i, light.specular.data(),
                   sizeof(Vec4Uniform));
            memcpy(uniformBuffers.lightPositions.data() + i, light.position.data(),
                   sizeof(Vec4Uniform));
            memcpy(uniformBuffers.lightDirections.data() + i, light.direction.data(),
                   sizeof(Vec3Uniform));
            uniformBuffers.spotlightExponents[i]    = light.spotlightExponent;
            uniformBuffers.spotlightCutoffAngles[i] = light.spotlightCutoffAngle;
            uniformBuffers.attenuationConsts[i]     = light.attenuationConst;
            uniformBuffers.attenuationLinears[i]    = light.attenuationLinear;
            uniformBuffers.attenuationQuadratics[i] = light.attenuationQuadratic;
        }

        setUniform4fv(programObject, programState.lightAmbientsLoc, kLightCount,
                      reinterpret_cast<float *>(uniformBuffers.lightAmbients.data()));
        setUniform4fv(programObject, programState.lightDiffusesLoc, kLightCount,
                      reinterpret_cast<float *>(uniformBuffers.lightDiffuses.data()));
        setUniform4fv(programObject, programState.lightSpecularsLoc, kLightCount,
                      reinterpret_cast<float *>(uniformBuffers.lightSpeculars.data()));
        setUniform4fv(programObject, programState.lightPositionsLoc, kLightCount,
                      reinterpret_cast<float *>(uniformBuffers.lightPositions.data()));
        setUniform3fv(programObject, programState.lightDirectionsLoc, kLightCount,
                      reinterpret_cast<float *>(uniformBuffers.lightDirections.data()));
        setUniform1fv(programObject, programState.lightSpotlightExponentsLoc, kLightCount,
                      reinterpret_cast<float *>(uniformBuffers.spotlightExponents.data()));
        setUniform1fv(programObject, programState.lightSpotlightCutoffAnglesLoc, kLightCount,
                      reinterpret_cast<float *>(uniformBuffers.spotlightCutoffAngles.data()));
        setUniform1fv(programObject, programState.lightAttenuationConstsLoc, kLightCount,
                      reinterpret_cast<float *>(uniformBuffers.attenuationConsts.data()));
        setUniform1fv(programObject, programState.lightAttenuationLinearsLoc, kLightCount,
                      reinterpret_cast<float *>(uniformBuffers.attenuationLinears.data()));
        setUniform1fv(programObject, programState.lightAttenuationQuadraticsLoc, kLightCount,
                      reinterpret_cast<float *>(uniformBuffers.attenuationQuadratics.data()));
    }

    if (gles1State.isDirty(GLES1State::DIRTY_GLES1_FOG))
    {
        const FogParameters &fog = gles1State.fogParameters();
        setUniform1f(programObject, programState.fogDensityLoc, fog.density);
        setUniform1f(programObject, programState.fogStartLoc, fog.start);
        setUniform1f(programObject, programState.fogEndLoc, fog.end);
        setUniform4fv(programObject, programState.fogColorLoc, 1, fog.color.data());
    }

    // Clip planes
    if (gles1State.isDirty(GLES1State::DIRTY_GLES1_CLIP_PLANES))
    {
        for (int i = 0; i < kClipPlaneCount; i++)
        {
            gles1State.getClipPlane(
                i, reinterpret_cast<float *>(uniformBuffers.clipPlanes.data() + i));
        }

        setUniform4fv(programObject, programState.clipPlanesLoc, kClipPlaneCount,
                      reinterpret_cast<float *>(uniformBuffers.clipPlanes.data()));
    }

    // Point rasterization
    {
        const PointParameters &pointParams = gles1State.mPointParameters;

        setUniform1f(programObject, programState.pointSizeMinLoc, pointParams.pointSizeMin);
        setUniform1f(programObject, programState.pointSizeMaxLoc, pointParams.pointSizeMax);
        setUniform3fv(programObject, programState.pointDistanceAttenuationLoc, 1,
                      pointParams.pointDistanceAttenuation.data());
    }

    // Draw texture
    {
        setUniform4fv(programObject, programState.drawTextureCoordsLoc, 1, mDrawTextureCoords);
        setUniform2fv(programObject, programState.drawTextureDimsLoc, 1, mDrawTextureDims);
    }

    gles1State.clearDirty();

    // None of those are changes in sampler, so there is no need to set the GL_PROGRAM dirty.
    // Otherwise, put the dirtying here.

    return angle::Result::Continue;
}

// static
int GLES1Renderer::VertexArrayIndex(ClientVertexArrayType type, const GLES1State &gles1)
{
    switch (type)
    {
        case ClientVertexArrayType::Vertex:
            return kVertexAttribIndex;
        case ClientVertexArrayType::Normal:
            return kNormalAttribIndex;
        case ClientVertexArrayType::Color:
            return kColorAttribIndex;
        case ClientVertexArrayType::PointSize:
            return kPointSizeAttribIndex;
        case ClientVertexArrayType::TextureCoord:
            return kTextureCoordAttribIndexBase + gles1.getClientTextureUnit();
        default:
            UNREACHABLE();
            return 0;
    }
}

// static
ClientVertexArrayType GLES1Renderer::VertexArrayType(int attribIndex)
{
    switch (attribIndex)
    {
        case kVertexAttribIndex:
            return ClientVertexArrayType::Vertex;
        case kNormalAttribIndex:
            return ClientVertexArrayType::Normal;
        case kColorAttribIndex:
            return ClientVertexArrayType::Color;
        case kPointSizeAttribIndex:
            return ClientVertexArrayType::PointSize;
        default:
            if (attribIndex < kTextureCoordAttribIndexBase + kTexUnitCount)
            {
                return ClientVertexArrayType::TextureCoord;
            }
            UNREACHABLE();
            return ClientVertexArrayType::InvalidEnum;
    }
}

// static
int GLES1Renderer::TexCoordArrayIndex(unsigned int unit)
{
    return kTextureCoordAttribIndexBase + unit;
}

void GLES1Renderer::drawTexture(Context *context,
                                State *glState,
                                float x,
                                float y,
                                float z,
                                float width,
                                float height)
{

    // get viewport
    const gl::Rectangle &viewport = glState->getViewport();

    // Translate from viewport to NDC for feeding the shader.
    // Recenter, rescale. (e.g., [0, 0, 1080, 1920] -> [-1, -1, 1, 1])
    float xNdc = scaleScreenCoordinateToNdc(x, static_cast<GLfloat>(viewport.width));
    float yNdc = scaleScreenCoordinateToNdc(y, static_cast<GLfloat>(viewport.height));
    float wNdc = scaleScreenDimensionToNdc(width, static_cast<GLfloat>(viewport.width));
    float hNdc = scaleScreenDimensionToNdc(height, static_cast<GLfloat>(viewport.height));

    float zNdc = 2.0f * clamp(z, 0.0f, 1.0f) - 1.0f;

    mDrawTextureCoords[0] = xNdc;
    mDrawTextureCoords[1] = yNdc;
    mDrawTextureCoords[2] = zNdc;

    mDrawTextureDims[0] = wNdc;
    mDrawTextureDims[1] = hNdc;

    mDrawTextureEnabled = true;

    AttributesMask prevAttributesMask = glState->gles1().getVertexArraysAttributeMask();

    setAttributesEnabled(context, glState, AttributesMask());

    glState->gles1().setAllDirty();

    context->drawArrays(PrimitiveMode::Triangles, 0, 6);

    setAttributesEnabled(context, glState, prevAttributesMask);

    mDrawTextureEnabled = false;
}

Shader *GLES1Renderer::getShader(ShaderProgramID handle) const
{
    return mShaderPrograms->getShader(handle);
}

Program *GLES1Renderer::getProgram(ShaderProgramID handle) const
{
    return mShaderPrograms->getProgram(handle);
}

angle::Result GLES1Renderer::compileShader(Context *context,
                                           ShaderType shaderType,
                                           const char *src,
                                           ShaderProgramID *shaderOut)
{
    rx::ContextImpl *implementation = context->getImplementation();
    const Limitations &limitations  = implementation->getNativeLimitations();

    ShaderProgramID shader = mShaderPrograms->createShader(implementation, limitations, shaderType);

    Shader *shaderObject = getShader(shader);
    ANGLE_CHECK(context, shaderObject, "Missing shader object", GL_INVALID_OPERATION);

    shaderObject->setSource(1, &src, nullptr);
    shaderObject->compile(context);

    *shaderOut = shader;

    if (!shaderObject->isCompiled(context))
    {
        GLint infoLogLength = shaderObject->getInfoLogLength(context);
        std::vector<char> infoLog(infoLogLength, 0);
        shaderObject->getInfoLog(context, infoLogLength - 1, nullptr, infoLog.data());

        ERR() << "Internal GLES 1 shader compile failed. Info log: " << infoLog.data();
        ANGLE_CHECK(context, false, "GLES1Renderer shader compile failed.", GL_INVALID_OPERATION);
        return angle::Result::Stop;
    }

    return angle::Result::Continue;
}

angle::Result GLES1Renderer::linkProgram(Context *context,
                                         State *glState,
                                         ShaderProgramID vertexShader,
                                         ShaderProgramID fragmentShader,
                                         const angle::HashMap<GLint, std::string> &attribLocs,
                                         ShaderProgramID *programOut)
{
    ShaderProgramID program = mShaderPrograms->createProgram(context->getImplementation());

    Program *programObject = getProgram(program);
    ANGLE_CHECK(context, programObject, "Missing program object", GL_INVALID_OPERATION);

    *programOut = program;

    programObject->attachShader(getShader(vertexShader));
    programObject->attachShader(getShader(fragmentShader));

    for (auto it : attribLocs)
    {
        GLint index             = it.first;
        const std::string &name = it.second;
        programObject->bindAttributeLocation(index, name.c_str());
    }

    ANGLE_TRY(programObject->link(context));
    programObject->resolveLink(context);

    ANGLE_TRY(glState->onProgramExecutableChange(context, programObject));

    if (!programObject->isLinked())
    {
        GLint infoLogLength = programObject->getExecutable().getInfoLogLength();
        std::vector<char> infoLog(infoLogLength, 0);
        programObject->getExecutable().getInfoLog(infoLogLength - 1, nullptr, infoLog.data());

        ERR() << "Internal GLES 1 shader link failed. Info log: " << infoLog.data();
        ANGLE_CHECK(context, false, "GLES1Renderer program link failed.", GL_INVALID_OPERATION);
        return angle::Result::Stop;
    }

    programObject->detachShader(context, getShader(vertexShader));
    programObject->detachShader(context, getShader(fragmentShader));

    return angle::Result::Continue;
}

const char *GLES1Renderer::getShaderBool(GLES1StateEnables state)
{
    if (mShaderState.mGLES1StateEnabled[state])
    {
        return "true";
    }
    else
    {
        return "false";
    }
}

void GLES1Renderer::addShaderDefine(std::stringstream &outStream,
                                    GLES1StateEnables state,
                                    const char *enableString)
{
    outStream << "\n";
    outStream << "#define " << enableString << " " << getShaderBool(state);
}

void GLES1Renderer::addShaderInt(std::stringstream &outStream, const char *name, int value)
{
    outStream << "\n";
    outStream << "const int " << name << " = " << value << ";";
}

void GLES1Renderer::addShaderIntTexArray(std::stringstream &outStream,
                                         const char *texString,
                                         GLES1ShaderState::IntTexArray &texState)
{
    outStream << "\n";
    outStream << "const int " << texString << "[kMaxTexUnits] = int[kMaxTexUnits](";
    for (int i = 0; i < kTexUnitCount; i++)
    {
        if (i != 0)
        {
            outStream << ", ";
        }
        outStream << texState[i];
    }
    outStream << ");";
}

void GLES1Renderer::addShaderBoolTexArray(std::stringstream &outStream,
                                          const char *name,
                                          GLES1ShaderState::BoolTexArray &value)
{
    outStream << std::boolalpha;
    outStream << "\n";
    outStream << "bool " << name << "[kMaxTexUnits] = bool[kMaxTexUnits](";
    for (int i = 0; i < kTexUnitCount; i++)
    {
        if (i != 0)
        {
            outStream << ", ";
        }
        outStream << value[i];
    }
    outStream << ");";
}

void GLES1Renderer::addShaderBoolLightArray(std::stringstream &outStream,
                                            const char *name,
                                            GLES1ShaderState::BoolLightArray &value)
{
    outStream << std::boolalpha;
    outStream << "\n";
    outStream << "bool " << name << "[kMaxLights] = bool[kMaxLights](";
    for (int i = 0; i < kLightCount; i++)
    {
        if (i != 0)
        {
            outStream << ", ";
        }
        outStream << value[i];
    }
    outStream << ");";
}

void GLES1Renderer::addShaderBoolClipPlaneArray(std::stringstream &outStream,
                                                const char *name,
                                                GLES1ShaderState::BoolClipPlaneArray &value)
{
    outStream << std::boolalpha;
    outStream << "\n";
    outStream << "bool " << name << "[kMaxClipPlanes] = bool[kMaxClipPlanes](";
    for (int i = 0; i < kClipPlaneCount; i++)
    {
        if (i != 0)
        {
            outStream << ", ";
        }
        outStream << value[i];
    }
    outStream << ");";
}

void GLES1Renderer::addVertexShaderDefs(std::stringstream &outStream)
{
    addShaderDefine(outStream, GLES1StateEnables::Lighting, "enable_lighting");
    addShaderDefine(outStream, GLES1StateEnables::ColorMaterial, "enable_color_material");
    addShaderDefine(outStream, GLES1StateEnables::DrawTexture, "enable_draw_texture");
    addShaderDefine(outStream, GLES1StateEnables::PointRasterization, "point_rasterization");
    addShaderDefine(outStream, GLES1StateEnables::RescaleNormal, "enable_rescale_normal");
    addShaderDefine(outStream, GLES1StateEnables::Normalize, "enable_normalize");
    addShaderDefine(outStream, GLES1StateEnables::LightModelTwoSided, "light_model_two_sided");

    // bool light_enables[kMaxLights] = bool[kMaxLights](...);
    addShaderBoolLightArray(outStream, "light_enables", mShaderState.lightEnables);
}

void GLES1Renderer::addFragmentShaderDefs(std::stringstream &outStream)
{
    addShaderDefine(outStream, GLES1StateEnables::Fog, "enable_fog");
    addShaderDefine(outStream, GLES1StateEnables::ClipPlanes, "enable_clip_planes");
    addShaderDefine(outStream, GLES1StateEnables::DrawTexture, "enable_draw_texture");
    addShaderDefine(outStream, GLES1StateEnables::PointRasterization, "point_rasterization");
    addShaderDefine(outStream, GLES1StateEnables::PointSprite, "point_sprite_enabled");
    addShaderDefine(outStream, GLES1StateEnables::AlphaTest, "enable_alpha_test");
    addShaderDefine(outStream, GLES1StateEnables::ShadeModelFlat, "shade_model_flat");

    // bool enable_texture_2d[kMaxTexUnits] = bool[kMaxTexUnits](...);
    addShaderBoolTexArray(outStream, "enable_texture_2d", mShaderState.tex2DEnables);

    // bool enable_texture_cube_map[kMaxTexUnits] = bool[kMaxTexUnits](...);
    addShaderBoolTexArray(outStream, "enable_texture_cube_map", mShaderState.texCubeEnables);

    // int texture_format[kMaxTexUnits] = int[kMaxTexUnits](...);
    addShaderIntTexArray(outStream, "texture_format", mShaderState.tex2DFormats);

    // bool point_sprite_coord_replace[kMaxTexUnits] = bool[kMaxTexUnits](...);
    addShaderBoolTexArray(outStream, "point_sprite_coord_replace",
                          mShaderState.pointSpriteCoordReplaces);

    // bool clip_plane_enables[kMaxClipPlanes] = bool[kMaxClipPlanes](...);
    addShaderBoolClipPlaneArray(outStream, "clip_plane_enables", mShaderState.clipPlaneEnables);

    // int texture_format[kMaxTexUnits] = int[kMaxTexUnits](...);
    addShaderIntTexArray(outStream, "texture_env_mode", mShaderState.texEnvModes);

    // int combine_rgb[kMaxTexUnits];
    addShaderIntTexArray(outStream, "combine_rgb", mShaderState.texCombineRgbs);

    // int combine_alpha[kMaxTexUnits];
    addShaderIntTexArray(outStream, "combine_alpha", mShaderState.texCombineAlphas);

    // int src0_rgb[kMaxTexUnits];
    addShaderIntTexArray(outStream, "src0_rgb", mShaderState.texCombineSrc0Rgbs);

    // int src0_alpha[kMaxTexUnits];
    addShaderIntTexArray(outStream, "src0_alpha", mShaderState.texCombineSrc0Alphas);

    // int src1_rgb[kMaxTexUnits];
    addShaderIntTexArray(outStream, "src1_rgb", mShaderState.texCombineSrc1Rgbs);

    // int src1_alpha[kMaxTexUnits];
    addShaderIntTexArray(outStream, "src1_alpha", mShaderState.texCombineSrc1Alphas);

    // int src2_rgb[kMaxTexUnits];
    addShaderIntTexArray(outStream, "src2_rgb", mShaderState.texCombineSrc2Rgbs);

    // int src2_alpha[kMaxTexUnits];
    addShaderIntTexArray(outStream, "src2_alpha", mShaderState.texCombineSrc2Alphas);

    // int op0_rgb[kMaxTexUnits];
    addShaderIntTexArray(outStream, "op0_rgb", mShaderState.texCombineOp0Rgbs);

    // int op0_alpha[kMaxTexUnits];
    addShaderIntTexArray(outStream, "op0_alpha", mShaderState.texCombineOp0Alphas);

    // int op1_rgb[kMaxTexUnits];
    addShaderIntTexArray(outStream, "op1_rgb", mShaderState.texCombineOp1Rgbs);

    // int op1_alpha[kMaxTexUnits];
    addShaderIntTexArray(outStream, "op1_alpha", mShaderState.texCombineOp1Alphas);

    // int op2_rgb[kMaxTexUnits];
    addShaderIntTexArray(outStream, "op2_rgb", mShaderState.texCombineOp2Rgbs);

    // int op2_alpha[kMaxTexUnits];
    addShaderIntTexArray(outStream, "op2_alpha", mShaderState.texCombineOp2Alphas);

    // int alpha_func;
    addShaderInt(outStream, "alpha_func", ToGLenum(mShaderState.alphaTestFunc));

    // int fog_mode;
    addShaderInt(outStream, "fog_mode", ToGLenum(mShaderState.fogMode));
}

angle::Result GLES1Renderer::initializeRendererProgram(Context *context, State *glState)
{
    // See if we have the shader for this combination of states
    if (mUberShaderState.find(mShaderState) != mUberShaderState.end())
    {
        Program *programObject = getProgram(getUberShaderState().programState.program);

        // If this is different than the current program, we need to sync everything
        // TODO: This could be optimized to only dirty state that differs between the two programs
        if (glState->getProgram()->id() != programObject->id())
        {
            glState->gles1().setAllDirty();
        }

        ANGLE_TRY(glState->setProgram(context, programObject));
        return angle::Result::Continue;
    }

    if (!mRendererProgramInitialized)
    {
        mShaderPrograms = new ShaderProgramManager();
    }

    // If we get here, we don't have a shader for this state, need to create it
    GLES1ProgramState &programState = mUberShaderState[mShaderState].programState;

    ShaderProgramID vertexShader;
    ShaderProgramID fragmentShader;

    std::stringstream GLES1DrawVShaderStateDefs;
    addVertexShaderDefs(GLES1DrawVShaderStateDefs);

    std::stringstream vertexStream;
    vertexStream << kGLES1DrawVShaderHeader;
    vertexStream << GLES1DrawVShaderStateDefs.str();
    vertexStream << kGLES1DrawVShader;

    ANGLE_TRY(
        compileShader(context, ShaderType::Vertex, vertexStream.str().c_str(), &vertexShader));

    std::stringstream GLES1DrawFShaderStateDefs;
    addFragmentShaderDefs(GLES1DrawFShaderStateDefs);

    std::stringstream fragmentStream;
    fragmentStream << kGLES1DrawFShaderVersion;
    if (mShaderState.mGLES1StateEnabled[GLES1StateEnables::LogicOpThroughFramebufferFetch])
    {
        if (context->getExtensions().shaderFramebufferFetchEXT)
        {
            fragmentStream << "#extension GL_EXT_shader_framebuffer_fetch : require\n";
        }
        else
        {
            fragmentStream << "#extension GL_EXT_shader_framebuffer_fetch_non_coherent : require\n";
        }
    }
    fragmentStream << kGLES1DrawFShaderHeader;
    fragmentStream << GLES1DrawFShaderStateDefs.str();
    fragmentStream << kGLES1DrawFShaderUniformDefs;
    if (mShaderState.mGLES1StateEnabled[GLES1StateEnables::LogicOpThroughFramebufferFetch])
    {
        if (context->getExtensions().shaderFramebufferFetchEXT)
        {
            fragmentStream << kGLES1DrawFShaderFramebufferFetchOutputDef;
        }
        else
        {
            fragmentStream << kGLES1DrawFShaderFramebufferFetchNonCoherentOutputDef;
        }
        fragmentStream << kGLES1DrawFShaderLogicOpFramebufferFetchEnabled;
    }
    else
    {
        fragmentStream << kGLES1DrawFShaderOutputDef;
        fragmentStream << kGLES1DrawFShaderLogicOpFramebufferFetchDisabled;
    }
    fragmentStream << kGLES1DrawFShaderFunctions;
    fragmentStream << kGLES1DrawFShaderMultitexturing;
    fragmentStream << kGLES1DrawFShaderMain;

    ANGLE_TRY(compileShader(context, ShaderType::Fragment, fragmentStream.str().c_str(),
                            &fragmentShader));

    angle::HashMap<GLint, std::string> attribLocs;

    attribLocs[(GLint)kVertexAttribIndex]    = "pos";
    attribLocs[(GLint)kNormalAttribIndex]    = "normal";
    attribLocs[(GLint)kColorAttribIndex]     = "color";
    attribLocs[(GLint)kPointSizeAttribIndex] = "pointsize";

    for (int i = 0; i < kTexUnitCount; i++)
    {
        std::stringstream ss;
        ss << "texcoord" << i;
        attribLocs[kTextureCoordAttribIndexBase + i] = ss.str();
    }

    ANGLE_TRY(linkProgram(context, glState, vertexShader, fragmentShader, attribLocs,
                          &programState.program));

    mShaderPrograms->deleteShader(context, vertexShader);
    mShaderPrograms->deleteShader(context, fragmentShader);

    Program *programObject = getProgram(programState.program);

    programState.projMatrixLoc      = programObject->getUniformLocation("projection");
    programState.modelviewMatrixLoc = programObject->getUniformLocation("modelview");
    programState.textureMatrixLoc   = programObject->getUniformLocation("texture_matrix");
    programState.modelviewInvTrLoc  = programObject->getUniformLocation("modelview_invtr");

    for (int i = 0; i < kTexUnitCount; i++)
    {
        std::stringstream ss2d;
        std::stringstream sscube;

        ss2d << "tex_sampler" << i;
        sscube << "tex_cube_sampler" << i;

        programState.tex2DSamplerLocs[i] = programObject->getUniformLocation(ss2d.str().c_str());
        programState.texCubeSamplerLocs[i] =
            programObject->getUniformLocation(sscube.str().c_str());
    }

    programState.textureEnvColorLoc = programObject->getUniformLocation("texture_env_color");
    programState.rgbScaleLoc        = programObject->getUniformLocation("texture_env_rgb_scale");
    programState.alphaScaleLoc      = programObject->getUniformLocation("texture_env_alpha_scale");

    programState.alphaTestRefLoc = programObject->getUniformLocation("alpha_test_ref");

    programState.materialAmbientLoc  = programObject->getUniformLocation("material_ambient");
    programState.materialDiffuseLoc  = programObject->getUniformLocation("material_diffuse");
    programState.materialSpecularLoc = programObject->getUniformLocation("material_specular");
    programState.materialEmissiveLoc = programObject->getUniformLocation("material_emissive");
    programState.materialSpecularExponentLoc =
        programObject->getUniformLocation("material_specular_exponent");

    programState.lightModelSceneAmbientLoc =
        programObject->getUniformLocation("light_model_scene_ambient");

    programState.lightAmbientsLoc   = programObject->getUniformLocation("light_ambients");
    programState.lightDiffusesLoc   = programObject->getUniformLocation("light_diffuses");
    programState.lightSpecularsLoc  = programObject->getUniformLocation("light_speculars");
    programState.lightPositionsLoc  = programObject->getUniformLocation("light_positions");
    programState.lightDirectionsLoc = programObject->getUniformLocation("light_directions");
    programState.lightSpotlightExponentsLoc =
        programObject->getUniformLocation("light_spotlight_exponents");
    programState.lightSpotlightCutoffAnglesLoc =
        programObject->getUniformLocation("light_spotlight_cutoff_angles");
    programState.lightAttenuationConstsLoc =
        programObject->getUniformLocation("light_attenuation_consts");
    programState.lightAttenuationLinearsLoc =
        programObject->getUniformLocation("light_attenuation_linears");
    programState.lightAttenuationQuadraticsLoc =
        programObject->getUniformLocation("light_attenuation_quadratics");

    programState.fogDensityLoc = programObject->getUniformLocation("fog_density");
    programState.fogStartLoc   = programObject->getUniformLocation("fog_start");
    programState.fogEndLoc     = programObject->getUniformLocation("fog_end");
    programState.fogColorLoc   = programObject->getUniformLocation("fog_color");

    programState.clipPlanesLoc = programObject->getUniformLocation("clip_planes");

    programState.logicOpLoc = programObject->getUniformLocation("logic_op");

    programState.pointSizeMinLoc = programObject->getUniformLocation("point_size_min");
    programState.pointSizeMaxLoc = programObject->getUniformLocation("point_size_max");
    programState.pointDistanceAttenuationLoc =
        programObject->getUniformLocation("point_distance_attenuation");

    programState.drawTextureCoordsLoc = programObject->getUniformLocation("draw_texture_coords");
    programState.drawTextureDimsLoc   = programObject->getUniformLocation("draw_texture_dims");
    programState.drawTextureNormalizedCropRectLoc =
        programObject->getUniformLocation("draw_texture_normalized_crop_rect");

    ANGLE_TRY(glState->setProgram(context, programObject));

    for (int i = 0; i < kTexUnitCount; i++)
    {
        setUniform1i(context, programObject, programState.tex2DSamplerLocs[i], i);
        setUniform1i(context, programObject, programState.texCubeSamplerLocs[i], i + kTexUnitCount);
    }
    glState->setObjectDirty(GL_PROGRAM);

    // We just created a new program, we need to sync everything
    glState->gles1().setAllDirty();

    mRendererProgramInitialized = true;
    return angle::Result::Continue;
}

void GLES1Renderer::setUniform1i(Context *context,
                                 Program *programObject,
                                 UniformLocation location,
                                 GLint value)
{
    if (location.value == -1)
        return;
    programObject->setUniform1iv(context, location, 1, &value);
}

void GLES1Renderer::setUniform1ui(Program *programObject, UniformLocation location, GLuint value)
{
    if (location.value == -1)
        return;
    programObject->setUniform1uiv(location, 1, &value);
}

void GLES1Renderer::setUniform1iv(Context *context,
                                  Program *programObject,
                                  UniformLocation location,
                                  GLint count,
                                  const GLint *value)
{
    if (location.value == -1)
        return;
    programObject->setUniform1iv(context, location, count, value);
}

void GLES1Renderer::setUniformMatrix4fv(Program *programObject,
                                        UniformLocation location,
                                        GLint count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    if (location.value == -1)
        return;
    programObject->setUniformMatrix4fv(location, count, transpose, value);
}

void GLES1Renderer::setUniform4fv(Program *programObject,
                                  UniformLocation location,
                                  GLint count,
                                  const GLfloat *value)
{
    if (location.value == -1)
        return;
    programObject->setUniform4fv(location, count, value);
}

void GLES1Renderer::setUniform3fv(Program *programObject,
                                  UniformLocation location,
                                  GLint count,
                                  const GLfloat *value)
{
    if (location.value == -1)
        return;
    programObject->setUniform3fv(location, count, value);
}

void GLES1Renderer::setUniform2fv(Program *programObject,
                                  UniformLocation location,
                                  GLint count,
                                  const GLfloat *value)
{
    if (location.value == -1)
        return;
    programObject->setUniform2fv(location, count, value);
}

void GLES1Renderer::setUniform1f(Program *programObject, UniformLocation location, GLfloat value)
{
    if (location.value == -1)
        return;
    programObject->setUniform1fv(location, 1, &value);
}

void GLES1Renderer::setUniform1fv(Program *programObject,
                                  UniformLocation location,
                                  GLint count,
                                  const GLfloat *value)
{
    if (location.value == -1)
        return;
    programObject->setUniform1fv(location, count, value);
}

void GLES1Renderer::setAttributesEnabled(Context *context, State *glState, AttributesMask mask)
{
    GLES1State &gles1 = glState->gles1();

    ClientVertexArrayType nonTexcoordArrays[] = {
        ClientVertexArrayType::Vertex,
        ClientVertexArrayType::Normal,
        ClientVertexArrayType::Color,
        ClientVertexArrayType::PointSize,
    };

    for (const ClientVertexArrayType attrib : nonTexcoordArrays)
    {
        int index = VertexArrayIndex(attrib, glState->gles1());

        if (mask.test(index))
        {
            gles1.setClientStateEnabled(attrib, true);
            context->enableVertexAttribArray(index);
        }
        else
        {
            gles1.setClientStateEnabled(attrib, false);
            context->disableVertexAttribArray(index);
        }
    }

    for (unsigned int i = 0; i < kTexUnitCount; i++)
    {
        int index = TexCoordArrayIndex(i);

        if (mask.test(index))
        {
            gles1.setTexCoordArrayEnabled(i, true);
            context->enableVertexAttribArray(index);
        }
        else
        {
            gles1.setTexCoordArrayEnabled(i, false);
            context->disableVertexAttribArray(index);
        }
    }
}

}  // namespace gl
