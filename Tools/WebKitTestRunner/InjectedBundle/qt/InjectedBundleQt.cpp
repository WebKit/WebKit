/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2011 University of Szeged. All rights reserved.
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

#include "config.h"
#include "InjectedBundle.h"
#include <QByteArray>
#include <QtGlobal>
#include <stdio.h>
#include <stdlib.h>
#include <wtf/AlwaysInline.h>

#if HAVE(SIGNAL_H)
#include <signal.h>
#endif

#if defined(__GLIBC__) && !defined(__UCLIBC__)
#include <execinfo.h>
#endif

namespace WTR {

static inline void printBacktrace()
{
#if defined(__GLIBC__) && !defined(__UCLIBC__)
    void* frames[256];
    size_t size = backtrace(frames, 256);
    if (!size)
        return;

    char** symbols = backtrace_symbols(frames, size);
    if (!symbols)
        return;

    for (unsigned i = 0; i < size; ++i)
        fprintf(stderr, "%u: %s\n", i, symbols[i]);

    fflush(stderr);
    free(symbols);
#endif
}

#if HAVE(SIGNAL_H)
static NO_RETURN void crashHandler(int signal)
{
    fprintf(stderr, "%s\n", strsignal(signal));
    printBacktrace();
    exit(128 + signal);
}
#endif

void InjectedBundle::platformInitialize(WKTypeRef)
{
    if (qgetenv("QT_WEBKIT2_DEBUG") == "1")
        return;

#if HAVE(SIGNAL_H)
    signal(SIGILL, crashHandler);    /* 4:   illegal instruction (not reset when caught) */
    signal(SIGTRAP, crashHandler);   /* 5:   trace trap (not reset when caught) */
    signal(SIGFPE, crashHandler);    /* 8:   floating point exception */
    signal(SIGBUS, crashHandler);    /* 10:  bus error */
    signal(SIGSEGV, crashHandler);   /* 11:  segmentation violation */
    signal(SIGSYS, crashHandler);    /* 12:  bad argument to system call */
    signal(SIGPIPE, crashHandler);   /* 13:  write on a pipe with no reader */
    signal(SIGXCPU, crashHandler);   /* 24:  exceeded CPU time limit */
    signal(SIGXFSZ, crashHandler);   /* 25:  exceeded file size limit */
#endif
}

} // namespace WTR
