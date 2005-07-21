/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
#include <qptrdict.h>
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
class RenderArena;

#if APPLE_CHANGES
class KWQAccObjectCache;
#endif

namespace khtml {
    class CSSStyleSelector;
    struct DashboardRegionValue;
    class DocLoader;
    class RenderImage;
    class Tokenizer;
    class XMLHandler;
}

#ifndef KHTML_NO_XBL
namespace XBL {
    class XBLBindingManager;
}
#endif

namespace DOM {
    class AbstractViewImpl;
    class AttrImpl;
    class CDATASectionImpl;
    class CSSStyleSheetImpl;
    class CSSMappedAttributeDeclarationImpl;
    class CommentImpl;
    class DocumentFragmentImpl;
    class DocumentImpl;
    class DocumentType;
    class DocumentTypeImpl;
    class EditingTextImpl;
    class ElementImpl;
    class EntityReferenceImpl;
    class EventImpl;
    class EventListener;
    class GenericRONamedNodeMapImpl;
    class HTMLCollectionImpl;
    class HTMLDocumentImpl;
    class HTMLElementImpl;
    class HTMLImageLoader;
    class HTMLMapElementImpl;
    class JSEditor;
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

    // A range of a node within a document that is "marked", such as being misspelled
    struct DocumentMarker
    {
        enum MarkerType {
            Spelling
            // Not doing grammar yet, but this is a placeholder for it
            // Grammar
        };
        
        enum MarkerType type;
        ulong startOffset, endOffset;
        
        bool operator == (const DocumentMarker &o) const {
            return type == o.type && startOffset == o.startOffset && endOffset == o.endOffset;
        }
        bool operator != (const DocumentMarker &o) const {
            return !(*this == o);
        }
    };
    
class DOMImplementationImpl : public khtml::Shared<DOMImplementationImpl>
{
public:
    DOMImplementationImpl();
    ~DOMImplementationImpl();

    MAIN_THREAD_ALLOCATED;

    // DOM methods & attributes for DOMImplementation
    bool hasFeature ( const DOMString &feature, const DOMString &version );
    DocumentTypeImpl *createDocumentType( const DOMString &qualifiedName, const DOMString &publicId,
                                          const DOMString &systemId, int &exceptioncode );
    DocumentImpl *createDocument( const DOMString &namespaceURI, const DOMString &qualifiedName,
                                  DocumentTypeImpl *doctype, int &exceptioncode );

    DOMImplementationImpl* getInterface(const DOMString& feature) const;

    // From the DOMImplementationCSS interface
    CSSStyleSheetImpl *createCSSStyleSheet(const DOMString &title, const DOMString &media, int &exceptioncode);

    // From the HTMLDOMImplementation interface
    HTMLDocumentImpl* createHTMLDocument( const DOMString& title);

    // Other methods (not part of DOM)
    DocumentImpl *createDocument( KHTMLView *v = 0 );
    HTMLDocumentImpl *createHTMLDocument( KHTMLView *v = 0 );

    // Returns the static instance of this class - only one instance of this class should
    // ever be present, and is used as a factory method for creating DocumentImpl objects
    static DOMImplementationImpl *instance();

protected:
    static DOMImplementationImpl *m_instance;
};


/**
 * @internal
 */
class DocumentImpl : public QObject, public ContainerNodeImpl
{
    Q_OBJECT
public:
    DocumentImpl(DOMImplementationImpl *_implementation, KHTMLView *v);
    ~DocumentImpl();

    // DOM methods & attributes for Document

    virtual DocumentTypeImpl *doctype() const; // returns 0 for HTML documents
    DocumentTypeImpl *realDocType() const { return m_doctype; }

    DOMImplementationImpl *implementation() const;
    virtual ElementImpl *documentElement() const;
    virtual ElementImpl *createElement(const DOMString &tagName, int &exceptioncode);
    DocumentFragmentImpl *createDocumentFragment ();
    TextImpl *createTextNode ( const DOMString &data );
    CommentImpl *createComment ( const DOMString &data );
    CDATASectionImpl *createCDATASection ( const DOMString &data );
    ProcessingInstructionImpl *createProcessingInstruction ( const DOMString &target, const DOMString &data );
    AttrImpl *createAttribute(const DOMString &name, int &exception) { return createAttributeNS(DOMString(), name, exception); }
    AttrImpl *createAttributeNS(const DOMString &namespaceURI, const DOMString &qualifiedName, int &exception);
    EntityReferenceImpl *createEntityReference ( const DOMString &name );
    NodeImpl *importNode( NodeImpl *importedNode, bool deep, int &exceptioncode );
    virtual ElementImpl *createElementNS(const DOMString &_namespaceURI, const DOMString &_qualifiedName, int &exceptioncode);
    ElementImpl *getElementById ( const DOMString &elementId ) const;
    ElementImpl *elementFromPoint ( const int _x, const int _y ) const;

