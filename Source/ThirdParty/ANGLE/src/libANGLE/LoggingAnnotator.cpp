//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// LoggingAnnotator.cpp: DebugAnnotator implementing logging
//

#include "libANGLE/LoggingAnnotator.h"

#include <platform/Platform.h>

namespace angle
{

bool LoggingAnnotator::getStatus()
{
    return false;
}

void LoggingAnnotator::logMessage(const gl::LogMessage &msg) const
{
    auto *plat = ANGLEPlatformCurrent();
    if (plat != nullptr)
    {
        switch (msg.getSeverity())
        {
            case gl::LOG_ERR:
                plat->logError(plat, msg.getMessage().c_str());
                break;
            case gl::LOG_WARN:
                plat->logWarning(plat, msg.getMessage().c_str());
                break;
            default:
                UNREACHABLE();
        }
    }
    else
    {
        gl::Trace(msg.getSeverity(), msg.getMessage().c_str());
    }
}

}  // namespace angle
