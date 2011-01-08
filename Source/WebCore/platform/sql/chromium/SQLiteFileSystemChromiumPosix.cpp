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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

using namespace WebCore;

// Chromium's Posix implementation of SQLite VFS.
// This is heavily based on SQLite's os_unix.c,
// without parts we don't need.

// Identifies a file by its device number and inode.
struct ChromiumFileId {
    dev_t dev; // Device number.
    ino_t ino; // Inode number.
};

// Information about file locks (one per open inode). Note that many open
// file descriptors may refer to the same inode.
struct ChromiumLockInfo {
    ChromiumFileId lockKey; // File identifier.
    int cnt; // Number of shared locks held.
    int locktype; // Type of the lock.
    int nRef; // Reference count.

    // Double-linked list pointers.
    ChromiumLockInfo* pNext; 
    ChromiumLockInfo* pPrev;
};

// Information about a file descriptor that cannot be closed immediately.
struct ChromiumUnusedFd {
    int fd; // File descriptor.
    int flags; // Flags this file descriptor was opened with.
    ChromiumUnusedFd* pNext; // Next unused file descriptor on the same file.
};

// Information about an open inode. When we want to close an inode
// that still has locks, we defer the close until all locks are cleared.
struct ChromiumOpenInfo {
    ChromiumFileId fileId; // The lookup key.
    int nRef; // Reference count.
    int nLock; // Number of outstanding locks.
    ChromiumUnusedFd* pUnused; // List of file descriptors to close.

    // Double-linked list pointers.
    ChromiumOpenInfo* pNext;
    ChromiumOpenInfo* pPrev;
};

// Keep track of locks and inodes in double-linked lists.
static struct ChromiumLockInfo* lockList = 0;
static struct ChromiumOpenInfo* openList = 0;

// Extension of sqlite3_file specific to the chromium VFS.
struct ChromiumFile {
    sqlite3_io_methods const* pMethod; // Implementation of sqlite3_file.
    ChromiumOpenInfo* pOpen; // Information about all open file descriptors for this file.
    ChromiumLockInfo* pLock; // Information about all locks for this file.
    int h; // File descriptor.
    int dirfd; // File descriptor for the file directory.
    unsigned char locktype; // Type of the lock used for this file.
    int lastErrno; // Value of errno for last operation on this file.
    ChromiumUnusedFd* pUnused; // Information about unused file descriptors for this file.
};

// The following constants specify the range of bytes used for locking.
// SQLiteSharedSize is the number of bytes available in the pool from which
// a random byte is selected for a shared lock.  The pool of bytes for
// shared locks begins at SQLiteSharedFirstByte. 
// The values are the same as used by SQLite for compatibility.
static const off_t SQLitePendingByte = 0x40000000;
static const off_t SQLiteReservedByte = SQLitePendingByte + 1;
static const off_t SQLiteSharedFirstByte = SQLitePendingByte + 2;
static const off_t SQLiteSharedSize = 510;

// Maps a POSIX error code to an SQLite error code.
static int sqliteErrorFromPosixError(int posixError, int sqliteIOErr)
{
    switch (posixError) {
    case 0: 
        return SQLITE_OK;
    case EAGAIN:
    case ETIMEDOUT:
    case EBUSY:
    case EINTR:
    case ENOLCK:  
        return SQLITE_BUSY;
    case EACCES: 
        // EACCES is like EAGAIN during locking operations.
        if ((sqliteIOErr == SQLITE_IOERR_LOCK) ||
            (sqliteIOErr == SQLITE_IOERR_UNLOCK) ||
            (sqliteIOErr == SQLITE_IOERR_RDLOCK) ||
            (sqliteIOErr == SQLITE_IOERR_CHECKRESERVEDLOCK))
            return SQLITE_BUSY;
        return SQLITE_PERM;
    case EPERM: 
        return SQLITE_PERM;
    case EDEADLK:
        return SQLITE_IOERR_BLOCKED;
    default: 
        return sqliteIOErr;
    }
}

// Releases a ChromiumLockInfo structure previously allocated by findLockInfo().
static void releaseLockInfo(ChromiumLockInfo* pLock)
{
    if (!pLock)
        return;

    pLock->nRef--;
    if (pLock->nRef > 0)
        return;

    if (pLock->pPrev) {
        ASSERT(pLock->pPrev->pNext == pLock);
        pLock->pPrev->pNext = pLock->pNext;
    } else {
        ASSERT(lockList == pLock);
        lockList = pLock->pNext;
    }
    if (pLock->pNext) {
        ASSERT(pLock->pNext->pPrev == pLock);
        pLock->pNext->pPrev = pLock->pPrev;
    }

    sqlite3_free(pLock);
}

