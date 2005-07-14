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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"

using namespace KJS;

class TestFunctionImp : public ObjectImp {
public:
  TestFunctionImp(int i, int length);
  virtual bool implementsCall() const { return true; }
  virtual Value call(ExecState *exec, Object &thisObj, const List &args);

  enum { Print, Debug, Quit };

private:
  int id;
};

TestFunctionImp::TestFunctionImp(int i, int length) : ObjectImp(), id(i)
{
  putDirect(lengthPropertyName,length,DontDelete|ReadOnly|DontEnum);
}

Value TestFunctionImp::call(ExecState *exec, Object &/*thisObj*/, const List &args)
{
  switch (id) {
  case Print:
  case Debug:
    fprintf(stderr,"--> %s\n",args[0].toString(exec).ascii());
    return Undefined();
  case Quit:
    exit(0);
    return Undefined();
  default:
    break;
  }

  return Undefined();
}

class VersionFunctionImp : public ObjectImp {
public:
  VersionFunctionImp() : ObjectImp() {}
  virtual bool implementsCall() const { return true; }
  virtual Value call(ExecState *exec, Object &thisObj, const List &args);
};

Value VersionFunctionImp::call(ExecState */*exec*/, Object &/*thisObj*/, const List &/*args*/)
{
  // We need this function for compatibility with the Mozilla JS tests but for now
  // we don't actually do any version-specific handling
  return Undefined();
}

class GlobalImp : public ObjectImp {
public:
  virtual UString className() const { return "global"; }
};

int main(int argc, char **argv)
{
  // expecting a filename
  if (argc < 2) {
    fprintf(stderr, "You have to specify at least one filename\n");
    return -1;
  }

  bool ret = true;
  {
    Interpreter::lock();

    Object global(new GlobalImp());

    // create interpreter
    Interpreter interp(global);
    // add debug() function
    global.put(interp.globalExec(), "debug", Object(new TestFunctionImp(TestFunctionImp::Debug,1)));
    // add "print" for compatibility with the mozilla js shell
    global.put(interp.globalExec(), "print", Object(new TestFunctionImp(TestFunctionImp::Print,1)));
    // add "quit" for compatibility with the mozilla js shell
    global.put(interp.globalExec(), "quit", Object(new TestFunctionImp(TestFunctionImp::Quit,0)));
    // add "version" for compatibility with the mozilla js shell 
    global.put(interp.globalExec(), "version", Object(new VersionFunctionImp()));

    for (int i = 1; i < argc; i++) {
      int code_len = 0;
      int code_alloc = 1024;
      char *code = (char*)malloc(code_alloc);

      const char *file = argv[i];
      if (strcmp(file, "-f") == 0)
	continue;
      FILE *f = fopen(file, "r");
      if (!f) {
        fprintf(stderr, "Error opening %s.\n", file);
        return 2;
      }

      while (!feof(f) && !ferror(f)) {
	size_t len = fread(code+code_len,1,code_alloc-code_len,f);
	code_len += len;
	if (code_len >= code_alloc) {
	  code_alloc *= 2;
	  code = (char*)realloc(code,code_alloc);
	}
      }
      code = (char*)realloc(code,code_len+1);
      code[code_len] = '\0';

      // run
      Completion comp(interp.evaluate(file, 1, code));

      fclose(f);

      if (comp.complType() == Throw) {
        ExecState *exec = interp.globalExec();
        Value exVal = comp.value();
        char *msg = exVal.toString(exec).ascii();
        int lineno = -1;
        if (exVal.type() == ObjectType) {
          Value lineVal = Object::dynamicCast(exVal).get(exec,"line");
          if (lineVal.type() == NumberType)
            lineno = int(lineVal.toNumber(exec));
        }
        if (lineno != -1)
          fprintf(stderr,"Exception, line %d: %s\n",lineno,msg);
        else
          fprintf(stderr,"Exception: %s\n",msg);
        ret = false;
      }
      else if (comp.complType() == ReturnValue) {
        char *msg = comp.value().toString(interp.globalExec()).ascii();
        fprintf(stderr,"Return value: %s\n",msg);
      }

      free(code);
    }

    Interpreter::unlock();
  } // end block, so that Interpreter and global get deleted

  if (ret)
    fprintf(stderr, "OK.\n");

#ifdef KJS_DEBUG_MEM
  Interpreter::finalCheck();
#endif
  return ret ? 0 : 3;
}
