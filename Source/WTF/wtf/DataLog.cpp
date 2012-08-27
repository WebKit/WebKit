/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DataLog.h"
#include <stdarg.h>
#include <wtf/Threading.h>

#define DATA_LOG_TO_FILE 0

// Uncomment to force logging to the given file regardless of what the environment variable says.
// #define DATA_LOG_FILENAME "/tmp/WTFLog.txt"

namespace WTF {

#if DATA_LOG_TO_FILE
static FILE* file;

static void initializeLogFileOnce()
{
#ifdef DATA_LOG_FILENAME
    const char* filename = DATA_LOG_FILENAME;
#else
    const char* filename = getenv("WTF_DATA_LOG_FILENAME");
#endif
    if (filename) {
        file = fopen(filename, "w");
        if (!file)
            fprintf(stderr, "Warning: Could not open log file %s for writing.\n", filename);
    }
    if (!file)
        file = stderr;
    
    setvbuf(file, 0, _IONBF, 0); // Prefer unbuffered output, so that we get a full log upon crash or deadlock.
}

#if OS(DARWIN)
static pthread_once_t initializeLogFileOnceKey = PTHREAD_ONCE_INIT;
#endif

static void initializeLogFile()
{
#if OS(DARWIN)
    pthread_once(&initializeLogFileOnceKey, initializeLogFileOnce);
#else
    if (!file)
        initializeLogFileOnce();
#endif
}

FILE* dataFile()
{
    initializeLogFile();
    return file;
}
#else // DATA_LOG_TO_FILE
FILE* dataFile()
{
    return stderr;
}
#endif // DATA_LOG_TO_FILE

void dataLogV(const char* format, va_list argList)
{
    vfprintf(dataFile(), format, argList);
}

void dataLog(const char* format, ...)
{
    va_list argList;
    va_start(argList, format);
    dataLogV(format, argList);
    va_end(argList);
}

} // namespace WTF

