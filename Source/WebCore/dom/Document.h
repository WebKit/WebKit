/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#pragma once

#include "CSSPropertyNames.h"
#include "CSSRegisteredCustomProperty.h"
#include "CanvasBase.h"
#include "ClientOrigin.h"
#include "ContainerNode.h"
#include "DisabledAdaptations.h"
#include "DocumentEventTiming.h"
#include "FocusOptions.h"
#include "FontSelectorClient.h"
#include "FragmentScriptingPermission.h"
#include "FrameDestructionObserver.h"
#include "FrameIdentifier.h"
#include "FrameLoaderTypes.h"
#include "GraphicsTypes.h"
#include "OrientationNotifier.h"
#include "PageIdentifier.h"
#include "PlatformEvent.h"
#include "PlaybackTargetClientContextIdentifier.h"
#include "ReferrerPolicy.h"
#include "RegistrableDomain.h"
#include "RenderPtr.h"
#include "ReportingClient.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "StringWithDirection.h"
#include "StyleColor.h"
#include "Supplementable.h"
#include "Timer.h"
#include "TreeScope.h"
#include "URLKeepingBlobAlive.h"
#include "UserActionElementSet.h"
#include "ViewportArguments.h"
#include "VisibilityState.h"
#include <wtf/Deque.h>
#include <wtf/FixedVector.h>
#include <wtf/Forward.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/Logger.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/Observer.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomStringHash.h>

#if PLATFORM(IOS_FAMILY)
#include "EventTrackingRegions.h"
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
#include <wtf/ThreadingPrimitives.h>
#endif

namespace JSC {
class CallFrame;
class InputCursor;
}

namespace WTF {
class TextStream;
}

namespace PAL {
class SessionID;
class TextEncoding;
}

