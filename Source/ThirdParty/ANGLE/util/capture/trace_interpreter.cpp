//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// trace_interpreter.cpp:
//   Parser and interpreter for the C-based replays.
//

#define ANGLE_REPLAY_EXPORT

#include "trace_interpreter.h"

#include "common/gl_enum_utils.h"
#include "common/string_utils.h"
#include "trace_fixture.h"

namespace angle
{
namespace
{
bool ShouldSkipFile(const std::string &file)
{
    // Skip non-C-source files.
    if (file.back() != 'c')
    {
        return true;
    }

    return false;
}

class Parser : angle::NonCopyable
{
  public:
    Parser(const std::string &stream, bool verboseLogging)
        : mStream(stream), mIndex(0), mVerboseLogging(verboseLogging)
    {}

    void getFunctionsAndShaders(TraceFunctionMap &functionsOut, TraceShaderMap &shadersOut)
    {
        parse();
        for (auto &iter : mFunctions)
        {
            std::string name    = iter.first;
            TraceFunction &func = iter.second;
            functionsOut.emplace(std::move(name), std::move(func));
        }
        mFunctions.clear();
        for (auto &iter : mShaders)
        {
            std::string name    = iter.first;
            TraceShader &shader = iter.second;
            shadersOut.emplace(std::move(name), std::move(shader));
        }
        mShaders.clear();
    }

  private:
    ANGLE_INLINE char peek() const { return mStream[mIndex]; }

    ANGLE_INLINE char look(size_t ahead) const { return mStream[mIndex + ahead]; }

    ANGLE_INLINE void advance() { mIndex++; }

    ANGLE_INLINE void advanceTo(char delim)
    {
        while (peek() != delim)
        {
            advance();
        }
    }

    bool check(const char *forString) const
    {
        return mStream.substr(mIndex, strlen(forString)) == forString;
    }

    void skipLine()
    {
        advanceTo('\n');
        skipWhitespace();
    }

    void skipWhitespace()
    {
        while (isspace(peek()))
        {
            advance();
        }
    }

    void skipNonWhitespace()
    {
        while (!isspace(peek()))
        {
            advance();
        }
    }

    // In our simplified trace C, every line that begins with a } either ends a function or a
    // shader. All lines inside the function begin with whitespace. So to find the end of the
    // function we just need to scan for a line beginning with }.
    void skipFunction()
    {
        while (peek() != '}')
        {
            advanceTo('\n');
            advance();
        }
        advance();
        skipWhitespace();
    }

    void readStringAppend(std::string *stringOut, char delim)
    {
        while (peek() != delim)
        {
            if (peek() == '\\')
            {
                advance();
                if (peek() == 'n')
                {
                    *stringOut += '\n';
                }
                else
                {
                    UNREACHABLE();
                }
            }
            else
            {
                *stringOut += peek();
            }
            advance();
        }
    }

    void readToken(Token &token, char delim)
    {
        size_t startIndex = mIndex;
        advanceTo(delim);
        memcpy(token, &mStream[startIndex], mIndex - startIndex);
        token[mIndex - startIndex] = 0;
    }

    void skipCast()
    {
        if (peek() == '(')
        {
            advanceTo(')');
            advance();
        }
    }

    void skipComments()
    {
        while (peek() == '/')
        {
            skipLine();
        }
    }

    void readFunction()
    {
        std::string funcName;
        TraceFunction func;

        // Skip past the "void" return value.
        skipNonWhitespace();
        advance();
        readStringAppend(&funcName, '(');
        if (mVerboseLogging)
        {
            printf("function: %s\n", funcName.c_str());
        }

        // Skip this function because of the switch statements.
        if (funcName == "ReplayFrame")
        {
            skipFunction();
            return;
        }

        skipLine();
        ASSERT(peek() == '{');
        skipLine();
        while (peek() != '}')
        {
            skipComments();

            Token nameToken;
            readToken(nameToken, '(');
            advance();
            ParamBuffer params;
            Token paramTokens[kMaxParameters];
            size_t numParams = 0;
            skipCast();
            size_t tokenStart = mIndex;
            while (peek() != ';')
            {
                // Skip casts.
                if (peek() == ',' || (peek() == ')' && mIndex != tokenStart))
                {
                    ASSERT(numParams <= kMaxParameters);
                    Token &token = paramTokens[numParams++];

                    memcpy(token, &mStream[tokenStart], mIndex - tokenStart);
                    token[mIndex - tokenStart] = 0;
                    advance();
                    skipWhitespace();
                    skipCast();
                    tokenStart = mIndex;
                }
                else
                {
                    advance();
                }
            }

            // Turn on if you want more spam.
            // if (mVerboseLogging)
            //{
            //    printf("call: %s(", nameToken);
            //    for (size_t paramIndex = 0; paramIndex < numParams; ++paramIndex)
            //    {
            //        if (paramIndex > 0)
            //        {
            //            printf(", ");
            //        }
            //        printf("%s", paramTokens[paramIndex]);
            //    }
            //    printf(")\n");
            //}

            // We pass in the shaders for specific use with C string array parameters.
            CallCapture call = ParseCallCapture(nameToken, numParams, paramTokens, mShaders);
            func.push_back(std::move(call));
            skipLine();
        }
        skipLine();

        addFunction(funcName, func);
    }

