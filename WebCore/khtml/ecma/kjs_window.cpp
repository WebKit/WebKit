// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003 Apple Computer, Inc.
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

#include <qstylesheet.h>
#include <qtimer.h>
#include <qinputdialog.h>
#include <qpaintdevicemetrics.h>
#include <qapplication.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <kparts/browserinterface.h>
#include <kwin.h>
#include <kwinmodule.h>
#include <kconfig.h>
#include <assert.h>
#include <qstyle.h>
#include "rendering/render_canvas.h"

#if APPLE_CHANGES
#include "KWQLogging.h"
#include "KWQKConfigBase.h"
#endif
#include <kjs/collector.h>
#include "kjs_proxy.h"
#include "kjs_window.h"
#include "kjs_navigator.h"
#include "kjs_html.h"
#include "kjs_range.h"
#include "kjs_traversal.h"
#include "kjs_css.h"
#include "kjs_events.h"

#include "khtmlview.h"
#include "khtml_part.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/dom_docimpl.h"
#include "html/html_documentimpl.h"

using namespace KJS;

namespace KJS {

////////////////////// History Object ////////////////////////

  class History : public ObjectImp {
    friend class HistoryFunc;
  public:
    History(ExecState *exec, KHTMLPart *p)
      : ObjectImp(exec->interpreter()->builtinObjectPrototype()), part(p) { }
    virtual Value get(ExecState *exec, const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Back, Forward, Go, Length };
    virtual UString toString(ExecState *exec) const;
  private:
    QGuardedPtr<KHTMLPart> part;
  };

  class FrameArray : public ObjectImp {
  public:
    FrameArray(ExecState *exec, KHTMLPart *p)
      : ObjectImp(exec->interpreter()->builtinObjectPrototype()), part(p) { }
    virtual Value get(ExecState *exec, const Identifier &propertyName) const;
    virtual UString toString(ExecState *exec) const;
  private:
    QGuardedPtr<KHTMLPart> part;
  };

#ifdef Q_WS_QWS
  class KonquerorFunc : public DOMFunction {
  public:
    KonquerorFunc(const Konqueror* k, const char* name)
      : DOMFunction(), konqueror(k), m_name(name) { }
    virtual Value tryCall(ExecState *exec, Object &thisObj, const List &args);

  private:
    const Konqueror* konqueror;
    QCString m_name;
  };
#endif
}; // namespace KJS

#include "kjs_window.lut.h"

////////////////////// Screen Object ////////////////////////

// table for screen object
/*
@begin ScreenTable 7
  height        Screen::Height		DontEnum|ReadOnly
  width         Screen::Width		DontEnum|ReadOnly
  colorDepth    Screen::ColorDepth	DontEnum|ReadOnly
  pixelDepth    Screen::PixelDepth	DontEnum|ReadOnly
  availLeft     Screen::AvailLeft	DontEnum|ReadOnly
  availTop      Screen::AvailTop	DontEnum|ReadOnly
  availHeight   Screen::AvailHeight	DontEnum|ReadOnly
  availWidth    Screen::AvailWidth	DontEnum|ReadOnly
@end
*/

const ClassInfo Screen::info = { "Screen", 0, &ScreenTable, 0 };

// We set the object prototype so that toString is implemented
Screen::Screen(ExecState *exec)
  : ObjectImp(exec->interpreter()->builtinObjectPrototype()) {}

Value Screen::get(ExecState *exec, const Identifier &p) const
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "Screen::get " << p.qstring() << endl;
#endif
  return lookupGetValue<Screen,ObjectImp>(exec,p,&ScreenTable,this);
}

Value Screen::getValueProperty(ExecState *exec, int token) const
{
  KWinModule info;
  QWidget *thisWidget = Window::retrieveActive(exec)->part()->view();
  QRect sg = QApplication::desktop()->screenGeometry(QApplication::desktop()->screenNumber(thisWidget));

  switch( token ) {
  case Height:
    return Number(sg.height());
  case Width:
    return Number(sg.width());
  case ColorDepth:
  case PixelDepth: {
    QPaintDeviceMetrics m(QApplication::desktop());
    return Number(m.depth());
  }
  case AvailLeft: {
    QRect clipped = info.workArea().intersect(sg);
    return Number(clipped.x()-sg.x());
  }
  case AvailTop: {
    QRect clipped = info.workArea().intersect(sg);
    return Number(clipped.y()-sg.y());
  }
  case AvailHeight: {
    QRect clipped = info.workArea().intersect(sg);
    return Number(clipped.height());
  }
  case AvailWidth: {
    QRect clipped = info.workArea().intersect(sg);
    return Number(clipped.width());
  }
  default:
    kdWarning() << "Screen::getValueProperty unhandled token " << token << endl;
    return Undefined();
  }
}

////////////////////// Window Object ////////////////////////

const ClassInfo Window::info = { "Window", 0, &WindowTable, 0 };

/*
@begin WindowTable 89
  closed	Window::Closed		DontDelete|ReadOnly
  crypto	Window::Crypto		DontDelete|ReadOnly
  defaultStatus	Window::DefaultStatus	DontDelete
  defaultstatus	Window::DefaultStatus	DontDelete
  status	Window::Status		DontDelete
  document	Window::Document	DontDelete|ReadOnly
  Node		Window::Node		DontDelete
  Event		Window::EventCtor	DontDelete
  Range		Window::Range		DontDelete
  NodeFilter	Window::NodeFilter	DontDelete
  DOMException	Window::DOMException	DontDelete
  CSSRule	Window::CSSRule		DontDelete
  frames	Window::Frames		DontDelete|ReadOnly
  history	Window::_History	DontDelete|ReadOnly
  event		Window::Event		DontDelete|ReadOnly
  innerHeight	Window::InnerHeight	DontDelete|ReadOnly
  innerWidth	Window::InnerWidth	DontDelete|ReadOnly
  length	Window::Length		DontDelete|ReadOnly
  location	Window::_Location	DontDelete
  name		Window::Name		DontDelete
  navigator	Window::_Navigator	DontDelete|ReadOnly
  clientInformation	Window::ClientInformation	DontDelete|ReadOnly
  konqueror	Window::_Konqueror	DontDelete|ReadOnly
  offscreenBuffering	Window::OffscreenBuffering	DontDelete|ReadOnly
  opener	Window::Opener		DontDelete|ReadOnly
  outerHeight	Window::OuterHeight	DontDelete|ReadOnly
  outerWidth	Window::OuterWidth	DontDelete|ReadOnly
  pageXOffset	Window::PageXOffset	DontDelete|ReadOnly
  pageYOffset	Window::PageYOffset	DontDelete|ReadOnly
  parent	Window::Parent		DontDelete|ReadOnly
  personalbar	Window::Personalbar	DontDelete|ReadOnly
  screenX	Window::ScreenX		DontDelete|ReadOnly
  screenY	Window::ScreenY		DontDelete|ReadOnly
  screenLeft	Window::ScreenLeft	DontDelete|ReadOnly
  screenTop	Window::ScreenTop	DontDelete|ReadOnly
  scrollbars	Window::Scrollbars	DontDelete|ReadOnly
  scroll	Window::Scroll		DontDelete|Function 2
  scrollBy	Window::ScrollBy	DontDelete|Function 2
  scrollTo	Window::ScrollTo	DontDelete|Function 2
  scrollX       Window::ScrollX         DontDelete|ReadOnly
  scrollY       Window::ScrollY         DontDelete|ReadOnly
  moveBy	Window::MoveBy		DontDelete|Function 2
  moveTo	Window::MoveTo		DontDelete|Function 2
  resizeBy	Window::ResizeBy	DontDelete|Function 2
  resizeTo	Window::ResizeTo	DontDelete|Function 2
  self		Window::Self		DontDelete|ReadOnly
  window	Window::_Window		DontDelete|ReadOnly
  top		Window::Top		DontDelete|ReadOnly
  screen	Window::_Screen		DontDelete|ReadOnly
  Image		Window::Image		DontDelete|ReadOnly
  Option	Window::Option		DontDelete|ReadOnly
  alert		Window::Alert		DontDelete|Function 1
  confirm	Window::Confirm		DontDelete|Function 1
  prompt	Window::Prompt		DontDelete|Function 2
  open		Window::Open		DontDelete|Function 3
  setTimeout	Window::SetTimeout	DontDelete|Function 2
  clearTimeout	Window::ClearTimeout	DontDelete|Function 1
  focus		Window::Focus		DontDelete|Function 0
  getSelection  Window::GetSelection    DontDelete|Function 0
  blur		Window::Blur		DontDelete|Function 0
  close		Window::Close		DontDelete|Function 0
  setInterval	Window::SetInterval	DontDelete|Function 2
  clearInterval	Window::ClearInterval	DontDelete|Function 1
  captureEvents	Window::CaptureEvents	DontDelete|Function 0
  releaseEvents	Window::ReleaseEvents	DontDelete|Function 0
# Warning, when adding a function to this object you need to add a case in Window::get
  addEventListener	Window::AddEventListener	DontDelete|Function 3
  removeEventListener	Window::RemoveEventListener	DontDelete|Function 3
  onabort	Window::Onabort		DontDelete
  onblur	Window::Onblur		DontDelete
  onchange	Window::Onchange	DontDelete
  onclick	Window::Onclick		DontDelete
  ondblclick	Window::Ondblclick	DontDelete
  ondragdrop	Window::Ondragdrop	DontDelete
  onerror	Window::Onerror		DontDelete
  onfocus	Window::Onfocus		DontDelete
  onkeydown	Window::Onkeydown	DontDelete
  onkeypress	Window::Onkeypress	DontDelete
  onkeyup	Window::Onkeyup		DontDelete
  onload	Window::Onload		DontDelete
  onmousedown	Window::Onmousedown	DontDelete
  onmousemove	Window::Onmousemove	DontDelete
  onmouseout	Window::Onmouseout	DontDelete
  onmouseover	Window::Onmouseover	DontDelete
  onmouseup	Window::Onmouseup	DontDelete
  onmove	Window::Onmove		DontDelete
  onreset	Window::Onreset		DontDelete
  onresize	Window::Onresize	DontDelete
  onselect	Window::Onselect	DontDelete
  onsubmit	Window::Onsubmit	DontDelete
  onunload	Window::Onunload	DontDelete
@end
*/
IMPLEMENT_PROTOFUNC(WindowFunc)

