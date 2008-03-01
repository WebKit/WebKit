/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reseved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "config.h"
#include "kjs_window.h"

#include "Base64.h"
#include "CString.h"
#include "Chrome.h"
#include "DOMWindow.h"
#include "Element.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "GCController.h"
#include "HTMLDocument.h"
#include "JSDOMWindow.h"
#include "JSEvent.h"
#include "JSAudioConstructor.h"
#include "JSHTMLCollection.h"
#include "JSHTMLOptionElementConstructor.h"
#include "JSXMLHttpRequest.h"
#include "JSLocation.h"
#include "Logging.h"
#include "MediaPlayer.h"
#include "Page.h"
#include "PausedTimeouts.h"
#include "PlatformScreen.h"
#include "PluginInfoStore.h"
#include "RenderView.h"
#include "ScheduledAction.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "WindowFeatures.h"
#include "htmlediting.h"
#include "kjs_css.h"
#include "kjs_events.h"
#include "kjs_navigator.h"
#include "kjs_proxy.h"
#include <wtf/AlwaysInline.h>
#include <wtf/MathExtras.h>

#if ENABLE(XSLT)
#include "JSXSLTProcessor.h"
#endif

using namespace WebCore;
using namespace EventNames;

namespace KJS {

static int lastUsedTimeoutId;

static int timerNestingLevel = 0;
const int cMaxTimerNestingLevel = 5;
const double cMinimumTimerInterval = 0.010;

struct WindowPrivate {
    WindowPrivate()
        : loc(0)
        , m_evt(0)
        , m_returnValueSlot(0)
    {
    }

    Window::ListenersMap jsEventListeners;
    Window::ListenersMap jsHTMLEventListeners;
    Window::UnprotectedListenersMap jsUnprotectedEventListeners;
    Window::UnprotectedListenersMap jsUnprotectedHTMLEventListeners;
    mutable WebCore::JSLocation* loc;
    WebCore::Event* m_evt;
    JSValue** m_returnValueSlot;

    typedef HashMap<int, DOMWindowTimer*> TimeoutsMap;
    TimeoutsMap m_timeouts;
};

class DOMWindowTimer : public TimerBase {
public:
    DOMWindowTimer(int timeoutId, int nestingLevel, Window* object, WebCore::ScheduledAction* action)
        : m_timeoutId(timeoutId)
        , m_nestingLevel(nestingLevel)
        , m_object(object)
        , m_action(action)
    {
    }

    virtual ~DOMWindowTimer()
    {
        JSLock lock;
        delete m_action;
    }

    int timeoutId() const { return m_timeoutId; }

    int nestingLevel() const { return m_nestingLevel; }
    void setNestingLevel(int n) { m_nestingLevel = n; }

    WebCore::ScheduledAction* action() const { return m_action; }
    WebCore::ScheduledAction* takeAction() { WebCore::ScheduledAction* a = m_action; m_action = 0; return a; }

private:
    virtual void fired();

    int m_timeoutId;
    int m_nestingLevel;
    Window* m_object;
    WebCore::ScheduledAction* m_action;
};

} // namespace KJS

#include "kjs_window.lut.h"

