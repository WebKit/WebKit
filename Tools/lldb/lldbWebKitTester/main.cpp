/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "DumpClassLayoutTesting.h"
#include <stdio.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

extern void breakForTestingSummaryProviders();
void breakForTestingSummaryProviders() { return; }

template<size_t length>
static String utf16String(const char16_t (&string)[length])
{
    StringBuilder builder;
    builder.reserveCapacity(length - 1);
    for (auto c : string)
        builder.append(static_cast<UChar>(c));
    return builder.toString();
}

enum class ExampleFlags {
    A = 1 << 0,
    B = 1 << 1,
    C = 1 << 2,
    D = 1 << 3,
    AAlias = A,
};

static void testSummaryProviders()
{
    String aNullString { "" };
    StringImpl* aNullStringImpl = aNullString.impl();

    String anEmptyString { "" };
    StringImpl* anEmptyStringImpl = anEmptyString.impl();

    String an8BitString { "résumé" };
    StringImpl* an8BitStringImpl = an8BitString.impl();

    String a16BitString = utf16String(u"\u1680Cappuccino\u1680");
    StringImpl* a16BitStringImpl = a16BitString.impl();

    Vector<int> anEmptyVector;
    Vector<int> aVectorWithOneItem;
    aVectorWithOneItem.reserveCapacity(16);
    aVectorWithOneItem.append(1);

    HashMap<unsigned, int> hashMapOfInts;
    hashMapOfInts.add(12, 23);
    hashMapOfInts.add(34, 45);

    HashSet<unsigned> hashSetOfInts;
    hashSetOfInts.add(42);

    HashMap<unsigned, Vector<int>> hashMapOfVectors;
    hashMapOfVectors.add(1, Vector<int>({2, 3}));

    OptionSet<ExampleFlags> exampleFlagsEmpty;
    OptionSet<ExampleFlags> exampleFlagsSimple { ExampleFlags::A, ExampleFlags::D, ExampleFlags::C };
    OptionSet<ExampleFlags> exampleFlagsAliasedFlag { ExampleFlags::AAlias, ExampleFlags::D };

    breakForTestingSummaryProviders();
}

int main(int argc, const char* argv[])
{
    avoidClassDeadStripping();
    testSummaryProviders();
    fprintf(stderr, "This executable does nothing and is only meant for debugging lldb_webkit.py.\n");
    return 0;
}
