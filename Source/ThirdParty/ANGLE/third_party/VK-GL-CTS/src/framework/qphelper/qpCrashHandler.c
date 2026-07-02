/*-------------------------------------------------------------------------
 * drawElements Quality Program Helper Library
 * -------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief System handler handler override
 *//*--------------------------------------------------------------------*/

#include "qpCrashHandler.h"
#include "qpDebugOut.h"

#include "deThread.h"
#include "deMemory.h"
#include "deString.h"
#include "deMutex.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#if (DE_OS == DE_OS_UNIX)
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#if defined(__GLIBC__) || defined(__FreeBSD__)
#include <execinfo.h>
#endif
#endif

#if 0
#define DBGPRINT(X) qpPrintf X
#else
#define DBGPRINT(X)
#endif

/* Crash info write helper. */
static void writeInfoFormat(qpWriteCrashInfoFunc writeFunc, void *userPtr, const char *format, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    writeFunc(userPtr, buf);
}

/* Shared crash info. */
typedef enum qpCrashType_e
{
    QP_CRASHTYPE_SEGMENTATION_FAULT = 0,
    QP_CRASHTYPE_ASSERT,
    QP_CRASHTYPE_UNHANDLED_EXCEPTION,
    QP_CRASHTYPE_OTHER,

    QP_CRASHTYPE_LAST
} qpCrashType;

typedef struct qpCrashInfo_s
{
    qpCrashType type;
    const char *message;
    const char *file;
    int line;
} qpCrashInfo;

static void qpCrashInfo_init(qpCrashInfo *info)
{
    info->type    = QP_CRASHTYPE_LAST;
    info->message = NULL;
    info->file    = NULL;
    info->line    = 0;
}

static void qpCrashInfo_set(qpCrashInfo *info, qpCrashType type, const char *message, const char *file, int line)
{
    info->type    = type;
    info->message = message;
    info->file    = file;
    info->line    = line;
}

static void qpCrashInfo_write(qpCrashInfo *info, qpWriteCrashInfoFunc writeInfo, void *userPtr)
{
    switch (info->type)
    {
    case QP_CRASHTYPE_SEGMENTATION_FAULT:
        writeInfoFormat(writeInfo, userPtr, "Segmentation fault: '%s'\n", info->message);
        break;

    case QP_CRASHTYPE_UNHANDLED_EXCEPTION:
        writeInfoFormat(writeInfo, userPtr, "Unhandled exception: '%s'\n", info->message);
        break;

    case QP_CRASHTYPE_ASSERT:
        writeInfoFormat(writeInfo, userPtr, "Assertion '%s' failed at %s:%d\n", info->message, info->file, info->line);
        break;

    case QP_CRASHTYPE_OTHER:
    default:
        writeInfoFormat(writeInfo, userPtr, "Crash: '%s'\n", info->message);
        break;
    }
}

static void defaultWriteInfo(void *userPtr, const char *infoString)
{
    DE_UNREF(userPtr);
    qpPrintf("%s", infoString);
}

static void defaultCrashHandler(qpCrashHandler *crashHandler, void *userPtr)
{
    DE_UNREF(userPtr);
    qpCrashHandler_writeCrashInfo(crashHandler, defaultWriteInfo, NULL);
    qpDief("Test process crashed");
}

#if (DE_OS == DE_OS_WIN32) && (DE_COMPILER == DE_COMPILER_MSC)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* DbgHelp.h generates C4091 */
#pragma warning(push)
#pragma warning(disable : 4091)
#include <DbgHelp.h>
#pragma warning(pop)

struct qpCrashHandler_s
{
    qpCrashHandlerFunc crashHandlerFunc;
    void *handlerUserPointer;

    deMutex crashHandlerLock;
    qpCrashInfo crashInfo;
    uintptr_t crashAddress;

    LPTOP_LEVEL_EXCEPTION_FILTER oldExceptionFilter;
};

qpCrashHandler *g_crashHandler = NULL;

