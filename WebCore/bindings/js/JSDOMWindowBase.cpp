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
#include "JSDOMWindowBase.h"

#include "Base64.h"
#include "CString.h"
#include "Console.h"
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
#include "JSAudioConstructor.h"
#include "JSDOMWindow.h"
#include "JSDOMWindow.h"
#include "JSEvent.h"
#include "JSHTMLCollection.h"
#include "JSHTMLOptionElementConstructor.h"
#include "JSImageConstructor.h"
#include "JSNode.h"
#include "JSXMLHttpRequestConstructor.h"
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
#include "kjs_events.h"
#include "kjs_proxy.h"
#include <wtf/AlwaysInline.h>
#include <wtf/MathExtras.h>

#if ENABLE(XSLT)
#include "JSXSLTProcessorConstructor.h"
#endif

#include "JSDOMWindowBase.lut.h"

using namespace KJS;

namespace WebCore {

using namespace EventNames;

static int lastUsedTimeoutId;

static int timerNestingLevel = 0;
const int cMaxTimerNestingLevel = 5;
const double cMinimumTimerInterval = 0.010;

struct JSDOMWindowBasePrivate {
    JSDOMWindowBasePrivate(JSDOMWindowShell* shell)
        : m_evt(0)
        , m_returnValueSlot(0)
        , m_shell(shell)
    {
    }

    JSDOMWindowBase::ListenersMap jsEventListeners;
    JSDOMWindowBase::ListenersMap jsHTMLEventListeners;
    JSDOMWindowBase::UnprotectedListenersMap jsUnprotectedEventListeners;
    JSDOMWindowBase::UnprotectedListenersMap jsUnprotectedHTMLEventListeners;
    Event* m_evt;
    JSValue** m_returnValueSlot;
    JSDOMWindowShell* m_shell;

    typedef HashMap<int, DOMWindowTimer*> TimeoutsMap;
    TimeoutsMap m_timeouts;
};

class DOMWindowTimer : public TimerBase {
public:
    DOMWindowTimer(int timeoutId, int nestingLevel, JSDOMWindowBase* object, ScheduledAction* action)
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

    ScheduledAction* action() const { return m_action; }
    ScheduledAction* takeAction() { ScheduledAction* a = m_action; m_action = 0; return a; }

private:
    virtual void fired();

