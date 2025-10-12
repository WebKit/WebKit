//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FrameCaptureCommon.cpp:
//   ANGLE Frame capture implementation for both GL and CL.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "libANGLE/capture/FrameCapture.h"

#define USE_SYSTEM_ZLIB
#include "compression_utils_portable.h"

namespace angle
{

std::string GetBinaryDataFilePath(bool compression, const std::string &captureLabel)
{
    std::stringstream fnameStream;
    fnameStream << FmtCapturePrefix(kNoContextId, captureLabel) << ".angledata";
    if (compression)
    {
        fnameStream << ".gz";
    }
    return fnameStream.str();
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

void WriteBinaryParamReplay(ReplayWriter &replayWriter,
                            std::ostream &out,
                            std::ostream &header,
                            const CallCapture &call,
                            const ParamCapture &param,
                            FrameCaptureBinaryData *binaryData)
{
    std::string varName = replayWriter.getInlineVariableName(call.entryPoint, param.name);

    ASSERT(param.data.size() == 1);
    const std::vector<uint8_t> &data = param.data[0];

    // Only inline strings (shaders) to simplify the C code.
    ParamType overrideType = param.type;
    if (param.type == ParamType::TvoidConstPointer)
    {
        overrideType = ParamType::TGLubyteConstPointer;
    }
    if (overrideType == ParamType::TGLcharPointer || overrideType == ParamType::TcharConstPointer)
    {
        // Inline if data is of type string
        std::string paramTypeString = ParamTypeToString(param.type);
        header << paramTypeString.substr(0, paramTypeString.length() - 1) << varName << "[] = { ";
        WriteInlineData<GLchar>(data, header);
        header << " };\n";
        out << varName;
    }
    else
    {
        // Store in binary file if data are not of type string
        // Round up to 16-byte boundary for cross ABI safety
        const size_t offset = binaryData->append(data.data(), data.size());
        out << "(" << ParamTypeToString(overrideType) << ")GetBinaryData(" << offset << ")";
    }
}

void WriteStringPointerParamReplay(ReplayWriter &replayWriter,
                                   std::ostream &out,
                                   std::ostream &header,
                                   const CallCapture &call,
                                   const ParamCapture &param)
{
    // Concatenate the strings to ensure we get an accurate counter
    std::vector<std::string> strings;
    for (const std::vector<uint8_t> &data : param.data)
    {
        // null terminate C style string
        ASSERT(data.size() > 0 && data.back() == '\0');
        strings.emplace_back(data.begin(), data.end() - 1);
    }

    bool isNewEntry     = false;
    std::string varName = replayWriter.getInlineStringSetVariableName(call.entryPoint, param.name,
                                                                      strings, &isNewEntry);

    if (isNewEntry)
    {
        header << "const char *" << (replayWriter.captureAPI == CaptureAPI::CL ? " " : "const ")
               << varName << "[] = { \n";

        for (const std::string &str : strings)
        {
            // Break up long strings for MSVC
            size_t copyLength = 0;
            std::string separator;
            for (size_t i = 0; i < str.length(); i += kStringLengthLimit)
            {
                if ((str.length() - i) <= kStringLengthLimit)
                {
                    copyLength = str.length() - i;
                    separator  = ",";
                }
                else
                {
                    copyLength = kStringLengthLimit;
                    separator  = "";
                }

                header << FmtMultiLineString(str.substr(i, copyLength)) << separator << "\n";
            }
        }

        header << "};\n";
    }

    out << varName;
}

void WriteComment(std::ostream &out, const CallCapture &call)
{
    // Read the string parameter
    const ParamCapture &stringParam =
        call.params.getParam("comment", ParamType::TGLcharConstPointer, 0);
    const std::vector<uint8_t> &data = stringParam.data[0];
    ASSERT(data.size() > 0 && data.back() == '\0');
    std::string str(data.begin(), data.end() - 1);

    // Write the string prefixed with single line comment
    out << "// " << str;
}

std::string EscapeString(const std::string &string)
{
    std::stringstream strstr;

    for (char c : string)
    {
        if (c == '\"' || c == '\\')
        {
            strstr << "\\";
        }
        strstr << c;
    }

    return strstr.str();
}

std::ostream &operator<<(std::ostream &os, gl::ContextID contextId)
{
    os << static_cast<int>(contextId.value);
    return os;
}

std::ostream &operator<<(std::ostream &os, const FmtCapturePrefix &fmt)
{
    if (fmt.captureLabel.empty())
    {
        os << "angle_capture";
    }
    else
    {
        os << fmt.captureLabel;
    }

    if (fmt.contextId == kSharedContextId)
    {
        os << "_shared";
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, FuncUsage usage)
{
    os << "(";
    if (usage != FuncUsage::Call)
    {
        os << "void";
    }
    os << ")";
    return os;
}

std::ostream &operator<<(std::ostream &os, const FmtReplayFunction &fmt)
{
    os << "Replay";

    if (fmt.contextId == kSharedContextId)
    {
        os << "Shared";
    }

    os << "Frame" << fmt.frameIndex;

    if (fmt.partId != kNoPartId)
    {
        os << "Part" << fmt.partId;
    }
    os << fmt.usage;
    return os;
}

std::ostream &operator<<(std::ostream &os, const FmtSetupFunction &fmt)
{
    os << "SetupReplay";

    if (fmt.contextId != kNoContextId)
    {
        os << "Context";
    }

    if (fmt.contextId == kSharedContextId)
    {
        os << "Shared";
    }
    else
    {
        os << fmt.contextId;
    }

    if (fmt.partId != kNoPartId)
    {
        os << "Part" << fmt.partId;
    }
    os << fmt.usage;
    return os;
}

std::ostream &operator<<(std::ostream &os, const FmtSetupFirstFrameFunction &fmt)
{
    os << "SetupFirstFrame()";
    return os;
}

std::ostream &operator<<(std::ostream &os, const FmtSetupInactiveFunction &fmt)
{
    if ((fmt.usage == FuncUsage::Call) && (fmt.partId == kNoPartId))
    {
        os << "if (gReplayResourceMode == angle::ReplayResourceMode::All)\n    {\n        ";
    }
    os << "SetupReplay";

    if (fmt.contextId != kNoContextId)
    {
        os << "Context";
    }

    if (fmt.contextId == kSharedContextId)
    {
        os << "Shared";
    }
    else
    {
        os << fmt.contextId;
    }

    os << "Inactive";

    if (fmt.partId != kNoPartId)
    {
        os << "Part" << fmt.partId;
    }

    os << fmt.usage;

    if ((fmt.usage == FuncUsage::Call) && (fmt.partId == kNoPartId))
    {
        os << ";\n    }";
    }
    return os;
}

std::ostream &operator<<(std::ostream &os, const FmtResetFunction &fmt)
{
    os << "ResetReplayContext";

    if (fmt.contextId == kSharedContextId)
    {
        os << "Shared";
    }
    else
    {
        os << fmt.contextId;
    }

    if (fmt.partId != kNoPartId)
    {
        os << "Part" << fmt.partId;
    }
    os << fmt.usage;
    return os;
}

std::ostream &operator<<(std::ostream &os, const FmtFunction &fmt)
{
    switch (fmt.funcType)
    {
        case ReplayFunc::Replay:
            os << FmtReplayFunction(fmt.contextId, fmt.usage, fmt.frameIndex, fmt.partId);
            break;

        case ReplayFunc::Setup:
            os << FmtSetupFunction(fmt.partId, fmt.contextId, fmt.usage);
            break;

        case ReplayFunc::SetupInactive:
            os << FmtSetupInactiveFunction(fmt.partId, fmt.contextId, fmt.usage);
            break;

        case ReplayFunc::Reset:
            os << FmtResetFunction(fmt.partId, fmt.contextId, fmt.usage);
            break;

        case ReplayFunc::SetupFirstFrame:
            os << FmtSetupFirstFrameFunction(fmt.partId);
            break;

        default:
            UNREACHABLE();
            break;
    }

    return os;
}

std::ostream &operator<<(std::ostream &ostr, const FmtMultiLineString &fmt)
{
    ASSERT(!fmt.strings.empty());
    bool first = true;
    for (const std::string &string : fmt.strings)
    {
        if (first)
        {
            first = false;
        }
        else
        {
            ostr << "\\n\"\n";
        }

        ostr << "\"" << EscapeString(string);
    }

    ostr << "\"";

    return ostr;
}

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

    constexpr char kAndroidOutputSubdir[] = "/angle_capture/";
    path += std::string(applicationId) + kAndroidOutputSubdir;

    // Check for existence of output path
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

FrameCapture::FrameCapture()  = default;
FrameCapture::~FrameCapture() = default;

void FrameCapture::reset()
{
    mSetupCalls.clear();
}

FrameCaptureShared::FrameCaptureShared()
    : mEnabled(true),
      mSerializeStateEnabled(false),
      mCompression(true),
      mClientVertexArrayMap{},
      mFrameIndex(1),
      mCaptureStartFrame(1),
      mCaptureEndFrame(0),
      mClientArraySizes{},
      mReadBufferSize(0),
      mResourceIDBufferSize(0),
      mHasResourceType{},
      mResourceIDToSetupCalls{},
      mMaxAccessedResourceIDs{},
      mCaptureTrigger(0),
      mEndCapture(0),
      mCaptureActive(false),
      mWindowSurfaceContextID({0})
{
    reset();

    std::string enabledFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kEnabledVarName, kAndroidEnabled);
    if (enabledFromEnv == "0")
    {
        mEnabled = false;
    }

    std::string startFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kFrameStartVarName, kAndroidFrameStart);
    if (!startFromEnv.empty())
    {
        mCaptureStartFrame = atoi(startFromEnv.c_str());
    }
    if (mCaptureStartFrame < 1)
    {
        WARN() << "Cannot use a capture start frame less than 1.";
        mCaptureStartFrame = 1;
    }

    std::string endFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kFrameEndVarName, kAndroidFrameEnd);
    if (!endFromEnv.empty())
    {
        mCaptureEndFrame = atoi(endFromEnv.c_str());
    }