// Releases a ChromiumOpenInfo structure previously allocated by findLockInfo().
static void releaseOpenInfo(ChromiumOpenInfo* pOpen)
{
    if (!pOpen)
        return;

    pOpen->nRef--;
    if (pOpen->nRef > 0)
        return;

    if (pOpen->pPrev) {
        ASSERT(pOpen->pPrev->pNext == pOpen);
        pOpen->pPrev->pNext = pOpen->pNext;
    } else {
        ASSERT(openList == pOpen);
        openList = pOpen->pNext;
    }
    if (pOpen->pNext) {
        ASSERT(pOpen->pNext->pPrev == pOpen);
        pOpen->pNext->pPrev = pOpen->pPrev;
    }

    ASSERT(!pOpen->pUnused); // Make sure we're not leaking memory and file descriptors.

    sqlite3_free(pOpen);
}

// Locates ChromiumLockInfo and ChromiumOpenInfo for given file descriptor (creating new ones if needed).
// Returns a SQLite error code.
static int findLockInfo(ChromiumFile* pFile, ChromiumLockInfo** ppLock, ChromiumOpenInfo** ppOpen)
{
    int fd = pFile->h;
    struct stat statbuf;
    int rc = fstat(fd, &statbuf);
    if (rc) {
        pFile->lastErrno = errno;
#ifdef EOVERFLOW
        if (pFile->lastErrno == EOVERFLOW)
            return SQLITE_NOLFS;
#endif
        return SQLITE_IOERR;
    }

#if OS(DARWIN)
    // On OS X on an msdos/fat filesystems, the inode number is reported
    // incorrectly for zero-size files. See http://www.sqlite.org/cvstrac/tktview?tn=3260.
    // To work around this problem we always increase the file size to 1 by writing a single byte
    // prior to accessing the inode number. The one byte written is an ASCII 'S' character which
    // also happens to be the first byte in the header of every SQLite database.  In this way,
    // if there is a race condition such that another thread has already populated the first page
    // of the database, no damage is done.
    if (!statbuf.st_size) {
        rc = write(fd, "S", 1);
        if (rc != 1)
            return SQLITE_IOERR;
        rc = fstat(fd, &statbuf);
        if (rc) {
            pFile->lastErrno = errno;
            return SQLITE_IOERR;
        }
    }
#endif

    ChromiumFileId fileId;
    memset(&fileId, 0, sizeof(fileId));
    fileId.dev = statbuf.st_dev;
    fileId.ino = statbuf.st_ino;

    ChromiumLockInfo* pLock = 0;

    if (ppLock) {
        pLock = lockList;
        while (pLock && memcmp(&fileId, &pLock->lockKey, sizeof(fileId)))
            pLock = pLock->pNext;
        if (pLock)
            pLock->nRef++;
        else {
            pLock = static_cast<ChromiumLockInfo*>(sqlite3_malloc(sizeof(*pLock)));
            if (!pLock)
                return SQLITE_NOMEM;
            pLock->lockKey = fileId;
            pLock->nRef = 1;
            pLock->cnt = 0;
            pLock->locktype = 0;
            pLock->pNext = lockList;
            pLock->pPrev = 0;
            if (lockList)
                lockList->pPrev = pLock;
            lockList = pLock;
        }
        *ppLock = pLock;
    }

    if (ppOpen) {
        ChromiumOpenInfo* pOpen = openList;
        while (pOpen && memcmp(&fileId, &pOpen->fileId, sizeof(fileId)))
            pOpen = pOpen->pNext;
        if (pOpen)
            pOpen->nRef++;
        else {
            pOpen = static_cast<ChromiumOpenInfo*>(sqlite3_malloc(sizeof(*pOpen)));
            if (!pOpen) {
                releaseLockInfo(pLock);
                return SQLITE_NOMEM;
            }
            memset(pOpen, 0, sizeof(*pOpen));
            pOpen->fileId = fileId;
            pOpen->nRef = 1;
            pOpen->pNext = openList;
            if (openList)
                openList->pPrev = pOpen;
            openList = pOpen;
        }
        *ppOpen = pOpen;
    }

    return rc;
}

// Checks if there is a RESERVED lock held on the specified file by this or any other process.
// If the lock is held, sets pResOut to a non-zero value. Returns a SQLite error code.
static int chromiumCheckReservedLock(sqlite3_file* id, int* pResOut)
{
    ChromiumFile* pFile = reinterpret_cast<ChromiumFile*>(id);
    ASSERT(pFile);

    // Look for locks held by this process.
    int reserved = 0;
    if (pFile->pLock->locktype > SQLITE_LOCK_SHARED)
        reserved = 1;

    // Look for locks held by other processes.
    int rc = SQLITE_OK;
    if (!reserved) {
        struct flock lock;
        lock.l_whence = SEEK_SET;
        lock.l_start = SQLiteReservedByte;
        lock.l_len = 1;
        lock.l_type = F_WRLCK;
        if (-1 == fcntl(pFile->h, F_GETLK, &lock)) {
            int tErrno = errno;
            rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_CHECKRESERVEDLOCK);
            pFile->lastErrno = tErrno;
        } else if (lock.l_type != F_UNLCK)
            reserved = 1;
    }

    *pResOut = reserved;
    return rc;
}

