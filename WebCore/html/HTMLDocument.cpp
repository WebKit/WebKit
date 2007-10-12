/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "HTMLDocument.h"

#include "CSSPropertyNames.h"
#include "CSSStyleSelector.h"
#include "CString.h"
#include "CookieJar.h"
#include "DocumentLoader.h"
#include "DocumentType.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLBodyElement.h"
#include "HTMLElement.h"
#include "HTMLElementFactory.h"
#include "HTMLNames.h"
#include "HTMLTokenizer.h"
#include "InspectorController.h"
#include "KURL.h"
#include "Page.h"

#include "DocTypeStrings.cpp"

namespace WebCore {

using namespace HTMLNames;

HTMLDocument::HTMLDocument(DOMImplementation* implementation, Frame* frame)
    : Document(implementation, frame)
{
    clearXMLVersion();
}

HTMLDocument::~HTMLDocument()
{
}

int HTMLDocument::width()
{
    updateLayoutIgnorePendingStylesheets();
    FrameView* frameView = view();
    return frameView ? frameView->contentsWidth() : 0;
}

int HTMLDocument::height()
{
    updateLayoutIgnorePendingStylesheets();
    FrameView* frameView = view();
    return frameView ? frameView->contentsHeight() : 0;
}

String HTMLDocument::dir()
{
    HTMLElement* b = body();
    if (!b)
        return String();
    return b->dir();
}

void HTMLDocument::setDir(const String& value)
{
    HTMLElement* b = body();
    if (b)
        b->setDir(value);
}

String HTMLDocument::designMode() const
{
    return inDesignMode() ? "on" : "off";
}

void HTMLDocument::setDesignMode(const String& value)
{
    InheritedBool mode;
    if (equalIgnoringCase(value, "on"))
        mode = on;
    else if (equalIgnoringCase(value, "off"))
        mode = off;
    else
        mode = inherit;
    Document::setDesignMode(mode);
}

String HTMLDocument::compatMode() const
{
    return inCompatMode() ? "BackCompat" : "CSS1Compat";
}

String HTMLDocument::bgColor()
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (!bodyElement)
        return String();
    return bodyElement->bgColor();
}

void HTMLDocument::setBgColor(const String& value)
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (bodyElement)
        bodyElement->setBgColor(value);
}

String HTMLDocument::fgColor()
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (!bodyElement)
        return String();
    return bodyElement->text();
}

void HTMLDocument::setFgColor(const String& value)
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (bodyElement)
        bodyElement->setText(value);
}

String HTMLDocument::alinkColor()
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (!bodyElement)
        return String();
    return bodyElement->aLink();
}

void HTMLDocument::setAlinkColor(const String& value)
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (bodyElement) {
        // This check is a bit silly, but some benchmarks like to set the
        // document's link colors over and over to the same value and we
        // don't want to incur a style update each time.
        if (bodyElement->aLink() != value)
            bodyElement->setALink(value);
    }
}

String HTMLDocument::linkColor()
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (!bodyElement)
        return String();
    return bodyElement->link();
}

void HTMLDocument::setLinkColor(const String& value)
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (bodyElement) {
        // This check is a bit silly, but some benchmarks like to set the
        // document's link colors over and over to the same value and we
        // don't want to incur a style update each time.
        if (bodyElement->link() != value)
            bodyElement->setLink(value);
    }
}

String HTMLDocument::vlinkColor()
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (!bodyElement)
        return String();
    return bodyElement->vLink();
}

void HTMLDocument::setVlinkColor(const String& value)
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (bodyElement) {
        // This check is a bit silly, but some benchmarks like to set the
        // document's link colors over and over to the same value and we
        // don't want to incur a style update each time.
        if (bodyElement->vLink() != value)
            bodyElement->setVLink(value);
    }
}

void HTMLDocument::captureEvents()
{
}

void HTMLDocument::releaseEvents()
{
}

Tokenizer *HTMLDocument::createTokenizer()
{
    bool reportErrors = false;
    if (frame())
        if (Page* page = frame()->page())
            reportErrors = page->inspectorController()->windowVisible();

    return new HTMLTokenizer(this, reportErrors);
}