    std::string binaryDataSizeFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kBinaryDataSizeVarName, kAndroidBinaryDataSize);
    if (!binaryDataSizeFromEnv.empty())
    {
        mBinaryData.setBinaryDataSize(atoi(binaryDataSizeFromEnv.c_str()));
    }

    std::string blockSizeFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kBlockSizeVarName, kAndroidBlockSize);
    if (!blockSizeFromEnv.empty())
    {
        mBinaryData.setBlockSize(atoi(blockSizeFromEnv.c_str()));
    }

    std::string captureTriggerFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kTriggerVarName, kAndroidTrigger);
    if (!captureTriggerFromEnv.empty())
    {
        mCaptureTrigger = atoi(captureTriggerFromEnv.c_str());

        // Using capture trigger, initialize frame range variables for MEC
        resetCaptureStartEndFrames();
    }

    std::string endCaptureFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kEndCaptureVarName, kAndroidEndCapture);
    if (!endCaptureFromEnv.empty())
    {
        mEndCapture      = atoi(endCaptureFromEnv.c_str());
        mCaptureEndFrame = std::numeric_limits<uint32_t>::max();
    }

    std::string labelFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kCaptureLabelVarName, kAndroidCaptureLabel);
    // --angle-per-test-capture-label sets the env var, not properties
    if (labelFromEnv.empty())
    {
        labelFromEnv = GetEnvironmentVar(kCaptureLabelVarName);
    }
    if (!labelFromEnv.empty())
    {
        // Optional label to provide unique file names and namespaces
        mCaptureLabel = labelFromEnv;
    }

    std::string compressionFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kCompressionVarName, kAndroidCompression);
    if (compressionFromEnv == "0")
    {
        mCompression = false;
    }
    std::string serializeStateFromEnv = angle::GetEnvironmentVar(kSerializeStateVarName);
    if (serializeStateFromEnv == "1")
    {
        mSerializeStateEnabled = true;
    }

    std::string validateSerialiedStateFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kValidationVarName, kAndroidValidation);
    if (validateSerialiedStateFromEnv == "1")
    {
        mValidateSerializedState = true;
    }

    mValidationExpression =
        GetEnvironmentVarOrUnCachedAndroidProperty(kValidationExprVarName, kAndroidValidationExpr);

    if (!mValidationExpression.empty())
    {
        INFO() << "Validation expression is " << kValidationExprVarName;
    }

    // TODO: Remove. http://anglebug.com/42266223
    std::string sourceExtFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kSourceExtVarName, kAndroidSourceExt);
    if (!sourceExtFromEnv.empty())
    {
        if (sourceExtFromEnv == "c" || sourceExtFromEnv == "cpp")
        {
            mReplayWriter.setSourceFileExtension(sourceExtFromEnv.c_str());
        }
        else
        {
            WARN() << "Invalid capture source extension: " << sourceExtFromEnv;
        }
    }

    std::string sourceSizeFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kSourceSizeVarName, kAndroidSourceSize);
    if (!sourceSizeFromEnv.empty())
    {
        int sourceSize = atoi(sourceSizeFromEnv.c_str());
        if (sourceSize < 0)
        {
            WARN() << "Invalid capture source size: " << sourceSize;
        }
        else
        {
            mReplayWriter.setSourceFileSizeThreshold(sourceSize);
        }
    }

    std::string forceShadowFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kForceShadowVarName, kAndroidForceShadow);
    if (forceShadowFromEnv == "1")
    {
        INFO() << "Force enabling shadow memory for coherent buffer tracking.";
        mCoherentBufferTracker.enableShadowMemory();
    }

    if (mFrameIndex == mCaptureStartFrame)
    {
        // Capture is starting from the first frame, so set the capture active to ensure all GLES
        // commands issued are handled correctly by maybeCapturePreCallUpdates() and
        // maybeCapturePostCallUpdates().
        setCaptureActive();
    }

    if (mCaptureEndFrame < mCaptureStartFrame)
    {
        // If we're still in a situation where start frame is after end frame,
        // capture cannot happen. Consider this a disabled state.
        // Note: We won't get here if trigger is in use, as it sets them equal but huge.
        mEnabled = false;
    }

    // Special case the output directory
    if (mEnabled)
    {
        // Only perform output directory checks if enabled
        // - This can avoid some expensive process name and filesystem checks
        // - We want to emit errors if the directory doesn't exist
        getOutputDirectory();
    }

    mMaxCLParamsSize[ParamType::Tcl_device_idPointer]   = 0;
    mMaxCLParamsSize[ParamType::Tcl_context]            = 0;
    mMaxCLParamsSize[ParamType::Tcl_platform_idPointer] = 0;
    mMaxCLParamsSize[ParamType::Tcl_command_queue]      = 0;
    mMaxCLParamsSize[ParamType::Tcl_program]            = 0;
    mMaxCLParamsSize[ParamType::Tcl_kernel]             = 0;
    mMaxCLParamsSize[ParamType::Tcl_mem]                = 0;
    mMaxCLParamsSize[ParamType::Tcl_eventPointer]       = 0;
    mMaxCLParamsSize[ParamType::Tcl_sampler]            = 0;
    mMaxCLParamsSize[ParamType::TvoidPointer]           = 0;
}

