/*
    This file is part of the KDE libraries

    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)
              (C) 1998 Waldo Bastian (bastian@kde.org)
              (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2004 Apple Computer, Inc.

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
//----------------------------------------------------------------------------
//
// KDE HTML Widget - Tokenizers

//#define TOKEN_DEBUG 1
//#define TOKEN_DEBUG 2

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

//#include <string.h>
#include "html/htmltokenizer.h"
#include "html/html_documentimpl.h"
#include "html/htmlparser.h"

#include "misc/loader.h"

#include "khtmlview.h"
#include "khtml_part.h"
#include "xml/dom_docimpl.h"
#include "css/csshelper.h"
#include "ecma/kjs_proxy.h"
#include <kcharsets.h>
#include <kglobal.h>
#include <ctype.h>
#include <assert.h>
#include <qvariant.h>
#include <kdebug.h>
#include <stdlib.h>

using namespace DOM::HTMLNames;

using DOM::AtomicString;
using DOM::AttributeImpl;
using DOM::DOMString;
using DOM::DOMStringImpl;
using DOM::DocumentImpl;
using DOM::emptyAtom;
using DOM::commentAtom;
using DOM::nullAtom;
using DOM::textAtom;
using DOM::QualifiedName;
using DOM::MappedAttributeImpl;
using DOM::NamedMappedAttrMapImpl;

// turn off inlining to void warning with newer gcc
#undef __inline
#define __inline
#include "kentities.c"
#undef __inline

// #define INSTRUMENT_LAYOUT_SCHEDULING 1

#define TOKENIZER_CHUNK_SIZE  4096

// FIXME: We would like this constant to be 200ms.  Yielding more aggressively results in increased
// responsiveness and better incremental rendering.  It slows down overall page-load on slower machines,
// though, so for now we set a value of 500.
#define TOKENIZER_TIME_DELAY  500

namespace khtml {

static const char commentStart [] = "<!--";
static const char scriptEnd [] = "</script";
static const char xmpEnd [] = "</xmp";
static const char styleEnd [] =  "</style";
static const char textareaEnd [] = "</textarea";
static const char titleEnd [] = "</title";

#define KHTML_ALLOC_QCHAR_VEC( N ) (QChar*) malloc( sizeof(QChar)*( N ) )
#define KHTML_REALLOC_QCHAR_VEC(P, N ) (QChar*) P = realloc(p, sizeof(QChar)*( N ))
#define KHTML_DELETE_QCHAR_VEC( P ) free((char*)( P ))

// Full support for MS Windows extensions to Latin-1.
// Technically these extensions should only be activated for pages
// marked "windows-1252" or "cp1252", but
// in the standard Microsoft way, these extensions infect hundreds of thousands
// of web pages.  Note that people with non-latin-1 Microsoft extensions
// are SOL.
//
// See: http://www.microsoft.com/globaldev/reference/WinCP.asp
//      http://www.bbsinc.com/iso8859.html
//      http://www.obviously.com/
//
// There may be better equivalents

// We need this for entities at least. For non-entity text, we could
// handle this in the text codec.

// To cover non-entity text, I think this function would need to be called
// in more places. There seem to be some places that don't call fixUpChar.

static const ushort windowsLatin1ExtensionArray[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, // 80-87
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F, // 88-8F
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, // 90-97
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178  // 98-9F
};

static ushort mapChar(ushort c)
{
    assert(c >= 0x80 && c <= 0x9F);
    return windowsLatin1ExtensionArray[c - 0x80];
}

static inline void fixUpChar(QChar &c)
{
    ushort code = c.unicode();
    if ((code & ~0x1F) == 0x0080)
        c = mapChar(code);
}

inline bool tagMatch(const char *s1, const QChar *s2, uint length)
{
    for (uint i = 0; i != length; ++i) {
        char c1 = s1[i];
        char uc1 = toupper(c1);
        QChar c2 = s2[i];
        if (c1 != c2 && uc1 != c2)
            return false;
    }
    return true;
}

void Token::addAttribute(DocumentImpl* doc, const AtomicString& attrName, const AtomicString& v)
{
    AttributeImpl* a = 0;
    if (!attrName.isEmpty() && attrName != "/") {
        a = new MappedAttributeImpl(attrName, v);
        if (!attrs) {
            attrs = new NamedMappedAttrMapImpl(0);
            attrs->ref();
        }
        attrs->insertAttribute(a);
    }
}

// ----------------------------------------------------------------------------

HTMLTokenizer::HTMLTokenizer(DOM::DocumentPtr *_doc, KHTMLView *_view, bool includesComments)
    : inWrite(false)
{
    view = _view;
    buffer = 0;
    scriptCode = 0;
    scriptCodeSize = scriptCodeMaxSize = scriptCodeResync = 0;
    charsets = KGlobal::charsets();
    parser = new HTMLParser(_view, _doc, includesComments);
    m_executingScript = 0;
    loadingExtScript = false;
    onHold = false;
    timerId = 0;
    includesCommentsInDOM = includesComments;
    loadStopped = false;
    
    begin();
}

HTMLTokenizer::HTMLTokenizer(DOM::DocumentPtr *_doc, DOM::DocumentFragmentImpl *i, bool includesComments)
    : inWrite(false)
{
    view = 0;
    buffer = 0;
    scriptCode = 0;
    scriptCodeSize = scriptCodeMaxSize = scriptCodeResync = 0;
    charsets = KGlobal::charsets();
    parser = new HTMLParser(i, _doc, includesComments);
    m_executingScript = 0;
    loadingExtScript = false;
    onHold = false;
    timerId = 0;
    includesCommentsInDOM = includesComments;
    loadStopped = false;

    begin();
}

void HTMLTokenizer::reset()
{
    assert(m_executingScript == 0);
    assert(onHold == false);

    while (!cachedScript.isEmpty())
        cachedScript.dequeue()->deref(this);

    if ( buffer )
        KHTML_DELETE_QCHAR_VEC(buffer);
    buffer = dest = 0;
    size = 0;

    if ( scriptCode )
        KHTML_DELETE_QCHAR_VEC(scriptCode);
    scriptCode = 0;
    scriptCodeSize = scriptCodeMaxSize = scriptCodeResync = 0;

    if (timerId) {
        killTimer(timerId);
        timerId = 0;
    }
    timerId = 0;
    allowYield = false;
    forceSynchronous = false;

    currToken.reset();
}

void HTMLTokenizer::begin()
{
    m_executingScript = 0;
    loadingExtScript = false;
    onHold = false;
    reset();
    size = 254;
    buffer = KHTML_ALLOC_QCHAR_VEC( 255 );
    dest = buffer;
    tag = NoTag;
    pending = NonePending;
    discard = NoneDiscard;
    plaintext = false;
    xmp = false;
    processingInstruction = false;
    script = false;
    escaped = false;
    style = false;
    skipLF = false;
    select = false;
    comment = false;
    server = false;
    textarea = false;
    title = false;
    startTag = false;
    tquote = NoQuote;
    searchCount = 0;
    Entity = NoEntity;
    loadingExtScript = false;
    scriptSrc = QString::null;
    pendingSrc.clear();
    currentPrependingSrc = 0;
    noMoreData = false;
    brokenComments = false;
    brokenServer = false;
    lineno = 0;
    scriptStartLineno = 0;
    tagStartLineno = 0;
    forceSynchronous = false;
}

void HTMLTokenizer::setForceSynchronous(bool force)
{
    forceSynchronous = force;
}

void HTMLTokenizer::processListing(TokenizerString list)
{
    // This function adds the listing 'list' as
    // preformatted text-tokens to the token-collection
    while ( !list.isEmpty() )
    {
        checkBuffer();

        if (skipLF && ( *list != '\n' ))
        {
            skipLF = false;
        }

        if (skipLF)
        {
            skipLF = false;
            ++list;
        }
        else if (( *list == '\n' ) || ( *list == '\r' ))
        {
            if (discard == LFDiscard)
            {
                // Ignore this LF
                discard = NoneDiscard; // We have discarded 1 LF
            }
            else
            {
                // Process this LF
                if (pending)
                    addPending();
                pending = LFPending;
            }
            /* Check for MS-DOS CRLF sequence */
            if (*list == '\r')
            {
                skipLF = true;
            }
            ++list;
        }
        else if ( *list == ' ' )
        {
            if (pending)
                addPending();
            pending = SpacePending;

            ++list;
        }
        else
        {
            discard = NoneDiscard;
            if (pending)
                addPending();

            *dest++ = *list;
            ++list;
        }

    }

    if (pending)
        addPending();
}

