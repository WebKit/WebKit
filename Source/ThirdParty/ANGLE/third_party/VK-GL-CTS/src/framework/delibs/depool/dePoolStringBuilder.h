#ifndef _DEPOOLSTRINGBUILDER_H
#define _DEPOOLSTRINGBUILDER_H
/*-------------------------------------------------------------------------
 * drawElements Memory Pool Library
 * --------------------------------
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
 * \brief String builder.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "deMemPool.h"

typedef struct dePoolStringBuilder_s dePoolStringBuilder;

dePoolStringBuilder *dePoolStringBuilder_create(deMemPool *pool);

bool dePoolStringBuilder_appendString(dePoolStringBuilder *builder, const char *str);
bool dePoolStringBuilder_appendFormat(dePoolStringBuilder *builder, const char *format, ...);
/* \todo [2009-09-05 petri] Other appends? printf style? */

int dePoolStringBuilder_getLength(dePoolStringBuilder *builder);
char *dePoolStringBuilder_dupToString(dePoolStringBuilder *builder);
char *dePoolStringBuilder_dupToPool(dePoolStringBuilder *builder, deMemPool *dstPool);

#endif /* _DEPOOLSTRINGBUILDER_H */
