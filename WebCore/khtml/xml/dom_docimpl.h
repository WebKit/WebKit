/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
#ifndef _DOM_DocumentImpl_h_
#define _DOM_DocumentImpl_h_

#include "xml/dom_elementimpl.h"
#include "xml/dom2_traversalimpl.h"
#include "misc/shared.h"

#include <qstringlist.h>
#include <qptrlist.h>
#include <qobject.h>
#include <qdict.h>
#include <qmap.h>
#include <qdatetime.h>

#include <kurl.h>

#if APPLE_CHANGES
#include "KWQSignal.h"
#include "decoder.h"
#endif

class QPaintDevice;
class QPaintDeviceMetrics;
class KHTMLView;
class KHTMLPart;
class Tokenizer;
class RenderArena;

#if APPLE_CHANGES
class KWQAccObjectCache;
#endif

namespace khtml {
    class CSSStyleSelector;
    class DocLoader;
    class CSSStyleSelectorList;
    class RenderImage;
}

namespace DOM {

    class AbstractViewImpl;
    class AttrImpl;
    class CDATASectionImpl;
    class CSSStyleSheetImpl;
    class CommentImpl;
    class DocumentFragmentImpl;
    class DocumentImpl;
    class DocumentType;
    class DocumentTypeImpl;
#if APPLE_CHANGES
    class DOMImplementation;
#endif
    class ElementImpl;
    class EntityReferenceImpl;
    class EventImpl;
    class EventListener;
    class GenericRONamedNodeMapImpl;
    class HTMLDocumentImpl;
    class HTMLElementImpl;
    class NodeFilter;
    class NodeFilterImpl;
    class NodeIteratorImpl;
    class NodeListImpl;
    class ProcessingInstructionImpl;
    class RangeImpl;
    class RegisteredEventListener;
    class StyleSheetImpl;
    class StyleSheetListImpl;
    class TextImpl;
    class TreeWalkerImpl;

class DOMImplementationImpl : public khtml::Shared<DOMImplementationImpl>
{
public:
    DOMImplementationImpl();
    ~DOMImplementationImpl();

    // DOM methods & attributes for DOMImplementation
    bool hasFeature ( const DOMString &feature, const DOMString &version );
    DocumentTypeImpl *createDocumentType( const DOMString &qualifiedName, const DOMString &publicId,
                                          const DOMString &systemId, int &exceptioncode );
    DocumentImpl *createDocument( const DOMString &namespaceURI, const DOMString &qualifiedName,
                                  const DocumentType &doctype, int &exceptioncode );

    DOMImplementationImpl* getInterface(const DOMString& feature) const;

    // From the DOMImplementationCSS interface
    CSSStyleSheetImpl *createCSSStyleSheet(DOMStringImpl *title, DOMStringImpl *media, int &exceptioncode);

    // From the HTMLDOMImplementation interface
    HTMLDocumentImpl* createHTMLDocument( const DOMString& title);

    // Other methods (not part of DOM)
    DocumentImpl *createDocument( KHTMLView *v = 0 );
    HTMLDocumentImpl *createHTMLDocument( KHTMLView *v = 0 );

    // Returns the static instance of this class - only one instance of this class should
    // ever be present, and is used as a factory method for creating DocumentImpl objects
    static DOMImplementationImpl *instance();

#if APPLE_CHANGES
    static DOMImplementation createInstance (DOMImplementationImpl *impl);
#endif

protected:
    static DOMImplementationImpl *m_instance;
};


/**
 * @internal
 */
class DocumentImpl : public QObject, public NodeBaseImpl
{
    Q_OBJECT
public:
    DocumentImpl(DOMImplementationImpl *_implementation, KHTMLView *v);
    ~DocumentImpl();

    // DOM methods & attributes for Document

    DocumentTypeImpl *doctype() const;

