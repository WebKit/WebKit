/****************************************************************************
** $Id$
**
** Implementation of QUrl class
**
** Created : 950429
**
** Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
**
** This file is part of the kernel module of the Qt GUI Toolkit.
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

#include "_qurl.h"
#include <config.h>

#ifdef USING_BORROWED_KURL

#ifndef QT_NO_DIR

#include <stdlib.h>

struct QUrlPrivate
{
    QString protocol;
    QString user;
    QString pass;
    QString host;
    QString path, cleanPath;
    QString refEncoded;
    QString queryEncoded;
    bool isValid;
    int port;
    bool cleanPathDirty;
};

/*!
  Replaces backslashes with slashes and removes multiple occurrences
  of slashes or backslashes if \c allowMultiple is FALSE.
*/

static void slashify( QString& s, bool allowMultiple = TRUE )
{
#ifdef _KWQ_
    fprintf (stderr, "UNSAFE API, should not be called.  This entire class will not be used when KURL is finished.\n");
#else
    bool justHadSlash = FALSE;
    for ( int i = 0; i < (int)s.length(); i++ ) {
	if ( !allowMultiple && justHadSlash &&
	     ( s[ i ] == '/' || s[ i ] == '\\' ) ) {
	    s.remove( i, 1 );
	    --i;
	    continue;
	}
	if ( s[ i ] == '\\' )
	    s[ i ] = '/';
	if ( s[ i ] == '/' )
	    justHadSlash = TRUE;
	else
	    justHadSlash = FALSE;
    }
#endif
}

// NOT REVISED
/*!
  \class QUrl qurl.h

  \brief The QUrl class provides mainly an URL parser and
  simplifies working with URLs.

  \ingroup misc

  The QUrl class is provided for a easy working with URLs.
  It does all parsing, decoding, encoding and so on.

  Mention that URL has some restrictions regarding the path
  encoding. URL works intern with the decoded path and
  and encoded query. For example in

  http://localhost/cgi-bin/test%20me.pl?cmd=Hello%20you

  would result in a decoded path "/cgi-bin/test me.pl"
  and in the encoded query "cmd=Hello%20you".
  Since path is internally always encoded you may NOT use
  "%00" in the path while this is ok for the query.

  QUrl is normally used like that:

  \code
  QUrl u( "http://www.trolltech.com" );
  // or
  QUrl u( "file:/home/myself/Mail", "Inbox" );
  \endcode

  Then you can access the parts of the URL, change them and do
  some more stuff.

  To allow easy working with QUrl and QString together, QUrl implements
  the needed cast and assign operators. So you can do following:

  \code
  QUrl u( "http://www.trolltech.com" );
  QString s = u;
  // or
  QString s( "http://www.trolltech.com" );
  QUrl u( s );
  \endcode

  If you want to use an URL to work on a hierarchical structures
  (e.g. locally or remote filesystem) the class QUrlOperator, which is derived
  fro QUrl, may be interesting for you.

  \sa QUrlOperator
*/


/*!
  Constructs an empty URL which, is invalid.
*/

QUrl::QUrl()
{
    d = new QUrlPrivate;
    d->isValid = FALSE;
    d->port = -1;
    d->cleanPathDirty = TRUE;
}

/*!
  Constructs and URL using \a url and parses this string.

  You can pass strings like "/home/qt", in this case the protocol
  "file" is assumed.
*/

QUrl::QUrl( const QString& url )
{
    d = new QUrlPrivate;
    d->protocol = "file";
    d->port = -1;
    parse( url );
}

/*!
  Copy constructor. Copies the data of \a url.
*/

QUrl::QUrl( const QUrl& url )
{
    d = new QUrlPrivate;
    *d = *url.d;
}

/*!
  Returns TRUE, if \a url is relative, else it returns FALSE.
*/

bool QUrl::isRelativeUrl( const QString &url )
{
    int colon = url.find( ":" );
    int slash = url.find( "/" );

    return ( slash != 0 && ( colon == -1 || ( slash != -1 && colon > slash ) ) );
}

