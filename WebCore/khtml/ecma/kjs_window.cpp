// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#include "kjs_window.h"

#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "JSMutationEvent.h"
#include "JSXMLHttpRequest.h"
#include "KWQEvent.h"
#include "KWQKConfigBase.h"
#include "KWQLogging.h"
#include "Screen.h"
#include "SelectionController.h"
#include "Shared.h"
#include "css_ruleimpl.h"
#include "css_stylesheetimpl.h"
#include "dom2_eventsimpl.h"
#include "dom2_rangeimpl.h"
#include "dom_elementimpl.h"
#include "dom_node.h"
#include "dom_position.h"
#include "PlatformString.h"
#include "domparser.h"
#include "html_documentimpl.h"
#include "htmlediting.h"
#include "kjs_css.h"
#include "kjs_events.h"
#include "kjs_html.h"
#include "kjs_navigator.h"
#include "kjs_proxy.h"
#include "kjs_range.h"
#include "kjs_traversal.h"
#include "render_canvas.h"
#include "xmlserializer.h"
#include <kconfig.h>
#include <kdebug.h>
#include <kjs/collector.h>
#include <klocale.h>
#include "FrameTreeNode.h"

#if KHTML_XSLT
#include "XSLTProcessor.h"
#endif

using namespace WebCore;
using namespace EventNames;

namespace KJS {

static int lastUsedTimeoutId;

class DOMWindowTimer : public TimerBase {
public:
    DOMWindowTimer(int timeoutId, WindowQObject* o, ScheduledAction* a)
        : m_timeoutId(timeoutId), m_object(o), m_action(a) { }
    virtual ~DOMWindowTimer() { delete m_action; }

    int timeoutId() const { return m_timeoutId; }
    ScheduledAction* action() const { return m_action; }
    ScheduledAction* takeAction() { ScheduledAction* a = m_action; m_action = 0; return a; }

private:
    virtual void fired();

    int m_timeoutId;
    WindowQObject* m_object;
    ScheduledAction* m_action;
};

class PausedTimeout {
public:
    int timeoutId;
    double nextFireInterval;
    double repeatInterval;
    ScheduledAction *action;
};

////////////////////// History Object ////////////////////////

  class History : public JSObject {
    friend class HistoryFunc;
  public:
    History(ExecState *exec, Frame *p)
      : JSObject(exec->lexicalInterpreter()->builtinObjectPrototype()), m_frame(p) { }
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Back, Forward, Go, Length };
    virtual UString toString(ExecState *exec) const;
  private:
    QGuardedPtr<Frame> m_frame;
  };

  class FrameArray : public JSObject {
  public:
    FrameArray(ExecState *exec, Frame *p)
      : JSObject(exec->lexicalInterpreter()->builtinObjectPrototype()), m_frame(p) { }
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token);
    virtual UString toString(ExecState *exec) const;
    enum { Length, Location };
  private:
    static JSValue *indexGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);
    static JSValue *nameGetter(ExecState *, JSObject *, const Identifier&, const PropertySlot&);

    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;

    QGuardedPtr<Frame> m_frame;
  };

}

#include "kjs_window.lut.h"

namespace KJS {

////////////////////// Screen Object ////////////////////////

// table for screen object
/*
@begin ScreenTable 7
  height        Screen::Height          DontEnum|ReadOnly
  width         Screen::Width           DontEnum|ReadOnly
  colorDepth    Screen::ColorDepth      DontEnum|ReadOnly
  pixelDepth    Screen::PixelDepth      DontEnum|ReadOnly
  availLeft     Screen::AvailLeft       DontEnum|ReadOnly
  availTop      Screen::AvailTop        DontEnum|ReadOnly
  availHeight   Screen::AvailHeight     DontEnum|ReadOnly
  availWidth    Screen::AvailWidth      DontEnum|ReadOnly
@end
*/

const ClassInfo Screen::info = { "Screen", 0, &ScreenTable, 0 };

// We set the object prototype so that toString is implemented
Screen::Screen(ExecState* exec, Frame* f)
  : JSObject(exec->lexicalInterpreter()->builtinObjectPrototype()), m_frame(f)
{
}

bool Screen::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<Screen, JSObject>(exec, &ScreenTable, this, propertyName, slot);
}

JSValue* Screen::getValueProperty(ExecState*, int token) const
{
  QWidget* widget = m_frame ? m_frame->view() : 0;

  switch (token) {
  case Height:
    return jsNumber(screenRect(widget).height());
  case Width:
    return jsNumber(screenRect(widget).width());
  case ColorDepth:
  case PixelDepth:
    return jsNumber(screenDepth(widget));
  case AvailLeft:
    return jsNumber(usableScreenRect(widget).x() - screenRect(widget).x());
  case AvailTop:
    return jsNumber(usableScreenRect(widget).y() - screenRect(widget).y());
  case AvailHeight:
    return jsNumber(usableScreenRect(widget).height());
  case AvailWidth:
    return jsNumber(usableScreenRect(widget).width());
  default:
    kdWarning() << "Screen::getValueProperty unhandled token " << token << endl;
    return jsUndefined();
  }
}

////////////////////// Window Object ////////////////////////

const ClassInfo Window::info = { "Window", 0, &WindowTable, 0 };

/*
@begin WindowTable 103
  closed        Window::Closed          DontDelete|ReadOnly
  crypto        Window::Crypto          DontDelete|ReadOnly
  defaultStatus Window::DefaultStatus   DontDelete
  defaultstatus Window::DefaultStatus   DontDelete
  status        Window::Status          DontDelete
  document      Window::Document        DontDelete|ReadOnly
  Node          Window::Node            DontDelete
  Event         Window::EventCtor       DontDelete
  Range         Window::Range           DontDelete
  NodeFilter    Window::NodeFilter      DontDelete
  DOMException  Window::DOMException    DontDelete
  CSSRule       Window::CSSRule         DontDelete
  MutationEvent Window::MutationEventCtor   DontDelete
  frames        Window::Frames          DontDelete|ReadOnly
  history       Window::_History        DontDelete|ReadOnly
  event         Window::Event           DontDelete
  innerHeight   Window::InnerHeight     DontDelete|ReadOnly
  innerWidth    Window::InnerWidth      DontDelete|ReadOnly
  length        Window::Length          DontDelete|ReadOnly
  location      Window::_Location       DontDelete
  locationbar   Window::Locationbar     DontDelete
  name          Window::Name            DontDelete
  navigator     Window::_Navigator      DontDelete|ReadOnly
  clientInformation     Window::ClientInformation       DontDelete|ReadOnly
  menubar       Window::Menubar         DontDelete|ReadOnly
  offscreenBuffering    Window::OffscreenBuffering      DontDelete|ReadOnly
  opener        Window::Opener          DontDelete|ReadOnly
  outerHeight   Window::OuterHeight     DontDelete|ReadOnly
  outerWidth    Window::OuterWidth      DontDelete|ReadOnly
  pageXOffset   Window::PageXOffset     DontDelete|ReadOnly
  pageYOffset   Window::PageYOffset     DontDelete|ReadOnly
  parent        Window::Parent          DontDelete|ReadOnly
  personalbar   Window::Personalbar     DontDelete|ReadOnly
  screenX       Window::ScreenX         DontDelete|ReadOnly
  screenY       Window::ScreenY         DontDelete|ReadOnly
  screenLeft    Window::ScreenLeft      DontDelete|ReadOnly
  screenTop     Window::ScreenTop       DontDelete|ReadOnly
  scrollbars    Window::Scrollbars      DontDelete|ReadOnly
  statusbar     Window::Statusbar       DontDelete|ReadOnly
  toolbar       Window::Toolbar         DontDelete|ReadOnly
  scroll        Window::Scroll          DontDelete|Function 2
  scrollBy      Window::ScrollBy        DontDelete|Function 2
  scrollTo      Window::ScrollTo        DontDelete|Function 2
  scrollX       Window::ScrollX         DontDelete|ReadOnly
  scrollY       Window::ScrollY         DontDelete|ReadOnly
  moveBy        Window::MoveBy          DontDelete|Function 2
  moveTo        Window::MoveTo          DontDelete|Function 2
  resizeBy      Window::ResizeBy        DontDelete|Function 2
  resizeTo      Window::ResizeTo        DontDelete|Function 2
  self          Window::Self            DontDelete|ReadOnly
  window        Window::_Window         DontDelete|ReadOnly
  top           Window::Top             DontDelete|ReadOnly
  screen        Window::_Screen         DontDelete|ReadOnly
  Image         Window::Image           DontDelete|ReadOnly
  Option        Window::Option          DontDelete|ReadOnly
  XMLHttpRequest        Window::XMLHttpRequest  DontDelete|ReadOnly
  XMLSerializer Window::XMLSerializer   DontDelete|ReadOnly
  DOMParser     Window::DOMParser       DontDelete|ReadOnly
  XSLTProcessor Window::XSLTProcessor   DontDelete|ReadOnly
  alert         Window::Alert           DontDelete|Function 1
  confirm       Window::Confirm         DontDelete|Function 1
  prompt        Window::Prompt          DontDelete|Function 2
  open          Window::Open            DontDelete|Function 3
  print         Window::Print           DontDelete|Function 2
  setTimeout    Window::SetTimeout      DontDelete|Function 2
  clearTimeout  Window::ClearTimeout    DontDelete|Function 1
  focus         Window::Focus           DontDelete|Function 0
  getSelection  Window::GetSelection    DontDelete|Function 0
  blur          Window::Blur            DontDelete|Function 0
  close         Window::Close           DontDelete|Function 0
  setInterval   Window::SetInterval     DontDelete|Function 2
  clearInterval Window::ClearInterval   DontDelete|Function 1
  captureEvents Window::CaptureEvents   DontDelete|Function 0
  releaseEvents Window::ReleaseEvents   DontDelete|Function 0
# Warning, when adding a function to this object you need to add a case in Window::get
  addEventListener      Window::AddEventListener        DontDelete|Function 3
  removeEventListener   Window::RemoveEventListener     DontDelete|Function 3
  onabort       Window::Onabort         DontDelete
  onblur        Window::Onblur          DontDelete
  onchange      Window::Onchange        DontDelete
  onclick       Window::Onclick         DontDelete
  ondblclick    Window::Ondblclick      DontDelete
  ondragdrop    Window::Ondragdrop      DontDelete
  onerror       Window::Onerror         DontDelete
  onfocus       Window::Onfocus         DontDelete
  onkeydown     Window::Onkeydown       DontDelete
  onkeypress    Window::Onkeypress      DontDelete
  onkeyup       Window::Onkeyup         DontDelete
  onload        Window::Onload          DontDelete
  onmousedown   Window::Onmousedown     DontDelete
  onmousemove   Window::Onmousemove     DontDelete
  onmouseout    Window::Onmouseout      DontDelete
  onmouseover   Window::Onmouseover     DontDelete
  onmouseup     Window::Onmouseup       DontDelete
  onmousewheel  Window::OnWindowMouseWheel      DontDelete
  onmove        Window::Onmove          DontDelete
  onreset       Window::Onreset         DontDelete
  onresize      Window::Onresize        DontDelete
  onscroll      Window::Onscroll        DontDelete
  onsearch      Window::Onsearch        DontDelete
  onselect      Window::Onselect        DontDelete
  onsubmit      Window::Onsubmit        DontDelete
  onunload      Window::Onunload        DontDelete
  onbeforeunload Window::Onbeforeunload DontDelete
  frameElement  Window::FrameElement    DontDelete|ReadOnly
  showModalDialog Window::ShowModalDialog    DontDelete|Function 1
@end
*/
KJS_IMPLEMENT_PROTOFUNC(WindowFunc)