namespace KJS {

////////////////////// Window Object ////////////////////////

const ClassInfo Window::info = { "Window", 0, &WindowTable };

/*
@begin WindowTable 118
# Warning, when adding a function to this object you need to add a case in Window::get
# -- Functions --
  atob                  windowProtoFuncAToB                DontDelete|Function 1
  btoa                  windowProtoFuncBToA                DontDelete|Function 1
  open                  windowProtoFuncOpen                DontDelete|Function 3
  setTimeout            windowProtoFuncSetTimeout          DontDelete|Function 2
  clearTimeout          windowProtoFuncClearTimeout        DontDelete|Function 1
  setInterval           windowProtoFuncSetInterval         DontDelete|Function 2
  clearInterval         windowProtoFuncClearTimeout        DontDelete|Function 1
  addEventListener      windowProtoFuncAddEventListener    DontDelete|Function 3
  removeEventListener   windowProtoFuncRemoveEventListener DontDelete|Function 3
  showModalDialog       windowProtoFuncShowModalDialog     DontDelete|Function 1
# Not implemented
  captureEvents         windowProtoFuncNotImplemented      DontDelete|Function 0
  releaseEvents         windowProtoFuncNotImplemented      DontDelete|Function 0

# -- Attributes --
  crypto                Window::Crypto              DontDelete|ReadOnly
  event                 Window::Event_              DontDelete
  location              Window::Location_           DontDelete
  navigator             Window::Navigator_          DontDelete
  clientInformation     Window::ClientInformation   DontDelete
# -- Event Listeners --
  onabort               Window::Onabort             DontDelete
  onblur                Window::Onblur              DontDelete
  onchange              Window::Onchange            DontDelete
  onclick               Window::Onclick             DontDelete
  ondblclick            Window::Ondblclick          DontDelete
  onerror               Window::Onerror             DontDelete
  onfocus               Window::Onfocus             DontDelete
  onkeydown             Window::Onkeydown           DontDelete
  onkeypress            Window::Onkeypress          DontDelete
  onkeyup               Window::Onkeyup             DontDelete
  onload                Window::Onload              DontDelete
  onmousedown           Window::Onmousedown         DontDelete
  onmousemove           Window::Onmousemove         DontDelete
  onmouseout            Window::Onmouseout          DontDelete
  onmouseover           Window::Onmouseover         DontDelete
  onmouseup             Window::Onmouseup           DontDelete
  onmousewheel          Window::OnWindowMouseWheel  DontDelete
  onreset               Window::Onreset             DontDelete
  onresize              Window::Onresize            DontDelete
  onscroll              Window::Onscroll            DontDelete
  onsearch              Window::Onsearch            DontDelete
  onselect              Window::Onselect            DontDelete
  onsubmit              Window::Onsubmit            DontDelete
  onunload              Window::Onunload            DontDelete
  onbeforeunload        Window::Onbeforeunload      DontDelete
# -- Constructors --
  Audio                 Window::Audio               DontDelete
  Image                 Window::Image               DontDelete
  Option                Window::Option              DontDelete
  XMLHttpRequest        Window::XMLHttpRequest      DontDelete
  XSLTProcessor         Window::XSLTProcessor_      DontDelete
@end
*/

Window::Window(JSObject* prototype, DOMWindow* window)
    : JSGlobalObject(prototype)
    , m_impl(window)
    , d(new WindowPrivate)
{
    // Window destruction is not thread-safe because of
    // the non-thread-safe WebCore structures it references.
    Collector::collectOnMainThreadOnly(this);

    // Time in milliseconds before the script timeout handler kicks in.
    setTimeoutTime(10000);
}

Window::~Window()
{
    clearAllTimeouts();

    // Clear any backpointers to the window

    ListenersMap::iterator i2 = d->jsEventListeners.begin();
    ListenersMap::iterator e2 = d->jsEventListeners.end();
    for (; i2 != e2; ++i2)
        i2->second->clearWindowObj();
    i2 = d->jsHTMLEventListeners.begin();
    e2 = d->jsHTMLEventListeners.end();
    for (; i2 != e2; ++i2)
        i2->second->clearWindowObj();

    UnprotectedListenersMap::iterator i1 = d->jsUnprotectedEventListeners.begin();
    UnprotectedListenersMap::iterator e1 = d->jsUnprotectedEventListeners.end();
    for (; i1 != e1; ++i1)
        i1->second->clearWindowObj();
    i1 = d->jsUnprotectedHTMLEventListeners.begin();
    e1 = d->jsUnprotectedHTMLEventListeners.end();
    for (; i1 != e1; ++i1)
        i1->second->clearWindowObj();
}

Window* Window::retrieveWindow(Frame* frame)
{
    JSObject* o = retrieve(frame)->getObject();

    ASSERT(o || !frame->scriptProxy()->isEnabled());
    return static_cast<Window*>(o);
}

Window* Window::retrieveActive(ExecState* exec)
{
    JSGlobalObject* globalObject = exec->dynamicGlobalObject();
    ASSERT(globalObject);
    return static_cast<Window*>(globalObject);
}

JSValue* Window::retrieve(Frame* frame)
{
    ASSERT(frame);
    if (frame->scriptProxy()->isEnabled())
        return frame->scriptProxy()->globalObject(); // the Global object is the "window"

    return jsUndefined(); // This can happen with JS disabled on the domain of that window
}

WebCore::JSLocation* Window::location() const
{
    if (!d->loc)
        d->loc = new JSLocation(0, impl()->frame()); // FIXME: we need to pass a prototype.
    return d->loc;
}

void Window::mark()
{
    Base::mark();
    if (d->loc && !d->loc->marked())
        d->loc->mark();
}

static bool allowPopUp(ExecState* exec)
{
    Frame* frame = Window::retrieveActive(exec)->impl()->frame();

    ASSERT(frame);
    if (frame->scriptProxy()->processingUserGesture())
        return true;
    Settings* settings = frame->settings();
    return settings && settings->JavaScriptCanOpenWindowsAutomatically();
}

static HashMap<String, String> parseModalDialogFeatures(const String& featuresArg)
{
    HashMap<String, String> map;

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

    return map;
}

static Frame* createWindow(ExecState* exec, Frame* openerFrame, const String& url,
    const String& frameName, const WindowFeatures& windowFeatures, JSValue* dialogArgs)
{
    Frame* activeFrame = Window::retrieveActive(exec)->impl()->frame();

    ResourceRequest request;
    if (activeFrame)
        request.setHTTPReferrer(activeFrame->loader()->outgoingReferrer());
    FrameLoadRequest frameRequest(request, frameName);

    FrameLoader* loader;
    if (activeFrame)
        // We need to use the active frame's loader to let FrameLoader know
        // which principal is requesting the navigation.  Unfortunately, there
        // might not be an activeFrame, in which case we resort to using the
        // opener's loader.
        //
        // See http://bugs.webkit.org/show_bug.cgi?id=16522
        loader = activeFrame->loader();
    else
        loader = openerFrame->loader();

    // FIXME: It's much better for client API if a new window starts with a URL, here where we
    // know what URL we are going to open. Unfortunately, this code passes the empty string
    // for the URL, but there's a reason for that. Before loading we have to set up the opener,
    // openedByDOM, and dialogArguments values. Also, to decide whether to use the URL we currently
    // do an allowsAccessFrom call using the window we create, which can't be done before creating it.
    // We'd have to resolve all those issues to pass the URL instead of "".

    bool created;
    Frame* newFrame = loader->createWindow(frameRequest, windowFeatures, created);
    if (!newFrame)
        return 0;

    newFrame->loader()->setOpener(openerFrame);
    newFrame->loader()->setOpenedByDOM();

    Window* newWindow = Window::retrieveWindow(newFrame);

    if (dialogArgs)
        newWindow->putDirect("dialogArguments", dialogArgs);

    if (!protocolIs(url, "javascript") || newWindow->allowsAccessFrom(exec)) {
        KURL completedURL = url.isEmpty() ? KURL("") : activeFrame->document()->completeURL(url);
        bool userGesture = activeFrame->scriptProxy()->processingUserGesture();

        if (created) {
            newFrame->loader()->changeLocation(completedURL, activeFrame->loader()->outgoingReferrer(), false, userGesture);
            if (Document* oldDoc = openerFrame->document())
                newFrame->document()->setBaseURL(oldDoc->baseURL());
        } else if (!url.isEmpty())
            newFrame->loader()->scheduleLocationChange(completedURL.string(), activeFrame->loader()->outgoingReferrer(), false, userGesture);
    }

    return newFrame;
}

static bool canShowModalDialog(const Frame* frame)
{
    if (!frame)
        return false;
    return frame->page()->chrome()->canRunModal();
}

static bool canShowModalDialogNow(const Frame* frame)
{
    if (!frame)
        return false;
    return frame->page()->chrome()->canRunModalNow();
}

static JSValue* showModalDialog(ExecState* exec, Frame* frame, const String& url, JSValue* dialogArgs, const String& featureArgs)
{
    if (!canShowModalDialogNow(frame) || !allowPopUp(exec))
        return jsUndefined();

    const HashMap<String, String> features = parseModalDialogFeatures(featureArgs);

    const bool trusted = false;

    // The following features from Microsoft's documentation are not implemented:
    // - default font settings
    // - width, height, left, and top specified in units other than "px"
    // - edge (sunken or raised, default is raised)
    // - dialogHide: trusted && boolFeature(features, "dialoghide"), makes dialog hide when you print
    // - help: boolFeature(features, "help", true), makes help icon appear in dialog (what does it do on Windows?)
    // - unadorned: trusted && boolFeature(features, "unadorned");

    if (!frame)
        return jsUndefined();

    FloatRect screenRect = screenAvailableRect(frame->view());

    WindowFeatures wargs;
    wargs.width = WindowFeatures::floatFeature(features, "dialogwidth", 100, screenRect.width(), 620); // default here came from frame size of dialog in MacIE
    wargs.widthSet = true;
    wargs.height = WindowFeatures::floatFeature(features, "dialogheight", 100, screenRect.height(), 450); // default here came from frame size of dialog in MacIE
    wargs.heightSet = true;

    wargs.x = WindowFeatures::floatFeature(features, "dialogleft", screenRect.x(), screenRect.right() - wargs.width, -1);
    wargs.xSet = wargs.x > 0;
    wargs.y = WindowFeatures::floatFeature(features, "dialogtop", screenRect.y(), screenRect.bottom() - wargs.height, -1);
    wargs.ySet = wargs.y > 0;

    if (WindowFeatures::boolFeature(features, "center", true)) {
        if (!wargs.xSet) {
            wargs.x = screenRect.x() + (screenRect.width() - wargs.width) / 2;
            wargs.xSet = true;
        }
        if (!wargs.ySet) {
            wargs.y = screenRect.y() + (screenRect.height() - wargs.height) / 2;
            wargs.ySet = true;
        }
    }

    wargs.dialog = true;
    wargs.resizable = WindowFeatures::boolFeature(features, "resizable");
    wargs.scrollbarsVisible = WindowFeatures::boolFeature(features, "scroll", true);
    wargs.statusBarVisible = WindowFeatures::boolFeature(features, "status", !trusted);
    wargs.menuBarVisible = false;
    wargs.toolBarVisible = false;
    wargs.locationBarVisible = false;
    wargs.fullscreen = false;

    Frame* dialogFrame = createWindow(exec, frame, url, "", wargs, dialogArgs);
    if (!dialogFrame)
        return jsUndefined();

    Window* dialogWindow = Window::retrieveWindow(dialogFrame);

    // Get the return value either just before clearing the dialog window's
    // properties (in Window::clear), or when on return from runModal.
    JSValue* returnValue = 0;
    dialogWindow->setReturnValueSlot(&returnValue);
    dialogFrame->page()->chrome()->runModal();
    dialogWindow->setReturnValueSlot(0);

    // If we don't have a return value, get it now.
    // Either Window::clear was not called yet, or there was no return value,
    // and in that case, there's no harm in trying again (no benefit either).
    if (!returnValue)
        returnValue = dialogWindow->getDirect("returnValue");

    return returnValue ? returnValue : jsUndefined();
}

JSValue *Window::getValueProperty(ExecState *exec, int token) const
{
   ASSERT(impl()->frame());

   switch (token) {
   case Crypto:
      return jsUndefined(); // FIXME: implement this
    case Event_:
      if (!allowsAccessFrom(exec))
        return jsUndefined();
      if (!d->m_evt)
        return jsUndefined();
      return toJS(exec, d->m_evt);
    case Location_:
      return location();
    case Navigator_:
    case ClientInformation: {
      if (!allowsAccessFrom(exec))
        return jsUndefined();
      // Store the navigator in the object so we get the same one each time.
      Navigator* n = new Navigator(exec->lexicalGlobalObject()->objectPrototype(), impl()->frame());
      // FIXME: this will make the "navigator" object accessible from windows that fail
      // the security check the first time, but not subsequent times, seems weird.
      const_cast<Window *>(this)->putDirect("navigator", n, DontDelete);
      const_cast<Window *>(this)->putDirect("clientInformation", n, DontDelete);
      return n;
    }
    case Image:
      if (!allowsAccessFrom(exec))
        return jsUndefined();
      // FIXME: this property (and the few below) probably shouldn't create a new object every
      // time
      return new ImageConstructorImp(exec, impl()->frame()->document());
    case Option:
      if (!allowsAccessFrom(exec))
        return jsUndefined();
      return new JSHTMLOptionElementConstructor(exec, impl()->frame()->document());
    case XMLHttpRequest:
      if (!allowsAccessFrom(exec))
        return jsUndefined();
      return new JSXMLHttpRequestConstructorImp(exec, impl()->frame()->document());
    case Audio:
#if ENABLE(VIDEO)
      if (!allowsAccessFrom(exec))
        return jsUndefined();
      if (!MediaPlayer::isAvailable())
        return jsUndefined();
      return new JSAudioConstructor(exec, impl()->frame()->document());
#else
      return jsUndefined();
#endif
#if ENABLE(XSLT)
    case XSLTProcessor_:
      if (!allowsAccessFrom(exec))
        return jsUndefined();
      return new XSLTProcessorConstructorImp(exec);
#else
    case XSLTProcessor_:
      return jsUndefined();
#endif
   }

   if (!allowsAccessFrom(exec))
     return jsUndefined();

   switch (token) {
   case Onabort:
     return getListener(exec, abortEvent);
   case Onblur:
     return getListener(exec, blurEvent);
   case Onchange:
     return getListener(exec, changeEvent);
   case Onclick:
     return getListener(exec, clickEvent);
   case Ondblclick:
     return getListener(exec, dblclickEvent);
   case Onerror:
     return getListener(exec, errorEvent);
   case Onfocus:
     return getListener(exec, focusEvent);
   case Onkeydown:
     return getListener(exec, keydownEvent);
   case Onkeypress:
     return getListener(exec, keypressEvent);
   case Onkeyup:
     return getListener(exec, keyupEvent);
   case Onload:
     return getListener(exec, loadEvent);
   case Onmousedown:
     return getListener(exec, mousedownEvent);
   case Onmousemove:
     return getListener(exec, mousemoveEvent);
   case Onmouseout:
     return getListener(exec, mouseoutEvent);
   case Onmouseover:
     return getListener(exec, mouseoverEvent);
   case Onmouseup:
     return getListener(exec, mouseupEvent);
   case OnWindowMouseWheel:
     return getListener(exec, mousewheelEvent);
   case Onreset:
     return getListener(exec, resetEvent);
   case Onresize:
     return getListener(exec,resizeEvent);
   case Onscroll:
     return getListener(exec,scrollEvent);
   case Onsearch:
     return getListener(exec,searchEvent);
   case Onselect:
     return getListener(exec,selectEvent);
   case Onsubmit:
     return getListener(exec,submitEvent);
   case Onbeforeunload:
     return getListener(exec, beforeunloadEvent);
   case Onunload:
     return getListener(exec, unloadEvent);
   }
   ASSERT_NOT_REACHED();
   return jsUndefined();
}

JSValue* Window::childFrameGetter(ExecState*, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{
    return retrieve(static_cast<Window*>(slot.slotBase())->impl()->frame()->tree()->child(AtomicString(propertyName)));
}

JSValue* Window::indexGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot& slot)
{
    return retrieve(static_cast<Window*>(slot.slotBase())->impl()->frame()->tree()->child(slot.index()));
}

JSValue* Window::namedItemGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    Window* thisObj = static_cast<Window*>(slot.slotBase());
    Document* doc = thisObj->impl()->frame()->document();
    ASSERT(thisObj->allowsAccessFrom(exec));
    ASSERT(doc);
    ASSERT(doc->isHTMLDocument());

