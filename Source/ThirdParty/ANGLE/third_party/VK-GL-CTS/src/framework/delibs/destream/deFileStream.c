/*-------------------------------------------------------------------------
 * drawElements Stream Library
 * ---------------------------
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
 * \brief Stream wrapper for deFile
 *//*--------------------------------------------------------------------*/
#include "deFileStream.h"

#include <stdlib.h>

typedef struct FileStream_s
{
    deFile *file;
    deStreamStatus status;
    const char *error;
} FileStream;

static deStreamResult fileIOStream_read(deStreamData *stream, void *buf, int32_t bufSize, int32_t *numRead)
{
    int64_t _numRead       = 0;
    FileStream *fileStream = (FileStream *)stream;

    deFileResult result = deFile_read(fileStream->file, buf, bufSize, &_numRead);
    *numRead            = (int32_t)_numRead;

    switch (result)
    {
    case DE_FILERESULT_SUCCESS:
        return DE_STREAMRESULT_SUCCESS;

    case DE_FILERESULT_ERROR:
        fileStream->error  = "deFile: DE_FILERESULT_ERROR";
        fileStream->status = DE_STREAMSTATUS_ERROR;
        return DE_STREAMRESULT_ERROR;

    case DE_FILERESULT_END_OF_FILE:
        return DE_STREAMRESULT_END_OF_STREAM;

    default:
        fileStream->error  = "Uknown: DE_FILERESULT";
        fileStream->status = DE_STREAMSTATUS_ERROR;
        return DE_STREAMRESULT_ERROR;
    }
}

static deStreamResult fileIOStream_write(deStreamData *stream, const void *buf, int32_t bufSize, int32_t *numWritten)
{
    int64_t _numWritten    = 0;
    FileStream *fileStream = (FileStream *)stream;

    deFileResult result = deFile_write(fileStream->file, buf, bufSize, &_numWritten);
    *numWritten         = (int32_t)_numWritten;

    switch (result)
    {
    case DE_FILERESULT_SUCCESS:
        return DE_STREAMRESULT_SUCCESS;

    case DE_FILERESULT_ERROR:
        fileStream->error  = "deFile: DE_FILERESULT_ERROR";
        fileStream->status = DE_STREAMSTATUS_ERROR;
        return DE_STREAMRESULT_ERROR;

    case DE_FILERESULT_END_OF_FILE:
        return DE_STREAMRESULT_END_OF_STREAM;

    default:
        fileStream->error  = "Uknown: DE_FILERESULT";
        fileStream->status = DE_STREAMSTATUS_ERROR;
        return DE_STREAMRESULT_ERROR;
    }
}

static const char *fileIOStream_getError(deStreamData *stream)
{
    FileStream *fileStream = (FileStream *)stream;
    /* \note [mika] There is only error reporting through return value in deFile */
    return fileStream->error;
}

static deStreamResult fileIOStream_flush(deStreamData *stream)
{
    /* \todo mika deFile doesn't have flush, how should this be handled? */
    DE_UNREF(stream);

    return DE_STREAMRESULT_SUCCESS;
}

static deStreamResult fileIOStream_deinit(deStreamData *stream)
{
    FileStream *fileStream = (FileStream *)stream;

    deFile_destroy(fileStream->file);

    free(fileStream);

    return DE_STREAMRESULT_SUCCESS;
}

static deStreamStatus fileIOStrem_getStatus(deStreamData *stream)
{
    FileStream *fileStream = (FileStream *)stream;
    return fileStream->status;
}

static const deIOStreamVFTable fileIOStreamVFTable = {fileIOStream_read,  fileIOStream_write,  fileIOStream_getError,
                                                      fileIOStream_flush, fileIOStream_deinit, fileIOStrem_getStatus};

static const deIOStreamVFTable fileInStreamVFTable = {
    fileIOStream_read, NULL, fileIOStream_getError, NULL, fileIOStream_deinit, fileIOStrem_getStatus};

static const deIOStreamVFTable fileOutStreamVFTable = {
    NULL, fileIOStream_write, fileIOStream_getError, fileIOStream_flush, fileIOStream_deinit, fileIOStrem_getStatus};

void fileIOStream_init(deIOStream *stream, const char *filename, deFileMode mode)
{
    FileStream *fileStream = NULL;

    DE_ASSERT(stream);

    fileStream = malloc(sizeof(FileStream));

    /* \note mika Check that file is readable and writeable, currently not supported by deFile */
    stream->vfTable    = &fileIOStreamVFTable;
    stream->streamData = (deStreamData *)fileStream;

    fileStream->file   = deFile_create(filename, mode);
    fileStream->status = DE_STREAMSTATUS_GOOD;
    fileStream->error  = NULL;

    if (!fileStream->file)
        fileStream->status = DE_STREAMSTATUS_ERROR;
}

void deFileInStream_init(deInStream *stream, const char *filename, deFileMode mode)
{
    FileStream *fileStream = NULL;

    DE_ASSERT(stream);

    fileStream = malloc(sizeof(FileStream));

    /* \note mika Check that file is readable, currently not supported by deFile */
    stream->ioStream.vfTable    = &fileInStreamVFTable;
    stream->ioStream.streamData = (deStreamData *)fileStream;

    fileStream->file   = deFile_create(filename, mode);
    fileStream->status = DE_STREAMSTATUS_GOOD;
    fileStream->error  = NULL;

    if (!fileStream->file)
        fileStream->status = DE_STREAMSTATUS_ERROR;
}

void deFileOutStream_init(deOutStream *stream, const char *filename, deFileMode mode)
{
    FileStream *fileStream = NULL;

    DE_ASSERT(stream);

    fileStream = malloc(sizeof(FileStream));

    /* \note mika Check that file is writeable, currently not supported by deFile */
    stream->ioStream.vfTable    = &fileOutStreamVFTable;
    stream->ioStream.streamData = (deStreamData *)fileStream;

    fileStream->file   = deFile_create(filename, mode);
    fileStream->status = DE_STREAMSTATUS_GOOD;
    fileStream->error  = NULL;

    if (!fileStream->file)
        fileStream->status = DE_STREAMSTATUS_ERROR;
}
