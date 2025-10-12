/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/Logger.h>

namespace WTF {

class LoggerHelper {
public:
    virtual ~LoggerHelper() = default;

    virtual const Logger& logger() const = 0;
    Ref<const Logger> protectedLogger() const { return logger(); }
    virtual ASCIILiteral logClassName() const = 0;
    virtual WTFLogChannel& logChannel() const = 0;
    virtual uint64_t logIdentifier() const = 0;

#if !RELEASE_LOG_DISABLED

#define LOGIDENTIFIER WTF::Logger::LogSiteIdentifier(logClassName(), __func__, logIdentifier())
#define LOGIDENTIFIER_WITH_THIS(thisPtr) WTF::Logger::LogSiteIdentifier((thisPtr)->logClassName(), __func__, (thisPtr)->logIdentifier())

#if VERBOSE_RELEASE_LOG
#define ALWAYS_LOG(...)     Ref { logger() }->logAlwaysVerbose(logChannel(), __FILE__, __func__, __LINE__, __VA_ARGS__)
#define ERROR_LOG(...)      Ref { logger() }->errorVerbose(logChannel(), __FILE__, __func__, __LINE__, __VA_ARGS__)
#define WARNING_LOG(...)    Ref { logger() }->warningVerbose(logChannel(), __FILE__, __func__, __LINE__, __VA_ARGS__)
#define INFO_LOG(...)       Ref { logger() }->infoVerbose(logChannel(), __FILE__, __func__, __LINE__, __VA_ARGS__)
#define DEBUG_LOG(...)      Ref { logger() }->debugVerbose(logChannel(), __FILE__, __func__, __LINE__, __VA_ARGS__)

#define ALWAYS_LOG_WITH_THIS(thisPtr, ...)     Ref { (thisPtr)->logger() }->logAlwaysVerbose((thisPtr)->logChannel(), __FILE__, __func__, __LINE__, __VA_ARGS__)
#define ERROR_LOG_WITH_THIS(thisPtr, ...)      Ref { (thisPtr)->logger() }->errorVerbose((thisPtr)->logChannel(), __FILE__, __func__, __LINE__, __VA_ARGS__)
#define INFO_LOG_WITH_THIS(thisPtr, ...)       Ref { (thisPtr)->logger() }->infoVerbose((thisPtr)->logChannel(), __FILE__, __func__, __LINE__, __VA_ARGS__)
#define DEBUG_LOG_WITH_THIS(thisPtr, ...)      Ref { (thisPtr)->logger() }->debugVerbose((thisPtr)->logChannel(), __FILE__, __func__, __LINE__, __VA_ARGS__)
#else
#define ALWAYS_LOG(...)     Ref { logger() }->logAlways(logChannel(), __VA_ARGS__)
#define ERROR_LOG(...)      Ref { logger() }->error(logChannel(), __VA_ARGS__)
#define WARNING_LOG(...)    Ref { logger() }->warning(logChannel(), __VA_ARGS__)
#define INFO_LOG(...)       Ref { logger() }->info(logChannel(), __VA_ARGS__)
#define DEBUG_LOG(...)      Ref { logger() }->debug(logChannel(), __VA_ARGS__)

#define ALWAYS_LOG_WITH_THIS(thisPtr, ...)     Ref { (thisPtr)->logger() }->logAlways((thisPtr)->logChannel(), __VA_ARGS__)
#define ERROR_LOG_WITH_THIS(thisPtr, ...)      Ref { (thisPtr)->logger() }->error((thisPtr)->logChannel(), __VA_ARGS__)
#define INFO_LOG_WITH_THIS(thisPtr, ...)       Ref { (thisPtr)->logger() }->info((thisPtr)->logChannel(), __VA_ARGS__)
#define DEBUG_LOG_WITH_THIS(thisPtr, ...)      Ref { (thisPtr)->logger() }->debug((thisPtr)->logChannel(), __VA_ARGS__)
#endif

#define WILL_LOG(_level_)   Ref { logger() }->willLog(logChannel(), _level_)

#define ALWAYS_LOG_IF(condition, ...)     if (condition) ALWAYS_LOG(__VA_ARGS__)
#define ERROR_LOG_IF(condition, ...)      if (condition) ERROR_LOG(__VA_ARGS__)
#define WARNING_LOG_IF(condition, ...)    if (condition) WARNING_LOG(__VA_ARGS__)
#define INFO_LOG_IF(condition, ...)       if (condition) INFO_LOG(__VA_ARGS__)
#define DEBUG_LOG_IF(condition, ...)      if (condition) DEBUG_LOG(__VA_ARGS__)

#define ALWAYS_LOG_IF_POSSIBLE(...)     if (RefPtr logger = loggerPtr()) logger->logAlways(logChannel(), __VA_ARGS__)
#define ERROR_LOG_IF_POSSIBLE(...)      if (RefPtr logger = loggerPtr()) logger->error(logChannel(), __VA_ARGS__)
#define WARNING_LOG_IF_POSSIBLE(...)    if (RefPtr logger = loggerPtr()) logger->warning(logChannel(), __VA_ARGS__)
#define INFO_LOG_IF_POSSIBLE(...)       if (RefPtr logger = loggerPtr()) logger->info(logChannel(), __VA_ARGS__)
#define DEBUG_LOG_IF_POSSIBLE(...)      if (RefPtr logger = loggerPtr()) logger->debug(logChannel(), __VA_ARGS__)
#define WILL_LOG_IF_POSSIBLE(_level_)   if (RefPtr logger = loggerPtr()) logger->willLog(logChannel(), _level_)

#define ALWAYS_LOG_WITH_THIS_IF_POSSIBLE(thisPtr, ...)  if (RefPtr logger = thisPtr->loggerPtr()) ALWAYS_LOG_WITH_THIS(thisPtr, __VA_ARGS__)
#define ERROR_LOG_WITH_THIS_IF_POSSIBLE(thisPtr, ...)   if (RefPtr logger = thisPtr->loggerPtr()) ERROR_LOG_WITH_THIS(thisPtr, __VA_ARGS__)
#define INFO_LOG_WITH_THIS_IF_POSSIBLE(thisPtr, ...)    if (RefPtr logger = thisPtr->loggerPtr()) INFO_LOG_WITH_THIS(thisPtr, __VA_ARGS__)
#define DEBUG_LOG_WITH_THIS_IF_POSSIBLE(thisPtr, ...)    if (RefPtr logger = thisPtr->loggerPtr()) DEBUG_LOG_WITH_THIS(thisPtr, __VA_ARGS__)

#if defined(__OBJC__)
#define OBJC_LOGIDENTIFIER WTF::Logger::LogSiteIdentifier(__PRETTY_FUNCTION__, self.logIdentifier)
#define OBJC_ALWAYS_LOG(...)     if (RefPtr<const Logger> logger = self.loggerPtr; logger && self.logChannel) logger->logAlways(*self.logChannel, __VA_ARGS__)
#define OBJC_ERROR_LOG(...)      if (RefPtr<const Logger> logger = self.loggerPtr; logger && self.logChannel) logger->error(*self.logChannel, __VA_ARGS__)
#define OBJC_WARNING_LOG(...)    if (RefPtr<const Logger> logger = self.loggerPtr; logger && self.logChannel) logger->warning(*self.logChannel, __VA_ARGS__)
#define OBJC_INFO_LOG(...)       if (RefPtr<const Logger> logger = self.loggerPtr; logger && self.logChannel) logger->info(*self.logChannel, __VA_ARGS__)
#define OBJC_DEBUG_LOG(...)      if (RefPtr<const Logger> logger = self.loggerPtr; logger && self.logChannel) logger->debug(*self.logChannel, __VA_ARGS__)
#endif