    int m_timeoutId;
    int m_nestingLevel;
    JSDOMWindowBase* m_object;
    ScheduledAction* m_action;
};

////////////////////// JSDOMWindowBase Object ////////////////////////

const ClassInfo JSDOMWindowBase::s_info = { "Window", 0, &JSDOMWindowBaseTable, 0 };

/*
@begin JSDOMWindowBaseTable 118
# -- Functions --
  atob                  WebCore::windowProtoFuncAToB                 DontDelete|Function 1
  btoa                  WebCore::windowProtoFuncBToA                 DontDelete|Function 1
  open                  WebCore::windowProtoFuncOpen                 DontDelete|Function 3
  setTimeout            WebCore::windowProtoFuncSetTimeout           DontDelete|Function 2
  clearTimeout          WebCore::windowProtoFuncClearTimeout         DontDelete|Function 1
  setInterval           WebCore::windowProtoFuncSetInterval          DontDelete|Function 2
  clearInterval         WebCore::windowProtoFuncClearTimeout         DontDelete|Function 1
  addEventListener      WebCore::windowProtoFuncAddEventListener     DontDelete|Function 3
  removeEventListener   WebCore::windowProtoFuncRemoveEventListener  DontDelete|Function 3
  showModalDialog       WebCore::windowProtoFuncShowModalDialog      DontDelete|Function 1
# Not implemented
  captureEvents         WebCore::windowProtoFuncNotImplemented       DontDelete|Function 0
  releaseEvents         WebCore::windowProtoFuncNotImplemented       DontDelete|Function 0

# -- Attributes --
  crypto                WebCore::JSDOMWindowBase::Crypto             DontDelete|ReadOnly
  event                 WebCore::JSDOMWindowBase::Event_             DontDelete
# -- Event Listeners --
  onabort               WebCore::JSDOMWindowBase::Onabort            DontDelete
  onblur                WebCore::JSDOMWindowBase::Onblur             DontDelete
  onchange              WebCore::JSDOMWindowBase::Onchange           DontDelete
  onclick               WebCore::JSDOMWindowBase::Onclick            DontDelete
  ondblclick            WebCore::JSDOMWindowBase::Ondblclick         DontDelete
  onerror               WebCore::JSDOMWindowBase::Onerror            DontDelete
  onfocus               WebCore::JSDOMWindowBase::Onfocus            DontDelete
  onkeydown             WebCore::JSDOMWindowBase::Onkeydown          DontDelete
  onkeypress            WebCore::JSDOMWindowBase::Onkeypress         DontDelete
  onkeyup               WebCore::JSDOMWindowBase::Onkeyup            DontDelete
  onload                WebCore::JSDOMWindowBase::Onload             DontDelete
  onmousedown           WebCore::JSDOMWindowBase::Onmousedown        DontDelete
  onmousemove           WebCore::JSDOMWindowBase::Onmousemove        DontDelete
  onmouseout            WebCore::JSDOMWindowBase::Onmouseout         DontDelete
  onmouseover           WebCore::JSDOMWindowBase::Onmouseover        DontDelete
  onmouseup             WebCore::JSDOMWindowBase::Onmouseup          DontDelete
  onmousewheel          WebCore::JSDOMWindowBase::OnWindowMouseWheel DontDelete
  onreset               WebCore::JSDOMWindowBase::Onreset            DontDelete
  onresize              WebCore::JSDOMWindowBase::Onresize           DontDelete
  onscroll              WebCore::JSDOMWindowBase::Onscroll           DontDelete
  onsearch              WebCore::JSDOMWindowBase::Onsearch           DontDelete
  onselect              WebCore::JSDOMWindowBase::Onselect           DontDelete
  onsubmit              WebCore::JSDOMWindowBase::Onsubmit           DontDelete
  onunload              WebCore::JSDOMWindowBase::Onunload           DontDelete
  onbeforeunload        WebCore::JSDOMWindowBase::Onbeforeunload     DontDelete
# -- Constructors --
  Audio                 WebCore::JSDOMWindowBase::Audio              DontDelete
  Image                 WebCore::JSDOMWindowBase::Image              DontDelete
  Option                WebCore::JSDOMWindowBase::Option             DontDelete
  XMLHttpRequest        WebCore::JSDOMWindowBase::XMLHttpRequest     DontDelete
  XSLTProcessor         WebCore::JSDOMWindowBase::XSLTProcessor      DontDelete
@end
*/

JSDOMWindowBase::JSDOMWindowBase(JSObject* prototype, DOMWindow* window, JSDOMWindowShell* shell)
    : JSGlobalObject(prototype, shell)
    , m_impl(window)
    , d(new JSDOMWindowBasePrivate(shell))
{
    // Time in milliseconds before the script timeout handler kicks in.
    setTimeoutTime(10000);
}

JSDOMWindowBase::~JSDOMWindowBase()
{
    clearAllTimeouts();

    // Clear any backpointers to the window

    ListenersMap::iterator i2 = d->jsEventListeners.begin();
    ListenersMap::iterator e2 = d->jsEventListeners.end();
    for (; i2 != e2; ++i2)
        i2->second->clearWindow();
    i2 = d->jsHTMLEventListeners.begin();
    e2 = d->jsHTMLEventListeners.end();
    for (; i2 != e2; ++i2)
        i2->second->clearWindow();

    UnprotectedListenersMap::iterator i1 = d->jsUnprotectedEventListeners.begin();
    UnprotectedListenersMap::iterator e1 = d->jsUnprotectedEventListeners.end();
    for (; i1 != e1; ++i1)
        i1->second->clearWindow();
    i1 = d->jsUnprotectedHTMLEventListeners.begin();
    e1 = d->jsUnprotectedHTMLEventListeners.end();
    for (; i1 != e1; ++i1)
        i1->second->clearWindow();
}

static bool allowPopUp(ExecState* exec)
{
    Frame* frame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();

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
    Frame* activeFrame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    ASSERT(activeFrame);

    ResourceRequest request;

    request.setHTTPReferrer(activeFrame->loader()->outgoingReferrer());
    FrameLoadRequest frameRequest(request, frameName);

    // FIXME: It's much better for client API if a new window starts with a URL, here where we
    // know what URL we are going to open. Unfortunately, this code passes the empty string
    // for the URL, but there's a reason for that. Before loading we have to set up the opener,
    // openedByDOM, and dialogArguments values. Also, to decide whether to use the URL we currently
    // do an allowsAccessFrom call using the window we create, which can't be done before creating it.
    // We'd have to resolve all those issues to pass the URL instead of "".

    bool created;
    // We pass in the opener frame here so it can be used for looking up the frame name, in case the active frame
    // is different from the opener frame, and the name references a frame relative to the opener frame, for example
    // "_self" or "_parent".
    Frame* newFrame = activeFrame->loader()->createWindow(openerFrame->loader(), frameRequest, windowFeatures, created);
    if (!newFrame)
        return 0;

    newFrame->loader()->setOpener(openerFrame);
    newFrame->loader()->setOpenedByDOM();

    JSDOMWindow* newWindow = toJSDOMWindow(newFrame);

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

    JSDOMWindow* dialogWindow = toJSDOMWindow(dialogFrame);

    // Get the return value either just before clearing the dialog window's
    // properties (in JSDOMWindowBase::clear), or when on return from runModal.
    JSValue* returnValue = 0;
    dialogWindow->setReturnValueSlot(&returnValue);
    dialogFrame->page()->chrome()->runModal();
    dialogWindow->setReturnValueSlot(0);

    // If we don't have a return value, get it now.
    // Either JSDOMWindowBase::clear was not called yet, or there was no return value,
    // and in that case, there's no harm in trying again (no benefit either).
    if (!returnValue)
        returnValue = dialogWindow->getDirect("returnValue");

    return returnValue ? returnValue : jsUndefined();
}

JSValue *JSDOMWindowBase::getValueProperty(ExecState *exec, int token) const
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
    case Image:
      if (!allowsAccessFrom(exec))
        return jsUndefined();
      // FIXME: this property (and the few below) probably shouldn't create a new object every
      // time
      return new (exec) JSImageConstructor(exec, impl()->frame()->document());
    case Option:
      if (!allowsAccessFrom(exec))
        return jsUndefined();
      return new (exec) JSHTMLOptionElementConstructor(exec, impl()->frame()->document());
    case XMLHttpRequest:
      if (!allowsAccessFrom(exec))
        return jsUndefined();
      return new (exec) JSXMLHttpRequestConstructor(exec, impl()->frame()->document());
    case Audio:
#if ENABLE(VIDEO)
      if (!allowsAccessFrom(exec))
        return jsUndefined();
      if (!MediaPlayer::isAvailable())
        return jsUndefined();
      return new (exec) JSAudioConstructor(exec, impl()->frame()->document());
#else
      return jsUndefined();
#endif
    case XSLTProcessor:
#if ENABLE(XSLT)
      if (!allowsAccessFrom(exec))
        return jsUndefined();
      return new (exec) JSXSLTProcessorConstructor(exec);
#else
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

JSValue* JSDOMWindowBase::childFrameGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{
    return toJS(exec, static_cast<JSDOMWindowBase*>(slot.slotBase())->impl()->frame()->tree()->child(AtomicString(propertyName))->domWindow());
}

JSValue* JSDOMWindowBase::indexGetter(ExecState* exec, JSObject*, const Identifier&, const PropertySlot& slot)
{
    return toJS(exec, static_cast<JSDOMWindowBase*>(slot.slotBase())->impl()->frame()->tree()->child(slot.index())->domWindow());
}

JSValue* JSDOMWindowBase::namedItemGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSDOMWindowBase* thisObj = static_cast<JSDOMWindowBase*>(slot.slotBase());
    Document* doc = thisObj->impl()->frame()->document();
    ASSERT(thisObj->allowsAccessFrom(exec));
    ASSERT(doc);
    ASSERT(doc->isHTMLDocument());

