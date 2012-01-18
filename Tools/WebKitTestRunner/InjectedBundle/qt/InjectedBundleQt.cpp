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
#include <wtf/Assertions.h>

#if HAVE(SIGNAL_H)
#include <signal.h>
#endif

namespace WTR {

#if HAVE(SIGNAL_H)
typedef void (*SignalHandler)(int);

static NO_RETURN void crashHandler(int sig)
{
    WTFReportBacktrace();
    exit(128 + sig);
}

static void setupSignalHandlers(SignalHandler handler)
{
    signal(SIGILL, handler);    /* 4:   illegal instruction (not reset when caught) */
    signal(SIGTRAP, handler);   /* 5:   trace trap (not reset when caught) */
    signal(SIGFPE, handler);    /* 8:   floating point exception */
    signal(SIGBUS, handler);    /* 10:  bus error */
    signal(SIGSEGV, handler);   /* 11:  segmentation violation */
    signal(SIGSYS, handler);    /* 12:  bad argument to system call */
    signal(SIGPIPE, handler);   /* 13:  write on a pipe with no reader */
    signal(SIGXCPU, handler);   /* 24:  exceeded CPU time limit */
    signal(SIGXFSZ, handler);   /* 25:  exceeded file size limit */
}

static void crashHook()
{
    setupSignalHandlers(SIG_DFL);
}
#endif

void InjectedBundle::platformInitialize(WKTypeRef)
{
    if (qgetenv("QT_WEBKIT2_DEBUG") == "1")
        return;

#if HAVE(SIGNAL_H)
    setupSignalHandlers(&crashHandler);
    WTFSetCrashHook(&crashHook);
#endif
}

} // namespace WTR
