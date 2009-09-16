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
#include "BeforeUnloadEvent.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSRuleList.h"
#include "CSSStyleSelector.h"
#include "CString.h"
#include "Chrome.h"
#include "Console.h"
#include "DOMSelection.h"
#include "DOMTimer.h"
#include "PageTransitionEvent.h"
#include "Document.h"
#include "Element.h"
#include "EventException.h"
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
#include "Media.h"
#include "MessageEvent.h"
#include "Navigator.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformScreen.h"
#include "PlatformString.h"
#include "Screen.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SuddenTermination.h"
#include "WebKitPoint.h"
#include <algorithm>
#include <wtf/MathExtras.h>

#if ENABLE(DATABASE)
#include "Database.h"
#endif

#if ENABLE(DOM_STORAGE)
#include "Storage.h"
#include "StorageArea.h"
#include "StorageNamespace.h"
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
#include "DOMApplicationCache.h"
#endif

#if ENABLE(NOTIFICATIONS)
#include "NotificationCenter.h"
#endif

using std::min;
using std::max;

namespace WebCore {

class PostMessageTimer : public TimerBase {
public:
    PostMessageTimer(DOMWindow* window, const String& message, const String& sourceOrigin, PassRefPtr<DOMWindow> source, PassOwnPtr<MessagePortChannelArray> channels, SecurityOrigin* targetOrigin)
        : m_window(window)
        , m_message(message)
        , m_origin(sourceOrigin)
        , m_source(source)
        , m_channels(channels)
        , m_targetOrigin(targetOrigin)
    {
    }

    PassRefPtr<MessageEvent> event(ScriptExecutionContext* context)
    {
        OwnPtr<MessagePortArray> messagePorts = MessagePort::entanglePorts(*context, m_channels.release());
        return MessageEvent::create(m_message, m_origin, "", m_source, messagePorts.release());
    }
    SecurityOrigin* targetOrigin() const { return m_targetOrigin.get(); }

private:
    virtual void fired()
    {
        m_window->postMessageTimerFired(this);
    }

