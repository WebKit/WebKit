/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "DOMWindow.h"

#include "BarInfo.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSRuleList.h"
#include "CSSStyleSelector.h"
#include "CString.h"
#include "Chrome.h"
#include "Console.h"
#include "DOMSelection.h"
#include "Document.h"
#include "Element.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "History.h"
#include "InspectorController.h"
#include "Location.h"
#include "MessageEvent.h"
#include "Navigator.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformScreen.h"
#include "PlatformString.h"
#include "Screen.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "WebKitPoint.h"
#include <algorithm>
#include <wtf/MathExtras.h>

#if ENABLE(DATABASE)
#include "Database.h"
#endif

#if ENABLE(DOM_STORAGE)
#include "LocalStorage.h"
#include "SessionStorage.h"
#include "Storage.h"
#include "StorageArea.h"
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
#include "DOMApplicationCache.h"
#endif

using std::min;
using std::max;

namespace WebCore {

class PostMessageTimer : public TimerBase {
public:
    PostMessageTimer(DOMWindow* window, PassRefPtr<MessageEvent> event, SecurityOrigin* targetOrigin)
        : m_window(window)
        , m_event(event)
        , m_targetOrigin(targetOrigin)
    {
    }

    MessageEvent* event() const { return m_event.get(); }
    SecurityOrigin* targetOrigin() const { return m_targetOrigin.get(); }

private:
    virtual void fired()
    {
        m_window->postMessageTimerFired(this);
    }