    RefPtr<HTMLCollection> collection = doc->windowNamedItems(propertyName);
    if (collection->length() == 1)
        return toJS(exec, collection->firstItem());
    return toJS(exec, collection.get());
}

bool JSDOMWindowBase::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
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

    const HashEntry* entry = JSDOMWindowBaseTable.entry(propertyName);
    if (entry) {
        if (entry->attributes & Function) {
            if (entry->functionValue == windowProtoFuncShowModalDialog) {
                if (!canShowModalDialog(impl()->frame()))
                    return false;
            }
            if (allowsAccessFrom(exec))
                slot.setStaticEntry(this, entry, staticFunctionGetter);
            else
                slot.setUndefined(this);
        } else
            slot.setStaticEntry(this, entry, staticValueGetter<JSDOMWindowBase>);
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
    Document* document = impl()->frame()->document();
    if (document && document->isHTMLDocument()) {
        AtomicStringImpl* atomicPropertyName = AtomicString::find(propertyName);
        if (atomicPropertyName && (static_cast<HTMLDocument*>(document)->hasNamedItem(atomicPropertyName) || document->hasElementWithId(atomicPropertyName))) {
            slot.setCustom(this, namedItemGetter);
            return true;
        }
    }

    return Base::getOwnPropertySlot(exec, propertyName, slot);
}