/*!
  Constructs and URL taking \a url as base (context) and
  \a relUrl as relative URL to \a url. If \a relUrl is not relative,
  \a relUrl is taken as new URL.

  For example, the path of

  \code
  QUrl u( "ftp://ftp.trolltech.com/qt/source", "qt-2.1.0.tar.gz" );
  \endcode

  will be "/qt/srource/qt-2.1.0.tar.gz".

  And

  \code
  QUrl u( "ftp://ftp.trolltech.com/qt/source", "/usr/local" );
  \endcode

  will result in a new URL,  "ftp://ftp.trolltech.com/usr/local",

  And

  \code
  QUrl u( "ftp://ftp.trolltech.com/qt/source", "file:/usr/local" );
  \endcode

  will result in a new URL, with "/usr/local" as path
  and "file" as protocol.

  Normally it is expected that the path of \a url points to
  a directory, even if the path has no  slash at the end. But
  if you want that the constructor handles the last
  part of the path as filename, if there is no slash at the end,
  and let it replace by the filename of \a relUrl
  (if it contains one), set \a checkSlash to TRUE.
*/

QUrl::QUrl( const QUrl& url, const QString& relUrl, bool checkSlash )
{
    d = new QUrlPrivate;
    QString rel = relUrl;
    slashify( rel );

    if ( !isRelativeUrl( rel ) ) {
	if ( rel[ 0 ] == QChar( '/' ) ) {
	    *this = url;
	    setEncodedPathAndQuery( rel );
	} else {
	    *this = rel;
	}
    } else {
	if ( rel[ 0 ] == '#' ) {
	    *this = url;
	    rel.remove( 0, 1 );
	    decode( rel );
	    setRef( rel );
	} else if ( rel[ 0 ] == '?' ) {
	    *this = url;
	    rel.remove( 0, 1 );
	    setQuery( rel );
	} else {
	    decode( rel );
	    *this = url;
	    if ( !checkSlash || d->cleanPath[ (int)path().length() - 1 ] == '/' ) {
		QString p = url.path();
		if ( p.isEmpty() )
		    p = "/";
		if ( p.right( 1 ) != "/" )
		    p += "/";
		p += rel;
		d->path = p;
		d->cleanPathDirty = TRUE;
	    } else {
		setFileName( rel );
	    }
	}
    }
}

/*!
  Destructor.
*/

QUrl::~QUrl()
{
    delete d;
    d = 0;
}

/*!
  Returns the protocol of the URL. It is something like
  "file" or "ftp".
*/

QString QUrl::protocol() const
{
    return d->protocol;
}

/*!
  Sets the protocol of the URL. This could be e.g.
  "file", "ftp", or something similar.
*/

void QUrl::setProtocol( const QString& protocol )
{
    d->protocol = protocol;
}

/*!
  Returns the username of the URL.
*/

QString QUrl::user() const
{
    return  d->user;
}

/*!
  Sets the username of the URL.
*/

void QUrl::setUser( const QString& user )
{
    d->user = user;
}

/*!
  Returns TRUE, if the URL contains an username,
  else FALSE;
*/

bool QUrl::hasUser() const
{
    return !d->user.isEmpty();
}

/*!
  Returns the password of the URL.
*/

QString QUrl::password() const
{
    return d->pass;
}

/*!
  Sets the password of the URL.
*/

void QUrl::setPassword( const QString& pass )
{
    d->pass = pass;
}

/*!
  Returns TRUE, if the URL contains a password,
  else FALSE;
*/

bool QUrl::hasPassword() const
{
    return !d->pass.isEmpty();
}

/*!
  Returns the hostname of the URL.
*/

QString QUrl::host() const
{
    return d->host;
}

/*!
  Sets the hostname of the URL.
*/

void QUrl::setHost( const QString& host )
{
    d->host = host;
}

/*!
  Returns TRUE, if the URL contains a hostname,
  else FALSE;
*/

bool QUrl::hasHost() const
{
    return !d->host.isEmpty();
}

/*!
  Returns the port of the URL.
*/

int QUrl::port() const
{
    return d->port;
}

/*!
  Sets the port of the URL.
*/

void QUrl::setPort( int port )
{
    d->port = port;
}

/*!
  Sets the path or the URL.
*/

void QUrl::setPath( const QString& path )
{
    d->path = path;
    slashify( d->path );
    d->cleanPathDirty = TRUE;
}

