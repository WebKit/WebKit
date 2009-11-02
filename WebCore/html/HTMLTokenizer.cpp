/*
    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)
              (C) 1998 Waldo Bastian (bastian@kde.org)
              (C) 1999 Lars Knoll (knoll@kde.org)
              (C) 1999 Antti Koivisto (koivisto@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
    Copyright (C) 2005, 2006 Alexey Proskuryakov (ap@nypop.com)
    Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)

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
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "HTMLParser.h"
#include "HTMLScriptElement.h"
#include "HTMLViewSourceDocument.h"
#include "ImageLoader.h"
#include "InspectorTimelineAgent.h"
#include "MappedAttribute.h"
#include "Page.h"
#include "PreloadScanner.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "XSSAuditor.h"
#include <wtf/ASCIICType.h>
#include <wtf/CurrentTime.h>

#include "HTMLEntityNames.c"

#define PRELOAD_SCANNER_ENABLED 1
// #define INSTRUMENT_LAYOUT_SCHEDULING 1

using namespace WTF;
using namespace std;

namespace WebCore {

using namespace HTMLNames;

#if MOBILE
// The mobile device needs to be responsive, as such the tokenizer chunk size is reduced.
// This value is used to define how many characters the tokenizer will process before 
// yeilding control.
static const int defaultTokenizerChunkSize = 256;
#else
static const int defaultTokenizerChunkSize = 4096;
#endif

#if MOBILE
// As the chunks are smaller (above), the tokenizer should not yield for as long a period, otherwise
// it will take way to long to load a page.
static const double defaultTokenizerTimeDelay = 0.300;
#else
// FIXME: We would like this constant to be 200ms.
// Yielding more aggressively results in increased responsiveness and better incremental rendering.
// It slows down overall page-load on slower machines, though, so for now we set a value of 500.
static const double defaultTokenizerTimeDelay = 0.500;
#endif

static const char commentStart [] = "<!--";
static const char doctypeStart [] = "<!doctype";
static const char publicStart [] = "public";
static const char systemStart [] = "system";
static const char scriptEnd [] = "</script";
static const char xmpEnd [] = "</xmp";
static const char styleEnd [] =  "</style";
static const char textareaEnd [] = "</textarea";
static const char titleEnd [] = "</title";
static const char iframeEnd [] = "</iframe";

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
        unsigned char uc1 = toASCIIUpper(static_cast<char>(c1));
        UChar c2 = s2[i];
        if (c1 != c2 && uc1 != c2)
            return false;
    }
    return true;
}

inline void Token::addAttribute(AtomicString& attrName, const AtomicString& attributeValue, bool viewSourceMode)
{
    if (!attrName.isEmpty()) {
        ASSERT(!attrName.contains('/'));
        RefPtr<MappedAttribute> a = MappedAttribute::create(attrName, attributeValue);
        if (!attrs) {
            attrs = NamedMappedAttrMap::create();
            attrs->reserveInitialCapacity(10);
        }
        attrs->insertAttribute(a.release(), viewSourceMode);
    }
    
    attrName = emptyAtom;
}

// ----------------------------------------------------------------------------

HTMLTokenizer::HTMLTokenizer(HTMLDocument* doc, bool reportErrors)
    : Tokenizer()
    , m_buffer(0)
    , m_scriptCode(0)
    , m_scriptCodeSize(0)
    , m_scriptCodeCapacity(0)
    , m_scriptCodeResync(0)
    , m_executingScript(0)
    , m_requestingScript(false)
    , m_hasScriptsWaitingForStylesheets(false)
    , m_timer(this, &HTMLTokenizer::timerFired)
    , m_doc(doc)
    , m_parser(new HTMLParser(doc, reportErrors))
    , m_inWrite(false)
    , m_fragment(false)
{
    begin();
}

HTMLTokenizer::HTMLTokenizer(HTMLViewSourceDocument* doc)
    : Tokenizer(true)
    , m_buffer(0)
    , m_scriptCode(0)
    , m_scriptCodeSize(0)
    , m_scriptCodeCapacity(0)
    , m_scriptCodeResync(0)
    , m_executingScript(0)
    , m_requestingScript(false)
    , m_hasScriptsWaitingForStylesheets(false)
    , m_timer(this, &HTMLTokenizer::timerFired)
    , m_doc(doc)
    , m_parser(0)
    , m_inWrite(false)
    , m_fragment(false)
{
    begin();
}

HTMLTokenizer::HTMLTokenizer(DocumentFragment* frag)
    : m_buffer(0)
    , m_scriptCode(0)
    , m_scriptCodeSize(0)
    , m_scriptCodeCapacity(0)
    , m_scriptCodeResync(0)
    , m_executingScript(0)
    , m_requestingScript(false)
    , m_hasScriptsWaitingForStylesheets(false)
    , m_timer(this, &HTMLTokenizer::timerFired)
    , m_doc(frag->document())
    , m_parser(new HTMLParser(frag))
    , m_inWrite(false)
    , m_fragment(true)
{
    begin();
}

void HTMLTokenizer::reset()
{
    ASSERT(m_executingScript == 0);

    while (!m_pendingScripts.isEmpty()) {
        CachedScript* cs = m_pendingScripts.first().get();
        m_pendingScripts.removeFirst();
        ASSERT(cache()->disabled() || cs->accessCount() > 0);
        cs->removeClient(this);
    }

    fastFree(m_buffer);
    m_buffer = m_dest = 0;
    m_bufferSize = 0;

    fastFree(m_scriptCode);
    m_scriptCode = 0;
    m_scriptCodeSize = m_scriptCodeCapacity = m_scriptCodeResync = 0;

    m_timer.stop();
    m_state.setAllowYield(false);
    m_state.setForceSynchronous(false);

    m_currentToken.reset();
    m_doctypeToken.reset();
    m_doctypeSearchCount = 0;
    m_doctypeSecondarySearchCount = 0;
    m_hasScriptsWaitingForStylesheets = false;
}

void HTMLTokenizer::begin()
{
    m_executingScript = 0;
    m_requestingScript = false;
    m_hasScriptsWaitingForStylesheets = false;
    m_state.setLoadingExtScript(false);
    reset();
    m_bufferSize = 254;
    m_buffer = static_cast<UChar*>(fastMalloc(sizeof(UChar) * 254));
    m_dest = m_buffer;
    tquote = NoQuote;
    searchCount = 0;
    m_state.setEntityState(NoEntity);
    m_scriptTagSrcAttrValue = String();
    m_pendingSrc.clear();
    m_currentPrependingSrc = 0;
    m_noMoreData = false;
    m_brokenComments = false;
    m_brokenServer = false;
    m_lineNumber = 0;
    m_currentScriptTagStartLineNumber = 0;
    m_currentTagStartLineNumber = 0;
    m_state.setForceSynchronous(false);

    Page* page = m_doc->page();
    if (page && page->hasCustomHTMLTokenizerTimeDelay())
        m_tokenizerTimeDelay = page->customHTMLTokenizerTimeDelay();
    else
        m_tokenizerTimeDelay = defaultTokenizerTimeDelay;

    if (page && page->hasCustomHTMLTokenizerChunkSize())
        m_tokenizerChunkSize = page->customHTMLTokenizerChunkSize();
    else
        m_tokenizerChunkSize = defaultTokenizerChunkSize;
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
                list.advance();
                continue;
            }
        }

        checkBuffer();

        if (*list == '\n' || *list == '\r') {
            if (state.discardLF())
                // Ignore this LF
                state.setDiscardLF(false); // We have discarded 1 LF
            else
                *m_dest++ = '\n';

            /* Check for MS-DOS CRLF sequence */
            if (*list == '\r')
                state.setSkipLF(true);

            list.advance();
        } else {
            state.setDiscardLF(false);
            *m_dest++ = *list;
            list.advance();
        }
    }

    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseNonHTMLText(SegmentedString& src, State state)
{
    ASSERT(state.inTextArea() || state.inTitle() || state.inIFrame() || !state.hasEntityState());
    ASSERT(!state.hasTagState());
    ASSERT(state.inXmp() + state.inTextArea() + state.inTitle() + state.inStyle() + state.inScript() + state.inIFrame() == 1);
    if (state.inScript() && !m_currentScriptTagStartLineNumber)
        m_currentScriptTagStartLineNumber = m_lineNumber;

    if (state.inComment()) 
        state = parseComment(src, state);

    int lastDecodedEntityPosition = -1;
    while (!src.isEmpty()) {
        checkScriptBuffer();
        UChar ch = *src;

        if (!m_scriptCodeResync && !m_brokenComments &&
            !state.inXmp() && ch == '-' && m_scriptCodeSize >= 3 && !src.escaped() &&
            m_scriptCode[m_scriptCodeSize - 3] == '<' && m_scriptCode[m_scriptCodeSize - 2] == '!' && m_scriptCode[m_scriptCodeSize - 1] == '-' &&
            (lastDecodedEntityPosition < m_scriptCodeSize - 3)) {
            state.setInComment(true);
            state = parseComment(src, state);
            continue;
        }
        if (m_scriptCodeResync && !tquote && ch == '>') {
            src.advancePastNonNewline();
            m_scriptCodeSize = m_scriptCodeResync - 1;
            m_scriptCodeResync = 0;
            m_scriptCode[m_scriptCodeSize] = m_scriptCode[m_scriptCodeSize + 1] = 0;
            if (state.inScript())
                state = scriptHandler(state);
            else {
                state = processListing(SegmentedString(m_scriptCode, m_scriptCodeSize), state);
                processToken();
                if (state.inStyle()) { 
                    m_currentToken.tagName = styleTag.localName();
                    m_currentToken.beginTag = false;
                } else if (state.inTextArea()) { 
                    m_currentToken.tagName = textareaTag.localName();
                    m_currentToken.beginTag = false;
                } else if (state.inTitle()) { 
                    m_currentToken.tagName = titleTag.localName();
                    m_currentToken.beginTag = false;
                } else if (state.inXmp()) {
                    m_currentToken.tagName = xmpTag.localName();
                    m_currentToken.beginTag = false;
                } else if (state.inIFrame()) {
                    m_currentToken.tagName = iframeTag.localName();
                    m_currentToken.beginTag = false;
                }
                processToken();
                state.setInStyle(false);
                state.setInScript(false);
                state.setInTextArea(false);
                state.setInTitle(false);
                state.setInXmp(false);
                state.setInIFrame(false);
                tquote = NoQuote;
                m_scriptCodeSize = m_scriptCodeResync = 0;
            }
            return state;
        }
        // possible end of tagname, lets check.
        if (!m_scriptCodeResync && !state.escaped() && !src.escaped() && (ch == '>' || ch == '/' || isASCIISpace(ch)) &&
             m_scriptCodeSize >= m_searchStopperLength &&
             tagMatch(m_searchStopper, m_scriptCode + m_scriptCodeSize - m_searchStopperLength, m_searchStopperLength) &&
             (lastDecodedEntityPosition < m_scriptCodeSize - m_searchStopperLength)) {
            m_scriptCodeResync = m_scriptCodeSize-m_searchStopperLength+1;
            tquote = NoQuote;
            continue;
        }
        if (m_scriptCodeResync && !state.escaped()) {
            if (ch == '\"')
                tquote = (tquote == NoQuote) ? DoubleQuote : ((tquote == SingleQuote) ? SingleQuote : NoQuote);
            else if (ch == '\'')
                tquote = (tquote == NoQuote) ? SingleQuote : (tquote == DoubleQuote) ? DoubleQuote : NoQuote;
            else if (tquote != NoQuote && (ch == '\r' || ch == '\n'))
                tquote = NoQuote;
        }
        state.setEscaped(!state.escaped() && ch == '\\');
        if (!m_scriptCodeResync && (state.inTextArea() || state.inTitle() || state.inIFrame()) && !src.escaped() && ch == '&') {
            UChar* scriptCodeDest = m_scriptCode + m_scriptCodeSize;
            src.advancePastNonNewline();
            state = parseEntity(src, scriptCodeDest, state, m_cBufferPos, true, false);
            if (scriptCodeDest == m_scriptCode + m_scriptCodeSize)
                lastDecodedEntityPosition = m_scriptCodeSize;
            else
                m_scriptCodeSize = scriptCodeDest - m_scriptCode;
        } else {
            m_scriptCode[m_scriptCodeSize++] = ch;
            src.advance(m_lineNumber);
        }
    }

    return state;
}
    