static LONG WINAPI unhandledExceptionFilter(struct _EXCEPTION_POINTERS *info)
{
    qpCrashType crashType = QP_CRASHTYPE_LAST;
    const char *reason    = NULL;

    /* Skip breakpoints. */
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT)
    {
        DBGPRINT(("qpCrashHandler::unhandledExceptionFilter(): breakpoint\n"));
        return EXCEPTION_CONTINUE_SEARCH;
    }

    /* If no handler present (how could that be?), don't handle. */
    if (g_crashHandler == NULL)
    {
        DBGPRINT(("qpCrashHandler::unhandledExceptionFilter(): no crash handler registered\n"));
        return EXCEPTION_CONTINUE_SEARCH;
    }

    /* If we have a debugger, let it handle the exception. */
    if (IsDebuggerPresent())
    {
        DBGPRINT(("qpCrashHandler::unhandledExceptionFilter(): debugger present\n"));
        return EXCEPTION_CONTINUE_SEARCH;
    }

    /* Acquire crash handler lock. Otherwise we might get strange behavior when multiple threads enter crash handler simultaneously. */
    deMutex_lock(g_crashHandler->crashHandlerLock);

    /* Map crash type. */
    switch (info->ExceptionRecord->ExceptionCode)
    {
    case EXCEPTION_ACCESS_VIOLATION:
        crashType = QP_CRASHTYPE_SEGMENTATION_FAULT;
        reason    = "Access violation";
        break;

    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        crashType = QP_CRASHTYPE_SEGMENTATION_FAULT;
        reason    = "Array bounds exceeded";
        break;

    case EXCEPTION_ILLEGAL_INSTRUCTION:
        crashType = QP_CRASHTYPE_OTHER;
        reason    = "Illegal instruction";
        break;

    case EXCEPTION_STACK_OVERFLOW:
        crashType = QP_CRASHTYPE_OTHER;
        reason    = "Stack overflow";
        break;

    default:
        /* \todo [pyry] Others. */
        crashType = QP_CRASHTYPE_OTHER;
        reason    = "";
        break;
    }

    /* Store reason. */
    qpCrashInfo_set(&g_crashHandler->crashInfo, crashType, reason, __FILE__, __LINE__);

    /* Store win32-specific crash info. */
    g_crashHandler->crashAddress = (uintptr_t)info->ExceptionRecord->ExceptionAddress;

    /* Handle the crash. */
    DBGPRINT(("qpCrashHandler::unhandledExceptionFilter(): handled quietly\n"));
    if (g_crashHandler->crashHandlerFunc != NULL)
        g_crashHandler->crashHandlerFunc(g_crashHandler, g_crashHandler->handlerUserPointer);

    /* Release lock. */
    deMutex_unlock(g_crashHandler->crashHandlerLock);

    return EXCEPTION_EXECUTE_HANDLER;
}

static void assertFailureCallback(const char *expr, const char *file, int line)
{
    /* Don't execute crash handler function if debugger is present. */
    if (IsDebuggerPresent())
    {
        DBGPRINT(("qpCrashHandler::assertFailureCallback(): debugger present\n"));
        return;
    }

    /* Acquire crash handler lock. */
    deMutex_lock(g_crashHandler->crashHandlerLock);

    /* Store info. */
    qpCrashInfo_set(&g_crashHandler->crashInfo, QP_CRASHTYPE_ASSERT, expr, file, line);
    g_crashHandler->crashAddress = 0;

    /* Handle the crash. */
    if (g_crashHandler->crashHandlerFunc != NULL)
        g_crashHandler->crashHandlerFunc(g_crashHandler, g_crashHandler->handlerUserPointer);

    /* Release lock. */
    deMutex_unlock(g_crashHandler->crashHandlerLock);
}

