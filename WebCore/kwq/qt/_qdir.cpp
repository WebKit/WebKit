/****************************************************************************
** $Id$
**
** Implementation of QDir class
**
** Created : 950427
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

#include <qdir.h>

// -------------------------------------------------------------------------

#ifndef QT_NO_DIR
#include "_qfileinfo.h"
#include "qregexp.h"
#include "qstringlist.h"
#include <stdlib.h>
#include <ctype.h>

// NOT REVISED
/*!
  \class QDir qdir.h
  \brief Traverses directory structures and contents in a
	    platform-independent way.

  \ingroup io

  A QDir can point to a file using either a relative or an absolute file
  path. Absolute file paths begin with the directory separator ('/') or a
  drive specification (not applicable to UNIX).	 Relative file names begin
  with a directory name or a file name and specify a path relative to the
  current directory.

  An example of an absolute path is the string "/tmp/quartz", a relative
  path might look like "src/fatlib". You can use the function isRelative()
  to check if a QDir is using a relative or an absolute file path. You can
  call the function convertToAbs() to convert a relative QDir to an
  absolute one.

  The directory "example" under the current directory is checked for existence
  in the example below:

  \code
    QDir d( "example" );			// "./example"
    if ( !d.exists() )
	qWarning( "Cannot find the example directory" );
  \endcode

  If you always use '/' as a directory separator, Qt will translate your
  paths to conform to the underlying operating system.

  cd() and cdUp() can be used to navigate the directory tree. Note that the
  logical cd and cdUp operations are not performed if the new directory does
  not exist.

  Example:
  \code
    QDir d = QDir::root();			// "/"
    if ( !d.cd("tmp") ) {			// "/tmp"
	qWarning( "Cannot find the \"/tmp\" directory" );
    } else {
	QFile f( d.filePath("ex1.txt") );	// "/tmp/ex1.txt"
	if ( !f.open(IO_ReadWrite) )
	    qWarning( "Cannot create the file %s", f.name() );
    }
  \endcode

  To read the contents of a directory you can use the entryList() and
  entryInfoList() functions.

  Example:
  \code
    #include <stdio.h>
    #include <qdir.h>

    //
    // This program scans the current directory and lists all files
    // that are not symbolic links, sorted by size with the smallest files
    // first.
    //

    int main( int argc, char **argv )
    {
	QDir d;
	d.setFilter( QDir::Files | QDir::Hidden | QDir::NoSymLinks );
	d.setSorting( QDir::Size | QDir::Reversed );

	const QFileInfoList *list = d.entryInfoList();
	QFileInfoListIterator it( *list );	// create list iterator
	QFileInfo *fi;				// pointer for traversing

	printf( "     BYTES FILENAME\n" );	// print header
	while ( (fi=it.current()) ) {		// for each file...
	    printf( "%10li %s\n", fi->size(), fi->fileName().data() );
	    ++it;				// goto next list element
	}
    }
  \endcode
*/


/*!
  Constructs a QDir pointing to the current directory.
  \sa currentDirPath()
*/

QDir::QDir()
{
    dPath = QString::fromLatin1(".");
    init();
}

/*!
  Constructs a QDir.

  \arg \e path is the directory.
  \arg \e nameFilter is the file name filter.
  \arg \e sortSpec is the sort specification, which describes how to
  sort the files in the directory.
  \arg \e filterSpec is the filter specification, which describes how
  to filter the files in the directory.

  Most of these arguments (except \e path) have optional values.

  Example:
  \code
    // lists all files in /tmp

    QDir d( "/tmp" );
    for ( int i=0; i<d.count(); i++ )
	printf( "%s\n", d[i] );
  \endcode

  If \e path is "" or null, the directory is set to "." (the current
  directory).  If \e nameFilter is "" or null, it is set to "*" (all
  files).

  No check is made to ensure that the directory exists.

  \sa exists(), setPath(), setNameFilter(), setFilter(), setSorting()
*/

