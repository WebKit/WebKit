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
#include "libANGLE/Query.h"
#include "libANGLE/ResourceMap.h"
#include "libANGLE/Shader.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/capture_gles_2_0_autogen.h"
#include "libANGLE/capture_gles_3_0_autogen.h"
#include "libANGLE/gl_enum_utils.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/queryutils.h"

#define USE_SYSTEM_ZLIB
#include "compression_utils_portable.h"

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
constexpr char kCompression[]         = "ANGLE_CAPTURE_COMPRESSION";

#if defined(ANGLE_PLATFORM_ANDROID)

constexpr char kAndroidCaptureEnabled[] = "debug.angle.capture.enabled";
constexpr char kAndroidOutDir[]         = "debug.angle.capture.out_dir";
constexpr char kAndroidFrameStart[]     = "debug.angle.capture.frame_start";
constexpr char kAndroidFrameEnd[]       = "debug.angle.capture.frame_end";
constexpr char kAndroidCaptureLabel[]   = "debug.angle.capture.label";
constexpr char kAndroidCompression[]    = "debug.angle.capture.compression";

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
        INFO() << "Frame capture read " << captureLabel << " from " << kAndroidCaptureLabel;
        setenv(kCaptureLabel, captureLabel.c_str(), 1);
    }

    std::string compression = AndroidGetEnvFromProp(kAndroidCompression);
    if (!compression.empty())
    {
        INFO() << "Frame capture read " << compression << " from " << kAndroidCompression;
        setenv(kCompression, compression.c_str(), 1);
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
    FmtCapturePrefix(gl::ContextID contextIdIn, const std::string &captureLabelIn)
        : contextId(contextIdIn), captureLabel(captureLabelIn)
    {}
    gl::ContextID contextId;
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
    os << "_capture_context" << static_cast<int>(fmt.contextId);
    return os;
}

struct FmtReplayFunction
{
    FmtReplayFunction(gl::ContextID contextIdIn, uint32_t frameIndexIn)
        : contextId(contextIdIn), frameIndex(frameIndexIn)
    {}
    gl::ContextID contextId;
    uint32_t frameIndex;
};

std::ostream &operator<<(std::ostream &os, const FmtReplayFunction &fmt)
{
    os << "ReplayContext" << static_cast<int>(fmt.contextId) << "Frame" << fmt.frameIndex << "()";
    return os;
}

std::string GetCaptureFileName(gl::ContextID contextId,
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
                               gl::ContextID contextId,
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

template <>
void WriteInlineData<GLchar>(const std::vector<uint8_t> &vec, std::ostream &out)
{
    const GLchar *data = reinterpret_cast<const GLchar *>(vec.data());
    size_t count       = vec.size() / sizeof(GLchar);

    if (data == nullptr || data[0] == '\0')
    {
        return;
    }

    out << "\"";

    for (size_t dataIndex = 0; dataIndex < count; ++dataIndex)
    {
        if (data[dataIndex] == '\0')
            break;

        out << static_cast<GLchar>(data[dataIndex]);
    }

    out << "\"";
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
            case ParamType::TGLcharPointer:
                WriteInlineData<GLchar>(data, header);
                break;
            default:
                INFO() << "Unhandled ParamType: " << angle::ParamTypeToString(overrideType)
                       << " in " << call.name();
                UNIMPLEMENTED();
                break;
        }

        header << " };\n";

        WriteParamStaticVarName(call, param, counter, out);
    }
}

