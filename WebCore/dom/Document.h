/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef Document_h
#define Document_h

#include "CachedResourceHandle.h"
#include "CheckedRadioButtons.h"
#include "CollectionCache.h"
#include "CollectionType.h"
#include "Color.h"
#include "ContainerNode.h"
#include "DocumentMarkerController.h"
#include "QualifiedName.h"
#include "ScriptExecutionContext.h"
#include "Timer.h"
#include <wtf/FixedArray.h>
#include <wtf/HashCountedSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

#if USE(JSC)
#include <runtime/WeakGCMap.h>
#endif

namespace WebCore {

class AsyncScriptRunner;
class Attr;
class AXObjectCache;
class CDATASection;
class CachedCSSStyleSheet;
class CachedScript;
class CanvasRenderingContext;
class CharacterData;
class CSSStyleDeclaration;
class CSSStyleSelector;
class CSSStyleSheet;
class Comment;
class Database;
class DOMImplementation;
class DOMSelection;
class DOMWindow;
class DatabaseThread;
class CachedResourceLoader;
class DocumentFragment;
class DocumentType;
class DocumentWeakReference;
class EditingText;
class Element;
class EntityReference;
class Event;
class EventListener;
class Frame;
class FrameView;
class HTMLCanvasElement;
class HTMLCollection;
class HTMLAllCollection;
class HTMLDocument;
class HTMLElement;
class HTMLFormElement;
class HTMLFrameOwnerElement;
class HTMLHeadElement;
class HTMLInputElement;
class HTMLMapElement;
class HitTestRequest;
class HitTestResult;
class InspectorTimelineAgent;
class IntPoint;
class DOMWrapperWorld;
class JSNode;
class MediaCanStartListener;
class MouseEventWithHitTestResults;
class NodeFilter;
class NodeIterator;
class Page;
class PlatformMouseEvent;
class ProcessingInstruction;
class Range;
class RegisteredEventListener;
class RenderArena;
class RenderView;
class ScriptableDocumentParser;
class ScriptElementData;
class SecurityOrigin;
class SerializedScriptValue;
class SegmentedString;
class Settings;
class StyleSheet;
class StyleSheetList;
class Text;
class TextResourceDecoder;
class DocumentParser;
class TreeWalker;
class XMLHttpRequest;

#if ENABLE(SVG)
class SVGDocumentExtensions;
#endif

#if ENABLE(XSLT)
class TransformSource;
#endif

#if ENABLE(XPATH)
class XPathEvaluator;
class XPathExpression;
class XPathNSResolver;
class XPathResult;
#endif

#if ENABLE(DASHBOARD_SUPPORT)
struct DashboardRegionValue;
#endif

typedef int ExceptionCode;

class FormElementKey {
public:
    FormElementKey(AtomicStringImpl* = 0, AtomicStringImpl* = 0);
    ~FormElementKey();
    FormElementKey(const FormElementKey&);
    FormElementKey& operator=(const FormElementKey&);

    AtomicStringImpl* name() const { return m_name; }
    AtomicStringImpl* type() const { return m_type; }

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    FormElementKey(WTF::HashTableDeletedValueType) : m_name(hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return m_name == hashTableDeletedValue(); }

private:
    void ref() const;
    void deref() const;

    static AtomicStringImpl* hashTableDeletedValue() { return reinterpret_cast<AtomicStringImpl*>(-1); }

    AtomicStringImpl* m_name;
    AtomicStringImpl* m_type;
};

inline bool operator==(const FormElementKey& a, const FormElementKey& b)
{
    return a.name() == b.name() && a.type() == b.type();
}

struct FormElementKeyHash {
    static unsigned hash(const FormElementKey&);
    static bool equal(const FormElementKey& a, const FormElementKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct FormElementKeyHashTraits : WTF::GenericHashTraits<FormElementKey> {
    static void constructDeletedValue(FormElementKey& slot) { new (&slot) FormElementKey(WTF::HashTableDeletedValue); }
    static bool isDeletedValue(const FormElementKey& value) { return value.isHashTableDeletedValue(); }
};

enum PageshowEventPersistence {
    PageshowEventNotPersisted = 0,
    PageshowEventPersisted = 1
};

enum StyleSelectorUpdateFlag { RecalcStyleImmediately, DeferRecalcStyle };

class Document : public ContainerNode, public ScriptExecutionContext {
public:
    static PassRefPtr<Document> create(Frame* frame, const KURL& url)
    {
        return adoptRef(new Document(frame, url, false, false));
    }
    static PassRefPtr<Document> createXHTML(Frame* frame, const KURL& url)
    {
        return adoptRef(new Document(frame, url, true, false));
    }
    virtual ~Document();

    using ContainerNode::ref;
    using ContainerNode::deref;

    // Nodes belonging to this document hold "self-only" references -
    // these are enough to keep the document from being destroyed, but
    // not enough to keep it from removing its children. This allows a
    // node that outlives its document to still have a valid document
    // pointer without introducing reference cycles

    void selfOnlyRef()
    {
        ASSERT(!m_deletionHasBegun);
        ++m_selfOnlyRefCount;
    }
    void selfOnlyDeref()
    {
        ASSERT(!m_deletionHasBegun);
        --m_selfOnlyRefCount;
        if (!m_selfOnlyRefCount && !refCount()) {
#ifndef NDEBUG
            m_deletionHasBegun = true;
#endif
            delete this;
        }
    }

    // DOM methods & attributes for Document

    DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(change);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(click);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(contextmenu);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dblclick);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dragenter);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dragover);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dragleave);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(drop);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dragstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(drag);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dragend);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(input);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(invalid);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(keydown);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(keypress);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(keyup);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mousedown);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mousemove);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseout);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseover);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseup);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mousewheel);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(scroll);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(select);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(submit);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(blur);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(focus);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(load);

    // WebKit extensions
    DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecut);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(cut);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecopy);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(copy);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(beforepaste);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(paste);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(reset);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(search);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(selectstart);