FrameCaptureShared::~FrameCaptureShared() {}

bool FrameCaptureShared::isCapturing() const
{
    // Currently we will always do a capture up until the last frame. In the future we could improve
    // mid execution capture by only capturing between the start and end frames. The only necessary
    // reason we need to capture before the start is for attached program and shader sources.
    return mEnabled;
}

uint32_t FrameCaptureShared::getFrameCount() const
{
    return mCaptureEndFrame - mCaptureStartFrame + 1;
}

uint32_t FrameCaptureShared::getReplayFrameIndex() const
{
    return mFrameIndex - mCaptureStartFrame + 1;
}

bool FrameCaptureShared::checkForCaptureEnd()
{
    if (mEndCapture == 0)
    {
        return false;
    }

    std::string captureEndStr = GetEndCapture();
    if (captureEndStr.empty())
    {
        return false;
    }

    uint32_t captureEnd = atoi(captureEndStr.c_str());
    if ((mEndCapture > 0) && (captureEnd == 0))
    {
        mCaptureEndFrame = mFrameIndex;
        mEndCapture      = 0;
        return true;
    }
    return false;
}

bool FrameCaptureShared::isRuntimeEnabled()
{
    if (!mRuntimeEnabled && mRuntimeInitialized)
    {
        return false;
    }
    if (mRuntimeEnabled)
    {
        return true;
    }

    uint32_t mCaptureStartFrame = 1;
    uint32_t mCaptureEndFrame   = 0;
    std::string enabledFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kEnabledVarName, kAndroidEnabled);

    std::string startFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kFrameStartVarName, kAndroidFrameStart);
    if (!startFromEnv.empty())
    {
        mCaptureStartFrame = atoi(startFromEnv.c_str());
    }
    if (mCaptureStartFrame < 1)
    {
        mCaptureStartFrame = 1;
    }

    std::string endFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kFrameEndVarName, kAndroidFrameEnd);
    if (!endFromEnv.empty())
    {
        mCaptureEndFrame = atoi(endFromEnv.c_str());
    }

    uint32_t mCaptureTrigger = 0;
    std::string captureTriggerFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kTriggerVarName, kAndroidTrigger);
    if (!captureTriggerFromEnv.empty())
    {
        mCaptureTrigger = atoi(captureTriggerFromEnv.c_str());
    }

    uint32_t mEndCapture = 0;
    std::string endCaptureFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kEndCaptureVarName, kAndroidEndCapture);
    if (!endCaptureFromEnv.empty())
    {
        mEndCapture = atoi(endCaptureFromEnv.c_str());
    }

    mRuntimeEnabled = enabledFromEnv != "0" &&
                      (mCaptureTrigger || mEndCapture ||
                       (mCaptureEndFrame != 0 && mCaptureEndFrame >= mCaptureStartFrame));

    mRuntimeInitialized = true;
    return mRuntimeEnabled;
}

