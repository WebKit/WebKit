#ifndef _DEFILE_H
#define _DEFILE_H
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
 * \brief File abstraction.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

DE_BEGIN_EXTERN_C

/* File types. */
typedef struct deFile_s deFile;

typedef enum deFileMode_e
{
    DE_FILEMODE_READ     = (1 << 0), /*!< Read access to file.                                            */
    DE_FILEMODE_WRITE    = (1 << 2), /*!< Write access to file.                                            */
    DE_FILEMODE_CREATE   = (1 << 3), /*!< Create file if it doesn't exist. Requires DE_FILEMODE_WRITE.    */
    DE_FILEMODE_OPEN     = (1 << 4), /*!< Open file if it exists.                                        */
    DE_FILEMODE_TRUNCATE = (1 << 5)  /*!< Truncate content of file. Requires DE_FILEMODE_OPEN.            */
} deFileMode;

typedef enum deFileFlag_e
{
    DE_FILE_NONBLOCKING   = (1 << 0), /*!< Set to non-blocking mode. Not supported on Win32!                */
    DE_FILE_CLOSE_ON_EXEC = (1 << 1)
} deFileFlag;

typedef enum deFileResult_e
{
    DE_FILERESULT_SUCCESS     = 0,
    DE_FILERESULT_END_OF_FILE = 1,
    DE_FILERESULT_WOULD_BLOCK = 2,
    DE_FILERESULT_ERROR       = 3,

    DE_FILERESULT_LAST
} deFileResult;

typedef enum deFilePosition_e
{
    DE_FILEPOSITION_BEGIN   = 0,
    DE_FILEPOSITION_END     = 1,
    DE_FILEPOSITION_CURRENT = 2,

    DE_FILEPOSITION_LAST
} deFilePosition;

/* File API. */

bool deFileExists(const char *filename);
bool deDeleteFile(const char *filename);

deFile *deFile_create(const char *filename, uint32_t mode);
deFile *deFile_createFromHandle(uintptr_t handle);
void deFile_destroy(deFile *file);

bool deFile_setFlags(deFile *file, uint32_t flags);

int64_t deFile_getPosition(const deFile *file);
bool deFile_seek(deFile *file, deFilePosition base, int64_t offset);
int64_t deFile_getSize(const deFile *file);

deFileResult deFile_read(deFile *file, void *buf, int64_t bufSize, int64_t *numRead);
deFileResult deFile_write(deFile *file, const void *buf, int64_t bufSize, int64_t *numWritten);

DE_END_EXTERN_C

#endif /* _DEFILE_H */
