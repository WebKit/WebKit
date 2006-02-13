// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004 Apple Computer, Inc.
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
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "JSLock.h"
#include "interpreter.h"
#include "object.h"
#include "types.h"
#include "value.h"

#include <kxmlcore/HashTraits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


using namespace KJS;
using namespace KXMLCore;

bool run(const char* fileName, Interpreter *interp);

class GlobalImp : public JSObject {
public:
  virtual UString className() const { return "global"; }
};

class TestFunctionImp : public JSObject {
public:
  TestFunctionImp(int i, int length);
  virtual bool implementsCall() const { return true; }
  virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);

  enum { Print, Debug, Quit, GC, Version, Run };

private:
  int id;
};

TestFunctionImp::TestFunctionImp(int i, int length) : JSObject(), id(i)
{
  putDirect(lengthPropertyName,length,DontDelete|ReadOnly|DontEnum);
}

JSValue *TestFunctionImp::callAsFunction(ExecState *exec, JSObject * /*thisObj*/, const List &args)
{
  switch (id) {
  case Print:
  case Debug:
    fprintf(stderr,"--> %s\n",args[0]->toString(exec).UTF8String().c_str());
    return jsUndefined();
  case Quit:
    exit(0);
    return jsUndefined();
  case GC:
  {
    JSLock lock;
    Interpreter::collect();
  }
  case Version:
    // We need this function for compatibility with the Mozilla JS tests but for now
    // we don't actually do any version-specific handling
    return jsUndefined();
  case Run:
    run(args[0]->toString(exec).UTF8String().c_str(), exec->dynamicInterpreter());
    return jsUndefined();
  default:
    assert(0);
  }

  return jsUndefined();
}

int main(int argc, char **argv)
{
  if (argc < 2) {
    fprintf(stderr, "Usage: testkjs file1 [file2...]\n");
    return -1;
  }

  bool success = true;
  {
    JSLock lock;
    
    // Unit tests for KXMLCore::IsInteger. Don't have a better place for them now.
    // FIXME: move these once we create a unit test directory for KXMLCore.
    assert(IsInteger<bool>::value);
    assert(IsInteger<char>::value);
    assert(IsInteger<signed char>::value);
    assert(IsInteger<unsigned char>::value);
    assert(IsInteger<short>::value);
    assert(IsInteger<unsigned short>::value);
    assert(IsInteger<int>::value);
    assert(IsInteger<unsigned int>::value);
    assert(IsInteger<long>::value);
    assert(IsInteger<unsigned long>::value);
    assert(IsInteger<long long>::value);
    assert(IsInteger<unsigned long long>::value);

    assert(!IsInteger<char *>::value);
    assert(!IsInteger<const char *>::value);
    assert(!IsInteger<volatile char *>::value);
    assert(!IsInteger<double>::value);
    assert(!IsInteger<float>::value);
    assert(!IsInteger<GlobalImp>::value);
    
    GlobalImp *global = new GlobalImp();

    // create interpreter
    Interpreter interp(global);
    // add debug() function
    global->put(interp.globalExec(), "debug", new TestFunctionImp(TestFunctionImp::Debug, 1));
    // add "print" for compatibility with the mozilla js shell
    global->put(interp.globalExec(), "print", new TestFunctionImp(TestFunctionImp::Print, 1));
    // add "quit" for compatibility with the mozilla js shell
    global->put(interp.globalExec(), "quit", new TestFunctionImp(TestFunctionImp::Quit, 0));
    // add "gc" for compatibility with the mozilla js shell
    global->put(interp.globalExec(), "gc", new TestFunctionImp(TestFunctionImp::GC, 0));
    // add "version" for compatibility with the mozilla js shell 
    global->put(interp.globalExec(), "version", new TestFunctionImp(TestFunctionImp::Version, 1));
    global->put(interp.globalExec(), "run", new TestFunctionImp(TestFunctionImp::Run, 1));

    for (int i = 1; i < argc; i++) {
      const char *fileName = argv[i];
      if (strcmp(fileName, "-f") == 0) // mozilla test driver script uses "-f" prefix for files
        continue;

      success = success && run(fileName, &interp);
    }
  } // end block, so that interpreter gets deleted

  if (success)
    fprintf(stderr, "OK.\n");
  
#ifdef KJS_DEBUG_MEM
  Interpreter::finalCheck();
#endif
  return success ? 0 : 3;
}

bool run(const char* fileName, Interpreter *interp)
{
  int code_size = 0;
  int code_capacity = 1024;
  char *code = (char*)malloc(code_capacity);
  
  FILE *f = fopen(fileName, "r");
  if (!f) {
    fprintf(stderr, "Error opening %s.\n", fileName);
    return 2;
  }
  
  while (!feof(f) && !ferror(f)) {
    code_size += fread(code + code_size, 1, code_capacity - code_size, f);
    if (code_size == code_capacity) { // guarantees space for trailing '\0'
      code_capacity *= 2;
      code = (char*)realloc(code, code_capacity);
      assert(code);
    }
  
    assert(code_size < code_capacity);
  }
  fclose(f);
  code[code_size] = '\0';

  // run
  Completion comp(interp->evaluate(fileName, 1, code));
  free(code);

  JSValue *exVal = comp.value();
  if (comp.complType() == Throw) {
    ExecState *exec = interp->globalExec();
    const char *msg = exVal->toString(exec).UTF8String().c_str();
    if (exVal->isObject()) {
      JSValue *lineVal = static_cast<JSObject *>(exVal)->get(exec, "line");
      if (lineVal->isNumber())
        fprintf(stderr, "Exception, line %d: %s\n", (int)lineVal->toNumber(exec), msg);
      else
        fprintf(stderr,"Exception: %s\n",msg);
    }
    
    return false;
  } else if (comp.complType() == ReturnValue) {
    const char *msg = exVal->toString(interp->globalExec()).UTF8String().c_str();
    fprintf(stderr,"Return value: %s\n",msg);
  }
  
  return true;
}