void HTMLTokenizer::parseSpecial(TokenizerString &src)
{
    assert( textarea || title || !Entity );
    assert( !tag );
    assert( xmp+textarea+title+style+script == 1 );
    if (script)
        scriptStartLineno = lineno+src.lineCount();

    if ( comment ) parseComment( src );

    while ( !src.isEmpty() ) {
        checkScriptBuffer();
        unsigned char ch = src->latin1();
        if ( !scriptCodeResync && !brokenComments && !textarea && !xmp && !title && ch == '-' && scriptCodeSize >= 3 && !src.escaped() && scriptCode[scriptCodeSize-3] == '<' && scriptCode[scriptCodeSize-2] == '!' && scriptCode[scriptCodeSize-1] == '-' ) {
            comment = true;
            parseComment( src );
            continue;
        }
        if ( scriptCodeResync && !tquote && ( ch == '>' ) ) {
            ++src;
            scriptCodeSize = scriptCodeResync-1;
            scriptCodeResync = 0;
            scriptCode[ scriptCodeSize ] = scriptCode[ scriptCodeSize + 1 ] = 0;
            if ( script )
                scriptHandler();
            else {
                processListing(TokenizerString(scriptCode, scriptCodeSize));
                processToken();
                if ( style )         { currToken.tagName = styleTag.localName(); currToken.beginTag = false; }
                else if ( textarea ) { currToken.tagName = textareaTag.localName(); currToken.beginTag = false; }
                else if ( title ) { currToken.tagName = titleTag.localName(); currToken.beginTag = false; }
                else if ( xmp )  { currToken.tagName = xmpTag.localName(); currToken.beginTag = false; }
                processToken();
                style = script = style = textarea = title = xmp = false;
                tquote = NoQuote;
                scriptCodeSize = scriptCodeResync = 0;
            }
            return;
        }
        // possible end of tagname, lets check.
        if ( !scriptCodeResync && !escaped && !src.escaped() && ( ch == '>' || ch == '/' || ch <= ' ' ) && ch &&
             scriptCodeSize >= searchStopperLen &&
             tagMatch( searchStopper, scriptCode+scriptCodeSize-searchStopperLen, searchStopperLen )) {
            scriptCodeResync = scriptCodeSize-searchStopperLen+1;
            tquote = NoQuote;
            continue;
        }
        if ( scriptCodeResync && !escaped ) {
            if(ch == '\"')
                tquote = (tquote == NoQuote) ? DoubleQuote : ((tquote == SingleQuote) ? SingleQuote : NoQuote);
            else if(ch == '\'')
                tquote = (tquote == NoQuote) ? SingleQuote : (tquote == DoubleQuote) ? DoubleQuote : NoQuote;
            else if (tquote != NoQuote && (ch == '\r' || ch == '\n'))
                tquote = NoQuote;
        }
        escaped = ( !escaped && ch == '\\' );
        if (!scriptCodeResync && (textarea||title) && !src.escaped() && ch == '&') {
            QChar *scriptCodeDest = scriptCode+scriptCodeSize;
            ++src;
            parseEntity(src,scriptCodeDest,true);
            scriptCodeSize = scriptCodeDest-scriptCode;
        }
        else {
            fixUpChar(scriptCode[scriptCodeSize] = *src);
            ++scriptCodeSize;
            ++src;
        }
    }
}

void HTMLTokenizer::scriptHandler()
{
    // We are inside a <script>
    bool doScriptExec = false;
    CachedScript* cs = 0;
    // don't load external scripts for standalone documents (for now)
    if (!scriptSrc.isEmpty() && parser->doc()->part()) {
        // forget what we just got; load from src url instead
        if ( !parser->skipMode() ) {
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
            if (!parser->doc()->ownerElement())
                printf("Requesting script at time %d\n", parser->doc()->elapsedTime());
#endif
            if ( (cs = parser->doc()->docLoader()->requestScript(scriptSrc, scriptSrcCharset) ))
                cachedScript.enqueue(cs);
        }
        scriptSrc=QString::null;
    }
    else {
#ifdef TOKEN_DEBUG
        kdDebug( 6036 ) << "---START SCRIPT---" << endl;
        kdDebug( 6036 ) << QString(scriptCode, scriptCodeSize) << endl;
        kdDebug( 6036 ) << "---END SCRIPT---" << endl;
#endif
        // Parse scriptCode containing <script> info
        doScriptExec = true;
    }
    processListing(TokenizerString(scriptCode, scriptCodeSize));
    QString exScript( buffer, dest-buffer );
    processToken();
    currToken.tagName = scriptTag.localName();
    currToken.beginTag = false;
    processToken();

    // Scripts following a frameset element should not be executed or even loaded in the case of extern scripts.
    bool followingFrameset = (parser->doc()->body() && parser->doc()->body()->hasTagName(framesetTag));
    TokenizerString *savedPrependingSrc = currentPrependingSrc;
    TokenizerString prependingSrc;
    currentPrependingSrc = &prependingSrc;
    if (!parser->skipMode() && !followingFrameset) {
        if (cs) {
             //kdDebug( 6036 ) << "cachedscript extern!" << endl;
             //kdDebug( 6036 ) << "src: *" << QString( src.current(), src.length() ).latin1() << "*" << endl;
             //kdDebug( 6036 ) << "pending: *" << pendingSrc.latin1() << "*" << endl;
	    if (savedPrependingSrc) {
		savedPrependingSrc->append(src);
	    } else {
		pendingSrc.prepend(src);
	    }
            setSrc(TokenizerString());
            scriptCodeSize = scriptCodeResync = 0;
            cs->ref(this);
            // will be 0 if script was already loaded and ref() executed it
            if (!cachedScript.isEmpty())
                loadingExtScript = true;
        }
        else if (view && doScriptExec && javascript ) {
            if (!m_executingScript)
                pendingSrc.prepend(src);
            else
                prependingSrc = src;
            setSrc(TokenizerString());
            scriptCodeSize = scriptCodeResync = 0;
            //QTime dt;
            //dt.start();
            scriptExecution( exScript, QString::null, scriptStartLineno );
	    //kdDebug( 6036 ) << "script execution time:" << dt.elapsed() << endl;
        }
    }

    script = false;
    scriptCodeSize = scriptCodeResync = 0;

    if ( !m_executingScript && !loadingExtScript ) {
	// kdDebug( 6036 ) << "adding pending Output to parsed string" << endl;
	src.append(pendingSrc);
	pendingSrc.clear();
    } else if (!prependingSrc.isEmpty()) {
	// restore first so that the write appends in the right place
	// (does not hurt to do it again below)
	currentPrependingSrc = savedPrependingSrc;

	// we need to do this slightly modified bit of one of the write() cases
	// because we want to prepend to pendingSrc rather than appending
	// if there's no previous prependingSrc
	if (loadingExtScript) {
	    if (currentPrependingSrc) {
		currentPrependingSrc->append(prependingSrc);
	    } else {
		pendingSrc.prepend(prependingSrc);
	    }
	} else {
	    write(prependingSrc, false);
	}
    }

    currentPrependingSrc = savedPrependingSrc;
}

