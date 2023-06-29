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

    if (meta.HasMember("KeyFrames"))
    {
        const rapidjson::Value &keyFrames = meta["KeyFrames"];
        if (!keyFrames.IsArray())
        {
            return false;
        }
        for (rapidjson::SizeType i = 0; i < keyFrames.Size(); i++)
        {
            int frame = keyFrames[i].GetInt();
            traceInfoOut->keyFrames.push_back(frame);
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

TraceLibrary::TraceLibrary(const std::string &traceName, const TraceInfo &traceInfo)
{
    std::stringstream libNameStr;
    SearchType searchType = SearchType::ModuleDir;

#if defined(ANGLE_TRACE_EXTERNAL_BINARIES)
    // This means we are using the binary build of traces on Android, which are
    // not bundled in the APK, but located in the app's home directory.
    searchType = SearchType::SystemDir;
    libNameStr << "/data/user/0/com.android.angle.test/angle_traces/";
#endif  // defined(ANGLE_TRACE_EXTERNAL_BINARIES)
#if !defined(ANGLE_PLATFORM_WINDOWS)
    libNameStr << "lib";
#endif  // !defined(ANGLE_PLATFORM_WINDOWS)
    libNameStr << traceName;
#if defined(ANGLE_PLATFORM_ANDROID) && defined(COMPONENT_BUILD)
    // Added to shared library names in Android component builds in
    // https://chromium.googlesource.com/chromium/src/+/9bacc8c4868cc802f69e1e858eea6757217a508f/build/toolchain/toolchain.gni#56
    libNameStr << ".cr";
#endif  // defined(ANGLE_PLATFORM_ANDROID) && defined(COMPONENT_BUILD)
    std::string libName = libNameStr.str();
    std::string loadError;
    mTraceLibrary.reset(OpenSharedLibraryAndGetError(libName.c_str(), searchType, &loadError));
    if (mTraceLibrary->getNative() == nullptr)
    {
        FATAL() << "Failed to load trace library (" << libName << "): " << loadError;
    }

    callFunc<SetupEntryPoints>("SetupEntryPoints", static_cast<angle::TraceCallbacks *>(this),
                               &mTraceFunctions);
    mTraceFunctions->SetTraceInfo(traceInfo);
    mTraceInfo = traceInfo;
}

uint8_t *TraceLibrary::LoadBinaryData(const char *fileName)
{
    std::ostringstream pathBuffer;
    pathBuffer << mBinaryDataDir << "/" << fileName;
    FILE *fp = fopen(pathBuffer.str().c_str(), "rb");
    if (fp == 0)
    {
        fprintf(stderr, "Error loading binary data file: %s\n", fileName);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (mTraceInfo.isBinaryDataCompressed)
    {
        if (!strstr(fileName, ".gz"))
        {
            fprintf(stderr, "Filename does not end in .gz");
            exit(1);
        }

        std::vector<uint8_t> compressedData(size);
        (void)fread(compressedData.data(), 1, size, fp);

        uint32_t uncompressedSize =
            zlib_internal::GetGzipUncompressedSize(compressedData.data(), compressedData.size());

        mBinaryData.resize(uncompressedSize + 1);  // +1 to make sure .data() is valid
        uLong destLen = uncompressedSize;
        int zResult =
            zlib_internal::GzipUncompressHelper(mBinaryData.data(), &destLen, compressedData.data(),
                                                static_cast<uLong>(compressedData.size()));

        if (zResult != Z_OK)
        {
            std::cerr << "Failure to decompressed binary data: " << zResult << "\n";
            exit(1);
        }
    }
    else
    {
        if (!strstr(fileName, ".angledata"))
        {
            fprintf(stderr, "Filename does not end in .angledata");
            exit(1);
        }
        mBinaryData.resize(size + 1);
        (void)fread(mBinaryData.data(), 1, size, fp);
    }
    fclose(fp);

    return mBinaryData.data();
}

}  // namespace angle