    RefPtr<DOMWindow> m_window;
    String m_message;
    String m_origin;
    RefPtr<DOMWindow> m_source;
    OwnPtr<MessagePortChannelArray> m_channels;
    RefPtr<SecurityOrigin> m_targetOrigin;
};

typedef HashMap<DOMWindow*, RegisteredEventListenerVector*> DOMWindowRegisteredEventListenerMap;

static DOMWindowRegisteredEventListenerMap& pendingUnloadEventListenerMap()
{
    DEFINE_STATIC_LOCAL(DOMWindowRegisteredEventListenerMap, eventListenerMap, ());
    return eventListenerMap;
}

static DOMWindowRegisteredEventListenerMap& pendingBeforeUnloadEventListenerMap()
{
    DEFINE_STATIC_LOCAL(DOMWindowRegisteredEventListenerMap, eventListenerMap, ());
    return eventListenerMap;
}

static bool allowsPendingBeforeUnloadListeners(DOMWindow* window)
{
    ASSERT_ARG(window, window);
    Frame* frame = window->frame();
    Page* page = frame->page();
    return page && frame == page->mainFrame();
}

static void addPendingEventListener(DOMWindowRegisteredEventListenerMap& map, DOMWindow* window, RegisteredEventListener* listener)
{
    ASSERT_ARG(window, window);
    ASSERT_ARG(listener, listener);

    if (map.isEmpty())
        disableSuddenTermination();

    pair<DOMWindowRegisteredEventListenerMap::iterator, bool> result = map.add(window, 0);
    if (result.second)
        result.first->second = new RegisteredEventListenerVector;
    result.first->second->append(listener);
}

static void removePendingEventListener(DOMWindowRegisteredEventListenerMap& map, DOMWindow* window, RegisteredEventListener* listener)
{
    ASSERT_ARG(window, window);
    ASSERT_ARG(listener, listener);

    DOMWindowRegisteredEventListenerMap::iterator it = map.find(window);
    ASSERT(it != map.end());

    RegisteredEventListenerVector* listeners = it->second;
    size_t index = listeners->find(listener);
    ASSERT(index != WTF::notFound);
    listeners->remove(index);

    if (!listeners->isEmpty())
        return;

    map.remove(it);
    delete listeners;

    if (map.isEmpty())
        enableSuddenTermination();
}

static void removePendingEventListeners(DOMWindowRegisteredEventListenerMap& map, DOMWindow* window)
{
    ASSERT_ARG(window, window);

    RegisteredEventListenerVector* listeners = map.take(window);
    if (!listeners)
        return;

    delete listeners;

    if (map.isEmpty())
        enableSuddenTermination();
}

bool DOMWindow::dispatchAllPendingBeforeUnloadEvents()
{
    DOMWindowRegisteredEventListenerMap& map = pendingBeforeUnloadEventListenerMap();
    if (map.isEmpty())
        return true;

    static bool alreadyDispatched = false;
    ASSERT(!alreadyDispatched);
    if (alreadyDispatched)
        return true;

    Vector<RefPtr<DOMWindow> > windows;
    DOMWindowRegisteredEventListenerMap::iterator mapEnd = map.end();
    for (DOMWindowRegisteredEventListenerMap::iterator it = map.begin(); it != mapEnd; ++it)
        windows.append(it->first);

    size_t size = windows.size();
    for (size_t i = 0; i < size; ++i) {
        DOMWindow* window = windows[i].get();
        RegisteredEventListenerVector* listeners = map.get(window);
        if (!listeners)
            continue;

        RegisteredEventListenerVector listenersCopy = *listeners;
        Frame* frame = window->frame();
        if (!frame->shouldClose(&listenersCopy))
            return false;
    }

    enableSuddenTermination();

    alreadyDispatched = true;

    return true;
}

unsigned DOMWindow::pendingUnloadEventListeners() const
{
    RegisteredEventListenerVector* listeners = pendingUnloadEventListenerMap().get(const_cast<DOMWindow*>(this));
    return listeners ? listeners->size() : 0;
}

void DOMWindow::dispatchAllPendingUnloadEvents()
{
    DOMWindowRegisteredEventListenerMap& map = pendingUnloadEventListenerMap();
    if (map.isEmpty())
        return;

    static bool alreadyDispatched = false;
    ASSERT(!alreadyDispatched);
    if (alreadyDispatched)
        return;

    Vector<RefPtr<DOMWindow> > windows;
    DOMWindowRegisteredEventListenerMap::iterator mapEnd = map.end();
    for (DOMWindowRegisteredEventListenerMap::iterator it = map.begin(); it != mapEnd; ++it)
        windows.append(it->first);

    size_t size = windows.size();
    for (size_t i = 0; i < size; ++i) {
        DOMWindow* window = windows[i].get();
        RegisteredEventListenerVector* listeners = map.get(window);
        if (!listeners)
            continue;
        RegisteredEventListenerVector listenersCopy = *listeners;
        window->dispatchPageTransitionEvent(EventNames().pagehideEvent, false);
        window->dispatchUnloadEvent(&listenersCopy);
    }

    enableSuddenTermination();

    alreadyDispatched = true;
}

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

void DOMWindow::parseModalDialogFeatures(const String& featuresArg, HashMap<String, String>& map)
{
    Vector<String> features;
    featuresArg.split(';', features);
    Vector<String>::const_iterator end = features.end();
    for (Vector<String>::const_iterator it = features.begin(); it != end; ++it) {
        String s = *it;
        int pos = s.find('=');
        int colonPos = s.find(':');
        if (pos >= 0 && colonPos >= 0)
            continue; // ignore any strings that have both = and :
        if (pos < 0)
            pos = colonPos;
        if (pos < 0) {
            // null string for value means key without value
            map.set(s.stripWhiteSpace().lower(), String());
        } else {
            String key = s.left(pos).stripWhiteSpace().lower();
            String val = s.substring(pos + 1).stripWhiteSpace().lower();
            int spacePos = val.find(' ');
            if (spacePos != -1)
                val = val.left(spacePos);
            map.set(key, val);
        }
    }
}

bool DOMWindow::allowPopUp(Frame* activeFrame)
{
    ASSERT(activeFrame);
    if (activeFrame->script()->processingUserGesture())
        return true;
    Settings* settings = activeFrame->settings();
    return settings && settings->javaScriptCanOpenWindowsAutomatically();
}

bool DOMWindow::canShowModalDialog(const Frame* frame)
{
    if (!frame)
        return false;
    Page* page = frame->page();
    if (!page)
        return false;
    return page->chrome()->canRunModal();
}

bool DOMWindow::canShowModalDialogNow(const Frame* frame)
{
    if (!frame)
        return false;
    Page* page = frame->page();
    if (!page)
        return false;
    return page->chrome()->canRunModalNow();
}

DOMWindow::DOMWindow(Frame* frame)
    : m_frame(frame)
{
}

DOMWindow::~DOMWindow()
{
    if (m_frame)
        m_frame->clearFormerDOMWindow(this);

    removePendingEventListeners(pendingUnloadEventListenerMap(), this);
    removePendingEventListeners(pendingBeforeUnloadEventListenerMap(), this);
}

ScriptExecutionContext* DOMWindow::scriptExecutionContext() const
{
    return document();
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

#if ENABLE(NOTIFICATIONS)
    m_notifications = 0;
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

    Document* document = this->document();
    if (!document)
        return 0;

    Page* page = document->page();
    if (!page)
        return 0;

    if (!page->settings()->sessionStorageEnabled())
        return 0;

    RefPtr<StorageArea> storageArea = page->sessionStorage()->storageArea(document->securityOrigin());
#if ENABLE(INSPECTOR)
    page->inspectorController()->didUseDOMStorage(storageArea.get(), false, m_frame);
#endif

    m_sessionStorage = Storage::create(m_frame, storageArea.release());
    return m_sessionStorage.get();
}

Storage* DOMWindow::localStorage() const
{
    if (m_localStorage)
        return m_localStorage.get();
    
    Document* document = this->document();
    if (!document)
        return 0;
        
    Page* page = document->page();
    if (!page)
        return 0;

    if (!page->settings()->localStorageEnabled())
        return 0;

    StorageNamespace* localStorage = page->group().localStorage();
    RefPtr<StorageArea> storageArea = localStorage ? localStorage->storageArea(document->securityOrigin()) : 0; 
    if (storageArea) {
#if ENABLE(INSPECTOR)
        page->inspectorController()->didUseDOMStorage(storageArea.get(), true, m_frame);
#endif
        m_localStorage = Storage::create(m_frame, storageArea.release());
    }

    return m_localStorage.get();
}
#endif

#if ENABLE(NOTIFICATIONS)
NotificationCenter* DOMWindow::webkitNotifications() const
{
    if (m_notifications)
        return m_notifications.get();

    Document* document = this->document();
    if (!document)
        return 0;
    
    Page* page = document->page();
    if (!page)
        return 0;

    NotificationPresenter* provider = page->chrome()->notificationPresenter();
    if (provider) 
        m_notifications = NotificationCenter::create(document, provider);    
      
    return m_notifications.get();
}
#endif

void DOMWindow::postMessage(const String& message, MessagePort* port, const String& targetOrigin, DOMWindow* source, ExceptionCode& ec)
{
    MessagePortArray ports;
    if (port)
        ports.append(port);
    postMessage(message, &ports, targetOrigin, source, ec);
}

void DOMWindow::postMessage(const String& message, const MessagePortArray* ports, const String& targetOrigin, DOMWindow* source, ExceptionCode& ec)
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

