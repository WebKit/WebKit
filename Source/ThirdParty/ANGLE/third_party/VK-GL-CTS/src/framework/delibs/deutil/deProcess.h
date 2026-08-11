#ifndef _DEPROCESS_H
#define _DEPROCESS_H
/*-------------------------------------------------------------------------
 * drawElements Utility Library
 * ----------------------------
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
 * \brief Process abstraction.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "deFile.h"

DE_BEGIN_EXTERN_C

/* Process types. */
typedef struct deProcess_s deProcess;

deProcess *deProcess_create(void);
void deProcess_destroy(deProcess *process);

bool deProcess_start(deProcess *process, const char *commandLine, const char *workingDirectory);
bool deProcess_isRunning(deProcess *process);
bool deProcess_waitForFinish(deProcess *process);

const char *deProcess_getLastError(const deProcess *process);
int deProcess_getExitCode(const deProcess *process);

/* Non-blocking operations. */
bool deProcess_terminate(deProcess *process);
bool deProcess_kill(deProcess *process);

/* Files are owned by process - don't call deFile_destroy(). */
deFile *deProcess_getStdIn(deProcess *process);
deFile *deProcess_getStdOut(deProcess *process);
deFile *deProcess_getStdErr(deProcess *process);

bool deProcess_closeStdIn(deProcess *process);
bool deProcess_closeStdOut(deProcess *process);
bool deProcess_closeStdErr(deProcess *process);

DE_END_EXTERN_C

#endif /* _DEPROCESS_H */
