/*
    This file is part of the KDE libraries

    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)
              (C) 1998 Waldo Bastian (bastian@kde.org)
              (C) 2001 Dirk Mueller (mueller@kde.org)

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
// $Id$

#ifndef HTMLTOKENIZER_H
#define HTMLTOKENIZER_H


//
// External Classes
//
///////////////////

class KCharsets;

//
// Internal Classes
//
///////////////////

class StringTokenizer;
class HTMLTokenizer;

#include <qstring.h>
#include <qobject.h>

#include "stringit.h"
#include "loader_client.h"
#include "xml/dom_elementimpl.h"
#include "misc/htmltags.h"
#include "xml/dom_stringimpl.h"

class KHTMLParser;
class KHTMLView;

namespace DOM {
    class DocumentPtr;
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
        Token() {
            id = 0;
            complexText = false;
            attrs = 0;
            text = 0;
            //qDebug("new token, creating %08lx", attrs);
        }
        ~Token() {
            if(attrs) attrs->deref();
            if(text) text->deref();
        }
        void insertAttr(AttrImpl* a)
        {
            if(!attrs) {
                attrs = new DOM::NamedAttrMapImpl(0);
                attrs->ref();
            }
            attrs->insertAttr(a);
        }
        void reset()
        {
            if(attrs) {
                attrs->deref();
                attrs = 0;
            }
            id = 0;
            complexText = false;
            if(text) {
                text->deref();
                text = 0;
            }
        }
        DOM::NamedAttrMapImpl* attrs;
        ushort id;
        DOMStringImpl* text;
        bool complexText;
    };
};

// The count of spaces used for each tab.
#define TAB_SIZE 8


class Tokenizer : public QObject
{
    Q_OBJECT
public:
    virtual void begin() = 0;
    // script output must be prepended, while new data
    // received during executing a script must be appended, hence the
    // extra bool to be able to distinguish between both cases. document.write()
    // always uses false, while khtmlpart uses true
    virtual void write( const QString &str, bool appendData) = 0;
    virtual void end() = 0;
    virtual void finish() = 0;
    virtual void setOnHold(bool /*_onHold*/) {}

signals:
    void finishedParsing();

};

//-----------------------------------------------------------------------------

/**
 * @internal
 * This class takes QStrings as input, and splits up the input streams into
 * tokens, which are passed on to the @ref KHTMLParser.
 */

class HTMLTokenizer : public Tokenizer, public khtml::CachedObjectClient
{
    Q_OBJECT
public:
    HTMLTokenizer(DOM::DocumentPtr *, KHTMLView * = 0);
    HTMLTokenizer(DOM::DocumentPtr *, DOM::DocumentFragmentImpl *frag);
    virtual ~HTMLTokenizer();

    void begin();
    void write( const QString &str, bool appendData );
    void end();
    void finish();
    virtual void setOnHold(bool _onHold);

protected:
    void reset();
    void addPending();
    void processToken();
    void processListing(khtml::DOMStringIt list);

    void parseComment(khtml::DOMStringIt &str);
    void parseText(khtml::DOMStringIt &str);
    void parseListing(khtml::DOMStringIt &str);
    void parseSpecial(khtml::DOMStringIt &str, bool begin);
    void parseTag(khtml::DOMStringIt &str);
    void parseEntity(khtml::DOMStringIt &str, QChar *&dest, bool start = false);
    void parseProcessingInstruction(khtml::DOMStringIt &str);
    void scriptHandler();
    void scriptExecution(const QString& script);
    void addPendingSource();

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

    // from CachedObjectClient
    void notifyFinished(khtml::CachedObject *finishedObj);
protected:
    // Internal buffers
    ///////////////////
    QChar *buffer;
    QChar *dest;

    khtml::Token currToken;

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

    enum
    {
        NonePending = 0,
        SpacePending,
        LFPending,
        TabPending
    } pending;

    // Discard line breaks immediately after start-tags
    // Discard spaces after '=' within tags
    enum
    {
        NoneDiscard = 0,
        SpaceDiscard,
        LFDiscard,
        AllDiscard  // discard all spaces, LF's etc until next non white char
    } discard;

    // Discard the LF part of CRLF sequence
    bool skipLF;

    // Flag to say that we have the '<' but not the character following it.
    bool startTag;

    // Flag to say, we are just parsing a tag, meaning, we are in the middle
    // of <tag...
    enum {
        NoTag = 0,
        TagName,
        SearchAttribute,
        AttributeName,
        SearchEqual,
        SearchValue,
        QuotedValue,
        Value,
        SearchEnd
    } tag;

    // Are we in a &... character entity description?
    enum {
        NoEntity = 0,
        SearchEntity,
        NumericSearch,
        Hexadecimal,
        Decimal,
        EntityName,
        SearchSemicolon
    } Entity;

    // are we in a <script> ... </script block
    bool script;

    QChar EntityChar;

    // Are we in a <pre> ... </pre> block
    bool pre;

    // if 'pre == true' we track in which column we are
    int prePos;

    // Are we in a <style> ... </style> block
    bool style;

    // Are we in a <select> ... </select> block
    bool select;

    // Are we in a <listing> ... </listing> block
    bool listing;

    // Are we in plain textmode ?
    bool plaintext;

    // XML processing instructions. Ignored at the moment
    bool processingInstruction;

    // Area we in a <!-- comment --> block
    bool comment;

    // Are we in a <textarea> ... </textarea> block
    bool textarea;

    // was the previous character escaped ?
    bool escaped;

    // name of an unknown attribute
    QString attrName;

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
    // true if we are waiting for an external script (<SCRIPT SRC=...) to load, i.e.
    // we don't do any parsing while this is true
    bool loadingExtScript;
    // if no more data is coming, just parse what we have (including ext scripts that
    // may be still downloading) and finish
    bool noMoreData;
    // URL to get source code of script from
    QString scriptSrc;
    QString scriptSrcCharset;
    bool javascript;
    // the HTML code we will parse after the external script we are waiting for has loaded
    QString pendingSrc;
    // true if we are executing a script while parsing a document. This causes the parsing of
    // the output of the script to be postponed until after the script has finished executing
    int m_executingScript;
    khtml::CachedScript *cachedScript;
    // you can pause the tokenizer if you need to display a dialog or something
    bool onHold;

    // if we found one broken comment, there are most likely others as well
    // store a flag to get rid of the O(n^2) behaviour in such a case.
    bool brokenComments;

#define CBUFLEN 14
    char cBuffer[CBUFLEN+2];
    unsigned int cBufferPos;

    QString _src;
    khtml::DOMStringIt src;

    KCharsets *charsets;
    KHTMLParser *parser;

    KHTMLView *view;
};

#endif // HTMLTOKENIZER

