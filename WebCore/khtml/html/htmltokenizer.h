/*
    This file is part of the KDE libraries

    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)
              (C) 1998 Waldo Bastian (bastian@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2003 Apple Computer, Inc.

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
// KDE HTML Widget -- Tokenizers

#ifndef HTMLTOKENIZER_H
#define HTMLTOKENIZER_H

#include <qstring.h>
#include <qobject.h>
#include <qptrqueue.h>

#include "misc/loader_client.h"
#include "xml/xml_tokenizer.h"
#include "html/html_elementimpl.h"

#ifdef __OBJC__
#define id id_AVOID_KEYWORD
#endif

class HTMLParser;
class KHTMLView;

namespace DOM {
    class DocumentFragmentImpl;
}

namespace khtml {

class CachedScript;


/**
 * @internal
 * represents one HTML tag. Consists of a numerical id, and the list
 * of attributes. Can also represent text. In this case the id = 0 and
 * text contains the text.
 */
class Token
{
public:
    Token() : beginTag(true), flat(false) { }

    void addAttribute(DOM::DocumentImpl*, const DOM::AtomicString& attrName, const DOM::AtomicString& v);

    bool isOpenTag(const DOM::QualifiedName& fullName) const { return beginTag && fullName.localName() == tagName; }
    bool isCloseTag(const DOM::QualifiedName& fullName) const { return !beginTag && fullName.localName() == tagName; }

    void reset()
    {
        attrs = 0;
        text = 0;        
        tagName = DOM::nullAtom;
        beginTag = true;
        flat = false;
    }

    RefPtr<DOM::NamedMappedAttrMapImpl> attrs;
    RefPtr<DOM::DOMStringImpl> text;
    DOM::AtomicString tagName;
    bool beginTag;
    bool flat;
};

//-----------------------------------------------------------------------------

class HTMLTokenizer : public Tokenizer, public CachedObjectClient
{
public:
    HTMLTokenizer(DOM::DocumentImpl *, KHTMLView * = 0, bool includesComments=false);
    HTMLTokenizer(DOM::DocumentImpl *, DOM::DocumentFragmentImpl *frag, bool includesComments=false);
    virtual ~HTMLTokenizer();

    virtual bool write(const TokenizerString &str, bool appendData);
    virtual void finish();
    virtual void setOnHold(bool onHold);
    virtual void setForceSynchronous(bool force);
    virtual bool isWaitingForScripts() const;
    virtual void stopParsing();
    virtual bool processingData() const;

protected:
    class State;

    // Where we are in parsing a tag
    void begin();
    void end();

    void reset();
    DOM::NodeImpl *processToken();

    State processListing(TokenizerString, State);
    State parseComment(TokenizerString&, State);
    State parseServer(TokenizerString&, State);
    State parseText(TokenizerString&, State);
    State parseSpecial(TokenizerString&, State);
    State parseTag(TokenizerString&, State);
    State parseEntity(TokenizerString &, QChar*& dest, State, unsigned& _cBufferPos, bool start, bool parsingTag);
    State parseProcessingInstruction(TokenizerString&, State);
    State scriptHandler(State);
    State scriptExecution(const QString& script, State state, QString scriptURL = QString(), int baseLine = 0);
    void setSrc(const TokenizerString &source);

    // check if we have enough space in the buffer.
    // if not enlarge it
    inline void checkBuffer(int len = 10)
    {
        if ( (dest - buffer) > size-len )
            enlargeBuffer(len);
    }
    inline void checkScriptBuffer(int len = 10)
    {
        if ( scriptCodeSize + len >= scriptCodeMaxSize )
            enlargeScriptBuffer(len);
    }

    void enlargeBuffer(int len);
    void enlargeScriptBuffer(int len);

    bool continueProcessing(int& processedCount, const QTime& startTime, State &state);
    void timerEvent(QTimerEvent*);
    void allDataProcessed();