/*!
  Returns TRUE, if the URL contains a path,
  else FALSE.
*/

bool QUrl::hasPath() const
{
    return !d->path.isEmpty();
}

/*!
  Sets the query of the URL. Must be encoded.
*/

void QUrl::setQuery( const QString& txt )
{
    d->queryEncoded = txt;
}

/*!
  Returns the query (encoded) of the URL.
*/

QString QUrl::query() const
{
    return d->queryEncoded;
}

/*!
  Returns the reference (encoded) of the URL.
*/

QString QUrl::ref() const
{
    return d->refEncoded;
}

/*!
  Sets the reference of the URL. Must be encoded.
*/

void QUrl::setRef( const QString& txt )
{
    d->refEncoded = txt;
}

/*!
  Returns TRUE, if the URL has a reference, else it returns FALSE.
*/

bool QUrl::hasRef() const
{
    return !d->refEncoded.isEmpty();
}

/*!
  Returns TRUE if the URL is valid, else FALSE.
  An URL is e.g. invalid if there was a parse error.
*/

bool QUrl::isValid() const
{
    return d->isValid;
}

/*!
  Resets all values if the URL to its default values
  and invalidates it.
*/

void QUrl::reset()
{
    d->protocol = "file";
    d->user = "";
    d->pass = "";
    d->host = "";
    d->path = "";
    d->queryEncoded = "";
    d->refEncoded = "";
    d->isValid = TRUE;
    d->port = -1;
    d->cleanPathDirty = TRUE;
}

/*!
  Parses the \a url.
*/

