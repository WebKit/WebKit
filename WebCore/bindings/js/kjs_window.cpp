// -*- c-basic-offset: 4 -*-
/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reseved.
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
#include "FrameView.h"
#include "GCController.h"
#include "HTMLDocument.h"
#include "JSCSSRule.h"
#include "JSCSSValue.h"
#include "JSDOMExceptionConstructor.h"
#include "JSDOMWindow.h"
#include "JSEvent.h"
#include "JSHTMLAudioElementConstructor.h"
#include "JSHTMLCollection.h"
#include "JSHTMLOptionElementConstructor.h"
#include "JSMutationEvent.h"
#include "JSNode.h"
#include "JSNodeFilter.h"
#include "JSRange.h"
#include "JSXMLHttpRequest.h"
#include "Logging.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "PlugInInfoStore.h"
#include "RenderView.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "WindowFeatures.h"
#include "htmlediting.h"
#include "kjs_css.h"
#include "kjs_events.h"
#include "kjs_navigator.h"
#include "kjs_proxy.h"
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
    mutable Location* loc;
    WebCore::Event *m_evt;
    JSValue** m_returnValueSlot;
    typedef HashMap<int, DOMWindowTimer*> TimeoutsMap;
    TimeoutsMap m_timeouts;
};

class DOMWindowTimer : public TimerBase {
public:
    DOMWindowTimer(int timeoutId, int nestingLevel, Window* o, ScheduledAction* a)
        : m_timeoutId(timeoutId), m_nestingLevel(nestingLevel), m_object(o), m_action(a) { }
    virtual ~DOMWindowTimer() 
    { 
        JSLock lock;
        delete m_action; 
    }

    int timeoutId() const { return m_timeoutId; }
    
    int nestingLevel() const { return m_nestingLevel; }
    void setNestingLevel(int n) { m_nestingLevel = n; }
    
    ScheduledAction* action() const { return m_action; }
    ScheduledAction* takeAction() { ScheduledAction* a = m_action; m_action = 0; return a; }

private:
    virtual void fired();

    int m_timeoutId;
    int m_nestingLevel;
    Window* m_object;
    ScheduledAction* m_action;
};