QDir::QDir( const QString &path, const QString &nameFilter,
	    int sortSpec, int filterSpec )
{
    init();
    dPath = cleanDirPath( path );
    if ( dPath.isEmpty() )
	dPath = QString::fromLatin1(".");
    nameFilt = nameFilter;
    if ( nameFilt.isEmpty() )
	nameFilt = QString::fromLatin1("*");
    filtS = (FilterSpec)filterSpec;
    sortS = (SortSpec)sortSpec;
}

/*!
  Constructs a QDir that is a copy of the given directory.
  \sa operator=()
*/

QDir::QDir( const QDir &d )
{
    dPath = d.dPath;
    fList = 0;
    fiList = 0;
    nameFilt = d.nameFilt;
    dirty = TRUE;
    allDirs = d.allDirs;
    filtS = d.filtS;
    sortS = d.sortS;
}


void QDir::init()
{
    fList = 0;
    fiList = 0;
    nameFilt = QString::fromLatin1("*");
    dirty = TRUE;
    allDirs = FALSE;
    filtS = All;
    sortS = SortSpec(Name | IgnoreCase);
}

/*!
  Destructs the QDir and cleans up.
*/

QDir::~QDir()
{
    if ( fList )
       delete fList;
    if ( fiList )
       delete fiList;
}


/*!
  Sets the path of the directory. The path is cleaned of redundant ".", ".."
  and multiple separators. No check is made to ensure that a directory
  with this path exists.

  The path can be either absolute or relative. Absolute paths begin with the
  directory separator ('/') or a drive specification (not
  applicable to UNIX).
  Relative file names begin with a directory name or a file name and specify
  a path relative to the current directory. An example of
  an absolute path is the string "/tmp/quartz", a relative path might look like
  "src/fatlib". You can use the function isRelative() to check if a QDir
  is using a relative or an absolute file path. You can call the function
  convertToAbs() to convert a relative QDir to an absolute one.

  \sa path(), absPath(), exists(), cleanDirPath(), dirName(),
      absFilePath(), isRelative(), convertToAbs()
*/

void QDir::setPath( const QString &path )
{
    dPath = cleanDirPath( path );
    if ( dPath.isEmpty() )
	dPath = QString::fromLatin1(".");
    dirty = TRUE;
}

/*!
  \fn  QString QDir::path() const
  Returns the path, this may contain symbolic links, but never contains
  redundant ".", ".." or multiple separators.

  The returned path can be either absolute or relative (see setPath()).

  \sa setPath(), absPath(), exists(), cleanDirPath(), dirName(),
  absFilePath(), convertSeparators()
*/

/*!
  Returns the absolute (a path that starts with '/') path, which may
  contain symbolic links, but never contains redundant ".", ".." or
  multiple separators.

  \sa setPath(), canonicalPath(), exists(),  cleanDirPath(), dirName(),
  absFilePath()
*/

QString QDir::absPath() const
{
    if ( QDir::isRelativePath(dPath) ) {
	QString tmp = currentDirPath();
	if ( tmp.right(1) != QString::fromLatin1("/") )
	    tmp += '/';
	tmp += dPath;
	return cleanDirPath( tmp );
    } else {
	return cleanDirPath( dPath );
    }
}

/*!
  Returns the name of the directory, this is NOT the same as the path, e.g.
  a directory with the name "mail", might have the path "/var/spool/mail".
  If the directory has no name (e.g. the root directory) a null string is
  returned.

  No check is made to ensure that a directory with this name actually exists.

  \sa path(), absPath(), absFilePath(), exists(), QString::isNull()
*/

QString QDir::dirName() const
{
    int pos = dPath.findRev( '/' );
    if ( pos == -1  )
	return dPath;
    return dPath.right( dPath.length() - pos - 1 );
}

/*!
  Returns the path name of a file in the directory. Does NOT check if
  the file actually exists in the directory. If the QDir is relative
  the returned path name will also be relative. Redundant multiple separators
  or "." and ".." directories in \e fileName will not be removed (see
  cleanDirPath()).

  If \e acceptAbsPath is TRUE a \e fileName starting with a separator
  ('/') will be returned without change.
  If \e acceptAbsPath is FALSE an absolute path will be appended to
  the directory path.

  \sa absFilePath(), isRelative(), canonicalPath()
*/