Window::Window(KHTMLPart *p)
  : ObjectImp(/*no proto*/), m_part(p), screen(0), history(0), frames(0), loc(0), m_evt(0)
{
  winq = new WindowQObject(this);
  //kdDebug(6070) << "Window::Window this=" << this << " part=" << m_part << " " << m_part->name() << endl;
}

Window::~Window()
{
  kdDebug(6070) << "Window::~Window this=" << this << " part=" << m_part << endl;
  delete winq;
}

Window *Window::retrieveWindow(KHTMLPart *p)
{
  Object obj = Object::dynamicCast( retrieve( p ) );
#ifndef NDEBUG
  // obj should never be null, except when javascript has been disabled in that part.
  if ( p && p->jScriptEnabled() )
  {
    assert( !obj.isNull() );
#ifndef QWS
    assert( dynamic_cast<KJS::Window*>(obj.imp()) ); // type checking
#endif
  }
#endif
  if ( obj.isNull() ) // JS disabled
    return 0;
  return static_cast<KJS::Window*>(obj.imp());
}

Window *Window::retrieveActive(ExecState *exec)
{
  ValueImp *imp = exec->interpreter()->globalObject().imp();
  assert( imp );
#ifndef QWS
  assert( dynamic_cast<KJS::Window*>(imp) );
#endif
  return static_cast<KJS::Window*>(imp);
}

Value Window::retrieve(KHTMLPart *p)
{
  assert(p);
  KJSProxy *proxy = KJSProxy::proxy( p );
  if (proxy) {
#ifdef KJS_VERBOSE
    kdDebug(6070) << "Window::retrieve part=" << p << " interpreter=" << proxy->interpreter() << " window=" << proxy->interpreter()->globalObject().imp() << endl;
#endif
    return proxy->interpreter()->globalObject(); // the Global object is the "window"
  } else
    return Undefined(); // This can happen with JS disabled on the domain of that window
}

Location *Window::location() const
{
  if (!loc)
    const_cast<Window*>(this)->loc = new Location(m_part);
  return loc;
}

// reference our special objects during garbage collection
void Window::mark()
{
  ObjectImp::mark();
  if (screen && !screen->marked())
    screen->mark();
  if (history && !history->marked())
    history->mark();
  if (frames && !frames->marked())
    frames->mark();
  //kdDebug(6070) << "Window::mark " << this << " marking loc=" << loc << endl;
  if (loc && !loc->marked())
    loc->mark();
}

bool Window::hasProperty(ExecState * /*exec*/, const Identifier &/*p*/) const
{
  //fprintf( stderr, "Window::hasProperty: always saying true\n" );

  // emulate IE behaviour: it doesn't throw exceptions when undeclared
  // variables are used. Returning true here will lead to get() returning
  // 'undefined' in those cases.
  return true;
}

UString Window::toString(ExecState *) const
{
  return "[object Window]";
}

