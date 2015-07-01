// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "common/event_tracer.h"

#include "common/debug.h"

namespace angle
{

const unsigned char *GetTraceCategoryEnabledFlag(const char *name)
{
    angle::Platform *platform = ANGLEPlatformCurrent();
    ASSERT(platform);

    const unsigned char *categoryEnabledFlag = platform->getTraceCategoryEnabledFlag(name);
    if (categoryEnabledFlag != nullptr)
    {
        return categoryEnabledFlag;
    }

    static unsigned char disabled = 0;
    return &disabled;
}

Platform::TraceEventHandle AddTraceEvent(char phase, const unsigned char* categoryGroupEnabled, const char* name, unsigned long long id,
                                         int numArgs, const char** argNames, const unsigned char* argTypes,
                                         const unsigned long long* argValues, unsigned char flags)
{
    angle::Platform *platform = ANGLEPlatformCurrent();
    ASSERT(platform);

    double timestamp = platform->monotonicallyIncreasingTime();

    if (timestamp != 0)
    {
        angle::Platform::TraceEventHandle handle =
            platform->addTraceEvent(phase,
                                    categoryGroupEnabled,
                                    name,
                                    id,
                                    timestamp,
                                    numArgs,
                                    argNames,
                                    argTypes,
                                    argValues,
                                    flags);
        ASSERT(handle != 0);
        return handle;
    }

    return static_cast<Platform::TraceEventHandle>(0);
}

}  // namespace angle
