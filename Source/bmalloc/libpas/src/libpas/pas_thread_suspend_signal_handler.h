/*
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#ifndef PAS_THREAD_SUSPEND_SIGNAL_HANDLER_H
#define PAS_THREAD_SUSPEND_SIGNAL_HANDLER_H

#include "pas_config.h"

#if !PAS_OS(DARWIN) && !PAS_OS(WINDOWS)

#include "pas_thread_suspend.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

// Do not use these directly, use pas_thread_suspend.
void pas_thread_suspend_signal_handler_install(void);
bool pas_thread_suspend_signal_handler_suspend(pas_thread_suspend_data*);
void pas_thread_suspend_signal_handler_resume(pas_thread_suspend_data*);
PAS_API int pas_thread_suspend_signal_number(void);
PAS_API void pas_thread_suspend_set_signal(int signal);

#if PAS_ENABLE_TESTING
PAS_API unsigned pas_thread_suspend_signal_handler_retry_count(void);
#endif

PAS_END_EXTERN_C;

#endif /* !PAS_OS(DARWIN) && !PAS_OS(WINDOWS) */

#endif /* PAS_THREAD_SUSPEND_SIGNAL_HANDLER_H */
