/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#ifndef PAS_THREAD_SUSPEND_LOCK_H
#define PAS_THREAD_SUSPEND_LOCK_H

#include "pas_lock.h"

PAS_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

PAS_BEGIN_EXTERN_C;

PAS_DECLARE_LOCK(pas_thread_suspend);

/* Who holds the thread suspend lock. Clients that share it set this after locking, and reset it
   to pas_thread_suspend_reason_libpas before unlocking. */
typedef enum {
    pas_thread_suspend_reason_libpas,
    pas_thread_suspend_reason_client,
} pas_thread_suspend_reason;

PAS_API extern pas_thread_suspend_reason pas_thread_suspend_current_reason;

#if PAS_OS(LINUX)
/* libpas suspends threads with the client's suspend signal. The client calls set_signal() once its
   handler is installed, and that handler calls handle_signal() when the reason is not the client.
   Until then, libpas cannot suspend threads. */
PAS_API void pas_thread_suspend_set_signal(int signal);
PAS_API void pas_thread_suspend_handle_signal(void);
PAS_API bool pas_thread_suspend_suspend(pthread_t thread);
PAS_API void pas_thread_suspend_resume(pthread_t thread);
#endif

PAS_END_EXTERN_C;

PAS_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif /* PAS_THREAD_SUSPEND_LOCK_H */
