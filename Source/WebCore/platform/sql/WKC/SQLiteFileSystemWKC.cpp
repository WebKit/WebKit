/*
 * Copyright (C) 2011-2014 ACCESS CO., LTD. All rights reserved.
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

#if ENABLE(SQL_DATABASE)

#include <wkc/wkcclib.h>
#include <wkc/wkcpeer.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sqlite3.h>

#ifdef __WKC_IMPLICIT_INCLUDE_SYSSTAT
#include <sys/stat.h>
#endif

#if COMPILER(RVCT)
// for missing errno
# include <errno-posix.h>
#endif

#define WKC_PENDING_LOCK_BYTE  (0x40000000)
#define WKC_RESERVED_LOCK_BYTE (WKC_PENDING_LOCK_BYTE+1)
#define WKC_SHARED_LOCK_FIRST  (WKC_PENDING_LOCK_BYTE+2)
#define WKC_SHARED_SIZE        510


namespace WebCore {

struct wkcFile_ {
    const sqlite3_io_methods *fMethod;
    void *fFile;
    char fFileName[MAX_PATH + 1];
    int fIsDeleteOnClose;
    int fLockType;
    int fLastError;
};
typedef struct wkcFile_ wkcFile;

static int
wkcClose(sqlite3_file* in_file)
{
    int err;
    wkcFile *file = (wkcFile *)in_file;

    err = wkcFileFClosePeer(file->fFile);
    if (err) {
        file->fLastError = wkcFileErrnoPeer();
    }

    if (err == 0 && file->fIsDeleteOnClose) {
        err = wkcFileUnlinkPeer(file->fFileName);
    }

    return (err == 0) ? SQLITE_OK : SQLITE_IOERR_CLOSE;
}

static int
wkcRead(sqlite3_file *id, void *pBuf, int amt, sqlite3_int64 offset)
{
    int err;
    wkcFile *file;
    wkc_uint64 reads;

    file = (wkcFile*)id;
    err = wkcFileFSeekPeer(file->fFile, offset, SEEK_SET);
    if (err != 0) {
        file->fLastError = wkcFileErrnoPeer();
        return SQLITE_IOERR_READ;
    }
    reads = wkcFileFReadPeer(pBuf, 1, amt, file->fFile);
    if (wkcFileFErrorPeer(file->fFile)) {
        file->fLastError = wkcFileErrnoPeer();
        return SQLITE_IOERR_READ;
    }
    if (reads < amt) {
        /* Unread parts of the buffer must be zero-filled */
        memset(&((char*)pBuf)[reads], 0, amt - reads);
        return SQLITE_IOERR_SHORT_READ;
    }

    return SQLITE_OK;
}

static int
wkcWrite(sqlite3_file *id, const void *pBuf, int amt, sqlite3_int64 offset)
{
    int err = 0;
    wkcFile *file;
    wkc_uint64 writes = 0;
    wkc_uint64 rest;
    char* pos;

    file = (wkcFile*)id;
    err = wkcFileFSeekPeer(file->fFile, offset, SEEK_SET);
    if (err != 0) {
        file->fLastError = wkcFileErrnoPeer();
        return SQLITE_IOERR_WRITE;
    }
    for (rest = (wkc_uint64)amt, pos = (char*)pBuf; rest > 0; pos += writes, rest -= writes) {
        writes = wkcFileFWritePeer(pos, 1, rest, file->fFile);
        if (!writes) {
            err = wkcFileErrnoPeer();
            break;
        }
    }
    if (!writes) {
        if (err == ENOSPC) {
            return SQLITE_FULL;
        } else {
            file->fLastError = err;
            return SQLITE_IOERR_WRITE;
        }
    }
    wkcFileFFlushPeer(file->fFile);

    return SQLITE_OK;
}