namespace WebCore {

class AXObjectCache;
class AppHighlightStorage;
class Attr;
class CanvasBase;
class CDATASection;
class CSSCustomPropertyValue;
class CSSFontSelector;
class CSSStyleDeclaration;
class CSSStyleSheet;
class CachedCSSStyleSheet;
class CachedFrameBase;
class CachedResourceLoader;
class CachedScript;
class CanvasRenderingContext2D;
class CharacterData;
class Comment;
class ConstantPropertyMap;
class DOMImplementation;
class DOMSelection;
class DOMWindow;
class DOMWrapperWorld;
class Database;
class DatabaseThread;
class DeviceMotionClient;
class DeviceMotionController;
class DeviceOrientationAndMotionAccessController;
class DeviceOrientationClient;
class DeviceOrientationController;
class DocumentFontLoader;
class DocumentFragment;
class DocumentLoader;
class DocumentMarkerController;
class DocumentParser;
class DocumentSharedObjectPool;
class DocumentTimeline;
class DocumentTimelinesController;
class DocumentType;
class EditingBehavior;
class Editor;
class EventLoop;
class EventLoopTaskGroup;
class ExtensionStyleSheets;
class FloatQuad;
class FloatRect;
class FontFaceSet;
class FontLoadRequest;
class FormController;
class Frame;
class FrameSelection;
class FrameView;
class FullscreenManager;
class HTMLAllCollection;
class HTMLAttachmentElement;
class HTMLBodyElement;
class HTMLCanvasElement;
class HTMLCollection;
class HTMLDialogElement;
class HTMLDocument;
class HTMLElement;
class HTMLFrameOwnerElement;
class HTMLHeadElement;
class HTMLIFrameElement;
class HTMLImageElement;
class HTMLMapElement;
class HTMLMediaElement;
class HTMLMetaElement;
class HTMLVideoElement;
class HighlightRegister;
class HitTestLocation;
class HitTestRequest;
class HitTestResult;
class IdleCallbackController;
class IdleRequestCallback;
class ImageBitmapRenderingContext;
class IntPoint;
class IntersectionObserver;
class JSNode;
class LayoutPoint;
class LayoutRect;
class LazyLoadImageObserver;
class LiveNodeList;
class Locale;
class Location;
class MediaCanStartListener;
class MediaPlaybackTarget;
class MediaPlaybackTargetClient;
class MediaProducer;
class MediaQueryList;
class MediaQueryMatcher;
class MessagePortChannelProvider;
class ModalContainerObserver;
class MouseEventWithHitTestResults;
class NodeFilter;
class NodeIterator;
class Page;
class PaintWorklet;
class PaintWorkletGlobalScope;
class PlatformMouseEvent;
class ProcessingInstruction;
class QualifiedName;
class Quirks;
class RTCNetworkManager;
class Range;
class Region;
class RenderTreeBuilder;
class RenderView;
class ReportingScope;
class RequestAnimationFrameCallback;
class ResizeObserver;
class SVGDocumentExtensions;
class SVGElement;
class SVGSVGElement;
class SVGUseElement;
class SWClientConnection;
class ScriptModuleLoader;
class ScriptRunner;
class ScriptableDocumentParser;
class ScriptedAnimationController;
class SecurityOrigin;
class SegmentedString;
class SelectorQuery;
class SelectorQueryCache;
class SerializedScriptValue;
class Settings;
class SleepDisabler;
class SpeechRecognition;
class StorageConnection;
class StringCallback;
class StyleSheet;
class StyleSheetContents;
class StyleSheetList;
class Text;
class TextAutoSizing;
class TextManipulationController;
class TextResourceDecoder;
class TransformSource;
class TreeWalker;
class UndoManager;
class VisibilityChangeClient;
class VisitedLinkState;
class WakeLockManager;
class WebAnimation;
class WebGL2RenderingContext;
class WebGLRenderingContext;
class WhitespaceCache;
class WindowEventLoop;
class WindowProxy;
class XPathEvaluator;
class XPathExpression;
class XPathNSResolver;
class XPathResult;

#if ENABLE(CONTENT_CHANGE_OBSERVER)
class ContentChangeObserver;
class DOMTimerHoldingTank;
#endif

#if HAVE(WEBGPU_IMPLEMENTATION)
class GPUCanvasContext;
#endif

struct ApplicationManifest;
struct BoundaryPoint;
struct HighlightRangeData;
struct IntersectionObserverData;
struct SecurityPolicyViolationEventInit;

template<typename> class ExceptionOr;

enum CollectionType;

enum class MediaProducerMediaState : uint32_t;
enum class MediaProducerMediaCaptureKind : uint8_t;
enum class MediaProducerMutedState : uint8_t;
enum class RouteSharingPolicy : uint8_t;
enum class ShouldOpenExternalURLsPolicy : uint8_t;
enum class RenderingUpdateStep : uint32_t;
enum class StyleColorOptions : uint8_t;
enum class MutationObserverOptionType : uint8_t;
enum class ViolationReportType : uint8_t;

using MediaProducerMediaStateFlags = OptionSet<MediaProducerMediaState>;
using MediaProducerMutedStateFlags = OptionSet<MediaProducerMutedState>;
using PlatformDisplayID = uint32_t;

namespace Style {
class Resolver;
class Scope;
class Update;
}

enum PageshowEventPersistence { PageshowEventNotPersisted, PageshowEventPersisted };

enum NodeListInvalidationType {
    DoNotInvalidateOnAttributeChanges,
    InvalidateOnClassAttrChange,
    InvalidateOnIdNameAttrChange,
    InvalidateOnNameAttrChange,
    InvalidateOnForTypeAttrChange,
    InvalidateForFormControls,
    InvalidateOnHRefAttrChange,
    InvalidateOnAnyAttrChange,
};
const int numNodeListInvalidationTypes = InvalidateOnAnyAttrChange + 1;

enum class EventHandlerRemoval { One, All };
using EventTargetSet = HashCountedSet<Node*>;

enum class DocumentCompatibilityMode : unsigned char {
    NoQuirksMode = 1,
    QuirksMode = 1 << 1,
    LimitedQuirksMode = 1 << 2
};

enum DimensionsCheck { WidthDimensionsCheck = 1 << 0, HeightDimensionsCheck = 1 << 1, AllDimensionsCheck = 1 << 2 };

enum class HttpEquivPolicy {
    Enabled,
    DisabledBySettings,
    DisabledByContentDispositionAttachmentSandbox
};

enum class CustomElementNameValidationStatus {
    Valid,
    FirstCharacterIsNotLowercaseASCIILetter,
    ContainsNoHyphen,
    ContainsUppercaseASCIILetter,
    ContainsDisallowedCharacter,
    ConflictsWithStandardElementName
};

using RenderingContext = std::variant<
#if ENABLE(WEBGL)
    RefPtr<WebGLRenderingContext>,
#endif
#if ENABLE(WEBGL2)
    RefPtr<WebGL2RenderingContext>,
#endif
#if HAVE(WEBGPU_IMPLEMENTATION)
    RefPtr<GPUCanvasContext>,
#endif
    RefPtr<ImageBitmapRenderingContext>,
    RefPtr<CanvasRenderingContext2D>
>;

class DocumentParserYieldToken {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT DocumentParserYieldToken(Document&);
    WEBCORE_EXPORT ~DocumentParserYieldToken();

private:
    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
};

class Document
    : public ContainerNode
    , public TreeScope
    , public ScriptExecutionContext
    , public FontSelectorClient
    , public FrameDestructionObserver
    , public Supplementable<Document>
    , public Logger::Observer
    , public CanvasObserver
    , public ReportingClient {
    WTF_MAKE_ISO_ALLOCATED_EXPORT(Document, WEBCORE_EXPORT);
public:
    using EventTarget::weakPtrFactory;
    using EventTarget::WeakValueType;
    using EventTarget::WeakPtrImplType;

    inline static Ref<Document> create(const Settings&, const URL&);
    static Ref<Document> createNonRenderedPlaceholder(Frame&, const URL&);
    static Ref<Document> create(Document&);

    virtual ~Document();

    // Nodes belonging to this document increase referencingNodeCount -
    // these are enough to keep the document from being destroyed, but
    // not enough to keep it from removing its children. This allows a
    // node that outlives its document to still have a valid document
    // pointer without introducing reference cycles.
    void incrementReferencingNodeCount()
    {
        ASSERT(!m_deletionHasBegun);
        ++m_referencingNodeCount;
    }

    void decrementReferencingNodeCount()
    {
        ASSERT(!m_deletionHasBegun || !m_referencingNodeCount);
        --m_referencingNodeCount;
        if (!m_referencingNodeCount && !refCount()) {
#if ASSERT_ENABLED
            m_deletionHasBegun = true;
#endif
            m_refCountAndParentBit = s_refCountIncrement; // Avoid double destruction through use of Ref<T>/RefPtr<T>. (This is a security mitigation in case of programmer error. It will ASSERT in debug builds.)
            delete this;
        }
    }

    unsigned referencingNodeCount() const { return m_referencingNodeCount; }

    void removedLastRef();

    using DocumentsMap = HashMap<ScriptExecutionContextIdentifier, Document*>;
    WEBCORE_EXPORT static DocumentsMap::ValuesIteratorRange allDocuments();
    WEBCORE_EXPORT static DocumentsMap& allDocumentsMap();

    MediaQueryMatcher& mediaQueryMatcher();

    using ContainerNode::ref;
    using ContainerNode::deref;
    using TreeScope::rootNode;

    bool canContainRangeEndPoint() const final { return true; }

    Element* elementForAccessKey(const String& key);
    void invalidateAccessKeyCache();

    ExceptionOr<SelectorQuery&> selectorQueryForString(const String&);
    void clearSelectorQueryCache();

    void setViewportArguments(const ViewportArguments& viewportArguments) { m_viewportArguments = viewportArguments; }
    WEBCORE_EXPORT ViewportArguments viewportArguments() const;

    OptionSet<DisabledAdaptations> disabledAdaptations() const { return m_disabledAdaptations; }
#if ASSERT_ENABLED
    bool didDispatchViewportPropertiesChanged() const { return m_didDispatchViewportPropertiesChanged; }
#endif

    WEBCORE_EXPORT DocumentType* doctype() const;

    WEBCORE_EXPORT DOMImplementation& implementation();
    
    Element* documentElement() const { return m_documentElement.get(); }
    static ptrdiff_t documentElementMemoryOffset() { return OBJECT_OFFSETOF(Document, m_documentElement); }

    WEBCORE_EXPORT Element* activeElement();
    WEBCORE_EXPORT bool hasFocus() const;
    void whenVisible(Function<void()>&&);

    bool hasManifest() const;
    
    WEBCORE_EXPORT ExceptionOr<Ref<Element>> createElementForBindings(const AtomString& tagName);
    WEBCORE_EXPORT Ref<DocumentFragment> createDocumentFragment();
    WEBCORE_EXPORT Ref<Text> createTextNode(String&& data);
    WEBCORE_EXPORT Ref<Comment> createComment(String&& data);
    WEBCORE_EXPORT ExceptionOr<Ref<CDATASection>> createCDATASection(String&& data);
    WEBCORE_EXPORT ExceptionOr<Ref<ProcessingInstruction>> createProcessingInstruction(String&& target, String&& data);
    WEBCORE_EXPORT ExceptionOr<Ref<Attr>> createAttribute(const AtomString& name);
    WEBCORE_EXPORT ExceptionOr<Ref<Attr>> createAttributeNS(const AtomString& namespaceURI, const AtomString& qualifiedName, bool shouldIgnoreNamespaceChecks = false);
    WEBCORE_EXPORT ExceptionOr<Ref<Node>> importNode(Node& nodeToImport, bool deep);
    WEBCORE_EXPORT ExceptionOr<Ref<Element>> createElementNS(const AtomString& namespaceURI, const AtomString& qualifiedName);
    WEBCORE_EXPORT Ref<Element> createElement(const QualifiedName&, bool createdByParser);

    static CustomElementNameValidationStatus validateCustomElementName(const AtomString&);

    WEBCORE_EXPORT RefPtr<Range> caretRangeFromPoint(int x, int y);
    std::optional<BoundaryPoint> caretPositionFromPoint(const LayoutPoint& clientPoint);

    WEBCORE_EXPORT Element* scrollingElementForAPI();
    WEBCORE_EXPORT Element* scrollingElement();

    enum ReadyState { Loading, Interactive,  Complete };
    ReadyState readyState() const { return m_readyState; }

    WEBCORE_EXPORT String defaultCharsetForLegacyBindings() const;

    inline String charset() const;
    WEBCORE_EXPORT String characterSetWithUTF8Fallback() const;
    inline PAL::TextEncoding textEncoding() const;

    inline AtomString encoding() const;

    WEBCORE_EXPORT void setCharset(const String&); // Used by ObjC / GOBject bindings only.

    void setContent(const String&);

    String suggestedMIMEType() const;

    void overrideMIMEType(const String&);
    WEBCORE_EXPORT String contentType() const;

    const AtomString& contentLanguage() const { return m_contentLanguage; }
    void setContentLanguage(const AtomString&);

    const AtomString& effectiveDocumentElementLanguage() const;
    void setDocumentElementLanguage(const AtomString&);
    TextDirection documentElementTextDirection() const { return m_documentElementTextDirection; }
    void setDocumentElementTextDirection(TextDirection textDirection) { m_documentElementTextDirection = textDirection; }

    String xmlEncoding() const { return m_xmlEncoding; }
    String xmlVersion() const { return m_xmlVersion; }
    enum class StandaloneStatus : uint8_t { Unspecified, Standalone, NotStandalone };
    bool xmlStandalone() const { return m_xmlStandalone == StandaloneStatus::Standalone; }
    StandaloneStatus xmlStandaloneStatus() const { return m_xmlStandalone; }
    bool hasXMLDeclaration() const { return m_hasXMLDeclaration; }

    bool shouldPreventEnteringBackForwardCacheForTesting() const { return m_shouldPreventEnteringBackForwardCacheForTesting; }
    void preventEnteringBackForwardCacheForTesting() { m_shouldPreventEnteringBackForwardCacheForTesting = true; }

    void setXMLEncoding(const String& encoding) { m_xmlEncoding = encoding; } // read-only property, only to be set from XMLDocumentParser
    WEBCORE_EXPORT ExceptionOr<void> setXMLVersion(const String&);
    WEBCORE_EXPORT void setXMLStandalone(bool);
    void setHasXMLDeclaration(bool hasXMLDeclaration) { m_hasXMLDeclaration = hasXMLDeclaration; }

    String documentURI() const { return m_documentURI; }
    WEBCORE_EXPORT void setDocumentURI(const String&);

    WEBCORE_EXPORT VisibilityState visibilityState() const;
    void visibilityStateChanged();
    WEBCORE_EXPORT bool hidden() const;

    void setTimerThrottlingEnabled(bool);
    bool isTimerThrottlingEnabled() const { return m_isTimerThrottlingEnabled; }

    void setVisibilityHiddenDueToDismissal(bool);

    WEBCORE_EXPORT ExceptionOr<Ref<Node>> adoptNode(Node& source);

    WEBCORE_EXPORT Ref<HTMLCollection> images();
    WEBCORE_EXPORT Ref<HTMLCollection> embeds();
    WEBCORE_EXPORT Ref<HTMLCollection> plugins(); // an alias for embeds() required for the JS DOM bindings.
    WEBCORE_EXPORT Ref<HTMLCollection> applets();
    WEBCORE_EXPORT Ref<HTMLCollection> links();
    WEBCORE_EXPORT Ref<HTMLCollection> forms();
    WEBCORE_EXPORT Ref<HTMLCollection> anchors();
    WEBCORE_EXPORT Ref<HTMLCollection> scripts();
    Ref<HTMLCollection> all();
    Ref<HTMLCollection> allFilteredByName(const AtomString&);

    Ref<HTMLCollection> windowNamedItems(const AtomString&);
    Ref<HTMLCollection> documentNamedItems(const AtomString&);

    WakeLockManager& wakeLockManager();

    // Other methods (not part of DOM)
    bool isSynthesized() const { return m_isSynthesized; }

    enum class DocumentClass : uint16_t {
        HTML = 1,
        XHTML = 1 << 1,
        Image = 1 << 2,
        Plugin = 1 << 3,
        Media = 1 << 4,
        SVG = 1 << 5,
        Text = 1 << 6,
        XML = 1 << 7,
#if ENABLE(MODEL_ELEMENT)
        Model = 1 << 8,
#endif
        PDF = 1 << 9,
    };

    using DocumentClasses = OptionSet<DocumentClass>;

    bool isHTMLDocument() const { return m_documentClasses.contains(DocumentClass::HTML); }
    bool isXHTMLDocument() const { return m_documentClasses.contains(DocumentClass::XHTML); }
    bool isXMLDocument() const { return m_documentClasses.contains(DocumentClass::XML); }
    bool isImageDocument() const { return m_documentClasses.contains(DocumentClass::Image); }
    bool isSVGDocument() const { return m_documentClasses.contains(DocumentClass::SVG); }
    bool isPluginDocument() const { return m_documentClasses.contains(DocumentClass::Plugin); }
    bool isMediaDocument() const { return m_documentClasses.contains(DocumentClass::Media); }
    bool isTextDocument() const { return m_documentClasses.contains(DocumentClass::Text); }
#if ENABLE(MODEL_ELEMENT)
    bool isModelDocument() const { return m_documentClasses.contains(DocumentClass::Model); }
#endif
    bool isPDFDocument() const { return m_documentClasses.contains(DocumentClass::PDF); }
    bool hasSVGRootNode() const;
    virtual bool isFrameSet() const { return false; }

    static ptrdiff_t documentClassesMemoryOffset() { return OBJECT_OFFSETOF(Document, m_documentClasses); }
    static uint32_t isHTMLDocumentClassFlag() { return static_cast<uint32_t>(DocumentClass::HTML); }

    bool isSrcdocDocument() const { return m_isSrcdocDocument; }

    bool sawElementsInKnownNamespaces() const { return m_sawElementsInKnownNamespaces; }

    Style::Resolver& userAgentShadowTreeStyleResolver();

    CSSFontSelector& fontSelector() { return m_fontSelector; }
    const CSSFontSelector& fontSelector() const { return m_fontSelector; }

    WEBCORE_EXPORT bool haveStylesheetsLoaded() const;
    bool isIgnoringPendingStylesheets() const { return m_ignorePendingStylesheets; }

    WEBCORE_EXPORT StyleSheetList& styleSheets();

    Style::Scope& styleScope() { return *m_styleScope; }
    const Style::Scope& styleScope() const { return *m_styleScope; }
    ExtensionStyleSheets& extensionStyleSheets() { return *m_extensionStyleSheets; }
    const ExtensionStyleSheets& extensionStyleSheets() const { return *m_extensionStyleSheets; }

    bool gotoAnchorNeededAfterStylesheetsLoad() { return m_gotoAnchorNeededAfterStylesheetsLoad; }
    void setGotoAnchorNeededAfterStylesheetsLoad(bool b) { m_gotoAnchorNeededAfterStylesheetsLoad = b; }

    void updateElementsAffectedByMediaQueries();
    void evaluateMediaQueriesAndReportChanges();

    FormController& formController();
    Vector<AtomString> formElementsState() const;
    void setStateForNewFormElements(const Vector<AtomString>&);

    WEBCORE_EXPORT FrameView* view() const; // Can be null.
    WEBCORE_EXPORT Page* page() const; // Can be null.
    const Settings& settings() const { return m_settings.get(); }
    EditingBehavior editingBehavior() const;

    const Quirks& quirks() const { return m_quirks; }

    float deviceScaleFactor() const;

    WEBCORE_EXPORT bool useSystemAppearance() const;
    WEBCORE_EXPORT bool useElevatedUserInterfaceLevel() const;
    WEBCORE_EXPORT bool useDarkAppearance(const RenderStyle*) const;

    OptionSet<StyleColorOptions> styleColorOptions(const RenderStyle*) const;
    CompositeOperator compositeOperatorForBackgroundColor(const Color&, const RenderObject&) const;

    WEBCORE_EXPORT Ref<Range> createRange();

    // The last bool parameter is for ObjC bindings.
    WEBCORE_EXPORT Ref<NodeIterator> createNodeIterator(Node& root, unsigned long whatToShow = 0xFFFFFFFF, RefPtr<NodeFilter>&& = nullptr, bool = false);

    // The last bool parameter is for ObjC bindings.
    WEBCORE_EXPORT Ref<TreeWalker> createTreeWalker(Node& root, unsigned long whatToShow = 0xFFFFFFFF, RefPtr<NodeFilter>&& = nullptr, bool = false);

    // Special support for editing
    WEBCORE_EXPORT Ref<CSSStyleDeclaration> createCSSStyleDeclaration();
    Ref<Text> createEditingTextNode(String&&);

    enum class ResolveStyleType { Normal, Rebuild };
    void resolveStyle(ResolveStyleType = ResolveStyleType::Normal);
    WEBCORE_EXPORT bool updateStyleIfNeeded();
    bool needsStyleRecalc() const;
    unsigned lastStyleUpdateSizeForTesting() const { return m_lastStyleUpdateSizeForTesting; }

    WEBCORE_EXPORT void updateLayout();
    
    // updateLayoutIgnorePendingStylesheets() forces layout even if we are waiting for pending stylesheet loads,
    // so calling this may cause a flash of unstyled content (FOUC).
    enum class RunPostLayoutTasks { Asynchronously, Synchronously };
    WEBCORE_EXPORT void updateLayoutIgnorePendingStylesheets(RunPostLayoutTasks = RunPostLayoutTasks::Asynchronously);

    std::unique_ptr<RenderStyle> styleForElementIgnoringPendingStylesheets(Element&, const RenderStyle* parentStyle, PseudoId = PseudoId::None);

    // Returns true if page box (margin boxes and page borders) is visible.
    WEBCORE_EXPORT bool isPageBoxVisible(int pageIndex);

    // Returns the preferred page size and margins in pixels, assuming 96
    // pixels per inch. pageSize, marginTop, marginRight, marginBottom,
    // marginLeft must be initialized to the default values that are used if
    // auto is specified.
    WEBCORE_EXPORT void pageSizeAndMarginsInPixels(int pageIndex, IntSize& pageSize, int& marginTop, int& marginRight, int& marginBottom, int& marginLeft);

    CachedResourceLoader& cachedResourceLoader() { return m_cachedResourceLoader; }

    void didBecomeCurrentDocumentInFrame();
    void destroyRenderTree();
    void willBeRemovedFromFrame();

    // Override ScriptExecutionContext methods to do additional work
    WEBCORE_EXPORT bool shouldBypassMainWorldContentSecurityPolicy() const final;
    void suspendActiveDOMObjects(ReasonForSuspension) final;
    void resumeActiveDOMObjects(ReasonForSuspension) final;
    void stopActiveDOMObjects() final;

    const Settings::Values& settingsValues() const final { return settings().values(); }

    void suspendDeviceMotionAndOrientationUpdates();
    void resumeDeviceMotionAndOrientationUpdates();

    void suspendFontLoading();

    RenderView* renderView() const { return m_renderView.get(); }
    const RenderStyle* initialContainingBlockStyle() const { return m_initialContainingBlockStyle.get(); } // This may end up differing from renderView()->style() due to adjustments.

    bool renderTreeBeingDestroyed() const { return m_renderTreeBeingDestroyed; }
    bool hasLivingRenderTree() const { return renderView() && !renderTreeBeingDestroyed(); }
    void updateRenderTree(std::unique_ptr<const Style::Update> styleUpdate);
    
    bool updateLayoutIfDimensionsOutOfDate(Element&, DimensionsCheck = AllDimensionsCheck);
    
    inline AXObjectCache* existingAXObjectCache() const;
    WEBCORE_EXPORT AXObjectCache* axObjectCache() const;
    void clearAXObjectCache();

    WEBCORE_EXPORT std::optional<PageIdentifier> pageID() const;
    std::optional<FrameIdentifier> frameID() const;

    // to get visually ordered hebrew and arabic pages right
    void setVisuallyOrdered();
    bool visuallyOrdered() const { return m_visuallyOrdered; }
    
    WEBCORE_EXPORT DocumentLoader* loader() const;

    WEBCORE_EXPORT ExceptionOr<RefPtr<WindowProxy>> openForBindings(DOMWindow& activeWindow, DOMWindow& firstDOMWindow, const String& url, const AtomString& name, const String& features);
    WEBCORE_EXPORT ExceptionOr<Document&> openForBindings(Document* entryDocument, const String&, const String&);

    // FIXME: We should rename this at some point and give back the name 'open' to the HTML specified ones.
    WEBCORE_EXPORT ExceptionOr<void> open(Document* entryDocument = nullptr);
    void implicitOpen();

    WEBCORE_EXPORT ExceptionOr<void> closeForBindings();

    // FIXME: We should rename this at some point and give back the name 'close' to the HTML specified one.
    WEBCORE_EXPORT void close();
    // In some situations (see the code), we ignore document.close().
    // explicitClose() bypass these checks and actually tries to close the
    // input stream.
    void explicitClose();
    // implicitClose() actually does the work of closing the input stream.
    void implicitClose();

    void cancelParsing();

    ExceptionOr<void> write(Document* entryDocument, SegmentedString&&);
    WEBCORE_EXPORT ExceptionOr<void> write(Document* entryDocument, FixedVector<String>&&);
    WEBCORE_EXPORT ExceptionOr<void> writeln(Document* entryDocument, FixedVector<String>&&);

    bool wellFormed() const { return m_wellFormed; }

    const URL& url() const final { return m_url; }
    void setURL(const URL&);
    const URL& urlForBindings() const { return m_url.url().isEmpty() ? aboutBlankURL() : m_url.url(); }

    const URL& creationURL() const { return m_creationURL; }

    // To understand how these concepts relate to one another, please see the
    // comments surrounding their declaration.
    const URL& baseURL() const { return m_baseURL; }
    void setBaseURLOverride(const URL&);
    const URL& baseURLOverride() const { return m_baseURLOverride; }
    const URL& baseElementURL() const { return m_baseElementURL; }
    const AtomString& baseTarget() const { return m_baseTarget; }
    void processBaseElement();

    URL baseURLForComplete(const URL& baseURLOverride) const;
    WEBCORE_EXPORT URL completeURL(const String&, ForceUTF8 = ForceUTF8::No) const final;
    URL completeURL(const String&, const URL& baseURLOverride, ForceUTF8 = ForceUTF8::No) const;

    inline bool shouldMaskURLForBindings(const URL&) const;
    inline const URL& maskedURLForBindingsIfNeeded(const URL&) const;
    static StaticStringImpl& maskedURLStringForBindings();
    static const URL& maskedURLForBindings();

    String userAgent(const URL&) const final;

    void disableEval(const String& errorMessage) final;
    void disableWebAssembly(const String& errorMessage) final;

    IDBClient::IDBConnectionProxy* idbConnectionProxy() final;
    StorageConnection* storageConnection();
    SocketProvider* socketProvider() final;
    RefPtr<RTCDataChannelRemoteHandlerConnection> createRTCDataChannelRemoteHandlerConnection() final;

#if ENABLE(WEB_RTC)
    RTCNetworkManager* rtcNetworkManager() { return m_rtcNetworkManager.get(); }
    WEBCORE_EXPORT void setRTCNetworkManager(Ref<RTCNetworkManager>&&);
#endif

    bool canNavigate(Frame* targetFrame, const URL& destinationURL = URL());

    bool usesStyleBasedEditability() const;
    void setHasElementUsingStyleBasedEditability();
    
    virtual Ref<DocumentParser> createParser();
    DocumentParser* parser() const { return m_parser.get(); }
    ScriptableDocumentParser* scriptableDocumentParser() const;
    
    bool printing() const { return m_printing; }
    void setPrinting(bool p) { m_printing = p; }

    bool paginatedForScreen() const { return m_paginatedForScreen; }
    void setPaginatedForScreen(bool p) { m_paginatedForScreen = p; }
    
    bool paginated() const { return printing() || paginatedForScreen(); }

    void setCompatibilityMode(DocumentCompatibilityMode);
    void lockCompatibilityMode() { m_compatibilityModeLocked = true; }
    static ptrdiff_t compatibilityModeMemoryOffset() { return OBJECT_OFFSETOF(Document, m_compatibilityMode); }

    WEBCORE_EXPORT String compatMode() const;

    bool inQuirksMode() const { return m_compatibilityMode == DocumentCompatibilityMode::QuirksMode; }
    bool inLimitedQuirksMode() const { return m_compatibilityMode == DocumentCompatibilityMode::LimitedQuirksMode; }
    bool inNoQuirksMode() const { return m_compatibilityMode == DocumentCompatibilityMode::NoQuirksMode; }

    void setReadyState(ReadyState);
    void setParsing(bool);
    bool parsing() const { return m_bParsing; }

    bool shouldScheduleLayout() const;
    bool isLayoutPending() const;
#if !LOG_DISABLED
    Seconds timeSinceDocumentCreation() const { return MonotonicTime::now() - m_documentCreationTime; };
#endif

    const Color& themeColor();

    void setTextColor(const Color& color) { m_textColor = color; }
    const Color& textColor() const { return m_textColor; }

    const Color& linkColor() const { return m_linkColor; }
    const Color& visitedLinkColor() const { return m_visitedLinkColor; }
    const Color& activeLinkColor() const { return m_activeLinkColor; }
    void setLinkColor(const Color& c) { m_linkColor = c; }
    void setVisitedLinkColor(const Color& c) { m_visitedLinkColor = c; }
    void setActiveLinkColor(const Color& c) { m_activeLinkColor = c; }
    void resetLinkColor();
    void resetVisitedLinkColor();
    void resetActiveLinkColor();
    VisitedLinkState& visitedLinkState() const { return *m_visitedLinkState; }

    MouseEventWithHitTestResults prepareMouseEvent(const HitTestRequest&, const LayoutPoint&, const PlatformMouseEvent&);
    // Returns whether focus was blocked. A true value does not necessarily mean the element was focused.
    // The element could have already been focused or may not be focusable (e.g. <input disabled>).
    WEBCORE_EXPORT bool setFocusedElement(Element*, const FocusOptions& = { });
    Element* focusedElement() const { return m_focusedElement.get(); }
    bool wasLastFocusByClick() const { return m_latestFocusTrigger == FocusTrigger::Click; }
    void setLatestFocusTrigger(FocusTrigger trigger) { m_latestFocusTrigger = trigger; }
    UserActionElementSet& userActionElements()  { return m_userActionElements; }
    const UserActionElementSet& userActionElements() const { return m_userActionElements; }

    void setFocusNavigationStartingNode(Node*);
    Element* focusNavigationStartingNode(FocusDirection) const;

    void didRejectSyncXHRDuringPageDismissal();
    bool shouldIgnoreSyncXHRs() const;

    enum class NodeRemoval { Node, ChildrenOfNode };
    void adjustFocusedNodeOnNodeRemoval(Node&, NodeRemoval = NodeRemoval::Node);
    void adjustFocusNavigationNodeOnNodeRemoval(Node&, NodeRemoval = NodeRemoval::Node);

    bool isAutofocusProcessed() const { return m_isAutofocusProcessed; }
    void setAutofocusProcessed() { m_isAutofocusProcessed = true; }

    void appendAutofocusCandidate(Element&);
    void clearAutofocusCandidates() { m_autofocusCandidates.clear(); }
    void flushAutofocusCandidates();

    void hoveredElementDidDetach(Element&);
    void elementInActiveChainDidDetach(Element&);

    enum class CaptureChange : uint8_t { Yes, No };
    void updateHoverActiveState(const HitTestRequest&, Element*, CaptureChange = CaptureChange::No);

    // Updates for :target (CSS3 selector).
    void setCSSTarget(Element*);
    Element* cssTarget() const { return m_cssTarget; }

    WEBCORE_EXPORT void scheduleFullStyleRebuild();
    void scheduleStyleRecalc();
    void unscheduleStyleRecalc();
    bool hasPendingStyleRecalc() const;
    bool hasPendingFullStyleRebuild() const;

    void registerNodeListForInvalidation(LiveNodeList&);
    void unregisterNodeListForInvalidation(LiveNodeList&);
    WEBCORE_EXPORT void registerCollection(HTMLCollection&);
    WEBCORE_EXPORT void unregisterCollection(HTMLCollection&);
    void collectionCachedIdNameMap(const HTMLCollection&);
    void collectionWillClearIdNameMap(const HTMLCollection&);
    bool shouldInvalidateNodeListAndCollectionCaches() const;
    bool shouldInvalidateNodeListAndCollectionCachesForAttribute(const QualifiedName& attrName) const;

    template <typename InvalidationFunction>
    void invalidateNodeListAndCollectionCaches(InvalidationFunction);

    void attachNodeIterator(NodeIterator&);
    void detachNodeIterator(NodeIterator&);
    void moveNodeIteratorsToNewDocument(Node& node, Document& newDocument)
    {
        if (!m_nodeIterators.isEmpty())
            moveNodeIteratorsToNewDocumentSlowCase(node, newDocument);
    }

    void attachRange(Range&);
    void detachRange(Range&);

    void updateRangesAfterChildrenChanged(ContainerNode&);
    // nodeChildrenWillBeRemoved is used when removing all node children at once.
    void nodeChildrenWillBeRemoved(ContainerNode&);
    // nodeWillBeRemoved is only safe when removing one node at a time.
    void nodeWillBeRemoved(Node&);
    void parentlessNodeMovedToNewDocument(Node&);

    enum class AcceptChildOperation { Replace, InsertOrAdd };
    bool canAcceptChild(const Node& newChild, const Node* refChild, AcceptChildOperation) const;

    void textInserted(Node&, unsigned offset, unsigned length);
    void textRemoved(Node&, unsigned offset, unsigned length);
    void textNodesMerged(Text& oldNode, unsigned offset);
    void textNodeSplit(Text& oldNode);

    void createDOMWindow();
    void takeDOMWindowFrom(Document&);

    DOMWindow* domWindow() const { return m_domWindow.get(); }
    // In DOM Level 2, the Document's DOMWindow is called the defaultView.
    WEBCORE_EXPORT WindowProxy* windowProxy() const;

    inline bool hasBrowsingContext() const; // Defined in DocumentInlines.h.

    Document& contextDocument() const;
    void setContextDocument(Document& document) { m_contextDocument = document; }
    
    OptionSet<ParserContentPolicy> parserContentPolicy() const { return m_parserContentPolicy; }
    void setParserContentPolicy(OptionSet<ParserContentPolicy> policy) { m_parserContentPolicy = policy; }

    // Helper functions for forwarding DOMWindow event related tasks to the DOMWindow if it exists.
    void setWindowAttributeEventListener(const AtomString& eventType, const QualifiedName& attributeName, const AtomString& value, DOMWrapperWorld&);
    WEBCORE_EXPORT void dispatchWindowEvent(Event&, EventTarget* = nullptr);
    void dispatchWindowLoadEvent();

    WEBCORE_EXPORT ExceptionOr<Ref<Event>> createEvent(const String& eventType);

    // keep track of what types of event listeners are registered, so we don't
    // dispatch events unnecessarily
    // FIXME: Consider using OptionSet.
    enum ListenerType {
        DOMSUBTREEMODIFIED_LISTENER          = 1 << 0,
        DOMNODEINSERTED_LISTENER             = 1 << 1,
        DOMNODEREMOVED_LISTENER              = 1 << 2,
        DOMNODEREMOVEDFROMDOCUMENT_LISTENER  = 1 << 3,
        DOMNODEINSERTEDINTODOCUMENT_LISTENER = 1 << 4,
        DOMCHARACTERDATAMODIFIED_LISTENER    = 1 << 5,
        OVERFLOWCHANGED_LISTENER             = 1 << 6,
        TRANSITIONEND_LISTENER               = 1 << 7,
        SCROLL_LISTENER                      = 1 << 8,
        FORCEWILLBEGIN_LISTENER              = 1 << 9,
        FORCECHANGED_LISTENER                = 1 << 10,
        FORCEDOWN_LISTENER                   = 1 << 11,
        FORCEUP_LISTENER                     = 1 << 12,
        FOCUSIN_LISTENER                     = 1 << 13,
        FOCUSOUT_LISTENER                    = 1 << 14,
    };

    bool hasListenerType(ListenerType listenerType) const { return (m_listenerTypes & listenerType); }
    bool hasListenerTypeForEventType(PlatformEvent::Type) const;
    void addListenerTypeIfNeeded(const AtomString& eventType);

    inline bool hasMutationObserversOfType(MutationObserverOptionType) const;
    bool hasMutationObservers() const { return !m_mutationObserverTypes.isEmpty(); }
    void addMutationObserverTypes(MutationObserverOptions types) { m_mutationObserverTypes.add(types); }

    CSSStyleDeclaration* getOverrideStyle(Element*, const String&) { return nullptr; }

    // Handles an HTTP header equivalent set by a meta tag using <meta http-equiv="..." content="...">. This is called
    // when a meta tag is encountered during document parsing, and also when a script dynamically changes or adds a meta
    // tag. This enables scripts to use meta tags to perform refreshes and set expiry dates in addition to them being
    // specified in an HTML file.
    void processMetaHttpEquiv(const String& equiv, const AtomString& content, bool isInDocumentHead);

#if PLATFORM(IOS_FAMILY)
    void processFormatDetection(const String&);

    // Called when <meta name="apple-mobile-web-app-orientations"> changes.
    void processWebAppOrientations();
#endif

#if ENABLE(CONTENT_CHANGE_OBSERVER)
    WEBCORE_EXPORT ContentChangeObserver& contentChangeObserver();

    DOMTimerHoldingTank* domTimerHoldingTankIfExists() { return m_domTimerHoldingTank.get(); }
    DOMTimerHoldingTank& domTimerHoldingTank();
#endif
    
    void processViewport(const String& features, ViewportArguments::Type origin);
    void processDisabledAdaptations(const String& adaptations);
    void updateViewportArguments();
    void processReferrerPolicy(const String& policy, ReferrerPolicySource);

    void metaElementThemeColorChanged(HTMLMetaElement&);

#if ENABLE(DARK_MODE_CSS)
    void processColorScheme(const String& colorScheme);
#endif

#if ENABLE(APPLICATION_MANIFEST)
    void processApplicationManifest(const ApplicationManifest&);
#endif

    // Returns the owning element in the parent document.
    // Returns nullptr if this is the top level document.
    WEBCORE_EXPORT HTMLFrameOwnerElement* ownerElement() const;

    // Used by DOM bindings; no direction known.
    const String& title() const { return m_title.string; }
    WEBCORE_EXPORT void setTitle(String&&);
    const StringWithDirection& titleWithDirection() const { return m_title; }

    WEBCORE_EXPORT const AtomString& dir() const;
    WEBCORE_EXPORT void setDir(const AtomString&);

    void titleElementAdded(Element& titleElement);
    void titleElementRemoved(Element& titleElement);
    void titleElementTextChanged(Element& titleElement);

    WEBCORE_EXPORT ExceptionOr<String> cookie();
    WEBCORE_EXPORT ExceptionOr<void> setCookie(const String&);

    WEBCORE_EXPORT String referrer();
    String referrerForBindings();

    WEBCORE_EXPORT String domain() const;
    ExceptionOr<void> setDomain(const String& newDomain);

    void overrideLastModified(const std::optional<WallTime>&);
    WEBCORE_EXPORT String lastModified() const;

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
    const URL& cookieURL() const { return m_cookieURL; }
    void setCookieURL(const URL&);

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
    const URL& firstPartyForCookies() const { return m_firstPartyForCookies; }
    void setFirstPartyForCookies(const URL& url) { m_firstPartyForCookies = url; }

    bool isFullyActive() const;

    // The full URL corresponding to the "site for cookies" in the Same-Site Cookies spec.,
    // <https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-00>. It is either
    // the URL of the top-level document or the null URL depending on whether the registrable
    // domain of this document's URL matches the registrable domain of its parent's/opener's
    // URL. For the top-level document, it is set to the document's URL.
    const URL& siteForCookies() const { return m_siteForCookies; }
    void setSiteForCookies(const URL& url) { m_siteForCookies = url; }
    bool isSameSiteForCookies(const URL&) const;

    // The following implements the rule from HTML 4 for what valid names are.
    // To get this right for all the XML cases, we probably have to improve this or move it
    // and make it sensitive to the type of document.
    static bool isValidName(const String&);

    // The following breaks a qualified name into a prefix and a local name.
    // It also does a validity check, and returns an error if the qualified name is invalid.
    static ExceptionOr<std::pair<AtomString, AtomString>> parseQualifiedName(const AtomString& qualifiedName);
    static ExceptionOr<QualifiedName> parseQualifiedName(const AtomString& namespaceURI, const AtomString& qualifiedName);

    // Checks to make sure prefix and namespace do not conflict (per DOM Core 3)
    static bool hasValidNamespaceForElements(const QualifiedName&);
    static bool hasValidNamespaceForAttributes(const QualifiedName&);

    // This is the "HTML body element" as defined by CSSOM View spec, the first body child of the
    // document element. See http://dev.w3.org/csswg/cssom-view/#the-html-body-element.
    WEBCORE_EXPORT HTMLBodyElement* body() const;

    // This is the "body element" as defined by HTML5, the first body or frameset child of the
    // document element. See https://html.spec.whatwg.org/multipage/dom.html#the-body-element-2.
    WEBCORE_EXPORT HTMLElement* bodyOrFrameset() const;
    WEBCORE_EXPORT ExceptionOr<void> setBodyOrFrameset(RefPtr<HTMLElement>&&);

    Location* location() const;

    WEBCORE_EXPORT HTMLHeadElement* head();

    DocumentMarkerController& markers() const { return *m_markers; }

    WEBCORE_EXPORT ExceptionOr<bool> execCommand(const String& command, bool userInterface = false, const String& value = String());
    WEBCORE_EXPORT ExceptionOr<bool> queryCommandEnabled(const String& command);
    WEBCORE_EXPORT ExceptionOr<bool> queryCommandIndeterm(const String& command);
    WEBCORE_EXPORT ExceptionOr<bool> queryCommandState(const String& command);
    WEBCORE_EXPORT ExceptionOr<bool> queryCommandSupported(const String& command);
    WEBCORE_EXPORT ExceptionOr<String> queryCommandValue(const String& command);

    UndoManager& undoManager() const { return m_undoManager.get(); }

    // designMode support
    enum InheritedBool { off = false, on = true, inherit };    
    void setDesignMode(InheritedBool value);
    bool inDesignMode() const;
    WEBCORE_EXPORT String designMode() const;
    WEBCORE_EXPORT void setDesignMode(const String&);

    Document* parentDocument() const;
    WEBCORE_EXPORT Document& topDocument() const;
    bool isTopDocument() const { return &topDocument() == this; }
    
    ScriptRunner& scriptRunner() { return *m_scriptRunner; }
    ScriptModuleLoader& moduleLoader() { return *m_moduleLoader; }

    Element* currentScript() const { return !m_currentScriptStack.isEmpty() ? m_currentScriptStack.last().get() : nullptr; }
    void pushCurrentScript(Element*);
    void popCurrentScript();

    bool shouldDeferAsynchronousScriptsUntilParsingFinishes() const;

    bool supportsPaintTiming() const;

#if ENABLE(XSLT)
    void scheduleToApplyXSLTransforms();
    void applyPendingXSLTransformsNowIfScheduled();
    RefPtr<Document> transformSourceDocument() { return m_transformSourceDocument; }
    void setTransformSourceDocument(Document* document) { m_transformSourceDocument = document; }

    void setTransformSource(std::unique_ptr<TransformSource>);
    TransformSource* transformSource() const { return m_transformSource.get(); }
#endif

    void incDOMTreeVersion() { m_domTreeVersion = ++s_globalTreeVersion; }
    uint64_t domTreeVersion() const { return m_domTreeVersion; }

    WEBCORE_EXPORT String originIdentifierForPasteboard() const;

    // XPathEvaluator methods
    WEBCORE_EXPORT ExceptionOr<Ref<XPathExpression>> createExpression(const String& expression, RefPtr<XPathNSResolver>&&);
    WEBCORE_EXPORT Ref<XPathNSResolver> createNSResolver(Node& nodeResolver);
    WEBCORE_EXPORT ExceptionOr<Ref<XPathResult>> evaluate(const String& expression, Node& contextNode, RefPtr<XPathNSResolver>&&, unsigned short type, XPathResult*);

    bool hasNodesWithNonFinalStyle() const { return m_hasNodesWithNonFinalStyle; }
    void setHasNodesWithNonFinalStyle() { m_hasNodesWithNonFinalStyle = true; }
    bool hasNodesWithMissingStyle() const { return m_hasNodesWithMissingStyle; }
    void setHasNodesWithMissingStyle() { m_hasNodesWithMissingStyle = true; }

    // Extension for manipulating canvas drawing contexts for use in CSS
    std::optional<RenderingContext> getCSSCanvasContext(const String& type, const String& name, int width, int height);
    HTMLCanvasElement* getCSSCanvasElement(const String& name);
    String nameForCSSCanvasElement(const HTMLCanvasElement&) const;

    bool isDNSPrefetchEnabled() const { return m_isDNSPrefetchEnabled; }
    void parseDNSPrefetchControlHeader(const String&);

    WEBCORE_EXPORT void postTask(Task&&) final; // Executes the task on context's thread asynchronously.

    EventLoopTaskGroup& eventLoop() final;
    WindowEventLoop& windowEventLoop();

    ScriptedAnimationController* scriptedAnimationController() { return m_scriptedAnimationController.get(); }
    void suspendScriptedAnimationControllerCallbacks();
    void resumeScriptedAnimationControllerCallbacks();

    void serviceRequestAnimationFrameCallbacks();
    void serviceRequestVideoFrameCallbacks();

    void windowScreenDidChange(PlatformDisplayID);

    void finishedParsing();

    enum BackForwardCacheState { NotInBackForwardCache, AboutToEnterBackForwardCache, InBackForwardCache };

    BackForwardCacheState backForwardCacheState() const { return m_backForwardCacheState; }
    void setBackForwardCacheState(BackForwardCacheState);

    void registerForDocumentSuspensionCallbacks(Element&);
    void unregisterForDocumentSuspensionCallbacks(Element&);

    void documentWillBecomeInactive();
    void suspend(ReasonForSuspension);
    void resume(ReasonForSuspension);

#if ENABLE(VIDEO)
    void registerMediaElement(HTMLMediaElement&);
    void unregisterMediaElement(HTMLMediaElement&);
#endif

    bool audioPlaybackRequiresUserGesture() const;
    bool videoPlaybackRequiresUserGesture() const;
    bool mediaDataLoadsAutomatically() const;

    void privateBrowsingStateDidChange(PAL::SessionID);

    void storageBlockingStateDidChange();

#if ENABLE(VIDEO)
    void registerForCaptionPreferencesChangedCallbacks(HTMLMediaElement&);
    void unregisterForCaptionPreferencesChangedCallbacks(HTMLMediaElement&);
    void captionPreferencesChanged();
    void setMediaElementShowingTextTrack(const HTMLMediaElement&);
    void clearMediaElementShowingTextTrack();
    void updateTextTrackRepresentationImageIfNeeded();
#endif

    void registerForVisibilityStateChangedCallbacks(VisibilityChangeClient&);
    void unregisterForVisibilityStateChangedCallbacks(VisibilityChangeClient&);

    WEBCORE_EXPORT void setShouldCreateRenderers(bool);
    bool shouldCreateRenderers();

    void setDecoder(RefPtr<TextResourceDecoder>&&);
    TextResourceDecoder* decoder() const { return m_decoder.get(); }

    WEBCORE_EXPORT String displayStringModifiedByEncoding(const String&) const;

    void updateEventRegions();

    void invalidateRenderingDependentRegions();
    void invalidateEventRegionsForFrame(HTMLFrameOwnerElement&);

    void invalidateEventListenerRegions();

    void removeAllEventListeners() final;

    WEBCORE_EXPORT const SVGDocumentExtensions* svgExtensions();
    WEBCORE_EXPORT SVGDocumentExtensions& accessSVGExtensions();

    void initSecurityContext();
    void initContentSecurityPolicy();

    void inheritPolicyContainerFrom(const PolicyContainer&) final;

    void updateURLForPushOrReplaceState(const URL&);
    void statePopped(Ref<SerializedScriptValue>&&);

    bool processingLoadEvent() const { return m_processingLoadEvent; }
    bool loadEventFinished() const { return m_loadEventFinished; }

    bool isContextThread() const final;
    bool isSecureContext() const final;
    bool isJSExecutionForbidden() const final { return false; }

    void queueTaskToDispatchEvent(TaskSource, Ref<Event>&&);
    void queueTaskToDispatchEventOnWindow(TaskSource, Ref<Event>&&);
    void enqueueOverflowEvent(Ref<Event>&&);
    void dispatchPageshowEvent(PageshowEventPersistence);
    WEBCORE_EXPORT void enqueueSecurityPolicyViolationEvent(SecurityPolicyViolationEventInit&&);
    void enqueueHashchangeEvent(const String& oldURL, const String& newURL);
    void dispatchPopstateEvent(RefPtr<SerializedScriptValue>&& stateObject);

    WEBCORE_EXPORT void addMediaCanStartListener(MediaCanStartListener&);
    WEBCORE_EXPORT void removeMediaCanStartListener(MediaCanStartListener&);
    MediaCanStartListener* takeAnyMediaCanStartListener();

    using DisplayChangedObserver = WTF::Observer<void(PlatformDisplayID)>;
    void addDisplayChangedObserver(const DisplayChangedObserver&);

#if ENABLE(FULLSCREEN_API)
    FullscreenManager& fullscreenManager() { return m_fullscreenManager; }
    const FullscreenManager& fullscreenManager() const { return m_fullscreenManager; }
#endif

#if ENABLE(POINTER_LOCK)
    WEBCORE_EXPORT void exitPointerLock();
#endif

    // Used to allow element that loads data without going through a FrameLoader to delay the 'load' event.
    void incrementLoadEventDelayCount() { ++m_loadEventDelayCount; }
    void decrementLoadEventDelayCount();
    bool isDelayingLoadEvent() const { return m_loadEventDelayCount; }
    void checkCompleted();

#if ENABLE(IOS_TOUCH_EVENTS)
#include <WebKitAdditions/DocumentIOS.h>
#endif

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
    DeviceMotionController& deviceMotionController() const;
    DeviceOrientationController& deviceOrientationController() const;
    WEBCORE_EXPORT void simulateDeviceOrientationChange(double alpha, double beta, double gamma);
#endif

#if ENABLE(DEVICE_ORIENTATION)
    DeviceOrientationAndMotionAccessController& deviceOrientationAndMotionAccessController();
#endif

    WEBCORE_EXPORT double monotonicTimestamp() const;
    const DocumentEventTiming& eventTiming() const { return m_eventTiming; }

    int requestAnimationFrame(Ref<RequestAnimationFrameCallback>&&);
    void cancelAnimationFrame(int id);

    int requestIdleCallback(Ref<IdleRequestCallback>&&, Seconds timeout);
    void cancelIdleCallback(int id);
    IdleCallbackController* idleCallbackController() { return m_idleCallbackController.get(); }

    EventTarget* errorEventTarget() final;
    void logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<Inspector::ScriptCallStack>&&) final;