Window::Window(Frame *p)
  : JSObject(/*no proto*/)
  , m_frame(p)
  , screen(0)
  , history(0)
  , frames(0)
  , loc(0)
  , m_selection(0)
  , m_locationbar(0)
  , m_menubar(0)
  , m_personalbar(0)
  , m_scrollbars(0)
  , m_statusbar(0)
  , m_toolbar(0)
  , m_evt(0)
  , m_returnValueSlot(0)
{
  winq = new WindowQObject(this);
}

Window::~Window()
{
    // Clear any backpointers to the window

    UnprotectedListenersMap::iterator i1 = jsUnprotectedEventListeners.begin();
    UnprotectedListenersMap::iterator e1 = jsUnprotectedEventListeners.end();
    for (; i1 != e1; ++i1)
        i1->second->clearWindowObj();

    ListenersMap::iterator i2 = jsEventListeners.begin();
    ListenersMap::iterator e2 = jsEventListeners.end();
    for (; i2 != e2; ++i2)
        i2->second->clearWindowObj();
    
    delete winq;
}

ScriptInterpreter *Window::interpreter() const
{
    return m_frame->jScript()->interpreter();
}

Window *Window::retrieveWindow(Frame *f)
{
    JSObject *o = retrieve(f)->getObject();

    ASSERT(o || !f->jScriptEnabled());
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
    if (KJSProxyImpl *proxy = p->jScript())
        return proxy->interpreter()->globalObject(); // the Global object is the "window"
  
    return jsUndefined(); // This can happen with JS disabled on the domain of that window
}

Location *Window::location() const
{
  if (!loc)
    const_cast<Window*>(this)->loc = new Location(m_frame);
  return loc;
}

Selection *Window::selection() const
{
  if (!m_selection)
    const_cast<Window*>(this)->m_selection = new Selection(m_frame);
  return m_selection;
}

BarInfo *Window::locationbar(ExecState *exec) const
{
  if (!m_locationbar)
    const_cast<Window*>(this)->m_locationbar = new BarInfo(exec, m_frame, BarInfo::Locationbar);
  return m_locationbar;
}

BarInfo *Window::menubar(ExecState *exec) const
{
  if (!m_menubar)
    const_cast<Window*>(this)->m_menubar = new BarInfo(exec, m_frame, BarInfo::Menubar);
  return m_menubar;
}

BarInfo *Window::personalbar(ExecState *exec) const
{
  if (!m_personalbar)
    const_cast<Window*>(this)->m_personalbar = new BarInfo(exec, m_frame, BarInfo::Personalbar);
  return m_personalbar;
}

BarInfo *Window::statusbar(ExecState *exec) const
{
  if (!m_statusbar)
    const_cast<Window*>(this)->m_statusbar = new BarInfo(exec, m_frame, BarInfo::Statusbar);
  return m_statusbar;
}

BarInfo *Window::toolbar(ExecState *exec) const
{
  if (!m_toolbar)
    const_cast<Window*>(this)->m_toolbar = new BarInfo(exec, m_frame, BarInfo::Toolbar);
  return m_toolbar;
}

BarInfo *Window::scrollbars(ExecState *exec) const
{
  if (!m_scrollbars)
    const_cast<Window*>(this)->m_scrollbars = new BarInfo(exec, m_frame, BarInfo::Scrollbars);
  return m_scrollbars;
}

// reference our special objects during garbage collection
void Window::mark()
{
  JSObject::mark();
  if (screen && !screen->marked())
    screen->mark();
  if (history && !history->marked())
    history->mark();
  if (frames && !frames->marked())
    frames->mark();
  //kdDebug(6070) << "Window::mark " << this << " marking loc=" << loc << endl;
  if (loc && !loc->marked())
    loc->mark();
  if (m_selection && !m_selection->marked())
    m_selection->mark();
  if (m_locationbar && !m_locationbar->marked())
    m_locationbar->mark();
  if (m_menubar && !m_menubar->marked())
    m_menubar->mark();
  if (m_personalbar && !m_personalbar->marked())
    m_personalbar->mark();
  if (m_scrollbars && !m_scrollbars->marked())
    m_scrollbars->mark();
  if (m_statusbar && !m_statusbar->marked())
    m_statusbar->mark();
  if (m_toolbar && !m_toolbar->marked())
    m_toolbar->mark();
}

UString Window::toString(ExecState *) const
{
  return "[object Window]";
}

static bool allowPopUp(ExecState *exec, Window *window)
{
    return window->frame()
        && (window->frame()->settings()->JavaScriptCanOpenWindowsAutomatically()
            || static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture());
}

static HashMap<DOMString, DOMString> parseModalDialogFeatures(ExecState *exec, JSValue *featuresArg)
{
    HashMap<DOMString, DOMString> map;

    QStringList features = QStringList::split(';', featuresArg->toString(exec).qstring());
    QStringList::ConstIterator end = features.end();
    for (QStringList::ConstIterator it = features.begin(); it != end; ++it) {
        QString s = *it;
        int pos = s.find('=');
        int colonPos = s.find(':');
        if (pos >= 0 && colonPos >= 0)
            continue; // ignore any strings that have both = and :
        if (pos < 0)
            pos = colonPos;
        if (pos < 0) {
            // null string for value means key without value
            map.set(s.stripWhiteSpace().lower(), DOMString());
        } else {
            QString key = s.left(pos).stripWhiteSpace().lower();
            QString val = s.mid(pos + 1).stripWhiteSpace().lower();
            int spacePos = val.find(' ');
            if (spacePos != -1)
                val = val.left(spacePos);
            map.set(key, val);
        }
    }

    return map;
}

static bool boolFeature(const HashMap<DOMString, DOMString>& features, const char* key, bool defaultValue = false)
{
    HashMap<DOMString, DOMString>::const_iterator it = features.find(key);
    if (it == features.end())
        return defaultValue;
    const DOMString& value = it->second;
    return value.isNull() || value == "1" || value == "yes" || value == "on";
}

static int intFeature(const HashMap<DOMString, DOMString> &features, const char *key, int min, int max, int defaultValue)
{
    HashMap<DOMString, DOMString>::const_iterator it = features.find(key);
    if (it == features.end())
        return defaultValue;
    QString value = it->second.qstring();
    // FIXME: Can't distinguish "0q" from string with no digits in it -- both return d == 0 and ok == false.
    // Would be good to tell them apart somehow since string with no digits should be default value and
    // "0q" should be minimum value.
    bool ok;
    double d = value.toDouble(&ok);
    if ((d == 0 && !ok) 
#if !WIN32
        || isnan(d)
#endif
         )
        return defaultValue;
    if (d < min || max <= min)
        return min;
    if (d > max)
        return max;
    return static_cast<int>(d);
}

static Frame *createNewWindow(ExecState *exec, Window *openerWindow, const QString &URL,
    const QString &frameName, const WindowArgs &windowArgs, JSValue *dialogArgs)
{
    Frame *openerPart = openerWindow->frame();
    Frame *activePart = Window::retrieveActive(exec)->frame();

    URLArgs uargs;

    uargs.frameName = frameName;
    if (activePart)
        uargs.metaData().set("referrer", activePart->referrer());
    uargs.serviceType = "text/html";

    // FIXME: It's much better for client API if a new window starts with a URL, here where we
    // know what URL we are going to open. Unfortunately, this code passes the empty string
    // for the URL, but there's a reason for that. Before loading we have to set up the opener,
    // openedByJS, and dialogArguments values. Also, to decide whether to use the URL we currently
    // do an isSafeScript call using the window we create, which can't be done before creating it.
    // We'd have to resolve all those issues to pass the URL instead of "".

    ObjectContents *newPart = 0;
    openerPart->browserExtension()->createNewWindow("", uargs, windowArgs, newPart);

    if (!newPart || !newPart->inherits("Frame"))
        return 0;

    Frame *newFrame = static_cast<Frame *>(newPart);
    Window *newWindow = Window::retrieveWindow(newFrame);

    newFrame->setOpener(openerPart);
    newFrame->setOpenedByJS(true);
    if (dialogArgs)
        newWindow->putDirect("dialogArguments", dialogArgs);

    DocumentImpl *activeDoc = activePart ? activePart->document() : 0;
    if (!URL.isEmpty() && activeDoc) {
        QString completedURL = activeDoc->completeURL(URL);
        if (!completedURL.startsWith("javascript:", false) || newWindow->isSafeScript(exec)) {
            bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
            newFrame->changeLocation(completedURL, activePart->referrer(), false, userGesture);
        }
    }

    return newFrame;
}

