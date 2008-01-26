/*
    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)
              (C) 1998 Waldo Bastian (bastian@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

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

#ifndef HTMLTokenizer_h
#define HTMLTokenizer_h

#include "DeprecatedPtrQueue.h"
#include "NamedMappedAttrMap.h"
#include "SegmentedString.h"
#include "Timer.h"
#include "Tokenizer.h"
#include "CachedResourceClient.h"
#include <wtf/Vector.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

class CachedScript;
class DocumentFragment;
class Document;
class HTMLDocument;
class HTMLViewSourceDocument;
class FrameView;
class HTMLParser;
class Node;

/**
 * @internal
 * represents one HTML tag. Consists of a numerical id, and the list
 * of attributes. Can also represent text. In this case the id = 0 and
 * text contains the text.
 */
class Token {
public:
    Token() : beginTag(true), flat(false), brokenXMLStyle(false), m_sourceInfo(0) { }
    ~Token() { }

    void addAttribute(Document*, AtomicString& attrName, const AtomicString& v, bool viewSourceMode);

    bool isOpenTag(const QualifiedName& fullName) const { return beginTag && fullName.localName() == tagName; }
    bool isCloseTag(const QualifiedName& fullName) const { return !beginTag && fullName.localName() == tagName; }

    void reset()
    {
        attrs = 0;
        text = 0;
        tagName = nullAtom;
        beginTag = true;
        flat = false;
        brokenXMLStyle = false;
        if (m_sourceInfo)
            m_sourceInfo->clear();
    }

    void addViewSourceChar(UChar c) { if (!m_sourceInfo.get()) m_sourceInfo.set(new Vector<UChar>); m_sourceInfo->append(c); }

    RefPtr<NamedMappedAttrMap> attrs;
    RefPtr<StringImpl> text;
    AtomicString tagName;
    bool beginTag;
    bool flat;
    bool brokenXMLStyle;
    OwnPtr<Vector<UChar> > m_sourceInfo;
};

//-----------------------------------------------------------------------------

class HTMLTokenizer : public Tokenizer, public CachedResourceClient {
public:
    HTMLTokenizer(HTMLDocument*, bool reportErrors);
    HTMLTokenizer(HTMLViewSourceDocument*);
    HTMLTokenizer(DocumentFragment*);
    virtual ~HTMLTokenizer();

    virtual bool write(const SegmentedString&, bool appendData);
    virtual void finish();
    virtual void setForceSynchronous(bool force);
    virtual bool isWaitingForScripts() const;
    virtual void stopParsing();
    virtual bool processingData() const;
    virtual int executingScript() const { return m_executingScript; }

    virtual int lineNumber() const { return m_lineNumber; }
    virtual int columnNumber() const { return 1; }

    bool processingContentWrittenByScript() const { return src.excludeLineNumbers(); }
    
    virtual void executeScriptsWaitingForStylesheets();
    
    virtual bool isHTMLTokenizer() const { return true; }
    HTMLParser* htmlParser() const { return parser; }

private:
    class State;

    // Where we are in parsing a tag
    void begin();
    void end();

    void reset();
    PassRefPtr<Node> processToken();

    State processListing(SegmentedString, State);
    State parseComment(SegmentedString&, State);
    State parseServer(SegmentedString&, State);
    State parseText(SegmentedString&, State);
    State parseSpecial(SegmentedString&, State);
    State parseTag(SegmentedString&, State);
    State parseEntity(SegmentedString&, UChar*& dest, State, unsigned& _cBufferPos, bool start, bool parsingTag);
    State parseProcessingInstruction(SegmentedString&, State);
    State scriptHandler(State);
    State scriptExecution(const String& script, State, const String& scriptURL, int baseLine = 0);
    void setSrc(const SegmentedString&);
 
    // check if we have enough space in the buffer.
    // if not enlarge it
    inline void checkBuffer(int len = 10)
    {
        if ((dest - buffer) > size - len)
            enlargeBuffer(len);
    }