qpCrashHandler *qpCrashHandler_create(qpCrashHandlerFunc handlerFunc, void *userPointer)
{
    /* Allocate & initialize. */
    qpCrashHandler *handler = (qpCrashHandler *)deCalloc(sizeof(qpCrashHandler));
    DBGPRINT(("qpCrashHandler::create() -- Win32\n"));
    if (!handler)
        return handler;

    DE_ASSERT(g_crashHandler == NULL);

    handler->crashHandlerFunc   = handlerFunc ? handlerFunc : defaultCrashHandler;
    handler->handlerUserPointer = userPointer;

    /* Create lock for crash handler. \note Has to be recursive or otherwise crash in assert failure causes deadlock. */
    {
        deMutexAttributes attr;
        attr.flags                = DE_MUTEX_RECURSIVE;
        handler->crashHandlerLock = deMutex_create(&attr);

        if (!handler->crashHandlerLock)
        {
            deFree(handler);
            return NULL;
        }
    }

    qpCrashInfo_init(&handler->crashInfo);
    handler->crashAddress = 0;

    /* Unhandled exception filter. */
    handler->oldExceptionFilter = SetUnhandledExceptionFilter(unhandledExceptionFilter);

    /* Prevent nasty error dialog. */
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

    /* DE_ASSERT callback. */
    deSetAssertFailureCallback(assertFailureCallback);

    g_crashHandler = handler;
    return handler;
}

void qpCrashHandler_destroy(qpCrashHandler *handler)
{
    DBGPRINT(("qpCrashHandler::destroy()\n"));

    DE_ASSERT(g_crashHandler == handler);

    deSetAssertFailureCallback(NULL);
    SetUnhandledExceptionFilter(handler->oldExceptionFilter);

    g_crashHandler = NULL;
    deFree(handler);
}

enum
{
    MAX_NAME_LENGTH = 64
};

void qpCrashHandler_writeCrashInfo(qpCrashHandler *handler, qpWriteCrashInfoFunc writeInfo, void *userPtr)
{
    void *addresses[32];
    HANDLE process;

    /* Symbol info struct. */
    uint8_t symInfoStorage[sizeof(SYMBOL_INFO) + MAX_NAME_LENGTH];
    SYMBOL_INFO *symInfo = (SYMBOL_INFO *)symInfoStorage;

    DE_STATIC_ASSERT(sizeof(TCHAR) == sizeof(uint8_t));

    /* Write basic info. */
    qpCrashInfo_write(&handler->crashInfo, writeInfo, userPtr);

    /* Acquire process handle and initialize symbols. */
    process = GetCurrentProcess();

    /* Write backtrace. */
    if (SymInitialize(process, NULL, TRUE))
    {
        int batchStart     = 0;
        int globalFrameNdx = 0;

        /* Initialize symInfo. */
        deMemset(symInfo, 0, sizeof(symInfoStorage));
        symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
        symInfo->MaxNameLen   = MAX_NAME_LENGTH;

        /* Print address and symbol where crash happened. */
        if (handler->crashAddress != 0)
        {
            BOOL symInfoOk = SymFromAddr(process, (DWORD64)handler->crashAddress, 0, symInfo);

            writeInfoFormat(writeInfo, userPtr, "  at %p %s%s\n", handler->crashAddress,
                            symInfoOk ? symInfo->Name : "(unknown)", symInfoOk ? "()" : "");
        }

        writeInfo(userPtr, "Backtrace:\n");

        for (;;)
        {
            int curFrame;
            int numInBatch;

            /* Get one batch. */
            numInBatch = CaptureStackBackTrace(batchStart, DE_LENGTH_OF_ARRAY(addresses), addresses, NULL);

            for (curFrame = 0; curFrame < numInBatch; curFrame++)
            {
                BOOL symInfoOk = SymFromAddr(process, (DWORD64)addresses[curFrame], 0, symInfo);

                writeInfoFormat(writeInfo, userPtr, "  %2d: %p %s%s\n", globalFrameNdx++, addresses[curFrame],
                                symInfoOk ? symInfo->Name : "(unknown)", symInfoOk ? "()" : "");
            }

            batchStart += numInBatch;

            /* Check if we hit end of stack trace. */
            if (numInBatch == 0 || numInBatch < DE_LENGTH_OF_ARRAY(addresses))
                break;
        }
    }
}

#else /* posix / generic implementation */

#if defined(QP_USE_SIGNAL_HANDLER)
#include <signal.h>
#endif

#if defined(QP_USE_SIGNAL_HANDLER)

typedef struct SignalInfo_s
{
    int signalNum;
    qpCrashType type;
    const char *name;
} SignalInfo;

