#ifndef _GLUSHADERLIBRARY_HPP
#define _GLUSHADERLIBRARY_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief Shader .test file utilities.
 *//*--------------------------------------------------------------------*/

#include "gluDefs.hpp"
#include "gluVarType.hpp"
#include "gluShaderProgram.hpp"
#include "tcuTestCase.hpp"

#include <string>
#include <vector>

namespace glu
{
namespace sl
{

enum CaseType
{
    CASETYPE_COMPLETE = 0,  //!< Has all shaders specified separately.
    CASETYPE_VERTEX_ONLY,   //!< "Both" case, vertex shader sub case.
    CASETYPE_FRAGMENT_ONLY, //!< "Both" case, fragment shader sub case.

    CASETYPE_LAST
};

enum ExpectResult
{
    EXPECT_PASS = 0,
    EXPECT_COMPILE_FAIL,
    EXPECT_LINK_FAIL,
    EXPECT_COMPILE_LINK_FAIL,
    EXPECT_VALIDATION_FAIL,
    EXPECT_BUILD_SUCCESSFUL,

    EXPECT_LAST
};

enum OutputType
{
    OUTPUT_RESULT = 0,
    OUTPUT_COLOR,

    OUTPUT_LAST
};

struct Value
{
    union Element
    {
        float float32;
        int32_t int32;
        int32_t bool32;
    };

    VarType type;
    std::string name;
    std::vector<Element> elements; // Scalar values (variable.varType.getScalarSize() * #values).
};

struct ValueBlock
{
    std::vector<Value> inputs;
    std::vector<Value> outputs;
    std::vector<Value> uniforms;
};

enum CapabilityType
{
    CAPABILITY_LIMIT = 0,
    CAPABILITY_FLAG,

    CAPABILITY_LAST
};

enum CapabilityFlag
{
    CAPABILITY_FULL_GLSL_ES_100_SUPPORT,
    CAPABILITY_ONLY_GLSL_ES_100_SUPPORT, // only ES2, no ES3 capability
    CAPABILITY_EXACTLY_ONE_DRAW_BUFFER   // gl_MaxDrawBuffers is exactly 1
};

struct RequiredCapability
{
    CapabilityType type;

    union
    {
        CapabilityFlag flagName;
        uint32_t enumName;
    };

    int referenceValue;

    RequiredCapability(CapabilityFlag flagName_)
        : type(CAPABILITY_FLAG)
        , flagName(flagName_)
        , referenceValue(0) // not used
    {
    }

    RequiredCapability(uint32_t enumName_, int referenceValue_)
        : type(CAPABILITY_LIMIT)
        , enumName(enumName_)
        , referenceValue(referenceValue_)
    {
    }
};

struct RequiredExtension
{
    std::vector<std::string> alternatives; // One or more extensions, at least one (but not all) must be supported
    uint32_t effectiveStages;              // Bitfield of shader stages requiring this extension

    RequiredExtension(const std::vector<std::string> &alternatives_, uint32_t effectiveStages_)
        : alternatives(alternatives_)
        , effectiveStages(effectiveStages_)
    {
    }

    RequiredExtension(const std::string &extension, uint32_t effectiveStages_) : effectiveStages(effectiveStages_)
    {
        alternatives.push_back(extension);
    }

    RequiredExtension(void) : effectiveStages(0u)
    {
    }
};

struct ProgramSpecification
{
    glu::ProgramSources sources;
    std::vector<RequiredExtension> requiredExtensions;
    uint32_t activeStages; // Has an effect only if sources.separable == true, must be 0 otherwise

    ProgramSpecification(void) : activeStages(0u)
    {
    }
};

struct ShaderCaseSpecification
{
    CaseType caseType;
    ExpectResult expectResult;
    OutputType outputType;
    DataType outputFormat;
    glu::GLSLVersion targetVersion;

    std::vector<RequiredCapability> requiredCaps;

    ValueBlock values;
    std::vector<ProgramSpecification> programs;

    ShaderCaseSpecification(void)
        : caseType(CASETYPE_LAST)
        , expectResult(EXPECT_LAST)
        , outputType(OUTPUT_RESULT)
        , outputFormat(TYPE_LAST)
        , targetVersion(glu::GLSL_VERSION_LAST)
    {
    }
};

bool isValid(const ValueBlock &block);
bool isValid(const ShaderCaseSpecification &spec);

bool isCapabilityRequired(CapabilityFlag capabilityFlag, const ShaderCaseSpecification &spec);

class ShaderCaseFactory
{
public:
    virtual ~ShaderCaseFactory()
    {
    }
    virtual tcu::TestCaseGroup *createGroup(const std::string &name, const std::string &description,
                                            const std::vector<tcu::TestNode *> &children) = 0;
    virtual tcu::TestCase *createCase(const std::string &name, const std::string &description,
                                      const ShaderCaseSpecification &spec)                = 0;
};

std::vector<tcu::TestNode *> parseFile(const tcu::Archive &archive, const std::string &filename,
                                       ShaderCaseFactory *caseFactory);

// Specialization utilties

struct ProgramSpecializationParams
{
    const ShaderCaseSpecification &caseSpec;
    const std::vector<RequiredExtension> requiredExtensions; // Extensions, must be resolved to single ext per entry
    const int maxPatchVertices;                              // Used by tess shaders only

    ProgramSpecializationParams(const ShaderCaseSpecification &caseSpec_,
                                const std::vector<RequiredExtension> &requiredExtensions_, int maxPatchVertices_)
        : caseSpec(caseSpec_)
        , requiredExtensions(requiredExtensions_)
        , maxPatchVertices(maxPatchVertices_)
    {
    }
};

void genCompareFunctions(std::ostringstream &stream, const ValueBlock &valueBlock, bool useFloatTypes);
std::string injectExtensionRequirements(const std::string &baseCode, const std::vector<RequiredExtension> &extensions,
                                        ShaderType shaderType);

// Other utilities

void dumpValues(tcu::TestLog &log, const ValueBlock &values, int arrayNdx);

} // namespace sl
} // namespace glu

#endif // _GLUSHADERLIBRARY_HPP