static bool canShowModalDialog(const Window *window)
{
    Frame *frame = window->frame();
    return frame && static_cast<BrowserExtension *>(frame->browserExtension())->canRunModal();
}

static bool canShowModalDialogNow(const Window *window)
{
    Frame *frame = window->frame();
    return frame && static_cast<BrowserExtension *>(frame->browserExtension())->canRunModalNow();
}

static JSValue *showModalDialog(ExecState *exec, Window *openerWindow, const List &args)
{
    UString URL = args[0]->toString(exec);

    if (!canShowModalDialogNow(openerWindow) || !allowPopUp(exec, openerWindow))
        return jsUndefined();
    
    const HashMap<DOMString, DOMString> features = parseModalDialogFeatures(exec, args[2]);

    bool trusted = false;

    WindowArgs wargs;

    // The following features from Microsoft's documentation are not implemented:
    // - default font settings
    // - width, height, left, and top specified in units other than "px"
    // - edge (sunken or raised, default is raised)
    // - dialogHide: trusted && boolFeature(features, "dialoghide"), makes dialog hide when you print
    // - help: boolFeature(features, "help", true), makes help icon appear in dialog (what does it do on Windows?)
    // - unadorned: trusted && boolFeature(features, "unadorned");

    IntRect screenRect = usableScreenRect(openerWindow->frame()->view());

    wargs.width = intFeature(features, "dialogwidth", 100, screenRect.width(), 620); // default here came from frame size of dialog in MacIE
    wargs.widthSet = true;
    wargs.height = intFeature(features, "dialogheight", 100, screenRect.height(), 450); // default here came from frame size of dialog in MacIE
    wargs.heightSet = true;

    wargs.x = intFeature(features, "dialogleft", screenRect.x(), screenRect.right() - wargs.width, -1);
    wargs.xSet = wargs.x > 0;
    wargs.y = intFeature(features, "dialogtop", screenRect.y(), screenRect.bottom() - wargs.height, -1);
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
    wargs.scrollBarsVisible = boolFeature(features, "scroll", true);
    wargs.statusBarVisible = boolFeature(features, "status", !trusted);
    wargs.menuBarVisible = false;
    wargs.toolBarVisible = false;
    wargs.locationBarVisible = false;
    wargs.fullscreen = false;
    
    Frame *dialogPart = createNewWindow(exec, openerWindow, URL.qstring(), "", wargs, args[1]);
    if (!dialogPart)
        return jsUndefined();

    Window *dialogWindow = Window::retrieveWindow(dialogPart);
    JSValue *returnValue = jsUndefined();
    dialogWindow->setReturnValueSlot(&returnValue);
    static_cast<BrowserExtension *>(dialogPart->browserExtension())->runModal();
    dialogWindow->setReturnValueSlot(NULL);
    return returnValue;
}

JSValue *Window::getValueProperty(ExecState *exec, int token) const
{
   ASSERT(token == Closed || m_frame);

   switch (token) {
   case Closed:
      return jsBoolean(m_frame.isNull());
   case Crypto:
      return jsUndefined(); // ###
   case DefaultStatus:
      return jsString(UString(m_frame->jsDefaultStatusBarText()));
   case Status:
      return jsString(UString(m_frame->jsStatusBarText()));
    case Node:
      return getNodeConstructor(exec);
    case Range:
      return getRangeConstructor(exec);
    case NodeFilter:
      return getNodeFilterConstructor(exec);
    case DOMException:
      return getDOMExceptionConstructor(exec);
    case CSSRule:
      return getCSSRuleConstructor(exec);
    case EventCtor:
      return getEventConstructor(exec);
    case MutationEventCtor:
      return JSMutationEvent::getConstructor(exec);
    case Frames:
      if (!frames)
        frames = new FrameArray(exec, m_frame);
      return frames;
    case _History:
      if (!history)
        history = new History(exec, m_frame);
      return history;
    case Event:
      if (!m_evt)
        return jsUndefined();
      return getDOMEvent(exec, m_evt);
    case InnerHeight:
      if (!m_frame->view())
        return jsUndefined();
      return jsNumber(m_frame->view()->visibleHeight());
    case InnerWidth:
      if (!m_frame->view())
        return jsUndefined();
      return jsNumber(m_frame->view()->visibleWidth());
    case Length:
      return jsNumber(m_frame->frames().count());
    case _Location:
      return location();
    case Name:
      return jsString(m_frame->treeNode()->name());
    case _Navigator:
    case ClientInformation: {
      // Store the navigator in the object so we get the same one each time.
      Navigator *n = new Navigator(exec, m_frame);
      // FIXME: this will make the "navigator" object accessible from windows that fail
      // the security check the first time, but not subsequent times, seems weird.
      const_cast<Window *>(this)->putDirect("navigator", n, DontDelete|ReadOnly);
      const_cast<Window *>(this)->putDirect("clientInformation", n, DontDelete|ReadOnly);
      return n;
    }
    case Locationbar:
      return locationbar(exec);
    case Menubar:
      return menubar(exec);
    case OffscreenBuffering:
      return jsBoolean(true);
    case Opener:
      if (m_frame->opener())
        return retrieve(m_frame->opener());
      else
        return jsNull();
    case OuterHeight:
    case OuterWidth:
    {
      if (m_frame->view()) {
        IntRect frame = m_frame->view()->topLevelWidget()->frameGeometry();
        return jsNumber(token == OuterHeight ? frame.height() : frame.width());
      } else
        return jsNumber(0);
    }
    case PageXOffset:
      if (!m_frame->view())
        return jsUndefined();
      updateLayout();
      return jsNumber(m_frame->view()->contentsX());
    case PageYOffset:
      if (!m_frame->view())
        return jsUndefined();
      updateLayout();
      return jsNumber(m_frame->view()->contentsY());
    case Parent:
      return retrieve(m_frame->treeNode()->parent() ? m_frame->treeNode()->parent() : (Frame*)m_frame);
    case Personalbar:
      return personalbar(exec);
    case ScreenLeft:
    case ScreenX:
      if (!m_frame->view())
        return jsUndefined();
      // We want to use frameGeometry here instead of mapToGlobal because it goes through
      // the windowFrame method of the WebKit's UI delegate. Also we don't want to try
      // to do anything relative to the screen the window is on.
      return jsNumber(m_frame->view()->topLevelWidget()->frameGeometry().x());
    case ScreenTop:
    case ScreenY:
      if (!m_frame->view())
        return jsUndefined();
      // See comment above in ScreenX.
      return jsNumber(m_frame->view()->topLevelWidget()->frameGeometry().y());
    case ScrollX:
      if (!m_frame->view())
        return jsUndefined();
      updateLayout();
      return jsNumber(m_frame->view()->contentsX());
    case ScrollY:
      if (!m_frame->view())
        return jsUndefined();
      updateLayout();
      return jsNumber(m_frame->view()->contentsY());
    case Scrollbars:
      return scrollbars(exec);
    case Statusbar:
      return statusbar(exec);
    case Toolbar:
      return toolbar(exec);
    case Self:
    case _Window:
      return retrieve(m_frame);
    case Top: {
      Frame *p = m_frame;
      while (p->treeNode()->parent())
        p = p->treeNode()->parent();
      return retrieve(p);
    }
    case _Screen:
      if (!screen)
        screen = new Screen(exec, m_frame);
      return screen;
    case Image:
      // FIXME: this property (and the few below) probably shouldn't create a new object every
      // time
      return new ImageConstructorImp(exec, m_frame->document());
    case Option:
      return new OptionConstructorImp(exec, m_frame->document());
    case XMLHttpRequest:
      return new JSXMLHttpRequestConstructorImp(exec, m_frame->document());
    case XMLSerializer:
      return new XMLSerializerConstructorImp(exec);
    case DOMParser:
      return new DOMParserConstructorImp(exec, m_frame->document());
#ifdef KHTML_XSLT
    case XSLTProcessor:
      return new XSLTProcessorConstructorImp(exec);
#else
    case XSLTProcessor:
      return jsUndefined();
#endif
    case FrameElement:
      if (DocumentImpl *doc = m_frame->document())
        if (ElementImpl *fe = doc->ownerElement())
          if (checkNodeSecurity(exec, fe)) {
            return getDOMNode(exec, fe);
          }
      return jsUndefined();
   }

   if (!isSafeScript(exec))
     return jsUndefined();

   switch (token) {
   case Document:
     if (!m_frame->document()) {
       m_frame->createEmptyDocument();
       m_frame->begin();
       m_frame->write("<HTML><BODY>");
       m_frame->end();
     }
     return getDOMNode(exec, m_frame->document());
   case Onabort:
     return getListener(exec, abortEvent);
   case Onblur:
     return getListener(exec, blurEvent);
   case Onchange:
     return getListener(exec, changeEvent);
   case Onclick:
     return getListener(exec, clickEvent);
   case Ondblclick:
     return getListener(exec, khtmlDblclickEvent);
   case Ondragdrop:
     return getListener(exec, khtmlDragdropEvent);
   case Onerror:
     return getListener(exec, khtmlErrorEvent);
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
   case Onmove:
     return getListener(exec, khtmlMoveEvent);
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

JSValue *Window::childFrameGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  Window *thisObj = static_cast<Window *>(slot.slotBase());
  return retrieve(thisObj->m_frame->childFrameNamed(propertyName.qstring()));
}

JSValue *Window::namedFrameGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  Window *thisObj = static_cast<Window *>(slot.slotBase());
  return retrieve(thisObj->m_frame->findFrame(propertyName.qstring()));
}

