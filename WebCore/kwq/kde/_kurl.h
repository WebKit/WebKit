/* This file is part of the KDE libraries
 *  Copyright (C) 1999 Torben Weis <weis@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 **/

#ifndef __kurl_h__
#define __kurl_h__ "$Id$"

#include <qstring.h>
#include <qvaluelist.h>

class QUrl;
class QStringList;

struct KURLPrivate;

/**
 * Represent and parse a URL.
 *
 * A prototypical URL looks like:
 * <pre>
 *   protocol:/user:password@hostname:port/path/to/file.ext#reference
 * </pre>
 *
 *  @ref KURL has some restrictions regarding the path
 * encoding. @ref KURL works internally with the decoded path and
 * and encoded query. For example,
 * <pre>
 * http://localhost/cgi-bin/test%20me.pl?cmd=Hello%20you
 * </pre>
 * would result in a decoded path "/cgi-bin/test me.pl"
 * and in the encoded query "?cmd=Hello%20you".
 * Since path is internally always encoded you may @em not use
 * "%00" in the path, although this is OK for the query.
 *
 *  @author  Torben Weis <weis@kde.org>
 */
class KURL
{
public:
  class List : public QValueList<KURL>
  {
  public:
      List() { }
      List(const QStringList &);
      QStringList toStringList() const;
  };
  /**
   * Construct an empty URL.
   */
  KURL();

  /**
   * destruktor
   */
  ~KURL();

  /**
   * Usual constructor, to construct from a string.
   * @param url A URL, not a filename. If the URL does not have a protocol
   *             part, "file:" is assumed.
   *             It is dangerous to feed unix filenames into this function,
   *             this will work most of the time but not always.
   *             For example "/home/Torben%20Weis" will be considered a URL
   *             pointing to the file "/home/Torben Weis" instead of to the
   *             file "/home/Torben%20Weis".
   *             This means that if you have a usual UNIX like path you
   *             should not use this constructor.
   *             Instead create an empty url and set the path by using
   *             @ref setPath().
   * @param encoding_hint Reserved, should be 0.
   */
  KURL( const QString& url, int encoding_hint = 0 );
  /**
   * Constructor taking a char * @p url, which is an _encoded_ representation
   * of the URL, exactly like the usual constructor. This is useful when
   * then URL, in its encoded form, is strictly ascii.
   */
  KURL( const char * url, int encoding_hint = 0 );
  /**
   * Copy constructor
   */
  KURL( const KURL& u );
  /**
   * Convert from a @ref QUrl.
   */
  KURL( const QUrl &u );
  /**
   * Constructor allowing relative URLs.
   *
   * @param _baseurl The base url.
   * @param _rel_url A relative or absolute URL.
   * If this is an absolute URL then @p _baseurl will be ignored.
   * If this is a relative URL it will be combined with @p _baseurl.
   * Note that _rel_url should be encoded too, in any case.
   * So do NOT pass a path here (use setPath or addPath instead).
   * @param encoding_hint Reserved, should be 0.
   */
  KURL( const KURL& _baseurl, const QString& _rel_url, int encoding_hint=0 );

  /**
   * Retrieve the protocol for the URL (i.e., file, http, etc.).
   **/
  QString protocol() const { return m_bIsMalformed ? QString::null : m_strProtocol; }
  /**
   * Set the protocol for the URL (i.e., file, http, etc.)
   **/
  void setProtocol( const QString& _txt );

  /**
   * Retrieve the decoded user name (login, user id, ...) included in the URL.
   **/
  QString user() const { return m_strUser; }
  /**
   * Set the user name (login, user id, ...) included the URL.
   *
   * Special characters in the user name will appear encoded in the URL.
   **/
  void setUser( const QString& _txt );
  /**
   * Test to see if this URL has a user name included in it.
   **/
  bool hasUser() const { return !m_strUser.isEmpty(); }

  /**
   * Retrieve the decoded password (corresponding to @ref user()) included in the URL.
   **/
  QString pass() const { return m_strPass; }
  /**
   * Set the password (corresponding to @ref user()) included in the URL.
   *
   * Special characters in the password will appear encoded in the URL.
   **/
  void setPass( const QString& _txt );
  /**
   * Test to see if this URL has a password included in it.
   **/
  bool hasPass() const { return !m_strPass.isEmpty(); }

  /**
   * Retrieve the decoded hostname included in the URL.
   **/
  QString host() const { return m_strHost; }
  /**
   * Set the hostname included in the URL.
   *
   * Special characters in the hostname will appear encoded in the URL.
   **/
  void setHost( const QString& _txt );
  /**
   * Test to see if this URL has a hostname included in it.
   **/
  bool hasHost() const { return !m_strHost.isEmpty(); }

