/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <kurl.h>

#include <qtextstream.h>

#include <kdom/Helper.h>
#include "AttrImpl.h"
#include "Namespace.h"
#include "DOMString.h"
#include "DOMException.h"
#include "RenderStyleDefs.h"
#include "LSSerializerImpl.h"

using namespace KDOM;

void Helper::SplitNamespace(DOMString &prefix, DOMString &name, DOMStringImpl *qualifiedName)
{
	int i = DOMString(qualifiedName).find(':');
	if(i == -1)
		name = DOMString(qualifiedName->copy());
	else
	{
		prefix = DOMString(qualifiedName->copy());
		name = prefix.split(i + 1);
	}
}

void Helper::SplitPrefixLocalName(DOMStringImpl *qualifiedName, DOMString &prefix, DOMString &localName, int colonPos)
{
	if(colonPos == -2)
	{
		QChar *qu = qualifiedName->unicode();
		for(unsigned int i = 0 ; i < qualifiedName->length() ; ++i)
		{
			if(qu[i] == ':')
			{
				colonPos = i;
				break;
			}
		}
	}

	if(colonPos >= 0)
	{
		prefix = DOMString(qualifiedName->copy());
		localName = prefix.split(colonPos + 1);
		prefix.truncate(colonPos);
	}
	else
		localName = DOMString(qualifiedName);
}

void Helper::CheckPrefix(const DOMString &prefix, const DOMString &, const DOMString &namespaceURI)
{
	// INVALID_CHARACTER_ERR: Raised if the specified prefix contains an illegal character.
	if(!Helper::ValidatePrefix(prefix.implementation()))
		throw new DOMExceptionImpl(INVALID_CHARACTER_ERR);

	// NAMESPACE_ERR:
	// - Raised if the specified prefix is malformed,
	// - if the namespaceURI is null, or
	// - if this node is an attribute and the qualifiedName of this node is "xmlns", or
	// - if the qualifiedName is null and the namespaceURI is different from null, or
	// - if the specified prefix is "xml" and the namespaceURI is different
	//   from "http://www.w3.org/XML/1998/namespace" [Namespaces]. or
	// - if this node is an attribute and the specified prefix is "xmlns" and
	//   the namespaceURI of this node is different from "http://www.w3.org/2000/xmlns/"
	if(IsMalformedPrefix(prefix) || namespaceURI.isNull() ||
	   (prefix[0] == 'x' && prefix[1] == 'm' && prefix[2] == 'l' && namespaceURI != NS_XML) ||
	   (prefix[0] == 'x' && prefix[1] == 'm' && prefix[2] == 'l' && prefix[3] == 'n' && prefix[4] == 's' && namespaceURI != NS_XMLNS))
		throw new DOMExceptionImpl(NAMESPACE_ERR);
}

void Helper::CheckQualifiedName(const DOMString &qualifiedName, const DOMString &namespaceURI, int &colonPos, bool nameCanBeNull, bool nameCanBeEmpty)
{
	// NAMESPACE_ERR: Raised if no qualifiedName supplied (not mentioned in the spec!) or when it's malformed
	if(!nameCanBeNull && qualifiedName.isNull())
		throw new DOMExceptionImpl(NAMESPACE_ERR);

	// INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
	if(!qualifiedName.isNull() && !ValidateQualifiedName(qualifiedName.implementation()) && (!qualifiedName.isEmpty() || !nameCanBeEmpty))
		throw new DOMExceptionImpl(INVALID_CHARACTER_ERR);

	// NAMESPACE_ERR:
	// - Raised if the qualifiedName is malformed,
	// - if the qualifiedName has a prefix and the namespaceURI is null, or
	// - if the qualifiedName is null and the namespaceURI is different from null
	// - if the qualifiedName has a prefix that is "xml" and the namespaceURI is different
	//   from "http://www.w3.org/XML/1998/namespace" [Namespaces].
	// - if the qualifiedName is "xmlns" and the namespaceURI is different from "http://www.w3.org/2000/xmlns/".
	colonPos = CheckMalformedQualifiedName(qualifiedName);

	if((colonPos >= 0 && namespaceURI.isNull()) || (qualifiedName.isNull() && !namespaceURI.isNull()) ||
	   (colonPos == 3 && qualifiedName[0] == 'x' && qualifiedName[1] == 'm' && qualifiedName[2] == 'l' &&
	   namespaceURI != NS_XML) || ((qualifiedName == "xmlns" ||
	   (colonPos == 5 && qualifiedName.string().startsWith(QString::fromLatin1("xmlns")))) &&
	   namespaceURI != NS_XMLNS))
		throw new DOMExceptionImpl(NAMESPACE_ERR);
}