#if ENABLE(TOUCH_EVENTS)
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchmove);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchend);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchcancel);
#endif
#if ENABLE(FULLSCREEN_API)
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitfullscreenchange);
#endif

    DocumentType* doctype() const { return m_docType.get(); }

    DOMImplementation* implementation() const;
    
    Element* documentElement() const
    {
        if (!m_documentElement)
            cacheDocumentElement();
        return m_documentElement.get();
    }
    
    virtual PassRefPtr<Element> createElement(const AtomicString& tagName, ExceptionCode&);
    PassRefPtr<DocumentFragment> createDocumentFragment();
    PassRefPtr<Text> createTextNode(const String& data);
    PassRefPtr<Comment> createComment(const String& data);
    PassRefPtr<CDATASection> createCDATASection(const String& data, ExceptionCode&);
    PassRefPtr<ProcessingInstruction> createProcessingInstruction(const String& target, const String& data, ExceptionCode&);
    PassRefPtr<Attr> createAttribute(const String& name, ExceptionCode&);
    PassRefPtr<Attr> createAttributeNS(const String& namespaceURI, const String& qualifiedName, ExceptionCode&, bool shouldIgnoreNamespaceChecks = false);
    PassRefPtr<EntityReference> createEntityReference(const String& name, ExceptionCode&);
    PassRefPtr<Node> importNode(Node* importedNode, bool deep, ExceptionCode&);
    virtual PassRefPtr<Element> createElementNS(const String& namespaceURI, const String& qualifiedName, ExceptionCode&);
    PassRefPtr<Element> createElement(const QualifiedName&, bool createdByParser);
    Element* getElementById(const AtomicString&) const;
    bool hasElementWithId(AtomicStringImpl* id) const;
    bool containsMultipleElementsWithId(const AtomicString& elementId) { return m_duplicateIds.contains(elementId.impl()); }

    /**
     * Retrieve all nodes that intersect a rect in the window's document, until it is fully enclosed by
     * the boundaries of node.
     *
     * @param centerX x reference for the rectangle in CSS pixels
     * @param centerY y reference for the rectangle in CSS pixels
     * @param hPadding How much to expand the rectangle horizontally
     * @param vPadding How much to expand the rectangle vertically
     * @param ignoreClipping whether or not to ignore the root scroll frame when retrieving the element.
     *        If false, this method returns null for coordinates outside of the viewport.
     */
    PassRefPtr<NodeList> nodesFromRect(int centerX, int centerY, unsigned hPadding, unsigned vPadding, bool ignoreClipping) const;
    Element* elementFromPoint(int x, int y) const;
    PassRefPtr<Range> caretRangeFromPoint(int x, int y);

    String readyState() const;

    String defaultCharset() const;
    
    // Synonyms backing similar DOM attributes. Use Document::encoding() to avoid virtual dispatch.
    String inputEncoding() const { return Document::encoding(); }
    String charset() const { return Document::encoding(); }
    String characterSet() const { return Document::encoding(); }

    void setCharset(const String&);

    String contentLanguage() const { return m_contentLanguage; }
    void setContentLanguage(const String& lang) { m_contentLanguage = lang; }

    String xmlEncoding() const { return m_xmlEncoding; }
    String xmlVersion() const { return m_xmlVersion; }
    bool xmlStandalone() const { return m_xmlStandalone; }

    void setXMLEncoding(const String& encoding) { m_xmlEncoding = encoding; } // read-only property, only to be set from XMLDocumentParser
    void setXMLVersion(const String&, ExceptionCode&);
    void setXMLStandalone(bool, ExceptionCode&);

    String documentURI() const { return m_documentURI; }
    void setDocumentURI(const String&);

    virtual KURL baseURI() const;

    PassRefPtr<Node> adoptNode(PassRefPtr<Node> source, ExceptionCode&);

    PassRefPtr<HTMLCollection> images();
    PassRefPtr<HTMLCollection> embeds();
    PassRefPtr<HTMLCollection> plugins(); // an alias for embeds() required for the JS DOM bindings.
    PassRefPtr<HTMLCollection> applets();
    PassRefPtr<HTMLCollection> links();
    PassRefPtr<HTMLCollection> forms();
    PassRefPtr<HTMLCollection> anchors();
    PassRefPtr<HTMLCollection> objects();
    PassRefPtr<HTMLCollection> scripts();
    PassRefPtr<HTMLCollection> windowNamedItems(const String& name);
    PassRefPtr<HTMLCollection> documentNamedItems(const String& name);

    PassRefPtr<HTMLAllCollection> all();

    // Find first anchor with the given name.
    // First searches for an element with the given ID, but if that fails, then looks
    // for an anchor with the given name. ID matching is always case sensitive, but
    // Anchor name matching is case sensitive in strict mode and not case sensitive in
    // quirks mode for historical compatibility reasons.
    Element* findAnchor(const String& name);

    CollectionCache* collectionInfo(CollectionType type)
    {
        ASSERT(type >= FirstUnnamedDocumentCachedType);
        unsigned index = type - FirstUnnamedDocumentCachedType;
        ASSERT(index < NumUnnamedDocumentCachedTypes);
        m_collectionInfo[index].checkConsistency();
        return &m_collectionInfo[index]; 
    }

    CollectionCache* nameCollectionInfo(CollectionType, const AtomicString& name);

    // Other methods (not part of DOM)
    bool isHTMLDocument() const { return m_isHTML; }
    bool isXHTMLDocument() const { return m_isXHTML; }
    virtual bool isImageDocument() const { return false; }
#if ENABLE(SVG)
    virtual bool isSVGDocument() const { return false; }
#else
    static bool isSVGDocument() { return false; }
#endif
    virtual bool isPluginDocument() const { return false; }
    virtual bool isMediaDocument() const { return false; }
#if ENABLE(WML)
    virtual bool isWMLDocument() const { return false; }
#endif
#if ENABLE(XHTMLMP)
    bool isXHTMLMPDocument() const; 
    bool shouldProcessNoscriptElement() const { return m_shouldProcessNoScriptElement; }
    void setShouldProcessNoscriptElement(bool shouldDo) { m_shouldProcessNoScriptElement = shouldDo; }
