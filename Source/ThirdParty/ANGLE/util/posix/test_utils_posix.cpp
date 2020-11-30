//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// test_utils_posix.cpp: Implementation of OS-specific functions for Posix systems

#include "util/test_utils.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <cstdarg>
#include <cstring>
#include <iostream>

#include "common/debug.h"
#include "common/platform.h"

#if !defined(ANGLE_PLATFORM_FUCHSIA)
#    include <sys/resource.h>
#endif

namespace angle
{
namespace
{
struct ScopedPipe
{
    ~ScopedPipe()
    {
        closeEndPoint(0);
        closeEndPoint(1);
    }

    void closeEndPoint(int index)
    {
        if (fds[index] >= 0)
        {
            close(fds[index]);
            fds[index] = -1;
        }
    }

    bool valid() const { return fds[0] != -1 || fds[1] != -1; }

    int fds[2] = {
        -1,
        -1,
    };
};

bool ReadFromFile(int fd, std::string *out)
{
    char buffer[256];
    ssize_t bytesRead = read(fd, buffer, sizeof(buffer));

    // If interrupted, retry.
    if (bytesRead < 0 && errno == EINTR)
    {
        return true;
    }

    // If failed, or nothing to read, we are done.
    if (bytesRead <= 0)
    {
        return false;
    }

    out->append(buffer, bytesRead);
    return true;
}

void ReadEntireFile(int fd, std::string *out)
{
    while (true)
    {
        if (!ReadFromFile(fd, out))
            break;
    }
}

class PosixProcess : public Process
{
  public:
    PosixProcess(const std::vector<const char *> &commandLineArgs,
                 bool captureStdOut,
                 bool captureStdErr)
    {
        if (commandLineArgs.empty())
        {
            return;
        }

        // Create pipes for stdout and stderr.
        if (captureStdOut)
        {
            if (pipe(mStdoutPipe.fds) != 0)
            {
                std::cerr << "Error calling pipe: " << errno << "\n";
                return;
            }
            if (fcntl(mStdoutPipe.fds[0], F_SETFL, O_NONBLOCK) == -1)
            {
                std::cerr << "Error calling fcntl: " << errno << "\n";
                return;
            }
        }
        if (captureStdErr)
        {
            if (pipe(mStderrPipe.fds) != 0)
            {
                std::cerr << "Error calling pipe: " << errno << "\n";
                return;
            }
            if (fcntl(mStderrPipe.fds[0], F_SETFL, O_NONBLOCK) == -1)
            {
                std::cerr << "Error calling fcntl: " << errno << "\n";
                return;
            }
        }

        mPID = fork();
        if (mPID < 0)
        {
            return;
        }

        mStarted = true;
        mTimer.start();

        if (mPID == 0)
        {
            // Child.  Execute the application.

            // Redirect stdout and stderr to the pipe fds.
            if (captureStdOut)
            {
                if (dup2(mStdoutPipe.fds[1], STDOUT_FILENO) < 0)
                {
                    _exit(errno);
                }
                mStdoutPipe.closeEndPoint(1);
            }
            if (captureStdErr)
            {
                if (dup2(mStderrPipe.fds[1], STDERR_FILENO) < 0)
                {
                    _exit(errno);
                }
                mStderrPipe.closeEndPoint(1);
            }

            // Execute the application, which doesn't return unless failed.  Note: execv takes argv
            // as `char * const *` for historical reasons.  It is safe to const_cast it:
            //
            // http://pubs.opengroup.org/onlinepubs/9699919799/functions/exec.html
            //
            // > The statement about argv[] and envp[] being constants is included to make explicit
            // to future writers of language bindings that these objects are completely constant.
            // Due to a limitation of the ISO C standard, it is not possible to state that idea in
            // standard C. Specifying two levels of const- qualification for the argv[] and envp[]
            // parameters for the exec functions may seem to be the natural choice, given that these
            // functions do not modify either the array of pointers or the characters to which the
            // function points, but this would disallow existing correct code. Instead, only the
            // array of pointers is noted as constant.
            std::vector<char *> args;
            for (const char *arg : commandLineArgs)
            {
                args.push_back(const_cast<char *>(arg));
            }
            args.push_back(nullptr);

            execv(commandLineArgs[0], args.data());
            std::cerr << "Error calling evecv: " << errno;
            _exit(errno);
        }
        // Parent continues execution.
        mStdoutPipe.closeEndPoint(1);
        mStderrPipe.closeEndPoint(1);
    }

    ~PosixProcess() override {}

    bool started() override { return mStarted; }

    bool finish() override
    {
        if (!mStarted)
        {
            return false;
        }

        if (mFinished)
        {
            return true;
        }

        while (!finished())
        {
            angle::Sleep(1);
        }

        return true;
    }

    bool finished() override
    {
        if (!mStarted)
        {
            return false;
        }

        if (mFinished)
        {
            return true;
        }

        int status        = 0;
        pid_t returnedPID = ::waitpid(mPID, &status, WNOHANG);

        if (returnedPID == -1 && errno != ECHILD)
        {
            std::cerr << "Error calling waitpid: " << ::strerror(errno) << "\n";
            return true;
        }

        if (returnedPID == mPID)
        {
            mFinished = true;
            mTimer.stop();
            readPipes();
            mExitCode = WEXITSTATUS(status);
            return true;
        }

        if (mStdoutPipe.valid())
        {
            ReadFromFile(mStdoutPipe.fds[0], &mStdout);
        }

        if (mStderrPipe.valid())
        {
            ReadFromFile(mStderrPipe.fds[0], &mStderr);
        }

        return false;
    }