    RefPtr<WebCore::HTMLCollection> collection = doc->windowNamedItems(propertyName);
    if (collection->length() == 1)
        return toJS(exec, collection->firstItem());
    return toJS(exec, collection.get());
}

bool Window::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    // Check for child frames by name before built-in properties to
    // match Mozilla. This does not match IE, but some sites end up
    // naming frames things that conflict with window properties that
    // are in Moz but not IE. Since we have some of these, we have to do
    // it the Moz way.
    if (impl()->frame()->tree()->child(propertyName)) {
        slot.setCustom(this, childFrameGetter);
        return true;
    }

    const HashEntry* entry = Lookup::findEntry(&WindowTable, propertyName);
    if (entry) {
        if (entry->attr & Function) {
            if (entry->value.functionValue == windowProtoFuncShowModalDialog) {
                if (!canShowModalDialog(impl()->frame()))
                    return false;
            }
            if (allowsAccessFrom(exec))
                slot.setStaticEntry(this, entry, staticFunctionGetter);
            else
                slot.setUndefined(this);
        } else
            slot.setStaticEntry(this, entry, staticValueGetter<Window>);
        return true;
    }

    // Do prototype lookup early so that functions and attributes in the prototype can have
    // precedence over the index and name getters.  
    JSValue* proto = prototype();
    if (proto->isObject()) {
        if (static_cast<JSObject*>(proto)->getPropertySlot(exec, propertyName, slot)) {
            if (!allowsAccessFrom(exec))
                slot.setUndefined(this);
            return true;
        }
    }

    // FIXME: Search the whole frame hierachy somewhere around here.
    // We need to test the correct priority order.

    // allow window[1] or parent[1] etc. (#56983)
    bool ok;
    unsigned i = propertyName.toArrayIndex(&ok);
    if (ok && i < impl()->frame()->tree()->childCount()) {
        slot.setCustomIndex(this, i, indexGetter);
        return true;
    }

    if (!allowsAccessFrom(exec)) {
        slot.setUndefined(this);
        return true;
    }

    // Allow shortcuts like 'Image1' instead of document.images.Image1
    Document* doc = impl()->frame()->document();
    if (doc && doc->isHTMLDocument()) {
        AtomicString atomicPropertyName = propertyName;
        if (static_cast<HTMLDocument*>(doc)->hasNamedItem(atomicPropertyName) || doc->getElementById(atomicPropertyName)) {
            slot.setCustom(this, namedItemGetter);
            return true;
        }
    }

    return Base::getOwnPropertySlot(exec, propertyName, slot);
}