Value Window::get(ExecState *exec, const Identifier &p) const
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "Window("<<this<<")::get " << p.qstring() << endl;
#endif
  if ( p == "closed" )
    return Boolean(m_part.isNull());

  // we don't want any operations on a closed window
  if (m_part.isNull())
    return Undefined();

  // Look for overrides first
  ValueImp * val = ObjectImp::getDirect(p);
  if (val) {
    //kdDebug(6070) << "Window::get found dynamic property '" << p.ascii() << "'" << endl;
    if (isSafeScript(exec))
      return Value(val);
  }

  const HashEntry* entry = Lookup::findEntry(&WindowTable, p);
  if (entry)
  {
    //kdDebug(6070) << "token: " << entry->value << endl;
    switch( entry->value ) {
    case Crypto:
      return Undefined(); // ###
    case DefaultStatus:
      return String(UString(m_part->jsDefaultStatusBarText()));
    case Status:
      return String(UString(m_part->jsStatusBarText()));
    case Document:
      if (isSafeScript(exec))
      {
        if (m_part->document().isNull()) {
#if APPLE_CHANGES
          KWQ(m_part)->createEmptyDocument();
#endif
          kdDebug(6070) << "Document.write: adding <HTML><BODY> to create document" << endl;
          m_part->begin();
          m_part->write("<HTML><BODY>");
          m_part->end();
        }
        Value val = getDOMNode(exec,m_part->document());
        return val;
      }
      else
        return Undefined();
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
    case Frames:
      return Value(frames ? frames :
                   (const_cast<Window*>(this)->frames = new FrameArray(exec,m_part)));
    case _History:
      return Value(history ? history :
                   (const_cast<Window*>(this)->history = new History(exec,m_part)));

    case Event:
      if (m_evt)
        return getDOMEvent(exec,*m_evt);
      else {
#ifdef KJS_VERBOSE
        kdWarning(6070) << "window(" << this << "," << m_part->name() << ").event, no event!" << endl;
#endif
        return Undefined();
      }
    case InnerHeight:
      if (!m_part->view())
        return Undefined();
      updateLayout();
      return Number(m_part->view()->visibleHeight());
    case InnerWidth:
      if (!m_part->view())
        return Undefined();
      updateLayout();
      return Number(m_part->view()->visibleWidth());
    case Length:
      return Number(m_part->frames().count());
    case _Location:
      if (isSafeScript(exec))
        return Value(location());
      else
        return Undefined();
    case Name:
      return String(m_part->name());
    case _Navigator:
    case ClientInformation: {
      // Store the navigator in the object so we get the same one each time.
      Navigator *n = new Navigator(exec, m_part);
      const_cast<Window *>(this)->putDirect("navigator", n, DontDelete|ReadOnly);
      const_cast<Window *>(this)->putDirect("clientInformation", n, DontDelete|ReadOnly);
      return Value(n);
    }
#ifdef Q_WS_QWS
    case _Konqueror:
      return Value(new Konqueror(m_part));
#endif
    case OffscreenBuffering:
      return Boolean(true);
    case Opener:
      if (!m_part->opener())
        return Null();    // ### a null Window might be better, but == null
      else                // doesn't work yet
        return retrieve(m_part->opener());
    case OuterHeight:
    case OuterWidth:
    {
      if (!m_part->view())
        return Number(0);
      KWin::Info inf = KWin::info(m_part->view()->topLevelWidget()->winId());
      return Number(entry->value == OuterHeight ?
                    inf.geometry.height() : inf.geometry.width());
    }
    case PageXOffset:
      if (!m_part->view())
        return Undefined();
      updateLayout();
      return Number(m_part->view()->contentsX());
    case PageYOffset:
      if (!m_part->view())
        return Undefined();
      updateLayout();
      return Number(m_part->view()->contentsY());
    case Parent:
      return Value(retrieve(m_part->parentPart() ? m_part->parentPart() : (KHTMLPart*)m_part));
    case Personalbar:
      return Undefined(); // ###
    case ScreenLeft:
    case ScreenX: {
      if (!m_part->view())
        return Undefined();
      QRect sg = QApplication::desktop()->screenGeometry(QApplication::desktop()->screenNumber(m_part->view()));
      return Number(m_part->view()->mapToGlobal(QPoint(0,0)).x() + sg.x());
    }
    case ScreenTop:
    case ScreenY: {
      if (!m_part->view())
        return Undefined();
      QRect sg = QApplication::desktop()->screenGeometry(QApplication::desktop()->screenNumber(m_part->view()));
      return Number(m_part->view()->mapToGlobal(QPoint(0,0)).y() + sg.y());
    }
    case ScrollX: {
      if (!m_part->view())
        return Undefined();
      updateLayout();
      return Number(m_part->view()->contentsX());
    }
    case ScrollY: {
      if (!m_part->view())
        return Undefined();
      updateLayout();
      return Number(m_part->view()->contentsY());
    }
    case Scrollbars:
      return Undefined(); // ###
    case Self:
    case _Window:
      return Value(retrieve(m_part));
    case Top: {
      KHTMLPart *p = m_part;
      while (p->parentPart())
        p = p->parentPart();
      return Value(retrieve(p));
    }
    case _Screen:
      return Value(screen ? screen :
                   (const_cast<Window*>(this)->screen = new Screen(exec)));
    case Image:
      return Value(new ImageConstructorImp(exec, m_part->document()));
    case Option:
      return Value(new OptionConstructorImp(exec, m_part->document()));
    case Alert:
    case Confirm:
    case Prompt:
    case Open:
    case Focus:
    case Blur:
    case Close:
    case Scroll: // compatibility
    case ScrollBy:
    case ScrollTo:
    case MoveBy:
    case MoveTo:
    case ResizeBy:
    case ResizeTo:
	return lookupOrCreateFunction<WindowFunc>(exec,p,this,entry->value,entry->params,entry->attr);
    case CaptureEvents:
    case ReleaseEvents:
    case AddEventListener:
    case RemoveEventListener:
    case SetTimeout:
    case ClearTimeout:
    case SetInterval:
    case ClearInterval:
    case GetSelection:
      if (isSafeScript(exec))
        return lookupOrCreateFunction<WindowFunc>(exec,p,this,entry->value,entry->params,entry->attr);
      else
        return Undefined();
    case Onabort:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::ABORT_EVENT);
      else
        return Undefined();
    case Onblur:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::BLUR_EVENT);
      else
        return Undefined();
    case Onchange:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::CHANGE_EVENT);
      else
        return Undefined();
    case Onclick:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::KHTML_CLICK_EVENT);
      else
        return Undefined();
    case Ondblclick:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::KHTML_DBLCLICK_EVENT);
      else
        return Undefined();
    case Ondragdrop:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::KHTML_DRAGDROP_EVENT);
      else
        return Undefined();
    case Onerror:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::KHTML_ERROR_EVENT);
      else
        return Undefined();
    case Onfocus:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::FOCUS_EVENT);
      else
        return Undefined();
    case Onkeydown:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::KHTML_KEYDOWN_EVENT);
      else
        return Undefined();
    case Onkeypress:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::KHTML_KEYPRESS_EVENT);
      else
        return Undefined();
    case Onkeyup:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::KHTML_KEYUP_EVENT);
      else
        return Undefined();
    case Onload:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::LOAD_EVENT);
      else
        return Undefined();
    case Onmousedown:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::MOUSEDOWN_EVENT);
      else
        return Undefined();
    case Onmousemove:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::MOUSEMOVE_EVENT);
      else
        return Undefined();
    case Onmouseout:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::MOUSEOUT_EVENT);
      else
        return Undefined();
    case Onmouseover:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::MOUSEOVER_EVENT);
      else
        return Undefined();
    case Onmouseup:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::MOUSEUP_EVENT);
      else
        return Undefined();
    case Onmove:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::KHTML_MOVE_EVENT);
      else
        return Undefined();
    case Onreset:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::RESET_EVENT);
      else
        return Undefined();
    case Onresize:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::RESIZE_EVENT);
      else
        return Undefined();
    case Onselect:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::SELECT_EVENT);
      else
        return Undefined();
    case Onsubmit:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::SUBMIT_EVENT);
      else
        return Undefined();
    case Onunload:
      if (isSafeScript(exec))
        return getListener(exec,DOM::EventImpl::UNLOAD_EVENT);
      else
        return Undefined();
    }
  }

  KHTMLPart *kp = m_part->findFrame( p.qstring() );
  if (kp)
    return Value(retrieve(kp));

  // allow shortcuts like 'Image1' instead of document.images.Image1
  if (isSafeScript(exec) &&
      m_part->document().isHTMLDocument()) { // might be XML
    DOM::HTMLCollection coll = m_part->htmlDocument().all();
    DOM::HTMLElement element = coll.namedItem(p.string());
    if (!element.isNull()) {
      return getDOMNode(exec,element);
    }
  }

  // give access to functions (and variables ?) from parent frameset
  if (m_part->parentPart())
  {
    Object parentObject = Object::dynamicCast( retrieve(m_part->parentPart()) );
    if ( !parentObject.isNull() )
    {
      Value ret = parentObject.get(exec,p);
      if (ret.type() != UndefinedType ) {
#ifdef KJS_VERBOSE
        kdDebug(6070) << "Window::get property " << p.qstring() << " found in parent part" << endl;
#endif
        return ret;
      }
    }
  }

  // This isn't necessarily a bug. Some code uses if(!window.blah) window.blah=1
  // But it can also mean something isn't loaded or implemented, hence the WARNING to help grepping.
#ifdef KJS_VERBOSE
  kdDebug(6070) << "WARNING: Window::get property not found: " << p.qstring() << endl;
#endif
  return Undefined();
}

void Window::put(ExecState* exec, const Identifier &propertyName, const Value &value, int attr)
{
  // Called by an internal KJS call (e.g. InterpreterImp's constructor) ?
  // If yes, save time and jump directly to ObjectImp.
  if ( (attr != None && attr != DontDelete)
       // Same thing if we have a local override (e.g. "var location")
       || ( ObjectImp::getDirect(propertyName) && isSafeScript(exec)) )
  {
    ObjectImp::put( exec, propertyName, value, attr );
    return;
  }

  const HashEntry* entry = Lookup::findEntry(&WindowTable, propertyName);
  if (entry)
  {
#ifdef KJS_VERBOSE
    kdDebug(6070) << "Window("<<this<<")::put " << propertyName.qstring() << endl;
#endif
    switch( entry->value ) {
    case Status: {
      String s = value.toString(exec);
      m_part->setJSStatusBarText(s.value().qstring());
      return;
    }
    case DefaultStatus: {
      String s = value.toString(exec);
      m_part->setJSDefaultStatusBarText(s.value().qstring());
      return;
    }
    case _Location: {
      KHTMLPart* p = Window::retrieveActive(exec)->m_part;
      if (p) {
        QString dstUrl = p->htmlDocument().completeURL(value.toString(exec).string()).string();
        if (dstUrl.find("javascript:", 0, false) || isSafeScript(exec))
        {
          bool userGesture = static_cast<ScriptInterpreter *>(exec->interpreter())->wasRunByUserGesture();
#if APPLE_CHANGES
          // We want a new history item if this JS was called via a user gesture
          m_part->scheduleRedirection(0, dstUrl, !userGesture, userGesture);
#else
          m_part->scheduleRedirection(0,
                                      dstUrl,
                                      false /*don't lock history*/, userGesture);
#endif
        }
      }
      return;
    }
    case Onabort:
      if (isSafeScript(exec))
        setListener(exec, DOM::EventImpl::ABORT_EVENT,value);
      return;
    case Onblur:
      if (isSafeScript(exec))
        setListener(exec, DOM::EventImpl::BLUR_EVENT,value);
      return;
    case Onchange:
      if (isSafeScript(exec))
        setListener(exec, DOM::EventImpl::CHANGE_EVENT,value);
      return;
    case Onclick:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::KHTML_CLICK_EVENT,value);
      return;
    case Ondblclick:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::KHTML_DBLCLICK_EVENT,value);
      return;
    case Ondragdrop:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::KHTML_DRAGDROP_EVENT,value);
      return;
    case Onerror:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::KHTML_ERROR_EVENT,value);
      return;
    case Onfocus:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::FOCUS_EVENT,value);
      return;
    case Onkeydown:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::KHTML_KEYDOWN_EVENT,value);
      return;
    case Onkeypress:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::KHTML_KEYPRESS_EVENT,value);
      return;
    case Onkeyup:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::KHTML_KEYUP_EVENT,value);
      return;
    case Onload:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::LOAD_EVENT,value);
      return;
    case Onmousedown:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::MOUSEDOWN_EVENT,value);
      return;
    case Onmousemove:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::MOUSEMOVE_EVENT,value);
      return;
    case Onmouseout:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::MOUSEOUT_EVENT,value);
      return;
    case Onmouseover:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::MOUSEOVER_EVENT,value);
      return;
    case Onmouseup:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::MOUSEUP_EVENT,value);
      return;
    case Onmove:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::KHTML_MOVE_EVENT,value);
      return;
    case Onreset:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::RESET_EVENT,value);
      return;
    case Onresize:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::RESIZE_EVENT,value);
      return;
    case Onselect:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::SELECT_EVENT,value);
      return;
    case Onsubmit:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::SUBMIT_EVENT,value);
      return;
    case Onunload:
      if (isSafeScript(exec))
        setListener(exec,DOM::EventImpl::UNLOAD_EVENT,value);
      return;
    case Name:
      if (isSafeScript(exec))
