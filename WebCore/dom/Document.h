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

#ifndef DOM_Document_h
#define DOM_Document_h

#include "Attr.h"
#include "Color.h"
#include "DeprecatedStringList.h"
#include "DocumentMarker.h"
#include "KURL.h"
#include "StringHash.h"
#include "Timer.h"
#include "Decoder.h"
#include "dom2_traversalimpl.h"
#include <kxmlcore/HashCountedSet.h>
#include "DeprecatedPtrList.h"

class RenderArena;

#ifndef KHTML_NO_XBL
namespace XBL {
    class XBLBindingManager;
}
#endif

namespace WebCore {

    class AccessibilityObjectCache;
    class CDATASection;
    class CSSStyleDeclaration;
    class CSSStyleSelector;
    class CSSStyleSheet;
    class Comment;
    class DOMImplementation;
    class DOMWindow;
    class DocLoader;
    class DocumentFragment;
    class DocumentType;
    class EditingText;
    class Element;
    class EntityReference;
    class Event;
    class EventListener;
    class Frame;
    class FrameView;
    class HTMLCollection;
    class HTMLDocument;
    class HTMLElement;
    class HTMLFormElement;
    class HTMLImageLoader;
    class HTMLInputElement;
    class HTMLMapElement;
    class IntPoint;
    class JSEditor;
    class MouseEventWithHitTestResults;
    class NameNodeList;
    class NodeFilter;
    class NodeIterator;
    class NodeList;
    class PlatformMouseEvent;
    class ProcessingInstruction;
    class Range;
    class RegisteredEventListener;
    class StyleSheet;
    class StyleSheetList;
    class Text;
    class Tokenizer;
    class TreeWalker;

#if __APPLE__
    struct DashboardRegionValue;
#endif

#if SVG_SUPPORT
    class SVGDocumentExtensions;
#endif

class Document : public ContainerNode
{
public:
    Document(DOMImplementation*, FrameView*);
    ~Document();

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

    virtual DocumentType* doctype() const; // returns 0 for HTML documents
    DocumentType* realDocType() const { return m_docType.get(); }

    DOMImplementation* implementation() const;
    virtual Element* documentElement() const;
    virtual PassRefPtr<Element> createElement(const String& tagName, ExceptionCode&);
    PassRefPtr<DocumentFragment> createDocumentFragment ();
    PassRefPtr<Text> createTextNode(const String& data);
    PassRefPtr<Comment> createComment(const String& data);
    PassRefPtr<CDATASection> createCDATASection(const String& data, ExceptionCode&);
    PassRefPtr<ProcessingInstruction> createProcessingInstruction(const String& target, const String& data, ExceptionCode&);
    PassRefPtr<Attr> createAttribute(const String& name, ExceptionCode& ec) { return createAttributeNS(String(), name, ec); }
    PassRefPtr<Attr> createAttributeNS(const String& namespaceURI, const String& qualifiedName, ExceptionCode&);
    PassRefPtr<EntityReference> createEntityReference(const String& name, ExceptionCode&);
    PassRefPtr<Node> importNode(Node* importedNode, bool deep, ExceptionCode&);
    virtual PassRefPtr<Element> createElementNS(const String& namespaceURI, const String& qualifiedName, ExceptionCode&);
    Element* getElementById(const AtomicString&) const;

    Element* elementFromPoint(int x, int y) const;
    String readyState();
    String inputEncoding();
    String defaultCharset();

    String charset() { return inputEncoding(); }
    String characterSet() { return inputEncoding(); }

    void setCharset(const String&);

    PassRefPtr<Node> adoptNode(PassRefPtr<Node> source, ExceptionCode&);
    
    PassRefPtr<NameNodeList> getElementsByName(const String& elementName);

    // Actually part of JSHTMLDocument, but used for giving XML documents a window title as well
    String title() const { return m_title; }
    void setTitle(const String&, Node *titleElement = 0);
    void removeTitle(Node *titleElement);

    PassRefPtr<HTMLCollection> images();
    PassRefPtr<HTMLCollection> embeds();
    PassRefPtr<HTMLCollection> applets();
    PassRefPtr<HTMLCollection> links();
    PassRefPtr<HTMLCollection> forms();
    PassRefPtr<HTMLCollection> anchors();
    PassRefPtr<HTMLCollection> all();
    PassRefPtr<HTMLCollection> objects();
    PassRefPtr<HTMLCollection> windowNamedItems(const String& name);
    PassRefPtr<HTMLCollection> documentNamedItems(const String& name);

    // DOM methods overridden from  parent classes

    virtual String nodeName() const;
    virtual NodeType nodeType() const;

