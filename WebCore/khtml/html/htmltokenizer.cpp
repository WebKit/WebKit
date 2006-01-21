/*
    This file is part of the KDE libraries

    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)
              (C) 1998 Waldo Bastian (bastian@kde.org)
              (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

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
#include "htmltokenizer.h"

#include "CachedScript.h"
#include "DocLoader.h"
#include "DocumentFragmentImpl.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "csshelper.h"
#include "html_documentimpl.h"
#include "HTMLElementImpl.h"
#include "htmlparser.h"
#include "kjs_proxy.h"
#include <assert.h>
#include <ctype.h>
#include <qvariant.h>
#include <stdlib.h>
#include "htmlnames.h"

// turn off inlining to allow proper linking on newer gcc (xmltokenizer.cpp also uses findEntity())
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

namespace WebCore {

using namespace HTMLNames;
using namespace EventNames;

static const char commentStart [] = "<!--";
static const char scriptEnd [] = "</script";
static const char xmpEnd [] = "</xmp";
static const char styleEnd [] =  "</style";
static const char textareaEnd [] = "</textarea";
static const char titleEnd [] = "</title";

#define KHTML_ALLOC_QCHAR_VEC( N ) (QChar*) fastMalloc( sizeof(QChar)*( N ) )
#define KHTML_DELETE_QCHAR_VEC( P ) fastFree((char*)( P ))

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

static inline QChar fixUpChar(QChar c)
{
    ushort code = c.unicode();
    if ((code & ~0x1F) != 0x0080)
        return c;
    return windowsLatin1ExtensionArray[code - 0x80];
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
        if (!attrs)
            attrs = new NamedMappedAttrMapImpl(0);
        attrs->insertAttribute(a);
    }
}

// ----------------------------------------------------------------------------

HTMLTokenizer::HTMLTokenizer(DocumentImpl* _doc, FrameView* _view, bool includesComments)
    : inWrite(false)
{
    view = _view;
    buffer = 0;
    scriptCode = 0;
    scriptCodeSize = scriptCodeMaxSize = scriptCodeResync = 0;
    parser = new HTMLParser(_view, _doc, includesComments);
    m_executingScript = 0;
    onHold = false;
    timerId = 0;
    includesCommentsInDOM = includesComments;
    
    begin();
}

HTMLTokenizer::HTMLTokenizer(DocumentImpl *_doc, DocumentFragmentImpl *i, bool includesComments)
    : inWrite(false)
{
    view = 0;
    buffer = 0;
    scriptCode = 0;
    scriptCodeSize = scriptCodeMaxSize = scriptCodeResync = 0;
    parser = new HTMLParser(i, _doc, includesComments);
    m_executingScript = 0;
    onHold = false;
    timerId = 0;
    includesCommentsInDOM = includesComments;

    begin();
}

void HTMLTokenizer::reset()
{
    assert(m_executingScript == 0);
    assert(onHold == false);

    while (!pendingScripts.isEmpty()) {
      CachedScript *cs = pendingScripts.dequeue();
      assert(cs->accessCount() > 0);
      cs->deref(this);
    }
    
    if (buffer)
        KHTML_DELETE_QCHAR_VEC(buffer);
    buffer = dest = 0;
    size = 0;

    if (scriptCode)
        KHTML_DELETE_QCHAR_VEC(scriptCode);
    scriptCode = 0;
    scriptCodeSize = scriptCodeMaxSize = scriptCodeResync = 0;

    if (timerId) {
        killTimer(timerId);
        timerId = 0;
    }
    timerId = 0;
    m_state.setAllowYield(false);
    m_state.setForceSynchronous(false);

    currToken.reset();
}

void HTMLTokenizer::begin()
{
    m_executingScript = 0;
    m_state.setLoadingExtScript(false);
    onHold = false;
    reset();
    size = 254;
    buffer = KHTML_ALLOC_QCHAR_VEC( 255 );
    dest = buffer;
    tquote = NoQuote;
    searchCount = 0;
    m_state.setEntityState(NoEntity);
    scriptSrc = QString::null;
    pendingSrc.clear();
    currentPrependingSrc = 0;
    noMoreData = false;
    brokenComments = false;
    brokenServer = false;
    lineno = 0;
    scriptStartLineno = 0;
    tagStartLineno = 0;
    m_state.setForceSynchronous(false);
}

void HTMLTokenizer::setForceSynchronous(bool force)
{
    m_state.setForceSynchronous(force);
}

HTMLTokenizer::State HTMLTokenizer::processListing(SegmentedString list, State state)
{
    // This function adds the listing 'list' as
    // preformatted text-tokens to the token-collection
    while (!list.isEmpty()) {
        if (state.skipLF()) {
            state.setSkipLF(false);
            if (*list == '\n') {
                ++list;
                continue;
            }
        }

        checkBuffer();

        if (*list == '\n' || *list == '\r') {
            if (state.discardLF())
                // Ignore this LF
                state.setDiscardLF(false); // We have discarded 1 LF
            else
                *dest++ = '\n';

            /* Check for MS-DOS CRLF sequence */
            if (*list == '\r')
                state.setSkipLF(true);

            ++list;
        } else {
            state.setDiscardLF(false);
            *dest++ = *list;
            ++list;
        }
    }

    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseSpecial(SegmentedString &src, State state)
{
    assert(state.inTextArea() || state.inTitle() || !state.hasEntityState());
    assert(!state.hasTagState());
    assert(state.inXmp() + state.inTextArea() + state.inTitle() + state.inStyle() + state.inScript() == 1 );
    if (state.inScript())
        scriptStartLineno = lineno + src.lineCount();

    if (state.inComment()) 
        state = parseComment(src, state);

    while ( !src.isEmpty() ) {
        checkScriptBuffer();
        unsigned char ch = src->latin1();
        if (!scriptCodeResync && !brokenComments && !state.inTextArea() && !state.inXmp() && !state.inTitle() && ch == '-' && scriptCodeSize >= 3 && !src.escaped() && scriptCode[scriptCodeSize-3] == '<' && scriptCode[scriptCodeSize-2] == '!' && scriptCode[scriptCodeSize-1] == '-') {
            state.setInComment(true);
            state = parseComment(src, state);
            continue;
        }
        if ( scriptCodeResync && !tquote && ( ch == '>' ) ) {
            ++src;
            scriptCodeSize = scriptCodeResync-1;
            scriptCodeResync = 0;
            scriptCode[ scriptCodeSize ] = scriptCode[ scriptCodeSize + 1 ] = 0;
            if (state.inScript())
                state = scriptHandler(state);
            else {
                state = processListing(SegmentedString(scriptCode, scriptCodeSize), state);
                processToken();
                if (state.inStyle()) { 
                    currToken.tagName = styleTag.localName(); 
                    currToken.beginTag = false; 
                } else if (state.inTextArea()) { 
                    currToken.tagName = textareaTag.localName(); 
                    currToken.beginTag = false; 
                } else if (state.inTitle()) { 
                    currToken.tagName = titleTag.localName(); 
                    currToken.beginTag = false; 
                } else if (state.inXmp()) {
                    currToken.tagName = xmpTag.localName(); 
                    currToken.beginTag = false; 
                }
                processToken();
                state.setInStyle(false);
                state.setInScript(false);
                state.setInTextArea(false);
                state.setInTitle(false);
                state.setInXmp(false);
                tquote = NoQuote;
                scriptCodeSize = scriptCodeResync = 0;
            }
            return state;
        }
        // possible end of tagname, lets check.
        if ( !scriptCodeResync && !state.escaped() && !src.escaped() && ( ch == '>' || ch == '/' || ch <= ' ' ) && ch &&
             scriptCodeSize >= searchStopperLen &&
             tagMatch( searchStopper, scriptCode+scriptCodeSize-searchStopperLen, searchStopperLen )) {
            scriptCodeResync = scriptCodeSize-searchStopperLen+1;
            tquote = NoQuote;
            continue;
        }
        if ( scriptCodeResync && !state.escaped() ) {
            if(ch == '\"')
                tquote = (tquote == NoQuote) ? DoubleQuote : ((tquote == SingleQuote) ? SingleQuote : NoQuote);
            else if(ch == '\'')
                tquote = (tquote == NoQuote) ? SingleQuote : (tquote == DoubleQuote) ? DoubleQuote : NoQuote;
            else if (tquote != NoQuote && (ch == '\r' || ch == '\n'))
                tquote = NoQuote;
        }
        state.setEscaped(!state.escaped() && ch == '\\');
        if (!scriptCodeResync && (state.inTextArea() || state.inTitle()) && !src.escaped() && ch == '&') {
            QChar *scriptCodeDest = scriptCode+scriptCodeSize;
            ++src;
            state = parseEntity(src, scriptCodeDest, state, m_cBufferPos, true, false);
            scriptCodeSize = scriptCodeDest-scriptCode;
        }
        else {
            scriptCode[scriptCodeSize++] = fixUpChar(*src);
            ++src;
        }
    }

    return state;
}

