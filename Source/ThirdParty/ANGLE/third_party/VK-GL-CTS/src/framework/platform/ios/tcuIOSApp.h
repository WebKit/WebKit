#ifndef _TCUIOSAPP_H
#define _TCUIOSAPP_H
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief iOS App Wrapper.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

typedef struct tcuIOSApp_s tcuIOSApp;

DE_BEGIN_EXTERN_C

tcuIOSApp *tcuIOSApp_create(void *view);
void tcuIOSApp_destroy(tcuIOSApp *app);

bool tcuIOSApp_iterate(tcuIOSApp *app);

DE_END_EXTERN_C

#endif /* _TCUIOSAPP_H */
