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
 * \brief Compiler test case.
 *//*--------------------------------------------------------------------*/

#include "glsShaderLibraryCase.hpp"

#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuSurface.hpp"

#include "tcuStringTemplate.hpp"
#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "gluDrawUtil.hpp"
#include "gluContextInfo.hpp"
#include "gluStrUtil.hpp"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include "deRandom.hpp"
#include "deInt32.h"
#include "deMath.h"
#include "deString.h"
#include "deStringUtil.hpp"
#include "deSharedPtr.hpp"

#include <map>
#include <vector>
#include <string>
#include <sstream>

namespace deqp
{
namespace gls
{

using namespace tcu;
using namespace glu;
using namespace glu::sl;

using std::map;
using std::ostringstream;
using std::pair;
using std::string;
using std::vector;

using de::SharedPtr;

// OpenGL-specific specialization utils

static vector<RequiredExtension> checkAndSpecializeExtensions(const vector<RequiredExtension> &src,
                                                              const ContextInfo &ctxInfo)
{
    vector<RequiredExtension> specialized;

    for (size_t extNdx = 0; extNdx < src.size(); ++extNdx)
    {
        const RequiredExtension &extension = src[extNdx];
        int supportedAltNdx                = -1;

        for (size_t alternativeNdx = 0; alternativeNdx < extension.alternatives.size(); ++alternativeNdx)
        {
            if (ctxInfo.isExtensionSupported(extension.alternatives[alternativeNdx].c_str()))
            {
                supportedAltNdx = (int)alternativeNdx;
                break;
            }
        }

        if (supportedAltNdx >= 0)
        {
            specialized.push_back(
                RequiredExtension(extension.alternatives[supportedAltNdx], extension.effectiveStages));
        }
        else
        {
            // no extension(s). Make a nice output
            std::ostringstream extensionList;

            for (size_t ndx = 0; ndx < extension.alternatives.size(); ++ndx)
            {
                if (!extensionList.str().empty())
                    extensionList << ", ";
                extensionList << extension.alternatives[ndx];
            }

            if (extension.alternatives.size() == 1)
                throw tcu::NotSupportedError("Test requires extension " + extensionList.str());
            else
                throw tcu::NotSupportedError("Test requires any extension of " + extensionList.str());
        }
    }

    return specialized;
}

static void checkImplementationLimits(const vector<RequiredCapability> &requiredCaps, const ContextInfo &ctxInfo)
{
    for (size_t capNdx = 0; capNdx < requiredCaps.size(); ++capNdx)
    {
        const RequiredCapability &capability = requiredCaps[capNdx];
        if (capability.type != CAPABILITY_LIMIT)
            continue;

        const uint32_t pname     = capability.enumName;
        const int requiredValue  = capability.referenceValue;
        const int supportedValue = ctxInfo.getInt((int)pname);

        if (supportedValue <= requiredValue)
            throw tcu::NotSupportedError("Test requires " + de::toString(glu::getGettableStateStr(pname)) + " (" +
                                         de::toString(supportedValue) + ") >= " + de::toString(requiredValue));
    }
}

// Shader source specialization

// This functions builds a matching vertex shader for a 'both' case, when
// the fragment shader is being tested.
// We need to build attributes and varyings for each 'input'.
static string genVertexShader(const ShaderCaseSpecification &spec)
{
    ostringstream res;
    const bool usesInout     = glslVersionUsesInOutQualifiers(spec.targetVersion);
    const char *const vtxIn  = usesInout ? "in" : "attribute";
    const char *const vtxOut = usesInout ? "out" : "varying";

    res << glu::getGLSLVersionDeclaration(spec.targetVersion) << "\n";

    // Declarations (position + attribute/varying for each input).
    res << "precision highp float;\n";
    res << "precision highp int;\n";
    res << "\n";
    res << vtxIn << " highp vec4 dEQP_Position;\n";

    for (size_t ndx = 0; ndx < spec.values.inputs.size(); ndx++)
    {
        const Value &val          = spec.values.inputs[ndx];
        const DataType basicType  = val.type.getBasicType();
        const DataType floatType  = getDataTypeFloatScalars(basicType);
        const char *const typeStr = getDataTypeName(floatType);

        res << vtxIn << " " << typeStr << " a_" << val.name << ";\n";

        if (getDataTypeScalarType(basicType) == TYPE_FLOAT)
            res << vtxOut << " " << typeStr << " " << val.name << ";\n";
        else
            res << vtxOut << " " << typeStr << " v_" << val.name << ";\n";
    }
    res << "\n";

    // Main function.
    // - gl_Position = dEQP_Position;
    // - for each input: write attribute directly to varying
    res << "void main()\n";
    res << "{\n";
    res << "    gl_Position = dEQP_Position;\n";
    for (size_t ndx = 0; ndx < spec.values.inputs.size(); ndx++)
    {
        const Value &val   = spec.values.inputs[ndx];
        const string &name = val.name;

        if (getDataTypeScalarType(val.type.getBasicType()) == TYPE_FLOAT)
            res << "    " << name << " = a_" << name << ";\n";
        else
            res << "    v_" << name << " = a_" << name << ";\n";
    }

    res << "}\n";
    return res.str();
}

static void genCompareOp(ostringstream &output, const char *dstVec4Var, const ValueBlock &valueBlock,
                         const char *nonFloatNamePrefix, const char *checkVarName)
{
    bool isFirstOutput = true;

    for (size_t ndx = 0; ndx < valueBlock.outputs.size(); ndx++)
    {
        const Value &val = valueBlock.outputs[ndx];

        // Check if we're only interested in one variable (then skip if not the right one).
        if (checkVarName && val.name != checkVarName)
            continue;

        // Prefix.
        if (isFirstOutput)
        {
            output << "bool RES = ";
            isFirstOutput = false;
        }
        else
            output << "RES = RES && ";

        // Generate actual comparison.
        if (getDataTypeScalarType(val.type.getBasicType()) == TYPE_FLOAT)
            output << "isOk(" << val.name << ", ref_" << val.name << ", 0.05);\n";
        else
            output << "isOk(" << nonFloatNamePrefix << val.name << ", ref_" << val.name << ");\n";
    }

    if (isFirstOutput)
        output << dstVec4Var << " = vec4(1.0);\n"; // \todo [petri] Should we give warning if not expect-failure case?
    else
        output << dstVec4Var << " = vec4(RES, RES, RES, 1.0);\n";
}

static inline bool supportsFragmentHighp(glu::GLSLVersion version)
{
    return version != glu::GLSL_VERSION_100_ES;
}

static string genFragmentShader(const ShaderCaseSpecification &spec)
{
    ostringstream shader;
    const bool usesInout      = glslVersionUsesInOutQualifiers(spec.targetVersion);
    const bool customColorOut = usesInout;
    const char *const fragIn  = usesInout ? "in" : "varying";
    const char *const prec    = supportsFragmentHighp(spec.targetVersion) ? "highp" : "mediump";

    shader << glu::getGLSLVersionDeclaration(spec.targetVersion) << "\n";

    shader << "precision " << prec << " float;\n";
    shader << "precision " << prec << " int;\n";
    shader << "\n";

    if (customColorOut)
    {
        shader << "layout(location = 0) out mediump vec4 dEQP_FragColor;\n";
        shader << "\n";
    }

    genCompareFunctions(shader, spec.values, true);
    shader << "\n";

    // Declarations (varying, reference for each output).
    for (size_t ndx = 0; ndx < spec.values.outputs.size(); ndx++)
    {
        const Value &val               = spec.values.outputs[ndx];
        const DataType basicType       = val.type.getBasicType();
        const DataType floatType       = getDataTypeFloatScalars(basicType);
        const char *const floatTypeStr = getDataTypeName(floatType);
        const char *const refTypeStr   = getDataTypeName(basicType);

        if (getDataTypeScalarType(basicType) == TYPE_FLOAT)
            shader << fragIn << " " << floatTypeStr << " " << val.name << ";\n";
        else
            shader << fragIn << " " << floatTypeStr << " v_" << val.name << ";\n";

        shader << "uniform " << refTypeStr << " ref_" << val.name << ";\n";
    }

    shader << "\n";
    shader << "void main()\n";
    shader << "{\n";

    shader << "    ";
    genCompareOp(shader, customColorOut ? "dEQP_FragColor" : "gl_FragColor", spec.values, "v_", nullptr);

    shader << "}\n";
    return shader.str();
}

// Specialize a shader for the vertex shader test case.
static string specializeVertexShader(const ShaderCaseSpecification &spec, const std::string &src,
                                     const vector<RequiredExtension> &extensions)
{
    ostringstream decl;
    ostringstream setup;
    ostringstream output;
    const bool usesInout     = glslVersionUsesInOutQualifiers(spec.targetVersion);
    const char *const vtxIn  = usesInout ? "in" : "attribute";
    const char *const vtxOut = usesInout ? "out" : "varying";

    // generated from "both" case
    DE_ASSERT(spec.caseType == CASETYPE_VERTEX_ONLY);

    // Output (write out position).
    output << "gl_Position = dEQP_Position;\n";

    // Declarations (position + attribute for each input, varying for each output).
    decl << vtxIn << " highp vec4 dEQP_Position;\n";

    for (size_t ndx = 0; ndx < spec.values.inputs.size(); ndx++)
    {
        const Value &val               = spec.values.inputs[ndx];
        const DataType basicType       = val.type.getBasicType();
        const DataType floatType       = getDataTypeFloatScalars(basicType);
        const char *const floatTypeStr = getDataTypeName(floatType);
        const char *const refTypeStr   = getDataTypeName(basicType);

        if (getDataTypeScalarType(basicType) == TYPE_FLOAT)
        {
            decl << vtxIn << " " << floatTypeStr << " " << val.name << ";\n";
        }
        else
        {
            decl << vtxIn << " " << floatTypeStr << " a_" << val.name << ";\n";
            setup << refTypeStr << " " << val.name << " = " << refTypeStr << "(a_" << val.name << ");\n";
        }
    }

    // \todo [2015-07-24 pyry] Why are uniforms missing?

    for (size_t ndx = 0; ndx < spec.values.outputs.size(); ndx++)
    {
        const Value &val               = spec.values.outputs[ndx];
        const DataType basicType       = val.type.getBasicType();
        const DataType floatType       = getDataTypeFloatScalars(basicType);
        const char *const floatTypeStr = getDataTypeName(floatType);
        const char *const refTypeStr   = getDataTypeName(basicType);

        if (getDataTypeScalarType(basicType) == TYPE_FLOAT)
            decl << vtxOut << " " << floatTypeStr << " " << val.name << ";\n";
        else
        {
            decl << vtxOut << " " << floatTypeStr << " v_" << val.name << ";\n";
            decl << refTypeStr << " " << val.name << ";\n";

            output << "v_" << val.name << " = " << floatTypeStr << "(" << val.name << ");\n";
        }
    }

    // Shader specialization.
    map<string, string> params;
    params.insert(pair<string, string>("DECLARATIONS", decl.str()));
    params.insert(pair<string, string>("SETUP", setup.str()));
    params.insert(pair<string, string>("OUTPUT", output.str()));
    params.insert(pair<string, string>("POSITION_FRAG_COLOR", "gl_Position"));

    StringTemplate tmpl(src);
    const string baseSrc = tmpl.specialize(params);
    const string withExt = injectExtensionRequirements(baseSrc, extensions, SHADERTYPE_VERTEX);

    return withExt;
}

// Specialize a shader for the fragment shader test case.
static string specializeFragmentShader(const ShaderCaseSpecification &spec, const std::string &src,
                                       const vector<RequiredExtension> &extensions)
{
    ostringstream decl;
    ostringstream setup;
    ostringstream output;

    const bool usesInout        = glslVersionUsesInOutQualifiers(spec.targetVersion);
    const bool customColorOut   = usesInout;
    const char *const fragIn    = usesInout ? "in" : "varying";
    const char *const fragColor = customColorOut ? "dEQP_FragColor" : "gl_FragColor";

    // generated from "both" case
    DE_ASSERT(spec.caseType == CASETYPE_FRAGMENT_ONLY);

    genCompareFunctions(decl, spec.values, false);
    genCompareOp(output, fragColor, spec.values, "", nullptr);

    if (customColorOut)
        decl << "layout(location = 0) out mediump vec4 dEQP_FragColor;\n";

    for (size_t ndx = 0; ndx < spec.values.inputs.size(); ndx++)
    {
        const Value &val               = spec.values.inputs[ndx];
        const DataType basicType       = val.type.getBasicType();
        const DataType floatType       = getDataTypeFloatScalars(basicType);
        const char *const floatTypeStr = getDataTypeName(floatType);
        const char *const refTypeStr   = getDataTypeName(basicType);

        if (getDataTypeScalarType(basicType) == TYPE_FLOAT)
            decl << fragIn << " " << floatTypeStr << " " << val.name << ";\n";
        else
        {
            decl << fragIn << " " << floatTypeStr << " v_" << val.name << ";\n";
            std::string offset =
                isDataTypeIntOrIVec(basicType) ?
                    " * 1.0025" :
                    ""; // \todo [petri] bit of a hack to avoid errors in chop() due to varying interpolation
            setup << refTypeStr << " " << val.name << " = " << refTypeStr << "(v_" << val.name << offset << ");\n";
        }
    }

    // \todo [2015-07-24 pyry] Why are uniforms missing?

    for (size_t ndx = 0; ndx < spec.values.outputs.size(); ndx++)
    {
        const Value &val             = spec.values.outputs[ndx];
        const DataType basicType     = val.type.getBasicType();
        const char *const refTypeStr = getDataTypeName(basicType);

        decl << "uniform " << refTypeStr << " ref_" << val.name << ";\n";
        decl << refTypeStr << " " << val.name << ";\n";
    }

    /* \todo [2010-04-01 petri] Check all outputs. */

    // Shader specialization.
    map<string, string> params;
    params.insert(pair<string, string>("DECLARATIONS", decl.str()));
    params.insert(pair<string, string>("SETUP", setup.str()));
    params.insert(pair<string, string>("OUTPUT", output.str()));
    params.insert(pair<string, string>("POSITION_FRAG_COLOR", fragColor));

    StringTemplate tmpl(src);
    const string baseSrc = tmpl.specialize(params);
    const string withExt = injectExtensionRequirements(baseSrc, extensions, SHADERTYPE_FRAGMENT);

    return withExt;
}

static void generateUniformDeclarations(std::ostream &dst, const ValueBlock &valueBlock)
{
    for (size_t ndx = 0; ndx < valueBlock.uniforms.size(); ndx++)
    {
        const Value &val          = valueBlock.uniforms[ndx];
        const char *const typeStr = getDataTypeName(val.type.getBasicType());

        if (val.name.find('.') == string::npos)
            dst << "uniform " << typeStr << " " << val.name << ";\n";
    }
}

static map<string, string> generateVertexSpecialization(const ProgramSpecializationParams &specParams)
{
    const bool usesInout = glslVersionUsesInOutQualifiers(specParams.caseSpec.targetVersion);
    const char *vtxIn    = usesInout ? "in" : "attribute";
    ostringstream decl;
    ostringstream setup;
    map<string, string> params;

    decl << vtxIn << " highp vec4 dEQP_Position;\n";

    for (size_t ndx = 0; ndx < specParams.caseSpec.values.inputs.size(); ndx++)
    {
        const Value &val          = specParams.caseSpec.values.inputs[ndx];
        const DataType basicType  = val.type.getBasicType();
        const char *const typeStr = getDataTypeName(val.type.getBasicType());

        if (getDataTypeScalarType(basicType) == TYPE_FLOAT)
        {
            decl << vtxIn << " " << typeStr << " " << val.name << ";\n";
        }
        else
        {
            const DataType floatType       = getDataTypeFloatScalars(basicType);
            const char *const floatTypeStr = getDataTypeName(floatType);

            decl << vtxIn << " " << floatTypeStr << " a_" << val.name << ";\n";
            setup << typeStr << " " << val.name << " = " << typeStr << "(a_" << val.name << ");\n";
        }
    }

    generateUniformDeclarations(decl, specParams.caseSpec.values);

    params.insert(pair<string, string>("VERTEX_DECLARATIONS", decl.str()));
    params.insert(pair<string, string>("VERTEX_SETUP", setup.str()));
    params.insert(pair<string, string>("VERTEX_OUTPUT", string("gl_Position = dEQP_Position;\n")));

    return params;
}

static map<string, string> generateFragmentSpecialization(const ProgramSpecializationParams &specParams)
{
    const bool usesInout        = glslVersionUsesInOutQualifiers(specParams.caseSpec.targetVersion);
    const bool customColorOut   = usesInout;
    const char *const fragColor = customColorOut ? "dEQP_FragColor" : "gl_FragColor";
    ostringstream decl;
    ostringstream output;
    map<string, string> params;

    genCompareFunctions(decl, specParams.caseSpec.values, false);
    genCompareOp(output, fragColor, specParams.caseSpec.values, "", nullptr);

    if (customColorOut)
        decl << "layout(location = 0) out mediump vec4 dEQP_FragColor;\n";

    for (size_t ndx = 0; ndx < specParams.caseSpec.values.outputs.size(); ndx++)
    {
        const Value &val             = specParams.caseSpec.values.outputs[ndx];
        const char *const refTypeStr = getDataTypeName(val.type.getBasicType());

        decl << "uniform " << refTypeStr << " ref_" << val.name << ";\n";
        decl << refTypeStr << " " << val.name << ";\n";
    }

    generateUniformDeclarations(decl, specParams.caseSpec.values);

    params.insert(pair<string, string>("FRAGMENT_DECLARATIONS", decl.str()));
    params.insert(pair<string, string>("FRAGMENT_OUTPUT", output.str()));
    params.insert(pair<string, string>("FRAG_COLOR", fragColor));

    return params;
}

static map<string, string> generateGeometrySpecialization(const ProgramSpecializationParams &specParams)
{
    ostringstream decl;
    map<string, string> params;

    decl << "layout (triangles) in;\n";
    decl << "layout (triangle_strip, max_vertices=3) out;\n";
    decl << "\n";

    generateUniformDeclarations(decl, specParams.caseSpec.values);

    params.insert(pair<string, string>("GEOMETRY_DECLARATIONS", decl.str()));

    return params;
}

static map<string, string> generateTessControlSpecialization(const ProgramSpecializationParams &specParams)
{
    ostringstream decl;
    ostringstream output;
    map<string, string> params;

    decl << "layout (vertices=3) out;\n";
    decl << "\n";

    generateUniformDeclarations(decl, specParams.caseSpec.values);

    output << "gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
              "gl_TessLevelInner[0] = 2.0;\n"
              "gl_TessLevelInner[1] = 2.0;\n"
              "gl_TessLevelOuter[0] = 2.0;\n"
              "gl_TessLevelOuter[1] = 2.0;\n"
              "gl_TessLevelOuter[2] = 2.0;\n"
              "gl_TessLevelOuter[3] = 2.0;";

    params.insert(pair<string, string>("TESSELLATION_CONTROL_DECLARATIONS", decl.str()));
    params.insert(pair<string, string>("TESSELLATION_CONTROL_OUTPUT", output.str()));
    params.insert(pair<string, string>("GL_MAX_PATCH_VERTICES", de::toString(specParams.maxPatchVertices)));

    return params;
}

static map<string, string> generateTessEvalSpecialization(const ProgramSpecializationParams &specParams)
{
    ostringstream decl;
    ostringstream output;
    map<string, string> params;

    decl << "layout (triangles) in;\n";
    decl << "\n";

    generateUniformDeclarations(decl, specParams.caseSpec.values);

    output << "gl_Position = gl_TessCoord[0] * gl_in[0].gl_Position + gl_TessCoord[1] * gl_in[1].gl_Position + "
              "gl_TessCoord[2] * gl_in[2].gl_Position;\n";

    params.insert(pair<string, string>("TESSELLATION_EVALUATION_DECLARATIONS", decl.str()));
    params.insert(pair<string, string>("TESSELLATION_EVALUATION_OUTPUT", output.str()));
    params.insert(pair<string, string>("GL_MAX_PATCH_VERTICES", de::toString(specParams.maxPatchVertices)));

    return params;
}

static void specializeShaderSources(
    ProgramSources &dst, const ProgramSources &src, const ProgramSpecializationParams &specParams,
    glu::ShaderType shaderType,
    map<string, string> (*specializationGenerator)(const ProgramSpecializationParams &specParams))
{
    if (!src.sources[shaderType].empty())
    {
        const map<string, string> tmplParams = specializationGenerator(specParams);

        for (size_t ndx = 0; ndx < src.sources[shaderType].size(); ++ndx)
        {
            const StringTemplate tmpl(src.sources[shaderType][ndx]);
            const std::string baseGLSLCode = tmpl.specialize(tmplParams);
            const std::string sourceWithExts =
                injectExtensionRequirements(baseGLSLCode, specParams.requiredExtensions, shaderType);

            dst << glu::ShaderSource(shaderType, sourceWithExts);
        }
    }
}

static void specializeProgramSources(glu::ProgramSources &dst, const glu::ProgramSources &src,
                                     const ProgramSpecializationParams &specParams)
{
    specializeShaderSources(dst, src, specParams, SHADERTYPE_VERTEX, generateVertexSpecialization);
    specializeShaderSources(dst, src, specParams, SHADERTYPE_FRAGMENT, generateFragmentSpecialization);
    specializeShaderSources(dst, src, specParams, SHADERTYPE_GEOMETRY, generateGeometrySpecialization);
    specializeShaderSources(dst, src, specParams, SHADERTYPE_TESSELLATION_CONTROL, generateTessControlSpecialization);
    specializeShaderSources(dst, src, specParams, SHADERTYPE_TESSELLATION_EVALUATION, generateTessEvalSpecialization);

    dst << ProgramSeparable(src.separable);
}

enum
{
    VIEWPORT_WIDTH  = 128,
    VIEWPORT_HEIGHT = 128
};

class BeforeDrawValidator : public glu::DrawUtilCallback
{
public:
    enum TargetType
    {
        TARGETTYPE_PROGRAM = 0,
        TARGETTYPE_PIPELINE,

