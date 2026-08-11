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
 * \brief Shader compilation performance tests.
 *//*--------------------------------------------------------------------*/

#include "es3pShaderCompilationCases.hpp"
#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "tcuMatrix.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuPlatform.hpp"
#include "tcuCommandLine.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuCPUWarmup.hpp"
#include "tcuStringTemplate.hpp"
#include "gluTexture.hpp"
#include "gluPixelTransfer.hpp"
#include "gluRenderContext.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "deClock.h"
#include "deMath.h"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include <map>
#include <algorithm>
#include <limits>
#include <iomanip>

using std::string;
using std::vector;
using tcu::Mat3;
using tcu::Mat4;
using tcu::TestLog;
using tcu::Vec3;
using tcu::Vec4;
using namespace glw; // GL types

namespace deqp
{

namespace gles3
{

namespace Performance
{

static const bool WARMUP_CPU_AT_BEGINNING_OF_CASE    = false;
static const bool WARMUP_CPU_BEFORE_EACH_MEASUREMENT = true;

static const int MAX_VIEWPORT_WIDTH  = 64;
static const int MAX_VIEWPORT_HEIGHT = 64;

static const int DEFAULT_MINIMUM_MEASUREMENT_COUNT              = 15;
static const float RELATIVE_MEDIAN_ABSOLUTE_DEVIATION_THRESHOLD = 0.05f;

// Texture size for the light shader and texture lookup shader cases.
static const int TEXTURE_WIDTH  = 64;
static const int TEXTURE_HEIGHT = 64;

template <typename T>
inline string toStringWithPadding(T value, int minLength)
{
    std::ostringstream s;
    s << std::setfill('0') << std::setw(minLength) << value;
    return s.str();
}

// Add some whitespace and comments to str. They should depend on uniqueNumber.
static string strWithWhiteSpaceAndComments(const string &str, uint32_t uniqueNumber)
{
    string res("");

    // Find the first newline.
    int firstLineEndNdx = 0;
    while (firstLineEndNdx < (int)str.size() && str[firstLineEndNdx] != '\n')
    {
        res += str[firstLineEndNdx];
        firstLineEndNdx++;
    }
    res += '\n';
    DE_ASSERT(firstLineEndNdx < (int)str.size());

    // Add the whitespaces and comments just after the first line.

    de::Random rnd(uniqueNumber);
    int numWS = rnd.getInt(10, 20);

    for (int i = 0; i < numWS; i++)
        res += " \t\n"[rnd.getInt(0, 2)];

    res += "/* unique comment " + de::toString(uniqueNumber) + " */\n";
    res += "// unique comment " + de::toString(uniqueNumber) + "\n";

    for (int i = 0; i < numWS; i++)
        res += " \t\n"[rnd.getInt(0, 2)];

    // Add the rest of the string.
    res.append(&str.c_str()[firstLineEndNdx + 1]);

    return res;
}

//! Helper for computing relative magnitudes while avoiding division by zero.
static float hackySafeRelativeResult(float x, float y)
{
    // \note A possible case is that x is standard deviation, and y is average
    //         (or similarly for median or some such). So, if y is 0, that
    //         probably means that x is also 0(ish) (because in practice we're
    //         dealing with non-negative values, in which case an average of 0
    //         implies that the samples are all 0 - note that the same isn't
    //         strictly true for things like median) so a relative result of 0
    //         wouldn't be that far from the truth.
    return y == 0.0f ? 0.0f : x / y;
}

template <typename T>
static float vectorFloatAverage(const vector<T> &v)
{
    DE_ASSERT(!v.empty());
    float result = 0.0f;
    for (int i = 0; i < (int)v.size(); i++)
        result += (float)v[i];
    return result / (float)v.size();
}

template <typename T>
static float vectorFloatMedian(const vector<T> &v)
{
    DE_ASSERT(!v.empty());
    vector<T> temp = v;
    std::sort(temp.begin(), temp.end());
    return temp.size() % 2 == 0 ? 0.5f * ((float)temp[temp.size() / 2 - 1] + (float)temp[temp.size() / 2]) :
                                  (float)temp[temp.size() / 2];
}

template <typename T>
static float vectorFloatMinimum(const vector<T> &v)
{
    DE_ASSERT(!v.empty());
    return (float)*std::min_element(v.begin(), v.end());
}

template <typename T>
static float vectorFloatMaximum(const vector<T> &v)
{
    DE_ASSERT(!v.empty());
    return (float)*std::max_element(v.begin(), v.end());
}

template <typename T>
static float vectorFloatStandardDeviation(const vector<T> &v)
{
    float average = vectorFloatAverage(v);
    float result  = 0.0f;
    for (int i = 0; i < (int)v.size(); i++)
    {
        float d = (float)v[i] - average;
        result += d * d;
    }
    return deFloatSqrt(result / (float)v.size());
}

template <typename T>
static float vectorFloatRelativeStandardDeviation(const vector<T> &v)
{
    return hackySafeRelativeResult(vectorFloatStandardDeviation(v), vectorFloatAverage(v));
}

template <typename T>
static float vectorFloatMedianAbsoluteDeviation(const vector<T> &v)
{
    float median = vectorFloatMedian(v);
    vector<float> absoluteDeviations(v.size());

    for (int i = 0; i < (int)v.size(); i++)
        absoluteDeviations[i] = deFloatAbs((float)v[i] - median);

    return vectorFloatMedian(absoluteDeviations);
}

template <typename T>
static float vectorFloatRelativeMedianAbsoluteDeviation(const vector<T> &v)
{
    return hackySafeRelativeResult(vectorFloatMedianAbsoluteDeviation(v), vectorFloatMedian(v));
}

template <typename T>
static float vectorFloatMaximumMinusMinimum(const vector<T> &v)
{
    return vectorFloatMaximum(v) - vectorFloatMinimum(v);
}

template <typename T>
static float vectorFloatRelativeMaximumMinusMinimum(const vector<T> &v)
{
    return hackySafeRelativeResult(vectorFloatMaximumMinusMinimum(v), vectorFloatMaximum(v));
}

template <typename T>
static vector<T> vectorLowestPercentage(const vector<T> &v, float factor)
{
    DE_ASSERT(0.0f < factor && factor <= 1.0f);

    int targetSize = (int)(deFloatCeil(factor * (float)v.size()));
    vector<T> temp = v;
    std::sort(temp.begin(), temp.end());

    while ((int)temp.size() > targetSize)
        temp.pop_back();

    return temp;
}

template <typename T>
static float vectorFloatFirstQuartile(const vector<T> &v)
{
    return vectorFloatMedian(vectorLowestPercentage(v, 0.5f));
}

// Helper function for combining 4 tcu::Vec4's into one tcu::Vector<float, 16>.
static tcu::Vector<float, 16> combineVec4ToVec16(const Vec4 &a0, const Vec4 &a1, const Vec4 &a2, const Vec4 &a3)
{
    tcu::Vector<float, 16> result;

    for (int vecNdx = 0; vecNdx < 4; vecNdx++)
    {
        const Vec4 &srcVec = vecNdx == 0 ? a0 : vecNdx == 1 ? a1 : vecNdx == 2 ? a2 : a3;
        for (int i = 0; i < 4; i++)
            result[vecNdx * 4 + i] = srcVec[i];
    }

    return result;
}

// Helper function for extending an n-sized (n <= 16) vector to a 16-sized vector (padded with zeros).
template <int Size>
static tcu::Vector<float, 16> vecTo16(const tcu::Vector<float, Size> &vec)
{
    DE_STATIC_ASSERT(Size <= 16);

    tcu::Vector<float, 16> res(0.0f);

    for (int i = 0; i < Size; i++)
        res[i] = vec[i];

    return res;
}

// Helper function for extending an n-sized (n <= 16) array to a 16-sized vector (padded with zeros).
template <int Size>
static tcu::Vector<float, 16> arrTo16(const tcu::Array<float, Size> &arr)
{
    DE_STATIC_ASSERT(Size <= 16);

    tcu::Vector<float, 16> res(0.0f);

    for (int i = 0; i < Size; i++)
        res[i] = arr[i];

    return res;
}

static string getShaderInfoLog(const glw::Functions &gl, uint32_t shader)
{
    string result;
    int infoLogLen = 0;
    vector<char> infoLogBuf;

    gl.getShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
    infoLogBuf.resize(infoLogLen + 1);
    gl.getShaderInfoLog(shader, infoLogLen + 1, nullptr, &infoLogBuf[0]);
    result = &infoLogBuf[0];

    return result;
}

static string getProgramInfoLog(const glw::Functions &gl, uint32_t program)
{
    string result;
    int infoLogLen = 0;
    vector<char> infoLogBuf;

    gl.getProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLen);
    infoLogBuf.resize(infoLogLen + 1);
    gl.getProgramInfoLog(program, infoLogLen + 1, nullptr, &infoLogBuf[0]);
    result = &infoLogBuf[0];

    return result;
}

enum LightType
{
    LIGHT_DIRECTIONAL = 0,
    LIGHT_POINT,

    LIGHT_LAST,
};

enum LoopType
{
    LOOP_TYPE_STATIC = 0,
    LOOP_TYPE_UNIFORM,
    LOOP_TYPE_DYNAMIC,

    LOOP_LAST
};

// For texture lookup cases: which texture lookups are inside a conditional statement.
enum ConditionalUsage
{
    CONDITIONAL_USAGE_NONE = 0,    // No conditional statements.
    CONDITIONAL_USAGE_FIRST_HALF,  // First numLookUps/2 lookups are inside a conditional statement.
    CONDITIONAL_USAGE_EVERY_OTHER, // First, third etc. lookups are inside conditional statements.

    CONDITIONAL_USAGE_LAST
};

enum ConditionalType
{
    CONDITIONAL_TYPE_STATIC = 0,
    CONDITIONAL_TYPE_UNIFORM,
    CONDITIONAL_TYPE_DYNAMIC,

    CONDITIONAL_TYPE_LAST
};

// For the invalid shader compilation tests; what kind of invalidity a shader shall contain.
enum ShaderValidity
{
    SHADER_VALIDITY_VALID = 0,
    SHADER_VALIDITY_INVALID_CHAR,
    SHADER_VALIDITY_SEMANTIC_ERROR,

    SHADER_VALIDITY_LAST
};

class ShaderCompilerCase : public TestCase
{
public:
    struct AttribSpec
    {
        string name;
        tcu::Vector<float, 16> value;

        AttribSpec(const string &n, const tcu::Vector<float, 16> &v) : name(n), value(v)
        {
        }
    };

    struct UniformSpec
    {
        enum Type
        {
            TYPE_FLOAT = 0,
            TYPE_VEC2,
            TYPE_VEC3,
            TYPE_VEC4,

            TYPE_MAT3,
            TYPE_MAT4,

            TYPE_TEXTURE_UNIT,

            TYPE_LAST
        };

        string name;
        Type type;
        tcu::Vector<float, 16> value;

        UniformSpec(const string &n, Type t, float v) : name(n), type(t), value(v)
        {
        }
        UniformSpec(const string &n, Type t, const tcu::Vector<float, 16> &v) : name(n), type(t), value(v)
        {
        }
    };

    ShaderCompilerCase(Context &context, const char *name, const char *description, int caseID, bool avoidCache,
                       bool addWhitespaceAndComments);
    ~ShaderCompilerCase(void);

    void init(void);

    IterateResult iterate(void);

protected:
    struct ProgramContext
    {
        string vertShaderSource;
        string fragShaderSource;
        vector<AttribSpec> vertexAttributes;
        vector<UniformSpec> uniforms;
    };

    uint32_t getSpecializationID(int measurementNdx)
        const; // Return an ID that depends on the case ID, current measurement index and time; used to specialize attribute names etc. (avoid shader caching).
    virtual ProgramContext generateShaderData(int measurementNdx)
        const = 0; // Generate shader sources and inputs. Attribute etc. names depend on above name specialization.

private:
    struct Measurement
    {
        // \note All times in microseconds. 32-bit integers would probably suffice (would need over an hour of test case runtime to overflow), but better safe than sorry.
        int64_t sourceSetTime;
        int64_t vertexCompileTime;
        int64_t fragmentCompileTime;
        int64_t programLinkTime;
        int64_t firstInputSetTime;
        int64_t firstDrawTime;

        int64_t secondInputSetTime;
        int64_t secondDrawTime;

        int64_t firstPhase(void) const
        {
            return sourceSetTime + vertexCompileTime + fragmentCompileTime + programLinkTime + firstInputSetTime +
                   firstDrawTime;
        }
        int64_t secondPhase(void) const
        {
            return secondInputSetTime + secondDrawTime;
        }

        int64_t totalTimeWithoutDraw(void) const
        {
            return firstPhase() - de::min(secondPhase(), firstInputSetTime + firstDrawTime);
        }

        Measurement(int64_t sourceSetTime_, int64_t vertexCompileTime_, int64_t fragmentCompileTime_,
                    int64_t programLinkTime_, int64_t firstInputSetTime_, int64_t firstDrawTime_,
                    int64_t secondInputSetTime_, int64_t secondDrawTime_)
            : sourceSetTime(sourceSetTime_)
            , vertexCompileTime(vertexCompileTime_)
            , fragmentCompileTime(fragmentCompileTime_)
            , programLinkTime(programLinkTime_)
            , firstInputSetTime(firstInputSetTime_)
            , firstDrawTime(firstDrawTime_)
            , secondInputSetTime(secondInputSetTime_)
            , secondDrawTime(secondDrawTime_)
        {
        }
    };

    struct ShadersAndProgram
    {
        uint32_t vertShader;
        uint32_t fragShader;
        uint32_t program;
    };

    struct Logs
    {
        string vert;
        string frag;
        string link;
    };

    struct BuildInfo
    {
        bool vertCompileSuccess;
        bool fragCompileSuccess;
        bool linkSuccess;

        Logs logs;
    };

    ShadersAndProgram createShadersAndProgram(void) const;
    void setShaderSources(uint32_t vertShader, uint32_t fragShader, const ProgramContext &) const;
    bool compileShader(uint32_t shader) const;
    bool linkAndUseProgram(uint32_t program) const;
    void setShaderInputs(uint32_t program, const ProgramContext &) const; // Set attribute pointers and uniforms.
    void draw(void) const;                                                // Clear, draw and finish.
    void cleanup(const ShadersAndProgram &, const ProgramContext &, bool linkSuccess) const; // Do GL deinitializations.

    Logs getLogs(const ShadersAndProgram &) const;
    void logProgramData(const BuildInfo &, const ProgramContext &) const;
    bool goodEnoughMeasurements(const vector<Measurement> &measurements) const;

    int m_viewportWidth;
    int m_viewportHeight;

    bool m_avoidCache; // If true, avoid caching between measurements as well (and not only between test cases).
    bool
        m_addWhitespaceAndComments; // If true, add random whitespace and comments to the source (good caching should ignore those).
    uint32_t m_startHash; // A hash from case id and time, at the time of construction.

    int m_minimumMeasurementCount;
    int m_maximumMeasurementCount;
};

class ShaderCompilerLightCase : public ShaderCompilerCase
{
public:
    ShaderCompilerLightCase(Context &context, const char *name, const char *description, int caseID, bool avoidCache,
                            bool addWhitespaceAndComments, bool isVertexCase, int numLights, LightType lightType);
    ~ShaderCompilerLightCase(void);

    void init(void);
    void deinit(void);

protected:
    ProgramContext generateShaderData(int measurementNdx) const;

private:
    int m_numLights;
    bool m_isVertexCase;
    LightType m_lightType;
    glu::Texture2D *m_texture;
};

class ShaderCompilerTextureCase : public ShaderCompilerCase
{
public:
    ShaderCompilerTextureCase(Context &context, const char *name, const char *description, int caseID, bool avoidCache,
                              bool addWhitespaceAndComments, int numLookups, ConditionalUsage conditionalUsage,
                              ConditionalType conditionalType);
    ~ShaderCompilerTextureCase(void);

    void init(void);
    void deinit(void);

protected:
    ProgramContext generateShaderData(int measurementNdx) const;

private:
    int m_numLookups;
    vector<glu::Texture2D *> m_textures;
    ConditionalUsage m_conditionalUsage;
    ConditionalType m_conditionalType;
};

class ShaderCompilerLoopCase : public ShaderCompilerCase
{
public:
    ShaderCompilerLoopCase(Context &context, const char *name, const char *description, int caseID, bool avoidCache,
                           bool addWhitespaceAndComments, bool isVertexCase, LoopType type, int numLoopIterations,
                           int nestingDepth);
    ~ShaderCompilerLoopCase(void);

protected:
    ProgramContext generateShaderData(int measurementNdx) const;

private:
    int m_numLoopIterations;
    int m_nestingDepth;
    bool m_isVertexCase;
    LoopType m_type;
};

class ShaderCompilerOperCase : public ShaderCompilerCase
{
public:
    ShaderCompilerOperCase(Context &context, const char *name, const char *description, int caseID, bool avoidCache,
                           bool addWhitespaceAndComments, bool isVertexCase, const char *oper, int numOperations);
    ~ShaderCompilerOperCase(void);

protected:
    ProgramContext generateShaderData(int measurementNdx) const;

private:
    string m_oper;
    int m_numOperations;
    bool m_isVertexCase;
};

class ShaderCompilerMandelbrotCase : public ShaderCompilerCase
{
public:
    ShaderCompilerMandelbrotCase(Context &context, const char *name, const char *description, int caseID,
                                 bool avoidCache, bool addWhitespaceAndComments, int numFractalIterations);
    ~ShaderCompilerMandelbrotCase(void);

protected:
    ProgramContext generateShaderData(int measurementNdx) const;

private:
    int m_numFractalIterations;
};

class InvalidShaderCompilerCase : public TestCase
{
public:
    // \note Similar to the ShaderValidity enum, but doesn't have a VALID type.
    enum InvalidityType
    {
        INVALIDITY_INVALID_CHAR = 0,
        INVALIDITY_SEMANTIC_ERROR,

