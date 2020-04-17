//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FrameCapture.cpp:
//   ANGLE Frame capture implementation.
//

#include "libANGLE/FrameCapture.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <string>

#include "sys/stat.h"

#include "common/system_utils.h"
#include "libANGLE/Context.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/Shader.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/capture_gles_2_0_autogen.h"
#include "libANGLE/capture_gles_3_0_autogen.h"
#include "libANGLE/gl_enum_utils.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/queryutils.h"

#if !ANGLE_CAPTURE_ENABLED
#    error Frame capture must be enbled to include this file.
#endif  // !ANGLE_CAPTURE_ENABLED

namespace angle
{
namespace
{

constexpr char kEnabledVarName[]      = "ANGLE_CAPTURE_ENABLED";
constexpr char kOutDirectoryVarName[] = "ANGLE_CAPTURE_OUT_DIR";
constexpr char kFrameStartVarName[]   = "ANGLE_CAPTURE_FRAME_START";
constexpr char kFrameEndVarName[]     = "ANGLE_CAPTURE_FRAME_END";
constexpr char kCaptureLabel[]        = "ANGLE_CAPTURE_LABEL";

#if defined(ANGLE_PLATFORM_ANDROID)

constexpr char kAndroidCaptureEnabled[] = "debug.angle.capture.enabled";
constexpr char kAndroidOutDir[]         = "debug.angle.capture.out_dir";
constexpr char kAndroidFrameStart[]     = "debug.angle.capture.frame_start";
constexpr char kAndroidFrameEnd[]       = "debug.angle.capture.frame_end";
constexpr char kAndroidCaptureLabel[]   = "debug.angle.capture.label";

constexpr int kStreamSize = 64;

constexpr char kAndroidOutputSubdir[] = "/angle_capture/";

// Call out to 'getprop' on a shell and return a string if the value was set
std::string AndroidGetEnvFromProp(const char *key)
{
    std::string command("getprop ");
    command += key;

    // Run the command and open a I/O stream to read results
    char stream[kStreamSize] = {};
    FILE *pipe               = popen(command.c_str(), "r");
    if (pipe != nullptr)
    {
        fgets(stream, kStreamSize, pipe);
        pclose(pipe);
    }

    // Right strip white space
    std::string result(stream);
    result.erase(result.find_last_not_of(" \n\r\t") + 1);
    return result;
}

void PrimeAndroidEnvironmentVariables()
{
    std::string enabled = AndroidGetEnvFromProp(kAndroidCaptureEnabled);
    if (!enabled.empty())
    {
        INFO() << "Frame capture read " << enabled << " from " << kAndroidCaptureEnabled;
        setenv(kEnabledVarName, enabled.c_str(), 1);
    }

    std::string outDir = AndroidGetEnvFromProp(kAndroidOutDir);
    if (!outDir.empty())
    {
        INFO() << "Frame capture read " << outDir << " from " << kAndroidOutDir;
        setenv(kOutDirectoryVarName, outDir.c_str(), 1);
    }

    std::string frameStart = AndroidGetEnvFromProp(kAndroidFrameStart);
    if (!frameStart.empty())
    {
        INFO() << "Frame capture read " << frameStart << " from " << kAndroidFrameStart;
        setenv(kFrameStartVarName, frameStart.c_str(), 1);
    }

    std::string frameEnd = AndroidGetEnvFromProp(kAndroidFrameEnd);
    if (!frameEnd.empty())
    {
        INFO() << "Frame capture read " << frameEnd << " from " << kAndroidFrameEnd;
        setenv(kFrameEndVarName, frameEnd.c_str(), 1);
    }

    std::string captureLabel = AndroidGetEnvFromProp(kAndroidCaptureLabel);
    if (!captureLabel.empty())
    {
        INFO() << "Capture label read " << captureLabel << " from " << kAndroidCaptureLabel;
        setenv(kCaptureLabel, captureLabel.c_str(), 1);
    }
}
#endif

std::string GetDefaultOutDirectory()
{
#if defined(ANGLE_PLATFORM_ANDROID)
    std::string path = "/sdcard/Android/data/";

    // Linux interface to get application id of the running process
    FILE *cmdline = fopen("/proc/self/cmdline", "r");
    char applicationId[512];
    if (cmdline)
    {
        fread(applicationId, 1, sizeof(applicationId), cmdline);
        fclose(cmdline);

        // Some package may have application id as <app_name>:<cmd_name>
        char *colonSep = strchr(applicationId, ':');
        if (colonSep)
        {
            *colonSep = '\0';
        }
    }
    else
    {
        ERR() << "not able to lookup application id";
    }

    path += std::string(applicationId) + kAndroidOutputSubdir;

    // Check for existance of output path
    struct stat dir_stat;
    if (stat(path.c_str(), &dir_stat) == -1)
    {
        ERR() << "Output directory '" << path
              << "' does not exist.  Create it over adb using mkdir.";
    }

    return path;
#else
    return std::string("./");
#endif  // defined(ANGLE_PLATFORM_ANDROID)
}

struct FmtCapturePrefix
{
    FmtCapturePrefix(int contextIdIn, const std::string &captureLabelIn)
        : contextId(contextIdIn), captureLabel(captureLabelIn)
    {}
    int contextId;
    const std::string &captureLabel;
};

std::ostream &operator<<(std::ostream &os, const FmtCapturePrefix &fmt)
{
    if (fmt.captureLabel.empty())
    {
        os << "angle";
    }
    else
    {
        os << fmt.captureLabel;
    }
    os << "_capture_context" << fmt.contextId;
    return os;
}

struct FmtReplayFunction
{
    FmtReplayFunction(int contextIdIn, uint32_t frameIndexIn)
        : contextId(contextIdIn), frameIndex(frameIndexIn)
    {}
    int contextId;
    uint32_t frameIndex;
};

std::ostream &operator<<(std::ostream &os, const FmtReplayFunction &fmt)
{
    os << "ReplayContext" << fmt.contextId << "Frame" << fmt.frameIndex << "()";
    return os;
}

std::string GetCaptureFileName(int contextId,
                               const std::string &captureLabel,
                               uint32_t frameIndex,
                               const char *suffix)
{
    std::stringstream fnameStream;
    fnameStream << FmtCapturePrefix(contextId, captureLabel) << "_frame" << std::setfill('0')
                << std::setw(3) << frameIndex << suffix;
    return fnameStream.str();
}

std::string GetCaptureFilePath(const std::string &outDir,
                               int contextId,
                               const std::string &captureLabel,
                               uint32_t frameIndex,
                               const char *suffix)
{
    return outDir + GetCaptureFileName(contextId, captureLabel, frameIndex, suffix);
}

void WriteParamStaticVarName(const CallCapture &call,
                             const ParamCapture &param,
                             int counter,
                             std::ostream &out)
{
    out << call.name() << "_" << param.name << "_" << counter;
}

void WriteGLFloatValue(std::ostream &out, GLfloat value)
{
    // Check for non-representable values
    ASSERT(std::numeric_limits<float>::has_infinity);
    ASSERT(std::numeric_limits<float>::has_quiet_NaN);

    if (std::isinf(value))
    {
        float negativeInf = -std::numeric_limits<float>::infinity();
        if (value == negativeInf)
        {
            out << "-";
        }
        out << "std::numeric_limits<float>::infinity()";
    }
    else if (std::isnan(value))
    {
        out << "std::numeric_limits<float>::quiet_NaN()";
    }
    else
    {
        out << value;
    }
}

template <typename T, typename CastT = T>
void WriteInlineData(const std::vector<uint8_t> &vec, std::ostream &out)
{
    const T *data = reinterpret_cast<const T *>(vec.data());
    size_t count  = vec.size() / sizeof(T);

    if (data == nullptr)
    {
        return;
    }

    out << static_cast<CastT>(data[0]);

    for (size_t dataIndex = 1; dataIndex < count; ++dataIndex)
    {
        out << ", " << static_cast<CastT>(data[dataIndex]);
    }
}

template <>
void WriteInlineData<GLfloat>(const std::vector<uint8_t> &vec, std::ostream &out)
{
    const float *data = reinterpret_cast<const GLfloat *>(vec.data());
    size_t count      = vec.size() / sizeof(GLfloat);

    if (data == nullptr)
    {
        return;
    }

    WriteGLFloatValue(out, data[0]);

    for (size_t dataIndex = 1; dataIndex < count; ++dataIndex)
    {
        out << ", ";
        WriteGLFloatValue(out, data[dataIndex]);
    }
}

constexpr size_t kInlineDataThreshold = 128;

void WriteStringParamReplay(std::ostream &out, const ParamCapture &param)
{
    const std::vector<uint8_t> &data = param.data[0];
    // null terminate C style string
    ASSERT(data.size() > 0 && data.back() == '\0');
    std::string str(data.begin(), data.end() - 1);
    out << "\"" << str << "\"";
}

void WriteStringPointerParamReplay(DataCounters *counters,
                                   std::ostream &out,
                                   std::ostream &header,
                                   const CallCapture &call,
                                   const ParamCapture &param)
{
    int counter = counters->getAndIncrement(call.entryPoint, param.name);

    header << "const char *";
    WriteParamStaticVarName(call, param, counter, header);
    header << "[] = { \n";

    for (const std::vector<uint8_t> &data : param.data)
    {
        // null terminate C style string
        ASSERT(data.size() > 0 && data.back() == '\0');
        std::string str(data.begin(), data.end() - 1);
        header << "    R\"(" << str << ")\",\n";
    }

    header << " };\n";
    WriteParamStaticVarName(call, param, counter, out);
}

template <typename ParamT>
void WriteResourceIDPointerParamReplay(DataCounters *counters,
                                       std::ostream &out,
                                       std::ostream &header,
                                       const CallCapture &call,
                                       const ParamCapture &param)
{
    int counter = counters->getAndIncrement(call.entryPoint, param.name);

    header << "const GLuint ";
    WriteParamStaticVarName(call, param, counter, header);
    header << "[] = { ";

    const ResourceIDType resourceIDType = GetResourceIDTypeFromParamType(param.type);
    ASSERT(resourceIDType != ResourceIDType::InvalidEnum);
    const char *name = GetResourceIDTypeName(resourceIDType);

    GLsizei n = call.params.getParamFlexName("n", "count", ParamType::TGLsizei, 0).value.GLsizeiVal;
    ASSERT(param.data.size() == 1);
    const ParamT *returnedIDs = reinterpret_cast<const ParamT *>(param.data[0].data());
    for (GLsizei resIndex = 0; resIndex < n; ++resIndex)
    {
        ParamT id = returnedIDs[resIndex];
        if (resIndex > 0)
        {
            header << ", ";
        }
        header << "g" << name << "Map[" << id.value << "]";
    }

    header << " };\n    ";

    WriteParamStaticVarName(call, param, counter, out);
}

void WriteBinaryParamReplay(DataCounters *counters,
                            std::ostream &out,
                            std::ostream &header,
                            const CallCapture &call,
                            const ParamCapture &param,
                            std::vector<uint8_t> *binaryData)
{
    int counter = counters->getAndIncrement(call.entryPoint, param.name);

    ASSERT(param.data.size() == 1);
    const std::vector<uint8_t> &data = param.data[0];

    if (data.size() > kInlineDataThreshold)
    {
        size_t offset = binaryData->size();
        binaryData->resize(offset + data.size());
        memcpy(binaryData->data() + offset, data.data(), data.size());
        if (param.type == ParamType::TvoidConstPointer || param.type == ParamType::TvoidPointer)
        {
            out << "&gBinaryData[" << offset << "]";
        }
        else
        {
            out << "reinterpret_cast<" << ParamTypeToString(param.type) << ">(&gBinaryData["
                << offset << "])";
        }
    }
    else
    {
        ParamType overrideType = param.type;
        if (param.type == ParamType::TGLvoidConstPointer ||
            param.type == ParamType::TvoidConstPointer)
        {
            overrideType = ParamType::TGLubyteConstPointer;
        }

        std::string paramTypeString = ParamTypeToString(overrideType);
        header << paramTypeString.substr(0, paramTypeString.length() - 1);
        WriteParamStaticVarName(call, param, counter, header);

        header << "[] = { ";

        switch (overrideType)
        {
            case ParamType::TGLintConstPointer:
                WriteInlineData<GLint>(data, header);
                break;
            case ParamType::TGLshortConstPointer:
                WriteInlineData<GLshort>(data, header);
                break;
            case ParamType::TGLfloatConstPointer:
                WriteInlineData<GLfloat>(data, header);
                break;
            case ParamType::TGLubyteConstPointer:
                WriteInlineData<GLubyte, int>(data, header);
                break;
            case ParamType::TGLuintConstPointer:
            case ParamType::TGLenumConstPointer:
                WriteInlineData<GLuint>(data, header);
                break;
            default:
                UNIMPLEMENTED();
                break;
        }

        header << " };\n";

        WriteParamStaticVarName(call, param, counter, out);
    }
}

void WriteCppReplayForCall(const CallCapture &call,
                           DataCounters *counters,
                           std::ostream &out,
                           std::ostream &header,
                           std::vector<uint8_t> *binaryData)
{
    std::ostringstream callOut;

    if (call.entryPoint == gl::EntryPoint::CreateShader ||
        call.entryPoint == gl::EntryPoint::CreateProgram)
    {
        GLuint id = call.params.getReturnValue().value.GLuintVal;
        callOut << "gShaderProgramMap[" << id << "] = ";
    }

    callOut << call.name() << "(";

    bool first = true;
    for (const ParamCapture &param : call.params.getParamCaptures())
    {
        if (!first)
        {
            callOut << ", ";
        }

        if (param.arrayClientPointerIndex != -1)
        {
            callOut << "gClientArrays[" << param.arrayClientPointerIndex << "]";
        }
        else if (param.readBufferSizeBytes > 0)
        {
            callOut << "reinterpret_cast<" << ParamTypeToString(param.type) << ">(gReadBuffer)";
        }
        else if (param.data.empty())
        {
            if (param.type == ParamType::TGLenum)
            {
                OutputGLenumString(callOut, param.enumGroup, param.value.GLenumVal);
            }
            else if (param.type == ParamType::TGLbitfield)
            {
                OutputGLbitfieldString(callOut, param.enumGroup, param.value.GLbitfieldVal);
            }
            else if (param.type == ParamType::TGLfloat)
            {
                WriteGLFloatValue(callOut, param.value.GLfloatVal);
            }
            else
            {
                callOut << param;
            }
        }
        else
        {
            switch (param.type)
            {
                case ParamType::TGLcharConstPointer:
                    WriteStringParamReplay(callOut, param);
                    break;
                case ParamType::TGLcharConstPointerPointer:
                    WriteStringPointerParamReplay(counters, callOut, header, call, param);
                    break;
                case ParamType::TBufferIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::BufferID>(counters, callOut, out, call,
                                                                    param);
                    break;
                case ParamType::TFenceNVIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::FenceNVID>(counters, callOut, out, call,
                                                                     param);
                    break;
                case ParamType::TFramebufferIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::FramebufferID>(counters, callOut, out,
                                                                         call, param);
                    break;
                case ParamType::TMemoryObjectIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::MemoryObjectID>(counters, callOut, out,
                                                                          call, param);
                    break;
                case ParamType::TProgramPipelineIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::ProgramPipelineID>(counters, callOut, out,
                                                                             call, param);
                    break;
                case ParamType::TQueryIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::QueryID>(counters, callOut, out, call,
                                                                   param);
                    break;
                case ParamType::TRenderbufferIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::RenderbufferID>(counters, callOut, out,
                                                                          call, param);
                    break;
                case ParamType::TSamplerIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::SamplerID>(counters, callOut, out, call,
                                                                     param);
                    break;
                case ParamType::TSemaphoreIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::SemaphoreID>(counters, callOut, out, call,
                                                                       param);
                    break;
                case ParamType::TTextureIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::TextureID>(counters, callOut, out, call,
                                                                     param);
                    break;
                case ParamType::TTransformFeedbackIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::TransformFeedbackID>(counters, callOut,
                                                                               out, call, param);
                    break;
                case ParamType::TVertexArrayIDConstPointer:
                    WriteResourceIDPointerParamReplay<gl::VertexArrayID>(counters, callOut, out,
                                                                         call, param);
                    break;
                default:
                    WriteBinaryParamReplay(counters, callOut, header, call, param, binaryData);
                    break;
            }
        }

        first = false;
    }

    callOut << ")";

    out << callOut.str();
}

size_t MaxClientArraySize(const gl::AttribArray<size_t> &clientArraySizes)
{
    size_t found = 0;
    for (size_t size : clientArraySizes)
    {
        if (size > found)
            found = size;
    }

    return found;
}

struct SaveFileHelper
{
    SaveFileHelper(const std::string &filePathIn, std::ios_base::openmode mode = std::ios::out)
        : ofs(filePathIn, mode), filePath(filePathIn)
    {
        if (!ofs.is_open())
        {
            FATAL() << "Could not open " << filePathIn;
        }
    }