    // from CachedObjectClient
    void notifyFinished(CachedObject *finishedObj);

protected:
    // Internal buffers
    ///////////////////
    QChar *buffer;
    QChar *dest;

    Token currToken;

    // the size of buffer
    int size;

    // Tokenizer flags
    //////////////////
    // are we in quotes within a html tag
    enum
    {
        NoQuote = 0,
        SingleQuote,
        DoubleQuote
    } tquote;

    // Are we in a &... character entity description?
    enum EntityState {
        NoEntity = 0,
        SearchEntity = 1,
        NumericSearch = 2,
        Hexadecimal = 3,
        Decimal = 4,
        EntityName = 5,
        SearchSemicolon = 6
    };
    unsigned EntityUnicodeValue;

    enum TagState {
        NoTag = 0,
        TagName = 1,
        SearchAttribute = 2,
        AttributeName = 3,
        SearchEqual = 4,
        SearchValue = 5,
        QuotedValue = 6,
        Value = 7,
        SearchEnd = 8
    };

    class State {
    public:
        State() : m_bits(0) {}

        TagState tagState() const { return static_cast<TagState>(m_bits & TagMask); }
        void setTagState(TagState t) { m_bits = (m_bits & ~TagMask) | t; }
        EntityState entityState() const { return static_cast<EntityState>((m_bits & EntityMask) >> EntityShift); }
        void setEntityState(EntityState e) { m_bits = (m_bits & ~EntityMask) | (e << EntityShift); }

        bool inScript() const { return testBit(InScript); }
        void setInScript(bool v) { setBit(InScript, v); }
        bool inStyle() const { return testBit(InStyle); }
        void setInStyle(bool v) { setBit(InStyle, v); }
        bool inSelect() const { return testBit(InSelect); }
        void setInSelect(bool v) { setBit(InSelect, v); }
        bool inXmp() const { return testBit(InXmp); }
        void setInXmp(bool v) { setBit(InXmp, v); }
        bool inTitle() const { return testBit(InTitle); }
        void setInTitle(bool v) { setBit(InTitle, v); }
        bool inPlainText() const { return testBit(InPlainText); }
        void setInPlainText(bool v) { setBit(InPlainText, v); }
        bool inProcessingInstruction() const { return testBit(InProcessingInstruction); }
        void setInProcessingInstruction(bool v) { return setBit(InProcessingInstruction, v); }
        bool inComment() const { return testBit(InComment); }
        void setInComment(bool v) { setBit(InComment, v); }
        bool inTextArea() const { return testBit(InTextArea); }
        void setInTextArea(bool v) { setBit(InTextArea, v); }
        bool escaped() const { return testBit(Escaped); }
        void setEscaped(bool v) { setBit(Escaped, v); }
        bool inServer() const { return testBit(InServer); }
        void setInServer(bool v) { setBit(InServer, v); }
        bool skipLF() const { return testBit(SkipLF); }
        void setSkipLF(bool v) { setBit(SkipLF, v); }
        bool startTag() const { return testBit(StartTag); }
        void setStartTag(bool v) { setBit(StartTag, v); }
        bool discardLF() const { return testBit(DiscardLF); }
        void setDiscardLF(bool v) { setBit(DiscardLF, v); }
        bool allowYield() const { return testBit(AllowYield); }
        void setAllowYield(bool v) { setBit(AllowYield, v); }
        bool loadingExtScript() const { return testBit(LoadingExtScript); }
        void setLoadingExtScript(bool v) { setBit(LoadingExtScript, v); }
        bool forceSynchronous() const { return testBit(ForceSynchronous); }
        void setForceSynchronous(bool v) { setBit(ForceSynchronous, v); }

        bool inAnySpecial() const { return m_bits & (InScript | InStyle | InXmp | InTextArea | InTitle); }
        bool hasTagState() const { return m_bits & TagMask; }
        bool hasEntityState() const { return m_bits & EntityMask; }

