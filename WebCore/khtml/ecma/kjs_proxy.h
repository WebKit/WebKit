/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999 Harri Porten (porten@kde.org)
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

#ifndef _KJS_PROXY_H_
#define _KJS_PROXY_H_

#include <qvariant.h>
#include <qstring.h>

class KJScript;
class KHTMLPart;

namespace DOM {
  class Node;
  class EventListener;
};

namespace KJS {
  class KJSO;
  class List;
}

// callback functions for KJSProxy
typedef KJScript* (KJSCreateFunc)(KHTMLPart *);
typedef QVariant (KJSEvalFunc)(KJScript *script, const QChar *, unsigned int,
			   const DOM::Node &, KHTMLPart *);
typedef void (KJSClearFunc)(KJScript *script, KHTMLPart *part);
typedef const char* (KJSSpecialFunc)(KJScript *script, const char *);
typedef void (KJSDestroyFunc)(KJScript *script);
typedef DOM::EventListener* (KJSCreateHTMLEventHandlerFunc)(KJScript *script, QString code, KHTMLPart *part);
extern "C" {
  KJSCreateFunc kjs_create;
  KJSEvalFunc kjs_eval;
  KJSClearFunc kjs_clear;
  KJSSpecialFunc kjs_special;
  KJSDestroyFunc kjs_destroy;
  KJSCreateHTMLEventHandlerFunc kjs_createHTMLEventHandler;
}

/**
 * @short Proxy class serving as interface when being dlopen'ed.
 */
class KJSProxy {
public:
  KJSProxy(KJScript *s, KJSCreateFunc cr, KJSEvalFunc e,
           KJSClearFunc c, KJSSpecialFunc sp, KJSDestroyFunc d,
	   KJSCreateHTMLEventHandlerFunc cheh)
    : create(cr), script(s), eval(e), clr(c), spec(sp), destr(d), 
      createHTMLEH(cheh), inEvaluate(false) { }
  ~KJSProxy() { (*destr)(script); }
  QVariant evaluate(const QChar *c, unsigned int l, const DOM::Node &n);
  const char *special(const char *c);
  void clear();
  DOM::EventListener *createHTMLEventHandler(QString code);
  KHTMLPart *khtmlpart;
  KJScript *jScript();
private:
  KJSCreateFunc *create;
  KJScript *script;
  KJSEvalFunc *eval;
  KJSClearFunc *clr;
  KJSSpecialFunc *spec;
  KJSDestroyFunc *destr;
  KJSCreateHTMLEventHandlerFunc *createHTMLEH;
  bool inEvaluate;
};

inline QVariant KJSProxy::evaluate(const QChar *c, unsigned int l,
			       const DOM::Node &n) {
  if (!script)
    script = (*create)(khtmlpart);
  QVariant r;
  if (inEvaluate)
    r = (*eval)(script, c, l, n, khtmlpart);
  else {
    inEvaluate = true;
    r = (*eval)(script, c, l, n, khtmlpart);
    inEvaluate = false;
  }
  return r;
}

inline const char *KJSProxy::special(const char *c) {
  return (script ? (*spec)(script, c) : "");
}

inline void KJSProxy::clear() {
  if (script) {
    (*clr)(script,khtmlpart);
    script = 0L;
  }
}

inline DOM::EventListener *KJSProxy::createHTMLEventHandler(QString code)
{
  if (!script)
    script = (*create)(khtmlpart);
  return (*createHTMLEH)(script,code,khtmlpart);
}

inline KJScript *KJSProxy::jScript()
{
  if (!script)
    script = (*create)(khtmlpart);
  return script;
}

#endif