void HTMLTokenizer::scriptExecution( const QString& str, QString scriptURL,
                                     int baseLine)
{
#if APPLE_CHANGES
    if (!view || !view->part())
        return;
#endif
    bool oldscript = script;
    m_executingScript++;
    script = false;
    QString url;    
    if (scriptURL.isNull())
      url = view->part()->xmlDocImpl()->URL();
    else
      url = scriptURL;

    TokenizerString *savedPrependingSrc = currentPrependingSrc;
    TokenizerString prependingSrc;
    currentPrependingSrc = &prependingSrc;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!parser->doc()->ownerElement())
        printf("beginning script execution at %d\n", parser->doc()->elapsedTime());
#endif

    view->part()->executeScript(url,baseLine,0,str);

    allowYield = true;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!parser->doc()->ownerElement())
        printf("ending script execution at %d\n", parser->doc()->elapsedTime());
#endif
    
    m_executingScript--;
    script = oldscript;

    if ( !m_executingScript && !loadingExtScript ) {
	// kdDebug( 6036 ) << "adding pending Output to parsed string" << endl;
	src.append(pendingSrc);
	pendingSrc.clear();
    } else if (!prependingSrc.isEmpty()) {
	// restore first so that the write appends in the right place
	// (does not hurt to do it again below)
	currentPrependingSrc = savedPrependingSrc;

	// we need to do this slightly modified bit of one of the write() cases
	// because we want to prepend to pendingSrc rather than appending
	// if there's no previous prependingSrc
	if (loadingExtScript) {
	    if (currentPrependingSrc) {
		currentPrependingSrc->append(prependingSrc);
	    } else {
		pendingSrc.prepend(prependingSrc);
	    }
	} else {
	    write(prependingSrc, false);
	}
    }

    currentPrependingSrc = savedPrependingSrc;
}

void HTMLTokenizer::parseComment(TokenizerString &src)
{
    // FIXME: Why does this code even run for comments inside <script> and <style>? This seems bogus.
    bool strict = !parser->doc()->inCompatMode() && !script && !style;
    int delimiterCount = 0;
    bool canClose = false;
    checkScriptBuffer(src.length());
    while ( !src.isEmpty() ) {
        scriptCode[ scriptCodeSize++ ] = *src;
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 1
        qDebug("comment is now: *%s*",
               QConstString((QChar*)src.operator->(), kMin(16U, src.length())).qstring().latin1());
#endif

        if (strict) {
            if (src->unicode() == '-') {
                delimiterCount++;
                if (delimiterCount == 2) {
                    delimiterCount = 0;
                    canClose = !canClose;
                }
            }
            else
                delimiterCount = 0;
        }

        if ((!strict || canClose) && src->unicode() == '>') {
            bool handleBrokenComments = brokenComments && !(script || style);
            int endCharsCount = 1; // start off with one for the '>' character
            if (!strict) {
                // In quirks mode just check for -->
                if (scriptCodeSize > 2 && scriptCode[scriptCodeSize-3] == '-' && scriptCode[scriptCodeSize-2] == '-') {
                    endCharsCount = 3;
                }
                else if (scriptCodeSize > 3 && scriptCode[scriptCodeSize-4] == '-' && scriptCode[scriptCodeSize-3] == '-' && 
                    scriptCode[scriptCodeSize-2] == '!') {
                    // Other browsers will accept --!> as a close comment, even though it's
                    // not technically valid.
                    endCharsCount = 4;
                }
            }
            if (canClose || handleBrokenComments || endCharsCount > 1) {
                ++src;
                if (!( script || xmp || textarea || style)) {
                    if (includesCommentsInDOM) {
                        checkScriptBuffer();
                        scriptCode[ scriptCodeSize ] = 0;
                        scriptCode[ scriptCodeSize + 1 ] = 0;
                        currToken.tagName = commentAtom;
                        currToken.beginTag = true;
                        processListing(TokenizerString(scriptCode, scriptCodeSize - endCharsCount));
                        processToken();
                        currToken.tagName = commentAtom;
                        currToken.beginTag = false;
                        processToken();
                    }
                    scriptCodeSize = 0;
                }
                comment = false;
                return; // Finished parsing comment
            }
        }
        ++src;
    }
}

void HTMLTokenizer::parseServer(TokenizerString &src)
{
    checkScriptBuffer(src.length());
    while ( !src.isEmpty() ) {
        scriptCode[ scriptCodeSize++ ] = *src;
        if (src->unicode() == '>' &&
            scriptCodeSize > 1 && scriptCode[scriptCodeSize-2] == '%') {
            ++src;
            server = false;
            scriptCodeSize = 0;
            return; // Finished parsing server include
        }
        ++src;
    }
}

void HTMLTokenizer::parseProcessingInstruction(TokenizerString &src)
{
    char oldchar = 0;
    while ( !src.isEmpty() )
    {
        unsigned char chbegin = src->latin1();
        if(chbegin == '\'') {
            tquote = tquote == SingleQuote ? NoQuote : SingleQuote;
        }
        else if(chbegin == '\"') {
            tquote = tquote == DoubleQuote ? NoQuote : DoubleQuote;
        }
        // Look for '?>'
        // some crappy sites omit the "?" before it, so
        // we look for an unquoted '>' instead. (IE compatible)
        else if ( chbegin == '>' && ( !tquote || oldchar == '?' ) )
        {
            // We got a '?>' sequence
            processingInstruction = false;
            ++src;
            discard=LFDiscard;
            return; // Finished parsing comment!
        }
        ++src;
        oldchar = chbegin;
    }
}

void HTMLTokenizer::parseText(TokenizerString &src)
{
    while ( !src.isEmpty() )
    {
        // do we need to enlarge the buffer?
        checkBuffer();

        // ascii is okay because we only do ascii comparisons
        unsigned char chbegin = src->latin1();

        if (skipLF && ( chbegin != '\n' ))
        {
            skipLF = false;
        }

        if (skipLF)
        {
            skipLF = false;
            ++src;
        }
        else if (( chbegin == '\n' ) || ( chbegin == '\r' ))
        {
            if (chbegin == '\r')
                skipLF = true;

            *dest++ = '\n';
            ++src;
        }
        else {
            *dest = *src;
            fixUpChar(*dest);
            ++dest;
            ++src;
        }
    }
}


