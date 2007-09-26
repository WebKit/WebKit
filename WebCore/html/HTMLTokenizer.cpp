/*
    This file is part of the KDE libraries

    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)
              (C) 1998 Waldo Bastian (bastian@kde.org)
              (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc.
    Copyright (C) 2005, 2006 Alexey Proskuryakov (ap@nypop.com)

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "HTMLTokenizer.h"

#include "CSSHelper.h"
#include "Cache.h"
#include "CachedScript.h"
#include "DocLoader.h"
#include "DocumentFragment.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "HTMLParser.h"
#include "HTMLScriptElement.h"
#include "HTMLViewSourceDocument.h"
#include "Settings.h"
#include "SystemTime.h"
#include "kjs_proxy.h"

#include "HTMLEntityNames.c"

// #define INSTRUMENT_LAYOUT_SCHEDULING 1

#if MOBILE
// The mobile device needs to be responsive, as such the tokenizer chunk size is reduced.
// This value is used to define how many characters the tokenizer will process before 
// yeilding control.
#define TOKENIZER_CHUNK_SIZE  256
#else
#define TOKENIZER_CHUNK_SIZE  4096
#endif

using namespace std;

namespace WebCore {

using namespace HTMLNames;
using namespace EventNames;

#if MOBILE
// As the chunks are smaller (above), the tokenizer should not yield for as long a period, otherwise
// it will take way to long to load a page.
const double tokenizerTimeDelay = 0.300;

#else
// FIXME: We would like this constant to be 200ms.
// Yielding more aggressively results in increased responsiveness and better incremental rendering.
// It slows down overall page-load on slower machines, though, so for now we set a value of 500.
const double tokenizerTimeDelay = 0.500;
#endif

static const char commentStart [] = "<!--";
static const char scriptEnd [] = "</script";
static const char xmpEnd [] = "</xmp";
static const char styleEnd [] =  "</style";
static const char textareaEnd [] = "</textarea";
static const char titleEnd [] = "</title";

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

// We only need this for entities. For non-entity text, we handle this in the text encoding.

static const UChar windowsLatin1ExtensionArray[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, // 80-87
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F, // 88-8F
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, // 90-97
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178  // 98-9F
};

static inline UChar fixUpChar(UChar c)
{
    if ((c & ~0x1F) != 0x0080)
        return c;
    return windowsLatin1ExtensionArray[c - 0x80];
}

static inline bool tagMatch(const char* s1, const UChar* s2, unsigned length)
{
    for (unsigned i = 0; i != length; ++i) {
        unsigned char c1 = s1[i];
        unsigned char uc1 = toupper(c1);
        UChar c2 = s2[i];
        if (c1 != c2 && uc1 != c2)
            return false;
    }
    return true;
}

inline void Token::addAttribute(Document* doc, AtomicString& attrName, const AtomicString& v, bool viewSourceMode)
{
    if (!attrName.isEmpty()) {
        ASSERT(!attrName.contains('/'));
        Attribute* a = new MappedAttribute(attrName, v);
        if (!attrs)
            attrs = new NamedMappedAttrMap(0);
        attrs->insertAttribute(a, viewSourceMode);
    }
    
    attrName = emptyAtom;
}

// ----------------------------------------------------------------------------

HTMLTokenizer::HTMLTokenizer(HTMLDocument* doc, bool reportErrors)
    : Tokenizer()
    , buffer(0)
    , scriptCode(0)
    , scriptCodeSize(0)
    , scriptCodeMaxSize(0)
    , scriptCodeResync(0)
    , m_executingScript(0)
    , m_requestingScript(false)
    , m_hasScriptsWaitingForStylesheets(false)
    , m_timer(this, &HTMLTokenizer::timerFired)
    , m_doc(doc)
    , parser(new HTMLParser(doc, reportErrors))
    , inWrite(false)
    , m_fragment(false)
{
    begin();
}

HTMLTokenizer::HTMLTokenizer(HTMLViewSourceDocument* doc)
    : Tokenizer(true)
    , buffer(0)
    , scriptCode(0)
    , scriptCodeSize(0)
    , scriptCodeMaxSize(0)
    , scriptCodeResync(0)
    , m_executingScript(0)
    , m_requestingScript(false)
    , m_hasScriptsWaitingForStylesheets(false)
    , m_timer(this, &HTMLTokenizer::timerFired)
    , m_doc(doc)
    , parser(0)
    , inWrite(false)
    , m_fragment(false)
{
    begin();
}

HTMLTokenizer::HTMLTokenizer(DocumentFragment* frag)
    : buffer(0)
    , scriptCode(0)
    , scriptCodeSize(0)
    , scriptCodeMaxSize(0)
    , scriptCodeResync(0)
    , m_executingScript(0)
    , m_requestingScript(false)
    , m_hasScriptsWaitingForStylesheets(false)
    , m_timer(this, &HTMLTokenizer::timerFired)
    , m_doc(frag->document())
    , inWrite(false)
    , m_fragment(true)
{
    parser = new HTMLParser(frag);
    begin();
}

void HTMLTokenizer::reset()
{
    ASSERT(m_executingScript == 0);

    while (!pendingScripts.isEmpty()) {
      CachedScript *cs = pendingScripts.dequeue();
      ASSERT(cache()->disabled() || cs->accessCount() > 0);
      cs->deref(this);
    }
    
    fastFree(buffer);
    buffer = dest = 0;
    size = 0;

    fastFree(scriptCode);
    scriptCode = 0;
    scriptCodeSize = scriptCodeMaxSize = scriptCodeResync = 0;

    m_timer.stop();
    m_state.setAllowYield(false);
    m_state.setForceSynchronous(false);

    currToken.reset();
}

void HTMLTokenizer::begin()
{
    m_executingScript = 0;
    m_requestingScript = false;
    m_hasScriptsWaitingForStylesheets = false;
    m_state.setLoadingExtScript(false);
    reset();
    size = 254;
    buffer = static_cast<UChar*>(fastMalloc(sizeof(UChar) * 254));
    dest = buffer;
    tquote = NoQuote;
    searchCount = 0;
    m_state.setEntityState(NoEntity);
    scriptSrc = String();
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
                list.advance(0);
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

            list.advance(0);
        } else {
            state.setDiscardLF(false);
            *dest++ = *list;
            list.advance(0);
        }
    }

    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseSpecial(SegmentedString &src, State state)
{
    ASSERT(state.inTextArea() || state.inTitle() || !state.hasEntityState());
    ASSERT(!state.hasTagState());
    ASSERT(state.inXmp() + state.inTextArea() + state.inTitle() + state.inStyle() + state.inScript() == 1 );
    if (state.inScript())
        scriptStartLineno = lineno;

    if (state.inComment()) 
        state = parseComment(src, state);

    while ( !src.isEmpty() ) {
        checkScriptBuffer();
        UChar ch = *src;

        if (!scriptCodeResync && !brokenComments && !state.inTextArea() && !state.inXmp() && ch == '-' && scriptCodeSize >= 3 && !src.escaped() && scriptCode[scriptCodeSize-3] == '<' && scriptCode[scriptCodeSize-2] == '!' && scriptCode[scriptCodeSize-1] == '-') {
            state.setInComment(true);
            state = parseComment(src, state);
            continue;
        }
        if (scriptCodeResync && !tquote && ch == '>') {
            src.advance(lineNumberPtr());
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
        if (!scriptCodeResync && !state.escaped() && !src.escaped() && (ch == '>' || ch == '/' || ch <= ' ') && ch &&
             scriptCodeSize >= searchStopperLen &&
             tagMatch( searchStopper, scriptCode+scriptCodeSize-searchStopperLen, searchStopperLen )) {
            scriptCodeResync = scriptCodeSize-searchStopperLen+1;
            tquote = NoQuote;
            continue;
        }
        if (scriptCodeResync && !state.escaped()) {
            if (ch == '\"')
                tquote = (tquote == NoQuote) ? DoubleQuote : ((tquote == SingleQuote) ? SingleQuote : NoQuote);
            else if (ch == '\'')
                tquote = (tquote == NoQuote) ? SingleQuote : (tquote == DoubleQuote) ? DoubleQuote : NoQuote;
            else if (tquote != NoQuote && (ch == '\r' || ch == '\n'))
                tquote = NoQuote;
        }
        state.setEscaped(!state.escaped() && ch == '\\');
        if (!scriptCodeResync && (state.inTextArea() || state.inTitle()) && !src.escaped() && ch == '&') {
            UChar* scriptCodeDest = scriptCode+scriptCodeSize;
            src.advance(lineNumberPtr());
            state = parseEntity(src, scriptCodeDest, state, m_cBufferPos, true, false);
            scriptCodeSize = scriptCodeDest-scriptCode;
        } else {
            scriptCode[scriptCodeSize++] = *src;
            src.advance(lineNumberPtr());
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
    bool followingFrameset = (m_doc->body() && m_doc->body()->hasTagName(framesetTag));
  
    CachedScript* cs = 0;
    // don't load external scripts for standalone documents (for now)
    if (!inViewSourceMode()) {
        if (!scriptSrc.isEmpty() && m_doc->frame()) {
            // forget what we just got; load from src url instead
            if (!parser->skipMode() && !followingFrameset) {
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
                if (!m_doc->ownerElement())
                    printf("Requesting script at time %d\n", m_doc->elapsedTime());
#endif
                // The parser might have been stopped by for example a window.close call in an earlier script.
                // If so, we don't want to load scripts.
                if (!m_parserStopped && (cs = m_doc->docLoader()->requestScript(scriptSrc, scriptSrcCharset)))
                    pendingScripts.enqueue(cs);
                else
                    scriptNode = 0;
            } else
                scriptNode = 0;
            scriptSrc = String();
        } else {
#ifdef TOKEN_DEBUG
            kdDebug( 6036 ) << "---START SCRIPT---" << endl;
            kdDebug( 6036 ) << DeprecatedString(scriptCode, scriptCodeSize) << endl;
            kdDebug( 6036 ) << "---END SCRIPT---" << endl;
#endif
            // Parse scriptCode containing <script> info
#if USE(LOW_BANDWIDTH_DISPLAY)
            if (m_doc->inLowBandwidthDisplay()) {
                // ideal solution is only skipping internal JavaScript if there is external JavaScript.
                // but internal JavaScript can use document.write() to create an external JavaScript,
                // so we have to skip internal JavaScript all the time.
                m_doc->frame()->loader()->needToSwitchOutLowBandwidthDisplay();
                doScriptExec = false;
            } else
#endif
            doScriptExec = static_cast<HTMLScriptElement*>(scriptNode.get())->shouldExecuteAsJavaScript();
            scriptNode = 0;
        }
    }

    state = processListing(SegmentedString(scriptCode, scriptCodeSize), state);
    DeprecatedString exScript(reinterpret_cast<DeprecatedChar*>(buffer), dest - buffer);
    processToken();
    currToken.tagName = scriptTag.localName();
    currToken.beginTag = false;
    processToken();

    state.setInScript(false);
    
    // FIXME: The script should be syntax highlighted.
    if (inViewSourceMode())
        return state;

    SegmentedString *savedPrependingSrc = currentPrependingSrc;
    SegmentedString prependingSrc;
    currentPrependingSrc = &prependingSrc;
    scriptCodeSize = scriptCodeResync = 0;

    if (!parser->skipMode() && !followingFrameset) {
        if (cs) {
            if (savedPrependingSrc)
                savedPrependingSrc->append(src);
            else
                pendingSrc.prepend(src);
            setSrc(SegmentedString());

            // the ref() call below may call notifyFinished if the script is already in cache,
            // and that mucks with the state directly, so we must write it back to the object.
            m_state = state;
            bool savedRequestingScript = m_requestingScript;
            m_requestingScript = true;
            cs->ref(this);
            m_requestingScript = savedRequestingScript;
            state = m_state;
            // will be 0 if script was already loaded and ref() executed it
            if (!pendingScripts.isEmpty())
                state.setLoadingExtScript(true);
        } else if (!m_fragment && doScriptExec) {
            if (!m_executingScript)
                pendingSrc.prepend(src);
            else
                prependingSrc = src;
            setSrc(SegmentedString());
            state = scriptExecution(exScript, state, DeprecatedString::null, scriptStartLineno);
        }
    }

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

HTMLTokenizer::State HTMLTokenizer::scriptExecution(const DeprecatedString& str, State state, DeprecatedString scriptURL, int baseLine)
{
    if (m_fragment || !m_doc->frame())
        return state;
    m_executingScript++;
    DeprecatedString url = scriptURL.isNull() ? m_doc->frame()->document()->URL() : scriptURL;

    SegmentedString *savedPrependingSrc = currentPrependingSrc;
    SegmentedString prependingSrc;
    currentPrependingSrc = &prependingSrc;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_doc->ownerElement())
        printf("beginning script execution at %d\n", m_doc->elapsedTime());
#endif

    m_state = state;
    m_doc->frame()->loader()->executeScript(url, baseLine, str);
    state = m_state;

    state.setAllowYield(true);

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_doc->ownerElement())
        printf("ending script execution at %d\n", m_doc->elapsedTime());
#endif
    
    m_executingScript--;

    if (!m_executingScript && !state.loadingExtScript()) {
        pendingSrc.prepend(prependingSrc);        
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
            if (currentPrependingSrc)
                currentPrependingSrc->append(prependingSrc);
            else
                pendingSrc.prepend(prependingSrc);
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
    checkScriptBuffer(src.length());
    while ( !src.isEmpty() ) {
        scriptCode[ scriptCodeSize++ ] = *src;

        if (*src == '>') {
            bool handleBrokenComments = brokenComments && !(state.inScript() || state.inStyle());
            int endCharsCount = 1; // start off with one for the '>' character
            if (scriptCodeSize > 2 && scriptCode[scriptCodeSize-3] == '-' && scriptCode[scriptCodeSize-2] == '-') {
                endCharsCount = 3;
            }
            else if (scriptCodeSize > 3 && scriptCode[scriptCodeSize-4] == '-' && scriptCode[scriptCodeSize-3] == '-' && 
                scriptCode[scriptCodeSize-2] == '!') {
                // Other browsers will accept --!> as a close comment, even though it's
                // not technically valid.
                endCharsCount = 4;
            }
            if (handleBrokenComments || endCharsCount > 1) {
                src.advance(lineNumberPtr());
                if (!(state.inTitle() || state.inScript() || state.inXmp() || state.inTextArea() || state.inStyle())) {
                    checkScriptBuffer();
                    scriptCode[scriptCodeSize] = 0;
                    scriptCode[scriptCodeSize + 1] = 0;
                    currToken.tagName = commentAtom;
                    currToken.beginTag = true;
                    state = processListing(SegmentedString(scriptCode, scriptCodeSize - endCharsCount), state);
                    processToken();
                    currToken.tagName = commentAtom;
                    currToken.beginTag = false;
                    processToken();
                    scriptCodeSize = 0;
                }
                state.setInComment(false);
                return state; // Finished parsing comment
            }
        }
        src.advance(lineNumberPtr());
    }

    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseServer(SegmentedString& src, State state)
{
    checkScriptBuffer(src.length());
    while (!src.isEmpty()) {
        scriptCode[scriptCodeSize++] = *src;
        if (*src == '>' &&
            scriptCodeSize > 1 && scriptCode[scriptCodeSize-2] == '%') {
            src.advance(lineNumberPtr());
            state.setInServer(false);
            scriptCodeSize = 0;
            return state; // Finished parsing server include
        }
        src.advance(lineNumberPtr());
    }
    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseProcessingInstruction(SegmentedString &src, State state)
{
    UChar oldchar = 0;
    while (!src.isEmpty()) {
        UChar chbegin = *src;
        if (chbegin == '\'')
            tquote = tquote == SingleQuote ? NoQuote : SingleQuote;
        else if (chbegin == '\"')
            tquote = tquote == DoubleQuote ? NoQuote : DoubleQuote;
        // Look for '?>'
        // Some crappy sites omit the "?" before it, so
        // we look for an unquoted '>' instead. (IE compatible)
        else if (chbegin == '>' && (!tquote || oldchar == '?')) {
            // We got a '?>' sequence
            state.setInProcessingInstruction(false);
            src.advance(lineNumberPtr());
            state.setDiscardLF(true);
            return state; // Finished parsing comment!
        }
        src.advance(lineNumberPtr());
        oldchar = chbegin;
    }
    
    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseText(SegmentedString &src, State state)
{
    while (!src.isEmpty()) {
        UChar cc = *src;

        if (state.skipLF()) {
            state.setSkipLF(false);
            if (cc == '\n') {
                src.advance(lineNumberPtr());
                continue;
            }
        }

        // do we need to enlarge the buffer?
        checkBuffer();

        if (cc == '\r') {
            state.setSkipLF(true);
            *dest++ = '\n';
        } else
            *dest++ = cc;
        src.advance(lineNumberPtr());
    }

    return state;
}


HTMLTokenizer::State HTMLTokenizer::parseEntity(SegmentedString &src, UChar*& dest, State state, unsigned &cBufferPos, bool start, bool parsingTag)
{
    if (start)
    {
        cBufferPos = 0;
        state.setEntityState(SearchEntity);
        EntityUnicodeValue = 0;
    }

    while(!src.isEmpty())
    {
        UChar cc = *src;
        switch(state.entityState()) {
        case NoEntity:
            ASSERT(state.entityState() != NoEntity);
            return state;
        
        case SearchEntity:
            if(cc == '#') {
                cBuffer[cBufferPos++] = cc;
                src.advance(lineNumberPtr());
                state.setEntityState(NumericSearch);
            }
            else
                state.setEntityState(EntityName);

            break;

        case NumericSearch:
            if (cc == 'x' || cc == 'X') {
                cBuffer[cBufferPos++] = cc;
                src.advance(lineNumberPtr());
                state.setEntityState(Hexadecimal);
            } else if (cc >= '0' && cc <= '9')
                state.setEntityState(Decimal);
            else
                state.setEntityState(SearchSemicolon);
            break;

        case Hexadecimal: {
            int ll = min(src.length(), 10 - cBufferPos);
            while (ll--) {
                cc = *src;
                if (!((cc >= '0' && cc <= '9') || (cc >= 'a' && cc <= 'f') || (cc >= 'A' && cc <= 'F'))) {
                    state.setEntityState(SearchSemicolon);
                    break;
                }
                int digit;
                if (cc < 'A')
                    digit = cc - '0';
                else
                    digit = (cc - 'A' + 10) & 0xF; // handle both upper and lower case without a branch
                EntityUnicodeValue = EntityUnicodeValue * 16 + digit;
                cBuffer[cBufferPos++] = cc;
                src.advance(lineNumberPtr());
            }
            if (cBufferPos == 10)  
                state.setEntityState(SearchSemicolon);
            break;
        }
        case Decimal:
        {
            int ll = min(src.length(), 9-cBufferPos);
            while(ll--) {
                cc = *src;

                if (!(cc >= '0' && cc <= '9')) {
                    state.setEntityState(SearchSemicolon);
                    break;
                }

                EntityUnicodeValue = EntityUnicodeValue * 10 + (cc - '0');
                cBuffer[cBufferPos++] = cc;
                src.advance(lineNumberPtr());
            }
            if (cBufferPos == 9)  
                state.setEntityState(SearchSemicolon);
            break;
        }
        case EntityName:
        {
            int ll = min(src.length(), 9-cBufferPos);
            while(ll--) {
                cc = *src;

                if (!((cc >= 'a' && cc <= 'z') || (cc >= '0' && cc <= '9') || (cc >= 'A' && cc <= 'Z'))) {
                    state.setEntityState(SearchSemicolon);
                    break;
                }

                cBuffer[cBufferPos++] = cc;
                src.advance(lineNumberPtr());
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
            if (EntityUnicodeValue > 0 && EntityUnicodeValue <= 0x10FFFF) {
                if (!inViewSourceMode()) {
                    if (*src == ';')
                        src.advance(lineNumberPtr());
                    if (EntityUnicodeValue <= 0xFFFF) {
                        checkBuffer();
                        src.push(fixUpChar(EntityUnicodeValue));
                    } else {
                        // Convert to UTF-16, using surrogate code points.
                        checkBuffer(2);
                        src.push(U16_LEAD(EntityUnicodeValue));
                        src.push(U16_TRAIL(EntityUnicodeValue));
                    }
                } else {
                    // FIXME: We should eventually colorize entities by sending them as a special token.
                    checkBuffer(11);
                    *dest++ = '&';
                    for (unsigned i = 0; i < cBufferPos; i++)
                        dest[i] = cBuffer[i];
                    dest += cBufferPos;
                    if (*src == ';') {
                        *dest++ = ';';
                        src.advance(lineNumberPtr());
                    }
                }
            } else {
                checkBuffer(10);
                // ignore the sequence, add it to the buffer as plaintext
                *dest++ = '&';
                for (unsigned i = 0; i < cBufferPos; i++)
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
    ASSERT(!state.hasEntityState());

    unsigned cBufferPos = m_cBufferPos;

    int* lineNoPtr = lineNumberPtr();
    bool lastIsSlash = false;

    while (!src.isEmpty()) {
        checkBuffer();
        switch(state.tagState()) {
        case NoTag:
        {
            m_cBufferPos = cBufferPos;
            return state;
        }
        case TagName:
        {
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 1
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
                        src.advance(lineNoPtr);
                        dest = buffer; // ignore the previous part of this tag
                        state.setInComment(true);
                        state.setTagState(NoTag);

                        // Fix bug 34302 at kde.bugs.org.  Go ahead and treat
                        // <!--> as a valid comment, since both mozilla and IE on windows
                        // can handle this case.  Only do this in quirks mode. -dwh
                        if (!src.isEmpty() && *src == '>' && m_doc->inCompatMode()) {
                          state.setInComment(false);
                          src.advance(lineNoPtr);
                          if (!src.isEmpty())
                              // cuts off high bits, which is okay
                              cBuffer[cBufferPos++] = *src;
                        }
                        else
                          state = parseComment(src, state);

                        m_cBufferPos = cBufferPos;
                        return state; // Finished parsing tag!
                    }
                    // cuts off high bits, which is okay
                    cBuffer[cBufferPos++] = *src;
                    src.advance(lineNoPtr);
                    break;
                }
                else
                    searchCount = 0; // Stop looking for '<!--' sequence
            }

            bool finish = false;
            unsigned int ll = min(src.length(), CBUFLEN - cBufferPos);
            while (ll--) {
                UChar curchar = *src;
                if (curchar <= ' ' || curchar == '>' || curchar == '<') {
                    finish = true;
                    break;
                }
                
                // tolower() shows up on profiles. This is faster!
                if (curchar >= 'A' && curchar <= 'Z' && !inViewSourceMode())
                    cBuffer[cBufferPos++] = curchar + ('a' - 'A');
                else
                    cBuffer[cBufferPos++] = curchar;
                src.advance(lineNoPtr);
            }

            // Disadvantage: we add the possible rest of the tag
            // as attribute names. ### judge if this causes problems
            if(finish || CBUFLEN == cBufferPos) {
                bool beginTag;
                char* ptr = cBuffer;
                unsigned int len = cBufferPos;
                cBuffer[cBufferPos] = '\0';
                if ((cBufferPos > 0) && (*ptr == '/')) {
                    // End Tag
                    beginTag = false;
                    ptr++;
                    len--;
                }
                else
                    // Start Tag
                    beginTag = true;

                // Ignore the / in fake xml tags like <br/>.  We trim off the "/" so that we'll get "br" as the tag name and not "br/".
                if (len > 1 && ptr[len-1] == '/' && !inViewSourceMode())
                    ptr[--len] = '\0';

                // Now that we've shaved off any invalid / that might have followed the name), make the tag.
                // FIXME: FireFox and WinIE turn !foo nodes into comments, we ignore comments. (fast/parser/tag-with-exclamation-point.html)
                if (ptr[0] != '!' || inViewSourceMode()) {
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
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 1
            qDebug("SearchAttribute");
#endif
            while(!src.isEmpty()) {
                UChar curchar = *src;
                // In this mode just ignore any quotes we encounter and treat them like spaces.
                if (curchar > ' ' && curchar != '\'' && curchar != '"') {
                    if (curchar == '<' || curchar == '>')
                        state.setTagState(SearchEnd);
                    else
                        state.setTagState(AttributeName);

                    cBufferPos = 0;
                    break;
                }
                if (inViewSourceMode())
                    currToken.addViewSourceChar(curchar);
                src.advance(lineNoPtr);
            }
            break;
        case AttributeName:
        {
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 1
            qDebug("AttributeName");
#endif
            int ll = min(src.length(), CBUFLEN-cBufferPos);
            while(ll--) {
                UChar curchar = *src;
                // If we encounter a "/" when scanning an attribute name, treat it as a delimiter.  This allows the 
                // cases like <input type=checkbox checked/> to work (and accommodates XML-style syntax as per HTML5).
                if (curchar <= '>' && (curchar >= '<' || curchar <= ' ' || curchar == '/')) {
                    cBuffer[cBufferPos] = '\0';
                    attrName = AtomicString(cBuffer);
                    dest = buffer;
                    *dest++ = 0;
                    state.setTagState(SearchEqual);
                    if (inViewSourceMode())
                        currToken.addViewSourceChar('a');
                    break;
                }
                
                // tolower() shows up on profiles. This is faster!
                if (curchar >= 'A' && curchar <= 'Z' && !inViewSourceMode())
                    cBuffer[cBufferPos++] = curchar + ('a' - 'A');
                else
                    cBuffer[cBufferPos++] = curchar;
                    
                src.advance(lineNoPtr);
            }
            if ( cBufferPos == CBUFLEN ) {
                cBuffer[cBufferPos] = '\0';
                attrName = AtomicString(cBuffer);
                dest = buffer;
                *dest++ = 0;
                state.setTagState(SearchEqual);
                if (inViewSourceMode())
                    currToken.addViewSourceChar('a');
            }
            break;
        }
        case SearchEqual:
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 1
            qDebug("SearchEqual");
#endif
            while(!src.isEmpty()) {
                UChar curchar = *src;

                if (lastIsSlash && curchar == '>') {
                    // This is a quirk (with a long sad history).  We have to do this
                    // since widgets do <script src="foo.js"/> and expect the tag to close.
                    if (currToken.tagName == scriptTag)
                        currToken.flat = true;
                    currToken.brokenXMLStyle = true;
                }

                // In this mode just ignore any quotes or slashes we encounter and treat them like spaces.
                if (curchar > ' ' && curchar != '\'' && curchar != '"' && curchar != '/') {
                    if(curchar == '=') {
#ifdef TOKEN_DEBUG
                        kdDebug(6036) << "found equal" << endl;
#endif
                        state.setTagState(SearchValue);
                        if (inViewSourceMode())
                            currToken.addViewSourceChar(curchar);
                        src.advance(lineNoPtr);
                    }
                    else {
                        currToken.addAttribute(m_doc, attrName, emptyAtom, inViewSourceMode());
                        dest = buffer;
                        state.setTagState(SearchAttribute);
                        lastIsSlash = false;
                    }
                    break;
                }
                if (inViewSourceMode())
                    currToken.addViewSourceChar(curchar);
                    
                lastIsSlash = curchar == '/';

                src.advance(lineNoPtr);
            }
            break;
        case SearchValue:
            while(!src.isEmpty()) {
                UChar curchar = *src;
                if(curchar > ' ') {
                    if(( curchar == '\'' || curchar == '\"' )) {
                        tquote = curchar == '\"' ? DoubleQuote : SingleQuote;
                        state.setTagState(QuotedValue);
                        if (inViewSourceMode())
                            currToken.addViewSourceChar(curchar);
                        src.advance(lineNoPtr);
                    } else
                        state.setTagState(Value);

                    break;
                }
                if (inViewSourceMode())
                    currToken.addViewSourceChar(curchar);
                src.advance(lineNoPtr);
            }
            break;
        case QuotedValue:
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 1
            qDebug("QuotedValue");
#endif
            while(!src.isEmpty()) {
                checkBuffer();

                UChar curchar = *src;
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
                    currToken.addAttribute(m_doc, attrName, v, inViewSourceMode());
                    if (inViewSourceMode())
                        currToken.addViewSourceChar('x');
                    state.setTagState(SearchAttribute);
                    dest = buffer;
                    tquote = NoQuote;
                    break;
                }
                
                if(curchar <= '\'' && !src.escaped()) {
                    // ### attributes like '&{blaa....};' are supposed to be treated as jscript.
                    if ( curchar == '&' )
                    {
                        src.advance(lineNoPtr);
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
                        if (attrName.isEmpty()) {
                            attrName = v; // Make the name match the value. (FIXME: Is this a WinIE quirk?)
                            if (inViewSourceMode())
                                currToken.addViewSourceChar('x');
                        } else if (inViewSourceMode())
                            currToken.addViewSourceChar('v');
                        currToken.addAttribute(m_doc, attrName, v, inViewSourceMode());
                        dest = buffer;
                        state.setTagState(SearchAttribute);
                        tquote = NoQuote;
                        if (inViewSourceMode())
                            currToken.addViewSourceChar(curchar);
                        src.advance(lineNoPtr);
                        break;
                    }
                }
                *dest++ = *src;
                src.advance(lineNoPtr);
            }
            break;
        case Value:
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 1
            qDebug("Value");
#endif
            while(!src.isEmpty()) {
                checkBuffer();
                UChar curchar = *src;
                if(curchar <= '>' && !src.escaped()) {
                    // parse Entities
                    if ( curchar == '&' )
                    {
                        src.advance(lineNoPtr);
                        state = parseEntity(src, dest, state, cBufferPos, true, true);
                        break;
                    }
                    // no quotes. Every space means end of value
                    // '/' does not delimit in IE!
                    if ( curchar <= ' ' || curchar == '>' )
                    {
                        AtomicString v(buffer+1, dest-buffer-1);
                        currToken.addAttribute(m_doc, attrName, v, inViewSourceMode());
                        if (inViewSourceMode())
                            currToken.addViewSourceChar('v');
                        dest = buffer;
                        state.setTagState(SearchAttribute);
                        break;
                    }
                }

                *dest++ = *src;
                src.advance(lineNoPtr);
            }
            break;
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

                if (inViewSourceMode())
                    currToken.addViewSourceChar(*src);
                src.advance(lineNoPtr);
            }
            if (src.isEmpty()) break;

            searchCount = 0; // Stop looking for '<!--' sequence
            state.setTagState(NoTag);
            tquote = NoQuote;

            if (*src != '<')
                src.advance(lineNoPtr);

            if (currToken.tagName == nullAtom) { //stop if tag is unknown
                m_cBufferPos = cBufferPos;
                return state;
            }

            AtomicString tagName = currToken.tagName;
#if defined(TOKEN_DEBUG) && TOKEN_DEBUG > 0
            kdDebug( 6036 ) << "appending Tag: " << tagName.deprecatedString() << endl;
#endif

            // Handle <script src="foo"/> like Mozilla/Opera. We have to do this now for Dashboard
            // compatibility.
            bool isSelfClosingScript = currToken.flat && currToken.beginTag && currToken.tagName == scriptTag;
            bool beginTag = !currToken.flat && currToken.beginTag;
            if (currToken.beginTag && currToken.tagName == scriptTag && !inViewSourceMode() && !parser->skipMode()) {
                Attribute* a = 0;
                scriptSrc = String();
                scriptSrcCharset = String();
                if (currToken.attrs && !m_fragment) {
                    Settings* settings = m_doc->settings();
                    if (settings && settings->isJavaScriptEnabled()) {
                        if ((a = currToken.attrs->getAttributeItem(srcAttr)))
                            scriptSrc = m_doc->completeURL(parseURL(a->value()));
                        if ((a = currToken.attrs->getAttributeItem(charsetAttr)))
                            scriptSrcCharset = a->value().domString().stripWhiteSpace();
                        if (scriptSrcCharset.isEmpty())
                            scriptSrcCharset = m_doc->frame()->loader()->encoding();
                    }
                }
            }

            RefPtr<Node> n = processToken();
            m_cBufferPos = cBufferPos;
            if (n) {
                if ((tagName == preTag || tagName == listingTag) && !inViewSourceMode()) {
                    if (beginTag)
                        state.setDiscardLF(true); // Discard the first LF after we open a pre.
                } else if (tagName == scriptTag && n) {
                    ASSERT(!scriptNode);
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
                    if (beginTag) {
                        searchStopper = textareaEnd;
                        searchStopperLen = 10;
                        state.setInTextArea(true);
                        state = parseSpecial(src, state);
                    }
                } else if (tagName == titleTag) {
                    if (beginTag) {
                        searchStopper = titleEnd;
                        searchStopperLen = 7;
                        State savedState = state;
                        SegmentedString savedSrc = src;
                        long savedLineno = lineno;
                        state.setInTitle(true);
                        state = parseSpecial(src, state);
                        if (state.inTitle() && src.isEmpty()) {
                            // We just ate the rest of the document as the title #text node!
                            // Reset the state then retokenize without special title handling.
                            // Let the parser clean up the missing </title> tag.
                            // FIXME: This is incorrect, because src.isEmpty() doesn't mean we're
                            // at the end of the document unless noMoreData is also true. We need
                            // to detect this case elsewhere, and save the state somewhere other
                            // than a local variable.
                            state = savedState;
                            src = savedSrc;
                            lineno = savedLineno;
                            scriptCodeSize = 0;
                        }
                    }
                } else if (tagName == xmpTag) {
                    if (beginTag) {
                        searchStopper = xmpEnd;
                        searchStopperLen = 5;
                        state.setInXmp(true);
                        state = parseSpecial(src, state);
                    }
                }
            }
            if (tagName == plaintextTag)
                state.setInPlainText(beginTag);
            return state; // Finished parsing tag!
        }
        } // end switch
    }
    m_cBufferPos = cBufferPos;
    return state;
}

inline bool HTMLTokenizer::continueProcessing(int& processedCount, double startTime, State &state)
{
    // We don't want to be checking elapsed time with every character, so we only check after we've
    // processed a certain number of characters.
    bool allowedYield = state.allowYield();
    state.setAllowYield(false);
    if (!state.loadingExtScript() && !state.forceSynchronous() && !m_executingScript && (processedCount > TOKENIZER_CHUNK_SIZE || allowedYield)) {
        processedCount = 0;
        if (currentTime() - startTime > tokenizerTimeDelay) {
            /* FIXME: We'd like to yield aggressively to give stylesheets the opportunity to
               load, but this hurts overall performance on slower machines.  For now turn this
               off.
            || (!m_doc->haveStylesheetsLoaded() && 
                (m_doc->documentElement()->id() != ID_HTML || m_doc->body()))) {*/
            // Schedule the timer to keep processing as soon as possible.
            m_timer.startOneShot(0);
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
            if (currentTime() - startTime > tokenizerTimeDelay)
                printf("Deferring processing of data because 500ms elapsed away from event loop.\n");