QString QDir::filePath( const QString &fileName,
			bool acceptAbsPath ) const
{
    if ( acceptAbsPath && !isRelativePath(fileName) )
	return QString(fileName);

    QString tmp = dPath;
    if ( tmp.isEmpty() || (tmp[(int)tmp.length()-1] != '/' && !!fileName &&
			   fileName[0] != '/') )
	tmp += '/';
    tmp += fileName;
    return tmp;
}

/*!
  Returns the absolute path name of a file in the directory. Does NOT check if
  the file actually exists in the directory. Redundant multiple separators
  or "." and ".." directories in \e fileName will NOT be removed (see
  cleanDirPath()).

  If \e acceptAbsPath is TRUE a \e fileName starting with a separator
  ('/') will be returned without change.
  if \e acceptAbsPath is FALSE an absolute path will be appended to
  the directory path.

  \sa filePath()
*/

QString QDir::absFilePath( const QString &fileName,
			   bool acceptAbsPath ) const
{
    if ( acceptAbsPath && !isRelativePath( fileName ) )
	return fileName;

    QString tmp = absPath();
    if ( tmp.isEmpty() || (tmp[(int)tmp.length()-1] != '/' && !!fileName &&
			   fileName[0] != '/') )
	tmp += '/';
    tmp += fileName;
    return tmp;
}


/*!
  Converts the '/' separators in \a pathName to system native
  separators.  Returns the translated string.

  On Windows, convertSeparators("c:/winnt/system32") returns
  "c:\winnt\system32".

  No conversion is done on UNIX.
*/

QString QDir::convertSeparators( const QString &pathName )
{
    QString n( pathName );
#if defined(_OS_FATFS_) || defined(_OS_OS2EMX_)
    for ( int i=0; i<(int)n.length(); i++ ) {
	if ( n[i] == '/' )
	    n[i] = '\\';
    }
#endif
    return n;
}


/*!
  Changes directory by descending into the given directory. Returns
  TRUE if the new directory exists and is readable. Note that the logical
  cd operation is NOT performed if the new directory does not exist.

  If \e acceptAbsPath is TRUE a path starting with a separator ('/')
  will cd to the absolute directory, if \e acceptAbsPath is FALSE
  any number of separators at the beginning of \e dirName will be removed.

  Example:
  \code
  QDir d = QDir::home();  // now points to home directory
  if ( !d.cd("c++") ) {	  // now points to "c++" under home directory if OK
      QFileInfo fi( d, "c++" );
      if ( fi.exists() ) {
	  if ( fi.isDir() )
	      qWarning( "Cannot cd into \"%s\".", (char*)d.absFilePath("c++") );
	  else
	      qWarning( "Cannot create directory \"%s\"\n"
		       "A file named \"c++\" already exists in \"%s\"",
		       (const char *)d.absFilePath("c++"),
		       (const char *)d.path() );
	  return;
      } else {
	  qWarning( "Creating directory \"%s\"",
		   (const char *) d.absFilePath("c++") );
	  if ( !d.mkdir( "c++" ) ) {
	      qWarning("Could not create directory \"%s\"",
		      (const char *)d.absFilePath("c++") );
	      return;
	  }
      }
  }
  \endcode

  Calling cd( ".." ) is equivalent to calling cdUp().

  \sa cdUp(), isReadable(), exists(), path()
*/

bool QDir::cd( const QString &dirName, bool acceptAbsPath )
{
    if ( dirName.isEmpty() || dirName==QString::fromLatin1(".") )
	return TRUE;
    QString old = dPath;
    if ( acceptAbsPath && !isRelativePath(dirName) ) {
	dPath = cleanDirPath( dirName );
    } else {
	if ( !isRoot() )
	    dPath += '/';
	dPath += dirName;
	if ( dirName.find('/') >= 0
		|| old == QString::fromLatin1(".")
		|| dirName == QString::fromLatin1("..") )
	    dPath = cleanDirPath( dPath );
    }
    if ( !exists() ) {
	dPath = old;			// regret
	return FALSE;
    }
    dirty = TRUE;
    return TRUE;
}

