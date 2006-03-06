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

#ifndef DOM_DocumentImpl_h
#define DOM_DocumentImpl_h

#include "Color.h"
#include "DocumentMarker.h"
#include "Shared.h"
#include "Timer.h"
#include "decoder.h"
#include "dom2_traversalimpl.h"
#include "dom_elementimpl.h"
#include <KURL.h>
#include <kxmlcore/HashCountedSet.h>
#include <kxmlcore/HashMap.h>
#include <qptrlist.h>
#include <QStringList.h>

class KWQAccObjectCache;
class RenderArena;

#ifndef KHTML_NO_XBL
namespace XBL {
    class XBLBindingManager;
}
#endif

namespace WebCore {

    class AbstractViewImpl;
    class AttrImpl;
    class CDATASectionImpl;
    class CSSStyleSelector;
    class CSSStyleSheetImpl;
    class CommentImpl;
    class DOMImplementationImpl;
    class DocLoader;
    class DocumentFragmentImpl;
    class DocumentTypeImpl;
    class EditingTextImpl;
    class ElementImpl;
    class EntityReferenceImpl;
    class EventImpl;
    class EventListener;
    class Frame;
    class FrameView;
    class HTMLCollectionImpl;
    class HTMLDocumentImpl;
    class HTMLElementImpl;
    class HTMLFormElementImpl;
    class HTMLImageLoader;
    class HTMLInputElementImpl;
    class HTMLMapElementImpl;
    class JSEditor;
    class NameNodeListImpl;
    class NodeFilterImpl;
    class NodeIteratorImpl;
    class NodeListImpl;
    class MouseEvent;
    class MouseEventWithHitTestResults;
    class ProcessingInstructionImpl;
    class RangeImpl;
    class RegisteredEventListener;
    class StyleSheetImpl;
    class StyleSheetListImpl;
    class TextImpl;
    class Tokenizer;
    class TreeWalkerImpl;

#if __APPLE__
    struct DashboardRegionValue;
#endif

#if SVG_SUPPORT
    class SVGDocumentExtensions;
#endif

class DocumentImpl : public ContainerNodeImpl
{
public:
    DocumentImpl(DOMImplementationImpl*, FrameView*);
    ~DocumentImpl();

    virtual void removedLastRef();

    // Nodes belonging to this document hold "self-only" references -
    // these are enough to keep the document from being destroyed, but
    // not enough to keep it from removing its children. This allows a
    // node that outlives its document to still have a valid document
    // pointer without introducing reference cycles

    void selfOnlyRef() { ++m_selfOnlyRefCount; }
    void selfOnlyDeref() {
        --m_selfOnlyRefCount;
        if (!m_selfOnlyRefCount && !refCount())
            delete this;
    }

    // DOM methods & attributes for Document

    virtual DocumentTypeImpl* doctype() const; // returns 0 for HTML documents
    DocumentTypeImpl* realDocType() const { return m_docType.get(); }

    DOMImplementationImpl* implementation() const;
    virtual ElementImpl* documentElement() const;
    virtual PassRefPtr<ElementImpl> createElement(const String& tagName, ExceptionCode&);
    PassRefPtr<DocumentFragmentImpl> createDocumentFragment ();
    PassRefPtr<TextImpl> createTextNode(const String& data);
    PassRefPtr<CommentImpl> createComment(const String& data);
    PassRefPtr<CDATASectionImpl> createCDATASection(const String& data, ExceptionCode&);
    PassRefPtr<ProcessingInstructionImpl> createProcessingInstruction(const String& target, const String& data, ExceptionCode&);
    PassRefPtr<AttrImpl> createAttribute(const String& name, ExceptionCode& ec) { return createAttributeNS(String(), name, ec); }
    PassRefPtr<AttrImpl> createAttributeNS(const String& namespaceURI, const String& qualifiedName, ExceptionCode&);
    PassRefPtr<EntityReferenceImpl> createEntityReference(const String& name, ExceptionCode&);
    PassRefPtr<NodeImpl> importNode(NodeImpl* importedNode, bool deep, ExceptionCode&);
    virtual PassRefPtr<ElementImpl> createElementNS(const String& namespaceURI, const String& qualifiedName, ExceptionCode&);
    ElementImpl* getElementById(const AtomicString&) const;
    ElementImpl* elementFromPoint(int x, int y) const;