void Window::put(ExecState* exec, const Identifier& propertyName, JSValue* value)
{
  const HashEntry* entry = Lookup::findEntry(&WindowTable, propertyName);
  if (entry) {
     if (entry->attr & Function) {
       if (allowsAccessFrom(exec))
         Base::put(exec, propertyName, value);
       return;
    }
    if (entry->attr & ReadOnly)
      return;

    switch (entry->value.intValue) {
    case Location_: {
      if (Frame* p = Window::retrieveActive(exec)->impl()->frame()) {
        // To avoid breaking old widgets, make "var location =" in a top-level frame create
        // a property named "location" instead of performing a navigation (<rdar://problem/5688039>).
        if (Settings* settings = p->settings()) {
          if (settings->usesDashboardBackwardCompatibilityMode() && !p->tree()->parent()) {
            if (allowsAccessFrom(exec))
              putDirect(propertyName, value);
            return;
          }
        }

        if (!p->loader()->shouldAllowNavigation(impl()->frame()))
          return;
        String dstUrl = p->loader()->completeURL(value->toString(exec)).string();
        if (!protocolIs(dstUrl, "javascript") || allowsAccessFrom(exec)) {
          bool userGesture = p->scriptProxy()->processingUserGesture();
          // We want a new history item if this JS was called via a user gesture
          impl()->frame()->loader()->scheduleLocationChange(dstUrl, p->loader()->outgoingReferrer(), false, userGesture);
        }
      }
      return;
    }
    case Onabort:
      if (allowsAccessFrom(exec))
        setListener(exec, abortEvent,value);
      return;
    case Onblur:
      if (allowsAccessFrom(exec))
        setListener(exec, blurEvent,value);
      return;
    case Onchange:
      if (allowsAccessFrom(exec))
        setListener(exec, changeEvent,value);
      return;
    case Onclick:
      if (allowsAccessFrom(exec))
        setListener(exec,clickEvent,value);
      return;
    case Ondblclick:
      if (allowsAccessFrom(exec))
        setListener(exec, dblclickEvent,value);
      return;
    case Onerror:
      if (allowsAccessFrom(exec))
        setListener(exec, errorEvent, value);
      return;
    case Onfocus:
      if (allowsAccessFrom(exec))
        setListener(exec,focusEvent,value);
      return;
    case Onkeydown:
      if (allowsAccessFrom(exec))
        setListener(exec,keydownEvent,value);
      return;
    case Onkeypress:
      if (allowsAccessFrom(exec))
        setListener(exec,keypressEvent,value);
      return;
    case Onkeyup:
      if (allowsAccessFrom(exec))
        setListener(exec,keyupEvent,value);
      return;
    case Onload:
      if (allowsAccessFrom(exec))
        setListener(exec,loadEvent,value);
      return;
    case Onmousedown:
      if (allowsAccessFrom(exec))
        setListener(exec,mousedownEvent,value);
      return;
    case Onmousemove:
      if (allowsAccessFrom(exec))
        setListener(exec,mousemoveEvent,value);
      return;
    case Onmouseout:
      if (allowsAccessFrom(exec))
        setListener(exec,mouseoutEvent,value);
      return;
    case Onmouseover:
      if (allowsAccessFrom(exec))
        setListener(exec,mouseoverEvent,value);
      return;
    case Onmouseup:
      if (allowsAccessFrom(exec))
        setListener(exec,mouseupEvent,value);
      return;
    case OnWindowMouseWheel:
      if (allowsAccessFrom(exec))
        setListener(exec, mousewheelEvent,value);
      return;
    case Onreset:
      if (allowsAccessFrom(exec))
        setListener(exec,resetEvent,value);
      return;
    case Onresize:
      if (allowsAccessFrom(exec))
        setListener(exec,resizeEvent,value);
      return;
    case Onscroll:
      if (allowsAccessFrom(exec))
        setListener(exec,scrollEvent,value);
      return;
    case Onsearch:
        if (allowsAccessFrom(exec))
            setListener(exec,searchEvent,value);
        return;
    case Onselect:
      if (allowsAccessFrom(exec))
        setListener(exec,selectEvent,value);
      return;
    case Onsubmit:
      if (allowsAccessFrom(exec))
        setListener(exec,submitEvent,value);
      return;
    case Onbeforeunload:
      if (allowsAccessFrom(exec))
        setListener(exec, beforeunloadEvent, value);
      return;
    case Onunload:
      if (allowsAccessFrom(exec))
        setListener(exec, unloadEvent, value);
      return;
    default:
      break;
    }
  }
  if (allowsAccessFrom(exec))
    Base::put(exec, propertyName, value);
}