    RefPtr<DOMWindow> m_window;
    RefPtr<MessageEvent> m_event;
    RefPtr<SecurityOrigin> m_targetOrigin;
};

// This function:
// 1) Validates the pending changes are not changing to NaN
// 2) Constrains the window rect to no smaller than 100 in each dimension and no
//    bigger than the the float rect's dimensions.
// 3) Constrain window rect to within the top and left boundaries of the screen rect
// 4) Constraint the window rect to within the bottom and right boundaries of the
//    screen rect.
// 5) Translate the window rect coordinates to be within the coordinate space of
//    the screen rect.
void DOMWindow::adjustWindowRect(const FloatRect& screen, FloatRect& window, const FloatRect& pendingChanges)
{
    // Make sure we're in a valid state before adjusting dimensions.
    ASSERT(isfinite(screen.x()));
    ASSERT(isfinite(screen.y()));
    ASSERT(isfinite(screen.width()));
    ASSERT(isfinite(screen.height()));
    ASSERT(isfinite(window.x()));
    ASSERT(isfinite(window.y()));
    ASSERT(isfinite(window.width()));
    ASSERT(isfinite(window.height()));
    
    // Update window values if new requested values are not NaN.
    if (!isnan(pendingChanges.x()))
        window.setX(pendingChanges.x());
    if (!isnan(pendingChanges.y()))
        window.setY(pendingChanges.y());
    if (!isnan(pendingChanges.width()))
        window.setWidth(pendingChanges.width());
    if (!isnan(pendingChanges.height()))
        window.setHeight(pendingChanges.height());
    
    // Resize the window to between 100 and the screen width and height.
    window.setWidth(min(max(100.0f, window.width()), screen.width()));
    window.setHeight(min(max(100.0f, window.height()), screen.height()));
    
    // Constrain the window position to the screen.
    window.setX(max(screen.x(), min(window.x(), screen.right() - window.width())));
    window.setY(max(screen.y(), min(window.y(), screen.bottom() - window.height())));
}

DOMWindow::DOMWindow(Frame* frame)
    : m_frame(frame)
{
}

DOMWindow::~DOMWindow()
{
    if (m_frame)
        m_frame->clearFormerDOMWindow(this);
}

void DOMWindow::disconnectFrame()
{
    m_frame = 0;
    clear();
}

void DOMWindow::clear()
{
    if (m_screen)
        m_screen->disconnectFrame();
    m_screen = 0;

    if (m_selection)
        m_selection->disconnectFrame();
    m_selection = 0;

    if (m_history)
        m_history->disconnectFrame();
    m_history = 0;

    if (m_locationbar)
        m_locationbar->disconnectFrame();
    m_locationbar = 0;

    if (m_menubar)
        m_menubar->disconnectFrame();
    m_menubar = 0;

    if (m_personalbar)
        m_personalbar->disconnectFrame();
    m_personalbar = 0;

    if (m_scrollbars)
        m_scrollbars->disconnectFrame();
    m_scrollbars = 0;

    if (m_statusbar)
        m_statusbar->disconnectFrame();
    m_statusbar = 0;

    if (m_toolbar)
        m_toolbar->disconnectFrame();
    m_toolbar = 0;

    if (m_console)
        m_console->disconnectFrame();
    m_console = 0;

    if (m_navigator)
        m_navigator->disconnectFrame();
    m_navigator = 0;

    if (m_location)
        m_location->disconnectFrame();
    m_location = 0;
    
#if ENABLE(DOM_STORAGE)
    if (m_sessionStorage)
        m_sessionStorage->disconnectFrame();
    m_sessionStorage = 0;

    if (m_localStorage)
        m_localStorage->disconnectFrame();
    m_localStorage = 0;
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    if (m_applicationCache)
        m_applicationCache->disconnectFrame();
    m_applicationCache = 0;
#endif
}

Screen* DOMWindow::screen() const
{
    if (!m_screen)
        m_screen = Screen::create(m_frame);
    return m_screen.get();
}

History* DOMWindow::history() const
{
    if (!m_history)
        m_history = History::create(m_frame);
    return m_history.get();
}

BarInfo* DOMWindow::locationbar() const
{
    if (!m_locationbar)
        m_locationbar = BarInfo::create(m_frame, BarInfo::Locationbar);
    return m_locationbar.get();
}

BarInfo* DOMWindow::menubar() const
{
    if (!m_menubar)
        m_menubar = BarInfo::create(m_frame, BarInfo::Menubar);
    return m_menubar.get();
}

BarInfo* DOMWindow::personalbar() const
{
    if (!m_personalbar)
        m_personalbar = BarInfo::create(m_frame, BarInfo::Personalbar);
    return m_personalbar.get();
}

BarInfo* DOMWindow::scrollbars() const
{
    if (!m_scrollbars)
        m_scrollbars = BarInfo::create(m_frame, BarInfo::Scrollbars);
    return m_scrollbars.get();
}

BarInfo* DOMWindow::statusbar() const
{
    if (!m_statusbar)
        m_statusbar = BarInfo::create(m_frame, BarInfo::Statusbar);
    return m_statusbar.get();
}

BarInfo* DOMWindow::toolbar() const
{
    if (!m_toolbar)
        m_toolbar = BarInfo::create(m_frame, BarInfo::Toolbar);
    return m_toolbar.get();
}

Console* DOMWindow::console() const
{
    if (!m_console)
        m_console = Console::create(m_frame);
    return m_console.get();
}

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
DOMApplicationCache* DOMWindow::applicationCache() const
{
    if (!m_applicationCache)
        m_applicationCache = DOMApplicationCache::create(m_frame);
    return m_applicationCache.get();
}
#endif

Navigator* DOMWindow::navigator() const
{
    if (!m_navigator)
        m_navigator = Navigator::create(m_frame);
    return m_navigator.get();
}

Location* DOMWindow::location() const
{
    if (!m_location)
        m_location = Location::create(m_frame);
    return m_location.get();
}

#if ENABLE(DOM_STORAGE)
Storage* DOMWindow::sessionStorage() const
{
    if (m_sessionStorage)
        return m_sessionStorage.get();
        
    Page* page = m_frame->page();
    if (!page)
        return 0;

    Document* document = m_frame->document();

    RefPtr<StorageArea> storageArea = page->sessionStorage()->storageArea(document->securityOrigin());
    page->inspectorController()->didUseDOMStorage(storageArea.get(), false, m_frame);

    m_sessionStorage = Storage::create(m_frame, storageArea.release());
    return m_sessionStorage.get();
}

Storage* DOMWindow::localStorage() const
{
    Document* document = this->document();
    if (!document)
        return 0;
        
    Page* page = document->page();
    if (!page)
        return 0;

    Settings* settings = document->settings();
    if (!settings || !settings->localStorageEnabled())
        return 0;

    LocalStorage* localStorage = page->group().localStorage();
    RefPtr<StorageArea> storageArea = localStorage ? localStorage->storageArea(document->securityOrigin()) : 0; 
    if (storageArea) {
        page->inspectorController()->didUseDOMStorage(storageArea.get(), true, m_frame);
        m_localStorage = Storage::create(m_frame, storageArea.release());
    }

    return m_localStorage.get();
}
#endif

void DOMWindow::postMessage(const String& message, MessagePort* messagePort, const String& targetOrigin, DOMWindow* source, ExceptionCode& ec)
{
    if (!m_frame)
        return;

    // Compute the target origin.  We need to do this synchronously in order
    // to generate the SYNTAX_ERR exception correctly.
    RefPtr<SecurityOrigin> target;
    if (targetOrigin != "*") {
        target = SecurityOrigin::createFromString(targetOrigin);
        if (target->isEmpty()) {
            ec = SYNTAX_ERR;
            return;
        }
    }

    RefPtr<MessagePort> newMessagePort;
    if (messagePort)
        newMessagePort = messagePort->clone(ec);
    if (ec)
        return;

    // Capture the source of the message.  We need to do this synchronously
    // in order to capture the source of the message correctly.
    Document* sourceDocument = source->document();
    if (!sourceDocument)
        return;
    String sourceOrigin = sourceDocument->securityOrigin()->toString();

    // Schedule the message.
    PostMessageTimer* timer = new PostMessageTimer(this, MessageEvent::create(message, sourceOrigin, "", source, newMessagePort), target.get());
    timer->startOneShot(0);
}

void DOMWindow::postMessageTimerFired(PostMessageTimer* t)
{
    OwnPtr<PostMessageTimer> timer(t);

    if (!document())
        return;

    if (timer->targetOrigin()) {
        // Check target origin now since the target document may have changed since the simer was scheduled.
        if (!timer->targetOrigin()->isSameSchemeHostPort(document()->securityOrigin())) {
            String message = String::format("Unable to post message to %s. Recipient has origin %s.\n", 
                timer->targetOrigin()->toString().utf8().data(), document()->securityOrigin()->toString().utf8().data());
            console()->addMessage(JSMessageSource, ErrorMessageLevel, message, 0, String());
            return;
        }
    }

    MessagePort* messagePort = timer->event()->messagePort();
    ASSERT(!messagePort || !messagePort->scriptExecutionContext());
    if (messagePort)
        messagePort->attachToContext(document());

    document()->dispatchWindowEvent(timer->event());
}

DOMSelection* DOMWindow::getSelection()
{
    if (!m_selection)
        m_selection = DOMSelection::create(m_frame);
    return m_selection.get();
}

Element* DOMWindow::frameElement() const
{
    if (!m_frame)
        return 0;

    return m_frame->ownerElement();
}

void DOMWindow::focus()
{
    if (!m_frame)
        return;

    m_frame->focusWindow();
}

void DOMWindow::blur()
{
    if (!m_frame)
        return;

    m_frame->unfocusWindow();
}

void DOMWindow::close()
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (m_frame != page->mainFrame())
        return;

