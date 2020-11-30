//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationES1.cpp: Validation functions for OpenGL ES 1.0 entry point parameters

#include "libANGLE/validationES1_autogen.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/ErrorStrings.h"
#include "libANGLE/GLES1State.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/queryutils.h"
#include "libANGLE/validationES.h"

#define ANGLE_VALIDATE_IS_GLES1(context)                                                        \
    do                                                                                          \
    {                                                                                           \
        if (context->getClientType() != EGL_OPENGL_API && context->getClientMajorVersion() > 1) \
        {                                                                                       \
            context->validationError(GL_INVALID_OPERATION, kGLES1Only);                         \
            return false;                                                                       \
        }                                                                                       \
    } while (0)

namespace gl
{
using namespace err;

bool ValidateAlphaFuncCommon(const Context *context, AlphaTestFunc func)
{
    switch (func)
    {
        case AlphaTestFunc::AlwaysPass:
        case AlphaTestFunc::Equal:
        case AlphaTestFunc::Gequal:
        case AlphaTestFunc::Greater:
        case AlphaTestFunc::Lequal:
        case AlphaTestFunc::Less:
        case AlphaTestFunc::Never:
        case AlphaTestFunc::NotEqual:
            return true;
        default:
            context->validationError(GL_INVALID_ENUM, kEnumNotSupported);
            return false;
    }
}

bool ValidateClientStateCommon(const Context *context, ClientVertexArrayType arrayType)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    switch (arrayType)
    {
        case ClientVertexArrayType::Vertex:
        case ClientVertexArrayType::Normal:
        case ClientVertexArrayType::Color:
        case ClientVertexArrayType::TextureCoord:
            return true;
        case ClientVertexArrayType::PointSize:
            if (!context->getExtensions().pointSizeArrayOES)
            {
                context->validationError(GL_INVALID_ENUM, kPointSizeArrayExtensionNotEnabled);
                return false;
            }
            return true;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidClientState);
            return false;
    }
}

bool ValidateBuiltinVertexAttributeCommon(const Context *context,
                                          ClientVertexArrayType arrayType,
                                          GLint size,
                                          VertexAttribType type,
                                          GLsizei stride,
                                          const void *pointer)
{
    ANGLE_VALIDATE_IS_GLES1(context);

    if (stride < 0)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidVertexPointerStride);
        return false;
    }

    int minSize = 1;
    int maxSize = 4;

    switch (arrayType)
    {
        case ClientVertexArrayType::Vertex:
        case ClientVertexArrayType::TextureCoord:
            minSize = 2;
            maxSize = 4;
            break;
        case ClientVertexArrayType::Normal:
            minSize = 3;
            maxSize = 3;
            break;
        case ClientVertexArrayType::Color:
            minSize = 4;
            maxSize = 4;
            break;
        case ClientVertexArrayType::PointSize:
            if (!context->getExtensions().pointSizeArrayOES)
            {
                context->validationError(GL_INVALID_ENUM, kPointSizeArrayExtensionNotEnabled);
                return false;
            }

            minSize = 1;
            maxSize = 1;
            break;
        default:
            UNREACHABLE();
            return false;
    }

    if (size < minSize || size > maxSize)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidVertexPointerSize);
        return false;
    }

    switch (type)
    {
        case VertexAttribType::Byte:
            if (arrayType == ClientVertexArrayType::PointSize)
            {
                context->validationError(GL_INVALID_ENUM, kInvalidVertexPointerType);
                return false;
            }
            break;
        case VertexAttribType::Short:
            if (arrayType == ClientVertexArrayType::PointSize ||
                arrayType == ClientVertexArrayType::Color)
            {
                context->validationError(GL_INVALID_ENUM, kInvalidVertexPointerType);
                return false;
            }
            break;
        case VertexAttribType::Fixed:
        case VertexAttribType::Float:
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidVertexPointerType);
            return false;
    }

    return true;
}

bool ValidateLightCaps(const Context *context, GLenum light)
{
    if (light < GL_LIGHT0 || light >= GL_LIGHT0 + context->getCaps().maxLights)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidLight);
        return false;
    }

    return true;
}

bool ValidateLightCommon(const Context *context,
                         GLenum light,
                         LightParameter pname,
                         const GLfloat *params)
{

    ANGLE_VALIDATE_IS_GLES1(context);

    if (!ValidateLightCaps(context, light))
    {
        return false;
    }

    switch (pname)
    {
        case LightParameter::Ambient:
        case LightParameter::Diffuse:
        case LightParameter::Specular:
        case LightParameter::Position:
        case LightParameter::SpotDirection:
            return true;
        case LightParameter::SpotExponent:
            if (params[0] < 0.0f || params[0] > 128.0f)
            {
                context->validationError(GL_INVALID_VALUE, kLightParameterOutOfRange);
                return false;
            }
            return true;
        case LightParameter::SpotCutoff:
            if (params[0] == 180.0f)
            {
                return true;
            }
            if (params[0] < 0.0f || params[0] > 90.0f)
            {
                context->validationError(GL_INVALID_VALUE, kLightParameterOutOfRange);
                return false;
            }
            return true;
        case LightParameter::ConstantAttenuation:
        case LightParameter::LinearAttenuation:
        case LightParameter::QuadraticAttenuation:
            if (params[0] < 0.0f)
            {
                context->validationError(GL_INVALID_VALUE, kLightParameterOutOfRange);
                return false;
            }
            return true;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidLightParameter);
            return false;
    }
}