// Performs a file locking operation on a range of bytes in a file.
// The |op| parameter should be one of F_RFLCK, F_WRLCK or F_UNLCK.
// Returns a Unix error code, and also writes it to pErrcode.
static int rangeLock(ChromiumFile* pFile, int op, int* pErrcode)
{
    struct flock lock;
    lock.l_type = op;
    lock.l_start = SQLiteSharedFirstByte;
    lock.l_whence = SEEK_SET;
    lock.l_len = SQLiteSharedSize;
    int rc = fcntl(pFile->h, F_SETLK, &lock);
    *pErrcode = errno;
    return rc;
}

// Locks the file with the lock specified by parameter locktype - one
// of the following:
//
//     (1) SQLITE_LOCK_SHARED
//     (2) SQLITE_LOCK_RESERVED
//     (3) SQLITE_LOCK_PENDING
//     (4) SQLITE_LOCK_EXCLUSIVE
//
// Sometimes when requesting one lock state, additional lock states
// are inserted in between.  The locking might fail on one of the later
// transitions leaving the lock state different from what it started but
// still short of its goal.  The following chart shows the allowed
// transitions and the inserted intermediate states:
//
//    UNLOCKED -> SHARED
//    SHARED -> RESERVED
//    SHARED -> (PENDING) -> EXCLUSIVE
//    RESERVED -> (PENDING) -> EXCLUSIVE
//    PENDING -> EXCLUSIVE
static int chromiumLock(sqlite3_file* id, int locktype)
{
    // To obtain a SHARED lock, a read-lock is obtained on the 'pending
    // byte'.  If this is successful, a random byte from the 'shared byte
    // range' is read-locked and the lock on the 'pending byte' released.
    //
    // A process may only obtain a RESERVED lock after it has a SHARED lock.
    // A RESERVED lock is implemented by grabbing a write-lock on the
    // 'reserved byte'. 
    //
    // A process may only obtain a PENDING lock after it has obtained a
    // SHARED lock. A PENDING lock is implemented by obtaining a write-lock
    // on the 'pending byte'. This ensures that no new SHARED locks can be
    // obtained, but existing SHARED locks are allowed to persist. A process
    // does not have to obtain a RESERVED lock on the way to a PENDING lock.
    // This property is used by the algorithm for rolling back a journal file
    // after a crash.
    //
    // An EXCLUSIVE lock, obtained after a PENDING lock is held, is
    // implemented by obtaining a write-lock on the entire 'shared byte
    // range'. Since all other locks require a read-lock on one of the bytes
    // within this range, this ensures that no other locks are held on the
    // database. 

    int rc = SQLITE_OK;
    struct flock lock;
    int s = 0;
    int tErrno;

    ChromiumFile* pFile = reinterpret_cast<ChromiumFile*>(id);
    ASSERT(pFile);

    ChromiumLockInfo* pLock = pFile->pLock;

    // If there is already a lock of this type or more restrictive, do nothing.
    if (pFile->locktype >= locktype)
        return SQLITE_OK;

    // Make sure we never move from unlocked to anything higher than shared lock.
    ASSERT(pFile->locktype != SQLITE_LOCK_NONE || locktype == SQLITE_LOCK_SHARED);

    // Make sure we never request a pending lock.
    ASSERT(locktype != SQLITE_LOCK_PENDING);

    // Make sure a shared lock is always held when a RESERVED lock is requested.
    ASSERT(locktype != SQLITE_LOCK_RESERVED || pFile->locktype == SQLITE_LOCK_SHARED);

    // If some thread using this PID has a lock via a different ChromiumFile
    // handle that precludes the requested lock, return BUSY.
    if (pFile->locktype != pLock->locktype &&
        (pLock->locktype >= SQLITE_LOCK_PENDING || locktype > SQLITE_LOCK_SHARED))
        return SQLITE_BUSY;

    // If a SHARED lock is requested, and some thread using this PID already
    // has a SHARED or RESERVED lock, then just increment reference counts.
    if (locktype == SQLITE_LOCK_SHARED &&
        (pLock->locktype == SQLITE_LOCK_SHARED || pLock->locktype == SQLITE_LOCK_RESERVED)) {
        ASSERT(!pFile->locktype);
        ASSERT(pLock->cnt > 0);
        pFile->locktype = SQLITE_LOCK_SHARED;
        pLock->cnt++;
        pFile->pOpen->nLock++;
        return SQLITE_OK;
    }

    // A PENDING lock is needed before acquiring a SHARED lock and before
    // acquiring an EXCLUSIVE lock.  For the SHARED lock, the PENDING will
    // be released.
    lock.l_len = 1;
    lock.l_whence = SEEK_SET;
    if (locktype == SQLITE_LOCK_SHARED ||
        (locktype == SQLITE_LOCK_EXCLUSIVE && pFile->locktype < SQLITE_LOCK_PENDING)) {
        lock.l_type = (locktype == SQLITE_LOCK_SHARED ? F_RDLCK : F_WRLCK);
        lock.l_start = SQLitePendingByte;
        s = fcntl(pFile->h, F_SETLK, &lock);
        if (s == -1) {
            tErrno = errno;
            rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_LOCK);
            if ((rc != SQLITE_OK) && (rc != SQLITE_BUSY))
                pFile->lastErrno = tErrno;
            return rc;
        }
    }

    if (locktype == SQLITE_LOCK_SHARED) {
        ASSERT(!pLock->cnt);
        ASSERT(!pLock->locktype);

        s = rangeLock(pFile, F_RDLCK, &tErrno);

        // Drop the temporary PENDING lock.
        lock.l_start = SQLitePendingByte;
        lock.l_len = 1;
        lock.l_type = F_UNLCK;
        if (fcntl(pFile->h, F_SETLK, &lock)) {
            if (s != -1) {
                tErrno = errno; 
                rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_UNLOCK); 
                if ((rc != SQLITE_OK) && (rc != SQLITE_BUSY))
                    pFile->lastErrno = tErrno;
                return rc;
            }
        }
        if (s == -1) {
            rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_LOCK);
            if ((rc != SQLITE_OK) && (rc != SQLITE_BUSY))
                pFile->lastErrno = tErrno;
        } else {
            pFile->locktype = SQLITE_LOCK_SHARED;
            pFile->pOpen->nLock++;
            pLock->cnt = 1;
        }
    } else if (locktype == SQLITE_LOCK_EXCLUSIVE && pLock->cnt > 1) {
        // We are trying for an exclusive lock but another thread in the
        // same process is still holding a shared lock.
        rc = SQLITE_BUSY;
    }  else {
        // The request was for a RESERVED or EXCLUSIVE lock.  It is
        // assumed that there is a SHARED or greater lock on the file
        // already.
        ASSERT(pFile->locktype);
        lock.l_type = F_WRLCK;
        switch (locktype) {
        case SQLITE_LOCK_RESERVED:
            lock.l_start = SQLiteReservedByte;
            s = fcntl(pFile->h, F_SETLK, &lock);
            tErrno = errno;
            break;
        case SQLITE_LOCK_EXCLUSIVE:
            s = rangeLock(pFile, F_WRLCK, &tErrno);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        if (s == -1) {
            rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_LOCK);
            if ((rc != SQLITE_OK) && (rc != SQLITE_BUSY))
                pFile->lastErrno = tErrno;
        }
    }
  
    if (rc == SQLITE_OK) {
        pFile->locktype = locktype;
        pLock->locktype = locktype;
    } else if (locktype == SQLITE_LOCK_EXCLUSIVE) {
        pFile->locktype = SQLITE_LOCK_PENDING;
        pLock->locktype = SQLITE_LOCK_PENDING;
    }

    return rc;
}