/*!
  Changes directory by moving one directory up the path followed to arrive
  at the current directory.

  Returns TRUE if the new directory exists and is readable. Note that the
  logical cdUp() operation is not performed if the new directory does not
  exist.

  \sa cd(), isReadable(), exists(), path()
*/

bool QDir::cdUp()
{
    return cd( QString::fromLatin1("..") );
}

/*!
  \fn QString QDir::nameFilter() const
  Returns the string set by setNameFilter()
*/

/*!
  Sets the name filter used by entryList() and entryInfoList().

  The name filter is a wildcarding filter that understands "*" and "?"
  wildcards, You may specify several filter entries separated by a " " or a ";". If
  you want entryList() and entryInfoList() to list all files ending with
  ".cpp" and all files ending with ".h", you simply call
  dir.setNameFilter("*.cpp *.h") or dir.setNameFilter("*.cpp;*.h")

  \sa nameFilter(), setFilter()
*/

void QDir::setNameFilter( const QString &nameFilter )
{
    nameFilt = nameFilter;
    if ( nameFilt.isEmpty() )
	nameFilt = QString::fromLatin1("*");
    dirty = TRUE;
}

/*!
  \fn QDir::FilterSpec QDir::filter() const
  Returns the value set by setFilter()
*/

/*! \enum QDir::FilterSpec

  This enum describes how QDir is to select what entries in a
  directory to return.  The filter value is specified by or-ing
  together values from the following list: <ul>

  <li> \c Dirs - List directories only
  <li> \c Files - List files only

  <li> \c  Drives - List disk drives (does nothing under unix)
  <li> \c  NoSymLinks - Do not list symbolic links (where they exist)
  <li> \c  Readable - List files for which the application has read access.
  <li> \c  Writable - List files for which the application has write access.
  <li> \c  Executable - List files for which the application has execute access
  <li> \c  Modified - Only list files that have been modified (does nothing
  under unix)
  <li> \c  Hidden - List hidden files (on unix, files starting with a .)
  <li> \c  System - List system files (does nothing under unix)
  </ul>

  If you do not set any of \c Readable, \c Writable or \c Executable,
  QDir will set all three of them.  This makes the default easy to
  write and at the same time useful.

  Examples: \c Readable|Writable means list all files for which the
  application has read access, write access or both.  \c Dirs|Drives
  means list drives, directories, all files that the application can
  read, write or execute, and also symlinks to such files/directories.
*/


/*!
  Sets the filter used by entryList() and entryInfoList(). The filter is used
  to specify the kind of files that should be returned by entryList() and
  entryInfoList().

  \sa filter(), setNameFilter()
*/

void QDir::setFilter( int filterSpec )
{
    if ( filtS == (FilterSpec) filterSpec )
	return;
    filtS = (FilterSpec) filterSpec;
    dirty = TRUE;
}

/*!
  \fn QDir::SortSpec QDir::sorting() const

  Returns the value set by setSorting()

  \sa setSorting()
*/

/*! \enum QDir::SortSpec

  This enum describes how QDir is to sort entries in a directory when
  it returns a list of them.  The sort value is specified by or-ing
  together values from the following list: <ul>

  <li> \c Name - sort by name
  <li> \c Time - sort by time (modification time)
  <li> \c Size - sort by file size
  <li> \c Unsorted - do not sort

  <li> \c DirsFirst - put all directories first in the list
  <li> \c Reversed - reverse the sort order
  <li> \c IgnoreCase - sort case-insensitively

  </ul>

  You can only specify one of the first four.  If you specify both \c
  DirsFirst and \c Reversed, directories are still put first but the
  list is otherwise reversed.
*/

// ### Unsorted+DirsFirst ? Unsorted+Reversed?

/*!
  Sets the sorting order used by entryList() and entryInfoList().

  The \e sortSpec is specified by or-ing values from the enum
  SortSpec. The different values are:

  One of these:
  <dl compact>
  <dt>Name<dd> Sort by name (alphabetical order).
  <dt>Time<dd> Sort by time (most recent first).
  <dt>Size<dd> Sort by size (largest first).
  <dt>Unsorted<dd> Use the operating system order (UNIX does NOT sort
  alphabetically).

  ORed with zero or more of these:

  <dt>DirsFirst<dd> Always put directory names first.
  <dt>Reversed<dd> Reverse sort order.
  <dt>IgnoreCase<dd> Ignore case when sorting by name.
  </dl>
*/

