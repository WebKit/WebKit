/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <JavaScriptCore/CorpsePlatform.h>
#include <wtf/DataLog.h>

#if ENABLE(MYA)

#include "LibJSCToolsTestUtilities.h"

#include <algorithm>
#include <bmalloc/bmalloc.h>
#include <memory>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <string_view>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/StringView.h>

namespace {

// A default that runs in well under a second, so that every run exercises the
// decoder on inputs nobody wrote. A longer hunt is a matter of passing a bigger
// count and a different seed.
constexpr uint64_t defaultFuzzSeed = 0x5eed1234;
constexpr unsigned defaultFuzzIterations = 20000;

void printUsage()
{
    dataLogLn("Usage: testLibJSCTools [--verbose] [--webcore] [<suite filter>]");
    dataLogLn("       testLibJSCTools --fuzz-trie [<seed> [<iterations>]]");
    dataLogLn("       testLibJSCTools --target <offset>");
    dataLogLn("");
    dataLogLn("  Runs the tests for libJavaScriptCoreTools. With a filter, only the");
    dataLogLn("  suites whose name contains it run. --webcore also describes every class of");
    dataLogLn("  the WebCore beside JavaScriptCore, which takes minutes and gigabytes.");
}

bool parseUint64(std::string_view text, uint64_t& out)
{
    uint8_t base = text.starts_with("0x") || text.starts_with("0X") ? 16 : 10;
    if (base == 16)
        text = text.substr(2);
    auto parsed = WTF::parseInteger<uint64_t>(StringView::fromLatin1(std::string(text).c_str()), base);
    if (!parsed)
        return false;
    out = *parsed;
    return true;
}

void runCorpseSuite()
{
    JSCToolsTest::testByteParser();
    JSCToolsTest::testDiagnostics();
    JSCToolsTest::testExportsTrie();
    JSCToolsTest::testAddress();
    JSCToolsTest::testProcess();
    JSCToolsTest::testSnapshot();
    JSCToolsTest::testRegion();
    JSCToolsTest::testMemory();
    JSCToolsTest::testThreads();
    JSCToolsTest::testSymbol();
}

// Targets run in other working directories, where a relative entry names other libraries.
void makeLibraryPathAbsolute()
{
#if OS(DARWIN)
    static constexpr const char* variable = "DYLD_FRAMEWORK_PATH";
#else
    static constexpr const char* variable = "LD_LIBRARY_PATH";
#endif
    const char* value = getenv(variable);
    if (!value)
        return;
    std::string_view entries { value };
    std::string absolute;
    for (size_t start = 0; start <= entries.size();) {
        size_t end = std::min(entries.find(':', start), entries.size());
        std::string entry { entries.substr(start, end - start) };
        std::unique_ptr<char, decltype(&free)> resolved { entry.empty() ? nullptr : realpath(entry.c_str(), nullptr), &free };
        if (start)
            absolute += ':';
        absolute += resolved ? std::string { resolved.get() } : entry;
        start = end + 1;
    }
    RELEASE_ASSERT(!setenv(variable, absolute.c_str(), 1));
}

} // anonymous namespace

int main(int argc, char** argv)
{
    uint64_t fuzzSeed = defaultFuzzSeed;
    uint64_t fuzzIterations = defaultFuzzIterations;
    bool fuzzOnly = false;

    // argv is wrapped in a span so that nothing here walks off the end of it.
    auto arguments = unsafeMakeSpan(argv, static_cast<size_t>(argc));
    for (size_t index = 1; index < arguments.size(); ++index) {
        std::string_view argument = arguments[index];
        if (argument == "--help" || argument == "-h") {
            printUsage();
            return 0;
        }
        if (argument == "--verbose") {
            JSCToolsTest::verbose = true;
            continue;
        }
        if (argument == "--webcore") {
            JSCToolsTest::describeWebCore = true;
            continue;
        }
        if (argument == "--target") {
            if (index + 1 >= arguments.size()) {
                dataLogLn("--target needs the offset of a create function into this executable, in hex");
                printUsage();
                return 1;
            }
            return JSCToolsTest::runCorpseTarget(arguments[index + 1]);
        }
        if (argument == "--fuzz-trie") {
            fuzzOnly = true;
            if (index + 1 < arguments.size() && parseUint64(arguments[index + 1], fuzzSeed)) {
                ++index;
                if (index + 1 < arguments.size() && parseUint64(arguments[index + 1], fuzzIterations))
                    ++index;
            }
            continue;
        }
        if (argument.starts_with("-")) {
            dataLogLn("Unknown option '", arguments[index], "'");
            printUsage();
            return 1;
        }
        JSCToolsTest::suiteFilter = arguments[index];
    }

    bmalloc::api::disableScavenger();
    makeLibraryPathAbsolute();

    dataLogLn("Starting libJavaScriptCoreTools tests");

    if (fuzzOnly)
        JSCToolsTest::fuzzExportsTrie(fuzzSeed, static_cast<unsigned>(fuzzIterations));
    else {
        JSCToolsTest::fuzzExportsTrie(fuzzSeed, static_cast<unsigned>(fuzzIterations));
        runCorpseSuite();
        JSCToolsTest::testTypeinfo();
        JSCToolsTest::testDebugInfo();
        JSCToolsTest::testHeapWalk();
        JSCToolsTest::testVM();
    }

    dataLogLn("Ran ", JSCToolsTest::assertionsRun, " assertions, ",
        JSCToolsTest::assertionsFailed, " failed, ",
        JSCToolsTest::suitesSkipped, " suites skipped, in ",
        static_cast<uint64_t>(JSCToolsTest::totalSuiteTime().milliseconds()), " ms");

    if (JSCToolsTest::assertionsFailed) {
        dataLogLn("Some libJavaScriptCoreTools tests FAILED!");
        return 1;
    }
    if (!JSCToolsTest::assertionsRun) {
        dataLogLn("No tests ran!");
        return 1;
    }

    dataLogLn("All libJavaScriptCoreTools tests PASSED!");
    return 0;
}

#else // libJavaScriptCoreTools support unavailable

int main(int, char**)
{
    // Report success so that a run here is not a failure.
    printf("Not supported platform for testLibJSCTools\n");
    return 0;
}

#endif // ENABLE(MYA)
