// -*- c-basic-offset: 2 -*-
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
 *
 */

#include <stdio.h>
#include <string.h>

#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"

using namespace KJS;

class TestFunctionImp : public ObjectImp {
public:
  TestFunctionImp() : ObjectImp() {}
  virtual bool implementsCall() const { return true; }
  virtual Value call(ExecState *exec, Object &thisObj, const List &args);
};

Value TestFunctionImp::call(ExecState *exec, Object &/*thisObj*/, const List &args)
{
  fprintf(stderr,"--> %s\n",args[0].toString(exec).ascii());
  return Undefined();
}

class VersionFunctionImp : public ObjectImp {
public:
  VersionFunctionImp() : ObjectImp() {}
  virtual bool implementsCall() const { return true; }
  virtual Value call(ExecState *exec, Object &thisObj, const List &args);
};

Value VersionFunctionImp::call(ExecState *exec, Object &/*thisObj*/, const List &args)
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
    global.put(interp.globalExec(), Identifier("debug"), Object(new TestFunctionImp()));
    // add "print" for compatibility with the mozilla js shell
    global.put(interp.globalExec(), Identifier("print"), Object(new TestFunctionImp()));
    // add "version" for compatibility with the mozilla js shell 
    global.put(interp.globalExec(), Identifier("version"), Object(new VersionFunctionImp()));

    const int BufferSize = 200000;
    char code[BufferSize];

    for (int i = 1; i < argc; i++) {
      const char *file = argv[i];
      if (strcmp(file, "-f") == 0)
	continue;
      FILE *f = fopen(file, "r");
      if (!f) {
        fprintf(stderr, "Error opening %s.\n", file);
        return 2;
      }
      int num = fread(code, 1, BufferSize, f);
      code[num] = '\0';
      if(num >= BufferSize)
        fprintf(stderr, "Warning: File may have been too long.\n");

      // run
      Completion comp(interp.evaluate(file, 1, code));

      fclose(f);

      if (comp.complType() == Throw) {
        ExecState *exec = interp.globalExec();
        Value exVal = comp.value();
        char *msg = exVal.toString(exec).ascii();
        int lineno = -1;
        if (exVal.type() == ObjectType) {
          Value lineVal = Object::dynamicCast(exVal).get(exec,Identifier("line"));
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
