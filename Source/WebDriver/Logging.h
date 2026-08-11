/*
 * Copyright (C) 2003-2020 Igalia S.L. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/Forward.h>


#define COMMA() ,
#define OPTIONAL_ARGS(...) __VA_OPT__(COMMA()) __VA_ARGS__

#define RELEASE_LOG_FORWARDABLE(category, logMessage, ...) RELEASE_LOG(category, MESSAGE_##logMessage OPTIONAL_ARGS(__VA_ARGS__))
#define RELEASE_LOG_INFO_FORWARDABLE(category, logMessage, ...) RELEASE_LOG_INFO(category, MESSAGE_##logMessage OPTIONAL_ARGS(__VA_ARGS__))
#define RELEASE_LOG_ERROR_FORWARDABLE(category, logMessage, ...) RELEASE_LOG_ERROR(category, MESSAGE_##logMessage OPTIONAL_ARGS(__VA_ARGS__))
#define RELEASE_LOG_FAULT_FORWARDABLE(category, logMessage, ...) RELEASE_LOG_FAULT(category, MESSAGE_##logMessage OPTIONAL_ARGS(__VA_ARGS__))

namespace WebDriver {

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED

#ifndef LOG_CHANNEL_PREFIX
#define LOG_CHANNEL_PREFIX WebDriverLog
#endif

#define WEBDRIVER_LOG_CHANNELS(M) \
    M(SessionHost) \
    M(WebDriverBiDi) \
    M(WebDriverClassic)

#undef DECLARE_LOG_CHANNEL
#define DECLARE_LOG_CHANNEL(name) \
    extern WTFLogChannel JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, name);

WEBDRIVER_LOG_CHANNELS(DECLARE_LOG_CHANNEL)

#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED

} // namespace WebDriver