    PassRefPtr<NodeImpl> adoptNode(PassRefPtr<NodeImpl> source, ExceptionCode&);
    
    PassRefPtr<NameNodeListImpl> getElementsByName(const String& elementName);

    // Actually part of HTMLDocument, but used for giving XML documents a window title as well
    String title() const { return m_title; }
    void setTitle(const String&, NodeImpl *titleElement = 0);
    void removeTitle(NodeImpl *titleElement);

    PassRefPtr<HTMLCollectionImpl> images();
    PassRefPtr<HTMLCollectionImpl> embeds();
    PassRefPtr<HTMLCollectionImpl> applets();
    PassRefPtr<HTMLCollectionImpl> links();
    PassRefPtr<HTMLCollectionImpl> forms();
    PassRefPtr<HTMLCollectionImpl> anchors();
    PassRefPtr<HTMLCollectionImpl> all();
    PassRefPtr<HTMLCollectionImpl> objects();
    PassRefPtr<HTMLCollectionImpl> windowNamedItems(const String& name);
    PassRefPtr<HTMLCollectionImpl> documentNamedItems(const String& name);

    // DOM methods overridden from  parent classes

    virtual String nodeName() const;
    virtual NodeType nodeType() const;

    // Other methods (not part of DOM)
    virtual bool isDocumentNode() const { return true; }
    virtual bool isHTMLDocument() const { return false; }

    CSSStyleSelector* styleSelector() const { return m_styleSelector; }

    ElementImpl* getElementByAccessKey(const String& key);
    
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
    void setRestoreState(const QStringList& s) { m_state = s; }
    QStringList& restoreState( ) { return m_state; }

    FrameView* view() const { return m_view; }
    Frame* frame() const;

    PassRefPtr<RangeImpl> createRange();

    PassRefPtr<NodeIteratorImpl> createNodeIterator(NodeImpl* root, unsigned whatToShow,
        PassRefPtr<NodeFilterImpl>, bool expandEntityReferences, ExceptionCode&);

    PassRefPtr<TreeWalkerImpl> createTreeWalker(NodeImpl* root, unsigned whatToShow, 
        PassRefPtr<NodeFilterImpl>, bool expandEntityReferences, ExceptionCode&);

    // Special support for editing
    PassRefPtr<CSSStyleDeclarationImpl> createCSSStyleDeclaration();
    PassRefPtr<EditingTextImpl> createEditingTextNode(const String&);

    virtual void recalcStyle( StyleChange = NoChange );
    static QPtrList<DocumentImpl> * changedDocuments;
    virtual void updateRendering();
    void updateLayout();
    void updateLayoutIgnorePendingStylesheets();
    static void updateDocumentsRendering();
    DocLoader* docLoader() { return m_docLoader; }

    virtual void attach();
    virtual void detach();

    RenderArena* renderArena() { return m_renderArena; }

    KWQAccObjectCache* getAccObjectCache();
    
    // to get visually ordered hebrew and arabic pages right
    void setVisuallyOrdered();

    void updateSelection();
    
    void open();
    void implicitOpen();
    void close();
    void implicitClose();
    void cancelParsing();

    void write(const String& text);
    void write(const QString &text);
    void writeln(const String& text);
    void finishParsing();
    void clear();

    QString URL() const { return m_url.isEmpty() ? "about:blank" : m_url; }
    void setURL(const QString& url);

    QString baseURL() const { return m_baseURL.isEmpty() ? URL() : m_baseURL; }
    void setBaseURL(const QString& baseURL) { m_baseURL = baseURL; }

