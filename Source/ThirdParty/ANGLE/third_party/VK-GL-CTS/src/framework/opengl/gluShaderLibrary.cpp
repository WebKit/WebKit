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

#include "gluShaderLibrary.hpp"

#include "tcuStringTemplate.hpp"
#include "tcuResource.hpp"
#include "tcuTestLog.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deFilePath.hpp"

#include "glwEnums.hpp"

#include <sstream>
#include <map>
#include <cstdlib>

namespace glu
{
namespace sl
{

using namespace tcu;

using de::UniquePtr;
using std::map;
using std::ostringstream;
using std::pair;
using std::string;
using std::vector;

// Specification

bool isValid(const ValueBlock &block)
{
    for (size_t storageNdx = 0; storageNdx < 3; ++storageNdx)
    {
        const vector<Value> &values = storageNdx == 0 ? block.inputs : storageNdx == 1 ? block.outputs : block.uniforms;
        const size_t refArrayLen =
            values.empty() ? 0 : (values[0].elements.size() / (size_t)values[0].type.getScalarSize());

        for (size_t valNdx = 0; valNdx < values.size(); ++valNdx)
        {
            const Value &value = values[valNdx];

            if (!value.type.isBasicType())
            {
                print("ERROR: Value '%s' is of unsupported type!\n", value.name.c_str());
                return false;
            }

            if (value.elements.size() != refArrayLen * (size_t)value.type.getScalarSize())
            {
                print("ERROR: Value '%s' has invalid number of scalars!\n", value.name.c_str());
                return false;
            }
        }
    }

    return true;
}

bool isValid(const ShaderCaseSpecification &spec)
{
    const uint32_t vtxFragMask = (1u << SHADERTYPE_VERTEX) | (1u << SHADERTYPE_FRAGMENT);
    const uint32_t tessCtrlEvalMask =
        (1u << SHADERTYPE_TESSELLATION_CONTROL) | (1u << SHADERTYPE_TESSELLATION_EVALUATION);
    const uint32_t supportedStageMask = vtxFragMask | tessCtrlEvalMask | (1u << SHADERTYPE_GEOMETRY);
    const bool isSeparable            = !spec.programs.empty() && spec.programs[0].sources.separable;

    if (spec.programs.empty())
    {
        print("ERROR: No programs specified!\n");
        return false;
    }

    if (isCapabilityRequired(CAPABILITY_FULL_GLSL_ES_100_SUPPORT, spec))
    {
        if (spec.targetVersion != GLSL_VERSION_100_ES)
        {
            print("ERROR: Full GLSL ES 1.00 support requested for other GLSL version!\n");
            return false;
        }

        if (spec.expectResult != EXPECT_PASS && spec.expectResult != EXPECT_VALIDATION_FAIL &&
            spec.expectResult != EXPECT_BUILD_SUCCESSFUL)
        {
            print("ERROR: Full GLSL ES 1.00 support doesn't make sense when expecting compile/link failure!\n");
            return false;
        }
    }

    if (!de::inBounds(spec.caseType, (CaseType)0, CASETYPE_LAST))
    {
        print("ERROR: Invalid case type!\n");
        return false;
    }

    if (!de::inBounds(spec.expectResult, (ExpectResult)0, EXPECT_LAST))
    {
        print("ERROR: Invalid expected result!\n");
        return false;
    }

    if (!isValid(spec.values))
        return false;

    if (!spec.values.inputs.empty() && !spec.values.outputs.empty() &&
        spec.values.inputs[0].elements.size() / spec.values.inputs[0].type.getScalarSize() !=
            spec.values.outputs[0].elements.size() / spec.values.outputs[0].type.getScalarSize())
    {
        print("ERROR: Number of input and output elements don't match!\n");
        return false;
    }

    if (isSeparable)
    {
        uint32_t usedStageMask = 0u;

        if (spec.caseType != CASETYPE_COMPLETE)
        {
            print("ERROR: Separable shaders supported only for complete cases!\n");
            return false;
        }

        for (size_t progNdx = 0; progNdx < spec.programs.size(); ++progNdx)
        {
            for (int shaderStageNdx = 0; shaderStageNdx < SHADERTYPE_LAST; ++shaderStageNdx)
            {
                const uint32_t curStageMask = (1u << shaderStageNdx);

                if (supportedStageMask & curStageMask)
                {
                    const bool hasShader = !spec.programs[progNdx].sources.sources[shaderStageNdx].empty();
                    const bool isEnabled = (spec.programs[progNdx].activeStages & curStageMask) != 0;

                    if (hasShader != isEnabled)
                    {
                        print("ERROR: Inconsistent source/enable for shader stage %s!\n",
                              getShaderTypeName((ShaderType)shaderStageNdx));
                        return false;
                    }

                    if (hasShader && (usedStageMask & curStageMask) != 0)
                    {
                        print("ERROR: Stage %s enabled on multiple programs!\n",
                              getShaderTypeName((ShaderType)shaderStageNdx));
                        return false;
                    }

                    if (isEnabled)
                        usedStageMask |= curStageMask;
                }
                else if (!spec.programs[progNdx].sources.sources[shaderStageNdx].empty())
                {
                    print("ERROR: Source specified for unsupported shader stage %s!\n",
                          getShaderTypeName((ShaderType)shaderStageNdx));
                    return false;
                }
            }
        }

        if ((usedStageMask & vtxFragMask) != vtxFragMask)
        {
            print("ERROR: Vertex and fragment shaders are mandatory!\n");
            return false;
        }

        if ((usedStageMask & tessCtrlEvalMask) != 0 && (usedStageMask & tessCtrlEvalMask) != tessCtrlEvalMask)
        {
            print("ERROR: Both tessellation control and eval shaders must be either enabled or disabled!\n");
            return false;
        }
    }
    else
    {
        const bool hasVertex   = !spec.programs[0].sources.sources[SHADERTYPE_VERTEX].empty();
        const bool hasFragment = !spec.programs[0].sources.sources[SHADERTYPE_FRAGMENT].empty();

        if (spec.programs.size() != 1)
        {
            print("ERROR: Only cases using separable programs can have multiple programs!\n");
            return false;
        }

        if (spec.caseType == CASETYPE_VERTEX_ONLY && (!hasVertex || hasFragment))
        {
            print("ERROR: Vertex-only case must have only vertex shader!\n");
            return false;
        }

        if (spec.caseType == CASETYPE_FRAGMENT_ONLY && (hasVertex || !hasFragment))
        {
            print("ERROR: Fragment-only case must have only fragment shader!\n");
            return false;
        }

        if (spec.caseType == CASETYPE_COMPLETE && (!hasVertex || !hasFragment))
        {
            print("ERROR: Complete case must have at least vertex and fragment shaders\n");
            return false;
        }
    }

    return true;
}

bool isCapabilityRequired(CapabilityFlag capabilityFlag, const ShaderCaseSpecification &spec)
{
    std::vector<RequiredCapability>::const_iterator currRequirement = spec.requiredCaps.begin();
    while (currRequirement != spec.requiredCaps.end())
    {
        if ((currRequirement->type == CAPABILITY_FLAG) && (currRequirement->flagName == capabilityFlag))
            return true;
        ++currRequirement;
    }

    return false;
}

// Parser

static const glu::GLSLVersion DEFAULT_GLSL_VERSION = glu::GLSL_VERSION_100_ES;

inline bool isWhitespace(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

inline bool isEOL(char c)
{
    return (c == '\r') || (c == '\n');
}

inline bool isNumeric(char c)
{
    return deInRange32(c, '0', '9');
}

inline bool isAlpha(char c)
{
    return deInRange32(c, 'a', 'z') || deInRange32(c, 'A', 'Z');
}

inline bool isCaseNameChar(char c)
{
    return deInRange32(c, 'a', 'z') || deInRange32(c, 'A', 'Z') || deInRange32(c, '0', '9') || (c == '_') ||
           (c == '-') || (c == '.');
}

class ShaderParser
{
public:
    ShaderParser(const tcu::Archive &archive, const std::string &filename, ShaderCaseFactory *caseFactory);
    ~ShaderParser(void);

    vector<tcu::TestNode *> parse(void);

private:
    enum Token
    {
        TOKEN_INVALID = 0,
        TOKEN_EOF,
        TOKEN_STRING,
        TOKEN_SHADER_SOURCE,

        TOKEN_INT_LITERAL,
        TOKEN_FLOAT_LITERAL,

        // identifiers
        TOKEN_IDENTIFIER,
        TOKEN_TRUE,
        TOKEN_FALSE,
        TOKEN_DESC,
        TOKEN_EXPECT,
        TOKEN_GROUP,
        TOKEN_CASE,
        TOKEN_END,
        TOKEN_OUTPUT_COLOR,
        TOKEN_FORMAT,
        TOKEN_VALUES,
        TOKEN_BOTH,
        TOKEN_VERTEX,
        TOKEN_FRAGMENT,
        TOKEN_UNIFORM,
        TOKEN_INPUT,
        TOKEN_OUTPUT,
        TOKEN_FLOAT,
        TOKEN_FLOAT_VEC2,
        TOKEN_FLOAT_VEC3,
        TOKEN_FLOAT_VEC4,
        TOKEN_FLOAT_MAT2,
        TOKEN_FLOAT_MAT2X3,
        TOKEN_FLOAT_MAT2X4,
        TOKEN_FLOAT_MAT3X2,
        TOKEN_FLOAT_MAT3,
        TOKEN_FLOAT_MAT3X4,
        TOKEN_FLOAT_MAT4X2,
        TOKEN_FLOAT_MAT4X3,
        TOKEN_FLOAT_MAT4,
        TOKEN_INT,
        TOKEN_INT_VEC2,
        TOKEN_INT_VEC3,
        TOKEN_INT_VEC4,
        TOKEN_UINT,
        TOKEN_UINT_VEC2,
        TOKEN_UINT_VEC3,
        TOKEN_UINT_VEC4,
        TOKEN_BOOL,
        TOKEN_BOOL_VEC2,
        TOKEN_BOOL_VEC3,
        TOKEN_BOOL_VEC4,
        TOKEN_VERSION,
        TOKEN_TESSELLATION_CONTROL,
        TOKEN_TESSELLATION_EVALUATION,
        TOKEN_GEOMETRY,
        TOKEN_REQUIRE,
        TOKEN_IN,
        TOKEN_IMPORT,
        TOKEN_PIPELINE_PROGRAM,
        TOKEN_ACTIVE_STAGES,

        // symbols
        TOKEN_ASSIGN,
        TOKEN_PLUS,
        TOKEN_MINUS,
        TOKEN_COMMA,
        TOKEN_VERTICAL_BAR,
        TOKEN_SEMI_COLON,
        TOKEN_LEFT_PAREN,
        TOKEN_RIGHT_PAREN,
        TOKEN_LEFT_BRACKET,
        TOKEN_RIGHT_BRACKET,
        TOKEN_LEFT_BRACE,
        TOKEN_RIGHT_BRACE,
        TOKEN_GREATER,

        TOKEN_LAST
    };

    void parseError(const std::string &errorStr);
    float parseFloatLiteral(const char *str);
    int parseIntLiteral(const char *str);
    string parseStringLiteral(const char *str);
    string parseShaderSource(const char *str);
    void advanceToken(void);
    void advanceToken(Token assumed);
    void assumeToken(Token token);
    DataType mapDataTypeToken(Token token);
    const char *getTokenName(Token token);
    uint32_t getShaderStageLiteralFlag(void);
    uint32_t getGLEnumFromName(const std::string &enumName);

    void parseValueElement(DataType dataType, Value &result);
    void parseValue(ValueBlock &valueBlock);
    void parseValueBlock(ValueBlock &valueBlock);
    uint32_t parseShaderStageList(void);
    void parseRequirement(vector<RequiredCapability> &requiredCaps, vector<RequiredExtension> &requiredExts);
    void parseExpectResult(ExpectResult &expectResult);
    void parseFormat(DataType &format);
    void parseGLSLVersion(glu::GLSLVersion &version);
    void parsePipelineProgram(ProgramSpecification &program);
    void parseShaderCase(vector<tcu::TestNode *> &shaderNodeList);
    void parseShaderGroup(vector<tcu::TestNode *> &shaderNodeList);
    void parseImport(vector<tcu::TestNode *> &shaderNodeList);

    const tcu::Archive &m_archive;
    const string m_filename;
    ShaderCaseFactory *const m_caseFactory;

    UniquePtr<tcu::Resource> m_resource;
    vector<char> m_input;

    const char *m_curPtr;
    Token m_curToken;
    std::string m_curTokenStr;
};

ShaderParser::ShaderParser(const tcu::Archive &archive, const string &filename, ShaderCaseFactory *caseFactroy)
    : m_archive(archive)
    , m_filename(filename)
    , m_caseFactory(caseFactroy)
    , m_resource(archive.getResource(m_filename.c_str()))
    , m_curPtr(nullptr)
    , m_curToken(TOKEN_LAST)
{
}

ShaderParser::~ShaderParser(void)
{
}

void ShaderParser::parseError(const std::string &errorStr)
{
    string atStr = string(m_curPtr, 80);
    throw tcu::InternalError((string("Parser error: ") + errorStr + " near '" + atStr + " ...'").c_str(), nullptr,
                             __FILE__, __LINE__);
}

float ShaderParser::parseFloatLiteral(const char *str)
{
    return (float)atof(str);
}

int ShaderParser::parseIntLiteral(const char *str)
{
    return atoi(str);
}

string ShaderParser::parseStringLiteral(const char *str)
{
    const char *p = str;
    char endChar  = *p++;
    ostringstream o;

    while (*p != endChar && *p)
    {
        if (*p == '\\')
        {
            switch (p[1])
            {
            case 0:
                DE_ASSERT(false);
                break;
            case 'n':
                o << '\n';
                break;
            case 't':
                o << '\t';
                break;
            default:
                o << p[1];
                break;
            }

            p += 2;
        }
        else
            o << *p++;
    }

    return o.str();
}

static string removeExtraIndentation(const string &source)
{
    // Detect indentation from first line.
    int numIndentChars = 0;
    for (int ndx = 0; ndx < (int)source.length() && isWhitespace(source[ndx]); ndx++)
        numIndentChars += source[ndx] == '\t' ? 4 : 1;

    // Process all lines and remove preceding indentation.
    ostringstream processed;
    {
        bool atLineStart       = true;
        int indentCharsOmitted = 0;

        for (int pos = 0; pos < (int)source.length(); pos++)
        {
            char c = source[pos];

            if (atLineStart && indentCharsOmitted < numIndentChars && (c == ' ' || c == '\t'))
            {
                indentCharsOmitted += c == '\t' ? 4 : 1;
            }
            else if (isEOL(c))
            {
                if (source[pos] == '\r' && source[pos + 1] == '\n')
                {
                    pos += 1;
                    processed << '\n';
                }
                else
                    processed << c;

                atLineStart        = true;
                indentCharsOmitted = 0;
            }
            else
            {
                processed << c;
                atLineStart = false;
            }
        }
    }

    return processed.str();
}

string ShaderParser::parseShaderSource(const char *str)
{
    const char *p = str + 2;
    ostringstream o;

    // Eat first empty line from beginning.
    while (*p == ' ')
        p++;
    if (*p == '\r')
        p++;
    if (*p == '\n')
        p++;

    while ((p[0] != '"') || (p[1] != '"'))
    {
        if (*p == '\\')
        {
            switch (p[1])
            {
            case 0:
                DE_ASSERT(false);
                break;
            case 'n':
                o << '\n';
                break;
            case 't':
                o << '\t';
                break;
            default:
                o << p[1];
                break;
            }

            p += 2;
        }
        else
            o << *p++;
    }

    return removeExtraIndentation(o.str());
}

void ShaderParser::advanceToken(void)
{
    // Skip old token.
    m_curPtr += m_curTokenStr.length();

    // Reset token (for safety).
    m_curToken    = TOKEN_INVALID;
    m_curTokenStr = "";

    // Eat whitespace & comments while they last.
    for (;;)
    {
        while (isWhitespace(*m_curPtr))
            m_curPtr++;

        // Check for EOL comment.
        if (*m_curPtr == '#')
        {
            while (*m_curPtr && !isEOL(*m_curPtr))
                m_curPtr++;
        }
        else
            break;
    }

    if (!*m_curPtr)
    {
        m_curToken    = TOKEN_EOF;
        m_curTokenStr = "<EOF>";
    }
    else if (isAlpha(*m_curPtr))
    {
        struct Named
        {
            const char *str;
            Token token;
        };

        static const Named s_named[] = {
            {"true", TOKEN_TRUE},
            {"false", TOKEN_FALSE},
            {"desc", TOKEN_DESC},
            {"expect", TOKEN_EXPECT},
            {"group", TOKEN_GROUP},
            {"case", TOKEN_CASE},
            {"end", TOKEN_END},
            {"output_color", TOKEN_OUTPUT_COLOR},
            {"format", TOKEN_FORMAT},
            {"values", TOKEN_VALUES},
            {"both", TOKEN_BOTH},
            {"vertex", TOKEN_VERTEX},
            {"fragment", TOKEN_FRAGMENT},
            {"uniform", TOKEN_UNIFORM},
            {"input", TOKEN_INPUT},
            {"output", TOKEN_OUTPUT},
            {"float", TOKEN_FLOAT},
            {"vec2", TOKEN_FLOAT_VEC2},
            {"vec3", TOKEN_FLOAT_VEC3},
            {"vec4", TOKEN_FLOAT_VEC4},
            {"mat2", TOKEN_FLOAT_MAT2},
            {"mat2x3", TOKEN_FLOAT_MAT2X3},
            {"mat2x4", TOKEN_FLOAT_MAT2X4},
            {"mat3x2", TOKEN_FLOAT_MAT3X2},
            {"mat3", TOKEN_FLOAT_MAT3},
            {"mat3x4", TOKEN_FLOAT_MAT3X4},
            {"mat4x2", TOKEN_FLOAT_MAT4X2},
            {"mat4x3", TOKEN_FLOAT_MAT4X3},
            {"mat4", TOKEN_FLOAT_MAT4},
            {"int", TOKEN_INT},
            {"ivec2", TOKEN_INT_VEC2},
            {"ivec3", TOKEN_INT_VEC3},
            {"ivec4", TOKEN_INT_VEC4},
            {"uint", TOKEN_UINT},
            {"uvec2", TOKEN_UINT_VEC2},
            {"uvec3", TOKEN_UINT_VEC3},
            {"uvec4", TOKEN_UINT_VEC4},
            {"bool", TOKEN_BOOL},
            {"bvec2", TOKEN_BOOL_VEC2},
            {"bvec3", TOKEN_BOOL_VEC3},
            {"bvec4", TOKEN_BOOL_VEC4},
            {"version", TOKEN_VERSION},
            {"tessellation_control", TOKEN_TESSELLATION_CONTROL},
            {"tessellation_evaluation", TOKEN_TESSELLATION_EVALUATION},
            {"geometry", TOKEN_GEOMETRY},
            {"require", TOKEN_REQUIRE},
            {"in", TOKEN_IN},
            {"import", TOKEN_IMPORT},
            {"pipeline_program", TOKEN_PIPELINE_PROGRAM},
            {"active_stages", TOKEN_ACTIVE_STAGES},
        };

        const char *end = m_curPtr + 1;
        while (isCaseNameChar(*end))
            end++;
        m_curTokenStr = string(m_curPtr, end - m_curPtr);

        m_curToken = TOKEN_IDENTIFIER;

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_named); ndx++)
        {
            if (m_curTokenStr == s_named[ndx].str)
            {
                m_curToken = s_named[ndx].token;
                break;
            }
        }
    }
    else if (isNumeric(*m_curPtr))
    {
        /* \todo [2010-03-31 petri] Hex? */
        const char *p = m_curPtr;
        while (isNumeric(*p))
            p++;
        if (*p == '.')
        {
            p++;
            while (isNumeric(*p))
                p++;

            if (*p == 'e' || *p == 'E')
            {
                p++;
                if (*p == '+' || *p == '-')
                    p++;
                DE_ASSERT(isNumeric(*p));
                while (isNumeric(*p))
                    p++;
            }

            m_curToken    = TOKEN_FLOAT_LITERAL;
            m_curTokenStr = string(m_curPtr, p - m_curPtr);
        }
        else
        {
            m_curToken    = TOKEN_INT_LITERAL;
            m_curTokenStr = string(m_curPtr, p - m_curPtr);
        }
    }
    else if (*m_curPtr == '"' && m_curPtr[1] == '"')
    {
        const char *p = m_curPtr + 2;

        while ((p[0] != '"') || (p[1] != '"'))
        {
            DE_ASSERT(*p);
            if (*p == '\\')
            {
                DE_ASSERT(p[1] != 0);
                p += 2;
            }
            else
                p++;
        }
        p += 2;

        m_curToken    = TOKEN_SHADER_SOURCE;
        m_curTokenStr = string(m_curPtr, (int)(p - m_curPtr));
    }
    else if (*m_curPtr == '"' || *m_curPtr == '\'')
    {
        char endChar  = *m_curPtr;
        const char *p = m_curPtr + 1;

        while (*p != endChar)
        {
            DE_ASSERT(*p);
            if (*p == '\\')
            {
                DE_ASSERT(p[1] != 0);
                p += 2;
            }
            else
                p++;
        }
        p++;

        m_curToken    = TOKEN_STRING;
        m_curTokenStr = string(m_curPtr, (int)(p - m_curPtr));
    }
    else
    {
        struct SimpleToken
        {
            const char *str;
            Token token;
        };

        static const SimpleToken s_simple[] = {
            {"=", TOKEN_ASSIGN},       {"+", TOKEN_PLUS},          {"-", TOKEN_MINUS},      {",", TOKEN_COMMA},
            {"|", TOKEN_VERTICAL_BAR}, {";", TOKEN_SEMI_COLON},    {"(", TOKEN_LEFT_PAREN}, {")", TOKEN_RIGHT_PAREN},
            {"[", TOKEN_LEFT_BRACKET}, {"]", TOKEN_RIGHT_BRACKET}, {"{", TOKEN_LEFT_BRACE}, {"}", TOKEN_RIGHT_BRACE},
            {">", TOKEN_GREATER},
        };

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_simple); ndx++)
        {
            if (strncmp(s_simple[ndx].str, m_curPtr, strlen(s_simple[ndx].str)) == 0)
            {
                m_curToken    = s_simple[ndx].token;
                m_curTokenStr = s_simple[ndx].str;
                return;
            }
        }

