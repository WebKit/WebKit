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
#include "JSDOMWindowCustom.h"
#include "JSEvent.h"
#include "JSEventListener.h"
#include "JSHTMLCollection.h"
#include "JSHTMLOptionElementConstructor.h"
#include "JSImageConstructor.h"
#include "JSMessageChannelConstructor.h"
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
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "WindowFeatures.h"
#include "htmlediting.h"
#include <kjs/Error.h>
#include <kjs/JSLock.h>
#include <wtf/AlwaysInline.h>
#include <wtf/MathExtras.h>

#if ENABLE(XSLT)
#include "JSXSLTProcessorConstructor.h"
#endif

using namespace JSC;

namespace WebCore {

static JSValue* windowProtoFuncAToB(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* windowProtoFuncBToA(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* windowProtoFuncOpen(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* windowProtoFuncSetTimeout(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* windowProtoFuncClearTimeoutOrInterval(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* windowProtoFuncSetInterval(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* windowProtoFuncAddEventListener(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* windowProtoFuncRemoveEventListener(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* windowProtoFuncShowModalDialog(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* windowProtoFuncNotImplemented(ExecState*, JSObject*, JSValue*, const ArgList&);

static JSValue* jsDOMWindowBaseCrypto(ExecState*, const Identifier&, const PropertySlot&);
static JSValue* jsDOMWindowBaseEvent(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseEvent(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnabort(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnabort(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnblur(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnblur(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnchange(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnchange(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnclick(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnclick(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOndblclick(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOndblclick(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnerror(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnerror(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnfocus(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnfocus(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnkeydown(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnkeydown(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnkeypress(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnkeypress(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnkeyup(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnkeyup(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnload(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnload(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnmousedown(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnmousedown(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnmousemove(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnmousemove(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnmouseout(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnmouseout(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnmouseover(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnmouseover(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnmouseup(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnmouseup(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnMouseWheel(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnMouseWheel(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnreset(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnreset(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnresize(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnresize(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnscroll(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnscroll(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnsearch(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnsearch(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnselect(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnselect(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnsubmit(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnsubmit(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnunload(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnunload(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnbeforeunload(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnbeforeunload(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnWebKitAnimationStart(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnWebKitAnimationStart(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnWebKitAnimationIteration(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnWebKitAnimationIteration(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnWebKitAnimationEnd(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnWebKitAnimationEnd(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOnWebKitTransitionEnd(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOnWebKitTransitionEnd(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseAudio(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseAudio(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseImage(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseImage(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseMessageChannel(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseMessageChannel(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseOption(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseOption(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseXMLHttpRequest(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseXMLHttpRequest(ExecState*, JSObject*, JSValue*);
static JSValue* jsDOMWindowBaseXSLTProcessor(ExecState*, const Identifier&, const PropertySlot&);
static void setJSDOMWindowBaseXSLTProcessor(ExecState*, JSObject*, JSValue*);

}

#include "JSDOMWindowBase.lut.h"

namespace WebCore {

using namespace EventNames;

static int lastUsedTimeoutId;

static int timerNestingLevel = 0;
const int cMaxTimerNestingLevel = 5;
const double cMinimumTimerInterval = 0.010;

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
        JSLock lock(false);
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
@begin JSDOMWindowBaseTable
# -- Functions --
  atob                          windowProtoFuncAToB                         DontDelete|Function 1
  btoa                          windowProtoFuncBToA                         DontDelete|Function 1
  open                          windowProtoFuncOpen                         DontDelete|Function 3
  setTimeout                    windowProtoFuncSetTimeout                   DontDelete|Function 2
  clearTimeout                  windowProtoFuncClearTimeoutOrInterval       DontDelete|Function 1
  setInterval                   windowProtoFuncSetInterval                  DontDelete|Function 2
  clearInterval                 windowProtoFuncClearTimeoutOrInterval       DontDelete|Function 1
  addEventListener              windowProtoFuncAddEventListener             DontDelete|Function 3
  removeEventListener           windowProtoFuncRemoveEventListener          DontDelete|Function 3
  showModalDialog               windowProtoFuncShowModalDialog              DontDelete|Function 1
# Not implemented
  captureEvents                 windowProtoFuncNotImplemented               DontDelete|Function 0
  releaseEvents                 windowProtoFuncNotImplemented               DontDelete|Function 0

# -- Attributes --
  crypto                        jsDOMWindowBaseCrypto                       DontDelete|ReadOnly
  event                         jsDOMWindowBaseEvent                        DontDelete
# -- Event Listeners --
  onabort                       jsDOMWindowBaseOnabort                      DontDelete
  onblur                        jsDOMWindowBaseOnblur                       DontDelete
  onchange                      jsDOMWindowBaseOnchange                     DontDelete
  onclick                       jsDOMWindowBaseOnclick                      DontDelete
  ondblclick                    jsDOMWindowBaseOndblclick                   DontDelete
  onerror                       jsDOMWindowBaseOnerror                      DontDelete
  onfocus                       jsDOMWindowBaseOnfocus                      DontDelete
  onkeydown                     jsDOMWindowBaseOnkeydown                    DontDelete
  onkeypress                    jsDOMWindowBaseOnkeypress                   DontDelete
  onkeyup                       jsDOMWindowBaseOnkeyup                      DontDelete
  onload                        jsDOMWindowBaseOnload                       DontDelete
  onmousedown                   jsDOMWindowBaseOnmousedown                  DontDelete
  onmousemove                   jsDOMWindowBaseOnmousemove                  DontDelete
  onmouseout                    jsDOMWindowBaseOnmouseout                   DontDelete
  onmouseover                   jsDOMWindowBaseOnmouseover                  DontDelete
  onmouseup                     jsDOMWindowBaseOnmouseup                    DontDelete
  onmousewheel                  jsDOMWindowBaseOnMouseWheel                 DontDelete
  onreset                       jsDOMWindowBaseOnreset                      DontDelete
  onresize                      jsDOMWindowBaseOnresize                     DontDelete
  onscroll                      jsDOMWindowBaseOnscroll                     DontDelete
  onsearch                      jsDOMWindowBaseOnsearch                     DontDelete
  onselect                      jsDOMWindowBaseOnselect                     DontDelete
  onsubmit                      jsDOMWindowBaseOnsubmit                     DontDelete
  onunload                      jsDOMWindowBaseOnunload                     DontDelete
  onbeforeunload                jsDOMWindowBaseOnbeforeunload               DontDelete
  onwebkitanimationstart        jsDOMWindowBaseOnWebKitAnimationStart       DontDelete
  onwebkitanimationiteration    jsDOMWindowBaseOnWebKitAnimationIteration   DontDelete
  onwebkitanimationend          jsDOMWindowBaseOnWebKitAnimationEnd         DontDelete
  onwebkittransitionend         jsDOMWindowBaseOnWebKitTransitionEnd        DontDelete
# -- Constructors --
  Audio                         jsDOMWindowBaseAudio                        DontDelete
  Image                         jsDOMWindowBaseImage                        DontDelete
  MessageChannel                jsDOMWindowBaseMessageChannel               DontDelete
  Option                        jsDOMWindowBaseOption                       DontDelete
  XMLHttpRequest                jsDOMWindowBaseXMLHttpRequest               DontDelete
  XSLTProcessor                 jsDOMWindowBaseXSLTProcessor                DontDelete
@end
*/

JSDOMWindowBase::JSDOMWindowBaseData::JSDOMWindowBaseData(PassRefPtr<DOMWindow> window, JSDOMWindowBase* jsWindow, JSDOMWindowShell* shell)
    : JSGlobalObjectData(jsWindow, shell)
    , impl(window)
    , evt(0)
    , returnValueSlot(0)
    , shell(shell)
{
}

JSDOMWindowBase::JSDOMWindowBase(PassRefPtr<StructureID> structure, PassRefPtr<DOMWindow> window, JSDOMWindowShell* shell)
    : JSGlobalObject(structure, new JSDOMWindowBaseData(window, this, shell), shell)
{
    // Time in milliseconds before the script timeout handler kicks in.
    setTimeoutTime(10000);

    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(Identifier(globalExec(), "document"), jsNull(), DontDelete | ReadOnly),
        GlobalPropertyInfo(Identifier(globalExec(), "window"), d()->shell, DontDelete | ReadOnly)
    };
    
    addStaticGlobals(staticGlobals, sizeof(staticGlobals) / sizeof(GlobalPropertyInfo));
}

void JSDOMWindowBase::updateDocument()
{
    ASSERT(d()->impl->document());
    ExecState* exec = globalExec();
    symbolTablePutWithAttributes(Identifier(exec, "document"), toJS(exec, d()->impl->document()), DontDelete | ReadOnly);
}

JSDOMWindowBase::~JSDOMWindowBase()
{
    if (d()->impl->frame())
        d()->impl->frame()->script()->clearFormerWindow(asJSDOMWindow(this));

    clearAllTimeouts();

    // Clear any backpointers to the window
    ListenersMap::iterator i2 = d()->jsEventListeners.begin();
    ListenersMap::iterator e2 = d()->jsEventListeners.end();
    for (; i2 != e2; ++i2)
        i2->second->clearWindow();

    i2 = d()->jsEventListenersAttachedToEventTargetNodes.begin();
    e2 = d()->jsEventListenersAttachedToEventTargetNodes.end();
    for (; i2 != e2; ++i2)
        i2->second->clearWindow();

    UnprotectedListenersMap::iterator i1 = d()->jsUnprotectedEventListeners.begin();
    UnprotectedListenersMap::iterator e1 = d()->jsUnprotectedEventListeners.end();
    for (; i1 != e1; ++i1)
        i1->second->clearWindow();

    i1 = d()->jsUnprotectedEventListenersAttachedToEventTargetNodes.begin();
    e1 = d()->jsUnprotectedEventListenersAttachedToEventTargetNodes.end();
    for (; i1 != e1; ++i1)
        i1->second->clearWindow();
}

static bool allowPopUp(ExecState* exec)
{
    Frame* frame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();

    ASSERT(frame);
    if (frame->script()->processingUserGesture())
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
        newWindow->putDirect(Identifier(exec, "dialogArguments"), dialogArgs);

    if (!protocolIs(url, "javascript") || newWindow->allowsAccessFrom(exec)) {
        KURL completedURL = url.isEmpty() ? KURL("") : activeFrame->document()->completeURL(url);
        bool userGesture = activeFrame->script()->processingUserGesture();

        if (created)
            newFrame->loader()->changeLocation(completedURL, activeFrame->loader()->outgoingReferrer(), false, userGesture);
        else if (!url.isEmpty())
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
        returnValue = dialogWindow->getDirect(Identifier(exec, "returnValue"));

    return returnValue ? returnValue : jsUndefined();
}

template<class ConstructorClass>
static JSObject* getDOMConstructor(JSC::ExecState* exec, const JSDOMWindowBase* window)
{
    if (JSObject* constructor = window->constructors().get(&ConstructorClass::s_info))
        return constructor;
    JSObject* constructor = new (exec) ConstructorClass(exec, window->impl()->document());
    ASSERT(!window->constructors().contains(&ConstructorClass::s_info));
    window->constructors().set(&ConstructorClass::s_info, constructor);
    return constructor;
}

JSValue* jsDOMWindowBaseCrypto(ExecState*, const Identifier&, const PropertySlot&)
{
    return jsUndefined(); // FIXME: implement this
}

JSValue* jsDOMWindowBaseEvent(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->currentEvent())
        return jsUndefined();
    return toJS(exec, static_cast<JSDOMWindowBase*>(slot.slotBase())->currentEvent());
}

JSValue* jsDOMWindowBaseImage(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return getDOMConstructor<JSImageConstructor>(exec, static_cast<JSDOMWindowBase*>(slot.slotBase()));
}

JSValue* jsDOMWindowBaseMessageChannel(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return getDOMConstructor<JSMessageChannelConstructor>(exec, static_cast<JSDOMWindowBase*>(slot.slotBase()));
}

JSValue* jsDOMWindowBaseOption(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return getDOMConstructor<JSHTMLOptionElementConstructor>(exec, static_cast<JSDOMWindowBase*>(slot.slotBase()));
}

JSValue* jsDOMWindowBaseXMLHttpRequest(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return getDOMConstructor<JSXMLHttpRequestConstructor>(exec, static_cast<JSDOMWindowBase*>(slot.slotBase()));
}

JSValue* jsDOMWindowBaseAudio(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
#if ENABLE(VIDEO)
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    if (!MediaPlayer::isAvailable())
        return jsUndefined();
    return getDOMConstructor<JSAudioConstructor>(exec, static_cast<JSDOMWindowBase*>(slot.slotBase()));
#else
    return jsUndefined();
#endif
}

JSValue* jsDOMWindowBaseXSLTProcessor(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
#if ENABLE(XSLT)
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return getDOMConstructor<JSXSLTProcessorConstructor>(exec);
#else
    return jsUndefined();
#endif
}

JSValue* jsDOMWindowBaseOnabort(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, abortEvent);
}

JSValue* jsDOMWindowBaseOnblur(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, blurEvent);
}

JSValue* jsDOMWindowBaseOnchange(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, changeEvent);
}

JSValue* jsDOMWindowBaseOnclick(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, clickEvent);
}

JSValue* jsDOMWindowBaseOndblclick(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, dblclickEvent);
}

JSValue* jsDOMWindowBaseOnerror(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, errorEvent);
}

JSValue* jsDOMWindowBaseOnfocus(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, focusEvent);
}

JSValue* jsDOMWindowBaseOnkeydown(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, keydownEvent);
}

JSValue* jsDOMWindowBaseOnkeypress(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, keypressEvent);
}

JSValue* jsDOMWindowBaseOnkeyup(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, keyupEvent);
}

JSValue* jsDOMWindowBaseOnload(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, loadEvent);
}

JSValue* jsDOMWindowBaseOnmousedown(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, mousedownEvent);
}

JSValue* jsDOMWindowBaseOnmousemove(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, mousemoveEvent);
}

JSValue* jsDOMWindowBaseOnmouseout(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, mouseoutEvent);
}

JSValue* jsDOMWindowBaseOnmouseover(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, mouseoverEvent);
}

JSValue* jsDOMWindowBaseOnmouseup(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, mouseupEvent);
}

JSValue* jsDOMWindowBaseOnMouseWheel(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, mousewheelEvent);
}

JSValue* jsDOMWindowBaseOnreset(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, resetEvent);
}

JSValue* jsDOMWindowBaseOnresize(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, resizeEvent);
}

JSValue* jsDOMWindowBaseOnscroll(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, scrollEvent);
}

JSValue* jsDOMWindowBaseOnsearch(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, searchEvent);
}

JSValue* jsDOMWindowBaseOnselect(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, selectEvent);
}

JSValue* jsDOMWindowBaseOnsubmit(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, submitEvent);
}

JSValue* jsDOMWindowBaseOnbeforeunload(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, beforeunloadEvent);
}

JSValue* jsDOMWindowBaseOnunload(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, unloadEvent);
}

JSValue* jsDOMWindowBaseOnWebKitAnimationStart(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, webkitAnimationStartEvent);
}

JSValue* jsDOMWindowBaseOnWebKitAnimationIteration(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, webkitAnimationIterationEvent);
}

JSValue* jsDOMWindowBaseOnWebKitAnimationEnd(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, webkitAnimationEndEvent);
}

JSValue* jsDOMWindowBaseOnWebKitTransitionEnd(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    if (!static_cast<JSDOMWindowBase*>(slot.slotBase())->allowsAccessFrom(exec))
        return jsUndefined();
    return static_cast<JSDOMWindowBase*>(slot.slotBase())->getListener(exec, webkitTransitionEndEvent);
}

JSValue* JSDOMWindowBase::childFrameGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    return toJS(exec, static_cast<JSDOMWindowBase*>(slot.slotBase())->impl()->frame()->tree()->child(AtomicString(propertyName))->domWindow());
}

JSValue* JSDOMWindowBase::indexGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return toJS(exec, static_cast<JSDOMWindowBase*>(slot.slotBase())->impl()->frame()->tree()->child(slot.index())->domWindow());
}

JSValue* JSDOMWindowBase::namedItemGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
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

    const HashEntry* entry = JSDOMWindowBaseTable.entry(exec, propertyName);
    if (entry) {
        if (entry->attributes() & Function) {
            if (entry->function() == windowProtoFuncShowModalDialog) {
                if (!canShowModalDialog(impl()->frame()))
                    return false;
            }
            if (allowsAccessFrom(exec))
                setUpStaticFunctionSlot(exec, entry, this, propertyName, slot);
            else
                slot.setUndefined();
        } else
            slot.setCustom(this, entry->propertyGetter());
        return true;
    }

    // Do prototype lookup early so that functions and attributes in the prototype can have
    // precedence over the index and name getters.  
    JSValue* proto = prototype();
    if (proto->isObject()) {
        if (static_cast<JSObject*>(proto)->getPropertySlot(exec, propertyName, slot)) {
            if (!allowsAccessFrom(exec))
                slot.setUndefined();
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
        slot.setUndefined();
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

void JSDOMWindowBase::put(ExecState* exec, const Identifier& propertyName, JSValue* value, PutPropertySlot& slot)
{
    const HashEntry* entry = JSDOMWindowBaseTable.entry(exec, propertyName);
    if (entry) {
        if (entry->attributes() & Function) {
            if (allowsAccessFrom(exec))
                Base::put(exec, propertyName, value, slot);
            return;
        }
        if (entry->attributes() & ReadOnly)
            return;

        // Don't call the put function for replacable properties.
        if (!(entry->propertyPutter() == setJSDOMWindowBaseEvent
                || entry->propertyPutter() == setJSDOMWindowBaseAudio
                || entry->propertyPutter() == setJSDOMWindowBaseImage
                || entry->propertyPutter() == setJSDOMWindowBaseOption
                || entry->propertyPutter() == setJSDOMWindowBaseMessageChannel
                || entry->propertyPutter() == setJSDOMWindowBaseXMLHttpRequest
                || entry->propertyPutter() == setJSDOMWindowBaseXSLTProcessor)) {
            entry->propertyPutter()(exec, this, value);
            return;
        }
    }

    if (allowsAccessFrom(exec))
        Base::put(exec, propertyName, value, slot);
}

void setJSDOMWindowBaseOnabort(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, abortEvent, value);
}

void setJSDOMWindowBaseOnblur(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, blurEvent, value);
}

void setJSDOMWindowBaseOnchange(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, changeEvent, value);
}

void setJSDOMWindowBaseOnclick(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, clickEvent, value);
}

void setJSDOMWindowBaseOndblclick(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, dblclickEvent, value);
}

void setJSDOMWindowBaseOnerror(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, errorEvent, value);
}

void setJSDOMWindowBaseOnfocus(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, focusEvent, value);
}

void setJSDOMWindowBaseOnkeydown(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, keydownEvent, value);
}

void setJSDOMWindowBaseOnkeypress(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, keypressEvent, value);
}

void setJSDOMWindowBaseOnkeyup(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, keyupEvent, value);
}

void setJSDOMWindowBaseOnload(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, loadEvent, value);
}

void setJSDOMWindowBaseOnmousedown(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, mousedownEvent, value);
}

void setJSDOMWindowBaseOnmousemove(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, mousemoveEvent, value);
}

void setJSDOMWindowBaseOnmouseout(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, mouseoutEvent, value);
}

void setJSDOMWindowBaseOnmouseover(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, mouseoverEvent, value);
}

void setJSDOMWindowBaseOnmouseup(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, mouseupEvent, value);
}

void setJSDOMWindowBaseOnMouseWheel(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, mousewheelEvent, value);
}

void setJSDOMWindowBaseOnreset(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, resetEvent, value);
}

void setJSDOMWindowBaseOnresize(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, resizeEvent, value);
}

void setJSDOMWindowBaseOnscroll(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, scrollEvent, value);
}

void setJSDOMWindowBaseOnsearch(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, searchEvent, value);
}

void setJSDOMWindowBaseOnselect(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, selectEvent, value);
}

void setJSDOMWindowBaseOnsubmit(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, submitEvent, value);
}

void setJSDOMWindowBaseOnbeforeunload(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, beforeunloadEvent, value);
}

void setJSDOMWindowBaseOnunload(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, unloadEvent, value);
}

void setJSDOMWindowBaseOnWebKitAnimationStart(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, webkitAnimationStartEvent, value);
}

void setJSDOMWindowBaseOnWebKitAnimationIteration(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, webkitAnimationIterationEvent, value);
}

void setJSDOMWindowBaseOnWebKitAnimationEnd(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, webkitAnimationEndEvent, value);
}

void setJSDOMWindowBaseOnWebKitTransitionEnd(ExecState* exec, JSObject* baseObject, JSValue* value)
{
    if (static_cast<JSDOMWindowBase*>(baseObject)->allowsAccessFrom(exec))
        static_cast<JSDOMWindowBase*>(baseObject)->setListener(exec, webkitTransitionEndEvent, value);
}

void setJSDOMWindowBaseEvent(ExecState*, JSObject*, JSValue*)
{
    ASSERT_NOT_REACHED();
}

void setJSDOMWindowBaseAudio(ExecState*, JSObject*, JSValue*)
{
    ASSERT_NOT_REACHED();
}

void setJSDOMWindowBaseImage(ExecState*, JSObject*, JSValue*)
{
    ASSERT_NOT_REACHED();
}

void setJSDOMWindowBaseMessageChannel(ExecState*, JSObject*, JSValue*)
{
    ASSERT_NOT_REACHED();
}

void setJSDOMWindowBaseOption(ExecState*, JSObject*, JSValue*)
{
    ASSERT_NOT_REACHED();
}

void setJSDOMWindowBaseXMLHttpRequest(ExecState*, JSObject*, JSValue*)
{
    ASSERT_NOT_REACHED();
}

void setJSDOMWindowBaseXSLTProcessor(ExecState*, JSObject*, JSValue*)
{
    ASSERT_NOT_REACHED();
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

    Settings* settings = frame->settings();
    if (!settings)
        return;
    
    if (settings->privateBrowsingEnabled())
        return;

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

    doc->setWindowEventListenerForType(eventType, findOrCreateJSEventListener(exec, func, true));
}

JSValue* JSDOMWindowBase::getListener(ExecState* exec, const AtomicString& eventType) const
{
    ASSERT(impl()->frame());
    Document* doc = impl()->frame()->document();
    if (!doc)
        return jsUndefined();

    EventListener* listener = doc->windowEventListenerForType(eventType);
    if (listener && static_cast<JSEventListener*>(listener)->listenerObj())
        return static_cast<JSEventListener*>(listener)->listenerObj();
    return jsNull();
}

JSEventListener* JSDOMWindowBase::findJSEventListener(JSValue* val, bool attachedToEventTargetNode)
{
    if (!val->isObject())
        return 0;
    JSObject* object = static_cast<JSObject*>(val);
    ListenersMap& listeners = attachedToEventTargetNode ? d()->jsEventListenersAttachedToEventTargetNodes : d()->jsEventListeners;
    return listeners.get(object);
}

PassRefPtr<JSEventListener> JSDOMWindowBase::findOrCreateJSEventListener(ExecState* exec, JSValue* val, bool attachedToEventTargetNode)
{
    if (JSEventListener* listener = findJSEventListener(val, attachedToEventTargetNode))
        return listener;

    if (!val->isObject())
        return 0;

    // The JSEventListener constructor adds it to our jsEventListeners map.
    return JSEventListener::create(static_cast<JSObject*>(val), static_cast<JSDOMWindow*>(this), attachedToEventTargetNode).get();
}

JSUnprotectedEventListener* JSDOMWindowBase::findJSUnprotectedEventListener(ExecState* exec, JSValue* val, bool attachedToEventTargetNode)
{
    if (!val->isObject())
        return 0;

    UnprotectedListenersMap& listeners = attachedToEventTargetNode ? d()->jsUnprotectedEventListenersAttachedToEventTargetNodes : d()->jsUnprotectedEventListeners;
    return listeners.get(static_cast<JSObject*>(val));
}

PassRefPtr<JSUnprotectedEventListener> JSDOMWindowBase::findOrCreateJSUnprotectedEventListener(ExecState* exec, JSValue* val, bool attachedToEventTargetNode)
{
    if (JSUnprotectedEventListener* listener = findJSUnprotectedEventListener(exec, val, attachedToEventTargetNode))
        return listener;

    if (!val->isObject())
        return 0;

    // The JSUnprotectedEventListener constructor adds it to our jsUnprotectedEventListeners map.
    return JSUnprotectedEventListener::create(static_cast<JSObject*>(val), static_cast<JSDOMWindow*>(this), attachedToEventTargetNode).get();
}

void JSDOMWindowBase::clearHelperObjectProperties()
{
    d()->evt = 0;
}

void JSDOMWindowBase::clear()
{
    JSLock lock(false);

    if (d()->returnValueSlot && !*d()->returnValueSlot)
        *d()->returnValueSlot = getDirect(Identifier(globalExec(), "returnValue"));

    clearAllTimeouts();
    clearHelperObjectProperties();
}

void JSDOMWindowBase::setCurrentEvent(Event* evt)
{
    d()->evt = evt;
}

Event* JSDOMWindowBase::currentEvent()
{
    return d()->evt;
}

JSObject* JSDOMWindowBase::toThisObject(ExecState*) const
{
    return shell();
}

JSDOMWindowShell* JSDOMWindowBase::shell() const
{
    return d()->shell;
}

JSGlobalData* JSDOMWindowBase::commonJSGlobalData()
{
    static JSGlobalData* globalData = JSGlobalData::create().releaseRef();
    return globalData;
}

JSValue* windowProtoFuncAToB(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSDOMWindow* window = toJSDOMWindow(thisValue);
    if (!window)
        return throwError(exec, TypeError);
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    JSValue* v = args.at(exec, 0);
    if (v->isNull())
        return jsEmptyString(exec);

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

JSValue* windowProtoFuncBToA(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSDOMWindow* window = toJSDOMWindow(thisValue);
    if (!window)
        return throwError(exec, TypeError);
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    JSValue* v = args.at(exec, 0);
    if (v->isNull())
        return jsEmptyString(exec);

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

JSValue* windowProtoFuncOpen(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSDOMWindow* window = toJSDOMWindow(thisValue);
    if (!window)
        return throwError(exec, TypeError);
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();
    Frame* activeFrame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    if (!activeFrame)
        return  jsUndefined();

    Page* page = frame->page();

    String urlString = valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, 0));
    AtomicString frameName = args.at(exec, 1)->isUndefinedOrNull() ? "_blank" : AtomicString(args.at(exec, 1)->toString(exec));

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
            bool userGesture = activeFrame->script()->processingUserGesture();
            frame->loader()->scheduleLocationChange(completedURL, activeFrame->loader()->outgoingReferrer(), false, userGesture);
        }
        return toJS(exec, frame->domWindow());
    }

    // In the case of a named frame or a new window, we'll use the createWindow() helper
    WindowFeatures windowFeatures(valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, 2)));
    FloatRect windowRect(windowFeatures.xSet ? windowFeatures.x : 0, windowFeatures.ySet ? windowFeatures.y : 0,
                         windowFeatures.widthSet ? windowFeatures.width : 0, windowFeatures.heightSet ? windowFeatures.height : 0);
    DOMWindow::adjustWindowRect(screenAvailableRect(page ? page->mainFrame()->view() : 0), windowRect, windowRect);

    windowFeatures.x = windowRect.x();
    windowFeatures.y = windowRect.y();
    windowFeatures.height = windowRect.height();
    windowFeatures.width = windowRect.width();

    frame = createWindow(exec, frame, urlString, frameName, windowFeatures, 0);

    if (!frame)
        return jsUndefined();

    return toJS(exec, frame->domWindow()); // global object
}

static JSValue* setTimeoutOrInterval(ExecState* exec, JSValue* thisValue, const ArgList& args, bool timeout)
{
    JSDOMWindow* window = toJSDOMWindow(thisValue);
    if (!window)
        return throwError(exec, TypeError);
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    JSValue* v = args.at(exec, 0);
    int delay = args.at(exec, 1)->toInt32(exec);
    if (v->isString())
        return jsNumber(exec, window->installTimeout(static_cast<JSString*>(v)->value(), delay, timeout));
    CallData callData;
    if (v->getCallData(callData) == CallTypeNone)
        return jsUndefined();
    ArgList argsTail;
    args.getSlice(2, argsTail);
    return jsNumber(exec, window->installTimeout(exec, v, argsTail, delay, timeout));
}

JSValue* windowProtoFuncClearTimeoutOrInterval(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSDOMWindow* window = toJSDOMWindow(thisValue);
    if (!window)
        return throwError(exec, TypeError);
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    window->clearTimeout(args.at(exec, 0)->toInt32(exec));
    return jsUndefined();
}

JSValue* windowProtoFuncSetTimeout(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    return setTimeoutOrInterval(exec, thisValue, args, true);
}

JSValue* windowProtoFuncSetInterval(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    return setTimeoutOrInterval(exec, thisValue, args, false);
}

JSValue* windowProtoFuncAddEventListener(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSDOMWindow* window = toJSDOMWindow(thisValue);
    if (!window)
        return throwError(exec, TypeError);
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    if (RefPtr<JSEventListener> listener = window->findOrCreateJSEventListener(exec, args.at(exec, 1))) {
        if (Document* doc = frame->document())
            doc->addWindowEventListener(AtomicString(args.at(exec, 0)->toString(exec)), listener.release(), args.at(exec, 2)->toBoolean(exec));
    }

    return jsUndefined();
}

JSValue* windowProtoFuncRemoveEventListener(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSDOMWindow* window = toJSDOMWindow(thisValue);
    if (!window)
        return throwError(exec, TypeError);
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    if (JSEventListener* listener = window->findJSEventListener(args.at(exec, 1))) {
        if (Document* doc = frame->document())
            doc->removeWindowEventListener(AtomicString(args.at(exec, 0)->toString(exec)), listener, args.at(exec, 2)->toBoolean(exec));
    }

    return jsUndefined();
}

JSValue* windowProtoFuncShowModalDialog(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    JSDOMWindow* window = toJSDOMWindow(thisValue);
    if (!window)
        return throwError(exec, TypeError);
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    Frame* frame = window->impl()->frame();
    if (!frame)
        return jsUndefined();

    return showModalDialog(exec, frame, valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, 0)), args.at(exec, 1), valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, 2)));
}

JSValue* windowProtoFuncNotImplemented(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    if (!toJSDOMWindow(thisValue))
        return throwError(exec, TypeError);
    return jsUndefined();
}

void JSDOMWindowBase::setReturnValueSlot(JSValue** slot)
{
    d()->returnValueSlot = slot;
}

////////////////////// timeouts ////////////////////////

void JSDOMWindowBase::clearAllTimeouts()
{
    deleteAllValues(d()->timeouts);
    d()->timeouts.clear();
}

int JSDOMWindowBase::installTimeout(ScheduledAction* a, int t, bool singleShot)
{
    int timeoutId = ++lastUsedTimeoutId;

    // avoid wraparound going negative on us
    if (timeoutId <= 0)
        timeoutId = 1;

    int nestLevel = timerNestingLevel + 1;
    DOMWindowTimer* timer = new DOMWindowTimer(timeoutId, nestLevel, this, a);
    ASSERT(!d()->timeouts.get(timeoutId));
    d()->timeouts.set(timeoutId, timer);
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

int JSDOMWindowBase::installTimeout(ExecState* exec, JSValue* func, const ArgList& args, int t, bool singleShot)
{
    return installTimeout(new ScheduledAction(exec, func, args), t, singleShot);
}

void JSDOMWindowBase::pauseTimeouts(OwnPtr<PausedTimeouts>& result)
{
    size_t timeoutsCount = d()->timeouts.size();
    if (!timeoutsCount) {
        result.clear();
        return;
    }

    PausedTimeout* t = new PausedTimeout[timeoutsCount];
    result.set(new PausedTimeouts(t, timeoutsCount));

    JSDOMWindowBaseData::TimeoutsMap::iterator it = d()->timeouts.begin();
    for (size_t i = 0; i != timeoutsCount; ++i, ++it) {
        int timeoutId = it->first;
        DOMWindowTimer* timer = it->second;
        t[i].timeoutId = timeoutId;
        t[i].nestingLevel = timer->nestingLevel();
        t[i].nextFireInterval = timer->nextFireInterval();
        t[i].repeatInterval = timer->repeatInterval();
        t[i].action = timer->takeAction();
    }
    ASSERT(it == d()->timeouts.end());

    deleteAllValues(d()->timeouts);
    d()->timeouts.clear();
}

void JSDOMWindowBase::resumeTimeouts(OwnPtr<PausedTimeouts>& timeouts)
{
    if (!timeouts)
        return;
    size_t count = timeouts->numTimeouts();
    PausedTimeout* array = timeouts->takeTimeouts();
    for (size_t i = 0; i != count; ++i) {
        int timeoutId = array[i].timeoutId;
        DOMWindowTimer* timer = new DOMWindowTimer(timeoutId, array[i].nestingLevel, this, array[i].action);
        d()->timeouts.set(timeoutId, timer);
        timer->start(array[i].nextFireInterval, array[i].repeatInterval);
    }
    delete [] array;
    timeouts.clear();
}

void JSDOMWindowBase::clearTimeout(int timeoutId, bool delAction)
{
    // timeout IDs have to be positive, and 0 and -1 are unsafe to
    // even look up since they are the empty and deleted value
    // respectively
    if (timeoutId <= 0)
        return;

    delete d()->timeouts.take(timeoutId);
}

void JSDOMWindowBase::timerFired(DOMWindowTimer* timer)
{
    // Simple case for non-one-shot timers.
    if (timer->isActive()) {
        int timeoutId = timer->timeoutId();

        timer->action()->execute(shell());
        // The DOMWindowTimer object may have been deleted or replaced during execution,
        // so we re-fetch it.
        timer = d()->timeouts.get(timeoutId);
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
    d()->timeouts.remove(timer->timeoutId());
    delete timer;
    action->execute(shell());

    JSLock lock(false);
    delete action;
}

void JSDOMWindowBase::disconnectFrame()
{
    clearAllTimeouts();
}

JSDOMWindowBase::ListenersMap& JSDOMWindowBase::jsEventListeners()
{
    return d()->jsEventListeners;
}

JSDOMWindowBase::ListenersMap& JSDOMWindowBase::jsEventListenersAttachedToEventTargetNodes()
{
    return d()->jsEventListenersAttachedToEventTargetNodes;
}

JSDOMWindowBase::UnprotectedListenersMap& JSDOMWindowBase::jsUnprotectedEventListeners()
{
    return d()->jsUnprotectedEventListeners;
}

JSDOMWindowBase::UnprotectedListenersMap& JSDOMWindowBase::jsUnprotectedEventListenersAttachedToEventTargetNodes()
{
    return d()->jsUnprotectedEventListenersAttachedToEventTargetNodes;
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
    return frame->script()->windowShell();
}

JSDOMWindow* toJSDOMWindow(Frame* frame)
{
    if (!frame)
        return 0;
    return frame->script()->windowShell()->window();
}

JSDOMWindow* toJSDOMWindow(JSValue* value)
{
    if (!value->isObject())
        return 0;
    const ClassInfo* classInfo = static_cast<JSObject*>(value)->classInfo();
    if (classInfo == &JSDOMWindow::s_info)
        return static_cast<JSDOMWindow*>(value);
    if (classInfo == &JSDOMWindowShell::s_info)
        return static_cast<JSDOMWindowShell*>(value)->window();
    return 0;
}

} // namespace WebCore