    void initDNSPrefetch();

    void didAddWheelEventHandler(Node&);
    void didRemoveWheelEventHandler(Node&, EventHandlerRemoval = EventHandlerRemoval::One);

    MonotonicTime lastHandledUserGestureTimestamp() const { return m_lastHandledUserGestureTimestamp; }
    bool hasHadUserInteraction() const { return static_cast<bool>(m_lastHandledUserGestureTimestamp); }
    void updateLastHandledUserGestureTimestamp(MonotonicTime);
    bool processingUserGestureForMedia() const;
    bool hasRecentUserInteractionForNavigationFromJS() const;
    void userActivatedMediaFinishedPlaying() { m_userActivatedMediaFinishedPlayingTimestamp = MonotonicTime::now(); }

    void setUserDidInteractWithPage(bool userDidInteractWithPage) { ASSERT(isTopDocument()); m_userDidInteractWithPage = userDidInteractWithPage; }
    bool userDidInteractWithPage() const { ASSERT(isTopDocument()); return m_userDidInteractWithPage; }

    // Used for testing. Count handlers in the main document, and one per frame which contains handlers.
    WEBCORE_EXPORT unsigned wheelEventHandlerCount() const;
    WEBCORE_EXPORT unsigned touchEventHandlerCount() const;

    WEBCORE_EXPORT void startTrackingStyleRecalcs();
    WEBCORE_EXPORT unsigned styleRecalcCount() const;

#if ENABLE(TOUCH_EVENTS)
    bool hasTouchEventHandlers() const { return m_touchEventTargets.get() ? m_touchEventTargets->size() : false; }
    bool touchEventTargetsContain(Node& node) const { return m_touchEventTargets ? m_touchEventTargets->contains(&node) : false; }
#else
    bool hasTouchEventHandlers() const { return false; }
    bool touchEventTargetsContain(Node&) const { return false; }
#endif
#if ENABLE(TOUCH_ACTION_REGIONS)
    bool mayHaveElementsWithNonAutoTouchAction() const { return m_mayHaveElementsWithNonAutoTouchAction; }
    void setMayHaveElementsWithNonAutoTouchAction() { m_mayHaveElementsWithNonAutoTouchAction = true; }
#endif
#if ENABLE(EDITABLE_REGION)
    bool mayHaveEditableElements() const { return m_mayHaveEditableElements; }
    void setMayHaveEditableElements() { m_mayHaveEditableElements = true; }
#endif