        INVALIDITY_LAST
    };

    InvalidShaderCompilerCase(Context &context, const char *name, const char *description, int caseID,
                              InvalidityType invalidityType);
    ~InvalidShaderCompilerCase(void);

    IterateResult iterate(void);

protected:
    struct ProgramContext
    {
        string vertShaderSource;
        string fragShaderSource;
    };

    uint32_t getSpecializationID(int measurementNdx)
        const; // Return an ID that depends on the case ID, current measurement index and time; used to specialize attribute names etc. (avoid shader caching).
    virtual ProgramContext generateShaderSources(int measurementNdx)
        const = 0; // Generate shader sources. Attribute etc. names depend on above name specialization.

    InvalidityType m_invalidityType;

private:
    struct Measurement
    {
        // \note All times in microseconds. 32-bit integers would probably suffice (would need over an hour of test case runtime to overflow), but better safe than sorry.
        int64_t sourceSetTime;
        int64_t vertexCompileTime;
        int64_t fragmentCompileTime;

        int64_t totalTime(void) const
        {
            return sourceSetTime + vertexCompileTime + fragmentCompileTime;
        }

        Measurement(int64_t sourceSetTime_, int64_t vertexCompileTime_, int64_t fragmentCompileTime_)
            : sourceSetTime(sourceSetTime_)
            , vertexCompileTime(vertexCompileTime_)
            , fragmentCompileTime(fragmentCompileTime_)
        {
        }
    };

    struct Shaders
    {
        uint32_t vertShader;
        uint32_t fragShader;
    };

    struct Logs
    {
        string vert;
        string frag;
    };

    struct BuildInfo
    {
        bool vertCompileSuccess;
        bool fragCompileSuccess;

        Logs logs;
    };

    Shaders createShaders(void) const;
    void setShaderSources(const Shaders &, const ProgramContext &) const;
    bool compileShader(uint32_t shader) const;
    void cleanup(const Shaders &) const;

    Logs getLogs(const Shaders &) const;
    void logProgramData(const BuildInfo &, const ProgramContext &) const;
    bool goodEnoughMeasurements(const vector<Measurement> &measurements) const;

    uint32_t m_startHash; // A hash from case id and time, at the time of construction.