    void readShader()
    {
        std::string name;
        TraceShader shader;

        while (peek() != 'g')
        {
            advance();
        }
        ASSERT(check("glShaderSource"));

        readStringAppend(&name, '[');
        if (mVerboseLogging)
        {
            printf("shader: %s\n", name.c_str());
        }
        skipLine();
        std::string source;
        while (peek() != '}')
        {
            advance();
            readStringAppend(&source, '\"');
            advance();
            if (peek() == ',')
            {
                shader.sources.push_back(std::move(source));
            }
            skipLine();
        }
        skipLine();

        for (const std::string &src : shader.sources)
        {
            shader.pointers.push_back(src.c_str());
        }

        mShaders[name] = std::move(shader);
    }

    void addFunction(const std::string &funcName, TraceFunction &func)
    {
        // Run initialize immediately so we can load the binary data.
        if (funcName == "InitReplay")
        {
            ReplayTraceFunction(func, {});
            func.clear();
        }
        mFunctions[funcName] = std::move(func);
    }

    void parse()
    {
        while (mIndex < mStream.size())
        {
            if (peek() == '#' || peek() == '/')
            {
                skipLine();
            }
            else if (peek() == 'v')
            {
                ASSERT(check("void "));
                readFunction();
            }
            else
            {
                readShader();
            }
        }
    }