int Helper::CheckMalformedQualifiedName(const DOMString &qualifiedName)
{
	int colonPos = -1;

	QChar *qu = qualifiedName.unicode();
	unsigned int len = qualifiedName.length();
	if(!len || !qu)
		return colonPos;
	
	for(unsigned int i = 0 ; i < len; i++)
	{
		if(qu[i] == ':')
		{
			if(colonPos > -1 ||  // already seen a ':'
				i == 0 ||        // empty prefix, but has ':'
				i == len - 1)    // empty local name
				throw new DOMExceptionImpl(NAMESPACE_ERR);

			colonPos = i;
		}
	}

	return colonPos;
}

bool Helper::IsMalformedPrefix(const DOMString &prefix)
{
	// TODO : find out definition of malformed prefix // don't forget the API docs
	return prefix.find(':') != -1;
}

bool Helper::ValidatePrefix(DOMStringImpl *name)
{
	// Null prefix is ok. If not null, reuse code from ValidAttributeName
	return !name || ValidateAttributeName(name);
}

bool Helper::ValidateAttributeName(DOMStringImpl *name)
{
	// Check if name is valid
	// http://www.w3.org/TR/2000/REC-xml-20001006#NT-Name
	if(!name || !name->length())
		return false;

	QChar *unicode = name->unicode();
	QChar ch = unicode[0];
	if(!ch.isLetter() && ch != '_' && ch != ':')
		return false; // first char isn't valid

#ifndef APPLE_COMPILE_HACK
	for(unsigned int i = 1; i < name->length(); ++i)
	{
		ch = unicode[i];
		if(!ch.isLetter() && !ch.isDigit() && ch != '.' &&
		   ch != '-' && ch != '_' && ch != ':' &&
		   ch.category() != QChar::Mark_SpacingCombining) // no idea what "extender is"
			return false;
	}
#endif // APPLE_COMPILE_HACK

	return true;
}

bool Helper::ValidateQualifiedName(DOMStringImpl *name)
{
	return ValidateAttributeName(name);
}

void Helper::CheckInUse(ElementImpl *ownerElement, AttrImpl *attr)
{
	// INUSE_ATTRIBUTE_ERR: Raised if attr is an Attr that is already an attribute of another Element object.
	// The DOM user must explicitly clone Attr nodes to re-use them in other elements.
	if(ownerElement != 0 && attr->ownerElement() != 0 && attr->ownerElement() != ownerElement)
		throw new DOMExceptionImpl(INUSE_ATTRIBUTE_ERR);
}

void Helper::CheckWrongDocument(DocumentImpl *ownerDocument, NodeImpl *node)
{
	// WRONG_DOCUMENT_ERR: Raised if node was created from a different document than the one that created this map.
	if(ownerDocument != 0 && node->ownerDocument() != 0 && node->ownerDocument() != ownerDocument)
		throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);
}

