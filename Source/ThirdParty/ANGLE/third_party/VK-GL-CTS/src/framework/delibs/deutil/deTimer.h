#ifndef _DETIMER_H
#define _DETIMER_H
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
 * \brief Periodic timer.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

DE_BEGIN_EXTERN_C

typedef void (*deTimerCallback)(void *arg);

typedef struct deTimer_s deTimer;

deTimer *deTimer_create(deTimerCallback callback, void *arg);
void deTimer_destroy(deTimer *timer);

bool deTimer_scheduleSingle(deTimer *timer, int milliseconds);
bool deTimer_scheduleInterval(deTimer *timer, int milliseconds);

bool deTimer_isActive(const deTimer *timer);
void deTimer_disable(deTimer *timer);

DE_END_EXTERN_C

#endif /* _DETIMER_H */