    SharedPtr<NameNodeListImpl> getElementsByName(const DOMString &elementName);

    // Actually part of HTMLDocument, but used for giving XML documents a window title as well
    DOMString title() const { return m_title; }
    void setTitle(DOMString, NodeImpl *titleElement = 0);
    void removeTitle(NodeImpl *titleElement);

    SharedPtr<HTMLCollectionImpl> images();
    SharedPtr<HTMLCollectionImpl> embeds();
    SharedPtr<HTMLCollectionImpl> applets();
    SharedPtr<HTMLCollectionImpl> links();
    SharedPtr<HTMLCollectionImpl> forms();
    SharedPtr<HTMLCollectionImpl> anchors();
    SharedPtr<HTMLCollectionImpl> all();
    SharedPtr<HTMLCollectionImpl> objects();
    SharedPtr<HTMLCollectionImpl> windowNamedItems(DOMString &name);
    SharedPtr<HTMLCollectionImpl> documentNamedItems(DOMString &name);

    // DOM methods overridden from  parent classes

    virtual DOMString nodeName() const;
    virtual unsigned short nodeType() const;

    // Other methods (not part of DOM)
    virtual bool isDocumentNode() const { return true; }
    virtual bool isHTMLDocument() const { return false; }

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
    bool usesSiblingRules() { return m_usesSiblingRules; }
    void setUsesSiblingRules(bool b) { m_usesSiblingRules = b; }\

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
        NodeFilterImpl *filter, bool expandEntityReferences, int &exceptioncode);

    TreeWalkerImpl *createTreeWalker(NodeImpl *root, unsigned long whatToShow, 
        NodeFilterImpl *filter, bool expandEntityReferences, int &exceptioncode);

    // Special support for editing
    CSSStyleDeclarationImpl *createCSSStyleDeclaration();
    EditingTextImpl *createEditingTextNode(const DOMString &text);

    virtual void recalcStyle( StyleChange = NoChange );
    static QPtrList<DocumentImpl> * changedDocuments;
    virtual void updateRendering();
    void updateLayout();
    void updateLayoutIgnorePendingStylesheets();
    static void updateDocumentsRendering();
    khtml::DocLoader *docLoader() { return m_docLoader; }

    virtual void attach();
    virtual void detach();

    RenderArena* renderArena() { return m_renderArena; }

#if APPLE_CHANGES
    KWQAccObjectCache* getAccObjectCache();
#endif
    
    // to get visually ordered hebrew and arabic pages right
    void setVisuallyOrdered();

    void updateSelection();
    
    void open();
    void implicitOpen();
    void close();
    void implicitClose();
    void write ( const DOMString &text );
    void write ( const QString &text );
    void writeln ( const DOMString &text );
    void finishParsing (  );
    void clear();

    QString URL() const { return m_url; }
    void setURL(const QString& url);

    QString baseURL() const { return m_baseURL.isEmpty() ? m_url : m_baseURL; }
    void setBaseURL(const QString& baseURL) { m_baseURL = baseURL; }

    QString baseTarget() const { return m_baseTarget; }
    void setBaseTarget(const QString& baseTarget) { m_baseTarget = baseTarget; }

    QString completeURL(const QString &);
    DOMString completeURL(const DOMString &);

    // from cachedObjectClient
    virtual void setStyleSheet(const DOMString &url, const DOMString &sheetStr);
    void setUserStyleSheet(const QString& sheet);
    QString userStyleSheet() const { return m_usersheet; }
    void setPrintStyleSheet(const QString& sheet) { m_printSheet = sheet; }
    QString printStyleSheet() const { return m_printSheet; }

    CSSStyleSheetImpl* elementSheet();
    virtual khtml::Tokenizer *createTokenizer();
    khtml::Tokenizer *tokenizer() { return m_tokenizer; }
    
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

    void setParsing(bool b);
    bool parsing() const { return m_bParsing; }
    int minimumLayoutDelay();
    bool shouldScheduleLayout();
    int elapsedTime() const;
    
    void setTextColor( QColor color ) { m_textColor = color; }
    QColor textColor() const { return m_textColor; }

    const QColor& linkColor() const { return m_linkColor; }
    const QColor& visitedLinkColor() const { return m_visitedLinkColor; }
    const QColor& activeLinkColor() const { return m_activeLinkColor; }
    void setLinkColor(const QColor& c) { m_linkColor = c; }
    void setVisitedLinkColor(const QColor& c) { m_visitedLinkColor = c; }
    void setActiveLinkColor(const QColor& c) { m_activeLinkColor = c; }
    void resetLinkColor();
    void resetVisitedLinkColor();
    void resetActiveLinkColor();
    