class PausedTimeout {
public:
    int timeoutId;
    int nestingLevel;
    double nextFireInterval;
    double repeatInterval;
    ScheduledAction *action;
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
  atob                  &WindowProtoFuncAToB::create                DontDelete|Function 1
  btoa                  &WindowProtoFuncBToA::create                DontDelete|Function 1
  scroll                &WindowProtoFuncScrollTo::create            DontDelete|Function 2
  scrollBy              &WindowProtoFuncScrollBy::create            DontDelete|Function 2
  scrollTo              &WindowProtoFuncScrollTo::create            DontDelete|Function 2
  moveBy                &WindowProtoFuncMoveBy::create              DontDelete|Function 2
  moveTo                &WindowProtoFuncMoveTo::create              DontDelete|Function 2
  resizeBy              &WindowProtoFuncResizeBy::create            DontDelete|Function 2
  resizeTo              &WindowProtoFuncResizeTo::create            DontDelete|Function 2
  open                  &WindowProtoFuncOpen::create                DontDelete|Function 3
  setTimeout            &WindowProtoFuncSetTimeout::create          DontDelete|Function 2
  clearTimeout          &WindowProtoFuncClearTimeout::create        DontDelete|Function 1
  setInterval           &WindowProtoFuncSetInterval::create         DontDelete|Function 2
  clearInterval         &WindowProtoFuncClearTimeout::create        DontDelete|Function 1
  addEventListener      &WindowProtoFuncAddEventListener::create    DontDelete|Function 3
  removeEventListener   &WindowProtoFuncRemoveEventListener::create DontDelete|Function 3
  showModalDialog       &WindowProtoFuncShowModalDialog::create     DontDelete|Function 1
# Not implemented
  captureEvents         &WindowProtoFuncNotImplemented::create      DontDelete|Function 0
  releaseEvents         &WindowProtoFuncNotImplemented::create      DontDelete|Function 0

# -- Attributes --
  crypto                Window::Crypto              DontDelete|ReadOnly
  event                 Window::Event_              DontDelete
  location              Window::Location_           DontDelete
  navigator             Window::Navigator_          DontDelete|ReadOnly
  clientInformation     Window::ClientInformation   DontDelete|ReadOnly
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
  DOMException          Window::DOMException        DontDelete
  Image                 Window::Image               DontDelete
  Option                Window::Option              DontDelete
  XMLHttpRequest        Window::XMLHttpRequest      DontDelete
  XSLTProcessor         Window::XSLTProcessor_      DontDelete
@end
*/

Window::Window(DOMWindow* window)
  : m_impl(window)
  , d(new WindowPrivate)
{
    // Window destruction is not thread-safe because of
    // the non-thread-safe WebCore structures it references.
    Collector::collectOnMainThreadOnly(this);
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

ScriptInterpreter* Window::interpreter() const
{
    Frame* frame = impl()->frame();
    if (!frame)
        return 0;

    return frame->scriptProxy()->interpreter();
}

Window *Window::retrieveWindow(Frame *f)
{
    JSObject *o = retrieve(f)->getObject();

    ASSERT(o || !f->settings() || !f->settings()->isJavaScriptEnabled());
    return static_cast<Window *>(o);
}

Window *Window::retrieveActive(ExecState *exec)
{
    JSValue *imp = exec->dynamicInterpreter()->globalObject();
    ASSERT(imp);
    return static_cast<Window*>(imp);
}

JSValue *Window::retrieve(Frame *p)
{
    ASSERT(p);
    if (KJSProxy *proxy = p->scriptProxy())
        return proxy->interpreter()->globalObject(); // the Global object is the "window"
  
    return jsUndefined(); // This can happen with JS disabled on the domain of that window
}

Location *Window::location() const
{
  if (!d->loc)
    d->loc = new Location(impl()->frame());
  return d->loc;
}

// reference our special objects during garbage collection
void Window::mark()
{
  JSObject::mark();
  if (d->loc && !d->loc->marked())
    d->loc->mark();
}

static bool allowPopUp(ExecState *exec, Window *window)
{
    Frame* frame = window->impl()->frame();
    if (!frame)
        return false;
    if (static_cast<ScriptInterpreter*>(exec->dynamicInterpreter())->wasRunByUserGesture())
        return true;
    Settings* settings = frame->settings();
    return settings && settings->JavaScriptCanOpenWindowsAutomatically();
}

static HashMap<String, String> parseModalDialogFeatures(ExecState *exec, JSValue *featuresArg)
{
    HashMap<String, String> map;

    Vector<String> features = valueToStringWithUndefinedOrNullCheck(exec, featuresArg).split(';');
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

static bool boolFeature(const HashMap<String, String>& features, const char* key, bool defaultValue = false)
{
    HashMap<String, String>::const_iterator it = features.find(key);
    if (it == features.end())
        return defaultValue;
    const String& value = it->second;
    return value.isNull() || value == "1" || value == "yes" || value == "on";
}

static float floatFeature(const HashMap<String, String> &features, const char *key, float min, float max, float defaultValue)
{
    HashMap<String, String>::const_iterator it = features.find(key);
    if (it == features.end())
        return defaultValue;
    // FIXME: Can't distinguish "0q" from string with no digits in it -- both return d == 0 and ok == false.
    // Would be good to tell them apart somehow since string with no digits should be default value and
    // "0q" should be minimum value.
    bool ok;
    double d = it->second.toDouble(&ok);
    if ((d == 0 && !ok) || isnan(d))
        return defaultValue;
    if (d < min || max <= min)
        return min;
    if (d > max)
        return max;
    return static_cast<int>(d);
}

static Frame* createWindow(ExecState* exec, Frame* openerFrame, const String& url,
    const String& frameName, const WindowFeatures& windowFeatures, JSValue* dialogArgs)
{
    Frame* activeFrame = Window::retrieveActive(exec)->impl()->frame();
    
    ResourceRequest request;
    if (activeFrame)
        request.setHTTPReferrer(activeFrame->loader()->outgoingReferrer());
    FrameLoadRequest frameRequest(request, frameName);

    // FIXME: It's much better for client API if a new window starts with a URL, here where we
    // know what URL we are going to open. Unfortunately, this code passes the empty string
    // for the URL, but there's a reason for that. Before loading we have to set up the opener,
    // openedByDOM, and dialogArguments values. Also, to decide whether to use the URL we currently
    // do an isSafeScript call using the window we create, which can't be done before creating it.
    // We'd have to resolve all those issues to pass the URL instead of "".

    bool created;
    Frame* newFrame = openerFrame->loader()->createWindow(frameRequest, windowFeatures, created);
    if (!newFrame)
        return 0;

    newFrame->loader()->setOpener(openerFrame);
    newFrame->loader()->setOpenedByDOM();

    Window* newWindow = Window::retrieveWindow(newFrame);    
    
    if (dialogArgs)
        newWindow->putDirect("dialogArguments", dialogArgs);

    if (!url.startsWith("javascript:", false) || newWindow->isSafeScript(exec)) {
        String completedURL = url.isEmpty() ? url : activeFrame->document()->completeURL(url);
        bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
        
        if (created) {
            newFrame->loader()->changeLocation(KURL(completedURL.deprecatedString()), activeFrame->loader()->outgoingReferrer(), false, userGesture);
            if (Document* oldDoc = openerFrame->document()) {
                newFrame->document()->setDomainInternal(oldDoc->domain());
                newFrame->document()->setBaseURL(oldDoc->baseURL());
            }
        } else if (!url.isEmpty())
            newFrame->loader()->scheduleLocationChange(completedURL, activeFrame->loader()->outgoingReferrer(), false, userGesture);
    }
    
    return newFrame;
}

static bool canShowModalDialog(const Window *window)
{
    Frame* frame = window->impl()->frame();
    if (!frame)
        return false;

    return frame->page()->chrome()->canRunModal();
}

static bool canShowModalDialogNow(const Window *window)
{
    Frame* frame = window->impl()->frame();
    if (!frame)
        return false;

    return frame->page()->chrome()->canRunModalNow();
}

static JSValue* showModalDialog(ExecState* exec, Window* openerWindow, const List& args)
{
    if (!canShowModalDialogNow(openerWindow) || !allowPopUp(exec, openerWindow))
        return jsUndefined();

    const HashMap<String, String> features = parseModalDialogFeatures(exec, args[2]);

    bool trusted = false;

    WindowFeatures wargs;

    // The following features from Microsoft's documentation are not implemented:
    // - default font settings
    // - width, height, left, and top specified in units other than "px"
    // - edge (sunken or raised, default is raised)
    // - dialogHide: trusted && boolFeature(features, "dialoghide"), makes dialog hide when you print
    // - help: boolFeature(features, "help", true), makes help icon appear in dialog (what does it do on Windows?)
    // - unadorned: trusted && boolFeature(features, "unadorned");
    Frame* frame = openerWindow->impl()->frame();
    if (!frame)
        return jsUndefined();

    FloatRect screenRect = screenAvailableRect(frame->view());

    wargs.width = floatFeature(features, "dialogwidth", 100, screenRect.width(), 620); // default here came from frame size of dialog in MacIE
    wargs.widthSet = true;
    wargs.height = floatFeature(features, "dialogheight", 100, screenRect.height(), 450); // default here came from frame size of dialog in MacIE
    wargs.heightSet = true;

    wargs.x = floatFeature(features, "dialogleft", screenRect.x(), screenRect.right() - wargs.width, -1);
    wargs.xSet = wargs.x > 0;
    wargs.y = floatFeature(features, "dialogtop", screenRect.y(), screenRect.bottom() - wargs.height, -1);
    wargs.ySet = wargs.y > 0;

    if (boolFeature(features, "center", true)) {
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
    wargs.resizable = boolFeature(features, "resizable");
    wargs.scrollbarsVisible = boolFeature(features, "scroll", true);
    wargs.statusBarVisible = boolFeature(features, "status", !trusted);
    wargs.menuBarVisible = false;
    wargs.toolBarVisible = false;
    wargs.locationBarVisible = false;
    wargs.fullscreen = false;
    
    Frame* dialogFrame = createWindow(exec, frame, valueToStringWithUndefinedOrNullCheck(exec, args[0]), "", wargs, args[1]);
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
      if (!isSafeScript(exec))
        return jsUndefined();
      return jsUndefined(); // FIXME: implement this
   case DOMException:
      if (!isSafeScript(exec))
        return jsUndefined();
      return getDOMExceptionConstructor(exec);
    case Event_:
      if (!isSafeScript(exec))
        return jsUndefined();
      if (!d->m_evt)
        return jsUndefined();
      return toJS(exec, d->m_evt);
    case Location_:
      return location();
    case Navigator_:
    case ClientInformation: {
      if (!isSafeScript(exec))
        return jsUndefined();
      // Store the navigator in the object so we get the same one each time.
      Navigator *n = new Navigator(exec, impl()->frame());
      // FIXME: this will make the "navigator" object accessible from windows that fail
      // the security check the first time, but not subsequent times, seems weird.
      const_cast<Window *>(this)->putDirect("navigator", n, DontDelete|ReadOnly);
      const_cast<Window *>(this)->putDirect("clientInformation", n, DontDelete|ReadOnly);
      return n;
    }
    case Image:
      if (!isSafeScript(exec))
        return jsUndefined();
      // FIXME: this property (and the few below) probably shouldn't create a new object every
      // time
      return new ImageConstructorImp(exec, impl()->frame()->document());
    case Option:
      if (!isSafeScript(exec))
        return jsUndefined();
      return new JSHTMLOptionElementConstructor(exec, impl()->frame()->document());
    case XMLHttpRequest:
      if (!isSafeScript(exec))
        return jsUndefined();
      return new JSXMLHttpRequestConstructorImp(exec, impl()->frame()->document());
    case Audio:
#if ENABLE(VIDEO)
      return new JSHTMLAudioElementConstructor(exec, impl()->frame()->document());
#else
      return jsUndefined();
#endif
#if ENABLE(XSLT)
    case XSLTProcessor_:
      if (!isSafeScript(exec))
        return jsUndefined();
      return new XSLTProcessorConstructorImp(exec);
#else
    case XSLTProcessor_:
      return jsUndefined();
#endif
   }

   if (!isSafeScript(exec))
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
   ASSERT(0);
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

JSValue *Window::namedItemGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  Window *thisObj = static_cast<Window *>(slot.slotBase());
  Document *doc = thisObj->impl()->frame()->document();
  ASSERT(thisObj->isSafeScript(exec) && doc && doc->isHTMLDocument());

  String name = propertyName;
  RefPtr<WebCore::HTMLCollection> collection = doc->windowNamedItems(name);
  if (collection->length() == 1)
    return toJS(exec, collection->firstItem());
  return toJS(exec, collection.get());
}

bool Window::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
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
      if (entry->value.functionValue == &WindowProtoFuncShowModalDialog::create) {
        if (!canShowModalDialog(this))
          return false;
      }
      if (isSafeScript(exec))
        slot.setStaticEntry(this, entry, staticFunctionGetter);
      else
        slot.setUndefined(this);
    } else
      slot.setStaticEntry(this, entry, staticValueGetter<Window>);
    return true;
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

  // allow shortcuts like 'Image1' instead of document.images.Image1
  Document* doc = impl()->frame()->document();
  if (doc && doc->isHTMLDocument()) {
    if (!isSafeScript(exec)) {
      slot.setUndefined(this);
      return true;
    }

    AtomicString atomicPropertyName = propertyName;
    if (static_cast<HTMLDocument*>(doc)->hasNamedItem(atomicPropertyName) || doc->getElementById(atomicPropertyName)) {
      slot.setCustom(this, namedItemGetter);
      return true;
    }
  }

  if (!isSafeScript(exec)) {
    slot.setUndefined(this);
    return true;
  }

  return JSObject::getOwnPropertySlot(exec, propertyName, slot);
}

void Window::put(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
{
  const HashEntry* entry = Lookup::findEntry(&WindowTable, propertyName);
  if (entry) {
     if (entry->attr & Function) {
       if (isSafeScript(exec))
         JSObject::put(exec, propertyName, value, attr);
       return;
    }
    if (entry->attr & ReadOnly)
      return;

    switch (entry->value.intValue) {
    case Location_: {
      Frame* p = Window::retrieveActive(exec)->impl()->frame();
      if (p) {
        DeprecatedString dstUrl = p->loader()->completeURL(DeprecatedString(value->toString(exec))).url();
        if (!dstUrl.startsWith("javascript:", false) || isSafeScript(exec)) {
          bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
          // We want a new history item if this JS was called via a user gesture
          impl()->frame()->loader()->scheduleLocationChange(dstUrl, p->loader()->outgoingReferrer(), false, userGesture);
        }
      }
      return;
    }
    case Onabort:
      if (isSafeScript(exec))
        setListener(exec, abortEvent,value);
      return;
    case Onblur:
      if (isSafeScript(exec))
        setListener(exec, blurEvent,value);
      return;
    case Onchange:
      if (isSafeScript(exec))
        setListener(exec, changeEvent,value);
      return;
    case Onclick:
      if (isSafeScript(exec))
        setListener(exec,clickEvent,value);
      return;
    case Ondblclick:
      if (isSafeScript(exec))
        setListener(exec, dblclickEvent,value);
      return;
    case Onerror:
      if (isSafeScript(exec))
        setListener(exec, errorEvent, value);
      return;
    case Onfocus:
      if (isSafeScript(exec))
        setListener(exec,focusEvent,value);
      return;
    case Onkeydown:
      if (isSafeScript(exec))
        setListener(exec,keydownEvent,value);
      return;
    case Onkeypress:
      if (isSafeScript(exec))
        setListener(exec,keypressEvent,value);
      return;
    case Onkeyup:
      if (isSafeScript(exec))
        setListener(exec,keyupEvent,value);
      return;
    case Onload:
      if (isSafeScript(exec))
        setListener(exec,loadEvent,value);
      return;
    case Onmousedown:
      if (isSafeScript(exec))
        setListener(exec,mousedownEvent,value);
      return;
    case Onmousemove:
      if (isSafeScript(exec))
        setListener(exec,mousemoveEvent,value);
      return;
    case Onmouseout:
      if (isSafeScript(exec))
        setListener(exec,mouseoutEvent,value);
      return;
    case Onmouseover:
      if (isSafeScript(exec))
        setListener(exec,mouseoverEvent,value);
      return;
    case Onmouseup:
      if (isSafeScript(exec))
        setListener(exec,mouseupEvent,value);
      return;
    case OnWindowMouseWheel:
      if (isSafeScript(exec))
        setListener(exec, mousewheelEvent,value);
      return;
    case Onreset:
      if (isSafeScript(exec))
        setListener(exec,resetEvent,value);
      return;
    case Onresize:
      if (isSafeScript(exec))
        setListener(exec,resizeEvent,value);
      return;
    case Onscroll:
      if (isSafeScript(exec))
        setListener(exec,scrollEvent,value);
      return;
    case Onsearch:
        if (isSafeScript(exec))
            setListener(exec,searchEvent,value);
        return;
    case Onselect:
      if (isSafeScript(exec))
        setListener(exec,selectEvent,value);
      return;
    case Onsubmit:
      if (isSafeScript(exec))
        setListener(exec,submitEvent,value);
      return;
    case Onbeforeunload:
      if (isSafeScript(exec))
        setListener(exec, beforeunloadEvent, value);
      return;
    case Onunload:
      if (isSafeScript(exec))
        setListener(exec, unloadEvent, value);
      return;
    default:
      break;
    }
  }
  if (isSafeScript(exec))
    JSObject::put(exec, propertyName, value, attr);
}

static bool shouldLoadAsEmptyDocument(const KURL &url)
{
  return url.protocol().lower() == "about" || url.isEmpty();
}

bool Window::isSafeScript(const ScriptInterpreter *origin, const ScriptInterpreter *target)
{
    if (origin == target)
        return true;
        
    Frame* originFrame = origin->frame();
    Frame* targetFrame = target->frame();

    // JS may be attempting to access the "window" object, which should be valid,
    // even if the document hasn't been constructed yet.  If the document doesn't
    // exist yet allow JS to access the window object.
    if (!targetFrame->document())
        return true;

    WebCore::Document *originDocument = originFrame->document();
    WebCore::Document *targetDocument = targetFrame->document();

    if (!targetDocument) {
        return false;
    }

    WebCore::String targetDomain = targetDocument->domain();

    // Always allow local pages to execute any JS.
    if (targetDomain.isNull())
        return true;

    WebCore::String originDomain = originDocument->domain();

    // if this document is being initially loaded as empty by its parent
    // or opener, allow access from any document in the same domain as
    // the parent or opener.
    if (shouldLoadAsEmptyDocument(targetFrame->loader()->url())) {
        Frame* ancestorFrame = targetFrame->loader()->opener() ? targetFrame->loader()->opener() : targetFrame->tree()->parent();
        while (ancestorFrame && shouldLoadAsEmptyDocument(ancestorFrame->loader()->url()))
            ancestorFrame = ancestorFrame->tree()->parent();
        if (ancestorFrame)
            originDomain = ancestorFrame->document()->domain();
    }

    if ( targetDomain == originDomain )
        return true;

    if (Interpreter::shouldPrintExceptions()) {
        printf("Unsafe JavaScript attempt to access frame with URL %s from frame with URL %s. Domains must match.\n", 
             targetDocument->URL().latin1(), originDocument->URL().latin1());
    }
    String message = String::format("Unsafe JavaScript attempt to access frame with URL %s from frame with URL %s. Domains must match.\n", 
                  targetDocument->URL().latin1(), originDocument->URL().latin1());
    if (Page* page = targetFrame->page())
        page->chrome()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, message, 1, String()); // FIXME: provide a real line number and source URL.

    return false;
}

bool Window::isSafeScript(ExecState *exec) const
{
  Frame* frame = impl()->frame();
  if (!frame)
    return false;
  Frame* activeFrame = static_cast<ScriptInterpreter*>(exec->dynamicInterpreter())->frame();
  if (!activeFrame)
    return false;
  if (activeFrame == frame)
    return true;

  WebCore::Document* thisDocument = frame->document();

  // JS may be attempting to access the "window" object, which should be valid,
  // even if the document hasn't been constructed yet.  If the document doesn't
  // exist yet allow JS to access the window object.
  if (!thisDocument)
      return true;

  WebCore::Document* actDocument = activeFrame->document();

  const SecurityOrigin& actSecurityOrigin = actDocument->securityOrigin();
  const SecurityOrigin& thisSecurityOrigin = thisDocument->securityOrigin();

  if (actSecurityOrigin.canAccess(thisSecurityOrigin))
    return true;

  // FIXME: this error message should contain more specifics of why the same origin check has failed.
  String message = String::format("Unsafe JavaScript attempt to access frame with URL %s from frame with URL %s. Domains, protocols and ports must match.\n",
                                  thisDocument->URL().utf8().data(), actDocument->URL().utf8().data());

  if (Interpreter::shouldPrintExceptions())
    printf("%s", message.utf8().data());

  if (Page* page = frame->page())
    page->chrome()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, message, 1, String());

