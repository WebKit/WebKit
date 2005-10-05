/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#include "config.h"
#include <kurl.h>

#include <qtextstream.h>

#include "kdom.h"
#include <kdom/Helper.h>
#include "AttrImpl.h"
#include "Namespace.h"
#include "DOMString.h"
#include "RenderStyleDefs.h"
#include "LSSerializerImpl.h"

using namespace KDOM;

void Helper::SplitPrefixLocalName(DOMStringImpl *qualifiedName, DOMStringImpl *&prefix, DOMStringImpl *&localName, int colonPos)
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
        prefix = qualifiedName->copy();
        localName = prefix->split(colonPos + 1);
        prefix->truncate(colonPos);
    }
    else
        localName = qualifiedName->copy();
}

void Helper::CheckPrefix(DOMStringImpl *prefixImpl, DOMStringImpl *, DOMStringImpl *namespaceURIImpl)
{
    DOMString prefix(prefixImpl);
    DOMString namespaceURI(namespaceURIImpl);

    // INVALID_CHARACTER_ERR: Raised if the specified prefix contains an illegal character.
    if(!Helper::ValidatePrefix(prefixImpl))
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
    if(IsMalformedPrefix(prefixImpl) || !namespaceURIImpl ||
       (prefix[0] == 'x' && prefix[1] == 'm' && prefix[2] == 'l' && namespaceURI != NS_XML) ||
       (prefix[0] == 'x' && prefix[1] == 'm' && prefix[2] == 'l' && prefix[3] == 'n' &&
        prefix[4] == 's' && namespaceURI != NS_XMLNS))
    {
        throw new DOMExceptionImpl(NAMESPACE_ERR);
    }
}

void Helper::CheckQualifiedName(DOMStringImpl *qualifiedNameImpl, DOMStringImpl *namespaceURIImpl, int &colonPos, bool nameCanBeNull, bool nameCanBeEmpty)
{
    DOMString qualifiedName(qualifiedNameImpl);
    DOMString namespaceURI(namespaceURIImpl);

    // NAMESPACE_ERR: Raised if no qualifiedName supplied (not mentioned in the spec!) or when it's malformed
    if(!nameCanBeNull && !qualifiedNameImpl)
        throw new DOMExceptionImpl(NAMESPACE_ERR);

    // INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
    if(qualifiedNameImpl && !ValidateQualifiedName(qualifiedNameImpl) &&
       (!qualifiedNameImpl->isEmpty() || !nameCanBeEmpty))
    {
        throw new DOMExceptionImpl(INVALID_CHARACTER_ERR);
    }

    // NAMESPACE_ERR:
    // - Raised if the qualifiedName is malformed,
    // - if the qualifiedName has a prefix and the namespaceURI is null, or
    // - if the qualifiedName is null and the namespaceURI is different from null
    // - if the qualifiedName has a prefix that is "xml" and the namespaceURI is different
    //   from "http://www.w3.org/XML/1998/namespace" [Namespaces].
    // - if the qualifiedName is "xmlns" and the namespaceURI is different from "http://www.w3.org/2000/xmlns/".
    colonPos = CheckMalformedQualifiedName(qualifiedNameImpl);

    if((colonPos >= 0 && namespaceURI.isNull()) || (qualifiedName.isNull() && !namespaceURI.isNull()) ||
       (colonPos == 3 && qualifiedName[0] == 'x' && qualifiedName[1] == 'm' && qualifiedName[2] == 'l' &&
        namespaceURI != NS_XML) ||
       (colonPos == 5 && qualifiedName[0] == 'x' && qualifiedName[1] == 'm' && qualifiedName[2] == 'l' &&
        qualifiedName[3] == 'n' && qualifiedName[4] == 's' && namespaceURI != NS_XMLNS))
    {
        throw new DOMExceptionImpl(NAMESPACE_ERR);
    }
}

