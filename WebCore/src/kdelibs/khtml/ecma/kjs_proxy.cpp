/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
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

#include "kjs_proxy.h"

#include <kjs/kjs.h>
#include <kjs/object.h>
#include <kjs/function.h>

#include <khtml_part.h>
#include <html_element.h>
#include <html_head.h>
#include <html_inline.h>
#include <html_image.h>

#include "kjs_binding.h"
#include "kjs_dom.h"
#include "kjs_html.h"
#include "kjs_window.h"
#include "kjs_navigator.h"
#include "kjs_debugwin.h"
#include "kjs_events.h"

using namespace KJS;

extern "C" {
  KJSProxy *kjs_html_init(KHTMLPart *khtmlpart);
}

#ifdef KJS_DEBUGGER
static KJSDebugWin *kjs_html_debugger = 0;
#endif

QVariant KJS::KJSOToVariant(KJSO obj) {
  QVariant res;
  switch (obj.type()) {
  case BooleanType:
    res = QVariant(obj.toBoolean().value(), 0);
    break;
  case NumberType:
    res = QVariant(obj.toNumber().value());
    break;
  case StringType:
    res = QVariant(obj.toString().value().qstring());
    break;
  default:
    // everything else will be 'invalid'
    break;
  }
  return res;
}


// initialize HTML module
KJSProxy *kjs_html_init(KHTMLPart *khtmlpart)
{
  KJScript *script = kjs_create(khtmlpart);

  // proxy class operating via callback functions
  KJSProxy *proxy = new KJSProxy(script, &kjs_create, &kjs_eval,
                                 &kjs_clear, &kjs_special, &kjs_destroy,
				 &kjs_createHTMLEventHandler);
  proxy->khtmlpart = khtmlpart;

#ifdef KJS_DEBUGGER
  // ### share and destroy
  if (!kjs_html_debugger)
      kjs_html_debugger = new KJSDebugWin();
#endif

  return proxy;
}

// init the interpreter
  KJScript* kjs_create(KHTMLPart *khtmlpart)
  {
    // Creating an interpreter doesn't mean it should be made current !
    KJScript *current = KJScript::current();

    KJScript *script = new KJScript();
#ifndef NDEBUG
    script->enableDebug();
#endif
    KJS::Imp *global = script->globalObject();

    global->setPrototype(new Window(khtmlpart));

    KJScript::setCurrent(current);
    return script;
  }

  // evaluate code. Returns the JS return value or an invalid QVariant
  // if there was none, an error occured or the type couldn't be converted.
  QVariant kjs_eval(KJScript *script, const QChar *c, unsigned int len,
		    const DOM::Node &n, KHTMLPart *khtmlpart)
  {
    script->init(); // set a valid current interpreter

#ifdef KJS_DEBUGGER
    kjs_html_debugger->attach(script);
    kjs_html_debugger->setCode(QString(c, len));
    kjs_html_debugger->setMode(KJS::Debugger::Step);
#endif

    KJS::KJSO thisNode = n.isNull() ? KJSO( Window::retrieve( khtmlpart ) ) : getDOMNode(n);

    KJS::Global::current().setExtra(khtmlpart);
    bool ret = script->evaluate(thisNode, c, len);
    if (script->recursion() == 0)
      KJS::Global::current().setExtra(0L);

#ifdef KJS_DEBUGGER
    kjs_html_debugger->setCode(QString::null);
#endif

    // let's try to convert the return value
    if (ret && script->returnValue())
      return KJSOToVariant(script->returnValue());
    else
      return QVariant();
  }
  // clear resources allocated by the interpreter
  void kjs_clear(KJScript *script, KHTMLPart * part)
  {
    script->clear();
    Window *win = Window::retrieveWindow(part);
    if (win)
        win->clear();
    //    delete script;
  }
  // for later extensions.
  const char *kjs_special(KJScript *, const char *)
  {
    // return something like a version number for now
    return "1";
  }
  void kjs_destroy(KJScript *script)
  {
    delete script;
  }

  DOM::EventListener* kjs_createHTMLEventHandler(KJScript *script, QString code, KHTMLPart *part)
  {
    script->init(); // set a valid current interpreter
    KJS::Constructor constr(KJS::Global::current().get("Function").imp());
    KJS::List args;
    args.append(KJS::String("event"));
    args.append(KJS::String(code));
    KJS::KJSO handlerFunc = constr.construct(args);
    return KJS::Window::retrieveWindow(part)->getJSEventListener(handlerFunc,true);
  }