bool Window::allowsAccessFrom(const JSGlobalObject* other) const
{
    SecurityOrigin::Reason reason;
    if (allowsAccessFromPrivate(other, reason))
        return true;
    printErrorMessage(crossDomainAccessErrorMessage(other, reason));
    return false;
}

bool Window::allowsAccessFrom(ExecState* exec) const
{
    SecurityOrigin::Reason reason;
    if (allowsAccessFromPrivate(exec, reason))
        return true;
    printErrorMessage(crossDomainAccessErrorMessage(exec->dynamicGlobalObject(), reason));
    return false;
}
    
bool Window::allowsAccessFromNoErrorMessage(ExecState* exec) const
{
    SecurityOrigin::Reason reason;
    return allowsAccessFromPrivate(exec, reason);
}

bool Window::allowsAccessFrom(ExecState* exec, String& message) const
{
    SecurityOrigin::Reason reason;
    if (allowsAccessFromPrivate(exec, reason))
        return true;
    message = crossDomainAccessErrorMessage(exec->dynamicGlobalObject(), reason);
    return false;
}
    
ALWAYS_INLINE bool Window::allowsAccessFromPrivate(const ExecState* exec, SecurityOrigin::Reason& reason) const
{
    if (allowsAccessFromPrivate(exec->dynamicGlobalObject(), reason))
        return true;
    if (reason == SecurityOrigin::DomainSetInDOMMismatch) {
        // If the only reason the access failed was a domainSetInDOM bit mismatch, try again against 
        // lexical global object <rdar://problem/5698200>
        if (allowsAccessFromPrivate(exec->lexicalGlobalObject(), reason))
            return true;
    }
    return false;
}

ALWAYS_INLINE bool Window::allowsAccessFromPrivate(const JSGlobalObject* other, SecurityOrigin::Reason& reason) const
{
    const Frame* originFrame = static_cast<const Window*>(other)->impl()->frame();
    if (!originFrame) {
        reason = SecurityOrigin::GenericMismatch;
        return false;
    }

    const Frame* targetFrame = impl()->frame();

    if (originFrame == targetFrame)
        return true;
    
    if (!targetFrame) {
        reason = SecurityOrigin::GenericMismatch;
        return false;
    }

    WebCore::Document* targetDocument = targetFrame->document();

    // JS may be attempting to access the "window" object, which should be valid,
    // even if the document hasn't been constructed yet.  If the document doesn't
    // exist yet allow JS to access the window object.
    if (!targetDocument)
        return true;

    WebCore::Document* originDocument = originFrame->document();

    const SecurityOrigin* originSecurityOrigin = originDocument->securityOrigin();
    const SecurityOrigin* targetSecurityOrigin = targetDocument->securityOrigin();

    if (originSecurityOrigin->canAccess(targetSecurityOrigin, reason))
        return true;

    return false;
}