    OwnPtr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(ports, ec);
    if (ec)
        return;

    // Capture the source of the message.  We need to do this synchronously
    // in order to capture the source of the message correctly.
    Document* sourceDocument = source->document();
    if (!sourceDocument)
        return;
    String sourceOrigin = sourceDocument->securityOrigin()->toString();

    // Schedule the message.
    PostMessageTimer* timer = new PostMessageTimer(this, message, sourceOrigin, source, channels.release(), target.get());
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
            console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, message, 0, String());
            return;
        }
    }

    ExceptionCode ec = 0;
    dispatchEvent(timer->event(document()), ec);
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

    m_frame->document()->updateStyleIfNeeded();

    Page* page = m_frame->page();
    if (!page)
        return;

    page->chrome()->runJavaScriptAlert(m_frame, message);
}

bool DOMWindow::confirm(const String& message)
{
    if (!m_frame)
        return false;

    m_frame->document()->updateStyleIfNeeded();

    Page* page = m_frame->page();
    if (!page)
        return false;

    return page->chrome()->runJavaScriptConfirm(m_frame, message);
}

String DOMWindow::prompt(const String& message, const String& defaultValue)
{
    if (!m_frame)
        return String();

    m_frame->document()->updateStyleIfNeeded();

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
    // FIXME: This function shouldn't need a frame to work.
    if (!m_frame)
        return 0;

    // The m_frame pointer is not zeroed out when the window is put into b/f cache, so it can hold an unrelated document/window pair.
    // FIXME: We should always zero out the frame pointer on navigation to avoid accidentally accessing the new frame content.
    if (m_frame->domWindow() != this)
        return 0;

    ASSERT(m_frame->document());
    return m_frame->document();
}

