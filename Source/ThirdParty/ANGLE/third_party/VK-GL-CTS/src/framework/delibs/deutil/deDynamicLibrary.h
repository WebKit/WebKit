#ifndef _DEDYNAMICLIBRARY_H
#define _DEDYNAMICLIBRARY_H
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
 * \brief Dynamic link library abstraction.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

DE_BEGIN_EXTERN_C

/* Dynamic link library. */
typedef struct deDynamicLibrary_s deDynamicLibrary;

/*--------------------------------------------------------------------*//*!
 * \brief Open dynamic library.
 * \param fileName Name or path to dynamic library.
 * \return Dynamic library handle, or NULL on failure.
 *
 * This function opens and loads dynamic library to current process.
 * If library is already loaded, its reference count will be increased.
 *//*--------------------------------------------------------------------*/
deDynamicLibrary *deDynamicLibrary_open(const char *fileName);

/*--------------------------------------------------------------------*//*!
 * \brief Load function symbol from dynamic library.
 * \param library Dynamic library
 * \param symbolName Name of function symbol
 * \return Function pointer or NULL on failure
 * \note Returned pointers will be invalidated if dynamic library is
 *       unloaded.
 *//*--------------------------------------------------------------------*/
deFunctionPtr deDynamicLibrary_getFunction(const deDynamicLibrary *library, const char *symbolName);

/*--------------------------------------------------------------------*//*!
 * \brief Close dynamic library.
 * \param library Dynamic library
 *
 * Closing library handle decrements reference count. Library is unloaded
 * from process if reference count reaches zero.
 *//*--------------------------------------------------------------------*/
void deDynamicLibrary_close(deDynamicLibrary *library);

DE_END_EXTERN_C

#endif /* _DEDYNAMICLIBRARY_H */
