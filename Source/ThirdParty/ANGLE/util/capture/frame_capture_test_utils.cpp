//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// frame_capture_test_utils:
//   Helper functions for capture and replay of traces.
//

#include "frame_capture_test_utils.h"

#include "common/frame_capture_utils.h"
#include "common/string_utils.h"
#include "trace_fixture.h"

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <fstream>

namespace angle
{
namespace
{
bool LoadJSONFromFile(const std::string &fileName, rapidjson::Document *doc)
{
    std::ifstream ifs(fileName);
    if (!ifs.is_open())
    {
        return false;
    }

    rapidjson::IStreamWrapper inWrapper(ifs);
    doc->ParseStream(inWrapper);
    return !doc->HasParseError();
}
}  // namespace

bool LoadTraceNamesFromJSON(const std::string jsonFilePath, std::vector<std::string> *namesOut)
{
    rapidjson::Document doc;
    if (!LoadJSONFromFile(jsonFilePath, &doc))
    {
        return false;
    }

    if (!doc.IsObject() || !doc.HasMember("traces") || !doc["traces"].IsArray())
    {
        return false;
    }

    // Read trace json into a list of trace names.
    std::vector<std::string> traces;

    rapidjson::Document::Array traceArray = doc["traces"].GetArray();
    for (rapidjson::SizeType arrayIndex = 0; arrayIndex < traceArray.Size(); ++arrayIndex)
    {
        const rapidjson::Document::ValueType &arrayElement = traceArray[arrayIndex];

        if (!arrayElement.IsString())
        {
            return false;
        }

        std::vector<std::string> traceAndVersion;
        angle::SplitStringAlongWhitespace(arrayElement.GetString(), &traceAndVersion);
        traces.push_back(traceAndVersion[0]);
    }

    *namesOut = std::move(traces);
    return true;
}

bool LoadTraceInfoFromJSON(const std::string &traceName,
                           const std::string &traceJsonPath,
                           TraceInfo *traceInfoOut)
{
    rapidjson::Document doc;
    if (!LoadJSONFromFile(traceJsonPath, &doc))
    {
        return false;
    }

    if (!doc.IsObject() || !doc.HasMember("TraceMetadata"))
    {
        return false;
    }

    const rapidjson::Document::Object &meta = doc["TraceMetadata"].GetObj();

    strncpy(traceInfoOut->name, traceName.c_str(), kTraceInfoMaxNameLen);
    traceInfoOut->contextClientMajorVersion = meta["ContextClientMajorVersion"].GetInt();
    traceInfoOut->contextClientMinorVersion = meta["ContextClientMinorVersion"].GetInt();
    traceInfoOut->frameEnd                  = meta["FrameEnd"].GetInt();
    traceInfoOut->frameStart                = meta["FrameStart"].GetInt();
    traceInfoOut->drawSurfaceHeight         = meta["DrawSurfaceHeight"].GetInt();
    traceInfoOut->drawSurfaceWidth          = meta["DrawSurfaceWidth"].GetInt();

    angle::HexStringToUInt(meta["DrawSurfaceColorSpace"].GetString(),
                           &traceInfoOut->drawSurfaceColorSpace);
    angle::HexStringToUInt(meta["DisplayPlatformType"].GetString(),
                           &traceInfoOut->displayPlatformType);
    angle::HexStringToUInt(meta["DisplayDeviceType"].GetString(), &traceInfoOut->displayDeviceType);

    traceInfoOut->configRedBits     = meta["ConfigRedBits"].GetInt();
    traceInfoOut->configGreenBits   = meta["ConfigGreenBits"].GetInt();
    traceInfoOut->configBlueBits    = meta["ConfigBlueBits"].GetInt();
    traceInfoOut->configAlphaBits   = meta["ConfigAlphaBits"].GetInt();
    traceInfoOut->configDepthBits   = meta["ConfigDepthBits"].GetInt();
    traceInfoOut->configStencilBits = meta["ConfigStencilBits"].GetInt();

    traceInfoOut->isBinaryDataCompressed = meta["IsBinaryDataCompressed"].GetBool();
    traceInfoOut->areClientArraysEnabled = meta["AreClientArraysEnabled"].GetBool();
    traceInfoOut->isBindGeneratesResourcesEnabled =
        meta["IsBindGeneratesResourcesEnabled"].GetBool();
    traceInfoOut->isWebGLCompatibilityEnabled = meta["IsWebGLCompatibilityEnabled"].GetBool();
    traceInfoOut->isRobustResourceInitEnabled = meta["IsRobustResourceInitEnabled"].GetBool();
    traceInfoOut->windowSurfaceContextId      = doc["WindowSurfaceContextID"].GetInt();

    if (doc.HasMember("RequiredExtensions"))
    {
        const rapidjson::Value &requiredExtensions = doc["RequiredExtensions"];
        if (!requiredExtensions.IsArray())
        {
            return false;
        }
        for (rapidjson::SizeType i = 0; i < requiredExtensions.Size(); i++)
        {
            std::string ext = std::string(requiredExtensions[i].GetString());
            traceInfoOut->requiredExtensions.push_back(ext);
        }
    }

    const rapidjson::Document::Array &traceFiles = doc["TraceFiles"].GetArray();
    for (const rapidjson::Value &value : traceFiles)
    {
        traceInfoOut->traceFiles.push_back(value.GetString());
    }

    traceInfoOut->initialized = true;
    return true;
}

template <typename T>
T GetParamValue(const ParamValue &value);

template <>
inline GLuint GetParamValue<GLuint>(const ParamValue &value)
{
    return value.GLuintVal;
}

template <>
inline GLint GetParamValue<GLint>(const ParamValue &value)
{
    return value.GLintVal;
}

template <>
inline const void *GetParamValue<const void *>(const ParamValue &value)
{
    return value.voidConstPointerVal;
}

template <>
inline GLuint64 GetParamValue<GLuint64>(const ParamValue &value)
{
    return value.GLuint64Val;
}

template <>
inline const char *GetParamValue<const char *>(const ParamValue &value)
{
    return value.GLcharConstPointerVal;
}

#if defined(ANGLE_PLATFORM_APPLE)
template <>
inline unsigned long GetParamValue<unsigned long>(const ParamValue &value)
{
    return static_cast<unsigned long>(value.GLuint64Val);
}
#endif  // defined(ANGLE_PLATFORM_APPLE)

template <typename T>
T GetParamValue(const ParamValue &value)
{
    static_assert(AssertFalse<T>::value, "No specialization for type.");
}

using Captures = std::vector<ParamCapture>;

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
    return GetParamValue<FnArg<Fn, Idx>>(cap[Idx].value);
}