HTMLTokenizer::State HTMLTokenizer::scriptHandler(State state)
{
    // We are inside a <script>
    bool doScriptExec = false;

    // (Bugzilla 3837) Scripts following a frameset element should not execute or, 
    // in the case of extern scripts, even load.
    bool followingFrameset = (parser->doc()->body() && parser->doc()->body()->hasTagName(framesetTag));
  
    CachedScript* cs = 0;
    // don't load external scripts for standalone documents (for now)
    if (!scriptSrc.isEmpty() && parser->doc()->frame()) {
        // forget what we just got; load from src url instead
        if (!parser->skipMode() && !followingFrameset) {
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
            if (!parser->doc()->ownerElement())
                printf("Requesting script at time %d\n", parser->doc()->elapsedTime());
#endif
            if ( (cs = parser->doc()->docLoader()->requestScript(scriptSrc, scriptSrcCharset) ))
                pendingScripts.enqueue(cs);
            else
                scriptNode = 0;
        }
        scriptSrc=QString::null;
    }
    else {
#ifdef TOKEN_DEBUG
        kdDebug( 6036 ) << "---START SCRIPT---" << endl;
        kdDebug( 6036 ) << QString(scriptCode, scriptCodeSize) << endl;
        kdDebug( 6036 ) << "---END SCRIPT---" << endl;
#endif
        scriptNode = 0;
        // Parse scriptCode containing <script> info
        doScriptExec = true;
    }
    state = processListing(SegmentedString(scriptCode, scriptCodeSize), state);
    QString exScript( buffer, dest-buffer );
    processToken();
    currToken.tagName = scriptTag.localName();
    currToken.beginTag = false;
    processToken();

    SegmentedString *savedPrependingSrc = currentPrependingSrc;
    SegmentedString prependingSrc;
    currentPrependingSrc = &prependingSrc;
    if (!parser->skipMode() && !followingFrameset) {
        if (cs) {
	    if (savedPrependingSrc) {
		savedPrependingSrc->append(src);
	    } else {
		pendingSrc.prepend(src);
	    }
            setSrc(SegmentedString());
            scriptCodeSize = scriptCodeResync = 0;

            // the ref() call below may call notifyFinished if the script is already in cache,
            // and that mucks with the state directly, so we must write it back to the object.
            m_state = state;
            cs->ref(this);
            state = m_state;
            // will be 0 if script was already loaded and ref() executed it
            if (!pendingScripts.isEmpty())
                state.setLoadingExtScript(true);
        }
        else if (view && doScriptExec && javascript ) {
            if (!m_executingScript)
                pendingSrc.prepend(src);
            else
                prependingSrc = src;
            setSrc(SegmentedString());
            scriptCodeSize = scriptCodeResync = 0;
            //QTime dt;
            //dt.start();
            state = scriptExecution(exScript, state, QString::null, scriptStartLineno);
        }
    }

    state.setInScript(false);
    scriptCodeSize = scriptCodeResync = 0;

    if (!m_executingScript && !state.loadingExtScript()) {
	src.append(pendingSrc);
	pendingSrc.clear();
    } else if (!prependingSrc.isEmpty()) {
	// restore first so that the write appends in the right place
	// (does not hurt to do it again below)
	currentPrependingSrc = savedPrependingSrc;

	// we need to do this slightly modified bit of one of the write() cases
	// because we want to prepend to pendingSrc rather than appending
	// if there's no previous prependingSrc
	if (state.loadingExtScript()) {
	    if (currentPrependingSrc) {
		currentPrependingSrc->append(prependingSrc);
	    } else {
		pendingSrc.prepend(prependingSrc);
	    }
	} else {
            m_state = state;
	    write(prependingSrc, false);
            state = m_state;
	}
    }

    currentPrependingSrc = savedPrependingSrc;

    return state;
}

