/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// How ownership works
// -------------------
//
// Big oh represents a refcounted relationship: owner O--- ownee
//
// WebView (for the toplevel frame only)
//    O
//    |
//   Page O------- Frame (m_mainFrame) O-------O FrameView
//                   ||
//                   ||
//               FrameLoader O-------- WebFrame (via FrameLoaderClient)
//
// FrameLoader and Frame are formerly one object that was split apart because
// it got too big. They basically have the same lifetime, hence the double line.
//
// WebFrame is refcounted and has one ref on behalf of the FrameLoader/Frame.
// This is not a normal reference counted pointer because that would require
// changing WebKit code that we don't control. Instead, it is created with this
// ref initially and it is removed when the FrameLoader is getting destroyed.
//
// WebFrames are created in two places, first in WebViewImpl when the root
// frame is created, and second in WebFrame::CreateChildFrame when sub-frames
// are created. WebKit will hook up this object to the FrameLoader/Frame
// and the refcount will be correct.
//
// How frames are destroyed
// ------------------------
//
// The main frame is never destroyed and is re-used. The FrameLoader is re-used
// and a reference to the main frame is kept by the Page.
//
// When frame content is replaced, all subframes are destroyed. This happens
// in FrameLoader::detachFromParent for each subframe.
//
// Frame going away causes the FrameLoader to get deleted. In FrameLoader's
// destructor, it notifies its client with frameLoaderDestroyed. This calls
// WebFrame::Closing and then derefs the WebFrame and will cause it to be
// deleted (unless an external someone is also holding a reference).

#include "config.h"
#include "WebFrameImpl.h"

#include "Chrome.h"
#include "ChromiumBridge.h"
#include "ClipboardUtilitiesChromium.h"
#include "Console.h"
#include "DOMUtilitiesPrivate.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentFragment.h" // Only needed for ReplaceSelectionCommand.h :(
#include "DocumentLoader.h"
#include "DocumentMarker.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FormState.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLCollection.h"
#include "HTMLFormElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLHeadElement.h"
#include "HTMLInputElement.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HistoryItem.h"
#include "InspectorController.h"
#include "Page.h"
#include "Performance.h"
#include "PlatformContextSkia.h"
#include "PluginDocument.h"
#include "PrintContext.h"
#include "RenderFrame.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ReplaceSelectionCommand.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "SVGSMILElement.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "ScrollTypes.h"
#include "ScrollbarTheme.h"
#include "SelectionController.h"
#include "Settings.h"
#include "SkiaUtils.h"
#include "SubstituteData.h"
#include "TextAffinity.h"
#include "TextIterator.h"
#include "WebAnimationControllerImpl.h"
#include "WebConsoleMessage.h"
#include "WebDataSourceImpl.h"
#include "WebDocument.h"
#include "WebFindOptions.h"
#include "WebFormElement.h"
#include "WebFrameClient.h"
#include "WebHistoryItem.h"
#include "WebInputElement.h"
#include "WebPasswordAutocompleteListener.h"
#include "WebPerformance.h"
#include "WebPlugin.h"
#include "WebPluginContainerImpl.h"
#include "WebRange.h"
#include "WebRect.h"
#include "WebScriptSource.h"
#include "WebSecurityOrigin.h"
#include "WebSize.h"
#include "WebURLError.h"
#include "WebVector.h"
#include "WebViewImpl.h"
#include "XPathResult.h"
#include "markup.h"

#include <algorithm>
#include <wtf/CurrentTime.h>


#if OS(DARWIN)
#include "LocalCurrentGraphicsContext.h"
#endif

#if OS(LINUX) || OS(FREEBSD)
#include <gdk/gdk.h>
#endif

using namespace WebCore;

namespace WebKit {

static int frameCount = 0;

// Key for a StatsCounter tracking how many WebFrames are active.
static const char* const webFrameActiveCount = "WebFrameActiveCount";

static const char* const osdType = "application/opensearchdescription+xml";
static const char* const osdRel = "search";

// Backend for contentAsPlainText, this is a recursive function that gets
// the text for the current frame and all of its subframes. It will append
// the text of each frame in turn to the |output| up to |maxChars| length.
//
// The |frame| must be non-null.
static void frameContentAsPlainText(size_t maxChars, Frame* frame,
                                    Vector<UChar>* output)
{
    Document* doc = frame->document();
    if (!doc)
        return;

    if (!frame->view())
        return;

    // TextIterator iterates over the visual representation of the DOM. As such,
    // it requires you to do a layout before using it (otherwise it'll crash).
    if (frame->view()->needsLayout())
        frame->view()->layout();

    // Select the document body.
    RefPtr<Range> range(doc->createRange());
    ExceptionCode exception = 0;
    range->selectNodeContents(doc->body(), exception);

    if (!exception) {
        // The text iterator will walk nodes giving us text. This is similar to
        // the plainText() function in TextIterator.h, but we implement the maximum
        // size and also copy the results directly into a wstring, avoiding the
        // string conversion.
        for (TextIterator it(range.get()); !it.atEnd(); it.advance()) {
            const UChar* chars = it.characters();
            if (!chars) {
                if (it.length()) {
                    // It appears from crash reports that an iterator can get into a state
                    // where the character count is nonempty but the character pointer is
                    // null. advance()ing it will then just add that many to the null
                    // pointer which won't be caught in a null check but will crash.
                    //
                    // A null pointer and 0 length is common for some nodes.
                    //
                    // IF YOU CATCH THIS IN A DEBUGGER please let brettw know. We don't
                    // currently understand the conditions for this to occur. Ideally, the
                    // iterators would never get into the condition so we should fix them
                    // if we can.
                    ASSERT_NOT_REACHED();
                    break;
                }

                // Just got a null node, we can forge ahead!
                continue;
            }
            size_t toAppend =
                std::min(static_cast<size_t>(it.length()), maxChars - output->size());
            output->append(chars, toAppend);
            if (output->size() >= maxChars)
                return; // Filled up the buffer.
        }
    }

    // The separator between frames when the frames are converted to plain text.
    const UChar frameSeparator[] = { '\n', '\n' };
    const size_t frameSeparatorLen = 2;

    // Recursively walk the children.
    FrameTree* frameTree = frame->tree();
    for (Frame* curChild = frameTree->firstChild(); curChild; curChild = curChild->tree()->nextSibling()) {
        // Ignore the text of non-visible frames.
        RenderView* contentRenderer = curChild->contentRenderer();
        RenderPart* ownerRenderer = curChild->ownerRenderer();        
        if (!contentRenderer || !contentRenderer->width() || !contentRenderer->height()
            || (contentRenderer->x() + contentRenderer->width() <= 0) || (contentRenderer->y() + contentRenderer->height() <= 0)
            || (ownerRenderer && ownerRenderer->style() && ownerRenderer->style()->visibility() != VISIBLE)) {
            continue;
        }

        // Make sure the frame separator won't fill up the buffer, and give up if
        // it will. The danger is if the separator will make the buffer longer than
        // maxChars. This will cause the computation above:
        //   maxChars - output->size()
        // to be a negative number which will crash when the subframe is added.
        if (output->size() >= maxChars - frameSeparatorLen)
            return;

        output->append(frameSeparator, frameSeparatorLen);
        frameContentAsPlainText(maxChars, curChild, output);
        if (output->size() >= maxChars)
            return; // Filled up the buffer.
    }
}

static long long generateFrameIdentifier()
{
    static long long next = 0;
    return ++next;
}

WebPluginContainerImpl* WebFrameImpl::pluginContainerFromFrame(Frame* frame)
{
    if (!frame)
        return 0;
    if (!frame->document() || !frame->document()->isPluginDocument())
        return 0;
    PluginDocument* pluginDocument = static_cast<PluginDocument*>(frame->document());
    return static_cast<WebPluginContainerImpl *>(pluginDocument->pluginWidget());
}

// Simple class to override some of PrintContext behavior. Some of the methods
// made virtual so that they can be overriden by ChromePluginPrintContext.
class ChromePrintContext : public PrintContext, public Noncopyable {
public:
    ChromePrintContext(Frame* frame)
        : PrintContext(frame)
        , m_printedPageWidth(0)
    {
    }

    virtual void begin(float width)
    {
        ASSERT(!m_printedPageWidth);
        m_printedPageWidth = width;
        PrintContext::begin(m_printedPageWidth);
    }

    virtual void end()
    {
        PrintContext::end();
    }

    virtual float getPageShrink(int pageNumber) const
    {
        IntRect pageRect = m_pageRects[pageNumber];
        return m_printedPageWidth / pageRect.width();
    }

