/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "ForwardIOStreamsAndroid.h"

#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wtf/StdLibExtras.h>

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

namespace {

const char optionInFIFO[] = "--in-fifo=";
const char optionOutFIFO[] = "--out-fifo=";
const char optionErrFIFO[] = "--err-fifo=";

void androidLogError(const char* format, ...) WTF_ATTRIBUTE_PRINTF(1, 2);

void androidLogError(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    __android_log_vprint(ANDROID_LOG_ERROR, "WebKit", format, args);
    va_end(args);
}

void removeArg(int index, int* argc, char*** argv)
{
    for (int i = index; i < *argc; ++i)
        (*argv)[i] = (*argv)[i + 1];
    --*argc;
}

void createFIFO(const char* fifoPath)
{
    unlink(fifoPath);
    // 0666 is rw-rw-rw-, to allow adb shell to read/write the fifo.
    // Explicitly call chmod to ensure the mode is set despite umask.
    if (mkfifo(fifoPath, 0666) || chmod(fifoPath, 0666)) {
        androidLogError("Failed to create fifo %s: %s\n", fifoPath, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void redirect(FILE* stream, const char* path, const char* mode)
{
    if (!freopen(path, mode, stream)) {
        androidLogError("Failed to redirect stream to file: %s: %s\n", path, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

} // namespace

namespace WebKit {

void maybeInitIOStreamForwardingForAndroid(int* argc, char*** argv)
{
    const char* inFIFO = 0;
    const char* outFIFO = 0;
    const char* errFIFO = 0;
    for (int i = 1; i < *argc; ) {
        const char* argument = (*argv)[i];
        if (strstr(argument, optionInFIFO) == argument) {
            inFIFO = argument + WTF_ARRAY_LENGTH(optionInFIFO) - 1;
            createFIFO(inFIFO);
            removeArg(i, argc, argv);
        } else if (strstr(argument, optionOutFIFO) == argument) {
            outFIFO = argument + WTF_ARRAY_LENGTH(optionOutFIFO) - 1;
            createFIFO(outFIFO);
            removeArg(i, argc, argv);
        } else if (strstr(argument, optionErrFIFO) == argument) {
            errFIFO = argument + WTF_ARRAY_LENGTH(optionErrFIFO) - 1;
            createFIFO(errFIFO);
            removeArg(i, argc, argv);
        } else
            ++i;
    }

    // The order of createFIFO() and redirectToFIFO() is important to avoid deadlock.
    if (outFIFO)
        redirect(stdout, outFIFO, "w");
    if (inFIFO)
        redirect(stdin, inFIFO, "r");
    if (errFIFO)
        redirect(stderr, errFIFO, "w");
    else {
        // Redirect stderr to stdout.
        dup2(1, 2);
    }
}

} // namespace WebKit