#endif
    virtual bool isFrameSet() const { return false; }
    
    CSSStyleSelector* styleSelector()
    { 
        if (!m_styleSelector)
            createStyleSelector();
        return m_styleSelector.get();
    }

    Element* getElementByAccessKey(const String& key) const;
    
    /**
     * Updates the pending sheet count and then calls updateStyleSelector.
     */
    void removePendingSheet();

    /**
     * This method returns true if all top-level stylesheets have loaded (including
     * any @imports that they may be loading).
     */
    bool haveStylesheetsLoaded() const
    {
        return m_pendingStylesheets <= 0 || m_ignorePendingStylesheets;
    }

    /**
     * Increments the number of pending sheets.  The <link> elements
     * invoke this to add themselves to the loading list.
     */
    void addPendingSheet() { m_pendingStylesheets++; }

    void addStyleSheetCandidateNode(Node*, bool createdByParser);
    void removeStyleSheetCandidateNode(Node*);

    bool gotoAnchorNeededAfterStylesheetsLoad() { return m_gotoAnchorNeededAfterStylesheetsLoad; }
    void setGotoAnchorNeededAfterStylesheetsLoad(bool b) { m_gotoAnchorNeededAfterStylesheetsLoad = b; }

    /**
     * Called when one or more stylesheets in the document may have been added, removed or changed.
     *
     * Creates a new style selector and assign it to this document. This is done by iterating through all nodes in
     * document (or those before <BODY> in a HTML document), searching for stylesheets. Stylesheets can be contained in
     * <LINK>, <STYLE> or <BODY> elements, as well as processing instructions (XML documents only). A list is
     * constructed from these which is used to create the a new style selector which collates all of the stylesheets
     * found and is used to calculate the derived styles for all rendering objects.
     */
    void styleSelectorChanged(StyleSelectorUpdateFlag);
    void recalcStyleSelector();

    bool usesDescendantRules() const { return m_usesDescendantRules; }
    void setUsesDescendantRules(bool b) { m_usesDescendantRules = b; }
    bool usesSiblingRules() const { return m_usesSiblingRules; }
    void setUsesSiblingRules(bool b) { m_usesSiblingRules = b; }
    bool usesFirstLineRules() const { return m_usesFirstLineRules; }
    void setUsesFirstLineRules(bool b) { m_usesFirstLineRules = b; }
    bool usesFirstLetterRules() const { return m_usesFirstLetterRules; }
    void setUsesFirstLetterRules(bool b) { m_usesFirstLetterRules = b; }
    bool usesBeforeAfterRules() const { return m_usesBeforeAfterRules; }
    void setUsesBeforeAfterRules(bool b) { m_usesBeforeAfterRules = b; }
    bool usesRemUnits() const { return m_usesRemUnits; }
    void setUsesRemUnits(bool b) { m_usesRemUnits = b; }
    bool usesLinkRules() const { return linkColor() != visitedLinkColor() || m_usesLinkRules; }
    void setUsesLinkRules(bool b) { m_usesLinkRules = b; }

    // Machinery for saving and restoring state when you leave and then go back to a page.
    void registerFormElementWithState(Element* e) { m_formElementsWithState.add(e); }
    void unregisterFormElementWithState(Element* e) { m_formElementsWithState.remove(e); }
    Vector<String> formElementsState() const;
    void setStateForNewFormElements(const Vector<String>&);
    bool hasStateForNewFormElements() const;
    bool takeStateForFormElement(AtomicStringImpl* name, AtomicStringImpl* type, String& state);

    FrameView* view() const; // can be NULL
    Frame* frame() const { return m_frame; } // can be NULL
    Page* page() const; // can be NULL
    Settings* settings() const; // can be NULL
#if ENABLE(INSPECTOR)
    InspectorTimelineAgent* inspectorTimelineAgent() const; // can be NULL
    virtual InspectorController* inspectorController() const; // can be NULL