  return false;
}

void Window::setListener(ExecState *exec, const AtomicString &eventType, JSValue *func)
{
  if (!isSafeScript(exec))
    return;
  Frame* frame = impl()->frame();
  if (!frame)
    return;
  Document* doc = frame->document();
  if (!doc)
    return;

  doc->setHTMLWindowEventListener(eventType, findOrCreateJSEventListener(func,true));
}

JSValue *Window::getListener(ExecState *exec, const AtomicString &eventType) const
{
  if (!isSafeScript(exec))
    return jsUndefined();
  Frame* frame = impl()->frame();
  if (!frame)
    return jsUndefined();
  Document* doc = frame->document();
  if (!doc)
    return jsUndefined();

  WebCore::EventListener *listener = doc->getHTMLWindowEventListener(eventType);
  if (listener && static_cast<JSEventListener*>(listener)->listenerObj())
    return static_cast<JSEventListener*>(listener)->listenerObj();
  else
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

JSEventListener *Window::findOrCreateJSEventListener(JSValue *val, bool html)
{
  JSEventListener *listener = findJSEventListener(val, html);
  if (listener)
    return listener;

  if (!val->isObject())
    return 0;
  JSObject *object = static_cast<JSObject *>(val);

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

JSUnprotectedEventListener *Window::findOrCreateJSUnprotectedEventListener(JSValue *val, bool html)
{
  JSUnprotectedEventListener *listener = findJSUnprotectedEventListener(val, html);
  if (listener)
    return listener;

  if (!val->isObject())
    return 0;
  JSObject *object = static_cast<JSObject *>(val);

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
  clearProperties();
  clearHelperObjectProperties();
  setPrototype(JSDOMWindowPrototype::self()); // clear the prototype

  // Now recreate a working global object for the next URL that will use us; but only if we haven't been
  // disconnected yet
  if (Frame* frame = impl()->frame())
    frame->scriptProxy()->interpreter()->initGlobalObject();

  // there's likely to be lots of garbage now
  gcController().garbageCollectSoon();
}

void Window::setCurrentEvent(Event *evt)
{
  d->m_evt = evt;
}

static void setWindowFeature(const String& keyString, const String& valueString, WindowFeatures& windowFeatures)
{
    int value;
    
    if (valueString.length() == 0 || // listing a key with no value is shorthand for key=yes
        valueString == "yes")
        value = 1;
    else
        value = valueString.toInt();
    
    if (keyString == "left" || keyString == "screenx") {
        windowFeatures.xSet = true;
        windowFeatures.x = value;
    } else if (keyString == "top" || keyString == "screeny") {
        windowFeatures.ySet = true;
        windowFeatures.y = value;
    } else if (keyString == "width" || keyString == "innerwidth") {
        windowFeatures.widthSet = true;
        windowFeatures.width = value;
    } else if (keyString == "height" || keyString == "innerheight") {
        windowFeatures.heightSet = true;
        windowFeatures.height = value;
    } else if (keyString == "menubar")
        windowFeatures.menuBarVisible = value;
    else if (keyString == "toolbar")
        windowFeatures.toolBarVisible = value;
    else if (keyString == "location")
        windowFeatures.locationBarVisible = value;
    else if (keyString == "status")
        windowFeatures.statusBarVisible = value;
    else if (keyString == "resizable")
        windowFeatures.resizable = value;
    else if (keyString == "fullscreen")
        windowFeatures.fullscreen = value;
    else if (keyString == "scrollbars")
        windowFeatures.scrollbarsVisible = value;
}

// Though isspace() considers \t and \v to be whitespace, Win IE doesn't.
static bool isSeparator(::UChar c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '=' || c == ',' || c == '\0';
}

static void parseWindowFeatures(const String& features, WindowFeatures& windowFeatures)
{
    /*
     The IE rule is: all features except for channelmode and fullscreen default to YES, but
     if the user specifies a feature string, all features default to NO. (There is no public
     standard that applies to this method.)
     
     <http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/open_0.asp>
     */
    
    windowFeatures.dialog = false;
    windowFeatures.fullscreen = false;
    
    windowFeatures.xSet = false;
    windowFeatures.ySet = false;
    windowFeatures.widthSet = false;
    windowFeatures.heightSet = false;
    
    if (features.length() == 0) {
        windowFeatures.menuBarVisible = true;
        windowFeatures.statusBarVisible = true;
        windowFeatures.toolBarVisible = true;
        windowFeatures.locationBarVisible = true;
        windowFeatures.scrollbarsVisible = true;
        windowFeatures.resizable = true;
        
        return;
    }
    
    windowFeatures.menuBarVisible = false;
    windowFeatures.statusBarVisible = false;
    windowFeatures.toolBarVisible = false;
    windowFeatures.locationBarVisible = false;
    windowFeatures.scrollbarsVisible = false;
    windowFeatures.resizable = false;
    
    // Tread lightly in this code -- it was specifically designed to mimic Win IE's parsing behavior.
    int keyBegin, keyEnd;
    int valueBegin, valueEnd;
    
    int i = 0;
    int length = features.length();
    String buffer = features.lower();
    while (i < length) {
        // skip to first non-separator, but don't skip past the end of the string
        while (isSeparator(buffer[i])) {
            if (i >= length)
                break;
            i++;
        }
        keyBegin = i;
        
        // skip to first separator
        while (!isSeparator(buffer[i]))
            i++;
        keyEnd = i;
        
        // skip to first '=', but don't skip past a ',' or the end of the string
        while (buffer[i] != '=') {
            if (buffer[i] == ',' || i >= length)
                break;
            i++;
        }
        
        // skip to first non-separator, but don't skip past a ',' or the end of the string
        while (isSeparator(buffer[i])) {
            if (buffer[i] == ',' || i >= length)
                break;
            i++;
        }
        valueBegin = i;
        
        // skip to first separator
        while (!isSeparator(buffer[i]))
            i++;
        valueEnd = i;
        
        ASSERT(i <= length);

        String keyString(buffer.substring(keyBegin, keyEnd - keyBegin));
        String valueString(buffer.substring(valueBegin, valueEnd - valueBegin));
        setWindowFeature(keyString, valueString, windowFeatures);
    }
}

// Explain the 4 things this function does.
// 1) Validates the pending changes are not changing to NaN
// 2) Constrains the window rect to no smaller than 100 in each dimension and no
//    bigger than the the float rect's dimensions.
// 3) Constrain window rect to within the top and left boundaries of the screen rect
// 4) Constraint the window rect to within the bottom and right boundaries of the
//    screen rect.
// 5) Translate the window rect coordinates to be within the coordinate space of
//    the screen rect.
static void adjustWindowRect(const FloatRect& screen, FloatRect& window, const FloatRect& pendingChanges)
{
    // Make sure we're in a valid state before adjusting dimensions
    ASSERT(!isnan(screen.x()) && !isnan(screen.y()) && !isnan(screen.width()) && !isnan(screen.height()) &&
           !isnan(window.x()) && !isnan(window.y()) && !isnan(window.width()) && !isnan(window.height()));
    
    // Update window values if they are not NaN
    if (!isnan(pendingChanges.x()))
        window.setX(pendingChanges.x());
    if (!isnan(pendingChanges.y()))
        window.setY(pendingChanges.y());
    if (!isnan(pendingChanges.width()))
        window.setWidth(pendingChanges.width());
    if (!isnan(pendingChanges.height()))
        window.setHeight(pendingChanges.height());
    
    // Resize the window to between 100 and the screen width and height if it's
    // outside of those ranges.
    window.setWidth(min(max(100.0f, window.width()), screen.width()));
    window.setHeight(min(max(100.0f, window.height()), screen.height()));
    
    // Constrain the window to the top and left of the screen if it's left or
    // above it.
    window.setX(max(window.x(), screen.x()));
    window.setY(max(window.y(), screen.y()));

    // Constrain the window to the bottom and right of the screen if it's past
    // the right or below it.
    window.setX(window.x() - max(0.0f, window.right() - screen.width()));
    window.setY(window.y() - max(0.0f, window.bottom() - screen.height()));
    
    // Adjust the window rect to be in the coordinate space of the screen rect
    window.setX(window.x() + screen.x());
    window.setY(window.y() + screen.y());
}

JSValue* WindowProtoFuncAToB::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    JSValue* v = args[0];
    UString s = v->toString(exec);

    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");
    if (v->isNull())
        return jsString();
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

JSValue* WindowProtoFuncBToA::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    JSValue* v = args[0];
    UString s = v->toString(exec);

    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");
    if (v->isNull())
        return jsString();
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

JSValue* WindowProtoFuncOpen::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    Page* page = frame->page();

    String urlString = valueToStringWithUndefinedOrNullCheck(exec, args[0]);
    AtomicString frameName = args[1]->isUndefinedOrNull() ? "_blank" : AtomicString(args[1]->toString(exec));

    // Because FrameTree::find() returns true for empty strings, we must check for empty framenames.
    // Otherwise, illegitimate window.open() calls with no name will pass right through the popup blocker.
    if (!allowPopUp(exec, window) && (frameName.isEmpty() || !frame->tree()->find(frameName)))
        return jsUndefined();

    // Get the target frame for the special cases of _top and _parent
    if (frameName == "_top")
        while (frame->tree()->parent())
              frame = frame->tree()->parent();
    else if (frameName == "_parent")
        if (frame->tree()->parent())
            frame = frame->tree()->parent();

    // In those cases, we can schedule a location change right now and return early
    if (frameName == "_top" || frameName == "_parent") {
        String completedURL;
        Frame* activeFrame = Window::retrieveActive(exec)->impl()->frame();
        if (!urlString.isEmpty() && activeFrame)
            completedURL = activeFrame->document()->completeURL(urlString);

        const Window* window = Window::retrieveWindow(frame);
        if (!completedURL.isEmpty() && (!completedURL.startsWith("javascript:", false) || (window && window->isSafeScript(exec)))) {
            bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
            frame->loader()->scheduleLocationChange(completedURL, activeFrame->loader()->outgoingReferrer(), false, userGesture);
        }
        return Window::retrieve(frame);
    }
    
    // In the case of a named frame or a new window, we'll use the createWindow() helper
    WindowFeatures windowFeatures;
    String features = valueToStringWithUndefinedOrNullCheck(exec, args[2]);
    parseWindowFeatures(features, windowFeatures);
    FloatRect windowRect(windowFeatures.x, windowFeatures.y, windowFeatures.width, windowFeatures.height);
    adjustWindowRect(screenAvailableRect(page->mainFrame()->view()), windowRect, windowRect);

    windowFeatures.x = windowRect.x();
    windowFeatures.y = windowRect.y();
    windowFeatures.height = windowRect.height();
    windowFeatures.width = windowRect.width();

    frame = createWindow(exec, frame, urlString, frameName, windowFeatures, 0);

    if (!frame)
        return jsUndefined();

    return Window::retrieve(frame); // global object
}

JSValue* WindowProtoFuncScrollBy::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    FrameView *widget = frame->view();

    window->updateLayout();
    if(args.size() >= 2 && widget)
      widget->scrollBy(args[0]->toInt32(exec), args[1]->toInt32(exec));
    return jsUndefined();
}