template <typename Fn, EnableIfNArgs<Fn, 1> = 0>
void DispatchCallCapture(Fn *fn, const Captures &cap)
{
    (*fn)(Arg<Fn, 0>(cap));
}

template <typename Fn, EnableIfNArgs<Fn, 2> = 0>
void DispatchCallCapture(Fn *fn, const Captures &cap)
{
    (*fn)(Arg<Fn, 0>(cap), Arg<Fn, 1>(cap));
}

template <typename Fn, EnableIfNArgs<Fn, 3> = 0>
void DispatchCallCapture(Fn *fn, const Captures &cap)
{
    (*fn)(Arg<Fn, 0>(cap), Arg<Fn, 1>(cap), Arg<Fn, 2>(cap));
}

template <typename Fn, EnableIfNArgs<Fn, 4> = 0>
void DispatchCallCapture(Fn *fn, const Captures &cap)
{
    (*fn)(Arg<Fn, 0>(cap), Arg<Fn, 1>(cap), Arg<Fn, 2>(cap), Arg<Fn, 3>(cap));
}

template <typename Fn, EnableIfNArgs<Fn, 20> = 0>
void DispatchCallCapture(Fn *fn, const Captures &cap)
{
    (*fn)(Arg<Fn, 0>(cap), Arg<Fn, 1>(cap), Arg<Fn, 2>(cap), Arg<Fn, 3>(cap), Arg<Fn, 4>(cap),
          Arg<Fn, 5>(cap), Arg<Fn, 6>(cap), Arg<Fn, 7>(cap), Arg<Fn, 8>(cap), Arg<Fn, 9>(cap),
          Arg<Fn, 10>(cap), Arg<Fn, 11>(cap), Arg<Fn, 12>(cap), Arg<Fn, 13>(cap), Arg<Fn, 14>(cap),
          Arg<Fn, 15>(cap), Arg<Fn, 16>(cap), Arg<Fn, 17>(cap), Arg<Fn, 18>(cap), Arg<Fn, 19>(cap));
}

void ReplayTraceFunction(const TraceFunction &func, const TraceFunctionMap &customFunctions)
{
    for (const CallCapture &call : func)
    {
        ReplayTraceFunctionCall(call, customFunctions);
    }
}

void ReplayCustomFunctionCall(const CallCapture &call, const TraceFunctionMap &customFunctions)
{
    ASSERT(call.entryPoint == EntryPoint::Invalid);
    const Captures &captures = call.params.getParamCaptures();

    if (call.customFunctionName == "UpdateClientArrayPointer")
    {
        DispatchCallCapture(UpdateClientArrayPointer, captures);
    }
    else if (call.customFunctionName == "SetTextureID")
    {
        DispatchCallCapture(SetTextureID, captures);
    }
    else if (call.customFunctionName == "UpdateTextureID")
    {
        DispatchCallCapture(UpdateTextureID, captures);
    }
    else if (call.customFunctionName == "InitializeReplay2")
    {
        DispatchCallCapture(InitializeReplay2, captures);
    }
    else if (call.customFunctionName == "UpdateBufferID")
    {
        DispatchCallCapture(UpdateBufferID, captures);
    }
    else if (call.customFunctionName == "UpdateRenderbufferID")
    {
        DispatchCallCapture(UpdateRenderbufferID, captures);
    }
    else if (call.customFunctionName == "CreateProgram")
    {
        DispatchCallCapture(CreateProgram, captures);
    }
    else if (call.customFunctionName == "CreateShader")
    {
        DispatchCallCapture(CreateShader, captures);
    }
    else if (call.customFunctionName == "UpdateUniformLocation")
    {
        DispatchCallCapture(UpdateUniformLocation, captures);
    }
    else if (call.customFunctionName == "UpdateCurrentProgram")
    {
        DispatchCallCapture(UpdateCurrentProgram, captures);
    }
    else if (call.customFunctionName == "UpdateFramebufferID")
    {
        DispatchCallCapture(UpdateFramebufferID, captures);
    }
    else if (call.customFunctionName == "UpdateTransformFeedbackID")
    {
        DispatchCallCapture(UpdateTransformFeedbackID, captures);
    }
    else
    {
        auto iter = customFunctions.find(call.customFunctionName);
        if (iter == customFunctions.end())
        {
            printf("Unknown custom function: %s\n", call.customFunctionName.c_str());
            UNREACHABLE();
        }
        else
        {
            ASSERT(call.params.empty());
            const TraceFunction &customFunc = iter->second;
            for (const CallCapture &customCall : customFunc)
            {
                // printf("replay: %s (%zu args)\n", customCall.name(),
                //        customCall.params.getParamCaptures().size());
                ReplayTraceFunctionCall(customCall, customFunctions);
            }
        }
    }
}
}  // namespace angle