bool ValidateLightSingleComponent(const Context *context,
                                  GLenum light,
                                  LightParameter pname,
                                  GLfloat param)
{
    if (!ValidateLightCommon(context, light, pname, &param))
    {
        return false;
    }

    if (GetLightParameterCount(pname) > 1)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidLightParameter);
        return false;
    }

    return true;
}

bool ValidateMaterialCommon(const Context *context,
                            GLenum face,
                            MaterialParameter pname,
                            const GLfloat *params)
{
    switch (pname)
    {
        case MaterialParameter::Ambient:
        case MaterialParameter::AmbientAndDiffuse:
        case MaterialParameter::Diffuse:
        case MaterialParameter::Specular:
        case MaterialParameter::Emission:
            return true;
        case MaterialParameter::Shininess:
            if (params[0] < 0.0f || params[0] > 128.0f)
            {
                context->validationError(GL_INVALID_VALUE, kMaterialParameterOutOfRange);
                return false;
            }
            return true;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidMaterialParameter);
            return false;
    }
}

bool ValidateMaterialSetting(const Context *context,
                             GLenum face,
                             MaterialParameter pname,
                             const GLfloat *params)
{
    ANGLE_VALIDATE_IS_GLES1(context);

    if (face != GL_FRONT_AND_BACK)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidMaterialFace);
        return false;
    }

    return ValidateMaterialCommon(context, face, pname, params);
}

bool ValidateMaterialQuery(const Context *context, GLenum face, MaterialParameter pname)
{
    ANGLE_VALIDATE_IS_GLES1(context);

    if (face != GL_FRONT && face != GL_BACK)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidMaterialFace);
        return false;
    }

    GLfloat dummyParams[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    return ValidateMaterialCommon(context, face, pname, dummyParams);
}

bool ValidateMaterialSingleComponent(const Context *context,
                                     GLenum face,
                                     MaterialParameter pname,
                                     GLfloat param)
{
    if (!ValidateMaterialSetting(context, face, pname, &param))
    {
        return false;
    }

    if (GetMaterialParameterCount(pname) > 1)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidMaterialParameter);
        return false;
    }

    return true;
}

bool ValidateLightModelCommon(const Context *context, GLenum pname)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    switch (pname)
    {
        case GL_LIGHT_MODEL_AMBIENT:
        case GL_LIGHT_MODEL_TWO_SIDE:
            return true;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidLightModelParameter);
            return false;
    }
}

bool ValidateLightModelSingleComponent(const Context *context, GLenum pname)
{
    if (!ValidateLightModelCommon(context, pname))
    {
        return false;
    }

    switch (pname)
    {
        case GL_LIGHT_MODEL_TWO_SIDE:
            return true;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidLightModelParameter);
            return false;
    }
}

bool ValidateClipPlaneCommon(const Context *context, GLenum plane)
{
    ANGLE_VALIDATE_IS_GLES1(context);

    if (plane < GL_CLIP_PLANE0 || plane >= GL_CLIP_PLANE0 + context->getCaps().maxClipPlanes)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidClipPlane);
        return false;
    }

    return true;
}

bool ValidateFogCommon(const Context *context, GLenum pname, const GLfloat *params)
{
    ANGLE_VALIDATE_IS_GLES1(context);

    switch (pname)
    {
        case GL_FOG_MODE:
        {
            GLenum modeParam = static_cast<GLenum>(params[0]);
            switch (modeParam)
            {
                case GL_EXP:
                case GL_EXP2:
                case GL_LINEAR:
                    return true;
                default:
                    context->validationError(GL_INVALID_VALUE, kInvalidFogMode);
                    return false;
            }
        }
        break;
        case GL_FOG_START:
        case GL_FOG_END:
        case GL_FOG_COLOR:
            break;
        case GL_FOG_DENSITY:
            if (params[0] < 0.0f)
            {
                context->validationError(GL_INVALID_VALUE, kInvalidFogDensity);
                return false;
            }
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidFogParameter);
            return false;
    }
    return true;
}