        // Otherwise invalid token.
        m_curToken    = TOKEN_INVALID;
        m_curTokenStr = *m_curPtr;
    }
}

void ShaderParser::advanceToken(Token assumed)
{
    assumeToken(assumed);
    advanceToken();
}

void ShaderParser::assumeToken(Token token)
{
    if (m_curToken != token)
        parseError(
            (string("unexpected token '") + m_curTokenStr + "', expecting '" + getTokenName(token) + "'").c_str());
    DE_TEST_ASSERT(m_curToken == token);
}

DataType ShaderParser::mapDataTypeToken(Token token)
{
    switch (token)
    {
    case TOKEN_FLOAT:
        return TYPE_FLOAT;
    case TOKEN_FLOAT_VEC2:
        return TYPE_FLOAT_VEC2;
    case TOKEN_FLOAT_VEC3:
        return TYPE_FLOAT_VEC3;
    case TOKEN_FLOAT_VEC4:
        return TYPE_FLOAT_VEC4;
    case TOKEN_FLOAT_MAT2:
        return TYPE_FLOAT_MAT2;
    case TOKEN_FLOAT_MAT2X3:
        return TYPE_FLOAT_MAT2X3;
    case TOKEN_FLOAT_MAT2X4:
        return TYPE_FLOAT_MAT2X4;
    case TOKEN_FLOAT_MAT3X2:
        return TYPE_FLOAT_MAT3X2;
    case TOKEN_FLOAT_MAT3:
        return TYPE_FLOAT_MAT3;
    case TOKEN_FLOAT_MAT3X4:
        return TYPE_FLOAT_MAT3X4;
    case TOKEN_FLOAT_MAT4X2:
        return TYPE_FLOAT_MAT4X2;
    case TOKEN_FLOAT_MAT4X3:
        return TYPE_FLOAT_MAT4X3;
    case TOKEN_FLOAT_MAT4:
        return TYPE_FLOAT_MAT4;
    case TOKEN_INT:
        return TYPE_INT;
    case TOKEN_INT_VEC2:
        return TYPE_INT_VEC2;
    case TOKEN_INT_VEC3:
        return TYPE_INT_VEC3;
    case TOKEN_INT_VEC4:
        return TYPE_INT_VEC4;
    case TOKEN_UINT:
        return TYPE_UINT;
    case TOKEN_UINT_VEC2:
        return TYPE_UINT_VEC2;
    case TOKEN_UINT_VEC3:
        return TYPE_UINT_VEC3;
    case TOKEN_UINT_VEC4:
        return TYPE_UINT_VEC4;
    case TOKEN_BOOL:
        return TYPE_BOOL;
    case TOKEN_BOOL_VEC2:
        return TYPE_BOOL_VEC2;
    case TOKEN_BOOL_VEC3:
        return TYPE_BOOL_VEC3;
    case TOKEN_BOOL_VEC4:
        return TYPE_BOOL_VEC4;
    default:
        return TYPE_INVALID;
    }
}