JSValue *Window::indexGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  Window *thisObj = static_cast<Window *>(slot.slotBase());
  
  QPtrList<ObjectContents> frames = thisObj->m_frame->frames();
  ObjectContents* frame = frames.at(slot.index());
  ASSERT(frame && frame->inherits("Frame"));

  return retrieve(static_cast<Frame*>(frame));
}

JSValue *Window::namedItemGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  Window *thisObj = static_cast<Window *>(slot.slotBase());
  DocumentImpl *doc = thisObj->m_frame->document();
  ASSERT(thisObj->isSafeScript(exec) && doc && doc->isHTMLDocument());

  DOMString name = propertyName.domString();
  RefPtr<DOM::HTMLCollectionImpl> collection = doc->windowNamedItems(name);
  if (collection->length() == 1)
    return getDOMNode(exec, collection->firstItem());
  else 
    return getHTMLCollection(exec, collection.get());
}

bool Window::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  // we don't want any properties other than "closed" on a closed window
  if (m_frame.isNull()) {
    if (propertyName == "closed") {
      slot.setStaticEntry(this, Lookup::findEntry(&WindowTable, "closed"), staticValueGetter<Window>);
      return true;
    }
    if (propertyName == "close") {
      const HashEntry* entry = Lookup::findEntry(&WindowTable, propertyName);
      slot.setStaticEntry(this, entry, staticFunctionGetter<WindowFunc>);
      return true;
    }

    slot.setUndefined(this);
    return true;
  }

  // Look for overrides first
  JSValue **val = getDirectLocation(propertyName);
  if (val) {
    if (isSafeScript(exec))
      slot.setValueSlot(this, val);
    else
      slot.setUndefined(this);
    return true;
  }

  // Check for child frames by name before built-in properties to
  // match Mozilla. This does not match IE, but some sites end up
  // naming frames things that conflict with window properties that
  // are in Moz but not IE. Since we have some of these, we have to do
  // it the Moz way.
  Frame *childFrame = m_frame->childFrameNamed(propertyName.qstring());
  if (childFrame) {
    slot.setCustom(this, childFrameGetter);
    return true;
  }

  const HashEntry* entry = Lookup::findEntry(&WindowTable, propertyName);
  if (entry) {
    if (entry->attr & Function) {
      switch (entry->value) {
      case Focus:
      case Blur:
      case Close:
        slot.setStaticEntry(this, entry, staticFunctionGetter<WindowFunc>);
        break;
      case ShowModalDialog:
        if (!canShowModalDialog(this))
          return false;
        // fall through
      default:
        if (isSafeScript(exec))
          slot.setStaticEntry(this, entry, staticFunctionGetter<WindowFunc>);
        else
          slot.setUndefined(this);
      } 
    } else
      slot.setStaticEntry(this, entry, staticValueGetter<Window>);
    return true;
  }

  Frame *kp = m_frame->findFrame(propertyName.qstring());
  if (kp) {
    slot.setCustom(this, namedFrameGetter);
    return true;
  }

  // allow window[1] or parent[1] etc. (#56983)
  bool ok;
  unsigned int i = propertyName.toArrayIndex(&ok);
  if (ok) {
    QPtrList<ObjectContents> frames = m_frame->frames();
    unsigned int len = frames.count();
    if (i < len) {
      ObjectContents* frame = frames.at(i);
      if (frame && frame->inherits("Frame")) {
        slot.setCustomIndex(this, i, indexGetter);
        return true;
      }
    }
  }

  // allow shortcuts like 'Image1' instead of document.images.Image1
  DocumentImpl *doc = m_frame->document();
  if (isSafeScript(exec) && doc && doc->isHTMLDocument()) {
    AtomicString name = propertyName.domString().impl();
    if (static_cast<HTMLDocumentImpl *>(doc)->hasNamedItem(name) || doc->getElementById(name)) {
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

void Window::put(ExecState* exec, const Identifier &propertyName, JSValue *value, int attr)
{
  // Called by an internal KJS call (e.g. InterpreterImp's constructor) ?
  // If yes, save time and jump directly to JSObject.
  if ( (attr != None && attr != DontDelete)
       // Same thing if we have a local override (e.g. "var location")
       || ( JSObject::getDirect(propertyName) && isSafeScript(exec)) )
  {
    JSObject::put( exec, propertyName, value, attr );
    return;
  }

  const HashEntry* entry = Lookup::findEntry(&WindowTable, propertyName);
  if (entry)
  {
    switch( entry->value ) {
    case Status:
      m_frame->setJSStatusBarText(value->toString(exec).qstring());
      return;
    case DefaultStatus:
      m_frame->setJSDefaultStatusBarText(value->toString(exec).qstring());
      return;
    case _Location: {
      Frame* p = Window::retrieveActive(exec)->m_frame;
      if (p) {
        QString dstUrl = p->document()->completeURL(value->toString(exec).qstring());
        if (!dstUrl.startsWith("javascript:", false) || isSafeScript(exec))
        {
          bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
          // We want a new history item if this JS was called via a user gesture
          m_frame->scheduleLocationChange(dstUrl, p->referrer(), !userGesture, userGesture);
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
        setListener(exec,khtmlDblclickEvent,value);
      return;
    case Ondragdrop:
      if (isSafeScript(exec))
        setListener(exec,khtmlDragdropEvent,value);
      return;
    case Onerror:
      if (isSafeScript(exec))
        setListener(exec,khtmlErrorEvent,value);
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
    case Onmove:
      if (isSafeScript(exec))
        setListener(exec,khtmlMoveEvent,value);
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
    case Name:
      if (isSafeScript(exec))
        m_frame->treeNode()->setName(value->toString(exec).qstring());
      return;
    default:
      break;
    }
  }
  if (isSafeScript(exec)) {
    JSObject::put(exec, propertyName, value, attr);
  }
}

bool Window::toBoolean(ExecState *) const
{
  return !m_frame.isNull();
}

void Window::scheduleClose()
{
  kdDebug(6070) << "Window::scheduleClose window.close() " << m_frame << endl;
  ASSERT(winq);
  m_frame->scheduleClose();
}

static bool shouldLoadAsEmptyDocument(const KURL &url)
{
  return url.protocol().lower() == "about" || url.isEmpty();
}

bool Window::isSafeScript(const ScriptInterpreter *origin, const ScriptInterpreter *target)
{
    if (origin == target)
        return true;
        
    Frame *originPart = origin->frame();
    Frame *targetPart = target->frame();

    // JS may be attempting to access the "window" object, which should be valid,
    // even if the document hasn't been constructed yet.  If the document doesn't
    // exist yet allow JS to access the window object.
    if (!targetPart->document())
        return true;

    DOM::DocumentImpl *originDocument = originPart->document();
    DOM::DocumentImpl *targetDocument = targetPart->document();

    if (!targetDocument) {
        return false;
    }

    DOM::DOMString targetDomain = targetDocument->domain();

    // Always allow local pages to execute any JS.
    if (targetDomain.isNull())
        return true;

    DOM::DOMString originDomain = originDocument->domain();

    // if this document is being initially loaded as empty by its parent
    // or opener, allow access from any document in the same domain as
    // the parent or opener.
    if (shouldLoadAsEmptyDocument(targetPart->url())) {
        Frame *ancestorPart = targetPart->opener() ? targetPart->opener() : targetPart->treeNode()->parent();
        while (ancestorPart && shouldLoadAsEmptyDocument(ancestorPart->url())) {
            ancestorPart = ancestorPart->treeNode()->parent();
        }

        if (ancestorPart)
            originDomain = ancestorPart->document()->domain();
    }

    if ( targetDomain == originDomain )
        return true;

    if (Interpreter::shouldPrintExceptions()) {
        printf("Unsafe JavaScript attempt to access frame with URL %s from frame with URL %s. Domains must match.\n", 
             targetDocument->URL().latin1(), originDocument->URL().latin1());
    }
    QString message;
    message.sprintf("Unsafe JavaScript attempt to access frame with URL %s from frame with URL %s. Domains must match.\n", 
                  targetDocument->URL().latin1(), originDocument->URL().latin1());
    targetPart->addMessageToConsole(DOMString(message), 1, DOMString()); //fixme: provide a real line number and sourceurl

    return false;
}

bool Window::isSafeScript(ExecState *exec) const
{
  if (m_frame.isNull()) { // frame deleted ? can't grant access
    kdDebug(6070) << "Window::isSafeScript: accessing deleted frame !" << endl;
    return false;
  }
  Frame *activePart = static_cast<ScriptInterpreter *>( exec->dynamicInterpreter() )->frame();
  if (!activePart) {
    kdDebug(6070) << "Window::isSafeScript: current interpreter's frame is 0L!" << endl;
    return false;
  }
  if ( activePart == m_frame ) // Not calling from another frame, no problem.
    return true;

  // JS may be attempting to access the "window" object, which should be valid,
  // even if the document hasn't been constructed yet.  If the document doesn't
  // exist yet allow JS to access the window object.
  if (!m_frame->document())
      return true;

  DOM::DocumentImpl* thisDocument = m_frame->document();
  DOM::DocumentImpl* actDocument = activePart->document();

  DOM::DOMString actDomain;

  if (!actDocument)
    actDomain = activePart->url().host();
  else
    actDomain = actDocument->domain();
  
  // FIXME: this really should be explicitly checking for the "file:" protocol instead
  // Always allow local pages to execute any JS.
  if (actDomain.isNull())
    return true;
  
  DOM::DOMString thisDomain = thisDocument->domain();

  // if this document is being initially loaded as empty by its parent
  // or opener, allow access from any document in the same domain as
  // the parent or opener.
  if (shouldLoadAsEmptyDocument(m_frame->url())) {
    Frame *ancestorPart = m_frame->opener() ? m_frame->opener() : m_frame->treeNode()->parent();
    while (ancestorPart && shouldLoadAsEmptyDocument(ancestorPart->url())) {
      ancestorPart = ancestorPart->treeNode()->parent();
    }
    
    if (ancestorPart)
      thisDomain = ancestorPart->document()->domain();
  }

  // FIXME: this should check that URL scheme and port match too, probably
  if (actDomain == thisDomain)
    return true;

  if (Interpreter::shouldPrintExceptions()) {
      printf("Unsafe JavaScript attempt to access frame with URL %s from frame with URL %s. Domains must match.\n", 
             thisDocument->URL().latin1(), actDocument->URL().latin1());
  }
  QString message;
  message.sprintf("Unsafe JavaScript attempt to access frame with URL %s from frame with URL %s. Domains must match.\n", 
                  thisDocument->URL().latin1(), actDocument->URL().latin1());
  m_frame->addMessageToConsole(DOMString(message), 1, DOMString());
  
  return false;
}

void Window::setListener(ExecState *exec, const AtomicString &eventType, JSValue *func)
{
  if (!isSafeScript(exec))
    return;
  DOM::DocumentImpl *doc = m_frame->document();
  if (!doc)
    return;

  doc->setHTMLWindowEventListener(eventType, getJSEventListener(func,true));
}

JSValue *Window::getListener(ExecState *exec, const AtomicString &eventType) const
{
  if (!isSafeScript(exec))
    return jsUndefined();
  DOM::DocumentImpl *doc = m_frame->document();
  if (!doc)
    return jsUndefined();

  DOM::EventListener *listener = doc->getHTMLWindowEventListener(eventType);
  if (listener && static_cast<JSEventListener*>(listener)->listenerObj())
    return static_cast<JSEventListener*>(listener)->listenerObj();
  else
    return jsNull();
}

JSEventListener *Window::getJSEventListener(JSValue *val, bool html)
{
  if (!val->isObject())
    return 0;
  JSObject *object = static_cast<JSObject *>(val);

  if (JSEventListener* listener = jsEventListeners.get(object))
    return listener;

  // Note that the JSEventListener constructor adds it to our jsEventListeners list
  return new JSEventListener(object, this, html);
}

JSUnprotectedEventListener *Window::getJSUnprotectedEventListener(JSValue *val, bool html)
{
  if (!val->isObject())
    return 0;
  JSObject* object = static_cast<JSObject *>(val);

  if (JSUnprotectedEventListener* listener = jsUnprotectedEventListeners.get(object))
    return listener;

  // The JSUnprotectedEventListener constructor adds it to our jsUnprotectedEventListeners map.
  return new JSUnprotectedEventListener(object, this, html);
}

void Window::clear()
{
  JSLock lock;
  if (m_returnValueSlot)
    if (JSValue *returnValue = getDirect("returnValue"))
      *m_returnValueSlot = returnValue;
  kdDebug(6070) << "Window::clear " << this << endl;
  delete winq;
  winq = new WindowQObject(this);

  clearProperties();
  // there's likely to be lots of garbage now
  Collector::collect();

  // Now recreate a working global object for the next URL that will use us
  interpreter()->initGlobalObject();
}

void Window::setCurrentEvent(EventImpl *evt)
{
  m_evt = evt;
  //kdDebug(6070) << "Window " << this << " (frame=" << m_frame << ")::setCurrentEvent m_evt=" << evt << endl;
}

static void setWindowFeature(const DOMString& keyString, const DOMString& valueString, WindowArgs& windowArgs)
{
    int value;
    
    if (valueString.length() == 0 || // listing a key with no value is shorthand for key=yes
        valueString == "yes")
        value = 1;
    else
        value = valueString.toInt();
    
    if (keyString == "left" || keyString == "screenx") {
        windowArgs.xSet = true;
        windowArgs.x = value;
    } else if (keyString == "top" || keyString == "screeny") {
        windowArgs.ySet = true;
        windowArgs.y = value;
    } else if (keyString == "width" || keyString == "innerwidth") {
        windowArgs.widthSet = true;
        windowArgs.width = value;
    } else if (keyString == "height" || keyString == "innerheight") {
        windowArgs.heightSet = true;
        windowArgs.height = value;
    } else if (keyString == "menubar")
        windowArgs.menuBarVisible = value;
    else if (keyString == "toolbar")
        windowArgs.toolBarVisible = value;
    else if (keyString == "location")
        windowArgs.locationBarVisible = value;
    else if (keyString == "status")
        windowArgs.statusBarVisible = value;
    else if (keyString == "resizable")
        windowArgs.resizable = value;
    else if (keyString == "fullscreen")
        windowArgs.fullscreen = value;
    else if (keyString == "scrollbars")
        windowArgs.scrollBarsVisible = value;
}

// Though isspace() considers \t and \v to be whitespace, Win IE doesn't.
static bool isSeparator(QChar c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '=' || c == ',' || c == '\0';
}

static void parseWindowFeatures(const DOMString& features, WindowArgs& windowArgs)
{
    /*
     The IE rule is: all features except for channelmode and fullscreen default to YES, but
     if the user specifies a feature string, all features default to NO. (There is no public
     standard that applies to this method.)
     
     <http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/open_0.asp>
     */
    
    windowArgs.dialog = false;
    windowArgs.fullscreen = false;
    
    windowArgs.xSet = false;
    windowArgs.ySet = false;
    windowArgs.widthSet = false;
    windowArgs.heightSet = false;
    
    if (features.length() == 0) {
        windowArgs.menuBarVisible = true;
        windowArgs.statusBarVisible = true;
        windowArgs.toolBarVisible = true;
        windowArgs.locationBarVisible = true;
        windowArgs.scrollBarsVisible = true;
        windowArgs.resizable = true;
        
        return;
    }
    
    windowArgs.menuBarVisible = false;
    windowArgs.statusBarVisible = false;
    windowArgs.toolBarVisible = false;
    windowArgs.locationBarVisible = false;
    windowArgs.scrollBarsVisible = false;
    windowArgs.resizable = false;
    
    // Tread lightly in this code -- it was specifically designed to mimic Win IE's parsing behavior.
    int keyBegin, keyEnd;
    int valueBegin, valueEnd;
    
    int i = 0;
    int length = features.length();
    DOMString buffer = features.lower();
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
        
        assert(i <= length);

        DOMString keyString(buffer.substring(keyBegin, keyEnd - keyBegin));
        DOMString valueString(buffer.substring(valueBegin, valueEnd - valueBegin));
        setWindowFeature(keyString, valueString, windowArgs);
    }
}

static void constrainToVisible(const IntRect& screen, WindowArgs& windowArgs)
{
    windowArgs.x += screen.x();
    if (windowArgs.x < screen.x() || windowArgs.x >= screen.right())
        windowArgs.x = screen.x(); // only safe choice until size is determined
    
    windowArgs.y += screen.y();
    if (windowArgs.y < screen.y() || windowArgs.y >= screen.bottom())
        windowArgs.y = screen.y(); // only safe choice until size is determined
    
    if (windowArgs.height > screen.height()) // should actually check workspace
        windowArgs.height = screen.height();
    if (windowArgs.height < 100)
        windowArgs.height = 100;
    
    if (windowArgs.width > screen.width()) // should actually check workspace
        windowArgs.width = screen.width();
    if (windowArgs.width < 100)
        windowArgs.width = 100;
}

JSValue *WindowFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  if (!thisObj->inherits(&Window::info))
    return throwError(exec, TypeError);
  Window *window = static_cast<Window *>(thisObj);
  Frame *frame = window->m_frame;
  if (!frame)
    return jsUndefined();

  FrameView *widget = frame->view();
  JSValue *v = args[0];
  UString s = v->toString(exec);
  DOMString str = s.domString();
  DOMString str2;

  switch (id) {
  case Window::Alert:
    if (frame && frame->document())
      frame->document()->updateRendering();
    frame->runJavaScriptAlert(str);
    return jsUndefined();
  case Window::Confirm:
    if (frame && frame->document())
      frame->document()->updateRendering();
    return jsBoolean(frame->runJavaScriptConfirm(str));
  case Window::Prompt:
  {
    if (frame && frame->document())
      frame->document()->updateRendering();
    DOMString message = args.size() >= 2 ? args[1]->toString(exec).domString() : DOMString();
    bool ok = frame->runJavaScriptPrompt(str, message, str2);
    if (ok)
        return jsString(str2);
    else
        return jsNull();
  }
  case Window::Open:
  {
      QString frameName = args[1]->isUndefinedOrNull() ? QString("_blank") : args[1]->toString(exec).qstring();
      if (!allowPopUp(exec, window)
            && !(frameName == "_top" || frameName == "_parent" || frameName == "_self" || frame->findFrame(frameName)))
          return jsUndefined();
      
      WindowArgs windowArgs;
      DOMString features = args[2]->isUndefinedOrNull() ? DOMString() : args[2]->toString(exec).domString();
      parseWindowFeatures(features, windowArgs);
      constrainToVisible(screenRect(widget), windowArgs);
      
      // prepare arguments
      KURL url;
      Frame* activePart = Window::retrieveActive(exec)->m_frame;
      if (!str.isEmpty() && activePart)
          url = activePart->document()->completeURL(str.qstring());

      URLArgs uargs;
      uargs.frameName = frameName;
      if (uargs.frameName == "_top") {
          while (frame->treeNode()->parent())
              frame = frame->treeNode()->parent();
          
          const Window* window = Window::retrieveWindow(frame);
          if (!url.url().startsWith("javascript:", false) || (window && window->isSafeScript(exec))) {
              bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
              frame->scheduleLocationChange(url.url(), activePart->referrer(), false/*don't lock history*/, userGesture);
          }
          return Window::retrieve(frame);
      }
      if (uargs.frameName == "_parent") {
          if (frame->treeNode()->parent())
              frame = frame->treeNode()->parent();
          
          const Window* window = Window::retrieveWindow(frame);
          if (!url.url().startsWith("javascript:", false) || (window && window->isSafeScript(exec))) {
              bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
              frame->scheduleLocationChange(url.url(), activePart->referrer(), false/*don't lock history*/, userGesture);
          }
          return Window::retrieve(frame);
      }
      uargs.serviceType = "text/html";
      
      // request window (new or existing if framename is set)
      ObjectContents* newPart = 0;
      uargs.metaData().set("referrer", activePart->referrer());
      frame->browserExtension()->createNewWindow("", uargs, windowArgs, newPart);
      if (!newPart || !newPart->inherits("Frame"))
          return jsUndefined();
      Frame *newFrame = static_cast<Frame*>(newPart);
      newFrame->setOpener(frame);
      newFrame->setOpenedByJS(true);
      
      if (!newFrame->document()) {
          DocumentImpl* oldDoc = frame->document();
          if (oldDoc && oldDoc->baseURL() != 0)
              newFrame->begin(oldDoc->baseURL());
          else
              newFrame->begin();
          newFrame->write("<HTML><BODY>");
          newFrame->end();          
          if (oldDoc) {
              newFrame->document()->setDomain(oldDoc->domain(), true);
              newFrame->document()->setBaseURL(oldDoc->baseURL());
          }
      }
      if (!url.isEmpty()) {
          const Window* window = Window::retrieveWindow(newFrame);
          if (!url.url().startsWith("javascript:", false) || (window && window->isSafeScript(exec))) {
              bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
              newFrame->scheduleLocationChange(url.url(), activePart->referrer(), false, userGesture);
          }
      }
      return Window::retrieve(newFrame); // global object
  }
  case Window::Print:
    frame->print();
    return jsUndefined();
  case Window::ScrollBy:
    window->updateLayout();
    if(args.size() >= 2 && widget)
      widget->scrollBy(args[0]->toInt32(exec), args[1]->toInt32(exec));
    return jsUndefined();
  case Window::Scroll:
  case Window::ScrollTo:
    window->updateLayout();
    if (args.size() >= 2 && widget)
      widget->setContentsPos(args[0]->toInt32(exec), args[1]->toInt32(exec));
    return jsUndefined();
  case Window::MoveBy:
    if(args.size() >= 2 && widget)
    {
      QWidget* tl = widget->topLevelWidget();
      IntRect sg = screenRect(widget);
      IntPoint dest = tl->pos() + IntPoint(args[0]->toInt32(exec), args[1]->toInt32(exec));
      // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
      if (sg.contains(IntRect(dest, IntSize(tl->width(), tl->height()))))
        tl->move(dest);
    }
    return jsUndefined();
  case Window::MoveTo:
    if(args.size() >= 2 && widget)
    {
      QWidget* tl = widget->topLevelWidget();
      IntRect sg = screenRect(widget);
      IntPoint dest(args[0]->toInt32(exec) + sg.x(), args[1]->toInt32(exec) + sg.y());
      // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
      if (sg.contains(IntRect(dest, IntSize(tl->width(), tl->height()))))
        tl->move(dest);
    }
    return jsUndefined();
  case Window::ResizeBy:
    if(args.size() >= 2 && widget)
    {
      QWidget* tl = widget->topLevelWidget();
      IntSize dest = tl->size() + IntSize(args[0]->toInt32(exec), args[1]->toInt32(exec));
      IntRect sg = screenRect(widget);
      // Security check: within desktop limits and bigger than 100x100 (per spec)
      if (tl->x() + dest.width() <= sg.right() && tl->y() + dest.height() <= sg.bottom()
           && dest.width() >= 100 && dest.height() >= 100)
      {
        // Take into account the window frame
        int deltaWidth = tl->frameGeometry().width() - tl->width();
        int deltaHeight = tl->frameGeometry().height() - tl->height();
        tl->resize(dest.width() - deltaWidth, dest.height() - deltaHeight);
      }
    }
    return jsUndefined();
  case Window::ResizeTo:
    if (args.size() >= 2 && widget) {
      QWidget* tl = widget->topLevelWidget();
      IntSize dest = IntSize(args[0]->toInt32(exec), args[1]->toInt32(exec));
      IntRect sg = screenRect(widget);
      // Security check: within desktop limits and bigger than 100x100 (per spec)
      if (tl->x() + dest.width() <= sg.right() && tl->y() + dest.height() <= sg.bottom() &&
           dest.width() >= 100 && dest.height() >= 100)
      {
        // Take into account the window frame
        int deltaWidth = tl->frameGeometry().width() - tl->width();
        int deltaHeight = tl->frameGeometry().height() - tl->height();
        tl->resize(dest.width() - deltaWidth, dest.height() - deltaHeight);
      }
    }
    return jsUndefined();
  case Window::SetTimeout:
    if (!window->isSafeScript(exec))
        return jsUndefined();
    if (args.size() >= 2 && v->isString()) {
      int i = args[1]->toInt32(exec);
      int r = (const_cast<Window*>(window))->installTimeout(s, i, true /*single shot*/);
      return jsNumber(r);
    }
    else if (args.size() >= 2 && v->isObject() && static_cast<JSObject *>(v)->implementsCall()) {
      JSValue *func = args[0];
      int i = args[1]->toInt32(exec);

      // All arguments after the second should go to the function
      // FIXME: could be more efficient
      List funcArgs = args.copyTail().copyTail();

      int r = (const_cast<Window*>(window))->installTimeout(func, funcArgs, i, true /*single shot*/);
      return jsNumber(r);
    }
    else
      return jsUndefined();
  case Window::SetInterval:
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

      // All arguments after the second should go to the function
      // FIXME: could be more efficient
      List funcArgs = args.copyTail().copyTail();

      int r = (const_cast<Window*>(window))->installTimeout(func, funcArgs, i, false);
      return jsNumber(r);
    }
    else
      return jsUndefined();
  case Window::ClearTimeout:
  case Window::ClearInterval:
    if (!window->isSafeScript(exec))
        return jsUndefined();
    (const_cast<Window*>(window))->clearTimeout(v->toInt32(exec));
    return jsUndefined();
  case Window::Focus:
    if (widget)
      widget->setActiveWindow();
    return jsUndefined();
  case Window::GetSelection:
    if (!window->isSafeScript(exec))
        return jsUndefined();
    return window->selection();
  case Window::Blur:
    frame->unfocusWindow();
    return jsUndefined();
  case Window::Close:
    /* From http://developer.netscape.com/docs/manuals/js/client/jsref/window.htm :
       The close method closes only windows opened by JavaScript using the open method.
       If you attempt to close any other window, a confirm is generated, which
       lets the user choose whether the window closes.
       This is a security feature to prevent "mail bombs" containing self.close().
       However, if the window has only one document (the current one) in its
       session history, the close is allowed without any confirm. This is a
       special case for one-off windows that need to open other windows and
       then dispose of themselves.
    */
    if (!frame->openedByJS())
    {
      // To conform to the SPEC, we only ask if the window
      // has more than one entry in the history (NS does that too).
      History history(exec, frame);
      if ( history.get( exec, lengthPropertyName )->toInt32(exec) <= 1
           // FIXME: How are we going to handle this?
           )
        (const_cast<Window*>(window))->scheduleClose();
    }
    else
    {
      (const_cast<Window*>(window))->scheduleClose();
    }
    return jsUndefined();
  case Window::CaptureEvents:
  case Window::ReleaseEvents:
        // If anyone implements these, they need the safescript security check.
        if (!window->isSafeScript(exec))
            return jsUndefined();

    // Do nothing for now. These are NS-specific legacy calls.
    break;
  case Window::AddEventListener:
        if (!window->isSafeScript(exec))
            return jsUndefined();
        if (JSEventListener *listener = Window::retrieveActive(exec)->getJSEventListener(args[1]))
            if (DocumentImpl *doc = frame->document())
                doc->addWindowEventListener(AtomicString(args[0]->toString(exec).domString()), listener, args[2]->toBoolean(exec));
        return jsUndefined();
  case Window::RemoveEventListener:
        if (!window->isSafeScript(exec))
            return jsUndefined();
        if (JSEventListener *listener = Window::retrieveActive(exec)->getJSEventListener(args[1]))
            if (DocumentImpl *doc = frame->document())
                doc->removeWindowEventListener(AtomicString(args[0]->toString(exec).domString()), listener, args[2]->toBoolean(exec));
        return jsUndefined();
  case Window::ShowModalDialog:
    return showModalDialog(exec, window, args);
  }
  return jsUndefined();
}

void Window::updateLayout() const
{
  DOM::DocumentImpl* docimpl = m_frame->document();
  if (docimpl)
    docimpl->updateLayoutIgnorePendingStylesheets();
}

////////////////////// ScheduledAction ////////////////////////

void ScheduledAction::execute(Window *window)
{
    if (!window->m_frame || !window->m_frame->jScript())
        return;

    ScriptInterpreter *interpreter = window->m_frame->jScript()->interpreter();

    interpreter->setProcessingTimerCallback(true);
  
    if (JSValue* func = m_func.get()) {
        if (func->isObject() && static_cast<JSObject *>(func)->implementsCall()) {
            ExecState *exec = interpreter->globalExec();
            ASSERT(window == interpreter->globalObject());
            JSLock lock;
            static_cast<JSObject *>(func)->call(exec, window, m_args);
            if (exec->hadException()) {
                JSObject* exception = exec->exception()->toObject(exec);
                exec->clearException();
                DOMString message = exception->get(exec, messagePropertyName)->toString(exec).domString();
                int lineNumber = exception->get(exec, "line")->toInt32(exec);
                if (Interpreter::shouldPrintExceptions())
                    printf("(timer):%s\n", message.qstring().utf8().data());
                window->m_frame->addMessageToConsole(message, lineNumber, DOMString());
            }
        }
    } else
        window->m_frame->executeScript(0, m_code);
  
    // Update our document's rendering following the execution of the timeout callback.
    // FIXME: Why? Why not other documents, for example?
    DocumentImpl *doc = window->m_frame->document();
    if (doc)
        doc->updateRendering();
  
    interpreter->setProcessingTimerCallback(false);
}

////////////////////// WindowQObject ////////////////////////

WindowQObject::WindowQObject(Window *w)
    : m_parent(w)
{
    connect(w->m_frame, SIGNAL(destroyed()), this, SLOT(parentDestroyed()));
}

void WindowQObject::parentDestroyed()
{
    deleteAllValues(m_timeouts);
    m_timeouts.clear();
}

int WindowQObject::installTimeout(ScheduledAction* a, int t, bool singleShot)
{
    int timeoutId = ++lastUsedTimeoutId;
    DOMWindowTimer* timer = new DOMWindowTimer(timeoutId, this, a);
    ASSERT(!m_timeouts.get(timeoutId));
    m_timeouts.set(timeoutId, timer);
    // Use a minimum interval of 10 ms to match other browsers.
    // Faster timers might be "better", but they're incompatible.
    double interval = t <= 10 ? 0.010 : t * 0.001;
    if (singleShot)
        timer->startOneShot(interval);
    else
        timer->startRepeating(interval);
    return timeoutId;
}

int WindowQObject::installTimeout(const UString& handler, int t, bool singleShot)
{
    return installTimeout(new ScheduledAction(handler.qstring()), t, singleShot);
}

int WindowQObject::installTimeout(JSValue* func, const List& args, int t, bool singleShot)
{
    return installTimeout(new ScheduledAction(func, args), t, singleShot);
}

PausedTimeouts *WindowQObject::pauseTimeouts()
{
    size_t count = m_timeouts.size();
    if (count == 0)
        return 0;

    PausedTimeout* t = new PausedTimeout [count];
    PausedTimeouts* result = new PausedTimeouts(t, count);

    TimeoutsMap::iterator it = m_timeouts.begin();
    for (size_t i = 0; i != count; ++i, ++it) {
        int timeoutId = it->first;
        DOMWindowTimer* timer = it->second;
        t[i].timeoutId = timeoutId;
        t[i].nextFireInterval = timer->nextFireInterval();
        t[i].repeatInterval = timer->repeatInterval();
        t[i].action = timer->takeAction();
    }
    ASSERT(it == m_timeouts.end());

    deleteAllValues(m_timeouts);
    m_timeouts.clear();

    return result;
}

void WindowQObject::resumeTimeouts(PausedTimeouts *timeouts)
{
    if (!timeouts)
        return;
    size_t count = timeouts->numTimeouts();
    PausedTimeout *array = timeouts->takeTimeouts();
    for (size_t i = 0; i != count; ++i) {
        int timeoutId = array[i].timeoutId;
        DOMWindowTimer* timer = new DOMWindowTimer(timeoutId, this, array[i].action);
        m_timeouts.set(timeoutId, timer);
        timer->start(array[i].nextFireInterval, array[i].repeatInterval);
    }
    delete [] array;
}

void WindowQObject::clearTimeout(int timeoutId, bool delAction)
{
    TimeoutsMap::iterator it = m_timeouts.find(timeoutId);
    if (it == m_timeouts.end())
        return;
    DOMWindowTimer* timer = it->second;
    m_timeouts.remove(it);
    delete timer;
}

void WindowQObject::timerFired(DOMWindowTimer* timer)
{
    // Simple case for non-one-shot timers.
    if (timer->isActive()) {
        timer->action()->execute(m_parent);
        return;
    }

    // Delete timer before executing the action for one-shot timers.
    ScheduledAction* action = timer->takeAction();
    m_timeouts.remove(timer->timeoutId());
    delete timer;
    action->execute(m_parent);
    delete action;
}

const ClassInfo FrameArray::info = { "FrameArray", 0, &FrameArrayTable, 0 };

/*
@begin FrameArrayTable 2
length          FrameArray::Length      DontDelete|ReadOnly
location        FrameArray::Location    DontDelete|ReadOnly
@end
*/

JSValue *FrameArray::getValueProperty(ExecState *exec, int token)
{
  switch (token) {
  case Length: {
    QPtrList<ObjectContents> frames = m_frame->frames();
    unsigned int len = frames.count();
    return jsNumber(len);
  }
  case Location:
    // non-standard property, but works in NS and IE
    if (JSObject *obj = Window::retrieveWindow(m_frame))
      return obj->get(exec, "location");
    return jsUndefined();
  default:
    ASSERT(0);
    return jsUndefined();
  }
}

JSValue *FrameArray::indexGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  FrameArray *thisObj = static_cast<FrameArray *>(slot.slotBase());
  ObjectContents *frame = thisObj->m_frame->frames().at(slot.index());

  if (frame && frame->inherits("Frame")) {
    Frame *khtml = static_cast<Frame*>(frame);
    return Window::retrieve(khtml);
  }

  return jsUndefined();
}

JSValue *FrameArray::nameGetter(ExecState *exec, JSObject *originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
  FrameArray *thisObj = static_cast<FrameArray *>(slot.slotBase());
  ObjectContents *frame = thisObj->m_frame->findFrame(propertyName.qstring());

  if (frame && frame->inherits("Frame")) {
    Frame *khtml = static_cast<Frame*>(frame);
    return  Window::retrieve(khtml);
  }

  return jsUndefined();
}

bool FrameArray::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  if (m_frame.isNull()) {
    slot.setUndefined(this);
    return true;
  }

  const HashEntry* entry = Lookup::findEntry(&FrameArrayTable, propertyName);
  if (entry) {
    slot.setStaticEntry(this, entry, staticValueGetter<FrameArray>);
    return true;
  }

  // check for the name or number
  ObjectContents *frame = m_frame->findFrame(propertyName.qstring());
  if (frame) {
    slot.setCustom(this, nameGetter);
    return true;
  }

  bool ok;
  unsigned int i = propertyName.toArrayIndex(&ok);
  if (ok && i < m_frame->frames().count()) {
    slot.setCustomIndex(this, i, indexGetter);
    return true;
  }

  return JSObject::getOwnPropertySlot(exec, propertyName, slot);
}

