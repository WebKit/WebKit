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

#ifdef KJS_DEBUGGER

#include "debugger.h"
#include "kjs.h"
#include "internal.h"
#include "ustring.h"

using namespace KJS;

Debugger::Debugger(KJScript *engine)
  : eng(0L),
    sid(-1)
{
  attach(engine);
}

Debugger::~Debugger()
{
  detach();
}

void Debugger::attach(KJScript *e)
{
  dmode = Disabled;
  if (e) {
    if (!eng || e->rep != eng->rep) {
      eng = e;
      eng->rep->attachDebugger(this);
    }
  } else {
    eng = 0L;
  }
  reset();
}

KJScript *Debugger::engine() const
{
  return eng;
}

void Debugger::detach()
{
  reset();
  if (!eng)
    return;
  eng->rep->attachDebugger(0L);
  eng = 0L;
}

void Debugger::setMode(Mode m)
{
  dmode = m;
}

Debugger::Mode Debugger::mode() const
{
  return dmode;
}

// supposed to be overriden by the user
bool Debugger::stopEvent()
{
  return true;
}

void Debugger::callEvent(const UString &, const UString &)
{
}

void Debugger::returnEvent()
{
}

void Debugger::reset()
{
  l = -1;
}

int Debugger::freeSourceId() const
{
  return eng ? eng->rep->sourceId()+1 : -1;
}

bool Debugger::setBreakpoint(int id, int line)
{
  if (!eng)
    return false;
  return eng->rep->setBreakpoint(id, line, true);
}

bool Debugger::deleteBreakpoint(int id, int line)
{
  if (!eng)
    return false;
  return eng->rep->setBreakpoint(id, line, false);
}

void Debugger::clearAllBreakpoints(int id)
{
  if (!eng)
    return;
  eng->rep->setBreakpoint(id, -1, false);
}

UString Debugger::varInfo(const UString &ident)
{
  if (!eng)
    return UString();

  int dot = ident.find('.');
  if (dot < 0)
      dot = ident.size();
  UString sub = ident.substr(0, dot);
  KJSO obj;
  // resolve base
  if (sub == "this") {
      obj = Context::current()->thisValue();
  } else {
      const List *chain = Context::current()->pScopeChain();
      ListIterator scope = chain->begin();
      while (scope != chain->end()) {
	  if (scope->hasProperty(ident)) {
	      obj = scope->get(ident);
	      break;
	  }
	  scope++;
      }
      if (scope == chain->end())
	return UString();
  }
  // look up each part of a.b.c.
  while (dot < ident.size()) {
    int olddot = dot;
    dot = ident.find('.', olddot+1);
    if (dot < 0)
      dot = ident.size();
    sub = ident.substr(olddot+1, dot-olddot-1);
    obj = obj.get(sub);
    if (!obj.isDefined())
      break;
  }

  return sub + "=" + objInfo(obj) + ":" + UString(obj.imp()->typeInfo()->name);
}

// called by varInfo() and recursively by itself on each properties
UString Debugger::objInfo(const KJSO &obj) const
{
  const char *cnames[] = { "Undefined", "Array", "String", "Boolean",
			   "Number", "Object", "Date", "RegExp",
			   "Error", "Function" };
  PropList *plist = obj.imp()->propList(0, 0, false);
  if (!plist)
    return obj.toString().value();
  else {
    UString result = "{";
    while (1) {
      result += plist->name + "=";
      KJSO p = obj.get(plist->name);
      result += objInfo(p) + ":";
      Object obj = Object::dynamicCast(p);
      if (obj.isNull())
	result += p.imp()->typeInfo()->name;
      else
	result += cnames[int(obj.getClass())];
      plist = plist->next;
      if (!plist)
	break;
      result += ",";
    }
    result += "}";
    return result;
  }
}

bool Debugger::setVar(const UString &ident, const KJSO &value)
{
  if (!eng)
    return false;
  const List *chain = Context::current()->pScopeChain();
  ListIterator scope = chain->begin();
  while (scope != chain->end()) {
    if (scope->hasProperty(ident)) {
      if (!scope->canPut(ident))
	return false;
      scope->put(ident, value);
      return true;
    }
    scope++;
  }
  // didn't find variable
  return false;
}

// called from the scripting engine each time a statement node is hit.
bool Debugger::hit(int line, bool breakPoint)
{
  l = line;
  if (!eng)
    return true;

  if (!breakPoint && ( mode() == Continue || mode() == Disabled ) )
      return true;

  bool ret = stopEvent();
  eng->init();	// in case somebody used a different interpreter meanwhile
  return ret;
}

#endif
