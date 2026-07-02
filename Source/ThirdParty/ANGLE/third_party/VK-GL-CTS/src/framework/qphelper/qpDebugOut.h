#ifndef _QPDEBUGOUT_H
#define _QPDEBUGOUT_H
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
 * \brief Debug output utilities.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

#include <stdarg.h>

DE_BEGIN_EXTERN_C

void qpPrint(const char *message);
void qpPrintf(const char *fmt, ...);
void qpPrintErrorf(const char *fmt, ...);
void qpPrintv(const char *fmt, va_list va);
void qpPrintErrorv(const char *fmt, va_list va);

void qpDief(const char *fmt, ...);
void qpDiev(const char *fmt, va_list va);

typedef bool (*writePtr)(int, const char *);
typedef bool (*writeFtmPtr)(int, const char *, va_list);

void qpRedirectOut(writePtr w, writeFtmPtr wftm);

DE_END_EXTERN_C

#endif /* _QPDEBUGOUT_H */