    ~SaveFileHelper() { printf("Saved '%s'.\n", filePath.c_str()); }

    template <typename T>
    SaveFileHelper &operator<<(const T &value)
    {
        ofs << value;
        if (ofs.bad())
        {
            FATAL() << "Error writing to " << filePath;
        }
        return *this;
    }

    std::ofstream ofs;
    std::string filePath;
};

void SaveBinaryData(const std::string &outDir,
                    std::ostream &out,
                    int contextId,
                    const std::string &captureLabel,
                    uint32_t frameIndex,
                    const char *suffix,
                    const std::vector<uint8_t> &binaryData)
{
    std::string binaryDataFileName =
        GetCaptureFileName(contextId, captureLabel, frameIndex, suffix);

    out << "    LoadBinaryData(\"" << binaryDataFileName << "\", "
        << static_cast<int>(binaryData.size()) << ");\n";

    std::string dataFilepath =
        GetCaptureFilePath(outDir, contextId, captureLabel, frameIndex, suffix);

    SaveFileHelper saveData(dataFilepath, std::ios::binary);
    saveData.ofs.write(reinterpret_cast<const char *>(binaryData.data()), binaryData.size());
}

void WriteCppReplay(const std::string &outDir,
                    int contextId,
                    const std::string &captureLabel,
                    uint32_t frameIndex,
                    const std::vector<CallCapture> &frameCalls,
                    const std::vector<CallCapture> &setupCalls)
{
    DataCounters counters;

    std::stringstream out;
    std::stringstream header;

    header << "#include \"" << FmtCapturePrefix(contextId, captureLabel) << ".h\"\n";
    header << "";
    header << "\n";
    header << "namespace\n";
    header << "{\n";

    if (!captureLabel.empty())
    {
        out << "namespace " << captureLabel << "\n";
        out << "{\n";
    }

    if (frameIndex == 0 || !setupCalls.empty())
    {
        out << "void SetupContext" << Str(contextId) << "Replay()\n";
        out << "{\n";

        std::stringstream setupCallStream;
        std::vector<uint8_t> setupBinaryData;

        for (const CallCapture &call : setupCalls)
        {
            setupCallStream << "    ";
            WriteCppReplayForCall(call, &counters, setupCallStream, header, &setupBinaryData);
            setupCallStream << ";\n";
        }

        if (!setupBinaryData.empty())
        {
            SaveBinaryData(outDir, out, contextId, captureLabel, frameIndex, ".setup.angledata",
                           setupBinaryData);
        }

        out << setupCallStream.str();

        out << "}\n";
        out << "\n";
    }

    out << "void " << FmtReplayFunction(contextId, frameIndex) << "\n";
    out << "{\n";

    std::stringstream callStream;
    std::vector<uint8_t> binaryData;

    for (const CallCapture &call : frameCalls)
    {
        callStream << "    ";
        WriteCppReplayForCall(call, &counters, callStream, header, &binaryData);
        callStream << ";\n";
    }

    if (!binaryData.empty())
    {
        SaveBinaryData(outDir, out, contextId, captureLabel, frameIndex, ".angledata", binaryData);
    }

    out << callStream.str();
    out << "}\n";

    if (!captureLabel.empty())
    {
        out << "} // namespace " << captureLabel << "\n";
    }

    header << "}  // namespace\n";

    {
        std::string outString    = out.str();
        std::string headerString = header.str();

        std::string cppFilePath =
            GetCaptureFilePath(outDir, contextId, captureLabel, frameIndex, ".cpp");

        SaveFileHelper saveCpp(cppFilePath);
        saveCpp << headerString << "\n" << outString;
    }
}

void WriteCppReplayIndexFiles(const std::string &outDir,
                              int contextId,
                              const std::string &captureLabel,
                              uint32_t frameStart,
                              uint32_t frameEnd,
                              size_t readBufferSize,
                              const gl::AttribArray<size_t> &clientArraySizes,
                              const HasResourceTypeMap &hasResourceType)
{
    size_t maxClientArraySize = MaxClientArraySize(clientArraySizes);

    std::stringstream header;
    std::stringstream source;

    header << "#pragma once\n";
    header << "\n";
    header << "#include \"util/gles_loader_autogen.h\"\n";
    header << "\n";
    header << "#include <cstdint>\n";
    header << "#include <cstdio>\n";
    header << "#include <cstring>\n";
    header << "#include <limits>\n";
    header << "#include <unordered_map>\n";
    header << "\n";
    header << "// Replay functions\n";
    header << "\n";
    header << "using ResourceMap = std::unordered_map<GLuint, GLuint>;\n";
    header << "\n";
    if (!captureLabel.empty())
    {
        header << "namespace " << captureLabel << "\n";
        header << "{\n";
    }
    header << "constexpr uint32_t kReplayFrameStart = " << frameStart << ";\n";
    header << "constexpr uint32_t kReplayFrameEnd = " << frameEnd << ";\n";
    header << "\n";
    header << "void SetupContext" << contextId << "Replay();\n";
    header << "void ReplayContext" << contextId << "Frame(uint32_t frameIndex);\n";
    header << "\n";
    for (uint32_t frameIndex = frameStart; frameIndex < frameEnd; ++frameIndex)
    {
        header << "void " << FmtReplayFunction(contextId, frameIndex) << ";\n";
    }
    header << "\n";
    header << "void SetBinaryDataDir(const char *dataDir);\n";
    header << "void LoadBinaryData(const char *fileName, size_t size);\n";
    header << "\n";
    header << "// Global state\n";
    header << "\n";
    header << "extern uint8_t *gBinaryData;\n";

    source << "#include \"" << FmtCapturePrefix(contextId, captureLabel) << ".h\"\n";
    source << "\n";
    source << "namespace\n";
    source << "{\n";
    source << "void UpdateResourceMap(ResourceMap *resourceMap, GLuint id, GLsizei "
              "readBufferOffset)\n";
    source << "{\n";
    source << "    GLuint returnedID;\n";
    std::string captureNamespace = !captureLabel.empty() ? captureLabel + "::" : "";
    source << "    memcpy(&returnedID, &" << captureNamespace
           << "gReadBuffer[readBufferOffset], sizeof(GLuint));\n ";
    source << "    (*resourceMap)[id] = returnedID;\n";
    source << "}\n";
    source << "\n";
    source << "const char *gBinaryDataDir = \".\";\n";
    source << "}  // namespace\n";
    source << "\n";

    if (!captureLabel.empty())
    {
        source << "namespace " << captureLabel << "\n";
        source << "{\n";
    }

    source << "uint8_t *gBinaryData = nullptr;\n";

    if (readBufferSize > 0)
    {
        header << "extern uint8_t gReadBuffer[" << readBufferSize << "];\n";
        source << "uint8_t gReadBuffer[" << readBufferSize << "];\n";
    }
    if (maxClientArraySize > 0)
    {
        header << "extern uint8_t gClientArrays[" << gl::MAX_VERTEX_ATTRIBS << "]["
               << maxClientArraySize << "];\n";
        source << "uint8_t gClientArrays[" << gl::MAX_VERTEX_ATTRIBS << "][" << maxClientArraySize
               << "];\n";
    }
    for (ResourceIDType resourceType : AllEnums<ResourceIDType>())
    {
        if (!hasResourceType[resourceType])
            continue;

        const char *name = GetResourceIDTypeName(resourceType);
        header << "extern ResourceMap g" << name << "Map;\n";
        source << "ResourceMap g" << name << "Map;\n";
    }

    header << "\n";

    source << "\n";
    source << "void ReplayContext" << contextId << "Frame(uint32_t frameIndex)\n";
    source << "{\n";
    source << "    switch (frameIndex)\n";
    source << "    {\n";
    for (uint32_t frameIndex = frameStart; frameIndex < frameEnd; ++frameIndex)
    {
        source << "        case " << frameIndex << ":\n";
        source << "            ReplayContext" << contextId << "Frame" << frameIndex << "();\n";
        source << "            break;\n";
    }
    source << "        default:\n";
    source << "            break;\n";
    source << "    }\n";
    source << "}\n";
    source << "\n";
    source << "void SetBinaryDataDir(const char *dataDir)\n";
    source << "{\n";
    source << "    gBinaryDataDir = dataDir;\n";
    source << "}\n";
    source << "\n";
    source << "void LoadBinaryData(const char *fileName, size_t size)\n";
    source << "{\n";
    source << "    if (gBinaryData != nullptr)\n";
    source << "    {\n";
    source << "        delete [] gBinaryData;\n";
    source << "    }\n";
    source << "    gBinaryData = new uint8_t[size];\n";
    source << "    char pathBuffer[1000] = {};\n";
    source << "    sprintf(pathBuffer, \"%s/%s\", gBinaryDataDir, fileName);\n";
    source << "    FILE *fp = fopen(pathBuffer, \"rb\");\n";
    source << "    (void)fread(gBinaryData, 1, size, fp);\n";
    source << "    fclose(fp);\n";
    source << "}\n";

    if (maxClientArraySize > 0)
    {
        header
            << "void UpdateClientArrayPointer(int arrayIndex, const void *data, uint64_t size);\n";

        source << "\n";
        source << "void UpdateClientArrayPointer(int arrayIndex, const void *data, uint64_t size)"
               << "\n";
        source << "{\n";
        source << "    memcpy(gClientArrays[arrayIndex], data, size);\n";
        source << "}\n";
    }

    for (ResourceIDType resourceType : AllEnums<ResourceIDType>())
    {
        if (!hasResourceType[resourceType])
            continue;
        const char *name = GetResourceIDTypeName(resourceType);
        header << "void Update" << name << "ID(GLuint id, GLsizei readBufferOffset);\n";

        source << "\n";
        source << "void Update" << name << "ID(GLuint id, GLsizei readBufferOffset)\n";
        source << "{\n";
        source << "    UpdateResourceMap(&g" << name << "Map, id, readBufferOffset);\n";
        source << "}\n";
    }

    if (!captureLabel.empty())
    {
        header << "} // namespace " << captureLabel << "\n";
        source << "} // namespace " << captureLabel << "\n";
    }

    {
        std::string headerContents = header.str();

        std::stringstream headerPathStream;
        headerPathStream << outDir << FmtCapturePrefix(contextId, captureLabel) << ".h";
        std::string headerPath = headerPathStream.str();

        SaveFileHelper saveHeader(headerPath);
        saveHeader << headerContents;
    }

    {
        std::string sourceContents = source.str();

        std::stringstream sourcePathStream;
        sourcePathStream << outDir << FmtCapturePrefix(contextId, captureLabel) << ".cpp";
        std::string sourcePath = sourcePathStream.str();

        SaveFileHelper saveSource(sourcePath);
        saveSource << sourceContents;
    }

    {
        std::stringstream indexPathStream;
        indexPathStream << outDir << FmtCapturePrefix(contextId, captureLabel) << "_files.txt";
        std::string indexPath = indexPathStream.str();

        SaveFileHelper saveIndex(indexPath);
        for (uint32_t frameIndex = frameStart; frameIndex <= frameEnd; ++frameIndex)
        {
            saveIndex << GetCaptureFileName(contextId, captureLabel, frameIndex, ".cpp") << "\n";
        }
    }
}

ProgramSources GetAttachedProgramSources(const gl::Program *program)
{
    ProgramSources sources;
    for (gl::ShaderType shaderType : gl::AllShaderTypes())
    {
        const gl::Shader *shader = program->getAttachedShader(shaderType);
        if (shader)
        {
            sources[shaderType] = shader->getSourceString();
        }
    }
    return sources;
}

template <typename IDType>
void CaptureUpdateResourceIDs(const CallCapture &call,
                              const ParamCapture &param,
                              std::vector<CallCapture> *callsOut)
{
    GLsizei n = call.params.getParamFlexName("n", "count", ParamType::TGLsizei, 0).value.GLsizeiVal;
    ASSERT(param.data.size() == 1);
    ResourceIDType resourceIDType = GetResourceIDTypeFromParamType(param.type);
    ASSERT(resourceIDType != ResourceIDType::InvalidEnum);
    const char *resourceName = GetResourceIDTypeName(resourceIDType);

    std::stringstream updateFuncNameStr;
    updateFuncNameStr << "Update" << resourceName << "ID";
    std::string updateFuncName = updateFuncNameStr.str();

    const IDType *returnedIDs = reinterpret_cast<const IDType *>(param.data[0].data());

    for (GLsizei idIndex = 0; idIndex < n; ++idIndex)
    {
        IDType id                = returnedIDs[idIndex];
        GLsizei readBufferOffset = idIndex * sizeof(gl::RenderbufferID);
        ParamBuffer params;
        params.addValueParam("id", ParamType::TGLuint, id.value);
        params.addValueParam("readBufferOffset", ParamType::TGLsizei, readBufferOffset);
        callsOut->emplace_back(updateFuncName, std::move(params));
    }
}

void MaybeCaptureUpdateResourceIDs(std::vector<CallCapture> *callsOut)
{
    const CallCapture &call = callsOut->back();

    switch (call.entryPoint)
    {
        case gl::EntryPoint::GenBuffers:
        {
            const ParamCapture &buffers =
                call.params.getParam("buffersPacked", ParamType::TBufferIDPointer, 1);
            CaptureUpdateResourceIDs<gl::BufferID>(call, buffers, callsOut);
            break;
        }

        case gl::EntryPoint::GenFencesNV:
        {
            const ParamCapture &fences =
                call.params.getParam("fencesPacked", ParamType::TFenceNVIDPointer, 1);
            CaptureUpdateResourceIDs<gl::FenceNVID>(call, fences, callsOut);
            break;
        }

        case gl::EntryPoint::GenFramebuffers:
        case gl::EntryPoint::GenFramebuffersOES:
        {
            const ParamCapture &framebuffers =
                call.params.getParam("framebuffersPacked", ParamType::TFramebufferIDPointer, 1);
            CaptureUpdateResourceIDs<gl::FramebufferID>(call, framebuffers, callsOut);
            break;
        }

        case gl::EntryPoint::GenPathsCHROMIUM:
        {
            // TODO(jmadill): Handle path IDs. http://anglebug.com/3662
            break;
        }

        case gl::EntryPoint::GenProgramPipelines:
        {
            const ParamCapture &pipelines =
                call.params.getParam("pipelinesPacked", ParamType::TProgramPipelineIDPointer, 1);
            CaptureUpdateResourceIDs<gl::ProgramPipelineID>(call, pipelines, callsOut);
            break;
        }

        case gl::EntryPoint::GenQueries:
        case gl::EntryPoint::GenQueriesEXT:
        {
            const ParamCapture &queries =
                call.params.getParam("idsPacked", ParamType::TQueryIDPointer, 1);
            CaptureUpdateResourceIDs<gl::QueryID>(call, queries, callsOut);
            break;
        }

        case gl::EntryPoint::GenRenderbuffers:
        case gl::EntryPoint::GenRenderbuffersOES:
        {
            const ParamCapture &renderbuffers =
                call.params.getParam("renderbuffersPacked", ParamType::TRenderbufferIDPointer, 1);
            CaptureUpdateResourceIDs<gl::RenderbufferID>(call, renderbuffers, callsOut);
            break;
        }

        case gl::EntryPoint::GenSamplers:
        {
            const ParamCapture &samplers =
                call.params.getParam("samplersPacked", ParamType::TSamplerIDPointer, 1);
            CaptureUpdateResourceIDs<gl::SamplerID>(call, samplers, callsOut);
            break;
        }

        case gl::EntryPoint::GenSemaphoresEXT:
        {
            const ParamCapture &semaphores =
                call.params.getParam("semaphoresPacked", ParamType::TSemaphoreIDPointer, 1);
            CaptureUpdateResourceIDs<gl::SemaphoreID>(call, semaphores, callsOut);
            break;
        }

        case gl::EntryPoint::GenTextures:
        {
            const ParamCapture &textures =
                call.params.getParam("texturesPacked", ParamType::TTextureIDPointer, 1);
            CaptureUpdateResourceIDs<gl::TextureID>(call, textures, callsOut);
            break;
        }

        case gl::EntryPoint::GenTransformFeedbacks:
        {
            const ParamCapture &xfbs =
                call.params.getParam("idsPacked", ParamType::TTransformFeedbackIDPointer, 1);
            CaptureUpdateResourceIDs<gl::TransformFeedbackID>(call, xfbs, callsOut);
            break;
        }

        case gl::EntryPoint::GenVertexArrays:
        case gl::EntryPoint::GenVertexArraysOES:
        {
            const ParamCapture &vertexArrays =
                call.params.getParam("arraysPacked", ParamType::TVertexArrayIDPointer, 1);
            CaptureUpdateResourceIDs<gl::VertexArrayID>(call, vertexArrays, callsOut);
            break;
        }

        default:
            break;
    }
}

bool IsDefaultCurrentValue(const gl::VertexAttribCurrentValueData &currentValue)
{
    if (currentValue.Type != gl::VertexAttribType::Float)
        return false;

    return currentValue.Values.FloatValues[0] == 0.0f &&
           currentValue.Values.FloatValues[1] == 0.0f &&
           currentValue.Values.FloatValues[2] == 0.0f && currentValue.Values.FloatValues[3] == 1.0f;
}

void CaptureMidExecutionSetup(const gl::Context *context,
                              std::vector<CallCapture> *setupCalls,
                              const ShaderSourceMap &cachedShaderSources,
                              const ProgramSourceMap &cachedProgramSources)
{
    const gl::State &apiState = context->getState();
    gl::State replayState(0, nullptr, nullptr, nullptr, EGL_OPENGL_ES_API,
                          apiState.getClientVersion(), false, true, true, true, false,
                          EGL_CONTEXT_PRIORITY_MEDIUM_IMG);

    // Small helper function to make the code more readable.
    auto cap = [setupCalls](CallCapture &&call) { setupCalls->emplace_back(std::move(call)); };

    // Currently this code assumes we can use create-on-bind. It does not support 'Gen' usage.
    // TODO(jmadill): Use handle mapping for captured objects. http://anglebug.com/3662

    // Capture Buffer data.
    const gl::BufferManager &buffers       = apiState.getBufferManagerForCapture();
    const gl::BoundBufferMap &boundBuffers = apiState.getBoundBuffersForCapture();

    for (const auto &bufferIter : buffers)
    {
        gl::BufferID id    = {bufferIter.first};
        gl::Buffer *buffer = bufferIter.second;

        if (id.value == 0)
        {
            continue;
        }

        // glBufferData. Would possibly be better implemented using a getData impl method.
        // Saving buffers that are mapped during a swap is not yet handled.
        if (buffer->getSize() == 0)
        {
            continue;
        }
        ASSERT(!buffer->isMapped());
        (void)buffer->mapRange(context, 0, static_cast<GLsizeiptr>(buffer->getSize()),
                               GL_MAP_READ_BIT);

        // Generate binding.
        cap(CaptureGenBuffers(replayState, true, 1, &id));
        MaybeCaptureUpdateResourceIDs(setupCalls);

        // Always use the array buffer binding point to upload data to keep things simple.
        if (buffer != replayState.getArrayBuffer())
        {
            replayState.setBufferBinding(context, gl::BufferBinding::Array, buffer);
            cap(CaptureBindBuffer(replayState, true, gl::BufferBinding::Array, id));
        }

        cap(CaptureBufferData(replayState, true, gl::BufferBinding::Array,
                              static_cast<GLsizeiptr>(buffer->getSize()), buffer->getMapPointer(),
                              buffer->getUsage()));

        GLboolean dontCare;
        (void)buffer->unmap(context, &dontCare);
    }

    // Vertex input states. Only handles GLES 2.0 states right now.
    // Must happen after buffer data initialization.
    // TODO(http://anglebug.com/3662): Complete state capture.
    const std::vector<gl::VertexAttribCurrentValueData> &currentValues =
        apiState.getVertexAttribCurrentValues();
    const std::vector<gl::VertexAttribute> &vertexAttribs =
        apiState.getVertexArray()->getVertexAttributes();
    const std::vector<gl::VertexBinding> &vertexBindings =
        apiState.getVertexArray()->getVertexBindings();

    for (GLuint attribIndex = 0; attribIndex < gl::MAX_VERTEX_ATTRIBS; ++attribIndex)
    {
        const gl::VertexAttribCurrentValueData &currentValue = currentValues[attribIndex];
        if (!IsDefaultCurrentValue(currentValue))
        {
            cap(CaptureVertexAttrib4fv(replayState, true, attribIndex,
                                       currentValue.Values.FloatValues));
        }

        const gl::VertexAttribute &attrib = vertexAttribs[attribIndex];
        const gl::VertexBinding &binding  = vertexBindings[attrib.bindingIndex];

        const gl::VertexAttribute defaultAttrib(attribIndex);
        const gl::VertexBinding defaultBinding;

        if (attrib.enabled != defaultAttrib.enabled)
        {
            cap(CaptureEnableVertexAttribArray(replayState, false, attribIndex));
        }

        if (attrib.format != defaultAttrib.format || attrib.pointer != defaultAttrib.pointer ||
            binding.getStride() != defaultBinding.getStride() ||
            binding.getBuffer().get() != nullptr)
        {
            gl::Buffer *buffer = binding.getBuffer().get();

            if (buffer != replayState.getArrayBuffer())
            {
                replayState.setBufferBinding(context, gl::BufferBinding::Array, buffer);
                cap(CaptureBindBuffer(replayState, true, gl::BufferBinding::Array, buffer->id()));
            }

            cap(CaptureVertexAttribPointer(replayState, true, attribIndex,
                                           attrib.format->channelCount,
                                           attrib.format->vertexAttribType, attrib.format->isNorm(),
                                           binding.getStride(), attrib.pointer));
        }

        if (binding.getDivisor() != 0)
        {
            cap(CaptureVertexAttribDivisor(replayState, true, attribIndex, binding.getDivisor()));
        }
    }

    // Capture Buffer bindings.
    for (gl::BufferBinding binding : angle::AllEnums<gl::BufferBinding>())
    {
        gl::BufferID bufferID = boundBuffers[binding].id();

        // Filter out redundant buffer binding commands. Note that the code in the previous section
        // only binds to ARRAY_BUFFER. Therefore we only check the array binding against the binding
        // we set earlier.
        bool isArray                  = binding == gl::BufferBinding::Array;
        const gl::Buffer *arrayBuffer = replayState.getArrayBuffer();
        if ((isArray && arrayBuffer->id() != bufferID) || (!isArray && bufferID.value != 0))
        {
            cap(CaptureBindBuffer(replayState, true, binding, bufferID));
        }
    }

    // Set a pack alignment of 1.
    gl::PixelPackState &currentPackState = replayState.getPackState();
    if (currentPackState.alignment != 1)
    {
        cap(CapturePixelStorei(replayState, true, GL_UNPACK_ALIGNMENT, 1));
        currentPackState.alignment = 1;
    }

    // Capture Texture setup and data.
    const gl::TextureManager &textures         = apiState.getTextureManagerForCapture();
    const gl::TextureBindingMap &boundTextures = apiState.getBoundTexturesForCapture();

    gl::TextureTypeMap<gl::TextureID> currentTextureBindings;

    for (const auto &textureIter : textures)
    {
        gl::TextureID id           = {textureIter.first};
        const gl::Texture *texture = textureIter.second;

        if (id.value == 0)
        {
            continue;
        }

        // Gen the Texture.
        cap(CaptureGenTextures(replayState, true, 1, &id));
        MaybeCaptureUpdateResourceIDs(setupCalls);
        cap(CaptureBindTexture(replayState, true, texture->getType(), id));

        currentTextureBindings[texture->getType()] = id;

        // Capture sampler parameter states.
        // TODO(jmadill): More sampler / texture states. http://anglebug.com/3662
        gl::SamplerState defaultSamplerState =
            gl::SamplerState::CreateDefaultForTarget(texture->getType());
        const gl::SamplerState &textureSamplerState = texture->getSamplerState();

        auto capTexParam = [cap, &replayState, texture](GLenum pname, GLint param) {
            cap(CaptureTexParameteri(replayState, true, texture->getType(), pname, param));
        };

        if (textureSamplerState.getMinFilter() != defaultSamplerState.getMinFilter())
        {
            capTexParam(GL_TEXTURE_MIN_FILTER, textureSamplerState.getMinFilter());
        }

        if (textureSamplerState.getMagFilter() != defaultSamplerState.getMagFilter())
        {
            capTexParam(GL_TEXTURE_MAG_FILTER, textureSamplerState.getMagFilter());
        }

        if (textureSamplerState.getWrapR() != defaultSamplerState.getWrapR())
        {
            capTexParam(GL_TEXTURE_WRAP_R, textureSamplerState.getWrapR());
        }

        if (textureSamplerState.getWrapS() != defaultSamplerState.getWrapS())
        {
            capTexParam(GL_TEXTURE_WRAP_S, textureSamplerState.getWrapS());
        }

        if (textureSamplerState.getWrapT() != defaultSamplerState.getWrapT())
        {
            capTexParam(GL_TEXTURE_WRAP_T, textureSamplerState.getWrapT());
        }

        // Iterate texture levels and layers.
        gl::ImageIndexIterator imageIter = gl::ImageIndexIterator::MakeGeneric(
            texture->getType(), 0, texture->getMipmapMaxLevel() + 1, gl::ImageIndex::kEntireLevel,
            gl::ImageIndex::kEntireLevel);
        while (imageIter.hasNext())
        {
            gl::ImageIndex index = imageIter.next();

            const gl::ImageDesc &desc = texture->getTextureState().getImageDesc(index);

            if (desc.size.empty())
                continue;

            const gl::InternalFormat &format = *desc.format.info;

            // Assume 2D or Cube for now.
            // TODO(jmadill): 3D and 2D array textures. http://anglebug.com/4048
            ASSERT(index.getType() == gl::TextureType::_2D ||
                   index.getType() == gl::TextureType::CubeMap);

            angle::MemoryBuffer data;

            // Use ANGLE_get_image to read back pixel data.
            if (context->getExtensions().getImageANGLE)
            {
                GLenum internalFormat = format.internalFormat;
                GLenum getFormat      = format.format;
                GLenum getType        = format.type;
                GLsizei pixelBytes    = format.pixelBytes;

                if (format.compressed)
                {
                    // Determine if we're emulating the compressed format.
                    GLenum readFormat = texture->getImplementationColorReadFormat(context);
                    GLenum readType   = texture->getImplementationColorReadType(context);

                    const gl::InternalFormat &nativeFormat =
                        gl::GetInternalFormatInfo(readFormat, readType);

                    if (!nativeFormat.compressed)
                    {
                        // Emulate with a non-compressed format.
                        internalFormat = nativeFormat.internalFormat;
                        getFormat      = readFormat;
                        getType        = readType;
                        pixelBytes     = nativeFormat.pixelBytes;
                    }
                    else
                    {
                        // Will need to add glGetCompressedTexImage support to ANGLE_get_image.
                        // TODO(jmadill): add glGetCompressedTexImage. http://anglebug.com/3944
                        UNIMPLEMENTED();
                    }
                }

                uint32_t dataSize = pixelBytes * desc.size.width * desc.size.height;

                bool result = data.resize(dataSize);
                ASSERT(result);

                gl::PixelPackState packState;
                packState.alignment = 1;

                (void)texture->getTexImage(context, packState, nullptr, index.getTarget(),
                                           index.getLevelIndex(), getFormat, getType, data.data());

                cap(CaptureTexImage2D(replayState, true, index.getTarget(), index.getLevelIndex(),
                                      internalFormat, desc.size.width, desc.size.height, 0,
                                      getFormat, getType, data.data()));
            }
            else
            {
                cap(CaptureTexImage2D(replayState, true, index.getTarget(), index.getLevelIndex(),
                                      format.internalFormat, desc.size.width, desc.size.height, 0,
                                      format.format, format.type, nullptr));
            }
        }
    }

    // Set Texture bindings.
    size_t currentActiveTexture = 0;
    for (gl::TextureType textureType : angle::AllEnums<gl::TextureType>())
    {
        const gl::TextureBindingVector &bindings = boundTextures[textureType];
        for (size_t bindingIndex = 0; bindingIndex < bindings.size(); ++bindingIndex)
        {
            gl::TextureID textureID = bindings[bindingIndex].id();

            if (textureID.value != 0)
            {
                if (currentActiveTexture != bindingIndex)
                {
                    cap(CaptureActiveTexture(replayState, true,
                                             GL_TEXTURE0 + static_cast<GLenum>(bindingIndex)));
                    currentActiveTexture = bindingIndex;
                }

                if (currentTextureBindings[textureType] != textureID)
                {
                    cap(CaptureBindTexture(replayState, true, textureType, textureID));
                    currentTextureBindings[textureType] = textureID;
                }
            }
        }
    }

    // Set active Texture.
    size_t stateActiveTexture = apiState.getActiveSampler();
    if (currentActiveTexture != stateActiveTexture)
    {
        cap(CaptureActiveTexture(replayState, true,
                                 GL_TEXTURE0 + static_cast<GLenum>(stateActiveTexture)));
    }

    // Capture Renderbuffers.
    const gl::RenderbufferManager &renderbuffers = apiState.getRenderbufferManagerForCapture();

    gl::RenderbufferID currentRenderbuffer = {0};
    for (const auto &renderbufIter : renderbuffers)
    {
        gl::RenderbufferID id                = {renderbufIter.first};
        const gl::Renderbuffer *renderbuffer = renderbufIter.second;

        // Generate renderbuffer id.
        cap(CaptureGenRenderbuffers(replayState, true, 1, &id));
        MaybeCaptureUpdateResourceIDs(setupCalls);
        cap(CaptureBindRenderbuffer(replayState, true, GL_RENDERBUFFER, id));

        currentRenderbuffer = id;

        GLenum internalformat = renderbuffer->getFormat().info->internalFormat;

        if (renderbuffer->getSamples() > 0)
        {
            // Note: We could also use extensions if available.
            cap(CaptureRenderbufferStorageMultisample(
                replayState, true, GL_RENDERBUFFER, renderbuffer->getSamples(), internalformat,
                renderbuffer->getWidth(), renderbuffer->getHeight()));
        }
        else
        {
            cap(CaptureRenderbufferStorage(replayState, true, GL_RENDERBUFFER, internalformat,
                                           renderbuffer->getWidth(), renderbuffer->getHeight()));
        }

        // TODO(jmadill): Capture renderbuffer contents. http://anglebug.com/3662
    }

    // Set Renderbuffer binding.
    if (currentRenderbuffer != apiState.getRenderbufferId())
    {
        cap(CaptureBindRenderbuffer(replayState, true, GL_RENDERBUFFER,
                                    apiState.getRenderbufferId()));
    }

    // Capture Framebuffers.
    const gl::FramebufferManager &framebuffers = apiState.getFramebufferManagerForCapture();

    gl::FramebufferID currentDrawFramebuffer = {0};
    gl::FramebufferID currentReadFramebuffer = {0};

    for (const auto &framebufferIter : framebuffers)
    {
        gl::FramebufferID id               = {framebufferIter.first};
        const gl::Framebuffer *framebuffer = framebufferIter.second;

        // The default Framebuffer exists (by default).
        if (framebuffer->isDefault())
            continue;

        cap(CaptureGenFramebuffers(replayState, true, 1, &id));
        MaybeCaptureUpdateResourceIDs(setupCalls);
        cap(CaptureBindFramebuffer(replayState, true, GL_FRAMEBUFFER, id));
        currentDrawFramebuffer = currentReadFramebuffer = id;

        // Color Attachments.
        for (const gl::FramebufferAttachment &colorAttachment : framebuffer->getColorAttachments())
        {
            if (!colorAttachment.isAttached())
            {
                continue;
            }

            GLuint resourceID = colorAttachment.getResource()->getId();

            // TODO(jmadill): Layer attachments. http://anglebug.com/3662
            if (colorAttachment.type() == GL_TEXTURE)
            {
                gl::ImageIndex index = colorAttachment.getTextureImageIndex();

                cap(CaptureFramebufferTexture2D(replayState, true, GL_FRAMEBUFFER,
                                                colorAttachment.getBinding(), index.getTarget(),
                                                {resourceID}, index.getLevelIndex()));
            }
            else
            {
                ASSERT(colorAttachment.type() == GL_RENDERBUFFER);
                cap(CaptureFramebufferRenderbuffer(replayState, true, GL_FRAMEBUFFER,
                                                   colorAttachment.getBinding(), GL_RENDERBUFFER,
                                                   {resourceID}));
            }
        }

        const gl::FramebufferAttachment *depthAttachment = framebuffer->getDepthAttachment();
        if (depthAttachment)
        {
            ASSERT(depthAttachment->type() == GL_RENDERBUFFER);
            GLuint resourceID = depthAttachment->getResource()->getId();
            cap(CaptureFramebufferRenderbuffer(replayState, true, GL_FRAMEBUFFER,
                                               GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, {resourceID}));
        }

        const gl::FramebufferAttachment *stencilAttachment = framebuffer->getStencilAttachment();
        if (stencilAttachment)
        {
            ASSERT(stencilAttachment->type() == GL_RENDERBUFFER);
            GLuint resourceID = stencilAttachment->getResource()->getId();
            cap(CaptureFramebufferRenderbuffer(replayState, true, GL_FRAMEBUFFER,
                                               GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                               {resourceID}));
        }

        // TODO(jmadill): Draw buffer states. http://anglebug.com/3662
    }

    // Capture framebuffer bindings.
    gl::FramebufferID stateReadFramebuffer = apiState.getReadFramebuffer()->id();
    gl::FramebufferID stateDrawFramebuffer = apiState.getDrawFramebuffer()->id();
    if (stateDrawFramebuffer == stateReadFramebuffer)
    {
        if (currentDrawFramebuffer != stateDrawFramebuffer ||
            currentReadFramebuffer != stateReadFramebuffer)
        {
            cap(CaptureBindFramebuffer(replayState, true, GL_FRAMEBUFFER, stateDrawFramebuffer));
            currentDrawFramebuffer = currentReadFramebuffer = stateDrawFramebuffer;
        }
    }
    else
    {
        if (currentDrawFramebuffer != stateDrawFramebuffer)
        {
            cap(CaptureBindFramebuffer(replayState, true, GL_DRAW_FRAMEBUFFER,
                                       currentDrawFramebuffer));
            currentDrawFramebuffer = stateDrawFramebuffer;
        }

        if (currentReadFramebuffer != stateReadFramebuffer)
        {
            cap(CaptureBindFramebuffer(replayState, true, GL_READ_FRAMEBUFFER,
                                       replayState.getReadFramebuffer()->id()));
            currentReadFramebuffer = stateReadFramebuffer;
        }
    }

    // Capture Shaders and Programs.
    const gl::ShaderProgramManager &shadersAndPrograms =
        apiState.getShaderProgramManagerForCapture();
    const gl::ResourceMap<gl::Shader, gl::ShaderProgramID> &shaders =
        shadersAndPrograms.getShadersForCapture();
    const gl::ResourceMap<gl::Program, gl::ShaderProgramID> &programs =
        shadersAndPrograms.getProgramsForCapture();

    // Capture Program binary state. Use shader ID 1 as a temporary shader ID.
    gl::ShaderProgramID tempShaderID = {1};
    for (const auto &programIter : programs)
    {
        gl::ShaderProgramID id = {programIter.first};
        gl::Program *program   = programIter.second;

        // Get last compiled shader source.
        const auto &foundSources = cachedProgramSources.find(id);
        ASSERT(foundSources != cachedProgramSources.end());
        const ProgramSources &linkedSources = foundSources->second;

        // Unlinked programs don't have an executable. Thus they don't need to be linked.
        if (!program->isLinked())
        {
            continue;
        }

        cap(CaptureCreateProgram(replayState, true, id.value));

        // Compile with last linked sources.
        for (gl::ShaderType shaderType : program->getState().getLinkedShaderStages())
        {
            const std::string &sourceString = linkedSources[shaderType];
            const char *sourcePointer       = sourceString.c_str();

            // Compile and attach the temporary shader. Then free it immediately.
            cap(CaptureCreateShader(replayState, true, shaderType, tempShaderID.value));
            cap(CaptureShaderSource(replayState, true, tempShaderID, 1, &sourcePointer, nullptr));
            cap(CaptureCompileShader(replayState, true, tempShaderID));
            cap(CaptureAttachShader(replayState, true, id, tempShaderID));
            cap(CaptureDeleteShader(replayState, true, tempShaderID));
        }

        cap(CaptureLinkProgram(replayState, true, id));
    }

    // Handle shaders.
    for (const auto &shaderIter : shaders)
    {
        gl::ShaderProgramID id = {shaderIter.first};
        gl::Shader *shader     = shaderIter.second;
        cap(CaptureCreateShader(replayState, true, shader->getType(), id.value));

        std::string shaderSource  = shader->getSourceString();
        const char *sourcePointer = shaderSource.empty() ? nullptr : shaderSource.c_str();

        // This does not handle some more tricky situations like attaching shaders to a non-linked
        // program. Or attaching uncompiled shaders. Or attaching and then deleting a shader.
        // TODO(jmadill): Handle trickier program uses. http://anglebug.com/3662
        if (shader->isCompiled())
        {
            const auto &foundSources = cachedShaderSources.find(id);
            ASSERT(foundSources != cachedShaderSources.end());
            const std::string &capturedSource = foundSources->second;

            if (capturedSource != shaderSource)
            {
                ASSERT(!capturedSource.empty());
                sourcePointer = capturedSource.c_str();
            }

            cap(CaptureShaderSource(replayState, true, id, 1, &sourcePointer, nullptr));
            cap(CaptureCompileShader(replayState, true, id));
        }

        if (sourcePointer && (!shader->isCompiled() || sourcePointer != shaderSource.c_str()))
        {
            cap(CaptureShaderSource(replayState, true, id, 1, &sourcePointer, nullptr));
        }
    }

    // For now we assume the installed program executable is the same as the current program.
    // TODO(jmadill): Handle installed program executable. http://anglebug.com/3662
    if (apiState.getProgram())
    {
        cap(CaptureUseProgram(replayState, true, apiState.getProgram()->id()));
    }

    // TODO(http://anglebug.com/3662): ES 3.x objects.

    // Capture GL Context states.
    // TODO(http://anglebug.com/3662): Complete state capture.
    auto capCap = [cap, &replayState](GLenum capEnum, bool capValue) {
        if (capValue)
        {
            cap(CaptureEnable(replayState, true, capEnum));
        }
        else
        {
            cap(CaptureDisable(replayState, true, capEnum));
        }
    };

    // Rasterizer state. Missing ES 3.x features.
    // TODO(http://anglebug.com/3662): Complete state capture.
    const gl::RasterizerState &defaultRasterState = replayState.getRasterizerState();
    const gl::RasterizerState &currentRasterState = apiState.getRasterizerState();
    if (currentRasterState.cullFace != defaultRasterState.cullFace)
    {
        capCap(GL_CULL_FACE, currentRasterState.cullFace);
    }

    if (currentRasterState.cullMode != defaultRasterState.cullMode)
    {
        cap(CaptureCullFace(replayState, true, currentRasterState.cullMode));
    }

    if (currentRasterState.frontFace != defaultRasterState.frontFace)
    {
        cap(CaptureFrontFace(replayState, true, currentRasterState.frontFace));
    }

    // Depth/stencil state.
    const gl::DepthStencilState &defaultDSState = replayState.getDepthStencilState();
    const gl::DepthStencilState &currentDSState = apiState.getDepthStencilState();
    if (defaultDSState.depthFunc != currentDSState.depthFunc)
    {
        cap(CaptureDepthFunc(replayState, true, currentDSState.depthFunc));
    }

    if (defaultDSState.depthMask != currentDSState.depthMask)
    {
        cap(CaptureDepthMask(replayState, true, gl::ConvertToGLBoolean(currentDSState.depthMask)));
    }

    if (defaultDSState.depthTest != currentDSState.depthTest)
    {
        capCap(GL_DEPTH_TEST, currentDSState.depthTest);
    }

    if (defaultDSState.stencilTest != currentDSState.stencilTest)
    {
        capCap(GL_STENCIL_TEST, currentDSState.stencilTest);
    }

    if (defaultDSState.stencilFunc != currentDSState.stencilFunc ||
        defaultDSState.stencilMask != currentDSState.stencilMask || apiState.getStencilRef() != 0)
    {
        cap(CaptureStencilFuncSeparate(replayState, true, GL_FRONT, currentDSState.stencilFunc,
                                       apiState.getStencilRef(), currentDSState.stencilMask));
    }

    if (defaultDSState.stencilBackFunc != currentDSState.stencilBackFunc ||
        defaultDSState.stencilBackMask != currentDSState.stencilBackMask ||
        apiState.getStencilBackRef() != 0)
    {
        cap(CaptureStencilFuncSeparate(replayState, true, GL_BACK, currentDSState.stencilBackFunc,
                                       apiState.getStencilBackRef(),
                                       currentDSState.stencilBackMask));
    }

    if (defaultDSState.stencilFail != currentDSState.stencilFail ||
        defaultDSState.stencilPassDepthFail != currentDSState.stencilPassDepthFail ||
        defaultDSState.stencilPassDepthPass != currentDSState.stencilPassDepthPass)
    {
        cap(CaptureStencilOpSeparate(replayState, true, GL_FRONT, currentDSState.stencilFail,
                                     currentDSState.stencilPassDepthFail,
                                     currentDSState.stencilPassDepthPass));
    }

    if (defaultDSState.stencilBackFail != currentDSState.stencilBackFail ||
        defaultDSState.stencilBackPassDepthFail != currentDSState.stencilBackPassDepthFail ||
        defaultDSState.stencilBackPassDepthPass != currentDSState.stencilBackPassDepthPass)
    {
        cap(CaptureStencilOpSeparate(replayState, true, GL_BACK, currentDSState.stencilBackFail,
                                     currentDSState.stencilBackPassDepthFail,
                                     currentDSState.stencilBackPassDepthPass));
    }

    if (defaultDSState.stencilWritemask != currentDSState.stencilWritemask)
    {
        cap(CaptureStencilMaskSeparate(replayState, true, GL_FRONT,
                                       currentDSState.stencilWritemask));
    }

    if (defaultDSState.stencilBackWritemask != currentDSState.stencilBackWritemask)
    {
        cap(CaptureStencilMaskSeparate(replayState, true, GL_BACK,
                                       currentDSState.stencilBackWritemask));
    }

    // Blend state.
    const gl::BlendState &defaultBlendState = replayState.getBlendState();
    const gl::BlendState &currentBlendState = apiState.getBlendState();

    if (currentBlendState.blend != defaultBlendState.blend)
    {
        capCap(GL_BLEND, currentBlendState.blend);
    }

    if (currentBlendState.sourceBlendRGB != defaultBlendState.sourceBlendRGB ||
        currentBlendState.destBlendRGB != defaultBlendState.destBlendRGB ||
        currentBlendState.sourceBlendAlpha != defaultBlendState.sourceBlendAlpha ||
        currentBlendState.destBlendAlpha != defaultBlendState.destBlendAlpha)
    {
        cap(CaptureBlendFuncSeparate(
            replayState, true, currentBlendState.sourceBlendRGB, currentBlendState.destBlendRGB,
            currentBlendState.sourceBlendAlpha, currentBlendState.destBlendAlpha));
    }

    if (currentBlendState.blendEquationRGB != defaultBlendState.blendEquationRGB ||
        currentBlendState.blendEquationAlpha != defaultBlendState.blendEquationAlpha)
    {
        cap(CaptureBlendEquationSeparate(replayState, true, currentBlendState.blendEquationRGB,
                                         currentBlendState.blendEquationAlpha));
    }

    if (currentBlendState.colorMaskRed != defaultBlendState.colorMaskRed ||
        currentBlendState.colorMaskGreen != defaultBlendState.colorMaskGreen ||
        currentBlendState.colorMaskBlue != defaultBlendState.colorMaskBlue ||
        currentBlendState.colorMaskAlpha != defaultBlendState.colorMaskAlpha)
    {
        cap(CaptureColorMask(replayState, true,
                             gl::ConvertToGLBoolean(currentBlendState.colorMaskRed),
                             gl::ConvertToGLBoolean(currentBlendState.colorMaskGreen),
                             gl::ConvertToGLBoolean(currentBlendState.colorMaskBlue),
                             gl::ConvertToGLBoolean(currentBlendState.colorMaskAlpha)));
    }

    const gl::ColorF &currentBlendColor = apiState.getBlendColor();
    if (currentBlendColor != gl::ColorF())
    {
        cap(CaptureBlendColor(replayState, true, currentBlendColor.red, currentBlendColor.green,
                              currentBlendColor.blue, currentBlendColor.alpha));
    }

    // Pixel storage states.
    // TODO(jmadill): ES 3.x+ implementation. http://anglebug.com/3662
    if (currentPackState.alignment != apiState.getPackAlignment())
    {
        cap(CapturePixelStorei(replayState, true, GL_UNPACK_ALIGNMENT,
                               apiState.getPackAlignment()));
        currentPackState.alignment = apiState.getPackAlignment();
    }

    // Clear state. Missing ES 3.x features.
    // TODO(http://anglebug.com/3662): Complete state capture.
    const gl::ColorF &currentClearColor = apiState.getColorClearValue();
    if (currentClearColor != gl::ColorF())
    {
        cap(CaptureClearColor(replayState, true, currentClearColor.red, currentClearColor.green,
                              currentClearColor.blue, currentClearColor.alpha));
    }

    if (apiState.getDepthClearValue() != 1.0f)
    {
        cap(CaptureClearDepthf(replayState, true, apiState.getDepthClearValue()));
    }

    // Viewport / scissor / clipping planes.
    const gl::Rectangle &currentViewport = apiState.getViewport();
    if (currentViewport != gl::Rectangle())
    {
        cap(CaptureViewport(replayState, true, currentViewport.x, currentViewport.y,
                            currentViewport.width, currentViewport.height));
    }

    if (apiState.getNearPlane() != 0.0f || apiState.getFarPlane() != 1.0f)
    {
        cap(CaptureDepthRangef(replayState, true, apiState.getNearPlane(), apiState.getFarPlane()));
    }

    if (apiState.isScissorTestEnabled())
    {
        capCap(GL_SCISSOR_TEST, apiState.isScissorTestEnabled());
    }

    const gl::Rectangle &currentScissor = apiState.getScissor();
    if (currentScissor != gl::Rectangle())
    {
        cap(CaptureScissor(replayState, true, currentScissor.x, currentScissor.y,
                           currentScissor.width, currentScissor.height));
    }

    // Allow the replayState object to be destroyed conveniently.
    replayState.setBufferBinding(context, gl::BufferBinding::Array, nullptr);
}
}  // namespace

ParamCapture::ParamCapture() : type(ParamType::TGLenum), enumGroup(gl::GLenumGroup::DefaultGroup) {}

ParamCapture::ParamCapture(const char *nameIn, ParamType typeIn)
    : name(nameIn), type(typeIn), enumGroup(gl::GLenumGroup::DefaultGroup)
{}

ParamCapture::~ParamCapture() = default;

ParamCapture::ParamCapture(ParamCapture &&other)
    : type(ParamType::TGLenum), enumGroup(gl::GLenumGroup::DefaultGroup)
{
    *this = std::move(other);
}

ParamCapture &ParamCapture::operator=(ParamCapture &&other)
{
    std::swap(name, other.name);
    std::swap(type, other.type);
    std::swap(value, other.value);
    std::swap(enumGroup, other.enumGroup);
    std::swap(data, other.data);
    std::swap(arrayClientPointerIndex, other.arrayClientPointerIndex);
    std::swap(readBufferSizeBytes, other.readBufferSizeBytes);
    return *this;
}

ParamBuffer::ParamBuffer() {}

ParamBuffer::~ParamBuffer() = default;

ParamBuffer::ParamBuffer(ParamBuffer &&other)
{
    *this = std::move(other);
}

ParamBuffer &ParamBuffer::operator=(ParamBuffer &&other)
{
    std::swap(mParamCaptures, other.mParamCaptures);
    std::swap(mClientArrayDataParam, other.mClientArrayDataParam);
    std::swap(mReadBufferSize, other.mReadBufferSize);
    std::swap(mReturnValueCapture, other.mReturnValueCapture);
    return *this;
}

ParamCapture &ParamBuffer::getParam(const char *paramName, ParamType paramType, int index)
{
    ParamCapture &capture = mParamCaptures[index];
    ASSERT(capture.name == paramName);
    ASSERT(capture.type == paramType);
    return capture;
}

const ParamCapture &ParamBuffer::getParam(const char *paramName,
                                          ParamType paramType,
                                          int index) const
{
    return const_cast<ParamBuffer *>(this)->getParam(paramName, paramType, index);
}

ParamCapture &ParamBuffer::getParamFlexName(const char *paramName1,
                                            const char *paramName2,
                                            ParamType paramType,
                                            int index)
{
    ParamCapture &capture = mParamCaptures[index];
    ASSERT(capture.name == paramName1 || capture.name == paramName2);
    ASSERT(capture.type == paramType);
    return capture;
}

const ParamCapture &ParamBuffer::getParamFlexName(const char *paramName1,
                                                  const char *paramName2,
                                                  ParamType paramType,
                                                  int index) const
{
    return const_cast<ParamBuffer *>(this)->getParamFlexName(paramName1, paramName2, paramType,
                                                             index);
}

void ParamBuffer::addParam(ParamCapture &&param)
{
    if (param.arrayClientPointerIndex != -1)
    {
        ASSERT(mClientArrayDataParam == -1);
        mClientArrayDataParam = static_cast<int>(mParamCaptures.size());
    }

    mReadBufferSize = std::max(param.readBufferSizeBytes, mReadBufferSize);
    mParamCaptures.emplace_back(std::move(param));
}

void ParamBuffer::addReturnValue(ParamCapture &&returnValue)
{
    mReturnValueCapture = std::move(returnValue);
}

ParamCapture &ParamBuffer::getClientArrayPointerParameter()
{
    ASSERT(hasClientArrayData());
    return mParamCaptures[mClientArrayDataParam];
}

CallCapture::CallCapture(gl::EntryPoint entryPointIn, ParamBuffer &&paramsIn)
    : entryPoint(entryPointIn), params(std::move(paramsIn))
{}

CallCapture::CallCapture(const std::string &customFunctionNameIn, ParamBuffer &&paramsIn)
    : entryPoint(gl::EntryPoint::Invalid),
      customFunctionName(customFunctionNameIn),
      params(std::move(paramsIn))
{}

CallCapture::~CallCapture() = default;

CallCapture::CallCapture(CallCapture &&other)
{
    *this = std::move(other);
}

CallCapture &CallCapture::operator=(CallCapture &&other)
{
    std::swap(entryPoint, other.entryPoint);
    std::swap(customFunctionName, other.customFunctionName);
    std::swap(params, other.params);
    return *this;
}

const char *CallCapture::name() const
{
    if (entryPoint == gl::EntryPoint::Invalid)
    {
        ASSERT(!customFunctionName.empty());
        return customFunctionName.c_str();
    }

    return gl::GetEntryPointName(entryPoint);
}

ReplayContext::ReplayContext(size_t readBufferSizebytes,
                             const gl::AttribArray<size_t> &clientArraysSizebytes)
{
    mReadBuffer.resize(readBufferSizebytes);

    for (uint32_t i = 0; i < clientArraysSizebytes.size(); i++)
    {
        mClientArraysBuffer[i].resize(clientArraysSizebytes[i]);
    }
}
ReplayContext::~ReplayContext() {}

FrameCapture::FrameCapture()
    : mEnabled(true),
      mClientVertexArrayMap{},
      mFrameIndex(0),
      mFrameStart(0),
      mFrameEnd(10),
      mClientArraySizes{},
      mReadBufferSize(0),
      mHasResourceType{}
{
    reset();

#if defined(ANGLE_PLATFORM_ANDROID)
    PrimeAndroidEnvironmentVariables();
#endif

    std::string enabledFromEnv = angle::GetEnvironmentVar(kEnabledVarName);
    if (enabledFromEnv == "0")
    {
        mEnabled = false;
    }

    std::string pathFromEnv = angle::GetEnvironmentVar(kOutDirectoryVarName);
    if (pathFromEnv.empty())
    {
        mOutDirectory = GetDefaultOutDirectory();
    }
    else
    {
        mOutDirectory = pathFromEnv;
    }

    // Ensure the capture path ends with a slash.
    if (mOutDirectory.back() != '\\' && mOutDirectory.back() != '/')
    {
        mOutDirectory += '/';
    }

    std::string startFromEnv = angle::GetEnvironmentVar(kFrameStartVarName);
    if (!startFromEnv.empty())
    {
        mFrameStart = atoi(startFromEnv.c_str());
    }

    std::string endFromEnv = angle::GetEnvironmentVar(kFrameEndVarName);
    if (!endFromEnv.empty())
    {
        mFrameEnd = atoi(endFromEnv.c_str());
    }

    std::string labelFromEnv = angle::GetEnvironmentVar(kCaptureLabel);
    if (!labelFromEnv.empty())
    {
        // Optional label to provide unique file names and namespaces
        mCaptureLabel = labelFromEnv;
    }
}

FrameCapture::~FrameCapture() = default;

void FrameCapture::maybeCaptureClientData(const gl::Context *context, const CallCapture &call)
{
    switch (call.entryPoint)
    {
        case gl::EntryPoint::VertexAttribPointer:
        {
            // Get array location
            GLuint index = call.params.getParam("index", ParamType::TGLuint, 0).value.GLuintVal;

            if (call.params.hasClientArrayData())
            {
                mClientVertexArrayMap[index] = static_cast<int>(mFrameCalls.size());
            }
            else
            {
                mClientVertexArrayMap[index] = -1;
            }
            break;
        }

        case gl::EntryPoint::DrawArrays:
        {
            if (context->getStateCache().hasAnyActiveClientAttrib())
            {
                // Get counts from paramBuffer.
                GLint firstVertex =
                    call.params.getParam("first", ParamType::TGLint, 1).value.GLintVal;
                GLsizei drawCount =
                    call.params.getParam("count", ParamType::TGLsizei, 2).value.GLsizeiVal;
                captureClientArraySnapshot(context, firstVertex + drawCount, 1);
            }
            break;
        }

        case gl::EntryPoint::DrawElements:
        {
            if (context->getStateCache().hasAnyActiveClientAttrib())
            {
                GLsizei count =
                    call.params.getParam("count", ParamType::TGLsizei, 1).value.GLsizeiVal;
                gl::DrawElementsType drawElementsType =
                    call.params.getParam("typePacked", ParamType::TDrawElementsType, 2)
                        .value.DrawElementsTypeVal;
                const void *indices =
                    call.params.getParam("indices", ParamType::TvoidConstPointer, 3)
                        .value.voidConstPointerVal;

                gl::IndexRange indexRange;

                bool restart = context->getState().isPrimitiveRestartEnabled();

                gl::Buffer *elementArrayBuffer =
                    context->getState().getVertexArray()->getElementArrayBuffer();
                if (elementArrayBuffer)
                {
                    size_t offset = reinterpret_cast<size_t>(indices);
                    (void)elementArrayBuffer->getIndexRange(context, drawElementsType, offset,
                                                            count, restart, &indexRange);
                }
                else
                {
                    indexRange = gl::ComputeIndexRange(drawElementsType, indices, count, restart);
                }

                // index starts from 0
                captureClientArraySnapshot(context, indexRange.end + 1, 1);
            }
            break;
        }

        case gl::EntryPoint::CompileShader:
        {
            // Refresh the cached shader sources.
            gl::ShaderProgramID shaderID =
                call.params.getParam("shaderPacked", ParamType::TShaderProgramID, 0)
                    .value.ShaderProgramIDVal;
            const gl::Shader *shader       = context->getShader(shaderID);
            mCachedShaderSources[shaderID] = shader->getSourceString();
            break;
        }

        case gl::EntryPoint::LinkProgram:
        {
            // Refresh the cached program sources.
            gl::ShaderProgramID programID =
                call.params.getParam("programPacked", ParamType::TShaderProgramID, 0)
                    .value.ShaderProgramIDVal;
            const gl::Program *program       = context->getProgramResolveLink(programID);
            mCachedProgramSources[programID] = GetAttachedProgramSources(program);
            break;
        }

        default:
            break;
    }
}

void FrameCapture::captureCall(const gl::Context *context, CallCapture &&call)
{
    // Process client data snapshots.
    maybeCaptureClientData(context, call);

    mReadBufferSize = std::max(mReadBufferSize, call.params.getReadBufferSize());
    mFrameCalls.emplace_back(std::move(call));

    // Process resource ID updates.
    MaybeCaptureUpdateResourceIDs(&mFrameCalls);
}

void FrameCapture::captureClientArraySnapshot(const gl::Context *context,
                                              size_t vertexCount,
                                              size_t instanceCount)
{
    const gl::VertexArray *vao = context->getState().getVertexArray();

    // Capture client array data.
    for (size_t attribIndex : context->getStateCache().getActiveClientAttribsMask())
    {
        const gl::VertexAttribute &attrib = vao->getVertexAttribute(attribIndex);
        const gl::VertexBinding &binding  = vao->getVertexBinding(attrib.bindingIndex);

        int callIndex = mClientVertexArrayMap[attribIndex];

        if (callIndex != -1)
        {
            size_t count = vertexCount;

            if (binding.getDivisor() > 0)
            {
                count = rx::UnsignedCeilDivide(static_cast<uint32_t>(instanceCount),
                                               binding.getDivisor());
            }

            // The last capture element doesn't take up the full stride.
            size_t bytesToCapture = (count - 1) * binding.getStride() + attrib.format->pixelBytes;

            CallCapture &call   = mFrameCalls[callIndex];
            ParamCapture &param = call.params.getClientArrayPointerParameter();
            ASSERT(param.type == ParamType::TvoidConstPointer);

            ParamBuffer updateParamBuffer;
            updateParamBuffer.addValueParam<GLint>("arrayIndex", ParamType::TGLint,
                                                   static_cast<uint32_t>(attribIndex));

            ParamCapture updateMemory("pointer", ParamType::TvoidConstPointer);
            CaptureMemory(param.value.voidConstPointerVal, bytesToCapture, &updateMemory);
            updateParamBuffer.addParam(std::move(updateMemory));

            updateParamBuffer.addValueParam<GLuint64>("size", ParamType::TGLuint64, bytesToCapture);

            mFrameCalls.emplace_back("UpdateClientArrayPointer", std::move(updateParamBuffer));

            mClientArraySizes[attribIndex] =
                std::max(mClientArraySizes[attribIndex], bytesToCapture);
        }
    }
}

void FrameCapture::onEndFrame(const gl::Context *context)
{
    // Note that we currently capture before the start frame to collect shader and program sources.
    if (!mFrameCalls.empty() && mFrameIndex >= mFrameStart)
    {
        WriteCppReplay(mOutDirectory, context->id(), mCaptureLabel, mFrameIndex, mFrameCalls,
                       mSetupCalls);

        // Save the index files after the last frame.
        if (mFrameIndex == mFrameEnd)
        {
            WriteCppReplayIndexFiles(mOutDirectory, context->id(), mCaptureLabel, mFrameStart,
                                     mFrameEnd, mReadBufferSize, mClientArraySizes,
                                     mHasResourceType);
        }
    }

    // Count resource IDs. This is also done on every frame. It could probably be done by checking
    // the GL state instead of the calls.
    for (const CallCapture &call : mFrameCalls)
    {
        for (const ParamCapture &param : call.params.getParamCaptures())
        {
            ResourceIDType idType = GetResourceIDTypeFromParamType(param.type);
            if (idType != ResourceIDType::InvalidEnum)
            {
                mHasResourceType.set(idType);
            }
        }
    }

    reset();
    mFrameIndex++;

    if (enabled() && mFrameIndex == mFrameStart)
    {
        mSetupCalls.clear();
        CaptureMidExecutionSetup(context, &mSetupCalls, mCachedShaderSources,
                                 mCachedProgramSources);
    }
}

DataCounters::DataCounters() = default;

DataCounters::~DataCounters() = default;

int DataCounters::getAndIncrement(gl::EntryPoint entryPoint, const std::string &paramName)
{
    Counter counterKey = {entryPoint, paramName};
    return mData[counterKey]++;
}

bool FrameCapture::enabled() const
{
    // Currently we will always do a capture up until the last frame. In the future we could improve
    // mid execution capture by only capturing between the start and end frames. The only necessary
    // reason we need to capture before the start is for attached program and shader sources.
    return mEnabled && mFrameIndex <= mFrameEnd;
}

void FrameCapture::replay(gl::Context *context)
{
    ReplayContext replayContext(mReadBufferSize, mClientArraySizes);
    for (const CallCapture &call : mFrameCalls)
    {
        INFO() << "frame index: " << mFrameIndex << " " << call.name();

        if (call.entryPoint == gl::EntryPoint::Invalid)
        {
            if (call.customFunctionName == "UpdateClientArrayPointer")
            {
                GLint arrayIndex =
                    call.params.getParam("arrayIndex", ParamType::TGLint, 0).value.GLintVal;
                ASSERT(arrayIndex < gl::MAX_VERTEX_ATTRIBS);

                const ParamCapture &pointerParam =
                    call.params.getParam("pointer", ParamType::TvoidConstPointer, 1);
                ASSERT(pointerParam.data.size() == 1);
                const void *pointer = pointerParam.data[0].data();

                size_t size = static_cast<size_t>(
                    call.params.getParam("size", ParamType::TGLuint64, 2).value.GLuint64Val);

                std::vector<uint8_t> &curClientArrayBuffer =
                    replayContext.getClientArraysBuffer()[arrayIndex];
                ASSERT(curClientArrayBuffer.size() >= size);
                memcpy(curClientArrayBuffer.data(), pointer, size);
            }
            continue;
        }

        ReplayCall(context, &replayContext, call);
    }
}

void FrameCapture::reset()
{
    mFrameCalls.clear();
    mSetupCalls.clear();
    mClientVertexArrayMap.fill(-1);

    // Do not reset replay-specific settings like the maximum read buffer size, client array sizes,
    // or the 'has seen' type map. We could refine this into per-frame and per-capture maximums if
    // necessary.
}

std::ostream &operator<<(std::ostream &os, const ParamCapture &capture)
{
    WriteParamTypeToStream(os, capture.type, capture.value);
    return os;
}

void CaptureMemory(const void *source, size_t size, ParamCapture *paramCapture)
{
    std::vector<uint8_t> data(size);
    memcpy(data.data(), source, size);
    paramCapture->data.emplace_back(std::move(data));
}

void CaptureString(const GLchar *str, ParamCapture *paramCapture)
{
    // include the '\0' suffix
    CaptureMemory(str, strlen(str) + 1, paramCapture);
}

gl::Program *GetLinkedProgramForCapture(const gl::State &glState, gl::ShaderProgramID handle)
{
    gl::Program *program = glState.getShaderProgramManagerForCapture().getProgram(handle);
    ASSERT(program->isLinked());
    return program;
}

void CaptureGetParameter(const gl::State &glState,
                         GLenum pname,
                         size_t typeSize,
                         ParamCapture *paramCapture)
{
    GLenum nativeType;
    unsigned int numParams;
    if (!gl::GetQueryParameterInfo(glState, pname, &nativeType, &numParams))
    {
        numParams = 1;
    }

    paramCapture->readBufferSizeBytes = typeSize * numParams;
}

void CaptureGenHandlesImpl(GLsizei n, GLuint *handles, ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLuint) * n;
    CaptureMemory(handles, paramCapture->readBufferSizeBytes, paramCapture);
}