    // Spools the printed page, a subrect of m_frame. Skip the scale step.
    // NativeTheme doesn't play well with scaling. Scaling is done browser side
    // instead. Returns the scale to be applied.
    // On Linux, we don't have the problem with NativeTheme, hence we let WebKit
    // do the scaling and ignore the return value.
    virtual float spoolPage(GraphicsContext& ctx, int pageNumber)
    {
        IntRect pageRect = m_pageRects[pageNumber];
        float scale = m_printedPageWidth / pageRect.width();

        ctx.save();
#if OS(LINUX) || OS(FREEBSD)
        ctx.scale(WebCore::FloatSize(scale, scale));
#endif
        ctx.translate(static_cast<float>(-pageRect.x()),
                      static_cast<float>(-pageRect.y()));
        ctx.clip(pageRect);
        m_frame->view()->paintContents(&ctx, pageRect);
        ctx.restore();
        return scale;
    }

    virtual void computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight)
    {
        return PrintContext::computePageRects(printRect, headerHeight, footerHeight, userScaleFactor, outPageHeight);
    }

    virtual int pageCount() const
    {
        return PrintContext::pageCount();
    }

    virtual bool shouldUseBrowserOverlays() const
    {
        return true;
    }

private:
    // Set when printing.
    float m_printedPageWidth;
};

// Simple class to override some of PrintContext behavior. This is used when
// the frame hosts a plugin that supports custom printing. In this case, we
// want to delegate all printing related calls to the plugin.
class ChromePluginPrintContext : public ChromePrintContext {
public:
    ChromePluginPrintContext(Frame* frame, int printerDPI)
        : ChromePrintContext(frame), m_pageCount(0), m_printerDPI(printerDPI)
    {
        // This HAS to be a frame hosting a full-mode plugin
        ASSERT(frame->document()->isPluginDocument());
    }

    virtual void begin(float width)
    {
    }

    virtual void end()
    {
        WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(m_frame);
        if (pluginContainer && pluginContainer->supportsPaginatedPrint())
            pluginContainer->printEnd();
        else
            ASSERT_NOT_REACHED();
    }

    virtual float getPageShrink(int pageNumber) const
    {
        // We don't shrink the page (maybe we should ask the widget ??)
        return 1.0;
    }

    virtual void computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight)
    {
        WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(m_frame);
        if (pluginContainer && pluginContainer->supportsPaginatedPrint())
            m_pageCount = pluginContainer->printBegin(IntRect(printRect), m_printerDPI);
        else
            ASSERT_NOT_REACHED();
    }

    virtual int pageCount() const
    {
        return m_pageCount;
    }

    // Spools the printed page, a subrect of m_frame.  Skip the scale step.
    // NativeTheme doesn't play well with scaling. Scaling is done browser side
    // instead.  Returns the scale to be applied.
    virtual float spoolPage(GraphicsContext& ctx, int pageNumber)
    {
        WebPluginContainerImpl* pluginContainer = WebFrameImpl::pluginContainerFromFrame(m_frame);
        if (pluginContainer && pluginContainer->supportsPaginatedPrint())
            pluginContainer->printPage(pageNumber, &ctx);
        else
            ASSERT_NOT_REACHED();
        return 1.0;
    }

    virtual bool shouldUseBrowserOverlays() const
    {
        return false;
    }

private:
    // Set when printing.
    int m_pageCount;
    int m_printerDPI;
};

static WebDataSource* DataSourceForDocLoader(DocumentLoader* loader)
{
    return loader ? WebDataSourceImpl::fromDocumentLoader(loader) : 0;
}


// WebFrame -------------------------------------------------------------------

class WebFrameImpl::DeferredScopeStringMatches {
public:
    DeferredScopeStringMatches(WebFrameImpl* webFrame,
                               int identifier,
                               const WebString& searchText,
                               const WebFindOptions& options,
                               bool reset)
        : m_timer(this, &DeferredScopeStringMatches::doTimeout)
        , m_webFrame(webFrame)
        , m_identifier(identifier)
        , m_searchText(searchText)
        , m_options(options)
        , m_reset(reset)
    {
        m_timer.startOneShot(0.0);
    }

private:
    void doTimeout(Timer<DeferredScopeStringMatches>*)
    {
        m_webFrame->callScopeStringMatches(
            this, m_identifier, m_searchText, m_options, m_reset);
    }

    Timer<DeferredScopeStringMatches> m_timer;
    RefPtr<WebFrameImpl> m_webFrame;
    int m_identifier;
    WebString m_searchText;
    WebFindOptions m_options;
    bool m_reset;
};


// WebFrame -------------------------------------------------------------------

int WebFrame::instanceCount()
{
    return frameCount;
}

WebFrame* WebFrame::frameForEnteredContext()
{
    Frame* frame =
        ScriptController::retrieveFrameForEnteredContext();
    return WebFrameImpl::fromFrame(frame);
}

WebFrame* WebFrame::frameForCurrentContext()
{
    Frame* frame =
        ScriptController::retrieveFrameForCurrentContext();
    return WebFrameImpl::fromFrame(frame);
}

WebFrame* WebFrame::fromFrameOwnerElement(const WebElement& element)
{
    return WebFrameImpl::fromFrameOwnerElement(
        PassRefPtr<Element>(element).get());
}

WebString WebFrameImpl::name() const
{
    return m_frame->tree()->name();
}

void WebFrameImpl::setName(const WebString& name)
{
    m_frame->tree()->setName(name);
}

long long WebFrameImpl::identifier() const
{
    return m_identifier;
}

WebURL WebFrameImpl::url() const
{
    const WebDataSource* ds = dataSource();
    if (!ds)
        return WebURL();
    return ds->request().url();
}

WebURL WebFrameImpl::favIconURL() const
{
    FrameLoader* frameLoader = m_frame->loader();
    // The URL to the favicon may be in the header. As such, only
    // ask the loader for the favicon if it's finished loading.
    if (frameLoader->state() == FrameStateComplete) {
        const KURL& url = frameLoader->iconURL();
        if (!url.isEmpty())
            return url;
    }
    return WebURL();
}

WebURL WebFrameImpl::openSearchDescriptionURL() const
{
    FrameLoader* frameLoader = m_frame->loader();
    if (frameLoader->state() == FrameStateComplete
        && m_frame->document() && m_frame->document()->head()
        && !m_frame->tree()->parent()) {
        HTMLHeadElement* head = m_frame->document()->head();
        if (head) {
            RefPtr<HTMLCollection> children = head->children();
            for (Node* child = children->firstItem(); child; child = children->nextItem()) {
                HTMLLinkElement* linkElement = toHTMLLinkElement(child);
                if (linkElement
                    && linkElement->type() == osdType
                    && linkElement->rel() == osdRel
                    && !linkElement->href().isEmpty())
                    return linkElement->href();
            }
        }
    }
    return WebURL();
}

WebString WebFrameImpl::encoding() const
{
    return frame()->loader()->writer()->encoding();
}

WebSize WebFrameImpl::scrollOffset() const
{
    FrameView* view = frameView();
    if (view)
        return view->scrollOffset();

    return WebSize();
}

WebSize WebFrameImpl::contentsSize() const
{
    return frame()->view()->contentsSize();
}

int WebFrameImpl::contentsPreferredWidth() const
{
    if (m_frame->document() && m_frame->document()->renderView())
        return m_frame->document()->renderView()->minPreferredLogicalWidth();
    return 0;
}

int WebFrameImpl::documentElementScrollHeight() const
{
    if (m_frame->document() && m_frame->document()->documentElement())
        return m_frame->document()->documentElement()->scrollHeight();
    return 0;
}

bool WebFrameImpl::hasVisibleContent() const
{
    return frame()->view()->visibleWidth() > 0 && frame()->view()->visibleHeight() > 0;
}

WebView* WebFrameImpl::view() const
{
    return viewImpl();
}

WebFrame* WebFrameImpl::opener() const
{
    Frame* opener = 0;
    if (m_frame)
        opener = m_frame->loader()->opener();
    return fromFrame(opener);
}

WebFrame* WebFrameImpl::parent() const
{
    Frame* parent = 0;
    if (m_frame)
        parent = m_frame->tree()->parent();
    return fromFrame(parent);
}

WebFrame* WebFrameImpl::top() const
{
    if (m_frame)
        return fromFrame(m_frame->tree()->top());

    return 0;
}

WebFrame* WebFrameImpl::firstChild() const
{
    return fromFrame(frame()->tree()->firstChild());
}

WebFrame* WebFrameImpl::lastChild() const
{
    return fromFrame(frame()->tree()->lastChild());
}

WebFrame* WebFrameImpl::nextSibling() const
{
    return fromFrame(frame()->tree()->nextSibling());
}

WebFrame* WebFrameImpl::previousSibling() const
{
    return fromFrame(frame()->tree()->previousSibling());
}

WebFrame* WebFrameImpl::traverseNext(bool wrap) const
{
    return fromFrame(frame()->tree()->traverseNextWithWrap(wrap));
}