void QDir::setSorting( int sortSpec )
{
    if ( sortS == (SortSpec) sortSpec )
	return;
    sortS = (SortSpec) sortSpec;
    dirty = TRUE;
}

/*!
  \fn bool QDir::matchAllDirs() const
  Returns the value set by setMatchAllDirs()

  \sa setMatchAllDirs()
*/

/*!
  If \e enable is TRUE, all directories will be listed (even if they do not
  match the filter or the name filter), otherwise only matched directories
  will be listed.

  \bug Currently, directories that do not match the filter will not be
  included (the name filter will be ignored as expected).

  \sa matchAllDirs()
*/

void QDir::setMatchAllDirs( bool enable )
{
    if ( (bool)allDirs == enable )
	return;
    allDirs = enable;
    dirty = TRUE;
}


/*!
  Returns the number of files that was found.
  Equivalent to entryList().count().
  \sa operator[](), entryList()
*/

uint QDir::count() const
{
    return entryList().count();
}

/*!
  Returns the file name at position \e index in the list of found file
  names.
  Equivalent to entryList().at(index).

  Returns null if the \e index is out of range or if the entryList()
  function failed.

  \sa count(), entryList()
*/

QString QDir::operator[]( int index ) const
{
    entryList();
    return fList && index >= 0 && index < (int)fList->count() ?
	(*fList)[index] : QString::null;
}


/*!
  This function is included to easy porting from Qt 1.x to Qt 2.0,
  it is the same as entryList(), but encodes the filenames as 8-bit
  strings using QFile::encodedName().

  It is more efficient to use entryList().
*/
QStrList QDir::encodedEntryList( int filterSpec, int sortSpec ) const
{
    QStrList r;
    QStringList l = entryList(filterSpec,sortSpec);
    for ( QStringList::Iterator it = l.begin(); it != l.end(); ++it ) {
	r.append( QFile::encodeName(*it) );
    }
    return r;
}

/*!
  This function is included to easy porting from Qt 1.x to Qt 2.0,
  it is the same as entryList(), but encodes the filenames as 8-bit
  strings using QFile::encodedName().

  It is more efficient to use entryList().
*/
QStrList QDir::encodedEntryList( const QString &nameFilter,
			   int filterSpec,
			   int sortSpec ) const
{
    QStrList r;
    QStringList l = entryList(nameFilter,filterSpec,sortSpec);
    for ( QStringList::Iterator it = l.begin(); it != l.end(); ++it ) {
	r.append( QFile::encodeName(*it) );
    }
    return r;
}



/*!
  Returns a list of the names of all files and directories in the directory
  indicated by the setSorting(), setFilter() and setNameFilter()
  specifications.

  The the filter and sorting specifications can be overridden using the
  \e filterSpec and \e sortSpec arguments.

  Returns an empty list if the directory is unreadable or does not exist.

  \sa entryInfoList(), setNameFilter(), setSorting(), setFilter(),
	encodedEntryList()
*/

QStringList QDir::entryList( int filterSpec, int sortSpec ) const
{
    if ( !dirty && filterSpec == (int)DefaultFilter &&
		   sortSpec   == (int)DefaultSort )
	return *fList;
    return entryList( nameFilt, filterSpec, sortSpec );
}

/*!
  Returns a list of the names of all files and directories in the directory
  indicated by the setSorting(), setFilter() and setNameFilter()
  specifications.

  The the filter and sorting specifications can be overridden using the
  \e nameFilter, \e filterSpec and \e sortSpec arguments.

  Returns and empty list if the directory is unreadable or does not exist.

  \sa entryInfoList(), setNameFilter(), setSorting(), setFilter(),
	encodedEntryList()
*/

QStringList QDir::entryList( const QString &nameFilter,
				 int filterSpec, int sortSpec ) const
{
    if ( filterSpec == (int)DefaultFilter )
	filterSpec = filtS;
    if ( sortSpec == (int)DefaultSort )
	sortSpec = sortS;
    QDir *that = (QDir*)this;			// mutable function
    if ( that->readDirEntries(nameFilter, filterSpec, sortSpec) )
	return *that->fList;
    else
	return QStringList();
}

