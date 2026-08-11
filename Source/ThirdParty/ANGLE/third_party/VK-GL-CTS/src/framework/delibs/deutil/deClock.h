#ifndef _DECLOCK_H
#define _DECLOCK_H
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
 * \brief System clock.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

DE_BEGIN_EXTERN_C

/*--------------------------------------------------------------------*//*!
 * \brief Get time in microseconds.
 * \return Current time in microseconds.
 *
 * \note No reference point is specified for values returned by this function.
 *       Use only for measuring time spans.
 *       Monotonic clock is used if platform supports it.
 *//*--------------------------------------------------------------------*/
uint64_t deGetMicroseconds(void);

/*--------------------------------------------------------------------*//*!
 * \brief Get time in seconds since the epoch.
 * \return Current time in seconds since the epoch.
 *//*--------------------------------------------------------------------*/
uint64_t deGetTime(void);

DE_END_EXTERN_C

#endif /* _DECLOCK_H */