void FrameCaptureShared::reset()
{
    mFrameCalls.clear();
    mClientVertexArrayMap.fill(-1);

    // Do not reset replay-specific settings like the maximum read buffer size, client array sizes,
    // or the 'has seen' type map. We could refine this into per-frame and per-capture maximums if
    // necessary.
}

// This function will clear FrameCaptureShared state so that mid-execution capture can be
// run multiple times.
void FrameCaptureShared::resetMidExecutionCapture(gl::Context *context)
{
    for (ResourceIDType resourceID : AllEnums<ResourceIDType>())
    {
        mResourceIDToSetupCalls[resourceID].clear();
    }

    egl::ShareGroup *shareGroup = context->getShareGroup();
    for (auto shareContext : shareGroup->getContexts())
    {
        FrameCapture *frameCapture = shareContext.second->getFrameCapture();
        frameCapture->reset();
        frameCapture->getStateResetHelper().reset();
    }

    mActiveFrameIndices.clear();
    mWroteIndexFile = false;
    std::fill(std::begin(mClientArraySizes), std::end(mClientArraySizes), 0);
    mReadBufferSize       = 0;
    mResourceIDBufferSize = 0;
    mHasResourceType.Zero();
    mBufferDataMap.clear();
    mMaxAccessedResourceIDs.fill(0);
    mResourceTracker.resetResourceTracking();
    mReplayWriter.reset();
    mShareGroupSetupCalls.clear();
    mDeferredLinkPrograms.clear();
    mActiveContexts.clear();
}

