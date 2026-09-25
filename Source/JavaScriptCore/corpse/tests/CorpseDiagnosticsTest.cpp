/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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

#if ENABLE(MYA)

#include "LibJSCToolsTestUtilities.h"

#include <JavaScriptCore/CorpseClient.h>
#include <JavaScriptCore/CorpseError.h>
#include <wtf/StringPrintStream.h>

namespace JSCToolsTest {

using JSC::Corpse::Client;
using JSC::Corpse::DiagnosticCounter;
using JSC::Corpse::Diagnostics;

namespace {

void readHeader()
{
    Diagnostics::count(DiagnosticCounter::ImageHeadersRead);
}

} // anonymous namespace

void testDiagnostics()
{
    SuiteTracer tracer("Diagnostics");
    if (!tracer.shouldRun())
        return;

    readHeader(); // With no scope open, a count goes nowhere.

    CORPSE_DIAGNOSTICS(outer, "running the %s suite", "Diagnostics");
    TEST_ASSERT_EQ(outer.value(DiagnosticCounter::ImageHeadersRead), uint64_t { 0 }, "a count made before the scope opened is not in it");
    readHeader();
    Diagnostics::count(DiagnosticCounter::ImagesListed, 40);
    Diagnostics::count(DiagnosticCounter::ExportsTriesSearched);
    TEST_ASSERT_EQ(outer.value(DiagnosticCounter::ImageHeadersRead), uint64_t { 1 }, "a count reaches its scope from a callee");
    TEST_ASSERT_EQ(outer.value(DiagnosticCounter::ImagesListed), uint64_t { 40 }, "a count adds what it is given");

    {
        CORPSE_DIAGNOSTICS(middle, "doing something in between");
        CORPSE_DIAGNOSTICS(inner, "reading more headers");
        readHeader();
        TEST_ASSERT_EQ(inner.value(DiagnosticCounter::ImageHeadersRead), uint64_t { 1 }, "the innermost scope takes a count");
        TEST_ASSERT_EQ(middle.value(DiagnosticCounter::ImageHeadersRead), uint64_t { 0 }, "an enclosing scope does not also take it");
        TEST_ASSERT_EQ(outer.value(DiagnosticCounter::ImageHeadersRead), uint64_t { 1 }, "nor does the outermost one");

        TEST_ASSERT_EQ(Diagnostics::context(), toUTF8CString(
            Client::name(), ":   while reading more headers: image headers read 1\n"_s,
            Client::name(), ":   while doing something in between\n"_s,
            Client::name(), ":   while running the Diagnostics suite: images listed 40, image headers read 1, exports tries searched 1\n"_s),
            "the context names every open scope, innermost first, with the counts that are not zero");
    }

    readHeader();
    TEST_ASSERT_EQ(outer.value(DiagnosticCounter::ImageHeadersRead), uint64_t { 2 }, "counting goes back to the outer scope once the inner ones close");
}

} // namespace JSCToolsTest

#endif // ENABLE(MYA)