bool ValidateTexEnvCommon(const Context *context,
                          TextureEnvTarget target,
                          TextureEnvParameter pname,
                          const GLfloat *params)
{
    ANGLE_VALIDATE_IS_GLES1(context);

    switch (target)
    {
        case TextureEnvTarget::Env:
            switch (pname)
            {
                case TextureEnvParameter::Mode:
                {
                    TextureEnvMode mode = FromGLenum<TextureEnvMode>(ConvertToGLenum(params[0]));
                    switch (mode)
                    {
                        case TextureEnvMode::Add:
                        case TextureEnvMode::Blend:
                        case TextureEnvMode::Combine:
                        case TextureEnvMode::Decal:
                        case TextureEnvMode::Modulate:
                        case TextureEnvMode::Replace:
                            break;
                        default:
                            context->validationError(GL_INVALID_VALUE, kInvalidTextureEnvMode);
                            return false;
                    }
                    break;
                }
                case TextureEnvParameter::CombineRgb:
                case TextureEnvParameter::CombineAlpha:
                {
                    TextureCombine combine = FromGLenum<TextureCombine>(ConvertToGLenum(params[0]));
                    switch (combine)
                    {
                        case TextureCombine::Add:
                        case TextureCombine::AddSigned:
                        case TextureCombine::Interpolate:
                        case TextureCombine::Modulate:
                        case TextureCombine::Replace:
                        case TextureCombine::Subtract:
                            break;
                        case TextureCombine::Dot3Rgb:
                        case TextureCombine::Dot3Rgba:
                            if (pname == TextureEnvParameter::CombineAlpha)
                            {
                                context->validationError(GL_INVALID_VALUE, kInvalidTextureCombine);
                                return false;
                            }
                            break;
                        default:
                            context->validationError(GL_INVALID_VALUE, kInvalidTextureCombine);
                            return false;
                    }
                    break;
                }
                case TextureEnvParameter::Src0Rgb:
                case TextureEnvParameter::Src1Rgb:
                case TextureEnvParameter::Src2Rgb:
                case TextureEnvParameter::Src0Alpha:
                case TextureEnvParameter::Src1Alpha:
                case TextureEnvParameter::Src2Alpha:
                {
                    TextureSrc combine = FromGLenum<TextureSrc>(ConvertToGLenum(params[0]));
                    switch (combine)
                    {
                        case TextureSrc::Constant:
                        case TextureSrc::Previous:
                        case TextureSrc::PrimaryColor:
                        case TextureSrc::Texture:
                            break;
                        default:
                            context->validationError(GL_INVALID_VALUE, kInvalidTextureCombineSrc);
                            return false;
                    }
                    break;
                }
                case TextureEnvParameter::Op0Rgb:
                case TextureEnvParameter::Op1Rgb:
                case TextureEnvParameter::Op2Rgb:
                case TextureEnvParameter::Op0Alpha:
                case TextureEnvParameter::Op1Alpha:
                case TextureEnvParameter::Op2Alpha:
                {
                    TextureOp operand = FromGLenum<TextureOp>(ConvertToGLenum(params[0]));
                    switch (operand)
                    {
                        case TextureOp::SrcAlpha:
                        case TextureOp::OneMinusSrcAlpha:
                            break;
                        case TextureOp::SrcColor:
                        case TextureOp::OneMinusSrcColor:
                            if (pname == TextureEnvParameter::Op0Alpha ||
                                pname == TextureEnvParameter::Op1Alpha ||
                                pname == TextureEnvParameter::Op2Alpha)
                            {
                                context->validationError(GL_INVALID_VALUE, kInvalidTextureCombine);
                                return false;
                            }
                            break;
                        default:
                            context->validationError(GL_INVALID_VALUE, kInvalidTextureCombineOp);
                            return false;
                    }
                    break;
                }
                case TextureEnvParameter::RgbScale:
                case TextureEnvParameter::AlphaScale:
                    if (params[0] != 1.0f && params[0] != 2.0f && params[0] != 4.0f)
                    {
                        context->validationError(GL_INVALID_VALUE, kInvalidTextureEnvScale);
                        return false;
                    }
                    break;
                case TextureEnvParameter::Color:
                    break;
                default:
                    context->validationError(GL_INVALID_ENUM, kInvalidTextureEnvParameter);
                    return false;
            }
            break;
        case TextureEnvTarget::PointSprite:
            if (!context->getExtensions().pointSpriteOES)
            {
                context->validationError(GL_INVALID_ENUM, kInvalidTextureEnvTarget);
                return false;
            }
            switch (pname)
            {
                case TextureEnvParameter::PointCoordReplace:
                    break;
                default:
                    context->validationError(GL_INVALID_ENUM, kInvalidTextureEnvParameter);
                    return false;
            }
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidTextureEnvTarget);
            return false;
    }
    return true;
}

bool ValidateGetTexEnvCommon(const Context *context,
                             TextureEnvTarget target,
                             TextureEnvParameter pname)
{
    GLfloat dummy[4] = {};
    switch (pname)
    {
        case TextureEnvParameter::Mode:
            ConvertPackedEnum(TextureEnvMode::Add, dummy);
            break;
        case TextureEnvParameter::CombineRgb:
        case TextureEnvParameter::CombineAlpha:
            ConvertPackedEnum(TextureCombine::Add, dummy);
            break;
        case TextureEnvParameter::Src0Rgb:
        case TextureEnvParameter::Src1Rgb:
        case TextureEnvParameter::Src2Rgb:
        case TextureEnvParameter::Src0Alpha:
        case TextureEnvParameter::Src1Alpha:
        case TextureEnvParameter::Src2Alpha:
            ConvertPackedEnum(TextureSrc::Constant, dummy);
            break;
        case TextureEnvParameter::Op0Rgb:
        case TextureEnvParameter::Op1Rgb:
        case TextureEnvParameter::Op2Rgb:
        case TextureEnvParameter::Op0Alpha:
        case TextureEnvParameter::Op1Alpha:
        case TextureEnvParameter::Op2Alpha:
            ConvertPackedEnum(TextureOp::SrcAlpha, dummy);
            break;
        case TextureEnvParameter::RgbScale:
        case TextureEnvParameter::AlphaScale:
        case TextureEnvParameter::PointCoordReplace:
            dummy[0] = 1.0f;
            break;
        default:
            break;
    }

    return ValidateTexEnvCommon(context, target, pname, dummy);
}