        TARGETTYPE_LAST
    };

    BeforeDrawValidator(const glw::Functions &gl, glw::GLuint target, TargetType targetType);

    void beforeDrawCall(void);

    const std::string &getInfoLog(void) const;
    glw::GLint getValidateStatus(void) const;

private:
    const glw::Functions &m_gl;
    const glw::GLuint m_target;
    const TargetType m_targetType;

    glw::GLint m_validateStatus;
    std::string m_logMessage;
};

BeforeDrawValidator::BeforeDrawValidator(const glw::Functions &gl, glw::GLuint target, TargetType targetType)
    : m_gl(gl)
    , m_target(target)
    , m_targetType(targetType)
    , m_validateStatus(-1)
{
    DE_ASSERT(targetType < TARGETTYPE_LAST);
}

void BeforeDrawValidator::beforeDrawCall(void)
{
    glw::GLint bytesWritten = 0;
    glw::GLint infoLogLength;
    std::vector<glw::GLchar> logBuffer;
    int stringLength;

    // validate
    if (m_targetType == TARGETTYPE_PROGRAM)
        m_gl.validateProgram(m_target);
    else if (m_targetType == TARGETTYPE_PIPELINE)
        m_gl.validateProgramPipeline(m_target);
    else
        DE_ASSERT(false);

    GLU_EXPECT_NO_ERROR(m_gl.getError(), "validate");

    // check status
    m_validateStatus = -1;

    if (m_targetType == TARGETTYPE_PROGRAM)
        m_gl.getProgramiv(m_target, GL_VALIDATE_STATUS, &m_validateStatus);
    else if (m_targetType == TARGETTYPE_PIPELINE)
        m_gl.getProgramPipelineiv(m_target, GL_VALIDATE_STATUS, &m_validateStatus);
    else
        DE_ASSERT(false);

    GLU_EXPECT_NO_ERROR(m_gl.getError(), "get validate status");
    TCU_CHECK(m_validateStatus == GL_TRUE || m_validateStatus == GL_FALSE);

    // read log

    infoLogLength = 0;

    if (m_targetType == TARGETTYPE_PROGRAM)
        m_gl.getProgramiv(m_target, GL_INFO_LOG_LENGTH, &infoLogLength);
    else if (m_targetType == TARGETTYPE_PIPELINE)
        m_gl.getProgramPipelineiv(m_target, GL_INFO_LOG_LENGTH, &infoLogLength);
    else
        DE_ASSERT(false);

    GLU_EXPECT_NO_ERROR(m_gl.getError(), "get info log length");

    if (infoLogLength <= 0)
    {
        m_logMessage.clear();
        return;
    }

    logBuffer.resize(
        infoLogLength + 2,
        '0'); // +1 for zero terminator (infoLogLength should include it, but better play it safe), +1 to make sure buffer is always larger

    if (m_targetType == TARGETTYPE_PROGRAM)
        m_gl.getProgramInfoLog(m_target, infoLogLength + 1, &bytesWritten, &logBuffer[0]);
    else if (m_targetType == TARGETTYPE_PIPELINE)
        m_gl.getProgramPipelineInfoLog(m_target, infoLogLength + 1, &bytesWritten, &logBuffer[0]);
    else
        DE_ASSERT(false);

    // just ignore bytesWritten to be safe, find the null terminator
    stringLength = (int)(std::find(logBuffer.begin(), logBuffer.end(), '0') - logBuffer.begin());
    m_logMessage.assign(&logBuffer[0], stringLength);
}

const std::string &BeforeDrawValidator::getInfoLog(void) const
{
    return m_logMessage;
}

glw::GLint BeforeDrawValidator::getValidateStatus(void) const
{
    return m_validateStatus;
}

// ShaderCase.

ShaderLibraryCase::ShaderLibraryCase(tcu::TestContext &testCtx, RenderContext &renderCtx,
                                     const glu::ContextInfo &contextInfo, const char *name, const char *description,
                                     const ShaderCaseSpecification &specification)
    : tcu::TestCase(testCtx, name, description)
    , m_renderCtx(renderCtx)
    , m_contextInfo(contextInfo)
    , m_spec(specification)
{
}

ShaderLibraryCase::~ShaderLibraryCase(void)
{
}

static inline void requireExtension(const glu::ContextInfo &info, const ShaderCaseSpecification &spec,
                                    const char *extension)
{
    if (!info.isExtensionSupported(extension))
        TCU_THROW(NotSupportedError, (string(getGLSLVersionName(spec.targetVersion)) + " is not supported").c_str());
}

void ShaderLibraryCase::init(void)
{
    DE_ASSERT(isValid(m_spec));

    // Check for ES compatibility extensions, e.g. if we are on desktop context but require GLSL ES
    if (!isContextTypeES(m_renderCtx.getType()) && glslVersionIsES(m_spec.targetVersion))
    {
        switch (m_spec.targetVersion)
        {
        case GLSL_VERSION_300_ES:
            requireExtension(m_contextInfo, m_spec, "GL_ARB_ES3_compatibility");
            break;
        case GLSL_VERSION_310_ES:
            requireExtension(m_contextInfo, m_spec, "GL_ARB_ES3_1_compatibility");
            break;
        case GLSL_VERSION_320_ES:
            requireExtension(m_contextInfo, m_spec, "GL_ARB_ES3_2_compatibility");
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else
    {
        if (!isGLSLVersionSupported(m_renderCtx.getType(), m_spec.targetVersion))
            TCU_THROW(NotSupportedError,
                      (string(getGLSLVersionName(m_spec.targetVersion)) + " is not supported").c_str());
    }

    checkImplementationLimits(m_spec.requiredCaps, m_contextInfo);

    // log the expected result
    switch (m_spec.expectResult)
    {
    case EXPECT_PASS:
        // Don't write anything
        break;

    case EXPECT_COMPILE_FAIL:
        m_testCtx.getLog() << tcu::TestLog::Message << "Expecting shader compilation to fail."
                           << tcu::TestLog::EndMessage;
        break;

    case EXPECT_LINK_FAIL:
        m_testCtx.getLog() << tcu::TestLog::Message << "Expecting program linking to fail." << tcu::TestLog::EndMessage;
        break;

    case EXPECT_COMPILE_LINK_FAIL:
        m_testCtx.getLog() << tcu::TestLog::Message << "Expecting either shader compilation or program linking to fail."
                           << tcu::TestLog::EndMessage;
        break;

    case EXPECT_VALIDATION_FAIL:
        m_testCtx.getLog() << tcu::TestLog::Message << "Expecting program validation to fail."
                           << tcu::TestLog::EndMessage;
        break;

    case EXPECT_BUILD_SUCCESSFUL:
        m_testCtx.getLog()
            << tcu::TestLog::Message
            << "Expecting shader compilation and program linking to succeed. Resulting program will not be executed."
            << tcu::TestLog::EndMessage;
        break;

    default:
        DE_ASSERT(false);
        break;
    }
}

static void setUniformValue(const glw::Functions &gl, const std::vector<uint32_t> &pipelinePrograms,
                            const std::string &name, const Value &val, int arrayNdx, tcu::TestLog &log)
{
    bool foundAnyMatch = false;

    for (int programNdx = 0; programNdx < (int)pipelinePrograms.size(); ++programNdx)
    {
        const DataType dataType = val.type.getBasicType();
        const int scalarSize    = getDataTypeScalarSize(dataType);
        const int loc           = gl.getUniformLocation(pipelinePrograms[programNdx], name.c_str());
        const int elemNdx       = arrayNdx * scalarSize;

        DE_ASSERT(elemNdx + scalarSize <= (int)val.elements.size());

        if (loc == -1)
            continue;

        foundAnyMatch = true;

        DE_STATIC_ASSERT(sizeof(Value::Element) == sizeof(glw::GLfloat));
        DE_STATIC_ASSERT(sizeof(Value::Element) == sizeof(glw::GLint));

        gl.useProgram(pipelinePrograms[programNdx]);

        switch (dataType)
        {
        case TYPE_FLOAT:
            gl.uniform1fv(loc, 1, &val.elements[elemNdx].float32);
            break;
        case TYPE_FLOAT_VEC2:
            gl.uniform2fv(loc, 1, &val.elements[elemNdx].float32);
            break;
        case TYPE_FLOAT_VEC3:
            gl.uniform3fv(loc, 1, &val.elements[elemNdx].float32);
            break;
        case TYPE_FLOAT_VEC4:
            gl.uniform4fv(loc, 1, &val.elements[elemNdx].float32);
            break;
        case TYPE_FLOAT_MAT2:
            gl.uniformMatrix2fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);
            break;
        case TYPE_FLOAT_MAT3:
            gl.uniformMatrix3fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);
            break;
        case TYPE_FLOAT_MAT4:
            gl.uniformMatrix4fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);
            break;
        case TYPE_INT:
            gl.uniform1iv(loc, 1, &val.elements[elemNdx].int32);
            break;
        case TYPE_INT_VEC2:
            gl.uniform2iv(loc, 1, &val.elements[elemNdx].int32);
            break;
        case TYPE_INT_VEC3:
            gl.uniform3iv(loc, 1, &val.elements[elemNdx].int32);
            break;
        case TYPE_INT_VEC4:
            gl.uniform4iv(loc, 1, &val.elements[elemNdx].int32);
            break;
        case TYPE_BOOL:
            gl.uniform1iv(loc, 1, &val.elements[elemNdx].int32);
            break;
        case TYPE_BOOL_VEC2:
            gl.uniform2iv(loc, 1, &val.elements[elemNdx].int32);
            break;
        case TYPE_BOOL_VEC3:
            gl.uniform3iv(loc, 1, &val.elements[elemNdx].int32);
            break;
        case TYPE_BOOL_VEC4:
            gl.uniform4iv(loc, 1, &val.elements[elemNdx].int32);
            break;
        case TYPE_UINT:
            gl.uniform1uiv(loc, 1, (const uint32_t *)&val.elements[elemNdx].int32);
            break;
        case TYPE_UINT_VEC2:
            gl.uniform2uiv(loc, 1, (const uint32_t *)&val.elements[elemNdx].int32);
            break;
        case TYPE_UINT_VEC3:
            gl.uniform3uiv(loc, 1, (const uint32_t *)&val.elements[elemNdx].int32);
            break;
        case TYPE_UINT_VEC4:
            gl.uniform4uiv(loc, 1, (const uint32_t *)&val.elements[elemNdx].int32);
            break;
        case TYPE_FLOAT_MAT2X3:
            gl.uniformMatrix2x3fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);
            break;
        case TYPE_FLOAT_MAT2X4:
            gl.uniformMatrix2x4fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);
            break;
        case TYPE_FLOAT_MAT3X2:
            gl.uniformMatrix3x2fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);
            break;
        case TYPE_FLOAT_MAT3X4:
            gl.uniformMatrix3x4fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);
            break;
        case TYPE_FLOAT_MAT4X2:
            gl.uniformMatrix4x2fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);
            break;
        case TYPE_FLOAT_MAT4X3:
            gl.uniformMatrix4x3fv(loc, 1, GL_FALSE, &val.elements[elemNdx].float32);
            break;

        case TYPE_SAMPLER_2D:
        case TYPE_SAMPLER_CUBE:
            DE_FATAL("implement!");
            break;

        default:
            DE_ASSERT(false);
        }
    }

    if (!foundAnyMatch)
        log << tcu::TestLog::Message << "WARNING // Uniform \"" << name
            << "\" location is not valid, location = -1. Cannot set value to the uniform." << tcu::TestLog::EndMessage;
}