#endif

    PassRefPtr<Range> createRange();

    PassRefPtr<NodeIterator> createNodeIterator(Node* root, unsigned whatToShow,
        PassRefPtr<NodeFilter>, bool expandEntityReferences, ExceptionCode&);

    PassRefPtr<TreeWalker> createTreeWalker(Node* root, unsigned whatToShow, 
        PassRefPtr<NodeFilter>, bool expandEntityReferences, ExceptionCode&);

    // Special support for editing
    PassRefPtr<CSSStyleDeclaration> createCSSStyleDeclaration();
    PassRefPtr<EditingText> createEditingTextNode(const String&);

    virtual void recalcStyle(StyleChange = NoChange);
    bool childNeedsAndNotInStyleRecalc();
    virtual void updateStyleIfNeeded();
    void updateLayout();
    void updateLayoutIgnorePendingStylesheets();
    PassRefPtr<RenderStyle> styleForElementIgnoringPendingStylesheets(Element*);
    PassRefPtr<RenderStyle> styleForPage(int pageIndex);

    // Returns true if page box (margin boxes and page borders) is visible.
    bool isPageBoxVisible(int pageIndex);

    // Returns the preferred page size and margins in pixels, assuming 96
    // pixels per inch. pageSize, marginTop, marginRight, marginBottom,
    // marginLeft must be initialized to the default values that are used if
    // auto is specified.
    void pageSizeAndMarginsInPixels(int pageIndex, IntSize& pageSize, int& marginTop, int& marginRight, int& marginBottom, int& marginLeft);

    static void updateStyleForAllDocuments(); // FIXME: Try to reduce the # of calls to this function.
    CachedResourceLoader* cachedResourceLoader() { return m_cachedResourceLoader.get(); }

    virtual void attach();
    virtual void detach();

    RenderArena* renderArena() { return m_renderArena.get(); }

    RenderView* renderView() const;

    void clearAXObjectCache();
    AXObjectCache* axObjectCache() const;
    
    // to get visually ordered hebrew and arabic pages right
    void setVisuallyOrdered();
    bool visuallyOrdered() const { return m_visuallyOrdered; }

    void open(Document* ownerDocument = 0);
    void implicitOpen();
    void close();
    void implicitClose();
    void cancelParsing();

    void write(const SegmentedString& text, Document* ownerDocument = 0);
    void write(const String& text, Document* ownerDocument = 0);
    void writeln(const String& text, Document* ownerDocument = 0);
    void finishParsing();

    bool wellFormed() const { return m_wellFormed; }

    const KURL& url() const { return m_url; }
    void setURL(const KURL&);

    const KURL& baseURL() const { return m_baseURL; }
    const String& baseTarget() const { return m_baseTarget; }
    void processBaseElement();

    KURL completeURL(const String&) const;

    virtual String userAgent(const KURL&) const;

    CSSStyleSheet* pageUserSheet();
    void clearPageUserSheet();
    void updatePageUserSheet();

    const Vector<RefPtr<CSSStyleSheet> >* pageGroupUserSheets() const;
    void clearPageGroupUserSheets();
    void updatePageGroupUserSheets();

    CSSStyleSheet* elementSheet();
    CSSStyleSheet* mappedElementSheet();
    
    virtual PassRefPtr<DocumentParser> createParser();
    DocumentParser* parser() const { return m_parser.get(); }
    ScriptableDocumentParser* scriptableDocumentParser() const;
    
    bool printing() const { return m_printing; }
    void setPrinting(bool p) { m_printing = p; }

    bool paginatedForScreen() const { return m_paginatedForScreen; }
    void setPaginatedForScreen(bool p) { m_paginatedForScreen = p; }
    
    bool paginated() const { return printing() || paginatedForScreen(); }

    enum CompatibilityMode { QuirksMode, LimitedQuirksMode, NoQuirksMode };

    virtual void setCompatibilityModeFromDoctype() { }
    void setCompatibilityMode(CompatibilityMode m);
    void lockCompatibilityMode() { m_compatibilityModeLocked = true; }
    CompatibilityMode compatibilityMode() const { return m_compatibilityMode; }

    bool inQuirksMode() const { return m_compatibilityMode == QuirksMode; }
    bool inLimitedQuirksMode() const { return m_compatibilityMode == LimitedQuirksMode; }
    bool inNoQuirksMode() const { return m_compatibilityMode == NoQuirksMode; }

    enum ReadyState {
        Loading,
        Interactive,
        Complete
    };
    void setReadyState(ReadyState);
    void setParsing(bool);
    bool parsing() const { return m_bParsing; }
    int minimumLayoutDelay();

    // This method is used by Android.
    void setExtraLayoutDelay(int delay) { m_extraLayoutDelay = delay; }

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
    
    MouseEventWithHitTestResults prepareMouseEvent(const HitTestRequest&, const IntPoint&, const PlatformMouseEvent&);

    StyleSheetList* styleSheets();

    /* Newly proposed CSS3 mechanism for selecting alternate
       stylesheets using the DOM. May be subject to change as
       spec matures. - dwh
    */
    String preferredStylesheetSet() const;
    String selectedStylesheetSet() const;
    void setSelectedStylesheetSet(const String&);

    bool setFocusedNode(PassRefPtr<Node>);
    Node* focusedNode() const { return m_focusedNode.get(); }

    void getFocusableNodes(Vector<RefPtr<Node> >&);
    
    // The m_ignoreAutofocus flag specifies whether or not the document has been changed by the user enough 
    // for WebCore to ignore the autofocus attribute on any form controls
    bool ignoreAutofocus() const { return m_ignoreAutofocus; };
    void setIgnoreAutofocus(bool shouldIgnore = true) { m_ignoreAutofocus = shouldIgnore; };

    void setHoverNode(PassRefPtr<Node>);
    Node* hoverNode() const { return m_hoverNode.get(); }

    void setActiveNode(PassRefPtr<Node>);
    Node* activeNode() const { return m_activeNode.get(); }

    void focusedNodeRemoved();
    void removeFocusedNodeOfSubtree(Node*, bool amongChildrenOnly = false);
    void hoveredNodeDetached(Node*);
    void activeChainNodeDetached(Node*);

    // Updates for :target (CSS3 selector).
    void setCSSTarget(Element*);
    Element* cssTarget() const { return m_cssTarget; }
    
    void scheduleForcedStyleRecalc();
    void scheduleStyleRecalc();
    void unscheduleStyleRecalc();
    void styleRecalcTimerFired(Timer<Document>*);

    void attachNodeIterator(NodeIterator*);
    void detachNodeIterator(NodeIterator*);

    void attachRange(Range*);
    void detachRange(Range*);

    void nodeChildrenChanged(ContainerNode*);
    // nodeChildrenWillBeRemoved is used when removing all node children at once.
    void nodeChildrenWillBeRemoved(ContainerNode*);
    // nodeWillBeRemoved is only safe when removing one node at a time.
    void nodeWillBeRemoved(Node*);

    void textInserted(Node*, unsigned offset, unsigned length);
    void textRemoved(Node*, unsigned offset, unsigned length);
    void textNodesMerged(Text* oldNode, unsigned offset);
    void textNodeSplit(Text* oldNode);

    DOMWindow* defaultView() const { return domWindow(); } 
    DOMWindow* domWindow() const;

    // Helper functions for forwarding DOMWindow event related tasks to the DOMWindow if it exists.
    void setWindowAttributeEventListener(const AtomicString& eventType, PassRefPtr<EventListener>);
    EventListener* getWindowAttributeEventListener(const AtomicString& eventType);
    void dispatchWindowEvent(PassRefPtr<Event>, PassRefPtr<EventTarget> = 0);
    void dispatchWindowLoadEvent();

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
        DOMCHARACTERDATAMODIFIED_LISTENER    = 0x40,
        OVERFLOWCHANGED_LISTENER             = 0x80,
        ANIMATIONEND_LISTENER                = 0x100,
        ANIMATIONSTART_LISTENER              = 0x200,
        ANIMATIONITERATION_LISTENER          = 0x400,
        TRANSITIONEND_LISTENER               = 0x800,
        BEFORELOAD_LISTENER                  = 0x1000,
        TOUCH_LISTENER                       = 0x2000,
        BEFOREPROCESS_LISTENER               = 0x4000
    };

    bool hasListenerType(ListenerType listenerType) const { return (m_listenerTypes & listenerType); }
    void addListenerType(ListenerType listenerType) { m_listenerTypes = m_listenerTypes | listenerType; }
    void addListenerTypeIfNeeded(const AtomicString& eventType);

    CSSStyleDeclaration* getOverrideStyle(Element*, const String& pseudoElt);

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
    Node* nextFocusableNode(Node* start, KeyboardEvent*);

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
    Node* previousFocusableNode(Node* start, KeyboardEvent*);

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
    void processViewport(const String& features);

    // Returns the owning element in the parent document.
    // Returns 0 if this is the top level document.
    HTMLFrameOwnerElement* ownerElement() const;

    String title() const { return m_title; }
    void setTitle(const String&, Element* titleElement = 0);
    void removeTitle(Element* titleElement);

    String cookie(ExceptionCode&) const;
    void setCookie(const String&, ExceptionCode&);

    String referrer() const;

    String domain() const;
    void setDomain(const String& newDomain, ExceptionCode&);

    String lastModified() const;

    // The cookieURL is used to query the cookie database for this document's
    // cookies. For example, if the cookie URL is http://example.com, we'll
    // use the non-Secure cookies for example.com when computing
    // document.cookie.
    //
    // Q: How is the cookieURL different from the document's URL?
    // A: The two URLs are the same almost all the time.  However, if one
    //    document inherits the security context of another document, it
    //    inherits its cookieURL but not its URL.
    //
    const KURL& cookieURL() const { return m_cookieURL; }

    // The firstPartyForCookies is used to compute whether this document
    // appears in a "third-party" context for the purpose of third-party
    // cookie blocking.  The document is in a third-party context if the
    // cookieURL and the firstPartyForCookies are from different hosts.
    //
    // Note: Some ports (including possibly Apple's) only consider the
    //       document in a third-party context if the cookieURL and the
    //       firstPartyForCookies have a different registry-controlled
    //       domain.
    //
    const KURL& firstPartyForCookies() const { return m_firstPartyForCookies; }
    void setFirstPartyForCookies(const KURL& url) { m_firstPartyForCookies = url; }
    
    // The following implements the rule from HTML 4 for what valid names are.
    // To get this right for all the XML cases, we probably have to improve this or move it
    // and make it sensitive to the type of document.
    static bool isValidName(const String&);

    // The following breaks a qualified name into a prefix and a local name.
    // It also does a validity check, and returns false if the qualified name
    // is invalid.  It also sets ExceptionCode when name is invalid.
    static bool parseQualifiedName(const String& qualifiedName, String& prefix, String& localName, ExceptionCode&);
    
    // Checks to make sure prefix and namespace do not conflict (per DOM Core 3)
    static bool hasPrefixNamespaceMismatch(const QualifiedName&);
    
    void addElementById(const AtomicString& elementId, Element *element);
    void removeElementById(const AtomicString& elementId, Element *element);

    void addImageMap(HTMLMapElement*);
    void removeImageMap(HTMLMapElement*);
    HTMLMapElement* getImageMap(const String& url) const;

    HTMLElement* body() const;
    void setBody(PassRefPtr<HTMLElement>, ExceptionCode&);

    HTMLHeadElement* head();

    DocumentMarkerController* markers() const { return m_markers.get(); }

    bool execCommand(const String& command, bool userInterface = false, const String& value = String());
    bool queryCommandEnabled(const String& command);
    bool queryCommandIndeterm(const String& command);
    bool queryCommandState(const String& command);
    bool queryCommandSupported(const String& command);
    String queryCommandValue(const String& command);
    
    // designMode support
    enum InheritedBool { off = false, on = true, inherit };    
    void setDesignMode(InheritedBool value);
    InheritedBool getDesignMode() const;
    bool inDesignMode() const;

    Document* parentDocument() const;
    Document* topDocument() const;

    int docID() const { return m_docID; }
    
    AsyncScriptRunner* asyncScriptRunner() { return m_asyncScriptRunner.get(); }