    int m_minimumMeasurementCount;
    int m_maximumMeasurementCount;
};

class InvalidShaderCompilerLightCase : public InvalidShaderCompilerCase
{
public:
    InvalidShaderCompilerLightCase(Context &context, const char *name, const char *description, int caseID,
                                   InvalidityType invalidityType, bool isVertexCase, int numLights,
                                   LightType lightType);
    ~InvalidShaderCompilerLightCase(void);

protected:
    ProgramContext generateShaderSources(int measurementNdx) const;

private:
    bool m_isVertexCase;
    int m_numLights;
    LightType m_lightType;
};

class InvalidShaderCompilerTextureCase : public InvalidShaderCompilerCase
{
public:
    InvalidShaderCompilerTextureCase(Context &context, const char *name, const char *description, int caseID,
                                     InvalidityType invalidityType, int numLookups, ConditionalUsage conditionalUsage,
                                     ConditionalType conditionalType);
    ~InvalidShaderCompilerTextureCase(void);

protected:
    ProgramContext generateShaderSources(int measurementNdx) const;

private:
    int m_numLookups;
    ConditionalUsage m_conditionalUsage;
    ConditionalType m_conditionalType;
};

class InvalidShaderCompilerLoopCase : public InvalidShaderCompilerCase
{
public:
    InvalidShaderCompilerLoopCase(Context &context, const char *name, const char *description, int caseID,
                                  InvalidityType invalidityType, bool, LoopType type, int numLoopIterations,
                                  int nestingDepth);
    ~InvalidShaderCompilerLoopCase(void);

protected:
    ProgramContext generateShaderSources(int measurementNdx) const;

private:
    bool m_isVertexCase;
    int m_numLoopIterations;
    int m_nestingDepth;
    LoopType m_type;
};

class InvalidShaderCompilerOperCase : public InvalidShaderCompilerCase
{
public:
    InvalidShaderCompilerOperCase(Context &context, const char *name, const char *description, int caseID,
                                  InvalidityType invalidityType, bool isVertexCase, const char *oper,
                                  int numOperations);
    ~InvalidShaderCompilerOperCase(void);

protected:
    ProgramContext generateShaderSources(int measurementNdx) const;

private:
    bool m_isVertexCase;
    string m_oper;
    int m_numOperations;
};

class InvalidShaderCompilerMandelbrotCase : public InvalidShaderCompilerCase
{
public:
    InvalidShaderCompilerMandelbrotCase(Context &context, const char *name, const char *description, int caseID,
                                        InvalidityType invalidityType, int numFractalIterations);
    ~InvalidShaderCompilerMandelbrotCase(void);

protected:
    ProgramContext generateShaderSources(int measurementNdx) const;

private:
    int m_numFractalIterations;
};

static string getNameSpecialization(uint32_t id)
{
    return "_" + toStringWithPadding(id, 10);
}

// Substitute StringTemplate parameters for attribute/uniform/varying name and constant expression specialization as well as possible shader compilation error causes.
static string specializeShaderSource(const string &shaderSourceTemplate, uint32_t cacheAvoidanceID,
                                     ShaderValidity validity)
{
    std::map<string, string> params;
    params["NAME_SPEC"] = getNameSpecialization(cacheAvoidanceID);
    params["FLOAT01"]   = de::floatToString((float)cacheAvoidanceID / (float)(std::numeric_limits<uint32_t>::max()), 6);
    params["SEMANTIC_ERROR"] = validity != SHADER_VALIDITY_SEMANTIC_ERROR ? "" : "\tfloat invalid = sin(1.0, 2.0);\n";
    params["INVALID_CHAR"] =
        validity != SHADER_VALIDITY_INVALID_CHAR ?
            "" :
            "@\n"; // \note Some implementations crash when the invalid character is the last character in the source, so use newline.

    return tcu::StringTemplate(shaderSourceTemplate).specialize(params);
}

// Function for generating the vertex shader of a (directional or point) light case.
static string lightVertexTemplate(int numLights, bool isVertexCase, LightType lightType)
{
    string resultTemplate;

    resultTemplate += "#version 300 es\n"
                      "in highp vec4 a_position${NAME_SPEC};\n"
                      "in mediump vec3 a_normal${NAME_SPEC};\n"
                      "in mediump vec4 a_texCoord0${NAME_SPEC};\n"
                      "uniform mediump vec3 u_material_ambientColor${NAME_SPEC};\n"
                      "uniform mediump vec4 u_material_diffuseColor${NAME_SPEC};\n"
                      "uniform mediump vec3 u_material_emissiveColor${NAME_SPEC};\n"
                      "uniform mediump vec3 u_material_specularColor${NAME_SPEC};\n"
                      "uniform mediump float u_material_shininess${NAME_SPEC};\n";

    for (int lightNdx = 0; lightNdx < numLights; lightNdx++)
    {
        string ndxStr = de::toString(lightNdx);

        resultTemplate += "uniform mediump vec3 u_light" + ndxStr +
                          "_color${NAME_SPEC};\n"
                          "uniform mediump vec3 u_light" +
                          ndxStr + "_direction${NAME_SPEC};\n";

        if (lightType == LIGHT_POINT)
            resultTemplate += "uniform mediump vec4 u_light" + ndxStr +
                              "_position${NAME_SPEC};\n"
                              "uniform mediump float u_light" +
                              ndxStr +
                              "_constantAttenuation${NAME_SPEC};\n"
                              "uniform mediump float u_light" +
                              ndxStr +
                              "_linearAttenuation${NAME_SPEC};\n"
                              "uniform mediump float u_light" +
                              ndxStr + "_quadraticAttenuation${NAME_SPEC};\n";
    }

    resultTemplate += "uniform highp mat4 u_mvpMatrix${NAME_SPEC};\n"
                      "uniform highp mat4 u_modelViewMatrix${NAME_SPEC};\n"
                      "uniform mediump mat3 u_normalMatrix${NAME_SPEC};\n"
                      "uniform mediump mat4 u_texCoordMatrix0${NAME_SPEC};\n"
                      "out mediump vec4 v_color${NAME_SPEC};\n"
                      "out mediump vec2 v_texCoord0${NAME_SPEC};\n";

    if (!isVertexCase)
    {
        resultTemplate += "out mediump vec3 v_eyeNormal${NAME_SPEC};\n";

        if (lightType == LIGHT_POINT)
            resultTemplate += "out mediump vec3 v_directionToLight${NAME_SPEC}[" + de::toString(numLights) +
                              "];\n"
                              "out mediump float v_distanceToLight${NAME_SPEC}[" +
                              de::toString(numLights) + "];\n";
    }

    resultTemplate +=
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
        "\n";

    if (lightType == LIGHT_POINT)
        resultTemplate +=
            "mediump float computeDistanceAttenuation (mediump float distToLight, mediump float constAtt, mediump "
            "float linearAtt, mediump float quadraticAtt)\n"
            "{\n"
            "    return 1.0 / (constAtt + linearAtt * distToLight + quadraticAtt * distToLight * distToLight);\n"
            "}\n"
            "\n";

    resultTemplate +=
        "void main (void)\n"
        "{\n"
        "    highp vec4 position = a_position${NAME_SPEC};\n"
        "    highp vec3 normal = a_normal${NAME_SPEC};\n"
        "    gl_Position = u_mvpMatrix${NAME_SPEC} * position * (0.95 + 0.05*${FLOAT01});\n"
        "    v_texCoord0${NAME_SPEC} = (u_texCoordMatrix0${NAME_SPEC} * a_texCoord0${NAME_SPEC}).xy;\n"
        "    mediump vec4 color = vec4(u_material_emissiveColor${NAME_SPEC}, u_material_diffuseColor${NAME_SPEC}.a);\n"
        "\n"
        "    highp vec4 eyePosition = u_modelViewMatrix${NAME_SPEC} * position;\n"
        "    mediump vec3 eyeNormal = normalize(u_normalMatrix${NAME_SPEC} * normal);\n";

    if (!isVertexCase)
        resultTemplate += "\tv_eyeNormal${NAME_SPEC} = eyeNormal;\n";

    resultTemplate += "\n";

    for (int lightNdx = 0; lightNdx < numLights; lightNdx++)
    {
        string ndxStr = de::toString(lightNdx);

        resultTemplate += "    /* Light " + ndxStr + " */\n";

        if (lightType == LIGHT_POINT)
        {
            resultTemplate +=
                "    mediump float distanceToLight" + ndxStr + " = distance(eyePosition, u_light" + ndxStr +
                "_position${NAME_SPEC});\n"
                "    mediump vec3 directionToLight" +
                ndxStr + " = normalize(direction(eyePosition, u_light" + ndxStr + "_position${NAME_SPEC}));\n";

            if (isVertexCase)
                resultTemplate += "    mediump vec3 halfVector" + ndxStr + " = normalize(directionToLight" + ndxStr +
                                  " + vec3(0.0, 0.0, 1.0));\n"
                                  "    color.rgb += computeLighting(directionToLight" +
                                  ndxStr + ", halfVector" + ndxStr + ", eyeNormal, u_light" + ndxStr +
                                  "_color${NAME_SPEC}, u_material_diffuseColor${NAME_SPEC}.rgb, "
                                  "u_material_specularColor${NAME_SPEC}, u_material_shininess${NAME_SPEC}) * "
                                  "computeDistanceAttenuation(distanceToLight" +
                                  ndxStr + ", u_light" + ndxStr +
                                  "_constantAttenuation${NAME_SPEC}, "
                                  "u_light" +
                                  ndxStr + "_linearAttenuation${NAME_SPEC}, u_light" + ndxStr +
                                  "_quadraticAttenuation${NAME_SPEC});\n";
            else
                resultTemplate += "    v_directionToLight${NAME_SPEC}[" + ndxStr + "] = directionToLight" + ndxStr +
                                  ";\n"
                                  "    v_distanceToLight${NAME_SPEC}[" +
                                  ndxStr + "]  = distanceToLight" + ndxStr + ";\n";
        }
        else if (lightType == LIGHT_DIRECTIONAL)
        {
            if (isVertexCase)
                resultTemplate += "    mediump vec3 directionToLight" + ndxStr + " = -u_light" + ndxStr +
                                  "_direction${NAME_SPEC};\n"
                                  "    mediump vec3 halfVector" +
                                  ndxStr + " = normalize(directionToLight" + ndxStr +
                                  " + vec3(0.0, 0.0, 1.0));\n"
                                  "    color.rgb += computeLighting(directionToLight" +
                                  ndxStr + ", halfVector" + ndxStr + ", eyeNormal, u_light" + ndxStr +
                                  "_color${NAME_SPEC}, u_material_diffuseColor${NAME_SPEC}.rgb, "
                                  "u_material_specularColor${NAME_SPEC}, u_material_shininess${NAME_SPEC});\n";
        }
        else
            DE_ASSERT(false);

        resultTemplate += "\n";
    }

    resultTemplate += "    v_color${NAME_SPEC} = color;\n"
                      "${SEMANTIC_ERROR}"
                      "}\n"
                      "${INVALID_CHAR}";

    return resultTemplate;
}

// Function for generating the fragment shader of a (directional or point) light case.
static string lightFragmentTemplate(int numLights, bool isVertexCase, LightType lightType)
{
    string resultTemplate;

    resultTemplate += "#version 300 es\n"
                      "layout(location = 0) out mediump vec4 o_color;\n";

    if (!isVertexCase)
    {
        resultTemplate += "uniform mediump vec3 u_material_ambientColor${NAME_SPEC};\n"
                          "uniform mediump vec4 u_material_diffuseColor${NAME_SPEC};\n"
                          "uniform mediump vec3 u_material_emissiveColor${NAME_SPEC};\n"
                          "uniform mediump vec3 u_material_specularColor${NAME_SPEC};\n"
                          "uniform mediump float u_material_shininess${NAME_SPEC};\n";

        for (int lightNdx = 0; lightNdx < numLights; lightNdx++)
        {
            string ndxStr = de::toString(lightNdx);

            resultTemplate += "uniform mediump vec3 u_light" + ndxStr +
                              "_color${NAME_SPEC};\n"
                              "uniform mediump vec3 u_light" +
                              ndxStr + "_direction${NAME_SPEC};\n";

            if (lightType == LIGHT_POINT)
                resultTemplate += "uniform mediump vec4 u_light" + ndxStr +
                                  "_position${NAME_SPEC};\n"
                                  "uniform mediump float u_light" +
                                  ndxStr +
                                  "_constantAttenuation${NAME_SPEC};\n"
                                  "uniform mediump float u_light" +
                                  ndxStr +
                                  "_linearAttenuation${NAME_SPEC};\n"
                                  "uniform mediump float u_light" +
                                  ndxStr + "_quadraticAttenuation${NAME_SPEC};\n";
        }
    }

    resultTemplate += "uniform sampler2D u_sampler0${NAME_SPEC};\n"
                      "in mediump vec4 v_color${NAME_SPEC};\n"
                      "in mediump vec2 v_texCoord0${NAME_SPEC};\n";

    if (!isVertexCase)
    {
        resultTemplate += "in mediump vec3 v_eyeNormal${NAME_SPEC};\n";

        if (lightType == LIGHT_POINT)
            resultTemplate += "in mediump vec3 v_directionToLight${NAME_SPEC}[" + de::toString(numLights) +
                              "];\n"
                              "in mediump float v_distanceToLight${NAME_SPEC}[" +
                              de::toString(numLights) + "];\n";

        resultTemplate += "mediump vec3 direction (mediump vec4 from, mediump vec4 to)\n"
                          "{\n"
                          "    return vec3(to.xyz * from.w - from.xyz * to.w);\n"
                          "}\n"
                          "\n";

        resultTemplate +=
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
            "\n";

        if (lightType == LIGHT_POINT)
            resultTemplate +=
                "mediump float computeDistanceAttenuation (mediump float distToLight, mediump float constAtt, mediump "
                "float linearAtt, mediump float quadraticAtt)\n"
                "{\n"
                "    return 1.0 / (constAtt + linearAtt * distToLight + quadraticAtt * distToLight * distToLight);\n"
                "}\n"
                "\n";
    }

    resultTemplate += "void main (void)\n"
                      "{\n"
                      "    mediump vec2 texCoord0 = v_texCoord0${NAME_SPEC}.xy;\n"
                      "    mediump vec4 color = v_color${NAME_SPEC};\n";

    if (!isVertexCase)
    {
        resultTemplate += "    mediump vec3 eyeNormal = normalize(v_eyeNormal${NAME_SPEC});\n"
                          "\n";

        for (int lightNdx = 0; lightNdx < numLights; lightNdx++)
        {
            string ndxStr = de::toString(lightNdx);

            resultTemplate += "    /* Light " + ndxStr + " */\n";

            if (lightType == LIGHT_POINT)
                resultTemplate += "    mediump vec3 directionToLight" + ndxStr +
                                  " = normalize(v_directionToLight${NAME_SPEC}[" + ndxStr +
                                  "]);\n"
                                  "    mediump float distanceToLight" +
                                  ndxStr + " = v_distanceToLight${NAME_SPEC}[" + ndxStr +
                                  "];\n"
                                  "    mediump vec3 halfVector" +
                                  ndxStr + " = normalize(directionToLight" + ndxStr +
                                  " + vec3(0.0, 0.0, 1.0));\n"
                                  "    color.rgb += computeLighting(directionToLight" +
                                  ndxStr + ", halfVector" + ndxStr + ", eyeNormal, u_light" + ndxStr +
                                  "_color${NAME_SPEC}, u_material_diffuseColor${NAME_SPEC}.rgb, "
                                  "u_material_specularColor${NAME_SPEC}, u_material_shininess${NAME_SPEC}) * "
                                  "computeDistanceAttenuation(distanceToLight" +
                                  ndxStr + ", u_light" + ndxStr +
                                  "_constantAttenuation${NAME_SPEC}, "
                                  "u_light" +
                                  ndxStr + "_linearAttenuation${NAME_SPEC}, u_light" + ndxStr +
                                  "_quadraticAttenuation${NAME_SPEC});\n"
                                  "\n";
            else if (lightType == LIGHT_DIRECTIONAL)
                resultTemplate += "    mediump vec3 directionToLight" + ndxStr + " = -u_light" + ndxStr +
                                  "_direction${NAME_SPEC};\n"
                                  "    mediump vec3 halfVector" +
                                  ndxStr + " = normalize(directionToLight" + ndxStr +
                                  " + vec3(0.0, 0.0, 1.0));\n"
                                  "    color.rgb += computeLighting(directionToLight" +
                                  ndxStr + ", halfVector" + ndxStr + ", eyeNormal, u_light" + ndxStr +
                                  "_color${NAME_SPEC}, u_material_diffuseColor${NAME_SPEC}.rgb, "
                                  "u_material_specularColor${NAME_SPEC}, u_material_shininess${NAME_SPEC});\n"
                                  "\n";
            else
                DE_ASSERT(false);
        }
    }

    resultTemplate += "    color *= texture(u_sampler0${NAME_SPEC}, texCoord0);\n"
                      "    o_color = color + ${FLOAT01};\n"
                      "${SEMANTIC_ERROR}"
                      "}\n"
                      "${INVALID_CHAR}";

    return resultTemplate;
}

// Function for generating the shader attributes of a (directional or point) light case.
static vector<ShaderCompilerCase::AttribSpec> lightShaderAttributes(const string &nameSpecialization)
{
    vector<ShaderCompilerCase::AttribSpec> result;

    result.push_back(ShaderCompilerCase::AttribSpec(
        "a_position" + nameSpecialization,
        combineVec4ToVec16(Vec4(-1.0f, -1.0f, 0.0f, 1.0f), Vec4(-1.0f, 1.0f, 0.0f, 1.0f), Vec4(1.0f, -1.0f, 0.0f, 1.0f),
                           Vec4(1.0f, 1.0f, 0.0f, 1.0f))));

    result.push_back(ShaderCompilerCase::AttribSpec(
        "a_normal" + nameSpecialization,
        combineVec4ToVec16(Vec4(0.0f, 0.0f, -1.0f, 0.0f), Vec4(0.0f, 0.0f, -1.0f, 0.0f), Vec4(0.0f, 0.0f, -1.0f, 0.0f),
                           Vec4(0.0f, 0.0f, -1.0f, 0.0f))));

    result.push_back(
        ShaderCompilerCase::AttribSpec("a_texCoord0" + nameSpecialization,
                                       combineVec4ToVec16(Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(1.0f, 0.0f, 0.0f, 0.0f),
                                                          Vec4(0.0f, 1.0f, 0.0f, 0.0f), Vec4(1.0f, 1.0f, 0.0f, 0.0f))));

    return result;
}

// Function for generating the shader uniforms of a (directional or point) light case.
static vector<ShaderCompilerCase::UniformSpec> lightShaderUniforms(const string &nameSpecialization, int numLights,
                                                                   LightType lightType)
{
    vector<ShaderCompilerCase::UniformSpec> result;

    result.push_back(ShaderCompilerCase::UniformSpec("u_material_ambientColor" + nameSpecialization,
                                                     ShaderCompilerCase::UniformSpec::TYPE_VEC3,
                                                     vecTo16(Vec3(0.5f, 0.7f, 0.9f))));

    result.push_back(ShaderCompilerCase::UniformSpec("u_material_diffuseColor" + nameSpecialization,
                                                     ShaderCompilerCase::UniformSpec::TYPE_VEC4,
                                                     vecTo16(Vec4(0.3f, 0.4f, 0.5f, 1.0f))));

    result.push_back(ShaderCompilerCase::UniformSpec("u_material_emissiveColor" + nameSpecialization,
                                                     ShaderCompilerCase::UniformSpec::TYPE_VEC3,
                                                     vecTo16(Vec3(0.7f, 0.2f, 0.2f))));

    result.push_back(ShaderCompilerCase::UniformSpec("u_material_specularColor" + nameSpecialization,
                                                     ShaderCompilerCase::UniformSpec::TYPE_VEC3,
                                                     vecTo16(Vec3(0.2f, 0.6f, 1.0f))));

    result.push_back(ShaderCompilerCase::UniformSpec("u_material_shininess" + nameSpecialization,
                                                     ShaderCompilerCase::UniformSpec::TYPE_FLOAT, 0.8f));

    for (int lightNdx = 0; lightNdx < numLights; lightNdx++)
    {
        string ndxStr = de::toString(lightNdx);

        result.push_back(ShaderCompilerCase::UniformSpec("u_light" + ndxStr + "_color" + nameSpecialization,
                                                         ShaderCompilerCase::UniformSpec::TYPE_VEC3,
                                                         vecTo16(Vec3(0.8f, 0.6f, 0.3f))));

        result.push_back(ShaderCompilerCase::UniformSpec("u_light" + ndxStr + "_direction" + nameSpecialization,
                                                         ShaderCompilerCase::UniformSpec::TYPE_VEC3,
                                                         vecTo16(Vec3(0.2f, 0.3f, 0.4f))));

        if (lightType == LIGHT_POINT)
        {
            result.push_back(ShaderCompilerCase::UniformSpec("u_light" + ndxStr + "_position" + nameSpecialization,
                                                             ShaderCompilerCase::UniformSpec::TYPE_VEC4,
                                                             vecTo16(Vec4(1.0f, 0.6f, 0.3f, 0.2f))));

            result.push_back(
                ShaderCompilerCase::UniformSpec("u_light" + ndxStr + "_constantAttenuation" + nameSpecialization,
                                                ShaderCompilerCase::UniformSpec::TYPE_FLOAT, 0.6f));

            result.push_back(
                ShaderCompilerCase::UniformSpec("u_light" + ndxStr + "_linearAttenuation" + nameSpecialization,
                                                ShaderCompilerCase::UniformSpec::TYPE_FLOAT, 0.5f));

            result.push_back(
                ShaderCompilerCase::UniformSpec("u_light" + ndxStr + "_quadraticAttenuation" + nameSpecialization,
                                                ShaderCompilerCase::UniformSpec::TYPE_FLOAT, 0.4f));
        }
    }

    result.push_back(ShaderCompilerCase::UniformSpec("u_mvpMatrix" + nameSpecialization,
                                                     ShaderCompilerCase::UniformSpec::TYPE_MAT4,
                                                     arrTo16(Mat4(1.0f).getColumnMajorData())));

    result.push_back(ShaderCompilerCase::UniformSpec("u_modelViewMatrix" + nameSpecialization,
                                                     ShaderCompilerCase::UniformSpec::TYPE_MAT4,
                                                     arrTo16(Mat4(1.0f).getColumnMajorData())));

    result.push_back(ShaderCompilerCase::UniformSpec("u_normalMatrix" + nameSpecialization,
                                                     ShaderCompilerCase::UniformSpec::TYPE_MAT3,
                                                     arrTo16(Mat3(1.0f).getColumnMajorData())));

    result.push_back(ShaderCompilerCase::UniformSpec("u_texCoordMatrix0" + nameSpecialization,
                                                     ShaderCompilerCase::UniformSpec::TYPE_MAT4,
                                                     arrTo16(Mat4(1.0f).getColumnMajorData())));

    result.push_back(ShaderCompilerCase::UniformSpec("u_sampler0" + nameSpecialization,
                                                     ShaderCompilerCase::UniformSpec::TYPE_TEXTURE_UNIT, 0.0f));

    return result;
}

// Function for generating a vertex shader with a for loop.
static string loopVertexTemplate(LoopType type, bool isVertexCase, int numLoopIterations, int nestingDepth)
{
    string resultTemplate;
    string loopBound = type == LOOP_TYPE_STATIC  ? de::toString(numLoopIterations) :
                       type == LOOP_TYPE_UNIFORM ? "int(u_loopBound${NAME_SPEC})" :
                       type == LOOP_TYPE_DYNAMIC ? "int(a_loopBound${NAME_SPEC})" :
                                                   "";

    DE_ASSERT(!loopBound.empty());

    resultTemplate += "#version 300 es\n"
                      "in highp vec4 a_position${NAME_SPEC};\n";

    if (type == LOOP_TYPE_DYNAMIC)
        resultTemplate += "in mediump float a_loopBound${NAME_SPEC};\n";

    resultTemplate += "in mediump vec4 a_value${NAME_SPEC};\n"
                      "out mediump vec4 v_value${NAME_SPEC};\n";

    if (isVertexCase)
    {
        if (type == LOOP_TYPE_UNIFORM)
            resultTemplate += "uniform mediump float u_loopBound${NAME_SPEC};\n";

        resultTemplate += "\n"
                          "void main()\n"
                          "{\n"
                          "    gl_Position = a_position${NAME_SPEC} * (0.95 + 0.05*${FLOAT01});\n"
                          "    mediump vec4 value = a_value${NAME_SPEC};\n";

        for (int i = 0; i < nestingDepth; i++)
        {
            string iterName = "i" + de::toString(i);
            resultTemplate += string(i + 1, '\t') + "for (int " + iterName + " = 0; " + iterName + " < " + loopBound +
                              "; " + iterName + "++)\n";
        }

        resultTemplate += string(nestingDepth + 1, '\t') + "value *= a_value${NAME_SPEC};\n";

        resultTemplate += "    v_value${NAME_SPEC} = value;\n";
    }
    else
    {
        if (type == LOOP_TYPE_DYNAMIC)
            resultTemplate += "out mediump float v_loopBound${NAME_SPEC};\n";

        resultTemplate += "\n"
                          "void main()\n"
                          "{\n"
                          "    gl_Position = a_position${NAME_SPEC} * (0.95 + 0.05*${FLOAT01});\n"
                          "    v_value${NAME_SPEC} = a_value${NAME_SPEC};\n";

        if (type == LOOP_TYPE_DYNAMIC)
            resultTemplate += "    v_loopBound${NAME_SPEC} = a_loopBound${NAME_SPEC};\n";
    }

    resultTemplate += "${SEMANTIC_ERROR}"
                      "}\n"
                      "${INVALID_CHAR}";

    return resultTemplate;
}

// Function for generating a fragment shader with a for loop.
static string loopFragmentTemplate(LoopType type, bool isVertexCase, int numLoopIterations, int nestingDepth)
{
    string resultTemplate;
    string loopBound = type == LOOP_TYPE_STATIC  ? de::toString(numLoopIterations) :
                       type == LOOP_TYPE_UNIFORM ? "int(u_loopBound${NAME_SPEC})" :
                       type == LOOP_TYPE_DYNAMIC ? "int(v_loopBound${NAME_SPEC})" :
                                                   "";

    DE_ASSERT(!loopBound.empty());

    resultTemplate += "#version 300 es\n"
                      "layout(location = 0) out mediump vec4 o_color;\n"
                      "in mediump vec4 v_value${NAME_SPEC};\n";

    if (!isVertexCase)
    {
        if (type == LOOP_TYPE_DYNAMIC)
            resultTemplate += "in mediump float v_loopBound${NAME_SPEC};\n";
        else if (type == LOOP_TYPE_UNIFORM)
            resultTemplate += "uniform mediump float u_loopBound${NAME_SPEC};\n";

        resultTemplate += "\n"
                          "void main()\n"
                          "{\n"
                          "    mediump vec4 value = v_value${NAME_SPEC};\n";

        for (int i = 0; i < nestingDepth; i++)
        {
            string iterName = "i" + de::toString(i);
            resultTemplate += string(i + 1, '\t') + "for (int " + iterName + " = 0; " + iterName + " < " + loopBound +
                              "; " + iterName + "++)\n";
        }

        resultTemplate += string(nestingDepth + 1, '\t') + "value *= v_value${NAME_SPEC};\n";

        resultTemplate += "    o_color = value + ${FLOAT01};\n";
    }
    else
        resultTemplate += "\n"
                          "void main()\n"
                          "{\n"
                          "    o_color = v_value${NAME_SPEC} + ${FLOAT01};\n";

    resultTemplate += "${SEMANTIC_ERROR}"
                      "}\n"
                      "${INVALID_CHAR}";

    return resultTemplate;
}

// Function for generating the shader attributes for a loop case.
static vector<ShaderCompilerCase::AttribSpec> loopShaderAttributes(const string &nameSpecialization, LoopType type,
                                                                   int numLoopIterations)
{
    vector<ShaderCompilerCase::AttribSpec> result;

    result.push_back(ShaderCompilerCase::AttribSpec(
        "a_position" + nameSpecialization,
        combineVec4ToVec16(Vec4(-1.0f, -1.0f, 0.0f, 1.0f), Vec4(-1.0f, 1.0f, 0.0f, 1.0f), Vec4(1.0f, -1.0f, 0.0f, 1.0f),
                           Vec4(1.0f, 1.0f, 0.0f, 1.0f))));

    result.push_back(
        ShaderCompilerCase::AttribSpec("a_value" + nameSpecialization,
                                       combineVec4ToVec16(Vec4(1.0f, 1.0f, 1.0f, 1.0f), Vec4(1.0f, 1.0f, 1.0f, 1.0f),
                                                          Vec4(1.0f, 1.0f, 1.0f, 1.0f), Vec4(1.0f, 1.0f, 1.0f, 1.0f))));

    if (type == LOOP_TYPE_DYNAMIC)
        result.push_back(ShaderCompilerCase::AttribSpec(
            "a_loopBound" + nameSpecialization, combineVec4ToVec16(Vec4((float)numLoopIterations, 0.0f, 0.0f, 0.0f),
                                                                   Vec4((float)numLoopIterations, 0.0f, 0.0f, 0.0f),
                                                                   Vec4((float)numLoopIterations, 0.0f, 0.0f, 0.0f),
                                                                   Vec4((float)numLoopIterations, 0.0f, 0.0f, 0.0f))));

    return result;
}

static vector<ShaderCompilerCase::UniformSpec> loopShaderUniforms(const string &nameSpecialization, LoopType type,
                                                                  int numLoopIterations)
{
    vector<ShaderCompilerCase::UniformSpec> result;

    if (type == LOOP_TYPE_UNIFORM)
        result.push_back(ShaderCompilerCase::UniformSpec(
            "u_loopBound" + nameSpecialization, ShaderCompilerCase::UniformSpec::TYPE_FLOAT, (float)numLoopIterations));

    return result;
}

// Function for generating the shader attributes for a case with only one attribute value in addition to the position attribute.
static vector<ShaderCompilerCase::AttribSpec> singleValueShaderAttributes(const string &nameSpecialization)
{
    vector<ShaderCompilerCase::AttribSpec> result;

    result.push_back(ShaderCompilerCase::AttribSpec(
        "a_position" + nameSpecialization,
        combineVec4ToVec16(Vec4(-1.0f, -1.0f, 0.0f, 1.0f), Vec4(-1.0f, 1.0f, 0.0f, 1.0f), Vec4(1.0f, -1.0f, 0.0f, 1.0f),
                           Vec4(1.0f, 1.0f, 0.0f, 1.0f))));

    result.push_back(
        ShaderCompilerCase::AttribSpec("a_value" + nameSpecialization,
                                       combineVec4ToVec16(Vec4(1.0f, 1.0f, 1.0f, 1.0f), Vec4(1.0f, 1.0f, 1.0f, 1.0f),
                                                          Vec4(1.0f, 1.0f, 1.0f, 1.0f), Vec4(1.0f, 1.0f, 1.0f, 1.0f))));

    return result;
}

// Function for generating a vertex shader with a binary operation chain.
static string binaryOpVertexTemplate(int numOperations, const char *op)
{
    string resultTemplate;

    resultTemplate += "#version 300 es\n"
                      "in highp vec4 a_position${NAME_SPEC};\n"
                      "in mediump vec4 a_value${NAME_SPEC};\n"
                      "out mediump vec4 v_value${NAME_SPEC};\n"
                      "\n"
                      "void main()\n"
                      "{\n"
                      "    gl_Position = a_position${NAME_SPEC} * (0.95 + 0.05*${FLOAT01});\n"
                      "    mediump vec4 value = ";

    for (int i = 0; i < numOperations; i++)
        resultTemplate += string(i > 0 ? op : "") + "a_value${NAME_SPEC}";

    resultTemplate += ";\n"
                      "    v_value${NAME_SPEC} = value;\n"
                      "${SEMANTIC_ERROR}"
                      "}\n"
                      "${INVALID_CHAR}";

    return resultTemplate;
}

// Function for generating a fragment shader with a binary operation chain.
static string binaryOpFragmentTemplate(int numOperations, const char *op)
{
    string resultTemplate;

    resultTemplate += "#version 300 es\n"
                      "layout(location = 0) out mediump vec4 o_color;\n"
                      "in mediump vec4 v_value${NAME_SPEC};\n"
                      "\n"
                      "void main()\n"
                      "{\n"
                      "    mediump vec4 value = ";

    for (int i = 0; i < numOperations; i++)
        resultTemplate += string(i > 0 ? op : "") + "v_value${NAME_SPEC}";

    resultTemplate += ";\n"
                      "    o_color = value + ${FLOAT01};\n"
                      "${SEMANTIC_ERROR}"
                      "}\n"
                      "${INVALID_CHAR}";

    return resultTemplate;
}

// Function for generating a vertex that takes one attribute in addition to position and just passes it to the fragment shader as a varying.
static string singleVaryingVertexTemplate(void)
{
    const char *resultTemplate = "#version 300 es\n"
                                 "in highp vec4 a_position${NAME_SPEC};\n"
                                 "in mediump vec4 a_value${NAME_SPEC};\n"
                                 "out mediump vec4 v_value${NAME_SPEC};\n"
                                 "\n"
                                 "void main()\n"
                                 "{\n"
                                 "    gl_Position = a_position${NAME_SPEC} * (0.95 + 0.05*${FLOAT01});\n"
                                 "    v_value${NAME_SPEC} = a_value${NAME_SPEC};\n"
                                 "${SEMANTIC_ERROR}"
                                 "}\n"
                                 "${INVALID_CHAR}";

    return resultTemplate;
}

// Function for generating a fragment shader that takes a single varying and uses it as the color.
static string singleVaryingFragmentTemplate(void)
{
    const char *resultTemplate = "#version 300 es\n"
                                 "layout(location = 0) out mediump vec4 o_color;\n"
                                 "in mediump vec4 v_value${NAME_SPEC};\n"
                                 "\n"
                                 "void main()\n"
                                 "{\n"
                                 "    o_color = v_value${NAME_SPEC} + ${FLOAT01};\n"
                                 "${SEMANTIC_ERROR}"
                                 "}\n"
                                 "${INVALID_CHAR}";

    return resultTemplate;
}

// Function for generating the vertex shader of a texture lookup case.
static string textureLookupVertexTemplate(ConditionalUsage conditionalUsage, ConditionalType conditionalType)
{
    string resultTemplate;
    bool conditionVaryingNeeded =
        conditionalUsage != CONDITIONAL_USAGE_NONE && conditionalType == CONDITIONAL_TYPE_DYNAMIC;

    resultTemplate += "#version 300 es\n"
                      "in highp vec4 a_position${NAME_SPEC};\n"
                      "in mediump vec2 a_coords${NAME_SPEC};\n"
                      "out mediump vec2 v_coords${NAME_SPEC};\n";

    if (conditionVaryingNeeded)
        resultTemplate += "in mediump float a_condition${NAME_SPEC};\n"
                          "out mediump float v_condition${NAME_SPEC};\n";

    resultTemplate += "\n"
                      "void main()\n"
                      "{\n"
                      "    gl_Position = a_position${NAME_SPEC} * (0.95 + 0.05*${FLOAT01});\n"
                      "    v_coords${NAME_SPEC} = a_coords${NAME_SPEC};\n";

    if (conditionVaryingNeeded)
        resultTemplate += "    v_condition${NAME_SPEC} = a_condition${NAME_SPEC};\n";

    resultTemplate += "${SEMANTIC_ERROR}"
                      "}\n"
                      "${INVALID_CHAR}";

    return resultTemplate;
}

// Function for generating the fragment shader of a texture lookup case.
static string textureLookupFragmentTemplate(int numLookups, ConditionalUsage conditionalUsage,
                                            ConditionalType conditionalType)
{
    string resultTemplate;

    resultTemplate += "#version 300 es\n"
                      "layout(location = 0) out mediump vec4 o_color;\n"
                      "in mediump vec2 v_coords${NAME_SPEC};\n";

    if (conditionalUsage != CONDITIONAL_USAGE_NONE && conditionalType == CONDITIONAL_TYPE_DYNAMIC)
        resultTemplate += "in mediump float v_condition${NAME_SPEC};\n";

    for (int i = 0; i < numLookups; i++)
        resultTemplate += "uniform sampler2D u_sampler" + de::toString(i) + "${NAME_SPEC};\n";

    if (conditionalUsage != CONDITIONAL_USAGE_NONE && conditionalType == CONDITIONAL_TYPE_UNIFORM)
        resultTemplate += "uniform mediump float u_condition${NAME_SPEC};\n";

    resultTemplate += "\n"
                      "void main()\n"
                      "{\n"
                      "    mediump vec4 color = vec4(0.0);\n";

    const char *conditionalTerm = conditionalType == CONDITIONAL_TYPE_STATIC  ? "1.0 > 0.0" :
                                  conditionalType == CONDITIONAL_TYPE_UNIFORM ? "u_condition${NAME_SPEC} > 0.0" :
                                  conditionalType == CONDITIONAL_TYPE_DYNAMIC ? "v_condition${NAME_SPEC} > 0.0" :
                                                                                nullptr;

    DE_ASSERT(conditionalTerm != nullptr);

    if (conditionalUsage == CONDITIONAL_USAGE_FIRST_HALF)
        resultTemplate += string("") + "    if (" + conditionalTerm +
                          ")\n"
                          "    {\n";

    for (int i = 0; i < numLookups; i++)
    {
        if (conditionalUsage == CONDITIONAL_USAGE_FIRST_HALF)
        {
            if (i < (numLookups + 1) / 2)
                resultTemplate += "\t";
        }
        else if (conditionalUsage == CONDITIONAL_USAGE_EVERY_OTHER)
        {
            if (i % 2 == 0)
                resultTemplate += string("") + "    if (" + conditionalTerm +
                                  ")\n"
                                  "\t";
        }

        resultTemplate += "    color += texture(u_sampler" + de::toString(i) + "${NAME_SPEC}, v_coords${NAME_SPEC});\n";

        if (conditionalUsage == CONDITIONAL_USAGE_FIRST_HALF && i == (numLookups - 1) / 2)
            resultTemplate += "\t}\n";
    }

    resultTemplate += "    o_color = color/" + de::toString(numLookups) + ".0 + ${FLOAT01};\n" +
                      "${SEMANTIC_ERROR}"
                      "}\n"
                      "${INVALID_CHAR}";

    return resultTemplate;
}

// Function for generating the shader attributes of a texture lookup case.
static vector<ShaderCompilerCase::AttribSpec> textureLookupShaderAttributes(const string &nameSpecialization,
                                                                            ConditionalUsage conditionalUsage,
                                                                            ConditionalType conditionalType)
{
    vector<ShaderCompilerCase::AttribSpec> result;

    result.push_back(ShaderCompilerCase::AttribSpec(
        "a_position" + nameSpecialization,
        combineVec4ToVec16(Vec4(-1.0f, -1.0f, 0.0f, 1.0f), Vec4(-1.0f, 1.0f, 0.0f, 1.0f), Vec4(1.0f, -1.0f, 0.0f, 1.0f),
                           Vec4(1.0f, 1.0f, 0.0f, 1.0f))));

    result.push_back(
        ShaderCompilerCase::AttribSpec("a_coords" + nameSpecialization,
                                       combineVec4ToVec16(Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(0.0f, 1.0f, 0.0f, 0.0f),
                                                          Vec4(1.0f, 0.0f, 0.0f, 0.0f), Vec4(1.0f, 1.0f, 0.0f, 0.0f))));

    if (conditionalUsage != CONDITIONAL_USAGE_NONE && conditionalType == CONDITIONAL_TYPE_DYNAMIC)
        result.push_back(ShaderCompilerCase::AttribSpec(
            "a_condition" + nameSpecialization, combineVec4ToVec16(Vec4(1.0f), Vec4(1.0f), Vec4(1.0f), Vec4(1.0f))));

    return result;
}

// Function for generating the shader uniforms of a texture lookup case.
static vector<ShaderCompilerCase::UniformSpec> textureLookupShaderUniforms(const string &nameSpecialization,
                                                                           int numLookups,
                                                                           ConditionalUsage conditionalUsage,
                                                                           ConditionalType conditionalType)
{
    vector<ShaderCompilerCase::UniformSpec> result;

    for (int i = 0; i < numLookups; i++)
        result.push_back(ShaderCompilerCase::UniformSpec("u_sampler" + de::toString(i) + nameSpecialization,
                                                         ShaderCompilerCase::UniformSpec::TYPE_TEXTURE_UNIT, (float)i));

    if (conditionalUsage != CONDITIONAL_USAGE_NONE && conditionalType == CONDITIONAL_TYPE_UNIFORM)
        result.push_back(ShaderCompilerCase::UniformSpec("u_condition" + nameSpecialization,
                                                         ShaderCompilerCase::UniformSpec::TYPE_FLOAT, 1.0f));

    return result;
}

static string mandelbrotVertexTemplate(void)
{
    const char *resultTemplate =
        "#version 300 es\n"
        "uniform highp mat4 u_mvp${NAME_SPEC};\n"
        "\n"
        "in highp vec4 a_vertex${NAME_SPEC};\n"
        "in highp vec4 a_coord${NAME_SPEC};\n"
        "\n"
        "out mediump vec2 v_coord${NAME_SPEC};\n"
        "\n"
        "void main(void)\n"
        "{\n"
        "    gl_Position = u_mvp${NAME_SPEC} * a_vertex${NAME_SPEC} * (0.95 + 0.05*${FLOAT01});\n"
        "\n"
        "    float xMin = -2.0;\n"
        "    float xMax = +0.5;\n"
        "    float yMin = -1.5;\n"
        "    float yMax = +1.5;\n"
        "\n"
        "    v_coord${NAME_SPEC}.x = a_coord${NAME_SPEC}.x * (xMax - xMin) + xMin;\n"
        "    v_coord${NAME_SPEC}.y = a_coord${NAME_SPEC}.y * (yMax - yMin) + yMin;\n"
        "${SEMANTIC_ERROR}"
        "}\n"
        "${INVALID_CHAR}";

    return resultTemplate;
}

static string mandelbrotFragmentTemplate(int numFractalIterations)
{
    string resultTemplate = "#version 300 es\n"
                            "layout(location = 0) out mediump vec4 o_color;\n"
                            "in mediump vec2 v_coord${NAME_SPEC};\n"
                            "\n"
                            "precision mediump float;\n"
                            "\n"
                            "#define NUM_ITERS " +
                            de::toString(numFractalIterations) +
                            "\n"
                            "\n"
                            "void main (void)\n"
                            "{\n"
                            "    vec2 coords = v_coord${NAME_SPEC};\n"
                            "    float u_limit = 2.0 * 2.0;\n"
                            "    vec2 tmp = vec2(0, 0);\n"
                            "    int iter;\n"
                            "\n"
                            "    for (iter = 0; iter < NUM_ITERS; iter++)\n"
                            "    {\n"
                            "        tmp = vec2((tmp.x + tmp.y) * (tmp.x - tmp.y), 2.0 * (tmp.x * tmp.y)) + coords;\n"
                            "\n"
                            "        if (dot(tmp, tmp) > u_limit)\n"
                            "            break;\n"
                            "    }\n"
                            "\n"
                            "    vec3 color = vec3(float(iter) * (1.0 / float(NUM_ITERS)));\n"
                            "\n"
                            "    o_color = vec4(color, 1.0) + ${FLOAT01};\n"
                            "${SEMANTIC_ERROR}"
                            "}\n"
                            "${INVALID_CHAR}";

    return resultTemplate;
}

static vector<ShaderCompilerCase::AttribSpec> mandelbrotShaderAttributes(const string &nameSpecialization)
{
    vector<ShaderCompilerCase::AttribSpec> result;

    result.push_back(ShaderCompilerCase::AttribSpec(
        "a_vertex" + nameSpecialization,
        combineVec4ToVec16(Vec4(-1.0f, -1.0f, 0.0f, 1.0f), Vec4(-1.0f, 1.0f, 0.0f, 1.0f), Vec4(1.0f, -1.0f, 0.0f, 1.0f),
                           Vec4(1.0f, 1.0f, 0.0f, 1.0f))));

    result.push_back(
        ShaderCompilerCase::AttribSpec("a_coord" + nameSpecialization,
                                       combineVec4ToVec16(Vec4(0.0f, 0.0f, 0.0f, 1.0f), Vec4(0.0f, 1.0f, 0.0f, 1.0f),
                                                          Vec4(1.0f, 0.0f, 0.0f, 1.0f), Vec4(1.0f, 1.0f, 0.0f, 1.0f))));

    return result;
}

static vector<ShaderCompilerCase::UniformSpec> mandelbrotShaderUniforms(const string &nameSpecialization)
{
    vector<ShaderCompilerCase::UniformSpec> result;

    result.push_back(ShaderCompilerCase::UniformSpec("u_mvp" + nameSpecialization,
                                                     ShaderCompilerCase::UniformSpec::TYPE_MAT4,
                                                     arrTo16(Mat4(1.0f).getColumnMajorData())));

    return result;
}

ShaderCompilerCase::ShaderCompilerCase(Context &context, const char *name, const char *description, int caseID,
                                       bool avoidCache, bool addWhitespaceAndComments)
    : TestCase(context, tcu::NODETYPE_PERFORMANCE, name, description)
    , m_viewportWidth(0)
    , m_viewportHeight(0)
    , m_avoidCache(avoidCache)
    , m_addWhitespaceAndComments(addWhitespaceAndComments)
    , m_startHash((uint32_t)(deUint64Hash(deGetTime()) ^ deUint64Hash(deGetMicroseconds()) ^ deInt32Hash(caseID)))
{
    int cmdLineIterCount      = context.getTestContext().getCommandLine().getTestIterationCount();
    m_minimumMeasurementCount = cmdLineIterCount > 0 ? cmdLineIterCount : DEFAULT_MINIMUM_MEASUREMENT_COUNT;
    m_maximumMeasurementCount = m_minimumMeasurementCount * 3;
}

ShaderCompilerCase::~ShaderCompilerCase(void)
{
}

uint32_t ShaderCompilerCase::getSpecializationID(int measurementNdx) const
{
    if (m_avoidCache)
        return m_startHash ^ (uint32_t)deInt32Hash((int32_t)measurementNdx);
    else
        return m_startHash;
}

void ShaderCompilerCase::init(void)
{
    const glw::Functions &gl              = m_context.getRenderContext().getFunctions();
    const tcu::RenderTarget &renderTarget = m_context.getRenderContext().getRenderTarget();

    m_viewportWidth  = deMin32(MAX_VIEWPORT_WIDTH, renderTarget.getWidth());
    m_viewportHeight = deMin32(MAX_VIEWPORT_HEIGHT, renderTarget.getHeight());

    gl.viewport(0, 0, m_viewportWidth, m_viewportHeight);
}

ShaderCompilerCase::ShadersAndProgram ShaderCompilerCase::createShadersAndProgram(void) const
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    ShadersAndProgram result;

