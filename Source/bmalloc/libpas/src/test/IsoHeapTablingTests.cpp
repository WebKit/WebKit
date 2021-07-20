/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#if PAS_ENABLE_ISO

#include "iso_test_heap.h"
#include "iso_test_heap_config.h"
#include <map>
#include <vector>

using namespace std;

namespace {

void testTabling(unsigned size,
                 pas_segregated_page_config_variant variant,
                 unsigned numPagesToEmpty,
                 unsigned numPagesNotToEmpty,
                 bool useTabling,
                 bool rageScavengeDuringRefill)
{
    static constexpr bool verbose = false;
    
    map<pas_segregated_view, vector<void*>> pagesToEmpty;
    map<pas_segregated_view, vector<void*>> pagesNotToEmpty;
    unsigned numberOfObjectsPerPage = pas_segregated_page_number_of_objects(
        size, *pas_heap_config_segregated_page_config_ptr_for_variant(&iso_test_heap_config, variant));

    auto fill =
        [&] (auto& map, unsigned limit) {
            for (unsigned pageIndex = 0; pageIndex < limit; ++pageIndex) {
                pas_segregated_view view = nullptr;
                vector<void*> objects;
                for (unsigned objectIndex = 0; objectIndex < numberOfObjectsPerPage; ++objectIndex) {
                    void* object = iso_test_allocate_common_primitive(size);
                    pas_segregated_view myView = pas_segregated_view_for_object(
                        reinterpret_cast<uintptr_t>(object),
                        &iso_test_heap_config);
                    if (verbose)
                        cout << "Allocated object " << object << " in view " << myView << "\n";
                    CHECK(myView);
                    if (!objectIndex) {
                        PAS_ASSERT(!view);
                        view = myView;
                    } else
                        CHECK_EQUAL(myView, view);
                    objects.push_back(object);
                }
                CHECK(view);
                CHECK(!map.count(view));
                map[view] = objects;
            }
        };

    fill(pagesToEmpty, numPagesToEmpty);
    fill(pagesNotToEmpty, numPagesNotToEmpty);

    for (auto& entry : pagesToEmpty) {
        while (entry.second.size()) {
            iso_test_deallocate(entry.second.back());
            entry.second.pop_back();
        }
    }
    for (auto& entry : pagesNotToEmpty) {
        while (entry.second.size() >= 2) {
            iso_test_deallocate(entry.second.back());
            entry.second.pop_back();
        }
    }

    scavenge();

    auto refill =
        [&] (auto& map, unsigned limit, unsigned exclude) {
            for (unsigned pageIndex = 0; pageIndex < limit; ++pageIndex) {
                pas_segregated_view view = nullptr;
                vector<void*> objects;
                for (unsigned objectIndex = 0;
                     objectIndex < numberOfObjectsPerPage - exclude;
                     ++objectIndex) {
                    if (rageScavengeDuringRefill)
                        scavenge();
                    void* object = iso_test_allocate_common_primitive(size);
                    pas_segregated_view myView = pas_segregated_view_for_object(
                        reinterpret_cast<uintptr_t>(object),
                        &iso_test_heap_config);
                    if (!objectIndex) {
                        PAS_ASSERT(!view);
                        view = myView;
                    } else
                        CHECK_EQUAL(myView, view);
                    objects.push_back(object);
                }
                PAS_ASSERT(view);
                CHECK(map.count(view));
                CHECK_EQUAL(map[view].size() + objects.size(), numberOfObjectsPerPage);
            }
        };

    if (useTabling) {
        refill(pagesNotToEmpty, numPagesNotToEmpty, 1);
        refill(pagesToEmpty, numPagesToEmpty, 0);
    } else {
        refill(pagesToEmpty, numPagesToEmpty, 0);
        refill(pagesNotToEmpty, numPagesNotToEmpty, 1);
    }
}

void testUntabling()
{
    vector<vector<void*>> objectsInPage;

    unsigned numberOfObjectsPerPage = pas_segregated_page_number_of_objects(
        16, ISO_TEST_HEAP_CONFIG.small_segregated_config);

    for (unsigned i = 0; i < 2; ++i) {
        vector<void*> objects;
        for (unsigned j = 0; j < numberOfObjectsPerPage; ++j)
            objects.push_back(iso_test_allocate_common_primitive(16));
        objectsInPage.push_back(objects);
    }

    for (void* object : objectsInPage[0])
        iso_test_deallocate(object);

    for (unsigned i = 1; i < objectsInPage[1].size(); ++i)
        iso_test_deallocate(objectsInPage[1][i]);

    scavenge();

    for (unsigned i = 1; i < objectsInPage[1].size(); ++i) {
        CHECK_EQUAL(iso_test_allocate_common_primitive(16),
                    objectsInPage[1][i]);
    }

    CHECK_EQUAL(iso_test_allocate_common_primitive(16),
                objectsInPage[0][0]);

    iso_test_deallocate(objectsInPage[1][0]);

    scavenge();

    CHECK_EQUAL(iso_test_allocate_common_primitive(16),
                objectsInPage[0][1]);
}

void addTablingTests(bool useTabling)
{
    ADD_TEST(testTabling(16, pas_small_segregated_page_config_variant, 1, 1, useTabling, false));
    ADD_TEST(testTabling(16, pas_small_segregated_page_config_variant, 1, 10, useTabling, false));
    ADD_TEST(testTabling(16, pas_small_segregated_page_config_variant, 10, 1, useTabling, false));
    ADD_TEST(testTabling(16, pas_small_segregated_page_config_variant, 10, 10, useTabling, false));
    ADD_TEST(testTabling(19968, pas_medium_segregated_page_config_variant, 1, 1, useTabling, false));
    ADD_TEST(testTabling(19968, pas_medium_segregated_page_config_variant, 1, 10, useTabling, false));
    ADD_TEST(testTabling(19968, pas_medium_segregated_page_config_variant, 10, 1, useTabling, false));
    ADD_TEST(testTabling(19968, pas_medium_segregated_page_config_variant, 10, 10, useTabling, false));

    ADD_TEST(testTabling(16, pas_small_segregated_page_config_variant, 1, 1, useTabling, true));
    ADD_TEST(testTabling(16, pas_small_segregated_page_config_variant, 1, 10, useTabling, true));
    ADD_TEST(testTabling(16, pas_small_segregated_page_config_variant, 10, 1, useTabling, true));
    ADD_TEST(testTabling(16, pas_small_segregated_page_config_variant, 10, 10, useTabling, true));
    ADD_TEST(testTabling(19968, pas_medium_segregated_page_config_variant, 1, 1, useTabling, true));
    ADD_TEST(testTabling(19968, pas_medium_segregated_page_config_variant, 1, 10, useTabling, true));
    ADD_TEST(testTabling(19968, pas_medium_segregated_page_config_variant, 10, 1, useTabling, true));
    ADD_TEST(testTabling(19968, pas_medium_segregated_page_config_variant, 10, 10, useTabling, true));
}

} // anonymous namespace

#endif // PAS_ENABLE_ISO

void addIsoHeapTablingTests()
{
    RunScavengerFully runScavengerFully;
    ForceExclusives forceExclusives;
    ForceTLAs forceTLAs;
    DisableBitfit disableBitfit;

    {
        TestScope enableTabling(
            "enable-tabling",
            [] () {
                pas_segregated_size_directory_use_tabling = true;
            });

        addTablingTests(true);
        
        ADD_TEST(testUntabling());
    }

    {
        TestScope enableTabling(
            "disable-tabling",
            [] () {
                pas_segregated_size_directory_use_tabling = false;
            });

        addTablingTests(false);
    }
}