static int
wkcTruncate(sqlite3_file *id, sqlite3_int64 nByte)
{
    int err = 0;
    wkcFile *file;

    file = (wkcFile*)id;
    err = wkcFileFTruncatePeer(file->fFile, nByte);

    if (err) {
        file->fLastError = wkcFileErrnoPeer();
    }

    return (err == 0) ? SQLITE_OK : SQLITE_IOERR_TRUNCATE;
}

static int
wkcSync(sqlite3_file *id, int flags)
{
    int err = 0;
    wkcFile *file;

    file = (wkcFile*)id;
    err = wkcFileFSyncPeer(file->fFile);
    if (err != 0) {
        file->fLastError = wkcFileErrnoPeer();
    }

    return (err == 0) ? SQLITE_OK : SQLITE_IOERR_FSYNC;
}

static int
wkcFileSize(sqlite3_file *id, sqlite3_int64 *pSize)
{
    int err = 0;
    struct stat st;
    wkcFile *file;

    file = (wkcFile*)id;
    err = wkcFileFStatPeer(file->fFile, &st);
    if (err != 0) {
        file->fLastError = wkcFileErrnoPeer();
        return SQLITE_IOERR_FSTAT;
    }
    *pSize = st.st_size;

    return SQLITE_OK;
}

static int
wkcLock(sqlite3_file *id, int locktype)
{
    int ret = 1;
    wkcFile *file;
    int pendingLock;
    int currentLockType;

    file = (wkcFile*)id;
    if(file->fLockType >= locktype){
        return SQLITE_OK;
    }

    currentLockType = file->fLockType;
    // temporary pending lock
    if((file->fLockType == SQLITE_LOCK_NONE) || ((locktype == SQLITE_LOCK_EXCLUSIVE) && (file->fLockType == SQLITE_LOCK_RESERVED))) {
        for(int cnt = 3; cnt>0; cnt--) {
            ret = wkcFileLockPeer(file->fFile, WKC_FILELOCK_EXCLUSIVE, WKC_PENDING_LOCK_BYTE, 1);
            if (ret) {
                file->fLastError = wkcFileErrnoPeer();
                break;
            }
            wkc_usleep(1000);
        }
    }
    pendingLock = ret;

    // shared lock
    if(ret && locktype == SQLITE_LOCK_SHARED) {
        ret = wkcFileLockPeer(file->fFile, WKC_FILELOCK_SHARED, WKC_SHARED_LOCK_FIRST, WKC_SHARED_SIZE);
        if(ret){
            currentLockType = SQLITE_LOCK_SHARED;
        } else {
            file->fLastError = wkcFileErrnoPeer();
        }
    }

    if(ret && locktype == SQLITE_LOCK_RESERVED) {
        ret = wkcFileLockPeer(file->fFile, WKC_FILELOCK_EXCLUSIVE, WKC_RESERVED_LOCK_BYTE, 1);
        if(ret){
            currentLockType = SQLITE_LOCK_RESERVED;
        } else {
            file->fLastError = wkcFileErrnoPeer();
        }
    }

    if(ret && locktype == SQLITE_LOCK_PENDING) {
        currentLockType = SQLITE_LOCK_PENDING;
        pendingLock = 0;
    }

    if(ret && locktype == SQLITE_LOCK_EXCLUSIVE) {
        wkcFileUnlockPeer(file->fFile, WKC_FILELOCK_SHARED, WKC_SHARED_LOCK_FIRST, WKC_SHARED_SIZE);
        ret = wkcFileLockPeer(file->fFile, WKC_FILELOCK_EXCLUSIVE, WKC_SHARED_LOCK_FIRST, WKC_SHARED_SIZE);
        if(ret){
            currentLockType = SQLITE_LOCK_EXCLUSIVE;
        } else {
            file->fLastError = wkcFileErrnoPeer();
        }
    }

    if(pendingLock && locktype == SQLITE_LOCK_SHARED){
        wkcFileUnlockPeer(file->fFile, WKC_FILELOCK_EXCLUSIVE, WKC_PENDING_LOCK_BYTE, 1);
    }

    file->fLockType = currentLockType;

    return ret ? SQLITE_OK : SQLITE_BUSY;
}