    result.vertShader = gl.createShader(GL_VERTEX_SHADER);
    result.fragShader = gl.createShader(GL_FRAGMENT_SHADER);
    result.program    = gl.createProgram();

    gl.attachShader(result.program, result.vertShader);
    gl.attachShader(result.program, result.fragShader);

    return result;
}

void ShaderCompilerCase::setShaderSources(uint32_t vertShader, uint32_t fragShader, const ProgramContext &progCtx) const
{
    const glw::Functions &gl         = m_context.getRenderContext().getFunctions();
    const char *vertShaderSourceCStr = progCtx.vertShaderSource.c_str();
    const char *fragShaderSourceCStr = progCtx.fragShaderSource.c_str();
    gl.shaderSource(vertShader, 1, &vertShaderSourceCStr, nullptr);
    gl.shaderSource(fragShader, 1, &fragShaderSourceCStr, nullptr);
}

bool ShaderCompilerCase::compileShader(uint32_t shader) const
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint status             = 0;
    gl.compileShader(shader);
    gl.getShaderiv(shader, GL_COMPILE_STATUS, &status);
    return status != 0;
}

bool ShaderCompilerCase::linkAndUseProgram(uint32_t program) const
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint linkStatus         = 0;

    gl.linkProgram(program);
    gl.getProgramiv(program, GL_LINK_STATUS, &linkStatus);

    if (linkStatus != 0)
        gl.useProgram(program);

    return linkStatus != 0;
}

void ShaderCompilerCase::setShaderInputs(uint32_t program, const ProgramContext &progCtx) const
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // Setup attributes.

    for (int attribNdx = 0; attribNdx < (int)progCtx.vertexAttributes.size(); attribNdx++)
    {
        int location = gl.getAttribLocation(program, progCtx.vertexAttributes[attribNdx].name.c_str());
        if (location >= 0)
        {
            gl.enableVertexAttribArray(location);
            gl.vertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, 0,
                                   progCtx.vertexAttributes[attribNdx].value.getPtr());
        }
    }

    // Setup uniforms.

    for (int uniformNdx = 0; uniformNdx < (int)progCtx.uniforms.size(); uniformNdx++)
    {
        int location = gl.getUniformLocation(program, progCtx.uniforms[uniformNdx].name.c_str());
        if (location >= 0)
        {
            const float *floatPtr = progCtx.uniforms[uniformNdx].value.getPtr();

            switch (progCtx.uniforms[uniformNdx].type)
            {
            case UniformSpec::TYPE_FLOAT:
                gl.uniform1fv(location, 1, floatPtr);
                break;
            case UniformSpec::TYPE_VEC2:
                gl.uniform2fv(location, 1, floatPtr);
                break;
            case UniformSpec::TYPE_VEC3:
                gl.uniform3fv(location, 1, floatPtr);
                break;
            case UniformSpec::TYPE_VEC4:
                gl.uniform4fv(location, 1, floatPtr);
                break;
            case UniformSpec::TYPE_MAT3:
                gl.uniformMatrix3fv(location, 1, GL_FALSE, floatPtr);
                break;
            case UniformSpec::TYPE_MAT4:
                gl.uniformMatrix4fv(location, 1, GL_FALSE, floatPtr);
                break;
            case UniformSpec::TYPE_TEXTURE_UNIT:
                gl.uniform1i(location, (GLint)deRoundFloatToInt32(*floatPtr));
                break;
            default:
                DE_ASSERT(false);
            }
        }
    }
}

void ShaderCompilerCase::draw(void) const
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    static const uint8_t indices[] = {0, 1, 2, 2, 1, 3};

    gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    gl.drawElements(GL_TRIANGLES, DE_LENGTH_OF_ARRAY(indices), GL_UNSIGNED_BYTE, indices);

    // \note Read one pixel to force compilation.
    uint32_t pixel;
    gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
}

void ShaderCompilerCase::cleanup(const ShadersAndProgram &shadersAndProgram, const ProgramContext &progCtx,
                                 bool linkSuccess) const
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    if (linkSuccess)
    {
        for (int attribNdx = 0; attribNdx < (int)progCtx.vertexAttributes.size(); attribNdx++)
        {
            int location =
                gl.getAttribLocation(shadersAndProgram.program, progCtx.vertexAttributes[attribNdx].name.c_str());
            if (location >= 0)
                gl.disableVertexAttribArray(location);
        }
    }

    gl.useProgram(0);
    gl.detachShader(shadersAndProgram.program, shadersAndProgram.vertShader);
    gl.detachShader(shadersAndProgram.program, shadersAndProgram.fragShader);
    gl.deleteShader(shadersAndProgram.vertShader);
    gl.deleteShader(shadersAndProgram.fragShader);
    gl.deleteProgram(shadersAndProgram.program);
}