        bool needsSpecialWriteHandling() const { return m_bits & (InScript | InStyle | InXmp | InTextArea | InTitle | TagMask | EntityMask | InPlainText | InComment | InServer | InProcessingInstruction | StartTag); }

    private:
        static const int EntityShift = 4;
        enum StateBits {
            TagMask = (1 << 4) - 1,
            EntityMask = (1 << 7) - (1 << 4),
            InScript = 1 << 7,
            InStyle = 1 << 8,
            InSelect = 1 << 9,
            InXmp = 1 << 10,
            InTitle = 1 << 11,
            InPlainText = 1 << 12,
            InProcessingInstruction = 1 << 13,
            InComment = 1 << 14,
            InTextArea = 1 << 15,
            Escaped = 1 << 16,
            InServer = 1 << 17,
            SkipLF = 1 << 18,
            StartTag = 1 << 19,
            DiscardLF = 1 << 20, // FIXME: should clarify difference between skip and discard
            AllowYield = 1 << 21,
            LoadingExtScript = 1 << 22,
            ForceSynchronous = 1 << 23,
        };
    
        void setBit(StateBits bit, bool value) 
        { 
            if (value) 
                m_bits |= bit; 
            else 
                m_bits &= ~bit;
        }
        bool testBit(StateBits bit) const { return m_bits & bit; }

        unsigned m_bits;
    };

    State m_state;

    bool brokenServer;

    // Name of an attribute that we just scanned.
    DOM::AtomicString attrName;
    
    // Used to store the code of a srcipting sequence
    QChar *scriptCode;
    // Size of the script sequenze stored in @ref #scriptCode
    int scriptCodeSize;
    // Maximal size that can be stored in @ref #scriptCode
    int scriptCodeMaxSize;
    // resync point of script code size
    int scriptCodeResync;

    // Stores characters if we are scanning for a string like "</script>"
    QChar searchBuffer[ 10 ];
    // Counts where we are in the string we are scanning for
    int searchCount;
    // The string we are searching for
    const QChar *searchFor;
    // the stopper string
    const char* searchStopper;
    // the stopper len
    int searchStopperLen;
    // if no more data is coming, just parse what we have (including ext scripts that
    // may be still downloading) and finish
    bool noMoreData;
    // URL to get source code of script from
    QString scriptSrc;
    QString scriptSrcCharset;
    bool javascript;
    // the HTML code we will parse after the external script we are waiting for has loaded
    TokenizerString pendingSrc;

    // the HTML code we will parse after this particular script has
    // loaded, but before all pending HTML
    TokenizerString *currentPrependingSrc;

    // true if we are executing a script while parsing a document. This causes the parsing of
    // the output of the script to be postponed until after the script has finished executing
    int m_executingScript;
    QPtrQueue<CachedScript> pendingScripts;
    RefPtr<DOM::NodeImpl> scriptNode;
    // you can pause the tokenizer if you need to display a dialog or something
    bool onHold;

    // if we found one broken comment, there are most likely others as well
    // store a flag to get rid of the O(n^2) behaviour in such a case.
    bool brokenComments;
    // current line number
    int lineno;
    // line number at which the current <script> started
    int scriptStartLineno;
    int tagStartLineno;

    // The timer for continued processing.
    int timerId;

    bool includesCommentsInDOM;

// This buffer can hold arbitrarily long user-defined attribute names, such as in EMBED tags.
// So any fixed number might be too small, but rather than rewriting all usage of this buffer
// we'll just make it large enough to handle all imaginable cases.
#define CBUFLEN 1024
    char cBuffer[CBUFLEN+2];
    unsigned int m_cBufferPos;
    
    TokenizerString src;
    HTMLParser *parser;
    QGuardedPtr<KHTMLView> view;    
    bool inWrite;
};

void parseHTMLDocumentFragment(const DOM::DOMString &, DOM::DocumentFragmentImpl *);

}

#undef id

#endif // HTMLTOKENIZER
