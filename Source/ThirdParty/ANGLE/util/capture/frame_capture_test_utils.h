//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// frame_capture_test_utils:
//   Helper functions for capture and replay of traces.
//

#ifndef UTIL_CAPTURE_FRAME_CAPTURE_TEST_UTILS_H_
#define UTIL_CAPTURE_FRAME_CAPTURE_TEST_UTILS_H_

#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <type_traits>
#include <vector>

#include "common/angleutils.h"
#include "common/debug.h"
#include "common/frame_capture_utils.h"
#include "common/system_utils.h"

#define USE_SYSTEM_ZLIB
#include "compression_utils_portable.h"

#define ANGLE_MACRO_STRINGIZE_AUX(a) #a
#define ANGLE_MACRO_STRINGIZE(a) ANGLE_MACRO_STRINGIZE_AUX(a)
#define ANGLE_MACRO_CONCAT_AUX(a, b) a##b
#define ANGLE_MACRO_CONCAT(a, b) ANGLE_MACRO_CONCAT_AUX(a, b)

namespace angle
{

inline uint8_t *DecompressBinaryData(const std::vector<uint8_t> &compressedData)
{
    uint32_t uncompressedSize =
        zlib_internal::GetGzipUncompressedSize(compressedData.data(), compressedData.size());

    std::unique_ptr<uint8_t[]> uncompressedData(new uint8_t[uncompressedSize]);
    uLong destLen = uncompressedSize;
    int zResult =
        zlib_internal::GzipUncompressHelper(uncompressedData.get(), &destLen, compressedData.data(),
                                            static_cast<uLong>(compressedData.size()));

    if (zResult != Z_OK)
    {
        std::cerr << "Failure to decompressed binary data: " << zResult << "\n";
        return nullptr;
    }

    return uncompressedData.release();
}

inline void DeleteBinaryData(uint8_t *uncompressedData)
{
    delete[] uncompressedData;
}

using DecompressCallback              = uint8_t *(*)(const std::vector<uint8_t> &);
using DeleteCallback                  = void (*)(uint8_t *);
using ValidateSerializedStateCallback = void (*)(const char *, const char *, uint32_t);

using SetBinaryDataDecompressCallbackFunc    = void (*)(DecompressCallback, DeleteCallback);
using SetBinaryDataDirFunc                   = void (*)(const char *);
using SetupReplayFunc                        = void (*)();
using ReplayFrameFunc                        = void (*)(uint32_t);
using ResetReplayFunc                        = void (*)();
using FinishReplayFunc                       = void (*)();
using GetSerializedContextStateFunc          = const char *(*)(uint32_t);
using SetValidateSerializedStateCallbackFunc = void (*)(ValidateSerializedStateCallback);

class TraceReplayInterface : angle::NonCopyable
{
  public:
    virtual ~TraceReplayInterface() {}

    virtual bool valid() const                                                                = 0;
    virtual void setBinaryDataDir(const char *dataDir)                                        = 0;
    virtual void setBinaryDataDecompressCallback(DecompressCallback decompressCallback,
                                                 DeleteCallback deleteCallback)               = 0;
    virtual void replayFrame(uint32_t frameIndex)                                             = 0;
    virtual void setupReplay()                                                                = 0;
    virtual void resetReplay()                                                                = 0;
    virtual void finishReplay()                                                               = 0;
    virtual const char *getSerializedContextState(uint32_t frameIndex)                        = 0;
    virtual void setValidateSerializedStateCallback(ValidateSerializedStateCallback callback) = 0;

  protected:
    TraceReplayInterface() {}
};

class TraceLibrary : public TraceReplayInterface
{
  public:
    TraceLibrary(const char *traceNameIn)
    {
        std::stringstream traceNameStr;
#if !defined(ANGLE_PLATFORM_WINDOWS)
        traceNameStr << "lib";
#endif  // !defined(ANGLE_PLATFORM_WINDOWS)
        traceNameStr << traceNameIn;
#if defined(ANGLE_PLATFORM_ANDROID) && defined(COMPONENT_BUILD)
        // Added to shared library names in Android component builds in
        // https://chromium.googlesource.com/chromium/src/+/9bacc8c4868cc802f69e1e858eea6757217a508f/build/toolchain/toolchain.gni#56
        traceNameStr << ".cr";
#endif  // defined(ANGLE_PLATFORM_ANDROID) && defined(COMPONENT_BUILD)
        std::string traceName = traceNameStr.str();
        mTraceLibrary.reset(OpenSharedLibrary(traceName.c_str(), SearchType::ModuleDir));
    }