int Helper::CheckMalformedQualifiedName(DOMStringImpl *qualifiedName)
{
    int colonPos = -1;

    QChar *qu = (qualifiedName ? qualifiedName->unicode() : 0);
    unsigned int len = (qualifiedName ? qualifiedName->length() : 0);
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

bool Helper::IsMalformedPrefix(DOMStringImpl *prefixImpl)
{
    // TODO : find out definition of malformed prefix // don't forget the API docs
    DOMString prefix(prefixImpl);
    return (prefix.find(':') != -1);
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
    if(!name || name->isEmpty())
        return false;

    QChar *unicode = name->unicode();
    QChar ch = unicode[0];
    if(!ch.isLetter() && ch != '_' && ch != ':')
        return false; // first char isn't valid

#ifndef APPLE_COMPILE_HACK
    unsigned int len = name->length();
    for(unsigned int i = 1; i < len; ++i)
    {
        ch = unicode[i];
        if(!ch.isLetter() && !ch.isDigit() && ch != '.' && ch != '-' && ch != '_' && ch != ':' &&
           ch.category() != QChar::Mark_SpacingCombining) // no idea what "extender is"
        {
            return false;
        }
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

void Helper::ShowException(DOMExceptionImpl *e)
{
    if(!e)
        return;

    DOMString res;
    switch(e->code())
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

    kdDebug() << "Caught exception: " << e->code() << " Name: " << res.string() << endl;
}

void Helper::PrintNode(QTextStream &ret, NodeImpl *node, bool, const QString &indentStr, unsigned short)
{
    LSSerializerImpl::PrintNode(ret, node, indentStr);
}

DOMStringImpl *Helper::ResolveURI(DOMStringImpl *relative, DOMStringImpl *base)
{
#ifndef APPLE_COMPILE_HACK
    return new DOMStringImpl(KURL::relativeURL(KURL((base ? base->string() : QString::null)),
                                       KURL((relative ? relative->string() : QString::null))));
#else
    //FIXME: this is ugly & broken.
    return relative;
#endif
}

bool Helper::IsValidNCName(DOMStringImpl *data)
{
    if(!data)
        return false;

        
#ifndef APPLE_COMPILE_HACK
    QChar ch = unicode[0];

    /* NCName   ::=   (Letter | '_') (NCNameChar)* */
    if(!ch.isLetter() && ch != '_')
        return false;

    /* NCNameChar   ::=   Letter | Digit | '.' | '-' | '_' | CombiningChar | Extender */
    const unsigned int len = data->length();
    for(unsigned int i = 1; i < len; ++i)
    {
        ch = unicode[i];
        
        if(!ch.isLetter() && !ch.isDigit() && ch != '.' && ch != '-' &&
           ch != '_' && ch.category() != QChar::Mark_SpacingCombining)
        {
            return false;
        }
    }
#endif

    return true;
}

bool Helper::IsValidQName(DOMStringImpl *data)
{
    if(!data)
        return false;

    const QChar *unicode = data->unicode();
    QChar ch = unicode[0];

#ifndef APPLE_COMPILE_HACK
    /* QName        ::=   (Prefix ':')? LocalPart
     * Prefix        ::=   NCName
     * LocalPart    ::=   NCName
     */
    if(!ch.isLetter() && ch != '_')
        return false;

    bool alreadyHasColon = false;

    const unsigned int len = data->length();
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

                alreadyHasColon = true;
            }
            else
                return false;
        }
    }
#endif

    return true;
}

