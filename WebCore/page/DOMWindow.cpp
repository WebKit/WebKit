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

#include "Base64.h"
#include "BarInfo.h"
#include "BeforeUnloadEvent.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSRuleList.h"
#include "CSSStyleSelector.h"
#include "Chrome.h"
#include "Console.h"
#include "Database.h"
#include "DatabaseCallback.h"
#include "DOMApplicationCache.h"
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
#include "IndexedDatabase.h"
#include "IndexedDatabaseRequest.h"
#include "InspectorController.h"
#include "InspectorTimelineAgent.h"
#include "Location.h"
#include "StyleMedia.h"
#include "MessageEvent.h"
#include "Navigator.h"
#include "NotificationCenter.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformScreen.h"
#include "PlatformString.h"
#include "Screen.h"
#include "SecurityOrigin.h"
#include "SerializedScriptValue.h"
#include "Settings.h"
#include "Storage.h"
#include "StorageArea.h"
#include "StorageNamespace.h"
#include "SuddenTermination.h"
#include "WebKitPoint.h"
#include <algorithm>
#include <wtf/text/CString.h>
#include <wtf/MathExtras.h>

using std::min;
using std::max;

namespace WebCore {

class PostMessageTimer : public TimerBase {
public:
    PostMessageTimer(DOMWindow* window, PassRefPtr<SerializedScriptValue> message, const String& sourceOrigin, PassRefPtr<DOMWindow> source, PassOwnPtr<MessagePortChannelArray> channels, SecurityOrigin* targetOrigin)
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
        return MessageEvent::create(messagePorts.release(), m_message, m_origin, "", m_source);
    }
    SecurityOrigin* targetOrigin() const { return m_targetOrigin.get(); }

private:
    virtual void fired()
    {
        m_window->postMessageTimerFired(this);
    }

    RefPtr<DOMWindow> m_window;
    RefPtr<SerializedScriptValue> m_message;
    String m_origin;
    RefPtr<DOMWindow> m_source;
    OwnPtr<MessagePortChannelArray> m_channels;
    RefPtr<SecurityOrigin> m_targetOrigin;
};

typedef HashCountedSet<DOMWindow*> DOMWindowSet;

static DOMWindowSet& windowsWithUnloadEventListeners()
{
    DEFINE_STATIC_LOCAL(DOMWindowSet, windowsWithUnloadEventListeners, ());
    return windowsWithUnloadEventListeners;
}

static DOMWindowSet& windowsWithBeforeUnloadEventListeners()
{
    DEFINE_STATIC_LOCAL(DOMWindowSet, windowsWithBeforeUnloadEventListeners, ());
    return windowsWithBeforeUnloadEventListeners;
}

static void addUnloadEventListener(DOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    if (set.isEmpty())
        disableSuddenTermination();
    set.add(domWindow);
}

static void removeUnloadEventListener(DOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.remove(it);
    if (set.isEmpty())
        enableSuddenTermination();
}

static void removeAllUnloadEventListeners(DOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.removeAll(it);
    if (set.isEmpty())
        enableSuddenTermination();
}

static void addBeforeUnloadEventListener(DOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    if (set.isEmpty())
        disableSuddenTermination();
    set.add(domWindow);
}

static void removeBeforeUnloadEventListener(DOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.remove(it);
    if (set.isEmpty())
        enableSuddenTermination();
}

static void removeAllBeforeUnloadEventListeners(DOMWindow* domWindow)
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    DOMWindowSet::iterator it = set.find(domWindow);
    if (it == set.end())
        return;
    set.removeAll(it);
    if (set.isEmpty())
        enableSuddenTermination();
}

static bool allowsBeforeUnloadListeners(DOMWindow* window)
{
    ASSERT_ARG(window, window);
    Frame* frame = window->frame();
    if (!frame)
        return false;
    Page* page = frame->page();
    if (!page)
        return false;
    return frame == page->mainFrame();
}