    bool valid() const override
    {
        return (mTraceLibrary != nullptr) && (mTraceLibrary->getNative() != nullptr);
    }

    void setBinaryDataDir(const char *dataDir) override
    {
        callFunc<SetBinaryDataDirFunc>("SetBinaryDataDir", dataDir);
    }

    void setBinaryDataDecompressCallback(DecompressCallback decompressCallback,
                                         DeleteCallback deleteCallback) override
    {
        callFunc<SetBinaryDataDecompressCallbackFunc>("SetBinaryDataDecompressCallback",
                                                      decompressCallback, deleteCallback);
    }

    void replayFrame(uint32_t frameIndex) override
    {
        callFunc<ReplayFrameFunc>("ReplayFrame", frameIndex);
    }

    void setupReplay() override { callFunc<SetupReplayFunc>("SetupReplay"); }

    void resetReplay() override { callFunc<ResetReplayFunc>("ResetReplay"); }

    void finishReplay() override { callFunc<FinishReplayFunc>("FinishReplay"); }

    const char *getSerializedContextState(uint32_t frameIndex) override
    {
        return callFunc<GetSerializedContextStateFunc>("GetSerializedContextState", frameIndex);
    }

    void setValidateSerializedStateCallback(ValidateSerializedStateCallback callback) override
    {
        return callFunc<SetValidateSerializedStateCallbackFunc>(
            "SetValidateSerializedStateCallback", callback);
    }

  private:
    template <typename FuncT, typename... ArgsT>
    typename std::invoke_result<FuncT, ArgsT...>::type callFunc(const char *funcName, ArgsT... args)
    {
        void *untypedFunc = mTraceLibrary->getSymbol(funcName);
        if (!untypedFunc)
        {
            fprintf(stderr, "Error loading function: %s\n", funcName);
            ASSERT(untypedFunc);
        }
        auto typedFunc = reinterpret_cast<FuncT>(untypedFunc);
        return typedFunc(args...);
    }