HTMLTokenizer::State HTMLTokenizer::scriptExecution(const QString& str, State state, 
                                                    QString scriptURL, int baseLine)
{
    if (!view || !view->frame())
        return state;
    bool oldscript = state.inScript();
    m_executingScript++;
    state.setInScript(false);
    QString url;    
    if (scriptURL.isNull())
      url = view->frame()->document()->URL();
    else
      url = scriptURL;

    SegmentedString *savedPrependingSrc = currentPrependingSrc;
    SegmentedString prependingSrc;
    currentPrependingSrc = &prependingSrc;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!parser->doc()->ownerElement())
        printf("beginning script execution at %d\n", parser->doc()->elapsedTime());
#endif

    m_state = state;
    view->frame()->executeScript(url,baseLine,0,str);
    state = m_state;

    state.setAllowYield(true);

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!parser->doc()->ownerElement())
        printf("ending script execution at %d\n", parser->doc()->elapsedTime());
#endif
    
    m_executingScript--;
    state.setInScript(oldscript);

    if (!m_executingScript && !state.loadingExtScript()) {
	src.append(pendingSrc);
	pendingSrc.clear();
    } else if (!prependingSrc.isEmpty()) {
	// restore first so that the write appends in the right place
	// (does not hurt to do it again below)
	currentPrependingSrc = savedPrependingSrc;

	// we need to do this slightly modified bit of one of the write() cases
	// because we want to prepend to pendingSrc rather than appending
	// if there's no previous prependingSrc
	if (state.loadingExtScript()) {
	    if (currentPrependingSrc) {
		currentPrependingSrc->append(prependingSrc);
	    } else {
		pendingSrc.prepend(prependingSrc);
	    }
	} else {
            m_state = state;
	    write(prependingSrc, false);
            state = m_state;
	}
    }

    currentPrependingSrc = savedPrependingSrc;

    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseComment(SegmentedString &src, State state)
{
    // FIXME: Why does this code even run for comments inside <script> and <style>? This seems bogus.
    bool strict = !parser->doc()->inCompatMode() && !state.inScript() && !state.inStyle();
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
            bool handleBrokenComments = brokenComments && !(state.inScript() || state.inStyle());
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
                if (!(state.inScript() || state.inXmp() || state.inTextArea() || state.inStyle())) {
                    if (includesCommentsInDOM) {
                        checkScriptBuffer();
                        scriptCode[ scriptCodeSize ] = 0;
                        scriptCode[ scriptCodeSize + 1 ] = 0;
                        currToken.tagName = commentAtom;
                        currToken.beginTag = true;
                        state = processListing(SegmentedString(scriptCode, scriptCodeSize - endCharsCount), state);
                        processToken();
                        currToken.tagName = commentAtom;
                        currToken.beginTag = false;
                        processToken();
                    }
                    scriptCodeSize = 0;
                }
                state.setInComment(false);
                return state; // Finished parsing comment
            }
        }
        ++src;
    }

    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseServer(SegmentedString& src, State state)
{
    checkScriptBuffer(src.length());
    while (!src.isEmpty()) {
        scriptCode[scriptCodeSize++] = *src;
        if (src->unicode() == '>' &&
            scriptCodeSize > 1 && scriptCode[scriptCodeSize-2] == '%') {
            ++src;
            state.setInServer(false);
            scriptCodeSize = 0;
            return state; // Finished parsing server include
        }
        ++src;
    }
    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseProcessingInstruction(SegmentedString &src, State state)
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
            state.setInProcessingInstruction(false);
            ++src;
            state.setDiscardLF(true);
            return state; // Finished parsing comment!
        }
        ++src;
        oldchar = chbegin;
    }
    
    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseText(SegmentedString &src, State state)
{
    while (!src.isEmpty()) {
        ushort cc = src->unicode();

        if (state.skipLF()) {
            state.setSkipLF(false);
            if (cc == '\n') {
                ++src;
                continue;
            }
        }

        // do we need to enlarge the buffer?
        checkBuffer();

        if (cc == '\r') {
            state.setSkipLF(true);
            *dest++ = '\n';
        } else
            *dest++ = fixUpChar(cc);
        ++src;
    }

    return state;
}