UString FrameArray::toString(ExecState *) const
{
  return "[object FrameArray]";
}

////////////////////// Location Object ////////////////////////

const ClassInfo Location::info = { "Location", 0, 0, 0 };
/*
@begin LocationTable 12
  assign        Location::Assign        DontDelete|Function 1
  hash          Location::Hash          DontDelete
  host          Location::Host          DontDelete
  hostname      Location::Hostname      DontDelete
  href          Location::Href          DontDelete
  pathname      Location::Pathname      DontDelete
  port          Location::Port          DontDelete
  protocol      Location::Protocol      DontDelete
  search        Location::Search        DontDelete
  [[==]]        Location::EqualEqual    DontDelete|ReadOnly
  toString      Location::ToString      DontDelete|Function 0
  replace       Location::Replace       DontDelete|Function 1
  reload        Location::Reload        DontDelete|Function 0
@end
*/
KJS_IMPLEMENT_PROTOFUNC(LocationFunc)
Location::Location(Frame *p) : m_frame(p)
{
}

JSValue *Location::getValueProperty(ExecState *exec, int token) const
{
  KURL url = m_frame->url();
  switch (token) {
  case Hash:
    return jsString(url.ref().isNull() ? QString("") : "#" + url.ref());
  case Host: {
    // Note: this is the IE spec. The NS spec swaps the two, it says
    // "The hostname property is the concatenation of the host and port properties, separated by a colon."
    // Bleh.
    UString str = url.host();
    if (url.port())
            str += ":" + QString::number((int)url.port());
    return jsString(str);
  }
  case Hostname:
    return jsString(url.host());
  case Href:
    if (!url.hasPath())
      return jsString(url.prettyURL() + "/");
    else
      return jsString(url.prettyURL());
  case Pathname:
    return jsString(url.path().isEmpty() ? QString("/") : url.path());
  case Port:
    return jsString(url.port() ? QString::number((int)url.port()) : QString::fromLatin1(""));
  case Protocol:
    return jsString(url.protocol()+":");
  case Search:
    return jsString(url.query());
  default:
    ASSERT(0);
    return jsUndefined();
  }
}