JSValue* WindowProtoFuncScrollTo::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    // Also the implementation for window.scroll()

    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    FrameView *widget = frame->view();

    window->updateLayout();
    if (args.size() >= 2 && widget)
      widget->setContentsPos(args[0]->toInt32(exec), args[1]->toInt32(exec));
    return jsUndefined();
}

JSValue* WindowProtoFuncMoveBy::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    Page* page = frame->page();

    if (args.size() >= 2 && page) {
      FloatRect fr = page->chrome()->windowRect();
      FloatRect update = fr;
      update.move(args[0]->toFloat(exec), args[1]->toFloat(exec));
      // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
      adjustWindowRect(screenAvailableRect(page->mainFrame()->view()), fr, update);
      page->chrome()->setWindowRect(fr);
    }
    return jsUndefined();
}

JSValue* WindowProtoFuncMoveTo::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    Page* page = frame->page();

    if (args.size() >= 2 && page) {
      FloatRect fr = page->chrome()->windowRect();
      FloatRect sr = screenAvailableRect(page->mainFrame()->view());
      fr.setLocation(sr.location());
      FloatRect update = fr;
      update.move(args[0]->toFloat(exec), args[1]->toFloat(exec));     
      // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
      adjustWindowRect(sr, fr, update);
      page->chrome()->setWindowRect(fr);
    }
    return jsUndefined();
}