#if ENABLE(XSLT)
    void applyXSLTransform(ProcessingInstruction* pi);
    PassRefPtr<Document> transformSourceDocument() { return m_transformSourceDocument; }
    void setTransformSourceDocument(Document* doc) { m_transformSourceDocument = doc; }

    void setTransformSource(PassOwnPtr<TransformSource>);
    TransformSource* transformSource() const { return m_transformSource.get(); }
#endif

    void incDOMTreeVersion() { ++m_domTreeVersion; }
    unsigned domTreeVersion() const { return m_domTreeVersion; }

    void setDocType(PassRefPtr<DocumentType>);

#if ENABLE(XPATH)
    // XPathEvaluator methods
    PassRefPtr<XPathExpression> createExpression(const String& expression,
                                                 XPathNSResolver* resolver,
                                                 ExceptionCode& ec);
    PassRefPtr<XPathNSResolver> createNSResolver(Node *nodeResolver);
    PassRefPtr<XPathResult> evaluate(const String& expression,
                                     Node* contextNode,
                                     XPathNSResolver* resolver,
                                     unsigned short type,
                                     XPathResult* result,
                                     ExceptionCode& ec);
#endif // ENABLE(XPATH)
    
    enum PendingSheetLayout { NoLayoutWithPendingSheets, DidLayoutWithPendingSheets, IgnoreLayoutWithPendingSheets };

    bool didLayoutWithPendingStylesheets() const { return m_pendingSheetLayout == DidLayoutWithPendingSheets; }
    
    void setHasNodesWithPlaceholderStyle() { m_hasNodesWithPlaceholderStyle = true; }

    const String& iconURL() const { return m_iconURL; }
    void setIconURL(const String& iconURL, const String& type);

    void setUseSecureKeyboardEntryWhenActive(bool);
    bool useSecureKeyboardEntryWhenActive() const;

    void addNodeListCache() { ++m_numNodeListCaches; }
    void removeNodeListCache() { ASSERT(m_numNodeListCaches > 0); --m_numNodeListCaches; }
    bool hasNodeListCaches() const { return m_numNodeListCaches; }

    void updateFocusAppearanceSoon(bool restorePreviousSelection);
    void cancelFocusAppearanceUpdate();
        
    // FF method for accessing the selection added for compatibility.
    DOMSelection* getSelection() const;
    
    // Extension for manipulating canvas drawing contexts for use in CSS
    CanvasRenderingContext* getCSSCanvasContext(const String& type, const String& name, int width, int height);
    HTMLCanvasElement* getCSSCanvasElement(const String& name);

    bool isDNSPrefetchEnabled() const { return m_isDNSPrefetchEnabled; }
    void parseDNSPrefetchControlHeader(const String&);

    virtual void reportException(const String& errorMessage, int lineNumber, const String& sourceURL);
    virtual void addMessage(MessageSource, MessageType, MessageLevel, const String& message, unsigned lineNumber, const String& sourceURL);
    virtual void postTask(PassOwnPtr<Task>); // Executes the task on context's thread asynchronously.

