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

#include "deFile.h"
#include "deMemory.h"

#if (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_OSX) || (DE_OS == DE_OS_IOS) || (DE_OS == DE_OS_ANDROID) || \
    (DE_OS == DE_OS_SYMBIAN) || (DE_OS == DE_OS_QNX) || (DE_OS == DE_OS_FUCHSIA)

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

struct deFile_s
{
    int fd;
};

bool deFileExists(const char *filename)
{
    struct stat st;
    int result = stat(filename, &st);
    return result == 0;
}

bool deDeleteFile(const char *filename)
{
    return unlink(filename) == 0;
}

deFile *deFile_createFromHandle(uintptr_t handle)
{
    int fd       = (int)handle;
    deFile *file = (deFile *)deCalloc(sizeof(deFile));
    if (!file)
    {
        close(fd);
        return file;
    }

    file->fd = fd;
    return file;
}

static int mapOpenMode(deFileMode mode)
{
    int flag = 0;

    /* Read, write or read and write access is required. */
    DE_ASSERT((mode & DE_FILEMODE_READ) != 0 || ((mode & DE_FILEMODE_WRITE) != 0));

    /* Create, open or create and open mode is required. */
    DE_ASSERT((mode & DE_FILEMODE_OPEN) != 0 || ((mode & DE_FILEMODE_CREATE) != 0));

    /* Require write when using create. */
    DE_ASSERT(!(mode & DE_FILEMODE_CREATE) || (mode & DE_FILEMODE_WRITE));

    /* Require write and open when using truncate */
    DE_ASSERT(!(mode & DE_FILEMODE_TRUNCATE) || ((mode & DE_FILEMODE_WRITE) && (mode & DE_FILEMODE_OPEN)));

    if (mode & DE_FILEMODE_READ)
        flag |= O_RDONLY;

    if (mode & DE_FILEMODE_WRITE)
        flag |= O_WRONLY;

    if (mode & DE_FILEMODE_TRUNCATE)
        flag |= O_TRUNC;

    if (mode & DE_FILEMODE_CREATE)
        flag |= O_CREAT;

    if (!(mode & DE_FILEMODE_OPEN))
        flag |= O_EXCL;

    return flag;
}

deFile *deFile_create(const char *filename, uint32_t mode)
{
    int fd = open(filename, mapOpenMode(mode), 0777);
    if (fd >= 0)
        return deFile_createFromHandle((uintptr_t)fd);
    else
        return NULL;
}

void deFile_destroy(deFile *file)
{
    close(file->fd);
    deFree(file);
}

bool deFile_setFlags(deFile *file, uint32_t flags)
{
    /* Non-blocking. */
    {
        int oldFlags = fcntl(file->fd, F_GETFL, 0);
        int newFlags = (flags & DE_FILE_NONBLOCKING) ? (oldFlags | O_NONBLOCK) : (oldFlags & ~O_NONBLOCK);
        if (fcntl(file->fd, F_SETFL, newFlags) != 0)
            return false;
    }

    /* Close on exec. */
    {
        int oldFlags = fcntl(file->fd, F_GETFD, 0);
        int newFlags = (flags & DE_FILE_CLOSE_ON_EXEC) ? (oldFlags | FD_CLOEXEC) : (oldFlags & ~FD_CLOEXEC);
        if (fcntl(file->fd, F_SETFD, newFlags) != 0)
            return false;
    }

    return true;
}