// ReplayWriter implementation.
ReplayWriter::ReplayWriter()
    : mSourceFileExtension(kDefaultSourceFileExt),
      mSourceFileSizeThreshold(kDefaultSourceFileSizeThreshold),
      mFrameIndex(1)
{}

ReplayWriter::~ReplayWriter()
{
    ASSERT(mPrivateFunctionPrototypes.empty());
    ASSERT(mPublicFunctionPrototypes.empty());
    ASSERT(mPrivateFunctions.empty());
    ASSERT(mPublicFunctions.empty());
    ASSERT(mGlobalVariableDeclarations.empty());
    ASSERT(mStaticVariableDeclarations.empty());
    ASSERT(mReplayHeaders.empty());
}

void ReplayWriter::setSourceFileExtension(const char *ext)
{
    mSourceFileExtension = ext;
}

void ReplayWriter::setSourceFileSizeThreshold(size_t sourceFileSizeThreshold)
{
    mSourceFileSizeThreshold = sourceFileSizeThreshold;
}

void ReplayWriter::setFilenamePattern(const std::string &pattern)
{
    if (mFilenamePattern != pattern)
    {
        mFilenamePattern = pattern;
    }
}

void ReplayWriter::setSourcePrologue(const std::string &prologue)
{
    mSourcePrologue = prologue;
}

