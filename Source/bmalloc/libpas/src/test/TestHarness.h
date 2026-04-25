/*
 * Copyright (c) 2018-2022 Apple Inc. All rights reserved.
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

#pragma once

#include <iostream>
#include <functional>
#include "pas_config.h"
#include "pas_heap.h"
#include "pas_heap_runtime_config.h"
#include "pas_object_kind.h"
#include "pas_page_sharing_mode.h"
#include "pas_scavenger.h"
#include "pas_string_stream.h"
#include <set>
#include <sstream>
#include <string>
#include <vector>

// We used to be able to turn TLCs and segregated heaps on and off. We may bring that back. For
// now, keep the old code in the tests that knew what to do for both. New code can just assume
// that both of these are true.
#define TLC 1
#define SEGHEAP 1

void* allocationConfigAllocate(size_t size, const char* name, pas_allocation_kind allocationKind, void* arg);
void allocationConfigDeallocate(void* ptr, size_t size, pas_allocation_kind allocationKind, void* arg);

extern const pas_allocation_config allocationConfig;

inline void stringStreamConstruct(pas_string_stream* stream)
{
    pas_string_stream_construct(stream, &allocationConfig);
}

template<typename T, typename Func>
inline std::string dumpToString(const T& value, const Func& func)
{
    pas_string_stream stream;
    stringStreamConstruct(&stream);
    func(value, &stream.base);
    std::string result = pas_string_stream_get_string(&stream);
    pas_string_stream_destruct(&stream);
    return result;
}

inline std::ostream& operator<<(std::ostream& out, pas_page_sharing_mode mode)
{
    out << pas_page_sharing_mode_get_string(mode);
    return out;
}

inline std::ostream& operator<<(std::ostream& out, pas_segregated_page_config_variant variant)
{
    out << pas_segregated_page_config_variant_get_string(variant);
    return out;
}

inline std::ostream& operator<<(std::ostream& out, pas_object_kind kind)
{
    out << pas_object_kind_get_string(kind);
    return out;
}

inline std::ostream& operator<<(std::ostream& out, pas_page_kind kind)
{
    out << pas_page_kind_get_string(kind);
    return out;
}

#define CHECK(expression) do { \
        if ((expression)) \
            break; \
        std::cout << __FILE__ << ":" << __LINE__ << ": " << __PRETTY_FUNCTION__ << " assertion " \
                  << #expression << " failed." << std::endl; \
        abort(); \
    } while (false)

#define CHECK_EQUAL(a, b) do { \
        auto _aValue = (a); \
        auto _bValue = (b); \
        if (_aValue == _bValue) \
            break; \
        std::cout << __FILE__ << ":" << __LINE__ << ": " << __PRETTY_FUNCTION__ << " assertion " \
                  << #a << " == " << #b << " failed:" << std::endl; \
        std::cout << "    " << #a << " = " << _aValue << std::endl; \
        std::cout << "    " << #b << " = " << _bValue << std::endl; \
        abort(); \
    } while (false)

#define CHECK_NOT_EQUAL(a, b) do { \
        auto _aValue = (a); \
        auto _bValue = (b); \
        if (_aValue != _bValue) \
            break; \
        std::cout << __FILE__ << ":" << __LINE__ << ": " << __PRETTY_FUNCTION__ << " assertion " \
                  << #a << " != " << #b << " failed:" << std::endl; \
        std::cout << "    " << #a << " = " << _aValue << std::endl; \
        std::cout << "    " << #b << " = " << _bValue << std::endl; \
        abort(); \
    } while (false)

#define CHECK_GREATER(a, b) do { \
        auto _aValue = (a); \
        auto _bValue = (b); \
        if (_aValue > _bValue) \
            break; \
        std::cout << __FILE__ << ":" << __LINE__ << ": " << __PRETTY_FUNCTION__ << " assertion " \
                  << #a << " > " << #b << " failed:" << std::endl; \
        std::cout << "    " << #a << " = " << _aValue << std::endl; \
        std::cout << "    " << #b << " = " << _bValue << std::endl; \
        abort(); \
    } while (false)

#define CHECK_GREATER_EQUAL(a, b) do { \
        auto _aValue = (a); \
        auto _bValue = (b); \
        if (_aValue >= _bValue) \
            break; \
        std::cout << __FILE__ << ":" << __LINE__ << ": " << __PRETTY_FUNCTION__ << " assertion " \
                  << #a << " >= " << #b << " failed:" << std::endl; \
        std::cout << "    " << #a << " = " << _aValue << std::endl; \
        std::cout << "    " << #b << " = " << _bValue << std::endl; \
        abort(); \
    } while (false)

#define CHECK_LESS(a, b) do { \
        auto _aValue = (a); \
        auto _bValue = (b); \
        if (_aValue < _bValue) \
            break; \
        std::cout << __FILE__ << ":" << __LINE__ << ": " << __PRETTY_FUNCTION__ << " assertion " \
                  << #a << " < " << #b << " failed:" << std::endl; \
        std::cout << "    " << #a << " = " << _aValue << std::endl; \
        std::cout << "    " << #b << " = " << _bValue << std::endl; \
        abort(); \
    } while (false)

#define CHECK_LESS_EQUAL(a, b) do { \
        auto _aValue = (a); \
        auto _bValue = (b); \
        if (_aValue <= _bValue) \
            break; \
        std::cout << __FILE__ << ":" << __LINE__ << ": " << __PRETTY_FUNCTION__ << " assertion " \
                  << #a << " <= " << #b << " failed:" << std::endl; \
        std::cout << "    " << #a << " = " << _aValue << std::endl; \
        std::cout << "    " << #b << " = " << _bValue << std::endl; \
        abort(); \
    } while (false)

#define CHECK_ALIGNMENT_EQUAL(a, b) do { \
        auto _aValue = (a); \
        auto _bValue = (b); \
        if (pas_alignment_is_equal(_aValue, _bValue)) \
            break; \
        std::cout << __FILE__ << ":" << __LINE__ << ": " << __PRETTY_FUNCTION__ << " assertion " \
                  << #a << " == " << #b << " failed:" << std::endl; \
        pas_string_stream stream; \
        stringStreamConstruct(&stream); \
        pas_alignment_dump(_aValue, reinterpret_cast<pas_stream*>(&stream)); \
        std::cout << "    " << #a << " = " << pas_string_stream_get_string(&stream) << std::endl; \
        pas_string_stream_reset(&stream); \
        pas_alignment_dump(_bValue, reinterpret_cast<pas_stream*>(&stream)); \
        std::cout << "    " << #b << " = " << pas_string_stream_get_string(&stream) << std::endl; \
        pas_string_stream_destruct(&stream); \
        abort(); \
    } while (false)

void testSucceeded();

void addTest(const std::string& file, int line,
             const std::string& name, std::function<void()> test);

// This is an improperly balanced, low-quality random number. But it uses a deterministic seed
// and its behavior is super easy to understand. This makes it ideal for tests that want to do
// random things but want to be reproducible.
unsigned deterministicRandomNumber(unsigned exclusiveUpperBound);

#define ADD_TEST(test) addTest(__FILE__, __LINE__, #test, [=] () { test; })
#define SKIP_TEST(test) do { if ((false)) ADD_TEST(test); } while (false)

struct TestScopeImpl;

class TestScope {
public:
    TestScope(const std::string& name,
              std::function<void()> setUp);
    ~TestScope();
    
private:
    TestScopeImpl* m_impl;
};

#define ADD_GROUP(command) ({ \
        TestScope testScope(#command, [] () {}); \
        command; \
    })

class RuntimeConfigTestScope : public TestScope {
public:
    RuntimeConfigTestScope(const std::string& name,
                           std::function<void(pas_heap_runtime_config&)> setUp);
};

class ForceExclusives : public RuntimeConfigTestScope {
public:
    ForceExclusives();
};

class ForceTLAs : public RuntimeConfigTestScope {
public:
    ForceTLAs();
};

class ForceBitfit : public RuntimeConfigTestScope {
public:
    ForceBitfit();
};

class DisableBitfit : public RuntimeConfigTestScope {
public:
    DisableBitfit();
};

class ForcePartials : public RuntimeConfigTestScope {
public:
    ForcePartials();
};

class ForceBaselines : public RuntimeConfigTestScope {
public:
    ForceBaselines();
};

class VerifyGranules : public TestScope {
public:
    VerifyGranules();
};

extern pas_scavenger_synchronous_operation_kind scavengeKind;

class RunScavengerFully : public TestScope {
public:
    RunScavengerFully();
};

class RunScavengerOnNonRemoteCaches : public TestScope {
public:
    RunScavengerOnNonRemoteCaches();
};

class SuspendScavengerScope : public TestScope {
public:
    SuspendScavengerScope();
};

class InstallVerifier : public TestScope {
public:
    InstallVerifier();
};

class EpochIsCounter : public TestScope {
public:
    EpochIsCounter();
};

class BootJITHeap : public TestScope {
public:
    BootJITHeap();
};

class EnablePageBalancing : public TestScope {
public:
    EnablePageBalancing();
};

class DisablePageBalancing : public TestScope {
public:
    DisablePageBalancing();
};

class DecommitZeroFill : public TestScope {
public:
    DecommitZeroFill();
};

bool hasScope(const std::string& filter);

void dumpObjectSet(const std::set<void*>& objects);
void dumpObjectsInHeap(pas_heap* heap);
void dumpObjectsInHeaps(const std::vector<pas_heap*>& heaps);

void forEachLiveObject(pas_heap* heap, std::function<bool(void*, size_t)> callback);

void verifyMinimumObjectDistance(const std::set<void*>& objects,
                                 size_t minimumDistance);

void verifyExactObjectDistance(const std::set<void*>& objects,
                               size_t exactDistance);

void scavenge();

void printStatusReport();

