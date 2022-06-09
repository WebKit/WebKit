//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_ENVIRONMENTVARIABLE_H_
#define COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_ENVIRONMENTVARIABLE_H_

#include <cstdlib>
#include <string>

#include "common/debug.h"

namespace sh
{

inline bool readBoolEnvVar(const char *var)
{
    const char *str = std::getenv(var);
    if (str == nullptr)
    {
        return false;
    }
    if (strcmp(str, "0") == 0)
    {
        return false;
    }
    ASSERT(strcmp(str, "1") == 0);
    return true;
}

inline std::string readStringEnvVar(const char *var)
{
    const char *str = std::getenv(var);
    if (str == nullptr)
    {
        return "";
    }
    return str;
}

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_ENVIRONMENTVARIABLE_H_