static int
wkcUnlock(sqlite3_file *id, int locktype)
{
    int ret = 1;
    wkcFile *file;
    int type;

    file = (wkcFile*)id;
    type = file->fLockType;

    if (type >= SQLITE_LOCK_EXCLUSIVE) {
        wkcFileUnlockPeer(file->fFile, WKC_FILELOCK_EXCLUSIVE, WKC_SHARED_LOCK_FIRST, WKC_SHARED_SIZE);
        if (locktype == SQLITE_LOCK_SHARED) {
            ret = wkcFileLockPeer(file->fFile, WKC_FILELOCK_SHARED, WKC_SHARED_LOCK_FIRST, WKC_SHARED_SIZE);
            if (!ret) {
                file->fLastError = wkcFileErrnoPeer();
            }
        }
    }
    if (type >= SQLITE_LOCK_RESERVED) {
        wkcFileUnlockPeer(file->fFile, WKC_FILELOCK_EXCLUSIVE, WKC_RESERVED_LOCK_BYTE, 1);
    }
    if (type >= SQLITE_LOCK_SHARED && locktype == SQLITE_LOCK_NONE) {
        wkcFileUnlockPeer(file->fFile, WKC_FILELOCK_SHARED, WKC_SHARED_LOCK_FIRST, WKC_SHARED_SIZE);
    }
    if (type >= SQLITE_LOCK_PENDING) {
        wkcFileUnlockPeer(file->fFile, WKC_FILELOCK_EXCLUSIVE, WKC_PENDING_LOCK_BYTE, 1);
    }
    file->fLockType = locktype;

    return ret ? SQLITE_OK : SQLITE_IOERR_UNLOCK;
}

static int
wkcCheckReservedLock(sqlite3_file *id, int *pResOut)
{
    int ret;
    wkcFile *file;

    file = (wkcFile*)id;

    if (file->fLockType >= SQLITE_LOCK_RESERVED) {
        ret = 1;
    } else {
        ret = wkcFileLockPeer(file->fFile, WKC_FILELOCK_EXCLUSIVE, WKC_RESERVED_LOCK_BYTE, 1);
        if (ret) {
            wkcFileUnlockPeer(file->fFile, WKC_FILELOCK_EXCLUSIVE, WKC_RESERVED_LOCK_BYTE, 1);
        }
        ret = !ret;
    }

    *pResOut = ret;

    return SQLITE_OK;
}

static int
wkcFileControl(sqlite3_file *id, int op, void *pArg)
{
    wkcFile *file;

    file = (wkcFile*)id;

    switch( op ){
        case SQLITE_FCNTL_LOCKSTATE:
            *(int*)pArg = file->fLockType;
            return SQLITE_OK;

        case SQLITE_LAST_ERRNO:
            *(int*)pArg = file->fLastError;
            return SQLITE_OK;

        case SQLITE_FCNTL_CHUNK_SIZE:
            // not impl
            return SQLITE_OK;

        case SQLITE_FCNTL_SIZE_HINT:
            // not impl
            return SQLITE_OK;

        case SQLITE_FCNTL_SYNC_OMITTED:
            // nothing to do
            return SQLITE_OK;

        default:
            break;
    }

    return SQLITE_NOTFOUND;
}

static int
wkcDeviceCharacteristics(sqlite3_file *id)
{
    return SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN;
}

static const sqlite3_io_methods wkcIoMethod = {
    2,                              /* iVersion */
    wkcClose,                       /* xClose */
    wkcRead,                        /* xRead */
    wkcWrite,                       /* xWrite */
    wkcTruncate,                    /* xTruncate */
    wkcSync,                        /* xSync */
    wkcFileSize,                    /* xFileSize */
    wkcLock,                        /* xLock */
    wkcUnlock,                      /* xUnlock */
    wkcCheckReservedLock,           /* xCheckReservedLock */
    wkcFileControl,                 /* xFileControl */
    0,                              /* xSectorSize */
    wkcDeviceCharacteristics,       /* xDeviceCharacteristics */
    0,                              /* xShmMap */
    0,                              /* xShmLock */
    0,                              /* xShmBarrier */
    0                               /* xShmUnmap */
};


