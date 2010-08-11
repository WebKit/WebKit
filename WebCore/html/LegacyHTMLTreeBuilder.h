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

#ifndef LegacyHTMLTreeBuilder_h
#define LegacyHTMLTreeBuilder_h

#include "FragmentScriptingPermission.h"
#include "HTMLParserErrorCodes.h"
#include "NamedNodeMap.h"
#include "QualifiedName.h"
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

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

/**
 * @internal
 * represents one HTML tag. Consists of a numerical id, and the list
 * of attributes. Can also represent text. In this case the id = 0 and
 * text contains the text.
 */
struct Token {
    Token()
        : beginTag(true)
        , selfClosingTag(false)
        , brokenXMLStyle(false)
        , m_sourceInfo(0)
    { }
    ~Token() { }

    void addAttribute(AtomicString& attrName, const AtomicString& v, bool viewSourceMode);

    bool isOpenTag(const QualifiedName& fullName) const { return beginTag && fullName.localName() == tagName; }
    bool isCloseTag(const QualifiedName& fullName) const { return !beginTag && fullName.localName() == tagName; }

    void reset()
    {
        attrs = 0;
        text = 0;
        tagName = nullAtom;
        beginTag = true;
        selfClosingTag = false;
        brokenXMLStyle = false;
        if (m_sourceInfo)
            m_sourceInfo->clear();
    }

    void addViewSourceChar(UChar c)
    {
        if (!m_sourceInfo.get())
            m_sourceInfo.set(new Vector<UChar>);
        m_sourceInfo->append(c);
    }

    RefPtr<NamedNodeMap> attrs;
    RefPtr<StringImpl> text;
    AtomicString tagName;
    bool beginTag;
    bool selfClosingTag;
    bool brokenXMLStyle;
    OwnPtr<Vector<UChar> > m_sourceInfo;
};

enum DoctypeState {
    DoctypeBegin,
    DoctypeBeforeName,
    DoctypeName,
    DoctypeAfterName,
    DoctypeBeforePublicID,
    DoctypePublicID,
    DoctypeAfterPublicID,
    DoctypeBeforeSystemID,
    DoctypeSystemID,
    DoctypeAfterSystemID,
    DoctypeBogus
};

class DoctypeToken {
public:
    DoctypeToken() {}

    void reset()
    {
        m_name.clear();
        m_publicID.clear();
        m_systemID.clear();
        m_state = DoctypeBegin;
        m_source.clear();
        m_forceQuirks = false;
    }

    DoctypeState state() { return m_state; }
    void setState(DoctypeState s) { m_state = s; }

    Vector<UChar> m_name;
    Vector<UChar> m_publicID;
    Vector<UChar> m_systemID;
    DoctypeState m_state;

    Vector<UChar> m_source;

    bool m_forceQuirks; // Used by the HTML5 parser.
};

//-----------------------------------------------------------------------------

/**
 * The parser for HTML. It receives a stream of tokens from the LegacyHTMLDocumentParser, and
 * builds up the Document structure from it.
 */
class LegacyHTMLTreeBuilder : public Noncopyable {
public:
    LegacyHTMLTreeBuilder(HTMLDocument*, bool reportErrors);
    LegacyHTMLTreeBuilder(DocumentFragment*, FragmentScriptingPermission = FragmentScriptingAllowed);
    virtual ~LegacyHTMLTreeBuilder();

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
    bool colCreateErrorCheck(Token*, RefPtr<Node>&);
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
    bool rpCreateErrorCheck(Token*, RefPtr<Node>&);
    bool rtCreateErrorCheck(Token*, RefPtr<Node>&);
    bool selectCreateErrorCheck(Token*, RefPtr<Node>&);
    bool tableCellCreateErrorCheck(Token*, RefPtr<Node>&);
    bool tableSectionCreateErrorCheck(Token*, RefPtr<Node>&);
    bool textCreateErrorCheck(Token*, RefPtr<Node>&);

    void processCloseTag(Token*);

    void limitDepth(int tagPriority);
    bool insertNodeAfterLimitDepth(Node*, bool flat = false);
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
    
    static bool isHeadingTag(const AtomicString& tagName);

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
    // Depth of the tree.
    unsigned m_treeDepth;

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
    FragmentScriptingPermission m_scriptingPermission;

    OwnPtr<HTMLParserQuirks> m_parserQuirks;
};

#if defined(BUILDING_ON_LEOPARD) || defined(BUILDING_ON_TIGER)
bool shouldCreateImplicitHead(Document*);
#else
inline bool shouldCreateImplicitHead(Document*) { return true; }
#endif

// Converts the specified string to a floating number.
// If the conversion fails, the return value is false. Take care that leading or trailing unnecessary characters make failures.  This returns false for an empty string input.
// The double* parameter may be 0.
bool parseToDoubleForNumberType(const String&, double*);
// Converts the specified number to a string. This is an implementation of
// HTML5's "algorithm to convert a number to a string" for NUMBER/RANGE types.
String serializeForNumberType(double);

}
    
#endif // LegacyHTMLTreeBuilder_h