void HTMLTokenizer::parseEntity(TokenizerString &src, QChar *&dest, bool start)
{
    if( start )
    {
        cBufferPos = 0;
        Entity = SearchEntity;
        EntityUnicodeValue = 0;
    }

    while( !src.isEmpty() )
    {
        ushort cc = src->unicode();
        switch(Entity) {
        case NoEntity:
            assert(Entity != NoEntity);
            return;
        
        case SearchEntity:
            if(cc == '#') {
                cBuffer[cBufferPos++] = cc;
                ++src;
                Entity = NumericSearch;
            }
            else
                Entity = EntityName;

            break;

        case NumericSearch:
            if(cc == 'x' || cc == 'X') {
                cBuffer[cBufferPos++] = cc;
                ++src;
                Entity = Hexadecimal;
            }
            else if(cc >= '0' && cc <= '9')
                Entity = Decimal;
            else
                Entity = SearchSemicolon;

            break;

        case Hexadecimal:
        {
            int ll = kMin(src.length(), 10-cBufferPos);
            while(ll--) {
                QChar csrc(src->lower());
                cc = csrc.cell();

                if(csrc.row() || !((cc >= '0' && cc <= '9') || (cc >= 'a' && cc <= 'f'))) {
                    Entity = SearchSemicolon;
                    break;
                }
                EntityUnicodeValue = EntityUnicodeValue*16 + (cc - ( cc < 'a' ? '0' : 'a' - 10));
                cBuffer[cBufferPos++] = cc;
                ++src;
            }
            if(cBufferPos == 10)  Entity = SearchSemicolon;
            break;
        }
        case Decimal:
        {
            int ll = kMin(src.length(), 9-cBufferPos);
            while(ll--) {
                cc = src->cell();

                if(src->row() || !(cc >= '0' && cc <= '9')) {
                    Entity = SearchSemicolon;
                    break;
                }

                EntityUnicodeValue = EntityUnicodeValue * 10 + (cc - '0');
                cBuffer[cBufferPos++] = cc;
                ++src;
            }
            if(cBufferPos == 9)  Entity = SearchSemicolon;
            break;
        }
        case EntityName:
        {
            int ll = kMin(src.length(), 9-cBufferPos);
            while(ll--) {
                QChar csrc = *src;
                cc = csrc.cell();

                if(csrc.row() || !((cc >= 'a' && cc <= 'z') ||
                                   (cc >= '0' && cc <= '9') || (cc >= 'A' && cc <= 'Z'))) {
                    Entity = SearchSemicolon;
                    break;
                }

                cBuffer[cBufferPos++] = cc;
                ++src;
            }
            if(cBufferPos == 9) Entity = SearchSemicolon;
            if(Entity == SearchSemicolon) {
                if(cBufferPos > 1) {
                    const entity *e = findEntity(cBuffer, cBufferPos);
                    if(e)
                        EntityUnicodeValue = e->code;

                    // be IE compatible
                    if(tag && EntityUnicodeValue > 255 && *src != ';')
                        EntityUnicodeValue = 0;
                }
            }
            else
                break;
        }
        case SearchSemicolon:

            //kdDebug( 6036 ) << "ENTITY " << EntityUnicodeValue << ", " << res << endl;

            // Don't allow surrogate code points, or values that are more than 21 bits.
            if ((EntityUnicodeValue > 0 && EntityUnicodeValue < 0xD800)
                    || (EntityUnicodeValue >= 0xE000 && EntityUnicodeValue <= 0x1FFFFF)) {
            
                if (*src == ';')
                    ++src;

                if (EntityUnicodeValue <= 0xFFFF) {
                    QChar c(EntityUnicodeValue);
                    fixUpChar(c);
                    checkBuffer();
                    src.push(c);
                } else {
                    // Convert to UTF-16, using surrogate code points.
                    QChar c1(0xD800 | (((EntityUnicodeValue >> 16) - 1) << 6) | ((EntityUnicodeValue >> 10) & 0x3F));
                    QChar c2(0xDC00 | (EntityUnicodeValue & 0x3FF));
                    checkBuffer(2);
                    src.push(c1);
                    src.push(c2);
                }

            } else {
#ifdef TOKEN_DEBUG
                kdDebug( 6036 ) << "unknown entity!" << endl;
#endif
                checkBuffer(10);
                // ignore the sequence, add it to the buffer as plaintext
                *dest++ = '&';
                for(unsigned int i = 0; i < cBufferPos; i++)
                    dest[i] = cBuffer[i];
                dest += cBufferPos;
            }

            Entity = NoEntity;
            return;
        }
    }
}