JSValue* WindowProtoFuncResizeBy::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    Page* page = frame->page();

    if (args.size() >= 2 && page) {
      FloatRect fr = page->chrome()->windowRect();
      FloatSize dest = fr.size() + FloatSize(args[0]->toFloat(exec), args[1]->toFloat(exec));
      FloatRect update(fr.location(), dest);
      adjustWindowRect(screenAvailableRect(page->mainFrame()->view()), fr, update);
      page->chrome()->setWindowRect(fr);
    }
    return jsUndefined();
}

JSValue* WindowProtoFuncResizeTo::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    Page* page = frame->page();

    if (args.size() >= 2 && page) {
      FloatRect fr = page->chrome()->windowRect();
      FloatSize dest = FloatSize(args[0]->toFloat(exec), args[1]->toFloat(exec));
      FloatRect update(fr.location(), dest);
      adjustWindowRect(screenAvailableRect(page->mainFrame()->view()), fr, update);
      page->chrome()->setWindowRect(fr);
    }
    return jsUndefined();
}

JSValue* WindowProtoFuncSetTimeout::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    JSValue *v = args[0];
    UString s = v->toString(exec);

    if (!window->isSafeScript(exec))
        return jsUndefined();
    if (v->isString()) {
      int i = args[1]->toInt32(exec);
      int r = (const_cast<Window*>(window))->installTimeout(s, i, true /*single shot*/);
      return jsNumber(r);
    }
    else if (v->isObject() && static_cast<JSObject *>(v)->implementsCall()) {
      JSValue *func = args[0];
      int i = args[1]->toInt32(exec);
      
      List argsTail;
      args.getSlice(2, argsTail);

      int r = (const_cast<Window*>(window))->installTimeout(func, argsTail, i, true /*single shot*/);
      return jsNumber(r);
    }
    else
      return jsUndefined();
}

