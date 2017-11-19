/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(NETWORK_CAPTURE)

#include "Logging.h"

#define CAPTURE_INTERNAL_DEBUGGING 0
#define VERBOSE_CAPTURE_INTERNAL_DEBUGGING 0

#define DEBUG_STR(s)                    (s).ascii().data()

#if RELEASE_LOG_DISABLED
#define STRING_SPECIFIER                "%s"
#else
#define STRING_SPECIFIER                "%{public}s"
#endif

#if CAPTURE_INTERNAL_DEBUGGING
#define DEBUG_LOG_QUOTE(str)            #str
#define DEBUG_LOG_EXPAND_AND_QUOTE(str) DEBUG_LOG_QUOTE(str)
#define DEBUG_LOG(format, ...)          RELEASE_LOG(Network, "#PLT: %p - " STRING_SPECIFIER "::" STRING_SPECIFIER ": " format, this, DEBUG_LOG_EXPAND_AND_QUOTE(DEBUG_CLASS), __FUNCTION__, ##__VA_ARGS__)
#define DEBUG_LOG_ERROR(format, ...)    RELEASE_LOG_ERROR(Network, "#PLT: %p - " STRING_SPECIFIER "::" STRING_SPECIFIER ": " format, this, DEBUG_LOG_EXPAND_AND_QUOTE(DEBUG_CLASS), __FUNCTION__, ##__VA_ARGS__)
#if VERBOSE_CAPTURE_INTERNAL_DEBUGGING
#define DEBUG_LOG_VERBOSE(format, ...)  DEBUG_LOG(format, ##__VA_ARGS__)
#else
#define DEBUG_LOG_VERBOSE(...)          ((void)0)
#endif
#else
#define DEBUG_LOG(...)                  ((void)0)
#define DEBUG_LOG_ERROR(...)            RELEASE_LOG_ERROR(Network, __VA_ARGS__)
#define DEBUG_LOG_VERBOSE(...)          ((void)0)
#endif

#endif // ENABLE(NETWORK_CAPTURE)
