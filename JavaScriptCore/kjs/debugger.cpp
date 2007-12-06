// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "debugger.h"

#include "JSGlobalObject.h"
#include "internal.h"
#include "ustring.h"

using namespace KJS;

// ------------------------------ Debugger -------------------------------------

namespace KJS {
  struct AttachedGlobalObject
  {
  public:
    AttachedGlobalObject(JSGlobalObject* o, AttachedGlobalObject* ai) : globalObj(o), next(ai) { ++Debugger::debuggersPresent; }
    ~AttachedGlobalObject() { --Debugger::debuggersPresent; }
    JSGlobalObject* globalObj;
    AttachedGlobalObject* next;
  };

}

int Debugger::debuggersPresent = 0;

Debugger::Debugger()
{
  rep = new DebuggerImp();
}

Debugger::~Debugger()
{
  detach(0);
  delete rep;
}

void Debugger::attach(JSGlobalObject* globalObject)
{
  Debugger* other = globalObject->debugger();
  if (other == this)
    return;
  if (other)
    other->detach(globalObject);
  globalObject->setDebugger(this);
  rep->globalObjects = new AttachedGlobalObject(globalObject, rep->globalObjects);
}

void Debugger::detach(JSGlobalObject* globalObj)
{
  // iterate the addresses where AttachedGlobalObject pointers are stored
  // so we can unlink items from the list
  AttachedGlobalObject **p = &rep->globalObjects;
  AttachedGlobalObject *q;
  while ((q = *p)) {
    if (!globalObj || q->globalObj == globalObj) {
      *p = q->next;
      q->globalObj->setDebugger(0);
      delete q;
    } else
      p = &q->next;
  }

  if (globalObj)
    latestExceptions.remove(globalObj);
  else
    latestExceptions.clear();
}

bool Debugger::hasHandledException(ExecState *exec, JSValue *exception)
{
    if (latestExceptions.get(exec->dynamicGlobalObject()).get() == exception)
        return true;

    latestExceptions.set(exec->dynamicGlobalObject(), exception);
    return false;
}

bool Debugger::sourceParsed(ExecState*, int /*sourceId*/, const UString &/*sourceURL*/, 
                           const UString &/*source*/, int /*startingLineNumber*/, int /*errorLine*/, const UString & /*errorMsg*/)
{
  return true;
}

bool Debugger::sourceUnused(ExecState*, int /*sourceId*/)
{
  return true;
}

bool Debugger::exception(ExecState*, int /*sourceId*/, int /*lineno*/,
                         JSValue* /*exception */)
{
  return true;
}

bool Debugger::atStatement(ExecState*, int /*sourceId*/, int /*firstLine*/,
                           int /*lastLine*/)
{
  return true;
}

bool Debugger::callEvent(ExecState*, int /*sourceId*/, int /*lineno*/,
                         JSObject* /*function*/, const List &/*args*/)
{
  return true;
}

bool Debugger::returnEvent(ExecState*, int /*sourceId*/, int /*lineno*/,
                           JSObject* /*function*/)
{
  return true;
}