void ReplayWriter::setHeaderPrologue(const std::string &prologue)
{
    mHeaderPrologue = prologue;
}

void ReplayWriter::addPublicFunction(const std::string &functionProto,
                                     const std::stringstream &headerStream,
                                     const std::stringstream &bodyStream)
{
    mPublicFunctionPrototypes.push_back(functionProto);

    std::string header = headerStream.str();
    std::string body   = bodyStream.str();

    if (!header.empty())
    {
        mReplayHeaders.emplace_back(header);
    }

    if (!body.empty())
    {
        mPublicFunctions.emplace_back(body);
    }
}

void ReplayWriter::addPrivateFunction(const std::string &functionProto,
                                      const std::stringstream &headerStream,
                                      const std::stringstream &bodyStream)
{
    mPrivateFunctionPrototypes.push_back(functionProto);

    std::string header = headerStream.str();
    std::string body   = bodyStream.str();

    if (!header.empty())
    {
        mReplayHeaders.emplace_back(header);
    }

    if (!body.empty())
    {
        mPrivateFunctions.emplace_back(body);
    }
}

std::string ReplayWriter::getInlineVariableName(EntryPoint entryPoint, const std::string &paramName)
{
    int counter = mDataTracker.getCounters().getAndIncrement(entryPoint, paramName);
    return GetVarName(entryPoint, paramName, counter);
}

int DataCounters::getAndIncrement(EntryPoint entryPoint, const std::string &paramName)
{
    Counter counterKey = {entryPoint, paramName};
    return mData[counterKey]++;
}

int StringCounters::getStringCounter(const std::vector<std::string> &strings)
{
    const auto &id = mStringCounterMap.find(strings);
    if (id == mStringCounterMap.end())
    {
        return kStringsNotFound;
    }
    else
    {
        return mStringCounterMap[strings];
    }
}

void StringCounters::setStringCounter(const std::vector<std::string> &strings, int &counter)
{
    ASSERT(counter >= 0);
    mStringCounterMap[strings] = counter;
}

StringCounters::StringCounters() = default;

StringCounters::~StringCounters() = default;

DataCounters::DataCounters() = default;

DataCounters::~DataCounters() = default;

DataTracker::DataTracker() = default;

DataTracker::~DataTracker() = default;

std::string ReplayWriter::getInlineStringSetVariableName(EntryPoint entryPoint,
                                                         const std::string &paramName,
                                                         const std::vector<std::string> &strings,
                                                         bool *isNewEntryOut)
{
    int counter    = mDataTracker.getStringCounters().getStringCounter(strings);
    *isNewEntryOut = (counter == kStringsNotFound);
    if (*isNewEntryOut)
    {
        // This is a unique set of strings, so set up their declaration and update the counter
        counter = mDataTracker.getCounters().getAndIncrement(entryPoint, paramName);
        mDataTracker.getStringCounters().setStringCounter(strings, counter);

        std::string varName = GetVarName(entryPoint, paramName, counter);

        std::stringstream declStream;
        declStream << "const char *" << (captureAPI == CaptureAPI::CL ? " " : "const ") << varName
                   << "[]";
        std::string decl = declStream.str();

        mGlobalVariableDeclarations.push_back(decl);

        return varName;
    }
    else
    {
        return GetVarName(entryPoint, paramName, counter);
    }
}