bool QUrl::parse( const QString& url )
{
    QString url_( url );
    slashify( url_ );

    if ( url_.isEmpty() ) {
	d->isValid = FALSE;
	return FALSE;
    }

    d->cleanPathDirty = TRUE;
    d->isValid = TRUE;
    QString oldProtocol = d->protocol;
    d->protocol = QString::null;

    const int Init 	= 0;
    const int Protocol 	= 1;
    const int Separator1= 2; // :
    const int Separator2= 3; // :/
    const int Separator3= 4; // :// or more slashes
    const int User 	= 5;
    const int Pass 	= 6;
    const int Host 	= 7;
    const int Path 	= 8;
    const int Ref 	= 9;
    const int Query 	= 10;
    const int Port 	= 11;
    const int Done 	= 12;

    const int InputAlpha= 1;
    const int InputDigit= 2;
    const int InputSlash= 3;
    const int InputColon= 4;
    const int InputAt 	= 5;
    const int InputHash = 6;
    const int InputQuery= 7;

    static uchar table[ 12 ][ 8 ] = {
     /* None       InputAlpha  InputDigit  InputSlash  InputColon  InputAt     InputHash   InputQuery */
	{ 0,       Protocol,   0,          Path,       0,          0,          0,          0,         }, // Init
	{ 0,       Protocol,   Protocol,   0,          Separator1, 0,          0,          0,         }, // Protocol
	{ 0,       Path,       Path,       Separator2, 0,          0,          0,          0,         }, // Separator1
	{ 0,       Path,       Path,       Separator3, 0,          0,          0,          0,         }, // Separator2
	{ 0,       User,       User,       Separator3, Pass,       Host,       0,          0,         }, // Separator3
	{ 0,       User,       User,       User,       Pass,       Host,       User,       User,      }, // User
	{ 0,       Pass,       Pass,       Pass,       Pass,       Host,       Pass,       Pass,      }, // Pass
	{ 0,       Host,       Host,       Path,       Port,       Host,       Ref,        Query,     }, // Host
	{ 0,       Path,       Path,       Path,       Path,       Path,       Ref,        Query,     }, // Path
	{ 0,       Ref,        Ref,        Ref,        Ref,        Ref,        Ref,        Query,     }, // Ref
	{ 0,       Query,      Query,      Query,      Query,      Query,      Query,      Query,     }, // Query
	{ 0,       0,          Port,       Path,       0,          0,          0,          0,         }  // Port
    };

    bool relPath = FALSE;

    relPath = FALSE;
    bool forceRel = FALSE;

    // if :/ is at pos 1, we have only one letter
    // before that separator => that's a drive letter!
    int cs = url_.find( ":/" );
    if ( cs == 1 )
	relPath = forceRel = TRUE;

    int hasNoHost = -1;
    if ( cs != -1 ) // if a protocol is there, find out if there is a host or directly the path after it
	hasNoHost = url_.find( "///", cs );
    table[ 4 ][ 1 ] = User;
    table[ 4 ][ 2 ] = User;
    if ( cs == -1 || forceRel ) { // we have a relative file
	if ( url.find( ':' ) == -1 || forceRel ) {
	    table[ 0 ][ 1 ] = Path;
	    // Filenames may also begin with a digit
	    table[ 0 ][ 2 ] = Path;
	} else {
	    table[ 0 ][ 1 ] = Protocol;
	}
	relPath = TRUE;
    } else { // some checking
	table[ 0 ][ 1 ] = Protocol;

	// find the part between the protocol and the path as the meaning
	// of that part is dependend on some chars
	++cs;
	while ( url_[ cs ] == '/' )
	    ++cs;
	int slash = url_.find( "/", cs );
	if ( slash == -1 )
	    slash = url_.length() - 1;
	QString tmp = url_.mid( cs, slash - cs + 1 );

	if ( !tmp.isEmpty() ) { // if this part exists

	    // look for the @ in this part
	    int at = tmp.find( "@" );
	    if ( at != -1 )
		at += cs;
	    // we have no @, which means host[:port], so directly
	    // after the protocol the host starts, or if the protocol
	    // is file or there were more than 2 slashes, it´s the
	    // path
	    if ( at == -1 ) {
		if ( url_.left( 4 ) == "file" || hasNoHost != -1 )
		    table[ 4 ][ 1 ] = Path;
		else
		    table[ 4 ][ 1 ] = Host;
		table[ 4 ][ 2 ] = table[ 4 ][ 1 ];
	    }
	}
    }

    int state = Init; // parse state
    int input; // input token

    QChar c = url_[ 0 ];
    int i = 0;
    QString port;

    while ( TRUE ) {
	switch ( c ) {
	case '?':
	    input = InputQuery;
	    break;
	case '#':
	    input = InputHash;
	    break;
	case '@':
	    input = InputAt;
	    break;
	case ':':
	    input = InputColon;
	    break;
	case '/':
	    input = InputSlash;
	    break;
	case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9': case '0':
	    input = InputDigit;
	    break;
	default:
	    input = InputAlpha;
	}

    	state = table[ state ][ input ];

	switch ( state ) {
	case Protocol:
	    d->protocol += c;
	    break;
	case User:
	    d->user += c;
	    break;
	case Pass:
	    d->pass += c;
	    break;
	case Host:
	    d->host += c;
	    break;
	case Path:
	    d->path += c;
	    break;
	case Ref:
	    d->refEncoded += c;
	    break;
	case Query:
	    d->queryEncoded += c;
	    break;
	case Port:
	    port += c;
	    break;
	default:
	    break;
	}

	++i;
	if ( i > (int)url_.length() - 1 || state == Done || state == 0 )
	    break;
	c = url_[ i ];

    }

    if ( !port.isEmpty() ) {
	port.remove( 0, 1 );
	d->port = atoi( port.latin1() );
    }

    // error
    if ( i < (int)url_.length() - 1 ) {
	d->isValid = FALSE;
	return FALSE;
    }


    if ( d->protocol.isEmpty() )
	d->protocol = oldProtocol;

    if ( d->path.isEmpty() )
	d->path = "/";

    // hack for windows
    if ( d->path.length() == 2 && d->path[ 1 ] == ':' )
	d->path += "/";

    // #### do some corrections, should be done nicer too
    if ( !d->pass.isEmpty() && d->pass[ 0 ] == ':' )
	d->pass.remove( 0, 1 );
    if ( !d->path.isEmpty() ) {
	if ( d->path[ 0 ] == '@' || d->path[ 0 ] == ':' )
	    d->path.remove( 0, 1 );
	if ( d->path[ 0 ] != '/' && !relPath && d->path[ 1 ] != ':' )
	    d->path.prepend( "/" );
    }
    if ( !d->refEncoded.isEmpty() && d->refEncoded[ 0 ] == '#' )
	d->refEncoded.remove( 0, 1 );
    if ( !d->queryEncoded.isEmpty() && d->queryEncoded[ 0 ] == '?' )
	d->queryEncoded.remove( 0, 1 );
    if ( !d->host.isEmpty() && d->host[ 0 ] == '@' )
	d->host.remove( 0, 1 );

#if defined(_OS_WIN32_)
    // hack for windows file://machine/path syntax
    if ( d->protocol == "file" ) {
	if ( url.left( 7 ) == "file://" &&
	     ( d->path.length() < 8 || d->path[ 7 ] != '/' ) &&
	     d->path[ 1 ] != '/' )
	    d->path.prepend( "/" );
    }
#endif

    decode( d->path );
    d->cleanPathDirty = TRUE;

#if 0
    qDebug( "URL: %s", url.latin1() );
    qDebug( "protocol: %s", d->protocol.latin1() );
    qDebug( "user: %s", d->user.latin1() );
    qDebug( "pass: %s", d->pass.latin1() );
    qDebug( "host: %s", d->host.latin1() );
    qDebug( "path: %s", path().latin1() );
    qDebug( "ref: %s", d->refEncoded.latin1() );
    qDebug( "query: %s", d->queryEncoded.latin1() );
    qDebug( "port: %d\n\n----------------------------\n\n", d->port );
#endif

    return TRUE;
}