PassRefPtr<Media> DOMWindow::media() const
{
    return Media::create(const_cast<DOMWindow*>(this));
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

int DOMWindow::setTimeout(ScheduledAction* action, int timeout)
{
    return DOMTimer::install(scriptExecutionContext(), action, timeout, true);
}

void DOMWindow::clearTimeout(int timeoutId)
{
    DOMTimer::removeById(scriptExecutionContext(), timeoutId);
}

int DOMWindow::setInterval(ScheduledAction* action, int timeout)
{
    return DOMTimer::install(scriptExecutionContext(), action, timeout, false);
}

void DOMWindow::clearInterval(int timeoutId)
{
    DOMTimer::removeById(scriptExecutionContext(), timeoutId);
}

void DOMWindow::handleEvent(Event* event, bool useCapture, RegisteredEventListenerVector* alternateListeners)
{
    RegisteredEventListenerVector& listeners = (alternateListeners ? *alternateListeners : m_eventListeners);
    if (listeners.isEmpty())
        return;

    // If any HTML event listeners are registered on the window, dispatch them here.
    RegisteredEventListenerVector listenersCopy = listeners;
    size_t size = listenersCopy.size();
    for (size_t i = 0; i < size; ++i) {
        RegisteredEventListener& r = *listenersCopy[i];
        if (r.eventType() == event->type() && r.useCapture() == useCapture && !r.removed())
            r.listener()->handleEvent(event, true);
    }
}

void DOMWindow::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    // Remove existing identical listener set with identical arguments.
    // The DOM 2 spec says that "duplicate instances are discarded" in this case.
    removeEventListener(eventType, listener.get(), useCapture);
    if (Document* document = this->document())
        document->addListenerTypeIfNeeded(eventType);

    RefPtr<RegisteredEventListener> registeredListener = RegisteredEventListener::create(eventType, listener, useCapture);
    m_eventListeners.append(registeredListener);

    if (eventType == eventNames().unloadEvent)
        addPendingEventListener(pendingUnloadEventListenerMap(), this, registeredListener.get());
    else if (eventType == eventNames().beforeunloadEvent && allowsPendingBeforeUnloadListeners(this))
        addPendingEventListener(pendingBeforeUnloadEventListenerMap(), this, registeredListener.get());
}

void DOMWindow::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    size_t size = m_eventListeners.size();
    for (size_t i = 0; i < size; ++i) {
        RegisteredEventListener& r = *m_eventListeners[i];
        if (r.eventType() == eventType && r.useCapture() == useCapture && *r.listener() == *listener) {
            if (eventType == eventNames().unloadEvent)
                removePendingEventListener(pendingUnloadEventListenerMap(), this, &r);
            else if (eventType == eventNames().beforeunloadEvent && allowsPendingBeforeUnloadListeners(this))
                removePendingEventListener(pendingBeforeUnloadEventListenerMap(), this, &r);
            r.setRemoved(true);
            m_eventListeners.remove(i);
            return;
        }
    }
}

bool DOMWindow::dispatchEvent(PassRefPtr<Event> e, ExceptionCode& ec)
{
    ASSERT(!eventDispatchForbidden());

    RefPtr<Event> event = e;
    if (!event || event->type().isEmpty()) {
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return true;
    }

    RefPtr<DOMWindow> protect(this);

    event->setTarget(this);
    event->setCurrentTarget(this);

    handleEvent(event.get(), true);
    handleEvent(event.get(), false);

    return !event->defaultPrevented();
}

void DOMWindow::dispatchEvent(const AtomicString& eventType, bool canBubble, bool cancelable)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    dispatchEvent(Event::create(eventType, canBubble, cancelable), ec);
}

// This function accommodates the Firefox quirk of dispatching the load, unload and
// beforeunload events on the window, but setting event.target to be the Document. 
inline void DOMWindow::dispatchEventWithDocumentAsTarget(PassRefPtr<Event> e, RegisteredEventListenerVector* alternateEventListeners)
{
    ASSERT(!eventDispatchForbidden());

    RefPtr<Event> event = e;
    RefPtr<DOMWindow> protect(this);
    RefPtr<Document> document = this->document();

    event->setTarget(document);
    event->setCurrentTarget(this);

    handleEvent(event.get(), true, alternateEventListeners);
    handleEvent(event.get(), false, alternateEventListeners);
}

