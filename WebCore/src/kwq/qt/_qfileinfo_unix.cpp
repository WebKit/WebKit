/****************************************************************************
** $Id$
**
** Implementation of QFileInfo class
**
** Created : 950628
**
** Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
**
** This file is part of the tools module of the Qt GUI Toolkit.
**
** This file may be distributed under the terms of the Q Public License
** as defined by Trolltech AS of Norway and appearing in the file
** LICENSE.QPL included in the packaging of this file.
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** Licensees holding valid Qt Enterprise Edition or Qt Professional Edition
** licenses for Unix/X11 or for Qt/Embedded may use this file in accordance
** with the Qt Commercial License Agreement provided with the Software.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.trolltech.com/pricing.html or email sales@trolltech.com for
**   information about Qt Commercial License Agreements.
** See http://www.trolltech.com/qpl/ for QPL licensing information.
** See http://www.trolltech.com/gpl/ for GPL licensing information.
**
** Contact info@trolltech.com if any conditions of this licensing are
** not clear to you.
**
**********************************************************************/

// KWQ hacks ---------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef USING_BORROWED_QDIR

// -------------------------------------------------------------------------

#if defined(_OS_SUN_)
#define readlink _qt_hide_readlink
#endif

#include <pwd.h>
#include <grp.h>

#include "_qfileinfo.h"
#include "qdatetime.h"
#include "_qdir.h"

#if defined(_OS_SUN_)
#undef readlink
extern "C" int readlink( const char *, void *, uint );
#endif


void QFileInfo::slashify( QString& )
{
    return;
}


void QFileInfo::makeAbs( QString & )
{
    return;
}

extern bool qt_file_access( const QString& fn, int t );

/*!
  Returns TRUE if we are pointing to a real file.
  \sa isDir(), isSymLink()
*/
bool QFileInfo::isFile() const
{
    if ( !fic || !cache )
	doStat();
    return fic ? (fic->st.st_mode & STAT_MASK) == STAT_REG : FALSE;
}

/*!
  Returns TRUE if we are pointing to a directory or a symbolic link to
  a directory.
  \sa isFile(), isSymLink()
*/

bool QFileInfo::isDir() const
{
    if ( !fic || !cache )
	doStat();
    return fic ? (fic->st.st_mode & STAT_MASK) == STAT_DIR : FALSE;
}

/*!
  Returns TRUE if we are pointing to a symbolic link.
  \sa isFile(), isDir(), readLink()
*/

bool QFileInfo::isSymLink() const
{
    if ( !fic || !cache )
	doStat();
    return fic ? fic->isSymLink : FALSE;
}


/*!
  Returns the name a symlink points to, or a null QString if the
  object does not refer to a symbolic link.

  This name may not represent an existing file; it is only a string.
  QFileInfo::exists() returns TRUE if the symlink points to an
  existing file.

  \sa exists(), isSymLink(), isDir(), isFile()
*/

QString QFileInfo::readLink() const
{
    QString r;

#if defined(_OS_UNIX_) && !defined(_OS_OS2EMX_)
    char s[PATH_MAX+1];
    if ( !isSymLink() )
	return QString();
    int len = readlink( QFile::encodeName(fn).data(), s, PATH_MAX );
    if ( len >= 0 ) {
	s[len] = '\0';
	r = QFile::decodeName(s);
    }
#endif

    return r;
}

static const uint nobodyID = (uint) -2;

/*!
  Returns the owner of the file.

  On systems where files do not have owners this function returns 0.

  Note that this function can be time-consuming under UNIX. (in the order
  of milliseconds on a 486 DX2/66 running Linux).

  \sa ownerId(), group(), groupId()
*/

QString QFileInfo::owner() const
{
    passwd *pw = getpwuid( ownerId() );
    if ( pw )
	return QFile::decodeName( pw->pw_name );
    return QString::null;
}

/*!
  Returns the id of the owner of the file.

  On systems where files do not have owners this function returns ((uint) -2).

  \sa owner(), group(), groupId()
*/

uint QFileInfo::ownerId() const
{
    if ( !fic || !cache )
	doStat();
    if ( fic )
	return fic->st.st_uid;
    return nobodyID;
}

/*!
  Returns the group the file belongs to.

  On systems where files do not have groups this function always
  returns 0.

  Note that this function can be time-consuming under UNIX (in the order of
  milliseconds on a 486 DX2/66 running Linux).

  \sa groupId(), owner(), ownerId()
*/

QString QFileInfo::group() const
{
    struct group *gr = getgrgid( groupId() );
    if ( gr )
	return QFile::decodeName( gr->gr_name );
    return QString::null;
}

/*!
  Returns the id of the group the file belongs to.

  On systems where files do not have groups this function always
  returns ((uind) -2).

  \sa group(), owner(), ownerId()
*/

uint QFileInfo::groupId() const
{
    if ( !fic || !cache )
	doStat();
    if ( fic )
	return fic->st.st_gid;
    return nobodyID;
}