static int
fileExists(const char *path)
{
    void* fp = wkcFileFOpenPeer(WKC_FILEOPEN_USAGE_DATABASE, path, "rb");
    if (!fp)
        return 0;
    wkcFileFClosePeer(fp);
    return 1;
}

#define WKC_SQLITE_FILEMODE_READONLY  "rb"
#define WKC_SQLITE_FILEMODE_CREATE    "wb+"
#define WKC_SQLITE_FILEMODE_READWRITE "rb+"

static int
wkcOpen(sqlite3_vfs*, const char *zName, sqlite3_file* zFile,
               int flags, int *pOutFlags)
{
    void *fp = 0;
    wkcFile *file = (wkcFile*)zFile;
    char fileName[MAX_PATH + 1];
    char fileMode[4] = {0};

    int isExclusive  = (flags & SQLITE_OPEN_EXCLUSIVE);
    int isDelete     = (flags & SQLITE_OPEN_DELETEONCLOSE);
    int isCreate     = (flags & SQLITE_OPEN_CREATE);
    int isReadonly   = (flags & SQLITE_OPEN_READONLY);
    int isReadWrite  = (flags & SQLITE_OPEN_READWRITE);

    ASSERT((isReadonly==0 || isReadWrite==0) && (isReadWrite || isReadonly));
    ASSERT(isCreate==0 || isReadWrite);
    ASSERT(isExclusive==0 || isCreate);
    ASSERT(isDelete==0 || isCreate);

    memset(file, 0, sizeof(wkcFile));

    if (!zName) {
        fp = wkcFileOpenTemporaryFilePeer("WKCDB", fileName, MAX_PATH);
    } else {
        if( isReadonly ){
            strncpy(fileMode, WKC_SQLITE_FILEMODE_READONLY, 2);
        }

        /* SQLITE_OPEN_EXCLUSIVE is used to make sure that a new file is 
         ** created. SQLite doesn't use it to indicate "exclusive access" 
         ** as it is usually understood.
         */
        if( isExclusive ){
            /* Creates a new file, only if it does not already exist. */
            /* If the file exists, it fails. */
            if (fileExists(zName)) {
                return SQLITE_CANTOPEN;
            }
        }
        if( isCreate ){
            /* Open existing file, or create if it doesn't exist */
            if (isExclusive || !fileExists(zName)) {
                strncpy(fileMode, WKC_SQLITE_FILEMODE_CREATE, 3);
            } else {
                strncpy(fileMode, WKC_SQLITE_FILEMODE_READWRITE, 3);
            }
        }else{
            /* Opens a file, only if it exists. */
            strncpy(fileMode, WKC_SQLITE_FILEMODE_READWRITE, 3);
        }

        fp = wkcFileFOpenPeer(WKC_FILEOPEN_USAGE_DATABASE, zName, fileMode);
    }
    if (!fp) {
        file->fLastError = wkcFileErrnoPeer();
        return SQLITE_CANTOPEN;
    }

    if(pOutFlags) {
        if(isReadWrite || !zName) {
            *pOutFlags = SQLITE_OPEN_READWRITE;
        } else {
            *pOutFlags = SQLITE_OPEN_READONLY;
        }
    }

    file->fMethod = &wkcIoMethod;
    file->fFile = fp;
    file->fIsDeleteOnClose = (isDelete) ? 1 : 0;
    if (zName) {
        strncpy(file->fFileName, zName, MAX_PATH);
    } else {
        strncpy(file->fFileName, fileName, MAX_PATH);
    }

    return SQLITE_OK;
}