/*!
  Assign operator. Parses \a url and assigns the resulting
  data to this class.

  You can pass strings like "/home/qt", in this case the protocol
  "file" is assumed.
*/

QUrl& QUrl::operator=( const QString& url )
{
    reset();
    parse( url );

    return *this;
}

/*!
  Assign operator. Assigns the data of \a url to this class.
*/

QUrl& QUrl::operator=( const QUrl& url )
{
    *d = *url.d;
    return *this;
}

/*!
  Compares this URL with \a url and returns TRUE if
  they are equal, else FALSE.
*/

bool QUrl::operator==( const QUrl& url ) const
{
    if ( !isValid() || !url.isValid() )
	return FALSE;

    if ( d->protocol == url.d->protocol &&
	 d->user == url.d->user &&
	 d->pass == url.d->pass &&
	 d->host == url.d->host &&
	 d->path == url.d->path &&
	 d->queryEncoded == url.d->queryEncoded &&
	 d->refEncoded == url.d->refEncoded &&
	 d->isValid == url.d->isValid &&
	 d->port == url.d->port )
	return TRUE;

    return FALSE;
}

/*!
  Compares this URL with \a url. \a url is parsed
  first. Returns TRUE if  \a url is equal to this url,
  else FALSE:
*/

bool QUrl::operator==( const QString& url ) const
{
    QUrl u( url );
    return ( *this == u );
}

/*!
  Sets the filename of the URL to \a name. If this
  url contains a fileName(), this is replaced by
  \a name. See the documentation of fileName()
  for a more detail discussion, about what is handled
  as file name and what as directory path.
*/

void QUrl::setFileName( const QString& name )
{
    QString fn( name );
    slashify( fn );

    while ( fn[ 0 ] == '/' )
	fn.remove( 0, 1 );

    QString p = path().isEmpty() ?
		QString( "/" ) : path();
    if ( !path().isEmpty() ) {
	int slash = p.findRev( QChar( '/' ) );
	if ( slash == -1 ) {
	    p = "/";
    } else if ( p[ (int)p.length() - 1 ] != '/' )
	p.truncate( slash + 1 );
    }

    p += fn;
    if ( !d->queryEncoded.isEmpty() )
	p += "?" + d->queryEncoded;
    setEncodedPathAndQuery( p );
}

/*!
  Returns the encoded path plus the query (encoded too).
*/

QString QUrl::encodedPathAndQuery()
{
    QString p = path();
    if ( p.isEmpty() )
	p = "/";

    encode( p );

    if ( !d->queryEncoded.isEmpty() ) {
	p += "?";
	p += d->queryEncoded;
    }

    return p;
}

/*!
  Sets path and query. Both have to be encoded.
*/