String Window::crossDomainAccessErrorMessage(const JSGlobalObject* other, SecurityOrigin::Reason) const
{
    const Frame* originFrame = static_cast<const Window*>(other)->impl()->frame();
    const Frame* targetFrame = impl()->frame();
    if (!originFrame || !targetFrame)
        return String();
    WebCore::Document* targetDocument = targetFrame->document();
    WebCore::Document* originDocument = originFrame->document();
    if (!originDocument || !targetDocument)
        return String();
    // FIXME: this error message should contain more specifics of why the same origin check has failed.
    return String::format("Unsafe JavaScript attempt to access frame with URL %s from frame with URL %s. Domains, protocols and ports must match.\n",
        targetDocument->url().string().utf8().data(), originDocument->url().string().utf8().data());
}

void Window::printErrorMessage(const String& message) const
{
    if (message.isEmpty())
        return;

    Frame* frame = impl()->frame();
    if (!frame)
        return;

    if (frame->settings()->privateBrowsingEnabled())
        return;

    if (Interpreter::shouldPrintExceptions())
        printf("%s", message.utf8().data());

    if (Page* page = frame->page())
        page->chrome()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, message, 1, String()); // FIXME: provide a real line number and source URL.
}

ExecState* Window::globalExec()
{
    // We need to make sure that any script execution happening in this
    // frame does not destroy it
    ASSERT(impl()->frame());
    impl()->frame()->keepAlive();
    return Base::globalExec();
}

bool Window::shouldInterruptScript() const
{
    ASSERT(impl()->frame());
    Page* page = impl()->frame()->page();

    // See <rdar://problem/5479443>. We don't think that page can ever be NULL
    // in this case, but if it is, we've gotten into a state where we may have
    // hung the UI, with no way to ask the client whether to cancel execution.
    // For now, our solution is just to cancel execution no matter what,
    // ensuring that we never hang. We might want to consider other solutions
    // if we discover problems with this one.
    ASSERT(page);
    if (!page)
        return true;

    return page->chrome()->shouldInterruptJavaScript();
}

void Window::setListener(ExecState* exec, const AtomicString& eventType, JSValue* func)
{
    ASSERT(impl()->frame());
    Document* doc = impl()->frame()->document();
    if (!doc)
        return;

    doc->setHTMLWindowEventListener(eventType, findOrCreateJSEventListener(func, true));
}

JSValue* Window::getListener(ExecState* exec, const AtomicString& eventType) const
{
    ASSERT(impl()->frame());
    Document* doc = impl()->frame()->document();
    if (!doc)
        return jsUndefined();

    WebCore::EventListener* listener = doc->getHTMLWindowEventListener(eventType);
    if (listener && static_cast<JSEventListener*>(listener)->listenerObj())
        return static_cast<JSEventListener*>(listener)->listenerObj();
    return jsNull();
}

JSEventListener* Window::findJSEventListener(JSValue* val, bool html)
{
    if (!val->isObject())
        return 0;
    JSObject* object = static_cast<JSObject*>(val);
    ListenersMap& listeners = html ? d->jsHTMLEventListeners : d->jsEventListeners;
    return listeners.get(object);
}

JSEventListener* Window::findOrCreateJSEventListener(JSValue* val, bool html)
{
    JSEventListener* listener = findJSEventListener(val, html);
    if (listener)
        return listener;

    if (!val->isObject())
        return 0;
    JSObject* object = static_cast<JSObject*>(val);

    // Note that the JSEventListener constructor adds it to our jsEventListeners list
    return new JSEventListener(object, this, html);
}

JSUnprotectedEventListener* Window::findJSUnprotectedEventListener(JSValue* val, bool html)
{
    if (!val->isObject())
        return 0;
    JSObject* object = static_cast<JSObject*>(val);
    UnprotectedListenersMap& listeners = html ? d->jsUnprotectedHTMLEventListeners : d->jsUnprotectedEventListeners;
    return listeners.get(object);
}

JSUnprotectedEventListener* Window::findOrCreateJSUnprotectedEventListener(JSValue* val, bool html)
{
    JSUnprotectedEventListener* listener = findJSUnprotectedEventListener(val, html);
    if (listener)
        return listener;
    if (!val->isObject())
        return 0;
    JSObject* object = static_cast<JSObject*>(val);

    // The JSUnprotectedEventListener constructor adds it to our jsUnprotectedEventListeners map.
    return new JSUnprotectedEventListener(object, this, html);
}

void Window::clearHelperObjectProperties()
{
    d->loc = 0;
    d->m_evt = 0;
}

void Window::clear()
{
  JSLock lock;

  if (d->m_returnValueSlot && !*d->m_returnValueSlot)
    *d->m_returnValueSlot = getDirect("returnValue");

  clearAllTimeouts();
  clearHelperObjectProperties();

  // Now recreate a working global object for the next URL that will use us; but only if we haven't been
  // disconnected yet
  if (Frame* frame = impl()->frame())
    frame->scriptProxy()->globalObject()->reset(JSDOMWindowPrototype::self());

  // there's likely to be lots of garbage now
  gcController().garbageCollectSoon();
}

void Window::setCurrentEvent(Event* evt)
{
    d->m_evt = evt;
}

Event* Window::currentEvent()
{
    return d->m_evt;
}

JSValue* windowProtoFuncAToB(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    if (!static_cast<Window*>(thisObj)->allowsAccessFrom(exec)) 
        return jsUndefined();

    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    JSValue* v = args[0];
    if (v->isNull())
        return jsString();

    UString s = v->toString(exec);
    if (!s.is8Bit()) {
        setDOMException(exec, INVALID_CHARACTER_ERR);
        return jsUndefined();
    }

    Vector<char> in(s.size());
    for (int i = 0; i < s.size(); ++i)
        in[i] = static_cast<char>(s.data()[i].unicode());
    Vector<char> out;

    if (!base64Decode(in, out))
        return throwError(exec, GeneralError, "Cannot decode base64");

    return jsString(String(out.data(), out.size()));
}

