/*
    This file is part of the KDE libraries

    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)
              (C) 1998 Waldo Bastian (bastian@kde.org)
              (C) 1999 Lars Knoll (knoll@kde.org)
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
// KDE HTML Widget -- HTML Parser

#ifndef HTMLPARSER_H
#define HTMLPARSER_H

#include "html/html_documentimpl.h"

class FrameView;
class HTMLStackElem;

namespace DOM {
    class HTMLDocumentImpl;
    class HTMLElementImpl;
    class NodeImpl;
    class HTMLFormElementImpl;
    class HTMLMapElementImpl;
    class HTMLHeadElementImpl;
    class DocumentFragmentImpl;
}

namespace khtml {
    class Token;
}

/**
 * The parser for html. It receives a stream of tokens from the HTMLTokenizer, and
 * builds up the Document structure form it.
 */
class HTMLParser
{
public:
    HTMLParser(FrameView *w, DOM::DocumentImpl *i, bool includesComments=false);
    HTMLParser(DOM::DocumentFragmentImpl *frag, DOM::DocumentImpl *doc, bool includesComments=false);
    virtual ~HTMLParser();

    /**
     * parses one token delivered by the tokenizer
     */
    DOM::NodeImpl *parseToken(khtml::Token *_t);
    
    /**
     * tokenizer says it's not going to be sending us any more tokens
     */
    void finished();

    /**
     * resets the parser
     */
    void reset();

    bool skipMode() const { return !discard_until.isNull(); }
    bool noSpaces() const { return !inBody; }
    bool selectMode() const { return inSelect; }

    DOM::HTMLDocumentImpl *doc() const { return static_cast<DOM::HTMLDocumentImpl *>(document); }

protected:
    void setCurrent(DOM::NodeImpl* newCurrent);
    void setSkipMode(const DOM::QualifiedName& qName) { discard_until = qName.localName(); }

    FrameView *HTMLWidget;
    DOM::DocumentImpl *document;

    /*
     * generate a node from the token
     */
    DOM::NodeImpl *getNode(khtml::Token *);
    bool textCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool commentCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool headCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool bodyCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool framesetCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool iframeCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool formCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool isindexCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool selectCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool ddCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool dtCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool nestedCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool nestedStyleCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool tableCellCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool tableSectionCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool noembedCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool noscriptCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool noframesCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool nolayerCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);
    bool mapCreateErrorCheck(khtml::Token*, DOM::NodeImpl*&);

    void processCloseTag(khtml::Token *);

    bool insertNode(DOM::NodeImpl *n, bool flat = false);
    bool handleError(DOM::NodeImpl* n, bool flat, const DOM::AtomicString& localName, int tagPriority);
    
    // The currently active element (the one new elements will be added to).  Can be a DocumentFragment, a Document or an Element.
    DOM::NodeImpl* current;

    bool currentIsReferenced;

    HTMLStackElem *blockStack;

    void pushBlock(const DOM::AtomicString& tagName, int _level);
    void popBlock(const DOM::AtomicString& tagName);
    void popBlock(const DOM::QualifiedName& qName) { return popBlock(qName.localName()); } // Convenience function for readability.
    void popOneBlock(bool delBlock = true);
    void popInlineBlocks();

    void freeBlock();

    void createHead();

    bool isResidualStyleTag(const DOM::AtomicString& tagName);
    bool isAffectedByResidualStyle(const DOM::AtomicString& tagName);
    void handleResidualStyleCloseTagAcrossBlocks(HTMLStackElem* elem);
    void reopenResidualStyleTags(HTMLStackElem* elem, DOM::NodeImpl* malformedTableParent);

    bool allowNestedRedundantTag(const DOM::AtomicString& tagName);
    
    static bool isHeaderTag(const DOM::AtomicString& tagName);
    void popNestedHeaderTag();

    bool isInline(DOM::NodeImpl* node) const;
    
    /*
     * currently active form
     */
    DOM::HTMLFormElementImpl *form;

    /*
     * current map
     */
    DOM::HTMLMapElementImpl *map;

    /*
     * the head element. Needed for crappy html which defines <base> after </head>
     */
    DOM::HTMLHeadElementImpl *head;

    /*
     * a possible <isindex> element in the head. Compatibility hack for
     * html from the stone age
     */
    RefPtr<DOM::NodeImpl> isindex;
    DOM::NodeImpl* handleIsindex(khtml::Token*);

    /*
     * inserts the stupid isIndex element.
     */
    void startBody();

    bool inBody;
    bool haveContent;
    bool haveFrameSet;
    bool end;
    bool inSelect;

    /*
     * tells the parser to discard all tags, until it reaches the one specified
     */
    DOM::AtomicString discard_until;

    bool headLoaded;
    int inStrayTableContent;

    bool includesCommentsInDOM;
};
    
#endif // HTMLPARSER_H