DOMStringImpl *Helper::parseURL(DOMStringImpl *url)
{
    if(!url)
        return 0;

    int o = 0;
    int l = url->length();
    while(o < l && (url->unicode()[o] <= ' ')) { o++; l--; }
    while(l > 0 && (url->unicode()[o + l - 1] <= ' ')) l--;

    if(l >= 5 && (url->unicode()[o].lower() == 'u') &&
       (url->unicode()[o + 1].lower() == 'r') && (url->unicode()[o + 2].lower() == 'l') &&
        url->unicode()[o + 3].latin1() == '(' && url->unicode()[o + l - 1].latin1() == ')')
    {
        o += 4;
        l -= 5;
    }

    while(o < l && (url->unicode()[o] <= ' ')) { o++; l--; }
    while(l > 0 && (url->unicode()[o + l - 1] <= ' ')) l--;

    if(l >= 2 && url->unicode()[o] == url->unicode()[o + l - 1] &&
       (url->unicode()[o].latin1() == '\'' || url->unicode()[o].latin1() == '\"'))
    {
        o++;
        l -= 2;
    }

    while(o < l && (url->unicode()[o] <= ' ')) { o++; l--; }
    while(l > 0 && (url->unicode()[o + l - 1] <= ' ')) l--;

    DOMStringImpl *j = new DOMStringImpl(url->unicode() + o,l);

    int nl = 0;
    for(int k = o; k < o + l; k++)
    {
        if(url->unicode()[k].unicode() > '\r')
            j->unicode()[nl++] = url->unicode()[k];
    }

    j->setLength(nl);
    return j;
}

// Internal helper for stringToLengthArray / stringToCoordsArray
static Length parseLength(const QChar *s, unsigned int l)
{
    if(l == 0)
        return Length(1, LT_RELATIVE);

    unsigned i = 0;
    while(i < l && s[i].isSpace())
        ++i;

    if(i < l && (s[i] == '+' || s[i] == '-'))
        ++i;

    while(i < l && s[i].isDigit())
        ++i;

    bool ok;
    int r = QConstString(s, i).string().toInt(&ok);

    /* Skip over any remaining digits, we are not that accurate (5.5% => 5%) */
    while(i < l && (s[i].isDigit() || s[i] == '.'))
        ++i;

    /* IE Quirk: Skip any whitespace (20 % => 20%) */
    while(i < l && s[i].isSpace())
        ++i;

    if(ok)
    {
        if(i == l)
            return Length(r, LT_FIXED);
        else
        {
            const QChar *next = s + i;

            if(*next == '%')
                return Length(r, LT_PERCENT);

            if(*next == '*')
                return Length(r, LT_RELATIVE);
        }

        return Length(r, LT_FIXED);
    }
    else
    {
        if(i < l)
        {
            const QChar *next = s + i;

            if(*next == '*')
                return Length(1, LT_RELATIVE);

            if(*next == '%')
                return Length(1, LT_RELATIVE);
        }
    }

    return Length(0, LT_RELATIVE);
}

Length *Helper::stringToLengthArray(DOMStringImpl *data, int &len)
{
    if(!data)
        return 0;

    QString str(data->unicode(), data->length());
    str = str.simplifyWhiteSpace();

    len = str.contains(',') + 1;
    Length *r = new Length[len];

    int i = 0;
    int pos = 0;
    int pos2;

    while((pos2 = str.find(',', pos)) != -1)
    {
        r[i++] = parseLength((QChar *) str.unicode() + pos, pos2 - pos);
        pos = pos2 + 1;
    }

    /* IE Quirk: If the last comma is the last char skip it and reduce len by one */
    if(str.length()-pos > 0)
        r[i] = parseLength((QChar *) str.unicode() + pos, str.length()-pos);
    else
        len--;

    return r;
}

Length *Helper::stringToCoordsArray(DOMStringImpl *data, int &len)
{
    if(!data)
        return 0;

    QChar *dataStr = data->unicode();
    unsigned int dataLen = data->length();

    QString str(dataStr, dataLen);
    for(unsigned int i = 0; i < dataLen; i++)
    {
        QChar cc = dataStr[i];
        if(cc > '9' || (cc < '0' && cc != '-' && cc != '*' && cc != '.'))
            str.replace(i,1," ");
    }

    str = str.simplifyWhiteSpace();

    len = str.contains(' ') + 1;
    Length *r = new Length[len];

    int i = 0;
    int pos = 0;
    int pos2;

    while((pos2 = str.find(' ', pos)) != -1)
    {
        r[i++] = parseLength((QChar *) str.unicode() + pos, pos2 - pos);
        pos = pos2 + 1;
    }

    r[i] = parseLength((QChar *) str.unicode() + pos, str.length() - pos);
    return r;
}

// vim:ts=4:noet
