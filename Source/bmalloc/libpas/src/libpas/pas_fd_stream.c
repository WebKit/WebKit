/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_fd_stream.h"

#include "pas_log.h"

static void fd_stream_vprintf(pas_stream* stream, const char* format, va_list arg_list)
{
    pas_fd_stream_vprintf((pas_fd_stream*)stream, format, arg_list);
}

pas_stream_functions pas_fd_stream_functions = {
    .vprintf = fd_stream_vprintf
};

pas_fd_stream pas_log_stream = PAS_FD_STREAM_INITIALIZER(PAS_LOG_DEFAULT_FD);

void pas_fd_stream_construct(pas_fd_stream* stream, int fd)
{
    stream->base.functions = &pas_fd_stream_functions;
    stream->fd = fd;
}

void pas_fd_stream_vprintf(pas_fd_stream* stream, const char* format, va_list arg_list)
{
    pas_vlog_fd(stream->fd, format, arg_list);
}

#endif /* LIBPAS_ENABLED */
