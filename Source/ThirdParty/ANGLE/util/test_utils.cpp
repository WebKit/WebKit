//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// system_utils: Defines common utility functions

#include "util/test_utils.h"

#include <cstring>
#include <fstream>

namespace angle
{
bool CreateTemporaryFile(char *tempFileNameOut, uint32_t maxFileNameLen)
{
    constexpr uint32_t kMaxPath = 1000u;
    char tempPath[kMaxPath];

    if (!GetTempDir(tempPath, kMaxPath))
        return false;

    return CreateTemporaryFileInDir(tempPath, tempFileNameOut, maxFileNameLen);
}

bool GetFileSize(const char *filePath, uint32_t *sizeOut)
{
    std::ifstream stream(filePath);
    if (!stream)
    {
        return false;
    }

    stream.seekg(0, std::ios::end);
    *sizeOut = static_cast<uint32_t>(stream.tellg());
    return true;
}

bool ReadEntireFileToString(const char *filePath, char *contentsOut, uint32_t maxLen)
{
    std::ifstream stream(filePath);
    if (!stream)
    {
        return false;
    }

    std::string contents;

    stream.seekg(0, std::ios::end);
    contents.reserve(static_cast<unsigned int>(stream.tellg()));
    stream.seekg(0, std::ios::beg);

    contents.assign((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

    strncpy(contentsOut, contents.c_str(), maxLen);
    return true;
}

// static
Process::~Process() = default;

ProcessHandle::ProcessHandle() : mProcess(nullptr) {}

ProcessHandle::ProcessHandle(Process *process) : mProcess(process) {}

ProcessHandle::ProcessHandle(const std::vector<const char *> &args,
                             bool captureStdout,
                             bool captureStderr)
    : mProcess(LaunchProcess(args, captureStdout, captureStderr))
{}

ProcessHandle::~ProcessHandle()
{
    reset();
}

ProcessHandle::ProcessHandle(ProcessHandle &&other) : mProcess(other.mProcess)
{
    other.mProcess = nullptr;
}

ProcessHandle &ProcessHandle::operator=(ProcessHandle &&rhs)
{
    std::swap(mProcess, rhs.mProcess);
    return *this;
}

void ProcessHandle::reset()
{
    if (mProcess)
    {
        delete mProcess;
        mProcess = nullptr;
    }
}
}  // namespace angle
