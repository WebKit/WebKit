/*
 *  Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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
#include "RegExp.h"

#include "InitializeThreading.h"
#include "JSCInlines.h"
#include "YarrFlags.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>

#if COMPILER(MSVC)
#include <crtdbg.h>
#include <mmsystem.h>
#include <windows.h>
#endif

const int MaxLineLength = 100 * 1024;

using namespace JSC;

struct CommandLine {
    CommandLine()
        : interactive(false)
        , verbose(false)
    {
    }

    bool interactive;
    bool verbose;
    Vector<String> arguments;
    Vector<String> files;
};

class StopWatch {
public:
    void start();
    void stop();
    long getElapsedMS(); // call stop() first

private:
    MonotonicTime m_startTime;
    MonotonicTime m_stopTime;
};

void StopWatch::start()
{
    m_startTime = MonotonicTime::now();
}

void StopWatch::stop()
{
    m_stopTime = MonotonicTime::now();
}

long StopWatch::getElapsedMS()
{
    return (m_stopTime - m_startTime).millisecondsAs<long>();
}

struct RegExpTest {
    RegExpTest()
        : offset(0)
        , result(0)
    {
    }

    String subject;
    int offset;
    int result;
    Vector<int, 32> expectVector;
};

class GlobalObject final : public JSGlobalObject {
public:
    using Base = JSGlobalObject;

    static GlobalObject* create(VM& vm, Structure* structure, const Vector<String>& arguments)
    {
        GlobalObject* globalObject = new (NotNull, allocateCell<GlobalObject>(vm)) GlobalObject(vm, structure, arguments);
        return globalObject;
    }

    DECLARE_INFO;

    static constexpr bool needsDestructor = true;

    static Structure* createStructure(VM& vm, JSValue prototype)
    {
        return Structure::create(vm, nullptr, prototype, TypeInfo(GlobalObjectType, StructureFlags), info());
    }

private:
    GlobalObject(VM&, Structure*, const Vector<String>& arguments);

    void finishCreation(VM& vm, const Vector<String>& arguments)
    {
        Base::finishCreation(vm);
        UNUSED_PARAM(arguments);
    }
};
STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(GlobalObject, JSGlobalObject);

const ClassInfo GlobalObject::s_info = { "global", &JSGlobalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(GlobalObject) };

GlobalObject::GlobalObject(VM& vm, Structure* structure, const Vector<String>& arguments)
    : JSGlobalObject(vm, structure)
{
    finishCreation(vm, arguments);
}

// Use SEH for Release builds only to get rid of the crash report dialog
// (luckily the same tests fail in Release and Debug builds so far). Need to
// be in a separate main function because the realMain function requires object
// unwinding.

#if COMPILER(MSVC) && !defined(_DEBUG)
#define TRY       __try {
#define EXCEPT(x) } __except (EXCEPTION_EXECUTE_HANDLER) { x; }
#else
#define TRY
#define EXCEPT(x)
#endif

int realMain(int argc, char** argv);

int main(int argc, char** argv)
{
#if OS(WINDOWS)
    // Cygwin calls ::SetErrorMode(SEM_FAILCRITICALERRORS), which we will inherit. This is bad for
    // testing/debugging, as it causes the post-mortem debugger not to be invoked. We reset the
    // error mode here to work around Cygwin's behavior. See <http://webkit.org/b/55222>.
    ::SetErrorMode(0);

#if defined(_DEBUG)
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
#endif

    timeBeginPeriod(1);
#endif

    JSC::initialize();

    // We can't use destructors in the following code because it uses Windows
    // Structured Exception Handling
    int res = 0;
    TRY
        res = realMain(argc, argv);
    EXCEPT(res = 3)
    return res;
}

static bool testOneRegExp(JSGlobalObject* globalObject, RegExp* regexp, RegExpTest* regExpTest, bool verbose, unsigned lineNumber)
{
    bool result = true;
    Vector<int> outVector;
    outVector.resize(regExpTest->expectVector.size());
    int matchResult = regexp->match(globalObject, regExpTest->subject, regExpTest->offset, outVector);

    if (matchResult != regExpTest->result) {
        result = false;
        if (verbose)
            printf("Line %d: results mismatch - expected %d got %d\n", lineNumber, regExpTest->result, matchResult);
    } else if (matchResult != -1) {
        if (outVector.size() != regExpTest->expectVector.size()) {
            result = false;
            if (verbose) {
#if OS(WINDOWS)
                printf("Line %d: output vector size mismatch - expected %Iu got %Iu\n", lineNumber, regExpTest->expectVector.size(), outVector.size());
#else
                printf("Line %d: output vector size mismatch - expected %zu got %zu\n", lineNumber, regExpTest->expectVector.size(), outVector.size());
#endif
            }
        } else if (outVector.size() % 2) {
            result = false;
            if (verbose) {
#if OS(WINDOWS)
                printf("Line %d: output vector size is odd (%Iu), should be even\n", lineNumber, outVector.size());
#else
                printf("Line %d: output vector size is odd (%zu), should be even\n", lineNumber, outVector.size());
#endif
            }
        } else {
            // Check in pairs since the first value of the pair could be -1 in which case the second doesn't matter.
            size_t pairCount = outVector.size() / 2;
            for (size_t i = 0; i < pairCount; ++i) {
                size_t startIndex = i*2;
                if (outVector[startIndex] != regExpTest->expectVector[startIndex]) {
                    result = false;
                    if (verbose) {
#if OS(WINDOWS)
                        printf("Line %d: output vector mismatch at index %Iu - expected %d got %d\n", lineNumber, startIndex, regExpTest->expectVector[startIndex], outVector[startIndex]);
#else
                        printf("Line %d: output vector mismatch at index %zu - expected %d got %d\n", lineNumber, startIndex, regExpTest->expectVector[startIndex], outVector[startIndex]);
#endif
                    }
                }
                if ((i > 0) && (regExpTest->expectVector[startIndex] != -1) && (outVector[startIndex+1] != regExpTest->expectVector[startIndex+1])) {
                    result = false;
                    if (verbose) {
#if OS(WINDOWS)
                        printf("Line %d: output vector mismatch at index %Iu - expected %d got %d\n", lineNumber, startIndex + 1, regExpTest->expectVector[startIndex + 1], outVector[startIndex + 1]);
#else
                        printf("Line %d: output vector mismatch at index %zu - expected %d got %d\n", lineNumber, startIndex + 1, regExpTest->expectVector[startIndex + 1], outVector[startIndex + 1]);
#endif
                    }
                }
            }
        }
    }

    return result;
}

static int scanString(char* buffer, int bufferLength, StringBuilder& builder, char termChar)
{
    bool escape = false;
    
    for (int i = 0; i < bufferLength; ++i) {
        UChar c = buffer[i];
        
        if (escape) {
            switch (c) {
            case '0':
                c = '\0';
                break;
            case 'a':
                c = '\a';
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'v':
                c = '\v';
                break;
            case '\\':
                c = '\\';
                break;
            case '?':
                c = '\?';
                break;
            case 'u':
                if ((i + 4) >= bufferLength)
                    return -1;
                unsigned int charValue;
                if (sscanf(buffer+i+1, "%04x", &charValue) != 1)
                    return -1;
                c = static_cast<UChar>(charValue);
                i += 4;
                break;
            }
            
            builder.append(c);
            escape = false;
        } else {
            if (c == termChar)
                return i;

            if (c == '\\')
                escape = true;
            else
                builder.append(c);
        }
    }

    return -1;
}

static RegExp* parseRegExpLine(VM& vm, char* line, int lineLength, const char** regexpError)
{
    StringBuilder pattern;

    if (line[0] != '/')
        return nullptr;

    int i = scanString(line + 1, lineLength - 1, pattern, '/') + 1;

    if ((i >= lineLength) || (line[i] != '/'))
        return nullptr;

    ++i;

    auto flags = Yarr::parseFlags(line + i);
    if (!flags) {
        *regexpError = Yarr::errorMessage(Yarr::ErrorCode::InvalidRegularExpressionFlags);
        return nullptr;
    }

    RegExp* r = RegExp::create(vm, pattern.toString(), flags.value());
    if (!r->isValid()) {
        *regexpError = r->errorMessage();
        return nullptr;
    }

    return r;
}

static RegExpTest* parseTestLine(char* line, int lineLength)
{
    StringBuilder subjectString;
    
    if ((line[0] != ' ') || (line[1] != '"'))
        return nullptr;

    int i = scanString(line + 2, lineLength - 2, subjectString, '"') + 2;

    if ((i >= (lineLength - 2)) || (line[i] != '"') || (line[i+1] != ',') || (line[i+2] != ' '))
        return nullptr;

    i += 3;
    
    int offset;
    
    if (sscanf(line + i, "%d, ", &offset) != 1)
        return nullptr;

    while (line[i] && line[i] != ' ')
        ++i;

    ++i;
    
    int matchResult;
    
    if (sscanf(line + i, "%d, ", &matchResult) != 1)
        return nullptr;
    
    while (line[i] && line[i] != ' ')
        ++i;
    
    ++i;
    
    if (line[i++] != '(')
        return nullptr;

    int start, end;
    
    RegExpTest* result = new RegExpTest();
    
    result->subject = subjectString.toString();
    result->offset = offset;
    result->result = matchResult;

    while (line[i] && line[i] != ')') {
        if (sscanf(line + i, "%d, %d", &start, &end) != 2) {
            delete result;
            return nullptr;
        }

        result->expectVector.append(start);
        result->expectVector.append(end);

        while (line[i] && (line[i] != ',') && (line[i] != ')'))
            i++;
        i++;
        while (line[i] && (line[i] != ',') && (line[i] != ')'))
            i++;

        if (line[i] == ')')
            break;
        if (!line[i] || (line[i] != ',')) {
            delete result;
            return nullptr;
        }
        i++;
    }

    return result;
}

static bool runFromFiles(GlobalObject* globalObject, const Vector<String>& files, bool verbose)
{
    String script;
    String fileName;
    Vector<char> scriptBuffer;
    unsigned tests = 0;
    unsigned failures = 0;
    Vector<char> lineBuffer(MaxLineLength + 1);

    VM& vm = globalObject->vm();

    bool success = true;
    for (size_t i = 0; i < files.size(); i++) {
        FILE* testCasesFile = fopen(files[i].utf8().data(), "rb");

        if (!testCasesFile) {
            printf("Unable to open test data file \"%s\"\n", files[i].utf8().data());
            continue;
        }
            
        RegExp* regexp = nullptr;
        size_t lineLength = 0;
        char* linePtr = nullptr;
        unsigned int lineNumber = 0;
        const char* regexpError = nullptr;

        while ((linePtr = fgets(lineBuffer.data(), MaxLineLength, testCasesFile))) {
            lineLength = strlen(linePtr);
            if (linePtr[lineLength - 1] == '\n') {
                linePtr[lineLength - 1] = '\0';
                --lineLength;
            }
            ++lineNumber;

            if (linePtr[0] == '#')
                continue;

            if (linePtr[0] == '/') {
                regexp = parseRegExpLine(vm, linePtr, lineLength, &regexpError);
                if (!regexp) {
                    failures++;
                    fprintf(stderr, "Failure on line %u. '%s' %s\n", lineNumber, linePtr, regexpError);
                }
            } else if (linePtr[0] == ' ') {
                RegExpTest* regExpTest = parseTestLine(linePtr, lineLength);
                
                if (regexp && regExpTest) {
                    ++tests;
                    if (!testOneRegExp(globalObject, regexp, regExpTest, verbose, lineNumber)) {
                        failures++;
                        printf("Failure on line %u\n", lineNumber);
                    }
                }
                
                if (regExpTest)
                    delete regExpTest;
            } else if (linePtr[0] == '-') {
                tests++;
                regexp = nullptr; // Reset the live regexp to avoid confusing other subsequent tests
                bool successfullyParsed = parseRegExpLine(vm, linePtr + 1, lineLength - 1, &regexpError);
                if (successfullyParsed) {
                    failures++;
                    fprintf(stderr, "Failure on line %u. '%s' %s\n", lineNumber, linePtr + 1, regexpError);
                }
            }
        }
        
        fclose(testCasesFile);
    }

    if (failures)
        printf("%u tests run, %u failures\n", tests, failures);
    else
        printf("%u tests passed\n", tests);

#if ENABLE(REGEXP_TRACING)
    vm.dumpRegExpTrace();
#endif
    return success;
}

#define RUNNING_FROM_XCODE 0

static NO_RETURN void printUsageStatement(bool help = false)
{
    fprintf(stderr, "Usage: regexp_test [options] file\n");
    fprintf(stderr, "  -h|--help  Prints this help message\n");
    fprintf(stderr, "  -v|--verbose  Verbose output\n");

    exit(help ? EXIT_SUCCESS : EXIT_FAILURE);
}

static void parseArguments(int argc, char** argv, CommandLine& options)
{
    int i = 1;
    for (; i < argc; ++i) {
        const char* arg = argv[i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
            printUsageStatement(true);
        if (!strcmp(arg, "-v") || !strcmp(arg, "--verbose"))
            options.verbose = true;
        else
            options.files.append(argv[i]);
    }

    for (; i < argc; ++i)
        options.arguments.append(argv[i]);
}

int realMain(int argc, char** argv)
{
    VM* vm = &VM::create(HeapType::Large).leakRef();
    JSLockHolder locker(vm);

    CommandLine options;
    parseArguments(argc, argv, options);

    GlobalObject* globalObject = GlobalObject::create(*vm, GlobalObject::createStructure(*vm, jsNull()), options.arguments);
    bool success = runFromFiles(globalObject, options.files, options.verbose);

    return success ? 0 : 3;
}

#if OS(WINDOWS)
extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(int argc, const char* argv[])
{
    return main(argc, const_cast<char**>(argv));
}
#endif