    // Other methods (not part of DOM)
    virtual bool isDocumentNode() const { return true; }
    virtual bool isHTMLDocument() const { return false; }

    CSSStyleSelector* styleSelector() const { return m_styleSelector; }

    Element* getElementByAccessKey(const String& key);
    
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

    DeprecatedString nextState();

    // Query all registered elements for their state
    DeprecatedStringList docState();
    void registerMaintainsState(Node* e) { m_maintainsState.append(e); }
    void deregisterMaintainsState(Node* e) { m_maintainsState.removeRef(e); }

    // Set the state the document should restore to
    void setRestoreState(const DeprecatedStringList& s) { m_state = s; }
    DeprecatedStringList& restoreState( ) { return m_state; }

    FrameView* view() const { return m_view; }
    Frame* frame() const;

    PassRefPtr<Range> createRange();

    PassRefPtr<NodeIterator> createNodeIterator(Node* root, unsigned whatToShow,
        PassRefPtr<NodeFilter>, bool expandEntityReferences, ExceptionCode&);

    PassRefPtr<TreeWalker> createTreeWalker(Node* root, unsigned whatToShow, 
        PassRefPtr<NodeFilter>, bool expandEntityReferences, ExceptionCode&);

    // Special support for editing
    PassRefPtr<CSSStyleDeclaration> createCSSStyleDeclaration();
    PassRefPtr<EditingText> createEditingTextNode(const String&);

    virtual void recalcStyle( StyleChange = NoChange );
    static DeprecatedPtrList<Document> * changedDocuments;
    virtual void updateRendering();
    void updateLayout();
    void updateLayoutIgnorePendingStylesheets();
    static void updateDocumentsRendering();
    DocLoader* docLoader() { return m_docLoader; }

    virtual void attach();
    virtual void detach();

    RenderArena* renderArena() { return m_renderArena; }

    AccessibilityObjectCache* getAccObjectCache() const;
    
    // to get visually ordered hebrew and arabic pages right
    void setVisuallyOrdered();

    void updateSelection();
    
    void open();
    void implicitOpen();
    void close();
    void implicitClose();
    void cancelParsing();

    void write(const String& text);
    void write(const DeprecatedString &text);
    void writeln(const String& text);
    void finishParsing();
    void clear();

    DeprecatedString URL() const { return m_url.isEmpty() ? "about:blank" : m_url; }
    void setURL(const DeprecatedString& url);

    DeprecatedString baseURL() const { return m_baseURL.isEmpty() ? URL() : m_baseURL; }
    void setBaseURL(const DeprecatedString& baseURL) { m_baseURL = baseURL; }

    String baseTarget() const { return m_baseTarget; }
    void setBaseTarget(const String& baseTarget) { m_baseTarget = baseTarget; }

    DeprecatedString completeURL(const DeprecatedString &);
    String completeURL(const String&);

    // from cachedObjectClient
    virtual void setStyleSheet(const String& url, const String& sheetStr);
    void setUserStyleSheet(const String& sheet);
    const String& userStyleSheet() const { return m_usersheet; }
    void setPrintStyleSheet(const String& sheet) { m_printSheet = sheet; }
    const String& printStyleSheet() const { return m_printSheet; }

    CSSStyleSheet* elementSheet();
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
    
    virtual void determineParseMode( const DeprecatedString &str );
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
    
    MouseEventWithHitTestResults prepareMouseEvent(bool readonly, bool active, bool mouseMove, const IntPoint& point, const PlatformMouseEvent&);

    virtual bool childAllowed(Node*);
    virtual bool childTypeAllowed(NodeType);
    virtual PassRefPtr<Node> cloneNode(bool deep);

    StyleSheetList* styleSheets();

    /* Newly proposed CSS3 mechanism for selecting alternate
       stylesheets using the DOM. May be subject to change as
       spec matures. - dwh
    */
    String preferredStylesheetSet();
    String selectedStylesheetSet();
    void setSelectedStylesheetSet(const String&);

    DeprecatedStringList availableStyleSheets() const;

    Node* focusNode() const { return m_focusNode.get(); }
    bool setFocusNode(PassRefPtr<Node>);
    void clearSelectionIfNeeded(Node*);

    Node* hoverNode() const { return m_hoverNode.get(); }
    void setHoverNode(PassRefPtr<Node>);
    void hoveredNodeDetached(Node*);
    void activeChainNodeDetached(Node*);
    
    Node* activeNode() const { return m_activeNode.get(); }
    void setActiveNode(PassRefPtr<Node>);

    // Updates for :target (CSS3 selector).
    void setCSSTarget(Node*);
    Node* getCSSTarget();
    
