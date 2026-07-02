/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include <wtf/PrintStream.h>

#include <inttypes.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

static constexpr size_t stringLengthThresholdToTriggerTruncation = 5000000;
static constexpr size_t stringLengthToTruncateToForPrinting = 1000;

PrintStream::PrintStream() = default;
PrintStream::~PrintStream() = default; // Force the vtable to be in this module

void PrintStream::printf(const char* format, ...)
{
    va_list argList;
    va_start(argList, format);
    vprintf(format, argList);
    va_end(argList);
}

void PrintStream::printfVariableFormat(const char* format, ...)
{
ALLOW_NONLITERAL_FORMAT_BEGIN
IGNORE_GCC_WARNINGS_BEGIN("suggest-attribute=format")
    va_list argList;
    va_start(argList, format);
    vprintf(format, argList);
    va_end(argList);
IGNORE_GCC_WARNINGS_END
ALLOW_NONLITERAL_FORMAT_END
}

void PrintStream::flush()
{
}

PrintStream& PrintStream::begin()
{
    return *this;
}

void PrintStream::end()
{
}

void printInternal(PrintStream& out, const char* string)
{
    out.printf("%s", string);
}

static void printExpectedCStringHelper(PrintStream& out, const char* type, Expected<CString, UTF8ConversionError> expectedCString)
{
    if (!expectedCString) [[unlikely]] {
        if (expectedCString.error() == UTF8ConversionError::OutOfMemory) {
            printInternal(out, "(Out of memory while converting ");
            printInternal(out, type);
            printInternal(out, " to utf8)");
        } else {
            printInternal(out, "(failed to convert ");
            printInternal(out, type);
            printInternal(out, " to utf8)");
        }
        return;
    }
    printInternal(out, expectedCString.value());
}

void printInternal(PrintStream& out, StringView string)
{
    printExpectedCStringHelper(out, "StringView", string.tryGetUTF8());
}

void printInternal(PrintStream& out, const CString& string)
{
    if (string.length() > stringLengthThresholdToTriggerTruncation) [[unlikely]] {
        size_t lengthNotPrinted = string.length() - stringLengthToTruncateToForPrinting;
        auto subString = makeString(string.span().first(stringLengthToTruncateToForPrinting), "...["_s, lengthNotPrinted, " characters not shown]"_s);
        printInternal(out, subString.utf8().data());
        return;
    }
    printInternal(out, string.data());
}

void printInternal(PrintStream& out, const String& string)
{
    printExpectedCStringHelper(out, "String", string.tryGetUTF8());
}

void printInternal(PrintStream& out, const AtomString& string)
{
    printExpectedCStringHelper(out, "String", string.string().tryGetUTF8());
}

void printInternal(PrintStream& out, const StringImpl* string)
{
    if (!string) {
        printInternal(out, "(null StringImpl*)");
        return;
    }
    printExpectedCStringHelper(out, "StringImpl*", string->tryGetUTF8());
}

void printInternal(PrintStream& stream, std::span<const char8_t> codeUnits)
{
    printInternal(stream, byteCast<char>(codeUnits));
}

void printInternal(PrintStream& out, bool value)
{
    out.print(boolForPrinting(value));
}

void printInternal(PrintStream& out, int value)
{
    out.printf("%d", value);
}

void printInternal(PrintStream& out, unsigned value)
{
    out.printf("%u", value);
}

void printInternal(PrintStream& out, char value)
{
    out.printf("%c", value);
}

void printInternal(PrintStream& out, signed char value)
{
    out.printf("%d", static_cast<int>(value));
}

void printInternal(PrintStream& out, unsigned char value)
{
    out.printf("%u", static_cast<unsigned>(value));
}

void printInternal(PrintStream& out, char16_t value)
{
    out.printf("%lc", static_cast<wint_t>(value));
}

void printInternal(PrintStream& out, char32_t value)
{
    // Print each char32_t as an integer.
    out.printf("%u", static_cast<unsigned>(value));
}

void printInternal(PrintStream& out, short value)
{
    out.printf("%d", static_cast<int>(value));
}

void printInternal(PrintStream& out, unsigned short value)
{
    out.printf("%u", static_cast<unsigned>(value));
}

void printInternal(PrintStream& out, long value)
{
    out.printf("%ld", value);
}

void printInternal(PrintStream& out, unsigned long value)
{
    out.printf("%lu", value);
}

void printInternal(PrintStream& out, long long value)
{
    out.printf("%lld", value);
}

void printInternal(PrintStream& out, unsigned long long value)
{
    out.printf("%llu", value);
}

void printInternal(PrintStream& out, float value)
{
    printInternal(out, static_cast<double>(value));
}

void printInternal(PrintStream& out, double value)
{
    out.printf("%lf", value);
}

void printInternal(PrintStream& out, RawHex value)
{
#if !CPU(ADDRESS64)
    if (value.is64Bit()) {
        out.printf("0x%" PRIx64, value.u64());
        return;
    }
#endif
#if OS(WINDOWS)
    out.printf("0x%p", value.ptr());
#else
    out.printf("%p", value.ptr());
#endif
}

void printInternal(PrintStream& out, RawPointer value)
{
#if OS(WINDOWS)
    out.printf("0x%p", value.value());
#else
    out.printf("%p", value.value());
#endif
}

void printInternal(PrintStream& out, MemoryDump value)
{
    auto span = value.span();
    auto sizeLimit = value.sizeLimit();

    out.printf("\n");
    if (span.data() == nullptr) [[unlikely]] {
        out.printf("%08" PRIxPTR ": (not dumping %zu bytes)", reinterpret_cast<uintptr_t>(span.data()), span.size());
        return;
    }
    if (span.empty()) [[unlikely]] {
        out.printf("%08" PRIxPTR ": (span is empty)", reinterpret_cast<uintptr_t>(span.data()));
        return;
    }

    for (size_t i = 0; i < span.size(); i += 16) {
        if (i >= sizeLimit) {
            size_t remainder = span.size() - i;
            out.printf("... (remaining %zu bytes not dumped)\n", remainder);
            break;
        }

        // Print address
        out.printf("%08" PRIxPTR ": ", reinterpret_cast<uintptr_t>(span.data() + i));

        // Print hex bytes
        for (size_t byteIndex = 0; byteIndex < 16; ++byteIndex) {
            if (i + byteIndex < span.size())
                out.printf("%02x ", static_cast<uint8_t>(span[i + byteIndex]));
            else
                out.printf("   ");
        }

        // Print ASCII interpretation
        out.printf(" ");
        for (size_t byteIndex = 0; byteIndex < 16 && i + byteIndex < span.size(); ++byteIndex) {
            std::byte byte = span[i + byteIndex];
            uint8_t byteValue = static_cast<uint8_t>(byte);
            char ch = (byteValue >= 32 && byteValue <= 126) ? static_cast<char>(byteValue) : '.';
            out.printf("%c", ch);
        }
        if (i + 16 < span.size())
            out.printf("\n");
    }
}

void printInternal(PrintStream& out, FixedWidthDouble value)
{
    out.printf("%*.*lf", value.width(), value.precision(), value.value());
}

void dumpCharacter(PrintStream& out, char value)
{
    out.printf("%c", value);
}

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
