//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// system_utils.h: declaration of OS-specific utility functions

#ifndef COMMON_SYSTEM_UTILS_H_
#define COMMON_SYSTEM_UTILS_H_

#include "common/Optional.h"
#include "common/angleutils.h"

#include <functional>
#include <string>

namespace angle
{
std::string GetExecutableName();
std::string GetExecutablePath();
std::string GetExecutableDirectory();
std::string GetModuleDirectory();
const char *GetSharedLibraryExtension();
const char *GetExecutableExtension();
char GetPathSeparator();
Optional<std::string> GetCWD();
bool SetCWD(const char *dirName);
bool SetEnvironmentVar(const char *variableName, const char *value);
bool UnsetEnvironmentVar(const char *variableName);
bool GetBoolEnvironmentVar(const char *variableName);
std::string GetEnvironmentVar(const char *variableName);
std::string GetEnvironmentVarOrUnCachedAndroidProperty(const char *variableName,
                                                       const char *propertyName);
std::string GetEnvironmentVarOrAndroidProperty(const char *variableName, const char *propertyName);
const char *GetPathSeparatorForEnvironmentVar();
bool PrependPathToEnvironmentVar(const char *variableName, const char *path);
bool IsDirectory(const char *filename);
bool IsFullPath(std::string dirName);
std::string GetRootDirectory();
std::string ConcatenatePath(std::string first, std::string second);

// Get absolute time in seconds.  Use this function to get an absolute time with an unknown origin.
double GetCurrentSystemTime();
// Get CPU time for current process in seconds.
double GetCurrentProcessCpuTime();

// Run an application and get the output.  Gets a nullptr-terminated set of args to execute the
// application with, and returns the stdout and stderr outputs as well as the exit code.
//
// Pass nullptr for stdoutOut/stderrOut if you don't need to capture. exitCodeOut is required.
//
// Returns false if it fails to actually execute the application.
bool RunApp(const std::vector<const char *> &args,
            std::string *stdoutOut,
            std::string *stderrOut,
            int *exitCodeOut);

class Library : angle::NonCopyable
{
  public:
    virtual ~Library() {}
    virtual void *getSymbol(const char *symbolName) = 0;
    virtual void *getNative() const                 = 0;
    virtual std::string getPath() const             = 0;

    template <typename FuncT>
    void getAs(const char *symbolName, FuncT *funcOut)
    {
        *funcOut = reinterpret_cast<FuncT>(getSymbol(symbolName));
    }
};

// Use SYSTEM_DIR to bypass loading ANGLE libraries with the same name as system DLLS
// (e.g. opengl32.dll)
enum class SearchType
{
    // Try to find the library in the same directory as the current module
    ModuleDir,
    // Load the library from the system directories
    SystemDir,
    // Get a reference to an already loaded shared library.
    AlreadyLoaded,
};

Library *OpenSharedLibrary(const char *libraryName, SearchType searchType);
Library *OpenSharedLibraryWithExtension(const char *libraryName, SearchType searchType);
Library *OpenSharedLibraryAndGetError(const char *libraryName,
                                      SearchType searchType,
                                      std::string *errorOut);
Library *OpenSharedLibraryWithExtensionAndGetError(const char *libraryName,
                                                   SearchType searchType,
                                                   std::string *errorOut);

// Returns true if the process is currently being debugged.
bool IsDebuggerAttached();

// Calls system APIs to break into the debugger.
void BreakDebugger();

bool ProtectMemory(uintptr_t start, size_t size);
bool UnprotectMemory(uintptr_t start, size_t size);

size_t GetPageSize();

// Return type of the PageFaultCallback
enum class PageFaultHandlerRangeType
{
    // The memory address was known by the page fault handler
    InRange,
    // The memory address was not in the page fault handler's range
    // and the signal will be forwarded to the default page handler.
    OutOfRange,
};

using PageFaultCallback = std::function<PageFaultHandlerRangeType(uintptr_t)>;

class PageFaultHandler : angle::NonCopyable
{
  public:
    PageFaultHandler(PageFaultCallback callback);
    virtual ~PageFaultHandler();

    // Registers OS level page fault handler for memory protection signals
    // and enables reception on PageFaultCallback
    virtual bool enable() = 0;

    // Unregisters OS level page fault handler and deactivates PageFaultCallback
    virtual bool disable() = 0;

  protected:
    PageFaultCallback mCallback;
};

// Creates single instance page fault handler
PageFaultHandler *CreatePageFaultHandler(PageFaultCallback callback);

}  // namespace angle

#endif  // COMMON_SYSTEM_UTILS_H_
