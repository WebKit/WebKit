/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef DOM_NodeImpl_h_
#define DOM_NodeImpl_h_

#include "DocPtr.h"
#include "PlatformString.h"
#include <kxmlcore/Assertions.h>
#include <kxmlcore/HashSet.h>
#include <kxmlcore/PassRefPtr.h>

class DeprecatedStringList;
class QTextStream;
class RenderArena;

namespace WebCore {

class AtomicString;
class ContainerNode;
class Document;
class Element;
class Event;
class EventListener;
class IntRect;
class NamedAttrMap;
class NodeList;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class PlatformWheelEvent;
class QualifiedName;
class RegisteredEventListener;
class RenderObject;
class RenderStyle;

typedef int ExceptionCode;

// this class implements nodes, which can have a parent but no children:
class Node : public TreeShared<Node> {
    friend class Document;
public:
    enum NodeType {
        ELEMENT_NODE = 1,
        ATTRIBUTE_NODE = 2,
        TEXT_NODE = 3,
        CDATA_SECTION_NODE = 4,
        ENTITY_REFERENCE_NODE = 5,
        ENTITY_NODE = 6,
        PROCESSING_INSTRUCTION_NODE = 7,
        COMMENT_NODE = 8,
        DOCUMENT_NODE = 9,
        DOCUMENT_TYPE_NODE = 10,
        DOCUMENT_FRAGMENT_NODE = 11,
        NOTATION_NODE = 12,
        XPATH_NAMESPACE_NODE = 13
    };

    Node(Document*);
    virtual ~Node();

    // DOM methods & attributes for Node

    virtual bool hasTagName(const QualifiedName&) const { return false; }
    virtual String nodeName() const = 0;
    virtual String nodeValue() const;
    virtual void setNodeValue(const String&, ExceptionCode&);
    virtual NodeType nodeType() const = 0;
    Node* parentNode() const { return parent(); }
    Node* previousSibling() const { return m_previous; }
    Node* nextSibling() const { return m_next; }
    virtual PassRefPtr<NodeList> childNodes();
    virtual Node* firstChild() const;
    virtual Node* lastChild() const;
    virtual bool hasAttributes() const;
    virtual NamedAttrMap* attributes() const;

    // These should all actually return a node, but this is only important for language bindings,
    // which will already know and hold a ref on the right node to return. Returning bool allows
    // these methods to be more efficient since they don't need to return a ref
    virtual bool insertBefore(PassRefPtr<Node> newChild, Node* refChild, ExceptionCode&);
    virtual bool replaceChild(PassRefPtr<Node> newChild, Node* oldChild, ExceptionCode&);
    virtual bool removeChild(Node* child, ExceptionCode&);
    virtual bool appendChild(PassRefPtr<Node> newChild, ExceptionCode&);

    virtual void remove(ExceptionCode&);
    virtual bool hasChildNodes() const;
    virtual PassRefPtr<Node> cloneNode(bool deep) = 0;
    virtual const AtomicString& localName() const;
    virtual const AtomicString& namespaceURI() const;
    virtual const AtomicString& prefix() const;
    virtual void setPrefix(const AtomicString&, ExceptionCode&);
    void normalize();
    static bool isSupported(const String& feature, const String& version);

    bool isSameNode(Node* other) const { return this == other; }
    bool isEqualNode(Node*) const;
    bool isDefaultNamespace(const String& namespaceURI) const;
    String lookupPrefix(const String& namespaceURI) const;
    String lookupNamespaceURI(const String& prefix) const;
    String lookupNamespacePrefix(const String& namespaceURI, const Element* originalElement) const;
    
    String textContent() const;
    void setTextContent(const String&, ExceptionCode&);
    
    Node* lastDescendant() const;

    // Other methods (not part of DOM)

    virtual bool isElementNode() const { return false; }
    virtual bool isHTMLElement() const { return false; }
#if SVG_SUPPORT
    virtual bool isSVGElement() const { return false; }
#endif
    virtual bool isStyledElement() const { return false; }
    virtual bool isAttributeNode() const { return false; }
    virtual bool isTextNode() const { return false; }
    virtual bool isCommentNode() const { return false; }
    virtual bool isDocumentNode() const { return false; }
    virtual bool isEventTargetNode() const { return false; }
    virtual bool isShadowNode() const { return false; }
    virtual Node* shadowParentNode() { return 0; }
    Node* shadowAncestorNode();
    bool isBlockFlow() const;
    bool isBlockFlowOrBlockTable() const;
    