static bool isTessellationPresent(const ShaderCaseSpecification &spec)
{
    if (spec.programs[0].sources.separable)
    {
        const uint32_t tessellationBits =
            (1 << glu::SHADERTYPE_TESSELLATION_CONTROL) | (1 << glu::SHADERTYPE_TESSELLATION_EVALUATION);

        for (int programNdx = 0; programNdx < (int)spec.programs.size(); ++programNdx)
            if (spec.programs[programNdx].activeStages & tessellationBits)
                return true;
        return false;
    }
    else
        return !spec.programs[0].sources.sources[glu::SHADERTYPE_TESSELLATION_CONTROL].empty() ||
               !spec.programs[0].sources.sources[glu::SHADERTYPE_TESSELLATION_EVALUATION].empty();
}

static bool isTessellationSupported(const glu::RenderContext &renderCtx, const glu::ContextInfo &ctxInfo)
{
    if (renderCtx.getType().getProfile() == PROFILE_ES)
    {
        const int majorVer = renderCtx.getType().getMajorVersion();
        const int minorVer = renderCtx.getType().getMinorVersion();

        return (majorVer > 3) || (majorVer == 3 && minorVer >= 2) ||
               ctxInfo.isExtensionSupported("GL_EXT_tessellation_shader");
    }
    else
        return false;
}