    bool mayHaveRenderedSVGRootElements() const { return m_mayHaveRenderedSVGRootElements; }
    void setMayHaveRenderedSVGRootElements() { m_mayHaveRenderedSVGRootElements = true; }

    bool mayHaveRenderedSVGForeignObjects() const { return m_mayHaveRenderedSVGForeignObjects; }
    void setMayHaveRenderedSVGForeignObjects() { m_mayHaveRenderedSVGForeignObjects = true; }

    void didAddTouchEventHandler(Node&);
    void didRemoveTouchEventHandler(Node&, EventHandlerRemoval = EventHandlerRemoval::One);

    void didRemoveEventTargetNode(Node&);

    const EventTargetSet* touchEventTargets() const
    {
#if ENABLE(TOUCH_EVENTS)
        return m_touchEventTargets.get();
#else
        return nullptr;
#endif
    }

    bool hasWheelEventHandlers() const { return m_wheelEventTargets.get() ? m_wheelEventTargets->size() : false; }
    const EventTargetSet* wheelEventTargets() const { return m_wheelEventTargets.get(); }

    using RegionFixedPair = std::pair<Region, bool>;
    RegionFixedPair absoluteEventRegionForNode(Node&);
    RegionFixedPair absoluteRegionForEventTargets(const EventTargetSet*);