    DOMImplementationImpl *implementation() const;
    ElementImpl *documentElement() const;
    virtual ElementImpl *createElement ( const DOMString &tagName, int &exceptioncode );
    DocumentFragmentImpl *createDocumentFragment ();
    TextImpl *createTextNode ( const DOMString &data );
    CommentImpl *createComment ( const DOMString &data );
    CDATASectionImpl *createCDATASection ( const DOMString &data );
    ProcessingInstructionImpl *createProcessingInstruction ( const DOMString &target, const DOMString &data );
    Attr createAttribute(NodeImpl::Id id);
    EntityReferenceImpl *createEntityReference ( const DOMString &name );
    NodeImpl *importNode( NodeImpl *importedNode, bool deep, int &exceptioncode );
    virtual ElementImpl *createElementNS ( const DOMString &_namespaceURI, const DOMString &_qualifiedName, int &exceptioncode );
    ElementImpl *getElementById ( const DOMString &elementId ) const;

    // Actually part of HTMLDocument, but used for giving XML documents a window title as well
    DOMString title() const { return m_title; }
    void setTitle(DOMString _title);

    // DOM methods overridden from  parent classes

    virtual DOMString nodeName() const;
    virtual unsigned short nodeType() const;

    // Other methods (not part of DOM)
    virtual bool isDocumentNode() const { return true; }
    virtual bool isHTMLDocument() const { return false; }

    virtual ElementImpl *createHTMLElement ( const DOMString &tagName, int &exceptioncode );

    khtml::CSSStyleSelector *styleSelector() { return m_styleSelector; }

    ElementImpl *DocumentImpl::getElementByAccessKey( const DOMString &key );
    
    /**
     * Updates the pending sheet count and then calls updateStyleSelector.
     */
    void stylesheetLoaded();

    /**
     * This method returns true if all top-level stylesheets have loaded (including
     * any @imports that they may be loading).
     */
    bool haveStylesheetsLoaded() { return m_pendingStylesheets <= 0 || m_ignorePendingStylesheets; }

    /**
     * Increments the number of pending sheets.  The <link> elements
     * invoke this to add themselves to the loading list.
     */
    void addPendingSheet() { m_pendingStylesheets++; }

    /**
     * Called when one or more stylesheets in the document may have been added, removed or changed.
     *
     * Creates a new style selector and assign it to this document. This is done by iterating through all nodes in
     * document (or those before <BODY> in a HTML document), searching for stylesheets. Stylesheets can be contained in
     * <LINK>, <STYLE> or <BODY> elements, as well as processing instructions (XML documents only). A list is
     * constructed from these which is used to create the a new style selector which collates all of the stylesheets
     * found and is used to calculate the derived styles for all rendering objects.
     */
    void updateStyleSelector();

    void recalcStyleSelector();

    bool usesDescendantRules() { return m_usesDescendantRules; }
    void setUsesDescendantRules(bool b) { m_usesDescendantRules = b; }
    
    QString nextState();

    // Query all registered elements for their state
    QStringList docState();
    void registerMaintainsState(NodeImpl* e) { m_maintainsState.append(e); }
    void deregisterMaintainsState(NodeImpl* e) { m_maintainsState.removeRef(e); }

    // Set the state the document should restore to
    void setRestoreState( const QStringList &s) { m_state = s; }
    QStringList &restoreState( ) { return m_state; }

    KHTMLView *view() const { return m_view; }
    KHTMLPart *part() const;

    RangeImpl *createRange();

    NodeIteratorImpl *createNodeIterator(NodeImpl *root, unsigned long whatToShow,
                                    NodeFilter &filter, bool entityReferenceExpansion, int &exceptioncode);

    TreeWalkerImpl *createTreeWalker(Node root, unsigned long whatToShow, NodeFilter &filter,
                            bool entityReferenceExpansion);

    virtual void recalcStyle( StyleChange = NoChange );
    static QPtrList<DocumentImpl> * changedDocuments;
    virtual void updateRendering();
    void updateLayout();
    static void updateDocumentsRendering();
    khtml::DocLoader *docLoader() { return m_docLoader; }