void Helper::ShowException(const DOMException &e)
{
	DOMString res;
	switch(e.code())
	{
		case INDEX_SIZE_ERR: { res = "INDEX_SIZE_ERR"; break; }
		case DOMSTRING_SIZE_ERR: { res = "DOMSTRING_SIZE_ERR"; break; }
		case HIERARCHY_REQUEST_ERR: { res = "HIERARCHY_REQUEST_ERR"; break; }
		case WRONG_DOCUMENT_ERR: { res = "WRONG_DOCUMENT_ERR"; break; }
		case INVALID_CHARACTER_ERR: { res = "INVALID_CHARACTER_ERR"; break; }
		case NO_DATA_ALLOWED_ERR: { res = "NO_DATA_ALLOWED_ERR"; break; }
		case NO_MODIFICATION_ALLOWED_ERR: { res = "NO_MODIFICATION_ALLOWED_ERR"; break; }
		case NOT_FOUND_ERR: { res = "NOT_FOUND_ERR"; break; }
		case NOT_SUPPORTED_ERR: { res = "NOT_SUPPORTED_ERR"; break; }
		case INUSE_ATTRIBUTE_ERR: { res = "INUSE_ATTRIBUTE_ERR"; break; }
		case INVALID_STATE_ERR: { res = "INVALID_STATE_ERR"; break; }
		case SYNTAX_ERR: { res = "SYNTAX_ERR"; break; }
		case INVALID_MODIFICATION_ERR: { res = "INVALID_MODIFICATION_ERR"; break; }
		case NAMESPACE_ERR: { res = "NAMESPACE_ERR"; break; }
		case INVALID_ACCESS_ERR: { res = "INVALID_ACCESS_ERR"; break; }
		default: { res = "UNKNOWN ERROR"; break; }
	}

	kdDebug() << "Caught exception: " << e.code() << " Name: " << res.string() << endl;
}

void Helper::PrintNode(QTextStream &ret, const Node &node, bool, const QString &indentStr, unsigned short)
{
	LSSerializerImpl::PrintNode(ret, static_cast<NodeImpl *>(node.handle()), indentStr);
}

DOMString Helper::ResolveURI(const DOMString &relative, const DOMString &base)
{
#ifndef APPLE_COMPILE_HACK
	return DOMString(KURL::relativeURL(KURL(base.string()), KURL(relative.string())));
#else
	//FIXME: this is ugly & broken.
	return relative;
#endif
}

bool Helper::IsValidNCName(const DOMString &data)
{
	const QChar *unicode = data.unicode();
	QChar ch = unicode[0];

	/* NCName   ::=   (Letter | '_') (NCNameChar)* */
	if(!ch.isLetter() && ch != '_')
		return false;
		
#ifndef APPLE_COMPILE_HACK
	/* NCNameChar   ::=   Letter | Digit | '.' | '-' | '_' | CombiningChar | Extender */
	const unsigned int len = data.length();
	for(unsigned int i = 1; i < len; ++i)
	{
		ch = unicode[i];
	
		if(!ch.isLetter() && !ch.isDigit() && ch != '.' && ch != '-' &&
		   ch != '_' && ch.category() != QChar::Mark_SpacingCombining)
			return false;
	}
#endif

	return true;
}

bool Helper::IsValidQName(const DOMString &data)
{
	const QChar *unicode = data.unicode();
	QChar ch = unicode[0];

	/* QName		::=   (Prefix ':')? LocalPart
	 * Prefix		::=   NCName
	 * LocalPart	::=   NCName
	 */
	if(!ch.isLetter() && ch != '_')
		return false;

#ifndef APPLE_COMPILE_HACK
	bool alreadyHasColon = false;

	const unsigned int len = data.length();
	for(unsigned int i = 1; i < len; ++i)
	{
		ch = unicode[i];
		
		if(!ch.isLetter() && !ch.isDigit() && ch != '.' && ch != '-' &&
		   ch != '_' && ch.category() != QChar::Mark_SpacingCombining)
		{
			if(ch == ':')
			{
				if(alreadyHasColon)
					return false;
				else
					alreadyHasColon = true;
			}
			else
				return false;
		}
	}
#endif

	return true;
}

