/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2006 Bjoern Graf (bjoern.graf@gmail.com)
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

#include "CodeGenerator.h"
#include "InitializeThreading.h"
#include "JSArray.h"
#include "JSLock.h"
#include "PrototypeFunction.h"
#include "SamplingTool.h"
#include "completion.h"
#include "interpreter.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#if !PLATFORM(WIN_OS)
#include <unistd.h>
#endif

#if HAVE(READLINE)
#include <readline/history.h>
#include <readline/readline.h>
#endif

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if PLATFORM(UNIX)
#include <signal.h>
#endif

#if COMPILER(MSVC)
#include <crtdbg.h>
#include <windows.h>
#endif

#if PLATFORM(QT)
#include <QCoreApplication>
#include <QDateTime>
#endif

using namespace JSC;
using namespace WTF;

static bool fillBufferWithContentsOfFile(const UString& fileName, Vector<char>& buffer);

static JSValue* functionPrint(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* functionDebug(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* functionGC(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* functionVersion(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* functionRun(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* functionLoad(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* functionReadline(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* functionQuit(ExecState*, JSObject*, JSValue*, const ArgList&);

struct Options {
    Options()
        : interactive(false)
        , prettyPrint(false)
        , dump(false)
    {
    }

    bool interactive;
    bool prettyPrint;
    bool dump;
    Vector<UString> fileNames;
    Vector<UString> arguments;
};

static const char interactivePrompt[] = "> ";
static const UString interpreterName("Interpreter");

class StopWatch {
public:
    void start();
    void stop();
    long getElapsedMS(); // call stop() first

private:
#if PLATFORM(QT)
    uint m_startTime;
    uint m_stopTime;
#elif PLATFORM(WIN_OS)
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
#if PLATFORM(QT)
    QDateTime t = QDateTime::currentDateTime();
    m_startTime = t.toTime_t() * 1000 + t.time().msec();
#elif PLATFORM(WIN_OS)
    m_startTime = timeGetTime();
#else
    gettimeofday(&m_startTime, 0);
#endif
}

void StopWatch::stop()
{
#if PLATFORM(QT)
    QDateTime t = QDateTime::currentDateTime();
    m_stopTime = t.toTime_t() * 1000 + t.time().msec();
#elif PLATFORM(WIN_OS)
    m_stopTime = timeGetTime();
#else
    gettimeofday(&m_stopTime, 0);
#endif
}

long StopWatch::getElapsedMS()
{
#if PLATFORM(WIN_OS) || PLATFORM(QT)
    return m_stopTime - m_startTime;
#else
    timeval elapsedTime;
    timersub(&m_stopTime, &m_startTime, &elapsedTime);

    return elapsedTime.tv_sec * 1000 + lroundf(elapsedTime.tv_usec / 1000.0f);
#endif
}

class GlobalObject : public JSGlobalObject {
public:
    GlobalObject(const Vector<UString>& arguments);
    virtual UString className() const { return "global"; }
};
COMPILE_ASSERT(!IsInteger<GlobalObject>::value, WTF_IsInteger_GlobalObject_false);
ASSERT_CLASS_FITS_IN_CELL(GlobalObject);

GlobalObject::GlobalObject(const Vector<UString>& arguments)
    : JSGlobalObject()
{
    putDirectFunction(globalExec(), new (globalExec()) PrototypeFunction(globalExec(), prototypeFunctionStructure(), 1, Identifier(globalExec(), "debug"), functionDebug));
    putDirectFunction(globalExec(), new (globalExec()) PrototypeFunction(globalExec(), prototypeFunctionStructure(), 1, Identifier(globalExec(), "print"), functionPrint));
    putDirectFunction(globalExec(), new (globalExec()) PrototypeFunction(globalExec(), prototypeFunctionStructure(), 0, Identifier(globalExec(), "quit"), functionQuit));
    putDirectFunction(globalExec(), new (globalExec()) PrototypeFunction(globalExec(), prototypeFunctionStructure(), 0, Identifier(globalExec(), "gc"), functionGC));
    putDirectFunction(globalExec(), new (globalExec()) PrototypeFunction(globalExec(), prototypeFunctionStructure(), 1, Identifier(globalExec(), "version"), functionVersion));
    putDirectFunction(globalExec(), new (globalExec()) PrototypeFunction(globalExec(), prototypeFunctionStructure(), 1, Identifier(globalExec(), "run"), functionRun));
    putDirectFunction(globalExec(), new (globalExec()) PrototypeFunction(globalExec(), prototypeFunctionStructure(), 1, Identifier(globalExec(), "load"), functionLoad));
    putDirectFunction(globalExec(), new (globalExec()) PrototypeFunction(globalExec(), prototypeFunctionStructure(), 0, Identifier(globalExec(), "readline"), functionReadline));

    JSObject* array = constructEmptyArray(globalExec());
    for (size_t i = 0; i < arguments.size(); ++i)
        array->put(globalExec(), i, jsString(globalExec(), arguments[i]));
    putDirect(Identifier(globalExec(), "arguments"), array);

    Interpreter::setShouldPrintExceptions(true);
}

JSValue* functionPrint(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    for (unsigned i = 0; i < args.size(); ++i) {
        if (i != 0)
            putchar(' ');
        
        printf("%s", args.at(exec, i)->toString(exec).UTF8String().c_str());
    }
    
    putchar('\n');
    fflush(stdout);
    return jsUndefined();
}

JSValue* functionDebug(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    fprintf(stderr, "--> %s\n", args.at(exec, 0)->toString(exec).UTF8String().c_str());
    return jsUndefined();
}

JSValue* functionGC(ExecState* exec, JSObject*, JSValue*, const ArgList&)
{
    JSLock lock(false);
    exec->heap()->collect();
    return jsUndefined();
}

JSValue* functionVersion(ExecState*, JSObject*, JSValue*, const ArgList&)
{
    // We need this function for compatibility with the Mozilla JS tests but for now
    // we don't actually do any version-specific handling
    return jsUndefined();
}

JSValue* functionRun(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    StopWatch stopWatch;
    UString fileName = args.at(exec, 0)->toString(exec);
    Vector<char> script;
    if (!fillBufferWithContentsOfFile(fileName, script))
        return throwError(exec, GeneralError, "Could not open file.");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();

    stopWatch.start();
    Interpreter::evaluate(globalObject->globalExec(), globalObject->globalScopeChain(), makeSource(script.data(), fileName));
    stopWatch.stop();

    return jsNumber(globalObject->globalExec(), stopWatch.getElapsedMS());
}

JSValue* functionLoad(ExecState* exec, JSObject*, JSValue*, const ArgList& args)
{
    UString fileName = args.at(exec, 0)->toString(exec);
    Vector<char> script;
    if (!fillBufferWithContentsOfFile(fileName, script))
        return throwError(exec, GeneralError, "Could not open file.");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    Interpreter::evaluate(globalObject->globalExec(), globalObject->globalScopeChain(), makeSource(script.data(), fileName));

    return jsUndefined();
}

JSValue* functionReadline(ExecState* exec, JSObject*, JSValue*, const ArgList&)
{
    Vector<char, 256> line;
    int c;
    while ((c = getchar()) != EOF) {
        // FIXME: Should we also break on \r? 
        if (c == '\n')
            break;
        line.append(c);
    }
    line.append('\0');
    return jsString(exec, line.data());
}

JSValue* functionQuit(ExecState*, JSObject*, JSValue*, const ArgList&)
{
    exit(0);
#if !COMPILER(MSVC)
    // MSVC knows that exit(0) never returns, so it flags this return statement as unreachable.
    return jsUndefined();
#endif
}

// Use SEH for Release builds only to get rid of the crash report dialog
// (luckily the same tests fail in Release and Debug builds so far). Need to
// be in a separate main function because the jscmain function requires object
// unwinding.

#if COMPILER(MSVC) && !defined(_DEBUG)
#define TRY       __try {
#define EXCEPT(x) } __except (EXCEPTION_EXECUTE_HANDLER) { x; }
#else
#define TRY
#define EXCEPT(x)
#endif

int jscmain(int argc, char** argv, JSGlobalData*);

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

#if PLATFORM(QT)
    QCoreApplication app(argc, argv);
#endif

    int res = 0;
    TRY
        res = jscmain(argc, argv, JSGlobalData::create().releaseRef());
    EXCEPT(res = 3)
    return res;
}

static bool prettyPrintScript(ExecState* exec, const UString& fileName, const Vector<char>& script)
{
    int errLine = 0;
    UString errMsg;
    RefPtr<ProgramNode> programNode = exec->globalData().parser->parse<ProgramNode>(exec, exec->dynamicGlobalObject()->debugger(), makeSource(script.data(), fileName), &errLine, &errMsg);
    if (!programNode) {
        fprintf(stderr, "%s:%d: %s.\n", fileName.UTF8String().c_str(), errLine, errMsg.UTF8String().c_str());
        return false;
    }

    printf("%s\n", programNode->toString().UTF8String().c_str());
    return true;
}

static bool runWithScripts(GlobalObject* globalObject, const Vector<UString>& fileNames, bool prettyPrint, bool dump)
{
    Vector<char> script;

    if (dump)
        CodeGenerator::setDumpsGeneratedCode(true);

#if ENABLE(OPCODE_SAMPLING)
    Machine* machine = globalObject->globalData()->machine;
    machine->setSampler(new SamplingTool(machine));
#endif

    bool success = true;
    for (size_t i = 0; i < fileNames.size(); i++) {
        UString fileName = fileNames[i];

        if (!fillBufferWithContentsOfFile(fileName, script))
            return false; // fail early so we can catch missing files

        if (prettyPrint)
            prettyPrintScript(globalObject->globalExec(), fileName, script);
        else {
#if ENABLE(OPCODE_SAMPLING)
            machine->sampler()->start();
#endif
            Completion completion = Interpreter::evaluate(globalObject->globalExec(), globalObject->globalScopeChain(), makeSource(script.data(), fileName));
            success = success && completion.complType() != Throw;
            if (dump) {
                if (completion.complType() == Throw)
                    printf("Exception: %s\n", completion.value()->toString(globalObject->globalExec()).ascii());
                else
                    printf("End: %s\n", completion.value()->toString(globalObject->globalExec()).ascii());
            }

            globalObject->globalExec()->clearException();

#if ENABLE(OPCODE_SAMPLING)
            machine->sampler()->stop();
#endif
        }
    }

#if ENABLE(OPCODE_SAMPLING)
    machine->sampler()->dump(globalObject->globalExec());
    delete machine->sampler();
#endif
    return success;
}

static void runInteractive(GlobalObject* globalObject)
{
    while (true) {
#if HAVE(READLINE)
        char* line = readline(interactivePrompt);
        if (!line)
            break;
        if (line[0])
            add_history(line);
        Completion completion = Interpreter::evaluate(globalObject->globalExec(), globalObject->globalScopeChain(), makeSource(line, interpreterName));
        free(line);
#else
        puts(interactivePrompt);
        Vector<char, 256> line;
        int c;
        while ((c = getchar()) != EOF) {
            // FIXME: Should we also break on \r? 
            if (c == '\n')
                break;
            line.append(c);
        }
        line.append('\0');
        Completion completion = Interpreter::evaluate(globalObject->globalExec(), globalObject->globalScopeChain(), makeSource(line.data(), interpreterName));
#endif
        if (completion.complType() == Throw)
            printf("Exception: %s\n", completion.value()->toString(globalObject->globalExec()).ascii());
        else
            printf("%s\n", completion.value()->toString(globalObject->globalExec()).UTF8String().c_str());

        globalObject->globalExec()->clearException();
    }
    printf("\n");
}

static void printUsageStatement()
{
    fprintf(stderr, "Usage: jsc [options] [files] [-- arguments]\n");
    fprintf(stderr, "  -d         Dumps bytecode (debug builds only)\n");
    fprintf(stderr, "  -f         Specifies a source file (deprecated)\n");
    fprintf(stderr, "  -h|--help  Prints this help message\n");
    fprintf(stderr, "  -i         Enables interactive mode (default if no files are specified)\n");
    fprintf(stderr, "  -p         Prints formatted source code\n");
    fprintf(stderr, "  -s         Installs signal handlers that exit on a crash (Unix platforms only)\n");
    exit(-1);
}

static void parseArguments(int argc, char** argv, Options& options)
{
    int i = 1;
    for (; i < argc; ++i) {
        const char* arg = argv[i];
        if (strcmp(arg, "-f") == 0) {
            if (++i == argc)
                printUsageStatement();
            options.fileNames.append(argv[i]);
            continue;
        }
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            printUsageStatement();
        }
        if (strcmp(arg, "-i") == 0) {
            options.interactive = true;
            continue;
        }
        if (strcmp(arg, "-p") == 0) {
            options.prettyPrint = true;
            continue;
        }
        if (strcmp(arg, "-d") == 0) {
            options.dump = true;
            continue;
        }
        if (strcmp(arg, "-s") == 0) {
#if PLATFORM(UNIX)
            signal(SIGILL, _exit);
            signal(SIGFPE, _exit);
            signal(SIGBUS, _exit);
            signal(SIGSEGV, _exit);
#endif
            continue;
        }
        if (strcmp(arg, "--") == 0) {
            ++i;
            break;
        }
        options.fileNames.append(argv[i]);
    }
    
    if (options.fileNames.isEmpty())
        options.interactive = true;
    
    for (; i < argc; ++i)
        options.arguments.append(argv[i]);
}

int jscmain(int argc, char** argv, JSGlobalData* globalData)
{
    JSC::initializeThreading();

    JSLock lock(false);

    Options options;
    parseArguments(argc, argv, options);

    GlobalObject* globalObject = new (globalData) GlobalObject(options.arguments);
    bool success = runWithScripts(globalObject, options.fileNames, options.prettyPrint, options.dump);
    if (options.interactive && success)
        runInteractive(globalObject);

    return success ? 0 : 3;
}

static bool fillBufferWithContentsOfFile(const UString& fileName, Vector<char>& buffer)
{
    FILE* f = fopen(fileName.UTF8String().c_str(), "r");
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", fileName.UTF8String().c_str());
        return false;
    }

    size_t buffer_size = 0;
    size_t buffer_capacity = 1024;

    buffer.resize(buffer_capacity);

    while (!feof(f) && !ferror(f)) {
        buffer_size += fread(buffer.data() + buffer_size, 1, buffer_capacity - buffer_size, f);
        if (buffer_size == buffer_capacity) { // guarantees space for trailing '\0'
            buffer_capacity *= 2;
            buffer.resize(buffer_capacity);
        }
    }
    fclose(f);
    buffer[buffer_size] = '\0';

    return true;
}