    QString baseTarget() const { return m_baseTarget; }
    void setBaseTarget(const QString& baseTarget) { m_baseTarget = baseTarget; }

    QString completeURL(const QString &);
    String completeURL(const String&);

    // from cachedObjectClient
    virtual void setStyleSheet(const String& url, const String& sheetStr);
    void setUserStyleSheet(const QString& sheet);
    QString userStyleSheet() const { return m_usersheet; }
    void setPrintStyleSheet(const QString& sheet) { m_printSheet = sheet; }
    QString printStyleSheet() const { return m_printSheet; }

    CSSStyleSheetImpl* elementSheet();
    virtual Tokenizer* createTokenizer();
    Tokenizer* tokenizer() { return m_tokenizer; }
    
    bool printing() const { return m_printing; }
    void setPrinting(bool p) { m_printing = p; }

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
    
    void setTextColor(const Color& color) { m_textColor = color; }
    Color textColor() const { return m_textColor; }

    const Color& linkColor() const { return m_linkColor; }
    const Color& visitedLinkColor() const { return m_visitedLinkColor; }
    const Color& activeLinkColor() const { return m_activeLinkColor; }
    void setLinkColor(const Color& c) { m_linkColor = c; }
    void setVisitedLinkColor(const Color& c) { m_visitedLinkColor = c; }
    void setActiveLinkColor(const Color& c) { m_activeLinkColor = c; }
    void resetLinkColor();
    void resetVisitedLinkColor();
    void resetActiveLinkColor();
    
    MouseEventWithHitTestResults prepareMouseEvent(bool readonly, bool active, bool mouseMove, int x, int y, MouseEvent*);

    virtual bool childAllowed(NodeImpl*);
    virtual bool childTypeAllowed(NodeType);
    virtual PassRefPtr<NodeImpl> cloneNode(bool deep);

    StyleSheetListImpl* styleSheets();

    /* Newly proposed CSS3 mechanism for selecting alternate
       stylesheets using the DOM. May be subject to change as
       spec matures. - dwh
    */
    String preferredStylesheetSet();
    String selectedStylesheetSet();
    void setSelectedStylesheetSet(const String&);

    QStringList availableStyleSheets() const;

    NodeImpl* focusNode() const { return m_focusNode.get(); }
    bool setFocusNode(PassRefPtr<NodeImpl>);
    void clearSelectionIfNeeded(NodeImpl*);

    NodeImpl* hoverNode() const { return m_hoverNode.get(); }
    void setHoverNode(PassRefPtr<NodeImpl>);
    void hoveredNodeDetached(NodeImpl*);
    void activeChainNodeDetached(NodeImpl*);
    
    NodeImpl* activeNode() const { return m_activeNode.get(); }
    void setActiveNode(PassRefPtr<NodeImpl>);

    // Updates for :target (CSS3 selector).
    void setCSSTarget(NodeImpl*);
    NodeImpl* getCSSTarget();
    
    void setDocumentChanged(bool);

    void attachNodeIterator(NodeIteratorImpl*);
    void detachNodeIterator(NodeIteratorImpl*);
    void notifyBeforeNodeRemoval(NodeImpl*);
    AbstractViewImpl* defaultView() const;
    PassRefPtr<EventImpl> createEvent(const String& eventType, ExceptionCode&);

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

    CSSStyleDeclarationImpl* getOverrideStyle(ElementImpl*, const String& pseudoElt);

    virtual void defaultEventHandler(EventImpl*);
    void handleWindowEvent(EventImpl*, bool useCapture);
    void setHTMLWindowEventListener(const AtomicString &eventType, PassRefPtr<EventListener>);
    EventListener* getHTMLWindowEventListener(const AtomicString &eventType);
    void removeHTMLWindowEventListener(const AtomicString &eventType);