    static uint64_t childLogIdentifier(uint64_t parentIdentifier, uint64_t childIdentifier)
    {
        static constexpr uint64_t parentMask = 0xffffffffffff0000ull;
        static constexpr uint64_t maskLowerWord = 0xffffull;
        return reinterpret_cast<uint64_t>((parentIdentifier & parentMask) | (childIdentifier & maskLowerWord));
    }

    static uint64_t uniqueLogIdentifier()
    {
        return cryptographicallyRandomNumber<uint64_t>();
    }

#else // RELEASE_LOG_DISABLED

#define LOGIDENTIFIER (std::nullopt)
#define LOGIDENTIFIER_WITH_THIS(thisPtr) (UNUSED_PARAM(thisPtr))

#define ALWAYS_LOG(channelName, ...)  (UNUSED_PARAM(channelName))
#define ERROR_LOG(channelName, ...)   (UNUSED_PARAM(channelName))
#define WARNING_LOG(channelName, ...) (UNUSED_PARAM(channelName))
#define NOTICE_LOG(channelName, ...)  (UNUSED_PARAM(channelName))
#define INFO_LOG(channelName, ...)    (UNUSED_PARAM(channelName))
#define DEBUG_LOG(channelName, ...)   (UNUSED_PARAM(channelName))
#define WILL_LOG(_level_)    false

#define ALWAYS_LOG_WITH_THIS(thisPtr, channelName, ...)   do { UNUSED_PARAM(thisPtr); UNUSED_PARAM(channelName); } while (0)
#define ERROR_LOG_WITH_THIS(thisPtr, channelName, ...)    do { UNUSED_PARAM(thisPtr); UNUSED_PARAM(channelName); } while (0)
#define INFO_LOG_WITH_THIS(thisPtr, channelName, ...)     do { UNUSED_PARAM(thisPtr); UNUSED_PARAM(channelName); } while (0)
#define DEBUG_LOG_WITH_THIS(thisPtr, channelName, ...)    do { UNUSED_PARAM(thisPtr); UNUSED_PARAM(channelName); } while (0)

#define ALWAYS_LOG_WITH_THIS_IF_POSSIBLE(thisPtr, channelName, ...)  do { UNUSED_PARAM(thisPtr); UNUSED_PARAM(channelName); } while (0)
#define ERROR_LOG_WITH_THIS_IF_POSSIBLE(thisPtr, channelName, ...)   do { UNUSED_PARAM(thisPtr); UNUSED_PARAM(channelName); } while (0)
#define INFO_LOG_WITH_THIS_IF_POSSIBLE(thisPtr, channelName, ...)    do { UNUSED_PARAM(thisPtr); UNUSED_PARAM(channelName); } while (0)
#define DEBUG_LOG_WITH_THIS_IF_POSSIBLE(thisPtr, channelName, ...)   do { UNUSED_PARAM(thisPtr); UNUSED_PARAM(channelName); } while (0)

#define ALWAYS_LOG_IF(condition, ...)     ((void)0)
#define ERROR_LOG_IF(condition, ...)      ((void)0)
#define WARNING_LOG_IF(condition, ...)    ((void)0)
#define INFO_LOG_IF(condition, ...)       ((void)0)
#define DEBUG_LOG_IF(condition, ...)      ((void)0)

#define ALWAYS_LOG_IF_POSSIBLE(...)     ((void)0)
#define ERROR_LOG_IF_POSSIBLE(...)      ((void)0)
#define WARNING_LOG_IF_POSSIBLE(...)    ((void)0)
#define INFO_LOG_IF_POSSIBLE(...)       ((void)0)
#define DEBUG_LOG_IF_POSSIBLE(...)      ((void)0)
#define WILL_LOG_IF_POSSIBLE(_level_)   ((void)0)

#endif // RELEASE_LOG_DISABLED

};

} // namespace WTF

using WTF::LoggerHelper;