  /**
   * Retrieve the port number included in the URL.
   * If there is no port number specified in the URL, returns 0.
   **/
  unsigned short int port() const { return m_iPort; }
  /**
   * Set the port number included in the URL.
   **/
  void setPort( unsigned short int _p );

  /**
   * @return The current decoded path. This does @em not include the query.
   *
   */
  QString path() const  { return m_strPath; }

  /**
   * @param _trailing May be ( -1, 0 +1 ). -1 strips a trailing '/', +1 adds
   *                  a trailing '/' if there is none yet and 0 returns the
   *                  path unchanged. If the URL has no path, then no '/' is added
   *                  anyway. And on the other side: If the path is "/", then this
   *                  character won't be stripped. Reason: "ftp://weis@host" means something
   *                  completely different than "ftp://weis@host/". So adding or stripping
   *                  the '/' would really alter the URL, while "ftp://host/path" and
   *                  "ftp://host/path/" mean the same directory.
   *
   * @return The current decoded path. This does not include the query.
   */
  QString path( int _trailing ) const;

  /**
   * path This is considered to be decoded. This means: %3f does not become decoded
   *      and the ? does not indicate the start of the query part.
   *      The query is not changed by this function.
   */
  void setPath( const QString& path );
  /**
   * Test to see if this URL has a path is included in it.
   **/
  bool hasPath() const { return !m_strPath.isEmpty(); }

  /** Removes all multiple directory separators ('/') and
   * resolves any "." or ".." found in the path.
   * Calls @ref QDir::cleanDirPath but saves the trailing slash if any.
   */
  void cleanPath();
   
  /**
   * Same as above except it takes a flag that allows you
   * to ignore the clean up of the multiple directory separators.
   * 
   * Some servers seem not to like the removal of extra '/' 
   * eventhough it is against the specification in RFC 2396.
   */   
  void cleanPath(bool cleanDirSeparator);

  /**
   * This is useful for HTTP. It looks first for '?' and decodes then.
   * The encoded path is the concatenation of the current path and the query.
   * @param encoding_hint Reserved, should be 0.
   */
  void setEncodedPathAndQuery( const QString& _txt, int encoding_hint = 0 );

  /**
   * @return The concatenation if the encoded path , '?' and the encoded query.
   *
   * @param _no_empty_path If set to true then an empty path is substituted by "/".
   * @param encoding_hint Reserved, should be 0.
   */
  QString encodedPathAndQuery( int _trailing = 0, bool _no_empty_path = false, int encoding_hint = 0) const;

  /**
   * @param _txt This is considered to be encoded. This has a good reason:
   * The query may contain the 0 character.
   *
   * The query should start with a '?'. If it doesn't '?' is prepended.
   * @param encoding_hint Reserved, should be 0.
   */
  void setQuery( const QString& _txt, int encoding_hint = 0);

  /**
   * @return The encoded query.
   * This has a good reason: The query may contain the 0 character.
   * If a query is present it always starts with a '?'.
   * A single '?' means an empty query.
   * An empty string means no query.
   */
  QString query() const { return m_strQuery_encoded; }

  /**
   * The reference is @em never decoded automatically.
   */
  QString ref() const { return m_strRef_encoded; }

  /**
   * Set the reference part (everything after '#').
   * @param _txt is considered encoded.
   */
  void setRef( const QString& _txt ) { m_strRef_encoded = _txt; }

  /**
   * @return @p true if the reference part of the URL is not empty. In a URL like
   *         http://www.kde.org/kdebase.tar#tar:/README it would return @p true, too.
   */
  bool hasRef() const { return !m_strRef_encoded.isEmpty(); }

  /**
   * @return The HTML-style reference.
   */
  QString htmlRef() const;

  /**
   * @return The HTML-style reference in its original form.
   */
  QString encodedHtmlRef() const;

  /**
   * Set the HTML-style reference.
   *
   * @param _ref This is considered to be @em not encoded in contrast to @ref setRef()
   *
   * @see htmlRef()
   */
  void setHTMLRef( const QString& _ref );

  /**
   * @return @p true if the URL has an HTML-style reference.
   *
   * @see htmlRef()
   */
  bool hasHTMLRef() const;

  /**
   * @return @p false if the URL is malformed. This function does @em not test
   *         whether sub URLs are well-formed, too.
   */
  bool isValid() const  { return !m_bIsMalformed; }
  /**
   * @deprecated
   */
  bool isMalformed() const { return !isValid(); }

  /**
   * @return @p true if the file is a plain local file and has no filter protocols
   *         attached to it.
   */
  bool isLocalFile() const;

  /**
   * @return @p true if the file has at least one sub URL.
   *         Use @ref split() to get the sub URLs.
   */
  bool hasSubURL() const;