JSValue* WindowProtoFuncClearTimeout::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    // Also the implementation for window.clearInterval()

    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    JSValue *v = args[0];

    if (!window->isSafeScript(exec))
        return jsUndefined();
    (const_cast<Window*>(window))->clearTimeout(v->toInt32(exec));
    return jsUndefined();
}

JSValue* WindowProtoFuncSetInterval::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    JSValue *v = args[0];
    UString s = v->toString(exec);

    if (!window->isSafeScript(exec))
        return jsUndefined();
    if (args.size() >= 2 && v->isString()) {
      int i = args[1]->toInt32(exec);
      int r = (const_cast<Window*>(window))->installTimeout(s, i, false);
      return jsNumber(r);
    }
    else if (args.size() >= 2 && v->isObject() && static_cast<JSObject *>(v)->implementsCall()) {
      JSValue *func = args[0];
      int i = args[1]->toInt32(exec);

      List argsTail;
      args.getSlice(2, argsTail);

      int r = (const_cast<Window*>(window))->installTimeout(func, argsTail, i, false);
      return jsNumber(r);
    }
    else
      return jsUndefined();

}

JSValue* WindowProtoFuncAddEventListener::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    if (!window->isSafeScript(exec))
        return jsUndefined();
    if (JSEventListener* listener = window->findOrCreateJSEventListener(args[1]))
        if (Document *doc = frame->document())
            doc->addWindowEventListener(AtomicString(args[0]->toString(exec)), listener, args[2]->toBoolean(exec));
    return jsUndefined();
}

