/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SQLiteFileSystem.h"

#include "ChromiumBridge.h"
#include <sqlite3.h>
#include <windows.h>

using namespace WebCore;

// Defined in Chromium's codebase in third_party/sqlite/src/os_win.c
extern "C" {
int chromium_sqlite3_initialize_win_sqlite3_file(sqlite3_file* file, HANDLE handle);
}

// Chromium's Windows implementation of SQLite VFS
namespace {

// Opens a file.
//
// vfs - pointer to the sqlite3_vfs object.
// fileName - the name of the file.
// id - the structure that will manipulate the newly opened file.
// desiredFlags - the desired open mode flags.
// usedFlags - the actual open mode flags that were used.
int chromiumOpen(sqlite3_vfs*, const char* fileName,
                 sqlite3_file* id, int desiredFlags, int* usedFlags)
{
    HANDLE h = ChromiumBridge::databaseOpenFile(fileName, desiredFlags);
    if (h == INVALID_HANDLE_VALUE) {
        if (desiredFlags & SQLITE_OPEN_READWRITE) {
            int newFlags = (desiredFlags | SQLITE_OPEN_READONLY) & ~SQLITE_OPEN_READWRITE;
            return chromiumOpen(0, fileName, id, newFlags, usedFlags);
        } else
            return SQLITE_CANTOPEN;
    }
    if (usedFlags) {
        if (desiredFlags & SQLITE_OPEN_READWRITE)
            *usedFlags = SQLITE_OPEN_READWRITE;
        else
            *usedFlags = SQLITE_OPEN_READONLY;
    }

    chromium_sqlite3_initialize_win_sqlite3_file(id, h);
    return SQLITE_OK;
}

// Deletes the given file.
//
// vfs - pointer to the sqlite3_vfs object.
// fileName - the name of the file.
// syncDir - determines if the directory to which this file belongs
//           should be synched after the file is deleted.
int chromiumDelete(sqlite3_vfs*, const char* fileName, int)
{
    return ChromiumBridge::databaseDeleteFile(fileName);
}

// Check the existance and status of the given file.
//
// vfs - pointer to the sqlite3_vfs object.
// fileName - the name of the file.
// flag - the type of test to make on this file.
// res - the result.
int chromiumAccess(sqlite3_vfs*, const char* fileName, int flag, int* res)
{
    DWORD attr = ChromiumBridge::databaseGetFileAttributes(fileName);
    switch (flag) {
    case SQLITE_ACCESS_READ:
    case SQLITE_ACCESS_EXISTS:
        *res = (attr != INVALID_FILE_ATTRIBUTES);
        break;
    case SQLITE_ACCESS_READWRITE:
        *res = ((attr & FILE_ATTRIBUTE_READONLY) == 0);
        break;
    default:
        return SQLITE_ERROR;
    }

    return SQLITE_OK;
}

// Turns a relative pathname into a full pathname.
//
// vfs - pointer to the sqlite3_vfs object.
// relativePath - the relative path.
// bufSize - the size of the output buffer in bytes.
// absolutePath - the output buffer where the absolute path will be stored.
int chromiumFullPathname(sqlite3_vfs* vfs, const char* relativePath,
                         int, char* absolutePath)
{
    // The renderer process doesn't need to know the absolute path of the file
    sqlite3_snprintf(vfs->mxPathname, absolutePath, "%s", relativePath);
    return SQLITE_OK;
}

#ifndef SQLITE_OMIT_LOAD_EXTENSION
// Returns NULL, thus disallowing loading libraries in the renderer process.
//
// vfs - pointer to the sqlite3_vfs object.
// fileName - the name of the shared library file.
void* chromiumDlOpen(sqlite3_vfs*, const char*)
{
    return 0;
}
#else
#define chromiumDlOpen 0
#endif // SQLITE_OMIT_LOAD_EXTENSION

} // namespace

namespace WebCore {

void SQLiteFileSystem::registerSQLiteVFS()
{
    sqlite3_vfs* win32_vfs = sqlite3_vfs_find("win32");
    static sqlite3_vfs chromium_vfs = {
        1,
        win32_vfs->szOsFile,
        win32_vfs->mxPathname,
        0,
        "chromium_vfs",
        win32_vfs->pAppData,
        chromiumOpen,
        chromiumDelete,
        chromiumAccess,
        chromiumFullPathname,
        chromiumDlOpen,
        win32_vfs->xDlError,
        win32_vfs->xDlSym,
        win32_vfs->xDlClose,
        win32_vfs->xRandomness,
        win32_vfs->xSleep,
        win32_vfs->xCurrentTime,
        win32_vfs->xGetLastError
    };
    sqlite3_vfs_register(&chromium_vfs, 0);
}

} // namespace WebCore