    inline void checkScriptBuffer(int len = 10)
    {
        if (scriptCodeSize + len >= scriptCodeMaxSize)
            enlargeScriptBuffer(len);
    }

    void enlargeBuffer(int len);
    void enlargeScriptBuffer(int len);

    bool continueProcessing(int& processedCount, double startTime, State&);
    void timerFired(Timer<HTMLTokenizer>*);
    void allDataProcessed();

    // from CachedResourceClient
    void notifyFinished(CachedResource *finishedObj);

    // Internal buffers
    ///////////////////
    UChar* buffer;
    UChar* dest;

    Token currToken;

    // the size of buffer
    int size;

    // Tokenizer flags
    //////////////////
    // are we in quotes within a html tag
    enum { NoQuote, SingleQuote, DoubleQuote } tquote;

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
        State() : m_bits(0) { }

        TagState tagState() const { return static_cast<TagState>(m_bits & TagMask); }
        void setTagState(TagState t) { m_bits = (m_bits & ~TagMask) | t; }
        EntityState entityState() const { return static_cast<EntityState>((m_bits & EntityMask) >> EntityShift); }
        void setEntityState(EntityState e) { m_bits = (m_bits & ~EntityMask) | (e << EntityShift); }

        bool inScript() const { return testBit(InScript); }
        void setInScript(bool v) { setBit(InScript, v); }
        bool inStyle() const { return testBit(InStyle); }
        void setInStyle(bool v) { setBit(InStyle, v); }
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
            // Bit 9 unused
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
            ForceSynchronous = 1 << 23
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
    AtomicString attrName;

    // Used to store the code of a srcipting sequence
    UChar* scriptCode;
    // Size of the script sequenze stored in @ref #scriptCode
    int scriptCodeSize;
    // Maximal size that can be stored in @ref #scriptCode
    int scriptCodeMaxSize;
    // resync point of script code size
    int scriptCodeResync;

    // Stores characters if we are scanning for a string like "</script>"
    UChar searchBuffer[10];
    // Counts where we are in the string we are scanning for
    int searchCount;
    // The string we are searching for
    const UChar* searchFor;
    // the stopper string
    const char* searchStopper;
    // the stopper len
    int searchStopperLen;
    // if no more data is coming, just parse what we have (including ext scripts that
    // may be still downloading) and finish
    bool noMoreData;
    // URL to get source code of script from
    String scriptSrc;
    String scriptSrcCharset;
    // the HTML code we will parse after the external script we are waiting for has loaded
    SegmentedString pendingSrc;

    // the HTML code we will parse after this particular script has
    // loaded, but before all pending HTML
    SegmentedString *currentPrependingSrc;

    // true if we are executing a script while parsing a document. This causes the parsing of
    // the output of the script to be postponed until after the script has finished executing
    int m_executingScript;
    DeprecatedPtrQueue<CachedScript> pendingScripts;
    RefPtr<Node> scriptNode;

    bool m_requestingScript;
    bool m_hasScriptsWaitingForStylesheets;

    // if we found one broken comment, there are most likely others as well
    // store a flag to get rid of the O(n^2) behaviour in such a case.
    bool brokenComments;
    // current line number
    int m_lineNumber;
    // line number at which the current <script> started
    int scriptStartLineno;
    int tagStartLineno;

    // The timer for continued processing.
    Timer<HTMLTokenizer> m_timer;

// This buffer can hold arbitrarily long user-defined attribute names, such as in EMBED tags.
// So any fixed number might be too small, but rather than rewriting all usage of this buffer
// we'll just make it large enough to handle all imaginable cases.
#define CBUFLEN 1024
    UChar cBuffer[CBUFLEN + 2];
    unsigned int m_cBufferPos;

    SegmentedString src;
    Document* m_doc;
    HTMLParser* parser;
    bool inWrite;
    bool m_fragment;
};

void parseHTMLDocumentFragment(const String&, DocumentFragment*);

UChar decodeNamedEntity(const char*);

} // namespace WebCore

#endif // HTMLTokenizer_h