WebFrame* WebFrameImpl::traversePrevious(bool wrap) const
{
    return fromFrame(frame()->tree()->traversePreviousWithWrap(wrap));
}

WebFrame* WebFrameImpl::findChildByName(const WebString& name) const
{
    return fromFrame(frame()->tree()->child(name));
}

WebFrame* WebFrameImpl::findChildByExpression(const WebString& xpath) const
{
    if (xpath.isEmpty())
        return 0;

    Document* document = m_frame->document();

    ExceptionCode ec = 0;
    PassRefPtr<XPathResult> xpathResult =
        document->evaluate(xpath,
        document,
        0, // namespace
        XPathResult::ORDERED_NODE_ITERATOR_TYPE,
        0, // XPathResult object
        ec);
    if (!xpathResult.get())
        return 0;

    Node* node = xpathResult->iterateNext(ec);

    if (!node || !node->isFrameOwnerElement())
        return 0;
    HTMLFrameOwnerElement* frameElement =
        static_cast<HTMLFrameOwnerElement*>(node);
    return fromFrame(frameElement->contentFrame());
}

WebDocument WebFrameImpl::document() const
{
    if (!m_frame || !m_frame->document())
        return WebDocument();
    return WebDocument(m_frame->document());
}

void WebFrameImpl::forms(WebVector<WebFormElement>& results) const
{
    if (!m_frame)
        return;

    RefPtr<HTMLCollection> forms = m_frame->document()->forms();
    size_t formCount = 0;
    for (size_t i = 0; i < forms->length(); ++i) {
        Node* node = forms->item(i);
        if (node && node->isHTMLElement())
            ++formCount;
    }

    WebVector<WebFormElement> temp(formCount);
    for (size_t i = 0; i < formCount; ++i) {
        Node* node = forms->item(i);
        // Strange but true, sometimes item can be 0.
        if (node && node->isHTMLElement())
            temp[i] = static_cast<HTMLFormElement*>(node);
    }
    results.swap(temp);
}

WebAnimationController* WebFrameImpl::animationController()
{
    return &m_animationController;
}

WebPerformance WebFrameImpl::performance() const
{
    if (!m_frame || !m_frame->domWindow())
        return WebPerformance();

    return WebPerformance(m_frame->domWindow()->webkitPerformance());
}

WebSecurityOrigin WebFrameImpl::securityOrigin() const
{
    if (!m_frame || !m_frame->document())
        return WebSecurityOrigin();

    return WebSecurityOrigin(m_frame->document()->securityOrigin());
}

void WebFrameImpl::grantUniversalAccess()
{
    ASSERT(m_frame && m_frame->document());
    if (m_frame && m_frame->document())
        m_frame->document()->securityOrigin()->grantUniversalAccess();
}

NPObject* WebFrameImpl::windowObject() const
{
    if (!m_frame)
        return 0;

    return m_frame->script()->windowScriptNPObject();
}

void WebFrameImpl::bindToWindowObject(const WebString& name, NPObject* object)
{
    ASSERT(m_frame);
    if (!m_frame || !m_frame->script()->canExecuteScripts(NotAboutToExecuteScript))
        return;

    String key = name;
#if USE(V8)
    m_frame->script()->bindToWindowObject(m_frame, key, object);
#else
    notImplemented();
#endif
}

void WebFrameImpl::executeScript(const WebScriptSource& source)
{
    m_frame->script()->executeScript(
        ScriptSourceCode(source.code, source.url, source.startLine));
}

void WebFrameImpl::executeScriptInIsolatedWorld(
    int worldId, const WebScriptSource* sourcesIn, unsigned numSources,
    int extensionGroup)
{
    Vector<ScriptSourceCode> sources;

    for (unsigned i = 0; i < numSources; ++i) {
        sources.append(ScriptSourceCode(
            sourcesIn[i].code, sourcesIn[i].url, sourcesIn[i].startLine));
    }

    m_frame->script()->evaluateInIsolatedWorld(worldId, sources, extensionGroup);
}