    virtual void attach();
    virtual void detach();

    RenderArena* renderArena() { return m_renderArena; }

#if APPLE_CHANGES
    KWQAccObjectCache* getExistingAccObjectCache() { return m_accCache; }
    KWQAccObjectCache* getOrCreateAccObjectCache();
#endif
    
    // to get visually ordered hebrew and arabic pages right
    void setVisuallyOrdered();

    void setSelection(NodeImpl* s, int sp, NodeImpl* e, int ep);
    void clearSelection();

    void open();
    void close();
    void closeInternal ( bool checkTokenizer );
    void write ( const DOMString &text );
    void write ( const QString &text );
    void writeln ( const DOMString &text );
    void finishParsing (  );
    void clear();

    QString URL() const { return m_url; }
    void setURL(QString url) { m_url = url; }

    QString baseURL() const { return m_baseURL.isEmpty() ? m_url : m_baseURL; }
    void setBaseURL(const QString& baseURL) { m_baseURL = baseURL; }

    QString baseTarget() const { return m_baseTarget; }
    void setBaseTarget(const QString& baseTarget) { m_baseTarget = baseTarget; }

#if APPLE_CHANGES
    QString completeURL(const QString &);
#else
    QString completeURL(const QString& url) { return KURL(baseURL(),url).url(); };
#endif

    // from cachedObjectClient
    virtual void setStyleSheet(const DOM::DOMString &url, const DOM::DOMString &sheetStr);
    void setUserStyleSheet(const QString& sheet);
    QString userStyleSheet() const { return m_usersheet; }
    void setPrintStyleSheet(const QString& sheet) { m_printSheet = sheet; }
    QString printStyleSheet() const { return m_printSheet; }

    CSSStyleSheetImpl* elementSheet();
    virtual Tokenizer *createTokenizer();
    Tokenizer *tokenizer() { return m_tokenizer; }

    QPaintDeviceMetrics *paintDeviceMetrics() { return m_paintDeviceMetrics; }
    QPaintDevice *paintDevice() const { return m_paintDevice; }
    void setPaintDevice( QPaintDevice *dev );

    enum HTMLMode {
        Html3,
        Html4,
        XHtml
    };

    enum ParseMode {
        Compat,
        AlmostStrict,
        Strict
    };
    
    virtual void determineParseMode( const QString &str );
    void setParseMode( ParseMode m ) { pMode = m; }
    ParseMode parseMode() const { return pMode; }

    bool inCompatMode() { return pMode == Compat; }
    bool inAlmostStrictMode() { return pMode == AlmostStrict; }
    bool inStrictMode() { return pMode == Strict; }
    
    void setHTMLMode( HTMLMode m ) { hMode = m; }
    HTMLMode htmlMode() const { return hMode; }

    void setParsing(bool b) { m_bParsing = b; }
    bool parsing() const { return m_bParsing; }

    void setTextColor( QColor color ) { m_textColor = color; }
    QColor textColor() const { return m_textColor; }

    // internal
    NodeImpl *findElement( Id id );

    bool prepareMouseEvent( bool readonly, int x, int y, MouseEvent *ev );

    virtual bool childAllowed( NodeImpl *newChild );
    virtual bool childTypeAllowed( unsigned short nodeType );
    virtual NodeImpl *cloneNode ( bool deep );

    // ### think about implementing ref'counting for the id's
    // in order to be able to reassign those that are no longer in use
    // (could make problems when it is still kept somewhere around, i.e. styleselector)
    NodeImpl::Id tagId(DOMStringImpl* _namespaceURI, DOMStringImpl *_name, bool readonly);
    DOMString tagName(NodeImpl::Id _id) const;

    NodeImpl::Id attrId(DOMStringImpl* _namespaceURI, DOMStringImpl *_name, bool readonly);
    DOMString attrName(NodeImpl::Id _id) const;