    // Used by <form> elements to indicate a malformed state of some kind, typically
    // used to keep from applying the bottom margin of the form.
    virtual bool isMalformed() { return false; }
    virtual void setMalformed(bool malformed) { }
    
    // These low-level calls give the caller responsibility for maintaining the integrity of the tree.
    void setPreviousSibling(Node* previous) { m_previous = previous; }
    void setNextSibling(Node* next) { m_next = next; }

    // FIXME: These two functions belong in editing -- "atomic node" is an editing concept.
    Node* previousNodeConsideringAtomicNodes() const;
    Node* nextNodeConsideringAtomicNodes() const;
    
    /** (Not part of the official DOM)
     * Returns the next leaf node.
     *
     * Using this function delivers leaf nodes as if the whole DOM tree were a linear chain of its leaf nodes.
     * @return next leaf node or 0 if there are no more.
     */
    Node* nextLeafNode() const;

    /** (Not part of the official DOM)
     * Returns the previous leaf node.
     *
     * Using this function delivers leaf nodes as if the whole DOM tree were a linear chain of its leaf nodes.
     * @return previous leaf node or 0 if there are no more.
     */
    Node* previousLeafNode() const;

    bool isEditableBlock() const;
    Element* enclosingBlockFlowElement() const;
    Element* enclosingBlockFlowOrTableElement() const;
    Element* enclosingInlineElement() const;
    Element* rootEditableElement() const;
    
    bool inSameRootEditableElement(Node*);
    bool inSameContainingBlockFlowElement(Node*);
    
    // Used by the parser. Checks against the DTD, unlike DOM operations like appendChild().
    // Also does not dispatch DOM mutation events.
    // Returns the appropriate container node for future insertions as you parse, or 0 for failure.
    virtual ContainerNode* addChild(PassRefPtr<Node>);

    // Called by the parser when this element's close tag is reached,
    // signalling that all child tags have been parsed and added.
    // This is needed for <applet> and <object> elements, which can't lay themselves out
    // until they know all of their nested <param>s. [Radar 3603191, 4040848].
    // Also used for script elements and some SVG elements for similar purposes,
    // but making parsing a special case in this respect should be avoided if possible.
    virtual void closeRenderer() { }

    // For <link> and <style> elements.
    virtual void sheetLoaded() { }

    bool hasID() const { return m_hasId; }
    bool hasClass() const { return m_hasClass; }
    bool hasStyle() const { return m_hasStyle; }
    bool active() const { return m_active; }
    bool inActiveChain() const { return m_inActiveChain; }
    bool hovered() const { return m_hovered; }
    bool focused() const { return m_focused; }
    bool attached() const { return m_attached; }
    void setAttached(bool b = true) { m_attached = b; }
    bool changed() const { return m_changed; }
    bool hasChangedChild() const { return m_hasChangedChild; }
    bool isLink() const { return m_isLink; }
    bool styleElement() const { return m_styleElement; }
    bool implicitNode() const { return m_implicit; }
    void setHasID(bool b = true) { m_hasId = b; }
    void setHasClass(bool b = true) { m_hasClass = b; }
    void setHasStyle(bool b = true) { m_hasStyle = b; }
    void setHasChangedChild( bool b = true ) { m_hasChangedChild = b; }
    void setInDocument(bool b = true) { m_inDocument = b; }
    void setInActiveChain(bool b = true) { m_inActiveChain = b; }
    void setChanged(bool b = true);

    virtual void setFocus(bool b = true) { m_focused = b; }
    virtual void setActive(bool b = true, bool pause=false) { m_active = b; }
    virtual void setHovered(bool b = true) { m_hovered = b; }

    unsigned short tabIndex() const { return m_tabIndex; }
    void setTabIndex(unsigned short i) { m_tabIndex = i; }