void WebFrameImpl::addMessageToConsole(const WebConsoleMessage& message)
{
    ASSERT(frame());

    MessageLevel webCoreMessageLevel;
    switch (message.level) {
    case WebConsoleMessage::LevelTip:
        webCoreMessageLevel = TipMessageLevel;
        break;
    case WebConsoleMessage::LevelLog:
        webCoreMessageLevel = LogMessageLevel;
        break;
    case WebConsoleMessage::LevelWarning:
        webCoreMessageLevel = WarningMessageLevel;
        break;
    case WebConsoleMessage::LevelError:
        webCoreMessageLevel = ErrorMessageLevel;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    frame()->domWindow()->console()->addMessage(
        OtherMessageSource, LogMessageType, webCoreMessageLevel, message.text,
        1, String());
}

void WebFrameImpl::collectGarbage()
{
    if (!m_frame)
        return;
    if (!m_frame->settings()->isJavaScriptEnabled())
        return;
    // FIXME: Move this to the ScriptController and make it JS neutral.
#if USE(V8)
    m_frame->script()->collectGarbage();
#else
    notImplemented();
#endif
}

#if USE(V8)
v8::Handle<v8::Value> WebFrameImpl::executeScriptAndReturnValue(
    const WebScriptSource& source)
{
    return m_frame->script()->executeScript(
        ScriptSourceCode(source.code, source.url, source.startLine)).v8Value();
}

// Returns the V8 context for this frame, or an empty handle if there is none.
v8::Local<v8::Context> WebFrameImpl::mainWorldScriptContext() const
{
    if (!m_frame)
        return v8::Local<v8::Context>();

    return V8Proxy::mainWorldContext(m_frame);
}
#endif

bool WebFrameImpl::insertStyleText(
    const WebString& css, const WebString& id) {
    Document* document = frame()->document();
    if (!document)
        return false;
    Element* documentElement = document->documentElement();
    if (!documentElement)
        return false;

    ExceptionCode err = 0;

    if (!id.isEmpty()) {
        Element* oldElement = document->getElementById(id);
        if (oldElement) {
            Node* parent = oldElement->parent();
            if (!parent)
                return false;
            parent->removeChild(oldElement, err);
        }
    }

    RefPtr<Element> stylesheet = document->createElement(
        HTMLNames::styleTag, false);
    if (!id.isEmpty())
        stylesheet->setAttribute(HTMLNames::idAttr, id);
    stylesheet->setTextContent(css, err);
    ASSERT(!err);
    Node* first = documentElement->firstChild();
    bool success = documentElement->insertBefore(stylesheet, first, err);
    ASSERT(success);
    return success;
}

void WebFrameImpl::reload(bool ignoreCache)
{
    m_frame->loader()->history()->saveDocumentAndScrollState();
    m_frame->loader()->reload(ignoreCache);
}

void WebFrameImpl::loadRequest(const WebURLRequest& request)
{
    ASSERT(!request.isNull());
    const ResourceRequest& resourceRequest = request.toResourceRequest();

    if (resourceRequest.url().protocolIs("javascript")) {
        loadJavaScriptURL(resourceRequest.url());
        return;
    }

    m_frame->loader()->load(resourceRequest, false);
}

void WebFrameImpl::loadHistoryItem(const WebHistoryItem& item)
{
    RefPtr<HistoryItem> historyItem = PassRefPtr<HistoryItem>(item);
    ASSERT(historyItem.get());

    // If there is no currentItem, which happens when we are navigating in
    // session history after a crash, we need to manufacture one otherwise WebKit
    // hoarks. This is probably the wrong thing to do, but it seems to work.
    RefPtr<HistoryItem> currentItem = m_frame->loader()->history()->currentItem();
    if (!currentItem) {
        currentItem = HistoryItem::create();
        currentItem->setLastVisitWasFailure(true);
        m_frame->loader()->history()->setCurrentItem(currentItem.get());
        viewImpl()->setCurrentHistoryItem(currentItem.get());
    }

    m_frame->loader()->history()->goToItem(
        historyItem.get(), FrameLoadTypeIndexedBackForward);
}

void WebFrameImpl::loadData(const WebData& data,
                            const WebString& mimeType,
                            const WebString& textEncoding,
                            const WebURL& baseURL,
                            const WebURL& unreachableURL,
                            bool replace)
{
    SubstituteData substData(data, mimeType, textEncoding, unreachableURL);
    ASSERT(substData.isValid());

    // If we are loading substitute data to replace an existing load, then
    // inherit all of the properties of that original request.  This way,
    // reload will re-attempt the original request.  It is essential that
    // we only do this when there is an unreachableURL since a non-empty
    // unreachableURL informs FrameLoader::reload to load unreachableURL
    // instead of the currently loaded URL.
    ResourceRequest request;
    if (replace && !unreachableURL.isEmpty())
        request = m_frame->loader()->originalRequest();
    request.setURL(baseURL);

    m_frame->loader()->load(request, substData, false);
    if (replace) {
        // Do this to force WebKit to treat the load as replacing the currently
        // loaded page.
        m_frame->loader()->setReplacing();
    }
}

void WebFrameImpl::loadHTMLString(const WebData& data,
                                  const WebURL& baseURL,
                                  const WebURL& unreachableURL,
                                  bool replace)
{
    loadData(data,
             WebString::fromUTF8("text/html"),
             WebString::fromUTF8("UTF-8"),
             baseURL,
             unreachableURL,
             replace);
}

bool WebFrameImpl::isLoading() const
{
    if (!m_frame)
        return false;
    return m_frame->loader()->isLoading();
}

void WebFrameImpl::stopLoading()
{
    if (!m_frame)
      return;

    // FIXME: Figure out what we should really do here.  It seems like a bug
    // that FrameLoader::stopLoading doesn't call stopAllLoaders.
    m_frame->loader()->stopAllLoaders();
    m_frame->loader()->stopLoading(UnloadEventPolicyNone);
}

WebDataSource* WebFrameImpl::provisionalDataSource() const
{
    FrameLoader* frameLoader = m_frame->loader();

    // We regard the policy document loader as still provisional.
    DocumentLoader* docLoader = frameLoader->provisionalDocumentLoader();
    if (!docLoader)
        docLoader = frameLoader->policyDocumentLoader();

    return DataSourceForDocLoader(docLoader);
}

WebDataSource* WebFrameImpl::dataSource() const
{
    return DataSourceForDocLoader(m_frame->loader()->documentLoader());
}

WebHistoryItem WebFrameImpl::previousHistoryItem() const
{
    // We use the previous item here because documentState (filled-out forms)
    // only get saved to history when it becomes the previous item.  The caller
    // is expected to query the history item after a navigation occurs, after
    // the desired history item has become the previous entry.
    return WebHistoryItem(viewImpl()->previousHistoryItem());
}

WebHistoryItem WebFrameImpl::currentHistoryItem() const
{
    // If we are still loading, then we don't want to clobber the current
    // history item as this could cause us to lose the scroll position and
    // document state.  However, it is OK for new navigations.
    if (m_frame->loader()->loadType() == FrameLoadTypeStandard
        || !m_frame->loader()->activeDocumentLoader()->isLoadingInAPISense())
        m_frame->loader()->history()->saveDocumentAndScrollState();

    return WebHistoryItem(m_frame->page()->backForwardList()->currentItem());
}

void WebFrameImpl::enableViewSourceMode(bool enable)
{
    if (m_frame)
        m_frame->setInViewSourceMode(enable);
}

bool WebFrameImpl::isViewSourceModeEnabled() const
{
    if (m_frame)
        return m_frame->inViewSourceMode();

    return false;
}

void WebFrameImpl::setReferrerForRequest(
    WebURLRequest& request, const WebURL& referrerURL) {
    String referrer;
    if (referrerURL.isEmpty())
        referrer = m_frame->loader()->outgoingReferrer();
    else
        referrer = referrerURL.spec().utf16();
    if (SecurityOrigin::shouldHideReferrer(request.url(), referrer))
        return;
    request.setHTTPHeaderField(WebString::fromUTF8("Referer"), referrer);
}

void WebFrameImpl::dispatchWillSendRequest(WebURLRequest& request)
{
    ResourceResponse response;
    m_frame->loader()->client()->dispatchWillSendRequest(
        0, 0, request.toMutableResourceRequest(), response);
}

void WebFrameImpl::commitDocumentData(const char* data, size_t length)
{
    m_frame->loader()->documentLoader()->commitData(data, length);
}

unsigned WebFrameImpl::unloadListenerCount() const
{
    return frame()->domWindow()->pendingUnloadEventListeners();
}

bool WebFrameImpl::isProcessingUserGesture() const
{
    return frame()->loader()->isProcessingUserGesture();
}

bool WebFrameImpl::willSuppressOpenerInNewFrame() const
{
    return frame()->loader()->suppressOpenerInNewFrame();
}

void WebFrameImpl::replaceSelection(const WebString& text)
{
    RefPtr<DocumentFragment> fragment = createFragmentFromText(
        frame()->selection()->toNormalizedRange().get(), text);
    applyCommand(ReplaceSelectionCommand::create(
        frame()->document(), fragment.get(), false, true, true));
}

void WebFrameImpl::insertText(const WebString& text)
{
    frame()->editor()->insertText(text, 0);
}

void WebFrameImpl::setMarkedText(
    const WebString& text, unsigned location, unsigned length)
{
    Editor* editor = frame()->editor();

    editor->confirmComposition(text);

    Vector<CompositionUnderline> decorations;
    editor->setComposition(text, decorations, location, length);
}

void WebFrameImpl::unmarkText()
{
    frame()->editor()->confirmCompositionWithoutDisturbingSelection();
}

bool WebFrameImpl::hasMarkedText() const
{
    return frame()->editor()->hasComposition();
}

WebRange WebFrameImpl::markedRange() const
{
    return frame()->editor()->compositionRange();
}

bool WebFrameImpl::firstRectForCharacterRange(unsigned location, unsigned length, WebRect& rect) const
{
    if ((location + length < location) && (location + length))
        length = 0;

    Element* selectionRoot = frame()->selection()->rootEditableElement();
    Element* scope = selectionRoot ? selectionRoot : frame()->document()->documentElement();
    RefPtr<Range> range = TextIterator::rangeFromLocationAndLength(scope, location, length);
    if (!range)
        return false;
    IntRect intRect = frame()->editor()->firstRectForRange(range.get());
    rect = WebRect(intRect.x(), intRect.y(), intRect.width(), intRect.height());

    return true;
}

bool WebFrameImpl::executeCommand(const WebString& name)
{
    ASSERT(frame());

    if (name.length() <= 2)
        return false;

    // Since we don't have NSControl, we will convert the format of command
    // string and call the function on Editor directly.
    String command = name;

    // Make sure the first letter is upper case.
    command.replace(0, 1, command.substring(0, 1).upper());

    // Remove the trailing ':' if existing.
    if (command[command.length() - 1] == UChar(':'))
        command = command.substring(0, command.length() - 1);

    if (command == "Copy") {
        WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
        if (pluginContainer) {
            pluginContainer->copy();
            return true;
        }
    }

    bool rv = true;

    // Specially handling commands that Editor::execCommand does not directly
    // support.
    if (command == "DeleteToEndOfParagraph") {
        Editor* editor = frame()->editor();
        if (!editor->deleteWithDirection(SelectionController::DirectionForward,
                                         ParagraphBoundary,
                                         true,
                                         false)) {
            editor->deleteWithDirection(SelectionController::DirectionForward,
                                        CharacterGranularity,
                                        true,
                                        false);
        }
    } else if (command == "Indent")
        frame()->editor()->indent();
    else if (command == "Outdent")
        frame()->editor()->outdent();
    else if (command == "DeleteBackward")
        rv = frame()->editor()->command(AtomicString("BackwardDelete")).execute();
    else if (command == "DeleteForward")
        rv = frame()->editor()->command(AtomicString("ForwardDelete")).execute();
    else if (command == "AdvanceToNextMisspelling") {
        // False must be passed here, or the currently selected word will never be
        // skipped.
        frame()->editor()->advanceToNextMisspelling(false);
    } else if (command == "ToggleSpellPanel")
        frame()->editor()->showSpellingGuessPanel();
    else
        rv = frame()->editor()->command(command).execute();
    return rv;
}

bool WebFrameImpl::executeCommand(const WebString& name, const WebString& value)
{
    ASSERT(frame());
    String webName = name;

    // moveToBeginningOfDocument and moveToEndfDocument are only handled by WebKit
    // for editable nodes.
    if (!frame()->editor()->canEdit() && webName == "moveToBeginningOfDocument")
        return viewImpl()->propagateScroll(ScrollUp, ScrollByDocument);

    if (!frame()->editor()->canEdit() && webName == "moveToEndOfDocument")
        return viewImpl()->propagateScroll(ScrollDown, ScrollByDocument);

    return frame()->editor()->command(webName).execute(value);
}

bool WebFrameImpl::isCommandEnabled(const WebString& name) const
{
    ASSERT(frame());
    return frame()->editor()->command(name).isEnabled();
}

void WebFrameImpl::enableContinuousSpellChecking(bool enable)
{
    if (enable == isContinuousSpellCheckingEnabled())
        return;
    frame()->editor()->toggleContinuousSpellChecking();
}

bool WebFrameImpl::isContinuousSpellCheckingEnabled() const
{
    return frame()->editor()->isContinuousSpellCheckingEnabled();
}

bool WebFrameImpl::hasSelection() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->hasSelection();

    // frame()->selection()->isNone() never returns true.
    return (frame()->selection()->start() != frame()->selection()->end());
}