static bool checkPixels(tcu::TestLog &log, const tcu::ConstPixelBufferAccess &surface)
{
    bool allWhite      = true;
    bool allBlack      = true;
    bool anyUnexpected = false;

    for (int y = 0; y < surface.getHeight(); y++)
    {
        for (int x = 0; x < surface.getWidth(); x++)
        {
            const tcu::IVec4 pixel = surface.getPixelInt(x, y);
            // Note: we really do not want to involve alpha in the check comparison
            // \todo [2010-09-22 kalle] Do we know that alpha would be one? If yes, could use color constants white and black.
            const bool isWhite = (pixel[0] == 255) && (pixel[1] == 255) && (pixel[2] == 255);
            const bool isBlack = (pixel[0] == 0) && (pixel[1] == 0) && (pixel[2] == 0);

            allWhite      = allWhite && isWhite;
            allBlack      = allBlack && isBlack;
            anyUnexpected = anyUnexpected || (!isWhite && !isBlack);
        }
    }

    if (!allWhite)
    {
        if (anyUnexpected)
            log << TestLog::Message
                << "WARNING: expecting all rendered pixels to be white or black, but got other colors as well!"
                << TestLog::EndMessage;
        else if (!allBlack)
            log << TestLog::Message
                << "WARNING: got inconsistent results over the image, when all pixels should be the same color!"
                << TestLog::EndMessage;

        return false;
    }

    return true;
}