    bool prepareMouseEvent( bool readonly, int x, int y, MouseEvent *ev );

    virtual bool childAllowed( NodeImpl *newChild );
    virtual bool childTypeAllowed( unsigned short nodeType );
    virtual NodeImpl *cloneNode ( bool deep );

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
    bool setFocusNode(NodeImpl *newFocusNode);

    NodeImpl *hoverNode() const { return m_hoverNode; }
    void setHoverNode(NodeImpl *newHoverNode);
    
    // Updates for :target (CSS3 selector).
    void setCSSTarget(NodeImpl* n);
    NodeImpl* getCSSTarget();
    
    void setDocumentChanged(bool);

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

    CSSStyleDeclarationImpl *getOverrideStyle(ElementImpl *elt, const DOMString &pseudoElt);

    typedef QMap<QString, ProcessingInstructionImpl*> LocalStyleRefs;
    LocalStyleRefs* localStyleRefs() { return &m_localStyleRefs; }

    virtual void defaultEventHandler(EventImpl *evt);
    void setHTMLWindowEventListener(int id, EventListener *listener);
    EventListener *getHTMLWindowEventListener(int id);
    void removeHTMLWindowEventListener(int id);

    void addWindowEventListener(int id, EventListener *listener, const bool useCapture);
    void removeWindowEventListener(int id, EventListener *listener, bool useCapture);
    bool hasWindowEventListener(int id);

    EventListener *createHTMLEventListener(QString code, NodeImpl *node);
    
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
    
    void dispatchImageLoadEventSoon(HTMLImageLoader*);
    void dispatchImageLoadEventsNow();
    void removeImage(HTMLImageLoader*);
    virtual void timerEvent(QTimerEvent *);
    
    // Returns the owning element in the parent document.
    // Returns 0 if this is the top level document.
    ElementImpl *ownerElement();

    DOMString domain() const;
    void setDomain( const DOMString &newDomain, bool force = false ); // not part of the DOM

    DOMString policyBaseURL() const { return m_policyBaseURL; }
    void setPolicyBaseURL(const DOMString &s) { m_policyBaseURL = s; }
    
    // The following implements the rule from HTML 4 for what valid names are.
    // To get this right for all the XML cases, we probably have to improve this or move it
    // and make it sensitive to the type of document.
    static bool isValidName(const DOMString &);
    
    void addElementById(const DOMString &elementId, ElementImpl *element);
    void removeElementById(const DOMString &elementId, ElementImpl *element);

    void addImageMap(HTMLMapElementImpl *);
    void removeImageMap(HTMLMapElementImpl *);
    HTMLMapElementImpl *getImageMap(const DOMString &URL) const;

    HTMLElementImpl* body();

    DOMString toString() const;
    
    bool execCommand(const DOMString &command, bool userInterface, const DOMString &value);
    bool queryCommandEnabled(const DOMString &command);
    bool queryCommandIndeterm(const DOMString &command);
    bool queryCommandState(const DOMString &command);
    bool queryCommandSupported(const DOMString &command);
    DOMString queryCommandValue(const DOMString &command);
    
    void addMarker(RangeImpl *range, DocumentMarker::MarkerType type);
    void removeMarker(RangeImpl *range, DocumentMarker::MarkerType type);
    void addMarker(NodeImpl *node, DocumentMarker marker);
    void removeMarker(NodeImpl *node, DocumentMarker marker);
    void removeAllMarkers(NodeImpl *node, ulong startOffset, long length);
    void removeAllMarkers(NodeImpl *node);
    void removeAllMarkers();
    void shiftMarkers(NodeImpl *node, ulong startOffset, long delta);
    QValueList<DocumentMarker> markersForNode(NodeImpl *node);
    
   /**
    * designMode support
    */
    enum InheritedBool {
        off=false,
        on=true,
        inherit
    };
    
    void setDesignMode(InheritedBool value);
    InheritedBool getDesignMode() const;
    bool inDesignMode() const;
    DocumentImpl *parentDocument() const;
    DocumentImpl *topDocument() const;

    int docID() const { return m_docID; }

#ifdef KHTML_XSLT
    void applyXSLTransform(ProcessingInstructionImpl* pi);
    void setTransformSource(void* doc) { m_transformSource = doc; }
    const void* transformSource() { return m_transformSource; }
    DocumentImpl* transformSourceDocument() { return m_transformSourceDocument; }
    void setTransformSourceDocument(DocumentImpl* doc);
#endif

#ifndef KHTML_NO_XBL
    // XBL methods
    XBL::XBLBindingManager* bindingManager() const { return m_bindingManager; }
#endif