/*!
  \fn bool QFileInfo::permission( int permissionSpec ) const

  Tests for file permissions.  The \e permissionSpec argument can be several
  flags of type PermissionSpec or'ed together to check for permission
  combinations.

  On systems where files do not have permissions this function always
  returns TRUE.

  Example:
  \code
    QFileInfo fi( "/tmp/tonsils" );
    if ( fi.permission( QFileInfo::WriteUser | QFileInfo::ReadGroup ) )
	qWarning( "Tonsils can be changed by me, and the group can read them.");
    if ( fi.permission( QFileInfo::WriteGroup | QFileInfo::WriteOther ) )
	qWarning( "Danger! Tonsils can be changed by the group or others!" );
  \endcode

  \sa isReadable(), isWritable(), isExecutable()
*/

bool QFileInfo::permission( int permissionSpec ) const
{
    if ( !fic || !cache )
	doStat();
    if ( fic ) {
	uint mask = 0;
	if ( permissionSpec & ReadUser)
	    mask |= S_IRUSR;
	if ( permissionSpec & WriteUser)
	    mask |= S_IWUSR;
	if ( permissionSpec & ExeUser)
	    mask |= S_IXUSR;
	if ( permissionSpec & ReadGroup)
	    mask |= S_IRGRP;
	if ( permissionSpec & WriteGroup)
	    mask |= S_IWGRP;
	if ( permissionSpec & ExeGroup)
	    mask |= S_IXGRP;
	if ( permissionSpec & ReadOther)
	    mask |= S_IROTH;
	if ( permissionSpec & WriteOther)
	    mask |= S_IWOTH;
	if ( permissionSpec & ExeOther)
	    mask |= S_IXOTH;
	if ( mask ) {
	   return (fic->st.st_mode & mask) == mask;
	} else {
#if defined(CHECK_NULL)
	   qWarning( "QFileInfo::permission: permissionSpec is 0" );
#endif
	   return TRUE;
	}
    } else {
	return FALSE;
    }
}

/*!
  Returns the file size in bytes, or 0 if the file does not exist if the size
  cannot be fetched.
*/

uint QFileInfo::size() const
{
    if ( !fic || !cache )
	doStat();
    if ( fic )
	return (uint)fic->st.st_size;
    else
	return 0;
}


/*!
  Returns the date and time when the file was last modified.
  \sa lastRead()
*/

QDateTime QFileInfo::lastModified() const
{
    QDateTime dt;
    if ( !fic || !cache )
	doStat();
    if ( fic )
	dt.setTime_t( fic->st.st_mtime );
    return dt;
}

/*!
  Returns the date and time when the file was last read (accessed).

  On systems that do not support last read times, the modification time is
  returned.

  \sa lastModified()
*/

QDateTime QFileInfo::lastRead() const
{
    QDateTime dt;
    if ( !fic || !cache )
	doStat();
    if ( fic )
	dt.setTime_t( fic->st.st_atime );
    return dt;
}


void QFileInfo::doStat() const
{
    QFileInfo *that = ((QFileInfo*)this);	// mutable function
    if ( !that->fic )
	that->fic = new QFileInfoCache;
    STATBUF *b = &that->fic->st;
    that->fic->isSymLink = FALSE;

#if defined(_OS_UNIX_) && defined(S_IFLNK)
    if ( ::lstat(QFile::encodeName(fn),b) == 0 ) {
	if ( S_ISLNK( b->st_mode ) )
	    that->fic->isSymLink = TRUE;
	else
	    return;
    }
#endif
    int r;

    r = STAT( QFile::encodeName(fn), b );

    if ( r != 0 && !that->fic->isSymLink ) {
	delete that->fic;
	that->fic = 0;
    }
}

/*!
  Returns the directory path of the file.

  If \e absPath is TRUE an absolute path is always returned.

  \sa dir(), filePath(), fileName(), isRelative()
*/
#ifndef QT_NO_DIR
QString QFileInfo::dirPath( bool absPath ) const
{
    QString s;
    if ( absPath )
	s = absFilePath();
    else
	s = fn;
    int pos = s.findRev( '/' );
    if ( pos == -1 ) {
	return QString::fromLatin1(".");
    } else {
	if ( pos == 0 )
	    return QString::fromLatin1( "/" );
	return s.left( pos );
    }
}
#endif
/*!
  Returns the name of the file, the file path is not included.

  Example:
  \code
     QFileInfo fi( "/tmp/abdomen.lower" );
     QString name = fi.fileName();		// name = "abdomen.lower"
  \endcode

  \sa isRelative(), filePath(), baseName(), extension()
*/

QString QFileInfo::fileName() const
{
    int p = fn.findRev( '/' );
    if ( p == -1 ) {
	return fn;
    } else {
	return fn.mid(p+1);
    }
}

/*!
  Returns the absolute path name.

  The absolute path name is the file name including the absolute path. If
  the QFileInfo is absolute (i.e. not relative) this function will return
  the same string as filePath().

  Note that this function can be time-consuming under UNIX. (in the order
  of milliseconds on a 486 DX2/66 running Linux).

  \sa isRelative(), filePath()
*/
#ifndef QT_NO_DIR
QString QFileInfo::absFilePath() const
{
    if ( QDir::isRelativePath(fn) ) {
	QString tmp = QDir::currentDirPath();
	tmp += '/';
	tmp += fn;
	makeAbs( tmp );
	return QDir::cleanDirPath( tmp );
    } else {
	QString tmp = fn;
	makeAbs( tmp );
	return QDir::cleanDirPath( tmp );
    }

}
#endif

// KWQ hacks ---------------------------------------------------------------

#endif // USING_BORROWED_QDIR

// -------------------------------------------------------------------------