  /**
   * Add to the current path.
   * Assumes that the current path is a directory. @p _txt is appended to the
   * current path. The function adds '/' if needed while concatenating.
   * This means it does not matter whether the current path has a trailing
   * '/' or not. If there is none, it becomes appended. If @p _txt
   * has a leading '/' then this one is stripped.
   *
   * @param _txt This is considered to be decoded
   */
  void addPath( const QString& _txt );

  /**
   * In comparison to @ref addPath() this function does not assume that the current path
   * is a directory. This is only assumed if the current path ends with '/'.
   *
   * Any reference is reset.
   *
   * @param _txt This is considered to be decoded. If the current path ends with '/'
   *             then @p _txt ist just appended, otherwise all text behind the last '/'
   *             in the current path is erased and @p _txt is appended then. It does
   *             not matter whether @p _txt starts with '/' or not.
   */
  void setFileName( const QString&_txt );

  /**
   * @return The filename of the current path. The returned string is decoded.
   *
   * @param _ignore_trailing_slash_in_path This tells whether a trailing '/' should be ignored.
   *                                     This means that the function would return "torben" for
   *                                     <tt>file:/hallo/torben/</tt> and <tt>file:/hallo/torben</tt>.
   *                                     If the flag is set to false, then everything behind the last '/'
   *                                     is considered to be the filename.
   */
  QString fileName( bool _ignore_trailing_slash_in_path = true ) const;
  QString filename( bool _ignore_trailing_slash_in_path = true ) const
  {
    return fileName(_ignore_trailing_slash_in_path);
  }

  /**
   * @return The directory part of the current path. Everything between the last and the second last '/'
   *         is returned. For example <tt>file:/hallo/torben/</tt> would return "/hallo/torben/" while
   *         <tt>file:/hallo/torben</tt> would return "hallo/". The returned string is decoded.
   *
   * @param _strip_trailing_slash_from_result tells whether the returned result should end with '/' or not.
   *                                          If the path is empty or just "/" then this flag has no effect.
   * @param _ignore_trailing_slash_in_path means that <tt>file:/hallo/torben</tt> and
   *                                       <tt>file:/hallo/torben/"</tt> would both return <tt>/hallo/</tt>
   *                                       or <tt>/hallo</tt> depending on the other flag
   */
  QString directory( bool _strip_trailing_slash_from_result = true,
		     bool _ignore_trailing_slash_in_path = true ) const;

  /**
   * Change directory by descending into the given directory.
   * It is assumed the current URL represents a directory.
   * If @p dir starts with a "/" the
   * current URL will be "protocol://host/dir" otherwise @p _dir will
   * be appended to the path. @p _dir can be ".."
   * This function won't strip protocols. That means that when you are in
   * file:/dir/dir2/my.tgz#tar:/ and you do cd("..") you will
   * still be in file:/dir/dir2/my.tgz#tar:/
   *
   * @return true
   */
  bool cd( const QString& _dir );

  /**
   * @return The complete URL, with all escape sequences intact.
   * Example: http://localhost:8080/test.cgi?test=hello%20world&name=fred
   *
   * @param _trailing This may be ( -1, 0 +1 ). -1 strips a trailing '/' from the path, +1 adds
   *                  a trailing '/' if there is none yet and 0 returns the
   *                  path unchanged.
   */
  QString url( int _trailing = 0 ) const;

  /**
   * @return The complete URL, with all escape sequences intact, encoded
   * in a given charset.
   * This is used in particular for encoding URLs in UTF-8 before using them
   * in a drag and drop operation.
   *
   * @param _trailing This may be ( -1, 0 +1 ). -1 strips a trailing '/' from the path, +1 adds
   *                  a trailing '/' if there is none yet and 0 returns the
   *                  path unchanged.
   * @param encoding_hint The charset to use for encoding (see QFont::Charset).
   */
  QString url( int _trailing, int encoding_hint ) const;

  /**
   * @return A human readable URL, with no non-necessary encodings/escaped
   * characters.
   * Example: http://localhost:8080/test.cgi?test=hello world&name=fred
   */
  QString prettyURL( int _trailing = 0) const;

  /**
   * Test to see if the @ref KURL is empty.
   **/
  bool isEmpty() const;

  /**
   * This function is useful to implement the "Up" button in a file manager for example.
   * @ref cd() never strips a sub-protocol. That means that if you are in
   * file:/home/x.tgz#gzip:/#tar:/ and hit the up button you expect to see
   * file:/home. The algorithm tries to go up on the right-most URL. If that is not
   * possible it strips the right most URL. It continues stripping URLs.
   */
  KURL upURL( ) const;