const char *ShaderParser::getTokenName(Token token)
{
    switch (token)
    {
    case TOKEN_INVALID:
        return "<invalid>";
    case TOKEN_EOF:
        return "<eof>";
    case TOKEN_STRING:
        return "<string>";
    case TOKEN_SHADER_SOURCE:
        return "source";

    case TOKEN_INT_LITERAL:
        return "<int>";
    case TOKEN_FLOAT_LITERAL:
        return "<float>";

    // identifiers
    case TOKEN_IDENTIFIER:
        return "<identifier>";
    case TOKEN_TRUE:
        return "true";
    case TOKEN_FALSE:
        return "false";
    case TOKEN_DESC:
        return "desc";
    case TOKEN_EXPECT:
        return "expect";
    case TOKEN_GROUP:
        return "group";
    case TOKEN_CASE:
        return "case";
    case TOKEN_END:
        return "end";
    case TOKEN_VALUES:
        return "values";
    case TOKEN_BOTH:
        return "both";
    case TOKEN_VERTEX:
        return "vertex";
    case TOKEN_FRAGMENT:
        return "fragment";
    case TOKEN_TESSELLATION_CONTROL:
        return "tessellation_control";
    case TOKEN_TESSELLATION_EVALUATION:
        return "tessellation_evaluation";
    case TOKEN_GEOMETRY:
        return "geometry";
    case TOKEN_REQUIRE:
        return "require";
    case TOKEN_UNIFORM:
        return "uniform";
    case TOKEN_INPUT:
        return "input";
    case TOKEN_OUTPUT:
        return "output";
    case TOKEN_FLOAT:
        return "float";
    case TOKEN_FLOAT_VEC2:
        return "vec2";
    case TOKEN_FLOAT_VEC3:
        return "vec3";
    case TOKEN_FLOAT_VEC4:
        return "vec4";
    case TOKEN_FLOAT_MAT2:
        return "mat2";
    case TOKEN_FLOAT_MAT2X3:
        return "mat2x3";
    case TOKEN_FLOAT_MAT2X4:
        return "mat2x4";
    case TOKEN_FLOAT_MAT3X2:
        return "mat3x2";
    case TOKEN_FLOAT_MAT3:
        return "mat3";
    case TOKEN_FLOAT_MAT3X4:
        return "mat3x4";
    case TOKEN_FLOAT_MAT4X2:
        return "mat4x2";
    case TOKEN_FLOAT_MAT4X3:
        return "mat4x3";
    case TOKEN_FLOAT_MAT4:
        return "mat4";
    case TOKEN_INT:
        return "int";
    case TOKEN_INT_VEC2:
        return "ivec2";
    case TOKEN_INT_VEC3:
        return "ivec3";
    case TOKEN_INT_VEC4:
        return "ivec4";
    case TOKEN_UINT:
        return "uint";
    case TOKEN_UINT_VEC2:
        return "uvec2";
    case TOKEN_UINT_VEC3:
        return "uvec3";
    case TOKEN_UINT_VEC4:
        return "uvec4";
    case TOKEN_BOOL:
        return "bool";
    case TOKEN_BOOL_VEC2:
        return "bvec2";
    case TOKEN_BOOL_VEC3:
        return "bvec3";
    case TOKEN_BOOL_VEC4:
        return "bvec4";
    case TOKEN_IN:
        return "in";
    case TOKEN_IMPORT:
        return "import";
    case TOKEN_PIPELINE_PROGRAM:
        return "pipeline_program";
    case TOKEN_ACTIVE_STAGES:
        return "active_stages";

    case TOKEN_ASSIGN:
        return "=";
    case TOKEN_PLUS:
        return "+";
    case TOKEN_MINUS:
        return "-";
    case TOKEN_COMMA:
        return ",";
    case TOKEN_VERTICAL_BAR:
        return "|";
    case TOKEN_SEMI_COLON:
        return ";";
    case TOKEN_LEFT_PAREN:
        return "(";
    case TOKEN_RIGHT_PAREN:
        return ")";
    case TOKEN_LEFT_BRACKET:
        return "[";
    case TOKEN_RIGHT_BRACKET:
        return "]";
    case TOKEN_LEFT_BRACE:
        return "{";
    case TOKEN_RIGHT_BRACE:
        return "}";
    case TOKEN_GREATER:
        return ">";

    default:
        return "<unknown>";
    }
}