    Settings* settings = m_frame->settings();
    bool allowScriptsToCloseWindows =
        settings && settings->allowScriptsToCloseWindows();

    if (m_frame->loader()->openedByDOM()
        || m_frame->loader()->getHistoryLength() <= 1
        || allowScriptsToCloseWindows)
        m_frame->scheduleClose();
}

void DOMWindow::print()
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    page->chrome()->print(m_frame);
}

void DOMWindow::stop()
{
    if (!m_frame)
        return;

    // We must check whether the load is complete asynchronously, because we might still be parsing
    // the document until the callstack unwinds.
    m_frame->loader()->stopForUserCancel(true);
}

void DOMWindow::alert(const String& message)
{
    if (!m_frame)
        return;

    m_frame->document()->updateRendering();

    Page* page = m_frame->page();
    if (!page)
        return;

    page->chrome()->runJavaScriptAlert(m_frame, message);
}

bool DOMWindow::confirm(const String& message)
{
    if (!m_frame)
        return false;

    m_frame->document()->updateRendering();

    Page* page = m_frame->page();
    if (!page)
        return false;

    return page->chrome()->runJavaScriptConfirm(m_frame, message);
}

String DOMWindow::prompt(const String& message, const String& defaultValue)
{
    if (!m_frame)
        return String();

    m_frame->document()->updateRendering();

    Page* page = m_frame->page();
    if (!page)
        return String();

    String returnValue;
    if (page->chrome()->runJavaScriptPrompt(m_frame, message, defaultValue, returnValue))
        return returnValue;

    return String();
}

