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
** licenses may use this file in accordance with the Qt Commercial License
** Agreement provided with the Software.
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

#include "_qfileinfo.h"
#include "qdatetime.h"
#include "_qdir.h"

extern bool qt_file_access( const QString& fn, int t );

// NOT REVISED
/*!
  \class QFileInfo qfileinfo.h
  \brief The QFileInfo class provides system-independent file information.

  \ingroup io

  QFileInfo provides information about a file's name and position (path) in
  the file system, its access rights and whether it is a directory or a
  symbolic link.  Its size and last modified/read times are also available.

  To speed up performance QFileInfo caches information about the file. Since
  files can be changed by other users or programs, or even by other parts of
  the same program there is a function that refreshes the file information;
  refresh(). If you would rather like a QFileInfo to access the file system
  every time you request information from it, you can call the function
  setCaching( FALSE ).

  A QFileInfo can point to a file using either a relative or an absolute
  file path. Absolute file paths begin with the directory separator
  ('/') or a drive specification (not applicable to UNIX).
  Relative file names begin with a directory name or a file name and specify
  a path relative to the current directory. An example of
  an absolute path is the string "/tmp/quartz". A relative path might look like
  "src/fatlib". You can use the function isRelative() to check if a QFileInfo
  is using a relative or an absolute file path. You can call the function
  convertToAbs() to convert a relative QFileInfo to an absolute one.

  If you need to read and traverse directories, see the QDir class.
*/


/*!
  Constructs a new empty QFileInfo.
*/

QFileInfo::QFileInfo()
{
    fic	  = 0;
    cache = TRUE;
}

/*!
  Constructs a new QFileInfo that gives information about the given file.
  The string given can be an absolute or a relative file path.

  \sa bool setFile(QString ), isRelative(), QDir::setCurrent(),
  QDir::isRelativePath()
*/

QFileInfo::QFileInfo( const QString &file )
{
    fn	  = file;
    slashify( fn );
    fic	  = 0;
    cache = TRUE;
}

/*!
  Constructs a new QFileInfo that gives information about \e file.

  If the file has a relative path, the QFileInfo will also have one.

  \sa isRelative()
*/

QFileInfo::QFileInfo( const QFile &file )
{
    fn	  = file.name();
    slashify( fn );
    fic	  = 0;
    cache = TRUE;
}

/*!
  Constructs a new QFileInfo that gives information about the file
  named \e fileName in the directory \e d.

  If the directory has a relative path, the QFileInfo will also have one.

  \sa isRelative()
*/
#ifndef QT_NO_DIR
QFileInfo::QFileInfo( const QDir &d, const QString &fileName )
{
    fn	  = d.filePath( fileName );
    slashify( fn );
    fic	  = 0;
    cache = TRUE;
}
#endif
/*!
  Constructs a new QFileInfo that is a copy of \e fi.
*/

QFileInfo::QFileInfo( const QFileInfo &fi )
{
    fn = fi.fn;
    if ( fi.fic ) {
	fic = new QFileInfoCache;
	*fic = *fi.fic;
    } else {
	fic = 0;
    }
    cache = fi.cache;
}

/*!
  Destructs the QFileInfo.
*/

QFileInfo::~QFileInfo()
{
    delete fic;
}


/*!
  Makes a copy of \e fi and assigns it to this QFileInfo.
*/

QFileInfo &QFileInfo::operator=( const QFileInfo &fi )
{
    fn = fi.fn;
    if ( !fi.fic ) {
	delete fic;
	fic = 0;
    } else {
	if ( !fic ) {
	    fic = new QFileInfoCache;
	    CHECK_PTR( fic );
	}
	*fic = *fi.fic;
    }
    cache = fi.cache;
    return *this;
}


/*!
  Sets the file to obtain information about.

  The string given can be an absolute or a relative file path. Absolute file
  paths begin with the directory separator (e.g. '/' under UNIX) or a drive
  specification (not applicable to UNIX). Relative file names begin with a
  directory name or a file name and specify a path relative to the current
  directory.

  Example:
  \code
    #include <qfileinfo.h>
    #include <qdir.h>

    void test()
    {
	QString absolute = "/liver/aorta";
	QString relative = "liver/aorta";
	QFileInfo fi1( absolute );
	QFileInfo fi2( relative );

	QDir::setCurrent( QDir::rootDirPath() );
				// fi1 and fi2 now point to the same file

	QDir::setCurrent( "/tmp" );
				// fi1 now points to "/liver/aorta",
				// while fi2 points to "/tmp/liver/aorta"
    }
  \endcode

  \sa isRelative(), QDir::setCurrent(), QDir::isRelativePath()
*/

void QFileInfo::setFile( const QString &file )
{
    fn = file;
    slashify( fn );
    delete fic;
    fic = 0;
}

/*!
  Sets the file to obtain information about.

  If the file has a relative path, the QFileInfo will also have one.

  \sa isRelative()
*/

void QFileInfo::setFile( const QFile &file )
{
    fn	= file.name();
    slashify( fn );
    delete fic;
    fic = 0;
}