DOMString Helper::parseURL(const DOMString &url)
{
	DOMStringImpl* i = url.implementation();
	if(!i)
		return DOMString();

	int o = 0;
	int l = i->length();
	while(o < l && (i->unicode()[o] <= ' ')) { o++; l--; }
	while(l > 0 && (i->unicode()[o + l - 1] <= ' ')) l--;

	if(l >= 5 && (i->unicode()[o].lower() == 'u') &&
	   (i->unicode()[o + 1].lower() == 'r') && (i->unicode()[o + 2].lower() == 'l') &&
	    i->unicode()[o + 3].latin1() == '(' && i->unicode()[o + l - 1].latin1() == ')')
	{
		o += 4;
		l -= 5;
	}

	while(o < l && (i->unicode()[o] <= ' ')) { o++; l--; }
	while(l > 0 && (i->unicode()[o + l - 1] <= ' ')) l--;

	if(l >= 2 && i->unicode()[o] == i->unicode()[o + l - 1] &&
	   (i->unicode()[o].latin1() == '\'' || i->unicode()[o].latin1() == '\"'))
	{
		o++;
		l -= 2;
	}

	while(o < l && (i->unicode()[o] <= ' ')) { o++; l--; }
	while(l > 0 && (i->unicode()[o + l - 1] <= ' ')) l--;

	DOMStringImpl *j = new DOMStringImpl(i->unicode() + o,l);

	int nl = 0;
	for(int k = o; k < o + l; k++)
	{
		if(i->unicode()[k].unicode() > '\r')
			j->unicode()[nl++] = i->unicode()[k];
	}

	j->setLength(nl);
	return DOMString(j);
}

// Used only for stringToLengthArray
static Length parseLength(const QChar *s, unsigned int l)
{
	const QChar *last = s + l - 1;
	if(l && *last == QChar('%'))
	{
		// CSS allows one decimal after the point, like 42.2%, but not
		// 42.22%; we ignore the non-integer part for speed/space reasons
		int i = QConstString(s, l).string().findRev('.');
		if(i >= 0 && i < int(l - 1))
			l = i + 1;

		bool ok;
		i = QConstString(s, l-1).string().toInt(&ok);

		if(ok)
			return Length(i, LT_PERCENT);

		// in case of weird constructs like 5*%
		last--; l--;
	}

	if(l == 0)
		return Length(0, LT_VARIABLE);

	if(*last == '*')
	{
		if(last == s)
			return Length(1, LT_RELATIVE);
		else
			return Length(QConstString(s, l - 1).string().toInt(), LT_RELATIVE);
	}

	// should we strip of the non-integer part here also?
	// CSS says no, all important browsers do so, including Mozilla. sigh.

	bool ok;

	// this ugly construct helps in case someone specifies a length as "100."
	int v = (int) QConstString(s, l).string().toFloat(&ok);

	if(ok)
		return Length(v, LT_FIXED);

	return Length(0, LT_VARIABLE);
}

Length *Helper::stringToLengthArray(const DOMString &data, int &len)
{
	QString str(data.unicode(), data.length());
	int pos = 0, pos2 = 0;

	// web authors are so stupid. This is a workaround to fix lists like
	// "1,2px 3 ,4"; make sure not to break percentage or relative widths
	// TODO: what about "auto" ?
	QChar space(' ');
	for(unsigned int i = 0; i < str.length(); i++)
	{
		char cc = str[i].latin1();
		if(cc > '9' || (cc < '0' && cc != '-' && cc != '*' && cc != '%' && cc != '.'))
			str.replace(i, 1, space);
			//str[i] = space;
	}
	
	str = str.simplifyWhiteSpace();
	len = str.contains(' ') + 1;
	
	Length *r = new Length[len];
	int i = 0;
	while((pos2 = str.find(' ', pos)) != -1)
	{
		r[i++] = parseLength(str.unicode()+pos, pos2 - pos);
		pos = pos2+1;
	}

	r[i] = parseLength(str.unicode() + pos, str.length() - pos);
	return r;
}

// vim:ts=4:noet