    void setHTMLWindowEventListener(const AtomicString& eventType, AttributeImpl*);

    void addWindowEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    void removeWindowEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
    bool hasWindowEventListener(const AtomicString& eventType);

    PassRefPtr<EventListener> createHTMLEventListener(const String& code, NodeImpl*);
    
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
    NodeImpl* nextFocusNode(NodeImpl* fromNode);

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
    NodeImpl* previousFocusNode(NodeImpl* fromNode);

    int nodeAbsIndex(NodeImpl*);
    NodeImpl* nodeWithAbsIndex(int absIndex);

    /**
     * Handles a HTTP header equivalent set by a meta tag using <meta http-equiv="..." content="...">. This is called
     * when a meta tag is encountered during document parsing, and also when a script dynamically changes or adds a meta
     * tag. This enables scripts to use meta tags to perform refreshes and set expiry dates in addition to them being
     * specified in a HTML file.
     *
     * @param equiv The http header name (value of the meta tag's "equiv" attribute)
     * @param content The header value (value of the meta tag's "content" attribute)
     */
    void processHttpEquiv(const String& equiv, const String& content);
    
    void dispatchImageLoadEventSoon(HTMLImageLoader*);
    void dispatchImageLoadEventsNow();
    void removeImage(HTMLImageLoader*);
    
    // Returns the owning element in the parent document.
    // Returns 0 if this is the top level document.
    ElementImpl* ownerElement();
    
    String referrer() const;
    String domain() const;
    void setDomain(const String& newDomain, bool force = false); // not part of the DOM

    String policyBaseURL() const { return m_policyBaseURL; }
    void setPolicyBaseURL(const String& s) { m_policyBaseURL = s; }
    
    // The following implements the rule from HTML 4 for what valid names are.
    // To get this right for all the XML cases, we probably have to improve this or move it
    // and make it sensitive to the type of document.
    static bool isValidName(const String&);

    // The following breaks a qualified name into a prefix and a local name.
    // It also does a validity check, and returns false if the qualified name is invalid
    // (empty string or invalid characters).
    static bool parseQualifiedName(const String& qualifiedName, String& prefix, String& localName);
    
    void addElementById(const AtomicString& elementId, ElementImpl *element);
    void removeElementById(const AtomicString& elementId, ElementImpl *element);

    void addImageMap(HTMLMapElementImpl*);
    void removeImageMap(HTMLMapElementImpl*);
    HTMLMapElementImpl* getImageMap(const String& URL) const;

    HTMLElementImpl* body();

    String toString() const;
    
    bool execCommand(const String& command, bool userInterface, const String& value);
    bool queryCommandEnabled(const String& command);
    bool queryCommandIndeterm(const String& command);
    bool queryCommandState(const String& command);
    bool queryCommandSupported(const String& command);
    String queryCommandValue(const String& command);
    
    void addMarker(RangeImpl*, DocumentMarker::MarkerType type);
    void addMarker(NodeImpl*, DocumentMarker marker);
    void copyMarkers(NodeImpl *srcNode, unsigned startOffset, int length, NodeImpl *dstNode, int delta, DocumentMarker::MarkerType markerType=DocumentMarker::AllMarkers);
    void removeMarkers(RangeImpl*, DocumentMarker::MarkerType markerType=DocumentMarker::AllMarkers);
    void removeMarkers(NodeImpl*, unsigned startOffset, int length, DocumentMarker::MarkerType markerType=DocumentMarker::AllMarkers);
    void removeMarkers(DocumentMarker::MarkerType markerType=DocumentMarker::AllMarkers);
    void removeMarkers(NodeImpl*);
    void shiftMarkers(NodeImpl*, unsigned startOffset, int delta, DocumentMarker::MarkerType markerType=DocumentMarker::AllMarkers);

    QValueList<DocumentMarker> markersForNode(NodeImpl*);
    