    // the namespace uri is mapped to the same id for both
    // tagnames as well as attributes.
    DOMStringImpl* namespaceURI(NodeImpl::Id _id) const;

    StyleSheetListImpl* styleSheets();

    /* Newly proposed CSS3 mechanism for selecting alternate
       stylesheets using the DOM. May be subject to change as
       spec matures. - dwh
    */
    DOMString preferredStylesheetSet();
    DOMString selectedStylesheetSet();
    void setSelectedStylesheetSet(const DOMString& aString);

    QStringList availableStyleSheets() const;

    NodeImpl *focusNode() const { return m_focusNode; }
    void setFocusNode(NodeImpl *newFocusNode);

    NodeImpl *hoverNode() const { return m_hoverNode; }
    void setHoverNode(NodeImpl *newHoverNode);
    
    // Updates for :target (CSS3 selector).
    void setCSSTarget(NodeImpl* n);
    NodeImpl* getCSSTarget();
    
    bool isDocumentChanged()	{ return m_docChanged; }
    virtual void setDocumentChanged(bool);
    void attachNodeIterator(NodeIteratorImpl *ni);
    void detachNodeIterator(NodeIteratorImpl *ni);
    void notifyBeforeNodeRemoval(NodeImpl *n);
    AbstractViewImpl *defaultView() const;
    EventImpl *createEvent(const DOMString &eventType, int &exceptioncode);

    // keep track of what types of event listeners are registered, so we don't
    // dispatch events unnecessarily
    enum ListenerType {
        DOMSUBTREEMODIFIED_LISTENER          = 0x01,
        DOMNODEINSERTED_LISTENER             = 0x02,
        DOMNODEREMOVED_LISTENER              = 0x04,
        DOMNODEREMOVEDFROMDOCUMENT_LISTENER  = 0x08,
        DOMNODEINSERTEDINTODOCUMENT_LISTENER = 0x10,
        DOMATTRMODIFIED_LISTENER             = 0x20,
        DOMCHARACTERDATAMODIFIED_LISTENER    = 0x40
    };

    bool hasListenerType(ListenerType listenerType) const { return (m_listenerTypes & listenerType); }
    void addListenerType(ListenerType listenerType) { m_listenerTypes = m_listenerTypes | listenerType; }

    CSSStyleDeclarationImpl *getOverrideStyle(ElementImpl *elt, DOMStringImpl *pseudoElt);

    typedef QMap<QString, ProcessingInstructionImpl*> LocalStyleRefs;
    LocalStyleRefs* localStyleRefs() { return &m_localStyleRefs; }

    virtual void defaultEventHandler(EventImpl *evt);
    void setHTMLWindowEventListener(int id, EventListener *listener);
    EventListener *getHTMLWindowEventListener(int id);
    void removeHTMLWindowEventListener(int id);

    void addWindowEventListener(int id, EventListener *listener, const bool useCapture);
    void removeWindowEventListener(int id, EventListener *listener, bool useCapture);
    bool hasWindowEventListener(int id);

    EventListener *createHTMLEventListener(QString code);

    /**
     * Searches through the document, starting from fromNode, for the next selectable element that comes after fromNode.
     * The order followed is as specified in section 17.11.1 of the HTML4 spec, which is elements with tab indexes
     * first (from lowest to highest), and then elements without tab indexes (in document order).
     *
     * @param fromNode The node from which to start searching. The node after this will be focused. May be null.
     *
     * @return The focus node that comes after fromNode
     *
     * See http://www.w3.org/TR/html4/interact/forms.html#h-17.11.1
     */
    NodeImpl *nextFocusNode(NodeImpl *fromNode);

    /**
     * Searches through the document, starting from fromNode, for the previous selectable element (that comes _before_)
     * fromNode. The order followed is as specified in section 17.11.1 of the HTML4 spec, which is elements with tab
     * indexes first (from lowest to highest), and then elements without tab indexes (in document order).
     *
     * @param fromNode The node from which to start searching. The node before this will be focused. May be null.
     *
     * @return The focus node that comes before fromNode
     *
     * See http://www.w3.org/TR/html4/interact/forms.html#h-17.11.1
     */
    NodeImpl *previousFocusNode(NodeImpl *fromNode);