void DOMWindow::dispatchLoadEvent()
{
    dispatchEventWithDocumentAsTarget(Event::create(eventNames().loadEvent, false, false));

    // For load events, send a separate load event to the enclosing frame only.
    // This is a DOM extension and is independent of bubbling/capturing rules of
    // the DOM.
    Element* ownerElement = document()->ownerElement();
    if (ownerElement) {
        RefPtr<Event> ownerEvent = Event::create(eventNames().loadEvent, false, false);
        ownerEvent->setTarget(ownerElement);
        ownerElement->dispatchGenericEvent(ownerEvent.release());
    }
}

void DOMWindow::dispatchUnloadEvent(RegisteredEventListenerVector* alternateEventListeners)
{
    dispatchEventWithDocumentAsTarget(Event::create(eventNames().unloadEvent, false, false), alternateEventListeners);
}

PassRefPtr<BeforeUnloadEvent> DOMWindow::dispatchBeforeUnloadEvent(RegisteredEventListenerVector* alternateEventListeners)
{
    RefPtr<BeforeUnloadEvent> beforeUnloadEvent = BeforeUnloadEvent::create();
    dispatchEventWithDocumentAsTarget(beforeUnloadEvent.get(), alternateEventListeners);
    return beforeUnloadEvent.release();
}

void DOMWindow::dispatchPageTransitionEvent(const AtomicString& eventType, bool persisted)
{
    dispatchEventWithDocumentAsTarget(PageTransitionEvent::create(eventType, persisted));
}

void DOMWindow::removeAllEventListeners()
{
    size_t size = m_eventListeners.size();
    for (size_t i = 0; i < size; ++i)
        m_eventListeners[i]->setRemoved(true);
    m_eventListeners.clear();

    removePendingEventListeners(pendingUnloadEventListenerMap(), this);
    removePendingEventListeners(pendingBeforeUnloadEventListenerMap(), this);
}

bool DOMWindow::hasEventListener(const AtomicString& eventType)
{
    size_t size = m_eventListeners.size();
    for (size_t i = 0; i < size; ++i) {
        if (m_eventListeners[i]->eventType() == eventType)
            return true;
    }
    return false;
}

void DOMWindow::setAttributeEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener)
{
    clearAttributeEventListener(eventType);
    if (listener)
        addEventListener(eventType, listener, false);
}

void DOMWindow::clearAttributeEventListener(const AtomicString& eventType)
{
    size_t size = m_eventListeners.size();
    for (size_t i = 0; i < size; ++i) {
        RegisteredEventListener& r = *m_eventListeners[i];
        if (r.eventType() == eventType && r.listener()->isAttribute()) {
            if (eventType == eventNames().unloadEvent)
                removePendingEventListener(pendingUnloadEventListenerMap(), this, &r);
            else if (eventType == eventNames().beforeunloadEvent && allowsPendingBeforeUnloadListeners(this))
                removePendingEventListener(pendingBeforeUnloadEventListenerMap(), this, &r);
            r.setRemoved(true);
            m_eventListeners.remove(i);
            return;
        }
    }
}

EventListener* DOMWindow::getAttributeEventListener(const AtomicString& eventType) const
{
    size_t size = m_eventListeners.size();
    for (size_t i = 0; i < size; ++i) {
        RegisteredEventListener& r = *m_eventListeners[i];
        if (r.eventType() == eventType && r.listener()->isAttribute())
            return r.listener();
    }
    return 0;
}

EventListener* DOMWindow::onabort() const
{
    return getAttributeEventListener(eventNames().abortEvent);
}

void DOMWindow::setOnabort(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().abortEvent, eventListener);
}

EventListener* DOMWindow::onblur() const
{
    return getAttributeEventListener(eventNames().blurEvent);
}

void DOMWindow::setOnblur(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().blurEvent, eventListener);
}

EventListener* DOMWindow::onchange() const
{
    return getAttributeEventListener(eventNames().changeEvent);
}

void DOMWindow::setOnchange(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().changeEvent, eventListener);
}

EventListener* DOMWindow::onclick() const
{
    return getAttributeEventListener(eventNames().clickEvent);
}

void DOMWindow::setOnclick(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().clickEvent, eventListener);
}