#if APPLE_CHANGES
        m_part->setName( value.toString(exec).qstring() );
#else
        m_part->setName( value.toString(exec).qstring().local8Bit().data() );
#endif
      return;
    default:
      break;
    }
  }
  if (isSafeScript(exec)) {
    //kdDebug(6070) << "Window("<<this<<")::put storing " << propertyName.qstring() << endl;
    ObjectImp::put(exec, propertyName, value, attr);
  }
}

bool Window::toBoolean(ExecState *) const
{
  return !m_part.isNull();
}

int Window::installTimeout(const UString &handler, int t, bool singleShot)
{
  return winq->installTimeout(handler, t, singleShot);
}

void Window::clearTimeout(int timerId)
{
  winq->clearTimeout(timerId);
}

#if APPLE_CHANGES
bool Window::hasTimeouts()
{
    return winq->hasTimeouts();
}

QMap<int, ScheduledAction*> *Window::pauseTimeouts(const void *key)
{
    return winq->pauseTimeouts(key);
}

void Window::resumeTimeouts(QMap<int, ScheduledAction*> *sa, const void *key)
{
    return winq->resumeTimeouts(sa, key);
}
#endif

void Window::scheduleClose()
{
  kdDebug(6070) << "Window::scheduleClose window.close() " << m_part << endl;
  Q_ASSERT(winq);
#if APPLE_CHANGES
  KWQ(m_part)->scheduleClose();
#else
  QTimer::singleShot( 0, winq, SLOT( timeoutClose() ) );
#endif
}

static bool shouldLoadAsEmptyDocument(const KURL &url)
{
  return url.protocol().lower() == "about" || url.isEmpty();
}

bool Window::isSafeScript(ExecState *exec) const
{
  if (m_part.isNull()) { // part deleted ? can't grant access
    kdDebug(6070) << "Window::isSafeScript: accessing deleted part !" << endl;
    return false;
  }
  KHTMLPart *activePart = static_cast<KJS::ScriptInterpreter *>( exec->interpreter() )->part();
  if (!activePart) {
    kdDebug(6070) << "Window::isSafeScript: current interpreter's part is 0L!" << endl;
    return false;
  }
  if ( activePart == m_part ) // Not calling from another frame, no problem.
    return true;

  // allow access from the window that opened this one if it made an initially empty document
  if ( ( activePart == m_part->opener() || activePart == m_part->parentPart() ) && 
       shouldLoadAsEmptyDocument(m_part->url()) ) {
    return true;
  }

  DOM::HTMLDocument thisDocument = m_part->htmlDocument();
#if !APPLE_CHANGES
  if ( thisDocument.isNull() ) {
    kdDebug(6070) << "Window::isSafeScript: trying to access an XML document !?" << endl;
    return false;
  }
#else
  // JS may be attempting to access the "window" object, which should be valid,
  // even if the document hasn't been constructed yet.  If the document doesn't
  // exist yet allow JS to access the window object.
  if (thisDocument.isNull())
    return true;
#endif

  DOM::HTMLDocument actDocument = activePart->htmlDocument();

  if ( actDocument.isNull() ) {
    kdDebug(6070) << "Window::isSafeScript: active part has no document!" << endl;
    return false;
  }

  DOM::DOMString actDomain = actDocument.domain();
  
  // Always allow local pages to execute any JS.
  if (actDomain.isNull())
    return true;
  
  DOM::DOMString thisDomain = thisDocument.domain();
  //kdDebug(6070) << "current domain:" << actDomain.string() << ", frame domain:" << thisDomain.string() << endl;
  if ( actDomain == thisDomain )
    return true;

  kdWarning(6070) << "Javascript: access denied for current frame '" << actDomain.string() << "' to frame '" << thisDomain.string() << "'" << endl;
  return false;
}

void Window::setListener(ExecState *exec, int eventId, Value func)
{
  if (!isSafeScript(exec))
    return;
  DOM::DocumentImpl *doc = static_cast<DOM::DocumentImpl*>(m_part->htmlDocument().handle());
  if (!doc)
    return;

  doc->setHTMLWindowEventListener(eventId,getJSEventListener(func,true));
}

Value Window::getListener(ExecState *exec, int eventId) const
{
  if (!isSafeScript(exec))
    return Undefined();
  DOM::DocumentImpl *doc = static_cast<DOM::DocumentImpl*>(m_part->htmlDocument().handle());
  if (!doc)
    return Undefined();

  DOM::EventListener *listener = doc->getHTMLWindowEventListener(eventId);
  if (listener)
    return static_cast<JSEventListener*>(listener)->listenerObj();
  else
    return Null();
}


JSEventListener *Window::getJSEventListener(const Value& val, bool html)
{
  // This function is so hot that it's worth coding it directly with imps.
  if (val.type() != ObjectType)
    return 0;
  ObjectImp *listenerObject = static_cast<ObjectImp *>(val.imp());

  QPtrListIterator<JSEventListener> it(jsEventListeners);
  for (; it.current(); ++it)
    if (it.current()->listenerObjImp() == listenerObject)
      return it.current();

  // Note that the JSEventListener constructor adds it to our jsEventListeners list
  return new JSEventListener(Object(listenerObject), Object(this), html);
}

void Window::clear( ExecState *exec )
{
  kdDebug(6070) << "Window::clear " << this << endl;
  delete winq;
  winq = new WindowQObject(this);;
  // Get rid of everything, those user vars could hold references to DOM nodes
  deleteAllProperties( exec );
  // Really delete those properties, so that the DOM nodes get deref'ed
  KJS::Collector::collect();
  // Now recreate a working global object for the next URL that will use us
  KJS::Interpreter *interpreter = KJSProxy::proxy( m_part )->interpreter();
  interpreter->initGlobalObject();
}

void Window::setCurrentEvent( DOM::Event *evt )
{
  m_evt = evt;
  //kdDebug(6070) << "Window " << this << " (part=" << m_part << ")::setCurrentEvent m_evt=" << evt << endl;
}