JSValue* windowProtoFuncBToA(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    if (!static_cast<Window*>(thisObj)->allowsAccessFrom(exec)) 
        return jsUndefined();

    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    JSValue* v = args[0];
    if (v->isNull())
        return jsString();

    UString s = v->toString(exec);
    if (!s.is8Bit()) {
        setDOMException(exec, INVALID_CHARACTER_ERR);
        return jsUndefined();
    }

    Vector<char> in(s.size());
    for (int i = 0; i < s.size(); ++i)
        in[i] = static_cast<char>(s.data()[i].unicode());
    Vector<char> out;

    base64Encode(in, out);

    return jsString(String(out.data(), out.size()));
}

JSValue* windowProtoFuncOpen(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    if (!window->allowsAccessFrom(exec)) 
        return jsUndefined();

    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();
    Frame* activeFrame = Window::retrieveActive(exec)->impl()->frame();
    if (!activeFrame)
        return  jsUndefined();

    Page* page = frame->page();

    String urlString = valueToStringWithUndefinedOrNullCheck(exec, args[0]);
    AtomicString frameName = args[1]->isUndefinedOrNull() ? "_blank" : AtomicString(args[1]->toString(exec));

    // Because FrameTree::find() returns true for empty strings, we must check for empty framenames.
    // Otherwise, illegitimate window.open() calls with no name will pass right through the popup blocker.
    if (!allowPopUp(exec) && (frameName.isEmpty() || !frame->tree()->find(frameName)))
        return jsUndefined();

    // Get the target frame for the special cases of _top and _parent.  In those
    // cases, we can schedule a location change right now and return early.
    bool topOrParent = false;
    if (frameName == "_top") {
        frame = frame->tree()->top();
        topOrParent = true;
    } else if (frameName == "_parent") {
        if (Frame* parent = frame->tree()->parent())
            frame = parent;
        topOrParent = true;
    }
    if (topOrParent) {
        if (!activeFrame->loader()->shouldAllowNavigation(frame))
            return jsUndefined();

        String completedURL;
        if (!urlString.isEmpty())
            completedURL = activeFrame->document()->completeURL(urlString).string();

        const Window* targetedWindow = Window::retrieveWindow(frame);
        if (!completedURL.isEmpty() && (!protocolIs(completedURL, "javascript") || (targetedWindow && targetedWindow->allowsAccessFrom(exec)))) {
            bool userGesture = activeFrame->scriptProxy()->processingUserGesture();
            frame->loader()->scheduleLocationChange(completedURL, activeFrame->loader()->outgoingReferrer(), false, userGesture);
        }
        return Window::retrieve(frame);
    }

    // In the case of a named frame or a new window, we'll use the createWindow() helper
    WindowFeatures windowFeatures(valueToStringWithUndefinedOrNullCheck(exec, args[2]));
    FloatRect windowRect(windowFeatures.x, windowFeatures.y, windowFeatures.width, windowFeatures.height);
    WebCore::DOMWindow::adjustWindowRect(screenAvailableRect(page->mainFrame()->view()), windowRect, windowRect);

    windowFeatures.x = windowRect.x();
    windowFeatures.y = windowRect.y();
    windowFeatures.height = windowRect.height();
    windowFeatures.width = windowRect.width();

    frame = createWindow(exec, frame, urlString, frameName, windowFeatures, 0);

    if (!frame)
        return jsUndefined();

    return Window::retrieve(frame); // global object
}

JSValue* windowProtoFuncSetTimeout(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    if (!window->allowsAccessFrom(exec)) 
        return jsUndefined();

    JSValue* v = args[0];
    if (v->isString())
        return jsNumber(window->installTimeout(v->toString(exec), args[1]->toInt32(exec), true /*single shot*/));
    if (v->isObject() && static_cast<JSObject*>(v)->implementsCall()) {
        List argsTail;
        args.getSlice(2, argsTail);
        return jsNumber(window->installTimeout(v, argsTail, args[1]->toInt32(exec), true /*single shot*/));
    }

    return jsUndefined();
}

JSValue* windowProtoFuncClearTimeout(ExecState* exec, JSObject* thisObj, const List& args)
{
    // Also the implementation for window.clearInterval()

    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    if (!window->allowsAccessFrom(exec)) 
        return jsUndefined();

    window->clearTimeout(args[0]->toInt32(exec));
    return jsUndefined();
}

JSValue* windowProtoFuncSetInterval(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    if (!window->allowsAccessFrom(exec)) 
        return jsUndefined();

    if (args.size() >= 2) {
        JSValue* v = args[0];
        int delay = args[1]->toInt32(exec);
        if (v->isString())
            return jsNumber(window->installTimeout(v->toString(exec), delay, false));
        if (v->isObject() && static_cast<JSObject*>(v)->implementsCall()) {
            List argsTail;
            args.getSlice(2, argsTail);
            return jsNumber(window->installTimeout(v, argsTail, delay, false));
        }
    }

    return jsUndefined();

}

JSValue* windowProtoFuncAddEventListener(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    if (!window->allowsAccessFrom(exec)) 
        return jsUndefined();
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    if (JSEventListener* listener = window->findOrCreateJSEventListener(args[1])) {
        if (Document* doc = frame->document())
            doc->addWindowEventListener(AtomicString(args[0]->toString(exec)), listener, args[2]->toBoolean(exec));
    }

    return jsUndefined();
}

JSValue* windowProtoFuncRemoveEventListener(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    if (!window->allowsAccessFrom(exec)) 
        return jsUndefined();
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    if (JSEventListener* listener = window->findJSEventListener(args[1])) {
        if (Document* doc = frame->document())
            doc->removeWindowEventListener(AtomicString(args[0]->toString(exec)), listener, args[2]->toBoolean(exec));
    }

    return jsUndefined();
}