uint32_t ShaderParser::getShaderStageLiteralFlag(void)
{
    switch (m_curToken)
    {
    case TOKEN_VERTEX:
        return (1 << glu::SHADERTYPE_VERTEX);
    case TOKEN_FRAGMENT:
        return (1 << glu::SHADERTYPE_FRAGMENT);
    case TOKEN_GEOMETRY:
        return (1 << glu::SHADERTYPE_GEOMETRY);
    case TOKEN_TESSELLATION_CONTROL:
        return (1 << glu::SHADERTYPE_TESSELLATION_CONTROL);
    case TOKEN_TESSELLATION_EVALUATION:
        return (1 << glu::SHADERTYPE_TESSELLATION_EVALUATION);

    default:
        parseError(std::string() + "invalid shader stage name, got " + m_curTokenStr);
        return 0;
    }
}

uint32_t ShaderParser::getGLEnumFromName(const std::string &enumName)
{
    static const struct
    {
        const char *name;
        uint32_t value;
    } names[] = {
        {"GL_MAX_VERTEX_IMAGE_UNIFORMS", GL_MAX_VERTEX_IMAGE_UNIFORMS},
        {"GL_MAX_VERTEX_ATOMIC_COUNTERS", GL_MAX_VERTEX_ATOMIC_COUNTERS},
        {"GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS", GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS},
        {"GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS", GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS},
    };

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(names); ++ndx)
        if (names[ndx].name == enumName)
            return names[ndx].value;

    parseError(std::string() + "unknown enum name, got " + enumName);
    return 0;
}

void ShaderParser::parseValueElement(DataType expectedDataType, Value &result)
{
    DataType scalarType = getDataTypeScalarType(expectedDataType);
    int scalarSize      = getDataTypeScalarSize(expectedDataType);

    /* \todo [2010-04-19 petri] Support arrays. */
    Value::Element elems[16];

    if (scalarSize > 1)
    {
        DE_ASSERT(mapDataTypeToken(m_curToken) == expectedDataType);
        advanceToken(); // data type (float, vec2, etc.)
        advanceToken(TOKEN_LEFT_PAREN);
    }

    for (int scalarNdx = 0; scalarNdx < scalarSize; scalarNdx++)
    {
        if (scalarType == TYPE_FLOAT)
        {
            float signMult = 1.0f;
            if (m_curToken == TOKEN_MINUS)
            {
                signMult = -1.0f;
                advanceToken();
            }

            assumeToken(TOKEN_FLOAT_LITERAL);
            elems[scalarNdx].float32 = signMult * parseFloatLiteral(m_curTokenStr.c_str());
            advanceToken(TOKEN_FLOAT_LITERAL);
        }
        else if (scalarType == TYPE_INT || scalarType == TYPE_UINT)
        {
            int signMult = 1;
            if (m_curToken == TOKEN_MINUS)
            {
                signMult = -1;
                advanceToken();
            }

            assumeToken(TOKEN_INT_LITERAL);
            elems[scalarNdx].int32 = signMult * parseIntLiteral(m_curTokenStr.c_str());
            advanceToken(TOKEN_INT_LITERAL);
        }
        else
        {
            DE_ASSERT(scalarType == TYPE_BOOL);
            elems[scalarNdx].bool32 = (m_curToken == TOKEN_TRUE);
            if (m_curToken != TOKEN_TRUE && m_curToken != TOKEN_FALSE)
                parseError(string("unexpected token, expecting bool: " + m_curTokenStr));
            advanceToken(); // true/false
        }

        if (scalarNdx != (scalarSize - 1))
            advanceToken(TOKEN_COMMA);
    }

    if (scalarSize > 1)
        advanceToken(TOKEN_RIGHT_PAREN);

    // Store results.
    for (int scalarNdx = 0; scalarNdx < scalarSize; scalarNdx++)
        result.elements.push_back(elems[scalarNdx]);
}