// Closes all file descriptors for given ChromiumFile for which the close has been deferred.
// Returns a SQLite error code.
static int closePendingFds(ChromiumFile* pFile)
{
    int rc = SQLITE_OK;
    ChromiumOpenInfo* pOpen = pFile->pOpen;
    ChromiumUnusedFd* pError = 0;
    ChromiumUnusedFd* pNext;
    for (ChromiumUnusedFd* p = pOpen->pUnused; p; p = pNext) {
        pNext = p->pNext;
        if (close(p->fd)) {
            pFile->lastErrno = errno;
            rc = SQLITE_IOERR_CLOSE;
            p->pNext = pError;
            pError = p;
        } else
            sqlite3_free(p);
    }
    pOpen->pUnused = pError;
    return rc;
}

// Lowers the locking level on file descriptor.
// locktype must be either SQLITE_LOCK_NONE or SQLITE_LOCK_SHARED.
static int chromiumUnlock(sqlite3_file* id, int locktype)
{
    ChromiumFile* pFile = reinterpret_cast<ChromiumFile*>(id);
    ASSERT(pFile);
    ASSERT(locktype <= SQLITE_LOCK_SHARED);

    if (pFile->locktype <= locktype)
        return SQLITE_OK;

    ChromiumLockInfo* pLock = pFile->pLock;
    ASSERT(pLock->cnt);

    struct flock lock;
    int rc = SQLITE_OK;
    int h = pFile->h;
    int tErrno;

    if (pFile->locktype > SQLITE_LOCK_SHARED) {
        ASSERT(pLock->locktype == pFile->locktype);

        if (locktype == SQLITE_LOCK_SHARED && rangeLock(pFile, F_RDLCK, &tErrno) == -1) {
            rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_RDLOCK);
            if ((rc != SQLITE_OK) && (rc != SQLITE_BUSY))
                pFile->lastErrno = tErrno;
            if (rc == SQLITE_OK)
                pFile->locktype = locktype;
            return rc;
        }
        lock.l_type = F_UNLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = SQLitePendingByte;
        lock.l_len = 2;
        if (fcntl(h, F_SETLK, &lock) != -1)
            pLock->locktype = SQLITE_LOCK_SHARED;
        else {
            tErrno = errno;
            rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_UNLOCK);
            if ((rc != SQLITE_OK) && (rc != SQLITE_BUSY))
                pFile->lastErrno = tErrno;
            if (rc == SQLITE_OK)
                pFile->locktype = locktype;
            return rc;
        }
    }
    if (locktype == SQLITE_LOCK_NONE) {
        struct ChromiumOpenInfo *pOpen;

        pLock->cnt--;

        // Release the lock using an OS call only when all threads in this same process have released the lock.
        if (!pLock->cnt) {
            lock.l_type = F_UNLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = lock.l_len = 0L;
            if (fcntl(h, F_SETLK, &lock) != -1)
                pLock->locktype = SQLITE_LOCK_NONE;
            else {
                tErrno = errno;
                rc = sqliteErrorFromPosixError(tErrno, SQLITE_IOERR_UNLOCK);
                if ((rc != SQLITE_OK) && (rc != SQLITE_BUSY))
                    pFile->lastErrno = tErrno;
                pLock->locktype = SQLITE_LOCK_NONE;
                pFile->locktype = SQLITE_LOCK_NONE;
            }
        }

        pOpen = pFile->pOpen;
        pOpen->nLock--;
        ASSERT(pOpen->nLock >= 0);
        if (!pOpen->nLock) {
            int rc2 = closePendingFds(pFile);
            if (rc == SQLITE_OK)
                rc = rc2;
        }
    }

    if (rc == SQLITE_OK)
        pFile->locktype = locktype;
    return rc;
}