    const std::string &mStream;
    size_t mIndex;
    TraceFunctionMap mFunctions;
    TraceShaderMap mShaders;
    bool mVerboseLogging = false;
};

void PackResourceID(ParamBuffer &params, const Token &token)
{
    ASSERT(token[0] == 'g');
    const char *start = strrchr(token, '[');
    ASSERT(start != nullptr && EndsWith(token, "]"));
    uint32_t value = static_cast<uint32_t>(atoi(start + 1));
    if (BeginsWith(token, "gShaderProgramMap"))
    {
        gl::ShaderProgramID id = {value};
        params.addUnnamedParam(ParamType::TShaderProgramID, id);
    }
    else if (BeginsWith(token, "gBufferMap"))
    {
        gl::BufferID id = {value};
        params.addUnnamedParam(ParamType::TBufferID, id);
    }
    else if (BeginsWith(token, "gTextureMap"))
    {
        gl::TextureID id = {value};
        params.addUnnamedParam(ParamType::TTextureID, id);
    }
    else if (BeginsWith(token, "gRenderbufferMap"))
    {
        gl::RenderbufferID id = {value};
        params.addUnnamedParam(ParamType::TRenderbufferID, id);
    }
    else if (BeginsWith(token, "gFramebufferMap"))
    {
        gl::FramebufferID id = {value};
        params.addUnnamedParam(ParamType::TFramebufferID, id);
    }
    else if (BeginsWith(token, "gTransformFeedbackMap"))
    {
        gl::TransformFeedbackID id = {value};
        params.addUnnamedParam(ParamType::TTransformFeedbackID, id);
    }
    else
    {
        printf("Unknown resource map: %s\n", token);
        UNREACHABLE();
    }
}

template <typename IntT>
void PackIntParameter(ParamBuffer &params, ParamType paramType, const Token &token)
{
    IntT value;

    if (token[0] == 'G')
    {
        ASSERT(BeginsWith(token, "GL_"));
        if (strchr(token, '|') == 0)
        {
            value = static_cast<IntT>(gl::StringToGLenum(token));
        }
        else
        {
            value = static_cast<IntT>(gl::StringToGLbitfield(token));
        }
    }
    else
    {
        ASSERT(isdigit(token[0]));
        value = static_cast<IntT>(atoi(token));
    }

    params.addUnnamedParam(paramType, value);
}

template <typename PointerT>
void PackMemPointer(ParamBuffer &params,
                    ParamType paramType,
                    const char *offsetString,
                    uint8_t *mem)
{
    ASSERT(gBinaryData);
    uint32_t offset = atoi(offsetString);
    params.addUnnamedParam(paramType, reinterpret_cast<PointerT>(&mem[offset]));
}

template <typename T>
void PackMutablePointerParameter(ParamBuffer &params, ParamType paramType, const Token &token)
{
    if (token[0] == '&')
    {
        ASSERT(BeginsWith(token, "&gReadBuffer[") && EndsWith(token, "]"));
        PackMemPointer<T *>(params, paramType, &token[strlen("&gReadBuffer[")], gReadBuffer);
    }
    else if (token[0] == 'g')
    {
        ASSERT(strcmp(token, "gReadBuffer") == 0);
        params.addUnnamedParam(paramType, reinterpret_cast<T *>(gReadBuffer));
    }
    else
    {
        UNREACHABLE();
    }
}

template <typename T>
void PackConstPointerParameter(ParamBuffer &params, ParamType paramType, const Token &token)
{
    // Handle nullptr, the literal "0".
    if (token[0] == '0' && token[1] == 0)
    {
        params.addUnnamedParam(paramType, reinterpret_cast<const T *>(0));
    }
    else if (token[0] == '&')
    {
        ASSERT(BeginsWith(token, "&gBinaryData[") && EndsWith(token, "]"));
        PackMemPointer<const T *>(params, paramType, &token[strlen("&gBinaryData[")], gBinaryData);
    }
    else
    {
        ASSERT(isdigit(token[0]));
        uint32_t offset = atoi(token);
        params.addUnnamedParam(paramType,
                               reinterpret_cast<const T *>(static_cast<uintptr_t>(offset)));
    }
}
}  // anonymous namespace

TraceInterpreter::TraceInterpreter(const TraceInfo &traceInfo,
                                   const char *testDataDir,
                                   bool verboseLogging)
    : mTraceInfo(traceInfo), mTestDataDir(testDataDir), mVerboseLogging(verboseLogging)
{}

TraceInterpreter::~TraceInterpreter() = default;

bool TraceInterpreter::valid() const
{
    return true;
}

void TraceInterpreter::setBinaryDataDir(const char *dataDir)
{
    SetBinaryDataDir(dataDir);
}

void TraceInterpreter::setBinaryDataDecompressCallback(DecompressCallback decompressCallback,
                                                       DeleteCallback deleteCallback)
{
    SetBinaryDataDecompressCallback(decompressCallback, deleteCallback);
}

void TraceInterpreter::replayFrame(uint32_t frameIndex)
{
    char funcName[kMaxTokenSize];
    snprintf(funcName, kMaxTokenSize, "ReplayContext%dFrame%u", mTraceInfo.windowSurfaceContextId,
             frameIndex);
    runTraceFunction(funcName);
}

void TraceInterpreter::setupReplay()
{
    for (const std::string &file : mTraceInfo.traceFiles)
    {
        if (ShouldSkipFile(file))
        {
            if (mVerboseLogging)
            {
                printf("Skipping function parsing for %s.\n", file.c_str());
            }
            continue;
        }

        if (mVerboseLogging)
        {
            printf("Parsing functions from %s\n", file.c_str());
        }
        std::stringstream pathStream;
        pathStream << mTestDataDir << GetPathSeparator() << file;
        std::string path = pathStream.str();

        std::string fileData;
        if (!ReadFileToString(path, &fileData))
        {
            UNREACHABLE();
        }

        Parser parser(fileData, mVerboseLogging);
        parser.getFunctionsAndShaders(mTraceFunctions, mTraceShaders);
    }

    if (mTraceFunctions.count("SetupReplay") == 0)
    {
        printf("Did not find a SetupReplay function to run among %zu parsed functions.",
               mTraceFunctions.size());
        exit(1);
    }

    runTraceFunction("SetupReplay");
}

void TraceInterpreter::resetReplay()
{
    runTraceFunction("ResetReplay");
}

void TraceInterpreter::finishReplay()
{
    FinishReplay();
}

const char *TraceInterpreter::getSerializedContextState(uint32_t frameIndex)
{
    // TODO: Necessary for complete self-testing. http://anglebug.com/7779
    UNREACHABLE();
    return nullptr;
}

void TraceInterpreter::setValidateSerializedStateCallback(ValidateSerializedStateCallback callback)
{
    SetValidateSerializedStateCallback(callback);
}

void TraceInterpreter::runTraceFunction(const char *name) const
{
    auto iter = mTraceFunctions.find(name);
    ASSERT(iter != mTraceFunctions.end());
    const TraceFunction &func = iter->second;
    ReplayTraceFunction(func, mTraceFunctions);
}

template <>
void PackParameter<uint32_t>(ParamBuffer &params, const Token &token, const TraceShaderMap &shaders)
{
    if (token[0] == 'g')
    {
        PackResourceID(params, token);
    }
    else
    {
        PackIntParameter<uint32_t>(params, ParamType::TGLuint, token);
    }
}

template <>
void PackParameter<int32_t>(ParamBuffer &params, const Token &token, const TraceShaderMap &shaders)
{
    if (BeginsWith(token, "gUniformLocations"))
    {
        const char *start = strrchr(token, '[');
        ASSERT(start != nullptr && EndsWith(token, "]"));
        int32_t value           = atoi(start + 1);
        gl::UniformLocation loc = {value};
        params.addUnnamedParam(ParamType::TUniformLocation, loc);
    }
    else
    {
        PackIntParameter<int32_t>(params, ParamType::TGLint, token);
    }
}

template <>
void PackParameter<void *>(ParamBuffer &params, const Token &token, const TraceShaderMap &shaders)
{
    void *value = 0;
    params.addUnnamedParam(ParamType::TvoidPointer, value);
}

template <>
void PackParameter<const int32_t *>(ParamBuffer &params,
                                    const Token &token,
                                    const TraceShaderMap &shaders)
{
    PackConstPointerParameter<int32_t>(params, ParamType::TGLintConstPointer, token);
}

template <>
void PackParameter<void **>(ParamBuffer &params, const Token &token, const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<int32_t *>(ParamBuffer &params,
                              const Token &token,
                              const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<uint64_t>(ParamBuffer &params, const Token &token, const TraceShaderMap &shaders)
{
    params.addUnnamedParam(ParamType::TGLuint64,
                           static_cast<GLuint64>(std::strtoull(token, nullptr, 10)));
}

template <>
void PackParameter<int64_t>(ParamBuffer &params, const Token &token, const TraceShaderMap &shaders)
{
    params.addUnnamedParam(ParamType::TGLint64,
                           static_cast<GLint64>(std::strtoll(token, nullptr, 10)));
}

template <>
void PackParameter<const int64_t *>(ParamBuffer &params,
                                    const Token &token,
                                    const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<int64_t *>(ParamBuffer &params,
                              const Token &token,
                              const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<uint64_t *>(ParamBuffer &params,
                               const Token &token,
                               const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<const char *>(ParamBuffer &params,
                                 const Token &token,
                                 const TraceShaderMap &shaders)
{
    if (token[0] == '"')
    {
        ASSERT(EndsWith(token, "\""));

        ParamCapture param(params.getNextParamName(), ParamType::TGLcharConstPointer);
        std::vector<uint8_t> data(&token[1], &token[strlen(token) - 1]);
        data.push_back(0);
        param.data.push_back(std::move(data));
        param.value.GLcharConstPointerVal = reinterpret_cast<const char *>(param.data[0].data());
        params.addParam(std::move(param));
    }
    else
    {
        PackConstPointerParameter<char>(params, ParamType::TGLcharConstPointer, token);
    }
}

template <>
void PackParameter<const void *>(ParamBuffer &params,
                                 const Token &token,
                                 const TraceShaderMap &shaders)
{
    PackConstPointerParameter<void>(params, ParamType::TvoidConstPointer, token);
}

template <>
void PackParameter<uint32_t *>(ParamBuffer &params,
                               const Token &token,
                               const TraceShaderMap &shaders)
{
    PackMutablePointerParameter<uint32_t>(params, ParamType::TGLuintPointer, token);
}

template <>
void PackParameter<const uint32_t *>(ParamBuffer &params,
                                     const Token &token,
                                     const TraceShaderMap &shaders)
{
    PackConstPointerParameter<uint32_t>(params, ParamType::TGLuintConstPointer, token);
}

template <>
void PackParameter<float>(ParamBuffer &params, const Token &token, const TraceShaderMap &shaders)
{
    params.addUnnamedParam(ParamType::TGLfloat, std::stof(token));
}

template <>
void PackParameter<uint8_t>(ParamBuffer &params, const Token &token, const TraceShaderMap &shaders)
{
    PackIntParameter<uint8_t>(params, ParamType::TGLubyte, token);
}

template <>
void PackParameter<float *>(ParamBuffer &params, const Token &token, const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<const float *>(ParamBuffer &params,
                                  const Token &token,
                                  const TraceShaderMap &shaders)
{
    PackConstPointerParameter<float>(params, ParamType::TGLfloatConstPointer, token);
}

template <>
void PackParameter<GLsync>(ParamBuffer &params, const Token &token, const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<const char *const *>(ParamBuffer &params,
                                        const Token &token,
                                        const TraceShaderMap &shaders)
{
    // Find the shader that corresponds to "token". Currently we only support shader string arrays.
    auto iter = shaders.find(token);
    ASSERT(iter != shaders.end());
    const TraceShader &shader = iter->second;
    params.addUnnamedParam(ParamType::TGLcharConstPointerPointer, shader.pointers.data());
}

template <>
void PackParameter<const char **>(ParamBuffer &params,
                                  const Token &token,
                                  const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<GLDEBUGPROCKHR>(ParamBuffer &params,
                                   const Token &token,
                                   const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<EGLDEBUGPROCKHR>(ParamBuffer &params,
                                    const Token &token,
                                    const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<const struct AHardwareBuffer *>(ParamBuffer &params,
                                                   const Token &token,
                                                   const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<EGLSetBlobFuncANDROID>(ParamBuffer &params,
                                          const Token &token,
                                          const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<EGLGetBlobFuncANDROID>(ParamBuffer &params,
                                          const Token &token,
                                          const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<int16_t>(ParamBuffer &params, const Token &token, const TraceShaderMap &shaders)
{
    PackIntParameter<int16_t>(params, ParamType::TGLshort, token);
}

template <>
void PackParameter<const int16_t *>(ParamBuffer &params,
                                    const Token &token,
                                    const TraceShaderMap &shaders)
{
    PackConstPointerParameter<int16_t>(params, ParamType::TGLshortConstPointer, token);
}

template <>
void PackParameter<char *>(ParamBuffer &params, const Token &token, const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<unsigned char *>(ParamBuffer &params,
                                    const Token &token,
                                    const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<const void *const *>(ParamBuffer &params,
                                        const Token &token,
                                        const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<const uint64_t *>(ParamBuffer &params,
                                     const Token &token,
                                     const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

#if defined(ANGLE_PLATFORM_WINDOWS)
template <>
void PackParameter<EGLNativeDisplayType>(ParamBuffer &params,
                                         const Token &token,
                                         const TraceShaderMap &shaders)
{
    UNREACHABLE();
}
#endif  // defined(ANGLE_PLATFORM_WINDOWS)

#if defined(ANGLE_PLATFORM_WINDOWS) || defined(ANGLE_PLATFORM_ANDROID)
template <>
void PackParameter<EGLNativeWindowType>(ParamBuffer &params,
                                        const Token &token,
                                        const TraceShaderMap &shaders)
{
    UNREACHABLE();
}

template <>
void PackParameter<EGLNativePixmapType>(ParamBuffer &params,
                                        const Token &token,
                                        const TraceShaderMap &shaders)
{
    UNREACHABLE();
}
#endif  // defined(ANGLE_PLATFORM_WINDOWS) || defined(ANGLE_PLATFORM_ANDROID)

#if defined(ANGLE_PLATFORM_APPLE) || !defined(ANGLE_IS_64_BIT_CPU)
template <>
void PackParameter<const long *>(ParamBuffer &params,
                                 const Token &token,
                                 const TraceShaderMap &shaders)
{
    PackConstPointerParameter<int64_t>(params, ParamType::TGLuint64ConstPointer, token);
}

template <>
void PackParameter<long *>(ParamBuffer &params, const Token &token, const TraceShaderMap &shaders)
{
    PackMutablePointerParameter<int64_t>(params, ParamType::TGLint64Pointer, token);
}

template <>
void PackParameter<long>(ParamBuffer &params, const Token &token, const TraceShaderMap &shaders)
{
    PackIntParameter<int64_t>(params, ParamType::TGLint64, token);
}

template <>
void PackParameter<unsigned long>(ParamBuffer &params,
                                  const Token &token,
                                  const TraceShaderMap &shaders)
{
    PackIntParameter<uint64_t>(params, ParamType::TGLuint64, token);
}
#endif  // defined(ANGLE_PLATFORM_APPLE) || !defined(ANGLE_IS_64_BIT_CPU)
}  // namespace angle