    LayoutRect absoluteEventHandlerBounds(bool&) final;

    bool visualUpdatesAllowed() const { return m_visualUpdatesAllowed; }

    bool isInDocumentWrite() { return m_writeRecursionDepth > 0; }

    void suspendScheduledTasks(ReasonForSuspension);
    void resumeScheduledTasks(ReasonForSuspension);

    void convertAbsoluteToClientQuads(Vector<FloatQuad>&, const RenderStyle&);
    void convertAbsoluteToClientRects(Vector<FloatRect>&, const RenderStyle&);
    void convertAbsoluteToClientRect(FloatRect&, const RenderStyle&);

    bool hasActiveParser();
    void incrementActiveParserCount() { ++m_activeParserCount; }
    void decrementActiveParserCount();

    std::unique_ptr<DocumentParserYieldToken> createParserYieldToken()
    {
        return makeUnique<DocumentParserYieldToken>(*this);
    }

    bool hasActiveParserYieldToken() const { return m_parserYieldTokenCount; }

    DocumentSharedObjectPool* sharedObjectPool() { return m_sharedObjectPool.get(); }

    void invalidateMatchedPropertiesCacheAndForceStyleRecalc();

    void didRemoveAllPendingStylesheet();

    bool inStyleRecalc() const { return m_inStyleRecalc; }
    bool inRenderTreeUpdate() const { return m_inRenderTreeUpdate; }
    bool isResolvingContainerQueries() const { return m_isResolvingContainerQueries; }
    bool isResolvingContainerQueriesForSelfOrAncestor() const;
    bool isResolvingTreeStyle() const { return m_isResolvingTreeStyle; }
    void setIsResolvingTreeStyle(bool);

    void updateTextRenderer(Text&, unsigned offsetOfReplacedText, unsigned lengthOfReplacedText);
    void updateSVGRenderer(SVGElement&);

    // Return a Locale for the default locale if the argument is null or empty.
    Locale& getCachedLocale(const AtomString& locale = nullAtom());

    const Document* templateDocument() const;
    Document& ensureTemplateDocument();
    void setTemplateDocumentHost(Document* templateDocumentHost) { m_templateDocumentHost = templateDocumentHost; }
    Document* templateDocumentHost() { return m_templateDocumentHost.get(); }

    Ref<DocumentFragment> documentFragmentForInnerOuterHTML();

    void didAssociateFormControl(Element&);
    bool hasDisabledFieldsetElement() const { return m_disabledFieldsetElementsCount; }
    void addDisabledFieldsetElement() { m_disabledFieldsetElementsCount++; }
    void removeDisabledFieldsetElement() { ASSERT(m_disabledFieldsetElementsCount); m_disabledFieldsetElementsCount--; }

    bool hasDataListElements() const { return m_dataListElementCount; }
    void incrementDataListElementCount() { ++m_dataListElementCount; }
    void decrementDataListElementCount() { ASSERT(m_dataListElementCount); --m_dataListElementCount; }

    void getParserLocation(String& url, unsigned& line, unsigned& column) const;

    WEBCORE_EXPORT void addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&&) final;

    // The following addConsoleMessage function is deprecated.
    // Callers should try to create the ConsoleMessage themselves.
    WEBCORE_EXPORT void addConsoleMessage(MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier = 0) final;

    // The following addMessage function is deprecated.
    // Callers should try to create the ConsoleMessage themselves.
    void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<Inspector::ScriptCallStack>&&, JSC::JSGlobalObject* = nullptr, unsigned long requestIdentifier = 0) final;

    SecurityOrigin& securityOrigin() const { return *SecurityContext::securityOrigin(); }
    SecurityOrigin& topOrigin() const final { return topDocument().securityOrigin(); }
    ClientOrigin clientOrigin() const { return { topOrigin().data(), securityOrigin().data() }; }

    inline bool isSameOriginAsTopDocument() const;
    bool shouldForceNoOpenerBasedOnCOOP() const;

    WEBCORE_EXPORT const CrossOriginOpenerPolicy& crossOriginOpenerPolicy() const final;

    void willLoadScriptElement(const URL&);
    void willLoadFrameElement(const URL&);

    Ref<FontFaceSet> fonts();

    void ensurePlugInsInjectedScript(DOMWrapperWorld&);

    void setVisualUpdatesAllowedByClient(bool);

#if ENABLE(WEB_CRYPTO)
    bool wrapCryptoKey(const Vector<uint8_t>& key, Vector<uint8_t>& wrappedKey) final;
    bool unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, Vector<uint8_t>& key) final;
#endif

    void setHasStyleWithViewportUnits() { m_hasStyleWithViewportUnits = true; }
    bool hasStyleWithViewportUnits() const { return m_hasStyleWithViewportUnits; }
    void updateViewportUnitsOnResize();

    WEBCORE_EXPORT void setNeedsDOMWindowResizeEvent();
    void setNeedsVisualViewportResize();
    void runResizeSteps();

    void addPendingScrollEventTarget(ContainerNode&);
    void setNeedsVisualViewportScrollEvent();
    void runScrollSteps();

    void invalidateScrollbars();

    WEBCORE_EXPORT void addAudioProducer(MediaProducer&);
    WEBCORE_EXPORT void removeAudioProducer(MediaProducer&);
    void setActiveSpeechRecognition(SpeechRecognition*);
    MediaProducerMediaStateFlags mediaState() const { return m_mediaState; }
    void noteUserInteractionWithMediaElement();
    inline bool isCapturing() const;
    WEBCORE_EXPORT void updateIsPlayingMedia();
    void pageMutedStateDidChange();

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void addPlaybackTargetPickerClient(MediaPlaybackTargetClient&);
    void removePlaybackTargetPickerClient(MediaPlaybackTargetClient&);
    void showPlaybackTargetPicker(MediaPlaybackTargetClient&, bool, RouteSharingPolicy, const String&);
    void playbackTargetPickerClientStateDidChange(MediaPlaybackTargetClient&, MediaProducerMediaStateFlags);

    void setPlaybackTarget(PlaybackTargetClientContextIdentifier, Ref<MediaPlaybackTarget>&&);
    void playbackTargetAvailabilityDidChange(PlaybackTargetClientContextIdentifier, bool);
    void setShouldPlayToPlaybackTarget(PlaybackTargetClientContextIdentifier, bool);
    void playbackTargetPickerWasDismissed(PlaybackTargetClientContextIdentifier);