    // designMode support
    enum InheritedBool { off = false, on = true, inherit };    
    void setDesignMode(InheritedBool value);
    InheritedBool getDesignMode() const;
    bool inDesignMode() const;

    DocumentImpl* parentDocument() const;
    DocumentImpl* topDocument() const;

    int docID() const { return m_docID; }

#if KHTML_XSLT
    void applyXSLTransform(ProcessingInstructionImpl* pi);
    void setTransformSource(void* doc) { m_transformSource = doc; }
    const void* transformSource() { return m_transformSource; }
    PassRefPtr<DocumentImpl> transformSourceDocument() { return m_transformSourceDocument; }
    void setTransformSourceDocument(DocumentImpl *doc) { m_transformSourceDocument = doc; }
#endif

#ifndef KHTML_NO_XBL
    // XBL methods
    XBL::XBLBindingManager* bindingManager() const { return m_bindingManager; }
#endif

    void incDOMTreeVersion() { ++m_domtree_version; }
    unsigned domTreeVersion() const { return m_domtree_version; }

    void setDocType(PassRefPtr<DocumentTypeImpl>);

    void finishedParsing();

protected:
    CSSStyleSelector *m_styleSelector;
    FrameView *m_view;
    QStringList m_state;

    DocLoader *m_docLoader;
    Tokenizer *m_tokenizer;
    QString m_url;
    QString m_baseURL;
    QString m_baseTarget;

    RefPtr<DocumentTypeImpl> m_docType;
    RefPtr<DOMImplementationImpl> m_implementation;

    RefPtr<StyleSheetImpl> m_sheet;
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

    RefPtr<CSSStyleSheetImpl> m_elemSheet;

    bool m_printing;

    ParseMode pMode;
    HTMLMode hMode;

    Color m_textColor;

    RefPtr<NodeImpl> m_focusNode;
    RefPtr<NodeImpl> m_hoverNode;
    RefPtr<NodeImpl> m_activeNode;

    unsigned m_domtree_version;
    
    // ### replace with something more efficient in lookup and insertion
    StringImpl** m_elementNames;
    unsigned short m_elementNameAlloc;
    unsigned short m_elementNameCount;

    StringImpl** m_attrNames;
    unsigned short m_attrNameAlloc;
    unsigned short m_attrNameCount;

    StringImpl** m_namespaceURIs;
    unsigned short m_namespaceURIAlloc;
    unsigned short m_namespaceURICount;

    QPtrList<NodeIteratorImpl> m_nodeIterators;
    RefPtr<AbstractViewImpl> m_defaultView;

    unsigned short m_listenerTypes;
    RefPtr<StyleSheetListImpl> m_styleSheets;
    QPtrList<RegisteredEventListener> m_windowEventListeners;
    QPtrList<NodeImpl> m_maintainsState;

    Color m_linkColor;
    Color m_visitedLinkColor;
    Color m_activeLinkColor;

    String m_preferredStylesheetSet;
    String m_selectedStylesheetSet;

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
    
    String m_title;
    bool m_titleSetExplicitly;
    RefPtr<NodeImpl> m_titleElement;
    
    RenderArena* m_renderArena;

    typedef HashMap<NodeImpl*, QValueList<DocumentMarker>*> MarkerMap;
    MarkerMap m_markers;

    KWQAccObjectCache* m_accCache;
    
    QPtrList<HTMLImageLoader> m_imageLoadEventDispatchSoonList;
    QPtrList<HTMLImageLoader> m_imageLoadEventDispatchingList;
    Timer<DocumentImpl> m_imageLoadEventTimer;

    NodeImpl* m_cssTarget;
    
    bool m_processingLoadEvent;
    double m_startTime;
    bool m_overMinimumLayoutThreshold;
    
#if KHTML_XSLT
    void* m_transformSource;
    RefPtr<DocumentImpl> m_transformSourceDocument;
#endif

#ifndef KHTML_NO_XBL
    XBL::XBLBindingManager* m_bindingManager; // The access point through which documents and elements communicate with XBL.
#endif
    
