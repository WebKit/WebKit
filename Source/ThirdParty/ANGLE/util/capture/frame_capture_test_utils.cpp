//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// frame_capture_test_utils:
//   Helper functions for capture and replay of traces.
//

#include "frame_capture_test_utils.h"

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

    const rapidjson::Document::Object &meta = doc["TraceMetadata"].GetObject();

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

    return true;
}
}  // namespace angle