bool DOMWindow::dispatchAllPendingBeforeUnloadEvents()
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    if (set.isEmpty())
        return true;

    static bool alreadyDispatched = false;
    ASSERT(!alreadyDispatched);
    if (alreadyDispatched)
        return true;

    Vector<RefPtr<DOMWindow> > windows;
    DOMWindowSet::iterator end = set.end();
    for (DOMWindowSet::iterator it = set.begin(); it != end; ++it)
        windows.append(it->first);

    size_t size = windows.size();
    for (size_t i = 0; i < size; ++i) {
        DOMWindow* window = windows[i].get();
        if (!set.contains(window))
            continue;

        Frame* frame = window->frame();
        if (!frame)
            continue;

        if (!frame->shouldClose())
            return false;
    }

    enableSuddenTermination();

    alreadyDispatched = true;

    return true;
}

unsigned DOMWindow::pendingUnloadEventListeners() const
{
    return windowsWithUnloadEventListeners().count(const_cast<DOMWindow*>(this));
}

void DOMWindow::dispatchAllPendingUnloadEvents()
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    if (set.isEmpty())
        return;

    static bool alreadyDispatched = false;
    ASSERT(!alreadyDispatched);
    if (alreadyDispatched)
        return;

    Vector<RefPtr<DOMWindow> > windows;
    DOMWindowSet::iterator end = set.end();
    for (DOMWindowSet::iterator it = set.begin(); it != end; ++it)
        windows.append(it->first);

    size_t size = windows.size();
    for (size_t i = 0; i < size; ++i) {
        DOMWindow* window = windows[i].get();
        if (!set.contains(window))
            continue;

        window->dispatchEvent(PageTransitionEvent::create(eventNames().pagehideEvent, false), window->document());
        window->dispatchEvent(Event::create(eventNames().unloadEvent, false, false), window->document());
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

    removeAllUnloadEventListeners(this);
    removeAllBeforeUnloadEventListeners(this);
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

    if (m_media)
        m_media->disconnectFrame();
    m_media = 0;
    
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
    if (m_notifications)
        m_notifications->disconnectFrame();
    m_notifications = 0;
#endif

#if ENABLE(INDEXED_DATABASE)
    if (m_indexedDatabaseRequest)
        m_indexedDatabaseRequest->disconnectFrame();
    m_indexedDatabaseRequest = 0;
#endif
}

#if ENABLE(ORIENTATION_EVENTS)
int DOMWindow::orientation() const
{
    if (!m_frame)
        return 0;
    
    return m_frame->orientation();
}
#endif

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
Storage* DOMWindow::sessionStorage(ExceptionCode& ec) const
{
    if (m_sessionStorage)
        return m_sessionStorage.get();

    Document* document = this->document();
    if (!document)
        return 0;

    if (!document->securityOrigin()->canAccessLocalStorage()) {
        ec = SECURITY_ERR;
        return 0;
    }

    Page* page = document->page();
    if (!page)
        return 0;

    RefPtr<StorageArea> storageArea = page->sessionStorage()->storageArea(document->securityOrigin());
#if ENABLE(INSPECTOR)
    page->inspectorController()->didUseDOMStorage(storageArea.get(), false, m_frame);
#endif

    m_sessionStorage = Storage::create(m_frame, storageArea.release());
    return m_sessionStorage.get();
}

