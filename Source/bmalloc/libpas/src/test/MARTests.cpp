/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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

#include <bit>
#include <mach/arm/kern_return.h>
#include <stdlib.h>
#include <unistd.h>

#include "TestHarness.h"

#include "pas_mar_registry.h"

using namespace std;

namespace {

void testRetrieval()
{
    struct pas_mar_registry registry = {
        { },
        { },
        0,
        0,
        { },
    };
    void* backtrace[8] = { (void*)0x1111, (void*)0x2222, (void*)0x3333, (void*)0x4444, (void*)0x5555, (void*)0x6666, (void*)0x7777, (void*)0x8888 };

    void* address = (void*)0x11223344;

    pas_mar_record_allocation(&registry, address, 32, 8, backtrace);

    auto result = pas_mar_get_allocation_record(&registry, address);
    CHECK(result.is_valid);
    CHECK_EQUAL(result.allocation_size, 32);
    CHECK_EQUAL(result.allocation_trace.num_frames, 8);
    CHECK_EQUAL(result.allocation_trace.backtrace_buffer[0], backtrace[0]);
}

void testRetrievalAfterCycling()
{
    struct pas_mar_registry registry = {
        { },
        { },
        0,
        0,
        { },
    };
    void* backtrace[8] = { (void*)0x1111, (void*)0x2222, (void*)0x3333, (void*)0x4444, (void*)0x5555, (void*)0x6666, (void*)0x7777, (void*)0x8888 };

    void* address = (void*)0x11223344;

    for (unsigned long i = 0; i < 1000; ++i)
        pas_mar_record_allocation(&registry, (void*)i, 32, 8, backtrace);

    pas_mar_record_allocation(&registry, address, 32, 8, backtrace);

    for (unsigned long i = 1; i < MARTrackedAllocations; ++i)
        pas_mar_record_allocation(&registry, (void*)i, 32, 8, backtrace);

    auto result = pas_mar_get_allocation_record(&registry, address);
    CHECK(result.is_valid);
    CHECK_EQUAL(result.allocation_size, 32);
    CHECK_EQUAL(result.allocation_trace.num_frames, 8);
    CHECK_EQUAL(result.allocation_trace.backtrace_buffer[0], backtrace[0]);
}

void testRetrievalAfterMultipleCycles()
{
    struct pas_mar_registry registry = {
        { },
        { },
        0,
        0,
        { },
    };
    void* backtrace[8] = { (void*)0x1111, (void*)0x2222, (void*)0x3333, (void*)0x4444, (void*)0x5555, (void*)0x6666, (void*)0x7777, (void*)0x8888 };

    void* address = (void*)0x11223344;

    for (unsigned long i = 0; i < 3 * MARTrackedAllocations; ++i)
        pas_mar_record_allocation(&registry, (void*)i, 32, 8, backtrace);

    pas_mar_record_allocation(&registry, address, 32, 8, backtrace);

    for (unsigned long i = 1; i < MARTrackedAllocations; ++i)
        pas_mar_record_allocation(&registry, (void*)i, 32, 8, backtrace);

    auto result = pas_mar_get_allocation_record(&registry, address);
    CHECK(result.is_valid);
    CHECK_EQUAL(result.allocation_size, 32);
    CHECK_EQUAL(result.allocation_trace.num_frames, 8);
    CHECK_EQUAL(result.allocation_trace.backtrace_buffer[0], backtrace[0]);
}

} // anonymous namespace

void addMARTests()
{
    ADD_TEST(testRetrieval());
    ADD_TEST(testRetrievalAfterCycling());
    ADD_TEST(testRetrievalAfterMultipleCycles());
}
