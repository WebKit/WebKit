/*
    This file is part of the KDE libraries

    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)
              (C) 1998 Waldo Bastian (bastian@kde.org)
              (C) 1999 Lars Knoll (knoll@kde.org)
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
//----------------------------------------------------------------------------
//
// KDE HTML Widget -- HTML Parser

#ifndef HTMLPARSER_H
#define HTMLPARSER_H

#include "html_documentimpl.h"

namespace WebCore {

class DocumentFragmentImpl;
class FrameView;
class HTMLElementImpl;
class HTMLFormElementImpl;
class HTMLHeadElementImpl;
class HTMLMapElementImpl;
class HTMLStackElem;
class Token;

/**
 * The parser for html. It receives a stream of tokens from the HTMLTokenizer, and
 * builds up the Document structure form it.
 */
class HTMLParser
{
public:
    HTMLParser(FrameView *w, DocumentImpl *i, bool includesComments=false);
    HTMLParser(DocumentFragmentImpl *frag, DocumentImpl *doc, bool includesComments=false);
    virtual ~HTMLParser();

    /**
     * parses one token delivered by the tokenizer
     */
    PassRefPtr<NodeImpl> parseToken(Token*);
    
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

    HTMLDocumentImpl *doc() const { return static_cast<HTMLDocumentImpl *>(document); }

protected:
    void setCurrent(NodeImpl* newCurrent);
    void setSkipMode(const QualifiedName& qName) { discard_until = qName.localName(); }

    FrameView *HTMLWidget;
    DocumentImpl *document;

    /*
     * generate a node from the token
     */
    PassRefPtr<NodeImpl> getNode(Token*);
    bool textCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool commentCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool headCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool bodyCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool framesetCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool iframeCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool formCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool isindexCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool selectCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool ddCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool dtCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool nestedCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool nestedStyleCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool tableCellCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool tableSectionCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool noembedCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool noscriptCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool noframesCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool nolayerCreateErrorCheck(Token*, RefPtr<NodeImpl>&);
    bool mapCreateErrorCheck(Token*, RefPtr<NodeImpl>&);

    void processCloseTag(Token *);

    bool insertNode(NodeImpl *n, bool flat = false);
    bool handleError(NodeImpl* n, bool flat, const AtomicString& localName, int tagPriority);
    
    // The currently active element (the one new elements will be added to).  Can be a DocumentFragment, a Document or an Element.
    NodeImpl* current;

    bool currentIsReferenced;

    HTMLStackElem *blockStack;

    void pushBlock(const AtomicString& tagName, int _level);
    void popBlock(const AtomicString& tagName);
    void popBlock(const QualifiedName& qName) { return popBlock(qName.localName()); } // Convenience function for readability.
    void popOneBlock(bool delBlock = true);
    void popInlineBlocks();

    void freeBlock();

    void createHead();

    bool isResidualStyleTag(const AtomicString& tagName);
    bool isAffectedByResidualStyle(const AtomicString& tagName);
    void handleResidualStyleCloseTagAcrossBlocks(HTMLStackElem* elem);
    void reopenResidualStyleTags(HTMLStackElem* elem, NodeImpl* malformedTableParent);

    bool allowNestedRedundantTag(const AtomicString& tagName);
    
    static bool isHeaderTag(const AtomicString& tagName);
    void popNestedHeaderTag();

    bool isInline(NodeImpl* node) const;
    
    /*
     * currently active form
     */
    HTMLFormElementImpl *form;

    /*
     * current map
     */
    HTMLMapElementImpl *map;

    /*
     * the head element. Needed for crappy html which defines <base> after </head>
     */
    HTMLHeadElementImpl *head;

    /*
     * a possible <isindex> element in the head. Compatibility hack for
     * html from the stone age
     */
    RefPtr<NodeImpl> isindex;
    NodeImpl* handleIsindex(Token*);

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
    AtomicString discard_until;

    bool headLoaded;
    int inStrayTableContent;

    bool includesCommentsInDOM;
};

}
    
#endif // HTMLPARSER_H