    std::unique_ptr<Library> mTraceLibrary;
};

static constexpr size_t kTraceInfoMaxNameLen = 128;

struct TraceInfo
{
    char name[kTraceInfoMaxNameLen];
    bool initialized = false;
    uint32_t contextClientMajorVersion;
    uint32_t contextClientMinorVersion;
    uint32_t frameStart;
    uint32_t frameEnd;
    uint32_t drawSurfaceWidth;
    uint32_t drawSurfaceHeight;
    uint32_t drawSurfaceColorSpace;
    uint32_t displayPlatformType;
    uint32_t displayDeviceType;
    int configRedBits;
    int configBlueBits;
    int configGreenBits;
    int configAlphaBits;
    int configDepthBits;
    int configStencilBits;
    bool isBinaryDataCompressed;
    bool areClientArraysEnabled;
    bool isBindGeneratesResourcesEnabled;
    bool isWebGLCompatibilityEnabled;
    bool isRobustResourceInitEnabled;
    std::vector<std::string> traceFiles;
    int windowSurfaceContextId;
    std::vector<std::string> requiredExtensions;
};

bool LoadTraceNamesFromJSON(const std::string jsonFilePath, std::vector<std::string> *namesOut);
bool LoadTraceInfoFromJSON(const std::string &traceName,
                           const std::string &traceJsonPath,
                           TraceInfo *traceInfoOut);

using TraceFunction    = std::vector<CallCapture>;
using TraceFunctionMap = std::map<std::string, TraceFunction>;

void ReplayTraceFunction(const TraceFunction &func, const TraceFunctionMap &customFunctions);
void ReplayTraceFunctionCall(const CallCapture &call, const TraceFunctionMap &customFunctions);
void ReplayCustomFunctionCall(const CallCapture &call, const TraceFunctionMap &customFunctions);

template <typename T>
struct AssertFalse : std::false_type
{};

GLuint GetResourceIDMapValue(ResourceIDType resourceIDType, GLuint key);

template <typename T>
T GetParamValue(ParamType type, const ParamValue &value);

template <>
inline GLuint GetParamValue<GLuint>(ParamType type, const ParamValue &value)
{
    ResourceIDType resourceIDType = GetResourceIDTypeFromParamType(type);
    if (resourceIDType == ResourceIDType::InvalidEnum)
    {
        return value.GLuintVal;
    }
    else
    {
        return GetResourceIDMapValue(resourceIDType, value.GLuintVal);
    }
}

template <>
inline GLint GetParamValue<GLint>(ParamType type, const ParamValue &value)
{
    return value.GLintVal;
}

template <>
inline const void *GetParamValue<const void *>(ParamType type, const ParamValue &value)
{
    return value.voidConstPointerVal;
}

template <>
inline GLuint64 GetParamValue<GLuint64>(ParamType type, const ParamValue &value)
{
    return value.GLuint64Val;
}

template <>
inline GLint64 GetParamValue<GLint64>(ParamType type, const ParamValue &value)
{
    return value.GLint64Val;
}

template <>
inline const char *GetParamValue<const char *>(ParamType type, const ParamValue &value)
{
    return value.GLcharConstPointerVal;
}

template <>
inline void *GetParamValue<void *>(ParamType type, const ParamValue &value)
{
    return value.voidPointerVal;
}

#if defined(ANGLE_IS_64_BIT_CPU)
template <>
inline const EGLAttrib *GetParamValue<const EGLAttrib *>(ParamType type, const ParamValue &value)
{
    return value.EGLAttribConstPointerVal;
}
#endif  // defined(ANGLE_IS_64_BIT_CPU)

template <>
inline const EGLint *GetParamValue<const EGLint *>(ParamType type, const ParamValue &value)
{
    return value.EGLintConstPointerVal;
}

template <>
inline const GLchar *const *GetParamValue<const GLchar *const *>(ParamType type,
                                                                 const ParamValue &value)
{
    return value.GLcharConstPointerPointerVal;
}

// On Apple platforms, std::is_same<uint64_t, long> is false despite being both 8 bits.
#if defined(ANGLE_PLATFORM_APPLE) || !defined(ANGLE_IS_64_BIT_CPU)
template <>
inline long GetParamValue<long>(ParamType type, const ParamValue &value)
{
    return static_cast<long>(value.GLint64Val);
}

template <>
inline unsigned long GetParamValue<unsigned long>(ParamType type, const ParamValue &value)
{
    return static_cast<unsigned long>(value.GLuint64Val);
}
#endif  // defined(ANGLE_PLATFORM_APPLE)

template <typename T>
T GetParamValue(ParamType type, const ParamValue &value)
{
    static_assert(AssertFalse<T>::value, "No specialization for type.");
}

template <typename T>
struct Traits;

template <typename... Args>
struct Traits<void(Args...)>
{
    static constexpr size_t NArgs = sizeof...(Args);
    template <size_t Idx>
    struct Arg
    {
        typedef typename std::tuple_element<Idx, std::tuple<Args...>>::type Type;
    };
};

template <typename Fn, size_t Idx>
using FnArg = typename Traits<Fn>::template Arg<Idx>::Type;

template <typename Fn, size_t NArgs>
using EnableIfNArgs = typename std::enable_if_t<Traits<Fn>::NArgs == NArgs, int>;

template <typename Fn, size_t Idx>
FnArg<Fn, Idx> Arg(const Captures &cap)
{
    ASSERT(Idx < cap.size());
    return GetParamValue<FnArg<Fn, Idx>>(cap[Idx].type, cap[Idx].value);
}
}  // namespace angle

#endif  // UTIL_CAPTURE_FRAME_CAPTURE_TEST_UTILS_H_