void JSDOMWindowBase::put(ExecState* exec, const Identifier& propertyName, JSValue* value)
{
  const HashEntry* entry = JSDOMWindowBaseTable.entry(propertyName);
  if (entry) {
     if (entry->attributes & Function) {
       if (allowsAccessFrom(exec))
         Base::put(exec, propertyName, value);
       return;
    }
    if (entry->attributes & ReadOnly)
      return;

    switch (entry->integerValue) {
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

bool JSDOMWindowBase::allowsAccessFrom(const JSGlobalObject* other) const
{
    if (allowsAccessFromPrivate(other))
        return true;
    printErrorMessage(crossDomainAccessErrorMessage(other));
    return false;
}

bool JSDOMWindowBase::allowsAccessFrom(ExecState* exec) const
{
    if (allowsAccessFromPrivate(exec->lexicalGlobalObject()))
        return true;
    printErrorMessage(crossDomainAccessErrorMessage(exec->lexicalGlobalObject()));
    return false;
}
    
bool JSDOMWindowBase::allowsAccessFromNoErrorMessage(ExecState* exec) const
{
    return allowsAccessFromPrivate(exec->lexicalGlobalObject());
}

bool JSDOMWindowBase::allowsAccessFrom(ExecState* exec, String& message) const
{
    if (allowsAccessFromPrivate(exec->lexicalGlobalObject()))
        return true;
    message = crossDomainAccessErrorMessage(exec->lexicalGlobalObject());
    return false;
}
    
ALWAYS_INLINE bool JSDOMWindowBase::allowsAccessFromPrivate(const JSGlobalObject* other) const
{
    const JSDOMWindow* originWindow = asJSDOMWindow(other);
    const JSDOMWindow* targetWindow = toJSDOMWindow(impl()->frame());

    if (originWindow == targetWindow)
        return true;

    // JS may be attempting to access the "window" object, which should be valid,
    // even if the document hasn't been constructed yet.  If the document doesn't
    // exist yet allow JS to access the window object.
    if (!originWindow->impl()->document())
        return true;

    const SecurityOrigin* originSecurityOrigin = originWindow->impl()->securityOrigin();
    const SecurityOrigin* targetSecurityOrigin = targetWindow->impl()->securityOrigin();

    return originSecurityOrigin->canAccess(targetSecurityOrigin);
}

String JSDOMWindowBase::crossDomainAccessErrorMessage(const JSGlobalObject* other) const
{
    KURL originURL = asJSDOMWindow(other)->impl()->url();
    KURL targetURL = impl()->frame()->document()->url();
    if (originURL.isNull() || targetURL.isNull())
        return String();

    // FIXME: this error message should contain more specifics of why the same origin check has failed.
    return String::format("Unsafe JavaScript attempt to access frame with URL %s from frame with URL %s. Domains, protocols and ports must match.\n",
        targetURL.string().utf8().data(), originURL.string().utf8().data());
}

void JSDOMWindowBase::printErrorMessage(const String& message) const
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

    impl()->console()->addMessage(JSMessageSource, ErrorMessageLevel, message, 1, String()); // FIXME: provide a real line number and source URL.
}

ExecState* JSDOMWindowBase::globalExec()
{
    // We need to make sure that any script execution happening in this
    // frame does not destroy it
    ASSERT(impl()->frame());
    impl()->frame()->keepAlive();
    return Base::globalExec();
}

bool JSDOMWindowBase::shouldInterruptScript() const
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

void JSDOMWindowBase::setListener(ExecState* exec, const AtomicString& eventType, JSValue* func)
{
    ASSERT(impl()->frame());
    Document* doc = impl()->frame()->document();
    if (!doc)
        return;

    doc->setHTMLWindowEventListener(eventType, findOrCreateJSEventListener(exec, func, true));
}

JSValue* JSDOMWindowBase::getListener(ExecState* exec, const AtomicString& eventType) const
{
    ASSERT(impl()->frame());
    Document* doc = impl()->frame()->document();
    if (!doc)
        return jsUndefined();

    EventListener* listener = doc->getHTMLWindowEventListener(eventType);
    if (listener && static_cast<JSEventListener*>(listener)->listenerObj())
        return static_cast<JSEventListener*>(listener)->listenerObj();
    return jsNull();
}

JSEventListener* JSDOMWindowBase::findJSEventListener(JSValue* val, bool html)
{
    if (!val->isObject())
        return 0;
    JSObject* object = static_cast<JSObject*>(val);
    ListenersMap& listeners = html ? d->jsHTMLEventListeners : d->jsEventListeners;
    return listeners.get(object);
}

JSEventListener* JSDOMWindowBase::findOrCreateJSEventListener(ExecState* exec, JSValue* val, bool html)
{
    JSEventListener* listener = findJSEventListener(val, html);
    if (listener)
        return listener;

    if (!val->isObject())
        return 0;
    JSObject* object = static_cast<JSObject*>(val);

    // Note that the JSEventListener constructor adds it to our jsEventListeners list
    return new JSEventListener(object, static_cast<JSDOMWindow*>(this), html);
}

JSUnprotectedEventListener* JSDOMWindowBase::findJSUnprotectedEventListener(ExecState* exec, JSValue* val, bool html)
{
    if (!val->isObject())
        return 0;
    JSObject* object = static_cast<JSObject*>(val);
    UnprotectedListenersMap& listeners = html ? d->jsUnprotectedHTMLEventListeners : d->jsUnprotectedEventListeners;
    return listeners.get(object);
}

JSUnprotectedEventListener* JSDOMWindowBase::findOrCreateJSUnprotectedEventListener(ExecState* exec, JSValue* val, bool html)
{
    JSUnprotectedEventListener* listener = findJSUnprotectedEventListener(exec, val, html);
    if (listener)
        return listener;
    if (!val->isObject())
        return 0;
    JSObject* object = static_cast<JSObject*>(val);

    // The JSUnprotectedEventListener constructor adds it to our jsUnprotectedEventListeners map.
    return new JSUnprotectedEventListener(object, static_cast<JSDOMWindow*>(this), html);
}

void JSDOMWindowBase::clearHelperObjectProperties()
{
    d->m_evt = 0;
}

void JSDOMWindowBase::clear()
{
  JSLock lock;

  if (d->m_returnValueSlot && !*d->m_returnValueSlot)
    *d->m_returnValueSlot = getDirect("returnValue");

  clearAllTimeouts();
  clearHelperObjectProperties();
}

void JSDOMWindowBase::setCurrentEvent(Event* evt)
{
    d->m_evt = evt;
}

Event* JSDOMWindowBase::currentEvent()
{
    return d->m_evt;
}

JSObject* JSDOMWindowBase::toThisObject(ExecState*) const
{
    return shell();
}

JSDOMWindowShell* JSDOMWindowBase::shell() const
{
    return d->m_shell;
}

JSValue* windowProtoFuncAToB(ExecState* exec, JSObject* thisObj, const List& args)
{
    ASSERT(!thisObj->inherits(&JSDOMWindow::s_info));
    if (!thisObj->inherits(&JSDOMWindowShell::s_info))
        return throwError(exec, TypeError);
    JSDOMWindow* window = static_cast<JSDOMWindowShell*>(thisObj)->window();
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    JSValue* v = args[0];
    if (v->isNull())
        return jsString(exec);

    UString s = v->toString(exec);
    if (!s.is8Bit()) {
        setDOMException(exec, INVALID_CHARACTER_ERR);
        return jsUndefined();
    }

    Vector<char> in(s.size());
    for (int i = 0; i < s.size(); ++i)
        in[i] = static_cast<char>(s.data()[i]);
    Vector<char> out;

    if (!base64Decode(in, out))
        return throwError(exec, GeneralError, "Cannot decode base64");

    return jsString(exec, String(out.data(), out.size()));
}

JSValue* windowProtoFuncBToA(ExecState* exec, JSObject* thisObj, const List& args)
{
    ASSERT(!thisObj->inherits(&JSDOMWindow::s_info));
    if (!thisObj->inherits(&JSDOMWindowShell::s_info))
        return throwError(exec, TypeError);
    JSDOMWindow* window = static_cast<JSDOMWindowShell*>(thisObj)->window();
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    JSValue* v = args[0];
    if (v->isNull())
        return jsString(exec);

    UString s = v->toString(exec);
    if (!s.is8Bit()) {
        setDOMException(exec, INVALID_CHARACTER_ERR);
        return jsUndefined();
    }

    Vector<char> in(s.size());
    for (int i = 0; i < s.size(); ++i)
        in[i] = static_cast<char>(s.data()[i]);
    Vector<char> out;

    base64Encode(in, out);

    return jsString(exec, String(out.data(), out.size()));
}

JSValue* windowProtoFuncOpen(ExecState* exec, JSObject* thisObj, const List& args)
{
    ASSERT(!thisObj->inherits(&JSDOMWindow::s_info));
    if (!thisObj->inherits(&JSDOMWindowShell::s_info))
        return throwError(exec, TypeError);
    JSDOMWindow* window = static_cast<JSDOMWindowShell*>(thisObj)->window();
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();
    Frame* activeFrame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
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

        const JSDOMWindow* targetedWindow = toJSDOMWindow(frame);
        if (!completedURL.isEmpty() && (!protocolIs(completedURL, "javascript") || (targetedWindow && targetedWindow->allowsAccessFrom(exec)))) {
            bool userGesture = activeFrame->scriptProxy()->processingUserGesture();
            frame->loader()->scheduleLocationChange(completedURL, activeFrame->loader()->outgoingReferrer(), false, userGesture);
        }
        return toJS(exec, frame->domWindow());
    }

    // In the case of a named frame or a new window, we'll use the createWindow() helper
    WindowFeatures windowFeatures(valueToStringWithUndefinedOrNullCheck(exec, args[2]));
    FloatRect windowRect(windowFeatures.x, windowFeatures.y, windowFeatures.width, windowFeatures.height);
    DOMWindow::adjustWindowRect(screenAvailableRect(page->mainFrame()->view()), windowRect, windowRect);

    windowFeatures.x = windowRect.x();
    windowFeatures.y = windowRect.y();
    windowFeatures.height = windowRect.height();
    windowFeatures.width = windowRect.width();

    frame = createWindow(exec, frame, urlString, frameName, windowFeatures, 0);

    if (!frame)
        return jsUndefined();

    return toJS(exec, frame->domWindow()); // global object
}

JSValue* windowProtoFuncSetTimeout(ExecState* exec, JSObject* thisObj, const List& args)
{
    ASSERT(!thisObj->inherits(&JSDOMWindow::s_info));
    if (!thisObj->inherits(&JSDOMWindowShell::s_info))
        return throwError(exec, TypeError);
    JSDOMWindow* window = static_cast<JSDOMWindowShell*>(thisObj)->window();
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    JSValue* v = args[0];
    if (v->isString())
        return jsNumber(exec, window->installTimeout(v->toString(exec), args[1]->toInt32(exec), true /*single shot*/));
    if (v->isObject() && static_cast<JSObject*>(v)->implementsCall()) {
        List argsTail;
        args.getSlice(2, argsTail);
        return jsNumber(exec, window->installTimeout(v, argsTail, args[1]->toInt32(exec), true /*single shot*/));
    }

    return jsUndefined();
}

JSValue* windowProtoFuncClearTimeout(ExecState* exec, JSObject* thisObj, const List& args)
{
    // Also the implementation for window.clearInterval()
    ASSERT(!thisObj->inherits(&JSDOMWindow::s_info));
    if (!thisObj->inherits(&JSDOMWindowShell::s_info))
        return throwError(exec, TypeError);
    JSDOMWindow* window = static_cast<JSDOMWindowShell*>(thisObj)->window();
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    window->clearTimeout(args[0]->toInt32(exec));
    return jsUndefined();
}

JSValue* windowProtoFuncSetInterval(ExecState* exec, JSObject* thisObj, const List& args)
{
    ASSERT(!thisObj->inherits(&JSDOMWindow::s_info));
    if (!thisObj->inherits(&JSDOMWindowShell::s_info))
        return throwError(exec, TypeError);
    JSDOMWindow* window = static_cast<JSDOMWindowShell*>(thisObj)->window();
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    if (args.size() >= 2) {
        JSValue* v = args[0];
        int delay = args[1]->toInt32(exec);
        if (v->isString())
            return jsNumber(exec, window->installTimeout(v->toString(exec), delay, false));
        if (v->isObject() && static_cast<JSObject*>(v)->implementsCall()) {
            List argsTail;
            args.getSlice(2, argsTail);
            return jsNumber(exec, window->installTimeout(v, argsTail, delay, false));
        }
    }

    return jsUndefined();

}

JSValue* windowProtoFuncAddEventListener(ExecState* exec, JSObject* thisObj, const List& args)
{
    ASSERT(!thisObj->inherits(&JSDOMWindow::s_info));
    if (!thisObj->inherits(&JSDOMWindowShell::s_info))
        return throwError(exec, TypeError);
    JSDOMWindow* window = static_cast<JSDOMWindowShell*>(thisObj)->window();
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    if (JSEventListener* listener = window->findOrCreateJSEventListener(exec, args[1])) {
        if (Document* doc = frame->document())
            doc->addWindowEventListener(AtomicString(args[0]->toString(exec)), listener, args[2]->toBoolean(exec));
    }

    return jsUndefined();
}

JSValue* windowProtoFuncRemoveEventListener(ExecState* exec, JSObject* thisObj, const List& args)
{
    ASSERT(!thisObj->inherits(&JSDOMWindow::s_info));
    if (!thisObj->inherits(&JSDOMWindowShell::s_info))
        return throwError(exec, TypeError);
    JSDOMWindow* window = static_cast<JSDOMWindowShell*>(thisObj)->window();
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
    ASSERT(!thisObj->inherits(&JSDOMWindow::s_info));
    if (!thisObj->inherits(&JSDOMWindowShell::s_info))
        return throwError(exec, TypeError);
    JSDOMWindow* window = static_cast<JSDOMWindowShell*>(thisObj)->window();
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    return showModalDialog(exec, frame, valueToStringWithUndefinedOrNullCheck(exec, args[0]), args[1], valueToStringWithUndefinedOrNullCheck(exec, args[2]));
}

JSValue* windowProtoFuncNotImplemented(ExecState* exec, JSObject* thisObj, const List& args)
{
    ASSERT(!thisObj->inherits(&JSDOMWindow::s_info));
    if (!thisObj->inherits(&JSDOMWindowShell::s_info))
        return throwError(exec, TypeError);
    return jsUndefined();
}

void JSDOMWindowBase::setReturnValueSlot(JSValue** slot)
{
    d->m_returnValueSlot = slot;
}

////////////////////// timeouts ////////////////////////

void JSDOMWindowBase::clearAllTimeouts()
{
    deleteAllValues(d->m_timeouts);
    d->m_timeouts.clear();
}

int JSDOMWindowBase::installTimeout(ScheduledAction* a, int t, bool singleShot)
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

int JSDOMWindowBase::installTimeout(const UString& handler, int t, bool singleShot)
{
    return installTimeout(new ScheduledAction(handler), t, singleShot);
}

int JSDOMWindowBase::installTimeout(JSValue* func, const List& args, int t, bool singleShot)
{
    return installTimeout(new ScheduledAction(func, args), t, singleShot);
}

PausedTimeouts* JSDOMWindowBase::pauseTimeouts()
{
    size_t count = d->m_timeouts.size();
    if (count == 0)
        return 0;

    PausedTimeout* t = new PausedTimeout [count];
    PausedTimeouts* result = new PausedTimeouts(t, count);

    JSDOMWindowBasePrivate::TimeoutsMap::iterator it = d->m_timeouts.begin();
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

void JSDOMWindowBase::resumeTimeouts(PausedTimeouts* timeouts)
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

void JSDOMWindowBase::clearTimeout(int timeoutId, bool delAction)
{
    // timeout IDs have to be positive, and 0 and -1 are unsafe to
    // even look up since they are the empty and deleted value
    // respectively
    if (timeoutId <= 0)
        return;

    delete d->m_timeouts.take(timeoutId);
}

void JSDOMWindowBase::timerFired(DOMWindowTimer* timer)
{
    // Simple case for non-one-shot timers.
    if (timer->isActive()) {
        int timeoutId = timer->timeoutId();

        timer->action()->execute(shell());
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
    ScheduledAction* action = timer->takeAction();
    d->m_timeouts.remove(timer->timeoutId());
    delete timer;
    action->execute(shell());

    JSLock lock;
    delete action;
}

void JSDOMWindowBase::disconnectFrame()
{
    clearAllTimeouts();
}

JSDOMWindowBase::ListenersMap& JSDOMWindowBase::jsEventListeners()
{
    return d->jsEventListeners;
}

JSDOMWindowBase::ListenersMap& JSDOMWindowBase::jsHTMLEventListeners()
{
    return d->jsHTMLEventListeners;
}

JSDOMWindowBase::UnprotectedListenersMap& JSDOMWindowBase::jsUnprotectedEventListeners()
{
    return d->jsUnprotectedEventListeners;
}

JSDOMWindowBase::UnprotectedListenersMap& JSDOMWindowBase::jsUnprotectedHTMLEventListeners()
{
    return d->jsUnprotectedHTMLEventListeners;
}

void DOMWindowTimer::fired()
{
    timerNestingLevel = m_nestingLevel;
    m_object->timerFired(this);
    timerNestingLevel = 0;
}

JSValue* toJS(ExecState*, DOMWindow* domWindow)
{
    if (!domWindow)
        return jsNull();
    Frame* frame = domWindow->frame();
    if (!frame)
        return jsNull();
    return frame->scriptProxy()->windowShell();
}

JSDOMWindow* toJSDOMWindow(Frame* frame)
{
    if (!frame)
        return 0;
    return frame->scriptProxy()->windowShell()->window();
}

JSDOMWindow* asJSDOMWindow(JSGlobalObject* globalObject)
{
    return static_cast<JSDOMWindow*>(globalObject);
}

const JSDOMWindow* asJSDOMWindow(const JSGlobalObject* globalObject)
{
    return static_cast<const JSDOMWindow*>(globalObject);
}

} // namespace WebCore