#if USE(JSC)
    typedef JSC::WeakGCMap<WebCore::Node*, JSNode*> JSWrapperCache;
    typedef HashMap<DOMWrapperWorld*, JSWrapperCache*> JSWrapperCacheMap;
    JSWrapperCacheMap& wrapperCacheMap() { return m_wrapperCacheMap; }
    JSWrapperCache* getWrapperCache(DOMWrapperWorld* world);
    JSWrapperCache* createWrapperCache(DOMWrapperWorld*);
    void destroyWrapperCache(DOMWrapperWorld*);
    void destroyAllWrapperCaches();
#endif

    virtual void finishedParsing();

    bool inPageCache() const { return m_inPageCache; }
    void setInPageCache(bool flag);
    
    // Elements can register themselves for the "documentWillBecomeInactive()" and  
    // "documentDidBecomeActive()" callbacks
    void registerForDocumentActivationCallbacks(Element*);
    void unregisterForDocumentActivationCallbacks(Element*);
    void documentWillBecomeInactive();
    void documentDidBecomeActive();

    void registerForMediaVolumeCallbacks(Element*);
    void unregisterForMediaVolumeCallbacks(Element*);
    void mediaVolumeDidChange();

    void setShouldCreateRenderers(bool);
    bool shouldCreateRenderers();

    void setDecoder(PassRefPtr<TextResourceDecoder>);
    TextResourceDecoder* decoder() const { return m_decoder.get(); }

    String displayStringModifiedByEncoding(const String&) const;
    PassRefPtr<StringImpl> displayStringModifiedByEncoding(PassRefPtr<StringImpl>) const;
    void displayBufferModifiedByEncoding(UChar* buffer, unsigned len) const;

    // Quirk for the benefit of Apple's Dictionary application.
    void setFrameElementsShouldIgnoreScrolling(bool ignore) { m_frameElementsShouldIgnoreScrolling = ignore; }
    bool frameElementsShouldIgnoreScrolling() const { return m_frameElementsShouldIgnoreScrolling; }

#if ENABLE(DASHBOARD_SUPPORT)
    void setDashboardRegionsDirty(bool f) { m_dashboardRegionsDirty = f; }
    bool dashboardRegionsDirty() const { return m_dashboardRegionsDirty; }
    bool hasDashboardRegions () const { return m_hasDashboardRegions; }
    void setHasDashboardRegions(bool f) { m_hasDashboardRegions = f; }
    const Vector<DashboardRegionValue>& dashboardRegions() const;
    void setDashboardRegions(const Vector<DashboardRegionValue>&);
#endif

    virtual void removeAllEventListeners();

    CheckedRadioButtons& checkedRadioButtons() { return m_checkedRadioButtons; }
    
#if ENABLE(SVG)
    const SVGDocumentExtensions* svgExtensions();
    SVGDocumentExtensions* accessSVGExtensions();
#endif

    void initSecurityContext();

    // Explicitly override the security origin for this document.
    // Note: It is dangerous to change the security origin of a document
    //       that already contains content.
    void setSecurityOrigin(SecurityOrigin*);

    void updateURLForPushOrReplaceState(const KURL&);
    void statePopped(SerializedScriptValue*);

    bool processingLoadEvent() const { return m_processingLoadEvent; }

#if ENABLE(DATABASE)
    virtual bool isDatabaseReadOnly() const;
    virtual void databaseExceededQuota(const String& name);
#endif

    virtual bool isContextThread() const;
    virtual bool isJSExecutionTerminated() const { return false; }

    void setUsingGeolocation(bool f) { m_usingGeolocation = f; }
    bool usingGeolocation() const { return m_usingGeolocation; };

#if ENABLE(WML)
    void setContainsWMLContent(bool value) { m_containsWMLContent = value; }
    bool containsWMLContent() const { return m_containsWMLContent; }

    void resetWMLPageState();
    void initializeWMLPageState();
#endif
    
    bool containsValidityStyleRules() const { return m_containsValidityStyleRules; }
    void setContainsValidityStyleRules() { m_containsValidityStyleRules = true; }

    void enqueueEvent(PassRefPtr<Event>);
    void enqueuePageshowEvent(PageshowEventPersistence);
    void enqueueHashchangeEvent(const String& oldURL, const String& newURL);
    void enqueuePopstateEvent(PassRefPtr<SerializedScriptValue> stateObject);

    void addMediaCanStartListener(MediaCanStartListener*);
    void removeMediaCanStartListener(MediaCanStartListener*);
    MediaCanStartListener* takeAnyMediaCanStartListener();

    const QualifiedName& idAttributeName() const { return m_idAttributeName; }
    
#if ENABLE(FULLSCREEN_API)
    bool webkitFullScreen() const { return m_isFullScreen; }
    bool webkitFullScreenKeyboardInputAllowed() const { return m_isFullScreen && m_areKeysEnabledInFullScreen; }
    Element* webkitCurrentFullScreenElement() const { return m_fullScreenElement.get(); }
    void webkitRequestFullScreenForElement(Element*, unsigned short flags);
    void webkitCancelFullScreen();
    
    void webkitWillEnterFullScreenForElement(Element*);
    void webkitDidEnterFullScreenForElement(Element*);
    void webkitWillExitFullScreenForElement(Element*);
    void webkitDidExitFullScreenForElement(Element*);