void HTMLTokenizer::parseTag(TokenizerString &src)
{
    assert(!Entity );

    while ( !src.isEmpty() )
    {
        checkBuffer();
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 1
        uint l = 0;
        while(l < src.length() && (*(src.operator->()+l)).latin1() != '>')
            l++;
        qDebug("src is now: *%s*, tquote: %d",
               QConstString((QChar*)src.operator->(), l).qstring().latin1(), tquote);
#endif
        switch(tag) {
        case NoTag:
        {
            return;
        }
        case TagName:
        {
#if defined(TOKEN_DEBUG) &&  TOKEN_DEBUG > 1
            qDebug("TagName");
#endif
            if (searchCount > 0)
            {
                if (*src == commentStart[searchCount])
                {
                    searchCount++;
                    if (searchCount == 4)
                    {
#ifdef TOKEN_DEBUG
                        kdDebug( 6036 ) << "Found comment" << endl;
#endif
                        // Found '<!--' sequence
                        ++src;
                        dest = buffer; // ignore the previous part of this tag
                        comment = true;
                        tag = NoTag;

                        // Fix bug 34302 at kde.bugs.org.  Go ahead and treat
                        // <!--> as a valid comment, since both mozilla and IE on windows
                        // can handle this case.  Only do this in quirks mode. -dwh
                        if (!src.isEmpty() && *src == '>' && parser->doc()->inCompatMode()) {
                          comment = false;
                          ++src;
                          if (!src.isEmpty())
                              cBuffer[cBufferPos++] = src->cell();
                        }
		        else
                          parseComment(src);

                        return; // Finished parsing tag!
                    }
                    // cuts of high part, is okay
                    cBuffer[cBufferPos++] = src->cell();
                    ++src;
                    break;
                }
                else
                    searchCount = 0; // Stop looking for '<!--' sequence
            }

            bool finish = false;
            unsigned int ll = kMin(src.length(), CBUFLEN-cBufferPos);
            while(ll--) {
                ushort curchar = *src;
                if(curchar <= ' ' || curchar == '>' ) {
                    finish = true;
                    break;
                }
                
                // tolower() shows up on profiles. This is faster!
                if (curchar >= 'A' && curchar <= 'Z')
                    cBuffer[cBufferPos++] = curchar + ('a' - 'A');
                else
                    cBuffer[cBufferPos++] = curchar;
                ++src;
            }

            // Disadvantage: we add the possible rest of the tag
            // as attribute names. ### judge if this causes problems
            if(finish || CBUFLEN == cBufferPos) {
                bool beginTag;
                char* ptr = cBuffer;
                unsigned int len = cBufferPos;
                cBuffer[cBufferPos] = '\0';
                if ((cBufferPos > 0) && (*ptr == '/'))
                {
                    // End Tag
                    beginTag = false;
                    ptr++;
                    len--;
                }
                else
                    // Start Tag
                    beginTag = true;

                // Ignore the / in fake xml tags like <br/>.  We trim off the "/" so that we'll get "br" as the tag name and not "br/".
                if (len > 1 && ptr[len-1] == '/')
                    ptr[--len] = '\0';

                // Now that we've shaved off any invalid / that might have followed the name), make the tag.
                if (ptr[0] != '!' && strcmp(ptr, "!doctype") != 0) {
                    currToken.tagName = AtomicString(ptr);
                    currToken.beginTag = beginTag;
                }
                dest = buffer;
                tag = SearchAttribute;
                cBufferPos = 0;
            }
            break;
        }
        case SearchAttribute:
        {
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 1
                qDebug("SearchAttribute");
#endif
            bool atespace = false;
            ushort curchar;
            while(!src.isEmpty()) {
                curchar = *src;
                // In this mode just ignore any quotes we encounter and treat them like spaces.
                if (curchar > ' ' && curchar != '\'' && curchar != '"') {
                    if (curchar == '<' || curchar == '>')
                        tag = SearchEnd;
                    else
                        tag = AttributeName;

                    cBufferPos = 0;
                    break;
                }
                atespace = true;
                ++src;
            }
            break;
        }
        case AttributeName:
        {
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 1
                qDebug("AttributeName");
#endif
            ushort curchar;
            int ll = kMin(src.length(), CBUFLEN-cBufferPos);

            while(ll--) {
                curchar = *src;
                if (curchar <= '>' && (curchar >= '=' || curchar <= ' ')) {
                    cBuffer[cBufferPos] = '\0';
                    attrName = AtomicString(cBuffer);
                    dest = buffer;
                    *dest++ = 0;
                    tag = SearchEqual;
                    // This is a deliberate quirk to match Mozilla and Opera.  We have to do this
                    // since sites that use the "standards-compliant" path sometimes send
                    // <script src="foo.js"/>.  Both Moz and Opera will honor this, despite it
                    // being bogus HTML.  They do not honor the "/" for other tags.  This behavior
                    // also deviates from WinIE, but in this case we'll just copy Moz and Opera.
                    if (currToken.tagName == scriptTag && curchar == '>' && attrName == "/")
                        currToken.flat = true;
                    break;
                }
                
                // tolower() shows up on profiles. This is faster!
                if (curchar >= 'A' && curchar <= 'Z')
                    cBuffer[cBufferPos++] = curchar + ('a' - 'A');
                else
                    cBuffer[cBufferPos++] = curchar;
                ++src;
            }
            if ( cBufferPos == CBUFLEN ) {
                cBuffer[cBufferPos] = '\0';
                attrName = AtomicString(cBuffer);
                dest = buffer;
                *dest++ = 0;
                tag = SearchEqual;
            }
            break;
        }
        case SearchEqual:
        {
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 1
                qDebug("SearchEqual");
#endif
            ushort curchar;
            bool atespace = false;
            while(!src.isEmpty()) {
                curchar = src->unicode();
                // In this mode just ignore any quotes we encounter and treat them like spaces.
                if (curchar > ' ' && curchar != '\'' && curchar != '"') {
                    if(curchar == '=') {
#ifdef TOKEN_DEBUG
                        kdDebug(6036) << "found equal" << endl;
#endif
                        tag = SearchValue;
                        ++src;
                    }
                    else {
                        currToken.addAttribute(parser->docPtr()->document(), attrName, emptyAtom);
                        dest = buffer;
                        tag = SearchAttribute;
                    }
                    break;
                }
                atespace = true;
                ++src;
            }
            break;
        }
        case SearchValue:
        {
            ushort curchar;
            while(!src.isEmpty()) {
                curchar = src->unicode();
                if(curchar > ' ') {
                    if(( curchar == '\'' || curchar == '\"' )) {
                        tquote = curchar == '\"' ? DoubleQuote : SingleQuote;
                        tag = QuotedValue;
                        ++src;
                    } else
                        tag = Value;

                    break;
                }
                ++src;
            }
            break;
        }
        case QuotedValue:
        {
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 1
                qDebug("QuotedValue");
#endif
            ushort curchar;
            while(!src.isEmpty()) {
                checkBuffer();

                curchar = src->unicode();
                if (curchar == '>' && attrName.isEmpty()) {
                    // Handle a case like <img '>.  Just go ahead and be willing
                    // to close the whole tag.  Don't consume the character and
                    // just go back into SearchEnd while ignoring the whole
                    // value.
                    // FIXME: Note that this is actually not a very good solution. It's
                    // an interim hack and doesn't handle the general case of
                    // unmatched quotes among attributes that have names. -dwh
                    while(dest > buffer+1 && (*(dest-1) == '\n' || *(dest-1) == '\r'))
                        dest--; // remove trailing newlines
                    AtomicString v(buffer+1, dest-buffer-1);
                    attrName = v; // Just make the name/value match. (FIXME: Is this some WinIE quirk?)
                    currToken.addAttribute(parser->docPtr()->document(), attrName, v);
                    tag = SearchAttribute;
                    dest = buffer;
                    tquote = NoQuote;
                    break;
                }
                
                if(curchar <= '\'' && !src.escaped()) {
                    // ### attributes like '&{blaa....};' are supposed to be treated as jscript.
                    if ( curchar == '&' )
                    {
                        ++src;
                        parseEntity(src, dest, true);
                        break;
                    }
                    else if ( (tquote == SingleQuote && curchar == '\'') ||
                              (tquote == DoubleQuote && curchar == '\"') )
                    {
                        // some <input type=hidden> rely on trailing spaces. argh
                        while(dest > buffer+1 && (*(dest-1) == '\n' || *(dest-1) == '\r'))
                            dest--; // remove trailing newlines
                        AtomicString v(buffer+1, dest-buffer-1);
                        if (attrName.isEmpty())
                            attrName = v; // Make the name match the value. (FIXME: Is this a WinIE quirk?)
                        currToken.addAttribute(parser->docPtr()->document(), attrName, v);

                        dest = buffer;
                        tag = SearchAttribute;
                        tquote = NoQuote;
                        ++src;
                        break;
                    }
                }
                *dest = *src;
                fixUpChar(*dest);
                ++dest;
                ++src;
            }
            break;
        }
        case Value:
        {
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 1
            qDebug("Value");
#endif
            ushort curchar;
            while(!src.isEmpty()) {
                checkBuffer();
                curchar = src->unicode();
                if(curchar <= '>' && !src.escaped()) {
                    // parse Entities
                    if ( curchar == '&' )
                    {
                        ++src;
                        parseEntity(src, dest, true);
                        break;
                    }
                    // no quotes. Every space means end of value
                    // '/' does not delimit in IE!
                    if ( curchar <= ' ' || curchar == '>' )
                    {
                        AtomicString v(buffer+1, dest-buffer-1);
                        currToken.addAttribute(parser->docPtr()->document(), attrName, v);
                        dest = buffer;
                        tag = SearchAttribute;
                        break;
                    }
                }

                *dest = *src;
                fixUpChar(*dest);
                ++dest;
                ++src;
            }
            break;
        }
        case SearchEnd:
        {
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 1
                qDebug("SearchEnd");
#endif
            while(!src.isEmpty()) {
                if (*src == '>' || *src == '<')
                    break;

                if (*src == '/')
                    currToken.flat = true;

                ++src;
            }
            if (src.isEmpty()) break;

            searchCount = 0; // Stop looking for '<!--' sequence
            tag = NoTag;
            tquote = NoQuote;

            if (*src != '<')
                ++src;

            if (currToken.tagName == nullAtom) //stop if tag is unknown
                return;

            AtomicString tagName = currToken.tagName;
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 0
            kdDebug( 6036 ) << "appending Tag: " << tagName.qstring() << endl;
#endif

            // Handle <script src="foo"/> like Mozilla/Opera. We have to do this now for Dashboard
            // compatibility.
            bool isSelfClosingScript = currToken.flat && currToken.beginTag && currToken.tagName == scriptTag;
            bool beginTag = !currToken.flat && currToken.beginTag;
            if (currToken.beginTag && currToken.tagName == scriptTag) {
                AttributeImpl* a = 0;
                bool foundTypeAttribute = false;
                scriptSrc = QString::null;
                scriptSrcCharset = QString::null;
                if ( currToken.attrs && /* potentially have a ATTR_SRC ? */
		     parser->doc()->part() &&
                     parser->doc()->part()->jScriptEnabled() && /* jscript allowed at all? */
                     view /* are we a regular tokenizer or just for innerHTML ? */
                    ) {
                    if ((a = currToken.attrs->getAttributeItem(srcAttr)))
                        scriptSrc = parser->doc()->completeURL(parseURL( a->value() ).qstring() );
                    if ((a = currToken.attrs->getAttributeItem(charsetAttr)))
                        scriptSrcCharset = a->value().qstring().stripWhiteSpace();
                    if ( scriptSrcCharset.isEmpty() )
                        scriptSrcCharset = parser->doc()->part()->encoding();
                    /* Check type before language, since language is deprecated */
                    if ((a = currToken.attrs->getAttributeItem(typeAttr)) != 0 && !a->value().qstring().isEmpty())
                        foundTypeAttribute = true;
                    else
                        a = currToken.attrs->getAttributeItem(languageAttr);
                }
                javascript = true;

                if( foundTypeAttribute ) {
                    /* 
                        Mozilla 1.5 accepts application/x-javascript, and some web references claim it is the only
                        correct variation, but WinIE 6 doesn't accept it.
                        Neither Mozilla 1.5 nor WinIE 6 accept application/javascript, application/ecmascript, or
                        application/x-ecmascript.
                        Mozilla 1.5 doesn't accept the text/javascript1.x formats, but WinIE 6 does.
                        Mozilla 1.5 doesn't accept text/jscript, text/ecmascript, and text/livescript, but WinIE 6 does.
                        Mozilla 1.5 allows leading and trailing whitespace, but WinIE 6 doesn't.
                        Mozilla 1.5 and WinIE 6 both accept the empty string, but neither accept a whitespace-only string.
                        We want to accept all the values that either of these browsers accept, but not other values.
                     */
                    QString type = a->value().qstring().stripWhiteSpace().lower();
                    if( type.compare("application/x-javascript") != 0 &&
                        type.compare("text/javascript") != 0 &&
                        type.compare("text/javascript1.0") != 0 &&
                        type.compare("text/javascript1.1") != 0 &&
                        type.compare("text/javascript1.2") != 0 &&
                        type.compare("text/javascript1.3") != 0 &&
                        type.compare("text/javascript1.4") != 0 &&
                        type.compare("text/javascript1.5") != 0 &&
                        type.compare("text/jscript") != 0 &&
                        type.compare("text/ecmascript") != 0 &&
                        type.compare("text/livescript") )
                        javascript = false;
                } else if( a ) {
                    /* 
                     Mozilla 1.5 doesn't accept jscript or ecmascript, but WinIE 6 does.
                     Mozilla 1.5 accepts javascript1.0, javascript1.4, and javascript1.5, but WinIE 6 accepts only 1.1 - 1.3.
                     Neither Mozilla 1.5 nor WinIE 6 accept leading or trailing whitespace.
                     We want to accept all the values that either of these browsers accept, but not other values.
                     */
                    QString lang = a->value().qstring();
                    lang = lang.lower();
                    if( lang.compare("") != 0 &&
                        lang.compare("javascript") != 0 &&
                        lang.compare("javascript1.0") != 0 &&
                        lang.compare("javascript1.1") != 0 &&
                        lang.compare("javascript1.2") != 0 &&
                        lang.compare("javascript1.3") != 0 &&
                        lang.compare("javascript1.4") != 0 &&
                        lang.compare("javascript1.5") != 0 &&
                        lang.compare("ecmascript") != 0 &&
                        lang.compare("livescript") != 0 &&
                        lang.compare("jscript") )
                        javascript = false;
                }
            }

            processToken();

            if (tagName == preTag) {
                discard = LFDiscard; // Discard the first LF after we open a pre.
            } else if (tagName == scriptTag) {
                if (beginTag) {
                    searchStopper = scriptEnd;
                    searchStopperLen = 8;
                    script = true;
                    parseSpecial(src);
                } else if (isSelfClosingScript) { // Handle <script src="foo"/>
                    script = true;
                    scriptHandler();
                }
            } else if (tagName == styleTag) {
                if (beginTag) {
                    searchStopper = styleEnd;
                    searchStopperLen = 7;
                    style = true;
                    parseSpecial(src);
                }
            } else if (tagName == textareaTag) {
                if(beginTag) {
                    searchStopper = textareaEnd;
                    searchStopperLen = 10;
                    textarea = true;
                    parseSpecial(src);
                }
            } else if (tagName == titleTag) {
                 if (beginTag) {
                    searchStopper = titleEnd;
                    searchStopperLen = 7;
                    title = true;
                    parseSpecial(src);
                }
            } else if (tagName == xmpTag) {
                if (beginTag) {
                    searchStopper = xmpEnd;
                    searchStopperLen = 5;
                    xmp = true;
                    parseSpecial(src);
                }
            } else if (tagName == selectTag)
                select = beginTag;
            else if (tagName == plaintextTag)
                plaintext = beginTag;
            return; // Finished parsing tag!
        }
        } // end switch
    }
    return;
}

