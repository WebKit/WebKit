/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "Printer.h"

namespace JSC {
namespace Printer {

void printConstCharString(PrintStream& out, Context& context)
{
    const char* str = context.data.as<const char*>();
    out.print(str);
}

void printIntptr(PrintStream& out, Context& context)
{
    out.print(context.data.as<intptr_t>());
}

void printUintptr(PrintStream& out, Context& context)
{
    out.print(context.data.as<uintptr_t>());
}

void printPointer(PrintStream& out, Context& context)
{
    out.print(RawPointer(context.data.as<const void*>()));
}

void setPrinter(PrintRecord& record, CString&& string)
{
    // FIXME: It would be nice if we can release the CStringBuffer from the CString
    // and take ownership of it here instead of copying it again.
    record.data.pointer = fastStrDup(string.data());
    record.printer = printConstCharString;
}

} // namespace Printer
} // namespace JSC