#endif
            return false;
        }
    }
    
    processedCount++;
    return true;
}

bool HTMLTokenizer::write(const SegmentedString& str, bool appendData)
{
#ifdef TOKEN_DEBUG
    kdDebug( 6036 ) << this << " Tokenizer::write(\"" << str.toString() << "\"," << appendData << ")" << endl;
#endif

    if (!buffer)
        return false;
    
    if (m_parserStopped)
        return false;

    SegmentedString source(str);
    if (m_executingScript)
        source.setExcludeLineNumbers();

    if ((m_executingScript && appendData) || !pendingScripts.isEmpty()) {
        // don't parse; we will do this later
        if (currentPrependingSrc)
            currentPrependingSrc->append(source);
        else
            pendingSrc.append(source);
        return false;
    }

    if (!src.isEmpty())
        src.append(source);
    else
        setSrc(source);

    // Once a timer is set, it has control of when the tokenizer continues.
    if (m_timer.isActive())
        return false;

    bool wasInWrite = inWrite;
    inWrite = true;
    
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_doc->ownerElement())
        printf("Beginning write at time %d\n", m_doc->elapsedTime());
#endif
    
    int processedCount = 0;
    double startTime = currentTime();

    Frame *frame = m_doc->frame();

    State state = m_state;

    int* lineNoPtr = lineNumberPtr();

    while (!src.isEmpty() && (!frame || !frame->loader()->isScheduledLocationChangePending())) {
        if (!continueProcessing(processedCount, startTime, state))
            break;

        // do we need to enlarge the buffer?
        checkBuffer();

        UChar cc = *src;

        bool wasSkipLF = state.skipLF();
        if (wasSkipLF)
            state.setSkipLF(false);

        if (wasSkipLF && (cc == '\n'))
            src.advance(0);
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
            src.advance(lineNoPtr);
            state = parseEntity(src, dest, state, m_cBufferPos, true, state.hasTagState());
        } else if (cc == '<' && !src.escaped()) {
            tagStartLineno = lineno;
            src.advance(lineNoPtr);
            state.setStartTag(true);
        } else if (cc == '\n' || cc == '\r') {
            if (state.discardLF())
                // Ignore this LF
                state.setDiscardLF(false); // We have discarded 1 LF
            else {
                // Process this LF
                *dest++ = '\n';
                if (cc == '\r' && !src.excludeLineNumbers())
                    lineno++;
            }

            /* Check for MS-DOS CRLF sequence */
            if (cc == '\r')
                state.setSkipLF(true);
            src.advance(lineNoPtr);
        } else {
            state.setDiscardLF(false);
            *dest++ = cc;
            src.advance(lineNoPtr);
        }
    }
    
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_doc->ownerElement())
        printf("Ending write at time %d\n", m_doc->elapsedTime());
