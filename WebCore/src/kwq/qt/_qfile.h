/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef _QFILE_H_
#define _QFILE_H_

#include <config.h>

#ifdef USING_BORROWED_QFILE

#include <KWQDef.h>

// USING_BORROWED_QFILE =======================================================

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#if !defined(PATH_MAX)
#if defined(MAXPATHLEN)
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 1024
#endif
#endif

#undef STATBUF
#undef STAT
#undef STAT_REG
#undef STAT_DIR
#undef STAT_LNK
#undef STAT_MASK
#undef FILENO
#undef OPEN
#undef CLOSE
#undef LSEEK
#undef READ
#undef WRITE
#undef ACCESS
#undef GETCWD
#undef CHDIR
#undef MKDIR
#undef RMDIR
#undef OPEN_RDONLY
#undef OPEN_WRONLY
#undef OPEN_CREAT
#undef OPEN_TRUNC
#undef OPEN_APPEND
#undef OPEN_TEXT
#undef OPEN_BINARY

#define STATBUF	struct stat
#define STATBUF4TSTAT	struct stat
#define STAT		::stat
#define FSTAT		::fstat
#define STAT_REG	S_IFREG
#define STAT_DIR	S_IFDIR
#define STAT_MASK	S_IFMT
#if defined(S_IFLNK)
# define STAT_LNK	S_IFLNK
#endif
#define FILENO		fileno
#define OPEN		::open
#define CLOSE		::close
#define LSEEK		::lseek
#define READ		::read
#define WRITE		::write
#define ACCESS		::access
#define GETCWD	::getcwd
#define CHDIR		::chdir
#define MKDIR		::mkdir
#define RMDIR		::rmdir
#define OPEN_RDONLY	O_RDONLY
#define OPEN_WRONLY	O_WRONLY
#define OPEN_RDWR	O_RDWR
#define OPEN_CREAT	O_CREAT
#define OPEN_TRUNC	O_TRUNC
#define OPEN_APPEND	O_APPEND
#if defined(O_TEXT)
# define OPEN_TEXT	O_TEXT
# define OPEN_BINARY O_BINARY
#endif

#define F_OK	0

struct QFileInfoCache
{
    STATBUF st;
    bool isSymLink;
};


#include "_qiodevice.h"
#include "qstring.h"
#include "qcstring.h"


// class QFile =================================================================

class QFile : public QIODevice {
public:

    // typedefs ----------------------------------------------------------------

    typedef QCString(*EncoderFn)(const QString &);
    typedef QString(*DecoderFn)(const QCString &);

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    static QCString encodeName(const QString &);
    static QString decodeName(const QCString &);
    static void setEncodingFunction(EncoderFn);
    static void setDecodingFunction(DecoderFn);
    static bool exists(const QString &);
    static bool remove(const QString &fileName);

    // constructors, copy constructors, and destructors ------------------------

    QFile();
    QFile(const QString &);
    ~QFile();

    // member functions --------------------------------------------------------

    bool exists() const;
    bool open(int);
    void close();
    int readBlock(char *, uint);
    uint size() const;


    bool remove();

    QString	name() const;
    void setName(const QString &);

    bool open(int, FILE *);
    bool open(int, int);
    void flush();

    int  at() const;
    bool at(int);
    bool atEnd() const;

    int writeBlock(const char *, uint);
    int writeBlock(const QByteArray &);
    int readLine(char *, uint);
    int readLine(QString &, uint);

    int getch();
    int putch(int);
    int ungetch(int);

    int  handle() const;

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------

protected:
    QString fn;
    FILE *fh;
    int fd;
    int length;
    bool ext_f;
    void *d;

// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    // note that these are "standard" (no pendantic stuff needed)
    QFile(const QFile &);
    QFile &operator=(const QFile &);

    void init();
    QCString ungetchBuffer;

}; // class QFile ==============================================================

inline int QFile::at() const
{
    return ioIndex;
}

inline QString QFile::name() const
{ return fn; }


#endif // USING_BORROWED_QFILE

#endif // _QFILE_H