WebRange WebFrameImpl::selectionRange() const
{
    return frame()->selection()->toNormalizedRange();
}

WebString WebFrameImpl::selectionAsText() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->selectionAsText();

    RefPtr<Range> range = frame()->selection()->toNormalizedRange();
    if (!range.get())
        return WebString();

    String text = range->text();
#if OS(WINDOWS)
    replaceNewlinesWithWindowsStyleNewlines(text);
#endif
    replaceNBSPWithSpace(text);
    return text;
}

WebString WebFrameImpl::selectionAsMarkup() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->selectionAsMarkup();

    RefPtr<Range> range = frame()->selection()->toNormalizedRange();
    if (!range.get())
        return WebString();

    return createMarkup(range.get(), 0);
}

void WebFrameImpl::selectWordAroundPosition(Frame* frame, VisiblePosition pos)
{
    VisibleSelection selection(pos);
    selection.expandUsingGranularity(WordGranularity);

    if (frame->selection()->shouldChangeSelection(selection)) {
        TextGranularity granularity = selection.isRange() ? WordGranularity : CharacterGranularity;
        frame->selection()->setSelection(selection, granularity);
    }
}

bool WebFrameImpl::selectWordAroundCaret()
{
    SelectionController* controller = frame()->selection();
    ASSERT(!controller->isNone());
    if (controller->isNone() || controller->isRange())
        return false;
    selectWordAroundPosition(frame(), controller->selection().visibleStart());
    return true;
}

int WebFrameImpl::printBegin(const WebSize& pageSize, int printerDPI, bool *useBrowserOverlays)
{
    ASSERT(!frame()->document()->isFrameSet());
    // If this is a plugin document, check if the plugin supports its own
    // printing. If it does, we will delegate all printing to that.
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer && pluginContainer->supportsPaginatedPrint())
        m_printContext.set(new ChromePluginPrintContext(frame(), printerDPI));
    else
        m_printContext.set(new ChromePrintContext(frame()));

    FloatRect rect(0, 0, static_cast<float>(pageSize.width),
                         static_cast<float>(pageSize.height));
    m_printContext->begin(rect.width());
    float pageHeight;
    // We ignore the overlays calculation for now since they are generated in the
    // browser. pageHeight is actually an output parameter.
    m_printContext->computePageRects(rect, 0, 0, 1.0, pageHeight);
    if (useBrowserOverlays)
        *useBrowserOverlays = m_printContext->shouldUseBrowserOverlays();

    return m_printContext->pageCount();
}

float WebFrameImpl::getPrintPageShrink(int page)
{
    // Ensure correct state.
    if (!m_printContext.get() || page < 0) {
        ASSERT_NOT_REACHED();
        return 0;
    }

    return m_printContext->getPageShrink(page);
}

float WebFrameImpl::printPage(int page, WebCanvas* canvas)
{
    // Ensure correct state.
    if (!m_printContext.get() || page < 0 || !frame() || !frame()->document()) {
        ASSERT_NOT_REACHED();
        return 0;
    }

#if OS(WINDOWS) || OS(LINUX) || OS(FREEBSD) || OS(SOLARIS)
    PlatformContextSkia context(canvas);
    GraphicsContext spool(&context);
#elif OS(DARWIN)
    GraphicsContext spool(canvas);
    LocalCurrentGraphicsContext localContext(&spool);
#endif

    return m_printContext->spoolPage(spool, page);
}

void WebFrameImpl::printEnd()
{
    ASSERT(m_printContext.get());
    if (m_printContext.get())
        m_printContext->end();
    m_printContext.clear();
}

bool WebFrameImpl::isPageBoxVisible(int pageIndex)
{
    return frame()->document()->isPageBoxVisible(pageIndex);
}

void WebFrameImpl::pageSizeAndMarginsInPixels(int pageIndex,
                                              WebSize& pageSize,
                                              int& marginTop,
                                              int& marginRight,
                                              int& marginBottom,
                                              int& marginLeft)
{
    IntSize size(pageSize.width, pageSize.height);
    frame()->document()->pageSizeAndMarginsInPixels(pageIndex,
                                                    size,
                                                    marginTop,
                                                    marginRight,
                                                    marginBottom,
                                                    marginLeft);
    pageSize = size;
}

bool WebFrameImpl::find(int identifier,
                        const WebString& searchText,
                        const WebFindOptions& options,
                        bool wrapWithinFrame,
                        WebRect* selectionRect)
{
    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();

    if (!options.findNext)
        frame()->page()->unmarkAllTextMatches();
    else
        setMarkerActive(m_activeMatch.get(), false); // Active match is changing.

    // Starts the search from the current selection.
    bool startInSelection = true;

    // If the user has selected something since the last Find operation we want
    // to start from there. Otherwise, we start searching from where the last Find
    // operation left off (either a Find or a FindNext operation).
    VisibleSelection selection(frame()->selection()->selection());
    bool activeSelection = !selection.isNone();
    if (!activeSelection && m_activeMatch) {
        selection = VisibleSelection(m_activeMatch.get());
        frame()->selection()->setSelection(selection);
    }

    ASSERT(frame() && frame()->view());
    bool found = frame()->editor()->findString(
        searchText, options.forward, options.matchCase, wrapWithinFrame,
        startInSelection);
    if (found) {
        // Store which frame was active. This will come in handy later when we
        // change the active match ordinal below.
        WebFrameImpl* oldActiveFrame = mainFrameImpl->m_activeMatchFrame;
        // Set this frame as the active frame (the one with the active highlight).
        mainFrameImpl->m_activeMatchFrame = this;

        // We found something, so we can now query the selection for its position.
        VisibleSelection newSelection(frame()->selection()->selection());
        IntRect currSelectionRect;

        // If we thought we found something, but it couldn't be selected (perhaps
        // because it was marked -webkit-user-select: none), we can't set it to
        // be active but we still continue searching. This matches Safari's
        // behavior, including some oddities when selectable and un-selectable text
        // are mixed on a page: see https://bugs.webkit.org/show_bug.cgi?id=19127.
        if (newSelection.isNone() || (newSelection.start() == newSelection.end()))
            m_activeMatch = 0;
        else {
            m_activeMatch = newSelection.toNormalizedRange();
            currSelectionRect = m_activeMatch->boundingBox();
            setMarkerActive(m_activeMatch.get(), true); // Active.
            // WebKit draws the highlighting for all matches.
            executeCommand(WebString::fromUTF8("Unselect"));
        }

        // Make sure no node is focused. See http://crbug.com/38700.
        frame()->document()->setFocusedNode(0);

        if (!options.findNext || activeSelection) {
            // This is either a Find operation or a Find-next from a new start point
            // due to a selection, so we set the flag to ask the scoping effort
            // to find the active rect for us so we can update the ordinal (n of m).
            m_locatingActiveRect = true;
        } else {
            if (oldActiveFrame != this) {
                // If the active frame has changed it means that we have a multi-frame
                // page and we just switch to searching in a new frame. Then we just
                // want to reset the index.
                if (options.forward)
                    m_activeMatchIndex = 0;
                else
                    m_activeMatchIndex = m_lastMatchCount - 1;
            } else {
                // We are still the active frame, so increment (or decrement) the
                // |m_activeMatchIndex|, wrapping if needed (on single frame pages).
                options.forward ? ++m_activeMatchIndex : --m_activeMatchIndex;
                if (m_activeMatchIndex + 1 > m_lastMatchCount)
                    m_activeMatchIndex = 0;
                if (m_activeMatchIndex == -1)
                    m_activeMatchIndex = m_lastMatchCount - 1;
            }
            if (selectionRect) {
                WebRect rect = frame()->view()->convertToContainingWindow(currSelectionRect);
                rect.x -= frameView()->scrollOffset().width();
                rect.y -= frameView()->scrollOffset().height();
                *selectionRect = rect;

                reportFindInPageSelection(rect, m_activeMatchIndex + 1, identifier);
            }
        }
    } else {
        // Nothing was found in this frame.
        m_activeMatch = 0;

        // Erase all previous tickmarks and highlighting.
        invalidateArea(InvalidateAll);
    }

    return found;
}

void WebFrameImpl::stopFinding(bool clearSelection)
{
    if (!clearSelection)
        setFindEndstateFocusAndSelection();
    cancelPendingScopingEffort();

    // Remove all markers for matches found and turn off the highlighting.
    frame()->document()->markers()->removeMarkers(DocumentMarker::TextMatch);
    frame()->editor()->setMarkedTextMatchesAreHighlighted(false);

    // Let the frame know that we don't want tickmarks or highlighting anymore.
    invalidateArea(InvalidateAll);
}