#endif
    
    inWrite = wasInWrite;

    m_state = state;

    if (noMoreData && !inWrite && !state.loadingExtScript() && !m_executingScript && !m_timer.isActive()) {
        end(); // this actually causes us to be deleted
        return true;
    }
    return false;
}

void HTMLTokenizer::stopParsing()
{
    Tokenizer::stopParsing();
    m_timer.stop();

    // The part needs to know that the tokenizer has finished with its data,
    // regardless of whether it happened naturally or due to manual intervention.
    if (!m_fragment && m_doc->frame())
        m_doc->frame()->loader()->tokenizerProcessedData();
}

bool HTMLTokenizer::processingData() const
{
    return m_timer.isActive() || inWrite;
}

void HTMLTokenizer::timerFired(Timer<HTMLTokenizer>*)
{
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_doc->ownerElement())
        printf("Beginning timer write at time %d\n", m_doc->elapsedTime());
#endif

    if (m_doc->view() && m_doc->view()->layoutPending() && !m_doc->minimumLayoutDelay()) {
        // Restart the timer and let layout win.  This is basically a way of ensuring that the layout
        // timer has higher priority than our timer.
        m_timer.startOneShot(0);
        return;
    }
    
    RefPtr<Frame> frame = m_fragment ? 0 : m_doc->frame();

    // Invoke write() as though more data came in.
    bool didCallEnd = write(SegmentedString(), true);
  
    // If we called end() during the write,  we need to let WebKit know that we're done processing the data.
    if (didCallEnd && frame)
        frame->loader()->tokenizerProcessedData();
}