    int nodeAbsIndex(NodeImpl *node);
    NodeImpl *nodeWithAbsIndex(int absIndex);

    /**
     * Handles a HTTP header equivalent set by a meta tag using <meta http-equiv="..." content="...">. This is called
     * when a meta tag is encountered during document parsing, and also when a script dynamically changes or adds a meta
     * tag. This enables scripts to use meta tags to perform refreshes and set expiry dates in addition to them being
     * specified in a HTML file.
     *
     * @param equiv The http header name (value of the meta tag's "equiv" attribute)
     * @param content The header value (value of the meta tag's "content" attribute)
     */
    void processHttpEquiv(const DOMString &equiv, const DOMString &content);
    
    void dispatchImageLoadEventSoon(khtml::RenderImage *);
    void dispatchImageLoadEventsNow();
    void removeImage(khtml::RenderImage *);
    virtual void timerEvent(QTimerEvent *);
    
    // Returns the owning element in the parent document.
    // Returns 0 if this is the top level document.
    ElementImpl *ownerElement();

    DOMString domain() const;
    void setDomain( const DOMString &newDomain, bool force = false ); // not part of the DOM
    
    // The following implements the rule from HTML 4 for what valid names are.
    // To get this right for all the XML cases, we probably have to improve this or move it
    // and make it sensitive to the type of document.
    static bool isValidName(const DOMString &);
    
    void addElementById(const DOMString &elementId, ElementImpl *element);
    void removeElementById(const DOMString &elementId, ElementImpl *element);

    HTMLElementImpl* body();

    DOMString toString() const;
    
signals:
    void finishedParsing();

protected:
    khtml::CSSStyleSelector *m_styleSelector;
    KHTMLView *m_view;
    QStringList m_state;

    khtml::DocLoader *m_docLoader;
    Tokenizer *m_tokenizer;
    QString m_url;
    QString m_baseURL;
    QString m_baseTarget;

    DocumentTypeImpl *m_doctype;
    DOMImplementationImpl *m_implementation;

    StyleSheetImpl *m_sheet;
    QString m_usersheet;
    QString m_printSheet;
    QStringList m_availableSheets;

    // Track the number of currently loading top-level stylesheets.  Sheets
    // loaded using the @import directive are not included in this count.
    // We use this count of pending sheets to detect when we can begin attaching
    // elements.
    int m_pendingStylesheets;

    // But sometimes you need to ignore pending stylesheet count to
    // force an immediate layout when requested by JS.
    bool m_ignorePendingStylesheets;

    CSSStyleSheetImpl *m_elemSheet;

    QPaintDevice *m_paintDevice;
    QPaintDeviceMetrics *m_paintDeviceMetrics;
    ParseMode pMode;
    HTMLMode hMode;

    QColor m_textColor;

    NodeImpl *m_focusNode;
    NodeImpl *m_hoverNode;
    
    // ### replace me with something more efficient
    // in lookup and insertion.
    DOMStringImpl **m_elementNames;
    unsigned short m_elementNameAlloc;
    unsigned short m_elementNameCount;

    DOMStringImpl **m_attrNames;
    unsigned short m_attrNameAlloc;
    unsigned short m_attrNameCount;

    DOMStringImpl** m_namespaceURIs;
    unsigned short m_namespaceURIAlloc;
    unsigned short m_namespaceURICount;

    QPtrList<NodeIteratorImpl> m_nodeIterators;
    AbstractViewImpl *m_defaultView;

    unsigned short m_listenerTypes;
    StyleSheetListImpl* m_styleSheets;
    LocalStyleRefs m_localStyleRefs; // references to inlined style elements
    QPtrList<RegisteredEventListener> m_windowEventListeners;
    QPtrList<NodeImpl> m_maintainsState;