void WebFrameImpl::scopeStringMatches(int identifier,
                                      const WebString& searchText,
                                      const WebFindOptions& options,
                                      bool reset)
{
    if (!shouldScopeMatches(searchText))
        return;

    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();

    if (reset) {
        // This is a brand new search, so we need to reset everything.
        // Scoping is just about to begin.
        m_scopingComplete = false;
        // Clear highlighting for this frame.
        if (frame()->editor()->markedTextMatchesAreHighlighted())
            frame()->page()->unmarkAllTextMatches();
        // Clear the counters from last operation.
        m_lastMatchCount = 0;
        m_nextInvalidateAfter = 0;

        m_resumeScopingFromRange = 0;

        mainFrameImpl->m_framesScopingCount++;

        // Now, defer scoping until later to allow find operation to finish quickly.
        scopeStringMatchesSoon(
            identifier,
            searchText,
            options,
            false); // false=we just reset, so don't do it again.
        return;
    }

    RefPtr<Range> searchRange(rangeOfContents(frame()->document()));

    ExceptionCode ec = 0, ec2 = 0;
    if (m_resumeScopingFromRange.get()) {
        // This is a continuation of a scoping operation that timed out and didn't
        // complete last time around, so we should start from where we left off.
        searchRange->setStart(m_resumeScopingFromRange->startContainer(),
                              m_resumeScopingFromRange->startOffset(ec2) + 1,
                              ec);
        if (ec || ec2) {
            if (ec2) // A non-zero |ec| happens when navigating during search.
                ASSERT_NOT_REACHED();
            return;
        }
    }

    // This timeout controls how long we scope before releasing control.  This
    // value does not prevent us from running for longer than this, but it is
    // periodically checked to see if we have exceeded our allocated time.
    const double maxScopingDuration = 0.1; // seconds

    int matchCount = 0;
    bool timedOut = false;
    double startTime = currentTime();
    do {
        // Find next occurrence of the search string.
        // FIXME: (http://b/1088245) This WebKit operation may run for longer
        // than the timeout value, and is not interruptible as it is currently
        // written. We may need to rewrite it with interruptibility in mind, or
        // find an alternative.
        RefPtr<Range> resultRange(findPlainText(searchRange.get(),
                                                searchText,
                                                true,
                                                options.matchCase));
        if (resultRange->collapsed(ec)) {
            if (!resultRange->startContainer()->isInShadowTree())
                break;

            searchRange = rangeOfContents(frame()->document());
            searchRange->setStartAfter(
                resultRange->startContainer()->shadowAncestorNode(), ec);
            continue;
        }

        // A non-collapsed result range can in some funky whitespace cases still not
        // advance the range's start position (4509328). Break to avoid infinite
        // loop. (This function is based on the implementation of
        // Frame::markAllMatchesForText, which is where this safeguard comes from).
        VisiblePosition newStart = endVisiblePosition(resultRange.get(), DOWNSTREAM);
        if (newStart == startVisiblePosition(searchRange.get(), DOWNSTREAM))
            break;

        // Only treat the result as a match if it is visible
        if (frame()->editor()->insideVisibleArea(resultRange.get())) {
            ++matchCount;

            setStart(searchRange.get(), newStart);
            Node* shadowTreeRoot = searchRange->shadowTreeRootNode();
            if (searchRange->collapsed(ec) && shadowTreeRoot)
                searchRange->setEnd(shadowTreeRoot, shadowTreeRoot->childNodeCount(), ec);

            // Catch a special case where Find found something but doesn't know what
            // the bounding box for it is. In this case we set the first match we find
            // as the active rect.
            IntRect resultBounds = resultRange->boundingBox();
            IntRect activeSelectionRect;
            if (m_locatingActiveRect) {
                activeSelectionRect = m_activeMatch.get() ?
                    m_activeMatch->boundingBox() : resultBounds;
            }

            // If the Find function found a match it will have stored where the
            // match was found in m_activeSelectionRect on the current frame. If we
            // find this rect during scoping it means we have found the active
            // tickmark.
            bool foundActiveMatch = false;
            if (m_locatingActiveRect && (activeSelectionRect == resultBounds)) {
                // We have found the active tickmark frame.
                mainFrameImpl->m_activeMatchFrame = this;
                foundActiveMatch = true;
                // We also know which tickmark is active now.
                m_activeMatchIndex = matchCount - 1;
                // To stop looking for the active tickmark, we set this flag.
                m_locatingActiveRect = false;

                // Notify browser of new location for the selected rectangle.
                resultBounds.move(-frameView()->scrollOffset().width(),
                                  -frameView()->scrollOffset().height());
                reportFindInPageSelection(
                    frame()->view()->convertToContainingWindow(resultBounds),
                    m_activeMatchIndex + 1,
                    identifier);
            }

            addMarker(resultRange.get(), foundActiveMatch);
        }

        m_resumeScopingFromRange = resultRange;
        timedOut = (currentTime() - startTime) >= maxScopingDuration;
    } while (!timedOut);

    // Remember what we search for last time, so we can skip searching if more
    // letters are added to the search string (and last outcome was 0).
    m_lastSearchString = searchText;

    if (matchCount > 0) {
        frame()->editor()->setMarkedTextMatchesAreHighlighted(true);

        m_lastMatchCount += matchCount;

        // Let the mainframe know how much we found during this pass.
        mainFrameImpl->increaseMatchCount(matchCount, identifier);
    }

    if (timedOut) {
        // If we found anything during this pass, we should redraw. However, we
        // don't want to spam too much if the page is extremely long, so if we
        // reach a certain point we start throttling the redraw requests.
        if (matchCount > 0)
            invalidateIfNecessary();

        // Scoping effort ran out of time, lets ask for another time-slice.
        scopeStringMatchesSoon(
            identifier,
            searchText,
            options,
            false); // don't reset.
        return; // Done for now, resume work later.
    }

    // This frame has no further scoping left, so it is done. Other frames might,
    // of course, continue to scope matches.
    m_scopingComplete = true;
    mainFrameImpl->m_framesScopingCount--;

    // If this is the last frame to finish scoping we need to trigger the final
    // update to be sent.
    if (!mainFrameImpl->m_framesScopingCount)
        mainFrameImpl->increaseMatchCount(0, identifier);

    // This frame is done, so show any scrollbar tickmarks we haven't drawn yet.
    invalidateArea(InvalidateScrollbar);
}

void WebFrameImpl::cancelPendingScopingEffort()
{
    deleteAllValues(m_deferredScopingWork);
    m_deferredScopingWork.clear();

    m_activeMatchIndex = -1;
}

void WebFrameImpl::increaseMatchCount(int count, int identifier)
{
    // This function should only be called on the mainframe.
    ASSERT(!parent());

    m_totalMatchCount += count;

    // Update the UI with the latest findings.
    if (client())
        client()->reportFindInPageMatchCount(identifier, m_totalMatchCount, !m_framesScopingCount);
}

void WebFrameImpl::reportFindInPageSelection(const WebRect& selectionRect,
                                             int activeMatchOrdinal,
                                             int identifier)
{
    // Update the UI with the latest selection rect.
    if (client())
        client()->reportFindInPageSelection(identifier, ordinalOfFirstMatchForFrame(this) + activeMatchOrdinal, selectionRect);
}

void WebFrameImpl::resetMatchCount()
{
    m_totalMatchCount = 0;
    m_framesScopingCount = 0;
}

WebString WebFrameImpl::contentAsText(size_t maxChars) const
{
    if (!m_frame)
        return WebString();

    Vector<UChar> text;
    frameContentAsPlainText(maxChars, m_frame, &text);
    return String::adopt(text);
}

WebString WebFrameImpl::contentAsMarkup() const
{
    return createFullMarkup(m_frame->document());
}

WebString WebFrameImpl::renderTreeAsText() const
{
    return externalRepresentation(m_frame);
}

WebString WebFrameImpl::counterValueForElementById(const WebString& id) const
{
    if (!m_frame)
        return WebString();

    Element* element = m_frame->document()->getElementById(id);
    if (!element)
        return WebString();

    return counterValueForElement(element);
}

WebString WebFrameImpl::markerTextForListItem(const WebElement& webElement) const
{
    return WebCore::markerTextForListItem(const_cast<Element*>(webElement.constUnwrap<Element>()));
}

int WebFrameImpl::pageNumberForElementById(const WebString& id,
                                           float pageWidthInPixels,
                                           float pageHeightInPixels) const
{
    if (!m_frame)
        return -1;

    Element* element = m_frame->document()->getElementById(id);
    if (!element)
        return -1;

    FloatSize pageSize(pageWidthInPixels, pageHeightInPixels);
    return PrintContext::pageNumberForElement(element, pageSize);
}