Value WindowFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&Window::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  Window *window = static_cast<Window *>(thisObj.imp());
  QString str, str2;

  KHTMLPart *part = window->m_part;
  if (!part)
    return Undefined();

  KHTMLView *widget = part->view();
  Value v = args[0];
  UString s = v.toString(exec);
  str = s.qstring();

  switch (id) {
  case Window::Alert:
    part->xmlDocImpl()->updateRendering();
#if APPLE_CHANGES
    KWQ(part)->runJavaScriptAlert(str);
#else
    KMessageBox::error(widget, QStyleSheet::convertFromPlainText(str), "JavaScript");
#endif
    return Undefined();
  case Window::Confirm:
    part->xmlDocImpl()->updateRendering();
#if APPLE_CHANGES
    return Boolean(KWQ(part)->runJavaScriptConfirm(str));
#else
    return Boolean((KMessageBox::warningYesNo(widget, QStyleSheet::convertFromPlainText(str), "JavaScript",
                                                i18n("OK"), i18n("Cancel")) == KMessageBox::Yes));
#endif
  case Window::Prompt:
    part->xmlDocImpl()->updateRendering();
    bool ok;
#if APPLE_CHANGES
    ok = KWQ(part)->runJavaScriptPrompt(str, args.size() >= 2 ? args[1].toString(exec).qstring() : QString::null, str2);
#else
    if (args.size() >= 2)
      str2 = QInputDialog::getText(i18n("Konqueror: Prompt"),
                                   QStyleSheet::convertFromPlainText(str),
                                   QLineEdit::Normal,
                                   args[1].toString(exec).qstring(), &ok);
    else
      str2 = QInputDialog::getText(i18n("Konqueror: Prompt"),
                                   QStyleSheet::convertFromPlainText(str),
                                   QLineEdit::Normal,
                                   QString::null, &ok);
#endif
    if ( ok )
        return String(str2);
    else
        return Null();
  case Window::Open:
  {
    KConfig *config = new KConfig("konquerorrc");
    config->setGroup("Java/JavaScript Settings");
#if !APPLE_CHANGES
    int policy = config->readUnsignedNumEntry( "WindowOpenPolicy", 0 ); // 0=allow, 1=ask, 2=deny, 3=smart
#else    
    int policy = config->readUnsignedNumEntry( part->settings(), "WindowOpenPolicy", 0 ); // 0=allow, 1=ask, 2=deny, 3=smart
#endif
    delete config;
    if ( policy == 1 ) {
#if !APPLE_CHANGES
      if ( KMessageBox::questionYesNo(widget,
                                      i18n( "This site is trying to open up a new browser "
                                            "window using Javascript.\n"
                                            "Do you want to allow this?" ),
                                      i18n( "Confirmation: Javascript Popup" ) ) == KMessageBox::Yes )
#endif
        policy = 0;
    } else if ( policy == 3 ) // smart
    {
      // window.open disabled unless from a key/mouse event
      if (static_cast<ScriptInterpreter *>(exec->interpreter())->wasRunByUserGesture())
#if !APPLE_CHANGES
        policy = 0;
#else
      {
        policy = 0;
	LOG(PopupBlocking, "Allowed JavaScript window open of %s", args[0].toString(exec).qstring().ascii());
      } else {
	LOG(PopupBlocking, "Blocked JavaScript window open of %s", args[0].toString(exec).qstring().ascii());
      }
#endif
    }

    QString frameName = !args[1].isNull() && args[1].type() != UndefinedType ?
                        args[1].toString(exec).qstring()
                        : QString("_blank");

    if ( policy != 0 && !(part->findFrame(frameName) || frameName == "_top" || frameName == "_parent" || frameName == "_self")) {
      return Undefined();
    } else {
      if (v.type() == UndefinedType)
        str = QString();

      KParts::WindowArgs winargs;

      // scan feature argument
      v = args[2];
      QString features;
      if (!v.isNull() && v.type() != UndefinedType && v.toString(exec).size() > 0) {
        features = v.toString(exec).qstring();
        // specifying window params means false defaults
        winargs.menuBarVisible = false;
        winargs.toolBarsVisible = false;
        winargs.statusBarVisible = false;
#if APPLE_CHANGES
	winargs.scrollbarsVisible = true;
#endif
        QStringList flist = QStringList::split(',', features);
        QStringList::ConstIterator it = flist.begin();
        while (it != flist.end()) {
          QString s = *it++;
          QString key, val;
          int pos = s.find('=');
          if (pos >= 0) {
            key = s.left(pos).stripWhiteSpace().lower();
            val = s.mid(pos + 1).stripWhiteSpace().lower();
	    int spacePos = val.find(' ');
	    if (spacePos != -1) {
	      val = val.left(spacePos);
	    }

            int scnum = QApplication::desktop()->screenNumber(widget->topLevelWidget());

	    QRect screen = QApplication::desktop()->screenGeometry(scnum);
            if (key == "left" || key == "screenx") {
              winargs.x = val.toInt() + screen.x();
	      if (winargs.x < screen.x() || winargs.x > screen.right())
		  winargs.x = screen.x(); // only safe choice until size is determined
#if APPLE_CHANGES
	      winargs.xSet = true;
#endif
            } else if (key == "top" || key == "screeny") {
              winargs.y = val.toInt() + screen.y();
	      if (winargs.y < screen.y() || winargs.y > screen.bottom())
		  winargs.y = screen.y(); // only safe choice until size is determined
#if APPLE_CHANGES
	      winargs.ySet = true;
#endif
            } else if (key == "height") {
              winargs.height = val.toInt() + 2*qApp->style().pixelMetric( QStyle::PM_DefaultFrameWidth ) + 2;
	      if (winargs.height > screen.height())  // should actually check workspace
		  winargs.height = screen.height();
              if (winargs.height < 100)
		  winargs.height = 100;
#if APPLE_CHANGES
	      winargs.heightSet = true;
#endif
            } else if (key == "width") {
              winargs.width = val.toInt() + 2*qApp->style().pixelMetric( QStyle::PM_DefaultFrameWidth ) + 2;
	      if (winargs.width > screen.width())    // should actually check workspace
		  winargs.width = screen.width();
              if (winargs.width < 100)
		  winargs.width = 100;
#if APPLE_CHANGES
	      winargs.widthSet = true;
#endif
            } else {
              goto boolargs;
	    }
            continue;
          } else {
            // leaving away the value gives true
            key = s.stripWhiteSpace().lower();
            val = "1";
          }
        boolargs:
          if (key == "menubar")
            winargs.menuBarVisible = (val == "1" || val == "yes");
          else if (key == "toolbar")
            winargs.toolBarsVisible = (val == "1" || val == "yes");
          else if (key == "location")  // ### missing in WindowArgs
            winargs.toolBarsVisible = (val == "1" || val == "yes");
          else if (key == "status" || key == "statusbar")
            winargs.statusBarVisible = (val == "1" || val == "yes");
          else if (key == "resizable")
            winargs.resizable = (val == "1" || val == "yes");
          else if (key == "fullscreen")
            winargs.fullscreen = (val == "1" || val == "yes");
#if APPLE_CHANGES
          else if (key == "scrollbars")
            winargs.scrollbarsVisible = !(val == "0" || val == "no");
#endif
        }
      }

      // prepare arguments
      KURL url;
      if (!str.isEmpty())
      {
        KHTMLPart* p = Window::retrieveActive(exec)->m_part;
        if ( p )
          url = p->htmlDocument().completeURL(str).string();
      }

      KParts::URLArgs uargs;
      uargs.frameName = frameName;
      if ( uargs.frameName == "_top" )
      {
	  // FIXME: referrer?
          while ( part->parentPart() )
              part = part->parentPart();
          bool userGesture = static_cast<ScriptInterpreter *>(exec->interpreter())->wasRunByUserGesture();
          part->scheduleRedirection(0, url.url(), false/*don't lock history*/, userGesture);
          return Window::retrieve(part);
      }
      if ( uargs.frameName == "_parent" )
      {
	  // FIXME: referrer?
          if ( part->parentPart() )
              part = part->parentPart();
          bool userGesture = static_cast<ScriptInterpreter *>(exec->interpreter())->wasRunByUserGesture();
          part->scheduleRedirection(0, url.url(), false/*don't lock history*/, userGesture);
          return Window::retrieve(part);
      }
      uargs.serviceType = "text/html";

      // request window (new or existing if framename is set)
      KParts::ReadOnlyPart *newPart = 0L;
      emit part->browserExtension()->createNewWindow("", uargs,winargs,newPart);
      if (newPart && newPart->inherits("KHTMLPart")) {
        KHTMLPart *khtmlpart = static_cast<KHTMLPart*>(newPart);
        //qDebug("opener set to %p (this Window's part) in new Window %p  (this Window=%p)",part,win,window);
        khtmlpart->setOpener(part);
        khtmlpart->setOpenedByJS(true);
        if (khtmlpart->document().isNull()) {
          khtmlpart->begin();
          khtmlpart->write("<HTML><BODY>");
          khtmlpart->end();
          if ( part->docImpl() ) {
            kdDebug(6070) << "Setting domain to " << part->docImpl()->domain().string() << endl;
            khtmlpart->docImpl()->setDomain( part->docImpl()->domain(), true );
            khtmlpart->docImpl()->setBaseURL( part->docImpl()->baseURL() );
          }
        }
#if APPLE_CHANGES
        if (!url.isEmpty()) {
          bool userGesture = static_cast<ScriptInterpreter *>(exec->interpreter())->wasRunByUserGesture();
          // FIXME: Need to pass referrer here.
          khtmlpart->scheduleRedirection(0, url.url(), false, userGesture);
	}
#else
        uargs.serviceType = QString::null;
        if (uargs.frameName == "_blank")
          uargs.frameName = QString::null;
        if (!url.isEmpty())
	  // FIXME: need to pass referrer here
          emit khtmlpart->browserExtension()->openURLRequest(url,uargs);
#endif
        return Window::retrieve(khtmlpart); // global object
      } else
        return Undefined();
    }
  }
  case Window::ScrollBy:
    window->updateLayout();
    if(args.size() == 2 && widget)
      widget->scrollBy(args[0].toInt32(exec), args[1].toInt32(exec));
    return Undefined();
  case Window::Scroll:
  case Window::ScrollTo:
    window->updateLayout();
    if(args.size() == 2 && widget)
      widget->setContentsPos(args[0].toInt32(exec), args[1].toInt32(exec));
    return Undefined();
  case Window::MoveBy:
    if(args.size() == 2 && widget)
    {
      QWidget * tl = widget->topLevelWidget();
	  QRect sg = QApplication::desktop()->screenGeometry(QApplication::desktop()->screenNumber(tl));
      QPoint dest = tl->pos() + QPoint( args[0].toInt32(exec), args[1].toInt32(exec) );
      // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
      if ( dest.x() >= sg.x() && dest.y() >= sg.x() &&
           dest.x()+tl->width() <= sg.width()+sg.x() &&
           dest.y()+tl->height() <= sg.height()+sg.y() )
        tl->move( dest );
    }
    return Undefined();
  case Window::MoveTo:
    if(args.size() == 2 && widget)
    {
      QWidget * tl = widget->topLevelWidget();
	  QRect sg = QApplication::desktop()->screenGeometry(QApplication::desktop()->screenNumber(tl));
      QPoint dest( args[0].toInt32(exec)+sg.x(), args[1].toInt32(exec)+sg.y() );
      // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
      if ( dest.x() >= sg.x() && dest.y() >= sg.y() &&
           dest.x()+tl->width() <= sg.width()+sg.x() &&
           dest.y()+tl->height() <= sg.height()+sg.y() )
        tl->move( dest );
    }
    return Undefined();
  case Window::ResizeBy:
    if(args.size() == 2 && widget)
    {
      QWidget * tl = widget->topLevelWidget();
      QSize dest = tl->size() + QSize( args[0].toInt32(exec), args[1].toInt32(exec) );
	  QRect sg = QApplication::desktop()->screenGeometry(QApplication::desktop()->screenNumber(tl));
      // Security check: within desktop limits and bigger than 100x100 (per spec)
      if ( tl->x()+dest.width() <= sg.x()+sg.width() &&
           tl->y()+dest.height() <= sg.y()+sg.height() &&
           dest.width() >= 100 && dest.height() >= 100 )
      {
        // Take into account the window frame
        int deltaWidth = tl->frameGeometry().width() - tl->width();
        int deltaHeight = tl->frameGeometry().height() - tl->height();
        tl->resize( dest.width() - deltaWidth, dest.height() - deltaHeight );
      }
    }
    return Undefined();
  case Window::ResizeTo:
    if(args.size() == 2 && widget)
    {
      QWidget * tl = widget->topLevelWidget();
      QSize dest = QSize( args[0].toInt32(exec), args[1].toInt32(exec) );
	  QRect sg = QApplication::desktop()->screenGeometry(QApplication::desktop()->screenNumber(tl));
      // Security check: within desktop limits and bigger than 100x100 (per spec)
      if ( tl->x()+dest.width() <= sg.x()+sg.width() &&
           tl->y()+dest.height() <= sg.y()+sg.height() &&
           dest.width() >= 100 && dest.height() >= 100 )
      {
        // Take into account the window frame
        int deltaWidth = tl->frameGeometry().width() - tl->width();
        int deltaHeight = tl->frameGeometry().height() - tl->height();
        tl->resize( dest.width() - deltaWidth, dest.height() - deltaHeight );
      }
    }
    return Undefined();
  case Window::SetTimeout:
    if (!window->isSafeScript(exec))
        return Undefined();
    if (args.size() == 2 && v.isA(StringType)) {
      int i = args[1].toInt32(exec);
      int r = (const_cast<Window*>(window))->installTimeout(s, i, true /*single shot*/);
      return Number(r);
    }
    else if (args.size() >= 2 && v.isA(ObjectType) && Object::dynamicCast(v).implementsCall()) {
      Value func = args[0];
      int i = args[1].toInt32(exec);
#if 0
//  ### TODO
      List *funcArgs = args.copy();
      funcArgs->removeFirst(); // all args after 2 go to the function
      funcArgs->removeFirst();
#endif
      int r = (const_cast<Window*>(window))->installTimeout(s, i, true /*single shot*/);
      return Number(r);
    }
    else
      return Undefined();
  case Window::SetInterval:
    if (!window->isSafeScript(exec))
        return Undefined();
    if (args.size() >= 2 && v.isA(StringType)) {
      int i = args[1].toInt32(exec);
      int r = (const_cast<Window*>(window))->installTimeout(s, i, false);
      return Number(r);
    }
    else if (args.size() >= 2 && !Object::dynamicCast(v).isNull() &&
	     Object::dynamicCast(v).implementsCall()) {
      Value func = args[0];
      int i = args[1].toInt32(exec);
#if 0
// ### TODO
      List *funcArgs = args.copy();
      funcArgs->removeFirst(); // all args after 2 go to the function
      funcArgs->removeFirst();
#endif
      int r = (const_cast<Window*>(window))->installTimeout(s, i, false);
      return Number(r);
    }
    else
      return Undefined();
  case Window::ClearTimeout:
  case Window::ClearInterval:
    if (!window->isSafeScript(exec))
        return Undefined();
    (const_cast<Window*>(window))->clearTimeout(v.toInt32(exec));
    return Undefined();
  case Window::Focus:
    if (widget)
      widget->setActiveWindow();
    return Undefined();
  case Window::GetSelection:
    if (!window->isSafeScript(exec))
        return Undefined();
    return String(part->selectedText());
  case Window::Blur:
#if APPLE_CHANGES
    KWQ(part)->unfocusWindow();
#else
    // TODO
#endif
    return Undefined();
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
    if (!part->openedByJS())
    {
      // To conform to the SPEC, we only ask if the window
      // has more than one entry in the history (NS does that too).
      History history(exec,part);
      if ( history.get( exec, lengthPropertyName ).toInt32(exec) <= 1
#if APPLE_CHANGES
           // FIXME: How are we going to handle this?
#else
           || KMessageBox::questionYesNo( window->part()->view(), i18n("Close window?"), i18n("Confirmation Required") ) == KMessageBox::Yes
#endif
           )
        (const_cast<Window*>(window))->scheduleClose();
    }
    else
    {
      (const_cast<Window*>(window))->scheduleClose();
    }
    return Undefined();
  case Window::CaptureEvents:
  case Window::ReleaseEvents:
        // If anyone implements these, they need the safescript security check.
        if (!window->isSafeScript(exec))
	    return Undefined();

    // Do nothing for now. These are NS-specific legacy calls.
    break;
  case Window::AddEventListener: {
        if (!window->isSafeScript(exec))
	    return Undefined();
	
        JSEventListener *listener = Window::retrieveActive(exec)->getJSEventListener(args[1]);
        if (listener) {
	    DOM::DocumentImpl* docimpl = static_cast<DOM::DocumentImpl *>(part->document().handle());
            docimpl->addWindowEventListener(DOM::EventImpl::typeToId(args[0].toString(exec).string()),listener,args[2].toBoolean(exec));
        }
        return Undefined();
    }
  case Window::RemoveEventListener: {
        if (!window->isSafeScript(exec))
	    return Undefined();
        JSEventListener *listener = Window::retrieveActive(exec)->getJSEventListener(args[1]);
        if (listener) {
	    DOM::DocumentImpl* docimpl = static_cast<DOM::DocumentImpl *>(part->document().handle());
            docimpl->removeWindowEventListener(DOM::EventImpl::typeToId(args[0].toString(exec).string()),listener,args[2].toBoolean(exec));
        }
        return Undefined();
    }

  }
  return Undefined();
}