template <>
void WriteParamValueToStream<ParamType::TGLboolean>(std::ostream &os, GLboolean value)
{
    switch (value)
    {
        case GL_TRUE:
            os << "GL_TRUE";
            break;
        case GL_FALSE:
            os << "GL_FALSE";
            break;
        default:
            os << "GL_INVALID_ENUM";
    }
}

template <>
void WriteParamValueToStream<ParamType::TvoidConstPointer>(std::ostream &os, const void *value)
{
    if (value == 0)
    {
        os << "nullptr";
    }
    else
    {
        os << "reinterpret_cast<const void *>("
           << static_cast<int>(reinterpret_cast<uintptr_t>(value)) << ")";
    }
}

template <>
void WriteParamValueToStream<ParamType::TGLDEBUGPROCKHR>(std::ostream &os, GLDEBUGPROCKHR value)
{}

template <>
void WriteParamValueToStream<ParamType::TGLDEBUGPROC>(std::ostream &os, GLDEBUGPROC value)
{}

template <>
void WriteParamValueToStream<ParamType::TBufferID>(std::ostream &os, gl::BufferID value)
{
    os << "gBufferMap[" << value.value << "]";
}

template <>
void WriteParamValueToStream<ParamType::TFenceNVID>(std::ostream &os, gl::FenceNVID value)
{
    os << "gFenceMap[" << value.value << "]";
}