// --------------------------------------------------------------------------
// not part of the DOM
// --------------------------------------------------------------------------

bool HTMLDocument::childAllowed(Node *newChild)
{
    return newChild->hasTagName(htmlTag) || newChild->isCommentNode();
}

PassRefPtr<Element> HTMLDocument::createElement(const String &name, ExceptionCode& ec)
{
    String lowerName(name.lower());
    if (!isValidName(lowerName)) {
        ec = INVALID_CHARACTER_ERR;
        return 0;
    }
    return HTMLElementFactory::createHTMLElement(AtomicString(lowerName), this, 0, false);
}

static void addItemToMap(HTMLDocument::NameCountMap& map, const String& name)
{
    if (name.length() == 0)
        return;
 
    HTMLDocument::NameCountMap::iterator it = map.find(name.impl()); 
    if (it == map.end())
        map.set(name.impl(), 1);
    else
        ++(it->second);
}

static void removeItemFromMap(HTMLDocument::NameCountMap& map, const String& name)
{
    if (name.length() == 0)
        return;
 
    HTMLDocument::NameCountMap::iterator it = map.find(name.impl()); 
    if (it == map.end())
        return;

    int oldVal = it->second;
    ASSERT(oldVal != 0);
    int newVal = oldVal - 1;
    if (newVal == 0)
        map.remove(it);
    else
        it->second = newVal;
}

void HTMLDocument::addNamedItem(const String& name)
{
    addItemToMap(namedItemCounts, name);
}

void HTMLDocument::removeNamedItem(const String &name)
{ 
    removeItemFromMap(namedItemCounts, name);
}

bool HTMLDocument::hasNamedItem(const String& name)
{
    return namedItemCounts.get(name.impl()) != 0;
}

void HTMLDocument::addDocExtraNamedItem(const String& name)
{
    addItemToMap(docExtraNamedItemCounts, name);
}

void HTMLDocument::removeDocExtraNamedItem(const String& name)
{ 
    removeItemFromMap(docExtraNamedItemCounts, name);
}

bool HTMLDocument::hasDocExtraNamedItem(const String& name)
{
    return docExtraNamedItemCounts.get(name.impl()) != 0;
}

const int PARSEMODE_HAVE_DOCTYPE        =       (1<<0);
const int PARSEMODE_HAVE_PUBLIC_ID      =       (1<<1);
const int PARSEMODE_HAVE_SYSTEM_ID      =       (1<<2);
const int PARSEMODE_HAVE_INTERNAL       =       (1<<3);