void QUrl::setEncodedPathAndQuery( const QString& path )
{
    d->cleanPathDirty = TRUE;
    int pos = path.find( '?' );
    if ( pos == -1 ) {
	d->path = path;
	d->queryEncoded = "";
    } else {
	d->path = path.left( pos );
	d->queryEncoded = path.mid( pos + 1 );
    }

    decode( d->path );
    d->cleanPathDirty = TRUE;
}

/*!
  Returns the path of the URL. If \a correct is TRUE,
  the path is cleaned (deals with too many or few
  slashes, cleans things like "/../..", etc). Else exactly the path
  which was parsed or set is returned.
*/

QString QUrl::path( bool correct ) const
{
    if ( !correct )
	return d->path;

    if ( d->cleanPathDirty ) {
	bool check = TRUE;
	if ( QDir::isRelativePath( d->path ) ) {
	    d->cleanPath = d->path;
	} else if ( isLocalFile() ) {
#if defined(_OS_WIN32_)
	    // hack for stuff like \\machine\path and //machine/path on windows
	    if ( ( d->path.left( 1 ) == "/" || d->path.left( 1 ) == "\\" ) &&
		 d->path.length() > 1 ) {
		d->cleanPath = d->path;
		bool share = d->cleanPath[ 1 ] == '\\' || d->cleanPath[ 1 ] == '/';
 		slashify( d->cleanPath, FALSE );
		d->cleanPath = QDir::cleanDirPath( d->cleanPath );
		if ( share ) {
		    check = FALSE;
		    d->cleanPath.prepend( "/" );
		}
	    }
#endif
	    if ( check ) {
		QFileInfo fi( d->path );
		if ( !fi.exists() )
		    d->cleanPath = d->path;
		else if ( fi.isDir() ) {
		    QString dir = QDir::cleanDirPath( QDir( d->path ).canonicalPath() ) + "/";
		    if ( dir == "//" )
			d->cleanPath = "/";
		    else
			d->cleanPath = dir;
		} else {
		    QString p = QDir::cleanDirPath( fi.dir().canonicalPath() );
		    d->cleanPath = p + "/" + fi.fileName();
		}
	    }
	} else {
	    if ( d->path != "/" && d->path[ (int)d->path.length() - 1 ] == '/' )
		d->cleanPath = QDir::cleanDirPath( d->path ) + "/";
	    else
		d->cleanPath = QDir::cleanDirPath( d->path );
	}

	if ( check )
	    slashify( d->cleanPath, FALSE );
	d->cleanPathDirty = FALSE;
    }

    return d->cleanPath;
}

/*!
  Returns TRUE, if the URL is a local file, else
  it returns FALSE;
*/

bool QUrl::isLocalFile() const
{
    return d->protocol == "file";
}

/*!
  Returns the filename of the URL. If the path of the URL
  doesn't have a slash at the end, the part between the last slash
  and the end of the path string is handled as filename. If the
  path has a  slash at the end, an empty string is returned here.
*/

QString QUrl::fileName() const
{
    if ( d->path.isEmpty() )
	return QString::null;

    return QFileInfo( d->path ).fileName();
}

/*!
  Adds the path \a pa to the path of the URL.
*/

void QUrl::addPath( const QString& pa )
{
    if ( pa.isEmpty() )
	return;

    QString p( pa );
    slashify( p );

    if ( path().isEmpty() ) {
	if ( p[ 0 ] != QChar( '/' ) )
	    d->path = "/" + p;
	else
	    d->path = p;
    } else {
	if ( p[ 0 ] != QChar( '/' ) && d->path[ (int)d->path.length() - 1 ] != '/' )
	    d->path += "/" + p;
	else
	    d->path += p;
    }
    d->cleanPathDirty = TRUE;
}

/*!
  Returns the directory path of the URL. This is the part
  of the path of this URL without the fileName(). See
  the documentation of fileName() for a discussion
  what is handled as file name and what as directory path.
*/

QString QUrl::dirPath() const
{
    if ( path().isEmpty() )
	return QString::null;
    
    QString s = path();
    // ### If anything is broken with regards to the directories in QFileDialog
    // et al, then this is probably the place to look
//    if ( s.right( 1 ) != "/" )
//	s += "/";
    s = QFileInfo( s ).dirPath();

//    s = QFileInfo( s ).dirPath( TRUE );
//    if ( s[ (int)s.length() - 1 ] != '/' )
//	s += "/";
    return s;
}