bool Location::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot) 
{
  if (m_frame.isNull())
    return false;
  
  const Window* window = Window::retrieveWindow(m_frame);
  if (!window || !window->isSafeScript(exec)) {
      slot.setUndefined(this);
      return true;
  }

  return getStaticPropertySlot<LocationFunc, Location, JSObject>(exec, &LocationTable, this, propertyName, slot);
}

void Location::put(ExecState *exec, const Identifier &p, JSValue *v, int attr)
{
  if (m_frame.isNull())
    return;

  QString str = v->toString(exec).qstring();
  KURL url = m_frame->url();
  const HashEntry *entry = Lookup::findEntry(&LocationTable, p);
  if (entry)
    switch (entry->value) {
    case Href: {
      Frame* p = Window::retrieveActive(exec)->frame();
      if ( p )
        url = p->document()->completeURL( str );
      else
        url = str;
      break;
    }
    case Hash:
      url.setRef(str);
      break;
    case Host: {
      QString host = str.left(str.find(":"));
      QString port = str.mid(str.find(":")+1);
      url.setHost(host);
      url.setPort(port.toUInt());
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
    }
  else {
    JSObject::put(exec, p, v, attr);
    return;
  }

  const Window* window = Window::retrieveWindow(m_frame);
  Frame* activePart = Window::retrieveActive(exec)->frame();
  if (!url.url().startsWith("javascript:", false) || (window && window->isSafeScript(exec))) {
    bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
    // We want a new history item if this JS was called via a user gesture
    m_frame->scheduleLocationChange(url.url(), activePart->referrer(), !userGesture, userGesture);
  }
}

JSValue *Location::toPrimitive(ExecState *exec, Type) const
{
  return jsString(toString(exec));
}

UString Location::toString(ExecState *) const
{
  if (!m_frame->url().hasPath())
    return m_frame->url().prettyURL()+"/";
  else
    return m_frame->url().prettyURL();
}

JSValue *LocationFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  if (!thisObj->inherits(&Location::info))
    return throwError(exec, TypeError);
  Location *location = static_cast<Location *>(thisObj);
  Frame *frame = location->frame();
  if (frame) {
      
    Window* window = Window::retrieveWindow(frame);
    if (!window->isSafeScript(exec) && id != Location::Replace)
        return jsUndefined();
      
    switch (id) {
    case Location::Replace:
    {
      QString str = args[0]->toString(exec).qstring();
      Frame* p = Window::retrieveActive(exec)->frame();
      if ( p ) {
        const Window* window = Window::retrieveWindow(frame);
        if (!str.startsWith("javascript:", false) || (window && window->isSafeScript(exec))) {
          bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
          frame->scheduleLocationChange(p->document()->completeURL(str), p->referrer(), true /*lock history*/, userGesture);
        }
      }
      break;
    }
    case Location::Reload:
    {
      const Window* window = Window::retrieveWindow(frame);
      Frame* activePart = Window::retrieveActive(exec)->frame();
      if (!frame->url().url().startsWith("javascript:", false) || (window && window->isSafeScript(exec))) {
        bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
        frame->scheduleLocationChange(frame->url().url(), activePart->referrer(), true/*lock history*/, userGesture);
      }
      break;
    }
    case Location::Assign:
    {
        Frame *p = Window::retrieveActive(exec)->frame();
        if (p) {
            const Window *window = Window::retrieveWindow(frame);
            QString dstUrl = p->document()->completeURL(args[0]->toString(exec).qstring());
            if (!dstUrl.startsWith("javascript:", false) || (window && window->isSafeScript(exec))) {
                bool userGesture = static_cast<ScriptInterpreter *>(exec->dynamicInterpreter())->wasRunByUserGesture();
                // We want a new history item if this JS was called via a user gesture
                frame->scheduleLocationChange(dstUrl, p->referrer(), !userGesture, userGesture);
            }
        }
        break;
    }
    case Location::ToString:
      return jsString(location->toString(exec));
    }
  } else
    kdDebug(6070) << "LocationFunc::tryExecute - no frame!" << endl;
  return jsUndefined();
}