bool ValidatePointParameterCommon(const Context *context,
                                  PointParameter pname,
                                  const GLfloat *params)
{
    ANGLE_VALIDATE_IS_GLES1(context);

    switch (pname)
    {
        case PointParameter::PointSizeMin:
        case PointParameter::PointSizeMax:
        case PointParameter::PointFadeThresholdSize:
        case PointParameter::PointDistanceAttenuation:
            for (unsigned int i = 0; i < GetPointParameterCount(pname); i++)
            {
                if (params[i] < 0.0f)
                {
                    context->validationError(GL_INVALID_VALUE, kInvalidPointParameterValue);
                    return false;
                }
            }
            break;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidPointParameter);
            return false;
    }

    return true;
}

bool ValidatePointSizeCommon(const Context *context, GLfloat size)
{
    ANGLE_VALIDATE_IS_GLES1(context);

    if (size <= 0.0f)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidPointSizeValue);
        return false;
    }

    return true;
}

bool ValidateDrawTexCommon(const Context *context, float width, float height)
{
    ANGLE_VALIDATE_IS_GLES1(context);

    if (width <= 0.0f || height <= 0.0f)
    {
        context->validationError(GL_INVALID_VALUE, kNonPositiveDrawTextureDimension);
        return false;
    }

    return true;
}

}  // namespace gl

namespace gl
{

bool ValidateAlphaFunc(const Context *context, AlphaTestFunc func, GLfloat ref)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return ValidateAlphaFuncCommon(context, func);
}

bool ValidateAlphaFuncx(const Context *context, AlphaTestFunc func, GLfixed ref)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return ValidateAlphaFuncCommon(context, func);
}

bool ValidateClearColorx(const Context *context,
                         GLfixed red,
                         GLfixed green,
                         GLfixed blue,
                         GLfixed alpha)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateClearDepthx(const Context *context, GLfixed depth)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateClientActiveTexture(const Context *context, GLenum texture)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return ValidateMultitextureUnit(context, texture);
}

bool ValidateClipPlanef(const Context *context, GLenum plane, const GLfloat *eqn)
{
    return ValidateClipPlaneCommon(context, plane);
}

bool ValidateClipPlanex(const Context *context, GLenum plane, const GLfixed *equation)
{
    return ValidateClipPlaneCommon(context, plane);
}

bool ValidateColor4f(const Context *context,
                     GLfloat red,
                     GLfloat green,
                     GLfloat blue,
                     GLfloat alpha)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateColor4ub(const Context *context,
                      GLubyte red,
                      GLubyte green,
                      GLubyte blue,
                      GLubyte alpha)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateColor4x(const Context *context,
                     GLfixed red,
                     GLfixed green,
                     GLfixed blue,
                     GLfixed alpha)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateColorPointer(const Context *context,
                          GLint size,
                          VertexAttribType type,
                          GLsizei stride,
                          const void *pointer)
{
    return ValidateBuiltinVertexAttributeCommon(context, ClientVertexArrayType::Color, size, type,
                                                stride, pointer);
}

bool ValidateCullFace(const Context *context, GLenum mode)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateDepthRangex(const Context *context, GLfixed n, GLfixed f)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    if (context->getExtensions().webglCompatibility && n > f)
    {
        context->validationError(GL_INVALID_OPERATION, kInvalidDepthRange);
        return false;
    }

    return true;
}

bool ValidateDisableClientState(const Context *context, ClientVertexArrayType arrayType)
{
    return ValidateClientStateCommon(context, arrayType);
}

bool ValidateEnableClientState(const Context *context, ClientVertexArrayType arrayType)
{
    return ValidateClientStateCommon(context, arrayType);
}

bool ValidateFogf(const Context *context, GLenum pname, GLfloat param)
{
    return ValidateFogCommon(context, pname, &param);
}

bool ValidateFogfv(const Context *context, GLenum pname, const GLfloat *params)
{
    return ValidateFogCommon(context, pname, params);
}

bool ValidateFogx(const Context *context, GLenum pname, GLfixed param)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    GLfloat asFloat =
        pname == GL_FOG_MODE ? static_cast<GLfloat>(param) : ConvertFixedToFloat(param);
    return ValidateFogCommon(context, pname, &asFloat);
}

bool ValidateFogxv(const Context *context, GLenum pname, const GLfixed *params)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    unsigned int paramCount = GetFogParameterCount(pname);
    GLfloat paramsf[4]      = {};

    if (pname == GL_FOG_MODE)
    {
        paramsf[0] = static_cast<GLfloat>(params[0]);
    }
    else
    {
        for (unsigned int i = 0; i < paramCount; i++)
        {
            paramsf[i] = ConvertFixedToFloat(params[i]);
        }
    }

    return ValidateFogCommon(context, pname, paramsf);
}

bool ValidateFrustumf(const Context *context,
                      GLfloat l,
                      GLfloat r,
                      GLfloat b,
                      GLfloat t,
                      GLfloat n,
                      GLfloat f)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    if (l == r || b == t || n == f || n <= 0.0f || f <= 0.0f)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidProjectionMatrix);
    }
    return true;
}