static const SignalInfo s_signals[] = {
    {SIGABRT, QP_CRASHTYPE_UNHANDLED_EXCEPTION, "SIGABRT"}, {SIGILL, QP_CRASHTYPE_OTHER, "SIGILL"},
    {SIGSEGV, QP_CRASHTYPE_SEGMENTATION_FAULT, "SIGSEGV"},  {SIGFPE, QP_CRASHTYPE_OTHER, "SIGFPE"},
    {SIGBUS, QP_CRASHTYPE_SEGMENTATION_FAULT, "SIGBUS"},    {SIGPIPE, QP_CRASHTYPE_OTHER, "SIGPIPE"}};

#endif /* QP_USE_SIGNAL_HANDLER */

struct qpCrashHandler_s
{
    qpCrashHandlerFunc crashHandlerFunc;
    void *handlerUserPointer;

    qpCrashInfo crashInfo;
    int crashSignal;

#if defined(QP_USE_SIGNAL_HANDLER)
    struct sigaction oldHandlers[DE_LENGTH_OF_ARRAY(s_signals)];
#endif
};

qpCrashHandler *g_crashHandler = NULL;

static void assertFailureCallback(const char *expr, const char *file, int line)
{
    /* Store info. */
    qpCrashInfo_set(&g_crashHandler->crashInfo, QP_CRASHTYPE_ASSERT, expr, file, line);

    /* Handle the crash. */
    if (g_crashHandler->crashHandlerFunc != NULL)
        g_crashHandler->crashHandlerFunc(g_crashHandler, g_crashHandler->handlerUserPointer);
}

#if defined(QP_USE_SIGNAL_HANDLER)

static const SignalInfo *getSignalInfo(int sigNum)
{
    int ndx;
    for (ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_signals); ndx++)
    {
        if (s_signals[ndx].signalNum == sigNum)
            return &s_signals[ndx];
    }
    return NULL;
}

static void signalHandler(int sigNum)
{
    const SignalInfo *info = getSignalInfo(sigNum);
    qpCrashType type       = info ? info->type : QP_CRASHTYPE_OTHER;
    const char *name       = info ? info->name : "Unknown signal";

    qpCrashInfo_set(&g_crashHandler->crashInfo, type, name, NULL, 0);

    if (g_crashHandler->crashHandlerFunc != NULL)
        g_crashHandler->crashHandlerFunc(g_crashHandler, g_crashHandler->handlerUserPointer);
}

#endif /* QP_USE_SIGNAL_HANDLER */

qpCrashHandler *qpCrashHandler_create(qpCrashHandlerFunc handlerFunc, void *userPointer)
{
    /* Allocate & initialize. */
    qpCrashHandler *handler = (qpCrashHandler *)deCalloc(sizeof(qpCrashHandler));
    DBGPRINT(("qpCrashHandler::create()\n"));
    if (!handler)
        return handler;

    DE_ASSERT(g_crashHandler == NULL);

    handler->crashHandlerFunc   = handlerFunc ? handlerFunc : defaultCrashHandler;
    handler->handlerUserPointer = userPointer;

    qpCrashInfo_init(&handler->crashInfo);

    g_crashHandler = handler;

    /* DE_ASSERT callback. */
    deSetAssertFailureCallback(assertFailureCallback);

#if defined(QP_USE_SIGNAL_HANDLER)
    /* Register signal handlers. */
    {
        struct sigaction action;
        int sigNdx;

        sigemptyset(&action.sa_mask);
        action.sa_handler = signalHandler;
        action.sa_flags   = 0;

        for (sigNdx = 0; sigNdx < DE_LENGTH_OF_ARRAY(s_signals); sigNdx++)
            sigaction(s_signals[sigNdx].signalNum, &action, &handler->oldHandlers[sigNdx]);
    }
#endif

    return handler;
}