void Window::updateLayout() const
{
  DOM::DocumentImpl* docimpl = static_cast<DOM::DocumentImpl *>(m_part->document().handle());
  if (docimpl) {
    docimpl->updateLayout();
  }
}


////////////////////// ScheduledAction ////////////////////////

ScheduledAction::ScheduledAction(Object _func, List _args, bool _singleShot)
{
  //kdDebug(6070) << "ScheduledAction::ScheduledAction(isFunction) " << this << endl;
  func = _func;
  args = _args;
  isFunction = true;
  singleShot = _singleShot;
}

ScheduledAction::ScheduledAction(const QString &_code, bool _singleShot)
{
  //kdDebug(6070) << "ScheduledAction::ScheduledAction(!isFunction) " << this << endl;
  //func = 0;
  //args = 0;
  code = _code;
  isFunction = false;
  singleShot = _singleShot;
}

void ScheduledAction::execute(Window *window)
{
  ScriptInterpreter *interpreter = static_cast<ScriptInterpreter *>(KJSProxy::proxy(window->m_part)->interpreter());

  interpreter->setProcessingTimerCallback(true);

  //kdDebug(6070) << "ScheduledAction::execute " << this << endl;
  if (isFunction) {
    if (func.implementsCall()) {
      // #### check this
      Q_ASSERT( window->m_part );
      if ( window->m_part )
      {
        KJS::Interpreter *interpreter = KJSProxy::proxy( window->m_part )->interpreter();
        ExecState *exec = interpreter->globalExec();
        Q_ASSERT( window == interpreter->globalObject().imp() );
        Object obj( window );
        func.call(exec,obj,args); // note that call() creates its own execution state for the func call
	if ( exec->hadException() ) {
#if APPLE_CHANGES
	  if (Interpreter::shouldPrintExceptions()) {
	    char *message = exec->exception().toObject(exec).get(exec, messagePropertyName).toString(exec).ascii();
	    printf("(timer):%s\n", message);
	  }
#endif
	  exec->clearException();
	}
      }
    }
  }
  else {
    window->m_part->executeScript(code);
  }

  interpreter->setProcessingTimerCallback(false);
}