void ShaderCompilerCase::logProgramData(const BuildInfo &buildInfo, const ProgramContext &progCtx) const
{
    m_testCtx.getLog() << TestLog::ShaderProgram(buildInfo.linkSuccess, buildInfo.logs.link)
                       << TestLog::Shader(QP_SHADER_TYPE_VERTEX, progCtx.vertShaderSource, buildInfo.vertCompileSuccess,
                                          buildInfo.logs.vert)
                       << TestLog::Shader(QP_SHADER_TYPE_FRAGMENT, progCtx.fragShaderSource,
                                          buildInfo.fragCompileSuccess, buildInfo.logs.frag)
                       << TestLog::EndShaderProgram;
}

ShaderCompilerCase::Logs ShaderCompilerCase::getLogs(const ShadersAndProgram &shadersAndProgram) const
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    Logs result;

    result.vert = getShaderInfoLog(gl, shadersAndProgram.vertShader);
    result.frag = getShaderInfoLog(gl, shadersAndProgram.fragShader);
    result.link = getProgramInfoLog(gl, shadersAndProgram.program);

    return result;
}

bool ShaderCompilerCase::goodEnoughMeasurements(const vector<Measurement> &measurements) const
{
    if ((int)measurements.size() < m_minimumMeasurementCount)
        return false;
    else
    {
        if ((int)measurements.size() >= m_maximumMeasurementCount)
            return true;
        else
        {
            vector<int64_t> totalTimesWithoutDraw;
            for (int i = 0; i < (int)measurements.size(); i++)
                totalTimesWithoutDraw.push_back(measurements[i].totalTimeWithoutDraw());
            return vectorFloatRelativeMedianAbsoluteDeviation(vectorLowestPercentage(totalTimesWithoutDraw, 0.5f)) <
                   RELATIVE_MEDIAN_ABSOLUTE_DEVIATION_THRESHOLD;
        }
    }
}

ShaderCompilerCase::IterateResult ShaderCompilerCase::iterate(void)
{
    // Before actual measurements, compile and draw with a minimal shader to avoid possible initial slowdowns in the actual test.
    {
        uint32_t specID = getSpecializationID(0);
        ProgramContext progCtx;
        progCtx.vertShaderSource = specializeShaderSource(singleVaryingVertexTemplate(), specID, SHADER_VALIDITY_VALID);
        progCtx.fragShaderSource =
            specializeShaderSource(singleVaryingFragmentTemplate(), specID, SHADER_VALIDITY_VALID);
        progCtx.vertexAttributes = singleValueShaderAttributes(getNameSpecialization(specID));

        ShadersAndProgram shadersAndProgram = createShadersAndProgram();
        setShaderSources(shadersAndProgram.vertShader, shadersAndProgram.fragShader, progCtx);

        BuildInfo buildInfo;
        buildInfo.vertCompileSuccess = compileShader(shadersAndProgram.vertShader);
        buildInfo.fragCompileSuccess = compileShader(shadersAndProgram.fragShader);
        buildInfo.linkSuccess        = linkAndUseProgram(shadersAndProgram.program);
        if (!(buildInfo.vertCompileSuccess && buildInfo.fragCompileSuccess && buildInfo.linkSuccess))
        {
            buildInfo.logs = getLogs(shadersAndProgram);
            logProgramData(buildInfo, progCtx);
            cleanup(shadersAndProgram, progCtx, buildInfo.linkSuccess);
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Compilation failed");
            return STOP;
        }
        setShaderInputs(shadersAndProgram.program, progCtx);
        draw();
        cleanup(shadersAndProgram, progCtx, buildInfo.linkSuccess);
    }

    vector<Measurement> measurements;
    // \note These are logged after measurements are done.
    ProgramContext latestProgramContext;
    BuildInfo latestBuildInfo;

    if (WARMUP_CPU_AT_BEGINNING_OF_CASE)
        tcu::warmupCPU();

    // Actual test measurements.
    while (!goodEnoughMeasurements(measurements))
    {
        // Create shaders, compile & link, set shader inputs and draw. Time measurement is done at relevant points.
        // \note Setting inputs and drawing are done twice in order to find out the time for actual compiling.

        // \note Shader data (sources and inputs) are generated and GL shader and program objects are created before any time measurements.
        ProgramContext progCtx              = generateShaderData((int)measurements.size());
        ShadersAndProgram shadersAndProgram = createShadersAndProgram();
        BuildInfo buildInfo;

        if (m_addWhitespaceAndComments)
        {
            const uint32_t hash      = m_startHash ^ (uint32_t)deInt32Hash((int32_t)measurements.size());
            progCtx.vertShaderSource = strWithWhiteSpaceAndComments(progCtx.vertShaderSource, hash);
            progCtx.fragShaderSource = strWithWhiteSpaceAndComments(progCtx.fragShaderSource, hash);
        }

        if (WARMUP_CPU_BEFORE_EACH_MEASUREMENT)
            tcu::warmupCPU();

        // \note Do NOT do anything too hefty between the first and last deGetMicroseconds() here (other than the gl calls); it would disturb the measurement.

        uint64_t startTime = deGetMicroseconds();

        setShaderSources(shadersAndProgram.vertShader, shadersAndProgram.fragShader, progCtx);
        uint64_t shaderSourceSetEndTime = deGetMicroseconds();

        buildInfo.vertCompileSuccess        = compileShader(shadersAndProgram.vertShader);
        uint64_t vertexShaderCompileEndTime = deGetMicroseconds();

        buildInfo.fragCompileSuccess          = compileShader(shadersAndProgram.fragShader);
        uint64_t fragmentShaderCompileEndTime = deGetMicroseconds();

        buildInfo.linkSuccess       = linkAndUseProgram(shadersAndProgram.program);
        uint64_t programLinkEndTime = deGetMicroseconds();

        // Check compilation and linking status here, after all compilation and linking gl calls are made.
        if (!(buildInfo.vertCompileSuccess && buildInfo.fragCompileSuccess && buildInfo.linkSuccess))
        {
            buildInfo.logs = getLogs(shadersAndProgram);
            logProgramData(buildInfo, progCtx);
            cleanup(shadersAndProgram, progCtx, buildInfo.linkSuccess);
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Compilation failed");
            return STOP;
        }

        setShaderInputs(shadersAndProgram.program, progCtx);
        uint64_t firstShaderInputSetEndTime = deGetMicroseconds();

        // Draw for the first time.
        draw();
        uint64_t firstDrawEndTime = deGetMicroseconds();

        // Set inputs and draw again.

        setShaderInputs(shadersAndProgram.program, progCtx);
        uint64_t secondShaderInputSetEndTime = deGetMicroseconds();

        draw();
        uint64_t secondDrawEndTime = deGetMicroseconds();

        // De-initializations (detach shaders etc.).

        buildInfo.logs = getLogs(shadersAndProgram);
        cleanup(shadersAndProgram, progCtx, buildInfo.linkSuccess);

        // Output measurement log later (after last measurement).

        measurements.push_back(Measurement((int64_t)(shaderSourceSetEndTime - startTime),
                                           (int64_t)(vertexShaderCompileEndTime - shaderSourceSetEndTime),
                                           (int64_t)(fragmentShaderCompileEndTime - vertexShaderCompileEndTime),
                                           (int64_t)(programLinkEndTime - fragmentShaderCompileEndTime),
                                           (int64_t)(firstShaderInputSetEndTime - programLinkEndTime),
                                           (int64_t)(firstDrawEndTime - firstShaderInputSetEndTime),
                                           (int64_t)(secondShaderInputSetEndTime - firstDrawEndTime),
                                           (int64_t)(secondDrawEndTime - secondShaderInputSetEndTime)));

        latestBuildInfo      = buildInfo;
        latestProgramContext = progCtx;

        m_testCtx.touchWatchdog(); // \note Measurements may take a while in a bad case.
    }

    // End of test case, log information about measurements.
    {
        TestLog &log = m_testCtx.getLog();

        vector<int64_t> sourceSetTimes;
        vector<int64_t> vertexCompileTimes;
        vector<int64_t> fragmentCompileTimes;
        vector<int64_t> programLinkTimes;
        vector<int64_t> firstInputSetTimes;
        vector<int64_t> firstDrawTimes;
        vector<int64_t> secondInputTimes;
        vector<int64_t> secondDrawTimes;
        vector<int64_t> firstPhaseTimes;
        vector<int64_t> secondPhaseTimes;
        vector<int64_t> totalTimesWithoutDraw;
        vector<int64_t> specializationTimes;

        if (!m_avoidCache)
            log << TestLog::Message
                << "Note: Testing cache hits, so the medians and averages exclude the first iteration."
                << TestLog::EndMessage;

        log << TestLog::Message << "Note: \"Specialization time\" means first draw time minus second draw time."
            << TestLog::EndMessage << TestLog::Message
            << "Note: \"Compilation time\" means the time up to (and including) linking, plus specialization time."
            << TestLog::EndMessage;

        log << TestLog::Section("IterationMeasurements", "Iteration measurements of compilation and linking times");

        DE_ASSERT((int)measurements.size() > (m_avoidCache ? 0 : 1));

        for (int ndx = 0; ndx < (int)measurements.size(); ndx++)
        {
            const Measurement &curMeas = measurements[ndx];

            // Subtract time of second phase (second input setup and draw) from first (from start to end of first draw).
            // \note Cap if second phase seems unreasonably high (higher than first input set and draw).
            int64_t timeWithoutDraw = curMeas.totalTimeWithoutDraw();

            // Specialization time = first draw - second draw time. Again, cap at 0 if second draw was longer than first draw.
            int64_t specializationTime = de::max<int64_t>(0, curMeas.firstDrawTime - curMeas.secondDrawTime);

            if (ndx > 0 ||
                m_avoidCache) // \note When allowing cache hits, don't account for the first measurement when calculating median or average.
            {
                sourceSetTimes.push_back(curMeas.sourceSetTime);
                vertexCompileTimes.push_back(curMeas.vertexCompileTime);
                fragmentCompileTimes.push_back(curMeas.fragmentCompileTime);
                programLinkTimes.push_back(curMeas.programLinkTime);
                firstInputSetTimes.push_back(curMeas.firstInputSetTime);
                firstDrawTimes.push_back(curMeas.firstDrawTime);
                firstPhaseTimes.push_back(curMeas.firstPhase());
                secondDrawTimes.push_back(curMeas.secondDrawTime);
                secondInputTimes.push_back(curMeas.secondInputSetTime);
                secondPhaseTimes.push_back(curMeas.secondPhase());
                totalTimesWithoutDraw.push_back(timeWithoutDraw);
                specializationTimes.push_back(specializationTime);
            }

            // Log this measurement.
            log << TestLog::Float("Measurement" + de::toString(ndx) + "CompilationTime",
                                  "Measurement " + de::toString(ndx) + " compilation time", "ms", QP_KEY_TAG_TIME,
                                  (float)timeWithoutDraw / 1000.0f)
                << TestLog::Float("Measurement" + de::toString(ndx) + "SpecializationTime",
                                  "Measurement " + de::toString(ndx) + " specialization time", "ms", QP_KEY_TAG_TIME,
                                  (float)specializationTime / 1000.0f);
        }

        // Log some statistics.

        for (int entireRangeOrLowestHalf = 0; entireRangeOrLowestHalf < 2; entireRangeOrLowestHalf++)
        {
            bool isEntireRange    = entireRangeOrLowestHalf == 0;
            string statNamePrefix = isEntireRange ? "" : "LowestHalf";
            vector<int64_t> rangeTotalTimes =
                isEntireRange ? totalTimesWithoutDraw : vectorLowestPercentage(totalTimesWithoutDraw, 0.5f);
            vector<int64_t> rangeSpecializationTimes =
                isEntireRange ? specializationTimes : vectorLowestPercentage(specializationTimes, 0.5f);

#define LOG_COMPILE_SPECIALIZE_TIME_STAT(NAME, DESC, FUNC)                                                            \
    log << TestLog::Float(statNamePrefix + "CompilationTime" + (NAME), (DESC) + string(" of compilation time"), "ms", \
                          QP_KEY_TAG_TIME, (FUNC)(rangeTotalTimes) / 1000.0f)                                         \
        << TestLog::Float(statNamePrefix + "SpecializationTime" + (NAME), (DESC) + string(" of specialization time"), \
                          "ms", QP_KEY_TAG_TIME, (FUNC)(rangeSpecializationTimes) / 1000.0f)

#define LOG_COMPILE_SPECIALIZE_RELATIVE_STAT(NAME, DESC, FUNC)                                                        \
    log << TestLog::Float(statNamePrefix + "CompilationTime" + (NAME), (DESC) + string(" of compilation time"), "",   \
                          QP_KEY_TAG_NONE, (FUNC)(rangeTotalTimes))                                                   \
        << TestLog::Float(statNamePrefix + "SpecializationTime" + (NAME), (DESC) + string(" of specialization time"), \
                          "", QP_KEY_TAG_NONE, (FUNC)(rangeSpecializationTimes))

            log << TestLog::Message << "\nStatistics computed from " << (isEntireRange ? "all" : "only the lowest 50%")
                << " of the above measurements:" << TestLog::EndMessage;

            LOG_COMPILE_SPECIALIZE_TIME_STAT("Median", "Median", vectorFloatMedian);
            LOG_COMPILE_SPECIALIZE_TIME_STAT("Average", "Average", vectorFloatAverage);
            LOG_COMPILE_SPECIALIZE_TIME_STAT("Minimum", "Minimum", vectorFloatMinimum);
            LOG_COMPILE_SPECIALIZE_TIME_STAT("Maximum", "Maximum", vectorFloatMaximum);
            LOG_COMPILE_SPECIALIZE_TIME_STAT("MedianAbsoluteDeviation", "Median absolute deviation",
                                             vectorFloatMedianAbsoluteDeviation);
            LOG_COMPILE_SPECIALIZE_RELATIVE_STAT("RelativeMedianAbsoluteDeviation",
                                                 "Relative median absolute deviation",
                                                 vectorFloatRelativeMedianAbsoluteDeviation);
            LOG_COMPILE_SPECIALIZE_TIME_STAT("StandardDeviation", "Standard deviation", vectorFloatStandardDeviation);
            LOG_COMPILE_SPECIALIZE_RELATIVE_STAT("RelativeStandardDeviation", "Relative standard deviation",
                                                 vectorFloatRelativeStandardDeviation);
            LOG_COMPILE_SPECIALIZE_TIME_STAT("MaxMinusMin", "Max-min", vectorFloatMaximumMinusMinimum);
            LOG_COMPILE_SPECIALIZE_RELATIVE_STAT("RelativeMaxMinusMin", "Relative max-min",
                                                 vectorFloatRelativeMaximumMinusMinimum);

#undef LOG_COMPILE_SPECIALIZE_RELATIVE_STAT
#undef LOG_COMPILE_SPECIALIZE_TIME_STAT

            if (!isEntireRange && vectorFloatRelativeMedianAbsoluteDeviation(rangeTotalTimes) >
                                      RELATIVE_MEDIAN_ABSOLUTE_DEVIATION_THRESHOLD)
                log << TestLog::Message
                    << "\nWARNING: couldn't achieve relative median absolute deviation under threshold value "
                    << RELATIVE_MEDIAN_ABSOLUTE_DEVIATION_THRESHOLD
                    << " for compilation time of the lowest 50% of measurements" << TestLog::EndMessage;
        }

        log << TestLog::EndSection; // End section IterationMeasurements

        for (int medianOrAverage = 0; medianOrAverage < 2; medianOrAverage++)
        {
            typedef float (*VecFunc)(const vector<int64_t> &);

            bool isMedian   = medianOrAverage == 0;
            string singular = isMedian ? "Median" : "Average";
            string plural   = singular + "s";
            VecFunc func    = isMedian ? (VecFunc)vectorFloatMedian<int64_t> : (VecFunc)vectorFloatAverage<int64_t>;

            log << TestLog::Section(plural + "PerPhase", plural + " per phase");

            for (int entireRangeOrLowestHalf = 0; entireRangeOrLowestHalf < 2; entireRangeOrLowestHalf++)
            {
                bool isEntireRange    = entireRangeOrLowestHalf == 0;
                string statNamePrefix = isEntireRange ? "" : "LowestHalf";
                float rangeSizeRatio  = isEntireRange ? 1.0f : 0.5f;

#define LOG_TIME(NAME, DESC, DATA)                                                                               \
    log << TestLog::Float(statNamePrefix + (NAME) + singular, singular + " of " + (DESC), "ms", QP_KEY_TAG_TIME, \
                          func(vectorLowestPercentage((DATA), rangeSizeRatio)) / 1000.0f)

                log << TestLog::Message
                    << (isEntireRange ? "For all measurements:" : "\nFor only the lowest 50% of the measurements:")
                    << TestLog::EndMessage;
                LOG_TIME("ShaderSourceSetTime", "shader source set time", sourceSetTimes);
                LOG_TIME("VertexShaderCompileTime", "vertex shader compile time", vertexCompileTimes);
                LOG_TIME("FragmentShaderCompileTime", "fragment shader compile time", fragmentCompileTimes);
                LOG_TIME("ProgramLinkTime", "program link time", programLinkTimes);
                LOG_TIME("FirstShaderInputSetTime", "first shader input set time", firstInputSetTimes);
                LOG_TIME("FirstDrawTime", "first draw time", firstDrawTimes);
                LOG_TIME("SecondShaderInputSetTime", "second shader input set time", secondInputTimes);
                LOG_TIME("SecondDrawTime", "second draw time", secondDrawTimes);

#undef LOG_TIME
            }

            log << TestLog::EndSection;
        }

        // Set result.

        {
            log << TestLog::Message
                << "Note: test result is the first quartile (i.e. median of the lowest half of measurements) of "
                   "compilation times"
                << TestLog::EndMessage;
            float result = vectorFloatFirstQuartile(totalTimesWithoutDraw) / 1000.0f;
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::floatToString(result, 2).c_str());
        }

        // Log shaders.

        if (m_avoidCache || m_addWhitespaceAndComments)
        {
            string msg = "Note: the following shaders are the ones from the last iteration; ";

            if (m_avoidCache)
                msg += "variables' names and some constant expressions";
            if (m_addWhitespaceAndComments)
                msg += string(m_avoidCache ? " as well as " : "") + "whitespace and comments";

            msg += " differ between iterations.";

            log << TestLog::Message << msg.c_str() << TestLog::EndMessage;
        }

        logProgramData(latestBuildInfo, latestProgramContext);

        return STOP;
    }
}