void HTMLTokenizer::addPending()
{
    if ( select && !script )
    {
        *dest++ = ' ';
    }
    else if ( textarea || script )
    {
        switch(pending) {
        case LFPending:  *dest++ = '\n'; break;
        case SpacePending: *dest++ = ' '; break;
        case NonePending:
            assert(0);
        }
    }
    else
    {
        switch (pending)
        {
        case SpacePending:
            // Insert a breaking space
            *dest++ = QChar(' ');
            break;

        case LFPending:
            *dest = '\n';
            dest++;
            break;

        case NonePending:
            assert(0);
            break;
        }
    }
    
    pending = NonePending;
}

void HTMLTokenizer::write(const TokenizerString &str, bool appendData)
{
#ifdef TOKEN_DEBUG
    kdDebug( 6036 ) << this << " Tokenizer::write(\"" << str.toString() << "\"," << appendData << ")" << endl;
#endif

    if (!buffer)
        return;
    
    if (loadStopped)
        return;

    if ( ( m_executingScript && appendData ) || !cachedScript.isEmpty() ) {
        // don't parse; we will do this later
	if (currentPrependingSrc) {
	    currentPrependingSrc->append(str);
	} else {
	    pendingSrc.append(str);
	}
        return;
    }

    if ( onHold ) {
        src.append(str);
        return;
    }
    
    if (!src.isEmpty())
        src.append(str);
    else
        setSrc(str);

    // Once a timer is set, it has control of when the tokenizer continues.
    if (timerId)
        return;

    bool wasInWrite = inWrite;
    inWrite = true;
    
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!parser->doc()->ownerElement())
        printf("Beginning write at time %d\n", parser->doc()->elapsedTime());
#endif
    
