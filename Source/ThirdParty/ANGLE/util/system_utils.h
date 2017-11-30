//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// system_utils.h: declaration of OS-specific utility functions

#ifndef UTIL_SYSTEM_UTILS_H_
#define UTIL_SYSTEM_UTILS_H_

#include <string>

#include <export.h>

#include "common/angleutils.h"
#include "common/system_utils.h"

namespace angle
{

// Cross platform equivalent of the Windows Sleep function
ANGLE_EXPORT void Sleep(unsigned int milliseconds);

ANGLE_EXPORT void SetLowPriorityProcess();

// Write a debug message, either to a standard output or Debug window.
ANGLE_EXPORT void WriteDebugMessage(const char *format, ...);

class ANGLE_EXPORT Library : angle::NonCopyable
{
  public:
    virtual ~Library() {}
    virtual void *getSymbol(const std::string &symbolName) = 0;
};

ANGLE_EXPORT Library *loadLibrary(const std::string &libraryName);

} // namespace angle

#endif  // UTIL_SYSTEM_UTILS_H_