/*!
  Returns a list of QFileInfo objects for all files and directories in
  the directory pointed to using the setSorting(), setFilter() and
  setNameFilter() specifications.

  The the filter and sorting specifications can be overridden using the
  \e filterSpec and \e sortSpec arguments.

  Returns 0 if the directory is unreadable or does not exist.

  The returned pointer is a const pointer to a QFileInfoList. The list is
  owned by the QDir object and will be reused on the next call to
  entryInfoList() for the same QDir instance. If you want to keep the
  entries of the list after a subsequent call to this function you will
  need to copy them.

  \sa entryList(), setNameFilter(), setSorting(), setFilter()
*/

const QFileInfoList *QDir::entryInfoList( int filterSpec, int sortSpec ) const
{
    if ( !dirty && filterSpec == (int)DefaultFilter &&
		   sortSpec   == (int)DefaultSort )
	return fiList;
    return entryInfoList( nameFilt, filterSpec, sortSpec );
}

/*!
  Returns a list of QFileInfo objects for all files and directories in
  the directory pointed to using the setSorting(), setFilter() and
  setNameFilter() specifications.

  The the filter and sorting specifications can be overridden using the
  \e nameFilter, \e filterSpec and \e sortSpec arguments.

  Returns 0 if the directory is unreadable or does not exist.

  The returned pointer is a const pointer to a QFileInfoList. The list is
  owned by the QDir object and will be reused on the next call to
  entryInfoList() for the same QDir instance. If you want to keep the
  entries of the list after a subsequent call to this function you will
  need to copy them.

  \sa entryList(), setNameFilter(), setSorting(), setFilter()
*/

const QFileInfoList *QDir::entryInfoList( const QString &nameFilter,
					  int filterSpec, int sortSpec ) const
{
    if ( filterSpec == (int)DefaultFilter )
	filterSpec = filtS;
    if ( sortSpec == (int)DefaultSort )
	sortSpec = sortS;
    QDir *that = (QDir*)this;			// mutable function
    if ( that->readDirEntries(nameFilter, filterSpec, sortSpec) )
	return that->fiList;
    else
	return 0;
}

/*!
  Returns TRUE if the directory exists. (If a file with the same
  name is found this function will of course return FALSE).

  \sa QFileInfo::exists(), QFile::exists()
*/

bool QDir::exists() const
{
    QFileInfo fi( dPath );
    return fi.exists() && fi.isDir();
}

/*!
  Returns TRUE if the directory path is relative to the current directory,
  FALSE if the path is absolute (e.g. under UNIX a path is relative if it
  does not start with a '/').

  According to Einstein this function should always return TRUE.

  \sa convertToAbs()
*/

bool QDir::isRelative() const
{
    return isRelativePath( dPath );
}

/*!
  Converts the directory path to an absolute path. If it is already
  absolute nothing is done.

  \sa isRelative()
*/

void QDir::convertToAbs()
{
    dPath = absPath();
}

/*!
  Makes a copy of d and assigns it to this QDir.
*/

QDir &QDir::operator=( const QDir &d )
{
    dPath    = d.dPath;
    delete fList;
    fList    = 0;
    delete fiList;
    fiList   = 0;
    nameFilt = d.nameFilt;
    dirty    = TRUE;
    allDirs  = d.allDirs;
    filtS    = d.filtS;
    sortS    = d.sortS;
    return *this;
}

/*!
  Sets the directory path to be the given path.
*/

QDir &QDir::operator=( const QString &path )
{
    dPath = cleanDirPath( path );
    dirty = TRUE;
    return *this;
}


/*!
  \fn bool QDir::operator!=( const QDir &d ) const
  Returns TRUE if the \e d and this dir have different path or
  different sort/filter settings, otherwise FALSE.
*/

/*!
  Returns TRUE if the \e d and this dir have the same path and all sort
  and filter settings are equal, otherwise FALSE.
*/