#endif

    bool writeDisabled() const { return m_writeDisabled; }
    void setWriteDisabled(bool flag) { m_writeDisabled = flag; }

    // Used to allow element that loads data without going through a FrameLoader to delay the 'load' event.
    void incrementLoadEventDelayCount() { ++m_loadEventDelayCount; }
    void decrementLoadEventDelayCount();
    bool isDelayingLoadEvent() const { return m_loadEventDelayCount; }

protected:
    Document(Frame*, const KURL&, bool isXHTML, bool isHTML);

    void clearXMLVersion() { m_xmlVersion = String(); }

private:
    void detachParser();

    typedef void (*ArgumentsCallback)(const String& keyString, const String& valueString, Document*, void* data);
    void processArguments(const String& features, void* data, ArgumentsCallback);

    virtual bool isDocument() const { return true; }
    virtual void removedLastRef();

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    virtual String nodeName() const;
    virtual NodeType nodeType() const;
    virtual bool childTypeAllowed(NodeType);
    virtual PassRefPtr<Node> cloneNode(bool deep);
    virtual bool canReplaceChild(Node* newChild, Node* oldChild);

    virtual void refScriptExecutionContext() { ref(); }
    virtual void derefScriptExecutionContext() { deref(); }

    virtual const KURL& virtualURL() const; // Same as url(), but needed for ScriptExecutionContext to implement it without a performance loss for direct calls.
    virtual KURL virtualCompleteURL(const String&) const; // Same as completeURL() for the same reason as above.

    void initDNSPrefetch();

    String encoding() const;

    void updateTitle();
    void updateFocusAppearanceTimerFired(Timer<Document>*);
    void updateBaseURL();

    void cacheDocumentElement() const;

    void createStyleSelector();

    void pendingEventTimerFired(Timer<Document>*);

    PassRefPtr<NodeList> handleZeroPadding(const HitTestRequest&, HitTestResult&) const;

    OwnPtr<CSSStyleSelector> m_styleSelector;
    bool m_didCalculateStyleSelector;

    Frame* m_frame;
    OwnPtr<CachedResourceLoader> m_cachedResourceLoader;
    RefPtr<DocumentParser> m_parser;
    bool m_wellFormed;

    // Document URLs.
    KURL m_url; // Document.URL: The URL from which this document was retrieved.
    KURL m_baseURL; // Node.baseURI: The URL to use when resolving relative URLs.
    KURL m_baseElementURL; // The URL set by the <base> element.
    KURL m_cookieURL; // The URL to use for cookie access.
    KURL m_firstPartyForCookies; // The policy URL for third-party cookie blocking.

    // Document.documentURI:
    // Although URL-like, Document.documentURI can actually be set to any
    // string by content.  Document.documentURI affects m_baseURL unless the
    // document contains a <base> element, in which case the <base> element
    // takes precedence.
    String m_documentURI;

    String m_baseTarget;

    RefPtr<DocumentType> m_docType;
    mutable RefPtr<DOMImplementation> m_implementation;

    // Track the number of currently loading top-level stylesheets.  Sheets
    // loaded using the @import directive are not included in this count.
    // We use this count of pending sheets to detect when we can begin attaching
    // elements.
    int m_pendingStylesheets;

    // But sometimes you need to ignore pending stylesheet count to
    // force an immediate layout when requested by JS.
    bool m_ignorePendingStylesheets;

    // If we do ignore the pending stylesheet count, then we need to add a boolean
    // to track that this happened so that we can do a full repaint when the stylesheets
    // do eventually load.
    PendingSheetLayout m_pendingSheetLayout;
    
    bool m_hasNodesWithPlaceholderStyle;

    RefPtr<CSSStyleSheet> m_elemSheet;
    RefPtr<CSSStyleSheet> m_mappedElementSheet;
    RefPtr<CSSStyleSheet> m_pageUserSheet;
    mutable OwnPtr<Vector<RefPtr<CSSStyleSheet> > > m_pageGroupUserSheets;
    mutable bool m_pageGroupUserSheetCacheValid;

    bool m_printing;
    bool m_paginatedForScreen;

    bool m_ignoreAutofocus;

    CompatibilityMode m_compatibilityMode;
    bool m_compatibilityModeLocked; // This is cheaper than making setCompatibilityMode virtual.

    Color m_textColor;

    RefPtr<Node> m_focusedNode;
    RefPtr<Node> m_hoverNode;
    RefPtr<Node> m_activeNode;
    mutable RefPtr<Element> m_documentElement;

    unsigned m_domTreeVersion;
    
    HashSet<NodeIterator*> m_nodeIterators;
    HashSet<Range*> m_ranges;

    unsigned short m_listenerTypes;

    RefPtr<StyleSheetList> m_styleSheets; // All of the stylesheets that are currently in effect for our media type and stylesheet set.
    
    typedef ListHashSet<Node*, 32> StyleSheetCandidateListHashSet;
    StyleSheetCandidateListHashSet m_styleSheetCandidateNodes; // All of the nodes that could potentially provide stylesheets to the document (<link>, <style>, <?xml-stylesheet>)

    typedef ListHashSet<Element*, 64> FormElementListHashSet;
    FormElementListHashSet m_formElementsWithState;

    typedef HashMap<FormElementKey, Vector<String>, FormElementKeyHash, FormElementKeyHashTraits> FormElementStateMap;
    FormElementStateMap m_stateForNewFormElements;
    
    Color m_linkColor;
    Color m_visitedLinkColor;
    Color m_activeLinkColor;

    String m_preferredStylesheetSet;
    String m_selectedStylesheetSet;

    bool m_loadingSheet;
    bool m_visuallyOrdered;
    ReadyState m_readyState;
    bool m_bParsing;
    
    Timer<Document> m_styleRecalcTimer;
    bool m_pendingStyleRecalcShouldForce;
    bool m_inStyleRecalc;
    bool m_closeAfterStyleRecalc;

    bool m_usesDescendantRules;
    bool m_usesSiblingRules;
    bool m_usesFirstLineRules;
    bool m_usesFirstLetterRules;
    bool m_usesBeforeAfterRules;
    bool m_usesRemUnits;
    bool m_usesLinkRules;
    bool m_gotoAnchorNeededAfterStylesheetsLoad;
    bool m_isDNSPrefetchEnabled;
    bool m_haveExplicitlyDisabledDNSPrefetch;
    bool m_frameElementsShouldIgnoreScrolling;
    bool m_containsValidityStyleRules;
    bool m_updateFocusAppearanceRestoresSelection;

    // http://www.whatwg.org/specs/web-apps/current-work/#write-neutralised
    bool m_writeDisabled;

    String m_title;
    String m_rawTitle;
    bool m_titleSetExplicitly;
    RefPtr<Element> m_titleElement;

    OwnPtr<RenderArena> m_renderArena;

    mutable AXObjectCache* m_axObjectCache;
    OwnPtr<DocumentMarkerController> m_markers;
    
    Timer<Document> m_updateFocusAppearanceTimer;

    Element* m_cssTarget;
    
    bool m_processingLoadEvent;
    RefPtr<SerializedScriptValue> m_pendingStateObject;
    double m_startTime;
    bool m_overMinimumLayoutThreshold;
    // This is used to increase the minimum delay between re-layouts. It is set
    // using setExtraLayoutDelay to modify the minimum delay used at different
    // points during the lifetime of the Document.
    int m_extraLayoutDelay;
    
    OwnPtr<AsyncScriptRunner> m_asyncScriptRunner;

