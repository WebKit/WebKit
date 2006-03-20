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

#include "HTMLDocument.h"

namespace WebCore {

class DocumentFragment;
class FrameView;
class HTMLElement;
class HTMLFormElement;
class HTMLHeadElement;
class HTMLMapElement;
class HTMLStackElem;
class Token;

/**
 * The parser for html. It receives a stream of tokens from the HTMLTokenizer, and
 * builds up the Document structure form it.
 */
class HTMLParser
{
public:
    HTMLParser(Document*);
    HTMLParser(DocumentFragment*);
    virtual ~HTMLParser();

    /**
     * parses one token delivered by the tokenizer
     */
    PassRefPtr<Node> parseToken(Token*);
    
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

    HTMLDocument *doc() const { return static_cast<HTMLDocument *>(document); }

private:
    void setCurrent(Node* newCurrent);
    void setSkipMode(const QualifiedName& qName) { discard_until = qName.localName(); }

    Document* document;

    /*
     * generate a node from the token
     */
    PassRefPtr<Node> getNode(Token*);
    bool textCreateErrorCheck(Token*, RefPtr<Node>&);
    bool commentCreateErrorCheck(Token*, RefPtr<Node>&);
    bool headCreateErrorCheck(Token*, RefPtr<Node>&);
    bool bodyCreateErrorCheck(Token*, RefPtr<Node>&);
    bool framesetCreateErrorCheck(Token*, RefPtr<Node>&);
    bool iframeCreateErrorCheck(Token*, RefPtr<Node>&);
    bool formCreateErrorCheck(Token*, RefPtr<Node>&);
    bool isindexCreateErrorCheck(Token*, RefPtr<Node>&);
    bool selectCreateErrorCheck(Token*, RefPtr<Node>&);
    bool ddCreateErrorCheck(Token*, RefPtr<Node>&);
    bool dtCreateErrorCheck(Token*, RefPtr<Node>&);
    bool nestedCreateErrorCheck(Token*, RefPtr<Node>&);
    bool nestedStyleCreateErrorCheck(Token*, RefPtr<Node>&);
    bool tableCellCreateErrorCheck(Token*, RefPtr<Node>&);
    bool tableSectionCreateErrorCheck(Token*, RefPtr<Node>&);
    bool noembedCreateErrorCheck(Token*, RefPtr<Node>&);
    bool noscriptCreateErrorCheck(Token*, RefPtr<Node>&);
    bool noframesCreateErrorCheck(Token*, RefPtr<Node>&);
    bool nolayerCreateErrorCheck(Token*, RefPtr<Node>&);
    bool mapCreateErrorCheck(Token*, RefPtr<Node>&);

    void processCloseTag(Token *);

    bool insertNode(Node *n, bool flat = false);
    bool handleError(Node* n, bool flat, const AtomicString& localName, int tagPriority);
    
    // The currently active element (the one new elements will be added to).  Can be a DocumentFragment, a Document or an Element.
    Node* current;

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
    void reopenResidualStyleTags(HTMLStackElem* elem, Node* malformedTableParent);

    bool allowNestedRedundantTag(const AtomicString& tagName);
    
    static bool isHeaderTag(const AtomicString& tagName);
    void popNestedHeaderTag();

    bool isInline(Node* node) const;
    
    /*
     * currently active form
     */
    HTMLFormElement *form;

    /*
     * current map
     */
    HTMLMapElement *map;

    /*
     * the head element. Needed for crappy html which defines <base> after </head>
     */
    HTMLHeadElement *head;

    /*
     * a possible <isindex> element in the head. Compatibility hack for
     * html from the stone age
     */
    RefPtr<Node> isindex;
    Node* handleIsindex(Token*);

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
    bool m_fragment;
    int inStrayTableContent;
};

}
    
#endif // HTMLPARSER_H