ScheduledAction::~ScheduledAction()
{
  //kdDebug(6070) << "ScheduledAction::~ScheduledAction " << this << endl;
}

////////////////////// WindowQObject ////////////////////////

WindowQObject::WindowQObject(Window *w)
  : parent(w)
{
  //kdDebug(6070) << "WindowQObject::WindowQObject " << this << endl;
  part = parent->m_part;
  connect( parent->m_part, SIGNAL( destroyed() ),
           this, SLOT( parentDestroyed() ) );
}

WindowQObject::~WindowQObject()
{
  //kdDebug(6070) << "WindowQObject::~WindowQObject " << this << endl;
  parentDestroyed(); // reuse same code
}

void WindowQObject::parentDestroyed()
{
  //kdDebug(6070) << "WindowQObject::parentDestroyed " << this << " we have " << scheduledActions.count() << " actions in the map" << endl;
  killTimers();
  QMapIterator<int,ScheduledAction*> it;
  for (it = scheduledActions.begin(); it != scheduledActions.end(); ++it) {
    ScheduledAction *action = *it;
    //kdDebug(6070) << "WindowQObject::parentDestroyed deleting action " << action << endl;
    delete action;
  }
  scheduledActions.clear();
}

int WindowQObject::installTimeout(const UString &handler, int t, bool singleShot)
{
  //kdDebug(6070) << "WindowQObject::installTimeout " << this << " " << handler.ascii() << endl;
  int id = startTimer(t);
  ScheduledAction *action = new ScheduledAction(handler.qstring(),singleShot);
  scheduledActions.insert(id, action);
  //kdDebug(6070) << this << " got id=" << id << " action=" << action << " - now having " << scheduledActions.count() << " actions"<<endl;
  return id;
}

int WindowQObject::installTimeout(const Value &func, List args, int t, bool singleShot)
{
  Object objFunc = Object::dynamicCast( func );
  int id = startTimer(t);
  scheduledActions.insert(id, new ScheduledAction(objFunc,args,singleShot));
  return id;
}

QMap<int, ScheduledAction*> *WindowQObject::pauseTimeouts(const void *key)
{
    QMapIterator<int,ScheduledAction*> it;

    QMap<int, KJS::ScheduledAction*>*pausedActions = new QMap<int, KJS::ScheduledAction*>;
    for (it = scheduledActions.begin(); it != scheduledActions.end(); ++it) {
        int timerId = it.key();
        pauseTimer (timerId, key);
        pausedActions->insert(timerId, it.data());
    }
    scheduledActions.clear();
    return pausedActions;
}

void WindowQObject::resumeTimeouts(QMap<int, ScheduledAction*> *sa, const void *key)
{
    QMapIterator<int,ScheduledAction*> it;
    for (it = sa->begin(); it != sa->end(); ++it) {
        int timerId = it.key();
        scheduledActions.insert(timerId, it.data());
    }
    sa->clear();
    resumeTimers (key, this);
}

void WindowQObject::clearTimeout(int timerId, bool delAction)
{
  //kdDebug(6070) << "WindowQObject::clearTimeout " << this << " timerId=" << timerId << " delAction=" << delAction << endl;
  killTimer(timerId);
  if (delAction) {
    QMapIterator<int,ScheduledAction*> it = scheduledActions.find(timerId);
    if (it != scheduledActions.end()) {
      ScheduledAction *action = *it;
      scheduledActions.remove(it);
      delete action;
    }
  }
}

void WindowQObject::timerEvent(QTimerEvent *e)
{
  QMapIterator<int,ScheduledAction*> it = scheduledActions.find(e->timerId());
  if (it != scheduledActions.end()) {
    ScheduledAction *action = *it;
    bool singleShot = action->singleShot;
    //kdDebug(6070) << "WindowQObject::timerEvent " << this << " action=" << action << " singleShot:" << singleShot << endl;

    // remove single shots installed by setTimeout()
    if (singleShot)
    {
      clearTimeout(e->timerId(),false);
      scheduledActions.remove(it);
    }
        
    if (!parent->part().isNull())
      action->execute(parent);

    // It is important to test singleShot and not action->singleShot here - the
    // action could have been deleted already if not single shot and if the
    // JS code called by execute() calls clearTimeout().
    if (singleShot)
      delete action;
  } else
    kdWarning(6070) << "WindowQObject::timerEvent this=" << this << " timer " << e->timerId()
                    << " not found (" << scheduledActions.count() << " actions in map)" << endl;
}

void WindowQObject::timeoutClose()
{
  if (!parent->part().isNull())
  {
    //kdDebug(6070) << "WindowQObject::timeoutClose -> closing window" << endl;
    delete parent->m_part;
  }
}

#if APPLE_CHANGES
bool WindowQObject::hasTimeouts()
{
    return scheduledActions.count();
}
#endif

Value FrameArray::get(ExecState *exec, const Identifier &p) const
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "FrameArray::get " << p.qstring() << " part=" << (void*)part << endl;
#endif
  if (part.isNull())
    return Undefined();

  QPtrList<KParts::ReadOnlyPart> frames = part->frames();
  int len = frames.count();
  if (p == lengthPropertyName)
    return Number(len);
  else if (p== "location") // non-standard property, but works in NS and IE
  {
    Object obj = Object::dynamicCast( Window::retrieve( part ) );
    if ( !obj.isNull() )
      return obj.get( exec, "location" );
    return Undefined();
  }

  // check for the name or number
  KParts::ReadOnlyPart *frame = part->findFrame(p.qstring());
  if (!frame) {
    int i = (int)p.toDouble();
    if (i >= 0 && i < len){
      frame = frames.at(i);
    }
  }

  // we are potentially fetching a reference to a another Window object here.
  // i.e. we may be accessing objects from another interpreter instance.
  // Therefore we have to be a bit careful with memory managment.
  if (frame && frame->inherits("KHTMLPart")) {
    KHTMLPart *khtml = static_cast<KHTMLPart*>(frame);
    return Window::retrieve(khtml);
  }

  return ObjectImp::get(exec, p);
}

UString FrameArray::toString(ExecState *) const
{
  return "[object FrameArray]";
}

////////////////////// Location Object ////////////////////////