EventListener* DOMWindow::ondblclick() const
{
    return getAttributeEventListener(eventNames().dblclickEvent);
}

void DOMWindow::setOndblclick(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dblclickEvent, eventListener);
}

EventListener* DOMWindow::ondrag() const
{
    return getAttributeEventListener(eventNames().dragEvent);
}

void DOMWindow::setOndrag(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dragEvent, eventListener);
}

EventListener* DOMWindow::ondragend() const
{
    return getAttributeEventListener(eventNames().dragendEvent);
}

void DOMWindow::setOndragend(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dragendEvent, eventListener);
}

EventListener* DOMWindow::ondragenter() const
{
    return getAttributeEventListener(eventNames().dragenterEvent);
}

void DOMWindow::setOndragenter(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dragenterEvent, eventListener);
}

EventListener* DOMWindow::ondragleave() const
{
    return getAttributeEventListener(eventNames().dragleaveEvent);
}

void DOMWindow::setOndragleave(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dragleaveEvent, eventListener);
}

EventListener* DOMWindow::ondragover() const
{
    return getAttributeEventListener(eventNames().dragoverEvent);
}

void DOMWindow::setOndragover(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dragoverEvent, eventListener);
}

EventListener* DOMWindow::ondragstart() const
{
    return getAttributeEventListener(eventNames().dragstartEvent);
}

void DOMWindow::setOndragstart(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dragstartEvent, eventListener);
}

EventListener* DOMWindow::ondrop() const
{
    return getAttributeEventListener(eventNames().dropEvent);
}

void DOMWindow::setOndrop(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().dropEvent, eventListener);
}

EventListener* DOMWindow::onerror() const
{
    return getAttributeEventListener(eventNames().errorEvent);
}

void DOMWindow::setOnerror(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().errorEvent, eventListener);
}

EventListener* DOMWindow::onfocus() const
{
    return getAttributeEventListener(eventNames().focusEvent);
}

void DOMWindow::setOnfocus(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().focusEvent, eventListener);
}

EventListener* DOMWindow::onhashchange() const
{
    return getAttributeEventListener(eventNames().hashchangeEvent);
}

void DOMWindow::setOnhashchange(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().hashchangeEvent, eventListener);
}

EventListener* DOMWindow::onkeydown() const
{
    return getAttributeEventListener(eventNames().keydownEvent);
}

void DOMWindow::setOnkeydown(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().keydownEvent, eventListener);
}

EventListener* DOMWindow::onkeypress() const
{
    return getAttributeEventListener(eventNames().keypressEvent);
}

void DOMWindow::setOnkeypress(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().keypressEvent, eventListener);
}

EventListener* DOMWindow::onkeyup() const
{
    return getAttributeEventListener(eventNames().keyupEvent);
}

void DOMWindow::setOnkeyup(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().keyupEvent, eventListener);
}

EventListener* DOMWindow::onload() const
{
    return getAttributeEventListener(eventNames().loadEvent);
}

void DOMWindow::setOnload(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().loadEvent, eventListener);
}

EventListener* DOMWindow::onmousedown() const
{
    return getAttributeEventListener(eventNames().mousedownEvent);
}

void DOMWindow::setOnmousedown(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().mousedownEvent, eventListener);
}

EventListener* DOMWindow::onmousemove() const
{
    return getAttributeEventListener(eventNames().mousemoveEvent);
}

void DOMWindow::setOnmousemove(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().mousemoveEvent, eventListener);
}

EventListener* DOMWindow::onmouseout() const
{
    return getAttributeEventListener(eventNames().mouseoutEvent);
}

void DOMWindow::setOnmouseout(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().mouseoutEvent, eventListener);
}

EventListener* DOMWindow::onmouseover() const
{
    return getAttributeEventListener(eventNames().mouseoverEvent);
}

void DOMWindow::setOnmouseover(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().mouseoverEvent, eventListener);
}

EventListener* DOMWindow::onmouseup() const
{
    return getAttributeEventListener(eventNames().mouseupEvent);
}

void DOMWindow::setOnmouseup(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().mouseupEvent, eventListener);
}

EventListener* DOMWindow::onmousewheel() const
{
    return getAttributeEventListener(eventNames().mousewheelEvent);
}

void DOMWindow::setOnmousewheel(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().mousewheelEvent, eventListener);
}

EventListener* DOMWindow::onoffline() const
{
    return getAttributeEventListener(eventNames().offlineEvent);
}