bool QDir::operator==( const QDir &d ) const
{
    return dPath    == d.dPath &&
	   nameFilt == d.nameFilt &&
	   allDirs  == d.allDirs &&
	   filtS    == d.filtS &&
	   sortS    == d.sortS;
}


/*!
  Removes a file.

  If \e acceptAbsPath is TRUE a path starting with a separator ('/')
  will remove the file with the absolute path, if \e acceptAbsPath is FALSE
  any number of separators at the beginning of \e fileName will be removed.

  Returns TRUE if successful, otherwise FALSE.
*/

bool QDir::remove( const QString &fileName, bool acceptAbsPath )
{
    if ( fileName.isEmpty() ) {
#if defined(CHECK_NULL)
	qWarning( "QDir::remove: Empty or null file name" );
#endif
	return FALSE;
    }
    QString p = filePath( fileName, acceptAbsPath );
    return QFile::remove( p );
}

/*!
  Checks for existence of a file.

  If \e acceptAbsPaths is TRUE a path starting with a separator ('/')
  will check the file with the absolute path, if \e acceptAbsPath is FALSE
  any number of separators at the beginning of \e name will be removed.

  Returns TRUE if the file exists, otherwise FALSE.

  \sa QFileInfo::exists(), QFile::exists()
*/

bool QDir::exists( const QString &name, bool acceptAbsPath )
{
    if ( name.isEmpty() ) {
#if defined(CHECK_NULL)
	qWarning( "QDir::exists: Empty or null file name" );
#endif
	return FALSE;
    }
    QString tmp = filePath( name, acceptAbsPath );
    return QFile::exists( tmp );
}

/*!
  Returns the native directory separator; '/' under UNIX and '\' under
  MS-DOS, Windows NT and OS/2.

  You do not need to use this function to build file paths. If you always
  use '/', Qt will translate your paths to conform to the underlying
  operating system.
*/

char QDir::separator()
{
#if defined(_OS_UNIX_)
    return '/';
#elif defined (_OS_FATFS_)
    return '\\';
#elif defined (_OS_MAC_)
    return ':';
#else
    return '/';
#endif
}

/*!
  Returns the current directory.
  \sa currentDirPath(), QDir::QDir()
*/

QDir QDir::current()
{
    return QDir( currentDirPath() );
}

/*!
  Returns the home directory.
  \sa homeDirPath()
*/

QDir QDir::home()
{
    return QDir( homeDirPath() );
}

/*!
  Returns the root directory.
  \sa rootDirPath() drives()
*/

QDir QDir::root()
{
    return QDir( rootDirPath() );
}

/*!
  \fn QString QDir::homeDirPath()

  Returns the absolute path for the user's home directory,
  \sa home()
*/

QStringList qt_makeFilterList( const QString &filter )
{
    if ( filter.isEmpty() )
	return QStringList();

    QChar sep( ';' );
    int i = filter.find( sep, 0 );
    if ( i == -1 && filter.find( ' ', 0 ) != -1 )
	sep = QChar( ' ' );

    QStringList lst = QStringList::split( sep, filter );
    QStringList lst2;
    QStringList::Iterator it = lst.begin();

    for ( ; it != lst.end(); ++it ) {
	QString s = *it;
	lst2 << s.stripWhiteSpace();
    }
    return lst2;
}

/*!
  Returns TRUE if the \e fileName matches one of the wildcards in the list \e filters.
  \sa QRegExp
*/

bool QDir::match( const QStringList &filters, const QString &fileName )
{
    QStringList::ConstIterator sit = filters.begin();
    bool matched = FALSE;
    for ( ; sit != filters.end(); ++sit ) {
	QRegExp regexp( *sit, FALSE, TRUE );
	if ( regexp.match( fileName ) != -1 ) {
	    matched = TRUE;
	    break;
	}
    }

    return matched;
}

/*!
  Returns TRUE if the \e fileName matches the wildcard \e filter.
  \a Filter may also contain multiple wildcards separated by spaces or
  semicolons.
  \sa QRegExp
*/

bool QDir::match( const QString &filter, const QString &fileName )
{
    QStringList lst = qt_makeFilterList( filter );
    return match( lst, fileName );
}