//     if (Entity)
//         parseEntity(src, dest);

    int processedCount = 0;
    QTime startTime;
    startTime.start();
    KWQUIEventTime eventTime;

    KHTMLPart* part = parser->doc()->part();
    while (!src.isEmpty() && (!part || !part->isScheduledLocationChangePending())) {
        if (!continueProcessing(processedCount, startTime, eventTime))
            break;

        // do we need to enlarge the buffer?
        checkBuffer();

        ushort cc = src->unicode();

        if (skipLF && (cc != '\n'))
            skipLF = false;

        if (skipLF) {
            skipLF = false;
            ++src;
        }
        else if ( Entity )
            parseEntity( src, dest );
        else if ( plaintext )
            parseText( src );
        else if (script)
            parseSpecial(src);
        else if (style)
            parseSpecial(src);
        else if (xmp)
            parseSpecial(src);
        else if (textarea)
            parseSpecial(src);
        else if (title)
            parseSpecial(src);
        else if (comment)
            parseComment(src);
        else if (server)
            parseServer(src);
        else if (processingInstruction)
            parseProcessingInstruction(src);
        else if (tag)
            parseTag(src);
        else if ( startTag )
        {
            startTag = false;

            switch(cc) {
            case '/':
                break;
            case '!':
            {
                // <!-- comment -->
                searchCount = 1; // Look for '<!--' sequence to start comment

                break;
            }
            case '?':
            {
                // xml processing instruction
                processingInstruction = true;
                tquote = NoQuote;
                parseProcessingInstruction(src);
                continue;

                break;
            }
            case '%':
                if (!brokenServer) {
                    // <% server stuff, handle as comment %>
                    server = true;
                    tquote = NoQuote;
                    parseServer(src);
                    continue;
                }
                // else fall through
            default:
            {
                if( ((cc >= 'a') && (cc <= 'z')) || ((cc >= 'A') && (cc <= 'Z')))
                {
                    // Start of a Start-Tag
                }
                else
                {
                    // Invalid tag
                    // Add as is
                    if (pending)
                        addPending();
                    *dest = '<';
                    dest++;
                    continue;
                }
            }
            }; // end case

            if ( pending ) {
                if ( script || (!parser->selectMode() && (!parser->noSpaces() || dest > buffer )))
                    addPending();
                // just forget it
                else
                    pending = NonePending;
            }

            if (cc == '/' && discard == AllDiscard)
                discard = NoneDiscard; // A close tag. No need to discard LF.
                    
            processToken();

            cBufferPos = 0;
            tag = TagName;
            parseTag(src);
        }
        else if ( cc == '&' && !src.escaped())
        {
            ++src;
            if ( pending )
                addPending();
            parseEntity(src, dest, true);
        }
        else if ( cc == '<' && !src.escaped())
        {
            tagStartLineno = lineno+src.lineCount();
            ++src;
            startTag = true;
        }
        else if (( cc == '\n' ) || ( cc == '\r' ))
        {
	    if (select && !script)
            {
                if (discard == LFDiscard)
                {
                    // Ignore this LF
                    discard = NoneDiscard; // We have discarded 1 LF
                }
                else if(discard == AllDiscard)
                {
                }
                else
                 {
                     // Process this LF
                    if (pending == NonePending)
                         pending = LFPending;
                }
            }
            else {
                if (discard == LFDiscard || discard == AllDiscard)
                {
                    // Ignore this LF
                    discard = NoneDiscard; // We have discarded 1 LF
                }
                else
                {
                    // Process this LF
                    if (pending)
                        addPending();
                    pending = LFPending;
                }
            }
            
            /* Check for MS-DOS CRLF sequence */
            if (cc == '\r')
            {
                skipLF = true;
            }
            ++src;
        }
        else if (cc == ' ')
        {
	    if (select && !script) {
                if(discard == SpaceDiscard)
                    discard = NoneDiscard;
                 else if(discard == AllDiscard)
                 { }
                 else
                     pending = SpacePending;
            
            }
            else {
                if (discard == AllDiscard)
                    discard = NoneDiscard;
            
                if (pending)
                    addPending();
                pending = SpacePending;
            }
            
            ++src;
        }
        else
        {
            if (pending)
                addPending();

            discard = NoneDiscard;
#if QT_VERSION < 300
            unsigned char row = src->row();
            if ( row > 0x05 && row < 0x10 || row > 0xfd )
                    currToken.complexText = true;
#endif
            *dest = *src;
            fixUpChar(*dest);
            ++dest;
            ++src;
        }
    }
    
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!parser->doc()->ownerElement())
        printf("Ending write at time %d\n", parser->doc()->elapsedTime());
#endif
    
    inWrite = wasInWrite;

    if (noMoreData && !inWrite && !loadingExtScript && !m_executingScript && !timerId)
        end(); // this actually causes us to be deleted
}

void HTMLTokenizer::stopped()
{
    if (timerId) {
        killTimer(timerId);
        timerId = 0;
    }
}

bool HTMLTokenizer::processingData() const
{
    return timerId != 0;
}

bool HTMLTokenizer::continueProcessing(int& processedCount, const QTime& startTime, const KWQUIEventTime& eventTime)
{
    // We don't want to be checking elapsed time with every character, so we only check after we've
    // processed a certain number of characters.
    bool allowedYield = allowYield;
    allowYield = false;
    if (!loadingExtScript && !forceSynchronous && !m_executingScript && (processedCount > TOKENIZER_CHUNK_SIZE || allowedYield)) {
        processedCount = 0;
        if (startTime.elapsed() > TOKENIZER_TIME_DELAY) {
            /* FIXME: We'd like to yield aggressively to give stylesheets the opportunity to
               load, but this hurts overall performance on slower machines.  For now turn this
               off.
            || (!parser->doc()->haveStylesheetsLoaded() && 
                (parser->doc()->documentElement()->id() != ID_HTML || parser->doc()->body()))) {*/
            // Schedule the timer to keep processing as soon as possible.
            if (!timerId)
                timerId = startTimer(0);
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
            if (eventTime.uiEventPending())
                printf("Deferring processing of data because of UI event.\n");
            else if (startTime.elapsed() > TOKENIZER_TIME_DELAY)
                printf("Deferring processing of data because 200ms elapsed away from event loop.\n");
#endif
            return false;
        }
    }
    
    processedCount++;
    return true;
}

void HTMLTokenizer::timerEvent(QTimerEvent* e)
{
    if (e->timerId() == timerId) {
        // Kill the timer.
        killTimer(timerId);
        timerId = 0;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
        if (!parser->doc()->ownerElement())
            printf("Beginning timer write at time %d\n", parser->doc()->elapsedTime());
#endif

        if (parser->doc()->view() && parser->doc()->view()->layoutPending() && !parser->doc()->minimumLayoutDelay()) {
            // Restart the timer and let layout win.  This is basically a way of ensuring that the layout
            // timer has higher priority than our timer.
            timerId = startTimer(0);
            return;
        }
        
        // Invoke write() as though more data came in.
        bool oldNoMoreData = noMoreData;
        noMoreData = false;  // This prevents write() from deleting the tokenizer.
        write(TokenizerString(), true);
        noMoreData = oldNoMoreData;
        
        // If the timer dies (and stays dead after the write),  we need to let WebKit know that we're done processing the data.
        allDataProcessed();
    }
}

void HTMLTokenizer::allDataProcessed()
{
    if (noMoreData && !inWrite && !loadingExtScript && !m_executingScript && !onHold && !timerId) {
        QGuardedPtr<KHTMLView> savedView = view;
        end();
        if (savedView) {
            KHTMLPart *part = savedView->part();
            if (part) {
                part->tokenizerProcessedData();
            }
        }
    }
}