#endif

    ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicyToPropagate() const;
    bool shouldEnforceContentDispositionAttachmentSandbox() const;
    void applyContentDispositionAttachmentSandbox();

    void addDynamicMediaQueryDependentImage(HTMLImageElement&);
    void removeDynamicMediaQueryDependentImage(HTMLImageElement&);

    void scheduleRenderingUpdate(OptionSet<RenderingUpdateStep>);

    void addIntersectionObserver(IntersectionObserver&);
    void removeIntersectionObserver(IntersectionObserver&);
    unsigned numberOfIntersectionObservers() const { return m_intersectionObservers.size(); }
    void updateIntersectionObservations();
    void scheduleInitialIntersectionObservationUpdate();
    IntersectionObserverData& ensureIntersectionObserverData();
    IntersectionObserverData* intersectionObserverDataIfExists() { return m_intersectionObserverData.get(); }

    void addResizeObserver(ResizeObserver&);
    void removeResizeObserver(ResizeObserver&);
    unsigned numberOfResizeObservers() const { return m_resizeObservers.size(); }
    bool hasResizeObservers();
    // Return the minDepth of the active observations.
    size_t gatherResizeObservations(size_t deeperThan);
    void deliverResizeObservations();
    bool hasSkippedResizeObservations() const;
    void setHasSkippedResizeObservations(bool);
    void updateResizeObservations(Page&);

#if ENABLE(MEDIA_STREAM)
    void setHasCaptureMediaStreamTrack() { m_hasHadCaptureMediaStreamTrack = true; }
    bool hasHadCaptureMediaStreamTrack() const { return m_hasHadCaptureMediaStreamTrack; }
    void stopMediaCapture(MediaProducerMediaCaptureKind);
    void mediaStreamCaptureStateChanged();
#endif

// FIXME: Find a better place for this functionality.
#if ENABLE(TELEPHONE_NUMBER_DETECTION)
    // These functions provide a two-level setting:
    //    - A user-settable wantsTelephoneNumberParsing (at the Page / WebView level)
    //    - A read-only telephoneNumberParsingAllowed which is set by the
    //      document if it has the appropriate meta tag.
    //    - isTelephoneNumberParsingEnabled() == isTelephoneNumberParsingAllowed() && page()->settings()->isTelephoneNumberParsingEnabled()
    WEBCORE_EXPORT bool isTelephoneNumberParsingAllowed() const;
    WEBCORE_EXPORT bool isTelephoneNumberParsingEnabled() const;
#endif

    using ContainerNode::setAttributeEventListener;
    void setAttributeEventListener(const AtomString& eventType, const QualifiedName& attributeName, const AtomString& value, DOMWrapperWorld& isolatedWorld);

    DOMSelection* getSelection();

    void didInsertInDocumentShadowRoot(ShadowRoot&);
    void didRemoveInDocumentShadowRoot(ShadowRoot&);
    const ListHashSet<ShadowRoot*>& inDocumentShadowRoots() const { return m_inDocumentShadowRoots; }

    void attachToCachedFrame(CachedFrameBase&);
    void detachFromCachedFrame(CachedFrameBase&);

    ConstantPropertyMap& constantProperties() const { return *m_constantPropertyMap; }

    void orientationChanged(int orientation);
    OrientationNotifier& orientationNotifier() { return m_orientationNotifier; }

    WEBCORE_EXPORT const AtomString& bgColor() const;
    WEBCORE_EXPORT void setBgColor(const AtomString&);
    WEBCORE_EXPORT const AtomString& fgColor() const;
    WEBCORE_EXPORT void setFgColor(const AtomString&);
    WEBCORE_EXPORT const AtomString& alinkColor() const;
    WEBCORE_EXPORT void setAlinkColor(const AtomString&);
    WEBCORE_EXPORT const AtomString& linkColorForBindings() const;
    WEBCORE_EXPORT void setLinkColorForBindings(const AtomString&);
    WEBCORE_EXPORT const AtomString& vlinkColor() const;
    WEBCORE_EXPORT void setVlinkColor(const AtomString&);

    // Per https://html.spec.whatwg.org/multipage/obsolete.html#dom-document-clear, this method does nothing.
    void clear() { }
    // Per https://html.spec.whatwg.org/multipage/obsolete.html#dom-document-captureevents, this method does nothing.
    void captureEvents() { }
    // Per https://html.spec.whatwg.org/multipage/obsolete.html#dom-document-releaseevents, this method does nothing.
    void releaseEvents() { }

#if ENABLE(TEXT_AUTOSIZING)
    TextAutoSizing& textAutoSizing();
#endif

    Logger& logger();
    WEBCORE_EXPORT static const Logger& sharedLogger();

    WEBCORE_EXPORT void setConsoleMessageListener(RefPtr<StringCallback>&&); // For testing.

    void updateAnimationsAndSendEvents();
    WEBCORE_EXPORT DocumentTimeline& timeline();
    DocumentTimeline* existingTimeline() const { return m_timeline.get(); }
    Vector<RefPtr<WebAnimation>> getAnimations();
    Vector<RefPtr<WebAnimation>> matchingAnimations(const Function<bool(Element&)>&);
    DocumentTimelinesController* timelinesController() const { return m_timelinesController.get(); }
    WEBCORE_EXPORT DocumentTimelinesController& ensureTimelinesController();
    void keyframesRuleDidChange(const String& name);

    void addTopLayerElement(Element&);
    void removeTopLayerElement(Element&);
    const ListHashSet<Ref<Element>>& topLayerElements() const { return m_topLayerElements; }
    bool hasTopLayerElement() const { return !m_topLayerElements.isEmpty(); }

    HTMLDialogElement* activeModalDialog() const;

    WhitespaceCache& whitespaceCache() { return m_whitespaceCache; }

#if ENABLE(ATTACHMENT_ELEMENT)
    void registerAttachmentIdentifier(const String&, const HTMLImageElement&);
    void didInsertAttachmentElement(HTMLAttachmentElement&);
    void didRemoveAttachmentElement(HTMLAttachmentElement&);
    WEBCORE_EXPORT RefPtr<HTMLAttachmentElement> attachmentForIdentifier(const String&) const;
    const HashMap<String, Ref<HTMLAttachmentElement>>& attachmentElementsByIdentifier() const { return m_attachmentIdentifierToElementMap; }
#endif

#if ENABLE(SERVICE_WORKER)
    void setServiceWorkerConnection(SWClientConnection*);
    void updateServiceWorkerClientData() final;
    WEBCORE_EXPORT void navigateFromServiceWorker(const URL&, CompletionHandler<void(bool)>&&);
#endif

#if ENABLE(VIDEO)
    void forEachMediaElement(const Function<void(HTMLMediaElement&)>&);
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    bool handlingTouchEvent() const { return m_handlingTouchEvent; }
#endif

#if ENABLE(TRACKING_PREVENTION)
    WEBCORE_EXPORT bool hasRequestedPageSpecificStorageAccessWithUserInteraction(const RegistrableDomain&);
    WEBCORE_EXPORT void setHasRequestedPageSpecificStorageAccessWithUserInteraction(const RegistrableDomain&);
    WEBCORE_EXPORT void wasLoadedWithDataTransferFromPrevalentResource();
    void downgradeReferrerToRegistrableDomain();
#endif

    void registerArticleElement(Element&);
    void unregisterArticleElement(Element&);
    void updateMainArticleElementAfterLayout();
    bool hasMainArticleElement() const { return !!m_mainArticleElement; }

    const CSSRegisteredCustomPropertySet& getCSSRegisteredCustomPropertySet() const { return m_CSSRegisteredPropertySet; }
    bool registerCSSProperty(CSSRegisteredCustomProperty&&);

    const FixedVector<CSSPropertyID>& exposedComputedCSSPropertyIDs();

#if ENABLE(CSS_PAINTING_API)
    PaintWorklet& ensurePaintWorklet();
    PaintWorkletGlobalScope* paintWorkletGlobalScopeForName(const String& name);
    void setPaintWorkletGlobalScopeForName(const String& name, Ref<PaintWorkletGlobalScope>&&);
#endif

    WEBCORE_EXPORT bool isRunningUserScripts() const;
    WEBCORE_EXPORT void setAsRunningUserScripts();

    void frameWasDisconnectedFromOwner();

    WEBCORE_EXPORT bool hitTest(const HitTestRequest&, HitTestResult&);
    bool hitTest(const HitTestRequest&, const HitTestLocation&, HitTestResult&);
#if ASSERT_ENABLED
    bool inHitTesting() const { return m_inHitTesting; }
#endif

    MessagePortChannelProvider& messagePortChannelProvider();

#if USE(SYSTEM_PREVIEW)
    WEBCORE_EXPORT void dispatchSystemPreviewActionEvent(const SystemPreviewInfo&, const String& message);
#endif

#if ENABLE(PICTURE_IN_PICTURE_API)
    HTMLVideoElement* pictureInPictureElement() const;
    void setPictureInPictureElement(HTMLVideoElement*);
#endif

    WEBCORE_EXPORT TextManipulationController& textManipulationController();
    TextManipulationController* textManipulationControllerIfExists() { return m_textManipulationController.get(); }

    bool hasHighlight() const;
    HighlightRegister* highlightRegisterIfExists() { return m_highlightRegister.get(); }
    HighlightRegister& highlightRegister();
    void updateHighlightPositions();

    HighlightRegister* fragmentHighlightRegisterIfExists() { return m_fragmentHighlightRegister.get(); }
    HighlightRegister& fragmentHighlightRegister();
        
#if ENABLE(APP_HIGHLIGHTS)
    HighlightRegister* appHighlightRegisterIfExists() { return m_appHighlightRegister.get(); }
    WEBCORE_EXPORT HighlightRegister& appHighlightRegister();

    WEBCORE_EXPORT AppHighlightStorage& appHighlightStorage();
    AppHighlightStorage* appHighlightStorageIfExists() const { return m_appHighlightStorage.get(); };
#endif

    bool allowsContentJavaScript() const;

    LazyLoadImageObserver& lazyLoadImageObserver();

    void setHasVisuallyNonEmptyCustomContent() { m_hasVisuallyNonEmptyCustomContent = true; }
    bool hasVisuallyNonEmptyCustomContent() const { return m_hasVisuallyNonEmptyCustomContent; }
    void enqueuePaintTimingEntryIfNeeded();

    Editor& editor() { return m_editor; }
    const Editor& editor() const { return m_editor; }
    FrameSelection& selection() { return m_selection; }
    const FrameSelection& selection() const { return m_selection; }
        
    void setFragmentDirective(const String& fragmentDirective) { m_fragmentDirective = fragmentDirective; }
    const String& fragmentDirective() const { return m_fragmentDirective; }

    void prepareCanvasesForDisplayIfNeeded();
    void clearCanvasPreparation(HTMLCanvasElement&);
    void canvasChanged(CanvasBase&, const std::optional<FloatRect>&) final;
    void canvasResized(CanvasBase&) final { };
    void canvasDestroyed(CanvasBase&) final;

    bool contains(const Node& node) const { return this == &node.treeScope() && node.isConnected(); }
    bool contains(const Node* node) const { return node && contains(*node); }

    WEBCORE_EXPORT JSC::VM& vm() final;

    String debugDescription() const;

    URL fallbackBaseURL() const;

    WEBCORE_EXPORT ModalContainerObserver* modalContainerObserver();
    ModalContainerObserver* modalContainerObserverIfExists() const;

    void createNewIdentifier();

    WEBCORE_EXPORT bool hasElementWithPendingUserAgentShadowTreeUpdate(Element&) const;
    void addElementWithPendingUserAgentShadowTreeUpdate(Element&);
    WEBCORE_EXPORT void removeElementWithPendingUserAgentShadowTreeUpdate(Element&);

    std::optional<PAL::SessionID> sessionID() const final;

    ReportingScope& reportingScope() const { return m_reportingScope.get(); }
    WEBCORE_EXPORT String endpointURIForToken(const String&) const final;

    bool hasSleepDisabler() const { return !!m_sleepDisabler; }

    void notifyReportObservers(Ref<Report>&&) final;
    void sendReportToEndpoints(const URL& baseURL, const Vector<String>& endpointURIs, const Vector<String>& endpointTokens, Ref<FormData>&& report, ViolationReportType) final;
    String httpUserAgent() const final;

    // This should be used over the settings lazy loading image flag due to a quirk, which may occur causing website images to fail to load properly.
    bool lazyImageLoadingEnabled() const;

