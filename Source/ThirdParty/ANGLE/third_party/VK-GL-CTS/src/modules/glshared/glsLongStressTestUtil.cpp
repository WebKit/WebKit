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
 * \brief Utilities for tests with gls::LongStressCase.
 *//*--------------------------------------------------------------------*/

#include "glsLongStressTestUtil.hpp"
#include "tcuStringTemplate.hpp"
#include "deStringUtil.hpp"

#include "glw.h"

using de::toString;
using std::map;
using std::string;
using tcu::Mat2;
using tcu::Mat3;
using tcu::Mat4;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

namespace deqp
{
namespace gls
{
namespace LongStressTestUtil
{

template <int Size>
static tcu::Matrix<float, Size, Size> translationMat(const float v)
{
    tcu::Matrix<float, Size, Size> res(1.0f);
    tcu::Vector<float, Size> col(v);
    col[Size - 1] = 1.0f;
    res.setColumn(Size - 1, col);
    return res;
}

// Specializes certain template patterns in templ for GLSL version m_glslVersion; params in additionalParams (optional) are also included in the substitution.
string ProgramLibrary::substitute(const string &templ, const map<string, string> &additionalParams) const
{
    const bool isGLSL3 = m_glslVersion == glu::GLSL_VERSION_300_ES;
    map<string, string> params;

    params["FRAG_HEADER"] = isGLSL3 ? "#version 300 es\nlayout(location = 0) out mediump vec4 dEQP_FragColor;\n" : "";
    params["VTX_HEADER"]  = isGLSL3 ? "#version 300 es\n" : "";
    params["VTX_IN"]      = isGLSL3 ? "in" : "attribute";
    params["VTX_OUT"]     = isGLSL3 ? "out" : "varying";
    params["FRAG_IN"]     = isGLSL3 ? "in" : "varying";
    params["FRAG_COLOR"]  = isGLSL3 ? "dEQP_FragColor" : "gl_FragColor";
    params["TEXTURE_2D_FUNC"] = isGLSL3 ? "texture" : "texture2D";
    params["NS"]              = "${NS}"; // \note Keep these as-is, they're handled by StressCase.

    params.insert(additionalParams.begin(), additionalParams.end());

    return tcu::StringTemplate(templ.c_str()).specialize(params);
}

string ProgramLibrary::substitute(const std::string &templ) const
{
    return substitute(templ, map<string, string>());
}

ProgramLibrary::ProgramLibrary(const glu::GLSLVersion glslVersion) : m_glslVersion(glslVersion)
{
    DE_ASSERT(glslVersion == glu::GLSL_VERSION_100_ES || glslVersion == glu::GLSL_VERSION_300_ES);
}

gls::ProgramContext ProgramLibrary::generateBufferContext(const int numUnusedAttributes) const
{
    static const char *const vertexTemplate = "${VTX_HEADER}"
                                              "${VTX_IN} highp vec3 a_position;\n"
                                              "${VTX_UNUSED_INPUTS}"
                                              "${VTX_OUT} mediump vec4 v_color;\n"
                                              "\n"
                                              "void main (void)\n"
                                              "{\n"
                                              "    gl_Position = vec4(a_position, 1.0);\n"
                                              "    v_color = ${VTX_COLOR_EXPRESSION};\n"
                                              "}\n";

    static const char *const fragmentTemplate = "${FRAG_HEADER}"
                                                "${FRAG_IN} mediump vec4 v_color;\n"
                                                "\n"
                                                "void main (void)\n"
                                                "{\n"
                                                "    ${FRAG_COLOR} = v_color;\n"
                                                "}\n";

    map<string, string> firstLevelParams;

    {
        string vtxUnusedInputs;
        string vtxColorExpr;
        for (int i = 0; i < numUnusedAttributes; i++)
        {
            vtxUnusedInputs += "${VTX_IN} mediump vec4 a_in" + toString(i) + ";\n";
            vtxColorExpr += string() + (i > 0 ? " + " : "") + "a_in" + toString(i);
        }

        firstLevelParams["VTX_UNUSED_INPUTS"]    = substitute(vtxUnusedInputs);
        firstLevelParams["VTX_COLOR_EXPRESSION"] = vtxColorExpr;
    }

    gls::ProgramContext context(substitute(vertexTemplate, firstLevelParams).c_str(),
                                substitute(fragmentTemplate).c_str(), "a_position");

    context.attributes.push_back(gls::VarSpec("a_position", Vec3(-0.1f), Vec3(0.1f)));

    for (int i = 0; i < numUnusedAttributes; i++)
        context.attributes.push_back(
            gls::VarSpec("a_in" + de::toString(i), Vec4(0.0f), Vec4(1.0f / (float)numUnusedAttributes)));

    return context;
}

gls::ProgramContext ProgramLibrary::generateTextureContext(const int numTextures, const int texWid, const int texHei,
                                                           const float positionFactor) const
{
    static const char *const vertexTemplate = "${VTX_HEADER}"
                                              "${VTX_IN} highp vec3 a_position;\n"
                                              "${VTX_IN} mediump vec2 a_texCoord;\n"
                                              "${VTX_OUT} mediump vec2 v_texCoord;\n"
                                              "uniform mediump mat4 u_posTrans;\n"
                                              "\n"
                                              "void main (void)\n"
                                              "{\n"
                                              "    gl_Position = u_posTrans * vec4(a_position, 1.0);\n"
                                              "    v_texCoord = a_texCoord;\n"
                                              "}\n";

    static const char *const fragmentTemplate = "${FRAG_HEADER}"
                                                "${FRAG_IN} mediump vec2 v_texCoord;\n"
                                                "uniform mediump sampler2D u_sampler;\n"
                                                "\n"
                                                "void main (void)\n"
                                                "{\n"
                                                "    ${FRAG_COLOR} = ${TEXTURE_2D_FUNC}(u_sampler, v_texCoord);\n"
                                                "}\n";

    gls::ProgramContext context(substitute(vertexTemplate).c_str(), substitute(fragmentTemplate).c_str(), "a_position");

    context.attributes.push_back(gls::VarSpec("a_position", Vec3(-positionFactor), Vec3(positionFactor)));
    context.attributes.push_back(gls::VarSpec("a_texCoord", Vec2(0.0f), Vec2(1.0f)));

    context.uniforms.push_back(gls::VarSpec("u_sampler", 0));
    context.uniforms.push_back(
        gls::VarSpec("u_posTrans", translationMat<4>(positionFactor - 1.0f), translationMat<4>(1.0f - positionFactor)));

    for (int i = 0; i < numTextures; i++)
        context.textureSpecs.push_back(gls::TextureSpec(
            glu::TextureTestUtil::TEXTURETYPE_2D, 0, texWid, texHei, GL_RGBA, GL_UNSIGNED_BYTE, GL_RGBA, true,
            GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT, Vec4(0.0f), Vec4(1.0f)));

    return context;
}

gls::ProgramContext ProgramLibrary::generateBufferAndTextureContext(const int numTextures, const int texWid,
                                                                    const int texHei) const
{
    static const char *const vertexTemplate = "${VTX_HEADER}"
                                              "${VTX_IN} highp vec3 a_position;\n"
                                              "${VTX_TEX_COORD_INPUTS}"
                                              "${VTX_TEX_COORD_OUTPUTS}"
                                              "\n"
                                              "void main (void)\n"
                                              "{\n"
                                              "    gl_Position = vec4(a_position, 1.0);\n"
                                              "${VTX_TEX_COORD_WRITES}"
                                              "}\n";

    static const char *const fragmentTemplate = "${FRAG_HEADER}"
                                                "${FRAG_TEX_COORD_INPUTS}"
                                                "${FRAG_SAMPLERS}"
                                                "\n"
                                                "void main (void)\n"
                                                "{\n"
                                                "    ${FRAG_COLOR} =${FRAG_COLOR_EXPRESSION};\n"
                                                "}\n";

    map<string, string> firstLevelParams;

    {
        string vtxTexCoordInputs;
        string vtxTexCoordOutputs;
        string vtxTexCoordWrites;
        string fragTexCoordInputs;
        string fragSamplers;
        string fragColorExpression;

        for (int i = 0; i < numTextures; i++)
        {
            vtxTexCoordInputs += "${VTX_IN} mediump vec2 a_texCoord" + toString(i) + ";\n";
            vtxTexCoordOutputs += "${VTX_OUT} mediump vec2 v_texCoord" + toString(i) + ";\n";
            vtxTexCoordWrites += "\tv_texCoord" + toString(i) + " = " + "a_texCoord" + toString(i) + ";\n";
            fragTexCoordInputs += "${FRAG_IN} mediump vec2 v_texCoord" + toString(i) + ";\n";
            fragSamplers += "uniform mediump sampler2D u_sampler" + toString(i) + ";\n";
            fragColorExpression += string() + (i > 0 ? " +" : "") + "\n\t\t${TEXTURE_2D_FUNC}(u_sampler" + toString(i) +
                                   ", v_texCoord" + toString(i) + ")";
        }

        firstLevelParams["VTX_TEX_COORD_INPUTS"]  = substitute(vtxTexCoordInputs);
        firstLevelParams["VTX_TEX_COORD_OUTPUTS"] = substitute(vtxTexCoordOutputs);
        firstLevelParams["VTX_TEX_COORD_WRITES"]  = vtxTexCoordWrites;
        firstLevelParams["FRAG_TEX_COORD_INPUTS"] = substitute(fragTexCoordInputs);
        firstLevelParams["FRAG_SAMPLERS"]         = fragSamplers;
        firstLevelParams["FRAG_COLOR_EXPRESSION"] = substitute(fragColorExpression);
    }

    gls::ProgramContext context(substitute(vertexTemplate, firstLevelParams).c_str(),
                                substitute(fragmentTemplate, firstLevelParams).c_str(), "a_position");

    context.attributes.push_back(gls::VarSpec("a_position", Vec3(-0.1f), Vec3(0.1f)));

    for (int i = 0; i < numTextures; i++)
    {
        context.attributes.push_back(gls::VarSpec("a_texCoord" + de::toString(i), Vec2(0.0f), Vec2(1.0f)));
        context.uniforms.push_back(gls::VarSpec("u_sampler" + de::toString(i), i));
        context.textureSpecs.push_back(gls::TextureSpec(
            glu::TextureTestUtil::TEXTURETYPE_2D, i, texWid, texHei, GL_RGBA, GL_UNSIGNED_BYTE, GL_RGBA, true,
            GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT, Vec4(0.0f), Vec4(1.0f / (float)numTextures)));
    }

    return context;
}

gls::ProgramContext ProgramLibrary::generateFragmentPointLightContext(const int texWid, const int texHei) const
{
    static const char *const vertexTemplate =
        "${VTX_HEADER}"
        "struct Material\n"
        "{\n"
        "    mediump vec3    ambientColor;\n"
        "    mediump vec4    diffuseColor;\n"
        "    mediump vec3    emissiveColor;\n"
        "    mediump vec3    specularColor;\n"
        "    mediump float    shininess;\n"
        "};\n"
        "\n"
        "struct Light\n"
        "{\n"
        "    mediump vec3    color;\n"
        "    mediump vec4    position;\n"
        "    mediump vec3    direction;\n"
        "    mediump float    constantAttenuation;\n"
        "    mediump float    linearAttenuation;\n"
        "    mediump float    quadraticAttenuation;\n"
        "};\n"
        "\n"
        "${VTX_IN} highp vec4    a_position${NS};\n"
        "${VTX_IN} mediump vec3    a_normal${NS};\n"
        "${VTX_IN} mediump vec3    a_color${NS};\n"
        "${VTX_IN} mediump vec4    a_texCoord0${NS};\n"
        "\n"
        "uniform Material        u_material${NS};\n"
        "uniform Light            u_light${NS}[1];\n"
        "uniform highp mat4        u_mvpMatrix${NS};\n"
        "uniform mediump mat4    u_modelViewMatrix${NS};\n"
        "uniform mediump mat3    u_normalMatrix${NS};\n"
        "uniform mediump mat4    u_texCoordMatrix0${NS};\n"
        "\n"
        "${VTX_OUT} mediump vec4    v_baseColor${NS};\n"
        "${VTX_OUT} mediump vec2    v_texCoord0${NS};\n"
        "\n"
        "${VTX_OUT} mediump vec3    v_eyeNormal${NS};\n"
        "${VTX_OUT} mediump vec3    v_directionToLight${NS}[1];\n"
        "${VTX_OUT} mediump float    v_distanceToLight${NS}[1];\n"
        "\n"
        "vec3 direction (vec4 from, vec4 to)\n"
        "{\n"
        "    return vec3(to.xyz * from.w - from.xyz * to.w);\n"
        "}\n"
        "\n"
        "void main (void)\n"
        "{\n"
        "    gl_Position = u_mvpMatrix${NS} * a_position${NS};\n"
        "    v_texCoord0${NS} = (u_texCoordMatrix0${NS} * a_texCoord0${NS}).xy;\n"
        "\n"
        "    mediump vec4 eyePosition = u_modelViewMatrix${NS} * a_position${NS};\n"
        "    mediump vec3 eyeNormal = normalize(u_normalMatrix${NS} * a_normal${NS});\n"
        "\n"
        "    vec4 color     = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "    color.rgb    += u_material${NS}.emissiveColor;\n"
        "\n"
        "    color.a        *= u_material${NS}.diffuseColor.a;\n"
        "\n"
        "    v_baseColor${NS} = color;\n"
        "\n"
        "    v_distanceToLight${NS}[0] = distance(eyePosition, u_light${NS}[0].position);\n"
        "    v_directionToLight${NS}[0] = normalize(direction(eyePosition, u_light${NS}[0].position));\n"
        "\n"
        "    v_eyeNormal${NS} = eyeNormal;\n"
        "}\n";

    static const char *const fragmentTemplate =
        "${FRAG_HEADER}"
        "struct Light\n"
        "{\n"
        "    mediump vec3    color;\n"
        "    mediump vec4    position;\n"
        "    mediump vec3    direction;\n"
        "    mediump float    constantAttenuation;\n"
        "    mediump float    linearAttenuation;\n"
        "    mediump float    quadraticAttenuation;\n"
        "};\n"
        "\n"
        "struct Material\n"
        "{\n"
        "    mediump vec3    ambientColor;\n"
        "    mediump vec4    diffuseColor;\n"
        "    mediump vec3    emissiveColor;\n"
        "    mediump vec3    specularColor;\n"
        "    mediump float    shininess;\n"
        "};\n"
        "\n"
        "uniform sampler2D        u_sampler0${NS};\n"
        "uniform Light            u_light${NS}[1];\n"
        "uniform Material        u_material${NS};\n"
        "\n"
        "${FRAG_IN} mediump vec4    v_baseColor${NS};\n"
        "${FRAG_IN} mediump vec2    v_texCoord0${NS};\n"
        "\n"
        "${FRAG_IN} mediump vec3    v_eyeNormal${NS};\n"
        "${FRAG_IN} mediump vec3    v_directionToLight${NS}[1];\n"
        "${FRAG_IN} mediump float    v_distanceToLight${NS}[1];\n"
        "\n"
        "mediump vec3 computeLighting (Light light, mediump vec3 directionToLight, mediump vec3 vertexEyeNormal)\n"
        "{\n"
        "    mediump float    normalDotDirection = max(dot(vertexEyeNormal, directionToLight), 0.0);\n"
        "    mediump    vec3    color = normalDotDirection * u_material${NS}.diffuseColor.rgb * light.color;\n"
        "\n"
        "    if (normalDotDirection != 0.0)\n"
        "    {\n"
        "        mediump vec3 h = normalize(directionToLight + vec3(0.0, 0.0, 1.0));\n"
        "        color.rgb += pow(max(dot(vertexEyeNormal, h), 0.0), u_material${NS}.shininess) * "
        "u_material${NS}.specularColor * light.color;\n"
        "    }\n"
        "\n"
        "    return color;\n"
        "}\n"
        "\n"
        "mediump float computePointLightAttenuation (Light light, mediump float distanceToLight)\n"
        "{\n"
        "    mediump float    constantAttenuation = light.constantAttenuation;\n"
        "    mediump float    linearAttenuation = light.linearAttenuation * distanceToLight;\n"
        "    mediump float    quadraticAttenuation = light.quadraticAttenuation * distanceToLight * distanceToLight;\n"
        "\n"
        "    return 1.0 / (constantAttenuation + linearAttenuation + quadraticAttenuation);\n"
        "}\n"
        "\n"
        "void main (void)\n"
        "{\n"
        "    mediump vec3 eyeNormal = normalize(v_eyeNormal${NS});\n"
        "    mediump vec4 color = v_baseColor${NS};\n"
        "\n"
        "    color.rgb += computePointLightAttenuation(u_light${NS}[0], v_distanceToLight${NS}[0]) * "
        "computeLighting(u_light${NS}[0], normalize(v_directionToLight${NS}[0]), eyeNormal);\n"
        "\n"
        "    color *= ${TEXTURE_2D_FUNC}(u_sampler0${NS}, v_texCoord0${NS});\n"
        "\n"
        "    ${FRAG_COLOR} = color;\n"
        "}\n";

    gls::ProgramContext context(substitute(vertexTemplate).c_str(), substitute(fragmentTemplate).c_str(),
                                "a_position${NS}");

    context.attributes.push_back(gls::VarSpec("a_position${NS}", Vec4(-1.0f), Vec4(1.0f)));
    context.attributes.push_back(gls::VarSpec("a_normal${NS}", Vec3(-1.0f), Vec3(1.0f)));
    context.attributes.push_back(gls::VarSpec("a_texCoord0${NS}", Vec4(-1.0f), Vec4(1.0f)));

    context.uniforms.push_back(gls::VarSpec("u_material${NS}.ambientColor", Vec3(0.0f), Vec3(1.0f)));
    context.uniforms.push_back(gls::VarSpec("u_material${NS}.diffuseColor", Vec4(0.0f), Vec4(1.0f)));
    context.uniforms.push_back(gls::VarSpec("u_material${NS}.emissiveColor", Vec3(0.0f), Vec3(1.0f)));
    context.uniforms.push_back(gls::VarSpec("u_material${NS}.specularColor", Vec3(0.0f), Vec3(1.0f)));
    context.uniforms.push_back(gls::VarSpec("u_material${NS}.shininess", 0.0f, 1.0f));

    context.uniforms.push_back(gls::VarSpec("u_light${NS}[0].color", Vec3(0.0f), Vec3(1.0f)));
    context.uniforms.push_back(gls::VarSpec("u_light${NS}[0].position", Vec4(-1.0f), Vec4(1.0f)));
    context.uniforms.push_back(gls::VarSpec("u_light${NS}[0].direction", Vec3(-1.0f), Vec3(1.0f)));
    context.uniforms.push_back(gls::VarSpec("u_light${NS}[0].constantAttenuation", 0.1f, 1.0f));
    context.uniforms.push_back(gls::VarSpec("u_light${NS}[0].linearAttenuation", 0.1f, 1.0f));
    context.uniforms.push_back(gls::VarSpec("u_light${NS}[0].quadraticAttenuation", 0.1f, 1.0f));

    context.uniforms.push_back(gls::VarSpec("u_mvpMatrix${NS}", translationMat<4>(-0.2f), translationMat<4>(0.2f)));
    context.uniforms.push_back(
        gls::VarSpec("u_modelViewMatrix${NS}", translationMat<4>(-0.2f), translationMat<4>(0.2f)));
    context.uniforms.push_back(gls::VarSpec("u_normalMatrix${NS}", translationMat<3>(-0.2f), translationMat<3>(0.2f)));
    context.uniforms.push_back(
        gls::VarSpec("u_texCoordMatrix0${NS}", translationMat<4>(-0.2f), translationMat<4>(0.2f)));

    context.uniforms.push_back(gls::VarSpec("u_sampler0${NS}", 0));

    context.textureSpecs.push_back(gls::TextureSpec(glu::TextureTestUtil::TEXTURETYPE_2D, 0, texWid, texHei, GL_RGBA,
                                                    GL_UNSIGNED_BYTE, GL_RGBA, true, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR,
                                                    GL_REPEAT, GL_REPEAT, Vec4(0.0f), Vec4(1.0f)));

    return context;
}

gls::ProgramContext ProgramLibrary::generateVertexUniformLoopLightContext(const int texWid, const int texHei) const
{
    static const char *const vertexTemplate =
        "${VTX_HEADER}"
        "struct Material {\n"
        "    mediump vec3 ambientColor;\n"
        "    mediump vec4 diffuseColor;\n"
        "    mediump vec3 emissiveColor;\n"
        "    mediump vec3 specularColor;\n"
        "    mediump float shininess;\n"
        "};\n"
        "struct Light {\n"
        "    mediump vec3 color;\n"
        "    mediump vec4 position;\n"
        "    mediump vec3 direction;\n"
        "    mediump float constantAttenuation;\n"
        "    mediump float linearAttenuation;\n"
        "    mediump float quadraticAttenuation;\n"
        "    mediump float spotExponent;\n"
        "    mediump float spotCutoff;\n"
        "};\n"
        "${VTX_IN} highp vec4 a_position${NS};\n"
        "${VTX_IN} mediump vec3 a_normal${NS};\n"
        "${VTX_IN} mediump vec4 a_texCoord0${NS};\n"
        "uniform Material u_material${NS};\n"
        "uniform Light u_directionalLight${NS}[1];\n"
        "uniform mediump int u_directionalLightCount${NS};\n"
        "uniform Light u_spotLight${NS}[4];\n"
        "uniform mediump int u_spotLightCount${NS};\n"
        "uniform highp mat4 u_mvpMatrix${NS};\n"
        "uniform highp mat4 u_modelViewMatrix${NS};\n"
        "uniform mediump mat3 u_normalMatrix${NS};\n"
        "uniform mediump mat4 u_texCoordMatrix0${NS};\n"
        "${VTX_OUT} mediump vec4 v_color${NS};\n"
        "${VTX_OUT} mediump vec2 v_texCoord0${NS};\n"
        "mediump vec3 direction (mediump vec4 from, mediump vec4 to)\n"
        "{\n"
        "    return vec3(to.xyz * from.w - from.xyz * to.w);\n"
        "}\n"
        "\n"
        "mediump vec3 computeLighting (\n"
        "    mediump vec3 directionToLight,\n"
        "    mediump vec3 halfVector,\n"
        "    mediump vec3 normal,\n"
        "    mediump vec3 lightColor,\n"
        "    mediump vec3 diffuseColor,\n"
        "    mediump vec3 specularColor,\n"
        "    mediump float shininess)\n"
        "{\n"
        "    mediump float normalDotDirection  = max(dot(normal, directionToLight), 0.0);\n"
        "    mediump vec3  color               = normalDotDirection * diffuseColor * lightColor;\n"
        "\n"
        "    if (normalDotDirection != 0.0)\n"
        "        color += pow(max(dot(normal, halfVector), 0.0), shininess) * specularColor * lightColor;\n"
        "\n"
        "    return color;\n"
        "}\n"
        "\n"
        "mediump float computeDistanceAttenuation (mediump float distToLight, mediump float constAtt, mediump float "
        "linearAtt, mediump float quadraticAtt)\n"
        "{\n"
        "    return 1.0 / (constAtt + linearAtt * distToLight + quadraticAtt * distToLight * distToLight);\n"
        "}\n"
        "\n"
        "mediump float computeSpotAttenuation (\n"
        "    mediump vec3  directionToLight,\n"
        "    mediump vec3  lightDir,\n"
        "    mediump float spotExponent,\n"
        "    mediump float spotCutoff)\n"
        "{\n"
        "    mediump float spotEffect = dot(lightDir, normalize(-directionToLight));\n"
        "\n"
        "    if (spotEffect < spotCutoff)\n"
        "        spotEffect = 0.0;\n"
        "\n"
        "    spotEffect = pow(spotEffect, spotExponent);\n"
        "    return spotEffect;\n"
        "}\n"
        "\n"
        "void main (void)\n"
        "{\n"
        "    highp vec4 position = a_position${NS};\n"
        "    highp vec3 normal = a_normal${NS};\n"
        "    gl_Position = u_mvpMatrix${NS} * position;\n"
        "    v_texCoord0${NS} = (u_texCoordMatrix0${NS} * a_texCoord0${NS}).xy;\n"
        "    mediump vec4 color = vec4(u_material${NS}.emissiveColor, u_material${NS}.diffuseColor.a);\n"
        "\n"
        "    highp vec4 eyePosition = u_modelViewMatrix${NS} * position;\n"
        "    mediump vec3 eyeNormal = normalize(u_normalMatrix${NS} * normal);\n"
        "    for (int i = 0; i < u_directionalLightCount${NS}; i++)\n"
        "    {\n"
        "        mediump vec3 directionToLight = -u_directionalLight${NS}[i].direction;\n"
        "        mediump vec3 halfVector = normalize(directionToLight + vec3(0.0, 0.0, 1.0));\n"
        "        color.rgb += computeLighting(directionToLight, halfVector, eyeNormal, "
        "u_directionalLight${NS}[i].color, u_material${NS}.diffuseColor.rgb, u_material${NS}.specularColor, "
        "u_material${NS}.shininess);\n"
        "    }\n"
        "\n"
        "    for (int i = 0; i < u_spotLightCount${NS}; i++)\n"
        "    {\n"
        "        mediump float distanceToLight = distance(eyePosition, u_spotLight${NS}[i].position);\n"
        "        mediump vec3 directionToLight = normalize(direction(eyePosition, u_spotLight${NS}[i].position));\n"
        "        mediump vec3 halfVector = normalize(directionToLight + vec3(0.0, 0.0, 1.0));\n"
        "        color.rgb += computeLighting(directionToLight, halfVector, eyeNormal, u_spotLight${NS}[i].color, "
        "u_material${NS}.diffuseColor.rgb, u_material${NS}.specularColor, u_material${NS}.shininess) * "
        "computeDistanceAttenuation(distanceToLight, u_spotLight${NS}[i].constantAttenuation, "
        "u_spotLight${NS}[i].linearAttenuation, u_spotLight${NS}[i].quadraticAttenuation) * "
        "computeSpotAttenuation(directionToLight, u_spotLight${NS}[i].direction, u_spotLight${NS}[i].spotExponent, "
        "u_spotLight${NS}[i].spotCutoff);\n"
        "    }\n"
        "\n"
        "\n"
        "    v_color${NS} = color;\n"
        "}\n";

    static const char *const fragmentTemplate = "${FRAG_HEADER}"
                                                "uniform sampler2D u_sampler0${NS};\n"
                                                "${FRAG_IN} mediump vec4 v_color${NS};\n"
                                                "${FRAG_IN} mediump vec2 v_texCoord0${NS};\n"
                                                "void main (void)\n"
                                                "{\n"
                                                "    mediump vec2 texCoord0 = v_texCoord0${NS};\n"
                                                "    mediump vec4 color = v_color${NS};\n"
                                                "    color *= ${TEXTURE_2D_FUNC}(u_sampler0${NS}, texCoord0);\n"
                                                "    ${FRAG_COLOR} = color;\n"
                                                "}\n";

    gls::ProgramContext context(substitute(vertexTemplate).c_str(), substitute(fragmentTemplate).c_str(),
                                "a_position${NS}");

    context.attributes.push_back(gls::VarSpec("a_position${NS}", Vec4(-1.0f), Vec4(1.0f)));
    context.attributes.push_back(gls::VarSpec("a_normal${NS}", Vec3(-1.0f), Vec3(1.0f)));
    context.attributes.push_back(gls::VarSpec("a_texCoord0${NS}", Vec4(-1.0f), Vec4(1.0f)));

    context.uniforms.push_back(gls::VarSpec("u_material${NS}.ambientColor", Vec3(0.0f), Vec3(1.0f)));
    context.uniforms.push_back(gls::VarSpec("u_material${NS}.diffuseColor", Vec4(0.0f), Vec4(1.0f)));
    context.uniforms.push_back(gls::VarSpec("u_material${NS}.emissiveColor", Vec3(0.0f), Vec3(1.0f)));
    context.uniforms.push_back(gls::VarSpec("u_material${NS}.specularColor", Vec3(0.0f), Vec3(1.0f)));
    context.uniforms.push_back(gls::VarSpec("u_material${NS}.shininess", 0.0f, 1.0f));

    context.uniforms.push_back(gls::VarSpec("u_directionalLight${NS}[0].color", Vec3(0.0f), Vec3(1.0f)));
    context.uniforms.push_back(gls::VarSpec("u_directionalLight${NS}[0].position", Vec4(-1.0f), Vec4(1.0f)));
    context.uniforms.push_back(gls::VarSpec("u_directionalLight${NS}[0].direction", Vec3(-1.0f), Vec3(1.0f)));
    context.uniforms.push_back(gls::VarSpec("u_directionalLight${NS}[0].constantAttenuation", 0.1f, 1.0f));
    context.uniforms.push_back(gls::VarSpec("u_directionalLight${NS}[0].linearAttenuation", 0.1f, 1.0f));
    context.uniforms.push_back(gls::VarSpec("u_directionalLight${NS}[0].quadraticAttenuation", 0.1f, 1.0f));
    context.uniforms.push_back(gls::VarSpec("u_directionalLight${NS}[0].spotExponent", 0.1f, 1.0f));
    context.uniforms.push_back(gls::VarSpec("u_directionalLight${NS}[0].spotCutoff", 0.1f, 1.0f));

    context.uniforms.push_back(gls::VarSpec("u_directionalLightCount${NS}", 1));

    for (int i = 0; i < 4; i++)
    {
        const std::string ndxStr = de::toString(i);

        context.uniforms.push_back(gls::VarSpec("u_spotLight${NS}[" + ndxStr + "].color", Vec3(0.0f), Vec3(1.0f)));
        context.uniforms.push_back(gls::VarSpec("u_spotLight${NS}[" + ndxStr + "].position", Vec4(-1.0f), Vec4(1.0f)));
        context.uniforms.push_back(gls::VarSpec("u_spotLight${NS}[" + ndxStr + "].direction", Vec3(-1.0f), Vec3(1.0f)));
        context.uniforms.push_back(gls::VarSpec("u_spotLight${NS}[" + ndxStr + "].constantAttenuation", 0.1f, 1.0f));
        context.uniforms.push_back(gls::VarSpec("u_spotLight${NS}[" + ndxStr + "].linearAttenuation", 0.1f, 1.0f));
        context.uniforms.push_back(gls::VarSpec("u_spotLight${NS}[" + ndxStr + "].quadraticAttenuation", 0.1f, 1.0f));
        context.uniforms.push_back(gls::VarSpec("u_spotLight${NS}[" + ndxStr + "].spotExponent", 0.1f, 1.0f));
        context.uniforms.push_back(gls::VarSpec("u_spotLight${NS}[" + ndxStr + "].spotCutoff", 0.1f, 1.0f));
    }

    context.uniforms.push_back(gls::VarSpec("u_spotLightCount${NS}", 4));

    context.uniforms.push_back(gls::VarSpec("u_mvpMatrix${NS}", translationMat<4>(-0.2f), translationMat<4>(0.2f)));
    context.uniforms.push_back(
        gls::VarSpec("u_modelViewMatrix${NS}", translationMat<4>(-0.2f), translationMat<4>(0.2f)));
    context.uniforms.push_back(gls::VarSpec("u_normalMatrix${NS}", translationMat<3>(-0.2f), translationMat<3>(0.2f)));
    context.uniforms.push_back(
        gls::VarSpec("u_texCoordMatrix0${NS}", translationMat<4>(-0.2f), translationMat<4>(0.2f)));

    context.uniforms.push_back(gls::VarSpec("u_sampler0${NS}", 0));

    context.textureSpecs.push_back(gls::TextureSpec(glu::TextureTestUtil::TEXTURETYPE_2D, 0, texWid, texHei, GL_RGBA,
                                                    GL_UNSIGNED_BYTE, GL_RGBA, true, GL_LINEAR, GL_LINEAR, GL_REPEAT,
                                                    GL_REPEAT, Vec4(0.0f), Vec4(1.0f)));

    return context;
}

} // namespace LongStressTestUtil
} // namespace gls
} // namespace deqp