JSValue* WindowProtoFuncRemoveEventListener::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    if (!window->isSafeScript(exec))
        return jsUndefined();
    if (JSEventListener* listener = window->findJSEventListener(args[1]))
        if (Document *doc = frame->document())
            doc->removeWindowEventListener(AtomicString(args[0]->toString(exec)), listener, args[2]->toBoolean(exec));
    return jsUndefined();
}

JSValue* WindowProtoFuncShowModalDialog::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    return showModalDialog(exec, window, args);
}

JSValue* WindowProtoFuncNotImplemented::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Window::info))
        return throwError(exec, TypeError);
    Window* window = static_cast<Window*>(thisObj);
    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    // If anyone implements these, they need the safe script security check.
    if (!window->isSafeScript(exec))
        return jsUndefined();
    // Not implemented.
    return jsUndefined();
}

void Window::updateLayout() const
{
  Frame* frame = impl()->frame();
  if (!frame)
    return;
  WebCore::Document* docimpl = frame->document();
  if (docimpl)
    docimpl->updateLayoutIgnorePendingStylesheets();
}

void Window::setReturnValueSlot(JSValue** slot)
{ 
    d->m_returnValueSlot = slot; 
}

////////////////////// ScheduledAction ////////////////////////

void ScheduledAction::execute(Window* window)
{
    RefPtr<Frame> frame = window->impl()->frame();
    if (!frame)
        return;

    KJSProxy* scriptProxy = frame->scriptProxy();
    if (!scriptProxy)
        return;

    RefPtr<ScriptInterpreter> interpreter = scriptProxy->interpreter();

    interpreter->setProcessingTimerCallback(true);

    if (JSValue* func = m_func.get()) {
        JSLock lock;
        if (func->isObject() && static_cast<JSObject*>(func)->implementsCall()) {
            ExecState* exec = interpreter->globalExec();
            ASSERT(window == interpreter->globalObject());
            
            List args;
            size_t size = m_args.size();
            for (size_t i = 0; i < size; ++i) {
                args.append(m_args[i]);
            }

            interpreter->startTimeoutCheck();
            static_cast<JSObject*>(func)->call(exec, window, args);
            interpreter->stopTimeoutCheck();
            if (exec->hadException()) {
                JSObject* exception = exec->exception()->toObject(exec);
                exec->clearException();
                String message = exception->get(exec, exec->propertyNames().message)->toString(exec);
                int lineNumber = exception->get(exec, "line")->toInt32(exec);
                if (Interpreter::shouldPrintExceptions())
                    printf("(timer):%s\n", message.utf8().data());
                if (Page* page = frame->page())
                    page->chrome()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, message, lineNumber, String());
            }
        }
    } else
        frame->loader()->executeScript(m_code);

    // Update our document's rendering following the execution of the timeout callback.
    // FIXME: Why not use updateDocumentsRendering to update rendering of all documents?
    // FIXME: Is this really the right point to do the update? We need a place that works
    // for all possible entry points that might possibly execute script, but this seems
    // to be a bit too low-level.
    if (Document* doc = frame->document())
        doc->updateRendering();
  
    interpreter->setProcessingTimerCallback(false);
}

////////////////////// timeouts ////////////////////////

void Window::clearAllTimeouts()
{
    deleteAllValues(d->m_timeouts);
    d->m_timeouts.clear();
}

int Window::installTimeout(ScheduledAction* a, int t, bool singleShot)
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

ScheduledAction::ScheduledAction(JSValue* func, const List& args)
    : m_func(func)
{
    List::const_iterator end = args.end();
    for (List::const_iterator it = args.begin(); it != end; ++it)
        m_args.append(*it);
}

int Window::installTimeout(const UString& handler, int t, bool singleShot)
{
    return installTimeout(new ScheduledAction(handler), t, singleShot);
}

int Window::installTimeout(JSValue* func, const List& args, int t, bool singleShot)
{
    return installTimeout(new ScheduledAction(func, args), t, singleShot);
}

PausedTimeouts* Window::pauseTimeouts()
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
        if (d->m_timeouts.contains(timeoutId) && timer->repeatInterval() && timer->repeatInterval() < cMinimumTimerInterval) {
            timer->setNestingLevel(timer->nestingLevel() + 1);
            if (timer->nestingLevel() >= cMaxTimerNestingLevel)
                timer->augmentRepeatInterval(cMinimumTimerInterval - timer->repeatInterval());
        }
        return;
    }

    // Delete timer before executing the action for one-shot timers.
    ScheduledAction* action = timer->takeAction();
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

////////////////////// Location Object ////////////////////////

const ClassInfo Location::info = { "Location", 0, &LocationTable };
/*
@begin LocationTable 12
  assign        &LocationProtoFuncAssign::create        DontDelete|Function 1
  hash          Location::Hash                          DontDelete
  host          Location::Host                          DontDelete
  hostname      Location::Hostname                      DontDelete
  href          Location::Href                          DontDelete
  pathname      Location::Pathname                      DontDelete
  port          Location::Port                          DontDelete
  protocol      Location::Protocol                      DontDelete
  search        Location::Search                        DontDelete
  toString      &LocationProtoFuncToString::create      DontEnum|DontDelete|Function 0
  replace       &LocationProtoFuncReplace::create       DontDelete|Function 1
  reload        &LocationProtoFuncReload::create        DontDelete|Function 0
@end
*/

Location::Location(Frame *p) : m_frame(p)
{
}