////////////////////// Selection Object ////////////////////////

const ClassInfo Selection::info = { "Selection", 0, 0, 0 };
/*
@begin SelectionTable 19
  anchorNode                Selection::AnchorNode               DontDelete|ReadOnly
  anchorOffset              Selection::AnchorOffset             DontDelete|ReadOnly
  focusNode                 Selection::FocusNode                DontDelete|ReadOnly
  focusOffset               Selection::FocusOffset              DontDelete|ReadOnly
  baseNode                  Selection::BaseNode                 DontDelete|ReadOnly
  baseOffset                Selection::BaseOffset               DontDelete|ReadOnly
  extentNode                Selection::ExtentNode               DontDelete|ReadOnly
  extentOffset              Selection::ExtentOffset             DontDelete|ReadOnly
  isCollapsed               Selection::IsCollapsed              DontDelete|ReadOnly
  type                      Selection::_Type                    DontDelete|ReadOnly
  [[==]]                    Selection::EqualEqual               DontDelete|ReadOnly
  toString                  Selection::ToString                 DontDelete|Function 0
  collapse                  Selection::Collapse                 DontDelete|Function 2
  collapseToEnd             Selection::CollapseToEnd            DontDelete|Function 0
  collapseToStart           Selection::CollapseToStart          DontDelete|Function 0
  empty                     Selection::Empty                    DontDelete|Function 0
  setBaseAndExtent          Selection::SetBaseAndExtent         DontDelete|Function 4
  setPosition               Selection::SetPosition              DontDelete|Function 2
  modify                    Selection::Modify                   DontDelete|Function 3
  getRangeAt                Selection::GetRangeAt               DontDelete|Function 1
@end
*/
KJS_IMPLEMENT_PROTOFUNC(SelectionFunc)
Selection::Selection(Frame *p) : m_frame(p)
{
}