JSValue* windowProtoFuncShowModalDialog(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    return showModalDialog(exec, frame, valueToStringWithUndefinedOrNullCheck(exec, args[0]), args[1], valueToStringWithUndefinedOrNullCheck(exec, args[2]));
}

JSValue* windowProtoFuncNotImplemented(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);

    return jsUndefined();
}

void Window::setReturnValueSlot(JSValue** slot)
{
    d->m_returnValueSlot = slot;
}

////////////////////// timeouts ////////////////////////

void Window::clearAllTimeouts()
{
    deleteAllValues(d->m_timeouts);
    d->m_timeouts.clear();
}

int Window::installTimeout(WebCore::ScheduledAction* a, int t, bool singleShot)
{
    int timeoutId = ++lastUsedTimeoutId;

    // avoid wraparound going negative on us
    if (timeoutId <= 0)
        timeoutId = 1;

    int nestLevel = timerNestingLevel + 1;
    DOMWindowTimer* timer = new DOMWindowTimer(timeoutId, nestLevel, this, a);
    ASSERT(!d->m_timeouts.get(timeoutId));
    d->m_timeouts.set(timeoutId, timer);
    // Use a minimum interval of 10 ms to match other browsers, but only once we've
    // nested enough to notice that we're repeating.
    // Faster timers might be "better", but they're incompatible.
    double interval = max(0.001, t * 0.001);
    if (interval < cMinimumTimerInterval && nestLevel >= cMaxTimerNestingLevel)
        interval = cMinimumTimerInterval;
    if (singleShot)
        timer->startOneShot(interval);
    else
        timer->startRepeating(interval);
    return timeoutId;
}

int Window::installTimeout(const UString& handler, int t, bool singleShot)
{
    return installTimeout(new WebCore::ScheduledAction(handler), t, singleShot);
}

int Window::installTimeout(JSValue* func, const List& args, int t, bool singleShot)
{
    return installTimeout(new WebCore::ScheduledAction(func, args), t, singleShot);
}

WebCore::PausedTimeouts* Window::pauseTimeouts()
{
    size_t count = d->m_timeouts.size();
    if (count == 0)
        return 0;

    PausedTimeout* t = new PausedTimeout [count];
    PausedTimeouts* result = new PausedTimeouts(t, count);

    WindowPrivate::TimeoutsMap::iterator it = d->m_timeouts.begin();
    for (size_t i = 0; i != count; ++i, ++it) {
        int timeoutId = it->first;
        DOMWindowTimer* timer = it->second;
        t[i].timeoutId = timeoutId;
        t[i].nestingLevel = timer->nestingLevel();
        t[i].nextFireInterval = timer->nextFireInterval();
        t[i].repeatInterval = timer->repeatInterval();
        t[i].action = timer->takeAction();
    }
    ASSERT(it == d->m_timeouts.end());

    deleteAllValues(d->m_timeouts);
    d->m_timeouts.clear();

    return result;
}

void Window::resumeTimeouts(PausedTimeouts* timeouts)
{
    if (!timeouts)
        return;
    size_t count = timeouts->numTimeouts();
    PausedTimeout* array = timeouts->takeTimeouts();
    for (size_t i = 0; i != count; ++i) {
        int timeoutId = array[i].timeoutId;
        DOMWindowTimer* timer = new DOMWindowTimer(timeoutId, array[i].nestingLevel, this, array[i].action);
        d->m_timeouts.set(timeoutId, timer);
        timer->start(array[i].nextFireInterval, array[i].repeatInterval);
    }
    delete [] array;
}

void Window::clearTimeout(int timeoutId, bool delAction)
{
    // timeout IDs have to be positive, and 0 and -1 are unsafe to
    // even look up since they are the empty and deleted value
    // respectively
    if (timeoutId <= 0)
        return;

    delete d->m_timeouts.take(timeoutId);
}

void Window::timerFired(DOMWindowTimer* timer)
{
    // Simple case for non-one-shot timers.
    if (timer->isActive()) {
        int timeoutId = timer->timeoutId();

        timer->action()->execute(this);
        // The DOMWindowTimer object may have been deleted or replaced during execution,
        // so we re-fetch it.
        timer = d->m_timeouts.get(timeoutId);
        if (!timer)
            return;

        if (timer->repeatInterval() && timer->repeatInterval() < cMinimumTimerInterval) {
            timer->setNestingLevel(timer->nestingLevel() + 1);
            if (timer->nestingLevel() >= cMaxTimerNestingLevel)
                timer->augmentRepeatInterval(cMinimumTimerInterval - timer->repeatInterval());
        }
        return;
    }

    // Delete timer before executing the action for one-shot timers.
    WebCore::ScheduledAction* action = timer->takeAction();
    d->m_timeouts.remove(timer->timeoutId());
    delete timer;
    action->execute(this);

    JSLock lock;
    delete action;
}

void Window::disconnectFrame()
{
    clearAllTimeouts();
    if (d->loc)
        d->loc->m_frame = 0;
}

Window::ListenersMap& Window::jsEventListeners()
{
    return d->jsEventListeners;
}

Window::ListenersMap& Window::jsHTMLEventListeners()
{
    return d->jsHTMLEventListeners;
}

Window::UnprotectedListenersMap& Window::jsUnprotectedEventListeners()
{
    return d->jsUnprotectedEventListeners;
}

Window::UnprotectedListenersMap& Window::jsUnprotectedHTMLEventListeners()
{
    return d->jsUnprotectedHTMLEventListeners;
}

/////////////////////////////////////////////////////////////////////////////

void DOMWindowTimer::fired()
{
    timerNestingLevel = m_nestingLevel;
    m_object->timerFired(this);
    timerNestingLevel = 0;
}

} // namespace KJS

using namespace KJS;

namespace WebCore {

JSValue* toJS(ExecState*, DOMWindow* domWindow)
{
    if (!domWindow)
        return jsNull();
    Frame* frame = domWindow->frame();
    if (!frame)
        return jsNull();
    return Window::retrieve(frame);
}

} // namespace WebCore
