/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Portions are Copyright (C) 2002 Netscape Communications Corporation.
 * Other contributors: David Baron <dbaron@fas.harvard.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Alternatively, the document type parsing portions of this file may be used
 * under the terms of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"
#include "html/html_documentimpl.h"

#include "DocumentTypeImpl.h"
#include "Frame.h"
#include "KWQKCookieJar.h"
#include "css/css_stylesheetimpl.h"
#include "css/cssstyleselector.h"
#include "cssproperties.h"
#include "dom/dom_exception.h"
#include "html/html_baseimpl.h"
#include "html/html_headimpl.h"
#include "html/html_imageimpl.h"
#include "html/html_imageimpl.h"
#include "html/htmltokenizer.h"
#include "htmlfactory.h"
#include "htmlnames.h"
#include "khtml_factory.h"
#include "khtml_settings.h"
#include "rendering/render_object.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/xml_tokenizer.h"
#include <kurl.h>
#include <stdlib.h>

// Turn off inlining to avoid warning with newer gcc.
//#undef __inline
//#define __inline
#include "doctypes.cpp"
//#undef __inline

namespace WebCore {

using namespace HTMLNames;

HTMLDocumentImpl::HTMLDocumentImpl(DOMImplementationImpl *_implementation, FrameView *v)
  : DocumentImpl(_implementation, v)
{
    bodyElement = 0;
    htmlElement = 0;
}

HTMLDocumentImpl::~HTMLDocumentImpl()
{
}

ElementImpl* HTMLDocumentImpl::documentElement() const
{
    return static_cast<ElementImpl*>(fastFirstChild());
}

DOMString HTMLDocumentImpl::lastModified() const
{
    if ( frame() )
        return frame()->lastModified();
    return DOMString();
}

DOMString HTMLDocumentImpl::cookie() const
{
    return KWQKCookieJar::cookie(URL());
}

void HTMLDocumentImpl::setCookie( const DOMString & value )
{
    return KWQKCookieJar::setCookie(URL(), m_policyBaseURL.qstring(), value.qstring());
}

void HTMLDocumentImpl::setBody(HTMLElementImpl *_body, int &exceptioncode)
{
    if (!_body) { 
        exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
        return;
    }
    HTMLElementImpl* b = body();
    if (!b)
        documentElement()->appendChild(_body, exceptioncode);
    else
        documentElement()->replaceChild(_body, b, exceptioncode);
}

Tokenizer *HTMLDocumentImpl::createTokenizer()
{
    return new HTMLTokenizer(this, m_view);
}

// --------------------------------------------------------------------------
// not part of the DOM
// --------------------------------------------------------------------------

bool HTMLDocumentImpl::childAllowed( NodeImpl *newChild )
{
    return newChild->hasTagName(htmlTag) || newChild->isCommentNode();
}

ElementImpl *HTMLDocumentImpl::createElement(const DOMString &name, int &exceptioncode)
{
    DOMString lowerName(name.lower());
    if (!isValidName(lowerName)) {
        exceptioncode = DOMException::INVALID_CHARACTER_ERR;
        return 0;
    }
    return HTMLElementFactory::createHTMLElement(AtomicString(lowerName), this, 0, false);
}

static void addItemToMap(HTMLDocumentImpl::NameCountMap& map, const DOMString& name)
{
    if (name.length() == 0)
        return;
 
    HTMLDocumentImpl::NameCountMap::iterator it = map.find(name.impl()); 
    if (it == map.end())
        map.set(name.impl(), 1);
    else
        ++(it->second);
}

static void removeItemFromMap(HTMLDocumentImpl::NameCountMap& map, const DOMString& name)
{
    if (name.length() == 0)
        return;
 
    HTMLDocumentImpl::NameCountMap::iterator it = map.find(name.impl()); 
    if (it == map.end())
        return;

    int oldVal = it->second;
    assert(oldVal != 0);
    int newVal = oldVal - 1;
    if (newVal == 0)
        map.remove(it);
    else
        it->second = newVal;
}

void HTMLDocumentImpl::addNamedItem(const DOMString& name)
{
    addItemToMap(namedItemCounts, name);
}

void HTMLDocumentImpl::removeNamedItem(const DOMString &name)
{ 
    removeItemFromMap(namedItemCounts, name);
}

bool HTMLDocumentImpl::hasNamedItem(const DOMString& name)
{
    return namedItemCounts.get(name.impl()) != 0;
}

void HTMLDocumentImpl::addDocExtraNamedItem(const DOMString& name)
{
    addItemToMap(docExtraNamedItemCounts, name);
}

void HTMLDocumentImpl::removeDocExtraNamedItem(const DOMString& name)
{ 
    removeItemFromMap(docExtraNamedItemCounts, name);
}

bool HTMLDocumentImpl::hasDocExtraNamedItem(const DOMString& name)
{
    return docExtraNamedItemCounts.get(name.impl()) != 0;
}

const int PARSEMODE_HAVE_DOCTYPE        =       (1<<0);
const int PARSEMODE_HAVE_PUBLIC_ID      =       (1<<1);
const int PARSEMODE_HAVE_SYSTEM_ID      =       (1<<2);
const int PARSEMODE_HAVE_INTERNAL       =       (1<<3);

static int parseDocTypePart(const QString& buffer, int index)
{
    while (true) {
        QChar ch = buffer[index];
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
            ++index;
        else if (ch == '-') {
            int tmpIndex=index;
            if (buffer[index+1] == '-' &&
                ((tmpIndex=buffer.find("--", index+2)) != -1))
                index = tmpIndex+2;
            else
                return index;
        }
        else
            return index;
    }
}

static bool containsString(const char* str, const QString& buffer, int offset)
{
    QString startString(str);
    if (offset + startString.length() > buffer.length())
        return false;
    
    QString bufferString = buffer.mid(offset, startString.length()).lower();
    QString lowerStart = startString.lower();

    return bufferString.startsWith(lowerStart);
}

static bool parseDocTypeDeclaration(const QString& buffer,
                                    int* resultFlags,
                                    QString& publicID,
                                    QString& systemID)
{
    bool haveDocType = false;
    *resultFlags = 0;

    // Skip through any comments and processing instructions.
    int index = 0;
    do {
        index = buffer.find('<', index);
        if (index == -1) break;
        QChar nextChar = buffer[index+1];
        if (nextChar == '!') {
            if (containsString("doctype", buffer, index+2)) {
                haveDocType = true;
                index += 9; // Skip "<!DOCTYPE"
                break;
            }
            index = parseDocTypePart(buffer,index);
            index = buffer.find('>', index);
        }
        else if (nextChar == '?')
            index = buffer.find('>', index);
        else
            break;
    } while (index != -1);

    if (!haveDocType)
        return true;
    *resultFlags |= PARSEMODE_HAVE_DOCTYPE;

    index = parseDocTypePart(buffer, index);
    if (!containsString("html", buffer, index))
        return false;
    
    index = parseDocTypePart(buffer, index+4);
    bool hasPublic = containsString("public", buffer, index);
    if (hasPublic) {
        index = parseDocTypePart(buffer, index+6);

        // We've read <!DOCTYPE HTML PUBLIC (not case sensitive).
        // Now we find the beginning and end of the public identifers
        // and system identifiers (assuming they're even present).
        QChar theChar = buffer[index];
        if (theChar != '\"' && theChar != '\'')
            return false;
        
        // |start| is the first character (after the quote) and |end|
        // is the final quote, so there are |end|-|start| characters.
        int publicIDStart = index+1;
        int publicIDEnd = buffer.find(theChar, publicIDStart);
        if (publicIDEnd == -1)
            return false;
        index = parseDocTypePart(buffer, publicIDEnd+1);
        QChar next = buffer[index];
        if (next == '>') {
            // Public identifier present, but no system identifier.
            // Do nothing.  Note that this is the most common
            // case.
        }
        else if (next == '\"' || next == '\'') {
            // We have a system identifier.
            *resultFlags |= PARSEMODE_HAVE_SYSTEM_ID;
            int systemIDStart = index+1;
            int systemIDEnd = buffer.find(next, systemIDStart);
            if (systemIDEnd == -1)
                return false;
            systemID = buffer.mid(systemIDStart, systemIDEnd - systemIDStart);
        }
        else if (next == '[') {
            // We found an internal subset.
            *resultFlags |= PARSEMODE_HAVE_INTERNAL;
        }
        else
            return false; // Something's wrong.

        // We need to trim whitespace off the public identifier.
        publicID = buffer.mid(publicIDStart, publicIDEnd - publicIDStart);
        publicID = publicID.stripWhiteSpace();
        *resultFlags |= PARSEMODE_HAVE_PUBLIC_ID;
    } else {
        if (containsString("system", buffer, index)) {
            // Doctype has a system ID but no public ID
            *resultFlags |= PARSEMODE_HAVE_SYSTEM_ID;
            index = parseDocTypePart(buffer, index+6);
            QChar next = buffer[index];
            if (next != '\"' && next != '\'')
                return false;
            int systemIDStart = index+1;
            int systemIDEnd = buffer.find(next, systemIDStart);
            if (systemIDEnd == -1)
                return false;
            systemID = buffer.mid(systemIDStart, systemIDEnd - systemIDStart);
            index = parseDocTypePart(buffer, systemIDEnd+1);
        }

        QChar nextChar = buffer[index];
        if (nextChar == '[')
            *resultFlags |= PARSEMODE_HAVE_INTERNAL;
        else if (nextChar != '>')
            return false;
    }

    return true;
}

void HTMLDocumentImpl::determineParseMode( const QString &str )
{
    // This code more or less mimics Mozilla's implementation (specifically the
    // doctype parsing implemented by David Baron in Mozilla's nsParser.cpp).
    //
    // There are three possible parse modes:
    // COMPAT - quirks mode emulates WinIE
    // and NS4.  CSS parsing is also relaxed in this mode, e.g., unit types can
    // be omitted from numbers.
    // ALMOST STRICT - This mode is identical to strict mode
    // except for its treatment of line-height in the inline box model.  For
    // now (until the inline box model is re-written), this mode is identical
    // to STANDARDS mode.
    // STRICT - no quirks apply.  Web pages will obey the specifications to
    // the letter.

    QString systemID, publicID;
    int resultFlags = 0;
    if (parseDocTypeDeclaration(str, &resultFlags, publicID, systemID)) {
        if (resultFlags & PARSEMODE_HAVE_DOCTYPE)
            setDocType(new DocumentTypeImpl(this, "HTML", publicID, systemID));
        if (!(resultFlags & PARSEMODE_HAVE_DOCTYPE)) {
            // No doctype found at all.  Default to quirks mode and Html4.
            pMode = Compat;
            hMode = Html4;
        }
        else if ((resultFlags & PARSEMODE_HAVE_INTERNAL) ||
                 !(resultFlags & PARSEMODE_HAVE_PUBLIC_ID)) {
            // Internal subsets always denote full standards, as does
            // a doctype without a public ID.
            pMode = Strict;
            hMode = Html4;
        }
        else {
            // We have to check a list of public IDs to see what we
            // should do.
            QString lowerPubID = publicID.lower();
            const char* pubIDStr = lowerPubID.latin1();
           
            // Look up the entry in our gperf-generated table.
            const PubIDInfo* doctypeEntry = findDoctypeEntry(pubIDStr, publicID.length());
            if (!doctypeEntry) {
                // The DOCTYPE is not in the list.  Assume strict mode.
                pMode = Strict;
                hMode = Html4;
                return;
            }

            switch ((resultFlags & PARSEMODE_HAVE_SYSTEM_ID) ?
                    doctypeEntry->mode_if_sysid :
                    doctypeEntry->mode_if_no_sysid)
            {
                case PubIDInfo::eQuirks3:
                    pMode = Compat;
                    hMode = Html3;
                    break;
                case PubIDInfo::eQuirks:
                    pMode = Compat;
                    hMode = Html4;
                    break;
                case PubIDInfo::eAlmostStandards:
                    pMode = AlmostStrict;
                    hMode = Html4;
                    break;
                 default:
                    assert(false);
            }
        }   
    }
    else {
        // Malformed doctype implies quirks mode.
        pMode = Compat;
        hMode = Html3;
    }
  
    m_styleSelector->strictParsing = !inCompatMode();
 
}

DocumentTypeImpl *HTMLDocumentImpl::doctype() const
{
    // According to a comment in dom_doc.cpp, doctype is null for HTML documents.
    return 0;
}

}