Storage* DOMWindow::localStorage(ExceptionCode& ec) const
{
    if (m_localStorage)
        return m_localStorage.get();

    Document* document = this->document();
    if (!document)
        return 0;

    if (!document->securityOrigin()->canAccessLocalStorage()) {
        ec = SECURITY_ERR;
        return 0;
    }

    Page* page = document->page();
    if (!page)
        return 0;

    if (!page->settings()->localStorageEnabled())
        return 0;

    RefPtr<StorageArea> storageArea = page->group().localStorage()->storageArea(document->securityOrigin());
#if ENABLE(INSPECTOR)
    page->inspectorController()->didUseDOMStorage(storageArea.get(), true, m_frame);
#endif

    m_localStorage = Storage::create(m_frame, storageArea.release());
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

#if ENABLE(INDEXED_DATABASE)
IndexedDatabaseRequest* DOMWindow::indexedDB() const
{
    if (m_indexedDatabaseRequest)
        return m_indexedDatabaseRequest.get();

    Document* document = this->document();
    if (!document)
        return 0;

    // FIXME: See if access is allowed.

    Page* page = document->page();
    if (!page)
        return 0;

    // FIXME: See if indexedDatabase access is allowed.

    m_indexedDatabaseRequest = IndexedDatabaseRequest::create(page->group().indexedDatabase(), m_frame);
    return m_indexedDatabaseRequest.get();
}
#endif

void DOMWindow::postMessage(PassRefPtr<SerializedScriptValue> message, MessagePort* port, const String& targetOrigin, DOMWindow* source, ExceptionCode& ec)
{
    MessagePortArray ports;
    if (port)
        ports.append(port);
    postMessage(message, &ports, targetOrigin, source, ec);
}

void DOMWindow::postMessage(PassRefPtr<SerializedScriptValue> message, const MessagePortArray* ports, const String& targetOrigin, DOMWindow* source, ExceptionCode& ec)
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

    dispatchEvent(timer->event(document()));
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

void DOMWindow::close(ScriptExecutionContext* context)
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (m_frame != page->mainFrame())
        return;

    if (context) {
        ASSERT(WTF::isMainThread());
        Frame* activeFrame = static_cast<Document*>(context)->frame();
        if (!activeFrame)
            return;

        if (!activeFrame->loader()->shouldAllowNavigation(m_frame))
            return;
    }

    Settings* settings = m_frame->settings();
    bool allowScriptsToCloseWindows = settings && settings->allowScriptsToCloseWindows();

    if (page->openedByDOM() || page->getHistoryLength() <= 1 || allowScriptsToCloseWindows)
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

static bool isSafeToConvertCharList(const String& string)
{
    for (unsigned i = 0; i < string.length(); i++) {
        if (string[i] > 0xFF)
            return false;
    }

    return true;
}

String DOMWindow::btoa(const String& stringToEncode, ExceptionCode& ec)
{
    if (stringToEncode.isNull())
        return String();

    if (!isSafeToConvertCharList(stringToEncode)) {
        ec = INVALID_CHARACTER_ERR;
        return String();
    }

    Vector<char> in;
    in.append(stringToEncode.characters(), stringToEncode.length());
    Vector<char> out;

    base64Encode(in, out);

    return String(out.data(), out.size());
}

String DOMWindow::atob(const String& encodedString, ExceptionCode& ec)
{
    if (encodedString.isNull())
        return String();

    if (!isSafeToConvertCharList(encodedString)) {
        ec = INVALID_CHARACTER_ERR;
        return String();
    }

    Vector<char> in;
    in.append(encodedString.characters(), encodedString.length());
    Vector<char> out;

    if (!base64Decode(in, out)) {
        ec = INVALID_CHARACTER_ERR;
        return String();
    }

    return String(out.data(), out.size());
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

PassRefPtr<StyleMedia> DOMWindow::styleMedia() const
{
    if (!m_media)
        m_media = StyleMedia::create(m_frame);
    return m_media.get();
}

PassRefPtr<CSSStyleDeclaration> DOMWindow::getComputedStyle(Element* elt, const String& pseudoElt) const
{
    if (!elt)
        return 0;

    return computedStyle(elt, false, pseudoElt);
}

PassRefPtr<CSSRuleList> DOMWindow::getMatchedCSSRules(Element* elt, const String&, bool authorOnly) const
{
    if (!m_frame)
        return 0;

    Document* doc = m_frame->document();
    return doc->styleSelector()->styleRulesForElement(elt, authorOnly, SameOriginCSSRulesOnly);
}

PassRefPtr<WebKitPoint> DOMWindow::webkitConvertPointFromNodeToPage(Node* node, const WebKitPoint* p) const
{
    if (!node || !p)
        return 0;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    FloatPoint pagePoint(p->x(), p->y());
    pagePoint = node->convertToPage(pagePoint);
    return WebKitPoint::create(pagePoint.x(), pagePoint.y());
}

PassRefPtr<WebKitPoint> DOMWindow::webkitConvertPointFromPageToNode(Node* node, const WebKitPoint* p) const
{
    if (!node || !p)
        return 0;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

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
PassRefPtr<Database> DOMWindow::openDatabase(const String& name, const String& version, const String& displayName, unsigned long estimatedSize, PassRefPtr<DatabaseCallback> creationCallback, ExceptionCode& ec)
{
    RefPtr<Database> database = 0;
    if (m_frame && Database::isAvailable() && m_frame->document()->securityOrigin()->canAccessDatabase())
        database = Database::openDatabase(m_frame->document(), name, version, displayName, estimatedSize, creationCallback, ec);

    if (!database && !ec)
        ec = SECURITY_ERR;

    return database;
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

    RefPtr<FrameView> view = m_frame->view();
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

int DOMWindow::setTimeout(PassOwnPtr<ScheduledAction> action, int timeout, ExceptionCode& ec)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context) {
        ec = INVALID_ACCESS_ERR;
        return -1;
    }
    return DOMTimer::install(context, action, timeout, true);
}

void DOMWindow::clearTimeout(int timeoutId)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;
    DOMTimer::removeById(context, timeoutId);
}

int DOMWindow::setInterval(PassOwnPtr<ScheduledAction> action, int timeout, ExceptionCode& ec)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context) {
        ec = INVALID_ACCESS_ERR;
        return -1;
    }
    return DOMTimer::install(context, action, timeout, false);
}