WebRect WebFrameImpl::selectionBoundsRect() const
{
    if (hasSelection())
        return IntRect(frame()->selection()->bounds(false));

    return WebRect();
}

bool WebFrameImpl::selectionStartHasSpellingMarkerFor(int from, int length) const
{
    if (!m_frame)
        return false;
    return m_frame->editor()->selectionStartHasSpellingMarkerFor(from, length);
}

bool WebFrameImpl::pauseSVGAnimation(const WebString& animationId, double time, const WebString& elementId)
{
#if !ENABLE(SVG)
    return false;
#else
    if (!m_frame)
        return false;

    Document* document = m_frame->document();
    if (!document || !document->svgExtensions())
        return false;

    Node* coreNode = document->getElementById(animationId);
    if (!coreNode || !SVGSMILElement::isSMILElement(coreNode))
        return false;

    return document->accessSVGExtensions()->sampleAnimationAtTime(elementId, static_cast<SVGSMILElement*>(coreNode), time);
#endif
}

// WebFrameImpl public ---------------------------------------------------------

PassRefPtr<WebFrameImpl> WebFrameImpl::create(WebFrameClient* client)
{
    return adoptRef(new WebFrameImpl(client));
}

WebFrameImpl::WebFrameImpl(WebFrameClient* client)
    : m_frameLoaderClient(this)
    , m_client(client)
    , m_activeMatchFrame(0)
    , m_activeMatchIndex(-1)
    , m_locatingActiveRect(false)
    , m_resumeScopingFromRange(0)
    , m_lastMatchCount(-1)
    , m_totalMatchCount(-1)
    , m_framesScopingCount(-1)
    , m_scopingComplete(false)
    , m_nextInvalidateAfter(0)
    , m_animationController(this)
    , m_identifier(generateFrameIdentifier())
{
    ChromiumBridge::incrementStatsCounter(webFrameActiveCount);
    frameCount++;
}

WebFrameImpl::~WebFrameImpl()
{
    ChromiumBridge::decrementStatsCounter(webFrameActiveCount);
    frameCount--;

    cancelPendingScopingEffort();
    clearPasswordListeners();
}

void WebFrameImpl::initializeAsMainFrame(WebViewImpl* webViewImpl)
{
    RefPtr<Frame> frame = Frame::create(webViewImpl->page(), 0, &m_frameLoaderClient);
    m_frame = frame.get();

    // Add reference on behalf of FrameLoader.  See comments in
    // WebFrameLoaderClient::frameLoaderDestroyed for more info.
    ref();

    // We must call init() after m_frame is assigned because it is referenced
    // during init().
    m_frame->init();
}

PassRefPtr<Frame> WebFrameImpl::createChildFrame(
    const FrameLoadRequest& request, HTMLFrameOwnerElement* ownerElement)
{
    RefPtr<WebFrameImpl> webframe(adoptRef(new WebFrameImpl(m_client)));

    // Add an extra ref on behalf of the Frame/FrameLoader, which references the
    // WebFrame via the FrameLoaderClient interface. See the comment at the top
    // of this file for more info.
    webframe->ref();

    RefPtr<Frame> childFrame = Frame::create(
        m_frame->page(), ownerElement, &webframe->m_frameLoaderClient);
    webframe->m_frame = childFrame.get();

    childFrame->tree()->setName(request.frameName());

    m_frame->tree()->appendChild(childFrame);

    // Frame::init() can trigger onload event in the parent frame,
    // which may detach this frame and trigger a null-pointer access
    // in FrameTree::removeChild. Move init() after appendChild call
    // so that webframe->mFrame is in the tree before triggering
    // onload event handler.
    // Because the event handler may set webframe->mFrame to null,
    // it is necessary to check the value after calling init() and
    // return without loading URL.
    // (b:791612)
    childFrame->init(); // create an empty document
    if (!childFrame->tree()->parent())
        return 0;

    m_frame->loader()->loadURLIntoChildFrame(
        request.resourceRequest().url(),
        request.resourceRequest().httpReferrer(),
        childFrame.get());

    // A synchronous navigation (about:blank) would have already processed
    // onload, so it is possible for the frame to have already been destroyed by
    // script in the page.
    if (!childFrame->tree()->parent())
        return 0;

    return childFrame.release();
}

void WebFrameImpl::layout()
{
    // layout this frame
    FrameView* view = m_frame->view();
    if (view)
        view->updateLayoutAndStyleIfNeededRecursive();
}

void WebFrameImpl::paintWithContext(GraphicsContext& gc, const WebRect& rect)
{
    IntRect dirtyRect(rect);
    gc.save();
    if (m_frame->document() && frameView()) {
        gc.clip(dirtyRect);
        frameView()->paint(&gc, dirtyRect);
        m_frame->page()->inspectorController()->drawNodeHighlight(gc);
    } else
        gc.fillRect(dirtyRect, Color::white, DeviceColorSpace);
    gc.restore();
}

void WebFrameImpl::paint(WebCanvas* canvas, const WebRect& rect)
{
    if (rect.isEmpty())
        return;
#if WEBKIT_USING_CG
    GraphicsContext gc(canvas);
    LocalCurrentGraphicsContext localContext(&gc);
#elif WEBKIT_USING_SKIA
    PlatformContextSkia context(canvas);

    // PlatformGraphicsContext is actually a pointer to PlatformContextSkia
    GraphicsContext gc(reinterpret_cast<PlatformGraphicsContext*>(&context));
#else
    notImplemented();
#endif
    paintWithContext(gc, rect);
}

void WebFrameImpl::createFrameView()
{
    ASSERT(m_frame); // If m_frame doesn't exist, we probably didn't init properly.

    Page* page = m_frame->page();
    ASSERT(page);
    ASSERT(page->mainFrame());

    bool isMainFrame = m_frame == page->mainFrame();
    if (isMainFrame && m_frame->view())
        m_frame->view()->setParentVisible(false);

    m_frame->setView(0);

    WebViewImpl* webView = viewImpl();

    RefPtr<FrameView> view;
    if (isMainFrame)
        view = FrameView::create(m_frame, webView->size());
    else
        view = FrameView::create(m_frame);

    m_frame->setView(view);

    if (webView->isTransparent())
        view->setTransparent(true);

    // FIXME: The Mac code has a comment about this possibly being unnecessary.
    // See installInFrame in WebCoreFrameBridge.mm
    if (m_frame->ownerRenderer())
        m_frame->ownerRenderer()->setWidget(view.get());

    if (HTMLFrameOwnerElement* owner = m_frame->ownerElement())
        view->setCanHaveScrollbars(owner->scrollingMode() != ScrollbarAlwaysOff);

    if (isMainFrame)
        view->setParentVisible(true);
}

WebFrameImpl* WebFrameImpl::fromFrame(Frame* frame)
{
    if (!frame)
        return 0;

    return static_cast<FrameLoaderClientImpl*>(frame->loader()->client())->webFrame();
}

WebFrameImpl* WebFrameImpl::fromFrameOwnerElement(Element* element)
{
    if (!element
        || !element->isFrameOwnerElement()
        || (!element->hasTagName(HTMLNames::iframeTag)
            && !element->hasTagName(HTMLNames::frameTag)))
        return 0;

    HTMLFrameOwnerElement* frameElement =
        static_cast<HTMLFrameOwnerElement*>(element);
    return fromFrame(frameElement->contentFrame());
}

WebViewImpl* WebFrameImpl::viewImpl() const
{
    if (!m_frame)
        return 0;

    return WebViewImpl::fromPage(m_frame->page());
}

WebDataSourceImpl* WebFrameImpl::dataSourceImpl() const
{
    return static_cast<WebDataSourceImpl*>(dataSource());
}

WebDataSourceImpl* WebFrameImpl::provisionalDataSourceImpl() const
{
    return static_cast<WebDataSourceImpl*>(provisionalDataSource());
}

void WebFrameImpl::setFindEndstateFocusAndSelection()
{
    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();

    if (this == mainFrameImpl->activeMatchFrame() && m_activeMatch.get()) {
        // If the user has set the selection since the match was found, we
        // don't focus anything.
        VisibleSelection selection(frame()->selection()->selection());
        if (!selection.isNone())
            return;

        // Try to find the first focusable node up the chain, which will, for
        // example, focus links if we have found text within the link.
        Node* node = m_activeMatch->firstNode();
        while (node && !node->isFocusable() && node != frame()->document())
            node = node->parent();

        if (node && node != frame()->document()) {
            // Found a focusable parent node. Set focus to it.
            frame()->document()->setFocusedNode(node);
            return;
        }

        // Iterate over all the nodes in the range until we find a focusable node.
        // This, for example, sets focus to the first link if you search for
        // text and text that is within one or more links.
        node = m_activeMatch->firstNode();
        while (node && node != m_activeMatch->pastLastNode()) {
            if (node->isFocusable()) {
                frame()->document()->setFocusedNode(node);
                return;
            }
            node = node->traverseNextNode();
        }

        // No node related to the active match was focusable, so set the
        // active match as the selection (so that when you end the Find session,
        // you'll have the last thing you found highlighted) and make sure that
        // we have nothing focused (otherwise you might have text selected but
        // a link focused, which is weird).
        frame()->selection()->setSelection(m_activeMatch.get());
        frame()->document()->setFocusedNode(0);
    }
}