// Closes all file handles for given ChromiumFile and sets all its fields to 0.
// Returns a SQLite error code.
static int chromiumCloseNoLock(sqlite3_file* id)
{
    ChromiumFile* pFile = reinterpret_cast<ChromiumFile*>(id);
    if (!pFile)
        return SQLITE_OK;
    if (pFile->dirfd >= 0) {
        if (close(pFile->dirfd)) {
            pFile->lastErrno = errno;
            return SQLITE_IOERR_DIR_CLOSE;
        }
        pFile->dirfd = -1;
    }
    if (pFile->h >= 0 && close(pFile->h)) {
        pFile->lastErrno = errno;
        return SQLITE_IOERR_CLOSE;
    }
    sqlite3_free(pFile->pUnused);
    memset(pFile, 0, sizeof(ChromiumFile));
    return SQLITE_OK;
}

// Closes a ChromiumFile, including locking operations. Returns a SQLite error code.
static int chromiumClose(sqlite3_file* id)
{
    if (!id)
        return SQLITE_OK;

    ChromiumFile* pFile = reinterpret_cast<ChromiumFile*>(id);
    chromiumUnlock(id, SQLITE_LOCK_NONE);
    if (pFile->pOpen && pFile->pOpen->nLock) {
        // If there are outstanding locks, do not actually close the file just
        // yet because that would clear those locks.
        ChromiumOpenInfo* pOpen = pFile->pOpen;
        ChromiumUnusedFd* p = pFile->pUnused;
        p->pNext = pOpen->pUnused;
        pOpen->pUnused = p;
        pFile->h = -1;
        pFile->pUnused = 0;
    }
    releaseLockInfo(pFile->pLock);
    releaseOpenInfo(pFile->pOpen);
    return chromiumCloseNoLock(id);
}

static int chromiumCheckReservedLockNoop(sqlite3_file*, int* pResOut)
{
    *pResOut = 0;
    return SQLITE_OK;
}

static int chromiumLockNoop(sqlite3_file*, int)
{
    return SQLITE_OK;
}

static int chromiumUnlockNoop(sqlite3_file*, int)
{
    return SQLITE_OK;
}

// Seeks to the requested offset and reads up to |cnt| bytes into |pBuf|. Returns number of bytes actually read.
static int seekAndRead(ChromiumFile* id, sqlite3_int64 offset, void* pBuf, int cnt)
{
    sqlite_int64 newOffset = lseek(id->h, offset, SEEK_SET);
    if (newOffset != offset) {
        id->lastErrno = (newOffset == -1) ? errno : 0;
        return -1;
    }
    int got = read(id->h, pBuf, cnt);
    if (got < 0)
        id->lastErrno = errno;
    return got;
}

// Reads data from file into a buffer. Returns a SQLite error code.
static int chromiumRead(sqlite3_file* id, void* pBuf, int amt, sqlite3_int64 offset)
{
    ChromiumFile* pFile = reinterpret_cast<ChromiumFile*>(id);
    ASSERT(pFile);

    // The bytes in the locking range should never be read.
    ASSERT(!pFile->pUnused || offset >= SQLitePendingByte + 512 || offset + amt <= SQLitePendingByte);

    int got = seekAndRead(pFile, offset, pBuf, amt);
    if (got == amt)
        return SQLITE_OK;

    if (got < 0)
        return SQLITE_IOERR_READ;

    // Unread parts of the buffer must be zero-filled.
    memset(&(reinterpret_cast<char*>(pBuf))[got], 0, amt - got);
    pFile->lastErrno = 0;
    return SQLITE_IOERR_SHORT_READ;
}

// Seeks to the requested offset and writes up to |cnt| bytes. Returns number of bytes actually written.
static int seekAndWrite(ChromiumFile* id, sqlite_int64 offset, const void* pBuf, int cnt)
{
    sqlite_int64 newOffset = lseek(id->h, offset, SEEK_SET);
    if (newOffset != offset) {
        id->lastErrno = (newOffset == -1) ? errno : 0;
        return -1;
    }
    int got = write(id->h, pBuf, cnt);
    if (got < 0)
        id->lastErrno = errno;
    return got;
}

