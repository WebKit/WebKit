/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import <kurl.h>
#import <kwqdebug.h>


typedef enum {
    // alpha 
    SchemeFirstChar = 1 << 0,

    // ( alpha | digit | "+" | "-" | "." )
    SchemeChar = 1 << 1,

    // mark        = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
    // unreserved  = alphanum | mark
    // ( unreserved | escaped | ";" | ":" | "&" | "=" | "+" | "$" | "," )
    UserInfoChar = 1 << 2,

    // alnum | "." | "-" | "%"
    HostnameChar = 1 << 3,

    // hexdigit | ":" | "%"
    IPv6Char = 1 << 4,

    // "#" | "?" | "/" | nul
    PathSegmentEndChar = 1 << 5,

    // digit | "A" | "B" | "C" | "D" | "E" | "F" | "a" | "b" | "c" | "d" | "e" | "f"
    HexDigitChar = 1 << 6,

    // not allowed in path and not ? or #
    BadChar = 1 << 7
} URLCharacterClasses;

static const unsigned char characterClassTable[256] = {
    /* 0 nul */ PathSegmentEndChar,    /* 1 soh */ BadChar,
    /* 2 stx */ BadChar,    /* 3 etx */ BadChar,    
    /* 4 eot */ BadChar,    /* 5 enq */ BadChar,    /* 6 ack */ BadChar,    /* 7 bel */ BadChar,
    /* 8 bs */ BadChar,     /* 9 ht */ BadChar,    /* 10 nl */ BadChar,    /* 11 vt */ BadChar,
    /* 12 np */ BadChar,    /* 13 cr */ BadChar,    /* 14 so */ BadChar,    /* 15 si */ BadChar,
    /* 16 dle */ BadChar,   /* 17 dc1 */ BadChar,   /* 18 dc2 */ BadChar,   /* 19 dc3 */ BadChar,
    /* 20 dc4 */ BadChar,   /* 21 nak */ BadChar,   /* 22 syn */ BadChar,   /* 23 etb */ BadChar,
    /* 24 can */ BadChar,   /* 25 em */ BadChar,    /* 26 sub */ BadChar,   /* 27 esc */ BadChar,
    /* 28 fs */ BadChar,    /* 29 gs */ BadChar,    /* 30 rs */ BadChar,    /* 31 us */ BadChar,
    /* 32 sp */ BadChar,    /* 33  ! */ UserInfoChar,
    /* 34  " */ BadChar,    /* 35  # */ PathSegmentEndChar,    
    /* 36  $ */ UserInfoChar,    /* 37  % */ UserInfoChar | HostnameChar | IPv6Char | BadChar,
    /* 38  & */ UserInfoChar,    /* 39  ' */ UserInfoChar,
    /* 40  ( */ UserInfoChar,    /* 41  ) */ UserInfoChar,    
    /* 42  * */ UserInfoChar,    /* 43  + */ SchemeChar | UserInfoChar,
    /* 44  , */ UserInfoChar,    
    /* 45  - */ SchemeChar | UserInfoChar | HostnameChar, 
    /* 46  . */ SchemeChar | UserInfoChar | HostnameChar,   
    /* 47  / */ PathSegmentEndChar,
    /* 48  0 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
    /* 49  1 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,    
    /* 50  2 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
    /* 51  3 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 52  4 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
    /* 53  5 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 54  6 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
    /* 55  7 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 56  8 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
    /* 57  9 */ SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 58  : */ UserInfoChar | IPv6Char,    /* 59  ; */ UserInfoChar,
    /* 60  < */ BadChar,    /* 61  = */ UserInfoChar,
    /* 62  > */ BadChar,    /* 63  ? */ PathSegmentEndChar,
    /* 64  @ */ 0,
    /* 65  A */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,    
    /* 66  B */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 67  C */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 68  D */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 69  E */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 70  F */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 71  G */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 72  H */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 73  I */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 74  J */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 75  K */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 76  L */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 77  M */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 78  N */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 79  O */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 80  P */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 81  Q */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 82  R */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 83  S */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 84  T */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 85  U */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 86  V */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 87  W */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 88  X */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 89  Y */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 90  Z */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 91  [ */ BadChar,
    /* 92  \ */ BadChar,    /* 93  ] */ BadChar,
    /* 94  ^ */ BadChar,    /* 95  _ */ UserInfoChar,
    /* 96  ` */ BadChar,
    /* 97  a */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 98  b */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
    /* 99  c */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 100  d */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
    /* 101  e */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char,
    /* 102  f */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar | HexDigitChar | IPv6Char, 
    /* 103  g */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 104  h */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 105  i */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 106  j */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 107  k */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 108  l */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 109  m */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 110  n */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 111  o */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 112  p */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 113  q */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 114  r */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 115  s */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 116  t */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 117  u */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 118  v */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 119  w */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 120  x */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 121  y */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar,
    /* 122  z */ SchemeFirstChar | SchemeChar | UserInfoChar | HostnameChar, 
    /* 123  { */ BadChar,
    /* 124  | */ BadChar,   /* 125  } */ BadChar,   /* 126  ~ */ UserInfoChar,   /* 127 del */ BadChar,
    /* 128 */ BadChar, /* 129 */ BadChar, /* 130 */ BadChar, /* 131 */ BadChar,
    /* 132 */ BadChar, /* 133 */ BadChar, /* 134 */ BadChar, /* 135 */ BadChar,
    /* 136 */ BadChar, /* 137 */ BadChar, /* 138 */ BadChar, /* 139 */ BadChar,
    /* 140 */ BadChar, /* 141 */ BadChar, /* 142 */ BadChar, /* 143 */ BadChar,
    /* 144 */ BadChar, /* 145 */ BadChar, /* 146 */ BadChar, /* 147 */ BadChar,
    /* 148 */ BadChar, /* 149 */ BadChar, /* 150 */ BadChar, /* 151 */ BadChar,
    /* 152 */ BadChar, /* 153 */ BadChar, /* 154 */ BadChar, /* 155 */ BadChar,
    /* 156 */ BadChar, /* 157 */ BadChar, /* 158 */ BadChar, /* 159 */ BadChar,
    /* 160 */ BadChar, /* 161 */ BadChar, /* 162 */ BadChar, /* 163 */ BadChar,
    /* 164 */ BadChar, /* 165 */ BadChar, /* 166 */ BadChar, /* 167 */ BadChar,
    /* 168 */ BadChar, /* 169 */ BadChar, /* 170 */ BadChar, /* 171 */ BadChar,
    /* 172 */ BadChar, /* 173 */ BadChar, /* 174 */ BadChar, /* 175 */ BadChar,
    /* 176 */ BadChar, /* 177 */ BadChar, /* 178 */ BadChar, /* 179 */ BadChar,
    /* 180 */ BadChar, /* 181 */ BadChar, /* 182 */ BadChar, /* 183 */ BadChar,
    /* 184 */ BadChar, /* 185 */ BadChar, /* 186 */ BadChar, /* 187 */ BadChar,
    /* 188 */ BadChar, /* 189 */ BadChar, /* 190 */ BadChar, /* 191 */ BadChar,
    /* 192 */ BadChar, /* 193 */ BadChar, /* 194 */ BadChar, /* 195 */ BadChar,
    /* 196 */ BadChar, /* 197 */ BadChar, /* 198 */ BadChar, /* 199 */ BadChar,
    /* 200 */ BadChar, /* 201 */ BadChar, /* 202 */ BadChar, /* 203 */ BadChar,
    /* 204 */ BadChar, /* 205 */ BadChar, /* 206 */ BadChar, /* 207 */ BadChar,
    /* 208 */ BadChar, /* 209 */ BadChar, /* 210 */ BadChar, /* 211 */ BadChar,
    /* 212 */ BadChar, /* 213 */ BadChar, /* 214 */ BadChar, /* 215 */ BadChar,
    /* 216 */ BadChar, /* 217 */ BadChar, /* 218 */ BadChar, /* 219 */ BadChar,
    /* 220 */ BadChar, /* 221 */ BadChar, /* 222 */ BadChar, /* 223 */ BadChar,
    /* 224 */ BadChar, /* 225 */ BadChar, /* 226 */ BadChar, /* 227 */ BadChar,
    /* 228 */ BadChar, /* 229 */ BadChar, /* 230 */ BadChar, /* 231 */ BadChar,
    /* 232 */ BadChar, /* 233 */ BadChar, /* 234 */ BadChar, /* 235 */ BadChar,
    /* 236 */ BadChar, /* 237 */ BadChar, /* 238 */ BadChar, /* 239 */ BadChar,
    /* 240 */ BadChar, /* 241 */ BadChar, /* 242 */ BadChar, /* 243 */ BadChar,
    /* 244 */ BadChar, /* 245 */ BadChar, /* 246 */ BadChar, /* 247 */ BadChar,
    /* 248 */ BadChar, /* 249 */ BadChar, /* 250 */ BadChar, /* 251 */ BadChar,
    /* 252 */ BadChar, /* 253 */ BadChar, /* 254 */ BadChar, /* 255 */ BadChar
};

// FIXME: convert to inline functions

#define IS_SCHEME_FIRST_CHAR(c) (characterClassTable[(unsigned char)c] & SchemeFirstChar)
#define IS_SCHEME_CHAR(c) (characterClassTable[(unsigned char)c] & SchemeChar)
#define IS_USERINFO_CHAR(c) (characterClassTable[(unsigned char)c] & UserInfoChar)
#define IS_HOSTNAME_CHAR(c) (characterClassTable[(unsigned char)c] & HostnameChar)
#define IS_IPV6_CHAR(c) (characterClassTable[(unsigned char)c] & IPv6Char)
#define IS_PATH_SEGMENT_END_CHAR(c) (characterClassTable[(unsigned char)c] & PathSegmentEndChar)
#define IS_BAD_CHAR(c) (characterClassTable[(unsigned char)c] & BadChar)
#define IS_HEX_DIGIT(c) (characterClassTable[(unsigned char)c] & HexDigitChar)


// KURL

KURL::KURL() :
    m_isValid(false)
{
}

KURL::KURL(const char *url, int encoding_hint) :
    m_isValid(true)
{
    if (url != NULL && url[0] == '/') {
	QString qurl = QString("file:") + url;
	parse(qurl.ascii());
    } else {
	parse(url);
    }
}

KURL::KURL(const QString &url, int encoding_hint) :
    m_isValid(true)

{
    if (!url.isEmpty() && url[0] == '/') {
	QString fileUrl = QString("file:") + url;
	parse(fileUrl.ascii());
    } else {
	parse(url.ascii());
    }
}

KURL::KURL(const KURL &base, const QString &relative)
{
    bool absolute = false;
    const char *str = relative.ascii();
    for (const char *p = str; *p != '\0'; ++p) {
	if (*p == ':') {
	    absolute = true;
	    break;
	} else if (*p == '/' || *p == '?' || *p == '#') {
	    break;
	}
    }
    
    if (absolute) {
	parse(str);
    } else {
	// workaround for sites that put leading whitespace on relative URLs
	while (*str == ' ') {
	    str++;
	}

	if (!base.m_isValid) {
	    QString newURL = base.urlString + relative;
	    parse(newURL.ascii());
	}

	switch(str[0]) {
	case '\0':
	    // empty
	    {
		*this = base;
		break;
	    }
	case '#':
	    // must be fragment reference only
	    {
		QString newURL = base.urlString.left(base.queryEndPos) + relative;
		parse(newURL.ascii());
		break;
	    }
	case '/':
	    // must be net-path or absolute-path reference
	    {
		if (str[1] == '/') {
		    // net-path
		    QString newURL = base.urlString.left(base.schemeEndPos + 1) + str;
		    parse(newURL.ascii());
		} else {
		    // abs-path
		    QString newURL = base.urlString.left(base.portEndPos) + str;
		    parse(newURL.ascii());
		}
		break;
	    }
	default:
	    {
		// must be relative-path reference
		QString newURL = base.urlString.left(base.portEndPos);
		char static_buffer[2048];
		char *buffer;
		bool usingStaticBuffer;
		
		if ((base.pathEndPos - base.portEndPos) + relative.length() >= 2048) {
		    buffer = (char *)malloc(((base.pathEndPos - base.portEndPos) + relative.length() + 1) * sizeof(char));
		    usingStaticBuffer = false;
		} else {
		    buffer = static_buffer;
		    usingStaticBuffer = true;
		}
		
		char *bufferPos = buffer;
		
		// first copy the base path 
		const char *baseString = base.urlString.ascii();
		const char *baseStringStart = baseString + base.portEndPos;
		const char *baseStringEnd = baseString + base.pathEndPos;
		
		// go back to the last slash
		while (baseStringEnd > baseStringStart && baseStringEnd[-1] != '/') {
		    baseStringEnd--;
		}
		
		// now copy the base path, accounting for "." and ".." segments
		const char *baseStringPos = baseStringStart;
		while (baseStringPos < baseStringEnd) {
		    if (baseStringPos[0] == '.' && bufferPos[-1] == '/') {
			if (baseStringPos[1] == '/' || baseStringPos + 1 == baseStringEnd) {
			    // skip over "." segments
			    baseStringPos += 2;
			    continue;
			} else if (baseStringPos[1] == '.' && (baseStringPos[2] == '/' ||
							       baseStringPos + 2 == baseStringEnd)) {
			    // skip over ".." segments and rewind the last segment
			    baseStringPos += 3;
			    if (bufferPos > buffer + 1) {
				bufferPos--;
			    }
			    while (bufferPos > buffer && bufferPos[-1] != '/') {
				bufferPos--;
			    }
			    // don't strip the slash before the last path segment if it was the final one
			    if (baseStringPos[2] != '/') {
				bufferPos++;
			    }
			    continue;
			}
		    }

		    *bufferPos = *baseStringPos;
		    baseStringPos++;
		    bufferPos++;
		}

		const char *relStringStart = relative.ascii();
		const char *relStringPos = relStringStart;
		
		while (*relStringPos != '\0' && *relStringPos != '?' && *relStringPos != '#') {
		    if (relStringPos[0] == '.' && bufferPos[-1] == '/') {
			if (IS_PATH_SEGMENT_END_CHAR(relStringPos[1])) {
			    // skip over "." segments
			    relStringPos += 1;
			    if (relStringPos[0] == '/') {
				relStringPos++;
			    }
			    continue;
			} else if (relStringPos[1] == '.' && IS_PATH_SEGMENT_END_CHAR(relStringPos[2])) {
			    // skip over ".." segments and rewind the last segment
			    relStringPos += 2;
			    if (relStringPos[0] == '/') {
				relStringPos++;
			    }
			    if (bufferPos > buffer + 1) {
				bufferPos--;
			    }
			    while (bufferPos > buffer + 1  && bufferPos[-1] != '/') {
				bufferPos--;
			    }
			    continue;
			}
		    }
		    
		    *bufferPos = *relStringPos;
		    relStringPos++;
		    bufferPos++;
		}

		// all done with the path work, now copy any remainder
		// of the relative reference; this will also add a null terminator
		strcpy(bufferPos, relStringPos);

		// and parse the thing (throwing away the fact that we
		// know the path bounds already, but what the heck)
		newURL += buffer;
		
		parse(newURL.ascii());
		
		if (!usingStaticBuffer) {
		    free(buffer);
		}
		
		break;
	    }
	}
    }
}

bool KURL::hasPath() const
{
    return m_isValid && pathEndPos != portEndPos;
}

QString KURL::protocol() const
{
    if (!m_isValid) {
	return QString();
    }

    return urlString.left(schemeEndPos);
}

QString KURL::host() const
{
    if (!m_isValid) {
	return QString();
    }

    int start = (passwordEndPos == userStartPos) ? passwordEndPos : passwordEndPos + 1;
    return decode_string(urlString.mid(start, hostEndPos - start));
}

unsigned short int KURL::port() const
{
    if (!m_isValid) {
	return 0;
    }

    if (hostEndPos != portEndPos) {
	bool ok;
	unsigned short result = urlString.mid(hostEndPos + 1, portEndPos - hostEndPos - 1).toUShort(&ok);
	if (!ok) {
	    result = 0;
	}
	return result;
    }

    return 0;
}

QString KURL::pass() const
{
    if (!m_isValid) {
	return QString();
    }

    if (passwordEndPos == userEndPos) {
	return QString();
    }

    return decode_string(urlString.mid(userEndPos + 1, passwordEndPos - userEndPos - 1)); 
}

QString KURL::user() const
{
    if (!m_isValid) {
	return QString();
    }

    return decode_string(urlString.mid(userStartPos, userEndPos - userStartPos));
}

QString KURL::ref() const
{
    if (!m_isValid) {
	return QString();
    }
    
    if (fragmentEndPos == queryEndPos) {
	return QString();
    }

    return urlString.mid(queryEndPos + 1, fragmentEndPos - queryEndPos - 1); 
}

QString KURL::query() const
{
    if (!m_isValid) {
	return QString();
    }

    return urlString.mid(pathEndPos, queryEndPos - pathEndPos); 
}

QString KURL::path() const
{
    if (!m_isValid) {
	return QString();
    }

    return decode_string(urlString.mid(portEndPos, pathEndPos - portEndPos)); 
}

void KURL::setProtocol(const QString &s)
{
    if (!m_isValid) {
	QString newURL = s + ":" + urlString;
	parse(newURL.ascii());
	return;
    }

    QString newURL = s + urlString.mid(schemeEndPos);
    parse(newURL.ascii());
}

void KURL::setHost(const QString &s)
{
    if (m_isValid) {
	bool slashSlashNeeded = userStartPos == schemeEndPos + 1;
	int hostStart = (passwordEndPos == userStartPos) ? passwordEndPos : passwordEndPos + 1;
	
	QString newURL = urlString.left(hostStart) + (slashSlashNeeded ? "//" : QString()) + s + urlString.mid(hostEndPos);
	parse(newURL.ascii());
    }
}

void KURL::setPort(unsigned short i)
{
    if (m_isValid) {
	bool colonNeeded = portEndPos == hostEndPos;
	int portStart = (colonNeeded ? hostEndPos : hostEndPos + 1);
	QString newURL = urlString.left(portStart) + (colonNeeded ? ":" : QString()) + QString::number(i) + urlString.mid(portEndPos);
	parse(newURL.ascii());
    }
}

void KURL::setRef(const QString &s)
{
    if (m_isValid) {
	QString newURL = urlString.left(queryEndPos) + (s.isEmpty() ? QString() : "#" + s);
	parse(newURL.ascii());
    }
}

void KURL::setQuery(const QString &query, int encoding_hint)
{
    QString q;
    if (m_isValid) {
	if (!query.isEmpty() && query[0] != '?') {
	    q = "?" + query;
	} else {
	    q = query;
	}

        QString newURL = urlString.left(pathEndPos) + q + urlString.mid(queryEndPos);
	parse(newURL.ascii());
    }
}

void KURL::setPath(const QString &s)
{
    if (m_isValid) {
	QString newURL = urlString.left(portEndPos) + s + urlString.mid(pathEndPos);
	parse(newURL.ascii());
    }
}

QString KURL::prettyURL(int trailing) const
{
    if (!m_isValid) {
        return urlString;
    }

    QString result = protocol() + ":";

    QString authority;

    if (hostEndPos != passwordEndPos) {
	if (userEndPos != userStartPos) {
	    authority += user();
	    authority += "@";
	}
	authority += host();
	if (port() != 0) {
	    authority += ":";
	    authority += QString::number(port());
	}
    }

    if (!authority.isEmpty()) {
        result += "//" + authority;
    }

    QString path = this->path();

    if (trailing == 1) {
        if (path.right(1) != "/" && !path.isEmpty()) {
	    path += "/";
	}
    } else if (trailing == -1) {
        if (path.right(1) == "/" && path.length() > 1) {
	    path = path.left(path.length()-1);
	}
    }

    result += path;
    result += query();

    if (fragmentEndPos != queryEndPos) {
        result += "#" + ref();
    }

    return result;
}

QString KURL::decode_string(const QString &urlString)
{
    // FIXME: do it yerself

    CFStringRef unescaped = CFURLCreateStringByReplacingPercentEscapes(NULL, urlString.getCFString(), CFSTR(""));
    QString qUnescaped = QString::fromCFString(unescaped);
    CFRelease(unescaped);

    return qUnescaped;
}

const char *hexDigits="0123456789ABCDEF";

static QString escapeBadChars(const QString &strToEscape)
{
    const char *str = strToEscape.ascii();
    char static_buffer[4096];
    bool usingStaticBuffer;
    char *buffer; 

    if (strToEscape.length() * 3 < 4096) {
	buffer = static_buffer;
	usingStaticBuffer = true;
    } else {
	buffer = (char *)malloc((strToEscape.length() * 3 + 1) * sizeof(char));
	usingStaticBuffer = false;
    }

    char *p = buffer;

    while (*str != '\0') {
	if (*str == '%' && IS_HEX_DIGIT(str[1]) && IS_HEX_DIGIT(str[2])) {
	    *p++ = *str++;
	    *p++ = *str++;
	    *p++ = *str++;
	} else if (IS_BAD_CHAR(*str)) {
	    *p++ = '%';
	    *p++ = hexDigits[(*str) / 16];
	    *p++ = hexDigits[(*str) % 16];
	    str++;
	} else {
	    *p++ = *str++;
	}
    }
    *p = '\0';
    
    QString result(buffer);
    if (!usingStaticBuffer) {
	free(buffer);
    }

    return result;
}



void KURL::parse(const char *url)
{
    m_isValid = true;

    if (url == NULL || url[0] == '\0') {
	// valid URL must be non-empty
	m_isValid = false;
	urlString = url;
	return;
    }

    if (!IS_SCHEME_FIRST_CHAR(url[0])) {
	// scheme must start with an alphabetic character
	m_isValid = false;
	urlString = url;
	return;
    }

    int schemeEnd = 0;
 
    while (IS_SCHEME_CHAR(url[schemeEnd])) {
	schemeEnd++;
    }

    if (url[schemeEnd] != ':') {
	m_isValid = false;
	urlString = url;
	return;
    }

    int userStart = schemeEnd + 1;
    int userEnd;
    int passwordStart;
    int passwordEnd;
    int hostStart;
    int hostEnd;
    int portStart;
    int portEnd;

    bool hierarchical = url[schemeEnd + 1] == '/';

    if (hierarchical && url[schemeEnd + 2] == '/') {
	// part after the scheme must be a net_path, parse the authority section

	// FIXME: authority characters may be scanned twice
	userStart += 2;
	userEnd = userStart;

	int colonPos = 0;
	while (IS_USERINFO_CHAR(url[userEnd])) {
	    if (url[userEnd] == ':' && colonPos == 0) {
		colonPos = userEnd;
	    }
	    userEnd++;
	}
	
	if (url[userEnd] == '@') {
	    // actual end of the userinfo, start on the host
	    if (colonPos != 0) {
		passwordEnd = userEnd;
		userEnd = colonPos;
		passwordStart = colonPos + 1;
	    } else {
		passwordStart = passwordEnd = userEnd;
	    }
	    hostStart = passwordEnd + 1;
	} else if (url[userEnd] == '\0' || url[userEnd] == '/' || url[userEnd] == '[') {
	    // hit the end of the authority, must have been no user
	    // or looks like an IPv6 hostname
	    // either way, try to parse it as a hostname
	    userEnd = userStart;
	    passwordStart = passwordEnd = userEnd;
	    hostStart = userStart;
	} else {
	    // invalid character
	    m_isValid = false;
	    urlString = url;
	    return;
	}

	hostEnd = hostStart;

	// IPV6 IP address
	if (url[hostEnd] == '[') {
	    hostEnd++;
	    while (IS_IPV6_CHAR(url[hostEnd])) {
		hostEnd++;
	    }
	    if (url[hostEnd] == ']') {
		hostEnd++;
	    } else {
		// invalid character
		m_isValid = false;
		urlString = url;
		return;
	    }
	} else {
	    while (IS_HOSTNAME_CHAR(url[hostEnd])) {
		hostEnd++;
	    }
	}
	
	if (url[hostEnd] == ':') {
	    portStart = portEnd = hostEnd + 1;
 
	    // possible start of port
	    portEnd = portStart;
	    while (isdigit(url[portEnd])) {
		portEnd++;
	    }
	} else {
	    portStart = portEnd = hostEnd;
	}

	if (url[portEnd] != '\0' && url[portEnd] != '/') {
	    // invalid character
	    m_isValid = false;
	    urlString = url;
	    return;
	}
    } else {
	// the part after the scheme must be an opaque_part or an abs_path
	userEnd = userStart;
	passwordStart = passwordEnd = userEnd;
	hostStart = hostEnd = passwordEnd;
	portStart = portEnd = hostEnd;
    }
	
    int pathStart = portEnd;
    int pathEnd;
    int queryStart;
    int queryEnd;
    int fragmentStart;
    int fragmentEnd;

    if (!hierarchical) {
	// FIXME: could have a fragment
	pathEnd = strlen(url);
	queryStart = queryEnd = pathEnd;
	fragmentStart = fragmentEnd = queryEnd;
    } else {
	pathEnd = pathStart;
	while (url[pathEnd] != '\0' && url[pathEnd] != '?' && url[pathEnd] != '#') {
	    pathEnd++;
	}
	
	queryStart = queryEnd = pathEnd;

	if (url[queryStart] == '?') {
	    while (url[queryEnd] != '\0' && url[queryEnd] != '#') {
		queryEnd++;
	    }
	}

	fragmentStart = fragmentEnd = queryEnd;

	if (url[fragmentStart] == '#') {
	    fragmentStart++;
	    // FIXME: don't use strlen, counting up should be faster
	    fragmentEnd = fragmentStart + strlen(url + fragmentStart);
	}
    }

    // assemble it all, remembering the real ranges

    urlString = QString(url, schemeEnd + 1);
    schemeEndPos = schemeEnd;

    QString authority;
    if (userEnd != userStart) {
	authority += QString(url + userStart, userEnd - userStart);
    }

    userEndPos = authority.length();

    if (passwordEnd != passwordStart) {
	// start ahead one to include the colon
	authority += QString(url + passwordStart - 1, passwordEnd - passwordStart + 1);
    }

    passwordEndPos = authority.length();

    if (!authority.isEmpty()) {
	authority += "@";
    }

    if (hostEnd != hostStart) {
	authority += QString(url + hostStart, hostEnd - hostStart);
    }

    hostEndPos = authority.length();

    if (portEnd != portStart) {
	// start ahead one to include the colon
	authority += QString(url + portStart -1, portEnd - portStart + 1);
    }
    
    portEndPos = authority.length();

    if (!authority.isEmpty() && authority != "localhost") {
	userStartPos = urlString.length() + 2;
	userEndPos += userStartPos;
	passwordEndPos += userStartPos;
	hostEndPos += userStartPos;
	portEndPos += userStartPos;

	urlString += "//" + authority;
    } else {
	userStartPos = userEndPos = urlString.length();
	passwordEndPos = userStartPos;
	hostEndPos = userStartPos;
	portEndPos = userStartPos;
    }

    if (pathEnd != pathStart) {
	QString path(url + pathStart, pathEnd - pathStart);
	urlString += escapeBadChars(path);
    }
    pathEndPos = urlString.length();
    
    if (queryEnd != queryStart) {
	urlString += escapeBadChars(QString(url + queryStart, queryEnd - queryStart));
    }
    queryEndPos = urlString.length();

    if (fragmentEnd != fragmentStart) {
	urlString += "#" + escapeBadChars(QString(url + fragmentStart, fragmentEnd - fragmentStart));
    }
    fragmentEndPos = urlString.length();
}

NSURL *KURL::getNSURL() const
{
    return [NSURL URLWithString:urlString.getNSString()];
}

QString KURL::encodedHtmlRef() const
{
    return ref();
}

QString KURL::htmlRef() const
{
    return decode_string(ref());
}

bool operator==(const KURL &a, const KURL &b)
{
    return a.urlString == b.urlString;
}