const ClassInfo Location::info = { "Location", 0, 0, 0 };
/*
@begin LocationTable 11
  hash		Location::Hash		DontDelete
  host		Location::Host		DontDelete
  hostname	Location::Hostname	DontDelete
  href		Location::Href		DontDelete
  pathname	Location::Pathname	DontDelete
  port		Location::Port		DontDelete
  protocol	Location::Protocol	DontDelete
  search	Location::Search	DontDelete
  [[==]]	Location::EqualEqual	DontDelete|ReadOnly
  toString	Location::ToString	DontDelete|Function 0
  replace	Location::Replace	DontDelete|Function 1
  reload	Location::Reload	DontDelete|Function 0
@end
*/
IMPLEMENT_PROTOFUNC(LocationFunc)
Location::Location(KHTMLPart *p) : m_part(p)
{
  //kdDebug(6070) << "Location::Location " << this << " m_part=" << (void*)m_part << endl;
}

Location::~Location()
{
  //kdDebug(6070) << "Location::~Location " << this << " m_part=" << (void*)m_part << endl;
}

Value Location::get(ExecState *exec, const Identifier &p) const
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "Location::get " << p.qstring() << " m_part=" << (void*)m_part << endl;
#endif

  if (m_part.isNull())
    return Undefined();

  KURL url = m_part->url();
  const HashEntry *entry = Lookup::findEntry(&LocationTable, p);
  if (entry)
    switch (entry->value) {
    case Hash:
      return String( url.ref().isNull() ? QString("") : "#" + url.ref() );
    case Host: {
      UString str = url.host();
      if (url.port())
        str += ":" + QString::number((int)url.port());
      return String(str);
      // Note: this is the IE spec. The NS spec swaps the two, it says
      // "The hostname property is the concatenation of the host and port properties, separated by a colon."
      // Bleh.
    }
    case Hostname:
      return String( url.host() );
    case Href:
      if (!url.hasPath())
        return String( url.prettyURL()+"/" );
      else
        return String( url.prettyURL() );
    case Pathname:
      return String( url.path().isEmpty() ? QString("/") : url.path() );
    case Port:
      return String( url.port() ? QString::number((int)url.port()) : QString::fromLatin1("") );
    case Protocol:
      return String( url.protocol()+":" );
    case Search:
      return String( url.query() );
    case EqualEqual: // [[==]]
      return String(toString(exec));
    case ToString:
      return lookupOrCreateFunction<LocationFunc>(exec,p,this,entry->value,entry->params,entry->attr);
    }
  // Look for overrides
  ValueImp * val = ObjectImp::getDirect(p);
  if (val)
    return Value(val);
  if (entry)
    switch (entry->value) {
    case Replace:
      return lookupOrCreateFunction<LocationFunc>(exec,p,this,entry->value,entry->params,entry->attr);
    case Reload:
      return lookupOrCreateFunction<LocationFunc>(exec,p,this,entry->value,entry->params,entry->attr);
    }

  return Undefined();
}

void Location::put(ExecState *exec, const Identifier &p, const Value &v, int attr)
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "Location::put " << p.qstring() << " m_part=" << (void*)m_part << endl;
#endif
  if (m_part.isNull())
    return;

  QString str = v.toString(exec).qstring();
  KURL url = m_part->url();
  const HashEntry *entry = Lookup::findEntry(&LocationTable, p);
  if (entry)
    switch (entry->value) {
    case Href: {
      KHTMLPart* p = Window::retrieveActive(exec)->part();
      if ( p )
        url = p->htmlDocument().completeURL( str ).string();
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
    ObjectImp::put(exec, p, v, attr);
    return;
  }
  bool userGesture = static_cast<ScriptInterpreter *>(exec->interpreter())->wasRunByUserGesture();
#if APPLE_CHANGES
  // We want a new history item if this JS was called via a user gesture
  m_part->scheduleRedirection(0, url.url(), !userGesture, userGesture);
#else
  m_part->scheduleRedirection(0, url.url(), false /*don't lock history*/, userGesture);
#endif
}

Value Location::toPrimitive(ExecState *exec, Type) const
{
  return String(toString(exec));
}

UString Location::toString(ExecState *) const
{
  if (!m_part->url().hasPath())
    return m_part->url().prettyURL()+"/";
  else
    return m_part->url().prettyURL();
}

Value LocationFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&Location::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  Location *location = static_cast<Location *>(thisObj.imp());
  KHTMLPart *part = location->part();
  if (part) {
    switch (id) {
    case Location::Replace:
    {
      QString str = args[0].toString(exec).qstring();
      KHTMLPart* p = Window::retrieveActive(exec)->part();
      if ( p ) {
	bool userGesture = static_cast<ScriptInterpreter *>(exec->interpreter())->wasRunByUserGesture();
        part->scheduleRedirection(0, p->htmlDocument().completeURL(str).string(), true /*lock history*/, userGesture);
      }
      break;
    }
    case Location::Reload:
    {
      bool userGesture = static_cast<ScriptInterpreter *>(exec->interpreter())->wasRunByUserGesture();
      part->scheduleRedirection(0, part->url().url(), true/*lock history*/, userGesture);
      break;
    }
    case Location::ToString:
      return String(location->toString(exec));
    }
  } else
    kdDebug(6070) << "LocationFunc::tryExecute - no part!" << endl;
  return Undefined();
}

////////////////////// History Object ////////////////////////

const ClassInfo History::info = { "History", 0, 0, 0 };
/*
@begin HistoryTable 4
  length	History::Length		DontDelete|ReadOnly
  back		History::Back		DontDelete|Function 0
  forward	History::Forward	DontDelete|Function 0
  go		History::Go		DontDelete|Function 1
@end
*/
IMPLEMENT_PROTOFUNC(HistoryFunc)

Value History::get(ExecState *exec, const Identifier &p) const
{
  return lookupGet<HistoryFunc,History,ObjectImp>(exec,p,&HistoryTable,this);
}

Value History::getValueProperty(ExecState *, int token) const
{
  switch (token) {
  case Length:
  {
    KParts::BrowserExtension *ext = part->browserExtension();
    if ( !ext )
      return Number( 0 );

    KParts::BrowserInterface *iface = ext->browserInterface();
    if ( !iface )
      return Number( 0 );

    QVariant length = iface->property( "historyLength" );

    if ( length.type() != QVariant::UInt )
      return Number( 0 );

    return Number( length.toUInt() );
  }
  default:
    kdWarning() << "Unhandled token in History::getValueProperty : " << token << endl;
    return Undefined();
  }
}

UString History::toString(ExecState *exec) const
{
  return "[object History]";
}

Value HistoryFunc::tryCall(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&History::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }
  History *history = static_cast<History *>(thisObj.imp());

  Value v = args[0];
  Number n;
  if(!v.isNull())
    n = v.toInteger(exec);

  int steps;
  switch (id) {
  case History::Back:
    steps = -1;
    break;
  case History::Forward:
    steps = 1;
    break;
  case History::Go:
    steps = n.intValue();
    break;
  default:
    return Undefined();
  }

  history->part->scheduleHistoryNavigation(steps);
  return Undefined();
}

/////////////////////////////////////////////////////////////////////////////

#ifdef Q_WS_QWS

const ClassInfo Konqueror::info = { "Konqueror", 0, 0, 0 };

bool Konqueror::hasProperty(ExecState *exec, const Identifier &p) const
{
  if ( p.qstring().startsWith( "goHistory" ) ) return false;

  return true;
}

Value Konqueror::get(ExecState *exec, const Identifier &p) const
{
  if ( p == "goHistory" || part->url().protocol() != "http" || part->url().host() != "localhost" )
    return Undefined();

  KParts::BrowserExtension *ext = part->browserExtension();
  if ( ext ) {
    KParts::BrowserInterface *iface = ext->browserInterface();
    if ( iface ) {
      QVariant prop = iface->property( p.qstring().latin1() );

      if ( prop.isValid() ) {
        switch( prop.type() ) {
        case QVariant::Int:
          return Number( prop.toInt() );
        case QVariant::String:
          return String( prop.toString() );
        default:
          break;
        }
      }
    }
  }

  return /*Function*/( new KonquerorFunc(this, p.qstring().latin1() ) );
}

Value KonquerorFunc::tryCall(ExecState *exec, Object &, const List &args)
{
  KParts::BrowserExtension *ext = konqueror->part->browserExtension();

  if(!ext)
    return Undefined();

  KParts::BrowserInterface *iface = ext->browserInterface();

  if ( !iface )
    return Undefined();

  QCString n = m_name.data();
  n += "()";
  iface->callMethod( n.data(), QVariant() );

  return Undefined();
}

UString Konqueror::toString(ExecState *) const
{
  return UString("[object Konqueror]");
}

#endif
/////////////////////////////////////////////////////////////////////////////

#include "kjs_window.moc"