void DOMWindow::clearInterval(int timeoutId)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;
    DOMTimer::removeById(context, timeoutId);
}

bool DOMWindow::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    if (!EventTarget::addEventListener(eventType, listener, useCapture))
        return false;

    if (Document* document = this->document())
        document->addListenerTypeIfNeeded(eventType);

    if (eventType == eventNames().unloadEvent)
        addUnloadEventListener(this);
    else if (eventType == eventNames().beforeunloadEvent && allowsBeforeUnloadListeners(this))
        addBeforeUnloadEventListener(this);

    return true;
}

bool DOMWindow::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    if (!EventTarget::removeEventListener(eventType, listener, useCapture))
        return false;

    if (eventType == eventNames().unloadEvent)
        removeUnloadEventListener(this);
    else if (eventType == eventNames().beforeunloadEvent && allowsBeforeUnloadListeners(this))
        removeBeforeUnloadEventListener(this);

    return true;
}

void DOMWindow::dispatchLoadEvent()
{
    dispatchEvent(Event::create(eventNames().loadEvent, false, false), document());

    // For load events, send a separate load event to the enclosing frame only.
    // This is a DOM extension and is independent of bubbling/capturing rules of
    // the DOM.
    Element* ownerElement = document()->ownerElement();
    if (ownerElement) {
        RefPtr<Event> ownerEvent = Event::create(eventNames().loadEvent, false, false);
        ownerEvent->setTarget(ownerElement);
        ownerElement->dispatchGenericEvent(ownerEvent.release());
    }

#if ENABLE(INSPECTOR)
    if (!frame() || !frame()->page())
        return;

    if (InspectorController* controller = frame()->page()->inspectorController())
        controller->mainResourceFiredLoadEvent(frame()->loader()->documentLoader(), url());
#endif
}

#if ENABLE(INSPECTOR)
InspectorTimelineAgent* DOMWindow::inspectorTimelineAgent() 
{
    if (frame() && frame()->page())
        return frame()->page()->inspectorTimelineAgent();
    return 0;
}
#endif

bool DOMWindow::dispatchEvent(PassRefPtr<Event> prpEvent, PassRefPtr<EventTarget> prpTarget)
{
    RefPtr<EventTarget> protect = this;
    RefPtr<Event> event = prpEvent;

    event->setTarget(prpTarget ? prpTarget : this);
    event->setCurrentTarget(this);
    event->setEventPhase(Event::AT_TARGET);

#if ENABLE(INSPECTOR)
    Page* inspectedPage = InspectorTimelineAgent::instanceCount() && frame() ? frame()->page() : 0;
    if (inspectedPage) {
        if (InspectorTimelineAgent* timelineAgent = hasEventListeners(event->type()) ? inspectedPage->inspectorTimelineAgent() : 0)
            timelineAgent->willDispatchEvent(*event);
        else
            inspectedPage = 0;
    }
#endif

    bool result = fireEventListeners(event.get());

#if ENABLE(INSPECTOR)
    if (inspectedPage)
        if (InspectorTimelineAgent* timelineAgent = inspectedPage->inspectorTimelineAgent())
            timelineAgent->didDispatchEvent();
#endif

    return result;
}

void DOMWindow::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();

    removeAllUnloadEventListeners(this);
    removeAllBeforeUnloadEventListeners(this);
}

void DOMWindow::captureEvents()
{
    // Not implemented.
}

void DOMWindow::releaseEvents()
{
    // Not implemented.
}

EventTargetData* DOMWindow::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* DOMWindow::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore
