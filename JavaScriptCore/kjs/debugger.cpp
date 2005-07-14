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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "debugger.h"
#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "internal.h"
#include "ustring.h"

using namespace KJS;

// ------------------------------ Debugger -------------------------------------

namespace KJS {
  struct AttachedInterpreter
  {
  public:
    AttachedInterpreter(Interpreter *i) : interp(i) {}
    Interpreter *interp;
    AttachedInterpreter *next;
  };

}

Debugger::Debugger()
{
  rep = new DebuggerImp();
}

Debugger::~Debugger()
{
  // detach from all interpreters
  while (rep->interps)
    detach(rep->interps->interp);

  delete rep;
}

void Debugger::attach(Interpreter *interp)
{
  if (interp->imp()->debugger() != this)
    interp->imp()->setDebugger(this);

  // add to the list of attached interpreters
  if (!rep->interps)
    rep->interps = new AttachedInterpreter(interp);
  else {
    AttachedInterpreter *ai = rep->interps;
    while (ai->next)
      ai = ai->next;
    ai->next = new AttachedInterpreter(interp);;
  }
}

void Debugger::detach(Interpreter *interp)
{
  if (interp->imp()->debugger() == this)
    interp->imp()->setDebugger(this);

  // remove from the list of attached interpreters
  if (rep->interps->interp == interp) {
    AttachedInterpreter *old = rep->interps;
    rep->interps = rep->interps->next;
    delete old;
  }

  AttachedInterpreter *ai = rep->interps;
  while (ai->next && ai->next->interp != interp)
    ai = ai->next;
  if (ai->next) {
    AttachedInterpreter *old = ai->next;
    ai->next = ai->next->next;
    delete old;
  }
}

bool Debugger::sourceParsed(ExecState */*exec*/, int /*sourceId*/,
                            const UString &/*source*/, int /*errorLine*/)
{
  return true;
}

bool Debugger::sourceUnused(ExecState */*exec*/, int /*sourceId*/)
{
  return true;
}

bool Debugger::exception(ExecState */*exec*/, int /*sourceId*/, int /*lineno*/,
                         Object &/*exceptionObj*/)
{
  return true;
}

bool Debugger::atStatement(ExecState */*exec*/, int /*sourceId*/, int /*firstLine*/,
                           int /*lastLine*/)
{
  return true;
}

bool Debugger::callEvent(ExecState */*exec*/, int /*sourceId*/, int /*lineno*/,
                         Object &/*function*/, const List &/*args*/)
{
  return true;
}

bool Debugger::returnEvent(ExecState */*exec*/, int /*sourceId*/, int /*lineno*/,
                           Object &/*function*/)
{
  return true;
}