HTMLTokenizer::State HTMLTokenizer::parseEntity(SegmentedString &src, QChar *&dest, State state, unsigned &cBufferPos, bool start, bool parsingTag)
{
    if (start)
    {
        cBufferPos = 0;
        state.setEntityState(SearchEntity);
        EntityUnicodeValue = 0;
    }

    while(!src.isEmpty())
    {
        ushort cc = src->unicode();
        switch(state.entityState()) {
        case NoEntity:
            assert(state.entityState() != NoEntity);
            return state;
        
        case SearchEntity:
            if(cc == '#') {
                cBuffer[cBufferPos++] = cc;
                ++src;
                state.setEntityState(NumericSearch);
            }
            else
                state.setEntityState(EntityName);

            break;

        case NumericSearch:
            if(cc == 'x' || cc == 'X') {
                cBuffer[cBufferPos++] = cc;
                ++src;
                state.setEntityState(Hexadecimal);
            }
            else if(cc >= '0' && cc <= '9')
                state.setEntityState(Decimal);
            else
                state.setEntityState(SearchSemicolon);

            break;

        case Hexadecimal:
        {
            int ll = kMin(src.length(), 10-cBufferPos);
            while(ll--) {
                QChar csrc(src->lower());
                cc = csrc.cell();

                if(csrc.row() || !((cc >= '0' && cc <= '9') || (cc >= 'a' && cc <= 'f'))) {
                    state.setEntityState(SearchSemicolon);
                    break;
                }
                EntityUnicodeValue = EntityUnicodeValue*16 + (cc - ( cc < 'a' ? '0' : 'a' - 10));
                cBuffer[cBufferPos++] = cc;
                ++src;
            }
            if (cBufferPos == 10)  
                state.setEntityState(SearchSemicolon);
            break;
        }
        case Decimal:
        {
            int ll = kMin(src.length(), 9-cBufferPos);
            while(ll--) {
                cc = src->cell();

                if(src->row() || !(cc >= '0' && cc <= '9')) {
                    state.setEntityState(SearchSemicolon);
                    break;
                }

                EntityUnicodeValue = EntityUnicodeValue * 10 + (cc - '0');
                cBuffer[cBufferPos++] = cc;
                ++src;
            }
            if (cBufferPos == 9)  
                state.setEntityState(SearchSemicolon);
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
                    state.setEntityState(SearchSemicolon);
                    break;
                }

                cBuffer[cBufferPos++] = cc;
                ++src;
            }
            if (cBufferPos == 9) 
                state.setEntityState(SearchSemicolon);
            if (state.entityState() == SearchSemicolon) {
                if(cBufferPos > 1) {
                    const Entity *e = findEntity(cBuffer, cBufferPos);
                    if(e)
                        EntityUnicodeValue = e->code;

                    // be IE compatible
                    if(parsingTag && EntityUnicodeValue > 255 && *src != ';')
                        EntityUnicodeValue = 0;
                }
            }
            else
                break;
        }
        case SearchSemicolon:
            // Don't allow values that are more than 21 bits.
            if (EntityUnicodeValue > 0 && EntityUnicodeValue <= 0x1FFFFF) {
            
                if (*src == ';')
                    ++src;

                if (EntityUnicodeValue <= 0xFFFF) {
                    checkBuffer();
                    src.push(fixUpChar(EntityUnicodeValue));
                } else {
                    // Convert to UTF-16, using surrogate code points.
                    QChar c1(0xD800 | (((EntityUnicodeValue >> 16) - 1) << 6) | ((EntityUnicodeValue >> 10) & 0x3F));
                    QChar c2(0xDC00 | (EntityUnicodeValue & 0x3FF));
                    checkBuffer(2);
                    src.push(c1);
                    src.push(c2);
                }
            } else {
                checkBuffer(10);
                // ignore the sequence, add it to the buffer as plaintext
                *dest++ = '&';
                for(unsigned int i = 0; i < cBufferPos; i++)
                    dest[i] = cBuffer[i];
                dest += cBufferPos;
            }

            state.setEntityState(NoEntity);
            return state;
        }
    }

    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseTag(SegmentedString &src, State state)
{
    assert(!state.hasEntityState());

    unsigned cBufferPos = m_cBufferPos;

    while (!src.isEmpty())
    {
        checkBuffer();
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 1
        uint l = 0;
        while(l < src.length() && (*(src.operator->()+l)).latin1() != '>')
            l++;
        qDebug("src is now: *%s*, tquote: %d",
               QConstString((QChar*)src.operator->(), l).qstring().latin1(), tquote);
#endif
        switch(state.tagState()) {
        case NoTag:
        {
            m_cBufferPos = cBufferPos;
            return state;
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
                        state.setInComment(true);
                        state.setTagState(NoTag);

                        // Fix bug 34302 at kde.bugs.org.  Go ahead and treat
                        // <!--> as a valid comment, since both mozilla and IE on windows
                        // can handle this case.  Only do this in quirks mode. -dwh
                        if (!src.isEmpty() && *src == '>' && parser->doc()->inCompatMode()) {
                          state.setInComment(false);
                          ++src;
                          if (!src.isEmpty())
                              cBuffer[cBufferPos++] = src->cell();
                        }
		        else
                          state = parseComment(src, state);

                        m_cBufferPos = cBufferPos;
                        return state; // Finished parsing tag!
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
                state.setTagState(SearchAttribute);
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
                        state.setTagState(SearchEnd);
                    else
                        state.setTagState(AttributeName);

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
                    state.setTagState(SearchEqual);
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
                state.setTagState(SearchEqual);
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
                        state.setTagState(SearchValue);
                        ++src;
                    }
                    else {
                        currToken.addAttribute(parser->doc(), attrName, emptyAtom);
                        dest = buffer;
                        state.setTagState(SearchAttribute);
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
                        state.setTagState(QuotedValue);
                        ++src;
                    } else
                        state.setTagState(Value);

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
                    currToken.addAttribute(parser->doc(), attrName, v);
                    state.setTagState(SearchAttribute);
                    dest = buffer;
                    tquote = NoQuote;
                    break;
                }
                
                if(curchar <= '\'' && !src.escaped()) {
                    // ### attributes like '&{blaa....};' are supposed to be treated as jscript.
                    if ( curchar == '&' )
                    {
                        ++src;
                        state = parseEntity(src, dest, state, cBufferPos, true, true);
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
                        currToken.addAttribute(parser->doc(), attrName, v);

                        dest = buffer;
                        state.setTagState(SearchAttribute);
                        tquote = NoQuote;
                        ++src;
                        break;
                    }
                }
                *dest++ = fixUpChar(*src);
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
                        state = parseEntity(src, dest, state, cBufferPos, true, true);
                        break;
                    }
                    // no quotes. Every space means end of value
                    // '/' does not delimit in IE!
                    if ( curchar <= ' ' || curchar == '>' )
                    {
                        AtomicString v(buffer+1, dest-buffer-1);
                        currToken.addAttribute(parser->doc(), attrName, v);
                        dest = buffer;
                        state.setTagState(SearchAttribute);
                        break;
                    }
                }

                *dest++ = fixUpChar(*src);
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
            state.setTagState(NoTag);
            tquote = NoQuote;

            if (*src != '<')
                ++src;

            if (currToken.tagName == nullAtom) { //stop if tag is unknown
                m_cBufferPos = cBufferPos;
                return state;
            }

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
		     parser->doc()->frame() &&
                     parser->doc()->frame()->jScriptEnabled() && /* jscript allowed at all? */
                     view /* are we a regular tokenizer or just for innerHTML ? */
                    ) {
                    if ((a = currToken.attrs->getAttributeItem(srcAttr)))
                        scriptSrc = parser->doc()->completeURL(parseURL(a->value()).qstring());
                    if ((a = currToken.attrs->getAttributeItem(charsetAttr)))
                        scriptSrcCharset = a->value().qstring().stripWhiteSpace();
                    if ( scriptSrcCharset.isEmpty() )
                        scriptSrcCharset = parser->doc()->frame()->encoding();
                    /* Check type before language, since language is deprecated */
                    if ((a = currToken.attrs->getAttributeItem(typeAttr)) != 0 && !a->value().isEmpty())
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
                    DOMString lang = a->value().domString().lower();
                    if( lang != "" &&
                        lang != "javascript" &&
                        lang != "javascript1.0" &&
                        lang != "javascript1.1" &&
                        lang != "javascript1.2" &&
                        lang != "javascript1.3" &&
                        lang != "javascript1.4" &&
                        lang != "javascript1.5" &&
                        lang != "ecmascript" &&
                        lang != "livescript" &&
                        lang != "jscript")
                        javascript = false;
                }
            }

            NodeImpl *n = processToken();

            if (tagName == preTag) {
                state.setDiscardLF(true); // Discard the first LF after we open a pre.
            } else if (tagName == scriptTag) {
                assert(!scriptNode);
                scriptNode = n;
                if (beginTag) {
                    searchStopper = scriptEnd;
                    searchStopperLen = 8;
                    state.setInScript(true);
                    state = parseSpecial(src, state);
                } else if (isSelfClosingScript) { // Handle <script src="foo"/>
                    state.setInScript(true);
                    state = scriptHandler(state);
                }
            } else if (tagName == styleTag) {
                if (beginTag) {
                    searchStopper = styleEnd;
                    searchStopperLen = 7;
                    state.setInStyle(true);
                    state = parseSpecial(src, state);
                }
            } else if (tagName == textareaTag) {
                if(beginTag) {
                    searchStopper = textareaEnd;
                    searchStopperLen = 10;
                    state.setInTextArea(true);
                    state = parseSpecial(src, state);
                }
            } else if (tagName == titleTag) {
                 if (beginTag) {
                    searchStopper = titleEnd;
                    searchStopperLen = 7;
                    state.setInTitle(true);
                    state = parseSpecial(src, state);
                }
            } else if (tagName == xmpTag) {
                if (beginTag) {
                    searchStopper = xmpEnd;
                    searchStopperLen = 5;
                    state.setInXmp(true);
                    state = parseSpecial(src, state);
                }
            } else if (tagName == selectTag)
                state.setInSelect(beginTag);
            else if (tagName == plaintextTag)
                state.setInPlainText(beginTag);
            m_cBufferPos = cBufferPos;
            return state; // Finished parsing tag!
        }
        } // end switch
    }
    m_cBufferPos = cBufferPos;
    return state;
}

inline bool HTMLTokenizer::continueProcessing(int& processedCount, const QTime& startTime, State &state)
{
    // We don't want to be checking elapsed time with every character, so we only check after we've
    // processed a certain number of characters.
    bool allowedYield = state.allowYield();
    state.setAllowYield(false);
    if (!state.loadingExtScript() && !state.forceSynchronous() && !m_executingScript && (processedCount > TOKENIZER_CHUNK_SIZE || allowedYield)) {
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
            if (startTime.elapsed() > TOKENIZER_TIME_DELAY)
                printf("Deferring processing of data because 200ms elapsed away from event loop.\n");
#endif
            return false;
        }
    }
    
    processedCount++;
    return true;
}

bool HTMLTokenizer::write(const SegmentedString &str, bool appendData)
{
#ifdef TOKEN_DEBUG
    kdDebug( 6036 ) << this << " Tokenizer::write(\"" << str.toString() << "\"," << appendData << ")" << endl;
#endif

    if (!buffer)
        return false;
    
    if (m_parserStopped)
        return false;

    if ( ( m_executingScript && appendData ) || !pendingScripts.isEmpty() ) {
        // don't parse; we will do this later
	if (currentPrependingSrc) {
	    currentPrependingSrc->append(str);
	} else {
	    pendingSrc.append(str);
	}
        return false;
    }

    if (onHold) {
        src.append(str);
        return false;
    }
    
    if (!src.isEmpty())
        src.append(str);
    else
        setSrc(str);

    // Once a timer is set, it has control of when the tokenizer continues.
    if (timerId)
        return false;

    bool wasInWrite = inWrite;
    inWrite = true;
    
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!parser->doc()->ownerElement())
        printf("Beginning write at time %d\n", parser->doc()->elapsedTime());
#endif
    
    int processedCount = 0;
    QTime startTime;
    startTime.start();

    Frame *frame = parser->doc()->frame();

    State state = m_state;

    while (!src.isEmpty() && (!frame || !frame->isScheduledLocationChangePending())) {
        if (!continueProcessing(processedCount, startTime, state))
            break;

        // do we need to enlarge the buffer?
        checkBuffer();

        ushort cc = src->unicode();

        bool wasSkipLF = state.skipLF();
        if (wasSkipLF)
            state.setSkipLF(false);

        if (wasSkipLF && (cc == '\n'))
            ++src;
        else if (state.needsSpecialWriteHandling()) {
            // it's important to keep needsSpecialWriteHandling with the flags this block tests
            if (state.hasEntityState())
                state = parseEntity(src, dest, state, m_cBufferPos, false, state.hasTagState());
            else if (state.inPlainText())
                state = parseText(src, state);
            else if (state.inAnySpecial())
                state = parseSpecial(src, state);
            else if (state.inComment())
                state = parseComment(src, state);
            else if (state.inServer())
                state = parseServer(src, state);
            else if (state.inProcessingInstruction())
                state = parseProcessingInstruction(src, state);
            else if (state.hasTagState())
                state = parseTag(src, state);
            else if (state.startTag()) {
                state.setStartTag(false);
                
                switch(cc) {
                case '/':
                    break;
                case '!': {
                    // <!-- comment -->
                    searchCount = 1; // Look for '<!--' sequence to start comment
                    
                    break;
                }
                case '?': {
                    // xml processing instruction
                    state.setInProcessingInstruction(true);
                    tquote = NoQuote;
                    state = parseProcessingInstruction(src, state);
                    continue;

                    break;
                }
                case '%':
                    if (!brokenServer) {
                        // <% server stuff, handle as comment %>
                        state.setInServer(true);
                        tquote = NoQuote;
                        state = parseServer(src, state);
                        continue;
                    }
                    // else fall through
                default: {
                    if( ((cc >= 'a') && (cc <= 'z')) || ((cc >= 'A') && (cc <= 'Z'))) {
                        // Start of a Start-Tag
                    } else {
                        // Invalid tag
                        // Add as is
                        *dest = '<';
                        dest++;
                        continue;
                    }
                }
                }; // end case

                processToken();

                m_cBufferPos = 0;
                state.setTagState(TagName);
                state = parseTag(src, state);
            }
        } else if (cc == '&' && !src.escaped()) {
            ++src;
            state = parseEntity(src, dest, state, m_cBufferPos, true, state.hasTagState());
        } else if (cc == '<' && !src.escaped()) {
            tagStartLineno = lineno+src.lineCount();
            ++src;
            state.setStartTag(true);
        } else if (cc == '\n' || cc == '\r') {
            if (state.discardLF())
                // Ignore this LF
                state.setDiscardLF(false); // We have discarded 1 LF
            else
                // Process this LF
                *dest++ = '\n';
            
            /* Check for MS-DOS CRLF sequence */
            if (cc == '\r')
                state.setSkipLF(true);
            ++src;
        } else {
            state.setDiscardLF(false);
#if QT_VERSION < 300
            unsigned char row = src->row();
            if ( row > 0x05 && row < 0x10 || row > 0xfd )
                    currToken.complexText = true;
#endif
            *dest++ = fixUpChar(*src);
            ++src;
        }
    }
    
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!parser->doc()->ownerElement())
        printf("Ending write at time %d\n", parser->doc()->elapsedTime());