template <>
void WriteParamValueToStream<ParamType::TFramebufferID>(std::ostream &os, gl::FramebufferID value)
{
    os << "gFramebufferMap[" << value.value << "]";
}

template <>
void WriteParamValueToStream<ParamType::TMemoryObjectID>(std::ostream &os, gl::MemoryObjectID value)
{
    os << "gMemoryObjectMap[" << value.value << "]";
}

template <>
void WriteParamValueToStream<ParamType::TPathID>(std::ostream &os, gl::PathID value)
{
    os << "gPathMap[" << value.value << "]";
}

template <>
void WriteParamValueToStream<ParamType::TProgramPipelineID>(std::ostream &os,
                                                            gl::ProgramPipelineID value)
{
    os << "gProgramPipelineMap[" << value.value << "]";
}

template <>
void WriteParamValueToStream<ParamType::TQueryID>(std::ostream &os, gl::QueryID value)
{
    os << "gQueryMap[" << value.value << "]";
}

template <>
void WriteParamValueToStream<ParamType::TRenderbufferID>(std::ostream &os, gl::RenderbufferID value)
{
    os << "gRenderbufferMap[" << value.value << "]";
}

template <>
void WriteParamValueToStream<ParamType::TSamplerID>(std::ostream &os, gl::SamplerID value)
{
    os << "gSamplerMap[" << value.value << "]";
}

template <>
void WriteParamValueToStream<ParamType::TSemaphoreID>(std::ostream &os, gl::SemaphoreID value)
{
    os << "gSempahoreMap[" << value.value << "]";
}

template <>
void WriteParamValueToStream<ParamType::TShaderProgramID>(std::ostream &os,
                                                          gl::ShaderProgramID value)
{
    os << "gShaderProgramMap[" << value.value << "]";
}

template <>
void WriteParamValueToStream<ParamType::TTextureID>(std::ostream &os, gl::TextureID value)
{
    os << "gTextureMap[" << value.value << "]";
}

template <>
void WriteParamValueToStream<ParamType::TTransformFeedbackID>(std::ostream &os,
                                                              gl::TransformFeedbackID value)
{
    os << "gTransformFeedbackMap[" << value.value << "]";
}

template <>
void WriteParamValueToStream<ParamType::TVertexArrayID>(std::ostream &os, gl::VertexArrayID value)
{
    os << "gVertexArrayMap[" << value.value << "]";
}
}  // namespace angle