protected:
    enum ConstructionFlags { Synthesized = 1, NonRenderedPlaceholder = 1 << 1 };
    WEBCORE_EXPORT Document(Frame*, const Settings&, const URL&, DocumentClasses = { }, unsigned constructionFlags = 0, ScriptExecutionContextIdentifier = { });

    void clearXMLVersion() { m_xmlVersion = String(); }

    virtual Ref<Document> cloneDocumentWithoutChildren() const;

private:
    friend class DocumentParserYieldToken;
    friend class Node;
    friend class ThrowOnDynamicMarkupInsertionCountIncrementer;
    friend class IgnoreOpensDuringUnloadCountIncrementer;
    friend class IgnoreDestructiveWriteCountIncrementer;

    void updateTitleElement(Element& changingTitleElement);
    void willDetachPage() final;
    void frameDestroyed() final;

    void commonTeardown();

    RenderObject* renderer() const = delete;
    void setRenderer(RenderObject*) = delete;

    void createRenderTree();
    void detachParser();

    DocumentEventTiming* documentEventTimingFromNavigationTiming();

    // ScriptExecutionContext
    CSSFontSelector* cssFontSelector() final { return m_fontSelector.ptr(); }
    std::unique_ptr<FontLoadRequest> fontLoadRequest(const String&, bool, bool, LoadedFromOpaqueSource) final;
    void beginLoadingFontSoon(FontLoadRequest&) final;

    // FontSelectorClient
    void fontsNeedUpdate(FontSelector&) final;

    bool isDocument() const final { return true; }

    void childrenChanged(const ChildChange&) final;

    String nodeName() const final;
    NodeType nodeType() const final;
    bool childTypeAllowed(NodeType) const final;
    Ref<Node> cloneNodeInternal(Document&, CloningOperation) final;
    void cloneDataFromDocument(const Document&);

    void refScriptExecutionContext() final { ref(); }
    void derefScriptExecutionContext() final { deref(); }

    Seconds minimumDOMTimerInterval() const final;

    Seconds domTimerAlignmentInterval(bool hasReachedMaxNestingLevel) const final;

    void updateTitleFromTitleElement();
    void updateTitle(const StringWithDirection&);
    void updateBaseURL();

    WeakPtr<HTMLMetaElement, WeakPtrImplWithEventTargetData> determineActiveThemeColorMetaElement();
    void themeColorChanged();

    void invalidateAccessKeyCacheSlowCase();
    void buildAccessKeyCache();

    void moveNodeIteratorsToNewDocumentSlowCase(Node&, Document&);

    void intersectionObserversInitialUpdateTimerFired();

    void loadEventDelayTimerFired();

    void pendingTasksTimerFired();
    bool isCookieAverse() const;

    void detachFromFrame();

    template<CollectionType> Ref<HTMLCollection> ensureCachedCollection();

    void dispatchDisabledAdaptationsDidChangeForMainFrame();

    void setVisualUpdatesAllowed(ReadyState);
    void setVisualUpdatesAllowed(bool);
    void visualUpdatesSuppressionTimerFired();

    void addListenerType(ListenerType listenerType) { m_listenerTypes |= listenerType; }

    void didAssociateFormControlsTimerFired();

    void wheelEventHandlersChanged(Node* = nullptr);

    HttpEquivPolicy httpEquivPolicy() const;
    AXObjectCache* existingAXObjectCacheSlow() const;

    bool shouldMaskURLForBindingsInternal(const URL&) const;

    // DOM Cookies caching.
    const String& cachedDOMCookies() const { return m_cachedDOMCookies; }
    void setCachedDOMCookies(const String&);
    bool isDOMCookieCacheValid() const { return m_cookieCacheExpiryTimer.isActive(); }
    void invalidateDOMCookieCache();
    void didLoadResourceSynchronously(const URL&) final;

    bool canNavigateInternal(Frame& targetFrame);
    bool isNavigationBlockedByThirdPartyIFrameRedirectBlocking(Frame& targetFrame, const URL& destinationURL);

#if USE(QUICK_LOOK)
    bool shouldEnforceQuickLookSandbox() const;
    void applyQuickLookSandbox();
#endif

    bool shouldEnforceHTTP09Sandbox() const;

    void platformSuspendOrStopActiveDOMObjects();

    void collectRangeDataFromRegister(Vector<WeakPtr<HighlightRangeData>>&, const HighlightRegister&);

    bool isBodyPotentiallyScrollable(HTMLBodyElement&);

    void didLogMessage(const WTFLogChannel&, WTFLogLevel, Vector<JSONLogValue>&&) final;
    static void configureSharedLogger();

    void addToDocumentsMap();
    void removeFromDocumentsMap();

    Style::Update& ensurePendingRenderTreeUpdate();

    NotificationClient* notificationClient() final;

    void updateSleepDisablerIfNeeded();

    const Ref<const Settings> m_settings;

    UniqueRef<Quirks> m_quirks;

    RefPtr<DOMWindow> m_domWindow;
    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_contextDocument;
    OptionSet<ParserContentPolicy> m_parserContentPolicy;

    Ref<CachedResourceLoader> m_cachedResourceLoader;
    RefPtr<DocumentParser> m_parser;

    unsigned m_parserYieldTokenCount { 0 };

    // Document URLs.
    URLKeepingBlobAlive m_url; // Document.URL: The URL from which this document was retrieved.
    URL m_creationURL; // https://html.spec.whatwg.org/multipage/webappapis.html#concept-environment-creation-url.
    URL m_baseURL; // Node.baseURI: The URL to use when resolving relative URLs.
    URL m_baseURLOverride; // An alternative base URL that takes precedence over m_baseURL (but not m_baseElementURL).
    URL m_baseElementURL; // The URL set by the <base> element.
    URL m_cookieURL; // The URL to use for cookie access.
    URL m_firstPartyForCookies; // The policy URL for third-party cookie blocking.
    URL m_siteForCookies; // The policy URL for Same-Site cookies.

    // Document.documentURI:
    // Although URL-like, Document.documentURI can actually be set to any
    // string by content.  Document.documentURI affects m_baseURL unless the
    // document contains a <base> element, in which case the <base> element
    // takes precedence.
    //
    // This property is read-only from JavaScript, but writable from Objective C.
    String m_documentURI;

    AtomString m_baseTarget;

    // MIME type of the document in case it was cloned or created by XHR.
    String m_overriddenMIMEType;

    std::unique_ptr<DOMImplementation> m_implementation;

    RefPtr<Node> m_focusNavigationStartingNode;
    Deque<WeakPtr<Element, WeakPtrImplWithEventTargetData>> m_autofocusCandidates;
    RefPtr<Element> m_focusedElement;
    RefPtr<Element> m_hoveredElement;
    RefPtr<Element> m_activeElement;
    RefPtr<Element> m_documentElement;
    UserActionElementSet m_userActionElements;

    uint64_t m_domTreeVersion;
    static uint64_t s_globalTreeVersion;

    mutable String m_uniqueIdentifier;

    HashSet<NodeIterator*> m_nodeIterators;
    HashSet<Range*> m_ranges;

    std::unique_ptr<Style::Scope> m_styleScope;
    std::unique_ptr<ExtensionStyleSheets> m_extensionStyleSheets;
    RefPtr<StyleSheetList> m_styleSheetList;

    std::unique_ptr<FormController> m_formController;

    Color m_cachedThemeColor;
    std::optional<Vector<WeakPtr<HTMLMetaElement, WeakPtrImplWithEventTargetData>>> m_metaThemeColorElements;
    WeakPtr<HTMLMetaElement, WeakPtrImplWithEventTargetData> m_activeThemeColorMetaElement;
    Color m_applicationManifestThemeColor;

    Color m_textColor { Color::black };
    Color m_linkColor;
    Color m_visitedLinkColor;
    Color m_activeLinkColor;
    const std::unique_ptr<VisitedLinkState> m_visitedLinkState;

    StringWithDirection m_title;
    StringWithDirection m_rawTitle;
    RefPtr<Element> m_titleElement;

    std::unique_ptr<AXObjectCache> m_axObjectCache;
    const std::unique_ptr<DocumentMarkerController> m_markers;
    
    Timer m_styleRecalcTimer;

    std::unique_ptr<Style::Update> m_pendingRenderTreeUpdate;

    Element* m_cssTarget { nullptr };

    std::unique_ptr<LazyLoadImageObserver> m_lazyLoadImageObserver;

#if !LOG_DISABLED
    MonotonicTime m_documentCreationTime;
#endif
    std::unique_ptr<ScriptRunner> m_scriptRunner;
    std::unique_ptr<ScriptModuleLoader> m_moduleLoader;

    Vector<RefPtr<Element>> m_currentScriptStack;

#if ENABLE(XSLT)
    void applyPendingXSLTransformsTimerFired();

    std::unique_ptr<TransformSource> m_transformSource;
    RefPtr<Document> m_transformSourceDocument;
    Timer m_applyPendingXSLTransformsTimer;
    bool m_hasPendingXSLTransforms { false };
#endif

    String m_xmlEncoding;
    String m_xmlVersion;
    StandaloneStatus m_xmlStandalone { StandaloneStatus::Unspecified };
    bool m_hasXMLDeclaration { false };

    AtomString m_contentLanguage;
    AtomString m_documentElementLanguage;
    TextDirection m_documentElementTextDirection;

    RefPtr<TextResourceDecoder> m_decoder;

    HashSet<LiveNodeList*> m_listsInvalidatedAtDocument;
    HashSet<HTMLCollection*> m_collectionsInvalidatedAtDocument;
    unsigned m_nodeListAndCollectionCounts[numNodeListInvalidationTypes];

    RefPtr<XPathEvaluator> m_xpathEvaluator;

    std::unique_ptr<SVGDocumentExtensions> m_svgExtensions;

    // Collection of canvas objects that need to do work after they've
    // rendered but before compositing, for the next frame. The set is
    // cleared after they've been called.
    WeakHashSet<HTMLCanvasElement, WeakPtrImplWithEventTargetData> m_canvasesNeedingDisplayPreparation;

#if ENABLE(DARK_MODE_CSS)
    OptionSet<ColorScheme> m_colorScheme;
    bool m_allowsColorSchemeTransformations { true };
#endif

    HashMap<String, RefPtr<HTMLCanvasElement>> m_cssCanvasElements;

    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_documentSuspensionCallbackElements;

#if ENABLE(VIDEO)
    WeakHashSet<HTMLMediaElement, WeakPtrImplWithEventTargetData> m_mediaElements;
#endif

#if ENABLE(VIDEO)
    WeakHashSet<HTMLMediaElement, WeakPtrImplWithEventTargetData> m_captionPreferencesChangedElements;
    WeakPtr<HTMLMediaElement, WeakPtrImplWithEventTargetData> m_mediaElementShowingTextTrack;
#endif

    WeakPtr<Element, WeakPtrImplWithEventTargetData> m_mainArticleElement;
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_articleElements;

    WeakHashSet<VisibilityChangeClient> m_visibilityStateCallbackClients;

    std::unique_ptr<HashMap<String, WeakPtr<Element, WeakPtrImplWithEventTargetData>, ASCIICaseInsensitiveHash>> m_accessKeyCache;

    std::unique_ptr<ConstantPropertyMap> m_constantPropertyMap;

    std::unique_ptr<SelectorQueryCache> m_selectorQueryCache;

    DocumentClasses m_documentClasses;

    RenderPtr<RenderView> m_renderView;
    std::unique_ptr<RenderStyle> m_initialContainingBlockStyle;

    WeakHashSet<MediaCanStartListener> m_mediaCanStartListeners;
    WeakHashSet<DisplayChangedObserver> m_displayChangedObservers;

#if ENABLE(FULLSCREEN_API)
    UniqueRef<FullscreenManager> m_fullscreenManager;