#endif
    
    inWrite = wasInWrite;

    m_state = state;

    if (noMoreData && !inWrite && !state.loadingExtScript() && !m_executingScript && !timerId) {
        end(); // this actually causes us to be deleted
        return true;
    }
    return false;
}

void HTMLTokenizer::stopParsing()
{
    Tokenizer::stopParsing();
    if (timerId) {
        killTimer(timerId);
        timerId = 0;
    }

    // The part needs to know that the tokenizer has finished with its data,
    // regardless of whether it happened naturally or due to manual intervention.
    if (view && view->frame())
      view->frame()->tokenizerProcessedData();
}

bool HTMLTokenizer::processingData() const
{
    return timerId != 0;
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
        QGuardedPtr<FrameView> savedView = view;
        bool didCallEnd = write(SegmentedString(), true);
      
        // If we called end() during the write,  we need to let WebKit know that we're done processing the data.
        if (didCallEnd && savedView) {
            Frame *frame = savedView->frame();
            if (frame) {
                frame->tokenizerProcessedData();
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
    if (!m_state.hasTagState())
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
    while((m_state.inComment() || m_state.inServer()) && scriptCode && scriptCodeSize) {
        // we've found an unmatched comment start
        if (m_state.inComment())
            brokenComments = true;
        else
            brokenServer = true;
        checkScriptBuffer();
        scriptCode[scriptCodeSize] = 0;
        scriptCode[scriptCodeSize + 1] = 0;
        int pos;
        QString food;
        if (m_state.inScript() || m_state.inStyle())
            food.setUnicode(scriptCode, scriptCodeSize);
        else if (m_state.inServer()) {
            food = "<";
            food += QString(scriptCode, scriptCodeSize);
        } else {
            pos = QConstString(scriptCode, scriptCodeSize).string().find('>');
            food.setUnicode(scriptCode+pos+1, scriptCodeSize-pos-1); // deep copy
        }
        KHTML_DELETE_QCHAR_VEC(scriptCode);
        scriptCode = 0;
        scriptCodeSize = scriptCodeMaxSize = scriptCodeResync = 0;
        m_state.setInComment(false);
        m_state.setInServer(false);
        if (!food.isEmpty())
            write(food, true);
    }
    // this indicates we will not receive any more data... but if we are waiting on
    // an external script to load, we can't finish parsing until that is done
    noMoreData = true;
    if (!inWrite && !m_state.loadingExtScript() && !m_executingScript && !onHold && !timerId)
        end(); // this actually causes us to be deleted
}

NodeImpl *HTMLTokenizer::processToken()
{
    KJSProxyImpl *jsProxy = (view && view->frame()) ? view->frame()->jScript() : 0L;    
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
        if (currToken.tagName != commentAtom)
            currToken.tagName = textAtom;
    }
    else if (currToken.tagName == nullAtom) {
        currToken.reset();
        if (jsProxy)
            jsProxy->setEventHandlerLineno(lineno+src.lineCount());
        return 0;
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
    NodeImpl *n = 0;
    
    if (!m_parserStopped) {
        // pass the token over to the parser, the parser DOES NOT delete the token
        n = parser->parseToken(&currToken);
    }
    
    currToken.reset();
    if (jsProxy)
        jsProxy->setEventHandlerLineno(0);
    return n;
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

    buffer = (QChar*)fastRealloc(buffer, newsize*sizeof(QChar));
    dest = buffer + oldoffs;
    size = newsize;
}

void HTMLTokenizer::enlargeScriptBuffer(int len)
{
    int newsize = kMax(scriptCodeMaxSize*2, scriptCodeMaxSize+len);
    scriptCode = (QChar*)fastRealloc(scriptCode, newsize*sizeof(QChar));
    scriptCodeMaxSize = newsize;
}

void HTMLTokenizer::notifyFinished(CachedObject */*finishedObj*/)
{
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!parser->doc()->ownerElement())
        printf("script loaded at %d\n", parser->doc()->elapsedTime());
#endif

    assert(!pendingScripts.isEmpty());
    bool finished = false;
    while (!finished && pendingScripts.head()->isLoaded()) {
#ifdef TOKEN_DEBUG
        kdDebug( 6036 ) << "Finished loading an external script" << endl;
#endif
        CachedScript* cs = pendingScripts.dequeue();
        assert(cs->accessCount() > 0);

        DOMString scriptSource = cs->script();
#ifdef TOKEN_DEBUG
        kdDebug( 6036 ) << "External script is:" << endl << scriptSource.qstring() << endl;
#endif
        setSrc(SegmentedString());

        // make sure we forget about the script before we execute the new one
        // infinite recursion might happen otherwise
        QString cachedScriptUrl( cs->url().qstring() );
        bool errorOccurred = cs->errorOccurred();
        cs->deref(this);
        RefPtr<NodeImpl> n = scriptNode;
        scriptNode = 0;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
        if (!parser->doc()->ownerElement())
            printf("external script beginning execution at %d\n", parser->doc()->elapsedTime());
#endif

	if (errorOccurred)
            n->dispatchHTMLEvent(errorEvent, false, false);
        else {
            m_state = scriptExecution(scriptSource.qstring(), m_state, cachedScriptUrl);
            n->dispatchHTMLEvent(loadEvent, false, false);
        }

        // The state of pendingScripts.isEmpty() can change inside the scriptExecution()
        // call above, so test afterwards.
        finished = pendingScripts.isEmpty();
        if (finished) {
            m_state.setLoadingExtScript(false);
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
            if (!parser->doc()->ownerElement())
                printf("external script finished execution at %d\n", parser->doc()->elapsedTime());
#endif
        }

        // 'inScript' is true when we are called synchronously from
        // parseScript(). In that case parseScript() will take care
        // of 'scriptOutput'.
        if (!m_state.inScript()) {
            SegmentedString rest = pendingSrc;
            pendingSrc.clear();
            write(rest, false);
            // we might be deleted at this point, do not
            // access any members.
        }
    }
}

bool HTMLTokenizer::isWaitingForScripts() const
{
    return m_state.loadingExtScript();
}

void HTMLTokenizer::setSrc(const SegmentedString &source)
{
    lineno += src.lineCount();
    src = source;
    src.resetLineCount();
}

void HTMLTokenizer::setOnHold(bool _onHold)
{
    onHold = _onHold;
}

void parseHTMLDocumentFragment(const DOMString &source, DocumentFragmentImpl *fragment)
{
    HTMLTokenizer tok(fragment->getDocument(), fragment);
    tok.setForceSynchronous(true);
    tok.write(source.qstring(), true);
    tok.finish();
    assert(!tok.processingData());      // make sure we're done (see 3963151)
}

}
