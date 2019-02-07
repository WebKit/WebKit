/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Logger.h>

namespace WTF {

class LoggerHelper {
public:
    virtual ~LoggerHelper() = default;

    virtual const Logger& logger() const = 0;
    virtual const char* logClassName() const = 0;
    virtual WTFLogChannel& logChannel() const = 0;
    virtual const void* logIdentifier() const = 0;

#if !RELEASE_LOG_DISABLED

#define LOGIDENTIFIER WTF::Logger::LogSiteIdentifier(logClassName(), __func__, logIdentifier())

#define ALWAYS_LOG(...)     logger().logAlways(logChannel(), __VA_ARGS__)
#define ERROR_LOG(...)      logger().error(logChannel(), __VA_ARGS__)
#define WARNING_LOG(...)    logger().warning(logChannel(), __VA_ARGS__)
#define NOTICE_LOG(...)     logger().notice(logChannel(), __VA_ARGS__)
#define INFO_LOG(...)       logger().info(logChannel(), __VA_ARGS__)
#define DEBUG_LOG(...)      logger().debug(logChannel(), __VA_ARGS__)
#define WILL_LOG(_level_)   logger().willLog(logChannel(), _level_)

    const void* childLogIdentifier(uint64_t identifier) const
    {
        static const int64_t parentMask = 0xffffffffffff0000l;
        static const int64_t maskLowerWord = 0xffffl;
        return reinterpret_cast<const void*>((reinterpret_cast<uint64_t>(logIdentifier()) & parentMask) | (identifier & maskLowerWord));
    }
#else

#define LOGIDENTIFIER (WTF::nullopt)

#define ALWAYS_LOG(...)     ((void)0)
#define ERROR_LOG(...)      ((void)0)
#define ERROR_LOG(...)      ((void)0)
#define WARNING_LOG(...)    ((void)0)
#define NOTICE_LOG(...)     ((void)0)
#define INFO_LOG(...)       ((void)0)
#define DEBUG_LOG(...)      ((void)0)
#define WILL_LOG(_level_)   false

#endif
    
};

} // namespace WTF

using WTF::LoggerHelper;