void ShaderParser::parseValue(ValueBlock &valueBlock)
{
    // Parsed results.
    vector<Value> *dstBlock = nullptr;
    DataType basicType      = TYPE_LAST;
    std::string valueName;

    // Parse storage.
    if (m_curToken == TOKEN_UNIFORM)
        dstBlock = &valueBlock.uniforms;
    else if (m_curToken == TOKEN_INPUT)
        dstBlock = &valueBlock.inputs;
    else if (m_curToken == TOKEN_OUTPUT)
        dstBlock = &valueBlock.outputs;
    else
        parseError(string("unexpected token encountered when parsing value classifier"));
    advanceToken();

    // Parse data type.
    basicType = mapDataTypeToken(m_curToken);
    if (basicType == TYPE_INVALID)
        parseError(string("unexpected token when parsing value data type: " + m_curTokenStr));
    advanceToken();

    // Parse value name.
    if (m_curToken == TOKEN_IDENTIFIER || m_curToken == TOKEN_STRING)
    {
        if (m_curToken == TOKEN_IDENTIFIER)
            valueName = m_curTokenStr;
        else
            valueName = parseStringLiteral(m_curTokenStr.c_str());
    }
    else
        parseError(string("unexpected token when parsing value name: " + m_curTokenStr));
    advanceToken();

    // Parse assignment operator.
    advanceToken(TOKEN_ASSIGN);

    {
        Value value;
        value.name = valueName;
        value.type = VarType(basicType, PRECISION_LAST);
        dstBlock->push_back(value);
    }

    // Parse actual value.
    if (m_curToken == TOKEN_LEFT_BRACKET) // value list
    {
        advanceToken(TOKEN_LEFT_BRACKET);

        for (;;)
        {
            parseValueElement(basicType, dstBlock->back());

            if (m_curToken == TOKEN_RIGHT_BRACKET)
                break;
            else if (m_curToken == TOKEN_VERTICAL_BAR)
            {
                advanceToken();
                continue;
            }
            else
                parseError(string("unexpected token in value element array: " + m_curTokenStr));
        }

        advanceToken(TOKEN_RIGHT_BRACKET);
    }
    else //  single elements
    {
        parseValueElement(basicType, dstBlock->back());
    }

    advanceToken(TOKEN_SEMI_COLON); // end of declaration
}

void ShaderParser::parseValueBlock(ValueBlock &valueBlock)
{
    advanceToken(TOKEN_VALUES);
    advanceToken(TOKEN_LEFT_BRACE);

    for (;;)
    {
        if (m_curToken == TOKEN_UNIFORM || m_curToken == TOKEN_INPUT || m_curToken == TOKEN_OUTPUT)
            parseValue(valueBlock);
        else if (m_curToken == TOKEN_RIGHT_BRACE)
            break;
        else
            parseError(string("unexpected token when parsing a value block: " + m_curTokenStr));
    }

    advanceToken(TOKEN_RIGHT_BRACE);
}

uint32_t ShaderParser::parseShaderStageList(void)
{
    uint32_t mask = 0;

    assumeToken(TOKEN_LEFT_BRACE);

    // don't allow 0-sized lists
    advanceToken();
    mask |= getShaderStageLiteralFlag();
    advanceToken();

    for (;;)
    {
        if (m_curToken == TOKEN_RIGHT_BRACE)
            break;
        else if (m_curToken == TOKEN_COMMA)
        {
            uint32_t stageFlag;
            advanceToken();

            stageFlag = getShaderStageLiteralFlag();
            if (stageFlag & mask)
                parseError(string("stage already set in the shader stage set: " + m_curTokenStr));

            mask |= stageFlag;
            advanceToken();
        }
        else
            parseError(string("invalid shader stage set token: " + m_curTokenStr));
    }
    advanceToken(TOKEN_RIGHT_BRACE);

    return mask;
}

void ShaderParser::parseRequirement(vector<RequiredCapability> &requiredCaps, vector<RequiredExtension> &requiredExts)
{
    advanceToken();
    assumeToken(TOKEN_IDENTIFIER);

    if (m_curTokenStr == "extension")
    {
        std::vector<std::string> anyExtensionStringList;
        uint32_t affectedCasesFlags = -1; // by default all stages

        advanceToken();
        assumeToken(TOKEN_LEFT_BRACE);

        advanceToken();
        assumeToken(TOKEN_STRING);

        anyExtensionStringList.push_back(parseStringLiteral(m_curTokenStr.c_str()));
        advanceToken();

        for (;;)
        {
            if (m_curToken == TOKEN_RIGHT_BRACE)
                break;
            else if (m_curToken == TOKEN_VERTICAL_BAR)
            {
                advanceToken();
                assumeToken(TOKEN_STRING);

                anyExtensionStringList.push_back(parseStringLiteral(m_curTokenStr.c_str()));
                advanceToken();
            }
            else
                parseError(string("invalid extension list token: " + m_curTokenStr));
        }
        advanceToken(TOKEN_RIGHT_BRACE);

        if (m_curToken == TOKEN_IN)
        {
            advanceToken();
            affectedCasesFlags = parseShaderStageList();
        }

        requiredExts.push_back(RequiredExtension(anyExtensionStringList, affectedCasesFlags));
    }
    else if (m_curTokenStr == "limit")
    {
        uint32_t limitEnum;
        int limitValue;

        advanceToken();

        assumeToken(TOKEN_STRING);
        limitEnum = getGLEnumFromName(parseStringLiteral(m_curTokenStr.c_str()));
        advanceToken();

        assumeToken(TOKEN_GREATER);
        advanceToken();

        assumeToken(TOKEN_INT_LITERAL);
        limitValue = parseIntLiteral(m_curTokenStr.c_str());
        advanceToken();

        requiredCaps.push_back(RequiredCapability(limitEnum, limitValue));
    }
    else if (m_curTokenStr == "full_glsl_es_100_support")
    {
        advanceToken();

        requiredCaps.push_back(RequiredCapability(CAPABILITY_FULL_GLSL_ES_100_SUPPORT));
    }
    else if (m_curTokenStr == "only_glsl_es_100_support")
    {
        advanceToken();

        requiredCaps.push_back(RequiredCapability(CAPABILITY_ONLY_GLSL_ES_100_SUPPORT));
    }
    else if (m_curTokenStr == "exactly_one_draw_buffer")
    {
        advanceToken();

        requiredCaps.push_back(RequiredCapability(CAPABILITY_EXACTLY_ONE_DRAW_BUFFER));
    }
    else
        parseError(string("invalid requirement value: " + m_curTokenStr));
}

void ShaderParser::parseExpectResult(ExpectResult &expectResult)
{
    assumeToken(TOKEN_IDENTIFIER);

    if (m_curTokenStr == "pass")
        expectResult = EXPECT_PASS;
    else if (m_curTokenStr == "compile_fail")
        expectResult = EXPECT_COMPILE_FAIL;
    else if (m_curTokenStr == "link_fail")
        expectResult = EXPECT_LINK_FAIL;
    else if (m_curTokenStr == "compile_or_link_fail")
        expectResult = EXPECT_COMPILE_LINK_FAIL;
    else if (m_curTokenStr == "validation_fail")
        expectResult = EXPECT_VALIDATION_FAIL;
    else if (m_curTokenStr == "build_successful")
        expectResult = EXPECT_BUILD_SUCCESSFUL;
    else
        parseError(string("invalid expected result value: " + m_curTokenStr));

    advanceToken();
}

void ShaderParser::parseFormat(DataType &format)
{
    format = mapDataTypeToken(m_curToken);
    advanceToken();
}

void ShaderParser::parseGLSLVersion(glu::GLSLVersion &version)
{
    int versionNum      = 0;
    std::string postfix = "";

    assumeToken(TOKEN_INT_LITERAL);
    versionNum = parseIntLiteral(m_curTokenStr.c_str());
    advanceToken();

    if (m_curToken == TOKEN_IDENTIFIER)
    {
        postfix = m_curTokenStr;
        advanceToken();
    }

    DE_STATIC_ASSERT(glu::GLSL_VERSION_LAST == 15);

    if (versionNum == 100 && postfix == "es")
        version = glu::GLSL_VERSION_100_ES;
    else if (versionNum == 300 && postfix == "es")
        version = glu::GLSL_VERSION_300_ES;
    else if (versionNum == 310 && postfix == "es")
        version = glu::GLSL_VERSION_310_ES;
    else if (versionNum == 320 && postfix == "es")
        version = glu::GLSL_VERSION_320_ES;
    else if (versionNum == 130)
        version = glu::GLSL_VERSION_130;
    else if (versionNum == 140)
        version = glu::GLSL_VERSION_140;
    else if (versionNum == 150)
        version = glu::GLSL_VERSION_150;
    else if (versionNum == 330)
        version = glu::GLSL_VERSION_330;
    else if (versionNum == 400)
        version = glu::GLSL_VERSION_400;
    else if (versionNum == 410)
        version = glu::GLSL_VERSION_410;
    else if (versionNum == 420)
        version = glu::GLSL_VERSION_420;
    else if (versionNum == 430)
        version = glu::GLSL_VERSION_430;
    else if (versionNum == 440)
        version = glu::GLSL_VERSION_440;
    else if (versionNum == 450)
        version = glu::GLSL_VERSION_450;
    else if (versionNum == 460)
        version = glu::GLSL_VERSION_460;
    else
        parseError("Unknown GLSL version");
}