void HTMLTokenizer::end()
{
    assert(timerId == 0);
    if (timerId) {
        // Clean up anyway.
        killTimer(timerId);
        timerId = 0;
    }

    if ( buffer == 0 ) {
        parser->finished();
        emit finishedParsing();
        return;
    }

    // parseTag is using the buffer for different matters
    if ( !tag )
        processToken();

    if(buffer)
        KHTML_DELETE_QCHAR_VEC(buffer);

    if(scriptCode)
        KHTML_DELETE_QCHAR_VEC(scriptCode);

    scriptCode = 0;
    scriptCodeSize = scriptCodeMaxSize = scriptCodeResync = 0;
    buffer = 0;
    parser->finished();
    emit finishedParsing();
}

void HTMLTokenizer::finish()
{
    // do this as long as we don't find matching comment ends
    while((comment || server) && scriptCode && scriptCodeSize)
    {
        // we've found an unmatched comment start
        if (comment)
            brokenComments = true;
        else
            brokenServer = true;
        checkScriptBuffer();
        scriptCode[ scriptCodeSize ] = 0;
        scriptCode[ scriptCodeSize + 1 ] = 0;
        int pos;
        QString food;
        if (script || style) {
            food.setUnicode(scriptCode, scriptCodeSize);
        }
        else if (server) {
            food = "<";
            food += QString(scriptCode, scriptCodeSize);
        }
        else {
            pos = QConstString(scriptCode, scriptCodeSize).string().find('>');
            food.setUnicode(scriptCode+pos+1, scriptCodeSize-pos-1); // deep copy
        }
        KHTML_DELETE_QCHAR_VEC(scriptCode);
        scriptCode = 0;
        scriptCodeSize = scriptCodeMaxSize = scriptCodeResync = 0;
        comment = server = false;
        if ( !food.isEmpty() )
            write(food, true);
    }
    // this indicates we will not receive any more data... but if we are waiting on
    // an external script to load, we can't finish parsing until that is done
    noMoreData = true;
    if (pending) // Flush any remaining whitespace.
        addPending();
    if (!inWrite && !loadingExtScript && !m_executingScript && !onHold && !timerId)
        end(); // this actually causes us to be deleted
}

void HTMLTokenizer::processToken()
{
    KJSProxy *jsProxy = (view && view->part()) ? view->part()->jScript() : 0L;    
    if (jsProxy)
        jsProxy->setEventHandlerLineno(tagStartLineno);
    if ( dest > buffer )
    {
#ifdef TOKEN_DEBUG
        if(currToken.tagName.length()) {
            qDebug( "unexpected token: %s, str: *%s*", currToken.tagName.qstring().latin1(),QConstString( buffer,dest-buffer ).qstring().latin1() );
            assert(0);
        }

#endif
        currToken.text = new DOMStringImpl( buffer, dest - buffer );
        currToken.text->ref();
        if (currToken.tagName != commentAtom)
            currToken.tagName = textAtom;
    }
    else if (currToken.tagName == nullAtom) {
        currToken.reset();
        if (jsProxy)
            jsProxy->setEventHandlerLineno(lineno+src.lineCount());
        return;
    }

    dest = buffer;

#ifdef TOKEN_DEBUG
    QString name = currToken.tagName.qstring();
    QString text;
    if(currToken.text)
        text = QConstString(currToken.text->s, currToken.text->l).qstring();

    kdDebug( 6036 ) << "Token --> " << name << endl;
    if (currToken.flat)
        kdDebug( 6036 ) << "Token is FLAT!" << endl;
    if(!text.isNull())
        kdDebug( 6036 ) << "text: \"" << text << "\"" << endl;
    unsigned l = currToken.attrs ? currToken.attrs->length() : 0;
    if(l) {
        kdDebug( 6036 ) << "Attributes: " << l << endl;
        for (unsigned i = 0; i < l; ++i) {
            AttributeImpl* c = currToken.attrs->attributeItem(i);
            kdDebug( 6036 ) << "    " << c->localName().qstring()
                            << "=\"" << c->value().qstring() << "\"" << endl;
        }
    }
    kdDebug( 6036 ) << endl;
#endif
    
    if (!loadStopped) {
        // pass the token over to the parser, the parser DOES NOT delete the token
        parser->parseToken(&currToken);
    }
    
    currToken.reset();
    if (jsProxy)
        jsProxy->setEventHandlerLineno(0);
}

HTMLTokenizer::~HTMLTokenizer()
{
    assert(!inWrite);
    reset();
    delete parser;
}


void HTMLTokenizer::enlargeBuffer(int len)
{
    int newsize = kMax(size*2, size+len);
    int oldoffs = (dest - buffer);

    buffer = (QChar*)realloc(buffer, newsize*sizeof(QChar));
    dest = buffer + oldoffs;
    size = newsize;
}

void HTMLTokenizer::enlargeScriptBuffer(int len)
{
    int newsize = kMax(scriptCodeMaxSize*2, scriptCodeMaxSize+len);
    scriptCode = (QChar*)realloc(scriptCode, newsize*sizeof(QChar));
    scriptCodeMaxSize = newsize;
}

void HTMLTokenizer::notifyFinished(CachedObject */*finishedObj*/)
{
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!parser->doc()->ownerElement())
        printf("script loaded at %d\n", parser->doc()->elapsedTime());
#endif

    assert(!cachedScript.isEmpty());
    bool finished = false;
    while (!finished && cachedScript.head()->isLoaded()) {
#ifdef TOKEN_DEBUG
        kdDebug( 6036 ) << "Finished loading an external script" << endl;
#endif
        CachedScript* cs = cachedScript.dequeue();
        DOMString scriptSource = cs->script();
#ifdef TOKEN_DEBUG
        kdDebug( 6036 ) << "External script is:" << endl << scriptSource.qstring() << endl;
#endif
        setSrc(TokenizerString());

        // make sure we forget about the script before we execute the new one
        // infinite recursion might happen otherwise
        QString cachedScriptUrl( cs->url().qstring() );
        cs->deref(this);

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
        if (!parser->doc()->ownerElement())
            printf("external script beginning execution at %d\n", parser->doc()->elapsedTime());
#endif

	scriptExecution( scriptSource.qstring(), cachedScriptUrl );

        // The state of cachedScript.isEmpty() can change inside the scriptExecution()
        // call above, so test afterwards.
        finished = cachedScript.isEmpty();
        if (finished) {
            loadingExtScript = false;
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
            if (!parser->doc()->ownerElement())
                printf("external script finished execution at %d\n", parser->doc()->elapsedTime());
#endif
        }

        // 'script' is true when we are called synchronously from
        // parseScript(). In that case parseScript() will take care
        // of 'scriptOutput'.
        if ( !script ) {
            TokenizerString rest = pendingSrc;
            pendingSrc.clear();
            write(rest, false);
            // we might be deleted at this point, do not
            // access any members.
        }
    }
}

bool HTMLTokenizer::isWaitingForScripts() const
{
    return loadingExtScript;
}

void HTMLTokenizer::setSrc(const TokenizerString &source)
{
    lineno += src.lineCount();
    src = source;
    src.resetLineCount();
}

void HTMLTokenizer::setOnHold(bool _onHold)
{
    onHold = _onHold;
}

}
