#ifndef _QPWATCHDOG_H
#define _QPWATCHDOG_H
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
 * \brief Watch dog for detecting timeouts
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

typedef struct qpWatchDog_s qpWatchDog;

typedef enum qpTimeoutReason_e
{
    QP_TIMEOUT_REASON_INTERVAL_LIMIT = 0,
    QP_TIMEOUT_REASON_TOTAL_LIMIT,

    QP_TIMEOUT_REASON_LAST
} qpTimeoutReason;

typedef void (*qpWatchDogFunc)(qpWatchDog *dog, void *userPtr, qpTimeoutReason reason);

DE_BEGIN_EXTERN_C

qpWatchDog *qpWatchDog_create(qpWatchDogFunc watchDogFunc, void *userPtr, int totalTimeLimitSecs,
                              int intervalTimeLimitSecs);
void qpWatchDog_destroy(qpWatchDog *dog);
void qpWatchDog_reset(qpWatchDog *dog);
void qpWatchDog_touch(qpWatchDog *dog);
void qpWatchDog_touchAndDisableIntervalTimeLimit(qpWatchDog *dog);
void qpWatchDog_touchAndEnableIntervalTimeLimit(qpWatchDog *dog);

DE_END_EXTERN_C

#endif /* _QPWATCHDOG_H */