    void setDocumentChanged(bool);

    void attachNodeIterator(NodeIterator*);
    void detachNodeIterator(NodeIterator*);
    void notifyBeforeNodeRemoval(Node*);
    DOMWindow* defaultView() const;
    PassRefPtr<Event> createEvent(const String& eventType, ExceptionCode&);

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

    CSSStyleDeclaration* getOverrideStyle(Element*, const String& pseudoElt);

    virtual void defaultEventHandler(Event*);
    void handleWindowEvent(Event*, bool useCapture);
    void setHTMLWindowEventListener(const AtomicString &eventType, PassRefPtr<EventListener>);
    EventListener* getHTMLWindowEventListener(const AtomicString &eventType);
    void removeHTMLWindowEventListener(const AtomicString &eventType);

    void setHTMLWindowEventListener(const AtomicString& eventType, Attribute*);

    void addWindowEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    void removeWindowEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
    bool hasWindowEventListener(const AtomicString& eventType);

    PassRefPtr<EventListener> createHTMLEventListener(const String& functionName, const String& code, Node*);
    
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
    Node* nextFocusNode(Node* fromNode);

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
    Node* previousFocusNode(Node* fromNode);

    int nodeAbsIndex(Node*);
    Node* nodeWithAbsIndex(int absIndex);

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
    Element* ownerElement() const;
    
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
    
    void addElementById(const AtomicString& elementId, Element *element);
    void removeElementById(const AtomicString& elementId, Element *element);

    void addImageMap(HTMLMapElement*);
    void removeImageMap(HTMLMapElement*);
    HTMLMapElement* getImageMap(const String& URL) const;

    HTMLElement* body();

    String toString() const;
    
    bool execCommand(const String& command, bool userInterface, const String& value);
    bool queryCommandEnabled(const String& command);
    bool queryCommandIndeterm(const String& command);
    bool queryCommandState(const String& command);
    bool queryCommandSupported(const String& command);
    String queryCommandValue(const String& command);
    
    void addMarker(Range*, DocumentMarker::MarkerType type);
    void addMarker(Node*, DocumentMarker marker);
    void copyMarkers(Node *srcNode, unsigned startOffset, int length, Node *dstNode, int delta, DocumentMarker::MarkerType markerType=DocumentMarker::AllMarkers);
    void removeMarkers(Range*, DocumentMarker::MarkerType markerType=DocumentMarker::AllMarkers);
    void removeMarkers(Node*, unsigned startOffset, int length, DocumentMarker::MarkerType markerType=DocumentMarker::AllMarkers);
    void removeMarkers(DocumentMarker::MarkerType markerType=DocumentMarker::AllMarkers);
    void removeMarkers(Node*);
    void shiftMarkers(Node*, unsigned startOffset, int delta, DocumentMarker::MarkerType markerType=DocumentMarker::AllMarkers);

    DeprecatedValueList<DocumentMarker> markersForNode(Node*);
    
    // designMode support
    enum InheritedBool { off = false, on = true, inherit };    
    void setDesignMode(InheritedBool value);
    InheritedBool getDesignMode() const;
    bool inDesignMode() const;

    Document* parentDocument() const;
    Document* topDocument() const;

    int docID() const { return m_docID; }

#if KHTML_XSLT
    void applyXSLTransform(ProcessingInstruction* pi);
    void setTransformSource(void* doc) { m_transformSource = doc; }
    const void* transformSource() { return m_transformSource; }
    PassRefPtr<Document> transformSourceDocument() { return m_transformSourceDocument; }
    void setTransformSourceDocument(Document *doc) { m_transformSourceDocument = doc; }
#endif

#ifndef KHTML_NO_XBL
    // XBL methods
    XBL::XBLBindingManager* bindingManager() const { return m_bindingManager; }
#endif

    void incDOMTreeVersion() { ++m_domtree_version; }
    unsigned domTreeVersion() const { return m_domtree_version; }

    void setDocType(PassRefPtr<DocumentType>);

    void finishedParsing();

protected:
    CSSStyleSelector *m_styleSelector;
    FrameView *m_view;
    DeprecatedStringList m_state;

    DocLoader *m_docLoader;
    Tokenizer *m_tokenizer;
    DeprecatedString m_url;
    DeprecatedString m_baseURL;
    String m_baseTarget;

    RefPtr<DocumentType> m_docType;
    RefPtr<DOMImplementation> m_implementation;

    RefPtr<StyleSheet> m_sheet;
    String m_usersheet;
    String m_printSheet;
    DeprecatedStringList m_availableSheets;