HTMLTokenizer::State HTMLTokenizer::scriptHandler(State state)
{
    // We are inside a <script>
    bool doScriptExec = false;
    int startLine = m_currentScriptTagStartLineNumber + 1; // Script line numbers are 1 based, HTMLTokenzier line numbers are 0 based
    
    // Reset m_currentScriptTagStartLineNumber to indicate that we've finished parsing the current script element
    m_currentScriptTagStartLineNumber = 0;

    // (Bugzilla 3837) Scripts following a frameset element should not execute or, 
    // in the case of extern scripts, even load.
    bool followingFrameset = (m_doc->body() && m_doc->body()->hasTagName(framesetTag));
  
    CachedScript* cs = 0;
    // don't load external scripts for standalone documents (for now)
    if (!inViewSourceMode()) {
        if (!m_scriptTagSrcAttrValue.isEmpty() && m_doc->frame()) {
            // forget what we just got; load from src url instead
            if (!m_parser->skipMode() && !followingFrameset) {
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
                if (!m_doc->ownerElement())
                    printf("Requesting script at time %d\n", m_doc->elapsedTime());
#endif
                // The parser might have been stopped by for example a window.close call in an earlier script.
                // If so, we don't want to load scripts.
                if (!m_parserStopped && m_scriptNode->dispatchBeforeLoadEvent(m_scriptTagSrcAttrValue) &&
                    (cs = m_doc->docLoader()->requestScript(m_scriptTagSrcAttrValue, m_scriptTagCharsetAttrValue)))
                    m_pendingScripts.append(cs);
                else
                    m_scriptNode = 0;
            } else
                m_scriptNode = 0;
            m_scriptTagSrcAttrValue = String();
        } else {
            // Parse m_scriptCode containing <script> info
            doScriptExec = m_scriptNode->shouldExecuteAsJavaScript();
#if ENABLE(XHTMLMP)
            if (!doScriptExec)
                m_doc->setShouldProcessNoscriptElement(true);
#endif
            m_scriptNode = 0;
        }
    }

    state = processListing(SegmentedString(m_scriptCode, m_scriptCodeSize), state);
    RefPtr<Node> node = processToken();
    String scriptString = node ? node->textContent() : "";
    m_currentToken.tagName = scriptTag.localName();
    m_currentToken.beginTag = false;
    processToken();

    state.setInScript(false);
    m_scriptCodeSize = m_scriptCodeResync = 0;
    
    // FIXME: The script should be syntax highlighted.
    if (inViewSourceMode())
        return state;

    SegmentedString* savedPrependingSrc = m_currentPrependingSrc;
    SegmentedString prependingSrc;
    m_currentPrependingSrc = &prependingSrc;

    if (!m_parser->skipMode() && !followingFrameset) {
        if (cs) {
            if (savedPrependingSrc)
                savedPrependingSrc->append(m_src);
            else
                m_pendingSrc.prepend(m_src);
            setSrc(SegmentedString());

            // the ref() call below may call notifyFinished if the script is already in cache,
            // and that mucks with the state directly, so we must write it back to the object.
            m_state = state;
            bool savedRequestingScript = m_requestingScript;
            m_requestingScript = true;
            cs->addClient(this);
            m_requestingScript = savedRequestingScript;
            state = m_state;
            // will be 0 if script was already loaded and ref() executed it
            if (!m_pendingScripts.isEmpty())
                state.setLoadingExtScript(true);
        } else if (!m_fragment && doScriptExec) {
            if (!m_executingScript)
                m_pendingSrc.prepend(m_src);
            else
                prependingSrc = m_src;
            setSrc(SegmentedString());
            state = scriptExecution(ScriptSourceCode(scriptString, m_doc->frame() ? m_doc->frame()->document()->url() : KURL(), startLine), state);
        }
    }

    if (!m_executingScript && !state.loadingExtScript()) {
        m_src.append(m_pendingSrc);
        m_pendingSrc.clear();
    } else if (!prependingSrc.isEmpty()) {
        // restore first so that the write appends in the right place
        // (does not hurt to do it again below)
        m_currentPrependingSrc = savedPrependingSrc;

        // we need to do this slightly modified bit of one of the write() cases
        // because we want to prepend to m_pendingSrc rather than appending
        // if there's no previous prependingSrc
        if (!m_pendingScripts.isEmpty()) {
            if (m_currentPrependingSrc)
                m_currentPrependingSrc->append(prependingSrc);
            else
                m_pendingSrc.prepend(prependingSrc);
        } else {
            m_state = state;
            write(prependingSrc, false);
            state = m_state;
        }
    }
    
#if PRELOAD_SCANNER_ENABLED
    if (!m_pendingScripts.isEmpty() && !m_executingScript) {
        if (!m_preloadScanner)
            m_preloadScanner.set(new PreloadScanner(m_doc));
        if (!m_preloadScanner->inProgress()) {
            m_preloadScanner->begin();
            m_preloadScanner->write(m_pendingSrc);
        }
    }
#endif
    m_currentPrependingSrc = savedPrependingSrc;

    return state;
}