bool DOMWindow::find(const String& string, bool caseSensitive, bool backwards, bool wrap, bool /*wholeWord*/, bool /*searchInFrames*/, bool /*showDialog*/) const
{
    if (!m_frame)
        return false;

    // FIXME (13016): Support wholeWord, searchInFrames and showDialog
    return m_frame->findString(string, !backwards, caseSensitive, wrap, false);
}

bool DOMWindow::offscreenBuffering() const
{
    return true;
}

int DOMWindow::outerHeight() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome()->windowRect().height());
}

int DOMWindow::outerWidth() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome()->windowRect().width());
}

int DOMWindow::innerHeight() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;
    
    return static_cast<int>(view->height() / m_frame->pageZoomFactor());
}

int DOMWindow::innerWidth() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    return static_cast<int>(view->width() / m_frame->pageZoomFactor());
}

int DOMWindow::screenX() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome()->windowRect().x());
}

int DOMWindow::screenY() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome()->windowRect().y());
}

int DOMWindow::scrollX() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    return static_cast<int>(view->scrollX() / m_frame->pageZoomFactor());
}

int DOMWindow::scrollY() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    return static_cast<int>(view->scrollY() / m_frame->pageZoomFactor());
}

bool DOMWindow::closed() const
{
    return !m_frame;
}

unsigned DOMWindow::length() const
{
    if (!m_frame)
        return 0;

    return m_frame->tree()->childCount();
}

String DOMWindow::name() const
{
    if (!m_frame)
        return String();

    return m_frame->tree()->name();
}

void DOMWindow::setName(const String& string)
{
    if (!m_frame)
        return;

    m_frame->tree()->setName(string);
}

String DOMWindow::status() const
{
    if (!m_frame)
        return String();

    return m_frame->jsStatusBarText();
}

void DOMWindow::setStatus(const String& string) 
{ 
    if (!m_frame) 
        return; 

    m_frame->setJSStatusBarText(string); 
} 
    
String DOMWindow::defaultStatus() const
{
    if (!m_frame)
        return String();

    return m_frame->jsDefaultStatusBarText();
} 

void DOMWindow::setDefaultStatus(const String& string) 
{ 
    if (!m_frame) 
        return; 

    m_frame->setJSDefaultStatusBarText(string);
}

DOMWindow* DOMWindow::self() const
{
    if (!m_frame)
        return 0;

    return m_frame->domWindow();
}

DOMWindow* DOMWindow::opener() const
{
    if (!m_frame)
        return 0;

    Frame* opener = m_frame->loader()->opener();
    if (!opener)
        return 0;

    return opener->domWindow();
}