// Writes data from buffer into a file. Returns a SQLite error code.
static int chromiumWrite(sqlite3_file* id, const void* pBuf, int amt, sqlite3_int64 offset)
{
    ChromiumFile* pFile = reinterpret_cast<ChromiumFile*>(id);
    ASSERT(pFile);
    ASSERT(amt > 0);

    // The bytes in the locking range should never be written.
    ASSERT(!pFile->pUnused || offset >= SQLitePendingByte + 512 || offset + amt <= SQLitePendingByte);

    int wrote = 0;
    while (amt > 0 && (wrote = seekAndWrite(pFile, offset, pBuf, amt)) > 0) {
        amt -= wrote;
        offset += wrote;
        pBuf = &(reinterpret_cast<const char*>(pBuf))[wrote];
    }
    if (amt > 0) {
        if (wrote < 0)
            return SQLITE_IOERR_WRITE;
        pFile->lastErrno = 0;
        return SQLITE_FULL;
    }
    return SQLITE_OK;
}

static bool syncWrapper(int fd, bool fullSync)
{
#if OS(DARWIN)
    bool success = false;
    if (fullSync)
        success = !fcntl(fd, F_FULLFSYNC, 0);
    if (!success)
        success = !fsync(fd);
    return success;
#else
    return !fdatasync(fd);
#endif
}

// Makes sure all writes to a particular file are committed to disk. Returns a SQLite error code.
static int chromiumSync(sqlite3_file* id, int flags)
{
    ASSERT((flags & 0x0F) == SQLITE_SYNC_NORMAL || (flags & 0x0F) == SQLITE_SYNC_FULL);

    ChromiumFile* pFile = reinterpret_cast<ChromiumFile*>(id);
    ASSERT(pFile);

    bool isFullSync = ((flags & 0x0F) == SQLITE_SYNC_FULL);

    if (!syncWrapper(pFile->h, isFullSync)) {
        pFile->lastErrno = errno;
        return SQLITE_IOERR_FSYNC;
    }

    if (pFile->dirfd >= 0) {
#if !OS(DARWIN)
        if (!isFullSync) {
            // Ignore directory sync failures, see http://www.sqlite.org/cvstrac/tktview?tn=1657.
            syncWrapper(pFile->dirfd, false);
        }
#endif
        if (!close(pFile->dirfd))
            pFile->dirfd = -1;
        else {
            pFile->lastErrno = errno;
            return SQLITE_IOERR_DIR_CLOSE;
        }
    }

    return SQLITE_OK;
}

// Truncates an open file to the specified size. Returns a SQLite error code.
static int chromiumTruncate(sqlite3_file* id, sqlite_int64 nByte)
{
    ChromiumFile* pFile = reinterpret_cast<ChromiumFile*>(id);
    ASSERT(pFile);

    if (ftruncate(pFile->h, nByte)) {
        pFile->lastErrno = errno;
        return SQLITE_IOERR_TRUNCATE;
    }

    return SQLITE_OK;
}

// Determines the size of a file in bytes. Returns a SQLite error code.
static int chromiumFileSize(sqlite3_file* id, sqlite_int64* pSize)
{
    ChromiumFile* pFile = reinterpret_cast<ChromiumFile*>(id);
    ASSERT(pFile);

    struct stat buf;
    if (fstat(pFile->h, &buf)) {
        pFile->lastErrno = errno;
        return SQLITE_IOERR_FSTAT;
    }
    *pSize = buf.st_size;

    // When opening a zero-size database, findLockInfo writes a single byte into that file
    // in order to work around a bug in the OS X msdos filesystem. In order to avoid problems
    // with upper layers, we need to report this file size as zero even though it is really 1.
    // See http://www.sqlite.org/cvstrac/tktview?tn=3260.
    if (*pSize == 1)
        *pSize = 0;

    return SQLITE_OK;
}

static int chromiumFileControl(sqlite3_file* id, int op, void* pArg)
{
    ChromiumFile* pFile = reinterpret_cast<ChromiumFile*>(id);
    ASSERT(pFile);

    switch (op) {
    case SQLITE_FCNTL_LOCKSTATE:
        *reinterpret_cast<int*>(pArg) = pFile->locktype;
        return SQLITE_OK;
    case SQLITE_LAST_ERRNO:
        *reinterpret_cast<int*>(pArg) = pFile->lastErrno;
        return SQLITE_OK;
    }
    return SQLITE_ERROR;
}

// Same as SQLITE_DEFAULT_SECTOR_SIZE from sqlite's os.h.
static const int SQLiteDefaultSectorSize = 512;

static int chromiumSectorSize(sqlite3_file*)
{
    return SQLiteDefaultSectorSize;
}

static int chromiumDeviceCharacteristics(sqlite3_file*)
{
    return 0;
}

static const sqlite3_io_methods posixIoMethods = {
    1,
    chromiumClose,
    chromiumRead,
    chromiumWrite,
    chromiumTruncate,
    chromiumSync,
    chromiumFileSize,
    chromiumLock,
    chromiumUnlock,
    chromiumCheckReservedLock,
    chromiumFileControl,
    chromiumSectorSize,
    chromiumDeviceCharacteristics
};