JSValue *Location::getValueProperty(ExecState *exec, int token) const
{
  KURL url = m_frame->loader()->url();
  switch (token) {
  case Hash:
    return jsString(url.ref().isNull() ? "" : "#" + url.ref());
  case Host: {
    // Note: this is the IE spec. The NS spec swaps the two, it says
    // "The hostname property is the concatenation of the host and port properties, separated by a colon."
    // Bleh.
    UString str = url.host();
    if (url.port())
        str += ":" + String::number((int)url.port());
    return jsString(str);
  }
  case Hostname:
    return jsString(url.host());
  case Href:
    if (!url.hasPath())
      return jsString(url.prettyURL() + "/");
    return jsString(url.prettyURL());
  case Pathname:
    return jsString(url.path().isEmpty() ? "/" : url.path());
  case Port:
    return jsString(url.port() ? String::number((int)url.port()) : "");
  case Protocol:
    return jsString(url.protocol() + ":");
  case Search:
    return jsString(url.query());
  default:
    ASSERT(0);
    return jsUndefined();
  }
}

bool Location::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot) 
{
  if (!m_frame)
    return false;
  
  const Window* window = Window::retrieveWindow(m_frame);
  
  const HashEntry *entry = Lookup::findEntry(&LocationTable, propertyName);
  if (!entry || !(entry->attr & KJS::Function) || (entry->value.functionValue != &LocationProtoFuncReplace::create
                                                   && entry->value.functionValue != &LocationProtoFuncReload::create
                                                   && entry->value.functionValue != &LocationProtoFuncAssign::create))  {
    if (!window || !window->isSafeScript(exec)) {
      slot.setUndefined(this);
      return true;
    }
  }

  return getStaticPropertySlot<Location, JSObject>(exec, &LocationTable, this, propertyName, slot);
}

void Location::put(ExecState *exec, const Identifier &p, JSValue *v, int attr)
{
  if (!m_frame)
    return;

  DeprecatedString str = v->toString(exec);
  KURL url = m_frame->loader()->url();
  const Window* window = Window::retrieveWindow(m_frame);
  bool sameDomainAccess = window && window->isSafeScript(exec);

  const HashEntry *entry = Lookup::findEntry(&LocationTable, p);

  if (entry) {
      // cross-domain access to the location is allowed when assigning the whole location,
      // but not when assigning the individual pieces, since that might inadvertently
      // disclose other parts of the original location.
      if (entry->value.intValue != Href && !sameDomainAccess)
          return;

      switch (entry->value.intValue) {
      case Href: {
          Frame* frame = Window::retrieveActive(exec)->impl()->frame();
          if (frame)
              url = frame->loader()->completeURL(str).url();
          else
              url = str;
          break;
      } 
      case Hash: {
          if (str.startsWith("#"))
              str = str.mid(1);
          
          if (url.ref() == str)
              return;

          url.setRef(str);
          break;
      }
      case Host: {
          url.setHostAndPort(str);
          break;
      }
      case Hostname:
          url.setHost(str);
          break;
      case Pathname:
          url.setPath(str);
          break;
      case Port:
          url.setPort(str.toUInt());
          break;
      case Protocol:
          url.setProtocol(str);
          break;
      case Search:
          url.setQuery(str);
          break;
      default:
          // Disallow changing other properties in LocationTable. e.g., "window.location.toString = ...".
          // <http://bugs.webkit.org/show_bug.cgi?id=12720>
          return;
      }
  } else {
      if (sameDomainAccess)
          JSObject::put(exec, p, v, attr);
      return;
  }

  Frame* activeFrame = Window::retrieveActive(exec)->impl()->frame();
  if (!url.url().startsWith("javascript:", false) || sameDomainAccess) {
    bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
    m_frame->loader()->scheduleLocationChange(url.url(), activeFrame->loader()->outgoingReferrer(), false, userGesture);
  }
}

JSValue* LocationProtoFuncReplace::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Location::info))
        return throwError(exec, TypeError);
    Location* location = static_cast<Location*>(thisObj);
    Frame* frame = location->frame();
    if (!frame)
        return jsUndefined();

    DeprecatedString str = args[0]->toString(exec);
    Frame* p = Window::retrieveActive(exec)->impl()->frame();
    if ( p ) {
      const Window* window = Window::retrieveWindow(frame);
      if (!str.startsWith("javascript:", false) || (window && window->isSafeScript(exec))) {
        bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
        frame->loader()->scheduleLocationChange(p->loader()->completeURL(str).url(), p->loader()->outgoingReferrer(), true, userGesture);
      }
    }

    return jsUndefined();
}

JSValue* LocationProtoFuncReload::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Location::info))
        return throwError(exec, TypeError);
    Location* location = static_cast<Location*>(thisObj);
    Frame* frame = location->frame();
    if (!frame)
        return jsUndefined();

    Window* window = Window::retrieveWindow(frame);
    if (!window->isSafeScript(exec))
        return jsUndefined();

    if (!frame->loader()->url().url().startsWith("javascript:", false) || (window && window->isSafeScript(exec))) {
      bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
      frame->loader()->scheduleRefresh(userGesture);
    }
    return jsUndefined();
}

JSValue* LocationProtoFuncAssign::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Location::info))
        return throwError(exec, TypeError);
    Location* location = static_cast<Location*>(thisObj);
    Frame* frame = location->frame();
    if (!frame)
        return jsUndefined();

    Window* window = Window::retrieveWindow(frame);
    if (!window->isSafeScript(exec))
        return jsUndefined();

    Frame *p = Window::retrieveActive(exec)->impl()->frame();
    if (p) {
        const Window *window = Window::retrieveWindow(frame);
        DeprecatedString dstUrl = p->loader()->completeURL(DeprecatedString(args[0]->toString(exec))).url();
        if (!dstUrl.startsWith("javascript:", false) || (window && window->isSafeScript(exec))) {
            bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
            // We want a new history item if this JS was called via a user gesture
            frame->loader()->scheduleLocationChange(dstUrl, p->loader()->outgoingReferrer(), false, userGesture);
        }
    }

    return jsUndefined();
}

JSValue* LocationProtoFuncToString::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&Location::info))
        return throwError(exec, TypeError);
    Location* location = static_cast<Location*>(thisObj);
    Frame* frame = location->frame();
    if (!frame)
        return jsUndefined();

    Window* window = Window::retrieveWindow(frame);
    if (!window->isSafeScript(exec))
        return jsUndefined();

    if (!frame || !Window::retrieveWindow(frame)->isSafeScript(exec))
        return jsString();

    if (!frame->loader()->url().hasPath())
        return jsString(frame->loader()->url().prettyURL() + "/");
    return jsString(frame->loader()->url().prettyURL());
}

/////////////////////////////////////////////////////////////////////////////

PausedTimeouts::~PausedTimeouts()
{
    PausedTimeout *array = m_array;
    if (!array)
        return;
    size_t count = m_length;
    for (size_t i = 0; i != count; ++i)
        delete array[i].action;
    delete [] array;
}

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