void ShaderParser::parsePipelineProgram(ProgramSpecification &program)
{
    advanceToken(TOKEN_PIPELINE_PROGRAM);

    for (;;)
    {
        if (m_curToken == TOKEN_END)
            break;
        else if (m_curToken == TOKEN_ACTIVE_STAGES)
        {
            advanceToken();
            program.activeStages = parseShaderStageList();
        }
        else if (m_curToken == TOKEN_REQUIRE)
        {
            vector<RequiredCapability> unusedCaps;
            size_t size = program.requiredExtensions.size();
            parseRequirement(unusedCaps, program.requiredExtensions);

            if (size == program.requiredExtensions.size())
                parseError("only extension requirements are allowed inside pipeline program");
        }
        else if (m_curToken == TOKEN_VERTEX || m_curToken == TOKEN_FRAGMENT ||
                 m_curToken == TOKEN_TESSELLATION_CONTROL || m_curToken == TOKEN_TESSELLATION_EVALUATION ||
                 m_curToken == TOKEN_GEOMETRY)
        {
            const Token token = m_curToken;
            string source;

            advanceToken();
            assumeToken(TOKEN_SHADER_SOURCE);
            source = parseShaderSource(m_curTokenStr.c_str());
            advanceToken();

            switch (token)
            {
            case TOKEN_VERTEX:
                program.sources.sources[SHADERTYPE_VERTEX].push_back(source);
                break;
            case TOKEN_FRAGMENT:
                program.sources.sources[SHADERTYPE_FRAGMENT].push_back(source);
                break;
            case TOKEN_TESSELLATION_CONTROL:
                program.sources.sources[SHADERTYPE_TESSELLATION_CONTROL].push_back(source);
                break;
            case TOKEN_TESSELLATION_EVALUATION:
                program.sources.sources[SHADERTYPE_TESSELLATION_EVALUATION].push_back(source);
                break;
            case TOKEN_GEOMETRY:
                program.sources.sources[SHADERTYPE_GEOMETRY].push_back(source);
                break;
            default:
                DE_FATAL("Unreachable");
            }
        }
        else
            parseError(string("invalid pipeline program value: " + m_curTokenStr));
    }
    advanceToken(TOKEN_END);

    if (program.activeStages == 0)
        parseError("program pipeline object must have active stages");
}

void ShaderParser::parseShaderCase(vector<tcu::TestNode *> &shaderNodeList)
{
    // Parse 'case'.
    advanceToken(TOKEN_CASE);

    // Parse case name.
    string caseName = m_curTokenStr;
    advanceToken(); // \note [pyry] All token types are allowed here.

    // \todo [pyry] Optimize by parsing most stuff directly to ShaderCaseSpecification

    // Setup case.
    GLSLVersion version       = DEFAULT_GLSL_VERSION;
    ExpectResult expectResult = EXPECT_PASS;
    OutputType outputType     = OUTPUT_RESULT;
    DataType format           = TYPE_LAST;
    string description;
    string bothSource;
    vector<string> vertexSources;
    vector<string> fragmentSources;
    vector<string> tessellationCtrlSources;
    vector<string> tessellationEvalSources;
    vector<string> geometrySources;
    ValueBlock valueBlock;
    bool valueBlockSeen = false;
    vector<RequiredCapability> requiredCaps;
    vector<RequiredExtension> requiredExts;
    vector<ProgramSpecification> pipelinePrograms;

    for (;;)
    {
        if (m_curToken == TOKEN_END)
            break;
        else if (m_curToken == TOKEN_DESC)
        {
            advanceToken();
            assumeToken(TOKEN_STRING);
            description = parseStringLiteral(m_curTokenStr.c_str());
            advanceToken();
        }
        else if (m_curToken == TOKEN_EXPECT)
        {
            advanceToken();
            parseExpectResult(expectResult);
        }
        else if (m_curToken == TOKEN_OUTPUT_COLOR)
        {
            outputType = OUTPUT_COLOR;
            advanceToken();
            parseFormat(format);
        }
        else if (m_curToken == TOKEN_VALUES)
        {
            if (valueBlockSeen)
                parseError("multiple value blocks");
            parseValueBlock(valueBlock);
            valueBlockSeen = true;
        }
        else if (m_curToken == TOKEN_BOTH || m_curToken == TOKEN_VERTEX || m_curToken == TOKEN_FRAGMENT ||
                 m_curToken == TOKEN_TESSELLATION_CONTROL || m_curToken == TOKEN_TESSELLATION_EVALUATION ||
                 m_curToken == TOKEN_GEOMETRY)
        {
            const Token token = m_curToken;
            string source;

            advanceToken();
            assumeToken(TOKEN_SHADER_SOURCE);
            source = parseShaderSource(m_curTokenStr.c_str());
            advanceToken();

            switch (token)
            {
            case TOKEN_VERTEX:
                vertexSources.push_back(source);
                break;
            case TOKEN_FRAGMENT:
                fragmentSources.push_back(source);
                break;
            case TOKEN_TESSELLATION_CONTROL:
                tessellationCtrlSources.push_back(source);
                break;
            case TOKEN_TESSELLATION_EVALUATION:
                tessellationEvalSources.push_back(source);
                break;
            case TOKEN_GEOMETRY:
                geometrySources.push_back(source);
                break;
            case TOKEN_BOTH:
            {
                if (!bothSource.empty())
                    parseError("multiple 'both' blocks");
                bothSource = source;
                break;
            }

            default:
                DE_FATAL("Unreachable");
            }
        }
        else if (m_curToken == TOKEN_VERSION)
        {
            advanceToken();
            parseGLSLVersion(version);
        }
        else if (m_curToken == TOKEN_REQUIRE)
        {
            parseRequirement(requiredCaps, requiredExts);
        }
        else if (m_curToken == TOKEN_PIPELINE_PROGRAM)
        {
            ProgramSpecification pipelineProgram;
            parsePipelineProgram(pipelineProgram);
            pipelineProgram.sources.separable = true;
            pipelinePrograms.push_back(pipelineProgram);
        }
        else
            parseError(string("unexpected token while parsing shader case: " + m_curTokenStr));
    }

    advanceToken(TOKEN_END); // case end

    if (!bothSource.empty())
    {
        if (!vertexSources.empty() || !fragmentSources.empty() || !tessellationCtrlSources.empty() ||
            !tessellationEvalSources.empty() || !geometrySources.empty() || !pipelinePrograms.empty())
        {
            parseError("'both' cannot be mixed with other shader stages");
        }

        // vertex
        {
            ShaderCaseSpecification spec;
            spec.caseType      = CASETYPE_VERTEX_ONLY;
            spec.expectResult  = expectResult;
            spec.targetVersion = version;
            spec.requiredCaps  = requiredCaps;
            spec.values        = valueBlock;

            spec.programs.resize(1);
            spec.programs[0].sources << VertexSource(bothSource);
            spec.programs[0].requiredExtensions = requiredExts;

            shaderNodeList.push_back(
                m_caseFactory->createCase(caseName + "_vertex", description, ShaderCaseSpecification(spec)));
        }

        // fragment
        {
            ShaderCaseSpecification spec;
            spec.caseType      = CASETYPE_FRAGMENT_ONLY;
            spec.expectResult  = expectResult;
            spec.targetVersion = version;
            spec.requiredCaps  = requiredCaps;
            spec.values        = valueBlock;

            spec.programs.resize(1);
            spec.programs[0].sources << FragmentSource(bothSource);
            spec.programs[0].requiredExtensions = requiredExts;

            shaderNodeList.push_back(
                m_caseFactory->createCase(caseName + "_fragment", description, ShaderCaseSpecification(spec)));
        }
    }
    else if (pipelinePrograms.empty())
    {
        ShaderCaseSpecification spec;
        spec.caseType      = CASETYPE_COMPLETE;
        spec.expectResult  = expectResult;
        spec.outputType    = outputType;
        spec.outputFormat  = format;
        spec.targetVersion = version;
        spec.requiredCaps  = requiredCaps;
        spec.values        = valueBlock;

        spec.programs.resize(1);
        spec.programs[0].sources.sources[SHADERTYPE_VERTEX].swap(vertexSources);
        spec.programs[0].sources.sources[SHADERTYPE_FRAGMENT].swap(fragmentSources);
        spec.programs[0].sources.sources[SHADERTYPE_TESSELLATION_CONTROL].swap(tessellationCtrlSources);
        spec.programs[0].sources.sources[SHADERTYPE_TESSELLATION_EVALUATION].swap(tessellationEvalSources);
        spec.programs[0].sources.sources[SHADERTYPE_GEOMETRY].swap(geometrySources);
        spec.programs[0].requiredExtensions.swap(requiredExts);

        shaderNodeList.push_back(m_caseFactory->createCase(caseName, description, ShaderCaseSpecification(spec)));
    }
    else
    {
        if (!vertexSources.empty() || !fragmentSources.empty() || !tessellationCtrlSources.empty() ||
            !tessellationEvalSources.empty() || !geometrySources.empty())
        {
            parseError("pipeline programs cannot be mixed with complete programs");
        }

        if (!requiredExts.empty())
            parseError("global extension requirements cannot be mixed with pipeline programs");

        // Pipeline case, multiple programs
        {
            ShaderCaseSpecification spec;
            spec.caseType      = CASETYPE_COMPLETE;
            spec.expectResult  = expectResult;
            spec.targetVersion = version;
            spec.requiredCaps  = requiredCaps;
            spec.values        = valueBlock;

            spec.programs.swap(pipelinePrograms);

            shaderNodeList.push_back(m_caseFactory->createCase(caseName, description, ShaderCaseSpecification(spec)));
        }
    }
}