ShaderCompilerLightCase::ShaderCompilerLightCase(Context &context, const char *name, const char *description,
                                                 int caseID, bool avoidCache, bool addWhitespaceAndComments,
                                                 bool isVertexCase, int numLights, LightType lightType)
    : ShaderCompilerCase(context, name, description, caseID, avoidCache, addWhitespaceAndComments)
    , m_numLights(numLights)
    , m_isVertexCase(isVertexCase)
    , m_lightType(lightType)
    , m_texture(nullptr)
{
}

ShaderCompilerLightCase::~ShaderCompilerLightCase(void)
{
    ShaderCompilerLightCase::deinit();
}

void ShaderCompilerLightCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;
}

void ShaderCompilerLightCase::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // Setup texture.

    DE_ASSERT(m_texture == nullptr);

    m_texture =
        new glu::Texture2D(m_context.getRenderContext(), GL_RGB, GL_UNSIGNED_BYTE, TEXTURE_WIDTH, TEXTURE_HEIGHT);

    tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(m_texture->getRefTexture().getFormat());

    m_texture->getRefTexture().allocLevel(0);
    tcu::fillWithComponentGradients(m_texture->getRefTexture().getLevel(0), fmtInfo.valueMin, fmtInfo.valueMax);

    gl.activeTexture(GL_TEXTURE0);
    gl.bindTexture(GL_TEXTURE_2D, m_texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_texture->upload();

    ShaderCompilerCase::init();
}

ShaderCompilerCase::ProgramContext ShaderCompilerLightCase::generateShaderData(int measurementNdx) const
{
    uint32_t specID = getSpecializationID(measurementNdx);
    string nameSpec = getNameSpecialization(specID);
    ProgramContext result;

    result.vertShaderSource = specializeShaderSource(lightVertexTemplate(m_numLights, m_isVertexCase, m_lightType),
                                                     specID, SHADER_VALIDITY_VALID);
    result.fragShaderSource = specializeShaderSource(lightFragmentTemplate(m_numLights, m_isVertexCase, m_lightType),
                                                     specID, SHADER_VALIDITY_VALID);
    result.vertexAttributes = lightShaderAttributes(nameSpec);
    result.uniforms         = lightShaderUniforms(nameSpec, m_numLights, m_lightType);

    return result;
}

ShaderCompilerTextureCase::ShaderCompilerTextureCase(Context &context, const char *name, const char *description,
                                                     int caseID, bool avoidCache, bool addWhitespaceAndComments,
                                                     int numLookups, ConditionalUsage conditionalUsage,
                                                     ConditionalType conditionalType)
    : ShaderCompilerCase(context, name, description, caseID, avoidCache, addWhitespaceAndComments)
    , m_numLookups(numLookups)
    , m_conditionalUsage(conditionalUsage)
    , m_conditionalType(conditionalType)
{
}

ShaderCompilerTextureCase::~ShaderCompilerTextureCase(void)
{
    ShaderCompilerTextureCase::deinit();
}

void ShaderCompilerTextureCase::deinit(void)
{
    for (vector<glu::Texture2D *>::iterator i = m_textures.begin(); i != m_textures.end(); i++)
        delete *i;
    m_textures.clear();
}

void ShaderCompilerTextureCase::init(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    // Setup texture.

    DE_ASSERT(m_textures.empty());

    m_textures.reserve(m_numLookups);

    for (int i = 0; i < m_numLookups; i++)
    {
        glu::Texture2D *tex =
            new glu::Texture2D(m_context.getRenderContext(), GL_RGB, GL_UNSIGNED_BYTE, TEXTURE_WIDTH, TEXTURE_HEIGHT);
        tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(tex->getRefTexture().getFormat());

        tex->getRefTexture().allocLevel(0);
        tcu::fillWithComponentGradients(tex->getRefTexture().getLevel(0), fmtInfo.valueMin, fmtInfo.valueMax);

        gl.activeTexture(GL_TEXTURE0 + i);
        gl.bindTexture(GL_TEXTURE_2D, tex->getGLTexture());
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        tex->upload();

        m_textures.push_back(tex);
    }

    ShaderCompilerCase::init();
}

ShaderCompilerCase::ProgramContext ShaderCompilerTextureCase::generateShaderData(int measurementNdx) const
{
    uint32_t specID = getSpecializationID(measurementNdx);
    string nameSpec = getNameSpecialization(specID);
    ProgramContext result;

    result.vertShaderSource = specializeShaderSource(textureLookupVertexTemplate(m_conditionalUsage, m_conditionalType),
                                                     specID, SHADER_VALIDITY_VALID);
    result.fragShaderSource =
        specializeShaderSource(textureLookupFragmentTemplate(m_numLookups, m_conditionalUsage, m_conditionalType),
                               specID, SHADER_VALIDITY_VALID);
    result.vertexAttributes = textureLookupShaderAttributes(nameSpec, m_conditionalUsage, m_conditionalType);
    result.uniforms = textureLookupShaderUniforms(nameSpec, m_numLookups, m_conditionalUsage, m_conditionalType);

    return result;
}

ShaderCompilerLoopCase::ShaderCompilerLoopCase(Context &context, const char *name, const char *description, int caseID,
                                               bool avoidCache, bool addWhitespaceAndComments, bool isVertexCase,
                                               LoopType type, int numLoopIterations, int nestingDepth)
    : ShaderCompilerCase(context, name, description, caseID, avoidCache, addWhitespaceAndComments)
    , m_numLoopIterations(numLoopIterations)
    , m_nestingDepth(nestingDepth)
    , m_isVertexCase(isVertexCase)
    , m_type(type)
{
}

ShaderCompilerLoopCase::~ShaderCompilerLoopCase(void)
{
}

ShaderCompilerCase::ProgramContext ShaderCompilerLoopCase::generateShaderData(int measurementNdx) const
{
    uint32_t specID = getSpecializationID(measurementNdx);
    string nameSpec = getNameSpecialization(specID);
    ProgramContext result;

    result.vertShaderSource = specializeShaderSource(
        loopVertexTemplate(m_type, m_isVertexCase, m_numLoopIterations, m_nestingDepth), specID, SHADER_VALIDITY_VALID);
    result.fragShaderSource =
        specializeShaderSource(loopFragmentTemplate(m_type, m_isVertexCase, m_numLoopIterations, m_nestingDepth),
                               specID, SHADER_VALIDITY_VALID);

    result.vertexAttributes = loopShaderAttributes(nameSpec, m_type, m_numLoopIterations);
    result.uniforms         = loopShaderUniforms(nameSpec, m_type, m_numLoopIterations);

    return result;
}

ShaderCompilerOperCase::ShaderCompilerOperCase(Context &context, const char *name, const char *description, int caseID,
                                               bool avoidCache, bool addWhitespaceAndComments, bool isVertexCase,
                                               const char *oper, int numOperations)
    : ShaderCompilerCase(context, name, description, caseID, avoidCache, addWhitespaceAndComments)
    , m_oper(oper)
    , m_numOperations(numOperations)
    , m_isVertexCase(isVertexCase)
{
}

ShaderCompilerOperCase::~ShaderCompilerOperCase(void)
{
}

ShaderCompilerCase::ProgramContext ShaderCompilerOperCase::generateShaderData(int measurementNdx) const
{
    uint32_t specID = getSpecializationID(measurementNdx);
    string nameSpec = getNameSpecialization(specID);
    ProgramContext result;

    if (m_isVertexCase)
    {
        result.vertShaderSource = specializeShaderSource(binaryOpVertexTemplate(m_numOperations, m_oper.c_str()),
                                                         specID, SHADER_VALIDITY_VALID);
        result.fragShaderSource =
            specializeShaderSource(singleVaryingFragmentTemplate(), specID, SHADER_VALIDITY_VALID);
    }
    else
    {
        result.vertShaderSource = specializeShaderSource(singleVaryingVertexTemplate(), specID, SHADER_VALIDITY_VALID);
        result.fragShaderSource = specializeShaderSource(binaryOpFragmentTemplate(m_numOperations, m_oper.c_str()),
                                                         specID, SHADER_VALIDITY_VALID);
    }

    result.vertexAttributes = singleValueShaderAttributes(nameSpec);

    result.uniforms.clear(); // No uniforms used.

    return result;
}

ShaderCompilerMandelbrotCase::ShaderCompilerMandelbrotCase(Context &context, const char *name, const char *description,
                                                           int caseID, bool avoidCache, bool addWhitespaceAndComments,
                                                           int numFractalIterations)
    : ShaderCompilerCase(context, name, description, caseID, avoidCache, addWhitespaceAndComments)
    , m_numFractalIterations(numFractalIterations)
{
}

ShaderCompilerMandelbrotCase::~ShaderCompilerMandelbrotCase(void)
{
}

ShaderCompilerCase::ProgramContext ShaderCompilerMandelbrotCase::generateShaderData(int measurementNdx) const
{
    uint32_t specID = getSpecializationID(measurementNdx);
    string nameSpec = getNameSpecialization(specID);
    ProgramContext result;

    result.vertShaderSource = specializeShaderSource(mandelbrotVertexTemplate(), specID, SHADER_VALIDITY_VALID);
    result.fragShaderSource =
        specializeShaderSource(mandelbrotFragmentTemplate(m_numFractalIterations), specID, SHADER_VALIDITY_VALID);

    result.vertexAttributes = mandelbrotShaderAttributes(nameSpec);
    result.uniforms         = mandelbrotShaderUniforms(nameSpec);

    return result;
}

InvalidShaderCompilerCase::InvalidShaderCompilerCase(Context &context, const char *name, const char *description,
                                                     int caseID, InvalidityType invalidityType)
    : TestCase(context, tcu::NODETYPE_PERFORMANCE, name, description)
    , m_invalidityType(invalidityType)
    , m_startHash((uint32_t)(deUint64Hash(deGetTime()) ^ deUint64Hash(deGetMicroseconds()) ^ deInt32Hash(caseID)))
{
    int cmdLineIterCount      = context.getTestContext().getCommandLine().getTestIterationCount();
    m_minimumMeasurementCount = cmdLineIterCount > 0 ? cmdLineIterCount : DEFAULT_MINIMUM_MEASUREMENT_COUNT;
    m_maximumMeasurementCount = 3 * m_minimumMeasurementCount;
}

InvalidShaderCompilerCase::~InvalidShaderCompilerCase(void)
{
}

uint32_t InvalidShaderCompilerCase::getSpecializationID(int measurementNdx) const
{
    return m_startHash ^ (uint32_t)deInt32Hash((int32_t)measurementNdx);
}

InvalidShaderCompilerCase::Shaders InvalidShaderCompilerCase::createShaders(void) const
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    Shaders result;

    result.vertShader = gl.createShader(GL_VERTEX_SHADER);
    result.fragShader = gl.createShader(GL_FRAGMENT_SHADER);

    return result;
}

void InvalidShaderCompilerCase::setShaderSources(const Shaders &shaders, const ProgramContext &progCtx) const
{
    const glw::Functions &gl         = m_context.getRenderContext().getFunctions();
    const char *vertShaderSourceCStr = progCtx.vertShaderSource.c_str();
    const char *fragShaderSourceCStr = progCtx.fragShaderSource.c_str();
    gl.shaderSource(shaders.vertShader, 1, &vertShaderSourceCStr, nullptr);
    gl.shaderSource(shaders.fragShader, 1, &fragShaderSourceCStr, nullptr);
}

bool InvalidShaderCompilerCase::compileShader(uint32_t shader) const
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint status;
    gl.compileShader(shader);
    gl.getShaderiv(shader, GL_COMPILE_STATUS, &status);
    return status != 0;
}

void InvalidShaderCompilerCase::logProgramData(const BuildInfo &buildInfo, const ProgramContext &progCtx) const
{
    m_testCtx.getLog() << TestLog::ShaderProgram(false, "(No linking done)")
                       << TestLog::Shader(QP_SHADER_TYPE_VERTEX, progCtx.vertShaderSource, buildInfo.vertCompileSuccess,
                                          buildInfo.logs.vert)
                       << TestLog::Shader(QP_SHADER_TYPE_FRAGMENT, progCtx.fragShaderSource,
                                          buildInfo.fragCompileSuccess, buildInfo.logs.frag)
                       << TestLog::EndShaderProgram;
}

InvalidShaderCompilerCase::Logs InvalidShaderCompilerCase::getLogs(const Shaders &shaders) const
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    Logs result;

    result.vert = getShaderInfoLog(gl, shaders.vertShader);
    result.frag = getShaderInfoLog(gl, shaders.fragShader);

    return result;
}

void InvalidShaderCompilerCase::cleanup(const Shaders &shaders) const
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    gl.deleteShader(shaders.vertShader);
    gl.deleteShader(shaders.fragShader);
}

bool InvalidShaderCompilerCase::goodEnoughMeasurements(const vector<Measurement> &measurements) const
{
    if ((int)measurements.size() < m_minimumMeasurementCount)
        return false;
    else
    {
        if ((int)measurements.size() >= m_maximumMeasurementCount)
            return true;
        else
        {
            vector<int64_t> totalTimes;
            for (int i = 0; i < (int)measurements.size(); i++)
                totalTimes.push_back(measurements[i].totalTime());
            return vectorFloatRelativeMedianAbsoluteDeviation(vectorLowestPercentage(totalTimes, 0.5f)) <
                   RELATIVE_MEDIAN_ABSOLUTE_DEVIATION_THRESHOLD;
        }
    }
}

