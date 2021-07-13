/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_LOG_N
#define PAS_LOG_N

#include "pas_utils.h"
#include <pthread.h>
#include <stdarg.h>

PAS_BEGIN_EXTERN_C;

#define PAS_LOG_MAX_BYTES 1024
#define PAS_LOG_DEFAULT_FD 1

/* If this is set to non-NULL, then all pthread_log calls from other threads wait. */
extern PAS_API pthread_t pas_thread_that_is_crash_logging;

/* Logging functions that don't require any allocation. You cannot log more than
   PAS_LOG_MAX_BYTES at a time. */

PAS_API void pas_vlog_fd(int fd, const char* format, va_list);
PAS_API void pas_log_fd(int fd, const char* format, ...);

PAS_API void pas_vlog(const char* format, va_list);
PAS_API void pas_log(const char* format, ...);

PAS_API void pas_start_crash_logging(void);

PAS_END_EXTERN_C;

#endif /* PAS_LOG_N */