    /**
     * Whether this node can receive the keyboard focus.
     */
    virtual bool isFocusable() const;
    virtual bool isKeyboardFocusable() const;
    virtual bool isMouseFocusable() const;

    virtual bool isControl() const { return false; } // Eventually the notion of what is a control will be extensible.
    virtual bool isEnabled() const { return true; }
    virtual bool isChecked() const { return false; }
    virtual bool isIndeterminate() const { return false; }
    virtual bool isReadOnlyControl() const { return false; }

    virtual bool isContentEditable() const;
    virtual bool isContentRichlyEditable() const;
    virtual IntRect getRect() const;

    enum StyleChange { NoChange, NoInherit, Inherit, Detach, Force };
    virtual void recalcStyle(StyleChange = NoChange) { }
    StyleChange diff(RenderStyle*, RenderStyle*) const;

    unsigned nodeIndex() const;

    // Returns the DOM ownerDocument attribute. This method never returns NULL, except in the case 
    // of (1) a Document node or (2) a DocumentType node that is not used with any Document yet. 
    virtual Document* ownerDocument() const;

    // Returns the document associated with this node. This method never returns NULL, except in the case 
    // of a DocumentType node that is not used with any Document yet. A Document node returns itself.
    Document* document() const 
    { 
      ASSERT(m_document || nodeType() == DOCUMENT_TYPE_NODE && !inDocument());
      return m_document.get(); 
    }
    void setDocument(Document*);

    // Returns true if this node is associated with a document and is in its associated document's
    // node tree, false otherwise.
    bool inDocument() const 
    { 
      ASSERT(m_document || !m_inDocument);
      return m_inDocument; 
    }
    
    virtual bool isReadOnlyNode();
    virtual bool childTypeAllowed(NodeType) { return false; }
    virtual unsigned childNodeCount() const;
    virtual Node* childNode(unsigned index) const;

    /**
     * Does a pre-order traversal of the tree to find the node next node after this one. This uses the same order that
     * the tags appear in the source file.
     *
     * @param stayWithin If not null, the traversal will stop once the specified node is reached. This can be used to
     * restrict traversal to a particular sub-tree.
     *
     * @return The next node, in document order
     *
     * see @ref traversePreviousNode()
     */
    Node* traverseNextNode(const Node* stayWithin = 0) const;
    
    /* Like traverseNextNode, but skips children and starts with the next sibling. */
    Node* traverseNextSibling(const Node* stayWithin = 0) const;

    /**
     * Does a reverse pre-order traversal to find the node that comes before the current one in document order
     *
     * see @ref traverseNextNode()
     */
    Node* traversePreviousNode() const;

    /* Like traversePreviousNode, but visits nodes before their children. */
    Node* traversePreviousNodePostOrder(const Node *stayWithin = 0) const;

    /**
     * Finds previous or next editable leaf node.
     */
    Node* previousEditable() const;
    Node* nextEditable() const;
    Node* nextEditable(int offset) const;

    RenderObject* renderer() const { return m_renderer; }
    RenderObject* nextRenderer();
    RenderObject* previousRenderer();
    void setRenderer(RenderObject* renderer) { m_renderer = renderer; }
    
    void checkSetPrefix(const AtomicString& prefix, ExceptionCode&);
    bool isAncestor(const Node*) const;

    // These two methods are mutually exclusive.  The former is used to do strict error-checking
    // when adding children via the public DOM API (e.g., appendChild()).  The latter is called only when parsing, 
    // to sanity-check against the DTD for error recovery.
    void checkAddChild(Node* newChild, ExceptionCode&); // Error-checking when adding via the DOM API
    virtual bool childAllowed(Node* newChild);          // Error-checking during parsing that checks the DTD

    // Used to determine whether range offsets use characters or node indices.
    virtual bool offsetInCharacters() const;

    // FIXME: These next 7 functions are mostly editing-specific and should be moved out.
    virtual int maxOffset() const;
    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;
    virtual int previousOffset(int current) const;
    virtual int nextOffset(int current) const;
    
#ifndef NDEBUG
    virtual void dump(QTextStream*, DeprecatedString indent = "") const;
#endif

    // -----------------------------------------------------------------------------
    // Integration with rendering tree