  KURL& operator=( const KURL& _u );
  KURL& operator=( const QString& _url );
  KURL& operator=( const char * _url );
  KURL& operator=( const QUrl & u );

  bool operator==( const KURL& _u ) const;
  bool operator==( const QString& _u ) const;
  bool operator!=( const KURL& _u ) const { return !( *this == _u ); }
  bool operator!=( const QString& _u ) const { return !( *this == _u ); }

  /**
   * Compare this url with @p u
   * @param ignore_trailing set to true to ignore trailing '/' characters.
   * @return true if both urls are the same
   * @see operator==. This function should be used if you want to
   * ignore trailing '/' characters.
   */
  bool cmp( const KURL &u, bool ignore_trailing = false ) const;

  /**
   * @return true if this url is a parent of @p u (or the same URL as @p u)
   * For instance, ftp://host/dir/ is a parent of ftp://host/dir/subdir/subsubdir/.
   */
  bool isParentOf( const KURL& u ) const;

  /**
   * Splits nested URLs like file:/home/weis/kde.tgz#gzip:/#tar:/kdebase
   * A URL like http://www.kde.org#tar:/kde/README.hml#ref1 will be split in
   * http://www.kde.org and tar:/kde/README.html#ref1.
   * That means in turn that "#ref1" is an HTML-style reference and not a new sub URL.
   * Since HTML-style references mark
   * a certain position in a document this reference is appended to every URL.
   * The idea behind this is that browsers, for example, only look at the first URL while
   * the rest is not of interest to them.
   *
   * @return An empty list on error or the list of split URLs.
   *
   * @param _url The URL that has to be split.
   */
  static List split( const QString& _url );

  /**
   * A convenience function.
   */
  static List split( const KURL& _url );

  /**
   * Reverses @ref split(). Only the first URL may have a reference. This reference
   * is considered to be HTML-like and is appended at the end of the resulting
   * joined URL.
   */
  static KURL join( const List& _list );

  /**
   * Convenience function
   *
   * Convert unicoded string to local encoding and use %-style
   * encoding for all common delimiters / non-ascii characters.
   * @param str String to encode
   * @param encoding_hint Reserved, should be 0.
   **/
  static QString encode_string(const QString &str, int encoding_hint = 0);

  /**
   * Convenience function
   *
   * Convert unicoded string to local encoding and use %-style
   * encoding for all common delimiters / non-ascii characters
   * as well as the slash '/'.
   * @param str String to encode
   * @param encoding_hint Reserved, should be 0.
   **/
  static QString encode_string_no_slash(const QString &str, int encoding_hint = 0);

  /**
   * Convenience function
   *
   * Decode %-style encoding and convert from local encoding to unicode.
   *
   * Revers of encode_string()
   * @param str String to decode
   * @param encoding_hint Reserved, should be 0.
   **/
  static QString decode_string(const QString &str, int encoding_hint = 0);

  /**
   * Convenience function
   *
   * Returns whether '_url' is likely to be a "relative" URL instead of
   * an "absolute" URL.
   * @param _url URL to examine
   * @return true when the URL is likely to be "relative", false otherwise.
   */
  static bool isRelativeURL(const QString &_url);

protected:
  void reset();
  void parse( const QString& _url, int encoding_hint = 0);

private:
  QString m_strProtocol;
  QString m_strUser;
  QString m_strPass;
  QString m_strHost;
  QString m_strPath;
  QString m_strRef_encoded;
  QString m_strQuery_encoded;
  KURLPrivate* d;
  bool m_bIsMalformed : 1;
  int freeForUse      : 7;
  unsigned short int m_iPort;
  QString m_strPath_encoded;

#ifndef QT_NO_DATASTREAM  
  friend QDataStream & operator<< (QDataStream & s, const KURL & a);
  friend QDataStream & operator>> (QDataStream & s, KURL & a);
#endif
};

/**
 * Compares URLs. They are parsed, split and compared.
 * Two malformed URLs with the same string representation
 * are nevertheless considered to be unequal.
 * That means no malformed URL equals anything else.
 */
bool urlcmp( const QString& _url1, const QString& _url2 );

/**
 * Compares URLs. They are parsed, split and compared.
 * Two malformed URLs with the same string representation
 * are nevertheless considered to be unequal.
 * That means no malformed URL equals anything else.
 *
 * @param _ignore_trailing Described in @ref KURL::cmp
 * @param _ignore_ref If @p true, disables comparison of HTML-style references.
 */
bool urlcmp( const QString& _url1, const QString& _url2, bool _ignore_trailing, bool _ignore_ref );

#ifndef QT_NO_DATASTREAM
QDataStream & operator<< (QDataStream & s, const KURL & a);
QDataStream & operator>> (QDataStream & s, KURL & a);
#endif

#endif