#endif

    WeakHashSet<HTMLImageElement, WeakPtrImplWithEventTargetData> m_dynamicMediaQueryDependentImages;

    Vector<WeakPtr<IntersectionObserver>> m_intersectionObservers;
    Timer m_intersectionObserversInitialUpdateTimer;
    // This is only non-null when this document is an explicit root.
    std::unique_ptr<IntersectionObserverData> m_intersectionObserverData;

    Vector<WeakPtr<ResizeObserver>> m_resizeObservers;

    Timer m_loadEventDelayTimer;

    ViewportArguments m_viewportArguments;
    OptionSet<DisabledAdaptations> m_disabledAdaptations;

    DocumentEventTiming m_eventTiming;

    RefPtr<MediaQueryMatcher> m_mediaQueryMatcher;
    
#if ENABLE(TOUCH_EVENTS)
    std::unique_ptr<EventTargetSet> m_touchEventTargets;
#endif
#if ENABLE(TOUCH_ACTION_REGIONS)
    bool m_mayHaveElementsWithNonAutoTouchAction { false };
#endif
#if ENABLE(EDITABLE_REGION)
    bool m_mayHaveEditableElements { false };
#endif

    bool m_mayHaveRenderedSVGForeignObjects { false };
    bool m_mayHaveRenderedSVGRootElements { false };

    std::unique_ptr<EventTargetSet> m_wheelEventTargets;

    MonotonicTime m_lastHandledUserGestureTimestamp;
    MonotonicTime m_userActivatedMediaFinishedPlayingTimestamp;

    void clearScriptedAnimationController();
    RefPtr<ScriptedAnimationController> m_scriptedAnimationController;

    std::unique_ptr<IdleCallbackController> m_idleCallbackController;

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
    std::unique_ptr<DeviceMotionClient> m_deviceMotionClient;
    std::unique_ptr<DeviceMotionController> m_deviceMotionController;
    std::unique_ptr<DeviceOrientationClient> m_deviceOrientationClient;
    std::unique_ptr<DeviceOrientationController> m_deviceOrientationController;
#endif

#if ENABLE(DEVICE_ORIENTATION)
    std::unique_ptr<DeviceOrientationAndMotionAccessController> m_deviceOrientationAndMotionAccessController;
#endif

    Timer m_pendingTasksTimer;
    Vector<Task> m_pendingTasks;

#if ENABLE(TEXT_AUTOSIZING)
    std::unique_ptr<TextAutoSizing> m_textAutoSizing;
#endif
        
    RefPtr<HighlightRegister> m_highlightRegister;
    RefPtr<HighlightRegister> m_fragmentHighlightRegister;
#if ENABLE(APP_HIGHLIGHTS)
    RefPtr<HighlightRegister> m_appHighlightRegister;
    std::unique_ptr<AppHighlightStorage> m_appHighlightStorage;
#endif

    Timer m_visualUpdatesSuppressionTimer;

    void clearSharedObjectPool();
    Timer m_sharedObjectPoolClearTimer;

    std::unique_ptr<DocumentSharedObjectPool> m_sharedObjectPool;

    using LocaleIdentifierToLocaleMap = HashMap<AtomString, std::unique_ptr<Locale>>;
    LocaleIdentifierToLocaleMap m_localeCache;

    RefPtr<Document> m_templateDocument;
    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_templateDocumentHost; // Manually managed weakref (backpointer from m_templateDocument).

    RefPtr<DocumentFragment> m_documentFragmentForInnerOuterHTML;

    Ref<CSSFontSelector> m_fontSelector;
    UniqueRef<DocumentFontLoader> m_fontLoader;

    WeakHashSet<MediaProducer> m_audioProducers;
    WeakPtr<SpeechRecognition> m_activeSpeechRecognition;

    ListHashSet<ShadowRoot*> m_inDocumentShadowRoots;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    using TargetIdToClientMap = HashMap<PlaybackTargetClientContextIdentifier, WebCore::MediaPlaybackTargetClient*>;
    TargetIdToClientMap m_idToClientMap;
    using TargetClientToIdMap = HashMap<WebCore::MediaPlaybackTargetClient*, PlaybackTargetClientContextIdentifier>;
    TargetClientToIdMap m_clientToIDMap;
#endif

    RefPtr<IDBClient::IDBConnectionProxy> m_idbConnectionProxy;

#if ENABLE(ATTACHMENT_ELEMENT)
    HashMap<String, Ref<HTMLAttachmentElement>> m_attachmentIdentifierToElementMap;
#endif

    Timer m_didAssociateFormControlsTimer;
    Timer m_cookieCacheExpiryTimer;

    RefPtr<SocketProvider> m_socketProvider;

    String m_cachedDOMCookies;

    std::optional<WallTime> m_overrideLastModified;

    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_associatedFormControls;
    unsigned m_disabledFieldsetElementsCount { 0 };

    unsigned m_dataListElementCount { 0 };

    unsigned m_listenerTypes { 0 };
    unsigned m_referencingNodeCount { 0 };
    int m_loadEventDelayCount { 0 };
    unsigned m_lastStyleUpdateSizeForTesting { 0 };

    // https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#throw-on-dynamic-markup-insertion-counter
    unsigned m_throwOnDynamicMarkupInsertionCount { 0 };

    // https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#ignore-opens-during-unload-counter
    unsigned m_ignoreOpensDuringUnloadCount { 0 };

    // https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#ignore-destructive-writes-counter
    unsigned m_ignoreDestructiveWriteCount { 0 };

    unsigned m_activeParserCount { 0 };
    unsigned m_styleRecalcCount { 0 };

    unsigned m_writeRecursionDepth { 0 };

    InheritedBool m_designMode { inherit };
    MediaProducerMediaStateFlags m_mediaState;
    bool m_userHasInteractedWithMediaElement { false };
    BackForwardCacheState m_backForwardCacheState { NotInBackForwardCache };
    ReadyState m_readyState { Complete };

    MutationObserverOptions m_mutationObserverTypes;

    bool m_activeParserWasAborted { false };
    bool m_writeRecursionIsTooDeep { false };
    bool m_wellFormed { false };
    bool m_createRenderers { true };

    bool m_hasNodesWithNonFinalStyle { false };
    bool m_hasNodesWithMissingStyle { false };
    // But sometimes you need to ignore pending stylesheet count to
    // force an immediate layout when requested by JS.
    bool m_ignorePendingStylesheets { false };

    bool m_hasElementUsingStyleBasedEditability { false };
    bool m_focusNavigationStartingNodeIsRemoved { false };

    bool m_printing { false };
    bool m_paginatedForScreen { false };

    DocumentCompatibilityMode m_compatibilityMode { DocumentCompatibilityMode::NoQuirksMode };
    bool m_compatibilityModeLocked { false }; // This is cheaper than making setCompatibilityMode virtual.

    // FIXME: Merge these 2 variables into an enum. Also, FrameLoader::m_didCallImplicitClose
    // is almost a duplication of this data, so that should probably get merged in too.
    // FIXME: Document::m_processingLoadEvent and DocumentLoader::m_wasOnloadDispatched are roughly the same
    // and should be merged.
    bool m_processingLoadEvent { false };
    bool m_loadEventFinished { false };

    bool m_visuallyOrdered { false };
    bool m_bParsing { false }; // FIXME: rename

    bool m_needsFullStyleRebuild { false };
    bool m_inStyleRecalc { false };
    bool m_inRenderTreeUpdate { false };
    bool m_isResolvingTreeStyle { false };
    bool m_isResolvingContainerQueries { false };

    bool m_gotoAnchorNeededAfterStylesheetsLoad { false };
    bool m_isDNSPrefetchEnabled { false };
    bool m_haveExplicitlyDisabledDNSPrefetch { false };

    bool m_isSynthesized { false };
    bool m_isNonRenderedPlaceholder { false };

    bool m_isAutofocusProcessed { false };

    bool m_sawElementsInKnownNamespaces { false };
    bool m_isSrcdocDocument { false };

    bool m_hasInjectedPlugInsScript { false };
    bool m_renderTreeBeingDestroyed { false };
    bool m_hasPreparedForDestruction { false };

    bool m_hasStyleWithViewportUnits { false };
    bool m_needsDOMWindowResizeEvent { false };
    bool m_needsVisualViewportResizeEvent { false };
    bool m_needsVisualViewportScrollEvent { false };
    bool m_isTimerThrottlingEnabled { false };
    bool m_isSuspended { false };

    bool m_scheduledTasksAreSuspended { false };
    bool m_visualUpdatesAllowed { true };

    bool m_areDeviceMotionAndOrientationUpdatesSuspended { false };
    bool m_userDidInteractWithPage { false };

    bool m_didEnqueueFirstContentfulPaint { false };

#if ASSERT_ENABLED
    bool m_inHitTesting { false };
#endif

#if ENABLE(TELEPHONE_NUMBER_DETECTION)
    bool m_isTelephoneNumberParsingAllowed { true };
#endif

    struct PendingScrollEventTargetList;
    std::unique_ptr<PendingScrollEventTargetList> m_pendingScrollEventTargetList;

#if ENABLE(MEDIA_STREAM)
    String m_idHashSalt;
    bool m_hasHadCaptureMediaStreamTrack { false };
#endif

#if ASSERT_ENABLED
    bool m_didDispatchViewportPropertiesChanged { false };
#endif

    bool m_updateTitleTaskScheduled { false };

    FocusTrigger m_latestFocusTrigger { FocusTrigger::Other };

    OrientationNotifier m_orientationNotifier;
    mutable RefPtr<Logger> m_logger;
    RefPtr<StringCallback> m_consoleMessageListener;

    static bool hasEverCreatedAnAXObjectCache;

    RefPtr<DocumentTimeline> m_timeline;
    std::unique_ptr<DocumentTimelinesController> m_timelinesController;

    RefPtr<WindowEventLoop> m_eventLoop;
    std::unique_ptr<EventLoopTaskGroup> m_documentTaskGroup;

#if ENABLE(SERVICE_WORKER)
    RefPtr<SWClientConnection> m_serviceWorkerConnection;
#endif

#if ENABLE(TRACKING_PREVENTION)
    RegistrableDomain m_registrableDomainRequestedPageSpecificStorageAccessWithUserInteraction { };
    String m_referrerOverride;
#endif
    
    CSSRegisteredCustomPropertySet m_CSSRegisteredPropertySet;

    std::optional<FixedVector<CSSPropertyID>> m_exposedComputedCSSPropertyIDs;

#if ENABLE(CSS_PAINTING_API)
    RefPtr<PaintWorklet> m_paintWorklet;
    HashMap<String, Ref<PaintWorkletGlobalScope>> m_paintWorkletGlobalScopes;
#endif
    unsigned m_numberOfRejectedSyncXHRs { 0 };
    bool m_isRunningUserScripts { false };
    bool m_shouldPreventEnteringBackForwardCacheForTesting { false };
    bool m_hasLoadedThirdPartyScript { false };
    bool m_hasLoadedThirdPartyFrame { false };
    bool m_hasVisuallyNonEmptyCustomContent { false };

    bool m_visibilityHiddenDueToDismissal { false };

    Ref<UndoManager> m_undoManager;
#if ENABLE(CONTENT_CHANGE_OBSERVER)
    std::unique_ptr<ContentChangeObserver> m_contentChangeObserver;
    std::unique_ptr<DOMTimerHoldingTank> m_domTimerHoldingTank;
#endif

#if ENABLE(PICTURE_IN_PICTURE_API)
    WeakPtr<HTMLVideoElement, WeakPtrImplWithEventTargetData> m_pictureInPictureElement;
#endif

    std::unique_ptr<TextManipulationController> m_textManipulationController;

    UniqueRef<Editor> m_editor;
    UniqueRef<FrameSelection> m_selection;
        
    String m_fragmentDirective;

    ListHashSet<Ref<Element>> m_topLayerElements;
    UniqueRef<WhitespaceCache> m_whitespaceCache;

#if ENABLE(WEB_RTC)
    RefPtr<RTCNetworkManager> m_rtcNetworkManager;
#endif

    std::unique_ptr<ModalContainerObserver> m_modalContainerObserver;

    Vector<Function<void()>> m_whenIsVisibleHandlers;

    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_elementsWithPendingUserAgentShadowTreeUpdates;

    Ref<ReportingScope> m_reportingScope;
    std::unique_ptr<WakeLockManager> m_wakeLockManager;

    std::unique_ptr<SleepDisabler> m_sleepDisabler;
};

Element* eventTargetElementForDocument(Document*);

WTF::TextStream& operator<<(WTF::TextStream&, const Document&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Document)
    static bool isType(const WebCore::ScriptExecutionContext& context) { return context.isDocument(); }
    static bool isType(const WebCore::Node& node) { return node.isDocumentNode(); }
    static bool isType(const WebCore::EventTarget& target) { return is<WebCore::Node>(target) && isType(downcast<WebCore::Node>(target)); }
SPECIALIZE_TYPE_TRAITS_END()