    /**
     * Attaches this node to the rendering tree. This calculates the style to be applied to the node and creates an
     * appropriate RenderObject which will be inserted into the tree (except when the style has display: none). This
     * makes the node visible in the FrameView.
     */
    virtual void attach();

    /**
     * Detaches the node from the rendering tree, making it invisible in the rendered view. This method will remove
     * the node's rendering object from the rendering tree and delete it.
     */
    virtual void detach();

    virtual void willRemove();
    void createRendererIfNeeded();
    virtual RenderStyle* createStyleForRenderer(RenderObject* parent);
    virtual bool rendererIsNeeded(RenderStyle*);
#if SVG_SUPPORT
    virtual bool childShouldCreateRenderer(Node*) const { return true; }
#endif
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

    // -----------------------------------------------------------------------------
    // Notification of document structure changes

    /**
     * Notifies the node that it has been inserted into the document. This is called during document parsing, and also
     * when a node is added through the DOM methods insertBefore(), appendChild() or replaceChild(). Note that this only
     * happens when the node becomes part of the document tree, i.e. only when the document is actually an ancestor of
     * the node. The call happens _after_ the node has been added to the tree.
     *
     * This is similar to the DOMNodeInsertedIntoDocument DOM event, but does not require the overhead of event
     * dispatching.
     */
    virtual void insertedIntoDocument();

    /**
     * Notifies the node that it is no longer part of the document tree, i.e. when the document is no longer an ancestor
     * node.
     *
     * This is similar to the DOMNodeRemovedFromDocument DOM event, but does not require the overhead of event
     * dispatching, and is called _after_ the node is removed from the tree.
     */
    virtual void removedFromDocument();

    // These functions are called whenever you are connected or disconnected from a tree.  That tree may be the main
    // document tree, or it could be another disconnected tree.  Override these functions to do any work that depends
    // on connectedness to some ancestor (e.g., an ancestor <form> for example).
    virtual void insertedIntoTree(bool deep) { }
    virtual void removedFromTree(bool deep) { }

    /**
     * Notifies the node that it's list of children have changed (either by adding or removing child nodes), or a child
     * node that is of the type CDATA_SECTION_NODE, TEXT_NODE or COMMENT_NODE has changed its value.
     */
    virtual void childrenChanged();

    virtual String toString() const = 0;

#ifndef NDEBUG
    virtual void formatForDebugger(char* buffer, unsigned length) const;

    void showNode(const char* prefix = "") const;
    void showTreeForThis() const;
    void showTreeAndMark(const Node* markedNode1, const char* markedLabel1, const Node* markedNode2 = 0, const char* markedLabel2 = 0) const;
#endif

    void registerNodeList(NodeList*);
    void unregisterNodeList(NodeList*);
    void notifyNodeListsChildrenChanged();
    void notifyLocalNodeListsChildrenChanged();
    void notifyNodeListsAttributeChanged();
    void notifyLocalNodeListsAttributeChanged();
    
    PassRefPtr<NodeList> getElementsByTagName(const String&);
    PassRefPtr<NodeList> getElementsByTagNameNS(const String& namespaceURI, const String& localName);

private: // members
    DocPtr<Document> m_document;
    Node* m_previous;
    Node* m_next;
    RenderObject* m_renderer;

protected:
    typedef HashSet<NodeList*> NodeListSet;
    NodeListSet* m_nodeLists;

    unsigned short m_tabIndex : 15;
    bool m_hasTabIndex : 1;

    bool m_hasId : 1;
    bool m_hasClass : 1;
    bool m_hasStyle : 1;
    bool m_attached : 1;
    bool m_changed : 1;
    bool m_hasChangedChild : 1;
    bool m_inDocument : 1;

    bool m_isLink : 1;
    bool m_specified : 1; // used in Attr; accessor functions there
    bool m_focused : 1;
    bool m_active : 1;
    bool m_hovered : 1;
    bool m_inActiveChain : 1;
    bool m_styleElement : 1; // contains stylesheet text
    bool m_implicit : 1; // implicitly generated by the parser

    bool m_inDetach : 1;

private:
    Element* ancestorElement() const;
};

} //namespace

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::Node*);
#endif

#endif