    int getExitCode() override { return mExitCode; }

    bool kill() override
    {
        if (!mStarted)
        {
            return false;
        }

        if (finished())
        {
            return true;
        }

        return (::kill(mPID, SIGTERM) == 0);
    }

  private:
    void readPipes()
    {
        // Close the write end of the pipes, so EOF can be generated when child exits.
        // Then read back the output of the child.
        if (mStdoutPipe.valid())
        {
            ReadEntireFile(mStdoutPipe.fds[0], &mStdout);
        }
        if (mStderrPipe.valid())
        {
            ReadEntireFile(mStderrPipe.fds[0], &mStderr);
        }
    }

    bool mStarted  = false;
    bool mFinished = false;
    ScopedPipe mStdoutPipe;
    ScopedPipe mStderrPipe;
    int mExitCode = 0;
    pid_t mPID    = -1;
};

std::string TempFileName()
{
    return std::string(".angle.XXXXXX");
}
}  // anonymous namespace

void Sleep(unsigned int milliseconds)
{
    // On Windows Sleep(0) yields while it isn't guaranteed by Posix's sleep
    // so we replicate Windows' behavior with an explicit yield.
    if (milliseconds == 0)
    {
        sched_yield();
    }
    else
    {
        timespec sleepTime = {
            .tv_sec  = milliseconds / 1000,
            .tv_nsec = (milliseconds % 1000) * 1000000,
        };

        nanosleep(&sleepTime, nullptr);
    }
}

void SetLowPriorityProcess()
{
#if !defined(ANGLE_PLATFORM_FUCHSIA)
    setpriority(PRIO_PROCESS, getpid(), 10);
#endif
}

void WriteDebugMessage(const char *format, ...)
{
    va_list vararg;
    va_start(vararg, format);
    vfprintf(stderr, format, vararg);
    va_end(vararg);
}

bool StabilizeCPUForBenchmarking()
{
#if !defined(ANGLE_PLATFORM_FUCHSIA)
    bool success = true;
    errno        = 0;
    setpriority(PRIO_PROCESS, getpid(), -20);
    if (errno)
    {
        // A friendly warning in case the test was run without appropriate permission.
        perror(
            "Warning: setpriority failed in StabilizeCPUForBenchmarking. Process will retain "
            "default priority");
        success = false;
    }
#    if ANGLE_PLATFORM_LINUX
    cpu_set_t affinity;
    CPU_SET(0, &affinity);
    errno = 0;
    if (sched_setaffinity(getpid(), sizeof(affinity), &affinity))
    {
        perror(
            "Warning: sched_setaffinity failed in StabilizeCPUForBenchmarking. Process will retain "
            "default affinity");
        success = false;
    }
#    else
    // TODO(jmadill): Implement for non-linux. http://anglebug.com/2923
#    endif

    return success;
#else  // defined(ANGLE_PLATFORM_FUCHSIA)
    return false;
#endif
}

bool GetTempDir(char *tempDirOut, uint32_t maxDirNameLen)
{
    const char *tmp = getenv("TMPDIR");
    if (tmp)
    {
        strncpy(tempDirOut, tmp, maxDirNameLen);
        return true;
    }

#if defined(ANGLE_PLATFORM_ANDROID)
    // TODO(jmadill): Android support. http://anglebug.com/3162
    // return PathService::Get(DIR_CACHE, path);
    return false;
#else
    strncpy(tempDirOut, "/tmp", maxDirNameLen);
    return true;
#endif
}

bool CreateTemporaryFileInDir(const char *dir, char *tempFileNameOut, uint32_t maxFileNameLen)
{
    std::string tempFile = TempFileName();
    sprintf(tempFileNameOut, "%s/%s", dir, tempFile.c_str());
    int fd = mkstemp(tempFileNameOut);
    close(fd);
    return fd != -1;
}

bool DeleteFile(const char *path)
{
    return unlink(path) == 0;
}

Process *LaunchProcess(const std::vector<const char *> &args,
                       bool captureStdout,
                       bool captureStderr)
{
    return new PosixProcess(args, captureStdout, captureStderr);
}

int NumberOfProcessors()
{
    // sysconf returns the number of "logical" (not "physical") processors on both
    // Mac and Linux.  So we get the number of max available "logical" processors.
    //
    // Note that the number of "currently online" processors may be fewer than the
    // returned value of NumberOfProcessors(). On some platforms, the kernel may
    // make some processors offline intermittently, to save power when system
    // loading is low.
    //
    // One common use case that needs to know the processor count is to create
    // optimal number of threads for optimization. It should make plan according
    // to the number of "max available" processors instead of "currently online"
    // ones. The kernel should be smart enough to make all processors online when
    // it has sufficient number of threads waiting to run.
    long res = sysconf(_SC_NPROCESSORS_CONF);
    if (res == -1)
    {
        return 1;
    }

    return static_cast<int>(res);
}

const char *GetNativeEGLLibraryNameWithExtension()
{
#if defined(ANGLE_PLATFORM_ANDROID)
    return "libEGL.so";
#elif defined(ANGLE_PLATFORM_LINUX)
    return "libEGL.so.1";
#else
    return "unknown_libegl";
#endif
}
}  // namespace angle