    // Track the number of currently loading top-level stylesheets.  Sheets
    // loaded using the @import directive are not included in this count.
    // We use this count of pending sheets to detect when we can begin attaching
    // elements.
    int m_pendingStylesheets;

    // But sometimes you need to ignore pending stylesheet count to
    // force an immediate layout when requested by JS.
    bool m_ignorePendingStylesheets;

    RefPtr<CSSStyleSheet> m_elemSheet;

    bool m_printing;

    ParseMode pMode;
    HTMLMode hMode;

    Color m_textColor;

    RefPtr<Node> m_focusNode;
    RefPtr<Node> m_hoverNode;
    RefPtr<Node> m_activeNode;

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

    DeprecatedPtrList<NodeIterator> m_nodeIterators;

    unsigned short m_listenerTypes;
    RefPtr<StyleSheetList> m_styleSheets;
    DeprecatedPtrList<RegisteredEventListener> m_windowEventListeners;
    DeprecatedPtrList<Node> m_maintainsState;

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
    RefPtr<Node> m_titleElement;
    
    RenderArena* m_renderArena;

    typedef HashMap<Node*, DeprecatedValueList<DocumentMarker>*> MarkerMap;
    MarkerMap m_markers;

    mutable AccessibilityObjectCache* m_accCache;
    
    DeprecatedPtrList<HTMLImageLoader> m_imageLoadEventDispatchSoonList;
    DeprecatedPtrList<HTMLImageLoader> m_imageLoadEventDispatchingList;
    Timer<Document> m_imageLoadEventTimer;

    Node* m_cssTarget;
    
    bool m_processingLoadEvent;
    double m_startTime;
    bool m_overMinimumLayoutThreshold;
    
#if KHTML_XSLT
    void* m_transformSource;
    RefPtr<Document> m_transformSourceDocument;
#endif

#ifndef KHTML_NO_XBL
    XBL::XBLBindingManager* m_bindingManager; // The access point through which documents and elements communicate with XBL.
#endif
    
    typedef HashMap<AtomicStringImpl*, HTMLMapElement*> ImageMapsByName;
    ImageMapsByName m_imageMapsByName;

    String m_policyBaseURL;

    typedef HashSet<Node*> NodeSet;
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
    const DeprecatedValueList<DashboardRegionValue> & dashboardRegions() const;
    void setDashboardRegions (const DeprecatedValueList<DashboardRegionValue>& regions);
#endif

    void removeAllEventListenersFromAllNodes();

    void registerDisconnectedNodeWithEventListeners(Node*);
    void unregisterDisconnectedNodeWithEventListeners(Node*);
    
    void radioButtonChecked(HTMLInputElement* caller, HTMLFormElement* form);
    HTMLInputElement* checkedRadioButtonForGroup(AtomicStringImpl* name, HTMLFormElement* form);
    void removeRadioButtonGroup(AtomicStringImpl* name, HTMLFormElement* form);
    
#if SVG_SUPPORT
    const SVGDocumentExtensions* svgExtensions();
    SVGDocumentExtensions* accessSVGExtensions();
#endif

private:
    void updateTitle();
    void removeAllDisconnectedNodeEventListeners();
    void imageLoadEventTimerFired(Timer<Document>*);

    JSEditor *jsEditor();

    JSEditor *m_jsEditor;
    bool relinquishesEditingFocus(Node*);
    bool acceptsEditingFocus(Node*);
    void didBeginEditing();
    void didEndEditing();

    mutable String m_domain;
    RenderObject *m_savedRenderer;
    int m_passwordFields;
    int m_secureForms;
    
    RefPtr<Decoder> m_decoder;

    mutable HashMap<AtomicStringImpl*, Element*> m_elementsById;
    mutable HashCountedSet<AtomicStringImpl*> m_duplicateIds;
    
    HashMap<StringImpl*, Element*, CaseInsensitiveHash> m_elementsByAccessKey;
    
    InheritedBool m_designMode;
    
    int m_selfOnlyRefCount;
    typedef HashMap<AtomicStringImpl*, HTMLInputElement*> NameToInputMap;
    typedef HashMap<HTMLFormElement*, NameToInputMap*> FormToGroupMap;
    FormToGroupMap m_selectedRadioButtons;
    
#if SVG_SUPPORT
    SVGDocumentExtensions* m_svgExtensions;
#endif
    
#if __APPLE__
    DeprecatedValueList<DashboardRegionValue> m_dashboardRegions;
    bool m_hasDashboardRegions;
    bool m_dashboardRegionsDirty;
#endif

    bool m_accessKeyMapValid;
    bool m_createRenderers;
    bool m_inPageCache;
};

} //namespace

#endif