bool ValidateFrustumx(const Context *context,
                      GLfixed l,
                      GLfixed r,
                      GLfixed b,
                      GLfixed t,
                      GLfixed n,
                      GLfixed f)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    if (l == r || b == t || n == f || n <= 0 || f <= 0)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidProjectionMatrix);
    }
    return true;
}

bool ValidateGetBufferParameteriv(const Context *context,
                                  GLenum target,
                                  GLenum pname,
                                  const GLint *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetClipPlanef(const Context *context, GLenum plane, const GLfloat *equation)
{
    return ValidateClipPlaneCommon(context, plane);
}

bool ValidateGetClipPlanex(const Context *context, GLenum plane, const GLfixed *equation)
{
    return ValidateClipPlaneCommon(context, plane);
}

bool ValidateGetFixedv(const Context *context, GLenum pname, const GLfixed *params)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    GLenum nativeType;
    unsigned int numParams = 0;
    return ValidateStateQuery(context, pname, &nativeType, &numParams);
}

bool ValidateGetLightfv(const Context *context,
                        GLenum light,
                        LightParameter pname,
                        const GLfloat *params)
{
    GLfloat dummyParams[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    return ValidateLightCommon(context, light, pname, dummyParams);
}

bool ValidateGetLightxv(const Context *context,
                        GLenum light,
                        LightParameter pname,
                        const GLfixed *params)
{
    GLfloat dummyParams[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    return ValidateLightCommon(context, light, pname, dummyParams);
}

bool ValidateGetMaterialfv(const Context *context,
                           GLenum face,
                           MaterialParameter pname,
                           const GLfloat *params)
{
    return ValidateMaterialQuery(context, face, pname);
}

bool ValidateGetMaterialxv(const Context *context,
                           GLenum face,
                           MaterialParameter pname,
                           const GLfixed *params)
{
    return ValidateMaterialQuery(context, face, pname);
}

bool ValidateGetTexEnvfv(const Context *context,
                         TextureEnvTarget target,
                         TextureEnvParameter pname,
                         const GLfloat *params)
{
    return ValidateGetTexEnvCommon(context, target, pname);
}

bool ValidateGetTexEnviv(const Context *context,
                         TextureEnvTarget target,
                         TextureEnvParameter pname,
                         const GLint *params)
{
    return ValidateGetTexEnvCommon(context, target, pname);
}

bool ValidateGetTexEnvxv(const Context *context,
                         TextureEnvTarget target,
                         TextureEnvParameter pname,
                         const GLfixed *params)
{
    return ValidateGetTexEnvCommon(context, target, pname);
}

bool ValidateGetTexParameterxv(const Context *context,
                               TextureType target,
                               GLenum pname,
                               const GLfixed *params)
{
    ANGLE_VALIDATE_IS_GLES1(context);

    if (!ValidateGetTexParameterBase(context, target, pname, nullptr))
    {
        return false;
    }

    return true;
}

bool ValidateLightModelf(const Context *context, GLenum pname, GLfloat param)
{
    return ValidateLightModelSingleComponent(context, pname);
}

bool ValidateLightModelfv(const Context *context, GLenum pname, const GLfloat *params)
{
    return ValidateLightModelCommon(context, pname);
}

bool ValidateLightModelx(const Context *context, GLenum pname, GLfixed param)
{
    return ValidateLightModelSingleComponent(context, pname);
}

bool ValidateLightModelxv(const Context *context, GLenum pname, const GLfixed *param)
{
    return ValidateLightModelCommon(context, pname);
}

bool ValidateLightf(const Context *context, GLenum light, LightParameter pname, GLfloat param)
{
    return ValidateLightSingleComponent(context, light, pname, param);
}

bool ValidateLightfv(const Context *context,
                     GLenum light,
                     LightParameter pname,
                     const GLfloat *params)
{
    return ValidateLightCommon(context, light, pname, params);
}

bool ValidateLightx(const Context *context, GLenum light, LightParameter pname, GLfixed param)
{
    return ValidateLightSingleComponent(context, light, pname, ConvertFixedToFloat(param));
}

bool ValidateLightxv(const Context *context,
                     GLenum light,
                     LightParameter pname,
                     const GLfixed *params)
{
    GLfloat paramsf[4];
    for (unsigned int i = 0; i < GetLightParameterCount(pname); i++)
    {
        paramsf[i] = ConvertFixedToFloat(params[i]);
    }

    return ValidateLightCommon(context, light, pname, paramsf);
}

bool ValidateLineWidthx(const Context *context, GLfixed width)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    if (width <= 0)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidWidth);
        return false;
    }

    return true;
}

bool ValidateLoadIdentity(const Context *context)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateLoadMatrixf(const Context *context, const GLfloat *m)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateLoadMatrixx(const Context *context, const GLfixed *m)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateLogicOp(const Context *context, LogicalOperation opcode)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    switch (opcode)
    {
        case LogicalOperation::And:
        case LogicalOperation::AndInverted:
        case LogicalOperation::AndReverse:
        case LogicalOperation::Clear:
        case LogicalOperation::Copy:
        case LogicalOperation::CopyInverted:
        case LogicalOperation::Equiv:
        case LogicalOperation::Invert:
        case LogicalOperation::Nand:
        case LogicalOperation::Noop:
        case LogicalOperation::Nor:
        case LogicalOperation::Or:
        case LogicalOperation::OrInverted:
        case LogicalOperation::OrReverse:
        case LogicalOperation::Set:
        case LogicalOperation::Xor:
            return true;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidLogicOp);
            return false;
    }
}

