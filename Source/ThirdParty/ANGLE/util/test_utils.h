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

namespace angle
{
// Cross platform equivalent of the Windows Sleep function
void Sleep(unsigned int milliseconds);

void SetLowPriorityProcess();

// Write a debug message, either to a standard output or Debug window.
void WriteDebugMessage(const char *format, ...);

// Set thread affinity and priority.
bool StabilizeCPUForBenchmarking();

// Set a crash handler to print stack traces.
using CrashCallback = std::function<void()>;
void InitCrashHandler(CrashCallback *callback);
void TerminateCrashHandler();

// Print a stack back trace.
void PrintStackBacktrace();

// Get temporary directory.
bool GetTempDir(char *tempDirOut, uint32_t maxDirNameLen);

// Creates a temporary file. The full path is placed in |tempFileNameOut|, and the
// function returns true if was successful in creating the file. The file will
// be empty and all handles closed after this function returns.
bool CreateTemporaryFile(char *tempFileNameOut, uint32_t maxFileNameLen);

// Same as CreateTemporaryFile but the file is created in |dir|.
bool CreateTemporaryFileInDir(const char *dir, char *tempFileNameOut, uint32_t maxFileNameLen);

// Deletes a file or directory.
bool DeleteSystemFile(const char *path);

// Reads a file contents into a string.
bool ReadEntireFileToString(const char *filePath, char *contentsOut, uint32_t maxLen);

// Compute a file's size.
bool GetFileSize(const char *filePath, uint32_t *sizeOut);

class ProcessHandle;

class Process : angle::NonCopyable
{
  public:
    virtual bool started()    = 0;
    virtual bool finished()   = 0;
    virtual bool finish()     = 0;
    virtual bool kill()       = 0;
    virtual int getExitCode() = 0;

    double getElapsedTimeSeconds() const { return mTimer.getElapsedWallClockTime(); }
    const std::string &getStdout() const { return mStdout; }
    const std::string &getStderr() const { return mStderr; }

  protected:
    friend class ProcessHandle;
    virtual ~Process();

    Timer mTimer;
    std::string mStdout;
    std::string mStderr;
};

enum class ProcessOutputCapture
{
    Nothing,
    // Capture stdout only
    StdoutOnly,
    // Capture stdout, and pipe stderr to stdout
    StdoutAndStderrInterleaved,
    // Capture stdout and stderr separately
    StdoutAndStderrSeparately,
};

class ProcessHandle final : angle::NonCopyable
{
  public:
    ProcessHandle();
    ProcessHandle(Process *process);
    ProcessHandle(const std::vector<const char *> &args, ProcessOutputCapture captureOutput);
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
Process *LaunchProcess(const std::vector<const char *> &args, ProcessOutputCapture captureOutput);

int NumberOfProcessors();

const char *GetNativeEGLLibraryNameWithExtension();

// Intercept Metal shader cache access to avoid slow caching mechanism that caused the test timeout
// in the past. Note:
// - If there is NO "--skip-file-hooking" switch in the argument list:
//   - This function will re-launch the app with additional argument "--skip-file-hooking".
//   - The running process's image & memory will be re-created.
// - If there is "--skip-file-hooking" switch in the argument list, this function will do nothing.
#if defined(ANGLE_PLATFORM_APPLE)
void InitMetalFileAPIHooking(int argc, char **argv);
#endif
}  // namespace angle

#endif  // UTIL_TEST_UTILS_H_