JSValue *Selection::getValueProperty(ExecState *exec, int token) const
{
    SelectionController s = m_frame->selection();
    const Window* window = Window::retrieveWindow(m_frame);
    if (!window)
        return jsUndefined();
        
    switch (token) {
    case AnchorNode:
        return getDOMNode(exec, s.anchorNode());
    case BaseNode:
        return getDOMNode(exec, s.baseNode());
    case AnchorOffset:
        return jsNumber(s.anchorOffset());
    case BaseOffset:
        return jsNumber(s.baseOffset());
    case FocusNode:
        return getDOMNode(exec, s.focusNode());
    case ExtentNode:
        return getDOMNode(exec, s.extentNode());
    case FocusOffset:
        return jsNumber(s.focusOffset());
    case ExtentOffset:
        return jsNumber(s.extentOffset());
    case IsCollapsed:
        return jsBoolean(s.isCollapsed());
    case _Type:
        return jsString(s.type());
    default:
        ASSERT(0);
        return jsUndefined();
    }
}

bool Selection::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  if (m_frame.isNull())
      return false;

  return getStaticPropertySlot<SelectionFunc, Selection, JSObject>(exec, &SelectionTable, this, propertyName, slot);
}

JSValue *Selection::toPrimitive(ExecState *exec, Type) const
{
  return jsString(toString(exec));
}

UString Selection::toString(ExecState *) const
{
    return UString((m_frame->selection()).toString());
}

JSValue *SelectionFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
    if (!thisObj->inherits(&Selection::info))
        return throwError(exec, TypeError);
    Selection *selection = static_cast<Selection *>(thisObj);
    Frame *frame = selection->frame();
    if (frame) {
        SelectionController s = frame->selection();
        
        switch (id) {
            case Selection::Collapse:
                s.collapse(toNode(args[0]), args[1]->toInt32(exec));
                break;
            case Selection::CollapseToEnd:
                s.collapseToEnd();
                break;
            case Selection::CollapseToStart:
                s.collapseToStart();
                break;
            case Selection::Empty:
                s.empty();
                break;
            case Selection::SetBaseAndExtent:
                s.setBaseAndExtent(toNode(args[0]), args[1]->toInt32(exec), toNode(args[2]), args[3]->toInt32(exec));
                break;
            case Selection::SetPosition:
                s.setPosition(toNode(args[0]), args[1]->toInt32(exec));
                break;
            case Selection::Modify:
                s.modify(args[0]->toString(exec).domString(), args[1]->toString(exec).domString(), args[2]->toString(exec).domString());
                break;
            case Selection::GetRangeAt:
                return getDOMRange(exec, s.getRangeAt(args[0]->toInt32(exec)).get());
            case Selection::ToString:
                return jsString(s.toString());
        }
        
        frame->setSelection(s);
    }

    return jsUndefined();
}

////////////////////// BarInfo Object ////////////////////////

const ClassInfo BarInfo::info = { "BarInfo", 0, 0, 0 };
/*
@begin BarInfoTable 1
  visible                BarInfo::Visible                        DontDelete|ReadOnly
@end
*/
BarInfo::BarInfo(ExecState *exec, Frame *p, Type barType) 
  : JSObject(exec->lexicalInterpreter()->builtinObjectPrototype())
  , m_frame(p)
  , m_type(barType)
{
}

JSValue *BarInfo::getValueProperty(ExecState *exec, int token) const
{
    ASSERT(token == Visible);
    switch (m_type) {
    case Locationbar:
        return jsBoolean(m_frame->locationbarVisible());
    case Menubar: 
        return jsBoolean(m_frame->locationbarVisible());
    case Personalbar:
        return jsBoolean(m_frame->personalbarVisible());
    case Scrollbars: 
        return jsBoolean(m_frame->scrollbarsVisible());
    case Statusbar:
        return jsBoolean(m_frame->statusbarVisible());
    case Toolbar:
        return jsBoolean(m_frame->toolbarVisible());
    default:
        return jsBoolean(false);
    }
}

bool BarInfo::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  if (m_frame.isNull())
    return false;
  
  return getStaticValueSlot<BarInfo, JSObject>(exec, &BarInfoTable, this, propertyName, slot);
}

////////////////////// History Object ////////////////////////

const ClassInfo History::info = { "History", 0, 0, 0 };
/*
@begin HistoryTable 4
  length        History::Length         DontDelete|ReadOnly
  back          History::Back           DontDelete|Function 0
  forward       History::Forward        DontDelete|Function 0
  go            History::Go             DontDelete|Function 1
@end
*/
KJS_IMPLEMENT_PROTOFUNC(HistoryFunc)

bool History::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticPropertySlot<HistoryFunc, History, JSObject>(exec, &HistoryTable, this, propertyName, slot);
}

JSValue *History::getValueProperty(ExecState *, int token) const
{
  switch (token) {
  case Length:
  {
    BrowserExtension *ext = m_frame->browserExtension();
    if (!ext)
      return jsNumber(0);

    return jsNumber(ext->getHistoryLength());
  }
  default:
    kdWarning() << "Unhandled token in History::getValueProperty : " << token << endl;
    return jsUndefined();
  }
}

UString History::toString(ExecState *exec) const
{
  return "[object History]";
}

JSValue *HistoryFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  if (!thisObj->inherits(&History::info))
    return throwError(exec, TypeError);
  History *history = static_cast<History *>(thisObj);

  int steps;
  switch (id) {
  case History::Back:
    steps = -1;
    break;
  case History::Forward:
    steps = 1;
    break;
  case History::Go:
    steps = args[0]->toInt32(exec);
    break;
  default:
    return jsUndefined();
  }

  history->m_frame->scheduleHistoryNavigation(steps);
  return jsUndefined();
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
    m_object->timerFired(this);
}

} // namespace KJS