HTMLTokenizer::State HTMLTokenizer::scriptExecution(const ScriptSourceCode& sourceCode, State state)
{
    if (m_fragment || !m_doc->frame())
        return state;
    m_executingScript++;
    
    SegmentedString* savedPrependingSrc = m_currentPrependingSrc;
    SegmentedString prependingSrc;
    m_currentPrependingSrc = &prependingSrc;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_doc->ownerElement())
        printf("beginning script execution at %d\n", m_doc->elapsedTime());
#endif

    m_state = state;
    m_doc->frame()->script()->executeScript(sourceCode);
    state = m_state;

    state.setAllowYield(true);

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_doc->ownerElement())
        printf("ending script execution at %d\n", m_doc->elapsedTime());
#endif
    
    m_executingScript--;

    if (!m_executingScript && !state.loadingExtScript()) {
        m_pendingSrc.prepend(prependingSrc);        
        m_src.append(m_pendingSrc);
        m_pendingSrc.clear();
    } else if (!prependingSrc.isEmpty()) {
        // restore first so that the write appends in the right place
        // (does not hurt to do it again below)
        m_currentPrependingSrc = savedPrependingSrc;

        // we need to do this slightly modified bit of one of the write() cases
        // because we want to prepend to m_pendingSrc rather than appending
        // if there's no previous prependingSrc
        if (!m_pendingScripts.isEmpty()) {
            if (m_currentPrependingSrc)
                m_currentPrependingSrc->append(prependingSrc);
            else
                m_pendingSrc.prepend(prependingSrc);
            
#if PRELOAD_SCANNER_ENABLED
            // We are stuck waiting for another script. Lets check the source that
            // was just document.write()n for anything to load.
            PreloadScanner documentWritePreloadScanner(m_doc);
            documentWritePreloadScanner.begin();
            documentWritePreloadScanner.write(prependingSrc);
            documentWritePreloadScanner.end();
#endif
        } else {
            m_state = state;
            write(prependingSrc, false);
            state = m_state;
        }
    }

    m_currentPrependingSrc = savedPrependingSrc;
    
    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseComment(SegmentedString& src, State state)
{
    // FIXME: Why does this code even run for comments inside <script> and <style>? This seems bogus.
    checkScriptBuffer(src.length());
    while (!src.isEmpty()) {
        UChar ch = *src;
        m_scriptCode[m_scriptCodeSize++] = ch;
        if (ch == '>') {
            bool handleBrokenComments = m_brokenComments && !(state.inScript() || state.inStyle());
            int endCharsCount = 1; // start off with one for the '>' character
            if (m_scriptCodeSize > 2 && m_scriptCode[m_scriptCodeSize-3] == '-' && m_scriptCode[m_scriptCodeSize-2] == '-') {
                endCharsCount = 3;
            } else if (m_scriptCodeSize > 3 && m_scriptCode[m_scriptCodeSize-4] == '-' && m_scriptCode[m_scriptCodeSize-3] == '-' && 
                m_scriptCode[m_scriptCodeSize-2] == '!') {
                // Other browsers will accept --!> as a close comment, even though it's
                // not technically valid.
                endCharsCount = 4;
            }
            if (handleBrokenComments || endCharsCount > 1) {
                src.advancePastNonNewline();
                if (!(state.inTitle() || state.inScript() || state.inXmp() || state.inTextArea() || state.inStyle() || state.inIFrame())) {
                    checkScriptBuffer();
                    m_scriptCode[m_scriptCodeSize] = 0;
                    m_scriptCode[m_scriptCodeSize + 1] = 0;
                    m_currentToken.tagName = commentAtom;
                    m_currentToken.beginTag = true;
                    state = processListing(SegmentedString(m_scriptCode, m_scriptCodeSize - endCharsCount), state);
                    processToken();
                    m_currentToken.tagName = commentAtom;
                    m_currentToken.beginTag = false;
                    processToken();
                    m_scriptCodeSize = 0;
                }
                state.setInComment(false);
                return state; // Finished parsing comment
            }
        }
        src.advance(m_lineNumber);
    }

    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseServer(SegmentedString& src, State state)
{
    checkScriptBuffer(src.length());
    while (!src.isEmpty()) {
        UChar ch = *src;
        m_scriptCode[m_scriptCodeSize++] = ch;
        if (ch == '>' && m_scriptCodeSize > 1 && m_scriptCode[m_scriptCodeSize - 2] == '%') {
            src.advancePastNonNewline();
            state.setInServer(false);
            m_scriptCodeSize = 0;
            return state; // Finished parsing server include
        }
        src.advance(m_lineNumber);
    }
    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseProcessingInstruction(SegmentedString& src, State state)
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
            src.advancePastNonNewline();
            state.setDiscardLF(true);
            return state; // Finished parsing comment!
        }
        src.advance(m_lineNumber);
        oldchar = chbegin;
    }
    
    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseText(SegmentedString& src, State state)
{
    while (!src.isEmpty()) {
        UChar cc = *src;

        if (state.skipLF()) {
            state.setSkipLF(false);
            if (cc == '\n') {
                src.advancePastNewline(m_lineNumber);
                continue;
            }
        }

        // do we need to enlarge the buffer?
        checkBuffer();

        if (cc == '\r') {
            state.setSkipLF(true);
            *m_dest++ = '\n';
        } else
            *m_dest++ = cc;
        src.advance(m_lineNumber);
    }

    return state;
}