static int parseDocTypePart(const String& buffer, int index)
{
    while (true) {
        UChar ch = buffer[index];
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

static bool containsString(const char* str, const String& buffer, int offset)
{
    String startString(str);
    if (offset + startString.length() > buffer.length())
        return false;
    
    String bufferString = buffer.substring(offset, startString.length()).lower();
    String lowerStart = startString.lower();

    return bufferString.startsWith(lowerStart);
}

static bool parseDocTypeDeclaration(const String& buffer,
                                    int* resultFlags,
                                    String& name,
                                    String& publicID,
                                    String& systemID)
{
    bool haveDocType = false;
    *resultFlags = 0;

    // Skip through any comments and processing instructions.
    int index = 0;
    do {
        index = buffer.find('<', index);
        if (index == -1) break;
        UChar nextChar = buffer[index+1];
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
    
    name = buffer.substring(index, 4);
    index = parseDocTypePart(buffer, index+4);
    bool hasPublic = containsString("public", buffer, index);
    if (hasPublic) {
        index = parseDocTypePart(buffer, index+6);

        // We've read <!DOCTYPE HTML PUBLIC (not case sensitive).
        // Now we find the beginning and end of the public identifers
        // and system identifiers (assuming they're even present).
        UChar theChar = buffer[index];
        if (theChar != '\"' && theChar != '\'')
            return false;
        
        // |start| is the first character (after the quote) and |end|
        // is the final quote, so there are |end|-|start| characters.
        int publicIDStart = index+1;
        int publicIDEnd = buffer.find(theChar, publicIDStart);
        if (publicIDEnd == -1)
            return false;
        index = parseDocTypePart(buffer, publicIDEnd+1);
        UChar next = buffer[index];
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
            systemID = buffer.substring(systemIDStart, systemIDEnd - systemIDStart);
        }
        else if (next == '[') {
            // We found an internal subset.
            *resultFlags |= PARSEMODE_HAVE_INTERNAL;
        }
        else
            return false; // Something's wrong.

        // We need to trim whitespace off the public identifier.
        publicID = buffer.substring(publicIDStart, publicIDEnd - publicIDStart);
        publicID = publicID.stripWhiteSpace();
        *resultFlags |= PARSEMODE_HAVE_PUBLIC_ID;
    } else {
        if (containsString("system", buffer, index)) {
            // Doctype has a system ID but no public ID
            *resultFlags |= PARSEMODE_HAVE_SYSTEM_ID;
            index = parseDocTypePart(buffer, index+6);
            UChar next = buffer[index];
            if (next != '\"' && next != '\'')
                return false;
            int systemIDStart = index+1;
            int systemIDEnd = buffer.find(next, systemIDStart);
            if (systemIDEnd == -1)
                return false;
            systemID = buffer.substring(systemIDStart, systemIDEnd - systemIDStart);
            index = parseDocTypePart(buffer, systemIDEnd+1);
        }

        UChar nextChar = buffer[index];
        if (nextChar == '[')
            *resultFlags |= PARSEMODE_HAVE_INTERNAL;
        else if (nextChar != '>')
            return false;
    }

    return true;
}

void HTMLDocument::determineParseMode(const String& str)
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

    String name, systemID, publicID;
    int resultFlags = 0;
    if (parseDocTypeDeclaration(str, &resultFlags, name, publicID, systemID)) {
        if (resultFlags & PARSEMODE_HAVE_DOCTYPE)
            setDocType(new DocumentType(this, name, publicID, systemID));
        if (!(resultFlags & PARSEMODE_HAVE_DOCTYPE)) {
            // No doctype found at all.  Default to quirks mode and Html4.
            setParseMode(Compat);
            setHTMLMode(Html4);
        }
        else if ((resultFlags & PARSEMODE_HAVE_INTERNAL) ||
                 !(resultFlags & PARSEMODE_HAVE_PUBLIC_ID)) {
            // Internal subsets always denote full standards, as does
            // a doctype without a public ID.
            setParseMode(Strict);
            setHTMLMode(Html4);
        }
        else {
            // We have to check a list of public IDs to see what we
            // should do.
            String lowerPubID = publicID.lower();
            CString pubIDStr = lowerPubID.latin1();
           
            // Look up the entry in our gperf-generated table.
            const PubIDInfo* doctypeEntry = findDoctypeEntry(pubIDStr.data(), pubIDStr.length());
            if (!doctypeEntry) {
                // The DOCTYPE is not in the list.  Assume strict mode.
                setParseMode(Strict);
                setHTMLMode(Html4);
                return;
            }

            switch ((resultFlags & PARSEMODE_HAVE_SYSTEM_ID) ?
                    doctypeEntry->mode_if_sysid :
                    doctypeEntry->mode_if_no_sysid)
            {
                case PubIDInfo::eQuirks3:
                    setParseMode(Compat);
                    setHTMLMode(Html3);
                    break;
                case PubIDInfo::eQuirks:
                    setParseMode(Compat);
                    setHTMLMode(Html4);
                    break;
                case PubIDInfo::eAlmostStandards:
                    setParseMode(AlmostStrict);
                    setHTMLMode(Html4);
                    break;
                 default:
                    ASSERT(false);
            }
        }   
    }
    else {
        // Malformed doctype implies quirks mode.
        setParseMode(Compat);
        setHTMLMode(Html3);
    }
  
    styleSelector()->strictParsing = !inCompatMode();
 
}
    
}
