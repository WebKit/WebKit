// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2006 Apple Computer, Inc.
 *  Copyright (C) 2006 Björn Graf (bjoern.graf@gmail.com)
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
#include "collector.h"

#include "HashTraits.h"
#include "JSLock.h"
#include "object.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#include "protect.h"

#if PLATFORM(WIN_OS)
#include <windows.h>
#include <crtdbg.h>
#endif

using namespace KJS;
using namespace WTF;

static void testIsInteger();
static char* createStringWithContentsOfFile(const char* fileName);

class StopWatch
{
public:
    void start();
    void stop();
    long getElapsedMS(); // call stop() first
    
private:
#if PLATFORM(WIN_OS)
    DWORD m_startTime;
    DWORD m_stopTime;
#else
    // Windows does not have timeval, disabling this class for now (bug 7399)
    timeval m_startTime;
    timeval m_stopTime;
#endif
};

void StopWatch::start()
{
#if PLATFORM(WIN_OS)
    m_startTime = timeGetTime();
#else
    gettimeofday(&m_startTime, 0);
#endif
}

void StopWatch::stop()
{
#if PLATFORM(WIN_OS)
    m_stopTime = timeGetTime();
#else
    gettimeofday(&m_stopTime, 0);
#endif
}

long StopWatch::getElapsedMS()
{
#if PLATFORM(WIN_OS)
    return m_stopTime - m_startTime;
#else
    timeval elapsedTime;
    timersub(&m_stopTime, &m_startTime, &elapsedTime);
    
    return elapsedTime.tv_sec * 1000 + lroundf(elapsedTime.tv_usec / 1000.0);
#endif
}

class GlobalImp : public JSObject {
public:
  virtual UString className() const { return "global"; }
};

class TestFunctionImp : public JSObject {
public:
  TestFunctionImp(int i, int length);
  virtual bool implementsCall() const { return true; }
  virtual JSValue* callAsFunction(ExecState* exec, JSObject* thisObj, const List &args);

  enum { Print, Debug, Quit, GC, Version, Run };

private:
  int id;
};

TestFunctionImp::TestFunctionImp(int i, int length) : JSObject(), id(i)
{
  putDirect(lengthPropertyName,length,DontDelete|ReadOnly|DontEnum);
}

JSValue* TestFunctionImp::callAsFunction(ExecState* exec, JSObject*, const List &args)
{
  switch (id) {
    case Print:
      printf("--> %s\n", args[0]->toString(exec).UTF8String().c_str());
      return jsUndefined();
    case Debug:
      fprintf(stderr, "--> %s\n", args[0]->toString(exec).UTF8String().c_str());
      return jsUndefined();
    case GC:
    {
      JSLock lock;
      Interpreter::collect();
      return jsUndefined();
    }
    case Version:
      // We need this function for compatibility with the Mozilla JS tests but for now
      // we don't actually do any version-specific handling
      return jsUndefined();
    case Run:
    {
      StopWatch stopWatch;
      char* fileName = strdup(args[0]->toString(exec).UTF8String().c_str());
      char* script = createStringWithContentsOfFile(fileName);
      if (!script)
        return throwError(exec, GeneralError, "Could not open file.");

      stopWatch.start();
      exec->dynamicInterpreter()->evaluate(fileName, 0, script);
      stopWatch.stop();

      free(script);
      free(fileName);
      
      return jsNumber(stopWatch.getElapsedMS());
    }
    case Quit:
      exit(0);
    default:
      abort();
  }
  return 0;
}

#if PLATFORM(WIN_OS)

// Use SEH for Release builds only to get rid of the crash report dialog
// (luckyly the same tests fail in Release and Debug builds so far). Need to
// be in a separate main function because the kjsmain function requires object
// unwinding.

#if defined(_DEBUG)
#define TRY
#define EXCEPT(x)
#else
#define TRY       __try {
#define EXCEPT(x) } __except (EXCEPTION_EXECUTE_HANDLER) { x; }
#endif

#else

#define TRY
#define EXCEPT(x)

#endif

int kjsmain(int argc, char** argv);

int main(int argc, char** argv)
{
#if defined(_DEBUG) && PLATFORM(WIN_OS)
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
#endif

    int res = 0;
    TRY
        res = kjsmain(argc, argv);
    EXCEPT(res = 3)
    return res;
}


bool doIt(int argc, char** argv)
{
  bool success = true;
  GlobalImp* global = new GlobalImp();

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
  
  Interpreter::setShouldPrintExceptions(true);
  
  for (int i = 1; i < argc; i++) {
    const char* fileName = argv[i];
    if (strcmp(fileName, "-f") == 0) // mozilla test driver script uses "-f" prefix for files
      continue;
    
    char* script = createStringWithContentsOfFile(fileName);
    if (!script) {
      success = false;
      break; // fail early so we can catch missing files
    }
    
    Completion completion = interp.evaluate(fileName, 0, script);
    success = success && completion.complType() != Throw;
    free(script);
  }

  return success;
}


int kjsmain(int argc, char** argv)
{
  if (argc < 2) {
    fprintf(stderr, "Usage: testkjs file1 [file2...]\n");
    return -1;
  }

  testIsInteger();

  JSLock lock;

  bool success = doIt(argc, argv);

#ifndef NDEBUG
  Collector::collect();
#endif

  if (success)
    fprintf(stderr, "OK.\n");
  
#ifdef KJS_DEBUG_MEM
  Interpreter::finalCheck();
#endif
  return success ? 0 : 3;
}

static void testIsInteger()
{
  // Unit tests for WTF::IsInteger. Don't have a better place for them now.
  // FIXME: move these once we create a unit test directory for WTF.

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

  assert(!IsInteger<char*>::value);
  assert(!IsInteger<const char* >::value);
  assert(!IsInteger<volatile char* >::value);
  assert(!IsInteger<double>::value);
  assert(!IsInteger<float>::value);
  assert(!IsInteger<GlobalImp>::value);
}

static char* createStringWithContentsOfFile(const char* fileName)
{
  char* buffer;
  
  int buffer_size = 0;
  int buffer_capacity = 1024;
  buffer = (char*)malloc(buffer_capacity);
  
  FILE* f = fopen(fileName, "r");
  if (!f) {
    fprintf(stderr, "Could not open file: %s\n", fileName);
    return 0;
  }
  
  while (!feof(f) && !ferror(f)) {
    buffer_size += fread(buffer + buffer_size, 1, buffer_capacity - buffer_size, f);
    if (buffer_size == buffer_capacity) { // guarantees space for trailing '\0'
      buffer_capacity *= 2;
      buffer = (char*)realloc(buffer, buffer_capacity);
      assert(buffer);
    }
    
    assert(buffer_size < buffer_capacity);
  }
  fclose(f);
  buffer[buffer_size] = '\0';
  
  return buffer;
}