void HTMLTokenizer::end()
{
    ASSERT(!m_timer.isActive());
    m_timer.stop(); // Only helps if assertion above fires, but do it anyway.

    if (buffer) {
        // parseTag is using the buffer for different matters
        if (!m_state.hasTagState())
            processToken();

        fastFree(scriptCode);
        scriptCode = 0;
        scriptCodeSize = scriptCodeMaxSize = scriptCodeResync = 0;

        fastFree(buffer);
        buffer = 0;
    }

    if (!inViewSourceMode())
        parser->finished();
    else
        m_doc->finishedParsing();
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
        String food;
        if (m_state.inScript() || m_state.inStyle())
            food = String(scriptCode, scriptCodeSize);
        else if (m_state.inServer()) {
            food = "<";
            food.append(String(scriptCode, scriptCodeSize));
        } else {
            pos = DeprecatedConstString(reinterpret_cast<DeprecatedChar*>(scriptCode), scriptCodeSize).string().find('>');
            food = String(scriptCode + pos + 1, scriptCodeSize - pos - 1);
        }
        fastFree(scriptCode);
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
    if (!inWrite && !m_state.loadingExtScript() && !m_executingScript && !m_timer.isActive())
        end(); // this actually causes us to be deleted
}

PassRefPtr<Node> HTMLTokenizer::processToken()
{
    KJSProxy* jsProxy = (!m_fragment && m_doc->frame()) ? m_doc->frame()->scriptProxy() : 0;
    if (jsProxy)
        jsProxy->setEventHandlerLineno(tagStartLineno);
    if (dest > buffer) {
#ifdef TOKEN_DEBUG
        if(currToken.tagName.length()) {
            qDebug( "unexpected token: %s, str: *%s*", currToken.tagName.deprecatedString().latin1(),DeprecatedConstString( buffer,dest-buffer ).deprecatedString().latin1() );
            ASSERT(0);
        }

#endif
        currToken.text = StringImpl::createStrippingNull(buffer, dest - buffer);
        if (currToken.tagName != commentAtom)
            currToken.tagName = textAtom;
    } else if (currToken.tagName == nullAtom) {
        currToken.reset();
        if (jsProxy)
            jsProxy->setEventHandlerLineno(lineno);
        return 0;
    }

    dest = buffer;

#ifdef TOKEN_DEBUG
    DeprecatedString name = currToken.tagName.deprecatedString();
    DeprecatedString text;
    if(currToken.text)
        text = DeprecatedConstString(currToken.text->unicode(), currToken.text->length()).deprecatedString();

    kdDebug( 6036 ) << "Token --> " << name << endl;
    if (currToken.flat)
        kdDebug( 6036 ) << "Token is FLAT!" << endl;
    if(!text.isNull())
        kdDebug( 6036 ) << "text: \"" << text << "\"" << endl;
    unsigned l = currToken.attrs ? currToken.attrs->length() : 0;
    if(l) {
        kdDebug( 6036 ) << "Attributes: " << l << endl;
        for (unsigned i = 0; i < l; ++i) {
            Attribute* c = currToken.attrs->attributeItem(i);
            kdDebug( 6036 ) << "    " << c->localName().deprecatedString()
                            << "=\"" << c->value().deprecatedString() << "\"" << endl;
        }
    }
    kdDebug( 6036 ) << endl;
#endif

    RefPtr<Node> n;
    
    if (!m_parserStopped) {
        if (inViewSourceMode())
            static_cast<HTMLViewSourceDocument*>(m_doc)->addViewSourceToken(&currToken);
        else
            // pass the token over to the parser, the parser DOES NOT delete the token
            n = parser->parseToken(&currToken);
    }
    currToken.reset();
    if (jsProxy)
        jsProxy->setEventHandlerLineno(0);

    return n.release();
}