void qpCrashHandler_destroy(qpCrashHandler *handler)
{
    DBGPRINT(("qpCrashHandler::destroy()\n"));

    DE_ASSERT(g_crashHandler == handler);

    deSetAssertFailureCallback(NULL);

#if defined(QP_USE_SIGNAL_HANDLER)
    /* Restore old handlers. */
    {
        int sigNdx;
        for (sigNdx = 0; sigNdx < DE_LENGTH_OF_ARRAY(s_signals); sigNdx++)
            sigaction(s_signals[sigNdx].signalNum, &handler->oldHandlers[sigNdx], NULL);
    }
#endif

    g_crashHandler = NULL;

    deFree(handler);
}

#if (DE_PTR_SIZE == 8)
#define PTR_FMT "0x%016"
#elif (DE_PTR_SIZE == 4)
#define PTR_FMT "0x%08"
#else
#error Unknwon DE_PTR_SIZE
#endif

void qpCrashHandler_writeCrashInfo(qpCrashHandler *crashHandler, qpWriteCrashInfoFunc writeInfo, void *userPtr)
{
    qpCrashInfo_write(&crashHandler->crashInfo, writeInfo, userPtr);

#if (DE_OS == DE_OS_UNIX && (defined(__GLIBC__) || defined(__FreeBSD__)))
    {
        char tmpFileName[] = "backtrace-XXXXXX";
        int tmpFile        = mkstemp(tmpFileName);

        if (tmpFile == -1)
        {
            writeInfoFormat(writeInfo, userPtr, "Failed to create tmpfile '%s' for the backtrace %s.", tmpFileName,
                            strerror(errno));
            return;
        }
        else
        {
            void *symbols[32];
            int symbolCount;
            int symbolNdx;

            /* Remove file from filesystem. */
            remove(tmpFileName);

            symbolCount = backtrace(symbols, DE_LENGTH_OF_ARRAY(symbols));
            backtrace_symbols_fd(symbols, symbolCount, tmpFile);

            if (lseek(tmpFile, 0, SEEK_SET) < 0)
            {
                writeInfoFormat(writeInfo, userPtr, "Failed to seek to the beginning of the trace file %s.",
                                strerror(errno));
                close(tmpFile);
                return;
            }

            for (symbolNdx = 0; symbolNdx < symbolCount; symbolNdx++)
            {
                char nameBuffer[256];
                size_t symbolNameLength = 0;
                char c;

                {
                    const int ret = snprintf(nameBuffer, DE_LENGTH_OF_ARRAY(nameBuffer), PTR_FMT PRIXPTR " : ",
                                             (uintptr_t)symbols[symbolNdx]);

                    if (ret < 0)
                    {
                        writeInfoFormat(writeInfo, userPtr, "Failed to print symbol pointer.");
                        symbolNameLength = 0;
                    }
                    else if (ret >= DE_LENGTH_OF_ARRAY(nameBuffer))
                    {
                        symbolNameLength                               = DE_LENGTH_OF_ARRAY(nameBuffer) - 1;
                        nameBuffer[DE_LENGTH_OF_ARRAY(nameBuffer) - 1] = '\0';
                    }
                    else
                        symbolNameLength = ret;
                }

                for (;;)
                {
                    if (read(tmpFile, &c, 1) == 1)
                    {
                        if (c == '\n')
                        {
                            /* Flush nameBuffer and move to next symbol. */
                            nameBuffer[symbolNameLength] = '\0';
                            writeInfo(userPtr, nameBuffer);
                            break;
                        }
                        else
                        {
                            /* Add character to buffer if there is still space left. */
                            if (symbolNameLength + 1 < DE_LENGTH_OF_ARRAY(nameBuffer))
                            {
                                nameBuffer[symbolNameLength] = c;
                                symbolNameLength++;
                            }
                        }
                    }
                    else
                    {
                        /* Flush nameBuffer. */
                        nameBuffer[symbolNameLength] = '\0';
                        writeInfo(userPtr, nameBuffer);

                        /* Temp file ended unexpectedly? */
                        writeInfoFormat(writeInfo, userPtr, "Unexpected EOF reading backtrace file '%s'", tmpFileName);
                        close(tmpFile);
                        tmpFile = -1;

                        break;
                    }
                }

                if (tmpFile == -1)
                    break;
            }

            if (tmpFile != -1)
                close(tmpFile);
        }
    }
#endif
}

#endif /* generic */
