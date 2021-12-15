//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// system_utils.cpp: Implementation of common functions

#include "common/system_utils.h"

#include <stdlib.h>

#if defined(ANGLE_PLATFORM_ANDROID)
#    include <sys/system_properties.h>
#endif

namespace angle
{
std::string GetExecutableName()
{
#if defined(ANGLE_PLATFORM_ANDROID) && __ANDROID_API__ >= 21
    // Support for "getprogname" function in bionic was introduced in L (API level 21)
    const char *executableName = getprogname();
    return (executableName) ? std::string(executableName) : "ANGLE";
#else
    std::string executableName = GetExecutablePath();
    size_t lastPathSepLoc      = executableName.find_last_of(GetPathSeparator());
    return (lastPathSepLoc > 0 ? executableName.substr(lastPathSepLoc + 1, executableName.length())
                               : "ANGLE");
#endif  // ANGLE_PLATFORM_ANDROID
}

// On Android return value cached in the process environment, if none, call
// GetEnvironmentVarOrUnCachedAndroidProperty if not in environment.
std::string GetEnvironmentVarOrAndroidProperty(const char *variableName, const char *propertyName)
{
#if defined(ANGLE_PLATFORM_ANDROID) && __ANDROID_API__ >= 21
    // Can't use GetEnvironmentVar here because that won't allow us to distinguish between the
    // environment being set to an empty string vs. not set at all.
    const char *variableValue = getenv(variableName);
    if (variableValue != nullptr)
    {
        std::string value(variableValue);
        return value;
    }
#endif
    return GetEnvironmentVarOrUnCachedAndroidProperty(variableName, propertyName);
}

// Call out to 'getprop' on a shell to get an Android property.  If the value was set, set an
// environment variable with that value.  Return the value of the environment variable.
std::string GetEnvironmentVarOrUnCachedAndroidProperty(const char *variableName,
                                                       const char *propertyName)
{
#if defined(ANGLE_PLATFORM_ANDROID) && __ANDROID_API__ >= 26
    std::string propertyValue;

    const prop_info *propertyInfo = __system_property_find(propertyName);
    if (propertyInfo != nullptr)
    {
        __system_property_read_callback(
            propertyInfo,
            [](void *cookie, const char *, const char *value, unsigned) {
                auto propertyValue = reinterpret_cast<std::string *>(cookie);
                *propertyValue     = value;
            },
            &propertyValue);
    }

    // Set the environment variable with the value.
    SetEnvironmentVar(variableName, propertyValue.c_str());
    return propertyValue;
#else
    // Return the environment variable's value.
    return GetEnvironmentVar(variableName);
#endif  // ANGLE_PLATFORM_ANDROID
}

bool GetBoolEnvironmentVar(const char *variableName)
{
    std::string envVarString = GetEnvironmentVar(variableName);
    return (!envVarString.empty() && envVarString == "1");
}

bool PrependPathToEnvironmentVar(const char *variableName, const char *path)
{
    std::string oldValue = GetEnvironmentVar(variableName);
    const char *newValue = nullptr;
    std::string buf;
    if (oldValue.empty())
    {
        newValue = path;
    }
    else
    {
        buf = path;
        buf += GetPathSeparatorForEnvironmentVar();
        buf += oldValue;
        newValue = buf.c_str();
    }
    return SetEnvironmentVar(variableName, newValue);
}

bool IsFullPath(std::string dirName)
{
    if (dirName.find(GetRootDirectory()) == 0)
    {
        return true;
    }
    return false;
}

std::string ConcatenatePath(std::string first, std::string second)
{
    if (first.empty())
    {
        return second;
    }
    if (second.empty())
    {
        return first;
    }
    if (IsFullPath(second))
    {
        return second;
    }
    bool firstRedundantPathSeparator = first.find_last_of(GetPathSeparator()) == first.length() - 1;
    bool secondRedundantPathSeparator = second.find(GetPathSeparator()) == 0;
    if (firstRedundantPathSeparator && secondRedundantPathSeparator)
    {
        return first + second.substr(1);
    }
    else if (firstRedundantPathSeparator || secondRedundantPathSeparator)
    {
        return first + second;
    }
    return first + GetPathSeparator() + second;
}

PageFaultHandler::PageFaultHandler(PageFaultCallback callback) : mCallback(callback) {}
PageFaultHandler::~PageFaultHandler() {}

}  // namespace angle
