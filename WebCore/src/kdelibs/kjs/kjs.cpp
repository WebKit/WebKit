/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <string.h>

#include "kjs.h"
#include "types.h"
#include "internal.h"
#include "collector.h"

using namespace KJS;

KJScript::KJScript()
{
  rep = new KJScriptImp(this);
  rep->init();
}

KJScript::~KJScript()
{
  delete rep;

#ifdef KJS_DEBUG_MEM
  //printf("Imp::count: %d\n", Imp::count);
  //  assert(Imp::count == 0);
#endif
}

void KJScript::init()
{
  rep->init();
}

Imp *KJScript::globalObject() const
{
  return rep->glob.imp();
}

void KJScript::setCurrent( KJScript *newCurr )
{
  KJScriptImp::curr = newCurr ? newCurr->rep : 0L;
}

KJScript *KJScript::current()
{
  return KJScriptImp::current() ? KJScriptImp::current()->scr : 0L;
}

int KJScript::recursion() const
{
  return rep->recursion;
}

bool KJScript::evaluate(const char *code)
{
  return rep->evaluate(UString(code).data(), strlen(code));
}

bool KJScript::evaluate(const UString &code)
{
  return rep->evaluate(code.data(), code.size());
}

bool KJScript::evaluate(const KJSO &thisV,
			const QChar *code, unsigned int length)
{
  return rep->evaluate((UChar*)code, length, thisV);
}

bool KJScript::call(const KJS::UString &func, const List &args)
{
  return rep->call(0, func, args);
}

bool KJScript::call(const KJSO &scope, const KJS::UString &func,
		    const KJS::List &args)
{
  return rep->call(scope.imp(), func, args);
}

bool KJScript::call(const KJS::KJSO &func, const KJSO &thisV,
		    const KJS::List &args, const KJS::List &extraScope)
{
  return rep->call(func, thisV, args, extraScope);
}

void KJScript::clear()
{
  rep->clear();
}

Imp *KJScript::returnValue() const
{
  return rep->retVal;
}

int KJScript::errorType() const
{
  return rep->errType;
}

int KJScript::errorLine() const
{
  return rep->errLine;
}

const char* KJScript::errorMsg() const
{
  return rep->errMsg.ascii();
}

bool KJScript::checkSyntax(const KJS::UString &code )
{
  return rep->evaluate(code.data(), code.size(), 0, true);
}

/**
 * @short Print to stderr for debugging purposes.
 */
namespace KJS {
  class DebugPrint : public InternalFunctionImp {
  public:
    Completion execute(const List &args)
      {
	KJSO v = args[0];
	String s = v.toString();
	fprintf(stderr, "---> %s\n", s.value().cstring().c_str());

	return Completion(Normal);
      }
  };
};

void KJScript::enableDebug()
{
  rep->init();
  Global::current().put("debug", Function(new DebugPrint()));
}