/*!
  Removes all multiple directory separators ('/') and resolves
  any "." or ".." found in the path.

  Symbolic links are kept.  This function does not return the
  canonical path, but rather the most simplified version of the input.
  "../stuff" becomes "stuff", "stuff/../nonsense" becomes "nonsense"
  and "\\stuff\\more\\..\\nonsense" becomes "\\stuff\\nonsense".

  \sa absPath() canonicalPath()
*/

QString QDir::cleanDirPath( const QString &filePath )
{
    QString name = filePath;
    QString newPath;

    if ( name.isEmpty() )
	return name;

    slashify( name );

    bool addedSeparator;
    if ( isRelativePath(name) ) {
	addedSeparator = TRUE;
	name.insert( 0, '/' );
    } else {
	addedSeparator = FALSE;
    }

    int ePos, pos, upLevel;

    pos = ePos = name.length();
    upLevel = 0;
    int len;

    while ( pos && (pos = name.findRev('/',--pos)) != -1 ) {
	len = ePos - pos - 1;
	if ( len == 2 && name.at(pos + 1) == '.'
		      && name.at(pos + 2) == '.' ) {
	    upLevel++;
	} else {
	    if ( len != 0 && (len != 1 || name.at(pos + 1) != '.') ) {
		if ( !upLevel )
		    newPath = QString::fromLatin1("/")
			+ name.mid(pos + 1, len) + newPath;
		else
		    upLevel--;
	    }
	}
	ePos = pos;
    }
    if ( addedSeparator ) {
	while ( upLevel-- )
	    newPath.insert( 0, QString::fromLatin1("/..") );
	if ( !newPath.isEmpty() )
	    newPath.remove( 0, 1 );
	else
	    newPath = QString::fromLatin1(".");
    } else {
	if ( newPath.isEmpty() )
	    newPath = QString::fromLatin1("/");
#if defined(_OS_FATFS_) || defined(_OS_OS2EMX_)
	if ( name[0] == '/' ) {
	    if ( name[1] == '/' )		// "\\machine\x\ ..."
		newPath.insert( 0, '/' );
	} else {
	    newPath = name.left(2) + newPath;
	}
#endif
    }
    return newPath;
}

int qt_cmp_si_sortSpec;

#if defined(Q_C_CALLBACKS)
extern "C" {
#endif

int qt_cmp_si( const void *n1, const void *n2 )
{
    if ( !n1 || !n2 )
	return 0;

    QDirSortItem* f1 = (QDirSortItem*)n1;
    QDirSortItem* f2 = (QDirSortItem*)n2;

    if ( qt_cmp_si_sortSpec & QDir::DirsFirst )
	if ( f1->item->isDir() != f2->item->isDir() )
	    return f1->item->isDir() ? -1 : 1;

    int r = 0;
    int sortBy = qt_cmp_si_sortSpec & QDir::SortByMask;

    switch ( sortBy ) {
      case QDir::Time:
	r = f1->item->lastModified().secsTo(f2->item->lastModified());
	break;
      case QDir::Size:
	r = f2->item->size() - f1->item->size();
	break;
      default:
	;
    }

    if ( r == 0 && sortBy != QDir::Unsorted ) {
	// Still not sorted - sort by name
	bool ic = qt_cmp_si_sortSpec & QDir::IgnoreCase;

	if ( f1->filename_cache.isNull() )
	    f1->filename_cache = ic ? f1->item->fileName().lower()
				    : f1->item->fileName();
	if ( f2->filename_cache.isNull() )
	    f2->filename_cache = ic ? f2->item->fileName().lower()
				    : f2->item->fileName();

	r = f1->filename_cache.compare(f2->filename_cache);
    }

    if ( r == 0 ) {
	// Enforce an order - the order the items appear in the array
	r = (char*)n1 - (char*)n2;
    }

    if ( qt_cmp_si_sortSpec & QDir::Reversed )
	return -r;
    else
	return r;
}

#if defined(Q_C_CALLBACKS)
}
#endif

#endif // QT_NO_DIR

// KWQ hacks ---------------------------------------------------------------

#endif // USING_BORROWED_QDIR

// -------------------------------------------------------------------------
