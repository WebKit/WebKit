#ifndef _QPCRASHHANDLER_H
#define _QPCRASHHANDLER_H
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
 * \brief System crash handler override
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

#if (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_OSX) || (DE_OS == DE_OS_IOS)
#define QP_USE_SIGNAL_HANDLER 1
#endif

typedef struct qpCrashHandler_s qpCrashHandler;

typedef void (*qpCrashHandlerFunc)(qpCrashHandler *crashHandler, void *userPtr);
typedef void (*qpWriteCrashInfoFunc)(void *userPtr, const char *infoString);

DE_BEGIN_EXTERN_C

qpCrashHandler *qpCrashHandler_create(qpCrashHandlerFunc handlerFunc, void *userPointer);
void qpCrashHandler_destroy(qpCrashHandler *crashHandler);
void qpCrashHandler_writeCrashInfo(qpCrashHandler *crashHandler, qpWriteCrashInfoFunc writeInfoFunc, void *userPointer);

DE_END_EXTERN_C

#endif /* _QPCRASHHANDLER_H */
