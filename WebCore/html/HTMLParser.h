/*
    Copyright (C) 1997 Martin Jones (mjones@kde.org)
              (C) 1997 Torben Weis (weis@kde.org)
              (C) 1998 Waldo Bastian (bastian@kde.org)
              (C) 1999 Lars Knoll (knoll@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.

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

#ifndef HTMLParser_h
#define HTMLParser_h

#include "QualifiedName.h"
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>
#include "HTMLParserErrorCodes.h"

namespace WebCore {

class DoctypeToken;
class Document;
class DocumentFragment;
class HTMLDocument;
class HTMLFormElement;
class HTMLHeadElement;
class HTMLMapElement;
class HTMLParserQuirks;
class Node;

struct HTMLStackElem;
struct Token;

/**
 * The parser for HTML. It receives a stream of tokens from the HTMLTokenizer, and
 * builds up the Document structure from it.
 */
class HTMLParser : Noncopyable {
public:
    HTMLParser(HTMLDocument*, bool reportErrors);
    HTMLParser(DocumentFragment*);
    virtual ~HTMLParser();

    /**
     * parses one token delivered by the tokenizer
     */
    PassRefPtr<Node> parseToken(Token*);
    
    // Parses a doctype token.
    void parseDoctypeToken(DoctypeToken*);

    /**
     * tokenizer says it's not going to be sending us any more tokens
     */
    void finished();

    /**
     * resets the parser
     */
    void reset();

    bool skipMode() const { return !m_skipModeTag.isNull(); }
    bool isHandlingResidualStyleAcrossBlocks() const { return m_handlingResidualStyleAcrossBlocks; }

private:
    void setCurrent(Node*);
    void derefCurrent();
    void setSkipMode(const QualifiedName& qName) { m_skipModeTag = qName.localName(); }

    PassRefPtr<Node> getNode(Token*);
    bool bodyCreateErrorCheck(Token*, RefPtr<Node>&);
    bool canvasCreateErrorCheck(Token*, RefPtr<Node>&);
    bool commentCreateErrorCheck(Token*, RefPtr<Node>&);
    bool ddCreateErrorCheck(Token*, RefPtr<Node>&);
    bool dtCreateErrorCheck(Token*, RefPtr<Node>&);
    bool formCreateErrorCheck(Token*, RefPtr<Node>&);
    bool framesetCreateErrorCheck(Token*, RefPtr<Node>&);
    bool headCreateErrorCheck(Token*, RefPtr<Node>&);
    bool iframeCreateErrorCheck(Token*, RefPtr<Node>&);
    bool isindexCreateErrorCheck(Token*, RefPtr<Node>&);
    bool mapCreateErrorCheck(Token*, RefPtr<Node>&);
    bool nestedCreateErrorCheck(Token*, RefPtr<Node>&);
    bool nestedPCloserCreateErrorCheck(Token*, RefPtr<Node>&);
    bool nestedStyleCreateErrorCheck(Token*, RefPtr<Node>&);
    bool noembedCreateErrorCheck(Token*, RefPtr<Node>&);
    bool noframesCreateErrorCheck(Token*, RefPtr<Node>&);
    bool nolayerCreateErrorCheck(Token*, RefPtr<Node>&);
    bool noscriptCreateErrorCheck(Token*, RefPtr<Node>&);
    bool pCloserCreateErrorCheck(Token*, RefPtr<Node>&);
    bool pCloserStrictCreateErrorCheck(Token*, RefPtr<Node>&);
    bool selectCreateErrorCheck(Token*, RefPtr<Node>&);
    bool tableCellCreateErrorCheck(Token*, RefPtr<Node>&);
    bool tableSectionCreateErrorCheck(Token*, RefPtr<Node>&);
    bool textCreateErrorCheck(Token*, RefPtr<Node>&);

    void processCloseTag(Token*);

    bool insertNode(Node*, bool flat = false);
    bool handleError(Node*, bool flat, const AtomicString& localName, int tagPriority);
    
    void pushBlock(const AtomicString& tagName, int level);
    void popBlock(const AtomicString& tagName, bool reportErrors = false);
    void popBlock(const QualifiedName& qName, bool reportErrors = false) { return popBlock(qName.localName(), reportErrors); } // Convenience function for readability.
    void popOneBlock();
    void moveOneBlockToStack(HTMLStackElem*& head);
    inline HTMLStackElem* popOneBlockCommon();
    void popInlineBlocks();

    void freeBlock();

    void createHead();

    static bool isResidualStyleTag(const AtomicString& tagName);
    static bool isAffectedByResidualStyle(const AtomicString& tagName);
    void handleResidualStyleCloseTagAcrossBlocks(HTMLStackElem*);
    void reopenResidualStyleTags(HTMLStackElem*, Node* malformedTableParent);

    bool allowNestedRedundantTag(const AtomicString& tagName);
    
    static bool isHeaderTag(const AtomicString& tagName);
    void popNestedHeaderTag();

    bool isInline(Node*) const;
    
    void startBody(); // inserts the isindex element
    PassRefPtr<Node> handleIsindex(Token*);

    void checkIfHasPElementInScope();
    bool hasPElementInScope()
    {
        if (m_hasPElementInScope == Unknown)
            checkIfHasPElementInScope();
        return m_hasPElementInScope == InScope;
    }

    void reportError(HTMLParserErrorCode errorCode, const AtomicString* tagName1 = 0, const AtomicString* tagName2 = 0, bool closeTags = false)
    { if (!m_reportErrors) return; reportErrorToConsole(errorCode, tagName1, tagName2, closeTags); }

    void reportErrorToConsole(HTMLParserErrorCode, const AtomicString* tagName1, const AtomicString* tagName2, bool closeTags);
    
    Document* m_document;

    // The currently active element (the one new elements will be added to). Can be a document fragment, a document or an element.
    Node* m_current;
    // We can't ref a document, but we don't want to constantly check if a node is a document just to decide whether to deref.
    bool m_didRefCurrent;

    HTMLStackElem* m_blockStack;

    // The number of tags with priority minBlockLevelTagPriority or higher
    // currently in m_blockStack. The parser enforces a cap on this value by
    // adding such new elements as siblings instead of children once it is reached.
    size_t m_blocksInStack;

    enum ElementInScopeState { NotInScope, InScope, Unknown }; 
    ElementInScopeState m_hasPElementInScope;

    RefPtr<HTMLFormElement> m_currentFormElement; // currently active form
    RefPtr<HTMLMapElement> m_currentMapElement; // current map
    RefPtr<HTMLHeadElement> m_head; // head element; needed for HTML which defines <base> after </head>
    RefPtr<Node> m_isindexElement; // a possible <isindex> element in the head

    bool m_inBody;
    bool m_haveContent;
    bool m_haveFrameSet;

    AtomicString m_skipModeTag; // tells the parser to discard all tags until it reaches the one specified

    bool m_isParsingFragment;
    bool m_reportErrors;
    bool m_handlingResidualStyleAcrossBlocks;
    int m_inStrayTableContent;

    OwnPtr<HTMLParserQuirks> m_parserQuirks;
};

#if defined(BUILDING_ON_LEOPARD) || defined(BUILDING_ON_TIGER)
bool shouldCreateImplicitHead(Document*);
#else
inline bool shouldCreateImplicitHead(Document*) { return true; }
#endif

}
    
#endif // HTMLParser_h