HTMLTokenizer::~HTMLTokenizer()
{
    ASSERT(!inWrite);
    reset();
    delete parser;
}


void HTMLTokenizer::enlargeBuffer(int len)
{
    int newSize = max(size * 2, size + len);
    int oldOffset = dest - buffer;
    buffer = static_cast<UChar*>(fastRealloc(buffer, newSize * sizeof(UChar)));
    dest = buffer + oldOffset;
    size = newSize;
}

void HTMLTokenizer::enlargeScriptBuffer(int len)
{
    int newSize = max(scriptCodeMaxSize * 2, scriptCodeMaxSize + len);
    scriptCode = static_cast<UChar*>(fastRealloc(scriptCode, newSize * sizeof(UChar)));
    scriptCodeMaxSize = newSize;
}
    
void HTMLTokenizer::executeScriptsWaitingForStylesheets()
{
    ASSERT(m_doc->haveStylesheetsLoaded());

    if (m_hasScriptsWaitingForStylesheets)
        notifyFinished(0);
}

void HTMLTokenizer::notifyFinished(CachedResource*)
{
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_doc->ownerElement())
        printf("script loaded at %d\n", m_doc->elapsedTime());
#endif

    ASSERT(!pendingScripts.isEmpty());

    // Make scripts loaded from file URLs wait for stylesheets to match Tiger behavior where
    // file loads were serialized in lower level.
    // FIXME: this should really be done for all script loads or the same effect should be achieved by other
    // means, like javascript suspend/resume
    m_hasScriptsWaitingForStylesheets = !m_doc->haveStylesheetsLoaded() && pendingScripts.head()->url().startsWith("file:", false);
    if (m_hasScriptsWaitingForStylesheets)
        return;

    bool finished = false;
    while (!finished && pendingScripts.head()->isLoaded()) {
#ifdef TOKEN_DEBUG
        kdDebug( 6036 ) << "Finished loading an external script" << endl;
#endif
        CachedScript* cs = pendingScripts.dequeue();
        ASSERT(cache()->disabled() || cs->accessCount() > 0);

        String scriptSource = cs->script();
#ifdef TOKEN_DEBUG
        kdDebug( 6036 ) << "External script is:" << endl << scriptSource.deprecatedString() << endl;
#endif
        setSrc(SegmentedString());

        // make sure we forget about the script before we execute the new one
        // infinite recursion might happen otherwise
        DeprecatedString cachedScriptUrl( cs->url().deprecatedString() );
        bool errorOccurred = cs->errorOccurred();
        cs->deref(this);
        RefPtr<Node> n = scriptNode.release();

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
        if (!m_doc->ownerElement())
            printf("external script beginning execution at %d\n", m_doc->elapsedTime());
#endif

        if (errorOccurred)
            EventTargetNodeCast(n.get())->dispatchHTMLEvent(errorEvent, true, false);
        else {
            if (static_cast<HTMLScriptElement*>(n.get())->shouldExecuteAsJavaScript())
                m_state = scriptExecution(scriptSource.deprecatedString(), m_state, cachedScriptUrl);
            EventTargetNodeCast(n.get())->dispatchHTMLEvent(loadEvent, false, false);
        }

        // The state of pendingScripts.isEmpty() can change inside the scriptExecution()
        // call above, so test afterwards.
        finished = pendingScripts.isEmpty();
        if (finished) {
            m_state.setLoadingExtScript(false);
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
            if (!m_doc->ownerElement())
                printf("external script finished execution at %d\n", m_doc->elapsedTime());
#endif
        }

        // 'm_requestingScript' is true when we are called synchronously from
        // scriptHandler(). In that case scriptHandler() will take care
        // of pendingSrc.
        if (!m_requestingScript) {
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
    src = source;
}

void parseHTMLDocumentFragment(const String& source, DocumentFragment* fragment)
{
    HTMLTokenizer tok(fragment);
    tok.setForceSynchronous(true);
    tok.write(source, true);
    tok.finish();
    ASSERT(!tok.processingData());      // make sure we're done (see 3963151)
}

UChar decodeNamedEntity(const char* name)
{
    const Entity* e = findEntity(name, strlen(name));
    return e ? e->code : 0;
}

}