void ReplayWriter::addStaticVariable(const std::string &customVarType,
                                     const std::string &customVarName)
{
    std::string decl = customVarType + " " + customVarName;
    mStaticVariableDeclarations.push_back(decl);
}

size_t ReplayWriter::getStoredReplaySourceSize() const
{
    size_t sum = 0;
    for (const std::string &header : mReplayHeaders)
    {
        sum += header.size();
    }
    for (const std::string &publicFunc : mPublicFunctions)
    {
        sum += publicFunc.size();
    }
    for (const std::string &privateFunc : mPrivateFunctions)
    {
        sum += privateFunc.size();
    }
    return sum;
}

// static
std::string ReplayWriter::GetVarName(EntryPoint entryPoint,
                                     const std::string &paramName,
                                     int counter)
{
    std::stringstream strstr;
    strstr << GetEntryPointName(entryPoint) << "_" << paramName << "_" << counter;
    return strstr.str();
}

void ReplayWriter::saveFrame()
{
    if (mReplayHeaders.empty() && mPublicFunctions.empty() && mPrivateFunctions.empty())
    {
        return;
    }

    ASSERT(!mSourceFileExtension.empty());

    std::stringstream strstr;
    strstr << mFilenamePattern << "_" << std::setfill('0') << std::setw(4) << mFrameIndex << "."
           << mSourceFileExtension;

    std::string frameFilePath = strstr.str();

    if (captureAPI == CaptureAPI::GL)
    {
        ++mFrameIndex;
    }

    writeReplaySource(frameFilePath);
}

void ReplayWriter::saveFrameIfFull()
{
    if (getStoredReplaySourceSize() < mSourceFileSizeThreshold)
    {
        INFO() << "Merging captured frame: " << getStoredReplaySourceSize()
               << " less than threshold of " << mSourceFileSizeThreshold << " bytes";
        return;
    }

    saveFrame();
}

void ReplayWriter::saveHeader()
{
    std::stringstream headerPathStream;
    headerPathStream << mFilenamePattern << ".h";
    std::string headerPath = headerPathStream.str();

    SaveFileHelper saveH(headerPath);

    saveH << mHeaderPrologue << "\n";

    saveH << "// Public functions are declared in "
          << (captureAPI == CaptureAPI::GL ? "trace_fixture.h.\n" : "trace_fixture_cl.h.\n");
    saveH << "\n";
    saveH << "// Private Functions\n";
    saveH << "\n";

    for (const std::string &proto : mPrivateFunctionPrototypes)
    {
        saveH << proto << ";\n";
    }

    saveH << "\n";
    saveH << "// Global variables\n";
    saveH << "\n";

    for (const std::string &globalVar : mGlobalVariableDeclarations)
    {
        saveH << "extern " << globalVar << ";\n";
    }

    for (const std::string &staticVar : mStaticVariableDeclarations)
    {
        saveH << "static " << staticVar << ";\n";
    }

    mPublicFunctionPrototypes.clear();
    mPrivateFunctionPrototypes.clear();
    mGlobalVariableDeclarations.clear();
    mStaticVariableDeclarations.clear();

    addWrittenFile(headerPath);
}

void ReplayWriter::saveIndexFilesAndHeader()
{
    ASSERT(!mSourceFileExtension.empty());

    std::stringstream sourcePathStream;
    sourcePathStream << mFilenamePattern << "." << mSourceFileExtension;
    std::string sourcePath = sourcePathStream.str();

    writeReplaySource(sourcePath);
    saveHeader();
}

void ReplayWriter::saveSetupFile()
{
    ASSERT(!mSourceFileExtension.empty());

    std::stringstream strstr;
    strstr << mFilenamePattern << "." << mSourceFileExtension;

    std::string frameFilePath = strstr.str();

    writeReplaySource(frameFilePath);
}