InvalidShaderCompilerCase::IterateResult InvalidShaderCompilerCase::iterate(void)
{
    ShaderValidity shaderValidity = m_invalidityType == INVALIDITY_INVALID_CHAR   ? SHADER_VALIDITY_INVALID_CHAR :
                                    m_invalidityType == INVALIDITY_SEMANTIC_ERROR ? SHADER_VALIDITY_SEMANTIC_ERROR :
                                                                                    SHADER_VALIDITY_LAST;

    DE_ASSERT(shaderValidity != SHADER_VALIDITY_LAST);

    // Before actual measurements, compile a minimal shader to avoid possible initial slowdowns in the actual test.
    {
        uint32_t specID = getSpecializationID(0);
        ProgramContext progCtx;
        progCtx.vertShaderSource = specializeShaderSource(singleVaryingVertexTemplate(), specID, shaderValidity);
        progCtx.fragShaderSource = specializeShaderSource(singleVaryingFragmentTemplate(), specID, shaderValidity);

        Shaders shaders = createShaders();
        setShaderSources(shaders, progCtx);

        BuildInfo buildInfo;
        buildInfo.vertCompileSuccess = compileShader(shaders.vertShader);
        buildInfo.fragCompileSuccess = compileShader(shaders.fragShader);
        if (buildInfo.vertCompileSuccess || buildInfo.fragCompileSuccess)
        {
            buildInfo.logs = getLogs(shaders);
            logProgramData(buildInfo, progCtx);
            cleanup(shaders);
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Compilation of a shader erroneously succeeded");
            return STOP;
        }
        cleanup(shaders);
    }

    vector<Measurement> measurements;
    // \note These are logged after measurements are done.
    ProgramContext latestProgramContext;
    BuildInfo latestBuildInfo;

    if (WARMUP_CPU_AT_BEGINNING_OF_CASE)
        tcu::warmupCPU();

    // Actual test measurements.
    while (!goodEnoughMeasurements(measurements))
    {
        // Create shader and compile. Measure time.

        // \note Shader sources are generated and GL shader objects are created before any time measurements.
        ProgramContext progCtx = generateShaderSources((int)measurements.size());
        Shaders shaders        = createShaders();
        BuildInfo buildInfo;

        if (WARMUP_CPU_BEFORE_EACH_MEASUREMENT)
            tcu::warmupCPU();

        // \note Do NOT do anything too hefty between the first and last deGetMicroseconds() here (other than the gl calls); it would disturb the measurement.

        uint64_t startTime = deGetMicroseconds();

        setShaderSources(shaders, progCtx);
        uint64_t shaderSourceSetEndTime = deGetMicroseconds();

        buildInfo.vertCompileSuccess        = compileShader(shaders.vertShader);
        uint64_t vertexShaderCompileEndTime = deGetMicroseconds();

        buildInfo.fragCompileSuccess          = compileShader(shaders.fragShader);
        uint64_t fragmentShaderCompileEndTime = deGetMicroseconds();

        buildInfo.logs = getLogs(shaders);

        // Both shader compilations should have failed.
        if (buildInfo.vertCompileSuccess || buildInfo.fragCompileSuccess)
        {
            logProgramData(buildInfo, progCtx);
            cleanup(shaders);
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Compilation of a shader erroneously succeeded");
            return STOP;
        }

        // De-initializations (delete shaders).

        cleanup(shaders);

        // Output measurement log later (after last measurement).

        measurements.push_back(Measurement((int64_t)(shaderSourceSetEndTime - startTime),
                                           (int64_t)(vertexShaderCompileEndTime - shaderSourceSetEndTime),
                                           (int64_t)(fragmentShaderCompileEndTime - vertexShaderCompileEndTime)));

        latestBuildInfo      = buildInfo;
        latestProgramContext = progCtx;

        m_testCtx.touchWatchdog(); // \note Measurements may take a while in a bad case.
    }

    // End of test case, log information about measurements.
    {
        TestLog &log = m_testCtx.getLog();

        vector<int64_t> sourceSetTimes;
        vector<int64_t> vertexCompileTimes;
        vector<int64_t> fragmentCompileTimes;
        vector<int64_t> totalTimes;

        log << TestLog::Section("IterationMeasurements", "Iteration measurements of compilation times");

        for (int ndx = 0; ndx < (int)measurements.size(); ndx++)
        {
            sourceSetTimes.push_back(measurements[ndx].sourceSetTime);
            vertexCompileTimes.push_back(measurements[ndx].vertexCompileTime);
            fragmentCompileTimes.push_back(measurements[ndx].fragmentCompileTime);
            totalTimes.push_back(measurements[ndx].totalTime());

            // Log this measurement.
            log << TestLog::Float("Measurement" + de::toString(ndx) + "Time",
                                  "Measurement " + de::toString(ndx) + " time", "ms", QP_KEY_TAG_TIME,
                                  (float)measurements[ndx].totalTime() / 1000.0f);
        }

        // Log some statistics.

        for (int entireRangeOrLowestHalf = 0; entireRangeOrLowestHalf < 2; entireRangeOrLowestHalf++)
        {
            bool isEntireRange         = entireRangeOrLowestHalf == 0;
            string statNamePrefix      = isEntireRange ? "" : "LowestHalf";
            vector<int64_t> rangeTimes = isEntireRange ? totalTimes : vectorLowestPercentage(totalTimes, 0.5f);

            log << TestLog::Message << "\nStatistics computed from " << (isEntireRange ? "all" : "only the lowest 50%")
                << " of the above measurements:" << TestLog::EndMessage;

#define LOG_TIME_STAT(NAME, DESC, FUNC)                                                                   \
    log << TestLog::Float(statNamePrefix + "TotalTime" + (NAME), (DESC) + string(" of total time"), "ms", \
                          QP_KEY_TAG_TIME, (FUNC)(rangeTimes) / 1000.0f)
#define LOG_RELATIVE_STAT(NAME, DESC, FUNC)                                                             \
    log << TestLog::Float(statNamePrefix + "TotalTime" + (NAME), (DESC) + string(" of total time"), "", \
                          QP_KEY_TAG_NONE, (FUNC)(rangeTimes))

            LOG_TIME_STAT("Median", "Median", vectorFloatMedian);
            LOG_TIME_STAT("Average", "Average", vectorFloatAverage);
            LOG_TIME_STAT("Minimum", "Minimum", vectorFloatMinimum);
            LOG_TIME_STAT("Maximum", "Maximum", vectorFloatMaximum);
            LOG_TIME_STAT("MedianAbsoluteDeviation", "Median absolute deviation", vectorFloatMedianAbsoluteDeviation);
            LOG_RELATIVE_STAT("RelativeMedianAbsoluteDeviation", "Relative median absolute deviation",
                              vectorFloatRelativeMedianAbsoluteDeviation);
            LOG_TIME_STAT("StandardDeviation", "Standard deviation", vectorFloatStandardDeviation);
            LOG_RELATIVE_STAT("RelativeStandardDeviation", "Relative standard deviation",
                              vectorFloatRelativeStandardDeviation);
            LOG_TIME_STAT("MaxMinusMin", "Max-min", vectorFloatMaximumMinusMinimum);
            LOG_RELATIVE_STAT("RelativeMaxMinusMin", "Relative max-min", vectorFloatRelativeMaximumMinusMinimum);

#undef LOG_TIME_STAT
#undef LOG_RELATIVE_STAT

            if (!isEntireRange &&
                vectorFloatRelativeMedianAbsoluteDeviation(rangeTimes) > RELATIVE_MEDIAN_ABSOLUTE_DEVIATION_THRESHOLD)
                log << TestLog::Message
                    << "\nWARNING: couldn't achieve relative median absolute deviation under threshold value "
                    << RELATIVE_MEDIAN_ABSOLUTE_DEVIATION_THRESHOLD << TestLog::EndMessage;
        }

        log << TestLog::EndSection; // End section IterationMeasurements

        for (int medianOrAverage = 0; medianOrAverage < 2; medianOrAverage++)
        {
            typedef float (*VecFunc)(const vector<int64_t> &);

            bool isMedian   = medianOrAverage == 0;
            string singular = isMedian ? "Median" : "Average";
            string plural   = singular + "s";
            VecFunc func    = isMedian ? (VecFunc)vectorFloatMedian<int64_t> : (VecFunc)vectorFloatAverage<int64_t>;

            log << TestLog::Section(plural + "PerPhase", plural + " per phase");

            for (int entireRangeOrLowestHalf = 0; entireRangeOrLowestHalf < 2; entireRangeOrLowestHalf++)
            {
                bool isEntireRange    = entireRangeOrLowestHalf == 0;
                string statNamePrefix = isEntireRange ? "" : "LowestHalf";
                float rangeSizeRatio  = isEntireRange ? 1.0f : 0.5f;

#define LOG_TIME(NAME, DESC, DATA)                                                                               \
    log << TestLog::Float(statNamePrefix + (NAME) + singular, singular + " of " + (DESC), "ms", QP_KEY_TAG_TIME, \
                          func(vectorLowestPercentage((DATA), rangeSizeRatio)) / 1000.0f)

                log << TestLog::Message
                    << (isEntireRange ? "For all measurements:" : "\nFor only the lowest 50% of the measurements:")
                    << TestLog::EndMessage;
                LOG_TIME("ShaderSourceSetTime", "shader source set time", sourceSetTimes);
                LOG_TIME("VertexShaderCompileTime", "vertex shader compile time", vertexCompileTimes);
                LOG_TIME("FragmentShaderCompileTime", "fragment shader compile time", fragmentCompileTimes);

#undef LOG_TIME
            }

            log << TestLog::EndSection;
        }

        // Set result.

        {
            log << TestLog::Message
                << "Note: test result is the first quartile (i.e. median of the lowest half of measurements) of total "
                   "times"
                << TestLog::EndMessage;
            float result = vectorFloatFirstQuartile(totalTimes) / 1000.0f;
            m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::floatToString(result, 2).c_str());
        }

        // Log shaders.

        log << TestLog::Message
            << "Note: the following shaders are the ones from the last iteration; variables' names and some constant "
               "expressions differ between iterations."
            << TestLog::EndMessage;

        logProgramData(latestBuildInfo, latestProgramContext);

        return STOP;
    }
}

InvalidShaderCompilerLightCase::InvalidShaderCompilerLightCase(Context &context, const char *name,
                                                               const char *description, int caseID,
                                                               InvalidityType invalidityType, bool isVertexCase,
                                                               int numLights, LightType lightType)
    : InvalidShaderCompilerCase(context, name, description, caseID, invalidityType)
    , m_isVertexCase(isVertexCase)
    , m_numLights(numLights)
    , m_lightType(lightType)
{
}

InvalidShaderCompilerLightCase::~InvalidShaderCompilerLightCase(void)
{
}

InvalidShaderCompilerCase::ProgramContext InvalidShaderCompilerLightCase::generateShaderSources(
    int measurementNdx) const
{
    uint32_t specID = getSpecializationID(measurementNdx);
    ProgramContext result;
    ShaderValidity shaderValidity = m_invalidityType == INVALIDITY_INVALID_CHAR   ? SHADER_VALIDITY_INVALID_CHAR :
                                    m_invalidityType == INVALIDITY_SEMANTIC_ERROR ? SHADER_VALIDITY_SEMANTIC_ERROR :
                                                                                    SHADER_VALIDITY_LAST;

    DE_ASSERT(shaderValidity != SHADER_VALIDITY_LAST);

    result.vertShaderSource =
        specializeShaderSource(lightVertexTemplate(m_numLights, m_isVertexCase, m_lightType), specID, shaderValidity);
    result.fragShaderSource =
        specializeShaderSource(lightFragmentTemplate(m_numLights, m_isVertexCase, m_lightType), specID, shaderValidity);

    return result;
}

InvalidShaderCompilerTextureCase::InvalidShaderCompilerTextureCase(Context &context, const char *name,
                                                                   const char *description, int caseID,
                                                                   InvalidityType invalidityType, int numLookups,
                                                                   ConditionalUsage conditionalUsage,
                                                                   ConditionalType conditionalType)
    : InvalidShaderCompilerCase(context, name, description, caseID, invalidityType)
    , m_numLookups(numLookups)
    , m_conditionalUsage(conditionalUsage)
    , m_conditionalType(conditionalType)
{
}

InvalidShaderCompilerTextureCase::~InvalidShaderCompilerTextureCase(void)
{
}

InvalidShaderCompilerCase::ProgramContext InvalidShaderCompilerTextureCase::generateShaderSources(
    int measurementNdx) const
{
    uint32_t specID = getSpecializationID(measurementNdx);
    ProgramContext result;
    ShaderValidity shaderValidity = m_invalidityType == INVALIDITY_INVALID_CHAR   ? SHADER_VALIDITY_INVALID_CHAR :
                                    m_invalidityType == INVALIDITY_SEMANTIC_ERROR ? SHADER_VALIDITY_SEMANTIC_ERROR :
                                                                                    SHADER_VALIDITY_LAST;

    DE_ASSERT(shaderValidity != SHADER_VALIDITY_LAST);

    result.vertShaderSource = specializeShaderSource(textureLookupVertexTemplate(m_conditionalUsage, m_conditionalType),
                                                     specID, shaderValidity);
    result.fragShaderSource = specializeShaderSource(
        textureLookupFragmentTemplate(m_numLookups, m_conditionalUsage, m_conditionalType), specID, shaderValidity);

    return result;
}

InvalidShaderCompilerLoopCase::InvalidShaderCompilerLoopCase(Context &context, const char *name,
                                                             const char *description, int caseID,
                                                             InvalidityType invalidityType, bool isVertexCase,
                                                             LoopType type, int numLoopIterations, int nestingDepth)
    : InvalidShaderCompilerCase(context, name, description, caseID, invalidityType)
    , m_isVertexCase(isVertexCase)
    , m_numLoopIterations(numLoopIterations)
    , m_nestingDepth(nestingDepth)
    , m_type(type)
{
}

InvalidShaderCompilerLoopCase::~InvalidShaderCompilerLoopCase(void)
{
}

InvalidShaderCompilerCase::ProgramContext InvalidShaderCompilerLoopCase::generateShaderSources(int measurementNdx) const
{
    uint32_t specID = getSpecializationID(measurementNdx);
    ProgramContext result;
    ShaderValidity shaderValidity = m_invalidityType == INVALIDITY_INVALID_CHAR   ? SHADER_VALIDITY_INVALID_CHAR :
                                    m_invalidityType == INVALIDITY_SEMANTIC_ERROR ? SHADER_VALIDITY_SEMANTIC_ERROR :
                                                                                    SHADER_VALIDITY_LAST;

    DE_ASSERT(shaderValidity != SHADER_VALIDITY_LAST);

    result.vertShaderSource = specializeShaderSource(
        loopVertexTemplate(m_type, m_isVertexCase, m_numLoopIterations, m_nestingDepth), specID, shaderValidity);
    result.fragShaderSource = specializeShaderSource(
        loopFragmentTemplate(m_type, m_isVertexCase, m_numLoopIterations, m_nestingDepth), specID, shaderValidity);

    return result;
}

InvalidShaderCompilerOperCase::InvalidShaderCompilerOperCase(Context &context, const char *name,
                                                             const char *description, int caseID,
                                                             InvalidityType invalidityType, bool isVertexCase,
                                                             const char *oper, int numOperations)
    : InvalidShaderCompilerCase(context, name, description, caseID, invalidityType)
    , m_isVertexCase(isVertexCase)
    , m_oper(oper)
    , m_numOperations(numOperations)
{
}

InvalidShaderCompilerOperCase::~InvalidShaderCompilerOperCase(void)
{
}

InvalidShaderCompilerCase::ProgramContext InvalidShaderCompilerOperCase::generateShaderSources(int measurementNdx) const
{
    uint32_t specID = getSpecializationID(measurementNdx);
    ProgramContext result;
    ShaderValidity shaderValidity = m_invalidityType == INVALIDITY_INVALID_CHAR   ? SHADER_VALIDITY_INVALID_CHAR :
                                    m_invalidityType == INVALIDITY_SEMANTIC_ERROR ? SHADER_VALIDITY_SEMANTIC_ERROR :
                                                                                    SHADER_VALIDITY_LAST;

    DE_ASSERT(shaderValidity != SHADER_VALIDITY_LAST);

    if (m_isVertexCase)
    {
        result.vertShaderSource =
            specializeShaderSource(binaryOpVertexTemplate(m_numOperations, m_oper.c_str()), specID, shaderValidity);
        result.fragShaderSource = specializeShaderSource(singleVaryingFragmentTemplate(), specID, shaderValidity);
    }
    else
    {
        result.vertShaderSource = specializeShaderSource(singleVaryingVertexTemplate(), specID, shaderValidity);
        result.fragShaderSource =
            specializeShaderSource(binaryOpFragmentTemplate(m_numOperations, m_oper.c_str()), specID, shaderValidity);
    }

    return result;
}

InvalidShaderCompilerMandelbrotCase::InvalidShaderCompilerMandelbrotCase(Context &context, const char *name,
                                                                         const char *description, int caseID,
                                                                         InvalidityType invalidityType,
                                                                         int numFractalIterations)
    : InvalidShaderCompilerCase(context, name, description, caseID, invalidityType)
    , m_numFractalIterations(numFractalIterations)
{
}

InvalidShaderCompilerMandelbrotCase::~InvalidShaderCompilerMandelbrotCase(void)
{
}

InvalidShaderCompilerCase::ProgramContext InvalidShaderCompilerMandelbrotCase::generateShaderSources(
    int measurementNdx) const
{
    uint32_t specID = getSpecializationID(measurementNdx);
    ProgramContext result;
    ShaderValidity shaderValidity = m_invalidityType == INVALIDITY_INVALID_CHAR   ? SHADER_VALIDITY_INVALID_CHAR :
                                    m_invalidityType == INVALIDITY_SEMANTIC_ERROR ? SHADER_VALIDITY_SEMANTIC_ERROR :
                                                                                    SHADER_VALIDITY_LAST;

    DE_ASSERT(shaderValidity != SHADER_VALIDITY_LAST);

    result.vertShaderSource = specializeShaderSource(mandelbrotVertexTemplate(), specID, shaderValidity);
    result.fragShaderSource =
        specializeShaderSource(mandelbrotFragmentTemplate(m_numFractalIterations), specID, shaderValidity);

    return result;
}

