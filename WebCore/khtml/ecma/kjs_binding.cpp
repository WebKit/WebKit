// -*- c-basic-offset: 2 -*-
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

#include <qptrdict.h>
#include <kdebug.h>

#include <kjs/kjs.h>
#include <kjs/object.h>
#include <kjs/function.h>
#include <kjs/operations.h>

#include <khtml_part.h>
#include <html_element.h>
#include <html_head.h>
#include <html_inline.h>
#include <html_image.h>
#include <dom_string.h>
#include <dom_exception.h>
#include <html_misc.h>
#include <css_stylesheet.h>
#include <dom2_events.h>
#include <dom2_range.h>

#include "kjs_binding.h"
#include "kjs_dom.h"
#include "kjs_html.h"
#include "kjs_text.h"
#include "kjs_window.h"
#include "kjs_navigator.h"

using namespace KJS;

/* TODO:
 * The catch all (...) clauses below shouldn't be necessary.
 * But they helped to view for example www.faz.net in an stable manner.
 * Those unknown exceptions should be treated as severe bugs and be fixed.
 *
 * these may be CSS exceptions - need to check - pmk
 */

KJSO DOMObject::get(const UString &p) const
{
  KJSO result;
  try {
    result = tryGet(p);
  }
  catch (DOM::DOMException e) {
    result = Undefined();
  }
  catch (...) {
    kdError(6070) << "Unknown exception in DOMObject::get()" << endl;
    result = String("Unknown exception");
  }

  return result;
}

void DOMObject::put(const UString &p, const KJSO& v)
{
  try {
    tryPut(p,v);
  }
  catch (DOM::DOMException e) {
  }
  catch (...) {
    kdError(6070) << "Unknown exception in DOMObject::put()" << endl;
  }
}

// should rather overload HostImp::toString() this way
String DOMObject::toString() const
{
  return String("[object " + UString(typeInfo()->name) + "]");
}

KJSO DOMFunction::get(const UString &p) const
{
  KJSO result;
  try {
    result = tryGet(p);
  }
  catch (DOM::DOMException e) {
    result = Undefined();
  }
  catch (...) {
    kdError(6070) << "Unknown exception in DOMFunction::get()" << endl;
    result = String("Unknown exception");
  }

  return result;
}

Completion DOMFunction::execute(const List &args)
{
  Completion completion;
  try {
    completion = tryExecute(args);
  }
  // pity there's no way to distinguish between these in JS code
  catch (DOM::DOMException e) {
    KJSO v = Error::create(GeneralError);
    v.put("code", Number(e.code));
    completion = Completion(Throw, v);
  }
  catch (DOM::RangeException e) {
    KJSO v = Error::create(GeneralError);
    v.put("code", Number(e.code));
    completion = Completion(Throw, v);
  }
  catch (DOM::CSSException e) {
    KJSO v = Error::create(GeneralError);
    v.put("code", Number(e.code));
    completion = Completion(Throw, v);
  }
  catch (DOM::EventException e) {
    KJSO v = Error::create(GeneralError);
    v.put("code", Number(e.code));
    completion = Completion(Throw, v);
  }
  catch (...) {
    kdError(6070) << "Unknown exception in DOMFunction::execute()" << endl;
    KJSO v = Error::create(GeneralError, "Unknown exception");
    completion = Completion(Throw, v);
  }
  return completion;
}

UString::UString(const QString &d)
{
  unsigned int len = d.length();
  UChar *dat = new UChar[len];
  memcpy(dat, d.unicode(), len * sizeof(UChar));
  rep = UString::Rep::create(dat, len);
}

UString::UString(const DOM::DOMString &d)
{
  if (d.isNull()) {
    attach(&Rep::null);
    return;
  }

  unsigned int len = d.length();
  UChar *dat = new UChar[len];
  memcpy(dat, d.unicode(), len * sizeof(UChar));
  rep = UString::Rep::create(dat, len);
}

DOM::DOMString UString::string() const
{
  return DOM::DOMString((QChar*) data(), size());
}

QString UString::qstring() const
{
  return QString((QChar*) data(), size());
}

QConstString UString::qconststring() const
{
  return QConstString((QChar*) data(), size());
}

DOM::Node KJS::toNode(const KJSO& obj)
{
  if (!obj.derivedFrom("Node"))
    return DOM::Node();

  const DOMNode *dobj = static_cast<const DOMNode*>(obj.imp());
  return dobj->toNode();
}

KJSO KJS::getString(DOM::DOMString s)
{
  if (s.isNull())
    return Null();
  else
    return String(s);
}

bool KJS::originCheck(const KURL &kurl1, const KURL &kurl2)
{
  if (kurl1.protocol() == kurl2.protocol() &&
      kurl1.host() == kurl2.host() &&
      kurl1.port() == kurl2.port() &&
      kurl1.user() == kurl2.user() &&
      kurl1.pass() == kurl2.pass())
    return true;
  else
    return false;
}