static const sqlite3_io_methods nolockIoMethods = {
    1,
    chromiumCloseNoLock,
    chromiumRead,
    chromiumWrite,
    chromiumTruncate,
    chromiumSync,
    chromiumFileSize,
    chromiumLockNoop,
    chromiumUnlockNoop,
    chromiumCheckReservedLockNoop,
    chromiumFileControl,
    chromiumSectorSize,
    chromiumDeviceCharacteristics
};

// Initializes a ChromiumFile. Returns a SQLite error code.
static int fillInChromiumFile(sqlite3_vfs* pVfs, int h, int dirfd, sqlite3_file* pId, const char* zFilename, int noLock)
{
    ChromiumFile* pNew = reinterpret_cast<ChromiumFile*>(pId);

    ASSERT(!pNew->pLock);
    ASSERT(!pNew->pOpen);

    pNew->h = h;
    pNew->dirfd = dirfd;

    int rc = SQLITE_OK;
    const sqlite3_io_methods* pLockingStyle;
    if (noLock)
        pLockingStyle = &nolockIoMethods;
    else {
        pLockingStyle = &posixIoMethods;
        rc = findLockInfo(pNew, &pNew->pLock, &pNew->pOpen);
        if (rc != SQLITE_OK) {
            // If an error occured in findLockInfo(), close the file descriptor
            // immediately. This can happen in two scenarios:
            //
            //   (a) A call to fstat() failed.
            //   (b) A malloc failed.
            //
            // Scenario (b) may only occur if the process is holding no other
            // file descriptors open on the same file. If there were other file
            // descriptors on this file, then no malloc would be required by
            // findLockInfo(). If this is the case, it is quite safe to close
            // handle h - as it is guaranteed that no posix locks will be released
            // by doing so.
            //
            // If scenario (a) caused the error then things are not so safe. The
            // implicit assumption here is that if fstat() fails, things are in
            // such bad shape that dropping a lock or two doesn't matter much.
            close(h);
            h = -1;
        }
    }

    pNew->lastErrno = 0;
    if (rc != SQLITE_OK) {
        if (dirfd >= 0)
            close(dirfd);
        if (h >= 0)
            close(h);
    } else
        pNew->pMethod = pLockingStyle;
    return rc;
}

// Searches for an unused file descriptor that was opened on the database 
// file identified by zPath with matching flags. Returns 0 if not found.
static ChromiumUnusedFd* findReusableFd(const char* zPath, int flags)
{
    ChromiumUnusedFd* pUnused = 0;

    struct stat sStat;
    if (!stat(zPath, &sStat)) {
        ChromiumFileId id;
        id.dev = sStat.st_dev;
        id.ino = sStat.st_ino;

        ChromiumOpenInfo* pO = 0;
        for (pO = openList; pO && memcmp(&id, &pO->fileId, sizeof(id)); pO = pO->pNext) { }
        if (pO) {
            ChromiumUnusedFd** pp;
            for (pp = &pO->pUnused; *pp && (*pp)->flags != flags; pp = &((*pp)->pNext)) { }
            pUnused = *pp;
            if (pUnused)
                *pp = pUnused->pNext;
        }
    }
    return pUnused;
}

// Opens a file.
//
// vfs - pointer to the sqlite3_vfs object.
// fileName - the name of the file.
// id - the structure that will manipulate the newly opened file.
// desiredFlags - the desired open mode flags.
// usedFlags - the actual open mode flags that were used.
static int chromiumOpen(sqlite3_vfs* vfs, const char* fileName,
                        sqlite3_file* id, int desiredFlags, int* usedFlags)
{
    // The mask 0x00007F00 gives us the 7 bits that determine the type of the file SQLite is trying to open.
    int fileType = desiredFlags & 0x00007F00;

    memset(id, 0, sizeof(ChromiumFile));
    ChromiumFile* chromiumFile = reinterpret_cast<ChromiumFile*>(id);
    int fd = -1;
    if (fileType == SQLITE_OPEN_MAIN_DB) {
        ChromiumUnusedFd* unusedFd = findReusableFd(fileName, desiredFlags);
        if (unusedFd)
            fd = unusedFd->fd;
        else {
            unusedFd = static_cast<ChromiumUnusedFd*>(sqlite3_malloc(sizeof(*unusedFd)));
            if (!unusedFd)
                return SQLITE_NOMEM;
        }
        chromiumFile->pUnused = unusedFd;
    }

    if (fd < 0) {
        fd = ChromiumBridge::databaseOpenFile(fileName, desiredFlags);
        if ((fd < 0) && (desiredFlags & SQLITE_OPEN_READWRITE)) {
            int newFlags = (desiredFlags & ~(SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)) | SQLITE_OPEN_READONLY;
            fd = ChromiumBridge::databaseOpenFile(fileName, newFlags);
        }
    }
    if (fd < 0) {
        sqlite3_free(chromiumFile->pUnused);
        return SQLITE_CANTOPEN;
    }

    if (usedFlags)
        *usedFlags = desiredFlags;
    if (chromiumFile->pUnused) {
        chromiumFile->pUnused->fd = fd;
        chromiumFile->pUnused->flags = desiredFlags;
    }

    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);

    int noLock = (fileType != SQLITE_OPEN_MAIN_DB);
    int rc = fillInChromiumFile(vfs, fd, -1, id, fileName, noLock);
    if (rc != SQLITE_OK)
        sqlite3_free(chromiumFile->pUnused);
    return rc;
}