    void incDOMTreeVersion() { ++m_domtree_version; }
    unsigned int domTreeVersion() const { return m_domtree_version; }

signals:
    void finishedParsing();

protected:
    khtml::CSSStyleSelector *m_styleSelector;
    KHTMLView *m_view;
    QStringList m_state;

    khtml::DocLoader *m_docLoader;
    khtml::Tokenizer *m_tokenizer;
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

    unsigned int m_domtree_version;
    
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

    QColor m_linkColor;
    QColor m_visitedLinkColor;
    QColor m_activeLinkColor;

    DOMString m_preferredStylesheetSet;

    bool m_loadingSheet;
    bool visuallyOrdered;
    bool m_bParsing;
    bool m_bAllDataReceived;
    bool m_docChanged;
    bool m_styleSelectorDirty;
    bool m_inStyleRecalc;
    bool m_closeAfterStyleRecalc;
    bool m_usesDescendantRules;
    bool m_usesSiblingRules;

    DOMString m_title;
    bool m_titleSetExplicitly;
    NodeImpl *m_titleElement;
    
    RenderArena* m_renderArena;

    QPtrDict< QValueList<DocumentMarker> > m_markers;

#if APPLE_CHANGES
    KWQAccObjectCache* m_accCache;
#endif
    
    QPtrList<HTMLImageLoader> m_imageLoadEventDispatchSoonList;
    QPtrList<HTMLImageLoader> m_imageLoadEventDispatchingList;
    int m_imageLoadEventTimer;

    NodeImpl* m_cssTarget;
    
    bool m_processingLoadEvent;
    QTime m_startTime;
    bool m_overMinimumLayoutThreshold;
    
#ifdef KHTML_XSLT
    void* m_transformSource;
    DocumentImpl* m_transformSourceDocument;
#endif

#ifndef KHTML_NO_XBL
    XBL::XBLBindingManager* m_bindingManager; // The access point through which documents and elements communicate with XBL.
#endif
    
    QMap<QString, HTMLMapElementImpl *> m_imageMapsByName;

    DOMString m_policyBaseURL;

    QPtrDict<NodeImpl> m_disconnectedNodesWithEventListeners;

    int m_docID; // A unique document identifier used for things like document-specific mapped attributes.

#if APPLE_CHANGES
public:
    KWQSignal m_finishedParsing;

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

    void setDashboardRegionsDirty(bool f) { m_dashboardRegionsDirty = f; }
    bool dashboardRegionsDirty() const { return m_dashboardRegionsDirty; }
    bool hasDashboardRegions () const { return m_hasDashboardRegions; }
    void setHasDashboardRegions (bool f) { m_hasDashboardRegions = f; }
    const QValueList<khtml::DashboardRegionValue> & dashboardRegions() const;
    void setDashboardRegions (const QValueList<khtml::DashboardRegionValue>& regions);
    
    void removeAllEventListenersFromAllNodes();

    void registerDisconnectedNodeWithEventListeners(NodeImpl *node);
    void unregisterDisconnectedNodeWithEventListeners(NodeImpl *node);

private:
    void updateTitle();
    void removeAllDisconnectedNodeEventListeners();

    JSEditor *jsEditor();

    JSEditor *m_jsEditor;
    bool relinquishesEditingFocus(NodeImpl *node);
    bool acceptsEditingFocus(NodeImpl *node);

    mutable DOMString m_domain;
    bool m_inPageCache;
    khtml::RenderObject *m_savedRenderer;
    int m_passwordFields;
    int m_secureForms;
    
    khtml::Decoder *m_decoder;

    mutable QDict<ElementImpl> m_elementsById;
    mutable QDict<char> m_idCount;
    
    QDict<ElementImpl> m_elementsByAccessKey;
    bool m_accessKeyDictValid;
 
    bool m_createRenderers;
    
    InheritedBool m_designMode;
    
    QValueList<khtml::DashboardRegionValue> m_dashboardRegions;
    bool m_hasDashboardRegions;
    bool m_dashboardRegionsDirty;
#endif
};

class DocumentFragmentImpl : public ContainerNodeImpl
{
public:
    DocumentFragmentImpl(DocumentPtr *doc);

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
    void setPublicId(const DOMString& publicId) { m_publicId = publicId; }
    void setSystemId(const DOMString& systemId) { m_systemId = systemId; }
    DOMImplementationImpl *implementation() const { return m_implementation; }
    void copyFrom(const DocumentTypeImpl&);

    virtual DOMString toString() const;

protected:
    DOMImplementationImpl *m_implementation;
    NamedNodeMapImpl* m_entities;
    NamedNodeMapImpl* m_notations;

    DOMString m_qualifiedName;
    DOMString m_publicId;
    DOMString m_systemId;
    DOMString m_subset;
};


} //namespace

#endif