    typedef HashMap<AtomicStringImpl*, HTMLMapElementImpl*> ImageMapsByName;
    ImageMapsByName m_imageMapsByName;

    String m_policyBaseURL;

    typedef HashSet<NodeImpl*> NodeSet;
    NodeSet m_disconnectedNodesWithEventListeners;

    int m_docID; // A unique document identifier used for things like document-specific mapped attributes.

public:
    bool inPageCache();
    void setInPageCache(bool flag);
    void restoreRenderer(RenderObject*);

    void passwordFieldAdded();
    void passwordFieldRemoved();
    bool hasPasswordField() const;

    void secureFormAdded();
    void secureFormRemoved();
    bool hasSecureForm() const;

    void setShouldCreateRenderers(bool);
    bool shouldCreateRenderers();
    
    void setDecoder(Decoder*);
    Decoder* decoder() const { return m_decoder.get(); }

    QChar backslashAsCurrencySymbol() const;

#if __APPLE__
    void setDashboardRegionsDirty(bool f) { m_dashboardRegionsDirty = f; }
    bool dashboardRegionsDirty() const { return m_dashboardRegionsDirty; }
    bool hasDashboardRegions () const { return m_hasDashboardRegions; }
    void setHasDashboardRegions (bool f) { m_hasDashboardRegions = f; }
    const QValueList<DashboardRegionValue> & dashboardRegions() const;
    void setDashboardRegions (const QValueList<DashboardRegionValue>& regions);
#endif

    void removeAllEventListenersFromAllNodes();

    void registerDisconnectedNodeWithEventListeners(NodeImpl*);
    void unregisterDisconnectedNodeWithEventListeners(NodeImpl*);
    
    void radioButtonChecked(HTMLInputElementImpl* caller, HTMLFormElementImpl* form);
    HTMLInputElementImpl* checkedRadioButtonForGroup(AtomicStringImpl* name, HTMLFormElementImpl* form);
    void removeRadioButtonGroup(AtomicStringImpl* name, HTMLFormElementImpl* form);
    
#if SVG_SUPPORT
    const SVGDocumentExtensions* svgExtensions();
    SVGDocumentExtensions* accessSVGExtensions();
#endif

private:
    void updateTitle();
    void removeAllDisconnectedNodeEventListeners();
    void imageLoadEventTimerFired(Timer<DocumentImpl>*);

    JSEditor *jsEditor();

    JSEditor *m_jsEditor;
    bool relinquishesEditingFocus(NodeImpl*);
    bool acceptsEditingFocus(NodeImpl*);
    void didBeginEditing();
    void didEndEditing();

    mutable String m_domain;
    RenderObject *m_savedRenderer;
    int m_passwordFields;
    int m_secureForms;
    
    RefPtr<Decoder> m_decoder;

    mutable HashMap<AtomicStringImpl*, ElementImpl*> m_elementsById;
    mutable HashCountedSet<AtomicStringImpl*> m_duplicateIds;
    
    HashMap<StringImpl*, ElementImpl*, CaseInsensitiveHash> m_elementsByAccessKey;
    
    InheritedBool m_designMode;
    
    int m_selfOnlyRefCount;
    typedef HashMap<AtomicStringImpl*, HTMLInputElementImpl*> NameToInputMap;
    typedef HashMap<HTMLFormElementImpl*, NameToInputMap*> FormToGroupMap;
    FormToGroupMap m_selectedRadioButtons;
    
#if SVG_SUPPORT
    SVGDocumentExtensions* m_svgExtensions;
#endif
    
#if __APPLE__
    QValueList<DashboardRegionValue> m_dashboardRegions;
    bool m_hasDashboardRegions;
    bool m_dashboardRegionsDirty;
#endif

    bool m_accessKeyMapValid;
    bool m_createRenderers;
    bool m_inPageCache;
};

} //namespace

#endif
