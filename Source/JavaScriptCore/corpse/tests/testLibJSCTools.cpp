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

#include <wtf/DataLog.h>

#if OS(MACOS) || USE(APPLE_INTERNAL_SDK)

#include "CorpseAddressTest.h"
#include "CorpseByteParserTest.h"
#include "CorpseExportsTrieTest.h"
#include "CorpseProcessTest.h"
#include "CorpseRegionTest.h"
#include "CorpseSnapshotTest.h"
#include "CorpseSymbolTest.h"
#include "CorpseThreadTest.h"
#include "LibJSCToolsTestUtilities.h"

#include <stdlib.h>
#include <string.h>
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
    dataLogLn("Usage: testLibJSCTools [<suite filter>]");
    dataLogLn("       testLibJSCTools --fuzz-trie [<seed> [<iterations>]]");
    dataLogLn("");
    dataLogLn("  Runs the tests for libJavaScriptCoreTools. With a filter, only the");
    dataLogLn("  suites whose name contains it run.");
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

    dataLogLn("Starting libJavaScriptCoreTools tests");

    if (fuzzOnly)
        JSCToolsTest::fuzzExportsTrie(fuzzSeed, static_cast<unsigned>(fuzzIterations));
    else {
        JSCToolsTest::testByteParser();
        JSCToolsTest::testExportsTrie();
        JSCToolsTest::fuzzExportsTrie(fuzzSeed, static_cast<unsigned>(fuzzIterations));
        JSCToolsTest::testAddress();
        JSCToolsTest::testProcess();
        JSCToolsTest::testSnapshot();
        JSCToolsTest::testRegion();
        JSCToolsTest::testThreads();
        JSCToolsTest::testSymbol();
    }

    dataLogLn("Ran ", JSCToolsTest::assertionsRun, " assertions, ",
        JSCToolsTest::assertionsFailed, " failed, ",
        JSCToolsTest::suitesSkipped, " suites skipped");

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

#else // !(OS(MACOS) || USE(APPLE_INTERNAL_SDK))

int main(int argc, char** argv)
{
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);

    // The corpse support is built on Mach task APIs, so there is nothing to test
    // on other platforms. Report success so that a run here is not a failure.
    dataLogLn("libJavaScriptCoreTools tests are disabled (Mach task APIs are not available)");
    return 0;
}

#endif // OS(MACOS) || USE(APPLE_INTERNAL_SDK)