uintptr_t SyncIndexValue(GLsync sync)
{
    return reinterpret_cast<uintptr_t>(sync);
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

    if (call.entryPoint == gl::EntryPoint::FenceSync)
    {
        GLsync sync = call.params.getReturnValue().value.GLsyncVal;
        callOut << "gSyncMap[" << SyncIndexValue(sync) << "] = ";
    }

    if (call.entryPoint == gl::EntryPoint::MapBufferRange ||
        call.entryPoint == gl::EntryPoint::MapBufferRangeEXT)
    {
        GLbitfield access =
            call.params.getParam("access", ParamType::TGLbitfield, 3).value.GLbitfieldVal;

        if (access & GL_MAP_WRITE_BIT)
        {
            // Track the returned pointer so we update its data when unmapped
            gl::BufferID bufferID = call.params.getMappedBufferID();
            callOut << "gMappedBufferData[";
            WriteParamValueReplay<ParamType::TBufferID>(callOut, call, bufferID);
            callOut << "] = ";
        }
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
            else if (param.type == ParamType::TGLsync)
            {
                callOut << "gSyncMap[" << SyncIndexValue(param.value.GLsyncVal) << "]";
            }
            else if (param.type == ParamType::TGLuint64 && param.name == "timeout")
            {
                if (param.value.GLuint64Val == GL_TIMEOUT_IGNORED)
                {
                    callOut << "GL_TIMEOUT_IGNORED";
                }
                else
                {
                    WriteParamCaptureReplay(callOut, call, param);
                }
            }
            else
            {
                WriteParamCaptureReplay(callOut, call, param);
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
  public:
    // We always use ios::binary to avoid inconsistent line endings when captured on Linux vs Win.
    SaveFileHelper(const std::string &filePathIn)
        : mOfs(filePathIn, std::ios::binary | std::ios::out), mFilePath(filePathIn)
    {
        if (!mOfs.is_open())
        {
            FATAL() << "Could not open " << filePathIn;
        }
    }

    ~SaveFileHelper() { printf("Saved '%s'.\n", mFilePath.c_str()); }

    template <typename T>
    SaveFileHelper &operator<<(const T &value)
    {
        mOfs << value;
        if (mOfs.bad())
        {
            FATAL() << "Error writing to " << mFilePath;
        }
        return *this;
    }

    void write(const uint8_t *data, size_t size)
    {
        mOfs.write(reinterpret_cast<const char *>(data), size);
    }

  private:
    std::ofstream mOfs;
    std::string mFilePath;
};

std::string GetBinaryDataFilePath(bool compression,
                                  gl::ContextID contextId,
                                  const std::string &captureLabel)
{
    std::stringstream fnameStream;
    fnameStream << FmtCapturePrefix(contextId, captureLabel) << ".angledata";
    if (compression)
    {
        fnameStream << ".gz";
    }
    return fnameStream.str();
}

void SaveBinaryData(bool compression,
                    const std::string &outDir,
                    gl::ContextID contextId,
                    const std::string &captureLabel,
                    const std::vector<uint8_t> &binaryData)
{
    std::string binaryDataFileName = GetBinaryDataFilePath(compression, contextId, captureLabel);
    std::string dataFilepath       = outDir + binaryDataFileName;

    SaveFileHelper saveData(dataFilepath);

    if (compression)
    {
        // Save compressed data.
        uLong uncompressedSize       = static_cast<uLong>(binaryData.size());
        uLong expectedCompressedSize = zlib_internal::GzipExpectedCompressedSize(uncompressedSize);

        std::vector<uint8_t> compressedData(expectedCompressedSize, 0);

        uLong compressedSize = expectedCompressedSize;
        int zResult = zlib_internal::GzipCompressHelper(compressedData.data(), &compressedSize,
                                                        binaryData.data(), uncompressedSize,
                                                        nullptr, nullptr);

        if (zResult != Z_OK)
        {
            FATAL() << "Error compressing binary data: " << zResult;
        }

        saveData.write(compressedData.data(), compressedSize);
    }
    else
    {
        saveData.write(binaryData.data(), binaryData.size());
    }
}

void WriteLoadBinaryDataCall(bool compression,
                             std::ostream &out,
                             gl::ContextID contextId,
                             const std::string &captureLabel)
{
    std::string binaryDataFileName = GetBinaryDataFilePath(compression, contextId, captureLabel);
    out << "    LoadBinaryData(\"" << binaryDataFileName << "\");\n";
}

void MaybeResetResources(std::stringstream &out,
                         ResourceIDType resourceIDType,
                         DataCounters *counters,
                         std::stringstream &header,
                         ResourceTracker *resourceTracker,
                         std::vector<uint8_t> *binaryData)
{
    switch (resourceIDType)
    {
        case ResourceIDType::Buffer:
        {
            BufferSet &newBuffers           = resourceTracker->getNewBuffers();
            BufferCalls &bufferRegenCalls   = resourceTracker->getBufferRegenCalls();
            BufferCalls &bufferRestoreCalls = resourceTracker->getBufferRestoreCalls();

            // If we have any new buffers generated and not deleted during the run, delete them now
            if (!newBuffers.empty())
            {
                out << "    const GLuint deleteBuffers[] = {";
                BufferSet::iterator bufferIter = newBuffers.begin();
                for (size_t i = 0; bufferIter != newBuffers.end(); ++i, ++bufferIter)
                {
                    if (i > 0)
                    {
                        out << ", ";
                    }
                    if ((i % 4) == 0)
                    {
                        out << "\n        ";
                    }
                    out << "gBufferMap[" << (*bufferIter).value << "]";
                }
                out << "};\n";
                out << "    glDeleteBuffers(" << newBuffers.size() << ", deleteBuffers);\n";
            }

            // If any of our starting buffers were deleted during the run, recreate them
            BufferSet &buffersToRegen = resourceTracker->getBuffersToRegen();
            for (const gl::BufferID id : buffersToRegen)
            {
                // Emit their regen calls
                for (CallCapture &call : bufferRegenCalls[id])
                {
                    out << "    ";
                    WriteCppReplayForCall(call, counters, out, header, binaryData);
                    out << ";\n";
                }
            }

            // If any of our starting buffers were modified during the run, restore their contents
            BufferSet &buffersToRestore = resourceTracker->getBuffersToRestore();
            for (const gl::BufferID id : buffersToRestore)
            {
                // Emit their restore calls
                for (CallCapture &call : bufferRestoreCalls[id])
                {
                    out << "    ";
                    WriteCppReplayForCall(call, counters, out, header, binaryData);
                    out << ";\n";
                }
            }
            break;
        }
        default:
            // TODO (http://anglebug.com/4599): Reset more than just buffers
            break;
    }
}

void WriteCppReplay(bool compression,
                    const std::string &outDir,
                    gl::ContextID contextId,
                    const std::string &captureLabel,
                    uint32_t frameIndex,
                    uint32_t frameEnd,
                    const std::vector<CallCapture> &frameCalls,
                    const std::vector<CallCapture> &setupCalls,
                    ResourceTracker *resourceTracker,
                    std::vector<uint8_t> *binaryData)
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
        out << "void SetupContext" << Str(static_cast<int>(contextId)) << "Replay()\n";
        out << "{\n";

        std::stringstream setupCallStream;

        WriteLoadBinaryDataCall(compression, setupCallStream, contextId, captureLabel);

        for (const CallCapture &call : setupCalls)
        {
            setupCallStream << "    ";
            WriteCppReplayForCall(call, &counters, setupCallStream, header, binaryData);
            setupCallStream << ";\n";
        }

        out << setupCallStream.str();

        out << "}\n";
        out << "\n";
    }

    if (frameIndex == frameEnd)
    {
        // Emit code to reset back to starting state
        out << "void ResetContext" << Str(static_cast<int>(contextId)) << "Replay()\n";
        out << "{\n";

        std::stringstream restoreCallStream;

        // For now, we only reset buffer states
        // TODO (http://anglebug.com/4599): Reset more state on frame loop
        for (ResourceIDType resourceType : AllEnums<ResourceIDType>())
        {
            MaybeResetResources(restoreCallStream, resourceType, &counters, header, resourceTracker,
                                binaryData);
        }

        out << restoreCallStream.str();

        out << "}\n";
        out << "\n";
    }

    out << "void " << FmtReplayFunction(contextId, frameIndex) << "\n";
    out << "{\n";

    std::stringstream callStream;

    for (const CallCapture &call : frameCalls)
    {
        callStream << "    ";
        WriteCppReplayForCall(call, &counters, callStream, header, binaryData);
        callStream << ";\n";
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

void WriteCppReplayIndexFiles(bool compression,
                              const std::string &outDir,
                              gl::ContextID contextId,
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
    header << "#include <vector>\n";
    header << "#include <unordered_map>\n";
    header << "\n";

    if (!captureLabel.empty())
    {
        header << "namespace " << captureLabel << "\n";
        header << "{\n";
    }
    header << "// Replay functions\n";
    header << "\n";
    header << "// Maps from <captured Program ID, captured location> to run-time location.\n";
    header
        << "using LocationsMap = std::unordered_map<GLuint, std::unordered_map<GLint, GLint>>;\n";
    header << "extern LocationsMap gUniformLocations;\n";
    header << "extern GLuint gCurrentProgram;\n";
    header << "void UpdateUniformLocation(GLuint program, const char *name, GLint location);\n";
    header << "void DeleteUniformLocations(GLuint program);\n";
    header << "void UpdateCurrentProgram(GLuint program);\n";
    header << "\n";
    header << "// Maps from captured Resource ID to run-time Resource ID.\n";
    header << "using ResourceMap = std::unordered_map<GLuint, GLuint>;\n";
    header << "\n";
    header << "\n";
    header << "constexpr uint32_t kReplayFrameStart = " << frameStart << ";\n";
    header << "constexpr uint32_t kReplayFrameEnd = " << frameEnd << ";\n";
    header << "\n";
    header << "void SetupContext" << static_cast<int>(contextId) << "Replay();\n";
    header << "void ReplayContext" << static_cast<int>(contextId)
           << "Frame(uint32_t frameIndex);\n";
    header << "void ResetContext" << static_cast<int>(contextId) << "Replay();\n";
    header << "\n";
    header << "using FramebufferChangeCallback = void(*)(void *userData, GLenum target, GLuint "
              "framebuffer);\n";
    header << "void SetFramebufferChangeCallback(void *userData, FramebufferChangeCallback "
              "callback);\n";
    header << "void OnFramebufferChange(GLenum target, GLuint framebuffer);\n";
    header << "\n";
    for (uint32_t frameIndex = frameStart; frameIndex < frameEnd; ++frameIndex)
    {
        header << "void " << FmtReplayFunction(contextId, frameIndex) << ";\n";
    }
    header << "\n";
    header << "constexpr bool kIsBinaryDataCompressed = " << (compression ? "true" : "false")
           << ";\n";
    header << "\n";
    header << "using DecompressCallback = uint8_t *(*)(const std::vector<uint8_t> &);\n";
    header << "void SetBinaryDataDecompressCallback(DecompressCallback callback);\n";
    header << "void SetBinaryDataDir(const char *dataDir);\n";
    header << "void LoadBinaryData(const char *fileName);\n";
    header << "\n";
    header << "// Global state\n";
    header << "\n";
    header << "extern uint8_t *gBinaryData;\n";

    source << "#include \"" << FmtCapturePrefix(contextId, captureLabel) << ".h\"\n";
    source << "\n";

    if (!captureLabel.empty())
    {
        source << "namespace " << captureLabel << "\n";
        source << "{\n";
    }

    source << "namespace\n";
    source << "{\n";
    source << "void UpdateResourceMap(ResourceMap *resourceMap, GLuint id, GLsizei "
              "readBufferOffset)\n";
    source << "{\n";
    source << "    GLuint returnedID;\n";
    std::string captureNamespace = !captureLabel.empty() ? captureLabel + "::" : "";
    source << "    memcpy(&returnedID, &" << captureNamespace
           << "gReadBuffer[readBufferOffset], sizeof(GLuint));\n";
    source << "    (*resourceMap)[id] = returnedID;\n";
    source << "}\n";
    source << "\n";
    source << "DecompressCallback gDecompressCallback;\n";
    source << "const char *gBinaryDataDir = \".\";\n";
    source << "FramebufferChangeCallback gFramebufferChangeCallback;\n";
    source << "void *gFramebufferChangeCallbackUserData;\n";
    source << "}  // namespace\n";
    source << "\n";
    source << "LocationsMap gUniformLocations;\n";
    source << "GLuint gCurrentProgram = 0;\n";
    source << "\n";
    source << "void UpdateUniformLocation(GLuint program, const char *name, GLint location)\n";
    source << "{\n";
    source << "    gUniformLocations[program][location] = glGetUniformLocation(program, name);\n";
    source << "}\n";
    source << "void DeleteUniformLocations(GLuint program)\n";
    source << "{\n";
    source << "    gUniformLocations.erase(program);\n";
    source << "}\n";
    source << "void UpdateCurrentProgram(GLuint program)\n";
    source << "{\n";
    source << "    gCurrentProgram = program;\n";
    source << "}\n";
    source << "\n";

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
        // TODO: Only emit resources needed by the frames (anglebug.com/4223)
        const char *name = GetResourceIDTypeName(resourceType);
        header << "extern ResourceMap g" << name << "Map;\n";
        source << "ResourceMap g" << name << "Map;\n";
    }

    header << "using SyncResourceMap = std::unordered_map<uintptr_t, GLsync>;\n";
    header << "extern SyncResourceMap gSyncMap;\n";
    source << "SyncResourceMap gSyncMap;\n";

    header << "\n";

    source << "\n";
    source << "void SetFramebufferChangeCallback(void *userData, FramebufferChangeCallback "
              "callback)\n";
    source << "{\n";
    source << "    gFramebufferChangeCallbackUserData = userData;\n";
    source << "    gFramebufferChangeCallback = callback;\n";
    source << "}\n";
    source << "\n";
    source << "void OnFramebufferChange(GLenum target, GLuint framebuffer)\n";
    source << "{\n";
    source << "    if (gFramebufferChangeCallback)\n";
    source << "        gFramebufferChangeCallback(gFramebufferChangeCallbackUserData, target, "
              "framebuffer);\n";
    source << "}\n";

    source << "\n";
    source << "void ReplayContext" << static_cast<int>(contextId) << "Frame(uint32_t frameIndex)\n";
    source << "{\n";
    source << "    switch (frameIndex)\n";
    source << "    {\n";
    for (uint32_t frameIndex = frameStart; frameIndex < frameEnd; ++frameIndex)
    {
        source << "        case " << frameIndex << ":\n";
        source << "            ReplayContext" << static_cast<int>(contextId) << "Frame"
               << frameIndex << "();\n";
        source << "            break;\n";
    }
    source << "        default:\n";
    source << "            break;\n";
    source << "    }\n";
    source << "}\n";
    source << "\n";
    source << "void SetBinaryDataDecompressCallback(DecompressCallback callback)\n";
    source << "{\n";
    source << "    gDecompressCallback = callback;\n";
    source << "}\n";
    source << "\n";
    source << "void SetBinaryDataDir(const char *dataDir)\n";
    source << "{\n";
    source << "    gBinaryDataDir = dataDir;\n";
    source << "}\n";
    source << "\n";
    source << "void LoadBinaryData(const char *fileName)\n";
    source << "{\n";
    source << "    if (gBinaryData != nullptr)\n";
    source << "    {\n";
    source << "        delete [] gBinaryData;\n";
    source << "    }\n";
    source << "    char pathBuffer[1000] = {};\n";
    source << "    sprintf(pathBuffer, \"%s/%s\", gBinaryDataDir, fileName);\n";
    source << "    FILE *fp = fopen(pathBuffer, \"rb\");\n";
    source << "    if (fp == 0)\n";
    source << "    {\n";
    source << "        fprintf(stderr, \"Error loading binary data file: %s\\n\", fileName);\n";
    source << "        exit(1);\n";
    source << "    }\n";
    source << "    fseek(fp, 0, SEEK_END);\n";
    source << "    long size = ftell(fp);\n";
    source << "    fseek(fp, 0, SEEK_SET);\n";
    source << "    if (gDecompressCallback)\n";
    source << "    {\n";
    source << "        if (!strstr(fileName, \".gz\"))\n";
    source << "        {\n";
    source << "            fprintf(stderr, \"Filename does not end in .gz\");\n";
    source << "            exit(1);\n";
    source << "        }\n";
    source << "        std::vector<uint8_t> compressedData(size);\n";
    source << "        (void)fread(compressedData.data(), 1, size, fp);\n";
    source << "        gBinaryData = gDecompressCallback(compressedData);\n";
    source << "    }\n";
    source << "    else\n";
    source << "    {\n";
    source << "        if (!strstr(fileName, \".angledata\"))\n";
    source << "        {\n";
    source << "            fprintf(stderr, \"Filename does not end in .angledata\");\n";
    source << "            exit(1);\n";
    source << "        }\n";
    source << "        gBinaryData = new uint8_t[size];\n";
    source << "        (void)fread(gBinaryData, 1, size, fp);\n";
    source << "    }\n";
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
        source << "    memcpy(gClientArrays[arrayIndex], data, static_cast<size_t>(size));\n";
        source << "}\n";
    }

    // Data types and functions for tracking contents of mapped buffers
    header << "using BufferHandleMap = std::unordered_map<GLuint, void*>;\n";
    header << "extern BufferHandleMap gMappedBufferData;\n";
    header << "void UpdateClientBufferData(GLuint bufferID, const void *source, GLsizei size);\n";
    source << "BufferHandleMap gMappedBufferData;\n";
    source << "\n";
    source << "void UpdateClientBufferData(GLuint bufferID, const void *source, GLsizei size)";
    source << "\n";
    source << "{\n";
    source << "    memcpy(gMappedBufferData[gBufferMap[bufferID]], source, size);\n";
    source << "}\n";

    for (ResourceIDType resourceType : AllEnums<ResourceIDType>())
    {
        // TODO: Only emit resources needed by the frames (anglebug.com/4223)
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

void CaptureUpdateUniformLocations(const gl::Program *program, std::vector<CallCapture> *callsOut)
{
    const std::vector<gl::LinkedUniform> &uniforms     = program->getState().getUniforms();
    const std::vector<gl::VariableLocation> &locations = program->getUniformLocations();

    for (GLint location = 0; location < static_cast<GLint>(locations.size()); ++location)
    {
        const gl::VariableLocation &locationVar = locations[location];
        const gl::LinkedUniform &uniform        = uniforms[locationVar.index];

        ParamBuffer params;
        params.addValueParam("program", ParamType::TShaderProgramID, program->id());

        std::string name = uniform.name;

        if (uniform.isArray())
        {
            if (locationVar.arrayIndex > 0)
            {
                // Non-sequential array uniform locations are not currently handled.
                // In practice array locations shouldn't ever be non-sequential.
                ASSERT(uniform.location == -1 ||
                       location == uniform.location + static_cast<int>(locationVar.arrayIndex));
                continue;
            }

            if (uniform.isArrayOfArrays())
            {
                UNIMPLEMENTED();
            }

            name = gl::StripLastArrayIndex(name);
        }

        ParamCapture nameParam("name", ParamType::TGLcharConstPointer);
        CaptureString(name.c_str(), &nameParam);
        params.addParam(std::move(nameParam));

        params.addValueParam("location", ParamType::TGLint, location);
        callsOut->emplace_back("UpdateUniformLocation", std::move(params));
    }
}

void CaptureDeleteUniformLocations(gl::ShaderProgramID program, std::vector<CallCapture> *callsOut)
{
    ParamBuffer params;
    params.addValueParam("program", ParamType::TShaderProgramID, program);
    callsOut->emplace_back("DeleteUniformLocations", std::move(params));
}

void CaptureOnFramebufferChange(GLenum target,
                                gl::FramebufferID framebufferID,
                                std::vector<CallCapture> *callsOut)
{
    ParamBuffer params;
    params.addValueParam("target", ParamType::TGLenum, target);
    params.addValueParam("framebuffer", ParamType::TFramebufferID, framebufferID);
    callsOut->emplace_back("OnFramebufferChange", std::move(params));
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

void CaptureUpdateCurrentProgram(const CallCapture &call, std::vector<CallCapture> *callsOut)
{
    const ParamCapture &param =
        call.params.getParam("programPacked", ParamType::TShaderProgramID, 0);
    gl::ShaderProgramID programID = param.value.ShaderProgramIDVal;

    ParamBuffer paramBuffer;
    paramBuffer.addValueParam("program", ParamType::TShaderProgramID, programID);

    callsOut->emplace_back("UpdateCurrentProgram", std::move(paramBuffer));
}

bool IsDefaultCurrentValue(const gl::VertexAttribCurrentValueData &currentValue)
{
    if (currentValue.Type != gl::VertexAttribType::Float)
        return false;

    return currentValue.Values.FloatValues[0] == 0.0f &&
           currentValue.Values.FloatValues[1] == 0.0f &&
           currentValue.Values.FloatValues[2] == 0.0f && currentValue.Values.FloatValues[3] == 1.0f;
}

bool IsQueryActive(const gl::State &glState, gl::QueryID &queryID)
{
    const gl::ActiveQueryMap &activeQueries = glState.getActiveQueriesForCapture();
    for (const auto &activeQueryIter : activeQueries)
    {
        const gl::Query *activeQuery = activeQueryIter.get();
        if (activeQuery && activeQuery->id() == queryID)
        {
            return true;
        }
    }

    return false;
}

void Capture(std::vector<CallCapture> *setupCalls, CallCapture &&call)
{
    setupCalls->emplace_back(std::move(call));
}

void CaptureFramebufferAttachment(std::vector<CallCapture> *setupCalls,
                                  const gl::State &replayState,
                                  const gl::FramebufferAttachment &attachment)
{
    GLuint resourceID = attachment.getResource()->getId();

    // TODO(jmadill): Layer attachments. http://anglebug.com/3662
    if (attachment.type() == GL_TEXTURE)
    {
        gl::ImageIndex index = attachment.getTextureImageIndex();

        Capture(setupCalls, CaptureFramebufferTexture2D(replayState, true, GL_FRAMEBUFFER,
                                                        attachment.getBinding(), index.getTarget(),
                                                        {resourceID}, index.getLevelIndex()));
    }
    else
    {
        ASSERT(attachment.type() == GL_RENDERBUFFER);
        Capture(setupCalls, CaptureFramebufferRenderbuffer(replayState, true, GL_FRAMEBUFFER,
                                                           attachment.getBinding(), GL_RENDERBUFFER,
                                                           {resourceID}));
    }
}

void CaptureUpdateUniformValues(const gl::State &replayState,
                                const gl::Context *context,
                                const gl::Program *program,
                                std::vector<CallCapture> *callsOut)
{
    if (!program->isLinked())
    {
        // We can't populate uniforms if the program hasn't been linked
        return;
    }

    // We need to bind the program and update its uniforms
    // TODO (http://anglebug.com/3662): Only bind if different from currently bound
    Capture(callsOut, CaptureUseProgram(replayState, true, program->id()));
    CaptureUpdateCurrentProgram(callsOut->back(), callsOut);

    const std::vector<gl::LinkedUniform> &uniforms = program->getState().getUniforms();

    for (size_t i = 0; i < uniforms.size(); i++)
    {
        const gl::LinkedUniform &uniform = uniforms[i];
        std::string uniformName          = uniform.name;

        int uniformCount = 1;
        if (uniform.isArray())
        {
            if (uniform.isArrayOfArrays())
            {
                UNIMPLEMENTED();
                continue;
            }

            uniformCount = uniform.arraySizes[0];
            uniformName  = gl::StripLastArrayIndex(uniformName);
        }

        gl::UniformLocation uniformLoc      = program->getUniformLocation(uniformName);
        const gl::UniformTypeInfo *typeInfo = uniform.typeInfo;
        int uniformSize                     = uniformCount * typeInfo->componentCount;

        switch (typeInfo->componentType)
        {
            case GL_FLOAT:
            {
                std::vector<GLfloat> uniformBuffer(uniformSize);
                program->getUniformfv(context, uniformLoc, uniformBuffer.data());
                switch (typeInfo->type)
                {
                    // Note: All matrix uniforms are populated without transpose
                    case GL_FLOAT_MAT4x3:
                        Capture(callsOut, CaptureUniformMatrix4x3fv(replayState, true, uniformLoc,
                                                                    uniformCount, false,
                                                                    uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT4x2:
                        Capture(callsOut, CaptureUniformMatrix4x2fv(replayState, true, uniformLoc,
                                                                    uniformCount, false,
                                                                    uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT4:
                        Capture(callsOut,
                                CaptureUniformMatrix4fv(replayState, true, uniformLoc, uniformCount,
                                                        false, uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT3x4:
                        Capture(callsOut, CaptureUniformMatrix3x4fv(replayState, true, uniformLoc,
                                                                    uniformCount, false,
                                                                    uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT3x2:
                        Capture(callsOut, CaptureUniformMatrix3x2fv(replayState, true, uniformLoc,
                                                                    uniformCount, false,
                                                                    uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT3:
                        Capture(callsOut,
                                CaptureUniformMatrix3fv(replayState, true, uniformLoc, uniformCount,
                                                        false, uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT2x4:
                        Capture(callsOut, CaptureUniformMatrix2x4fv(replayState, true, uniformLoc,
                                                                    uniformCount, false,
                                                                    uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT2x3:
                        Capture(callsOut, CaptureUniformMatrix2x3fv(replayState, true, uniformLoc,
                                                                    uniformCount, false,
                                                                    uniformBuffer.data()));
                        break;
                    case GL_FLOAT_MAT2:
                        Capture(callsOut,
                                CaptureUniformMatrix2fv(replayState, true, uniformLoc, uniformCount,
                                                        false, uniformBuffer.data()));
                        break;
                    case GL_FLOAT_VEC4:
                        Capture(callsOut, CaptureUniform4fv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    case GL_FLOAT_VEC3:
                        Capture(callsOut, CaptureUniform3fv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    case GL_FLOAT_VEC2:
                        Capture(callsOut, CaptureUniform2fv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    case GL_FLOAT:
                        Capture(callsOut, CaptureUniform1fv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    default:
                        UNIMPLEMENTED();
                        break;
                }
                break;
            }
            case GL_INT:
            {
                std::vector<GLint> uniformBuffer(uniformSize);
                program->getUniformiv(context, uniformLoc, uniformBuffer.data());
                switch (typeInfo->componentCount)
                {
                    case 4:
                        Capture(callsOut, CaptureUniform4iv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    case 3:
                        Capture(callsOut, CaptureUniform3iv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    case 2:
                        Capture(callsOut, CaptureUniform2iv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    case 1:
                        Capture(callsOut, CaptureUniform1iv(replayState, true, uniformLoc,
                                                            uniformCount, uniformBuffer.data()));
                        break;
                    default:
                        UNIMPLEMENTED();
                        break;
                }
                break;
            }
            case GL_UNSIGNED_INT:
            {
                std::vector<GLuint> uniformBuffer(uniformSize);
                program->getUniformuiv(context, uniformLoc, uniformBuffer.data());
                switch (typeInfo->componentCount)
                {
                    case 4:
                        Capture(callsOut, CaptureUniform4uiv(replayState, true, uniformLoc,
                                                             uniformCount, uniformBuffer.data()));
                        break;
                    case 3:
                        Capture(callsOut, CaptureUniform3uiv(replayState, true, uniformLoc,
                                                             uniformCount, uniformBuffer.data()));
                        break;
                    case 2:
                        Capture(callsOut, CaptureUniform2uiv(replayState, true, uniformLoc,
                                                             uniformCount, uniformBuffer.data()));
                        break;
                    case 1:
                        Capture(callsOut, CaptureUniform1uiv(replayState, true, uniformLoc,
                                                             uniformCount, uniformBuffer.data()));
                        break;
                    default:
                        UNIMPLEMENTED();
                        break;
                }
                break;
            }
            default:
                UNIMPLEMENTED();
                break;
        }
    }
}

void CaptureVertexArrayData(std::vector<CallCapture> *setupCalls,
                            const gl::Context *context,
                            const gl::VertexArray *vertexArray,
                            gl::State *replayState)
{
    const std::vector<gl::VertexAttribute> &vertexAttribs = vertexArray->getVertexAttributes();
    const std::vector<gl::VertexBinding> &vertexBindings  = vertexArray->getVertexBindings();

    for (GLuint attribIndex = 0; attribIndex < gl::MAX_VERTEX_ATTRIBS; ++attribIndex)
    {
        const gl::VertexAttribute defaultAttrib(attribIndex);
        const gl::VertexBinding defaultBinding;

        const gl::VertexAttribute &attrib = vertexAttribs[attribIndex];
        const gl::VertexBinding &binding  = vertexBindings[attrib.bindingIndex];

        if (attrib.enabled != defaultAttrib.enabled)
        {
            Capture(setupCalls, CaptureEnableVertexAttribArray(*replayState, false, attribIndex));
        }

        if (attrib.format != defaultAttrib.format || attrib.pointer != defaultAttrib.pointer ||
            binding.getStride() != defaultBinding.getStride() ||
            binding.getBuffer().get() != nullptr)
        {
            gl::Buffer *buffer = binding.getBuffer().get();

            if (buffer != replayState->getArrayBuffer())
            {
                replayState->setBufferBinding(context, gl::BufferBinding::Array, buffer);
                Capture(setupCalls, CaptureBindBuffer(*replayState, true, gl::BufferBinding::Array,
                                                      buffer->id()));
            }

            Capture(setupCalls, CaptureVertexAttribPointer(
                                    *replayState, true, attribIndex, attrib.format->channelCount,
                                    attrib.format->vertexAttribType, attrib.format->isNorm(),
                                    binding.getStride(), attrib.pointer));
        }

        if (binding.getDivisor() != 0)
        {
            Capture(setupCalls, CaptureVertexAttribDivisor(*replayState, true, attribIndex,
                                                           binding.getDivisor()));
        }
    }
}

void CaptureTextureStorage(std::vector<CallCapture> *setupCalls,
                           gl::State *replayState,
                           const gl::Texture *texture)
{
    // Use mip-level 0 for the base dimensions
    gl::ImageIndex imageIndex = gl::ImageIndex::MakeFromType(texture->getType(), 0);
    const gl::ImageDesc &desc = texture->getTextureState().getImageDesc(imageIndex);

    switch (texture->getType())
    {
        case gl::TextureType::_2D:
        case gl::TextureType::CubeMap:
        {
            Capture(setupCalls, CaptureTexStorage2D(*replayState, true, texture->getType(),
                                                    texture->getImmutableLevels(),
                                                    desc.format.info->internalFormat,
                                                    desc.size.width, desc.size.height));
            break;
        }
        case gl::TextureType::_3D:
        case gl::TextureType::_2DArray:
        {
            Capture(setupCalls, CaptureTexStorage3D(
                                    *replayState, true, texture->getType(),
                                    texture->getImmutableLevels(), desc.format.info->internalFormat,
                                    desc.size.width, desc.size.height, desc.size.depth));
            break;
        }
        default:
            UNIMPLEMENTED();
            break;
    }
}

void CaptureTextureContents(std::vector<CallCapture> *setupCalls,
                            gl::State *replayState,
                            const gl::Texture *texture,
                            const gl::ImageIndex &index,
                            const gl::ImageDesc &desc,
                            GLuint size,
                            const void *data)
{
    const gl::InternalFormat &format = *desc.format.info;

    bool is3D =
        (index.getType() == gl::TextureType::_3D || index.getType() == gl::TextureType::_2DArray);

    if (format.compressed)
    {
        if (is3D)
        {
            if (texture->getImmutableFormat())
            {
                Capture(setupCalls,
                        CaptureCompressedTexSubImage3D(
                            *replayState, true, index.getTarget(), index.getLevelIndex(), 0, 0, 0,
                            desc.size.width, desc.size.height, desc.size.depth,
                            format.internalFormat, size, data));
            }
            else
            {
                Capture(setupCalls,
                        CaptureCompressedTexImage3D(*replayState, true, index.getTarget(),
                                                    index.getLevelIndex(), format.internalFormat,
                                                    desc.size.width, desc.size.height,
                                                    desc.size.depth, 0, size, data));
            }
        }
        else
        {
            if (texture->getImmutableFormat())
            {
                Capture(setupCalls,
                        CaptureCompressedTexSubImage2D(
                            *replayState, true, index.getTarget(), index.getLevelIndex(), 0, 0,
                            desc.size.width, desc.size.height, format.internalFormat, size, data));
            }
            else
            {
                Capture(setupCalls, CaptureCompressedTexImage2D(
                                        *replayState, true, index.getTarget(),
                                        index.getLevelIndex(), format.internalFormat,
                                        desc.size.width, desc.size.height, 0, size, data));
            }
        }
    }
    else
    {
        if (is3D)
        {
            if (texture->getImmutableFormat())
            {
                Capture(setupCalls,
                        CaptureTexSubImage3D(*replayState, true, index.getTarget(),
                                             index.getLevelIndex(), 0, 0, 0, desc.size.width,
                                             desc.size.height, desc.size.depth, format.format,
                                             format.type, data));
            }
            else
            {
                Capture(
                    setupCalls,
                    CaptureTexImage3D(*replayState, true, index.getTarget(), index.getLevelIndex(),
                                      format.internalFormat, desc.size.width, desc.size.height,
                                      desc.size.depth, 0, format.format, format.type, data));
            }
        }
        else
        {
            if (texture->getImmutableFormat())
            {
                Capture(setupCalls,
                        CaptureTexSubImage2D(*replayState, true, index.getTarget(),
                                             index.getLevelIndex(), 0, 0, desc.size.width,
                                             desc.size.height, format.format, format.type, data));
            }
            else
            {
                Capture(setupCalls, CaptureTexImage2D(*replayState, true, index.getTarget(),
                                                      index.getLevelIndex(), format.internalFormat,
                                                      desc.size.width, desc.size.height, 0,
                                                      format.format, format.type, data));
            }
        }
    }
}

// TODO(http://anglebug.com/4599): Improve reset/restore call generation
// There are multiple ways to track reset calls for individual resources. For now, we are tracking
// separate lists of instructions that mirror the calls created during mid-execution setup. Other
// methods could involve passing the original CallCaptures to this function, or tracking the
// indices of original setup calls.
void CaptureBufferResetCalls(const gl::State &replayState,
                             ResourceTracker *resourceTracker,
                             gl::BufferID *id,
                             const gl::Buffer *buffer)
{
    // Track this as a starting resource that may need to be restored.
    BufferSet &startingBuffers = resourceTracker->getStartingBuffers();
    startingBuffers.insert(*id);

    // Track calls to regenerate a given buffer
    BufferCalls &bufferRegenCalls = resourceTracker->getBufferRegenCalls();
    Capture(&bufferRegenCalls[*id], CaptureDeleteBuffers(replayState, true, 1, id));
    Capture(&bufferRegenCalls[*id], CaptureGenBuffers(replayState, true, 1, id));
    MaybeCaptureUpdateResourceIDs(&bufferRegenCalls[*id]);

    // Track calls to restore a given buffer's contents
    BufferCalls &bufferRestoreCalls = resourceTracker->getBufferRestoreCalls();
    Capture(&bufferRestoreCalls[*id],
            CaptureBindBuffer(replayState, true, gl::BufferBinding::Array, *id));
    Capture(&bufferRestoreCalls[*id],
            CaptureBufferData(replayState, true, gl::BufferBinding::Array,
                              static_cast<GLsizeiptr>(buffer->getSize()), buffer->getMapPointer(),
                              buffer->getUsage()));
}

void CaptureMidExecutionSetup(const gl::Context *context,
                              std::vector<CallCapture> *setupCalls,
                              ResourceTracker *resourceTracker,
                              const ShaderSourceMap &cachedShaderSources,
                              const ProgramSourceMap &cachedProgramSources,
                              const TextureLevelDataMap &cachedTextureLevelData)
{
    const gl::State &apiState = context->getState();
    gl::State replayState(nullptr, nullptr, nullptr, EGL_OPENGL_ES_API, apiState.getClientVersion(),
                          false, true, true, true, false, EGL_CONTEXT_PRIORITY_MEDIUM_IMG);

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

        // Generate the calls needed to restore this buffer to original state for frame looping
        CaptureBufferResetCalls(replayState, resourceTracker, &id, buffer);

        GLboolean dontCare;
        (void)buffer->unmap(context, &dontCare);
    }

    // Vertex input states. Only handles GLES 2.0 states right now.
    // Must happen after buffer data initialization.
    // TODO(http://anglebug.com/3662): Complete state capture.

    // Capture default vertex attribs
    const std::vector<gl::VertexAttribCurrentValueData> &currentValues =
        apiState.getVertexAttribCurrentValues();

    for (GLuint attribIndex = 0; attribIndex < gl::MAX_VERTEX_ATTRIBS; ++attribIndex)
    {
        const gl::VertexAttribCurrentValueData &defaultValue = currentValues[attribIndex];
        if (!IsDefaultCurrentValue(defaultValue))
        {
            Capture(setupCalls, CaptureVertexAttrib4fv(replayState, true, attribIndex,
                                                       defaultValue.Values.FloatValues));
        }
    }

    // Capture vertex array objects
    const gl::VertexArrayMap &vertexArrayMap = context->getVertexArraysForCapture();
    gl::VertexArrayID boundVertexArrayID     = {0};
    for (const auto &vertexArrayIter : vertexArrayMap)
    {
        gl::VertexArrayID vertexArrayID = {vertexArrayIter.first};
        cap(CaptureGenVertexArrays(replayState, true, 1, &vertexArrayID));
        MaybeCaptureUpdateResourceIDs(setupCalls);

        if (vertexArrayIter.second)
        {
            const gl::VertexArray *vertexArray = vertexArrayIter.second;

            // Bind the vertexArray (unless default) and populate it
            if (vertexArrayID.value != 0)
            {
                cap(CaptureBindVertexArray(replayState, true, vertexArrayID));
                boundVertexArrayID = vertexArrayID;
            }
            CaptureVertexArrayData(setupCalls, context, vertexArray, &replayState);
        }
    }

    // Bind the current vertex array
    const gl::VertexArray *currentVertexArray = apiState.getVertexArray();
    if (currentVertexArray->id() != boundVertexArrayID)
    {
        cap(CaptureBindVertexArray(replayState, true, currentVertexArray->id()));
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
        if ((isArray && arrayBuffer && arrayBuffer->id() != bufferID) ||
            (!isArray && bufferID.value != 0))
        {
            cap(CaptureBindBuffer(replayState, true, binding, bufferID));
        }
    }

    // Set a unpack alignment of 1.
    gl::PixelUnpackState &currentUnpackState = replayState.getUnpackState();
    if (currentUnpackState.alignment != 1)
    {
        cap(CapturePixelStorei(replayState, true, GL_UNPACK_ALIGNMENT, 1));
        currentUnpackState.alignment = 1;
    }

    // Capture Texture setup and data.
    const gl::TextureManager &textures         = apiState.getTextureManagerForCapture();
    const gl::TextureBindingMap &boundTextures = apiState.getBoundTexturesForCapture();

    gl::TextureTypeMap<gl::TextureID> currentTextureBindings;

    for (const auto &textureIter : textures)
    {
        gl::TextureID id     = {textureIter.first};
        gl::Texture *texture = textureIter.second;

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

        auto capTexParamf = [cap, &replayState, texture](GLenum pname, GLfloat param) {
            cap(CaptureTexParameterf(replayState, true, texture->getType(), pname, param));
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

        if (textureSamplerState.getMinLod() != defaultSamplerState.getMinLod())
        {
            capTexParamf(GL_TEXTURE_MIN_LOD, textureSamplerState.getMinLod());
        }

        if (textureSamplerState.getMaxLod() != defaultSamplerState.getMaxLod())
        {
            capTexParamf(GL_TEXTURE_MAX_LOD, textureSamplerState.getMaxLod());
        }

        if (textureSamplerState.getCompareMode() != defaultSamplerState.getCompareMode())
        {
            capTexParam(GL_TEXTURE_COMPARE_MODE, textureSamplerState.getCompareMode());
        }

        if (textureSamplerState.getCompareFunc() != defaultSamplerState.getCompareFunc())
        {
            capTexParam(GL_TEXTURE_COMPARE_FUNC, textureSamplerState.getCompareFunc());
        }

        // Texture parameters
        if (texture->getSwizzleRed() != GL_RED)
        {
            capTexParam(GL_TEXTURE_SWIZZLE_R, texture->getSwizzleRed());
        }

        if (texture->getSwizzleGreen() != GL_GREEN)
        {
            capTexParam(GL_TEXTURE_SWIZZLE_G, texture->getSwizzleGreen());
        }

        if (texture->getSwizzleBlue() != GL_BLUE)
        {
            capTexParam(GL_TEXTURE_SWIZZLE_B, texture->getSwizzleBlue());
        }

        if (texture->getSwizzleAlpha() != GL_ALPHA)
        {
            capTexParam(GL_TEXTURE_SWIZZLE_A, texture->getSwizzleAlpha());
        }

        if (texture->getBaseLevel() != 0)
        {
            capTexParam(GL_TEXTURE_BASE_LEVEL, texture->getBaseLevel());
        }

        if (texture->getMaxLevel() != 1000)
        {
            capTexParam(GL_TEXTURE_MAX_LEVEL, texture->getMaxLevel());
        }

        // If the texture is immutable, initialize it with TexStorage
        if (texture->getImmutableFormat())
        {
            CaptureTextureStorage(setupCalls, &replayState, texture);
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

            // Check for supported textures
            ASSERT(index.getType() == gl::TextureType::_2D ||
                   index.getType() == gl::TextureType::_3D ||
                   index.getType() == gl::TextureType::_2DArray ||
                   index.getType() == gl::TextureType::CubeMap);

            if (format.compressed)
            {
                // For compressed images, we've tracked a copy of the incoming data, so we can
                // use that rather than try to read data back that may have been converted.

                // Look up the data for the requested texture
                const auto &foundTextureLevels = cachedTextureLevelData.find(texture->id());
                ASSERT(foundTextureLevels != cachedTextureLevelData.end());

                // For that texture, look up the data for the given level
                GLint level                   = index.getLevelIndex();
                const auto &foundTextureLevel = foundTextureLevels->second.find(level);
                ASSERT(foundTextureLevel != foundTextureLevels->second.end());
                const std::vector<uint8_t> &capturedTextureLevel = foundTextureLevel->second;

                // Use the shadow copy of the data to populate the call
                CaptureTextureContents(setupCalls, &replayState, texture, index, desc,
                                       static_cast<GLuint>(capturedTextureLevel.size()),
                                       capturedTextureLevel.data());
            }
            else
            {
                // Use ANGLE_get_image to read back pixel data.
                if (context->getExtensions().getImageANGLE)
                {
                    GLenum getFormat = format.format;
                    GLenum getType   = format.type;

                    angle::MemoryBuffer data;

                    const gl::Extents size(desc.size.width, desc.size.height, desc.size.depth);
                    const gl::PixelUnpackState &unpack = apiState.getUnpackState();

                    GLuint endByte = 0;
                    bool unpackSize =
                        format.computePackUnpackEndByte(getType, size, unpack, true, &endByte);
                    ASSERT(unpackSize);

                    bool result = data.resize(endByte);
                    ASSERT(result);

                    gl::PixelPackState packState;
                    packState.alignment = 1;

                    (void)texture->getTexImage(context, packState, nullptr, index.getTarget(),
                                               index.getLevelIndex(), getFormat, getType,
                                               data.data());

                    CaptureTextureContents(setupCalls, &replayState, texture, index, desc,
                                           static_cast<GLuint>(data.size()), data.data());
                }
                else
                {
                    CaptureTextureContents(setupCalls, &replayState, texture, index, desc, 0,
                                           nullptr);
                }
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

            CaptureFramebufferAttachment(setupCalls, replayState, colorAttachment);
        }

        const gl::FramebufferAttachment *depthAttachment = framebuffer->getDepthAttachment();
        if (depthAttachment)
        {
            ASSERT(depthAttachment->getBinding() == GL_DEPTH_ATTACHMENT);
            CaptureFramebufferAttachment(setupCalls, replayState, *depthAttachment);
        }

        const gl::FramebufferAttachment *stencilAttachment = framebuffer->getStencilAttachment();
        if (stencilAttachment)
        {
            ASSERT(stencilAttachment->getBinding() == GL_STENCIL_ATTACHMENT);
            CaptureFramebufferAttachment(setupCalls, replayState, *stencilAttachment);
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
        gl::ShaderProgramID id     = {programIter.first};
        const gl::Program *program = programIter.second;

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
        for (gl::ShaderType shaderType : program->getExecutable().getLinkedShaderStages())
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

        // Gather XFB varyings
        std::vector<std::string> xfbVaryings;
        for (const gl::TransformFeedbackVarying &xfbVarying :
             program->getState().getLinkedTransformFeedbackVaryings())
        {
            xfbVaryings.push_back(xfbVarying.nameWithArrayIndex());
        }

        if (!xfbVaryings.empty())
        {
            std::vector<const char *> varyingsStrings;
            for (const std::string &varyingString : xfbVaryings)
            {
                varyingsStrings.push_back(varyingString.data());
            }

            GLenum xfbMode = program->getState().getTransformFeedbackBufferMode();
            cap(CaptureTransformFeedbackVaryings(replayState, true, id,
                                                 static_cast<GLint>(xfbVaryings.size()),
                                                 varyingsStrings.data(), xfbMode));
        }

        // Force the attributes to be bound the same way as in the existing program.
        // This can affect attributes that are optimized out in some implementations.
        for (const sh::ShaderVariable &attrib : program->getState().getProgramInputs())
        {
            ASSERT(attrib.location != -1);
            cap(CaptureBindAttribLocation(
                replayState, true, id, static_cast<GLuint>(attrib.location), attrib.name.c_str()));
        }

        cap(CaptureLinkProgram(replayState, true, id));
        CaptureUpdateUniformLocations(program, setupCalls);
        CaptureUpdateUniformValues(replayState, context, program, setupCalls);
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
        CaptureUpdateCurrentProgram(setupCalls->back(), setupCalls);
    }

    // TODO(http://anglebug.com/3662): ES 3.x objects.

    // Create existing queries. Note that queries may be genned and not yet started. In that
    // case the queries will exist in the query map as nullptr entries.
    const gl::QueryMap &queryMap = context->getQueriesForCapture();
    for (gl::QueryMap::Iterator queryIter = queryMap.beginWithNull();
         queryIter != queryMap.endWithNull(); ++queryIter)
    {
        ASSERT(queryIter->first);
        gl::QueryID queryID = {queryIter->first};

        cap(CaptureGenQueries(replayState, true, 1, &queryID));
        MaybeCaptureUpdateResourceIDs(setupCalls);

        gl::Query *query = queryIter->second;
        if (query)
        {
            gl::QueryType queryType = query->getType();

            // Begin the query to generate the object
            cap(CaptureBeginQuery(replayState, true, queryType, queryID));

            // End the query if it was not active
            if (!IsQueryActive(apiState, queryID))
            {
                cap(CaptureEndQuery(replayState, true, queryType));
            }
        }
    }

    // Transform Feedback
    const gl::TransformFeedbackMap &xfbMap = context->getTransformFeedbacksForCapture();
    for (const auto &xfbIter : xfbMap)
    {
        gl::TransformFeedbackID xfbID = {xfbIter.first};
        cap(CaptureGenTransformFeedbacks(replayState, true, 1, &xfbID));
        MaybeCaptureUpdateResourceIDs(setupCalls);

        gl::TransformFeedback *xfb = xfbIter.second;
        if (!xfb)
        {
            // The object was never created
            continue;
        }

        // Bind XFB to create the object
        cap(CaptureBindTransformFeedback(replayState, true, GL_TRANSFORM_FEEDBACK, xfbID));

        // Bind the buffers associated with this XFB object
        for (size_t i = 0; i < xfb->getIndexedBufferCount(); ++i)
        {
            const gl::OffsetBindingPointer<gl::Buffer> &xfbBuffer = xfb->getIndexedBuffer(i);

            // Note: Buffers bound with BindBufferBase can be used with BindBuffer
            cap(CaptureBindBufferRange(replayState, true, gl::BufferBinding::TransformFeedback, 0,
                                       xfbBuffer.id(), xfbBuffer.getOffset(), xfbBuffer.getSize()));
        }

        if (xfb->isActive() || xfb->isPaused())
        {
            // We don't support active XFB in MEC yet
            UNIMPLEMENTED();
        }
    }

    // Bind the current XFB buffer after populating XFB objects
    gl::TransformFeedback *currentXFB = apiState.getCurrentTransformFeedback();
    cap(CaptureBindTransformFeedback(replayState, true, GL_TRANSFORM_FEEDBACK, currentXFB->id()));

    // Capture Sampler Objects
    const gl::SamplerManager &samplers = apiState.getSamplerManagerForCapture();
    for (const auto &samplerIter : samplers)
    {
        gl::SamplerID samplerID = {samplerIter.first};
        cap(CaptureGenSamplers(replayState, true, 1, &samplerID));
        MaybeCaptureUpdateResourceIDs(setupCalls);

        gl::Sampler *sampler = samplerIter.second;
        if (!sampler)
        {
            continue;
        }

        gl::SamplerState defaultSamplerState;
        if (sampler->getMinFilter() != defaultSamplerState.getMinFilter())
        {
            cap(CaptureSamplerParameteri(replayState, true, samplerID, GL_TEXTURE_MIN_FILTER,
                                         sampler->getMinFilter()));
        }
        if (sampler->getMagFilter() != defaultSamplerState.getMagFilter())
        {
            cap(CaptureSamplerParameteri(replayState, true, samplerID, GL_TEXTURE_MAG_FILTER,
                                         sampler->getMagFilter()));
        }
        if (sampler->getWrapS() != defaultSamplerState.getWrapS())
        {
            cap(CaptureSamplerParameteri(replayState, true, samplerID, GL_TEXTURE_WRAP_S,
                                         sampler->getWrapS()));
        }
        if (sampler->getWrapR() != defaultSamplerState.getWrapR())
        {
            cap(CaptureSamplerParameteri(replayState, true, samplerID, GL_TEXTURE_WRAP_R,
                                         sampler->getWrapR()));
        }
        if (sampler->getWrapT() != defaultSamplerState.getWrapT())
        {
            cap(CaptureSamplerParameteri(replayState, true, samplerID, GL_TEXTURE_WRAP_T,
                                         sampler->getWrapT()));
        }
        if (sampler->getMinLod() != defaultSamplerState.getMinLod())
        {
            cap(CaptureSamplerParameterf(replayState, true, samplerID, GL_TEXTURE_MIN_LOD,
                                         sampler->getMinLod()));
        }
        if (sampler->getMaxLod() != defaultSamplerState.getMaxLod())
        {
            cap(CaptureSamplerParameterf(replayState, true, samplerID, GL_TEXTURE_MAX_LOD,
                                         sampler->getMaxLod()));
        }
        if (sampler->getCompareMode() != defaultSamplerState.getCompareMode())
        {
            cap(CaptureSamplerParameteri(replayState, true, samplerID, GL_TEXTURE_COMPARE_MODE,
                                         sampler->getCompareMode()));
        }
        if (sampler->getCompareFunc() != defaultSamplerState.getCompareFunc())
        {
            cap(CaptureSamplerParameteri(replayState, true, samplerID, GL_TEXTURE_COMPARE_FUNC,
                                         sampler->getCompareFunc()));
        }
    }

    // Bind samplers
    const gl::SamplerBindingVector &samplerBindings = apiState.getSamplers();
    for (GLuint bindingIndex = 0; bindingIndex < static_cast<GLuint>(samplerBindings.size());
         ++bindingIndex)
    {
        gl::SamplerID samplerID = samplerBindings[bindingIndex].id();
        if (samplerID.value != 0)
        {
            cap(CaptureBindSampler(replayState, true, bindingIndex, samplerID));
        }
    }

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
    gl::PixelPackState &currentPackState = replayState.getPackState();
    if (currentPackState.alignment != apiState.getPackAlignment())
    {
        cap(CapturePixelStorei(replayState, true, GL_PACK_ALIGNMENT, apiState.getPackAlignment()));
        currentPackState.alignment = apiState.getPackAlignment();
    }

    if (currentPackState.rowLength != apiState.getPackRowLength())
    {
        cap(CapturePixelStorei(replayState, true, GL_PACK_ROW_LENGTH, apiState.getPackRowLength()));
        currentPackState.rowLength = apiState.getPackRowLength();
    }

    if (currentPackState.skipRows != apiState.getPackSkipRows())
    {
        cap(CapturePixelStorei(replayState, true, GL_PACK_SKIP_ROWS, apiState.getPackSkipRows()));
        currentPackState.skipRows = apiState.getPackSkipRows();
    }

    if (currentPackState.skipPixels != apiState.getPackSkipPixels())
    {
        cap(CapturePixelStorei(replayState, true, GL_PACK_SKIP_PIXELS,
                               apiState.getPackSkipPixels()));
        currentPackState.skipPixels = apiState.getPackSkipPixels();
    }

    // We set unpack alignment above, no need to change it here
    ASSERT(currentUnpackState.alignment == 1);
    if (currentUnpackState.rowLength != apiState.getUnpackRowLength())
    {
        cap(CapturePixelStorei(replayState, true, GL_UNPACK_ROW_LENGTH,
                               apiState.getUnpackRowLength()));
        currentUnpackState.rowLength = apiState.getUnpackRowLength();
    }

    if (currentUnpackState.skipRows != apiState.getUnpackSkipRows())
    {
        cap(CapturePixelStorei(replayState, true, GL_UNPACK_SKIP_ROWS,
                               apiState.getUnpackSkipRows()));
        currentUnpackState.skipRows = apiState.getUnpackSkipRows();
    }

    if (currentUnpackState.skipPixels != apiState.getUnpackSkipPixels())
    {
        cap(CapturePixelStorei(replayState, true, GL_UNPACK_SKIP_PIXELS,
                               apiState.getUnpackSkipPixels()));
        currentUnpackState.skipPixels = apiState.getUnpackSkipPixels();
    }

    if (currentUnpackState.imageHeight != apiState.getUnpackImageHeight())
    {
        cap(CapturePixelStorei(replayState, true, GL_UNPACK_IMAGE_HEIGHT,
                               apiState.getUnpackImageHeight()));
        currentUnpackState.imageHeight = apiState.getUnpackImageHeight();
    }

    if (currentUnpackState.skipImages != apiState.getUnpackSkipImages())
    {
        cap(CapturePixelStorei(replayState, true, GL_UNPACK_SKIP_IMAGES,
                               apiState.getUnpackSkipImages()));
        currentUnpackState.skipImages = apiState.getUnpackSkipImages();
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

    if (apiState.getStencilClearValue() != 0)
    {
        cap(CaptureClearStencil(replayState, true, apiState.getStencilClearValue()));
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

    if (apiState.isDitherEnabled())
    {
        capCap(GL_DITHER, apiState.isDitherEnabled());
    }

    const gl::SyncManager &syncs = apiState.getSyncManagerForCapture();
    for (const auto &syncIter : syncs)
    {
        // TODO: Create existing sync objects (http://anglebug.com/3662)
        (void)syncIter;
        UNIMPLEMENTED();
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
    std::swap(mMappedBufferID, other.mMappedBufferID);
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
      mCompression(true),
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

    std::string compressionFromEnv = angle::GetEnvironmentVar(kCompression);
    if (compressionFromEnv == "0")
    {
        mCompression = false;
    }
}

FrameCapture::~FrameCapture() = default;

void FrameCapture::captureCompressedTextureData(const gl::Context *context, const CallCapture &call)
{
    // For compressed textures, track a shadow copy of the data
    // for use during mid-execution capture, rather than reading it back
    // with ANGLE_get_image

    // Storing the compressed data is handled the same for all entry points,
    // they just have slightly different parameter locations
    int dataParamOffset    = -1;
    int xoffsetParamOffset = -1;
    int yoffsetParamOffset = -1;
    int zoffsetParamOffset = -1;
    int widthParamOffset   = -1;
    int heightParamOffset  = -1;
    int depthParamOffset   = -1;
    switch (call.entryPoint)
    {
        case gl::EntryPoint::CompressedTexSubImage3D:
            xoffsetParamOffset = 2;
            yoffsetParamOffset = 3;
            zoffsetParamOffset = 4;
            widthParamOffset   = 5;
            heightParamOffset  = 6;
            depthParamOffset   = 7;
            dataParamOffset    = 10;
            break;
        case gl::EntryPoint::CompressedTexImage3D:
            widthParamOffset  = 4;
            heightParamOffset = 5;
            depthParamOffset  = 6;
            dataParamOffset   = 9;
            break;
        case gl::EntryPoint::CompressedTexSubImage2D:
            xoffsetParamOffset = 2;
            yoffsetParamOffset = 3;
            widthParamOffset   = 4;
            heightParamOffset  = 5;
            dataParamOffset    = 8;
            break;
        case gl::EntryPoint::CompressedTexImage2D:
            widthParamOffset  = 3;
            heightParamOffset = 4;
            dataParamOffset   = 7;
            break;
        default:
            // There should be no other callers of this function
            ASSERT(0);
            break;
    }

    gl::Buffer *pixelUnpackBuffer =
        context->getState().getTargetBuffer(gl::BufferBinding::PixelUnpack);

    const uint8_t *data = static_cast<const uint8_t *>(
        call.params.getParam("data", ParamType::TvoidConstPointer, dataParamOffset)
            .value.voidConstPointerVal);

    GLsizei imageSize = call.params.getParam("imageSize", ParamType::TGLsizei, dataParamOffset - 1)
                            .value.GLsizeiVal;

    const uint8_t *pixelData = nullptr;

    if (pixelUnpackBuffer)
    {
        // If using pixel unpack buffer, map the buffer and track its data
        ASSERT(!pixelUnpackBuffer->isMapped());
        (void)pixelUnpackBuffer->mapRange(context, reinterpret_cast<GLintptr>(data), imageSize,
                                          GL_MAP_READ_BIT);

        pixelData = reinterpret_cast<const uint8_t *>(pixelUnpackBuffer->getMapPointer());
    }
    else
    {
        pixelData = data;
    }

    if (!pixelData)
    {
        // If no pointer was provided and we weren't able to map the buffer, there is no data to
        // capture
        return;
    }

    // Look up the texture type
    gl::TextureTarget targetPacked =
        call.params.getParam("targetPacked", ParamType::TTextureTarget, 0).value.TextureTargetVal;
    gl::TextureType textureType = gl::TextureTargetToType(targetPacked);

    // Create a copy of the incoming data
    std::vector<uint8_t> compressedData;
    compressedData.assign(pixelData, pixelData + imageSize);

    // Look up the currently bound texture
    gl::Texture *texture = context->getState().getTargetTexture(textureType);
    ASSERT(texture);

    // Record the data, indexed by textureID and level
    GLint level             = call.params.getParam("level", ParamType::TGLint, 1).value.GLintVal;
    auto foundTextureLevels = mCachedTextureLevelData.find(texture->id());
    if (foundTextureLevels == mCachedTextureLevelData.end())
    {
        // Initialize the texture ID data.
        auto emplaceResult = mCachedTextureLevelData.emplace(texture->id(), TextureLevels());
        ASSERT(emplaceResult.second);
        foundTextureLevels = emplaceResult.first;
    }

    // Get the format of the texture for use with the compressed block size math.
    const gl::InternalFormat &format = *texture->getFormat(targetPacked, level).info;

    TextureLevels &foundLevels = foundTextureLevels->second;
    auto foundLevel            = foundLevels.find(level);

    // Divide dimensions according to block size.
    const gl::Extents &levelExtents = texture->getExtents(targetPacked, level);

    if (foundLevel == foundLevels.end())
    {
        // Initialize texture rectangle data. Default init to zero for stability.
        GLuint sizeInBytes;
        bool result = format.computeCompressedImageSize(levelExtents, &sizeInBytes);
        ASSERT(result);

        std::vector<uint8_t> newPixelData(sizeInBytes, 0);
        auto emplaceResult = foundLevels.emplace(level, std::move(newPixelData));
        ASSERT(emplaceResult.second);
        foundLevel = emplaceResult.first;
    }

    // Unpack the various pixel rectangle parameters.
    ASSERT(widthParamOffset != -1);
    ASSERT(heightParamOffset != -1);
    GLsizei pixelWidth =
        call.params.getParam("width", ParamType::TGLsizei, widthParamOffset).value.GLsizeiVal;
    GLsizei pixelHeight =
        call.params.getParam("height", ParamType::TGLsizei, heightParamOffset).value.GLsizeiVal;
    GLsizei pixelDepth = 1;
    if (depthParamOffset != -1)
    {
        pixelDepth =
            call.params.getParam("depth", ParamType::TGLsizei, depthParamOffset).value.GLsizeiVal;
    }

    GLint xoffset = 0;
    GLint yoffset = 0;
    GLint zoffset = 0;

    if (xoffsetParamOffset != -1)
    {
        xoffset =
            call.params.getParam("xoffset", ParamType::TGLint, xoffsetParamOffset).value.GLintVal;
    }

    if (yoffsetParamOffset != -1)
    {
        yoffset =
            call.params.getParam("yoffset", ParamType::TGLint, yoffsetParamOffset).value.GLintVal;
    }

    if (zoffsetParamOffset != -1)
    {
        zoffset =
            call.params.getParam("zoffset", ParamType::TGLint, zoffsetParamOffset).value.GLintVal;
    }

    // Since we're dealing in 4x4 blocks, scale down the width/height pixel offsets.
    ASSERT(format.compressedBlockWidth == 4);
    ASSERT(format.compressedBlockHeight == 4);
    ASSERT(format.compressedBlockDepth == 1);
    pixelWidth >>= 2;
    pixelHeight >>= 2;
    xoffset >>= 2;
    yoffset >>= 2;

    // Update pixel data.
    std::vector<uint8_t> &levelData = foundLevel->second;

    GLint pixelBytes = static_cast<GLint>(format.pixelBytes);

    GLint pixelRowPitch   = pixelWidth * pixelBytes;
    GLint pixelDepthPitch = pixelRowPitch * pixelHeight;
    GLint levelRowPitch   = (levelExtents.width >> 2) * pixelBytes;
    GLint levelDepthPitch = levelRowPitch * (levelExtents.height >> 2);

    for (GLint zindex = 0; zindex < pixelDepth; ++zindex)
    {
        GLint z = zindex + zoffset;
        for (GLint yindex = 0; yindex < pixelHeight; ++yindex)
        {
            GLint y           = yindex + yoffset;
            GLint pixelOffset = zindex * pixelDepthPitch + yindex * pixelRowPitch;
            GLint levelOffset = z * levelDepthPitch + y * levelRowPitch + xoffset * pixelBytes;
            memcpy(&levelData[levelOffset], &pixelData[pixelOffset], pixelRowPitch);
        }
    }

    if (pixelUnpackBuffer)
    {
        GLboolean success;
        (void)pixelUnpackBuffer->unmap(context, &success);
        ASSERT(success);
    }
}

void FrameCapture::maybeCaptureClientData(const gl::Context *context, CallCapture &call)
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

        case gl::EntryPoint::DeleteBuffers:
        {
            GLsizei count = call.params.getParam("n", ParamType::TGLsizei, 0).value.GLsizeiVal;
            const gl::BufferID *bufferIDs =
                call.params.getParam("buffersPacked", ParamType::TBufferIDConstPointer, 1)
                    .value.BufferIDConstPointerVal;
            for (GLsizei i = 0; i < count; i++)
            {
                // For each buffer being deleted, check our backup of data and remove it
                const auto &bufferDataInfo = mBufferDataMap.find(bufferIDs[i]);
                if (bufferDataInfo != mBufferDataMap.end())
                {
                    mBufferDataMap.erase(bufferDataInfo);
                }
                // If we're capturing, track what new buffers have been genned
                if (mFrameIndex >= mFrameStart)
                {
                    mResourceTracker.setDeletedBuffer(bufferIDs[i]);
                }
            }
            break;
        }

        case gl::EntryPoint::GenBuffers:
        {
            GLsizei count = call.params.getParam("n", ParamType::TGLsizei, 0).value.GLsizeiVal;
            const gl::BufferID *bufferIDs =
                call.params.getParam("buffersPacked", ParamType::TBufferIDPointer, 1)
                    .value.BufferIDPointerVal;
            for (GLsizei i = 0; i < count; i++)
            {
                // If we're capturing, track what new buffers have been genned
                if (mFrameIndex >= mFrameStart)
                {
                    mResourceTracker.setGennedBuffer(bufferIDs[i]);
                }
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

        case gl::EntryPoint::CompressedTexImage1D:
        case gl::EntryPoint::CompressedTexSubImage1D:
        {
            UNIMPLEMENTED();
            break;
        }

        case gl::EntryPoint::CompressedTexImage2D:
        case gl::EntryPoint::CompressedTexImage3D:
        case gl::EntryPoint::CompressedTexSubImage2D:
        case gl::EntryPoint::CompressedTexSubImage3D:
        {
            captureCompressedTextureData(context, call);
            break;
        }

        case gl::EntryPoint::DeleteTextures:
        {
            // Free any TextureLevelDataMap entries being tracked for this texture
            // This is to cover the scenario where a texture has been created, its
            // levels cached, then texture deleted and recreated, receiving the same ID

            // Look up how many textures are being deleted
            GLsizei n = call.params.getParam("n", ParamType::TGLsizei, 0).value.GLsizeiVal;

            // Look up the pointer to list of textures
            const gl::TextureID *textureIDs =
                call.params.getParam("texturesPacked", ParamType::TTextureIDConstPointer, 1)
                    .value.TextureIDConstPointerVal;

            // For each texture listed for deletion
            for (int32_t i = 0; i < n; ++i)
            {
                // Look it up in the cache, and delete it if found
                const auto &foundTextureLevels = mCachedTextureLevelData.find(textureIDs[i]);
                if (foundTextureLevels != mCachedTextureLevelData.end())
                {
                    // Delete all texture levels at once
                    mCachedTextureLevelData.erase(foundTextureLevels);
                }
            }
            break;
        }

        case gl::EntryPoint::MapBuffer:
        {
            UNIMPLEMENTED();
            break;
        }
        case gl::EntryPoint::MapBufferOES:
        {
            UNIMPLEMENTED();
            break;
        }
        case gl::EntryPoint::UnmapNamedBuffer:
        {
            UNIMPLEMENTED();
            break;
        }

        case gl::EntryPoint::MapBufferRange:
        case gl::EntryPoint::MapBufferRangeEXT:
        {
            // Use the access bits to see if contents may be modified
            GLbitfield access =
                call.params.getParam("access", ParamType::TGLbitfield, 3).value.GLbitfieldVal;

            if (access & GL_MAP_WRITE_BIT)
            {
                // If this buffer was mapped writable, we don't have any visibility into what
                // happens to it. Therefore, remember the details about it, and we'll read it back
                // on Unmap to repopulate it during replay.

                gl::BufferBinding target =
                    call.params.getParam("targetPacked", ParamType::TBufferBinding, 0)
                        .value.BufferBindingVal;
                GLintptr offset =
                    call.params.getParam("offset", ParamType::TGLintptr, 1).value.GLintptrVal;
                GLsizeiptr length =
                    call.params.getParam("length", ParamType::TGLsizeiptr, 2).value.GLsizeiptrVal;

                gl::Buffer *buffer           = context->getState().getTargetBuffer(target);
                mBufferDataMap[buffer->id()] = std::make_pair(offset, length);

                // Track the bufferID that was just mapped
                call.params.setMappedBufferID(buffer->id());

                // Remember that it was mapped writable, for use during state reset
                mResourceTracker.setBufferModified(buffer->id());
            }
            break;
        }

        case gl::EntryPoint::UnmapBuffer:
        case gl::EntryPoint::UnmapBufferOES:
        {
            // See if we need to capture the buffer contents
            captureMappedBufferSnapshot(context, call);
            break;
        }

        case gl::EntryPoint::BufferData:
        case gl::EntryPoint::BufferSubData:
        {
            gl::BufferBinding target =
                call.params.getParam("targetPacked", ParamType::TBufferBinding, 0)
                    .value.BufferBindingVal;

            gl::Buffer *buffer = context->getState().getTargetBuffer(target);

            // Track that this buffer's contents have been modified
            mResourceTracker.setBufferModified(buffer->id());

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

    maybeCapturePostCallUpdates(context);
}

void FrameCapture::maybeCapturePostCallUpdates(const gl::Context *context)
{
    // Process resource ID updates.
    MaybeCaptureUpdateResourceIDs(&mFrameCalls);

    const CallCapture &lastCall = mFrameCalls.back();
    switch (lastCall.entryPoint)
    {
        case gl::EntryPoint::LinkProgram:
        {
            const ParamCapture &param =
                lastCall.params.getParam("programPacked", ParamType::TShaderProgramID, 0);
            const gl::Program *program =
                context->getProgramResolveLink(param.value.ShaderProgramIDVal);
            CaptureUpdateUniformLocations(program, &mFrameCalls);
            break;
        }
        case gl::EntryPoint::UseProgram:
            CaptureUpdateCurrentProgram(lastCall, &mFrameCalls);
            break;
        case gl::EntryPoint::DeleteProgram:
        {
            const ParamCapture &param =
                lastCall.params.getParam("programPacked", ParamType::TShaderProgramID, 0);
            CaptureDeleteUniformLocations(param.value.ShaderProgramIDVal, &mFrameCalls);
            break;
        }
        case gl::EntryPoint::BindFramebuffer:
        {
            const ParamCapture &target = lastCall.params.getParam("target", ParamType::TGLenum, 0);
            const ParamCapture &framebuffer =
                lastCall.params.getParam("framebufferPacked", ParamType::TFramebufferID, 1);
            CaptureOnFramebufferChange(target.value.GLenumVal, framebuffer.value.FramebufferIDVal,
                                       &mFrameCalls);
            break;
        }
        default:
            break;
    }
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

void FrameCapture::captureMappedBufferSnapshot(const gl::Context *context, const CallCapture &call)
{
    // If the buffer was mapped writable, we need to restore its data, since we have no visibility
    // into what the client did to the buffer while mapped
    // This sequence will result in replay calls like this:
    //   ...
    //   gMappedBufferData[gBufferMap[42]] = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 65536,
    //                                                        GL_MAP_WRITE_BIT);
    //   ...
    //   UpdateClientBufferData(42, &gBinaryData[164631024], 65536);
    //   glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    //   ...

    // Re-map the buffer, using the info we tracked about the buffer
    gl::BufferBinding target =
        call.params.getParam("targetPacked", ParamType::TBufferBinding, 0).value.BufferBindingVal;

    gl::Buffer *buffer         = context->getState().getTargetBuffer(target);
    const auto &bufferDataInfo = mBufferDataMap.find(buffer->id());
    if (bufferDataInfo == mBufferDataMap.end())
    {
        // This buffer was not marked writable, so we did not back it up
        return;
    }

    GLintptr offset   = bufferDataInfo->second.first;
    GLsizeiptr length = bufferDataInfo->second.second;

    // Map the buffer so we can copy its contents out
    ASSERT(!buffer->isMapped());
    angle::Result result = buffer->mapRange(context, offset, length, GL_MAP_READ_BIT);
    if (result != angle::Result::Continue)
    {
        ERR() << "Failed to mapRange of buffer" << std::endl;
    }
    const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer->getMapPointer());

    // Create the parameters to our helper for use during replay
    ParamBuffer dataParamBuffer;

    // Pass in the target buffer ID
    dataParamBuffer.addValueParam("dest", ParamType::TGLuint, buffer->id().value);

    // Capture the current buffer data with a binary param
    ParamCapture captureData("source", ParamType::TvoidConstPointer);
    CaptureMemory(data, length, &captureData);
    dataParamBuffer.addParam(std::move(captureData));

    // Also track its size for use with memcpy
    dataParamBuffer.addValueParam<GLsizeiptr>("size", ParamType::TGLsizeiptr, length);

    // Call the helper that populates the buffer with captured data
    mFrameCalls.emplace_back("UpdateClientBufferData", std::move(dataParamBuffer));

    // Unmap the buffer and move on
    GLboolean dontCare;
    (void)buffer->unmap(context, &dontCare);
}

void FrameCapture::onEndFrame(const gl::Context *context)
{
    // Note that we currently capture before the start frame to collect shader and program sources.
    if (!mFrameCalls.empty() && mFrameIndex >= mFrameStart)
    {
        WriteCppReplay(mCompression, mOutDirectory, context->id(), mCaptureLabel, mFrameIndex,
                       mFrameEnd, mFrameCalls, mSetupCalls, &mResourceTracker, &mBinaryData);

        // Save the index files after the last frame.
        if (mFrameIndex == mFrameEnd)
        {
            WriteCppReplayIndexFiles(mCompression, mOutDirectory, context->id(), mCaptureLabel,
                                     mFrameStart, mFrameEnd, mReadBufferSize, mClientArraySizes,
                                     mHasResourceType);

            if (!mBinaryData.empty())
            {
                SaveBinaryData(mCompression, mOutDirectory, context->id(), mCaptureLabel,
                               mBinaryData);
                mBinaryData.clear();
            }
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
        CaptureMidExecutionSetup(context, &mSetupCalls, &mResourceTracker, mCachedShaderSources,
                                 mCachedProgramSources, mCachedTextureLevelData);
    }
}

DataCounters::DataCounters() = default;

DataCounters::~DataCounters() = default;

int DataCounters::getAndIncrement(gl::EntryPoint entryPoint, const std::string &paramName)
{
    Counter counterKey = {entryPoint, paramName};
    return mData[counterKey]++;
}

ResourceTracker::ResourceTracker() = default;

ResourceTracker::~ResourceTracker() = default;

void ResourceTracker::setDeletedBuffer(gl::BufferID id)
{
    if (mNewBuffers.find(id) != mNewBuffers.end())
    {
        // This is a buffer genned after MEC was initialized, just clear it, since there will be no
        // actions required for it to return to starting state.
        mNewBuffers.erase(id);
        return;
    }

    // Ensure this buffer was in our starting set
    // It's possible this could fire if the app deletes buffers that were never generated
    ASSERT(mStartingBuffers.find(id) != mStartingBuffers.end());

    // In this case, the app is deleting a buffer we started with, we need to regen on loop
    mBuffersToRegen.insert(id);
    mBuffersToRestore.insert(id);
}

void ResourceTracker::setGennedBuffer(gl::BufferID id)
{
    if (mStartingBuffers.find(id) == mStartingBuffers.end())
    {
        // This is a buffer genned after MEC was initialized, track it
        mNewBuffers.insert(id);
        return;
    }
}

void ResourceTracker::setBufferModified(gl::BufferID id)
{
    // If this was a starting buffer, we need to track it for restore
    if (mStartingBuffers.find(id) != mStartingBuffers.end())
    {
        mBuffersToRestore.insert(id);
    }
}

bool FrameCapture::isCapturing() const
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

void CaptureStringLimit(const GLchar *str, uint32_t limit, ParamCapture *paramCapture)
{
    // Write the incoming string up to limit, including null terminator
    size_t length = strlen(str) + 1;

    if (length > limit)
    {
        // If too many characters, resize the string to fit in the limit
        std::string newStr = str;
        newStr.resize(limit - 1);
        CaptureString(newStr.c_str(), paramCapture);
    }
    else
    {
        CaptureMemory(str, length, paramCapture);
    }
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
void WriteParamValueReplay<ParamType::TGLboolean>(std::ostream &os,
                                                  const CallCapture &call,
                                                  GLboolean value)
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
void WriteParamValueReplay<ParamType::TvoidConstPointer>(std::ostream &os,
                                                         const CallCapture &call,
                                                         const void *value)
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
void WriteParamValueReplay<ParamType::TGLDEBUGPROCKHR>(std::ostream &os,
                                                       const CallCapture &call,
                                                       GLDEBUGPROCKHR value)
{}

template <>
void WriteParamValueReplay<ParamType::TGLDEBUGPROC>(std::ostream &os,
                                                    const CallCapture &call,
                                                    GLDEBUGPROC value)
{}

template <>
void WriteParamValueReplay<ParamType::TBufferID>(std::ostream &os,
                                                 const CallCapture &call,
                                                 gl::BufferID value)
{
    os << "gBufferMap[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TFenceNVID>(std::ostream &os,
                                                  const CallCapture &call,
                                                  gl::FenceNVID value)
{
    os << "gFenceMap[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TFramebufferID>(std::ostream &os,
                                                      const CallCapture &call,
                                                      gl::FramebufferID value)
{
    os << "gFramebufferMap[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TMemoryObjectID>(std::ostream &os,
                                                       const CallCapture &call,
                                                       gl::MemoryObjectID value)
{
    os << "gMemoryObjectMap[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TProgramPipelineID>(std::ostream &os,
                                                          const CallCapture &call,
                                                          gl::ProgramPipelineID value)
{
    os << "gProgramPipelineMap[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TQueryID>(std::ostream &os,
                                                const CallCapture &call,
                                                gl::QueryID value)
{
    os << "gQueryMap[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TRenderbufferID>(std::ostream &os,
                                                       const CallCapture &call,
                                                       gl::RenderbufferID value)
{
    os << "gRenderbufferMap[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TSamplerID>(std::ostream &os,
                                                  const CallCapture &call,
                                                  gl::SamplerID value)
{
    os << "gSamplerMap[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TSemaphoreID>(std::ostream &os,
                                                    const CallCapture &call,
                                                    gl::SemaphoreID value)
{
    os << "gSempahoreMap[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TShaderProgramID>(std::ostream &os,
                                                        const CallCapture &call,
                                                        gl::ShaderProgramID value)
{
    os << "gShaderProgramMap[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TGLsync>(std::ostream &os,
                                               const CallCapture &call,
                                               GLsync value)
{
    os << "gSyncMap[" << SyncIndexValue(value) << "]";
}

template <>
void WriteParamValueReplay<ParamType::TTextureID>(std::ostream &os,
                                                  const CallCapture &call,
                                                  gl::TextureID value)
{
    os << "gTextureMap[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TTransformFeedbackID>(std::ostream &os,
                                                            const CallCapture &call,
                                                            gl::TransformFeedbackID value)
{
    os << "gTransformFeedbackMap[" << value.value << "]";
}

template <>
void WriteParamValueReplay<ParamType::TVertexArrayID>(std::ostream &os,
                                                      const CallCapture &call,
                                                      gl::VertexArrayID value)
{
    os << "gVertexArrayMap[" << value.value << "]";
}

bool FindShaderProgramIDInCall(const CallCapture &call, gl::ShaderProgramID *idOut)
{
    for (const ParamCapture &param : call.params.getParamCaptures())
    {
        if (param.type == ParamType::TShaderProgramID && param.name == "programPacked")
        {
            *idOut = param.value.ShaderProgramIDVal;
            return true;
        }
    }

    return false;
}

template <>
void WriteParamValueReplay<ParamType::TUniformLocation>(std::ostream &os,
                                                        const CallCapture &call,
                                                        gl::UniformLocation value)
{
    if (value.value == -1)
    {
        os << "-1";
        return;
    }

    os << "gUniformLocations[";

    // Find the program from the call parameters.
    gl::ShaderProgramID programID;
    if (FindShaderProgramIDInCall(call, &programID))
    {
        os << "gShaderProgramMap[" << programID.value << "]";
    }
    else
    {
        os << "gCurrentProgram";
    }

    os << "][" << value.value << "]";
}
}  // namespace angle
