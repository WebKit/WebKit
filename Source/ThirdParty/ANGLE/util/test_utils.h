//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// test_utils.h: declaration of OS-specific utility functions

#ifndef UTIL_TEST_UTILS_H_
#define UTIL_TEST_UTILS_H_

#include <functional>
#include <string>
#include <vector>

#include "common/angleutils.h"
#include "util/Timer.h"
#include "util/util_export.h"

namespace angle
{
// Cross platform equivalent of the Windows Sleep function
ANGLE_UTIL_EXPORT void Sleep(unsigned int milliseconds);

ANGLE_UTIL_EXPORT void SetLowPriorityProcess();

// Write a debug message, either to a standard output or Debug window.
ANGLE_UTIL_EXPORT void WriteDebugMessage(const char *format, ...);

// Set thread affinity and priority.
ANGLE_UTIL_EXPORT bool StabilizeCPUForBenchmarking();

// Set a crash handler to print stack traces.
using CrashCallback = std::function<void()>;
ANGLE_UTIL_EXPORT void InitCrashHandler(CrashCallback *callback);
ANGLE_UTIL_EXPORT void TerminateCrashHandler();

// Print a stack back trace.
ANGLE_UTIL_EXPORT void PrintStackBacktrace();

// Get temporary directory.
ANGLE_UTIL_EXPORT bool GetTempDir(char *tempDirOut, uint32_t maxDirNameLen);

// Creates a temporary file. The full path is placed in |path|, and the
// function returns true if was successful in creating the file. The file will
// be empty and all handles closed after this function returns.
ANGLE_UTIL_EXPORT bool CreateTemporaryFile(char *tempFileNameOut, uint32_t maxFileNameLen);

// Same as CreateTemporaryFile but the file is created in |dir|.
ANGLE_UTIL_EXPORT bool CreateTemporaryFileInDir(const char *dir,
                                                char *tempFileNameOut,
                                                uint32_t maxFileNameLen);

// Deletes a file or directory.
ANGLE_UTIL_EXPORT bool DeleteFile(const char *path);

// Reads a file contents into a string.
ANGLE_UTIL_EXPORT bool ReadEntireFileToString(const char *filePath,
                                              char *contentsOut,
                                              uint32_t maxLen);

// Compute a file's size.
ANGLE_UTIL_EXPORT bool GetFileSize(const char *filePath, uint32_t *sizeOut);

class ANGLE_UTIL_EXPORT ProcessHandle;

class ANGLE_UTIL_EXPORT Process : angle::NonCopyable
{
  public:
    virtual bool started()    = 0;
    virtual bool finished()   = 0;
    virtual bool finish()     = 0;
    virtual bool kill()       = 0;
    virtual int getExitCode() = 0;

    double getElapsedTimeSeconds() const { return mTimer.getElapsedTime(); }
    const std::string &getStdout() const { return mStdout; }
    const std::string &getStderr() const { return mStderr; }

  protected:
    friend class ANGLE_UTIL_EXPORT ProcessHandle;
    virtual ~Process();

    Timer mTimer;
    std::string mStdout;
    std::string mStderr;
};

class ANGLE_UTIL_EXPORT ProcessHandle final : angle::NonCopyable
{
  public:
    ProcessHandle();
    ProcessHandle(Process *process);
    ProcessHandle(const std::vector<const char *> &args, bool captureStdout, bool captureStderr);
    ~ProcessHandle();
    ProcessHandle(ProcessHandle &&other);
    ProcessHandle &operator=(ProcessHandle &&rhs);

    Process *operator->() { return mProcess; }
    const Process *operator->() const { return mProcess; }

    operator bool() const { return mProcess != nullptr; }

    void reset();

  private:
    Process *mProcess;
};

// Launch a process and optionally get the output. Uses a vector of c strings as command line
// arguments to the child process. Returns a Process handle which can be used to retrieve
// the stdout and stderr outputs as well as the exit code.
//
// Pass false for stdoutOut/stderrOut if you don't need to capture them.
//
// On success, returns a Process pointer with started() == true.
// On failure, returns a Process pointer with started() == false.
ANGLE_UTIL_EXPORT Process *LaunchProcess(const std::vector<const char *> &args,
                                         bool captureStdout,
                                         bool captureStderr);

ANGLE_UTIL_EXPORT int NumberOfProcessors();

}  // namespace angle

#endif  // UTIL_TEST_UTILS_H_