    DOMString m_preferredStylesheetSet;

    bool m_loadingSheet;
    bool visuallyOrdered;
    bool m_bParsing;
    bool m_docChanged;
    bool m_styleSelectorDirty;
    bool m_inStyleRecalc;
    bool m_usesDescendantRules;
    
    DOMString m_title;
    
    RenderArena* m_renderArena;

#if APPLE_CHANGES
    KWQAccObjectCache* m_accCache;
#endif
    
    QPtrList<khtml::RenderImage> m_imageLoadEventDispatchSoonList;
    QPtrList<khtml::RenderImage> m_imageLoadEventDispatchingList;
    int m_imageLoadEventTimer;

    NodeImpl* m_cssTarget;
    
    bool m_processingLoadEvent;
    QTime m_startTime;

#if APPLE_CHANGES
public:
    KWQSignal m_finishedParsing;

    static Document createInstance (DocumentImpl *impl);

    bool inPageCache();
    void setInPageCache(bool flag);
    void restoreRenderer(khtml::RenderObject* render);

    void passwordFieldAdded();
    void passwordFieldRemoved();
    bool hasPasswordField() const;

    void secureFormAdded();
    void secureFormRemoved();
    bool hasSecureForm() const;

    void setShouldCreateRenderers(bool f);
    bool shouldCreateRenderers();
    
    void setDecoder(khtml::Decoder *);
    khtml::Decoder *decoder() const { return m_decoder; }

private:
    mutable DOMString m_domain;
    bool m_inPageCache;
    khtml::RenderObject *m_savedRenderer;
    int m_passwordFields;
    int m_secureForms;
    
    khtml::Decoder *m_decoder;

    QDict<ElementImpl> m_elementsById;
    
    QDict<ElementImpl> m_elementsByAccessKey;
    bool m_accessKeyDictValid;
 
    bool m_createRenderers;
#endif
};

class DocumentFragmentImpl : public NodeBaseImpl
{
public:
    DocumentFragmentImpl(DocumentPtr *doc);
    DocumentFragmentImpl(const DocumentFragmentImpl &other);

    // DOM methods overridden from  parent classes
    virtual DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual NodeImpl *cloneNode ( bool deep );

    // Other methods (not part of DOM)
    virtual bool childTypeAllowed( unsigned short type );

    virtual DOMString toString() const;
};


class DocumentTypeImpl : public NodeImpl
{
public:
    DocumentTypeImpl(DOMImplementationImpl *_implementation, DocumentPtr *doc,
                     const DOMString &qualifiedName, const DOMString &publicId,
                     const DOMString &systemId);
    ~DocumentTypeImpl();

    // DOM methods & attributes for DocumentType
    NamedNodeMapImpl *entities() const { return m_entities; }
    NamedNodeMapImpl *notations() const { return m_notations; }

    DOMString name() const { return m_qualifiedName; }
    DOMString publicId() const { return m_publicId; }
    DOMString systemId() const { return m_systemId; }
    DOMString internalSubset() const { return m_subset; }

    // DOM methods overridden from  parent classes
    virtual DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual bool childTypeAllowed( unsigned short type );
    virtual NodeImpl *cloneNode ( bool deep );

    // Other methods (not part of DOM)
    void setName(const DOMString& n) { m_qualifiedName = n; }
    DOMImplementationImpl *implementation() const { return m_implementation; }
    void copyFrom(const DocumentTypeImpl&);

    virtual DOMString toString() const;

#if APPLE_CHANGES
    static DocumentType createInstance (DocumentTypeImpl *impl);
#endif

protected:
    DOMImplementationImpl *m_implementation;
    NamedNodeMapImpl* m_entities;
    NamedNodeMapImpl* m_notations;

    DOMString m_qualifiedName;
    DOMString m_publicId;
    DOMString m_systemId;
    DOMString m_subset;
};

}; //namespace
#endif