DOMWindow* DOMWindow::parent() const
{
    if (!m_frame)
        return 0;

    Frame* parent = m_frame->tree()->parent(true);
    if (parent)
        return parent->domWindow();

    return m_frame->domWindow();
}

DOMWindow* DOMWindow::top() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return m_frame->tree()->top(true)->domWindow();
}

Document* DOMWindow::document() const
{
    if (!m_frame)
        return 0;

    ASSERT(m_frame->document());
    return m_frame->document();
}

PassRefPtr<CSSStyleDeclaration> DOMWindow::getComputedStyle(Element* elt, const String&) const
{
    if (!elt)
        return 0;

    // FIXME: This needs take pseudo elements into account.
    return computedStyle(elt);
}

PassRefPtr<CSSRuleList> DOMWindow::getMatchedCSSRules(Element* elt, const String& pseudoElt, bool authorOnly) const
{
    if (!m_frame)
        return 0;

    Document* doc = m_frame->document();

    if (!pseudoElt.isEmpty())
        return doc->styleSelector()->pseudoStyleRulesForElement(elt, pseudoElt, authorOnly);
    return doc->styleSelector()->styleRulesForElement(elt, authorOnly);
}

PassRefPtr<WebKitPoint> DOMWindow::webkitConvertPointFromNodeToPage(Node* node, const WebKitPoint* p) const
{
    if (!node || !p)
        return 0;
        
    FloatPoint pagePoint(p->x(), p->y());
    pagePoint = node->convertToPage(pagePoint);
    return WebKitPoint::create(pagePoint.x(), pagePoint.y());
}

PassRefPtr<WebKitPoint> DOMWindow::webkitConvertPointFromPageToNode(Node* node, const WebKitPoint* p) const
{
    if (!node || !p)
        return 0;
        
    FloatPoint nodePoint(p->x(), p->y());
    nodePoint = node->convertFromPage(nodePoint);
    return WebKitPoint::create(nodePoint.x(), nodePoint.y());
}

double DOMWindow::devicePixelRatio() const
{
    if (!m_frame)
        return 0.0;

    Page* page = m_frame->page();
    if (!page)
        return 0.0;

    return page->chrome()->scaleFactor();
}

#if ENABLE(DATABASE)
PassRefPtr<Database> DOMWindow::openDatabase(const String& name, const String& version, const String& displayName, unsigned long estimatedSize, ExceptionCode& ec)
{
    if (!m_frame)
        return 0;

    Document* doc = m_frame->document();

    Settings* settings = m_frame->settings();
    if (!settings || !settings->databasesEnabled())
        return 0;

    return Database::openDatabase(doc, name, version, displayName, estimatedSize, ec);
}
#endif

void DOMWindow::scrollBy(int x, int y) const
{
    if (!m_frame)
        return;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    FrameView* view = m_frame->view();
    if (!view)
        return;

    view->scrollBy(IntSize(x, y));
}

void DOMWindow::scrollTo(int x, int y) const
{
    if (!m_frame)
        return;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    FrameView* view = m_frame->view();
    if (!view)
        return;

    int zoomedX = static_cast<int>(x * m_frame->pageZoomFactor());
    int zoomedY = static_cast<int>(y * m_frame->pageZoomFactor());
    view->setScrollPosition(IntPoint(zoomedX, zoomedY));
}

void DOMWindow::moveBy(float x, float y) const
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (m_frame != page->mainFrame())
        return;

    FloatRect fr = page->chrome()->windowRect();
    FloatRect update = fr;
    update.move(x, y);
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    adjustWindowRect(screenAvailableRect(page->mainFrame()->view()), fr, update);
    page->chrome()->setWindowRect(fr);
}