void DOMWindow::setOnoffline(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().offlineEvent, eventListener);
}

EventListener* DOMWindow::ononline() const
{
    return getAttributeEventListener(eventNames().onlineEvent);
}

void DOMWindow::setOnonline(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().onlineEvent, eventListener);
}

EventListener* DOMWindow::onpagehide() const
{
    return getAttributeEventListener(eventNames().pagehideEvent);
}

void DOMWindow::setOnpagehide(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().pagehideEvent, eventListener);
}

EventListener* DOMWindow::onpageshow() const
{
    return getAttributeEventListener(eventNames().pageshowEvent);
}

void DOMWindow::setOnpageshow(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().pageshowEvent, eventListener);
}

EventListener* DOMWindow::onreset() const
{
    return getAttributeEventListener(eventNames().resetEvent);
}

void DOMWindow::setOnreset(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().resetEvent, eventListener);
}

EventListener* DOMWindow::onresize() const
{
    return getAttributeEventListener(eventNames().resizeEvent);
}

void DOMWindow::setOnresize(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().resizeEvent, eventListener);
}

EventListener* DOMWindow::onscroll() const
{
    return getAttributeEventListener(eventNames().scrollEvent);
}

void DOMWindow::setOnscroll(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().scrollEvent, eventListener);
}

EventListener* DOMWindow::onsearch() const
{
    return getAttributeEventListener(eventNames().searchEvent);
}

void DOMWindow::setOnsearch(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().searchEvent, eventListener);
}

EventListener* DOMWindow::onselect() const
{
    return getAttributeEventListener(eventNames().selectEvent);
}

void DOMWindow::setOnselect(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().selectEvent, eventListener);
}

EventListener* DOMWindow::onstorage() const
{
    return getAttributeEventListener(eventNames().storageEvent);
}

void DOMWindow::setOnstorage(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().storageEvent, eventListener);
}

EventListener* DOMWindow::onsubmit() const
{
    return getAttributeEventListener(eventNames().submitEvent);
}

void DOMWindow::setOnsubmit(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().submitEvent, eventListener);
}

EventListener* DOMWindow::onunload() const
{
    return getAttributeEventListener(eventNames().unloadEvent);
}

void DOMWindow::setOnunload(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().unloadEvent, eventListener);
}

EventListener* DOMWindow::onbeforeunload() const
{
    return getAttributeEventListener(eventNames().beforeunloadEvent);
}

void DOMWindow::setOnbeforeunload(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().beforeunloadEvent, eventListener);
}

EventListener* DOMWindow::onwebkitanimationstart() const
{
    return getAttributeEventListener(eventNames().webkitAnimationStartEvent);
}

void DOMWindow::setOnwebkitanimationstart(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().webkitAnimationStartEvent, eventListener);
}

EventListener* DOMWindow::onwebkitanimationiteration() const
{
    return getAttributeEventListener(eventNames().webkitAnimationIterationEvent);
}

void DOMWindow::setOnwebkitanimationiteration(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().webkitAnimationIterationEvent, eventListener);
}

EventListener* DOMWindow::onwebkitanimationend() const
{
    return getAttributeEventListener(eventNames().webkitAnimationEndEvent);
}

void DOMWindow::setOnwebkitanimationend(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().webkitAnimationEndEvent, eventListener);
}

EventListener* DOMWindow::onwebkittransitionend() const
{
    return getAttributeEventListener(eventNames().webkitTransitionEndEvent);
}

void DOMWindow::setOnwebkittransitionend(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().webkitTransitionEndEvent, eventListener);
}

EventListener* DOMWindow::oncanplay() const
{
    return getAttributeEventListener(eventNames().canplayEvent);
}

void DOMWindow::setOncanplay(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().canplayEvent, eventListener);
}

EventListener* DOMWindow::oncanplaythrough() const
{
    return getAttributeEventListener(eventNames().canplaythroughEvent);
}

void DOMWindow::setOncanplaythrough(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().canplaythroughEvent, eventListener);
}

EventListener* DOMWindow::ondurationchange() const
{
    return getAttributeEventListener(eventNames().durationchangeEvent);
}

void DOMWindow::setOndurationchange(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().durationchangeEvent, eventListener);
}

EventListener* DOMWindow::onemptied() const
{
    return getAttributeEventListener(eventNames().emptiedEvent);
}

void DOMWindow::setOnemptied(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().emptiedEvent, eventListener);
}