bool ShaderLibraryCase::execute(void)
{
    const float quadSize                  = 1.0f;
    static const float s_positions[4 * 4] = {-quadSize, -quadSize, 0.0f, 1.0f, -quadSize, +quadSize, 0.0f, 1.0f,
                                             +quadSize, -quadSize, 0.0f, 1.0f, +quadSize, +quadSize, 0.0f, 1.0f};

    static const uint16_t s_indices[2 * 3] = {0, 1, 2, 1, 3, 2};

    TestLog &log             = m_testCtx.getLog();
    const glw::Functions &gl = m_renderCtx.getFunctions();

    // Compute viewport.
    const tcu::RenderTarget &renderTarget = m_renderCtx.getRenderTarget();
    de::Random rnd(deStringHash(getName()));
    const int width                = deMin32(renderTarget.getWidth(), VIEWPORT_WIDTH);
    const int height               = deMin32(renderTarget.getHeight(), VIEWPORT_HEIGHT);
    const int viewportX            = rnd.getInt(0, renderTarget.getWidth() - width);
    const int viewportY            = rnd.getInt(0, renderTarget.getHeight() - height);
    const int numVerticesPerDraw   = 4;
    const bool tessellationPresent = isTessellationPresent(m_spec);
    const bool separablePrograms   = m_spec.programs[0].sources.separable;

    bool allCompilesOk     = true;
    bool allLinksOk        = true;
    const char *failReason = nullptr;

    vector<ProgramSources> specializedSources(m_spec.programs.size());

    uint32_t vertexProgramID = -1;
    vector<uint32_t> pipelineProgramIDs;
    vector<SharedPtr<ShaderProgram>> programs;
    SharedPtr<ProgramPipeline> programPipeline;

    GLU_EXPECT_NO_ERROR(gl.getError(), "ShaderCase::execute(): start");

    if (isCapabilityRequired(CAPABILITY_ONLY_GLSL_ES_100_SUPPORT, m_spec) && glu::IsES3Compatible(gl))
        return true;

    if (isCapabilityRequired(CAPABILITY_EXACTLY_ONE_DRAW_BUFFER, m_spec))
    {
        // on unextended ES2 there is only one draw buffer
        // and there is no GL_MAX_DRAW_BUFFERS query
        glw::GLint maxDrawBuffers = 0;
        gl.getIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
        if ((gl.getError() == GL_NO_ERROR) && (maxDrawBuffers > 1))
            throw tcu::NotSupportedError("Test requires exactly one draw buffer");
    }

    // Specialize shaders
    if (m_spec.caseType == CASETYPE_VERTEX_ONLY)
    {
        const vector<RequiredExtension> reqExt =
            checkAndSpecializeExtensions(m_spec.programs[0].requiredExtensions, m_contextInfo);

        DE_ASSERT(m_spec.programs.size() == 1 && m_spec.programs[0].sources.sources[SHADERTYPE_VERTEX].size() == 1);
        specializedSources[0] << glu::VertexSource(specializeVertexShader(
                                     m_spec, m_spec.programs[0].sources.sources[SHADERTYPE_VERTEX][0], reqExt))
                              << glu::FragmentSource(genFragmentShader(m_spec));
    }
    else if (m_spec.caseType == CASETYPE_FRAGMENT_ONLY)
    {
        const vector<RequiredExtension> reqExt =
            checkAndSpecializeExtensions(m_spec.programs[0].requiredExtensions, m_contextInfo);

        DE_ASSERT(m_spec.programs.size() == 1 && m_spec.programs[0].sources.sources[SHADERTYPE_FRAGMENT].size() == 1);
        specializedSources[0] << glu::VertexSource(genVertexShader(m_spec))
                              << glu::FragmentSource(specializeFragmentShader(
                                     m_spec, m_spec.programs[0].sources.sources[SHADERTYPE_FRAGMENT][0], reqExt));
    }
    else
    {
        DE_ASSERT(m_spec.caseType == CASETYPE_COMPLETE);

        const int maxPatchVertices =
            isTessellationPresent(m_spec) && isTessellationSupported(m_renderCtx, m_contextInfo) ?
                m_contextInfo.getInt(GL_MAX_PATCH_VERTICES) :
                0;

        for (size_t progNdx = 0; progNdx < m_spec.programs.size(); progNdx++)
        {
            const ProgramSpecializationParams progSpecParams(
                m_spec, checkAndSpecializeExtensions(m_spec.programs[progNdx].requiredExtensions, m_contextInfo),
                maxPatchVertices);

            specializeProgramSources(specializedSources[progNdx], m_spec.programs[progNdx].sources, progSpecParams);
        }
    }

    if (!separablePrograms)
    {
        de::SharedPtr<glu::ShaderProgram> program(new glu::ShaderProgram(m_renderCtx, specializedSources[0]));

        vertexProgramID = program->getProgram();
        pipelineProgramIDs.push_back(program->getProgram());
        programs.push_back(program);

        // Check that compile/link results are what we expect.

        DE_STATIC_ASSERT(glu::SHADERTYPE_VERTEX == 0);
        for (int stage = glu::SHADERTYPE_VERTEX; stage < glu::SHADERTYPE_LAST; ++stage)
            if (program->hasShader((glu::ShaderType)stage) && !program->getShaderInfo((glu::ShaderType)stage).compileOk)
                allCompilesOk = false;

        if (!program->getProgramInfo().linkOk)
            allLinksOk = false;

        log << *program;
    }
    else
    {
        // Separate programs
        for (size_t programNdx = 0; programNdx < m_spec.programs.size(); ++programNdx)
        {
            de::SharedPtr<glu::ShaderProgram> program(
                new glu::ShaderProgram(m_renderCtx, specializedSources[programNdx]));

            if (m_spec.programs[programNdx].activeStages & (1u << glu::SHADERTYPE_VERTEX))
                vertexProgramID = program->getProgram();

            pipelineProgramIDs.push_back(program->getProgram());
            programs.push_back(program);

            // Check that compile/link results are what we expect.

            DE_STATIC_ASSERT(glu::SHADERTYPE_VERTEX == 0);
            for (int stage = glu::SHADERTYPE_VERTEX; stage < glu::SHADERTYPE_LAST; ++stage)
                if (program->hasShader((glu::ShaderType)stage) &&
                    !program->getShaderInfo((glu::ShaderType)stage).compileOk)
                    allCompilesOk = false;

            if (!program->getProgramInfo().linkOk)
                allLinksOk = false;

            // Log program and active stages
            {
                const tcu::ScopedLogSection section(log, "Program", "Program " + de::toString(programNdx + 1));
                tcu::MessageBuilder builder(&log);
                bool firstStage = true;

                builder << "Pipeline uses stages: ";
                for (int stage = glu::SHADERTYPE_VERTEX; stage < glu::SHADERTYPE_LAST; ++stage)
                {
                    if (m_spec.programs[programNdx].activeStages & (1u << stage))
                    {
                        if (!firstStage)
                            builder << ", ";
                        builder << glu::getShaderTypeName((glu::ShaderType)stage);
                        firstStage = true;
                    }
                }
                builder << tcu::TestLog::EndMessage;

                log << *program;
            }
        }
    }

    switch (m_spec.expectResult)
    {
    case EXPECT_PASS:
    case EXPECT_VALIDATION_FAIL:
    case EXPECT_BUILD_SUCCESSFUL:
        if (!allCompilesOk)
            failReason = "expected shaders to compile and link properly, but failed to compile.";
        else if (!allLinksOk)
            failReason = "expected shaders to compile and link properly, but failed to link.";
        break;

    case EXPECT_COMPILE_FAIL:
        if (allCompilesOk && !allLinksOk)
            failReason = "expected compilation to fail, but shaders compiled and link failed.";
        else if (allCompilesOk)
            failReason = "expected compilation to fail, but shaders compiled correctly.";
        break;

    case EXPECT_LINK_FAIL:
        if (!allCompilesOk)
            failReason = "expected linking to fail, but unable to compile.";
        else if (allLinksOk)
            failReason = "expected linking to fail, but passed.";
        break;

    case EXPECT_COMPILE_LINK_FAIL:
        if (allCompilesOk && allLinksOk)
            failReason = "expected compile or link to fail, but passed.";
        break;

    default:
        DE_ASSERT(false);
        return false;
    }

    if (failReason != nullptr)
    {
        // \todo [2010-06-07 petri] These should be handled in the test case?
        log << TestLog::Message << "ERROR: " << failReason << TestLog::EndMessage;

        if (isCapabilityRequired(CAPABILITY_FULL_GLSL_ES_100_SUPPORT, m_spec))
        {
            log << TestLog::Message
                << "Assuming build failure is caused by implementation not supporting full GLSL ES 100 specification, "
                   "which is not required."
                << TestLog::EndMessage;

            if (allCompilesOk && !allLinksOk)
            {
                // Used features are detectable at compile time. If implementation parses shader
                // at link time, report it as quality warning.
                m_testCtx.setTestResult(QP_TEST_RESULT_QUALITY_WARNING, failReason);
            }
            else
                m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Full GLSL ES 100 is not supported");
        }
        else if (m_spec.expectResult == EXPECT_COMPILE_FAIL && allCompilesOk && !allLinksOk)
        {
            // If implementation parses shader at link time, report it as quality warning.
            m_testCtx.setTestResult(QP_TEST_RESULT_QUALITY_WARNING, failReason);
        }
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, failReason);
        return false;
    }

    // Return if shader is not intended to be run
    if (m_spec.expectResult == EXPECT_COMPILE_FAIL || m_spec.expectResult == EXPECT_COMPILE_LINK_FAIL ||
        m_spec.expectResult == EXPECT_LINK_FAIL || m_spec.expectResult == EXPECT_BUILD_SUCCESSFUL)
        return true;

    // Setup viewport.
    gl.viewport(viewportX, viewportY, width, height);

    if (separablePrograms)
    {
        programPipeline = de::SharedPtr<glu::ProgramPipeline>(new glu::ProgramPipeline(m_renderCtx));

        // Setup pipeline
        gl.bindProgramPipeline(programPipeline->getPipeline());
        for (int programNdx = 0; programNdx < (int)m_spec.programs.size(); ++programNdx)
        {
            uint32_t shaderFlags = 0;
            for (int stage = glu::SHADERTYPE_VERTEX; stage < glu::SHADERTYPE_LAST; ++stage)
                if (m_spec.programs[programNdx].activeStages & (1u << stage))
                    shaderFlags |= glu::getGLShaderTypeBit((glu::ShaderType)stage);

            programPipeline->useProgramStages(shaderFlags, pipelineProgramIDs[programNdx]);
        }

        programPipeline->activeShaderProgram(vertexProgramID);
        GLU_EXPECT_NO_ERROR(gl.getError(), "setup pipeline");
    }
    else
    {
        // Start using program
        gl.useProgram(vertexProgramID);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram()");
    }

    // Fetch location for positions positions.
    int positionLoc = gl.getAttribLocation(vertexProgramID, "dEQP_Position");
    if (positionLoc == -1)
    {
        string errStr = string("no location found for attribute 'dEQP_Position'");
        TCU_FAIL(errStr.c_str());
    }

    // Iterate all value blocks.
    {
        const ValueBlock &valueBlock = m_spec.values;

        // always render at least one pass even if there is no input/output data
        const int numRenderPasses = valueBlock.outputs.empty() ? 1 :
                                                                 (int)valueBlock.outputs[0].elements.size() /
                                                                     valueBlock.outputs[0].type.getScalarSize();

        // Iterate all array sub-cases.
        for (int arrayNdx = 0; arrayNdx < numRenderPasses; arrayNdx++)
        {
            vector<VertexArrayBinding> vertexArrays;
            int attribValueNdx = 0;
            vector<vector<float>> attribValues(valueBlock.inputs.size());
            glw::GLenum postDrawError;
            BeforeDrawValidator beforeDrawValidator(
                gl, (separablePrograms) ? (programPipeline->getPipeline()) : (vertexProgramID),
                (separablePrograms) ? (BeforeDrawValidator::TARGETTYPE_PIPELINE) :
                                      (BeforeDrawValidator::TARGETTYPE_PROGRAM));

            vertexArrays.push_back(va::Float(positionLoc, 4, numVerticesPerDraw, 0, &s_positions[0]));

            // Collect VA pointer for inputs
            for (size_t valNdx = 0; valNdx < valueBlock.inputs.size(); valNdx++)
            {
                const Value &val            = valueBlock.inputs[valNdx];
                const char *const valueName = val.name.c_str();
                const DataType dataType     = val.type.getBasicType();
                const int scalarSize        = getDataTypeScalarSize(dataType);

                // Replicate values four times.
                std::vector<float> &scalars = attribValues[attribValueNdx++];
                scalars.resize(numVerticesPerDraw * scalarSize);
                if (isDataTypeFloatOrVec(dataType) || isDataTypeMatrix(dataType))
                {
                    for (int repNdx = 0; repNdx < numVerticesPerDraw; repNdx++)
                        for (int ndx = 0; ndx < scalarSize; ndx++)
                            scalars[repNdx * scalarSize + ndx] = val.elements[arrayNdx * scalarSize + ndx].float32;
                }
                else
                {
                    // convert to floats.
                    for (int repNdx = 0; repNdx < numVerticesPerDraw; repNdx++)
                    {
                        for (int ndx = 0; ndx < scalarSize; ndx++)
                        {
                            float v = (float)val.elements[arrayNdx * scalarSize + ndx].int32;
                            DE_ASSERT(val.elements[arrayNdx * scalarSize + ndx].int32 == (int)v);
                            scalars[repNdx * scalarSize + ndx] = v;
                        }
                    }
                }

                // Attribute name prefix.
                string attribPrefix = "";
                // \todo [2010-05-27 petri] Should latter condition only apply for vertex cases (or actually non-fragment cases)?
                if ((m_spec.caseType == CASETYPE_FRAGMENT_ONLY) || (getDataTypeScalarType(dataType) != TYPE_FLOAT))
                    attribPrefix = "a_";

                // Input always given as attribute.
                string attribName = attribPrefix + valueName;
                int attribLoc     = gl.getAttribLocation(vertexProgramID, attribName.c_str());
                if (attribLoc == -1)
                {
                    log << TestLog::Message << "Warning: no location found for attribute '" << attribName << "'"
                        << TestLog::EndMessage;
                    continue;
                }

                if (isDataTypeMatrix(dataType))
                {
                    int numCols = getDataTypeMatrixNumColumns(dataType);
                    int numRows = getDataTypeMatrixNumRows(dataType);
                    DE_ASSERT(scalarSize == numCols * numRows);

                    for (int i = 0; i < numCols; i++)
                        vertexArrays.push_back(va::Float(attribLoc + i, numRows, numVerticesPerDraw,
                                                         scalarSize * (int)sizeof(float), &scalars[i * numRows]));
                }
                else
                {
                    DE_ASSERT(isDataTypeFloatOrVec(dataType) || isDataTypeIntOrIVec(dataType) ||
                              isDataTypeUintOrUVec(dataType) || isDataTypeBoolOrBVec(dataType));
                    vertexArrays.push_back(va::Float(attribLoc, scalarSize, numVerticesPerDraw, 0, &scalars[0]));
                }

                GLU_EXPECT_NO_ERROR(gl.getError(), "set vertex attrib array");
            }

            GLU_EXPECT_NO_ERROR(gl.getError(), "before set uniforms");

            // set reference values for outputs.
            for (size_t valNdx = 0; valNdx < valueBlock.outputs.size(); valNdx++)
            {
                const Value &val            = valueBlock.outputs[valNdx];
                const char *const valueName = val.name.c_str();

                // Set reference value.
                string refName = string("ref_") + valueName;
                setUniformValue(gl, pipelineProgramIDs, refName, val, arrayNdx, m_testCtx.getLog());
                GLU_EXPECT_NO_ERROR(gl.getError(), "set reference uniforms");
            }

            // set uniform values
            for (size_t valNdx = 0; valNdx < valueBlock.uniforms.size(); valNdx++)
            {
                const Value &val            = valueBlock.uniforms[valNdx];
                const char *const valueName = val.name.c_str();

                setUniformValue(gl, pipelineProgramIDs, valueName, val, arrayNdx, m_testCtx.getLog());
                GLU_EXPECT_NO_ERROR(gl.getError(), "set uniforms");
            }

            // Clear.
            gl.clearColor(0.125f, 0.25f, 0.5f, 1.0f);
            gl.clear(GL_COLOR_BUFFER_BIT);
            GLU_EXPECT_NO_ERROR(gl.getError(), "clear buffer");

            // Use program or pipeline
            if (separablePrograms)
                gl.useProgram(0);
            else
                gl.useProgram(vertexProgramID);

            // Draw.
            if (tessellationPresent)
            {
                gl.patchParameteri(GL_PATCH_VERTICES, 3);
                GLU_EXPECT_NO_ERROR(gl.getError(), "set patchParameteri(PATCH_VERTICES, 3)");
            }

            draw(m_renderCtx, vertexProgramID, (int)vertexArrays.size(), &vertexArrays[0],
                 (tessellationPresent) ? (pr::Patches(DE_LENGTH_OF_ARRAY(s_indices), &s_indices[0])) :
                                         (pr::Triangles(DE_LENGTH_OF_ARRAY(s_indices), &s_indices[0])),
                 (m_spec.expectResult == EXPECT_VALIDATION_FAIL) ? (&beforeDrawValidator) : (nullptr));

            postDrawError = gl.getError();

            if (m_spec.expectResult == EXPECT_PASS)
            {
                // Read back results.
                Surface surface(width, height);
                const float w  = s_positions[3];
                const int minY = deCeilFloatToInt32(((-quadSize / w) * 0.5f + 0.5f) * (float)height + 1.0f);
                const int maxY = deFloorFloatToInt32(((+quadSize / w) * 0.5f + 0.5f) * (float)height - 0.5f);
                const int minX = deCeilFloatToInt32(((-quadSize / w) * 0.5f + 0.5f) * (float)width + 1.0f);
                const int maxX = deFloorFloatToInt32(((+quadSize / w) * 0.5f + 0.5f) * (float)width - 0.5f);

                GLU_EXPECT_NO_ERROR(postDrawError, "draw");

                glu::readPixels(m_renderCtx, viewportX, viewportY, surface.getAccess());
                GLU_EXPECT_NO_ERROR(gl.getError(), "read pixels");

                if (!checkPixels(log,
                                 tcu::getSubregion(surface.getAccess(), minX, minY, maxX - minX + 1, maxY - minY + 1)))
                {
                    log << TestLog::Message << "INCORRECT RESULT for sub-case " << arrayNdx + 1 << " of "
                        << numRenderPasses << "):" << TestLog::EndMessage;

                    log << TestLog::Message << "Failing shader input/output values:" << TestLog::EndMessage;
                    dumpValues(log, valueBlock, arrayNdx);

                    // Dump image on failure.
                    log << TestLog::Image("Result", "Rendered result image", surface);

                    gl.useProgram(0);
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
                    return false;
                }
            }
            else if (m_spec.expectResult == EXPECT_VALIDATION_FAIL)
            {
                log << TestLog::Message << "Draw call generated error: " << glu::getErrorStr(postDrawError) << " "
                    << ((postDrawError == GL_INVALID_OPERATION) ? ("(expected)") : ("(unexpected)")) << "\n"
                    << "Validate status: " << glu::getBooleanStr(beforeDrawValidator.getValidateStatus()) << " "
                    << ((beforeDrawValidator.getValidateStatus() == GL_FALSE) ? ("(expected)") : ("(unexpected)"))
                    << "\n"
                    << "Info log: "
                    << ((beforeDrawValidator.getInfoLog().empty()) ? ("[empty string]") :
                                                                     (beforeDrawValidator.getInfoLog()))
                    << "\n"
                    << TestLog::EndMessage;

                // test result

                if (postDrawError != GL_NO_ERROR && postDrawError != GL_INVALID_OPERATION)
                {
                    m_testCtx.setTestResult(
                        QP_TEST_RESULT_FAIL,
                        ("Draw: got unexpected error: " + de::toString(glu::getErrorStr(postDrawError))).c_str());
                    return false;
                }

                if (beforeDrawValidator.getValidateStatus() == GL_TRUE)
                {
                    if (postDrawError == GL_NO_ERROR)
                        m_testCtx.setTestResult(
                            QP_TEST_RESULT_FAIL,
                            "expected validation and rendering to fail but validation and rendering succeeded");
                    else if (postDrawError == GL_INVALID_OPERATION)
                        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL,
                                                "expected validation and rendering to fail but validation succeeded "
                                                "(rendering failed as expected)");
                    else
                        DE_ASSERT(false);
                    return false;
                }
                else if (beforeDrawValidator.getValidateStatus() == GL_FALSE && postDrawError == GL_NO_ERROR)
                {
                    m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "expected validation and rendering to fail but "
                                                                 "rendering succeeded (validation failed as expected)");
                    return false;
                }
                else if (beforeDrawValidator.getValidateStatus() == GL_FALSE && postDrawError == GL_INVALID_OPERATION)
                {
                    // Validation does not depend on input values, no need to test all values
                    return true;
                }
                else
                    DE_ASSERT(false);
            }
            else
                DE_ASSERT(false);
        }
    }

    gl.useProgram(0);
    if (separablePrograms)
        gl.bindProgramPipeline(0);

    GLU_EXPECT_NO_ERROR(gl.getError(), "ShaderCase::execute(): end");
    return true;
}

TestCase::IterateResult ShaderLibraryCase::iterate(void)
{
    // Initialize state to pass.
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

    bool executeOk = execute();

    DE_ASSERT(executeOk ? m_testCtx.getTestResult() == QP_TEST_RESULT_PASS :
                          m_testCtx.getTestResult() != QP_TEST_RESULT_PASS);
    DE_UNREF(executeOk);
    return TestCase::STOP;
}

} // namespace gls
} // namespace deqp
