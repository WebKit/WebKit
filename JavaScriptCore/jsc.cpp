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

#include "BytecodeGenerator.h"
#include "Completion.h"
#include "CurrentTime.h"
#include "InitializeThreading.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSLock.h"
#include "JSString.h"
#include "PrototypeFunction.h"
#include "SamplingTool.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !OS(WINDOWS)
#include <unistd.h>
#endif

#if HAVE(READLINE)
#include <readline/history.h>
#include <readline/readline.h>
#endif

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if HAVE(SIGNAL_H)
#include <signal.h>
#endif

#if COMPILER(MSVC) && !OS(WINCE)
#include <crtdbg.h>
#include <mmsystem.h>
#include <windows.h>
#endif

#if PLATFORM(QT)
#include <QCoreApplication>
#include <QDateTime>
#endif

using namespace JSC;
using namespace WTF;

static void cleanupGlobalData(JSGlobalData*);
static bool fillBufferWithContentsOfFile(const UString& fileName, Vector<char>& buffer);

static JSValue JSC_HOST_CALL functionPrint(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL functionDebug(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL functionGC(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL functionVersion(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL functionRun(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL functionLoad(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL functionCheckSyntax(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL functionReadline(ExecState*, JSObject*, JSValue, const ArgList&);
static NO_RETURN JSValue JSC_HOST_CALL functionQuit(ExecState*, JSObject*, JSValue, const ArgList&);

#if ENABLE(SAMPLING_FLAGS)
static JSValue JSC_HOST_CALL functionSetSamplingFlags(ExecState*, JSObject*, JSValue, const ArgList&);
static JSValue JSC_HOST_CALL functionClearSamplingFlags(ExecState*, JSObject*, JSValue, const ArgList&);
#endif

struct Script {
    bool isFile;
    char* argument;

    Script(bool isFile, char *argument)
        : isFile(isFile)
        , argument(argument)
    {
    }
};

struct Options {
    Options()
        : interactive(false)
        , dump(false)
    {
    }

    bool interactive;
    bool dump;
    Vector<Script> scripts;
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
    double m_startTime;
    double m_stopTime;
};

void StopWatch::start()
{
    m_startTime = currentTime();
}

void StopWatch::stop()
{
    m_stopTime = currentTime();
}

long StopWatch::getElapsedMS()
{
    return static_cast<long>((m_stopTime - m_startTime) * 1000);
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
    putDirectFunction(globalExec(), new (globalExec()) NativeFunctionWrapper(globalExec(), prototypeFunctionStructure(), 1, Identifier(globalExec(), "debug"), functionDebug));
    putDirectFunction(globalExec(), new (globalExec()) NativeFunctionWrapper(globalExec(), prototypeFunctionStructure(), 1, Identifier(globalExec(), "print"), functionPrint));
    putDirectFunction(globalExec(), new (globalExec()) NativeFunctionWrapper(globalExec(), prototypeFunctionStructure(), 0, Identifier(globalExec(), "quit"), functionQuit));
    putDirectFunction(globalExec(), new (globalExec()) NativeFunctionWrapper(globalExec(), prototypeFunctionStructure(), 0, Identifier(globalExec(), "gc"), functionGC));
    putDirectFunction(globalExec(), new (globalExec()) NativeFunctionWrapper(globalExec(), prototypeFunctionStructure(), 1, Identifier(globalExec(), "version"), functionVersion));
    putDirectFunction(globalExec(), new (globalExec()) NativeFunctionWrapper(globalExec(), prototypeFunctionStructure(), 1, Identifier(globalExec(), "run"), functionRun));
    putDirectFunction(globalExec(), new (globalExec()) NativeFunctionWrapper(globalExec(), prototypeFunctionStructure(), 1, Identifier(globalExec(), "load"), functionLoad));
    putDirectFunction(globalExec(), new (globalExec()) NativeFunctionWrapper(globalExec(), prototypeFunctionStructure(), 1, Identifier(globalExec(), "checkSyntax"), functionCheckSyntax));
    putDirectFunction(globalExec(), new (globalExec()) NativeFunctionWrapper(globalExec(), prototypeFunctionStructure(), 0, Identifier(globalExec(), "readline"), functionReadline));

#if ENABLE(SAMPLING_FLAGS)
    putDirectFunction(globalExec(), new (globalExec()) NativeFunctionWrapper(globalExec(), prototypeFunctionStructure(), 1, Identifier(globalExec(), "setSamplingFlags"), functionSetSamplingFlags));
    putDirectFunction(globalExec(), new (globalExec()) NativeFunctionWrapper(globalExec(), prototypeFunctionStructure(), 1, Identifier(globalExec(), "clearSamplingFlags"), functionClearSamplingFlags));
#endif

    JSObject* array = constructEmptyArray(globalExec());
    for (size_t i = 0; i < arguments.size(); ++i)
        array->put(globalExec(), i, jsString(globalExec(), arguments[i]));
    putDirect(Identifier(globalExec(), "arguments"), array);
}

JSValue JSC_HOST_CALL functionPrint(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    for (unsigned i = 0; i < args.size(); ++i) {
        if (i)
            putchar(' ');

        printf("%s", args.at(i).toString(exec).UTF8String().c_str());
    }

    putchar('\n');
    fflush(stdout);
    return jsUndefined();
}

JSValue JSC_HOST_CALL functionDebug(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    fprintf(stderr, "--> %s\n", args.at(0).toString(exec).UTF8String().c_str());
    return jsUndefined();
}

JSValue JSC_HOST_CALL functionGC(ExecState* exec, JSObject*, JSValue, const ArgList&)
{
    JSLock lock(SilenceAssertionsOnly);
    exec->heap()->collectAllGarbage();
    return jsUndefined();
}

JSValue JSC_HOST_CALL functionVersion(ExecState*, JSObject*, JSValue, const ArgList&)
{
    // We need this function for compatibility with the Mozilla JS tests but for now
    // we don't actually do any version-specific handling
    return jsUndefined();
}

JSValue JSC_HOST_CALL functionRun(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    StopWatch stopWatch;
    UString fileName = args.at(0).toString(exec);
    Vector<char> script;
    if (!fillBufferWithContentsOfFile(fileName, script))
        return throwError(exec, GeneralError, "Could not open file.");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();

    stopWatch.start();
    evaluate(globalObject->globalExec(), globalObject->globalScopeChain(), makeSource(script.data(), fileName));
    stopWatch.stop();

    return jsNumber(globalObject->globalExec(), stopWatch.getElapsedMS());
}

JSValue JSC_HOST_CALL functionLoad(ExecState* exec, JSObject* o, JSValue v, const ArgList& args)
{
    UNUSED_PARAM(o);
    UNUSED_PARAM(v);
    UString fileName = args.at(0).toString(exec);
    Vector<char> script;
    if (!fillBufferWithContentsOfFile(fileName, script))
        return throwError(exec, GeneralError, "Could not open file.");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    Completion result = evaluate(globalObject->globalExec(), globalObject->globalScopeChain(), makeSource(script.data(), fileName));
    if (result.complType() == Throw)
        exec->setException(result.value());
    return result.value();
}

JSValue JSC_HOST_CALL functionCheckSyntax(ExecState* exec, JSObject* o, JSValue v, const ArgList& args)
{
    UNUSED_PARAM(o);
    UNUSED_PARAM(v);
    UString fileName = args.at(0).toString(exec);
    Vector<char> script;
    if (!fillBufferWithContentsOfFile(fileName, script))
        return throwError(exec, GeneralError, "Could not open file.");

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    Completion result = checkSyntax(globalObject->globalExec(), makeSource(script.data(), fileName));
    if (result.complType() == Throw)
        exec->setException(result.value());
    return result.value();
}

#if ENABLE(SAMPLING_FLAGS)
JSValue JSC_HOST_CALL functionSetSamplingFlags(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    for (unsigned i = 0; i < args.size(); ++i) {
        unsigned flag = static_cast<unsigned>(args.at(i).toNumber(exec));
        if ((flag >= 1) && (flag <= 32))
            SamplingFlags::setFlag(flag);
    }
    return jsNull();
}

JSValue JSC_HOST_CALL functionClearSamplingFlags(ExecState* exec, JSObject*, JSValue, const ArgList& args)
{
    for (unsigned i = 0; i < args.size(); ++i) {
        unsigned flag = static_cast<unsigned>(args.at(i).toNumber(exec));
        if ((flag >= 1) && (flag <= 32))
            SamplingFlags::clearFlag(flag);
    }
    return jsNull();
}
#endif

JSValue JSC_HOST_CALL functionReadline(ExecState* exec, JSObject*, JSValue, const ArgList&)
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

JSValue JSC_HOST_CALL functionQuit(ExecState* exec, JSObject*, JSValue, const ArgList&)
{
    // Technically, destroying the heap in the middle of JS execution is a no-no,
    // but we want to maintain compatibility with the Mozilla test suite, so
    // we pretend that execution has terminated to avoid ASSERTs, then tear down the heap.
    exec->globalData().dynamicGlobalObject = 0;

    cleanupGlobalData(&exec->globalData());
    exit(EXIT_SUCCESS);

#if COMPILER(MSVC) && OS(WINCE)
    // Without this, Visual Studio will complain that this method does not return a value.
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
#if defined(_DEBUG) && OS(WINDOWS)
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
#endif

#if COMPILER(MSVC) && !OS(WINCE)
    timeBeginPeriod(1);
#endif

#if PLATFORM(QT)
    QCoreApplication app(argc, argv);
#endif

    // Initialize JSC before getting JSGlobalData.
    JSC::initializeThreading();

    // We can't use destructors in the following code because it uses Windows
    // Structured Exception Handling
    int res = 0;
    JSGlobalData* globalData = JSGlobalData::create().releaseRef();
    TRY
        res = jscmain(argc, argv, globalData);
    EXCEPT(res = 3)

    cleanupGlobalData(globalData);
    return res;
}

static void cleanupGlobalData(JSGlobalData* globalData)
{
    JSLock lock(SilenceAssertionsOnly);
    globalData->heap.destroy();
    globalData->deref();
}

static bool runWithScripts(GlobalObject* globalObject, const Vector<Script>& scripts, bool dump)
{
    UString script;
    UString fileName;
    Vector<char> scriptBuffer;

    if (dump)
        BytecodeGenerator::setDumpsGeneratedCode(true);

    JSGlobalData* globalData = globalObject->globalData();

#if ENABLE(SAMPLING_FLAGS)
    SamplingFlags::start();
#endif

    bool success = true;
    for (size_t i = 0; i < scripts.size(); i++) {
        if (scripts[i].isFile) {
            fileName = scripts[i].argument;
            if (!fillBufferWithContentsOfFile(fileName, scriptBuffer))
                return false; // fail early so we can catch missing files
            script = scriptBuffer.data();
        } else {
            script = scripts[i].argument;
            fileName = "[Command Line]";
        }

        globalData->startSampling();

        Completion completion = evaluate(globalObject->globalExec(), globalObject->globalScopeChain(), makeSource(script, fileName));
        success = success && completion.complType() != Throw;
        if (dump) {
            if (completion.complType() == Throw)
                printf("Exception: %s\n", completion.value().toString(globalObject->globalExec()).ascii());
            else
                printf("End: %s\n", completion.value().toString(globalObject->globalExec()).ascii());
        }

        globalData->stopSampling();
        globalObject->globalExec()->clearException();
    }

#if ENABLE(SAMPLING_FLAGS)
    SamplingFlags::stop();
#endif
    globalData->dumpSampleData(globalObject->globalExec());
#if ENABLE(SAMPLING_COUNTERS)
    AbstractSamplingCounter::dump();
#endif
    return success;
}

#define RUNNING_FROM_XCODE 0

static void runInteractive(GlobalObject* globalObject)
{
    while (true) {
#if HAVE(READLINE) && !RUNNING_FROM_XCODE
        char* line = readline(interactivePrompt);
        if (!line)
            break;
        if (line[0])
            add_history(line);
        Completion completion = evaluate(globalObject->globalExec(), globalObject->globalScopeChain(), makeSource(line, interpreterName));
        free(line);
#else
        printf("%s", interactivePrompt);
        Vector<char, 256> line;
        int c;
        while ((c = getchar()) != EOF) {
            // FIXME: Should we also break on \r? 
            if (c == '\n')
                break;
            line.append(c);
        }
        if (line.isEmpty())
            break;
        line.append('\0');
        Completion completion = evaluate(globalObject->globalExec(), globalObject->globalScopeChain(), makeSource(line.data(), interpreterName));
#endif
        if (completion.complType() == Throw)
            printf("Exception: %s\n", completion.value().toString(globalObject->globalExec()).ascii());
        else
            printf("%s\n", completion.value().toString(globalObject->globalExec()).UTF8String().c_str());

        globalObject->globalExec()->clearException();
    }
    printf("\n");
}

static NO_RETURN void printUsageStatement(JSGlobalData* globalData, bool help = false)
{
    fprintf(stderr, "Usage: jsc [options] [files] [-- arguments]\n");
    fprintf(stderr, "  -d         Dumps bytecode (debug builds only)\n");
    fprintf(stderr, "  -e         Evaluate argument as script code\n");
    fprintf(stderr, "  -f         Specifies a source file (deprecated)\n");
    fprintf(stderr, "  -h|--help  Prints this help message\n");
    fprintf(stderr, "  -i         Enables interactive mode (default if no files are specified)\n");
#if HAVE(SIGNAL_H)
    fprintf(stderr, "  -s         Installs signal handlers that exit on a crash (Unix platforms only)\n");
#endif

    cleanupGlobalData(globalData);
    exit(help ? EXIT_SUCCESS : EXIT_FAILURE);
}

static void parseArguments(int argc, char** argv, Options& options, JSGlobalData* globalData)
{
    int i = 1;
    for (; i < argc; ++i) {
        const char* arg = argv[i];
        if (!strcmp(arg, "-f")) {
            if (++i == argc)
                printUsageStatement(globalData);
            options.scripts.append(Script(true, argv[i]));
            continue;
        }
        if (!strcmp(arg, "-e")) {
            if (++i == argc)
                printUsageStatement(globalData);
            options.scripts.append(Script(false, argv[i]));
            continue;
        }
        if (!strcmp(arg, "-i")) {
            options.interactive = true;
            continue;
        }
        if (!strcmp(arg, "-d")) {
            options.dump = true;
            continue;
        }
        if (!strcmp(arg, "-s")) {
#if HAVE(SIGNAL_H)
            signal(SIGILL, _exit);
            signal(SIGFPE, _exit);
            signal(SIGBUS, _exit);
            signal(SIGSEGV, _exit);
#endif
            continue;
        }
        if (!strcmp(arg, "--")) {
            ++i;
            break;
        }
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
            printUsageStatement(globalData, true);
        options.scripts.append(Script(true, argv[i]));
    }

    if (options.scripts.isEmpty())
        options.interactive = true;

    for (; i < argc; ++i)
        options.arguments.append(argv[i]);
}

int jscmain(int argc, char** argv, JSGlobalData* globalData)
{
    JSLock lock(SilenceAssertionsOnly);

    Options options;
    parseArguments(argc, argv, options, globalData);

    GlobalObject* globalObject = new (globalData) GlobalObject(options.arguments);
    bool success = runWithScripts(globalObject, options.scripts, options.dump);
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

    size_t bufferSize = 0;
    size_t bufferCapacity = 1024;

    buffer.resize(bufferCapacity);

    while (!feof(f) && !ferror(f)) {
        bufferSize += fread(buffer.data() + bufferSize, 1, bufferCapacity - bufferSize, f);
        if (bufferSize == bufferCapacity) { // guarantees space for trailing '\0'
            bufferCapacity *= 2;
            buffer.resize(bufferCapacity);
        }
    }
    fclose(f);
    buffer[bufferSize] = '\0';

    return true;
}