EventListener* DOMWindow::onended() const
{
    return getAttributeEventListener(eventNames().endedEvent);
}

void DOMWindow::setOnended(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().endedEvent, eventListener);
}

EventListener* DOMWindow::onloadeddata() const
{
    return getAttributeEventListener(eventNames().loadeddataEvent);
}

void DOMWindow::setOnloadeddata(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().loadeddataEvent, eventListener);
}

EventListener* DOMWindow::onloadedmetadata() const
{
    return getAttributeEventListener(eventNames().loadedmetadataEvent);
}

void DOMWindow::setOnloadedmetadata(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().loadedmetadataEvent, eventListener);
}

EventListener* DOMWindow::onpause() const
{
    return getAttributeEventListener(eventNames().pauseEvent);
}

void DOMWindow::setOnpause(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().pauseEvent, eventListener);
}

EventListener* DOMWindow::onplay() const
{
    return getAttributeEventListener(eventNames().playEvent);
}

void DOMWindow::setOnplay(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().playEvent, eventListener);
}

EventListener* DOMWindow::onplaying() const
{
    return getAttributeEventListener(eventNames().playingEvent);
}

void DOMWindow::setOnplaying(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().playingEvent, eventListener);
}

EventListener* DOMWindow::onratechange() const
{
    return getAttributeEventListener(eventNames().ratechangeEvent);
}

void DOMWindow::setOnratechange(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().ratechangeEvent, eventListener);
}

EventListener* DOMWindow::onseeked() const
{
    return getAttributeEventListener(eventNames().seekedEvent);
}

void DOMWindow::setOnseeked(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().seekedEvent, eventListener);
}

EventListener* DOMWindow::onseeking() const
{
    return getAttributeEventListener(eventNames().seekingEvent);
}

void DOMWindow::setOnseeking(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().seekingEvent, eventListener);
}

EventListener* DOMWindow::ontimeupdate() const
{
    return getAttributeEventListener(eventNames().timeupdateEvent);
}

void DOMWindow::setOntimeupdate(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().timeupdateEvent, eventListener);
}

EventListener* DOMWindow::onvolumechange() const
{
    return getAttributeEventListener(eventNames().volumechangeEvent);
}

void DOMWindow::setOnvolumechange(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().volumechangeEvent, eventListener);
}

EventListener* DOMWindow::onwaiting() const
{
    return getAttributeEventListener(eventNames().waitingEvent);
}

void DOMWindow::setOnwaiting(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().waitingEvent, eventListener);
}

EventListener* DOMWindow::onloadstart() const
{
    return getAttributeEventListener(eventNames().loadstartEvent);
}

void DOMWindow::setOnloadstart(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().loadstartEvent, eventListener);
}

EventListener* DOMWindow::onprogress() const
{
    return getAttributeEventListener(eventNames().progressEvent);
}

void DOMWindow::setOnprogress(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().progressEvent, eventListener);
}

EventListener* DOMWindow::onstalled() const
{
    return getAttributeEventListener(eventNames().stalledEvent);
}

void DOMWindow::setOnstalled(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().stalledEvent, eventListener);
}

EventListener* DOMWindow::onsuspend() const
{
    return getAttributeEventListener(eventNames().suspendEvent);
}

void DOMWindow::setOnsuspend(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().suspendEvent, eventListener);
}

EventListener* DOMWindow::oninput() const
{
    return getAttributeEventListener(eventNames().inputEvent);
}

void DOMWindow::setOninput(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().inputEvent, eventListener);
}

EventListener* DOMWindow::onmessage() const
{
    return getAttributeEventListener(eventNames().messageEvent);
}

void DOMWindow::setOnmessage(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().messageEvent, eventListener);
}

EventListener* DOMWindow::oncontextmenu() const
{
    return getAttributeEventListener(eventNames().contextmenuEvent);
}

void DOMWindow::setOncontextmenu(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().contextmenuEvent, eventListener);
}

EventListener* DOMWindow::oninvalid() const
{
    return getAttributeEventListener(eventNames().invalidEvent);
}

void DOMWindow::setOninvalid(PassRefPtr<EventListener> eventListener)
{
    setAttributeEventListener(eventNames().invalidEvent, eventListener);
}

void DOMWindow::captureEvents()
{
    // Not implemented.
}

void DOMWindow::releaseEvents()
{
    // Not implemented.
}

} // namespace WebCore
