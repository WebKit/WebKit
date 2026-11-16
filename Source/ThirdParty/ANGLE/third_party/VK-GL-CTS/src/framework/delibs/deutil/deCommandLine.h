#ifndef _DECOMMANDLINE_H
#define _DECOMMANDLINE_H
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
 * \brief Command line parser.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

DE_BEGIN_EXTERN_C

typedef struct deCommandLine_s
{
    int numArgs;
    char **args;
    char *argBuf;
} deCommandLine;

deCommandLine *deCommandLine_parse(const char *cmdLine);
void deCommandLine_destroy(deCommandLine *cmdLine);

void deCommandLine_selfTest(void);

DE_END_EXTERN_C

#endif /* _DECOMMANDLINE_H */