void ShaderParser::parseShaderGroup(vector<tcu::TestNode *> &shaderNodeList)
{
    // Parse 'case'.
    advanceToken(TOKEN_GROUP);

    // Parse case name.
    string name = m_curTokenStr;
    advanceToken(); // \note [pyry] We don't want to check token type here (for instance to allow "uniform") group.

    // Parse description.
    assumeToken(TOKEN_STRING);
    string description = parseStringLiteral(m_curTokenStr.c_str());
    advanceToken(TOKEN_STRING);

    std::vector<tcu::TestNode *> children;

    // Parse group children.
    for (;;)
    {
        if (m_curToken == TOKEN_END)
            break;
        else if (m_curToken == TOKEN_GROUP)
            parseShaderGroup(children);
        else if (m_curToken == TOKEN_CASE)
            parseShaderCase(children);
        else if (m_curToken == TOKEN_IMPORT)
            parseImport(children);
        else
            parseError(string("unexpected token while parsing shader group: " + m_curTokenStr));
    }

    advanceToken(TOKEN_END); // group end

    // Create group node.
    tcu::TestCaseGroup *groupNode = m_caseFactory->createGroup(name, description, children);
    shaderNodeList.push_back(groupNode);
}

void ShaderParser::parseImport(vector<tcu::TestNode *> &shaderNodeList)
{
    std::string importFileName;

    advanceToken(TOKEN_IMPORT);

    assumeToken(TOKEN_STRING);
    importFileName = parseStringLiteral(m_curTokenStr.c_str());
    advanceToken(TOKEN_STRING);

    {
        ShaderParser subParser(m_archive,
                               de::FilePath::join(de::FilePath(m_filename).getDirName(), importFileName).getPath(),
                               m_caseFactory);
        const vector<tcu::TestNode *> importedCases = subParser.parse();

        // \todo [2015-08-03 pyry] Not exception safe
        shaderNodeList.insert(shaderNodeList.end(), importedCases.begin(), importedCases.end());
    }
}

vector<tcu::TestNode *> ShaderParser::parse(void)
{
    const int dataLen = m_resource->getSize();

    m_input.resize(dataLen + 1);
    m_resource->setPosition(0);
    m_resource->read((uint8_t *)&m_input[0], dataLen);
    m_input[dataLen] = '\0';

    // Initialize parser.
    m_curPtr      = &m_input[0];
    m_curToken    = TOKEN_INVALID;
    m_curTokenStr = "";
    advanceToken();

    vector<tcu::TestNode *> nodeList;

    // Parse all cases.
    for (;;)
    {
        if (m_curToken == TOKEN_CASE)
            parseShaderCase(nodeList);
        else if (m_curToken == TOKEN_GROUP)
            parseShaderGroup(nodeList);
        else if (m_curToken == TOKEN_IMPORT)
            parseImport(nodeList);
        else if (m_curToken == TOKEN_EOF)
            break;
        else
            parseError(string("invalid token encountered at main level: '") + m_curTokenStr + "'");
    }

    assumeToken(TOKEN_EOF);
    // printf("  parsed %d test cases.\n", caseList.size());
    return nodeList;
}

std::vector<tcu::TestNode *> parseFile(const tcu::Archive &archive, const std::string &filename,
                                       ShaderCaseFactory *caseFactory)
{
    sl::ShaderParser parser(archive, filename, caseFactory);

    return parser.parse();
}

// Execution utilities

static void dumpValue(tcu::TestLog &log, const Value &val, const char *storageName, int arrayNdx)
{
    const char *const valueName = val.name.c_str();
    const DataType dataType     = val.type.getBasicType();
    int scalarSize              = getDataTypeScalarSize(dataType);
    ostringstream result;

    result << "    " << storageName << " ";

    result << getDataTypeName(dataType) << " " << valueName << ":";

    if (isDataTypeScalar(dataType))
        result << " ";
    if (isDataTypeVector(dataType))
        result << " [ ";
    else if (isDataTypeMatrix(dataType))
        result << "\n";

    if (isDataTypeScalarOrVector(dataType))
    {
        for (int scalarNdx = 0; scalarNdx < scalarSize; scalarNdx++)
        {
            int elemNdx             = arrayNdx;
            const Value::Element &e = val.elements[elemNdx * scalarSize + scalarNdx];
            result << ((scalarNdx != 0) ? ", " : "");

            if (isDataTypeFloatOrVec(dataType))
                result << e.float32;
            else if (isDataTypeIntOrIVec(dataType))
                result << e.int32;
            else if (isDataTypeUintOrUVec(dataType))
                result << (uint32_t)e.int32;
            else if (isDataTypeBoolOrBVec(dataType))
                result << (e.bool32 ? "true" : "false");
        }
    }
    else if (isDataTypeMatrix(dataType))
    {
        int numRows = getDataTypeMatrixNumRows(dataType);
        int numCols = getDataTypeMatrixNumColumns(dataType);
        for (int rowNdx = 0; rowNdx < numRows; rowNdx++)
        {
            result << "       [ ";
            for (int colNdx = 0; colNdx < numCols; colNdx++)
            {
                int elemNdx = arrayNdx;
                float v     = val.elements[elemNdx * scalarSize + rowNdx * numCols + colNdx].float32;
                result << ((colNdx == 0) ? "" : ", ") << v;
            }
            result << " ]\n";
        }
    }

    if (isDataTypeScalar(dataType))
        result << "\n";
    else if (isDataTypeVector(dataType))
        result << " ]\n";

    log << TestLog::Message << result.str() << TestLog::EndMessage;
}

static void dumpValues(tcu::TestLog &log, const vector<Value> &values, const char *storageName, int arrayNdx)
{
    for (size_t valNdx = 0; valNdx < values.size(); valNdx++)
        dumpValue(log, values[valNdx], storageName, arrayNdx);
}

void dumpValues(tcu::TestLog &log, const ValueBlock &values, int arrayNdx)
{
    dumpValues(log, values.inputs, "input", arrayNdx);
    dumpValues(log, values.outputs, "expected", arrayNdx);
    dumpValues(log, values.uniforms, "uniform", arrayNdx);
}

static void generateExtensionStatements(std::ostringstream &buf, const std::vector<RequiredExtension> &extensions,
                                        glu::ShaderType type)
{
    for (size_t ndx = 0; ndx < extensions.size(); ++ndx)
    {
        DE_ASSERT(extensions[ndx].effectiveStages != 0u && extensions[ndx].alternatives.size() == 1);

        if ((extensions[ndx].effectiveStages & (1u << (uint32_t)type)) != 0)
            buf << "#extension " << extensions[ndx].alternatives[0] << " : require\n";
    }
}

// Injects #extension XXX : require lines after the last preprocessor directive in the shader code. Does not support line continuations
std::string injectExtensionRequirements(const std::string &baseCode, const std::vector<RequiredExtension> &extensions,
                                        glu::ShaderType shaderType)
{
    std::istringstream baseCodeBuf(baseCode);
    std::ostringstream resultBuf;
    std::string line;
    bool firstNonPreprocessorLine = true;
    std::ostringstream extStr;

    generateExtensionStatements(extStr, extensions, shaderType);

    // skip if no requirements
    if (extStr.str().empty())
        return baseCode;

    while (std::getline(baseCodeBuf, line))
    {
        // begins with '#'?
        const std::string::size_type firstNonWhitespace = line.find_first_not_of("\t ");
        const bool isPreprocessorDirective =
            (firstNonWhitespace != std::string::npos && line.at(firstNonWhitespace) == '#');

        // Inject #extensions
        if (!isPreprocessorDirective && firstNonPreprocessorLine)
        {
            firstNonPreprocessorLine = false;
            resultBuf << extStr.str();
        }

        resultBuf << line << "\n";
    }

    return resultBuf.str();
}