bool ValidateMaterialf(const Context *context, GLenum face, MaterialParameter pname, GLfloat param)
{
    return ValidateMaterialSingleComponent(context, face, pname, param);
}

bool ValidateMaterialfv(const Context *context,
                        GLenum face,
                        MaterialParameter pname,
                        const GLfloat *params)
{
    return ValidateMaterialSetting(context, face, pname, params);
}

bool ValidateMaterialx(const Context *context, GLenum face, MaterialParameter pname, GLfixed param)
{
    return ValidateMaterialSingleComponent(context, face, pname, ConvertFixedToFloat(param));
}

bool ValidateMaterialxv(const Context *context,
                        GLenum face,
                        MaterialParameter pname,
                        const GLfixed *params)
{
    GLfloat paramsf[4];

    for (unsigned int i = 0; i < GetMaterialParameterCount(pname); i++)
    {
        paramsf[i] = ConvertFixedToFloat(params[i]);
    }

    return ValidateMaterialSetting(context, face, pname, paramsf);
}

bool ValidateMatrixMode(const Context *context, MatrixType mode)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    switch (mode)
    {
        case MatrixType::Projection:
        case MatrixType::Modelview:
        case MatrixType::Texture:
            return true;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidMatrixMode);
            return false;
    }
}

bool ValidateMultMatrixf(const Context *context, const GLfloat *m)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateMultMatrixx(const Context *context, const GLfixed *m)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateMultiTexCoord4f(const Context *context,
                             GLenum target,
                             GLfloat s,
                             GLfloat t,
                             GLfloat r,
                             GLfloat q)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return ValidateMultitextureUnit(context, target);
}

bool ValidateMultiTexCoord4x(const Context *context,
                             GLenum target,
                             GLfixed s,
                             GLfixed t,
                             GLfixed r,
                             GLfixed q)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return ValidateMultitextureUnit(context, target);
}

bool ValidateNormal3f(const Context *context, GLfloat nx, GLfloat ny, GLfloat nz)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateNormal3x(const Context *context, GLfixed nx, GLfixed ny, GLfixed nz)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateNormalPointer(const Context *context,
                           VertexAttribType type,
                           GLsizei stride,
                           const void *pointer)
{
    return ValidateBuiltinVertexAttributeCommon(context, ClientVertexArrayType::Normal, 3, type,
                                                stride, pointer);
}

bool ValidateOrthof(const Context *context,
                    GLfloat l,
                    GLfloat r,
                    GLfloat b,
                    GLfloat t,
                    GLfloat n,
                    GLfloat f)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    // [OpenGL ES 1.1.12] section 2.10.2 page 31:
    // If l is equal to r, b is equal to t, or n is equal to f, the
    // error INVALID VALUE results.
    if (l == r || b == t || n == f)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidProjectionMatrix);
    }
    return true;
}

bool ValidateOrthox(const Context *context,
                    GLfixed l,
                    GLfixed r,
                    GLfixed b,
                    GLfixed t,
                    GLfixed n,
                    GLfixed f)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    if (l == r || b == t || n == f)
    {
        context->validationError(GL_INVALID_VALUE, kInvalidProjectionMatrix);
    }
    return true;
}

bool ValidatePointParameterf(const Context *context, PointParameter pname, GLfloat param)
{
    unsigned int paramCount = GetPointParameterCount(pname);
    if (paramCount != 1)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidPointParameter);
        return false;
    }

    return ValidatePointParameterCommon(context, pname, &param);
}

bool ValidatePointParameterfv(const Context *context, PointParameter pname, const GLfloat *params)
{
    return ValidatePointParameterCommon(context, pname, params);
}

bool ValidatePointParameterx(const Context *context, PointParameter pname, GLfixed param)
{
    unsigned int paramCount = GetPointParameterCount(pname);
    if (paramCount != 1)
    {
        context->validationError(GL_INVALID_ENUM, kInvalidPointParameter);
        return false;
    }

    GLfloat paramf = ConvertFixedToFloat(param);
    return ValidatePointParameterCommon(context, pname, &paramf);
}

bool ValidatePointParameterxv(const Context *context, PointParameter pname, const GLfixed *params)
{
    GLfloat paramsf[4] = {};
    for (unsigned int i = 0; i < GetPointParameterCount(pname); i++)
    {
        paramsf[i] = ConvertFixedToFloat(params[i]);
    }
    return ValidatePointParameterCommon(context, pname, paramsf);
}

bool ValidatePointSize(const Context *context, GLfloat size)
{
    return ValidatePointSizeCommon(context, size);
}

bool ValidatePointSizex(const Context *context, GLfixed size)
{
    return ValidatePointSizeCommon(context, ConvertFixedToFloat(size));
}

bool ValidatePolygonOffsetx(const Context *context, GLfixed factor, GLfixed units)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidatePopMatrix(const Context *context)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    const auto &stack = context->getState().gles1().currentMatrixStack();
    if (stack.size() == 1)
    {
        context->validationError(GL_STACK_UNDERFLOW, kMatrixStackUnderflow);
        return false;
    }
    return true;
}

