/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "Test.h"
#include <wtf/StackTrace.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/unicode/CharacterNames.h>

#if ((PLATFORM(GTK) || PLATFORM(WPE)) && defined(NDEBUG)) || (PLATFORM(WIN) && !defined(NDEBUG))
#define MAYBE(name) DISABLED_##name
#else
#define MAYBE(name) name
#endif

namespace TestWebKitAPI {

TEST(StackTraceTest, MAYBE(StackTraceWorks))
{
    static constexpr int framesToShow = 31;
    void* samples[framesToShow];
    int frames = framesToShow;
    WTFGetBacktrace(samples, &frames);
    StackTracePrinter stackTrace { { samples, static_cast<size_t>(frames) } };
    StringPrintStream out;
    out.print(stackTrace);
    auto results = out.toString().split(newlineCharacter);
    ASSERT_GT(results.size(), 2u);
    EXPECT_TRUE(results[0].contains("WTFGetBacktrace"_s));
    EXPECT_TRUE(results[1].contains("StackTraceWorks"_s));
}

namespace {
struct Frame {
    int number;
    void* address;
    String name;
};
}

static void expectEqualFrames(Span<void* const> expectedStack, StackTrace& testedStack, bool skipFirstFrameAddress)
{
    Vector<Frame> expected;
    StackTraceSymbolResolver { expectedStack }.forEach([&expected](int number, void* address, const char* name) {
        expected.append({ number, address, String::fromLatin1(name) });
    });
    Vector<Frame> tested;
    StackTraceSymbolResolver { testedStack }.forEach([&tested](int number, void* address, const char* name) {
        tested.append({ number, address, String::fromLatin1(name) });
    });
    ASSERT_EQ(expected.size(), tested.size());

    for (size_t j = 0; j < expected.size(); ++j) {
        EXPECT_EQ(expected[j].number, tested[j].number) << " j:" << j;
        EXPECT_EQ(expected[j].name, tested[j].name) << " j:" << j << " " << expected[j].name << " != " << tested[j].name;
        // Address of tested capture call in vs the verification capture call is different.
        if (skipFirstFrameAddress && !j)
            continue;
        EXPECT_EQ(expected[j].address, tested[j].address) << "j:" << j;
    }
}

// Test that captureStackTrace() returns [a, b, c, ...].
// Test that captureStackTrace(limit, skip) is able to respect the requested limit and skip.
TEST(StackTraceTest, MAYBE(CaptureStackTraceLimitAndSkip))
{
    // Verify captureStackTrace() results by manually inspecting WTFGetBacktrace().
    // WTFGetBacktrace() returns [WTFGetBacktrace, a, b, c, ...].
    static constexpr int maxFramesToCapture = 100;
    void* stack[maxFramesToCapture];
    int frames = maxFramesToCapture;
    WTFGetBacktrace(stack, &frames);
    // Skip WTFGetBacktrace.
    --frames;
    void** expectedStack = stack + 1;

    // Test odd case where StackTrace::captureStackTrace(0) returns 1 result.
    {
        Span<void* const> expected { expectedStack, 1 };
        {
            SCOPED_TRACE("Zero returns one result case");
            auto tested = StackTrace::captureStackTrace(0);
            expectEqualFrames(expected, *tested, true);
        }
        {
            SCOPED_TRACE("Zero returns one result case example 1");
            // The above is odd because using 0 is the same as using 1.
            auto tested = StackTrace::captureStackTrace(1);
            expectEqualFrames(expected, *tested, true);
        }
    }

    // Test limiting the frame capture depth to `i = 1..` works.
    // Test also 7 more than what's really available.
    size_t testedMaxFrames = static_cast<size_t>(frames + 7);
    for (size_t maxFrames = 1; maxFrames < testedMaxFrames; ++maxFrames) {
        for (size_t skipFrames = 0; skipFrames < testedMaxFrames; ++skipFrames) {
            SCOPED_TRACE(makeString("maxFrames: ", maxFrames, " skipFrames: ", skipFrames));
            size_t expectedFrameCount = std::min(maxFrames + skipFrames, static_cast<size_t>(frames));
            size_t expectedSkipFrames = std::min(skipFrames, expectedFrameCount);
            Span<void* const> expected { expectedStack + expectedSkipFrames, expectedFrameCount - expectedSkipFrames };

            auto tested = StackTrace::captureStackTrace(maxFrames, skipFrames);

            bool shouldSkipFirstFrameAddressComparison = !skipFrames;
            expectEqualFrames(expected, *tested, shouldSkipFirstFrameAddressComparison);
        }
    }
}

}

#undef MAYBE
