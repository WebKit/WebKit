/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
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
 * $Id$
 */
#ifndef _DOM_DocumentImpl_h_
#define _DOM_DocumentImpl_h_

#include "dom_nodeimpl.h"
#include "dom2_traversalimpl.h"

#include <qlist.h>
#include <qstringlist.h>
#include <qobject.h>
#include <qdict.h>

class QPaintDevice;
class QPaintDeviceMetrics;
class KHTMLView;
class Tokenizer;

namespace khtml {
    class CSSStyleSelector;
    class DocLoader;
    class CSSStyleSelectorList;
}

namespace DOM {

    class ElementImpl;
    class DocumentFragmentImpl;
    class TextImpl;
    class CDATASectionImpl;
    class CommentImpl;
    class AttrImpl;
    class EntityReferenceImpl;
    class NodeListImpl;
    class StyleSheetListImpl;
    class RangeImpl;
    class NodeIteratorImpl;
    class TreeWalkerImpl;
    class NodeFilterImpl;
    class DocumentTypeImpl;
    class GenericRONamedNodeMapImpl;
    class ProcessingInstructionImpl;
    class HTMLElementImpl;
    class StyleSheetImpl;
    class CSSStyleSheetImpl;
    class AbstractViewImpl;
    class EventImpl;
    class EventListener;
    class RegisteredEventListener;

class DOMImplementationImpl : public DomShared
{
public:
    DOMImplementationImpl();
    ~DOMImplementationImpl();

    bool hasFeature ( const DOMString &feature, const DOMString &version );
    CSSStyleSheetImpl *createCSSStyleSheet(DOMStringImpl *title, DOMStringImpl *media);
};


/**
 * @internal
 */
class DocumentImpl : public QObject, public NodeBaseImpl
{
    Q_OBJECT
public:
    DocumentImpl(KHTMLView *v=0);
    ~DocumentImpl();

    virtual const DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual bool isDocumentNode() const { return true; }

    virtual bool isHTMLDocument() const { return false; }

    virtual void applyChanges(bool top=true, bool force=true);

    DocumentTypeImpl *doctype() const;

    DOMImplementationImpl *implementation() const;

    ElementImpl *documentElement() const;

    virtual ElementImpl *createElement ( const DOMString &tagName );
    virtual ElementImpl *createElementNS ( const DOMString &_namespaceURI, const DOMString &_qualifiedName );
    virtual ElementImpl *createHTMLElement ( const DOMString &tagName );

    DocumentFragmentImpl *createDocumentFragment ();

    TextImpl *createTextNode ( const DOMString &data );

    CommentImpl *createComment ( const DOMString &data );

    CDATASectionImpl *createCDATASection ( const DOMString &data );

    ProcessingInstructionImpl *createProcessingInstruction ( const DOMString &target, const DOMString &data );

    AttrImpl *createAttribute ( const DOMString &name );
    AttrImpl *createAttributeNS ( const DOMString &_namespaceURI, const DOMString &_qualifiedName );

    EntityReferenceImpl *createEntityReference ( const DOMString &name );

    NodeListImpl *getElementsByTagName ( const DOMString &tagname );

    khtml::CSSStyleSelector *styleSelector() { return m_styleSelector; }
    void createSelector();

    // Used to maintain list of all elements in the document
    // that want to save and restore state.
    // Returns the state the element should restored to.
    QString registerElement(ElementImpl *);

    // Used to maintain list of all forms in document
    void removeElement(ElementImpl *);

    // Query all registered elements for their state
    QStringList state();

    // Set the state the document should restore to
    void setRestoreState( const QStringList &s) { m_state = s; }

    KHTMLView *view() const { return m_view; }

    RangeImpl *createRange();

    NodeIteratorImpl *createNodeIterator(NodeImpl *root, unsigned long whatToShow,
                                    NodeFilter filter, bool entityReferenceExpansion, int &exceptioncode);

    TreeWalkerImpl *createTreeWalker(Node root, unsigned long whatToShow, NodeFilter filter,
                            bool entityReferenceExpansion);

    QList<NodeImpl> changedNodes;
    virtual void setChanged(bool b=true);
    virtual void recalcStyle();
    virtual void updateRendering();
    khtml::DocLoader *docLoader() { return m_docLoader; }
    
    void attach(KHTMLView *w=0);
    virtual void detach();

    // to get visually ordered hebrew and arabic pages right
    void setVisuallyOrdered();

    void setSelection(NodeImpl* s, int sp, NodeImpl* e, int ep);
    void clearSelection();

    void open (  );
    void close (  );
    void write ( const DOMString &text );
    void write ( const QString &text );
    void writeln ( const DOMString &text );
    void finishParsing (  );
    void clear();
    // moved from HTMLDocument in DOM2
    ElementImpl *getElementById ( const DOMString &elementId );
    DOMString URL() const { return url; }
    void setURL(DOMString _url) { url = _url; }

    DOMString baseURL() const;

    // from cachedObjectClient
    virtual void setStyleSheet(const DOM::DOMString &url, const DOM::DOMString &sheetStr);
    void setUserStyleSheet(const QString& sheet);
    QString userStyleSheet() const { return m_usersheet; }

    CSSStyleSheetImpl* elementSheet();
    virtual Tokenizer *createTokenizer();
    Tokenizer *tokenizer() { return m_tokenizer; }

    QPaintDeviceMetrics *paintDeviceMetrics() { return m_paintDeviceMetrics; }
    QPaintDevice *paintDevice() const { return m_paintDevice; }
    void setPaintDevice( QPaintDevice *dev );