void DOMWindow::moveTo(float x, float y) const
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (m_frame != page->mainFrame())
        return;

    FloatRect fr = page->chrome()->windowRect();
    FloatRect sr = screenAvailableRect(page->mainFrame()->view());
    fr.setLocation(sr.location());
    FloatRect update = fr;
    update.move(x, y);     
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    adjustWindowRect(sr, fr, update);
    page->chrome()->setWindowRect(fr);
}

void DOMWindow::resizeBy(float x, float y) const
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (m_frame != page->mainFrame())
        return;

    FloatRect fr = page->chrome()->windowRect();
    FloatSize dest = fr.size() + FloatSize(x, y);
    FloatRect update(fr.location(), dest);
    adjustWindowRect(screenAvailableRect(page->mainFrame()->view()), fr, update);
    page->chrome()->setWindowRect(fr);
}

void DOMWindow::resizeTo(float width, float height) const
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (m_frame != page->mainFrame())
        return;

    FloatRect fr = page->chrome()->windowRect();
    FloatSize dest = FloatSize(width, height);
    FloatRect update(fr.location(), dest);
    adjustWindowRect(screenAvailableRect(page->mainFrame()->view()), fr, update);
    page->chrome()->setWindowRect(fr);
}

inline void DOMWindow::setInlineEventListenerForType(const AtomicString& eventType, PassRefPtr<EventListener> eventListener)
{
    Document* document = this->document();
    if (!document)
        return;
    document->setWindowInlineEventListenerForType(eventType, eventListener);
}

inline EventListener* DOMWindow::inlineEventListenerForType(const AtomicString& eventType) const
{
    Document* document = this->document();
    if (!document)
        return 0;
    return document->windowInlineEventListenerForType(eventType);
}

EventListener* DOMWindow::onabort() const
{
    return inlineEventListenerForType(eventNames().abortEvent);
}

void DOMWindow::setOnabort(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().abortEvent, eventListener);
}

EventListener* DOMWindow::onblur() const
{
    return inlineEventListenerForType(eventNames().blurEvent);
}

void DOMWindow::setOnblur(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().blurEvent, eventListener);
}

EventListener* DOMWindow::onchange() const
{
    return inlineEventListenerForType(eventNames().changeEvent);
}

void DOMWindow::setOnchange(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().changeEvent, eventListener);
}

EventListener* DOMWindow::onclick() const
{
    return inlineEventListenerForType(eventNames().clickEvent);
}

void DOMWindow::setOnclick(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().clickEvent, eventListener);
}

EventListener* DOMWindow::ondblclick() const
{
    return inlineEventListenerForType(eventNames().dblclickEvent);
}

void DOMWindow::setOndblclick(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().dblclickEvent, eventListener);
}

EventListener* DOMWindow::onerror() const
{
    return inlineEventListenerForType(eventNames().errorEvent);
}

void DOMWindow::setOnerror(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().errorEvent, eventListener);
}

EventListener* DOMWindow::onfocus() const
{
    return inlineEventListenerForType(eventNames().focusEvent);
}

void DOMWindow::setOnfocus(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().focusEvent, eventListener);
}

EventListener* DOMWindow::onkeydown() const
{
    return inlineEventListenerForType(eventNames().keydownEvent);
}

void DOMWindow::setOnkeydown(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().keydownEvent, eventListener);
}

EventListener* DOMWindow::onkeypress() const
{
    return inlineEventListenerForType(eventNames().keypressEvent);
}

void DOMWindow::setOnkeypress(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().keypressEvent, eventListener);
}

EventListener* DOMWindow::onkeyup() const
{
    return inlineEventListenerForType(eventNames().keyupEvent);
}

void DOMWindow::setOnkeyup(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().keyupEvent, eventListener);
}

EventListener* DOMWindow::onload() const
{
    return inlineEventListenerForType(eventNames().loadEvent);
}

void DOMWindow::setOnload(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().loadEvent, eventListener);
}

EventListener* DOMWindow::onmousedown() const
{
    return inlineEventListenerForType(eventNames().mousedownEvent);
}

