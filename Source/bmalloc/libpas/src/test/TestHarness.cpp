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

#include "TestHarness.h"

#include "Verifier.h"
#include <atomic>
#include "bmalloc_heap_config.h"
#include "hotbit_heap_config.h"
#include "iso_heap_config.h"
#include "iso_test_heap_config.h"
#include "jit_heap.h"
#include "jit_heap_config.h"
#include "minalign32_heap_config.h"
#include "pagesize64k_heap_config.h"
#include "pas_epoch.h"
#include "pas_fd_stream.h"
#include "pas_heap.h"
#include "pas_segregated_page.h"
#include "pas_segregated_page_config.h"
#include "pas_segregated_size_directory.h"
#include "pas_status_reporter.h"
#include "pas_utility_heap.h"
#include "pas_utils.h"
#include "thingy_heap_config.h"
#include <sstream>
#include <sys/wait.h>
#include <vector>
#include <unistd.h>

using namespace std;

void* allocationConfigAllocate(size_t size, const char* name, pas_allocation_kind allocationKind, void* arg)
{
    PAS_ASSERT(allocationKind == pas_object_allocation);
    PAS_ASSERT(!arg);
    return malloc(size);
}

void allocationConfigDeallocate(void* ptr, size_t size, pas_allocation_kind allocationKind, void* arg)
{
    PAS_ASSERT(allocationKind == pas_object_allocation);
    PAS_ASSERT(!arg);
    free(ptr);
}

const pas_allocation_config allocationConfig = {
    .allocate = allocationConfigAllocate,
    .deallocate = allocationConfigDeallocate,
    .arg = nullptr
};

struct TestScopeImpl {
    TestScopeImpl(const string& name,
                  function<void()> setUp,
                  TestScopeImpl* next)
        : name(name)
        , setUp(setUp)
        , next(next)
    {
    }
    
    string name;
    function<void()> setUp;
    TestScopeImpl* next;
};

