//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// trace_interpreter.h:
//   Parser and interpreter for the C-based replays.
//

#ifndef ANGLE_TRACE_INTERPRETER_H_
#define ANGLE_TRACE_INTERPRETER_H_

#include "common/frame_capture_utils.h"
#include "frame_capture_test_utils.h"

namespace angle
{
struct TraceString
{
    std::vector<std::string> strings;
    std::vector<const char *> pointers;
};
using TraceStringMap = std::map<std::string, TraceString>;

class TraceInterpreter : public TraceReplayInterface
{
  public:
    TraceInterpreter(const TraceInfo &traceInfo, const char *testDataDir, bool verboseLogging);
    ~TraceInterpreter() override;

    bool valid() const override;
    void setBinaryDataDir(const char *dataDir) override;
    void setBinaryDataDecompressCallback(DecompressCallback decompressCallback,
                                         DeleteCallback deleteCallback) override;
    void replayFrame(uint32_t frameIndex) override;
    void setupReplay() override;
    void resetReplay() override;
    void finishReplay() override;
    const char *getSerializedContextState(uint32_t frameIndex) override;
    void setValidateSerializedStateCallback(ValidateSerializedStateCallback callback) override;

  private:
    void runTraceFunction(const char *name) const;

    const TraceInfo &mTraceInfo;
    const char *mTestDataDir;
    TraceFunctionMap mTraceFunctions;
    TraceStringMap mTraceStrings;
    bool mVerboseLogging;
};

constexpr size_t kMaxTokenSize  = 200;
constexpr size_t kMaxParameters = 32;
using Token                     = char[kMaxTokenSize];

CallCapture ParseCallCapture(const Token &nameToken,
                             size_t numParamTokens,
                             const Token *paramTokens,
                             const TraceStringMap &strings);

template <typename T>
void PackParameter(ParamBuffer &params, const Token &token, const TraceStringMap &strings);

template <>
void PackParameter<uint32_t>(ParamBuffer &params,
                             const Token &token,
                             const TraceStringMap &strings);

template <>
void PackParameter<int32_t>(ParamBuffer &params, const Token &token, const TraceStringMap &strings);

template <>
void PackParameter<void *>(ParamBuffer &params, const Token &token, const TraceStringMap &strings);

template <>
void PackParameter<const int32_t *>(ParamBuffer &params,
                                    const Token &token,
                                    const TraceStringMap &strings);

template <>
void PackParameter<void **>(ParamBuffer &params, const Token &token, const TraceStringMap &strings);

template <>
void PackParameter<int32_t *>(ParamBuffer &params,
                              const Token &token,
                              const TraceStringMap &strings);

template <>
void PackParameter<uint64_t>(ParamBuffer &params,
                             const Token &token,
                             const TraceStringMap &strings);

template <>
void PackParameter<int64_t>(ParamBuffer &params, const Token &token, const TraceStringMap &strings);

template <>
void PackParameter<const int64_t *>(ParamBuffer &params,
                                    const Token &token,
                                    const TraceStringMap &strings);

template <>
void PackParameter<int64_t *>(ParamBuffer &params,
                              const Token &token,
                              const TraceStringMap &strings);

template <>
void PackParameter<uint64_t *>(ParamBuffer &params,
                               const Token &token,
                               const TraceStringMap &strings);

template <>
void PackParameter<const char *>(ParamBuffer &params,
                                 const Token &token,
                                 const TraceStringMap &strings);

template <>
void PackParameter<const void *>(ParamBuffer &params,
                                 const Token &token,
                                 const TraceStringMap &strings);

template <>
void PackParameter<uint32_t *>(ParamBuffer &params,
                               const Token &token,
                               const TraceStringMap &strings);

template <>
void PackParameter<const uint32_t *>(ParamBuffer &params,
                                     const Token &token,
                                     const TraceStringMap &strings);

template <>
void PackParameter<float>(ParamBuffer &params, const Token &token, const TraceStringMap &strings);

template <>
void PackParameter<uint8_t>(ParamBuffer &params, const Token &token, const TraceStringMap &strings);

template <>
void PackParameter<float *>(ParamBuffer &params, const Token &token, const TraceStringMap &strings);

template <>
void PackParameter<const float *>(ParamBuffer &params,
                                  const Token &token,
                                  const TraceStringMap &strings);

template <>
void PackParameter<GLsync>(ParamBuffer &params, const Token &token, const TraceStringMap &strings);

template <>
void PackParameter<const char *const *>(ParamBuffer &params,
                                        const Token &token,
                                        const TraceStringMap &strings);

template <>
void PackParameter<const char **>(ParamBuffer &params,
                                  const Token &token,
                                  const TraceStringMap &strings);

template <>
void PackParameter<GLDEBUGPROCKHR>(ParamBuffer &params,
                                   const Token &token,
                                   const TraceStringMap &strings);

template <>
void PackParameter<EGLDEBUGPROCKHR>(ParamBuffer &params,
                                    const Token &token,
                                    const TraceStringMap &strings);

template <>
void PackParameter<const struct AHardwareBuffer *>(ParamBuffer &params,
                                                   const Token &token,
                                                   const TraceStringMap &strings);

template <>
void PackParameter<EGLSetBlobFuncANDROID>(ParamBuffer &params,
                                          const Token &token,
                                          const TraceStringMap &strings);

template <>
void PackParameter<EGLGetBlobFuncANDROID>(ParamBuffer &params,
                                          const Token &token,
                                          const TraceStringMap &strings);

template <>
void PackParameter<int16_t>(ParamBuffer &params, const Token &token, const TraceStringMap &strings);

template <>
void PackParameter<const int16_t *>(ParamBuffer &params,
                                    const Token &token,
                                    const TraceStringMap &strings);

template <>
void PackParameter<char *>(ParamBuffer &params, const Token &token, const TraceStringMap &strings);

template <>
void PackParameter<unsigned char *>(ParamBuffer &params,
                                    const Token &token,
                                    const TraceStringMap &strings);

template <>
void PackParameter<const void *const *>(ParamBuffer &params,
                                        const Token &token,
                                        const TraceStringMap &strings);

template <>
void PackParameter<const uint64_t *>(ParamBuffer &params,
                                     const Token &token,
                                     const TraceStringMap &strings);

#if defined(ANGLE_PLATFORM_WINDOWS)

template <>
void PackParameter<EGLNativeDisplayType>(ParamBuffer &params,
                                         const Token &token,
                                         const TraceStringMap &strings);
#endif  // defined(ANGLE_PLATFORM_WINDOWS)

#if defined(ANGLE_PLATFORM_WINDOWS) || defined(ANGLE_PLATFORM_ANDROID)
template <>
void PackParameter<EGLNativeWindowType>(ParamBuffer &params,
                                        const Token &token,
                                        const TraceStringMap &strings);

template <>
void PackParameter<EGLNativePixmapType>(ParamBuffer &params,
                                        const Token &token,
                                        const TraceStringMap &strings);
#endif  // defined(ANGLE_PLATFORM_WINDOWS) || defined(ANGLE_PLATFORM_ANDROID)

// On Apple platforms, std::is_same<uint64_t, long> is false despite being both 8 bits.
#if defined(ANGLE_PLATFORM_APPLE) || !defined(ANGLE_IS_64_BIT_CPU)
template <>
void PackParameter<const long *>(ParamBuffer &params,
                                 const Token &token,
                                 const TraceStringMap &strings);
template <>
void PackParameter<long *>(ParamBuffer &params, const Token &token, const TraceStringMap &strings);

template <>
void PackParameter<long>(ParamBuffer &params, const Token &token, const TraceStringMap &strings);

template <>
void PackParameter<unsigned long>(ParamBuffer &params,
                                  const Token &token,
                                  const TraceStringMap &strings);
#endif  // defined(ANGLE_PLATFORM_APPLE) || !defined(ANGLE_IS_64_BIT_CPU)

template <typename T>
void PackParameter(ParamBuffer &params, const Token &token, const TraceStringMap &strings)
{
    static_assert(AssertFalse<T>::value, "No specialization for type.");
}

template <typename T>
struct ParameterPacker;

template <typename Ret>
struct ParameterPacker<Ret()>
{
    static void Pack(ParamBuffer &params, const Token *tokens, const TraceStringMap &strings) {}
};

template <typename Ret, typename Arg>
struct ParameterPacker<Ret(Arg)>
{
    static void Pack(ParamBuffer &params, const Token *tokens, const TraceStringMap &strings)
    {
        PackParameter<Arg>(params, tokens[0], strings);
    }
};

template <typename Ret, typename Arg, typename... Args>
struct ParameterPacker<Ret(Arg, Args...)>
{
    static void Pack(ParamBuffer &params, const Token *tokens, const TraceStringMap &strings)
    {
        PackParameter<Arg>(params, tokens[0], strings);
        ParameterPacker<Ret(Args...)>::Pack(params, &tokens[1], strings);
    }
};

template <typename Ret, typename Arg, typename... Args>
struct ParameterPacker<Ret (*)(Arg, Args...)>
{
    static void Pack(ParamBuffer &params, const Token *tokens, const TraceStringMap &strings)
    {
        PackParameter<Arg>(params, tokens[0], strings);
        ParameterPacker<Ret(Args...)>::Pack(params, &tokens[1], strings);
    }
};

template <typename Func>
struct RemoveStdCall;

template <typename Ret, typename... Args>
struct RemoveStdCall<Ret KHRONOS_APIENTRY(Args...)>
{
    using Type = Ret(Args...);
};

template <typename Func>
struct RemoveStdCall
{
    using Type = Func;
};

template <typename Func>
ParamBuffer ParseParameters(const Token *paramTokens, const TraceStringMap &strings)
{
    ParamBuffer params;
    ParameterPacker<typename RemoveStdCall<Func>::Type>::Pack(params, paramTokens, strings);
    return params;
}
}  // namespace angle

#endif  // ANGLE_TRACE_INTERPRETER_H_
