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
 * \brief Test log writer.
 *//*--------------------------------------------------------------------*/

#include "xeTestLogWriter.hpp"
#include "xeXMLWriter.hpp"
#include "deStringUtil.hpp"

#include <fstream>

namespace xe
{

/* Batch result writer. */

struct ContainerValue
{
    ContainerValue(const std::string &value_) : value(value_)
    {
    }
    ContainerValue(const char *value_) : value(value_)
    {
    }
    std::string value;
};

std::ostream &operator<<(std::ostream &stream, const ContainerValue &value)
{
    if (value.value.find(' ') != std::string::npos)
    {
        // Escape.
        stream << '"';
        for (std::string::const_iterator i = value.value.begin(); i != value.value.end(); i++)
        {
            if (*i == '"' || *i == '\\')
                stream << '\\';
            stream << *i;
        }
        stream << '"';
    }
    else
        stream << value.value;

    return stream;
}

static void writeSessionInfo(const SessionInfo &info, std::ostream &stream)
{
    if (!info.releaseName.empty())
        stream << "#sessionInfo releaseName " << ContainerValue(info.releaseName) << "\n";

    if (!info.releaseId.empty())
        stream << "#sessionInfo releaseId " << ContainerValue(info.releaseId) << "\n";

    if (!info.targetName.empty())
        stream << "#sessionInfo targetName " << ContainerValue(info.targetName) << "\n";

    if (!info.candyTargetName.empty())
        stream << "#sessionInfo candyTargetName " << ContainerValue(info.candyTargetName) << "\n";

    if (!info.configName.empty())
        stream << "#sessionInfo configName " << ContainerValue(info.configName) << "\n";

    if (!info.resultName.empty())
        stream << "#sessionInfo resultName " << ContainerValue(info.resultName) << "\n";

    // \note Current format uses unescaped timestamps for some strange reason.
    if (!info.timestamp.empty())
        stream << "#sessionInfo timestamp " << info.timestamp << "\n";
}

static void writeTestCase(const TestCaseResultData &caseData, std::ostream &stream)
{
    stream << "\n#beginTestCaseResult " << caseData.getTestCasePath() << "\n";

    if (caseData.getDataSize() > 0)
    {
        stream.write((const char *)caseData.getData(), caseData.getDataSize());

        uint8_t lastCh = caseData.getData()[caseData.getDataSize() - 1];
        if (lastCh != '\n' && lastCh != '\r')
            stream << "\n";
    }

    TestStatusCode dataCode = caseData.getStatusCode();
    if (dataCode == TESTSTATUSCODE_CRASH || dataCode == TESTSTATUSCODE_TIMEOUT || dataCode == TESTSTATUSCODE_TERMINATED)
        stream << "#terminateTestCaseResult " << getTestStatusCodeName(dataCode) << "\n";
    else
        stream << "#endTestCaseResult\n";
}

void writeTestLog(const BatchResult &result, std::ostream &stream)
{
    writeSessionInfo(result.getSessionInfo(), stream);

    stream << "#beginSession\n";

    for (int ndx = 0; ndx < result.getNumTestCaseResults(); ndx++)
    {
        ConstTestCaseResultPtr caseData = result.getTestCaseResult(ndx);
        writeTestCase(*caseData, stream);
    }

    stream << "\n#endSession\n";
}

void writeBatchResultToFile(const BatchResult &result, const char *filename)
{
    std::ofstream str(filename, std::ofstream::binary | std::ofstream::trunc);
    writeTestLog(result, str);
    str.close();
}

/* Test result log writer. */

static const char *getImageFormatName(ri::Image::Format format)
{
    switch (format)
    {
    case ri::Image::FORMAT_RGB888:
        return "RGB888";
    case ri::Image::FORMAT_RGBA8888:
        return "RGBA8888";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

static const char *getImageCompressionName(ri::Image::Compression compression)
{
    switch (compression)
    {
    case ri::Image::COMPRESSION_NONE:
        return "None";
    case ri::Image::COMPRESSION_PNG:
        return "PNG";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

static const char *getSampleValueTagName(ri::ValueInfo::ValueTag tag)
{
    switch (tag)
    {
    case ri::ValueInfo::VALUETAG_PREDICTOR:
        return "Predictor";
    case ri::ValueInfo::VALUETAG_RESPONSE:
        return "Response";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

inline const char *getBoolName(bool val)
{
    return val ? "True" : "False";
}

// \todo [2012-09-07 pyry] Move to tcutil?
class Base64Formatter
{
public:
    const uint8_t *data;
    int numBytes;

    Base64Formatter(const uint8_t *data_, int numBytes_) : data(data_), numBytes(numBytes_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const Base64Formatter &fmt)
{
    static const char s_base64Table[64] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
        'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
        's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

    const uint8_t *data = fmt.data;
    int numBytes        = fmt.numBytes;
    int srcNdx          = 0;

    DE_ASSERT(data && (numBytes > 0));

    /* Loop all input chars. */
    while (srcNdx < numBytes)
    {
        int numRead = de::min(3, numBytes - srcNdx);
        uint8_t s0  = data[srcNdx];
        uint8_t s1  = (numRead >= 2) ? data[srcNdx + 1] : 0;
        uint8_t s2  = (numRead >= 3) ? data[srcNdx + 2] : 0;
        char d[4];

        srcNdx += numRead;

        d[0] = s_base64Table[s0 >> 2];
        d[1] = s_base64Table[((s0 & 0x3) << 4) | (s1 >> 4)];
        d[2] = s_base64Table[((s1 & 0xF) << 2) | (s2 >> 6)];
        d[3] = s_base64Table[s2 & 0x3F];

        if (numRead < 3)
            d[3] = '=';
        if (numRead < 2)
            d[2] = '=';

        /* Write data. */
        str.write(&d[0], sizeof(d));
    }

    return str;
}

inline Base64Formatter toBase64(const uint8_t *bytes, int numBytes)
{
    return Base64Formatter(bytes, numBytes);
}

static const char *getStatusName(bool value)
{
    return value ? "OK" : "Fail";
}

static void writeResultItem(const ri::Item &item, xml::Writer &dst)
{
    using xml::Writer;

    switch (item.getType())
    {
    case ri::TYPE_RESULT:
        // Ignored here, written at end.
        break;

    case ri::TYPE_TEXT:
        dst << Writer::BeginElement("Text") << static_cast<const ri::Text &>(item).text << Writer::EndElement;
        break;

    case ri::TYPE_NUMBER:
    {
        const ri::Number &number = static_cast<const ri::Number &>(item);
        dst << Writer::BeginElement("Number") << Writer::Attribute("Name", number.name)
            << Writer::Attribute("Description", number.description) << Writer::Attribute("Unit", number.unit)
            << Writer::Attribute("Tag", number.tag) << number.value << Writer::EndElement;
        break;
    }

    case ri::TYPE_IMAGE:
    {
        const ri::Image &image = static_cast<const ri::Image &>(item);
        dst << Writer::BeginElement("Image") << Writer::Attribute("Name", image.name)
            << Writer::Attribute("Description", image.description)
            << Writer::Attribute("Width", de::toString(image.width))
            << Writer::Attribute("Height", de::toString(image.height))
            << Writer::Attribute("Format", getImageFormatName(image.format))
            << Writer::Attribute("CompressionMode", getImageCompressionName(image.compression))
            << toBase64(&image.data[0], (int)image.data.size()) << Writer::EndElement;
        break;
    }

    case ri::TYPE_IMAGESET:
    {
        const ri::ImageSet &imageSet = static_cast<const ri::ImageSet &>(item);
        dst << Writer::BeginElement("ImageSet") << Writer::Attribute("Name", imageSet.name)
            << Writer::Attribute("Description", imageSet.description);

        for (int ndx = 0; ndx < imageSet.images.getNumItems(); ndx++)
            writeResultItem(imageSet.images.getItem(ndx), dst);

        dst << Writer::EndElement;
        break;
    }

    case ri::TYPE_SHADER:
    {
        const ri::Shader &shader = static_cast<const ri::Shader &>(item);
        const char *tagName      = nullptr;

        switch (shader.shaderType)
        {
        case ri::Shader::SHADERTYPE_VERTEX:
            tagName = "VertexShader";
            break;
        case ri::Shader::SHADERTYPE_FRAGMENT:
            tagName = "FragmentShader";
            break;
        case ri::Shader::SHADERTYPE_GEOMETRY:
            tagName = "GeometryShader";
            break;
        case ri::Shader::SHADERTYPE_TESS_CONTROL:
            tagName = "TessControlShader";
            break;
        case ri::Shader::SHADERTYPE_TESS_EVALUATION:
            tagName = "TessEvaluationShader";
            break;
        case ri::Shader::SHADERTYPE_COMPUTE:
            tagName = "ComputeShader";
            break;
        case ri::Shader::SHADERTYPE_RAYGEN:
            tagName = "RaygenShader";
            break;
        case ri::Shader::SHADERTYPE_ANY_HIT:
            tagName = "AnyHitShader";
            break;
        case ri::Shader::SHADERTYPE_CLOSEST_HIT:
            tagName = "ClosestHitShader";
            break;
        case ri::Shader::SHADERTYPE_MISS:
            tagName = "MissShader";
            break;
        case ri::Shader::SHADERTYPE_INTERSECTION:
            tagName = "IntersectionShader";
            break;
        case ri::Shader::SHADERTYPE_CALLABLE:
            tagName = "CallableShader";
            break;
        case ri::Shader::SHADERTYPE_TASK:
            tagName = "TaskShader";
            break;
        case ri::Shader::SHADERTYPE_MESH:
            tagName = "MeshShader";
            break;

        default:
            throw Error("Unknown shader type");
        }

        dst << Writer::BeginElement(tagName) << Writer::Attribute("CompileStatus", getStatusName(shader.compileStatus));

        writeResultItem(shader.source, dst);
        writeResultItem(shader.infoLog, dst);

        dst << Writer::EndElement;
        break;
    }

    case ri::TYPE_SHADERPROGRAM:
    {
        const ri::ShaderProgram &program = static_cast<const ri::ShaderProgram &>(item);
        dst << Writer::BeginElement("ShaderProgram")
            << Writer::Attribute("LinkStatus", getStatusName(program.linkStatus));

        writeResultItem(program.linkInfoLog, dst);

        for (int ndx = 0; ndx < program.shaders.getNumItems(); ndx++)
            writeResultItem(program.shaders.getItem(ndx), dst);

        dst << Writer::EndElement;
        break;
    }

    case ri::TYPE_SHADERSOURCE:
        dst << Writer::BeginElement("ShaderSource") << static_cast<const ri::ShaderSource &>(item).source
            << Writer::EndElement;
        break;

    case ri::TYPE_SPIRVSOURCE:
        dst << Writer::BeginElement("SpirVAssemblySource") << static_cast<const ri::SpirVSource &>(item).source
            << Writer::EndElement;
        break;

    case ri::TYPE_INFOLOG:
        dst << Writer::BeginElement("InfoLog") << static_cast<const ri::InfoLog &>(item).log << Writer::EndElement;
        break;

    case ri::TYPE_SECTION:
    {
        const ri::Section &section = static_cast<const ri::Section &>(item);
        dst << Writer::BeginElement("Section") << Writer::Attribute("Name", section.name)
            << Writer::Attribute("Description", section.description);

        for (int ndx = 0; ndx < section.items.getNumItems(); ndx++)
            writeResultItem(section.items.getItem(ndx), dst);

        dst << Writer::EndElement;
        break;
    }

    case ri::TYPE_KERNELSOURCE:
        dst << Writer::BeginElement("KernelSource") << static_cast<const ri::KernelSource &>(item).source
            << Writer::EndElement;
        break;

    case ri::TYPE_COMPILEINFO:
    {
        const ri::CompileInfo &compileInfo = static_cast<const ri::CompileInfo &>(item);
        dst << Writer::BeginElement("CompileInfo") << Writer::Attribute("Name", compileInfo.name)
            << Writer::Attribute("Description", compileInfo.description)
            << Writer::Attribute("CompileStatus", getStatusName(compileInfo.compileStatus));

        writeResultItem(compileInfo.infoLog, dst);

        dst << Writer::EndElement;
        break;
    }

    case ri::TYPE_EGLCONFIG:
    {
        const ri::EglConfig &config = static_cast<const ri::EglConfig &>(item);
        dst << Writer::BeginElement("EglConfig") << Writer::Attribute("BufferSize", de::toString(config.bufferSize))
            << Writer::Attribute("RedSize", de::toString(config.redSize))
            << Writer::Attribute("GreenSize", de::toString(config.greenSize))
            << Writer::Attribute("BlueSize", de::toString(config.blueSize))
            << Writer::Attribute("LuminanceSize", de::toString(config.luminanceSize))
            << Writer::Attribute("AlphaSize", de::toString(config.alphaSize))
            << Writer::Attribute("AlphaMaskSize", de::toString(config.alphaMaskSize))
            << Writer::Attribute("BindToTextureRGB", getBoolName(config.bindToTextureRGB))
            << Writer::Attribute("BindToTextureRGBA", getBoolName(config.bindToTextureRGBA))
            << Writer::Attribute("ColorBufferType", config.colorBufferType)
            << Writer::Attribute("ConfigCaveat", config.configCaveat)
            << Writer::Attribute("ConfigID", de::toString(config.configID))
            << Writer::Attribute("Conformant", config.conformant)
            << Writer::Attribute("DepthSize", de::toString(config.depthSize))
            << Writer::Attribute("Level", de::toString(config.level))
            << Writer::Attribute("MaxPBufferWidth", de::toString(config.maxPBufferWidth))
            << Writer::Attribute("MaxPBufferHeight", de::toString(config.maxPBufferHeight))
            << Writer::Attribute("MaxPBufferPixels", de::toString(config.maxPBufferPixels))
            << Writer::Attribute("MaxSwapInterval", de::toString(config.maxSwapInterval))
            << Writer::Attribute("MinSwapInterval", de::toString(config.minSwapInterval))
            << Writer::Attribute("NativeRenderable", getBoolName(config.nativeRenderable))
            << Writer::Attribute("RenderableType", config.renderableType)
            << Writer::Attribute("SampleBuffers", de::toString(config.sampleBuffers))
            << Writer::Attribute("Samples", de::toString(config.samples))
            << Writer::Attribute("StencilSize", de::toString(config.stencilSize))
            << Writer::Attribute("SurfaceTypes", config.surfaceTypes)
            << Writer::Attribute("TransparentType", config.transparentType)
            << Writer::Attribute("TransparentRedValue", de::toString(config.transparentRedValue))
            << Writer::Attribute("TransparentGreenValue", de::toString(config.transparentGreenValue))
            << Writer::Attribute("TransparentBlueValue", de::toString(config.transparentBlueValue))
            << Writer::EndElement;
        break;
    }

    case ri::TYPE_EGLCONFIGSET:
    {
        const ri::EglConfigSet &configSet = static_cast<const ri::EglConfigSet &>(item);
        dst << Writer::BeginElement("EglConfigSet") << Writer::Attribute("Name", configSet.name)
            << Writer::Attribute("Description", configSet.description);

        for (int ndx = 0; ndx < configSet.configs.getNumItems(); ndx++)
            writeResultItem(configSet.configs.getItem(ndx), dst);

        dst << Writer::EndElement;
        break;
    }

    case ri::TYPE_SAMPLELIST:
    {
        const ri::SampleList &list = static_cast<const ri::SampleList &>(item);
        dst << Writer::BeginElement("SampleList") << Writer::Attribute("Name", list.name)
            << Writer::Attribute("Description", list.description);

        writeResultItem(list.sampleInfo, dst);

        for (int ndx = 0; ndx < list.samples.getNumItems(); ndx++)
            writeResultItem(list.samples.getItem(ndx), dst);

        dst << Writer::EndElement;
        break;
    }

    case ri::TYPE_SAMPLEINFO:
    {
        const ri::SampleInfo &info = static_cast<const ri::SampleInfo &>(item);
        dst << Writer::BeginElement("SampleInfo");
        for (int ndx = 0; ndx < info.valueInfos.getNumItems(); ndx++)
            writeResultItem(info.valueInfos.getItem(ndx), dst);
        dst << Writer::EndElement;
        break;
    }

    case ri::TYPE_VALUEINFO:
    {
        const ri::ValueInfo &info = static_cast<const ri::ValueInfo &>(item);
        dst << Writer::BeginElement("ValueInfo") << Writer::Attribute("Name", info.name)
            << Writer::Attribute("Description", info.description)
            << Writer::Attribute("Tag", getSampleValueTagName(info.tag));
        if (!info.unit.empty())
            dst << Writer::Attribute("Unit", info.unit);
        dst << Writer::EndElement;
        break;
    }

    case ri::TYPE_SAMPLE:
    {
        const ri::Sample &sample = static_cast<const ri::Sample &>(item);
        dst << Writer::BeginElement("Sample");
        for (int ndx = 0; ndx < sample.values.getNumItems(); ndx++)
            writeResultItem(sample.values.getItem(ndx), dst);
        dst << Writer::EndElement;
        break;
    }

    case ri::TYPE_SAMPLEVALUE:
    {
        const ri::SampleValue &value = static_cast<const ri::SampleValue &>(item);
        dst << Writer::BeginElement("Value") << value.value << Writer::EndElement;
        break;
    }

    default:
        XE_FAIL("Unsupported result item");
    }
}

void writeTestResult(const TestCaseResult &result, xe::xml::Writer &xmlWriter)
{
    using xml::Writer;

    xmlWriter << Writer::BeginElement("TestCaseResult") << Writer::Attribute("Version", result.caseVersion)
              << Writer::Attribute("CasePath", result.casePath)
              << Writer::Attribute("CaseType", getTestCaseTypeName(result.caseType));

    for (int ndx = 0; ndx < result.resultItems.getNumItems(); ndx++)
        writeResultItem(result.resultItems.getItem(ndx), xmlWriter);

    // Result item is not logged until end.
    xmlWriter << Writer::BeginElement("Result")
              << Writer::Attribute("StatusCode", getTestStatusCodeName(result.statusCode)) << result.statusDetails
              << Writer::EndElement;

    xmlWriter << Writer::EndElement;
}

void writeTestResult(const TestCaseResult &result, std::ostream &stream)
{
    xml::Writer xmlWriter(stream);
    stream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    writeTestResult(result, xmlWriter);
}

void writeTestResultToFile(const TestCaseResult &result, const char *filename)
{
    std::ofstream str(filename, std::ofstream::binary | std::ofstream::trunc);
    writeTestResult(result, str);
    str.close();
}

} // namespace xe