    enum ParseMode {
        Unknown,
        Compat,
        Transitional,
        Strict
    };
    void determineParseMode( const QString &str );
    void setParseMode( ParseMode m ) { pMode = m; }
    ParseMode parseMode() const { return pMode; }

    void setTextColor( DOMString color ) { m_textColor = color; }
    DOMString textColor() const { return m_textColor; }

    // internal
    NodeImpl *findElement( int id );

    /**
     * find next link for keyboard traversal.
     * @param start node to start search from
     * @param forward whether to search forward or backward.
     */
    ElementImpl *findNextLink(ElementImpl *start, bool forward);

    // overrides NodeImpl
    virtual bool prepareMouseEvent( int x, int y,
                                    int _tx, int _ty,
                                    MouseEvent *ev );

    virtual bool childAllowed( NodeImpl *newChild );
    virtual bool childTypeAllowed( unsigned short nodeType );
    virtual NodeImpl *cloneNode ( bool deep, int &exceptioncode );

    unsigned short elementId(DOMStringImpl *_name);
    DOMStringImpl *elementName(unsigned short _id) const;

    StyleSheetListImpl* styleSheets();
    
    ElementImpl *focusNode();
    void setFocusNode(ElementImpl *);

    virtual DocumentImpl *getDocument()
        { return this; }

    void attachNodeIterator(NodeIteratorImpl *ni);
    void detachNodeIterator(NodeIteratorImpl *ni);
    void notifyBeforeNodeRemoval(NodeImpl *n);
    AbstractViewImpl *defaultView() const;
    EventImpl *createEvent(const DOMString &eventType, int &exceptioncode);

    // keep track of what types of event listeners are registered, so we don't
    // dispatch events un-necessarily
    enum ListenerType {
        DOMSUBTREEMODIFIED_LISTENER          = 0x01,
        DOMNODEINSERTED_LISTENER             = 0x02,
        DOMNODEREMOVED_LISTENER              = 0x04,
        DOMNODEREMOVEDFROMDOCUMENT_LISTENER  = 0x08,
        DOMNODEINSERTEDINTODOCUMENT_LISTENER = 0x10,
        DOMATTRMODIFIED_LISTENER             = 0x20,
        DOMCHARACTERDATAMODIFIED_LISTENER    = 0x40
    };

    bool hasListenerType(ListenerType listenerType) { return (m_listenerTypes & listenerType); }
    void addListenerType(ListenerType listenerType) { m_listenerTypes = m_listenerTypes | listenerType; }

    CSSStyleDeclarationImpl *getOverrideStyle(ElementImpl *elt, DOMStringImpl *pseudoElt);
    virtual void defaultEventHandler(EventImpl *evt);
    virtual void setWindowEventListener(int id, EventListener *listener);
    EventListener *getWindowEventListener(int id);
    virtual void removeWindowEventListener(int id);
    EventListener *createHTMLEventListener(QString code);

signals:
    void finishedParsing();

private:
    ElementImpl *findSelectableElement( NodeImpl *start, bool forward = true);
    ElementImpl *findLink(ElementImpl *start, bool forward, int tabIndexHint=-1);
    int findHighestTabIndex();
    ElementImpl *notabindex(DOM::ElementImpl *cur, bool forward);
    ElementImpl *intabindex(DOM::ElementImpl *cur, bool forward);
    ElementImpl *tabindexzero(DOM::ElementImpl *cur, bool forward);

protected:
    khtml::CSSStyleSelector *m_styleSelector;
    KHTMLView *m_view;
    QList<ElementImpl> m_registeredElements;
    QStringList m_state;

    khtml::DocLoader *m_docLoader;
    bool visuallyOrdered;
    Tokenizer *m_tokenizer;
    DOMString url;
    DocumentTypeImpl *m_doctype;
    DOMImplementationImpl *m_implementation;

    StyleSheetImpl *m_sheet;
    QString m_usersheet;
    bool m_loadingSheet;

    CSSStyleSheetImpl *m_elemSheet;

    QPaintDevice *m_paintDevice;
    QPaintDeviceMetrics *m_paintDeviceMetrics;
    ParseMode pMode;

    DOMString m_textColor;
    DOMStringImpl **m_elementNames;
    unsigned short m_elementNameAlloc;
    unsigned short m_elementNameCount;

    ElementImpl *m_focusNode;
    QList<NodeIteratorImpl> m_nodeIterators;
    AbstractViewImpl *m_defaultView;

    unsigned short m_listenerTypes;
    StyleSheetListImpl* m_styleSheets;
    QList<RegisteredEventListener> m_windowEventListeners;
};

class DocumentFragmentImpl : public NodeBaseImpl
{
public:
    DocumentFragmentImpl(DocumentPtr *doc);
    DocumentFragmentImpl(const DocumentFragmentImpl &other);

    virtual const DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual NodeImpl *cloneNode ( bool deep, int &exceptioncode );

protected:
    virtual bool childTypeAllowed( unsigned short type );
};


class DocumentTypeImpl : public NodeImpl
{
public:
    DocumentTypeImpl(DocumentPtr *doc);
    ~DocumentTypeImpl();

    virtual const DOMString name() const;
    virtual NamedNodeMapImpl *entities() const;
    virtual NamedNodeMapImpl *notations() const;
    virtual const DOMString nodeName() const;
    virtual unsigned short nodeType() const;

    GenericRONamedNodeMapImpl *m_entities;
    GenericRONamedNodeMapImpl *m_notations;
    virtual bool childTypeAllowed( unsigned short type );
    virtual NodeImpl *cloneNode ( bool deep, int &exceptioncode );

};

}; //namespace
#endif