HTMLTokenizer::State HTMLTokenizer::parseEntity(SegmentedString& src, UChar*& dest, State state, unsigned& cBufferPos, bool start, bool parsingTag)
{
    if (start) {
        cBufferPos = 0;
        state.setEntityState(SearchEntity);
        EntityUnicodeValue = 0;
    }

    while (!src.isEmpty()) {
        UChar cc = *src;
        switch (state.entityState()) {
        case NoEntity:
            ASSERT(state.entityState() != NoEntity);
            return state;
        
        case SearchEntity:
            if (cc == '#') {
                m_cBuffer[cBufferPos++] = cc;
                src.advancePastNonNewline();
                state.setEntityState(NumericSearch);
            } else
                state.setEntityState(EntityName);
            break;

        case NumericSearch:
            if (cc == 'x' || cc == 'X') {
                m_cBuffer[cBufferPos++] = cc;
                src.advancePastNonNewline();
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
                m_cBuffer[cBufferPos++] = cc;
                src.advancePastNonNewline();
            }
            if (cBufferPos == 10)  
                state.setEntityState(SearchSemicolon);
            break;
        }
        case Decimal:
        {
            int ll = min(src.length(), 9-cBufferPos);
            while (ll--) {
                cc = *src;

                if (!(cc >= '0' && cc <= '9')) {
                    state.setEntityState(SearchSemicolon);
                    break;
                }

                EntityUnicodeValue = EntityUnicodeValue * 10 + (cc - '0');
                m_cBuffer[cBufferPos++] = cc;
                src.advancePastNonNewline();
            }
            if (cBufferPos == 9)  
                state.setEntityState(SearchSemicolon);
            break;
        }
        case EntityName:
        {
            int ll = min(src.length(), 9-cBufferPos);
            while (ll--) {
                cc = *src;

                if (!((cc >= 'a' && cc <= 'z') || (cc >= '0' && cc <= '9') || (cc >= 'A' && cc <= 'Z'))) {
                    state.setEntityState(SearchSemicolon);
                    break;
                }

                m_cBuffer[cBufferPos++] = cc;
                src.advancePastNonNewline();
            }
            if (cBufferPos == 9) 
                state.setEntityState(SearchSemicolon);
            if (state.entityState() == SearchSemicolon) {
                if (cBufferPos > 1) {
                    // Since the maximum length of entity name is 9,
                    // so a single char array which is allocated on
                    // the stack, its length is 10, should be OK.
                    // Also if we have an illegal character, we treat it
                    // as illegal entity name.
                    unsigned testedEntityNameLen = 0;
                    char tmpEntityNameBuffer[10];

                    ASSERT(cBufferPos < 10);
                    for (; testedEntityNameLen < cBufferPos; ++testedEntityNameLen) {
                        if (m_cBuffer[testedEntityNameLen] > 0x7e)
                            break;
                        tmpEntityNameBuffer[testedEntityNameLen] = m_cBuffer[testedEntityNameLen];
                    }

                    const Entity *e;

                    if (testedEntityNameLen == cBufferPos)
                        e = findEntity(tmpEntityNameBuffer, cBufferPos);
                    else
                        e = 0;

                    if (e)
                        EntityUnicodeValue = e->code;

                    // be IE compatible
                    if (parsingTag && EntityUnicodeValue > 255 && *src != ';')
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
                        src.advancePastNonNewline();
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
                    // 12 bytes required: up to 10 bytes in m_cBuffer plus the
                    // leading '&' and trailing ';'
                    checkBuffer(12);
                    *dest++ = '&';
                    for (unsigned i = 0; i < cBufferPos; i++)
                        dest[i] = m_cBuffer[i];
                    dest += cBufferPos;
                    if (*src == ';') {
                        *dest++ = ';';
                        src.advancePastNonNewline();
                    }
                }
            } else {
                // 11 bytes required: up to 10 bytes in m_cBuffer plus the
                // leading '&'
                checkBuffer(11);
                // ignore the sequence, add it to the buffer as plaintext
                *dest++ = '&';
                for (unsigned i = 0; i < cBufferPos; i++)
                    dest[i] = m_cBuffer[i];
                dest += cBufferPos;
            }

            state.setEntityState(NoEntity);
            return state;
        }
    }

    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseDoctype(SegmentedString& src, State state)
{
    ASSERT(state.inDoctype());
    while (!src.isEmpty() && state.inDoctype()) {
        UChar c = *src;
        bool isWhitespace = c == '\r' || c == '\n' || c == '\t' || c == ' ';
        switch (m_doctypeToken.state()) {
            case DoctypeBegin: {
                m_doctypeToken.setState(DoctypeBeforeName);
                if (isWhitespace) {
                    src.advance(m_lineNumber);
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                }
                break;
            }
            case DoctypeBeforeName: {
                if (c == '>') {
                    // Malformed.  Just exit.
                    src.advancePastNonNewline();
                    state.setInDoctype(false);
                    if (inViewSourceMode())
                        processDoctypeToken();
                } else if (isWhitespace) {
                    src.advance(m_lineNumber);
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                } else
                    m_doctypeToken.setState(DoctypeName);
                break;
            }
            case DoctypeName: {
                if (c == '>') {
                    // Valid doctype. Emit it.
                    src.advancePastNonNewline();
                    state.setInDoctype(false);
                    processDoctypeToken();
                } else if (isWhitespace) {
                    m_doctypeSearchCount = 0; // Used now to scan for PUBLIC
                    m_doctypeSecondarySearchCount = 0; // Used now to scan for SYSTEM
                    m_doctypeToken.setState(DoctypeAfterName);
                    src.advance(m_lineNumber);
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                } else {
                    src.advancePastNonNewline();
                    m_doctypeToken.m_name.append(c);
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                }
                break;
            }
            case DoctypeAfterName: {
                if (c == '>') {
                    // Valid doctype. Emit it.
                    src.advancePastNonNewline();
                    state.setInDoctype(false);
                    processDoctypeToken();
                } else if (!isWhitespace) {
                    src.advancePastNonNewline();
                    if (toASCIILower(c) == publicStart[m_doctypeSearchCount]) {
                        m_doctypeSearchCount++;
                        if (m_doctypeSearchCount == 6)
                            // Found 'PUBLIC' sequence
                            m_doctypeToken.setState(DoctypeBeforePublicID);
                    } else if (m_doctypeSearchCount > 0) {
                        m_doctypeSearchCount = 0;
                        m_doctypeToken.setState(DoctypeBogus);
                    } else if (toASCIILower(c) == systemStart[m_doctypeSecondarySearchCount]) {
                        m_doctypeSecondarySearchCount++;
                        if (m_doctypeSecondarySearchCount == 6)
                            // Found 'SYSTEM' sequence
                            m_doctypeToken.setState(DoctypeBeforeSystemID);
                    } else {
                        m_doctypeSecondarySearchCount = 0;
                        m_doctypeToken.setState(DoctypeBogus);
                    }
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                } else {
                    src.advance(m_lineNumber); // Whitespace keeps us in the after name state.
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                }
                break;
            }
            case DoctypeBeforePublicID: {
                if (c == '\"' || c == '\'') {
                    tquote = c == '\"' ? DoubleQuote : SingleQuote;
                    m_doctypeToken.setState(DoctypePublicID);
                    src.advancePastNonNewline();
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                } else if (c == '>') {
                    // Considered bogus.  Don't process the doctype.
                    src.advancePastNonNewline();
                    state.setInDoctype(false);
                    if (inViewSourceMode())
                        processDoctypeToken();
                } else if (isWhitespace) {
                    src.advance(m_lineNumber);
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                } else
                    m_doctypeToken.setState(DoctypeBogus);
                break;
            }
            case DoctypePublicID: {
                if ((c == '\"' && tquote == DoubleQuote) || (c == '\'' && tquote == SingleQuote)) {
                    src.advancePastNonNewline();
                    m_doctypeToken.setState(DoctypeAfterPublicID);
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                } else if (c == '>') {
                     // Considered bogus.  Don't process the doctype.
                    src.advancePastNonNewline();
                    state.setInDoctype(false);
                    if (inViewSourceMode())
                        processDoctypeToken();
                } else {
                    m_doctypeToken.m_publicID.append(c);
                    src.advance(m_lineNumber);
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                }
                break;
            }
            case DoctypeAfterPublicID:
                if (c == '\"' || c == '\'') {
                    tquote = c == '\"' ? DoubleQuote : SingleQuote;
                    m_doctypeToken.setState(DoctypeSystemID);
                    src.advancePastNonNewline();
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                } else if (c == '>') {
                    // Valid doctype. Emit it now.
                    src.advancePastNonNewline();
                    state.setInDoctype(false);
                    processDoctypeToken();
                } else if (isWhitespace) {
                    src.advance(m_lineNumber);
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                } else
                    m_doctypeToken.setState(DoctypeBogus);
                break;
            case DoctypeBeforeSystemID:
                if (c == '\"' || c == '\'') {
                    tquote = c == '\"' ? DoubleQuote : SingleQuote;
                    m_doctypeToken.setState(DoctypeSystemID);
                    src.advancePastNonNewline();
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                } else if (c == '>') {
                    // Considered bogus.  Don't process the doctype.
                    src.advancePastNonNewline();
                    state.setInDoctype(false);
                } else if (isWhitespace) {
                    src.advance(m_lineNumber);
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                } else
                    m_doctypeToken.setState(DoctypeBogus);
                break;
            case DoctypeSystemID:
                if ((c == '\"' && tquote == DoubleQuote) || (c == '\'' && tquote == SingleQuote)) {
                    src.advancePastNonNewline();
                    m_doctypeToken.setState(DoctypeAfterSystemID);
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                } else if (c == '>') {
                     // Considered bogus.  Don't process the doctype.
                    src.advancePastNonNewline();
                    state.setInDoctype(false);
                    if (inViewSourceMode())
                        processDoctypeToken();
                } else {
                    m_doctypeToken.m_systemID.append(c);
                    src.advance(m_lineNumber);
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                }
                break;
            case DoctypeAfterSystemID:
                if (c == '>') {
                    // Valid doctype. Emit it now.
                    src.advancePastNonNewline();
                    state.setInDoctype(false);
                    processDoctypeToken();
                } else if (isWhitespace) {
                    src.advance(m_lineNumber);
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                } else
                    m_doctypeToken.setState(DoctypeBogus);
                break;
            case DoctypeBogus:
                if (c == '>') {
                    // Done with the bogus doctype.
                    src.advancePastNonNewline();
                    state.setInDoctype(false);
                    if (inViewSourceMode())
                       processDoctypeToken();
                } else {
                    src.advance(m_lineNumber); // Just keep scanning for '>'
                    if (inViewSourceMode())
                        m_doctypeToken.m_source.append(c);
                }
                break;
            default:
                break;
        }
    }
    return state;
}

HTMLTokenizer::State HTMLTokenizer::parseTag(SegmentedString& src, State state)
{
    ASSERT(!state.hasEntityState());

    unsigned cBufferPos = m_cBufferPos;

    bool lastIsSlash = false;

    while (!src.isEmpty()) {
        checkBuffer();
        switch (state.tagState()) {
        case NoTag:
        {
            m_cBufferPos = cBufferPos;
            return state;
        }
        case TagName:
        {
            if (searchCount > 0) {
                if (*src == commentStart[searchCount]) {
                    searchCount++;
                    if (searchCount == 2)
                        m_doctypeSearchCount++; // A '!' is also part of a doctype, so we are moving through that still as well.
                    else
                        m_doctypeSearchCount = 0;
                    if (searchCount == 4) {
                        // Found '<!--' sequence
                        src.advancePastNonNewline();
                        m_dest = m_buffer; // ignore the previous part of this tag
                        state.setInComment(true);
                        state.setTagState(NoTag);

                        // Fix bug 34302 at kde.bugs.org.  Go ahead and treat
                        // <!--> as a valid comment, since both mozilla and IE on windows
                        // can handle this case.  Only do this in quirks mode. -dwh
                        if (!src.isEmpty() && *src == '>' && m_doc->inCompatMode()) {
                            state.setInComment(false);
                            src.advancePastNonNewline();
                            if (!src.isEmpty())
                                m_cBuffer[cBufferPos++] = *src;
                        } else
                          state = parseComment(src, state);

                        m_cBufferPos = cBufferPos;
                        return state; // Finished parsing tag!
                    }
                    m_cBuffer[cBufferPos++] = *src;
                    src.advancePastNonNewline();
                    break;
                } else
                    searchCount = 0; // Stop looking for '<!--' sequence
            }
            
            if (m_doctypeSearchCount > 0) {
                if (toASCIILower(*src) == doctypeStart[m_doctypeSearchCount]) {
                    m_doctypeSearchCount++;
                    m_cBuffer[cBufferPos++] = *src;
                    src.advancePastNonNewline();
                    if (m_doctypeSearchCount == 9) {
                        // Found '<!DOCTYPE' sequence
                        state.setInDoctype(true);
                        state.setTagState(NoTag);
                        m_doctypeToken.reset();
                        if (inViewSourceMode())
                            m_doctypeToken.m_source.append(m_cBuffer, cBufferPos);
                        state = parseDoctype(src, state);
                        m_cBufferPos = cBufferPos;
                        return state;
                    }
                    break;
                } else
                    m_doctypeSearchCount = 0; // Stop looking for '<!DOCTYPE' sequence
            }

            bool finish = false;
            unsigned int ll = min(src.length(), CBUFLEN - cBufferPos);
            while (ll--) {
                UChar curchar = *src;
                if (isASCIISpace(curchar) || curchar == '>' || curchar == '<') {
                    finish = true;
                    break;
                }
                
                // tolower() shows up on profiles. This is faster!
                if (curchar >= 'A' && curchar <= 'Z' && !inViewSourceMode())
                    m_cBuffer[cBufferPos++] = curchar + ('a' - 'A');
                else
                    m_cBuffer[cBufferPos++] = curchar;
                src.advancePastNonNewline();
            }

            // Disadvantage: we add the possible rest of the tag
            // as attribute names. ### judge if this causes problems
            if (finish || CBUFLEN == cBufferPos) {
                bool beginTag;
                UChar* ptr = m_cBuffer;
                unsigned int len = cBufferPos;
                m_cBuffer[cBufferPos] = '\0';
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
                    m_currentToken.tagName = AtomicString(ptr);
                    m_currentToken.beginTag = beginTag;
                }
                m_dest = m_buffer;
                state.setTagState(SearchAttribute);
                cBufferPos = 0;
            }
            break;
        }
        case SearchAttribute:
            while (!src.isEmpty()) {
                UChar curchar = *src;
                // In this mode just ignore any quotes we encounter and treat them like spaces.
                if (!isASCIISpace(curchar) && curchar != '\'' && curchar != '"') {
                    if (curchar == '<' || curchar == '>')
                        state.setTagState(SearchEnd);
                    else
                        state.setTagState(AttributeName);

                    cBufferPos = 0;
                    break;
                }
                if (inViewSourceMode())
                    m_currentToken.addViewSourceChar(curchar);
                src.advance(m_lineNumber);
            }
            break;
        case AttributeName:
        {
            m_rawAttributeBeforeValue.clear();
            int ll = min(src.length(), CBUFLEN - cBufferPos);
            while (ll--) {
                UChar curchar = *src;
                // If we encounter a "/" when scanning an attribute name, treat it as a delimiter.  This allows the 
                // cases like <input type=checkbox checked/> to work (and accommodates XML-style syntax as per HTML5).
                if (curchar <= '>' && (curchar >= '<' || isASCIISpace(curchar) || curchar == '/')) {
                    m_cBuffer[cBufferPos] = '\0';
                    m_attrName = AtomicString(m_cBuffer);
                    m_dest = m_buffer;
                    *m_dest++ = 0;
                    state.setTagState(SearchEqual);
                    if (inViewSourceMode())
                        m_currentToken.addViewSourceChar('a');
                    break;
                }
                
                // tolower() shows up on profiles. This is faster!
                if (curchar >= 'A' && curchar <= 'Z' && !inViewSourceMode())
                    m_cBuffer[cBufferPos++] = curchar + ('a' - 'A');
                else
                    m_cBuffer[cBufferPos++] = curchar;
                    
                m_rawAttributeBeforeValue.append(curchar);
                src.advance(m_lineNumber);
            }
            if (cBufferPos == CBUFLEN) {
                m_cBuffer[cBufferPos] = '\0';
                m_attrName = AtomicString(m_cBuffer);
                m_dest = m_buffer;
                *m_dest++ = 0;
                state.setTagState(SearchEqual);
                if (inViewSourceMode())
                    m_currentToken.addViewSourceChar('a');
            }
            break;
        }
        case SearchEqual:
            while (!src.isEmpty()) {
                UChar curchar = *src;

                if (lastIsSlash && curchar == '>') {
                    // This is a quirk (with a long sad history).  We have to do this
                    // since widgets do <script src="foo.js"/> and expect the tag to close.
                    if (m_currentToken.tagName == scriptTag)
                        m_currentToken.selfClosingTag = true;
                    m_currentToken.brokenXMLStyle = true;
                }

                // In this mode just ignore any quotes or slashes we encounter and treat them like spaces.
                if (!isASCIISpace(curchar) && curchar != '\'' && curchar != '"' && curchar != '/') {
                    if (curchar == '=') {
                        state.setTagState(SearchValue);
                        if (inViewSourceMode())
                            m_currentToken.addViewSourceChar(curchar);
                        m_rawAttributeBeforeValue.append(curchar);
                        src.advancePastNonNewline();
                    } else {
                        m_currentToken.addAttribute(m_attrName, emptyAtom, inViewSourceMode());
                        m_dest = m_buffer;
                        state.setTagState(SearchAttribute);
                        lastIsSlash = false;
                    }
                    break;
                }

                lastIsSlash = curchar == '/';

                if (inViewSourceMode())
                    m_currentToken.addViewSourceChar(curchar);
                m_rawAttributeBeforeValue.append(curchar);
                src.advance(m_lineNumber);
            }
            break;
        case SearchValue:
            while (!src.isEmpty()) {
                UChar curchar = *src;
                if (!isASCIISpace(curchar)) {
                    if (curchar == '\'' || curchar == '\"') {
                        tquote = curchar == '\"' ? DoubleQuote : SingleQuote;
                        state.setTagState(QuotedValue);
                        if (inViewSourceMode())
                            m_currentToken.addViewSourceChar(curchar);
                        m_rawAttributeBeforeValue.append(curchar);
                        src.advancePastNonNewline();
                    } else
                        state.setTagState(Value);

                    break;
                }
                if (inViewSourceMode())
                    m_currentToken.addViewSourceChar(curchar);
                m_rawAttributeBeforeValue.append(curchar);
                src.advance(m_lineNumber);
            }
            break;
        case QuotedValue:
            while (!src.isEmpty()) {
                checkBuffer();

                UChar curchar = *src;
                if (curchar <= '>' && !src.escaped()) {
                    if (curchar == '>' && m_attrName.isEmpty()) {
                        // Handle a case like <img '>.  Just go ahead and be willing
                        // to close the whole tag.  Don't consume the character and
                        // just go back into SearchEnd while ignoring the whole
                        // value.
                        // FIXME: Note that this is actually not a very good solution.
                        // It doesn't handle the general case of
                        // unmatched quotes among attributes that have names. -dwh
                        while (m_dest > m_buffer + 1 && (m_dest[-1] == '\n' || m_dest[-1] == '\r'))
                            m_dest--; // remove trailing newlines
                        AtomicString attributeValue(m_buffer + 1, m_dest - m_buffer - 1);
                        if (!attributeValue.contains('/'))
                            m_attrName = attributeValue; // Just make the name/value match. (FIXME: Is this some WinIE quirk?)
                        m_currentToken.addAttribute(m_attrName, attributeValue, inViewSourceMode());
                        if (inViewSourceMode())
                            m_currentToken.addViewSourceChar('x');
                        state.setTagState(SearchAttribute);
                        m_dest = m_buffer;
                        tquote = NoQuote;
                        break;
                    }
                    
                    if (curchar == '&') {
                        src.advancePastNonNewline();
                        state = parseEntity(src, m_dest, state, cBufferPos, true, true);
                        break;
                    }

                    if ((tquote == SingleQuote && curchar == '\'') || (tquote == DoubleQuote && curchar == '\"')) {
                        // some <input type=hidden> rely on trailing spaces. argh
                        while (m_dest > m_buffer + 1 && (m_dest[-1] == '\n' || m_dest[-1] == '\r'))
                            m_dest--; // remove trailing newlines
                        AtomicString attributeValue(m_buffer + 1, m_dest - m_buffer - 1);
                        if (m_attrName.isEmpty() && !attributeValue.contains('/')) {
                            m_attrName = attributeValue; // Make the name match the value. (FIXME: Is this a WinIE quirk?)
                            if (inViewSourceMode())
                                m_currentToken.addViewSourceChar('x');
                        } else if (inViewSourceMode())
                            m_currentToken.addViewSourceChar('v');

                        if (m_currentToken.beginTag && m_currentToken.tagName == scriptTag && !inViewSourceMode() && !m_parser->skipMode() && m_attrName == srcAttr) {
                            String context(m_rawAttributeBeforeValue.data(), m_rawAttributeBeforeValue.size());
                            if (m_XSSAuditor && !m_XSSAuditor->canLoadExternalScriptFromSrc(context, attributeValue))
                                attributeValue = blankURL().string();
                        }

                        m_currentToken.addAttribute(m_attrName, attributeValue, inViewSourceMode());
                        m_dest = m_buffer;
                        state.setTagState(SearchAttribute);
                        tquote = NoQuote;
                        if (inViewSourceMode())
                            m_currentToken.addViewSourceChar(curchar);
                        src.advancePastNonNewline();
                        break;
                    }
                }

                *m_dest++ = curchar;
                src.advance(m_lineNumber);
            }
            break;
        case Value:
            while (!src.isEmpty()) {
                checkBuffer();
                UChar curchar = *src;
                if (curchar <= '>' && !src.escaped()) {
                    // parse Entities
                    if (curchar == '&') {
                        src.advancePastNonNewline();
                        state = parseEntity(src, m_dest, state, cBufferPos, true, true);
                        break;
                    }
                    // no quotes. Every space means end of value
                    // '/' does not delimit in IE!
                    if (isASCIISpace(curchar) || curchar == '>') {
                        AtomicString attributeValue(m_buffer + 1, m_dest - m_buffer - 1);

                        if (m_currentToken.beginTag && m_currentToken.tagName == scriptTag && !inViewSourceMode() && !m_parser->skipMode() && m_attrName == srcAttr) {
                            String context(m_rawAttributeBeforeValue.data(), m_rawAttributeBeforeValue.size());
                            if (m_XSSAuditor && !m_XSSAuditor->canLoadExternalScriptFromSrc(context, attributeValue))
                                attributeValue = blankURL().string();
                        }

                        m_currentToken.addAttribute(m_attrName, attributeValue, inViewSourceMode());
                        if (inViewSourceMode())
                            m_currentToken.addViewSourceChar('v');
                        m_dest = m_buffer;
                        state.setTagState(SearchAttribute);
                        break;
                    }
                }

                *m_dest++ = curchar;
                src.advance(m_lineNumber);
            }
            break;
        case SearchEnd:
        {
            while (!src.isEmpty()) {
                UChar ch = *src;
                if (ch == '>' || ch == '<')
                    break;
                if (ch == '/')
                    m_currentToken.selfClosingTag = true;
                if (inViewSourceMode())
                    m_currentToken.addViewSourceChar(ch);
                src.advance(m_lineNumber);
            }
            if (src.isEmpty())
                break;

            searchCount = 0; // Stop looking for '<!--' sequence
            state.setTagState(NoTag);
            tquote = NoQuote;

            if (*src != '<')
                src.advance(m_lineNumber);

            if (m_currentToken.tagName == nullAtom) { //stop if tag is unknown
                m_cBufferPos = cBufferPos;
                return state;
            }

            AtomicString tagName = m_currentToken.tagName;

            // Handle <script src="foo"/> like Mozilla/Opera. We have to do this now for Dashboard
            // compatibility.
            bool isSelfClosingScript = m_currentToken.selfClosingTag && m_currentToken.beginTag && m_currentToken.tagName == scriptTag;
            bool beginTag = !m_currentToken.selfClosingTag && m_currentToken.beginTag;
            if (m_currentToken.beginTag && m_currentToken.tagName == scriptTag && !inViewSourceMode() && !m_parser->skipMode()) {
                Attribute* a = 0;
                m_scriptTagSrcAttrValue = String();
                m_scriptTagCharsetAttrValue = String();
                if (m_currentToken.attrs && !m_fragment) {
                    if (m_doc->frame() && m_doc->frame()->script()->isEnabled()) {
                        if ((a = m_currentToken.attrs->getAttributeItem(srcAttr)))
                            m_scriptTagSrcAttrValue = m_doc->completeURL(deprecatedParseURL(a->value())).string();
                    }
                }
            }

            RefPtr<Node> n = processToken();
            m_cBufferPos = cBufferPos;
            if (n || inViewSourceMode()) {
                State savedState = state;
                SegmentedString savedSrc = src;
                long savedLineno = m_lineNumber;
                if ((tagName == preTag || tagName == listingTag) && !inViewSourceMode()) {
                    if (beginTag)
                        state.setDiscardLF(true); // Discard the first LF after we open a pre.
                } else if (tagName == scriptTag) {
                    ASSERT(!m_scriptNode);
                    m_scriptNode = static_pointer_cast<HTMLScriptElement>(n);
                    if (m_scriptNode)
                        m_scriptTagCharsetAttrValue = m_scriptNode->scriptCharset();
                    if (beginTag) {
                        m_searchStopper = scriptEnd;
                        m_searchStopperLength = 8;
                        state.setInScript(true);
                        state = parseNonHTMLText(src, state);
                    } else if (isSelfClosingScript) { // Handle <script src="foo"/>
                        state.setInScript(true);
                        state = scriptHandler(state);
                    }
                } else if (tagName == styleTag) {
                    if (beginTag) {
                        m_searchStopper = styleEnd;
                        m_searchStopperLength = 7;
                        state.setInStyle(true);
                        state = parseNonHTMLText(src, state);
                    }
                } else if (tagName == textareaTag) {
                    if (beginTag) {
                        m_searchStopper = textareaEnd;
                        m_searchStopperLength = 10;
                        state.setInTextArea(true);
                        state = parseNonHTMLText(src, state);
                    }
                } else if (tagName == titleTag) {
                    if (beginTag) {
                        m_searchStopper = titleEnd;
                        m_searchStopperLength = 7;
                        state.setInTitle(true);
                        state = parseNonHTMLText(src, state);
                    }
                } else if (tagName == xmpTag) {
                    if (beginTag) {
                        m_searchStopper = xmpEnd;
                        m_searchStopperLength = 5;
                        state.setInXmp(true);
                        state = parseNonHTMLText(src, state);
                    }
                } else if (tagName == iframeTag) {
                    if (beginTag) {
                        m_searchStopper = iframeEnd;
                        m_searchStopperLength = 8;
                        state.setInIFrame(true);
                        state = parseNonHTMLText(src, state);
                    }
                }
                if (src.isEmpty() && (state.inTitle() || inViewSourceMode()) && !state.inComment() && !(state.inScript() && m_currentScriptTagStartLineNumber)) {
                    // We just ate the rest of the document as the #text node under the special tag!
                    // Reset the state then retokenize without special handling.
                    // Let the parser clean up the missing close tag.
                    // FIXME: This is incorrect, because src.isEmpty() doesn't mean we're
                    // at the end of the document unless m_noMoreData is also true. We need
                    // to detect this case elsewhere, and save the state somewhere other
                    // than a local variable.
                    state = savedState;
                    src = savedSrc;
                    m_lineNumber = savedLineno;
                    m_scriptCodeSize = 0;
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
    if (!state.loadingExtScript() && !state.forceSynchronous() && !m_executingScript && (processedCount > m_tokenizerChunkSize || allowedYield)) {
        processedCount = 0;
        if (currentTime() - startTime > m_tokenizerTimeDelay) {
            /* FIXME: We'd like to yield aggressively to give stylesheets the opportunity to
               load, but this hurts overall performance on slower machines.  For now turn this
               off.
            || (!m_doc->haveStylesheetsLoaded() && 
                (m_doc->documentElement()->id() != ID_HTML || m_doc->body()))) {*/
            // Schedule the timer to keep processing as soon as possible.
            m_timer.startOneShot(0);
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
            if (currentTime() - startTime > m_tokenizerTimeDelay)
                printf("Deferring processing of data because 500ms elapsed away from event loop.\n");
#endif
            return false;
        }
    }
    
    processedCount++;
    return true;
}
    
void HTMLTokenizer::write(const SegmentedString& str, bool appendData)
{
    if (!m_buffer)
        return;
    
    if (m_parserStopped)
        return;

    SegmentedString source(str);
    if (m_executingScript)
        source.setExcludeLineNumbers();

    if ((m_executingScript && appendData) || !m_pendingScripts.isEmpty()) {
        // don't parse; we will do this later
        if (m_currentPrependingSrc)
            m_currentPrependingSrc->append(source);
        else {
            m_pendingSrc.append(source);
#if PRELOAD_SCANNER_ENABLED
            if (m_preloadScanner && m_preloadScanner->inProgress() && appendData)
                m_preloadScanner->write(source);
#endif
        }
        return;
    }
    
#if PRELOAD_SCANNER_ENABLED
    if (m_preloadScanner && m_preloadScanner->inProgress() && appendData)
        m_preloadScanner->end();
#endif

    if (!m_src.isEmpty())
        m_src.append(source);
    else
        setSrc(source);

    // Once a timer is set, it has control of when the tokenizer continues.
    if (m_timer.isActive())
        return;

    bool wasInWrite = m_inWrite;
    m_inWrite = true;
    
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_doc->ownerElement())
        printf("Beginning write at time %d\n", m_doc->elapsedTime());
#endif

    int processedCount = 0;
    double startTime = currentTime();

#if ENABLE(INSPECTOR)
    InspectorTimelineAgent* timelineAgent = m_doc->inspectorTimelineAgent();
    if (timelineAgent)
        timelineAgent->willWriteHTML();
#endif
  
    Frame* frame = m_doc->frame();

    State state = m_state;

    while (!m_src.isEmpty() && (!frame || !frame->redirectScheduler()->locationChangePending())) {
        if (!continueProcessing(processedCount, startTime, state))
            break;

        // do we need to enlarge the buffer?
        checkBuffer();

        UChar cc = *m_src;

        bool wasSkipLF = state.skipLF();
        if (wasSkipLF)
            state.setSkipLF(false);

        if (wasSkipLF && (cc == '\n'))
            m_src.advance();
        else if (state.needsSpecialWriteHandling()) {
            // it's important to keep needsSpecialWriteHandling with the flags this block tests
            if (state.hasEntityState())
                state = parseEntity(m_src, m_dest, state, m_cBufferPos, false, state.hasTagState());
            else if (state.inPlainText())
                state = parseText(m_src, state);
            else if (state.inAnyNonHTMLText())
                state = parseNonHTMLText(m_src, state);
            else if (state.inComment())
                state = parseComment(m_src, state);
            else if (state.inDoctype())
                state = parseDoctype(m_src, state);
            else if (state.inServer())
                state = parseServer(m_src, state);
            else if (state.inProcessingInstruction())
                state = parseProcessingInstruction(m_src, state);
            else if (state.hasTagState())
                state = parseTag(m_src, state);
            else if (state.startTag()) {
                state.setStartTag(false);
                
                switch (cc) {
                case '/':
                    break;
                case '!': {
                    // <!-- comment --> or <!DOCTYPE ...>
                    searchCount = 1; // Look for '<!--' sequence to start comment or '<!DOCTYPE' sequence to start doctype
                    m_doctypeSearchCount = 1;
                    break;
                }
                case '?': {
                    // xml processing instruction
                    state.setInProcessingInstruction(true);
                    tquote = NoQuote;
                    state = parseProcessingInstruction(m_src, state);
                    continue;

                    break;
                }
                case '%':
                    if (!m_brokenServer) {
                        // <% server stuff, handle as comment %>
                        state.setInServer(true);
                        tquote = NoQuote;
                        state = parseServer(m_src, state);
                        continue;
                    }
                    // else fall through
                default: {
                    if ( ((cc >= 'a') && (cc <= 'z')) || ((cc >= 'A') && (cc <= 'Z'))) {
                        // Start of a Start-Tag
                    } else {
                        // Invalid tag
                        // Add as is
                        *m_dest = '<';
                        m_dest++;
                        continue;
                    }
                }
                }; // end case

                processToken();

                m_cBufferPos = 0;
                state.setTagState(TagName);
                state = parseTag(m_src, state);
            }
        } else if (cc == '&' && !m_src.escaped()) {
            m_src.advancePastNonNewline();
            state = parseEntity(m_src, m_dest, state, m_cBufferPos, true, state.hasTagState());
        } else if (cc == '<' && !m_src.escaped()) {
            m_currentTagStartLineNumber = m_lineNumber;
            m_src.advancePastNonNewline();
            state.setStartTag(true);
            state.setDiscardLF(false);
        } else if (cc == '\n' || cc == '\r') {
            if (state.discardLF())
                // Ignore this LF
                state.setDiscardLF(false); // We have discarded 1 LF
            else {
                // Process this LF
                *m_dest++ = '\n';
                if (cc == '\r' && !m_src.excludeLineNumbers())
                    m_lineNumber++;
            }

            /* Check for MS-DOS CRLF sequence */
            if (cc == '\r')
                state.setSkipLF(true);
            m_src.advance(m_lineNumber);
        } else {
            state.setDiscardLF(false);
            *m_dest++ = cc;
            m_src.advancePastNonNewline();
        }
    }
    
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_doc->ownerElement())
        printf("Ending write at time %d\n", m_doc->elapsedTime());
#endif

#if ENABLE(INSPECTOR)
    if (timelineAgent)
        timelineAgent->didWriteHTML();
#endif

    m_inWrite = wasInWrite;

    m_state = state;

    if (m_noMoreData && !m_inWrite && !state.loadingExtScript() && !m_executingScript && !m_timer.isActive())
        end(); // this actually causes us to be deleted
    
    // After parsing, go ahead and dispatch image beforeload/load events.
    ImageLoader::dispatchPendingEvents();
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
    return m_timer.isActive() || m_inWrite;
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

    // Invoke write() as though more data came in. This might cause us to get deleted.
    write(SegmentedString(), true);
}

void HTMLTokenizer::end()
{
    ASSERT(!m_timer.isActive());
    m_timer.stop(); // Only helps if assertion above fires, but do it anyway.

    if (m_buffer) {
        // parseTag is using the buffer for different matters
        if (!m_state.hasTagState())
            processToken();

        fastFree(m_scriptCode);
        m_scriptCode = 0;
        m_scriptCodeSize = m_scriptCodeCapacity = m_scriptCodeResync = 0;

        fastFree(m_buffer);
        m_buffer = 0;
    }

    if (!inViewSourceMode())
        m_parser->finished();
    else
        m_doc->finishedParsing();
}

void HTMLTokenizer::finish()
{
    // do this as long as we don't find matching comment ends
    while ((m_state.inComment() || m_state.inServer()) && m_scriptCode && m_scriptCodeSize) {
        // we've found an unmatched comment start
        if (m_state.inComment())
            m_brokenComments = true;
        else
            m_brokenServer = true;
        checkScriptBuffer();
        m_scriptCode[m_scriptCodeSize] = 0;
        m_scriptCode[m_scriptCodeSize + 1] = 0;
        int pos;
        String food;
        if (m_state.inScript() || m_state.inStyle() || m_state.inTextArea())
            food = String(m_scriptCode, m_scriptCodeSize);
        else if (m_state.inServer()) {
            food = "<";
            food.append(m_scriptCode, m_scriptCodeSize);
        } else {
            pos = find(m_scriptCode, m_scriptCodeSize, '>');
            food = String(m_scriptCode + pos + 1, m_scriptCodeSize - pos - 1);
        }
        fastFree(m_scriptCode);
        m_scriptCode = 0;
        m_scriptCodeSize = m_scriptCodeCapacity = m_scriptCodeResync = 0;
        m_state.setInComment(false);
        m_state.setInServer(false);
        if (!food.isEmpty())
            write(food, true);
    }
    // this indicates we will not receive any more data... but if we are waiting on
    // an external script to load, we can't finish parsing until that is done
    m_noMoreData = true;
    if (!m_inWrite && !m_state.loadingExtScript() && !m_executingScript && !m_timer.isActive())
        end(); // this actually causes us to be deleted
}

PassRefPtr<Node> HTMLTokenizer::processToken()
{
    ScriptController* scriptController = (!m_fragment && m_doc->frame()) ? m_doc->frame()->script() : 0;
    if (scriptController && scriptController->isEnabled())
        // FIXME: Why isn't this m_currentScriptTagStartLineNumber?  I suspect this is wrong.
        scriptController->setEventHandlerLineNumber(m_currentTagStartLineNumber + 1); // Script line numbers are 1 based.
    if (m_dest > m_buffer) {
        m_currentToken.text = StringImpl::createStrippingNullCharacters(m_buffer, m_dest - m_buffer);
        if (m_currentToken.tagName != commentAtom)
            m_currentToken.tagName = textAtom;
    } else if (m_currentToken.tagName == nullAtom) {
        m_currentToken.reset();
        if (scriptController)
            scriptController->setEventHandlerLineNumber(m_lineNumber + 1); // Script line numbers are 1 based.
        return 0;
    }

    m_dest = m_buffer;

    RefPtr<Node> n;
    
    if (!m_parserStopped) {
        if (NamedMappedAttrMap* map = m_currentToken.attrs.get())
            map->shrinkToLength();
        if (inViewSourceMode())
            static_cast<HTMLViewSourceDocument*>(m_doc)->addViewSourceToken(&m_currentToken);
        else
            // pass the token over to the parser, the parser DOES NOT delete the token
            n = m_parser->parseToken(&m_currentToken);
    }
    m_currentToken.reset();
    if (scriptController)
        scriptController->setEventHandlerLineNumber(0);

    return n.release();
}

void HTMLTokenizer::processDoctypeToken()
{
    if (inViewSourceMode())
        static_cast<HTMLViewSourceDocument*>(m_doc)->addViewSourceDoctypeToken(&m_doctypeToken);
    else
        m_parser->parseDoctypeToken(&m_doctypeToken);
}

HTMLTokenizer::~HTMLTokenizer()
{
    ASSERT(!m_inWrite);
    reset();
}


void HTMLTokenizer::enlargeBuffer(int len)
{
    // Resize policy: Always at least double the size of the buffer each time.
    int delta = max(len, m_bufferSize);

    // Check for overflow.
    // For now, handle overflow the same way we handle fastRealloc failure, with CRASH.
    static const int maxSize = INT_MAX / sizeof(UChar);
    if (delta > maxSize - m_bufferSize)
        CRASH();

    int newSize = m_bufferSize + delta;
    int oldOffset = m_dest - m_buffer;
    m_buffer = static_cast<UChar*>(fastRealloc(m_buffer, newSize * sizeof(UChar)));
    m_dest = m_buffer + oldOffset;
    m_bufferSize = newSize;
}

void HTMLTokenizer::enlargeScriptBuffer(int len)
{
    // Resize policy: Always at least double the size of the buffer each time.
    int delta = max(len, m_scriptCodeCapacity);

    // Check for overflow.
    // For now, handle overflow the same way we handle fastRealloc failure, with CRASH.
    static const int maxSize = INT_MAX / sizeof(UChar);
    if (delta > maxSize - m_scriptCodeCapacity)
        CRASH();

    int newSize = m_scriptCodeCapacity + delta;
    // If we allow fastRealloc(ptr, 0), it will call CRASH(). We run into this
    // case if the HTML being parsed begins with "<!--" and there's more data
    // coming.
    if (!newSize) {
        ASSERT(!m_scriptCode);
        return;
    }

    m_scriptCode = static_cast<UChar*>(fastRealloc(m_scriptCode, newSize * sizeof(UChar)));
    m_scriptCodeCapacity = newSize;
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

    ASSERT(!m_pendingScripts.isEmpty());

    // Make external scripts wait for external stylesheets.
    // FIXME: This needs to be done for inline scripts too.
    m_hasScriptsWaitingForStylesheets = !m_doc->haveStylesheetsLoaded();
    if (m_hasScriptsWaitingForStylesheets)
        return;

    bool finished = false;
    while (!finished && m_pendingScripts.first()->isLoaded()) {
        CachedScript* cs = m_pendingScripts.first().get();
        m_pendingScripts.removeFirst();
        ASSERT(cache()->disabled() || cs->accessCount() > 0);

        setSrc(SegmentedString());

        // make sure we forget about the script before we execute the new one
        // infinite recursion might happen otherwise
        ScriptSourceCode sourceCode(cs);
        bool errorOccurred = cs->errorOccurred();
        cs->removeClient(this);

        RefPtr<Node> n = m_scriptNode.release();

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
        if (!m_doc->ownerElement())
            printf("external script beginning execution at %d\n", m_doc->elapsedTime());
#endif

        if (errorOccurred)
            n->dispatchEvent(Event::create(eventNames().errorEvent, true, false));
        else {
            if (static_cast<HTMLScriptElement*>(n.get())->shouldExecuteAsJavaScript())
                m_state = scriptExecution(sourceCode, m_state);
#if ENABLE(XHTMLMP)
            else
                m_doc->setShouldProcessNoscriptElement(true);
#endif
            n->dispatchEvent(Event::create(eventNames().loadEvent, false, false));
        }

        // The state of m_pendingScripts.isEmpty() can change inside the scriptExecution()
        // call above, so test afterwards.
        finished = m_pendingScripts.isEmpty();
        if (finished) {
            ASSERT(!m_hasScriptsWaitingForStylesheets);
            m_state.setLoadingExtScript(false);
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
            if (!m_doc->ownerElement())
                printf("external script finished execution at %d\n", m_doc->elapsedTime());
#endif
        } else if (m_hasScriptsWaitingForStylesheets) {
            // m_hasScriptsWaitingForStylesheets flag might have changed during the script execution.
            // If it did we are now blocked waiting for stylesheets and should not execute more scripts until they arrive.
            finished = true;
        }

        // 'm_requestingScript' is true when we are called synchronously from
        // scriptHandler(). In that case scriptHandler() will take care
        // of m_pendingSrc.
        if (!m_requestingScript) {
            SegmentedString rest = m_pendingSrc;
            m_pendingSrc.clear();
            write(rest, false);
            // we might be deleted at this point, do not access any members.
        }
    }
}

bool HTMLTokenizer::isWaitingForScripts() const
{
    return m_state.loadingExtScript();
}

void HTMLTokenizer::setSrc(const SegmentedString& source)
{
    m_src = source;
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