void DOMWindow::setOnmousedown(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().mousedownEvent, eventListener);
}

EventListener* DOMWindow::onmousemove() const
{
    return inlineEventListenerForType(eventNames().mousemoveEvent);
}

void DOMWindow::setOnmousemove(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().mousemoveEvent, eventListener);
}

EventListener* DOMWindow::onmouseout() const
{
    return inlineEventListenerForType(eventNames().mouseoutEvent);
}

void DOMWindow::setOnmouseout(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().mouseoutEvent, eventListener);
}

EventListener* DOMWindow::onmouseover() const
{
    return inlineEventListenerForType(eventNames().mouseoverEvent);
}

void DOMWindow::setOnmouseover(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().mouseoverEvent, eventListener);
}

EventListener* DOMWindow::onmouseup() const
{
    return inlineEventListenerForType(eventNames().mouseupEvent);
}

void DOMWindow::setOnmouseup(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().mouseupEvent, eventListener);
}

EventListener* DOMWindow::onmousewheel() const
{
    return inlineEventListenerForType(eventNames().mousewheelEvent);
}

void DOMWindow::setOnmousewheel(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().mousewheelEvent, eventListener);
}

EventListener* DOMWindow::onreset() const
{
    return inlineEventListenerForType(eventNames().resetEvent);
}

void DOMWindow::setOnreset(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().resetEvent, eventListener);
}

EventListener* DOMWindow::onresize() const
{
    return inlineEventListenerForType(eventNames().resizeEvent);
}

void DOMWindow::setOnresize(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().resizeEvent, eventListener);
}

EventListener* DOMWindow::onscroll() const
{
    return inlineEventListenerForType(eventNames().scrollEvent);
}

void DOMWindow::setOnscroll(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().scrollEvent, eventListener);
}

EventListener* DOMWindow::onsearch() const
{
    return inlineEventListenerForType(eventNames().searchEvent);
}

void DOMWindow::setOnsearch(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().searchEvent, eventListener);
}

EventListener* DOMWindow::onselect() const
{
    return inlineEventListenerForType(eventNames().selectEvent);
}

void DOMWindow::setOnselect(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().selectEvent, eventListener);
}

EventListener* DOMWindow::onsubmit() const
{
    return inlineEventListenerForType(eventNames().submitEvent);
}

void DOMWindow::setOnsubmit(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().submitEvent, eventListener);
}

EventListener* DOMWindow::onunload() const
{
    return inlineEventListenerForType(eventNames().unloadEvent);
}

void DOMWindow::setOnunload(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().unloadEvent, eventListener);
}

EventListener* DOMWindow::onbeforeunload() const
{
    return inlineEventListenerForType(eventNames().beforeunloadEvent);
}

void DOMWindow::setOnbeforeunload(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().beforeunloadEvent, eventListener);
}

EventListener* DOMWindow::onwebkitanimationstart() const
{
    return inlineEventListenerForType(eventNames().webkitAnimationStartEvent);
}

void DOMWindow::setOnwebkitanimationstart(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().webkitAnimationStartEvent, eventListener);
}

EventListener* DOMWindow::onwebkitanimationiteration() const
{
    return inlineEventListenerForType(eventNames().webkitAnimationIterationEvent);
}

void DOMWindow::setOnwebkitanimationiteration(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().webkitAnimationIterationEvent, eventListener);
}

EventListener* DOMWindow::onwebkitanimationend() const
{
    return inlineEventListenerForType(eventNames().webkitAnimationEndEvent);
}

void DOMWindow::setOnwebkitanimationend(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().webkitAnimationEndEvent, eventListener);
}

EventListener* DOMWindow::onwebkittransitionend() const
{
    return inlineEventListenerForType(eventNames().webkitTransitionEndEvent);
}

void DOMWindow::setOnwebkittransitionend(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().webkitTransitionEndEvent, eventListener);
}

} // namespace WebCore