// Deletes the given file.
//
// vfs - pointer to the sqlite3_vfs object.
// fileName - the name of the file.
// syncDir - determines if the directory to which this file belongs
//           should be synched after the file is deleted.
static int chromiumDelete(sqlite3_vfs*, const char* fileName, int syncDir)
{
    return ChromiumBridge::databaseDeleteFile(fileName, syncDir);
}

// Check the existance and status of the given file.
//
// vfs - pointer to the sqlite3_vfs object.
// fileName - the name of the file.
// flag - the type of test to make on this file.
// res - the result.
static int chromiumAccess(sqlite3_vfs*, const char* fileName, int flag, int* res)
{
    int attr = static_cast<int>(ChromiumBridge::databaseGetFileAttributes(fileName));
    if (attr < 0) {
        *res = 0;
        return SQLITE_OK;
    }

    switch (flag) {
    case SQLITE_ACCESS_EXISTS:
        *res = 1; // if the file doesn't exist, attr < 0
        break;
    case SQLITE_ACCESS_READWRITE:
        *res = (attr & W_OK) && (attr & R_OK);
        break;
    case SQLITE_ACCESS_READ:
        *res = (attr & R_OK);
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
static int chromiumFullPathname(sqlite3_vfs* vfs, const char* relativePath,
                                int, char* absolutePath)
{
    // The renderer process doesn't need to know the absolute path of the file
    sqlite3_snprintf(vfs->mxPathname, absolutePath, "%s", relativePath);
    return SQLITE_OK;
}

#ifndef SQLITE_OMIT_LOAD_EXTENSION
// We disallow loading DSOs inside the renderer process, so the following procedures are no-op.
static void* chromiumDlOpen(sqlite3_vfs*, const char*)
{
    return 0;
}

static void chromiumDlError(sqlite3_vfs*, int, char*)
{
}

static void (*chromiumDlSym(sqlite3_vfs*, void*, const char*))()
{
    return 0;
}

static void chromiumDlClose(sqlite3_vfs*, void*)
{
}
#else
#define chromiumDlOpen 0
#define chromiumDlError 0
#define chromiumDlSym 0
#define chromiumDlClose 0
#endif // SQLITE_OMIT_LOAD_EXTENSION

// Generates a seed for SQLite's PRNG.
static int chromiumRandomness(sqlite3_vfs*, int nBuf, char *zBuf)
{
    ASSERT(static_cast<size_t>(nBuf) >= (sizeof(time_t) + sizeof(int)));

    memset(zBuf, 0, nBuf);
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        time_t t;
        time(&t);
        memcpy(zBuf, &t, sizeof(t));
        int pid = getpid();
        memcpy(&zBuf[sizeof(t)], &pid, sizeof(pid));
        return sizeof(t) + sizeof(pid);
    }
    nBuf = read(fd, zBuf, nBuf);
    close(fd);
    return nBuf;
}

// Sleeps for at least |microseconds|, and returns the actual
// amount of time spent sleeping (in microseconds).
static int chromiumSleep(sqlite3_vfs*, int microseconds)
{
#if OS(DARWIN)
    usleep(microseconds);
    return microseconds;
#else
    // Round to the nearest second.
    int seconds = (microseconds + 999999) / 1000000;
    sleep(seconds);
    return seconds * 1000000;
#endif
}

// Retrieves the current system time (UTC).
static int chromiumCurrentTime(sqlite3_vfs*, double* now)
{
    struct timeval timeval;
    gettimeofday(&timeval, 0);
    *now = 2440587.5 + timeval.tv_sec / 86400.0 + timeval.tv_usec / 86400000000.0;
    return 0;
}

// This is not yet implemented in SQLite core.
static int chromiumGetLastError(sqlite3_vfs*, int, char*)
{
    return 0;
}

// Same as MAX_PATHNAME from sqlite's os_unix.c.
static const int chromiumMaxPathname = 512;

namespace WebCore {

void SQLiteFileSystem::registerSQLiteVFS()
{
    static sqlite3_vfs chromium_vfs = {
        1,
        sizeof(ChromiumFile),
        chromiumMaxPathname,
        0,
        "chromium_vfs",
        0,
        chromiumOpen,
        chromiumDelete,
        chromiumAccess,
        chromiumFullPathname,
        chromiumDlOpen,
        chromiumDlError,
        chromiumDlSym,
        chromiumDlClose,
        chromiumRandomness,
        chromiumSleep,
        chromiumCurrentTime,
        chromiumGetLastError
    };
    sqlite3_vfs_register(&chromium_vfs, 0);
}

} // namespace WebCore