static int mapSeekPosition(deFilePosition position)
{
    switch (position)
    {
    case DE_FILEPOSITION_BEGIN:
        return SEEK_SET;
    case DE_FILEPOSITION_END:
        return SEEK_END;
    case DE_FILEPOSITION_CURRENT:
        return SEEK_CUR;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

bool deFile_seek(deFile *file, deFilePosition base, int64_t offset)
{
    return lseek(file->fd, (off_t)offset, mapSeekPosition(base)) >= 0;
}

int64_t deFile_getPosition(const deFile *file)
{
    return lseek(file->fd, 0, SEEK_CUR);
}

int64_t deFile_getSize(const deFile *file)
{
    int64_t size   = 0;
    int64_t curPos = lseek(file->fd, 0, SEEK_CUR);

    if (curPos < 0)
        return -1;

    size = lseek(file->fd, 0, SEEK_END);

    if (size < 0)
        return -1;

    lseek(file->fd, (off_t)curPos, SEEK_SET);

    return size;
}

static deFileResult mapReadWriteResult(int64_t numBytes)
{
    if (numBytes > 0)
        return DE_FILERESULT_SUCCESS;
    else if (numBytes == 0)
        return DE_FILERESULT_END_OF_FILE;
    else
        return errno == EAGAIN ? DE_FILERESULT_WOULD_BLOCK : DE_FILERESULT_ERROR;
}

deFileResult deFile_read(deFile *file, void *buf, int64_t bufSize, int64_t *numReadPtr)
{
    int64_t numRead = read(file->fd, buf, (size_t)bufSize);

    if (numReadPtr)
        *numReadPtr = numRead;

    return mapReadWriteResult(numRead);
}

deFileResult deFile_write(deFile *file, const void *buf, int64_t bufSize, int64_t *numWrittenPtr)
{
    int64_t numWritten = write(file->fd, buf, (size_t)bufSize);

    if (numWrittenPtr)
        *numWrittenPtr = numWritten;

    return mapReadWriteResult(numWritten);
}

#elif (DE_OS == DE_OS_WIN32)

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct deFile_s
{
    HANDLE handle;
};

bool deFileExists(const char *filename)
{
    return GetFileAttributes(filename) != INVALID_FILE_ATTRIBUTES;
}

bool deDeleteFile(const char *filename)
{
    return DeleteFile(filename) == TRUE;
}

deFile *deFile_createFromHandle(uintptr_t handle)
{
    deFile *file = (deFile *)deCalloc(sizeof(deFile));
    if (!file)
    {
        CloseHandle((HANDLE)handle);
        return file;
    }

    file->handle = (HANDLE)handle;
    return file;
}

deFile *deFile_create(const char *filename, uint32_t mode)
{
    DWORD access  = 0;
    DWORD create  = OPEN_EXISTING;
    HANDLE handle = NULL;

    /* Read, write or read and write access is required. */
    DE_ASSERT((mode & DE_FILEMODE_READ) != 0 || ((mode & DE_FILEMODE_WRITE) != 0));

    /* Create, open or create and open mode is required. */
    DE_ASSERT((mode & DE_FILEMODE_OPEN) != 0 || ((mode & DE_FILEMODE_CREATE) != 0));

    /* Require write when using create. */
    DE_ASSERT(!(mode & DE_FILEMODE_CREATE) || (mode & DE_FILEMODE_WRITE));

    /* Require write and open when using truncate */
    DE_ASSERT(!(mode & DE_FILEMODE_TRUNCATE) || ((mode & DE_FILEMODE_WRITE) && (mode & DE_FILEMODE_OPEN)));

    if (mode & DE_FILEMODE_READ)
        access |= GENERIC_READ;

    if (mode & DE_FILEMODE_WRITE)
        access |= GENERIC_WRITE;

    if ((mode & DE_FILEMODE_TRUNCATE))
    {
        if ((mode & DE_FILEMODE_CREATE) && (mode & DE_FILEMODE_OPEN))
            create = CREATE_ALWAYS;
        else if (mode & DE_FILEMODE_OPEN)
            create = TRUNCATE_EXISTING;
        else
            DE_ASSERT(false);
    }
    else
    {
        if ((mode & DE_FILEMODE_CREATE) && (mode & DE_FILEMODE_OPEN))
            create = OPEN_ALWAYS;
        else if (mode & DE_FILEMODE_CREATE)
            create = CREATE_NEW;
        else if (mode & DE_FILEMODE_OPEN)
            create = OPEN_EXISTING;
        else
            DE_ASSERT(false);
    }

    handle = CreateFile(filename, access, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, create,
                        FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE)
        return NULL;

    return deFile_createFromHandle((uintptr_t)handle);
}

void deFile_destroy(deFile *file)
{
    CloseHandle(file->handle);
    deFree(file);
}

bool deFile_setFlags(deFile *file, uint32_t flags)
{
    /* Non-blocking. */
    if (flags & DE_FILE_NONBLOCKING)
        return false; /* Not supported. */

    /* Close on exec. */
    if (!SetHandleInformation(file->handle, HANDLE_FLAG_INHERIT,
                              (flags & DE_FILE_CLOSE_ON_EXEC) ? HANDLE_FLAG_INHERIT : 0))
        return false;

    return true;
}

bool deFile_seek(deFile *file, deFilePosition base, int64_t offset)
{
    DWORD method  = 0;
    LONG lowBits  = (LONG)(offset & 0xFFFFFFFFll);
    LONG highBits = (LONG)((offset >> 32) & 0xFFFFFFFFll);

    switch (base)
    {
    case DE_FILEPOSITION_BEGIN:
        method = FILE_BEGIN;
        break;
    case DE_FILEPOSITION_END:
        method = FILE_END;
        break;
    case DE_FILEPOSITION_CURRENT:
        method = FILE_CURRENT;
        break;
    default:
        DE_ASSERT(false);
        return false;
    }

    return SetFilePointer(file->handle, lowBits, &highBits, method) != INVALID_SET_FILE_POINTER;
}

int64_t deFile_getPosition(const deFile *file)
{
    LONG highBits = 0;
    LONG lowBits  = SetFilePointer(file->handle, 0, &highBits, FILE_CURRENT);

    return (int64_t)(((uint64_t)highBits << 32) | (uint64_t)lowBits);
}

int64_t deFile_getSize(const deFile *file)
{
    DWORD highBits = 0;
    DWORD lowBits  = GetFileSize(file->handle, &highBits);

    return (int64_t)(((uint64_t)highBits << 32) | (uint64_t)lowBits);
}

static deFileResult mapReadWriteResult(BOOL retVal, DWORD numBytes)
{
    if (retVal && numBytes > 0)
        return DE_FILERESULT_SUCCESS;
    else if (retVal && numBytes == 0)
        return DE_FILERESULT_END_OF_FILE;
    else
    {
        DWORD error = GetLastError();

        if (error == ERROR_HANDLE_EOF)
            return DE_FILERESULT_END_OF_FILE;
        else
            return DE_FILERESULT_ERROR;
    }
}

deFileResult deFile_read(deFile *file, void *buf, int64_t bufSize, int64_t *numReadPtr)
{
    DWORD bufSize32 = (DWORD)bufSize;
    DWORD numRead32 = 0;
    BOOL result;

    /* \todo [2011-10-03 pyry] 64-bit IO. */
    DE_ASSERT((int64_t)bufSize32 == bufSize);

    result = ReadFile(file->handle, buf, bufSize32, &numRead32, NULL);

    if (numReadPtr)
        *numReadPtr = (int64_t)numRead32;

    return mapReadWriteResult(result, numRead32);
}

deFileResult deFile_write(deFile *file, const void *buf, int64_t bufSize, int64_t *numWrittenPtr)
{
    DWORD bufSize32    = (DWORD)bufSize;
    DWORD numWritten32 = 0;
    BOOL result;

    /* \todo [2011-10-03 pyry] 64-bit IO. */
    DE_ASSERT((int64_t)bufSize32 == bufSize);

    result = WriteFile(file->handle, buf, bufSize32, &numWritten32, NULL);

    if (numWrittenPtr)
        *numWrittenPtr = (int64_t)numWritten32;

    return mapReadWriteResult(result, numWritten32);
}

#else
#error Implement deFile for your OS.
#endif