bool ValidatePushMatrix(const Context *context)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    const auto &stack = context->getState().gles1().currentMatrixStack();
    if (stack.size() == stack.max_size())
    {
        context->validationError(GL_STACK_OVERFLOW, kMatrixStackOverflow);
        return false;
    }
    return true;
}

bool ValidateRotatef(const Context *context, GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateRotatex(const Context *context, GLfixed angle, GLfixed x, GLfixed y, GLfixed z)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateSampleCoveragex(const Context *context, GLclampx value, GLboolean invert)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateScalef(const Context *context, GLfloat x, GLfloat y, GLfloat z)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateScalex(const Context *context, GLfixed x, GLfixed y, GLfixed z)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateShadeModel(const Context *context, ShadingModel mode)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    switch (mode)
    {
        case ShadingModel::Flat:
        case ShadingModel::Smooth:
            return true;
        default:
            context->validationError(GL_INVALID_ENUM, kInvalidShadingModel);
            return false;
    }
}

bool ValidateTexCoordPointer(const Context *context,
                             GLint size,
                             VertexAttribType type,
                             GLsizei stride,
                             const void *pointer)
{
    return ValidateBuiltinVertexAttributeCommon(context, ClientVertexArrayType::TextureCoord, size,
                                                type, stride, pointer);
}

bool ValidateTexEnvf(const Context *context,
                     TextureEnvTarget target,
                     TextureEnvParameter pname,
                     GLfloat param)
{
    return ValidateTexEnvCommon(context, target, pname, &param);
}

bool ValidateTexEnvfv(const Context *context,
                      TextureEnvTarget target,
                      TextureEnvParameter pname,
                      const GLfloat *params)
{
    return ValidateTexEnvCommon(context, target, pname, params);
}

bool ValidateTexEnvi(const Context *context,
                     TextureEnvTarget target,
                     TextureEnvParameter pname,
                     GLint param)
{
    GLfloat paramf = static_cast<GLfloat>(param);
    return ValidateTexEnvCommon(context, target, pname, &paramf);
}

bool ValidateTexEnviv(const Context *context,
                      TextureEnvTarget target,
                      TextureEnvParameter pname,
                      const GLint *params)
{
    GLfloat paramsf[4];
    for (unsigned int i = 0; i < GetTextureEnvParameterCount(pname); i++)
    {
        paramsf[i] = static_cast<GLfloat>(params[i]);
    }
    return ValidateTexEnvCommon(context, target, pname, paramsf);
}

bool ValidateTexEnvx(const Context *context,
                     TextureEnvTarget target,
                     TextureEnvParameter pname,
                     GLfixed param)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    GLfloat paramsf[4] = {};
    ConvertTextureEnvFromFixed(pname, &param, paramsf);
    return ValidateTexEnvCommon(context, target, pname, paramsf);
}

bool ValidateTexEnvxv(const Context *context,
                      TextureEnvTarget target,
                      TextureEnvParameter pname,
                      const GLfixed *params)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    GLfloat paramsf[4] = {};
    ConvertTextureEnvFromFixed(pname, params, paramsf);
    return ValidateTexEnvCommon(context, target, pname, paramsf);
}

bool ValidateTexParameterBaseForGLfixed(const Context *context,
                                        TextureType target,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        bool vectorParams,
                                        const GLfixed *params)
{
    // Convert GLfixed parameter for GL_TEXTURE_MAX_ANISOTROPY_EXT independently
    // since it compares against 1 and maxTextureAnisotropy instead of just 0
    // (other values are fine to leave unconverted since they only check positive or negative or
    // are used as enums)
    GLfloat paramValue;
    if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT)
    {
        paramValue = ConvertFixedToFloat(static_cast<GLfixed>(params[0]));
    }
    else
    {
        paramValue = static_cast<GLfloat>(params[0]);
    }
    return ValidateTexParameterBase(context, target, pname, bufSize, vectorParams, &paramValue);
}

bool ValidateTexParameterx(const Context *context, TextureType target, GLenum pname, GLfixed param)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return ValidateTexParameterBaseForGLfixed(context, target, pname, -1, false, &param);
}

bool ValidateTexParameterxv(const Context *context,
                            TextureType target,
                            GLenum pname,
                            const GLfixed *params)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return ValidateTexParameterBaseForGLfixed(context, target, pname, -1, true, params);
}

bool ValidateTranslatef(const Context *context, GLfloat x, GLfloat y, GLfloat z)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateTranslatex(const Context *context, GLfixed x, GLfixed y, GLfixed z)
{
    ANGLE_VALIDATE_IS_GLES1(context);
    return true;
}

bool ValidateVertexPointer(const Context *context,
                           GLint size,
                           VertexAttribType type,
                           GLsizei stride,
                           const void *pointer)
{
    return ValidateBuiltinVertexAttributeCommon(context, ClientVertexArrayType::Vertex, size, type,
                                                stride, pointer);
}

bool ValidateDrawTexfOES(const Context *context,
                         GLfloat x,
                         GLfloat y,
                         GLfloat z,
                         GLfloat width,
                         GLfloat height)
{
    return ValidateDrawTexCommon(context, width, height);
}

bool ValidateDrawTexfvOES(const Context *context, const GLfloat *coords)
{
    return ValidateDrawTexCommon(context, coords[3], coords[4]);
}