void ReplayWriter::writeReplaySource(const std::string &filename)
{
    SaveFileHelper saveCpp(filename);

    saveCpp << mSourcePrologue << "\n";
    for (const std::string &header : mReplayHeaders)
    {
        saveCpp << header << "\n";
    }

    saveCpp << "// Private Functions\n";
    saveCpp << "\n";

    for (const std::string &func : mPrivateFunctions)
    {
        saveCpp << func << "\n";
    }

    saveCpp << "// Public Functions\n";
    saveCpp << "\n";

    if (mFilenamePattern == "cpp")
    {
        saveCpp << "extern \"C\"\n";
        saveCpp << "{\n";
    }

    for (const std::string &func : mPublicFunctions)
    {
        saveCpp << func << "\n";
    }

    if (mFilenamePattern == "cpp")
    {
        saveCpp << "}  // extern \"C\"\n";
    }

    mReplayHeaders.clear();
    mPrivateFunctions.clear();
    mPublicFunctions.clear();

    addWrittenFile(filename);
}

std::string GetBaseName(const std::string &nameWithPath)
{
    std::vector<std::string> result = angle::SplitString(
        nameWithPath, "/\\", WhitespaceHandling::TRIM_WHITESPACE, SplitResult::SPLIT_WANT_NONEMPTY);
    ASSERT(!result.empty());
    return result.back();
}

void ReplayWriter::addWrittenFile(const std::string &filename)
{
    std::string writtenFile = GetBaseName(filename);
    ASSERT(std::find(mWrittenFiles.begin(), mWrittenFiles.end(), writtenFile) ==
           mWrittenFiles.end());
    mWrittenFiles.push_back(writtenFile);
}

std::vector<std::string> ReplayWriter::getAndResetWrittenFiles()
{
    std::vector<std::string> results = std::move(mWrittenFiles);
    std::sort(results.begin(), results.end());
    ASSERT(mWrittenFiles.empty());
    return results;
}

void AddComment(std::vector<CallCapture> *outCalls, const std::string &comment)
{
    ParamBuffer commentParamBuffer;
    ParamCapture commentParam("comment", ParamType::TGLcharConstPointer);
    CaptureString(comment.c_str(), &commentParam);
    commentParamBuffer.addParam(std::move(commentParam));
    outCalls->emplace_back("Comment", std::move(commentParamBuffer));
}

bool FrameCaptureShared::mRuntimeEnabled     = false;
bool FrameCaptureShared::mRuntimeInitialized = false;

void FrameCaptureShared::getOutputDirectory()
{
    std::string pathFromEnv =
        GetEnvironmentVarOrUnCachedAndroidProperty(kOutDirectoryVarName, kAndroidOutDir);
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

std::string GetEndCapture()
{
    // Use the GetAndSet variant to improve future lookup times
    return GetAndSetEnvironmentVarOrUnCachedAndroidProperty(kEndCaptureVarName, kAndroidEndCapture);
}

TrackedResource::TrackedResource() = default;

TrackedResource::~TrackedResource() = default;

ResourceTracker::ResourceTracker() = default;

ResourceTracker::~ResourceTracker() = default;

StateResetHelper::StateResetHelper() = default;

StateResetHelper::~StateResetHelper() = default;

CoherentBufferTracker::CoherentBufferTracker()
    : mEnabled(false), mHasBeenReset(false), mShadowMemoryEnabled(false)
{
    mPageSize = GetPageSize();
}

CoherentBufferTracker::~CoherentBufferTracker()
{
    disable();
}

void CoherentBufferTracker::disable()
{
    if (!mEnabled)
    {
        return;
    }

    if (mPageFaultHandler->disable())
    {
        mEnabled = false;
    }
    else
    {
        ERR() << "Could not disable page fault handler.";
    }

    if (mShadowMemoryEnabled && mBuffers.size() > 0)
    {
        WARN() << "Disabling coherent buffer tracking while leaving shadow memory without "
                  "synchronization. Expect rendering artifacts after capture ends.";
    }
}

}  // namespace angle