void WebFrameImpl::didFail(const ResourceError& error, bool wasProvisional)
{
    if (!client())
        return;
    WebURLError webError = error;
    if (wasProvisional)
        client()->didFailProvisionalLoad(this, webError);
    else
        client()->didFailLoad(this, webError);
}

void WebFrameImpl::setCanHaveScrollbars(bool canHaveScrollbars)
{
    m_frame->view()->setCanHaveScrollbars(canHaveScrollbars);
}

bool WebFrameImpl::registerPasswordListener(
    WebInputElement inputElement,
    WebPasswordAutocompleteListener* listener)
{
    RefPtr<HTMLInputElement> element(inputElement.unwrap<HTMLInputElement>());
    if (!m_passwordListeners.add(element, listener).second) {
        delete listener;
        return false;
    }
    return true;
}

void WebFrameImpl::notifiyPasswordListenerOfAutocomplete(
    const WebInputElement& inputElement)
{
    const HTMLInputElement* element = inputElement.constUnwrap<HTMLInputElement>();
    WebPasswordAutocompleteListener* listener = getPasswordListener(element);
    // Password listeners need to autocomplete other fields that depend on the
    // input element with autofill suggestions.
    if (listener)
        listener->performInlineAutocomplete(element->value(), false, false);
}

WebPasswordAutocompleteListener* WebFrameImpl::getPasswordListener(
    const HTMLInputElement* inputElement)
{
    return m_passwordListeners.get(RefPtr<HTMLInputElement>(const_cast<HTMLInputElement*>(inputElement)));
}

// WebFrameImpl private --------------------------------------------------------

void WebFrameImpl::closing()
{
    m_frame = 0;
}

void WebFrameImpl::invalidateArea(AreaToInvalidate area)
{
    ASSERT(frame() && frame()->view());
    FrameView* view = frame()->view();

    if ((area & InvalidateAll) == InvalidateAll)
        view->invalidateRect(view->frameRect());
    else {
        if ((area & InvalidateContentArea) == InvalidateContentArea) {
            IntRect contentArea(
                view->x(), view->y(), view->visibleWidth(), view->visibleHeight());
            IntRect frameRect = view->frameRect();
            contentArea.move(-frameRect.topLeft().x(), -frameRect.topLeft().y());
            view->invalidateRect(contentArea);
        }

        if ((area & InvalidateScrollbar) == InvalidateScrollbar) {
            // Invalidate the vertical scroll bar region for the view.
            IntRect scrollBarVert(
                view->x() + view->visibleWidth(), view->y(),
                ScrollbarTheme::nativeTheme()->scrollbarThickness(),
                view->visibleHeight());
            IntRect frameRect = view->frameRect();
            scrollBarVert.move(-frameRect.topLeft().x(), -frameRect.topLeft().y());
            view->invalidateRect(scrollBarVert);
        }
    }
}

void WebFrameImpl::addMarker(Range* range, bool activeMatch)
{
    // Use a TextIterator to visit the potentially multiple nodes the range
    // covers.
    TextIterator markedText(range);
    for (; !markedText.atEnd(); markedText.advance()) {
        RefPtr<Range> textPiece = markedText.range();
        int exception = 0;

        DocumentMarker marker = {
            DocumentMarker::TextMatch,
            textPiece->startOffset(exception),
            textPiece->endOffset(exception),
            "",
            activeMatch
        };

        if (marker.endOffset > marker.startOffset) {
            // Find the node to add a marker to and add it.
            Node* node = textPiece->startContainer(exception);
            frame()->document()->markers()->addMarker(node, marker);

            // Rendered rects for markers in WebKit are not populated until each time
            // the markers are painted. However, we need it to happen sooner, because
            // the whole purpose of tickmarks on the scrollbar is to show where
            // matches off-screen are (that haven't been painted yet).
            Vector<DocumentMarker> markers = frame()->document()->markers()->markersForNode(node);
            frame()->document()->markers()->setRenderedRectForMarker(
                textPiece->startContainer(exception),
                markers[markers.size() - 1],
                range->boundingBox());
        }
    }
}

void WebFrameImpl::setMarkerActive(Range* range, bool active)
{
    WebCore::ExceptionCode ec;
    if (!range || range->collapsed(ec))
        return;

    frame()->document()->markers()->setMarkersActive(range, active);
}

int WebFrameImpl::ordinalOfFirstMatchForFrame(WebFrameImpl* frame) const
{
    int ordinal = 0;
    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();
    // Iterate from the main frame up to (but not including) |frame| and
    // add up the number of matches found so far.
    for (WebFrameImpl* it = mainFrameImpl;
         it != frame;
         it = static_cast<WebFrameImpl*>(it->traverseNext(true))) {
        if (it->m_lastMatchCount > 0)
            ordinal += it->m_lastMatchCount;
    }
    return ordinal;
}

bool WebFrameImpl::shouldScopeMatches(const String& searchText)
{
    // Don't scope if we can't find a frame or a view or if the frame is not visible.
    // The user may have closed the tab/application, so abort.
    if (!frame() || !frame()->view() || !hasVisibleContent())
        return false;

    ASSERT(frame()->document() && frame()->view());

    // If the frame completed the scoping operation and found 0 matches the last
    // time it was searched, then we don't have to search it again if the user is
    // just adding to the search string or sending the same search string again.
    if (m_scopingComplete && !m_lastSearchString.isEmpty() && !m_lastMatchCount) {
        // Check to see if the search string prefixes match.
        String previousSearchPrefix =
            searchText.substring(0, m_lastSearchString.length());

        if (previousSearchPrefix == m_lastSearchString)
            return false; // Don't search this frame, it will be fruitless.
    }

    return true;
}

void WebFrameImpl::scopeStringMatchesSoon(int identifier, const WebString& searchText,
                                          const WebFindOptions& options, bool reset)
{
    m_deferredScopingWork.append(new DeferredScopeStringMatches(
        this, identifier, searchText, options, reset));
}

void WebFrameImpl::callScopeStringMatches(DeferredScopeStringMatches* caller,
                                          int identifier, const WebString& searchText,
                                          const WebFindOptions& options, bool reset)
{
    m_deferredScopingWork.remove(m_deferredScopingWork.find(caller));

    scopeStringMatches(identifier, searchText, options, reset);

    // This needs to happen last since searchText is passed by reference.
    delete caller;
}

void WebFrameImpl::invalidateIfNecessary()
{
    if (m_lastMatchCount > m_nextInvalidateAfter) {
        // FIXME: (http://b/1088165) Optimize the drawing of the tickmarks and
        // remove this. This calculation sets a milestone for when next to
        // invalidate the scrollbar and the content area. We do this so that we
        // don't spend too much time drawing the scrollbar over and over again.
        // Basically, up until the first 500 matches there is no throttle.
        // After the first 500 matches, we set set the milestone further and
        // further out (750, 1125, 1688, 2K, 3K).
        static const int startSlowingDownAfter = 500;
        static const int slowdown = 750;
        int i = (m_lastMatchCount / startSlowingDownAfter);
        m_nextInvalidateAfter += i * slowdown;

        invalidateArea(InvalidateScrollbar);
    }
}

void WebFrameImpl::clearPasswordListeners()
{
    deleteAllValues(m_passwordListeners);
    m_passwordListeners.clear();
}

void WebFrameImpl::loadJavaScriptURL(const KURL& url)
{
    // This is copied from ScriptController::executeIfJavaScriptURL.
    // Unfortunately, we cannot just use that method since it is private, and
    // it also doesn't quite behave as we require it to for bookmarklets.  The
    // key difference is that we need to suppress loading the string result
    // from evaluating the JS URL if executing the JS URL resulted in a
    // location change.  We also allow a JS URL to be loaded even if scripts on
    // the page are otherwise disabled.

    if (!m_frame->document() || !m_frame->page())
        return;

    String script = decodeURLEscapeSequences(url.string().substring(strlen("javascript:")));
    ScriptValue result = m_frame->script()->executeScript(script, true);

    String scriptResult;
    if (!result.getString(scriptResult))
        return;

    if (!m_frame->redirectScheduler()->locationChangePending())
        m_frame->loader()->writer()->replaceDocument(scriptResult);
}

} // namespace WebKit