bool ValidateDrawTexiOES(const Context *context,
                         GLint x,
                         GLint y,
                         GLint z,
                         GLint width,
                         GLint height)
{
    return ValidateDrawTexCommon(context, static_cast<GLfloat>(width),
                                 static_cast<GLfloat>(height));
}

bool ValidateDrawTexivOES(const Context *context, const GLint *coords)
{
    return ValidateDrawTexCommon(context, static_cast<GLfloat>(coords[3]),
                                 static_cast<GLfloat>(coords[4]));
}

bool ValidateDrawTexsOES(const Context *context,
                         GLshort x,
                         GLshort y,
                         GLshort z,
                         GLshort width,
                         GLshort height)
{
    return ValidateDrawTexCommon(context, static_cast<GLfloat>(width),
                                 static_cast<GLfloat>(height));
}

bool ValidateDrawTexsvOES(const Context *context, const GLshort *coords)
{
    return ValidateDrawTexCommon(context, static_cast<GLfloat>(coords[3]),
                                 static_cast<GLfloat>(coords[4]));
}

bool ValidateDrawTexxOES(const Context *context,
                         GLfixed x,
                         GLfixed y,
                         GLfixed z,
                         GLfixed width,
                         GLfixed height)
{
    return ValidateDrawTexCommon(context, ConvertFixedToFloat(width), ConvertFixedToFloat(height));
}

bool ValidateDrawTexxvOES(const Context *context, const GLfixed *coords)
{
    return ValidateDrawTexCommon(context, ConvertFixedToFloat(coords[3]),
                                 ConvertFixedToFloat(coords[4]));
}

bool ValidateCurrentPaletteMatrixOES(const Context *context, GLuint matrixpaletteindex)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateLoadPaletteFromModelViewMatrixOES(const Context *context)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateMatrixIndexPointerOES(const Context *context,
                                   GLint size,
                                   GLenum type,
                                   GLsizei stride,
                                   const void *pointer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateWeightPointerOES(const Context *context,
                              GLint size,
                              GLenum type,
                              GLsizei stride,
                              const void *pointer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidatePointSizePointerOES(const Context *context,
                                 VertexAttribType type,
                                 GLsizei stride,
                                 const void *pointer)
{
    return ValidateBuiltinVertexAttributeCommon(context, ClientVertexArrayType::PointSize, 1, type,
                                                stride, pointer);
}

bool ValidateQueryMatrixxOES(const Context *context, const GLfixed *mantissa, const GLint *exponent)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGenFramebuffersOES(const Context *context,
                                GLsizei n,
                                const FramebufferID *framebuffers)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateDeleteFramebuffersOES(const Context *context,
                                   GLsizei n,
                                   const FramebufferID *framebuffers)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGenRenderbuffersOES(const Context *context,
                                 GLsizei n,
                                 const RenderbufferID *renderbuffers)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateDeleteRenderbuffersOES(const Context *context,
                                    GLsizei n,
                                    const RenderbufferID *renderbuffers)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateBindFramebufferOES(const Context *context, GLenum target, FramebufferID framebuffer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateBindRenderbufferOES(const Context *context, GLenum target, RenderbufferID renderbuffer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateCheckFramebufferStatusOES(const Context *context, GLenum target)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateFramebufferRenderbufferOES(const Context *context,
                                        GLenum target,
                                        GLenum attachment,
                                        GLenum rbtarget,
                                        RenderbufferID renderbuffer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateFramebufferTexture2DOES(const Context *context,
                                     GLenum target,
                                     GLenum attachment,
                                     TextureTarget textarget,
                                     TextureID texture,
                                     GLint level)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGenerateMipmapOES(const Context *context, TextureType target)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetFramebufferAttachmentParameterivOES(const Context *context,
                                                    GLenum target,
                                                    GLenum attachment,
                                                    GLenum pname,
                                                    const GLint *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetRenderbufferParameterivOES(const Context *context,
                                           GLenum target,
                                           GLenum pname,
                                           const GLint *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateIsFramebufferOES(const Context *context, FramebufferID framebuffer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateIsRenderbufferOES(const Context *context, RenderbufferID renderbuffer)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateRenderbufferStorageOES(const Context *context,
                                    GLenum target,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height)
{
    UNIMPLEMENTED();
    return true;
}

// GL_OES_texture_cube_map

bool ValidateGetTexGenfvOES(const Context *context,
                            GLenum coord,
                            GLenum pname,
                            const GLfloat *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetTexGenivOES(const Context *context, GLenum coord, GLenum pname, const int *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateGetTexGenxvOES(const Context *context,
                            GLenum coord,
                            GLenum pname,
                            const GLfixed *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexGenfvOES(const Context *context, GLenum coord, GLenum pname, const GLfloat *params)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexGenivOES(const Context *context, GLenum coord, GLenum pname, const GLint *param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexGenxvOES(const Context *context, GLenum coord, GLenum pname, const GLint *param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexGenfOES(const Context *context, GLenum coord, GLenum pname, GLfloat param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexGeniOES(const Context *context, GLenum coord, GLenum pname, GLint param)
{
    UNIMPLEMENTED();
    return true;
}

bool ValidateTexGenxOES(const Context *context, GLenum coord, GLenum pname, GLfixed param)
{
    UNIMPLEMENTED();
    return true;
}

}  // namespace gl