/*!
  Sets the file to obtains information about to \e fileName in the
  directory \e d.

  If the directory has a relative path, the QFileInfo will also have one.

  \sa isRelative()
*/
#ifndef QT_NO_DIR
void QFileInfo::setFile( const QDir &d, const QString &fileName )
{
    fn	= d.filePath( fileName );
    slashify( fn );
    delete fic;
    fic = 0;
}
#endif

/*!
  Returns TRUE if the file pointed to exists, otherwise FALSE.
*/

bool QFileInfo::exists() const
{
    return qt_file_access( fn, F_OK );
}

/*!
  Refresh the information about the file, i.e. read in information from the
  file system the next time a cached property is fetched.

  \sa setCaching()
*/

void QFileInfo::refresh() const
{
    QFileInfo *that = (QFileInfo*)this;		// Mutable function
    delete that->fic;
    that->fic = 0;
}

/*!
  \fn bool QFileInfo::caching() const
  Returns TRUE if caching is enabled.
  \sa setCaching(), refresh()
*/

/*!
  Enables caching of file information if \e enable is TRUE, or disables it
  if \e enable is FALSE.

  When caching is enabled, QFileInfo reads the file information the first
  time

  Caching is enabled by default.

  \sa refresh(), caching()
*/

void QFileInfo::setCaching( bool enable )
{
    if ( cache == enable )
	return;
    cache = enable;
    if ( cache ) {
	delete fic;
	fic = 0;
    }
}


/*!
  Returns the name, i.e. the file name including the path (which can be
  absolute or relative).

  \sa isRelative(), absFilePath()
*/

QString QFileInfo::filePath() const
{
    return fn;
}

/*!
  Returns the base name of the file.

  The base name consists of all characters in the file name up to (but not
  including) the first '.' character.  The path is not included.

  Example:
  \code
     QFileInfo fi( "/tmp/abdomen.lower" );
     QString base = fi.baseName();		// base = "abdomen"
  \endcode

  \sa fileName(), extension()
*/

QString QFileInfo::baseName() const
{
    QString tmp = fileName();
    int pos = tmp.find( '.' );
    if ( pos == -1 )
	return tmp;
    else
	return tmp.left( pos );
}

/*!
  Returns the extension name of the file.

  If \a complete is TRUE (the default), extension() returns the string
  of all characters in the file name after (but not including) the
  first '.'  character.  For a file named "archive.tar.gz" this
  returns "tar.gz".

  If \a complete is FALSE, extension() returns the string of all
  characters in the file name after (but not including) the last '.'
  character.  For a file named "archive.tar.gz" this returns "gz".

  Example:
  \code
     QFileInfo fi( "lex.yy.c" );
     QString ext = fi.extension();		// ext = "yy.c"
     QString ext = fi.extension( FALSE );	// ext = "c"
  \endcode

  \sa fileName(), baseName()

*/

QString QFileInfo::extension( bool complete ) const
{
    QString s = fileName();
    int pos = complete ? s.find( '.' ) : s.findRev( '.' );
    if ( pos < 0 )
	return QString::fromLatin1( "" );
    else
	return s.right( s.length() - pos - 1 );
}

/*!
  Returns the directory path of the file.

  If the QFileInfo is relative and \e absPath is FALSE, the QDir will be
  relative, otherwise it will be absolute.

  \sa dirPath(), filePath(), fileName(), isRelative()
*/
#ifndef QT_NO_DIR
QDir QFileInfo::dir( bool absPath ) const
{
    return QDir( dirPath(absPath) );
}
#endif


/*!
  Returns TRUE if the file is readable.
  \sa isWritable(), isExecutable(), permission()
*/

bool QFileInfo::isReadable() const
{
    return qt_file_access( fn, R_OK );
}

/*!
  Returns TRUE if the file is writable.
  \sa isReadable(), isExecutable(), permission()
*/

bool QFileInfo::isWritable() const
{
    return qt_file_access( fn, W_OK );
}

/*!
  Returns TRUE if the file is executable.
  \sa isReadable(), isWritable(), permission()
*/

bool QFileInfo::isExecutable() const
{
    return qt_file_access( fn, X_OK );
}


/*!
  Returns TRUE if the file path name is relative to the current directory,
  FALSE if the path is absolute (e.g. under UNIX a path is relative if it
  does not start with a '/').

  According to Einstein this function should always return TRUE.
*/
#ifndef QT_NO_DIR
bool QFileInfo::isRelative() const
{
    return QDir::isRelativePath( fn );
}

/*!
  Converts the file path name to an absolute path.

  If it is already absolute nothing is done.

  \sa filePath(), isRelative()
*/

bool QFileInfo::convertToAbs()
{
    if ( isRelative() )
	fn = absFilePath();
    return QDir::isRelativePath( fn );
}
#endif

// KWQ hacks ---------------------------------------------------------------

#endif // USING_BORROWED_QDIR

// -------------------------------------------------------------------------
