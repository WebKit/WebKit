/*-------------------------------------------------------------------------
 * drawElements Quality Program Test Executor
 * ------------------------------------------
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
 * \brief Test case result parser.
 *//*--------------------------------------------------------------------*/

#include "xeTestResultParser.hpp"
#include "xeTestCaseResult.hpp"
#include "xeBatchResult.hpp"
#include "deString.h"
#include "deInt32.h"

#include <sstream>
#include <stdlib.h>

using std::string;
using std::vector;

namespace xe
{

static inline int toInt(const char *str)
{
    return atoi(str);
}

static inline double toDouble(const char *str)
{
    return atof(str);
}

static inline int64_t toInt64(const char *str)
{
    std::istringstream s(str);
    int64_t val;

    s >> val;

    return val;
}

static inline bool toBool(const char *str)
{
    return strcmp(str, "OK") == 0 || strcmp(str, "True") == 0;
}

static const char *stripLeadingWhitespace(const char *str)
{
    int whitespaceCount = 0;

    while (str[whitespaceCount] != 0 && (str[whitespaceCount] == ' ' || str[whitespaceCount] == '\t' ||
                                         str[whitespaceCount] == '\r' || str[whitespaceCount] == '\n'))
        whitespaceCount += 1;

    return str + whitespaceCount;
}

struct EnumMapEntry
{
    uint32_t hash;
    const char *name;
    int value;
};

static const EnumMapEntry s_statusCodeMap[] = {
    {0x7c8a99bc, "Pass", TESTSTATUSCODE_PASS},
    {0x7c851ca1, "Fail", TESTSTATUSCODE_FAIL},
    {0x10ecd324, "QualityWarning", TESTSTATUSCODE_QUALITY_WARNING},
    {0x341ae835, "CompatibilityWarning", TESTSTATUSCODE_COMPATIBILITY_WARNING},
    {0x058acbca, "Pending", TESTSTATUSCODE_PENDING},
    {0xc4d74b26, "Running", TESTSTATUSCODE_RUNNING},
    {0x6409f93c, "NotSupported", TESTSTATUSCODE_NOT_SUPPORTED},
    {0xfa5a9ab7, "ResourceError", TESTSTATUSCODE_RESOURCE_ERROR},
    {0xad6793ec, "InternalError", TESTSTATUSCODE_INTERNAL_ERROR},
    {0x838f3034, "Canceled", TESTSTATUSCODE_CANCELED},
    {0x42b6efac, "Timeout", TESTSTATUSCODE_TIMEOUT},
    {0x0cfb98f6, "Crash", TESTSTATUSCODE_CRASH},
    {0xe326e01d, "Disabled", TESTSTATUSCODE_DISABLED},
    {0x77061af2, "Terminated", TESTSTATUSCODE_TERMINATED},
    {0xd9e6b393, "Waiver", TESTSTATUSCODE_WAIVER}};

static const EnumMapEntry s_resultItemMap[] = {{0xce8ac2e4, "Result", ri::TYPE_RESULT},
                                               {0x7c8cdcea, "Text", ri::TYPE_TEXT},
                                               {0xc6540c6e, "Number", ri::TYPE_NUMBER},
                                               {0x0d656c88, "Image", ri::TYPE_IMAGE},
                                               {0x8ac9ee14, "ImageSet", ri::TYPE_IMAGESET},
                                               {0x1181fa5a, "VertexShader", ri::TYPE_SHADER},
                                               {0xa93daef0, "FragmentShader", ri::TYPE_SHADER},
                                               {0x8f066128, "GeometryShader", ri::TYPE_SHADER},
                                               {0x235a931c, "TessControlShader", ri::TYPE_SHADER},
                                               {0xa1bf7153, "TessEvaluationShader", ri::TYPE_SHADER},
                                               {0x6c1415d9, "ComputeShader", ri::TYPE_SHADER},
                                               {0x68738b22, "RaygenShader", ri::TYPE_SHADER},
                                               {0x51d29ce9, "AnyHitShader", ri::TYPE_SHADER},
                                               {0x8c64a6be, "ClosestHitShader", ri::TYPE_SHADER},
                                               {0xb30ed398, "MissShader", ri::TYPE_SHADER},
                                               {0x26150e53, "IntersectionShader", ri::TYPE_SHADER},
                                               {0x7e50944c, "CallableShader", ri::TYPE_SHADER},
                                               {0x925c7349, "MeshShader", ri::TYPE_SHADER},
                                               {0xc3a35d6f, "TaskShader", ri::TYPE_SHADER},
                                               {0x72863a54, "ShaderProgram", ri::TYPE_SHADERPROGRAM},
                                               {0xb4efc08d, "ShaderSource", ri::TYPE_SHADERSOURCE},
                                               {0xaee4380a, "SpirVAssemblySource", ri::TYPE_SPIRVSOURCE},
                                               {0xff265913, "InfoLog", ri::TYPE_INFOLOG},
                                               {0x84159b73, "EglConfig", ri::TYPE_EGLCONFIG},
                                               {0xdd34391f, "EglConfigSet", ri::TYPE_EGLCONFIGSET},
                                               {0xebbb3aba, "Section", ri::TYPE_SECTION},
                                               {0xa0f15677, "KernelSource", ri::TYPE_KERNELSOURCE},
                                               {0x1ee9083a, "CompileInfo", ri::TYPE_COMPILEINFO},
                                               {0xf1004023, "SampleList", ri::TYPE_SAMPLELIST},
                                               {0xf0feae93, "SampleInfo", ri::TYPE_SAMPLEINFO},
                                               {0x2aa6f14e, "ValueInfo", ri::TYPE_VALUEINFO},
                                               {0xd09429e7, "Sample", ri::TYPE_SAMPLE},
                                               {0x0e4a4722, "Value", ri::TYPE_SAMPLEVALUE}};

static const EnumMapEntry s_imageFormatMap[] = {{0xcc4ffac8, "RGB888", ri::Image::FORMAT_RGB888},
                                                {0x20dcb0c1, "RGBA8888", ri::Image::FORMAT_RGBA8888}};

static const EnumMapEntry s_compressionMap[] = {{0x7c89bbd5, "None", ri::Image::COMPRESSION_NONE},
                                                {0x0b88118a, "PNG", ri::Image::COMPRESSION_PNG}};

static const EnumMapEntry s_shaderTypeFromTagMap[] = {
    {0x1181fa5a, "VertexShader", ri::Shader::SHADERTYPE_VERTEX},
    {0xa93daef0, "FragmentShader", ri::Shader::SHADERTYPE_FRAGMENT},
    {0x8f066128, "GeometryShader", ri::Shader::SHADERTYPE_GEOMETRY},
    {0x235a931c, "TessControlShader", ri::Shader::SHADERTYPE_TESS_CONTROL},
    {0xa1bf7153, "TessEvaluationShader", ri::Shader::SHADERTYPE_TESS_EVALUATION},
    {0x6c1415d9, "ComputeShader", ri::Shader::SHADERTYPE_COMPUTE},
    {0x68738b22, "RaygenShader", ri::Shader::SHADERTYPE_RAYGEN},
    {0x51d29ce9, "AnyHitShader", ri::Shader::SHADERTYPE_ANY_HIT},
    {0x8c64a6be, "ClosestHitShader", ri::Shader::SHADERTYPE_CLOSEST_HIT},
    {0xb30ed398, "MissShader", ri::Shader::SHADERTYPE_MISS},
    {0x26150e53, "IntersectionShader", ri::Shader::SHADERTYPE_INTERSECTION},
    {0x7e50944c, "CallableShader", ri::Shader::SHADERTYPE_CALLABLE},
    {0xc3a35d6f, "TaskShader", ri::Shader::SHADERTYPE_TASK},
    {0x925c7349, "MeshShader", ri::Shader::SHADERTYPE_MESH},
};

static const EnumMapEntry s_testTypeMap[] = {{0x7fa80959, "SelfValidate", TESTCASETYPE_SELF_VALIDATE},
                                             {0xdb797567, "Capability", TESTCASETYPE_CAPABILITY},
                                             {0x2ca3ec10, "Accuracy", TESTCASETYPE_ACCURACY},
                                             {0xa48ac277, "Performance", TESTCASETYPE_PERFORMANCE}};

static const EnumMapEntry s_logVersionMap[] = {
    {0x0b7dac93, "0.2.0", TESTLOGVERSION_0_2_0}, {0x0b7db0d4, "0.3.0", TESTLOGVERSION_0_3_0},
    {0x0b7db0d5, "0.3.1", TESTLOGVERSION_0_3_1}, {0x0b7db0d6, "0.3.2", TESTLOGVERSION_0_3_2},
    {0x0b7db0d7, "0.3.3", TESTLOGVERSION_0_3_3}, {0x0b7db0d8, "0.3.4", TESTLOGVERSION_0_3_4}};

static const EnumMapEntry s_sampleValueTagMap[] = {
    {0xddf2d0d1, "Predictor", ri::ValueInfo::VALUETAG_PREDICTOR},
    {0x9bee2c34, "Response", ri::ValueInfo::VALUETAG_RESPONSE},
};

#if defined(DE_DEBUG)
static void printHashes(const char *name, const EnumMapEntry *entries, int numEntries)
{
    printf("%s:\n", name);

    for (int ndx = 0; ndx < numEntries; ndx++)
        printf("0x%08x\t%s\n", deStringHash(entries[ndx].name), entries[ndx].name);

    printf("\n");
}

#define PRINT_HASHES(MAP) printHashes(#MAP, MAP, DE_LENGTH_OF_ARRAY(MAP))

void TestResultParser_printHashes(void)
{
    PRINT_HASHES(s_statusCodeMap);
    PRINT_HASHES(s_resultItemMap);
    PRINT_HASHES(s_imageFormatMap);
    PRINT_HASHES(s_compressionMap);
    PRINT_HASHES(s_shaderTypeFromTagMap);
    PRINT_HASHES(s_testTypeMap);
    PRINT_HASHES(s_logVersionMap);
    PRINT_HASHES(s_sampleValueTagMap);
}
#endif

static inline int getEnumValue(const char *enumName, const EnumMapEntry *entries, int numEntries, const char *name)
{
    uint32_t hash = deStringHash(name);

    for (int ndx = 0; ndx < numEntries; ndx++)
    {
        if (entries[ndx].hash == hash && strcmp(entries[ndx].name, name) == 0)
            return entries[ndx].value;
    }

    throw TestResultParseError(string("Could not map '") + name + "' to " + enumName);
}

TestStatusCode getTestStatusCode(const char *statusCode)
{
    return (TestStatusCode)getEnumValue("status code", s_statusCodeMap, DE_LENGTH_OF_ARRAY(s_statusCodeMap),
                                        statusCode);
}

static ri::Type getResultItemType(const char *elemName)
{
    return (ri::Type)getEnumValue("result item type", s_resultItemMap, DE_LENGTH_OF_ARRAY(s_resultItemMap), elemName);
}

static ri::Image::Format getImageFormat(const char *imageFormat)
{
    return (ri::Image::Format)getEnumValue("image format", s_imageFormatMap, DE_LENGTH_OF_ARRAY(s_imageFormatMap),
                                           imageFormat);
}

static ri::Image::Compression getImageCompression(const char *compression)
{
    return (ri::Image::Compression)getEnumValue("image compression", s_compressionMap,
                                                DE_LENGTH_OF_ARRAY(s_compressionMap), compression);
}

static ri::Shader::ShaderType getShaderTypeFromTagName(const char *shaderType)
{
    return (ri::Shader::ShaderType)getEnumValue("shader type", s_shaderTypeFromTagMap,
                                                DE_LENGTH_OF_ARRAY(s_shaderTypeFromTagMap), shaderType);
}

static TestCaseType getTestCaseType(const char *caseType)
{
    return (TestCaseType)getEnumValue("test case type", s_testTypeMap, DE_LENGTH_OF_ARRAY(s_testTypeMap), caseType);
}

static TestLogVersion getTestLogVersion(const char *logVersion)
{
    return (TestLogVersion)getEnumValue("test log version", s_logVersionMap, DE_LENGTH_OF_ARRAY(s_logVersionMap),
                                        logVersion);
}

static ri::ValueInfo::ValueTag getSampleValueTag(const char *tag)
{
    return (ri::ValueInfo::ValueTag)getEnumValue("sample value tag", s_sampleValueTagMap,
                                                 DE_LENGTH_OF_ARRAY(s_sampleValueTagMap), tag);
}

static TestCaseType getTestCaseTypeFromPath(const char *casePath)
{
    if (deStringBeginsWith(casePath, "dEQP-GLES2."))
    {
        const char *group = casePath + 11;
        if (deStringBeginsWith(group, "capability."))
            return TESTCASETYPE_CAPABILITY;
        else if (deStringBeginsWith(group, "accuracy."))
            return TESTCASETYPE_ACCURACY;
        else if (deStringBeginsWith(group, "performance."))
            return TESTCASETYPE_PERFORMANCE;
    }

    return TESTCASETYPE_SELF_VALIDATE;
}

static ri::NumericValue getNumericValue(const std::string &value)
{
    const bool isFloat = value.find('.') != std::string::npos || value.find('e') != std::string::npos;

    if (isFloat)
    {
        const double num = toDouble(stripLeadingWhitespace(value.c_str()));
        return ri::NumericValue(num);
    }
    else
    {
        const int64_t num = toInt64(stripLeadingWhitespace(value.c_str()));
        return ri::NumericValue(num);
    }
}

TestResultParser::TestResultParser(void)
    : m_result(nullptr)
    , m_state(STATE_NOT_INITIALIZED)
    , m_logVersion(TESTLOGVERSION_LAST)
    , m_curItemList(nullptr)
    , m_base64DecodeOffset(0)
{
}

TestResultParser::~TestResultParser(void)
{
}

void TestResultParser::clear(void)
{
    m_xmlParser.clear();
    m_itemStack.clear();

    m_result             = nullptr;
    m_state              = STATE_NOT_INITIALIZED;
    m_logVersion         = TESTLOGVERSION_LAST;
    m_curItemList        = nullptr;
    m_base64DecodeOffset = 0;
    m_curNumValue.clear();
}

void TestResultParser::init(TestCaseResult *dstResult)
{
    clear();
    m_result      = dstResult;
    m_state       = STATE_INITIALIZED;
    m_curItemList = &dstResult->resultItems;
}

TestResultParser::ParseResult TestResultParser::parse(const uint8_t *bytes, int numBytes)
{
    DE_ASSERT(m_result && m_state != STATE_NOT_INITIALIZED);

    try
    {
        bool resultChanged = false;

        m_xmlParser.feed(bytes, numBytes);

        for (;;)
        {
            xml::Element curElement = m_xmlParser.getElement();

            if (curElement == xml::ELEMENT_INCOMPLETE || curElement == xml::ELEMENT_END_OF_STRING)
                break;

            switch (curElement)
            {
            case xml::ELEMENT_START:
                handleElementStart();
                break;
            case xml::ELEMENT_END:
                handleElementEnd();
                break;
            case xml::ELEMENT_DATA:
                handleData();
                break;

            default:
                DE_ASSERT(false);
            }

            resultChanged = true;
            m_xmlParser.advance();
        }

        if (m_xmlParser.getElement() == xml::ELEMENT_END_OF_STRING)
        {
            if (m_state != STATE_TEST_CASE_RESULT_ENDED)
                throw TestResultParseError("Unexpected end of log data");

            return PARSERESULT_COMPLETE;
        }
        else
            return resultChanged ? PARSERESULT_CHANGED : PARSERESULT_NOT_CHANGED;
    }
    catch (const TestResultParseError &e)
    {
        // Set error code to result.
        m_result->statusCode    = TESTSTATUSCODE_INTERNAL_ERROR;
        m_result->statusDetails = e.what();

        return PARSERESULT_ERROR;
    }
    catch (const xml::ParseError &e)
    {
        // Set error code to result.
        m_result->statusCode    = TESTSTATUSCODE_INTERNAL_ERROR;
        m_result->statusDetails = e.what();

        return PARSERESULT_ERROR;
    }
}

const char *TestResultParser::getAttribute(const char *name)
{
    if (!m_xmlParser.hasAttribute(name))
        throw TestResultParseError(string("Missing attribute '") + name + "' in <" + m_xmlParser.getElementName() +
                                   ">");

    return m_xmlParser.getAttribute(name);
}

ri::Item *TestResultParser::getCurrentItem(void)
{
    return !m_itemStack.empty() ? m_itemStack.back() : nullptr;
}

ri::List *TestResultParser::getCurrentItemList(void)
{
    DE_ASSERT(m_curItemList);
    return m_curItemList;
}

void TestResultParser::updateCurrentItemList(void)
{
    m_curItemList = nullptr;

    for (vector<ri::Item *>::reverse_iterator i = m_itemStack.rbegin(); i != m_itemStack.rend(); i++)
    {
        ri::Item *item = *i;
        ri::Type type  = item->getType();

        if (type == ri::TYPE_IMAGESET)
            m_curItemList = &static_cast<ri::ImageSet *>(item)->images;
        else if (type == ri::TYPE_SECTION)
            m_curItemList = &static_cast<ri::Section *>(item)->items;
        else if (type == ri::TYPE_EGLCONFIGSET)
            m_curItemList = &static_cast<ri::EglConfigSet *>(item)->configs;
        else if (type == ri::TYPE_SHADERPROGRAM)
            m_curItemList = &static_cast<ri::ShaderProgram *>(item)->shaders;

        if (m_curItemList)
            break;
    }

    if (!m_curItemList)
        m_curItemList = &m_result->resultItems;
}

void TestResultParser::pushItem(ri::Item *item)
{
    m_itemStack.push_back(item);
    updateCurrentItemList();
}

void TestResultParser::popItem(void)
{
    m_itemStack.pop_back();
    updateCurrentItemList();
}

void TestResultParser::handleElementStart(void)
{
    const char *elemName = m_xmlParser.getElementName();

    if (m_state == STATE_INITIALIZED)
    {
        // Expect TestCaseResult.
        if (strcmp(elemName, "TestCaseResult") != 0)
            throw TestResultParseError(string("Expected <TestCaseResult>, got <") + elemName + ">");

        const char *version = getAttribute("Version");
        m_logVersion        = getTestLogVersion(version);
        // \note Currently assumed that all known log versions are supported.

        m_result->caseVersion = version;
        m_result->casePath    = getAttribute("CasePath");
        m_result->caseType    = TESTCASETYPE_SELF_VALIDATE;

        if (m_xmlParser.hasAttribute("CaseType"))
            m_result->caseType = getTestCaseType(m_xmlParser.getAttribute("CaseType"));
        else
        {
            // Do guess based on path for legacy log files.
            if (m_logVersion >= TESTLOGVERSION_0_3_2)
                throw TestResultParseError("Missing CaseType attribute in <TestCaseResult>");
            m_result->caseType = getTestCaseTypeFromPath(m_result->casePath.c_str());
        }

        m_state = STATE_IN_TEST_CASE_RESULT;
    }
    else
    {
        ri::List *curList    = getCurrentItemList();
        ri::Type itemType    = getResultItemType(elemName);
        ri::Item *item       = nullptr;
        ri::Item *parentItem = getCurrentItem();
        ri::Type parentType  = parentItem ? parentItem->getType() : ri::TYPE_LAST;

        switch (itemType)
        {
        case ri::TYPE_RESULT:
        {
            ri::Result *result = curList->allocItem<ri::Result>();
            result->statusCode = getTestStatusCode(getAttribute("StatusCode"));
            item               = result;
            break;
        }

        case ri::TYPE_TEXT:
            item = curList->allocItem<ri::Text>();
            break;

        case ri::TYPE_SECTION:
        {
            ri::Section *section = curList->allocItem<ri::Section>();
            section->name        = getAttribute("Name");
            section->description = getAttribute("Description");
            item                 = section;
            break;
        }

        case ri::TYPE_NUMBER:
        {
            ri::Number *number  = curList->allocItem<ri::Number>();
            number->name        = getAttribute("Name");
            number->description = getAttribute("Description");
            number->unit        = getAttribute("Unit");

            if (m_xmlParser.hasAttribute("Tag"))
                number->tag = m_xmlParser.getAttribute("Tag");

            item = number;

            m_curNumValue.clear();
            break;
        }

        case ri::TYPE_IMAGESET:
        {
            ri::ImageSet *imageSet = curList->allocItem<ri::ImageSet>();
            imageSet->name         = getAttribute("Name");
            imageSet->description  = getAttribute("Description");
            item                   = imageSet;
            break;
        }

        case ri::TYPE_IMAGE:
        {
            ri::Image *image   = curList->allocItem<ri::Image>();
            image->name        = getAttribute("Name");
            image->description = getAttribute("Description");
            image->width       = toInt(getAttribute("Width"));
            image->height      = toInt(getAttribute("Height"));
            image->format      = getImageFormat(getAttribute("Format"));
            image->compression = getImageCompression(getAttribute("CompressionMode"));
            item               = image;
            break;
        }

        case ri::TYPE_SHADERPROGRAM:
        {
            ri::ShaderProgram *shaderProgram = curList->allocItem<ri::ShaderProgram>();
            shaderProgram->linkStatus        = toBool(getAttribute("LinkStatus"));
            item                             = shaderProgram;
            break;
        }

        case ri::TYPE_SHADER:
        {
            if (parentType != ri::TYPE_SHADERPROGRAM)
                throw TestResultParseError(string("<") + elemName + "> outside of <ShaderProgram>");

            ri::Shader *shader = curList->allocItem<ri::Shader>();

            shader->shaderType    = getShaderTypeFromTagName(elemName);
            shader->compileStatus = toBool(getAttribute("CompileStatus"));

            item = shader;
            break;
        }

        case ri::TYPE_SPIRVSOURCE:
        {
            if (parentType != ri::TYPE_SHADERPROGRAM)
                throw TestResultParseError(string("<") + elemName + "> outside of <ShaderProgram>");
            item = curList->allocItem<ri::SpirVSource>();
            break;
        }

        case ri::TYPE_SHADERSOURCE:
            if (parentType == ri::TYPE_SHADER)
                item = &static_cast<ri::Shader *>(parentItem)->source;
            else
                throw TestResultParseError("Unexpected <ShaderSource>");
            break;

        case ri::TYPE_INFOLOG:
            if (parentType == ri::TYPE_SHADERPROGRAM)
                item = &static_cast<ri::ShaderProgram *>(parentItem)->linkInfoLog;
            else if (parentType == ri::TYPE_SHADER)
                item = &static_cast<ri::Shader *>(parentItem)->infoLog;
            else if (parentType == ri::TYPE_COMPILEINFO)
                item = &static_cast<ri::CompileInfo *>(parentItem)->infoLog;
            else
                throw TestResultParseError("Unexpected <InfoLog>");
            break;

        case ri::TYPE_KERNELSOURCE:
            item = curList->allocItem<ri::KernelSource>();
            break;

        case ri::TYPE_COMPILEINFO:
        {
            ri::CompileInfo *info = curList->allocItem<ri::CompileInfo>();
            info->name            = getAttribute("Name");
            info->description     = getAttribute("Description");
            info->compileStatus   = toBool(getAttribute("CompileStatus"));
            item                  = info;
            break;
        }

        case ri::TYPE_EGLCONFIGSET:
        {
            ri::EglConfigSet *set = curList->allocItem<ri::EglConfigSet>();
            set->name             = getAttribute("Name");
            set->description = m_xmlParser.hasAttribute("Description") ? m_xmlParser.getAttribute("Description") : "";
            item             = set;
            break;
        }

        case ri::TYPE_EGLCONFIG:
        {
            ri::EglConfig *config         = curList->allocItem<ri::EglConfig>();
            config->bufferSize            = toInt(getAttribute("BufferSize"));
            config->redSize               = toInt(getAttribute("RedSize"));
            config->greenSize             = toInt(getAttribute("GreenSize"));
            config->blueSize              = toInt(getAttribute("BlueSize"));
            config->luminanceSize         = toInt(getAttribute("LuminanceSize"));
            config->alphaSize             = toInt(getAttribute("AlphaSize"));
            config->alphaMaskSize         = toInt(getAttribute("AlphaMaskSize"));
            config->bindToTextureRGB      = toBool(getAttribute("BindToTextureRGB"));
            config->bindToTextureRGBA     = toBool(getAttribute("BindToTextureRGBA"));
            config->colorBufferType       = getAttribute("ColorBufferType");
            config->configCaveat          = getAttribute("ConfigCaveat");
            config->configID              = toInt(getAttribute("ConfigID"));
            config->conformant            = getAttribute("Conformant");
            config->depthSize             = toInt(getAttribute("DepthSize"));
            config->level                 = toInt(getAttribute("Level"));
            config->maxPBufferWidth       = toInt(getAttribute("MaxPBufferWidth"));
            config->maxPBufferHeight      = toInt(getAttribute("MaxPBufferHeight"));
            config->maxPBufferPixels      = toInt(getAttribute("MaxPBufferPixels"));
            config->maxSwapInterval       = toInt(getAttribute("MaxSwapInterval"));
            config->minSwapInterval       = toInt(getAttribute("MinSwapInterval"));
            config->nativeRenderable      = toBool(getAttribute("NativeRenderable"));
            config->renderableType        = getAttribute("RenderableType");
            config->sampleBuffers         = toInt(getAttribute("SampleBuffers"));
            config->samples               = toInt(getAttribute("Samples"));
            config->stencilSize           = toInt(getAttribute("StencilSize"));
            config->surfaceTypes          = getAttribute("SurfaceTypes");
            config->transparentType       = getAttribute("TransparentType");
            config->transparentRedValue   = toInt(getAttribute("TransparentRedValue"));
            config->transparentGreenValue = toInt(getAttribute("TransparentGreenValue"));
            config->transparentBlueValue  = toInt(getAttribute("TransparentBlueValue"));
            item                          = config;
            break;
        }

        case ri::TYPE_SAMPLELIST:
        {
            ri::SampleList *list = curList->allocItem<ri::SampleList>();
            list->name           = getAttribute("Name");
            list->description    = getAttribute("Description");
            item                 = list;
            break;
        }

        case ri::TYPE_SAMPLEINFO:
        {
            if (parentType != ri::TYPE_SAMPLELIST)
                throw TestResultParseError("<SampleInfo> outside of <SampleList>");

            ri::SampleList *list = static_cast<ri::SampleList *>(parentItem);
            ri::SampleInfo *info = &list->sampleInfo;

            item = info;
            break;
        }

        case ri::TYPE_VALUEINFO:
        {
            if (parentType != ri::TYPE_SAMPLEINFO)
                throw TestResultParseError("<ValueInfo> outside of <SampleInfo>");

            ri::SampleInfo *sampleInfo = static_cast<ri::SampleInfo *>(parentItem);
            ri::ValueInfo *valueInfo   = sampleInfo->valueInfos.allocItem<ri::ValueInfo>();

            valueInfo->name        = getAttribute("Name");
            valueInfo->description = getAttribute("Description");
            valueInfo->tag         = getSampleValueTag(getAttribute("Tag"));

            if (m_xmlParser.hasAttribute("Unit"))
                valueInfo->unit = getAttribute("Unit");

            item = valueInfo;
            break;
        }

        case ri::TYPE_SAMPLE:
        {
            if (parentType != ri::TYPE_SAMPLELIST)
                throw TestResultParseError("<Sample> outside of <SampleList>");

            ri::SampleList *list = static_cast<ri::SampleList *>(parentItem);
            ri::Sample *sample   = list->samples.allocItem<ri::Sample>();

            item = sample;
            break;
        }

        case ri::TYPE_SAMPLEVALUE:
        {
            if (parentType != ri::TYPE_SAMPLE)
                throw TestResultParseError("<Value> outside of <Sample>");

            ri::Sample *sample     = static_cast<ri::Sample *>(parentItem);
            ri::SampleValue *value = sample->values.allocItem<ri::SampleValue>();

            item = value;
            break;
        }

        default:
            throw TestResultParseError(string("Unsupported element '") + elemName + ("'"));
        }

        DE_ASSERT(item);
        pushItem(item);

        // Reset base64 decoding offset.
        m_base64DecodeOffset = 0;
    }
}

void TestResultParser::handleElementEnd(void)
{
    const char *elemName = m_xmlParser.getElementName();

    if (m_state != STATE_IN_TEST_CASE_RESULT)
        throw TestResultParseError(string("Unexpected </") + elemName + "> outside of <TestCaseResult>");

    if (strcmp(elemName, "TestCaseResult") == 0)
    {
        // Logs from buggy test cases may contain invalid XML.
        // DE_ASSERT(getCurrentItem() == nullptr);
        // \todo [2012-11-22 pyry] Log warning.

        m_state = STATE_TEST_CASE_RESULT_ENDED;
    }
    else
    {
        ri::Type itemType = getResultItemType(elemName);
        ri::Item *curItem = getCurrentItem();

        if (!curItem || itemType != curItem->getType())
            throw TestResultParseError(string("Unexpected </") + elemName + ">");

        if (itemType == ri::TYPE_RESULT)
        {
            ri::Result *result      = static_cast<ri::Result *>(curItem);
            m_result->statusCode    = result->statusCode;
            m_result->statusDetails = result->details;
        }
        else if (itemType == ri::TYPE_NUMBER)
        {
            // Parse value for number.
            ri::Number *number = static_cast<ri::Number *>(curItem);
            number->value      = getNumericValue(m_curNumValue);
            m_curNumValue.clear();
        }
        else if (itemType == ri::TYPE_SAMPLEVALUE)
        {
            ri::SampleValue *value = static_cast<ri::SampleValue *>(curItem);
            value->value           = getNumericValue(m_curNumValue);
            m_curNumValue.clear();
        }

        popItem();
    }
}

void TestResultParser::handleData(void)
{
    ri::Item *curItem = getCurrentItem();
    ri::Type type     = curItem ? curItem->getType() : ri::TYPE_LAST;

    switch (type)
    {
    case ri::TYPE_RESULT:
        m_xmlParser.appendDataStr(static_cast<ri::Result *>(curItem)->details);
        break;

    case ri::TYPE_TEXT:
        m_xmlParser.appendDataStr(static_cast<ri::Text *>(curItem)->text);
        break;

    case ri::TYPE_SHADERSOURCE:
        m_xmlParser.appendDataStr(static_cast<ri::ShaderSource *>(curItem)->source);
        break;

    case ri::TYPE_SPIRVSOURCE:
        m_xmlParser.appendDataStr(static_cast<ri::SpirVSource *>(curItem)->source);
        break;

    case ri::TYPE_INFOLOG:
        m_xmlParser.appendDataStr(static_cast<ri::InfoLog *>(curItem)->log);
        break;

    case ri::TYPE_KERNELSOURCE:
        m_xmlParser.appendDataStr(static_cast<ri::KernelSource *>(curItem)->source);
        break;

    case ri::TYPE_NUMBER:
    case ri::TYPE_SAMPLEVALUE:
        m_xmlParser.appendDataStr(m_curNumValue);
        break;

    case ri::TYPE_IMAGE:
    {
        ri::Image *image = static_cast<ri::Image *>(curItem);

        // Base64 decode.
        int numBytesIn = m_xmlParser.getDataSize();

        for (int inNdx = 0; inNdx < numBytesIn; inNdx++)
        {
            uint8_t byte        = m_xmlParser.getDataByte(inNdx);
            uint8_t decodedBits = 0;

            if (de::inRange<int8_t>(byte, 'A', 'Z'))
                decodedBits = (uint8_t)(byte - 'A');
            else if (de::inRange<int8_t>(byte, 'a', 'z'))
                decodedBits = (uint8_t)(('Z' - 'A' + 1) + (byte - 'a'));
            else if (de::inRange<int8_t>(byte, '0', '9'))
                decodedBits = (uint8_t)(('Z' - 'A' + 1) + ('z' - 'a' + 1) + (byte - '0'));
            else if (byte == '+')
                decodedBits = ('Z' - 'A' + 1) + ('z' - 'a' + 1) + ('9' - '0' + 1);
            else if (byte == '/')
                decodedBits = ('Z' - 'A' + 1) + ('z' - 'a' + 1) + ('9' - '0' + 2);
            else if (byte == '=')
            {
                // Padding at end - remove last byte.
                if (image->data.empty())
                    throw TestResultParseError("Malformed base64 data");
                image->data.pop_back();
                continue;
            }
            else
                continue; // Not an B64 input character.

            int phase = m_base64DecodeOffset % 4;

            if (phase == 0)
                image->data.resize(image->data.size() + 3, 0);

            if ((int)image->data.size() < (m_base64DecodeOffset >> 2) * 3 + 3)
                throw TestResultParseError("Malformed base64 data");
            uint8_t *outPtr = &image->data[(m_base64DecodeOffset >> 2) * 3];

            switch (phase)
            {
            case 0:
                outPtr[0] |= (uint8_t)(decodedBits << 2);
                break;
            case 1:
                outPtr[0] = (uint8_t)(outPtr[0] | (uint8_t)(decodedBits >> 4));
                outPtr[1] = (uint8_t)(outPtr[1] | (uint8_t)((decodedBits & 0xF) << 4));
                break;
            case 2:
                outPtr[1] = (uint8_t)(outPtr[1] | (uint8_t)(decodedBits >> 2));
                outPtr[2] = (uint8_t)(outPtr[2] | (uint8_t)((decodedBits & 0x3) << 6));
                break;
            case 3:
                outPtr[2] |= decodedBits;
                break;
            default:
                DE_ASSERT(false);
            }

            m_base64DecodeOffset += 1;
        }

        break;
    }

    default:
        // Just ignore data.
        break;
    }
}

//! Helper for parsing TestCaseResult from TestCaseResultData.
void parseTestCaseResultFromData(TestResultParser *parser, TestCaseResult *result, const TestCaseResultData &data)
{
    DE_ASSERT(result->resultItems.getNumItems() == 0);

    // Initialize status codes etc. from data.
    result->casePath      = data.getTestCasePath();
    result->caseType      = TESTCASETYPE_SELF_VALIDATE;
    result->statusCode    = data.getStatusCode();
    result->statusDetails = data.getStatusDetails();

    if (data.getDataSize() > 0)
    {
        parser->init(result);

        const TestResultParser::ParseResult parseResult = parser->parse(data.getData(), data.getDataSize());

        if (result->statusCode == TESTSTATUSCODE_LAST)
        {
            result->statusCode = TESTSTATUSCODE_INTERNAL_ERROR;

            if (parseResult == TestResultParser::PARSERESULT_ERROR)
                result->statusDetails = "Test case result parsing failed";
            else if (parseResult != TestResultParser::PARSERESULT_COMPLETE)
                result->statusDetails = "Incomplete test case result";
            else
                result->statusDetails = "Test case result is missing <Result> item";
        }
    }
    else if (result->statusCode == TESTSTATUSCODE_LAST)
    {
        result->statusCode    = TESTSTATUSCODE_TERMINATED;
        result->statusDetails = "Empty test case result";
    }

    if (result->casePath.empty())
        throw Error("Empty test case path in result");

    if (result->caseType == TESTCASETYPE_LAST)
        throw Error("Invalid test case type in result");

    DE_ASSERT(result->statusCode != TESTSTATUSCODE_LAST);
}

} // namespace xe