/*!
  Encodes the string \a url.
*/

void QUrl::encode( QString& url )
{
#ifdef _KWQ_
    fprintf (stderr, "UNSAFE API, should not be called.  This entire class will not be used when KURL is finished.\n");
#else
    int oldlen = url.length();

    if ( !oldlen )
	return;

    QString newUrl;
    int newlen = 0;

    for ( int i = 0; i < oldlen ;++i ) {
	ushort inCh = url[ i ].unicode();

	if ( inCh >= 128 ||
	     QString( "<>#@\"&%$:,;?={}|^~[]\'`\\ \n\t\r" ).contains(inCh) ) {
	    newUrl[ newlen++ ] = QChar( '%' );

	    ushort c = inCh / 16;
	    c += c > 9 ? 'A' - 10 : '0';
	    newUrl[ newlen++ ] = c;

	    c = inCh % 16;
	    c += c > 9 ? 'A' - 10 : '0';
	    newUrl[ newlen++ ] = c;
	} else {
	    newUrl[ newlen++ ] = url[ i ];
	}
    }

    url = newUrl;
#endif
}

#ifndef _KWQ_
static ushort hex_to_int( ushort c )
{
    if ( c >= 'A' && c <= 'F')
	return c - 'A' + 10;
    if ( c >= 'a' && c <= 'f')
	return c - 'a' + 10;
    if ( c >= '0' && c <= '9')
	return c - '0';
    return 0;
}
#endif


/*!
  Decodes the string \a url.
*/

void QUrl::decode( QString& url )
{
#ifdef _KWQ_
    fprintf (stderr, "UNSAFE API, should not be called.  This entire class will not be used when KURL is finished.\n");
#else
    int oldlen = url.length();
    if ( !oldlen )
	return;

    int newlen = 0;

    QString newUrl;

    int i = 0;
    while ( i < oldlen ) {
	ushort c = url[ i++ ].unicode();
	if ( c == '%' ) {
	    c = hex_to_int( url[ i ].unicode() ) * 16 + hex_to_int( url[ i + 1 ].unicode() );
	    i += 2;
	}
	newUrl [ newlen++ ] = c;
    }

    url = newUrl;
#endif
}

/*!  Composes a string of the URL and returns it. If \a encodedPath is
  TRUE, the path in the returned string will be encoded. If \a
  forcePrependProtocol is TRUE, the protocol (file:/) is also
  prepended to local filenames, else no protocol is prepended for
  local filenames.
*/

QString QUrl::toString( bool encodedPath, bool forcePrependProtocol ) const
{
    QString res, p = path();
    if ( encodedPath )
	encode( p );

    if ( isLocalFile() ) {
	if ( forcePrependProtocol )
	    res = d->protocol + ":" + p;
	else
	    res = p;
    } else if ( d->protocol == "mailto" ) {
	res = d->protocol + ":" + p;
    } else {
	res = d->protocol + "://";
	if ( !d->user.isEmpty() || !d->pass.isEmpty() ) {
	    if ( !d->user.isEmpty() )
		res += d->user;
	    if ( !d->pass.isEmpty() )
		res += ":" + d->pass;
	    res += "@";
	}
	res += d->host;
	if ( d->port != -1 )
	    res += ":" + QString( "%1" ).arg( d->port );
	res += p;
    }

    if ( !d->refEncoded.isEmpty() )
	res += "#" + d->refEncoded;
    if ( !d->queryEncoded.isEmpty() )
	res += "?" + d->queryEncoded;

    return res;
}

/*!
  Composes a string of the URL and returns it.

  \sa QUrl::toString()
*/

QUrl::operator QString() const
{
    return toString();
}

/*!
  Goes one directory up.
*/

bool QUrl::cdUp()
{
    d->path += "/..";
    d->cleanPathDirty = TRUE;
    return TRUE;
}

#endif // QT_NO_NETWORKPROTOCOL

#endif USING_BORROWED_KURL