#if ENABLE(XSLT)
    OwnPtr<TransformSource> m_transformSource;
    RefPtr<Document> m_transformSourceDocument;
#endif

    typedef HashMap<AtomicStringImpl*, HTMLMapElement*> ImageMapsByName;
    ImageMapsByName m_imageMapsByName;

    int m_docID; // A unique document identifier used for things like document-specific mapped attributes.

    String m_xmlEncoding;
    String m_xmlVersion;
    bool m_xmlStandalone;

    String m_contentLanguage;

#if ENABLE(XHTMLMP)
    bool m_shouldProcessNoScriptElement;
#endif

    RenderObject* m_savedRenderer;
    
    RefPtr<TextResourceDecoder> m_decoder;

    // We maintain the invariant that m_duplicateIds is the count of all elements with a given ID
    // excluding the one referenced in m_elementsById, if any. This means it one less than the total count
    // when the first node with a given ID is cached, otherwise the same as the total count.
    mutable HashMap<AtomicStringImpl*, Element*> m_elementsById;
    mutable HashCountedSet<AtomicStringImpl*> m_duplicateIds;
    
    mutable HashMap<StringImpl*, Element*, CaseFoldingHash> m_elementsByAccessKey;
    
    InheritedBool m_designMode;
    
    int m_selfOnlyRefCount;

    CheckedRadioButtons m_checkedRadioButtons;

    typedef HashMap<AtomicStringImpl*, CollectionCache*> NamedCollectionMap;
    FixedArray<CollectionCache, NumUnnamedDocumentCachedTypes> m_collectionInfo;
    FixedArray<NamedCollectionMap, NumNamedDocumentCachedTypes> m_nameCollectionInfo;

#if ENABLE(XPATH)
    RefPtr<XPathEvaluator> m_xpathEvaluator;
#endif
    
#if ENABLE(SVG)
    OwnPtr<SVGDocumentExtensions> m_svgExtensions;
#endif
    
#if ENABLE(DASHBOARD_SUPPORT)
    Vector<DashboardRegionValue> m_dashboardRegions;
    bool m_hasDashboardRegions;
    bool m_dashboardRegionsDirty;
#endif

    HashMap<String, RefPtr<HTMLCanvasElement> > m_cssCanvasElements;

    mutable bool m_accessKeyMapValid;
    bool m_createRenderers;
    bool m_inPageCache;
    String m_iconURL;
    
    HashSet<Element*> m_documentActivationCallbackElements;
    HashSet<Element*> m_mediaVolumeCallbackElements;

    bool m_useSecureKeyboardEntryWhenActive;

    bool m_isXHTML;
    bool m_isHTML;

    unsigned m_numNodeListCaches;

#if USE(JSC)
    JSWrapperCacheMap m_wrapperCacheMap;
    JSWrapperCache* m_normalWorldWrapperCache;
#endif

    bool m_usingGeolocation;

    Timer<Document> m_pendingEventTimer;
    Vector<RefPtr<Event> > m_pendingEventQueue;

#if ENABLE(WML)
    bool m_containsWMLContent;
#endif

    RefPtr<DocumentWeakReference> m_weakReference;

    HashSet<MediaCanStartListener*> m_mediaCanStartListeners;

    QualifiedName m_idAttributeName;
    
#if ENABLE(FULLSCREEN_API)
    bool m_isFullScreen;
    bool m_areKeysEnabledInFullScreen;
    RefPtr<Element> m_fullScreenElement;
#endif

    int m_loadEventDelayCount;
};

inline bool Document::hasElementWithId(AtomicStringImpl* id) const
{
    ASSERT(id);
    return m_elementsById.contains(id) || m_duplicateIds.contains(id);
}
    
inline bool Node::isDocumentNode() const
{
    return this == m_document;
}

// here because it uses a Document method but we really want to inline it
inline Node::Node(Document* document, ConstructionType type)
    : m_document(document)
    , m_previous(0)
    , m_next(0)
    , m_renderer(0)
    , m_nodeFlags(type)
{
    if (m_document)
        m_document->selfOnlyRef();
#if !defined(NDEBUG) || (defined(DUMP_NODE_STATISTICS) && DUMP_NODE_STATISTICS)
    trackForDebugging();
#endif
}

} // namespace WebCore

#endif // Document_h