static int 
wkcDelete(sqlite3_vfs*, const char *zName, int)
{
    int err;

    if (!fileExists(zName)) {
        return SQLITE_OK;
    }

    err = wkcFileUnlinkPeer(zName);

    return err ? SQLITE_OK : SQLITE_IOERR_DELETE;
}

static int
wkcAccess(sqlite3_vfs*, const char *zPath, int flags, int *pResOut)
{
    int amode = 0;

    switch( flags ){
    case SQLITE_ACCESS_EXISTS:
        amode = 0x0;
        break;
    case SQLITE_ACCESS_READWRITE:
        amode = 0x2|0x4;
        break;
    case SQLITE_ACCESS_READ:
        amode = 0x4;
        break;

    default:
        break;
    }

    *pResOut = (wkcFileAccessPeer(zPath, amode)==0);
    if( flags==SQLITE_ACCESS_EXISTS && *pResOut ){
        struct stat buf;
        if( 0==wkcFileStatPeer(zPath, &buf) && buf.st_size==0 ){
            *pResOut = 0;
        }
    }
    return SQLITE_OK;
}

static int
wkcFullPathname(sqlite3_vfs *, const char *zRelative, int nFull, char *zFull)
{
    strncpy(zFull, zRelative, nFull);
    zFull[nFull - 1] = 0;

    return SQLITE_OK;
}

static int
wkcRandomness(sqlite3_vfs *, int nBuf, char *zBuf)
{
    return wkcRandomNumbersPeer((unsigned char *)zBuf, nBuf);
}

static int
wkcSleep(sqlite3_vfs *, int microsec)
{
    wkc_usleep(microsec);

    return microsec;
}

static int
wkcCurrentTimeInt64(sqlite3_vfs *, sqlite3_int64 *piNow)
{
    static const sqlite3_int64 unixEpoch = 24405875*(sqlite3_int64)8640000;
    time_t now;

    now = wkc_time(0);
    *piNow = unixEpoch + 1000*(sqlite3_int64)now;

    return 0;
}

static int
wkcCurrentTime(sqlite3_vfs *, double *prNow)
{
    sqlite3_int64 i;

    wkcCurrentTimeInt64(0, &i);
    *prNow = i/86400000.0;

    return 0;
}

static int
wkcGetLastError(sqlite3_vfs*, int, char *)
{
    // nothing to do
    return 0;
}

void SQLiteFileSystem::registerSQLiteVFS()
{
    static sqlite3_vfs wkcVfs = {
        3,                   /* iVersion */
        sizeof(wkcFile),     /* szOsFile */
        MAX_PATH,            /* mxPathname */
        0,                   /* pNext */
        "wkc",               /* zName */
        0,                   /* pAppData */
        wkcOpen,             /* xOpen */
        wkcDelete,           /* xDelete */
        wkcAccess,           /* xAccess */
        wkcFullPathname,     /* xFullPathname */
        0,                   /* xDlOpen */
        0,                   /* xDlError */
        0,                   /* xDlSym */
        0,                   /* xDlClose */
        wkcRandomness,       /* xRandomness */
        wkcSleep,            /* xSleep */
        wkcCurrentTime,      /* xCurrentTime */
        wkcGetLastError,     /* xGetLastError */
        wkcCurrentTimeInt64, /* xCurrentTimeInt64 */
        0,                   /* xSetSystemCall */
        0,                   /* xGetSystemCall */
        0,                   /* xNextSystemCall */
    };

    sqlite3_vfs_register(&wkcVfs, 1);
}

} // namespace WebCore
// ENABLE(SQL_DATABASE) is off for the current Wii U WKC profile. The legacy
// fallback definition of SQLiteFileSystem::registerSQLiteVFS() is not declared
// by the current upstream SQLiteFileSystem interface, so do not emit it.
#endif // ENABLE(SQL_DATABASE)