#define FOR_EACH_RUNTIME_CONFIG(name, callback) ({ \
        (callback)(name ## _intrinsic_runtime_config.base); \
        (callback)(name ## _primitive_runtime_config.base); \
        (callback)(name ## _typed_runtime_config.base); \
        (callback)(name ## _flex_runtime_config.base); \
    })

RuntimeConfigTestScope::RuntimeConfigTestScope(
    const string& name,
    function<void(pas_heap_runtime_config&)> setUp)
    : TestScope(
        name,
        [=] () {
            FOR_EACH_RUNTIME_CONFIG(thingy, setUp);
            FOR_EACH_RUNTIME_CONFIG(iso, setUp);
            FOR_EACH_RUNTIME_CONFIG(iso_test, setUp);
            FOR_EACH_RUNTIME_CONFIG(minalign32, setUp);
            FOR_EACH_RUNTIME_CONFIG(pagesize64k, setUp);
            FOR_EACH_RUNTIME_CONFIG(bmalloc, setUp);
            FOR_EACH_RUNTIME_CONFIG(hotbit, setUp);
            setUp(pas_utility_heap_runtime_config);
            setUp(jit_heap_runtime_config);
        })
{
}

ForceExclusives::ForceExclusives()
    : RuntimeConfigTestScope(
        "force-exclusives",
        [] (pas_heap_runtime_config& runtimeConfig) {
            runtimeConfig.directory_size_bound_for_partial_views = 0;
        })
{
}

ForceTLAs::ForceTLAs()
    : RuntimeConfigTestScope(
        "force-tlas",
        [] (pas_heap_runtime_config& runtimeConfig) {
            runtimeConfig.directory_size_bound_for_baseline_allocators = 0;
        })
{
}

ForceBitfit::ForceBitfit()
    : RuntimeConfigTestScope(
        "force-bitfit",
        [] (pas_heap_runtime_config& runtimeConfig) {
            if (&runtimeConfig != &pas_utility_heap_runtime_config) {
                runtimeConfig.max_segregated_object_size = 0;
                runtimeConfig.max_bitfit_object_size = UINT_MAX;
            }
        })
{
}

DisableBitfit::DisableBitfit()
    : RuntimeConfigTestScope(
        "disable-bitfit",
        [] (pas_heap_runtime_config& runtimeConfig) {
            runtimeConfig.max_bitfit_object_size = 0;
        })
{
}

ForcePartials::ForcePartials()
    : RuntimeConfigTestScope(
        "force-partials",
        [] (pas_heap_runtime_config& runtimeConfig) {
            if (&runtimeConfig == &pas_utility_heap_runtime_config)
                return;
            runtimeConfig.directory_size_bound_for_partial_views = UINT8_MAX;
        })
{
}

ForceBaselines::ForceBaselines()
    : RuntimeConfigTestScope(
        "force-baselines",
        [] (pas_heap_runtime_config& runtimeConfig) {
            runtimeConfig.directory_size_bound_for_baseline_allocators = UINT_MAX;
        })
{
}

pas_scavenger_synchronous_operation_kind scavengeKind;

RunScavengerFully::RunScavengerFully()
    : TestScope(
        "run-scavenger-fully",
        [] () { scavengeKind = pas_scavenger_run_synchronously_now_kind; })
{
}

RunScavengerOnNonRemoteCaches::RunScavengerOnNonRemoteCaches()
    : TestScope(
        "run-scavenger-on-non-remote-caches",
        [] () { scavengeKind = pas_scavenger_clear_all_caches_except_remote_tlcs_kind; })
{
}

SuspendScavengerScope::SuspendScavengerScope()
    : TestScope(
        "suspend-scavenger",
        [] () { pas_scavenger_suspend(); })
{
}

VerifyGranules::VerifyGranules()
    : TestScope(
        "verify-granules",
        [] () {
            pas_segregated_page_deallocate_should_verify_granules = true;
        })
{
}

InstallVerifier::InstallVerifier()
    : TestScope(
        "install-verifier",
        [] () {
            pas_install_verifier();
        })
{
}

EpochIsCounter::EpochIsCounter()
    : TestScope(
        "epoch-is-counter",
        [] () {
            pas_epoch_is_counter = true;
            pas_scavenger_max_epoch_delta = 0;
        })
{
}

BootJITHeap::BootJITHeap()
    : TestScope(
        "boot-jit-heap",
        [] () {
#if PAS_ENABLE_JIT
            constexpr unsigned numRegions = 50;

            for (unsigned i = numRegions; i--;) {
                size_t size = pas_round_up_to_power_of_2(5000000, pas_page_malloc_alignment());
                void* base = valloc(size);
                jit_heap_add_fresh_memory(
                    pas_range_create(reinterpret_cast<uintptr_t>(base),
                                     reinterpret_cast<uintptr_t>(base) + size));
            }
#endif // PAS_ENABLE_JIT
        })
{
}

EnablePageBalancing::EnablePageBalancing()
    : TestScope(
        "enable-page-balancing",
        [] () {
            pas_physical_page_sharing_pool_balancing_enabled = true;
        })
{
}

DisablePageBalancing::DisablePageBalancing()
    : TestScope(
        "disable-page-balancing",
        [] () {
            pas_physical_page_sharing_pool_balancing_enabled = false;
        })
{
}

DecommitZeroFill::DecommitZeroFill()
    : TestScope(
        "decommit-zero-fill",
        [] () {
#if PAS_OS(DARWIN)
            pas_page_malloc_decommit_zero_fill = true;
#endif // PAS_OS(DARWIN)
        })
{
}

namespace {

template<typename Func>
void iterateForward(TestScopeImpl* scope, const Func& func)
{
    if (!scope)
        return;
    iterateForward(scope->next, func);
    func(scope);
}

string currentSuite;
bool runningOneTest;

struct Test {
    Test() = default;
    
    Test(size_t index,
         const string& file, int line,
         const string& suite, const string& name,
         function<void()> payload,
         TestScopeImpl* scope)
        : index(index)
        , file(file)
        , line(line)
        , suite(suite)
        , name(name)
        , payload(payload)
        , scope(scope)
    {
    }
    
    string fullName() const
    {
        ostringstream out;
        out << "(" << index << "):" << file << ":" << line << "/" << suite;
        iterateForward(
            scope,
            [&] (TestScopeImpl* scope) {
                out << "/" << scope->name;
            });
        out << "/" << name;
        return out.str();
    }
    
    void run() const
    {
        iterateForward(
            scope,
            [&] (TestScopeImpl* scope) {
                scope->setUp();
            });
        payload();
    }
    
    size_t index;
    string file;
    int line { 0 };
    string suite;
    string name;
    function<void()> payload;
    TestScopeImpl* scope;
};

vector<Test> tests;
TestScopeImpl* topScope;

unsigned testsPassed;
unsigned testsRan;

static constexpr char successByte = 'S';

int resultPipe[2];

} // anonymous namespace

void addBitfieldVectorTests();
void addBitfitTests();
void addBitvectorTests();
void addBmallocTests();
void addCartesianTreeTests();
void addCoalignTests();
void addEnumerationTests();
void addExpendableMemoryTests();
void addExtendedGCDTests();
void addHashtableTests();
void addHeapRefAllocatorIndexTests();
void addIsoDynamicPrimitiveHeapTests();
void addIsoHeapChaosTests();
void addIsoHeapPageSharingTests();
void addIsoHeapPartialAndBaselineTests();
void addIsoHeapReservedMemoryTests();
void addJITHeapTests();
void addLargeFreeHeapTests();
void addLargeSharingPoolTests();
void addLockFreeReadPtrPtrHashtableTests();
void addLotsOfHeapsAndThreadsTests();
void addMemalignTests();
void addMinHeapTests();
void addPGMTests();
void addRaceTests();
void addRedBlackTreeTests();
void addTLCDecommitTests();
void addTSDTests();
void addThingyAndUtilityHeapAllocationTests();
void addUtilsTests();
void addViewCacheTests();

void testSucceeded()
{
    if (runningOneTest) {
        cout << "    PASS!" << endl;
        cout << endl;
        cout << "Exiting early due to test success." << endl;
        cout << endl;
        cout << "THE ONE TEST PASSED!" << endl;
        exit(0);
    }

    ssize_t writeResult = write(resultPipe[1], &successByte, 1);
    PAS_ASSERT(writeResult == 1);
    exit(0);
    PAS_ASSERT(!"Should have exited");
}

unsigned deterministicRandomNumber(unsigned exclusiveUpperBound)
{
    // This code is adapted from the JetStream benchmark suite.

    // Copyright 2013 the V8 project authors. All rights reserved.
    // Copyright (C) 2015 Apple Inc. All rights reserved.
    // Redistribution and use in source and binary forms, with or without
    // modification, are permitted provided that the following conditions are
    // met:
    //
    //     * Redistributions of source code must retain the above copyright
    //       notice, this list of conditions and the following disclaimer.
    //     * Redistributions in binary form must reproduce the above
    //       copyright notice, this list of conditions and the following
    //       disclaimer in the documentation and/or other materials provided
    //       with the distribution.
    //     * Neither the name of Google Inc. nor the names of its
    //       contributors may be used to endorse or promote products derived
    //       from this software without specific prior written permission.
    //
    // THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    // "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    // LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    // A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    // OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    // SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    // LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    // DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    // THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    // (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    // OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    
    static atomic<unsigned> seed(49734321 + 19);
    
    for (;;) {
        unsigned value = seed;
        unsigned expectedValue = value;
        
        value = ((value + 0x7ed55d16) + (value << 12)) & 0xffffffff;
        value = ((value ^ 0xc761c23c) ^ (value >> 19)) & 0xffffffff;
        value = ((value + 0x165667b1) + (value << 5))  & 0xffffffff;
        value = ((value + 0xd3a2646c) ^ (value << 9))  & 0xffffffff;
        value = ((value + 0xfd7046c5) + (value << 3))  & 0xffffffff;
        value = ((value ^ 0xb55a4f09) ^ (value >> 16)) & 0xffffffff;
        
        if (seed.compare_exchange_weak(expectedValue, value))
            return value % exclusiveUpperBound;
    }
}

void addTest(const string& file, int line, const string& name, function<void()> test)
{
    CHECK(currentSuite.size());
    tests.push_back(Test(tests.size(), file, line, currentSuite, name, test, topScope));
}

TestScope::TestScope(const string& name,
                     function<void()> setUp)
    : m_impl(new TestScopeImpl(name, setUp, topScope))
{
    topScope = m_impl;
}

TestScope::~TestScope()
{
    PAS_ASSERT(m_impl == topScope);
    topScope = topScope->next;
}

bool hasScope(const string& filter)
{
    for (TestScopeImpl* scope = topScope; scope; scope = scope->next) {
        if (scope->name.find(filter) != string::npos)
            return true;
    }
    return false;
}

void dumpObjectSet(const set<void*>& objects)
{
    cout << "Object set:";
    for (void* object : objects) {
        cout << " " << object;
    }
    cout << endl;
}

void dumpObjectsInHeap(pas_heap* heap)
{
    cout << "Objects:";
    forEachLiveObject(
        heap,
        [&] (void* object, size_t size) -> bool {
            cout << " " << object;
            return true;
        });
    cout << "\n";
}

void dumpObjectsInHeaps(const vector<pas_heap*>& heaps)
{
    cout << "Objects in heaps:\n";
    for (pas_heap* heap : heaps) {
        cout << "In heap " << heap << "\n";
        dumpObjectsInHeap(heap);
    }
}

namespace {
bool forEachLiveObjectAdapter(pas_heap* heap,
                              uintptr_t begin,
                              size_t size,
                              void* arg)
{
    function<bool(void*, size_t)>* callback = reinterpret_cast<function<bool(void*, size_t)>*>(arg);
    return (*callback)(reinterpret_cast<void*>(begin), size);
}
}

void forEachLiveObject(pas_heap* heap, function<bool(void*, size_t)> callback)
{
    pas_heap_for_each_live_object(heap, forEachLiveObjectAdapter, &callback,
                                  pas_lock_is_not_held);
}

void verifyMinimumObjectDistance(const set<void*>& objects,
                                 size_t minimumDistance)
{
    uintptr_t lastBegin = 0;
    for (void* object : objects) {
        uintptr_t begin = reinterpret_cast<uintptr_t>(object);
        
        CHECK_GREATER_EQUAL(begin, lastBegin);
        
        if (begin < lastBegin + minimumDistance) {
            cout << "Object " << reinterpret_cast<void*>(lastBegin) << " is too close to " << object << endl;
            dumpObjectSet(objects);
        }
        
        CHECK_GREATER_EQUAL(begin - lastBegin, minimumDistance);
        
        lastBegin = begin;
    }
}

void verifyExactObjectDistance(const set<void*>& objects,
                               size_t exactDistance)
{
    uintptr_t lastBegin = 0;
    bool hasLast = false;
    for (void* object : objects) {
        uintptr_t begin = reinterpret_cast<uintptr_t>(object);
        
        CHECK_GREATER_EQUAL(begin, lastBegin);

        if (hasLast) {
            if (begin != lastBegin + exactDistance) {
                cout << "Object " << reinterpret_cast<void*>(lastBegin) << " is wrong distance to " << object << endl;
                dumpObjectSet(objects);
            }
            
            CHECK_EQUAL(begin - lastBegin, exactDistance);
        }
        
        lastBegin = begin;
        hasLast = true;
    }
}

void runOneTest(const Test& test)
{
    runningOneTest = true;
    cout << "Running " << test.fullName() << "..." << endl;
    test.run();
    testSucceeded();
    CHECK(!"Should not have gotten here.");
}

void runForkedTest(const Test& test)
{
    cout << "Running " << test.fullName() << "..." << endl;
    
    testsRan++;

    int pipeResult = pipe(resultPipe);
    PAS_ASSERT(!pipeResult);
    
    int forkResult = fork();
    PAS_ASSERT(forkResult >= 0);
    
    if (!forkResult) {
        close(resultPipe[0]);
        test.run();
        ssize_t writeResult = write(resultPipe[1], &successByte, 1);
        PAS_ASSERT(writeResult == 1);
        exit(0);
        PAS_ASSERT(!"Should have exited");
    }
    
    close(resultPipe[1]);
    
    bool didReadAByte = false;
    char resultByte;
    for (;;) {
        ssize_t readResult = read(resultPipe[0], &resultByte, 1);

        if (readResult == 1) {
            didReadAByte = true;
            break;
        }
        
        if (!readResult)
            break;
        
        PAS_ASSERT(readResult == -1);
        PAS_ASSERT(errno == EINTR);
    }
    
    close(resultPipe[0]);
    
    int waitStatus;
    for (;;) {
        int pid = wait(&waitStatus);
        
        if (pid >= 0) {
            if (WIFSTOPPED(waitStatus))
                continue;
            break;
        }
        
        PAS_ASSERT(errno == EINTR);
    }
    
    if (WIFEXITED(waitStatus)) {
        if (!WEXITSTATUS(waitStatus) && didReadAByte) {
            if (resultByte == successByte) {
                cout << "    PASS!" << endl;
                testsPassed++;
                return;
            }
            
            cout << "     FAIL: unexpected result byte: " << static_cast<int>(resultByte) << endl;
            return;
        }

        cout << "    FAIL: unexpected exit with code " << WEXITSTATUS(waitStatus) << endl;
        return;
    }
    
    if (WIFSIGNALED(waitStatus)) {
        cout << "    CRASH: with signal " << WTERMSIG(waitStatus) << endl;
        return;
    }
    
    cout << "    FAIL: child process terminated with unknown status code " << waitStatus << endl;
}

void runTests(const vector<Test>& tests)
{
    CHECK(tests.size());
    
    if (tests.size() == 1)
        runOneTest(tests[0]);
    
    for (const Test& test : tests)
        runForkedTest(test);

    PAS_ASSERT(testsPassed <= testsRan);
    cout << endl;
    cout << testsPassed << "/" << testsRan << " TESTS PASSED!" << endl;
    if (testsPassed == testsRan) {
        cout << endl;
        cout << "ALL TESTS PASSED!" << endl;
        exit(0);
    }
    
    cout << endl;
    cout << (testsRan - testsPassed) << "/" << testsRan << " TESTS FAILED!" << endl;
    cout << endl;
    if (!testsPassed)
        cout << "ALL TESTS FAILED!" << endl;
    else
        cout << "SOME TESTS FAILED!" << endl;
    exit(1);
}

void runFilteredTests(const string& filter)
{
    vector<Test> filteredTests;
    
    for (const Test& test : tests) {
        if (test.fullName().find(filter) != string::npos)
            filteredTests.push_back(test);
    }
    
    if (filteredTests.empty()) {
        cerr << "Error: Did not find any tests to run for filter " << filter << endl;
        exit(1);
    }
    
    runTests(filteredTests);
}

#define ADD_SUITE(name) do { \
        currentSuite = #name; \
        add##name##Tests(); \
        currentSuite = ""; \
    } while (false)

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    
#if SEGHEAP
    pas_segregated_page_config_do_validate = true;
#endif

    // Run the Thingy tests first because they catch the most bugs.
    ADD_SUITE(ThingyAndUtilityHeapAllocation);

    // Run the rest of the tests in alphabetical order.
    ADD_SUITE(BitfieldVector);
    ADD_SUITE(Bitfit);
    ADD_SUITE(Bitvector);
    ADD_SUITE(Bmalloc);
    ADD_SUITE(CartesianTree);
    ADD_SUITE(Coalign);
    ADD_SUITE(Enumeration);
    ADD_SUITE(ExpendableMemory);
    ADD_SUITE(ExtendedGCD);
    ADD_SUITE(Hashtable);
    ADD_SUITE(HeapRefAllocatorIndex);
    ADD_SUITE(IsoDynamicPrimitiveHeap);
    ADD_SUITE(IsoHeapChaos);
    ADD_SUITE(IsoHeapPageSharing);
    ADD_SUITE(IsoHeapPartialAndBaseline);
    ADD_SUITE(IsoHeapReservedMemory);
    ADD_SUITE(JITHeap);
    ADD_SUITE(LargeFreeHeap);
    ADD_SUITE(LargeSharingPool);
    ADD_SUITE(LockFreeReadPtrPtrHashtable);
    ADD_SUITE(LotsOfHeapsAndThreads);
    ADD_SUITE(Memalign);
    ADD_SUITE(MinHeap);
    ADD_SUITE(PGM);
    ADD_SUITE(Race);
    ADD_SUITE(RedBlackTree);
    ADD_SUITE(TLCDecommit);
    ADD_SUITE(TSD);
    ADD_SUITE(Utils);
    ADD_SUITE(ViewCache);
    
    string filter;
    
    switch (argc) {
    case 1:
        break;
        
    case 2:
        filter = argv[1];
        break;
        
    default:
        PAS_ASSERT(!"Wrong number of arguments");
    }
    
    runFilteredTests(filter);
    
    CHECK(!"Should not have reached here.");
    return 1;
}

void scavenge()
{
    pas_scavenger_perform_synchronous_operation(scavengeKind);
}

void printStatusReport()
{
    pas_fd_stream stdout_stream;
    pas_fd_stream_construct(&stdout_stream, 1);
    pas_heap_lock_lock();
    unsigned old_enabled = pas_status_reporter_enabled;
    pas_status_reporter_enabled = 100;
    pas_status_reporter_dump_everything(&stdout_stream.base);
    pas_status_reporter_enabled = old_enabled;
    pas_heap_lock_unlock();
}