void addShaderCompilationPerformanceCases(TestCaseGroup &parentGroup)
{
    Context &context = parentGroup.getContext();
    int caseID       = 0; // Increment this after adding each case. Used for avoiding cache hits between cases.

    TestCaseGroup *validGroup   = new TestCaseGroup(context, "valid_shader", "Valid Shader Compiler Cases");
    TestCaseGroup *invalidGroup = new TestCaseGroup(context, "invalid_shader", "Invalid Shader Compiler Cases");
    TestCaseGroup *cacheGroup   = new TestCaseGroup(context, "cache", "Allow shader caching");
    parentGroup.addChild(validGroup);
    parentGroup.addChild(invalidGroup);
    parentGroup.addChild(cacheGroup);

    TestCaseGroup *invalidCharGroup =
        new TestCaseGroup(context, "invalid_char", "Invalid Character Shader Compiler Cases");
    TestCaseGroup *semanticErrorGroup =
        new TestCaseGroup(context, "semantic_error", "Semantic Error Shader Compiler Cases");
    invalidGroup->addChild(invalidCharGroup);
    invalidGroup->addChild(semanticErrorGroup);

    // Lighting shader compilation cases.

    {
        static const int lightCounts[] = {1, 2, 4, 8};

        TestCaseGroup *validLightingGroup = new TestCaseGroup(context, "lighting", "Shader Compiler Lighting Cases");
        TestCaseGroup *invalidCharLightingGroup =
            new TestCaseGroup(context, "lighting", "Invalid Character Shader Compiler Lighting Cases");
        TestCaseGroup *semanticErrorLightingGroup =
            new TestCaseGroup(context, "lighting", "Semantic Error Shader Compiler Lighting Cases");
        TestCaseGroup *cacheLightingGroup =
            new TestCaseGroup(context, "lighting", "Shader Compiler Lighting Cache Cases");
        validGroup->addChild(validLightingGroup);
        invalidCharGroup->addChild(invalidCharLightingGroup);
        semanticErrorGroup->addChild(semanticErrorLightingGroup);
        cacheGroup->addChild(cacheLightingGroup);

        for (int lightType = 0; lightType < (int)LIGHT_LAST; lightType++)
        {
            const char *lightTypeName = lightType == (int)LIGHT_DIRECTIONAL ? "directional" :
                                        lightType == (int)LIGHT_POINT       ? "point" :
                                                                              nullptr;

            DE_ASSERT(lightTypeName != nullptr);

            for (int isFrag = 0; isFrag <= 1; isFrag++)
            {
                bool isVertex           = isFrag == 0;
                const char *vertFragStr = isVertex ? "vertex" : "fragment";

                for (int lightCountNdx = 0; lightCountNdx < DE_LENGTH_OF_ARRAY(lightCounts); lightCountNdx++)
                {
                    int numLights = lightCounts[lightCountNdx];

                    string caseName =
                        string("") + lightTypeName + "_" + de::toString(numLights) + "_lights_" + vertFragStr;

                    // Valid shader case, no-cache and cache versions.

                    validLightingGroup->addChild(new ShaderCompilerLightCase(context, caseName.c_str(), "", caseID++,
                                                                             true /* avoid cache */, false, isVertex,
                                                                             numLights, (LightType)lightType));
                    cacheLightingGroup->addChild(new ShaderCompilerLightCase(context, caseName.c_str(), "", caseID++,
                                                                             false /* allow cache */, false, isVertex,
                                                                             numLights, (LightType)lightType));

                    // Invalid shader cases.

                    for (int invalidityType = 0; invalidityType < (int)InvalidShaderCompilerCase::INVALIDITY_LAST;
                         invalidityType++)
                    {
                        TestCaseGroup *curInvalidGroup =
                            invalidityType == (int)InvalidShaderCompilerCase::INVALIDITY_INVALID_CHAR ?
                                invalidCharLightingGroup :
                            invalidityType == (int)InvalidShaderCompilerCase::INVALIDITY_SEMANTIC_ERROR ?
                                semanticErrorLightingGroup :
                                nullptr;

                        DE_ASSERT(curInvalidGroup != nullptr);

                        curInvalidGroup->addChild(new InvalidShaderCompilerLightCase(
                            context, caseName.c_str(), "", caseID++,
                            (InvalidShaderCompilerCase::InvalidityType)invalidityType, isVertex, numLights,
                            (LightType)lightType));
                    }
                }
            }
        }
    }

    // Texture lookup shader compilation cases.

    {
        static const int texLookupCounts[] = {1, 2, 4, 8};

        TestCaseGroup *validTexGroup = new TestCaseGroup(context, "texture", "Shader Compiler Texture Lookup Cases");
        TestCaseGroup *invalidCharTexGroup =
            new TestCaseGroup(context, "texture", "Invalid Character Shader Compiler Texture Lookup Cases");
        TestCaseGroup *semanticErrorTexGroup =
            new TestCaseGroup(context, "texture", "Semantic Error Shader Compiler Texture Lookup Cases");
        TestCaseGroup *cacheTexGroup =
            new TestCaseGroup(context, "texture", "Shader Compiler Texture Lookup Cache Cases");
        validGroup->addChild(validTexGroup);
        invalidCharGroup->addChild(invalidCharTexGroup);
        semanticErrorGroup->addChild(semanticErrorTexGroup);
        cacheGroup->addChild(cacheTexGroup);

        for (int conditionalUsage = 0; conditionalUsage < (int)CONDITIONAL_USAGE_LAST; conditionalUsage++)
        {
            const char *conditionalUsageName = conditionalUsage == (int)CONDITIONAL_USAGE_NONE ? "no_conditionals" :
                                               conditionalUsage == (int)CONDITIONAL_USAGE_FIRST_HALF  ? "first_half" :
                                               conditionalUsage == (int)CONDITIONAL_USAGE_EVERY_OTHER ? "every_other" :
                                                                                                        nullptr;

            DE_ASSERT(conditionalUsageName != nullptr);

            int lastConditionalType = conditionalUsage == (int)CONDITIONAL_USAGE_NONE ? 1 : (int)CONDITIONAL_TYPE_LAST;

            for (int conditionalType = 0; conditionalType < lastConditionalType; conditionalType++)
            {
                const char *conditionalTypeName =
                    conditionalType == (int)CONDITIONAL_TYPE_STATIC  ? "static_conditionals" :
                    conditionalType == (int)CONDITIONAL_TYPE_UNIFORM ? "uniform_conditionals" :
                    conditionalType == (int)CONDITIONAL_TYPE_DYNAMIC ? "dynamic_conditionals" :
                                                                       nullptr;

                DE_ASSERT(conditionalTypeName != nullptr);

                for (int lookupCountNdx = 0; lookupCountNdx < DE_LENGTH_OF_ARRAY(texLookupCounts); lookupCountNdx++)
                {
                    int numLookups = texLookupCounts[lookupCountNdx];

                    string caseName =
                        de::toString(numLookups) + "_lookups_" + conditionalUsageName +
                        (conditionalUsage == (int)CONDITIONAL_USAGE_NONE ? "" : string("_") + conditionalTypeName);

                    // Valid shader case, no-cache and cache versions.

                    validTexGroup->addChild(new ShaderCompilerTextureCase(
                        context, caseName.c_str(), "", caseID++, true /* avoid cache */, false, numLookups,
                        (ConditionalUsage)conditionalUsage, (ConditionalType)conditionalType));
                    cacheTexGroup->addChild(new ShaderCompilerTextureCase(
                        context, caseName.c_str(), "", caseID++, false /* allow cache */, false, numLookups,
                        (ConditionalUsage)conditionalUsage, (ConditionalType)conditionalType));

                    // Invalid shader cases.

                    for (int invalidityType = 0; invalidityType < (int)InvalidShaderCompilerCase::INVALIDITY_LAST;
                         invalidityType++)
                    {
                        TestCaseGroup *curInvalidGroup =
                            invalidityType == (int)InvalidShaderCompilerCase::INVALIDITY_INVALID_CHAR ?
                                invalidCharTexGroup :
                            invalidityType == (int)InvalidShaderCompilerCase::INVALIDITY_SEMANTIC_ERROR ?
                                semanticErrorTexGroup :
                                nullptr;

                        DE_ASSERT(curInvalidGroup != nullptr);

                        curInvalidGroup->addChild(new InvalidShaderCompilerTextureCase(
                            context, caseName.c_str(), "", caseID++,
                            (InvalidShaderCompilerCase::InvalidityType)invalidityType, numLookups,
                            (ConditionalUsage)conditionalUsage, (ConditionalType)conditionalType));
                    }
                }
            }
        }
    }

    // Loop shader compilation cases.

    {
        static const int loopIterCounts[]    = {10, 100, 1000};
        static const int maxLoopNestingDepth = 3;
        static const int maxTotalLoopIterations =
            2000; // If <loop iteration count> ** <loop nesting depth> (where ** is exponentiation) exceeds this, don't generate the case.

        TestCaseGroup *validLoopGroup = new TestCaseGroup(context, "loop", "Shader Compiler Loop Cases");
        TestCaseGroup *invalidCharLoopGroup =
            new TestCaseGroup(context, "loop", "Invalid Character Shader Compiler Loop Cases");
        TestCaseGroup *semanticErrorLoopGroup =
            new TestCaseGroup(context, "loop", "Semantic Error Shader Compiler Loop Cases");
        TestCaseGroup *cacheLoopGroup = new TestCaseGroup(context, "loop", "Shader Compiler Loop Cache Cases");
        validGroup->addChild(validLoopGroup);
        invalidCharGroup->addChild(invalidCharLoopGroup);
        semanticErrorGroup->addChild(semanticErrorLoopGroup);
        cacheGroup->addChild(cacheLoopGroup);

        for (int loopType = 0; loopType < (int)LOOP_LAST; loopType++)
        {
            const char *loopTypeName = loopType == (int)LOOP_TYPE_STATIC  ? "static" :
                                       loopType == (int)LOOP_TYPE_UNIFORM ? "uniform" :
                                       loopType == (int)LOOP_TYPE_DYNAMIC ? "dynamic" :
                                                                            nullptr;

            DE_ASSERT(loopTypeName != nullptr);

            TestCaseGroup *validLoopTypeGroup         = new TestCaseGroup(context, loopTypeName, "");
            TestCaseGroup *invalidCharLoopTypeGroup   = new TestCaseGroup(context, loopTypeName, "");
            TestCaseGroup *semanticErrorLoopTypeGroup = new TestCaseGroup(context, loopTypeName, "");
            TestCaseGroup *cacheLoopTypeGroup         = new TestCaseGroup(context, loopTypeName, "");
            validLoopGroup->addChild(validLoopTypeGroup);
            invalidCharLoopGroup->addChild(invalidCharLoopTypeGroup);
            semanticErrorLoopGroup->addChild(semanticErrorLoopTypeGroup);
            cacheLoopGroup->addChild(cacheLoopTypeGroup);

            for (int isFrag = 0; isFrag <= 1; isFrag++)
            {
                bool isVertex           = isFrag == 0;
                const char *vertFragStr = isVertex ? "vertex" : "fragment";

                // \note Non-static loop cases with different iteration counts have identical shaders, so only make one of each.
                int loopIterCountMaxNdx = loopType != (int)LOOP_TYPE_STATIC ? 1 : DE_LENGTH_OF_ARRAY(loopIterCounts);

                for (int nestingDepth = 1; nestingDepth <= maxLoopNestingDepth; nestingDepth++)
                {
                    for (int loopIterCountNdx = 0; loopIterCountNdx < loopIterCountMaxNdx; loopIterCountNdx++)
                    {
                        int numIterations = loopIterCounts[loopIterCountNdx];

                        if (deFloatPow((float)numIterations, (float)nestingDepth) > (float)maxTotalLoopIterations)
                            continue; // Don't generate too heavy tasks.

                        string validCaseName = de::toString(numIterations) + "_iterations_" +
                                               de::toString(nestingDepth) + "_levels_" + vertFragStr;

                        // Valid shader case, no-cache and cache versions.

                        validLoopTypeGroup->addChild(new ShaderCompilerLoopCase(
                            context, validCaseName.c_str(), "", caseID++, true /* avoid cache */, false, isVertex,
                            (LoopType)loopType, numIterations, nestingDepth));
                        cacheLoopTypeGroup->addChild(new ShaderCompilerLoopCase(
                            context, validCaseName.c_str(), "", caseID++, false /* allow cache */, false, isVertex,
                            (LoopType)loopType, numIterations, nestingDepth));

                        // Invalid shader cases.

                        for (int invalidityType = 0; invalidityType < (int)InvalidShaderCompilerCase::INVALIDITY_LAST;
                             invalidityType++)
                        {
                            TestCaseGroup *curInvalidGroup =
                                invalidityType == (int)InvalidShaderCompilerCase::INVALIDITY_INVALID_CHAR ?
                                    invalidCharLoopTypeGroup :
                                invalidityType == (int)InvalidShaderCompilerCase::INVALIDITY_SEMANTIC_ERROR ?
                                    semanticErrorLoopTypeGroup :
                                    nullptr;

                            DE_ASSERT(curInvalidGroup != nullptr);

                            string invalidCaseName = de::toString(nestingDepth) + "_levels_" + vertFragStr;

                            if (loopType == (int)LOOP_TYPE_STATIC)
                                invalidCaseName =
                                    de::toString(numIterations) + "_iterations_" +
                                    invalidCaseName; // \note For invalid, non-static loop cases the iteration count means nothing (since no uniforms or attributes are set).

                            curInvalidGroup->addChild(new InvalidShaderCompilerLoopCase(
                                context, invalidCaseName.c_str(), "", caseID++,
                                (InvalidShaderCompilerCase::InvalidityType)invalidityType, isVertex, (LoopType)loopType,
                                numIterations, nestingDepth));
                        }
                    }
                }
            }
        }
    }

    // Multiplication shader compilation cases.

    {
        static const int multiplicationCounts[] = {10, 100, 1000};

        TestCaseGroup *validMulGroup =
            new TestCaseGroup(context, "multiplication", "Shader Compiler Multiplication Cases");
        TestCaseGroup *invalidCharMulGroup =
            new TestCaseGroup(context, "multiplication", "Invalid Character Shader Compiler Multiplication Cases");
        TestCaseGroup *semanticErrorMulGroup =
            new TestCaseGroup(context, "multiplication", "Semantic Error Shader Compiler Multiplication Cases");
        TestCaseGroup *cacheMulGroup =
            new TestCaseGroup(context, "multiplication", "Shader Compiler Multiplication Cache Cases");
        validGroup->addChild(validMulGroup);
        invalidCharGroup->addChild(invalidCharMulGroup);
        semanticErrorGroup->addChild(semanticErrorMulGroup);
        cacheGroup->addChild(cacheMulGroup);

        for (int isFrag = 0; isFrag <= 1; isFrag++)
        {
            bool isVertex           = isFrag == 0;
            const char *vertFragStr = isVertex ? "vertex" : "fragment";

            for (int operCountNdx = 0; operCountNdx < DE_LENGTH_OF_ARRAY(multiplicationCounts); operCountNdx++)
            {
                int numOpers = multiplicationCounts[operCountNdx];

                string caseName = de::toString(numOpers) + "_operations_" + vertFragStr;

                // Valid shader case, no-cache and cache versions.

                validMulGroup->addChild(new ShaderCompilerOperCase(
                    context, caseName.c_str(), "", caseID++, true /* avoid cache */, false, isVertex, "*", numOpers));
                cacheMulGroup->addChild(new ShaderCompilerOperCase(
                    context, caseName.c_str(), "", caseID++, false /* allow cache */, false, isVertex, "*", numOpers));

                // Invalid shader cases.

                for (int invalidityType = 0; invalidityType < (int)InvalidShaderCompilerCase::INVALIDITY_LAST;
                     invalidityType++)
                {
                    TestCaseGroup *curInvalidGroup =
                        invalidityType == (int)InvalidShaderCompilerCase::INVALIDITY_INVALID_CHAR ?
                            invalidCharMulGroup :
                        invalidityType == (int)InvalidShaderCompilerCase::INVALIDITY_SEMANTIC_ERROR ?
                            semanticErrorMulGroup :
                            nullptr;

                    DE_ASSERT(curInvalidGroup != nullptr);

                    curInvalidGroup->addChild(new InvalidShaderCompilerOperCase(
                        context, caseName.c_str(), "", caseID++,
                        (InvalidShaderCompilerCase::InvalidityType)invalidityType, isVertex, "*", numOpers));
                }
            }
        }
    }

    // Mandelbrot shader compilation cases.

    {
        static const int mandelbrotIterationCounts[] = {32, 64, 128};

        TestCaseGroup *validMandelbrotGroup =
            new TestCaseGroup(context, "mandelbrot", "Shader Compiler Mandelbrot Fractal Cases");
        TestCaseGroup *invalidCharMandelbrotGroup =
            new TestCaseGroup(context, "mandelbrot", "Invalid Character Shader Compiler Mandelbrot Fractal Cases");
        TestCaseGroup *semanticErrorMandelbrotGroup =
            new TestCaseGroup(context, "mandelbrot", "Semantic Error Shader Compiler Mandelbrot Fractal Cases");
        TestCaseGroup *cacheMandelbrotGroup =
            new TestCaseGroup(context, "mandelbrot", "Shader Compiler Mandelbrot Fractal Cache Cases");
        validGroup->addChild(validMandelbrotGroup);
        invalidCharGroup->addChild(invalidCharMandelbrotGroup);
        semanticErrorGroup->addChild(semanticErrorMandelbrotGroup);
        cacheGroup->addChild(cacheMandelbrotGroup);

        for (int iterCountNdx = 0; iterCountNdx < DE_LENGTH_OF_ARRAY(mandelbrotIterationCounts); iterCountNdx++)
        {
            int numFractalIterations = mandelbrotIterationCounts[iterCountNdx];
            string caseName          = de::toString(numFractalIterations) + "_iterations";

            // Valid shader case, no-cache and cache versions.

            validMandelbrotGroup->addChild(new ShaderCompilerMandelbrotCase(
                context, caseName.c_str(), "", caseID++, true /* avoid cache */, false, numFractalIterations));
            cacheMandelbrotGroup->addChild(new ShaderCompilerMandelbrotCase(
                context, caseName.c_str(), "", caseID++, false /* allow cache */, false, numFractalIterations));

            // Invalid shader cases.

            for (int invalidityType = 0; invalidityType < (int)InvalidShaderCompilerCase::INVALIDITY_LAST;
                 invalidityType++)
            {
                TestCaseGroup *curInvalidGroup =
                    invalidityType == (int)InvalidShaderCompilerCase::INVALIDITY_INVALID_CHAR ?
                        invalidCharMandelbrotGroup :
                    invalidityType == (int)InvalidShaderCompilerCase::INVALIDITY_SEMANTIC_ERROR ?
                        semanticErrorMandelbrotGroup :
                        nullptr;

                DE_ASSERT(curInvalidGroup != nullptr);

                curInvalidGroup->addChild(new InvalidShaderCompilerMandelbrotCase(
                    context, caseName.c_str(), "", caseID++, (InvalidShaderCompilerCase::InvalidityType)invalidityType,
                    numFractalIterations));
            }
        }
    }

    // Cases testing cache behaviour when whitespace and comments are added.

    {
        TestCaseGroup *whitespaceCommentCacheGroup = new TestCaseGroup(
            context, "cache_whitespace_comment", "Cases testing the effect of whitespace and comments on caching");
        parentGroup.addChild(whitespaceCommentCacheGroup);

        // \note Add just a small subset of the cases that were added above for the main performance tests.

        // Cases with both vertex and fragment variants.
        for (int isFrag = 0; isFrag <= 1; isFrag++)
        {
            bool isVertex        = isFrag == 0;
            string vtxFragSuffix = isVertex ? "_vertex" : "_fragment";
            string dirLightName  = "directional_2_lights" + vtxFragSuffix;
            string loopName      = "static_loop_100_iterations" + vtxFragSuffix;
            string multCase      = "multiplication_100_operations" + vtxFragSuffix;

            whitespaceCommentCacheGroup->addChild(new ShaderCompilerLightCase(
                context, dirLightName.c_str(), "", caseID++, false, true, isVertex, 2, LIGHT_DIRECTIONAL));
            whitespaceCommentCacheGroup->addChild(new ShaderCompilerLoopCase(
                context, loopName.c_str(), "", caseID++, false, true, isVertex, LOOP_TYPE_STATIC, 100, 1));
            whitespaceCommentCacheGroup->addChild(
                new ShaderCompilerOperCase(context, multCase.c_str(), "", caseID++, false, true, isVertex, "*", 100));
        }

        // Cases that don't have vertex and fragment variants.
        whitespaceCommentCacheGroup->addChild(new ShaderCompilerTextureCase(context, "texture_4_lookups", "", caseID++,
                                                                            false, true, 4, CONDITIONAL_USAGE_NONE,
                                                                            CONDITIONAL_TYPE_STATIC));
        whitespaceCommentCacheGroup->addChild(
            new ShaderCompilerMandelbrotCase(context, "mandelbrot_32_operations", "", caseID++, false, true, 32));
    }
}

} // namespace Performance
} // namespace gles3
} // namespace deqp