void genCompareFunctions(ostringstream &stream, const ValueBlock &valueBlock, bool useFloatTypes)
{
    bool cmpTypeFound[TYPE_LAST];
    for (int i = 0; i < TYPE_LAST; i++)
        cmpTypeFound[i] = false;

    for (size_t valueNdx = 0; valueNdx < valueBlock.outputs.size(); valueNdx++)
    {
        const Value &val                              = valueBlock.outputs[valueNdx];
        cmpTypeFound[(size_t)val.type.getBasicType()] = true;
    }

    if (useFloatTypes)
    {
        if (cmpTypeFound[TYPE_BOOL])
            stream << "bool isOk (float a, bool b) { return ((a > 0.5) == b); }\n";
        if (cmpTypeFound[TYPE_BOOL_VEC2])
            stream << "bool isOk (vec2 a, bvec2 b) { return (greaterThan(a, vec2(0.5)) == b); }\n";
        if (cmpTypeFound[TYPE_BOOL_VEC3])
            stream << "bool isOk (vec3 a, bvec3 b) { return (greaterThan(a, vec3(0.5)) == b); }\n";
        if (cmpTypeFound[TYPE_BOOL_VEC4])
            stream << "bool isOk (vec4 a, bvec4 b) { return (greaterThan(a, vec4(0.5)) == b); }\n";
        if (cmpTypeFound[TYPE_INT])
            stream << "bool isOk (float a, int b)  { float atemp = a+0.5; return (float(b) <= atemp && atemp <= "
                      "float(b+1)); }\n";
        if (cmpTypeFound[TYPE_INT_VEC2])
            stream << "bool isOk (vec2 a, ivec2 b) { return (ivec2(floor(a + 0.5)) == b); }\n";
        if (cmpTypeFound[TYPE_INT_VEC3])
            stream << "bool isOk (vec3 a, ivec3 b) { return (ivec3(floor(a + 0.5)) == b); }\n";
        if (cmpTypeFound[TYPE_INT_VEC4])
            stream << "bool isOk (vec4 a, ivec4 b) { return (ivec4(floor(a + 0.5)) == b); }\n";
        if (cmpTypeFound[TYPE_UINT])
            stream << "bool isOk (float a, uint b) { float atemp = a+0.5; return (float(b) <= atemp && atemp <= "
                      "float(b+1u)); }\n";
        if (cmpTypeFound[TYPE_UINT_VEC2])
            stream << "bool isOk (vec2 a, uvec2 b) { return (uvec2(floor(a + 0.5)) == b); }\n";
        if (cmpTypeFound[TYPE_UINT_VEC3])
            stream << "bool isOk (vec3 a, uvec3 b) { return (uvec3(floor(a + 0.5)) == b); }\n";
        if (cmpTypeFound[TYPE_UINT_VEC4])
            stream << "bool isOk (vec4 a, uvec4 b) { return (uvec4(floor(a + 0.5)) == b); }\n";
    }
    else
    {
        if (cmpTypeFound[TYPE_BOOL])
            stream << "bool isOk (bool a, bool b)   { return (a == b); }\n";
        if (cmpTypeFound[TYPE_BOOL_VEC2])
            stream << "bool isOk (bvec2 a, bvec2 b) { return (a == b); }\n";
        if (cmpTypeFound[TYPE_BOOL_VEC3])
            stream << "bool isOk (bvec3 a, bvec3 b) { return (a == b); }\n";
        if (cmpTypeFound[TYPE_BOOL_VEC4])
            stream << "bool isOk (bvec4 a, bvec4 b) { return (a == b); }\n";
        if (cmpTypeFound[TYPE_INT])
            stream << "bool isOk (int a, int b)     { return (a == b); }\n";
        if (cmpTypeFound[TYPE_INT_VEC2])
            stream << "bool isOk (ivec2 a, ivec2 b) { return (a == b); }\n";
        if (cmpTypeFound[TYPE_INT_VEC3])
            stream << "bool isOk (ivec3 a, ivec3 b) { return (a == b); }\n";
        if (cmpTypeFound[TYPE_INT_VEC4])
            stream << "bool isOk (ivec4 a, ivec4 b) { return (a == b); }\n";
        if (cmpTypeFound[TYPE_UINT])
            stream << "bool isOk (uint a, uint b)   { return (a == b); }\n";
        if (cmpTypeFound[TYPE_UINT_VEC2])
            stream << "bool isOk (uvec2 a, uvec2 b) { return (a == b); }\n";
        if (cmpTypeFound[TYPE_UINT_VEC3])
            stream << "bool isOk (uvec3 a, uvec3 b) { return (a == b); }\n";
        if (cmpTypeFound[TYPE_UINT_VEC4])
            stream << "bool isOk (uvec4 a, uvec4 b) { return (a == b); }\n";
    }

    if (cmpTypeFound[TYPE_FLOAT])
        stream << "bool isOk (float a, float b, float eps) { return (abs(a-b) <= (eps*abs(b) + eps)); }\n";
    if (cmpTypeFound[TYPE_FLOAT_VEC2])
        stream
            << "bool isOk (vec2 a, vec2 b, float eps) { return all(lessThanEqual(abs(a-b), (eps*abs(b) + eps))); }\n";
    if (cmpTypeFound[TYPE_FLOAT_VEC3])
        stream
            << "bool isOk (vec3 a, vec3 b, float eps) { return all(lessThanEqual(abs(a-b), (eps*abs(b) + eps))); }\n";
    if (cmpTypeFound[TYPE_FLOAT_VEC4])
        stream
            << "bool isOk (vec4 a, vec4 b, float eps) { return all(lessThanEqual(abs(a-b), (eps*abs(b) + eps))); }\n";

    if (cmpTypeFound[TYPE_FLOAT_MAT2])
        stream << "bool isOk (mat2 a, mat2 b, float eps) { vec2 diff = max(abs(a[0]-b[0]), abs(a[1]-b[1])); return "
                  "all(lessThanEqual(diff, vec2(eps))); }\n";
    if (cmpTypeFound[TYPE_FLOAT_MAT2X3])
        stream << "bool isOk (mat2x3 a, mat2x3 b, float eps) { vec3 diff = max(abs(a[0]-b[0]), abs(a[1]-b[1])); return "
                  "all(lessThanEqual(diff, vec3(eps))); }\n";
    if (cmpTypeFound[TYPE_FLOAT_MAT2X4])
        stream << "bool isOk (mat2x4 a, mat2x4 b, float eps) { vec4 diff = max(abs(a[0]-b[0]), abs(a[1]-b[1])); return "
                  "all(lessThanEqual(diff, vec4(eps))); }\n";
    if (cmpTypeFound[TYPE_FLOAT_MAT3X2])
        stream << "bool isOk (mat3x2 a, mat3x2 b, float eps) { vec2 diff = max(max(abs(a[0]-b[0]), abs(a[1]-b[1])), "
                  "abs(a[2]-b[2])); return all(lessThanEqual(diff, vec2(eps))); }\n";
    if (cmpTypeFound[TYPE_FLOAT_MAT3])
        stream << "bool isOk (mat3 a, mat3 b, float eps) { vec3 diff = max(max(abs(a[0]-b[0]), abs(a[1]-b[1])), "
                  "abs(a[2]-b[2])); return all(lessThanEqual(diff, vec3(eps))); }\n";
    if (cmpTypeFound[TYPE_FLOAT_MAT3X4])
        stream << "bool isOk (mat3x4 a, mat3x4 b, float eps) { vec4 diff = max(max(abs(a[0]-b[0]), abs(a[1]-b[1])), "
                  "abs(a[2]-b[2])); return all(lessThanEqual(diff, vec4(eps))); }\n";
    if (cmpTypeFound[TYPE_FLOAT_MAT4X2])
        stream << "bool isOk (mat4x2 a, mat4x2 b, float eps) { vec2 diff = max(max(abs(a[0]-b[0]), abs(a[1]-b[1])), "
                  "max(abs(a[2]-b[2]), abs(a[3]-b[3]))); return all(lessThanEqual(diff, vec2(eps))); }\n";
    if (cmpTypeFound[TYPE_FLOAT_MAT4X3])
        stream << "bool isOk (mat4x3 a, mat4x3 b, float eps) { vec3 diff = max(max(abs(a[0]-b[0]), abs(a[1]-b[1])), "
                  "max(abs(a[2]-b[2]), abs(a[3]-b[3]))); return all(lessThanEqual(diff, vec3(eps))); }\n";
    if (cmpTypeFound[TYPE_FLOAT_MAT4])
        stream << "bool isOk (mat4 a, mat4 b, float eps) { vec4 diff = max(max(abs(a[0]-b[0]), abs(a[1]-b[1])), "
                  "max(abs(a[2]-b[2]), abs(a[3]-b[3]))); return all(lessThanEqual(diff, vec4(eps))); }\n";
}

} // namespace sl
} // namespace glu
