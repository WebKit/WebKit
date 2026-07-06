//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ResourceMap_unittest:
//   Unit tests for the ResourceMap template class.
//

#include <gtest/gtest.h>
#include <map>

#include "libANGLE/ResourceMap.h"

using namespace gl;

namespace gl
{
template <>
inline GLuint GetIDValue(int id)
{
    return id;
}
template <>
inline GLuint GetIDValue(unsigned int id)
{
    return id;
}
template <>
inline GLuint GetIDValue(uint16_t id)
{
    return id;
}
// For the purpose of unit testing, |int| is considered private (not needing lock), |unsigned int|
// is considered shared (needing lock), and |uint16_t| additionally has a small flat array to
// exercise all three storage levels.
template <>
struct ResourceMapParams<unsigned int>
{
    static constexpr size_t kInitialFlatResourcesSize    = 0xC0;
    static constexpr size_t kAdditionalFlatResourcesSize = 0;
    static constexpr bool kNeedsLock                     = true;
};
template <>
struct ResourceMapParams<uint16_t>
{
    static constexpr size_t kInitialFlatResourcesSize    = 3;
    static constexpr size_t kAdditionalFlatResourcesSize = 7;
    static constexpr bool kNeedsLock                     = true;
};
}  // namespace gl

namespace
{
// The resourceMap class uses a lock for "unsigned int" types to support this unit test.
using LocklessType = int;
using LockedType   = unsigned int;
using AdditionalFlatType = uint16_t;

template <typename T>
void AssignAndErase()
{
    constexpr size_t kSize = 300;
    ResourceMap<size_t, T> resourceMap;
    std::vector<size_t> objects(kSize, 1);
    for (size_t index = 0; index < kSize; ++index)
    {
        resourceMap.assign(index + 1, &objects[index]);
    }

    for (size_t index = 0; index < kSize; ++index)
    {
        size_t *found = nullptr;
        ASSERT_TRUE(resourceMap.erase(index + 1, &found));
        ASSERT_EQ(&objects[index], found);
    }

    ASSERT_TRUE(UnsafeResourceMapIter(resourceMap).empty());
}

// Tests assigning slots in the map and then deleting elements.
TEST(ResourceMapTest, AssignAndEraseLockless)
{
    AssignAndErase<LocklessType>();
}
// Tests assigning slots in the map and then deleting elements.
TEST(ResourceMapTest, AssignAndEraseLocked)
{
    AssignAndErase<LockedType>();
}

template <typename T>
void AssignAndClear()
{
    constexpr size_t kSize = 280;
    ResourceMap<size_t, T> resourceMap;
    std::vector<size_t> objects(kSize, 1);
    for (size_t index = 0; index < kSize; ++index)
    {
        resourceMap.assign(index + 1, &objects[index]);
    }

    resourceMap.clear();
    ASSERT_TRUE(UnsafeResourceMapIter(resourceMap).empty());
}

// Tests assigning slots in the map and then using clear() to free it.
TEST(ResourceMapTest, AssignAndClearLockless)
{
    AssignAndClear<LocklessType>();
}
// Tests assigning slots in the map and then using clear() to free it.
TEST(ResourceMapTest, AssignAndClearLocked)
{
    AssignAndClear<LockedType>();
}

template <typename T>
void BigGrowth()
{
    constexpr size_t kSize = 8;

    ResourceMap<size_t, T> resourceMap;
    std::vector<size_t> objects;

    for (size_t index = 0; index < kSize; ++index)
    {
        objects.push_back(index);
    }

    // Assign a large value.
    constexpr size_t kLargeIndex = 128;
    objects.push_back(kLargeIndex);

    for (size_t &object : objects)
    {
        resourceMap.assign(object, &object);
    }

    for (size_t object : objects)
    {
        size_t *found = nullptr;
        ASSERT_TRUE(resourceMap.erase(object, &found));
        ASSERT_EQ(object, *found);
    }

    ASSERT_TRUE(UnsafeResourceMapIter(resourceMap).empty());
}

// Tests growing a map more than double the size.
TEST(ResourceMapTest, BigGrowthLockless)
{
    BigGrowth<LocklessType>();
}
// Tests growing a map more than double the size.
TEST(ResourceMapTest, BigGrowthLocked)
{
    BigGrowth<LockedType>();
}

template <typename T>
void QueryUnassigned()
{
    constexpr size_t kSize        = 8;
    constexpr T kZero             = 0;
    constexpr T kIdInFlatRange    = 10;
    constexpr T kIdOutOfFlatRange = 500;

    ResourceMap<size_t, T> resourceMap;
    std::vector<size_t> objects;

    for (size_t index = 0; index < kSize; ++index)
    {
        objects.push_back(index);
    }

    ASSERT_FALSE(resourceMap.contains(kZero));
    ASSERT_EQ(nullptr, resourceMap.query(kZero));
    ASSERT_FALSE(resourceMap.contains(kIdOutOfFlatRange));
    ASSERT_EQ(nullptr, resourceMap.query(kIdOutOfFlatRange));

    for (size_t &object : objects)
    {
        resourceMap.assign(object, &object);
    }

    ASSERT_FALSE(UnsafeResourceMapIter(resourceMap).empty());

    for (size_t &object : objects)
    {
        ASSERT_TRUE(resourceMap.contains(object));
        ASSERT_EQ(&object, resourceMap.query(object));
    }

    ASSERT_FALSE(resourceMap.contains(kIdInFlatRange));
    ASSERT_EQ(nullptr, resourceMap.query(kIdInFlatRange));
    ASSERT_FALSE(resourceMap.contains(kIdOutOfFlatRange));
    ASSERT_EQ(nullptr, resourceMap.query(kIdOutOfFlatRange));

    for (size_t object : objects)
    {
        size_t *found = nullptr;
        ASSERT_TRUE(resourceMap.erase(object, &found));
        ASSERT_EQ(object, *found);
    }

    ASSERT_TRUE(UnsafeResourceMapIter(resourceMap).empty());

    ASSERT_FALSE(resourceMap.contains(kZero));
    ASSERT_EQ(nullptr, resourceMap.query(kZero));
    ASSERT_FALSE(resourceMap.contains(kIdOutOfFlatRange));
    ASSERT_EQ(nullptr, resourceMap.query(kIdOutOfFlatRange));
}

// Tests querying unassigned or erased values.
TEST(ResourceMapTest, QueryUnassignedLockless)
{
    QueryUnassigned<LocklessType>();
}
// Tests querying unassigned or erased values.
TEST(ResourceMapTest, QueryUnassignedLocked)
{
    QueryUnassigned<LockedType>();
}

template <typename IDType>
void ConcurrentAccess(size_t iterations, size_t idCycleSize)
{
    if (std::is_same_v<ResourceMapMutex, angle::NoOpMutex>)
    {
        GTEST_SKIP() << "Test skipped: Locking is disabled in build.";
    }

    constexpr size_t kThreadCount = 13;

    ResourceMap<size_t, IDType> resourceMap;

    std::array<std::thread, kThreadCount> threads;
    std::array<std::map<IDType, size_t>, kThreadCount> insertedIds;

    for (size_t i = 0; i < kThreadCount; ++i)
    {
        threads[i] = std::thread([&, i]() {
            // Each thread manipulates a different set of ids.  The resource map guarantees that the
            // data structure itself is thread-safe, not accesses to the same id.
            for (size_t j = 0; j < iterations; ++j)
            {
                const IDType id =
                    static_cast<IDType>((j % (idCycleSize / kThreadCount)) * kThreadCount + i);

                ASSERT_LE(id, 0xFFFFu);
                ASSERT_LE(j, 0xFFFFu);
                const size_t value = id | j << 16;

                size_t *valuePtr = reinterpret_cast<size_t *>(value);

                const size_t *queryResult = resourceMap.query(id);
                const bool containsResult = resourceMap.contains(id);

                const bool expectContains = insertedIds[i].count(id) > 0;
                if (expectContains)
                {
                    EXPECT_TRUE(containsResult);
                    const IDType queryResultInt =
                        static_cast<IDType>(reinterpret_cast<size_t>(queryResult) & 0xFFFF);
                    const size_t queryResultIteration = reinterpret_cast<size_t>(queryResult) >> 16;
                    EXPECT_EQ(queryResultInt, id);
                    EXPECT_LT(queryResultIteration, j);

                    size_t *erasedValue = nullptr;
                    const bool erased   = resourceMap.erase(id, &erasedValue);

                    EXPECT_TRUE(erased);
                    EXPECT_EQ(erasedValue, queryResult);

                    insertedIds[i].erase(id);
                }
                else
                {
                    EXPECT_FALSE(containsResult);
                    EXPECT_EQ(queryResult, nullptr);

                    resourceMap.assign(id, valuePtr);
                    EXPECT_TRUE(resourceMap.contains(id));

                    ASSERT_TRUE(insertedIds[i].count(id) == 0);
                    insertedIds[i][id] = value;
                }
            }
        });
    }

    for (size_t i = 0; i < kThreadCount; ++i)
    {
        threads[i].join();
    }

    // Verify that every value that is expected to be there is actually there
    std::map<size_t, size_t> allIds;
    size_t allIdsPrevSize = 0;

    for (size_t i = 0; i < kThreadCount; ++i)
    {
        // Merge all the sets together.  The sets are disjoint, which is verified by the ASSERT_EQ.
        allIds.insert(insertedIds[i].begin(), insertedIds[i].end());
        ASSERT_EQ(allIds.size(), allIdsPrevSize + insertedIds[i].size());
        allIdsPrevSize = allIds.size();

        // Make sure every id that is expected to be there is actually there.
        for (auto &idValue : insertedIds[i])
        {
            EXPECT_TRUE(resourceMap.contains(idValue.first));
            EXPECT_EQ(resourceMap.query(idValue.first), reinterpret_cast<size_t *>(idValue.second));
        }
    }

    // Verify that every value that is NOT expected to be there isn't actually there
    for (auto &idValue : UnsafeResourceMapIter(resourceMap))
    {
        EXPECT_TRUE(allIds.count(idValue.first) == 1);
        EXPECT_EQ(idValue.second, reinterpret_cast<size_t *>(allIds[idValue.first]));
    }

    resourceMap.clear();
}

// Tests that concurrent access to thread-safe resource maps works for small ids that are mostly in
// the flat map range.
TEST(ResourceMapTest, ConcurrentAccessSmallIds)
{
    ConcurrentAccess<LockedType>(50'000, 128);
}
// Tests that concurrent access to thread-safe resource maps works for a wider range of ids.
TEST(ResourceMapTest, ConcurrentAccessLargeIds)
{
    ConcurrentAccess<LockedType>(10'000, 20'000);
}
// Tests that concurrent access to thread-safe resource maps works for the initial flat array, the
// secondary flat array and the hashmap.
TEST(ResourceMapTest, ConcurrentAccessFlatArraysAndHashMap)
{
    // Set idCycleSize to 26 (2 * kThreadCount) so that each thread gets 2 IDs and covers the flat
    // arrays and the hashmap.
    ConcurrentAccess<AdditionalFlatType>(20'000, 26);
}

// Tests growth across the initial flat array, the secondary flat array, and the hashmap.
TEST(ResourceMapTest, GrowthAcrossFlatArraysAndHashMap)
{
    // Handle Layout
    // - initial flat array     : 1, 2
    // - additional flat array  : 3, 4, 5, 6, 7, 8, 9
    // - hashmap                : 10, 11, 12
    constexpr size_t kTotalHandle = 12;
    std::vector<size_t> objects(kTotalHandle);
    for (size_t index = 0; index < kTotalHandle; ++index)
    {
        objects[index] = index + 1;
    }

    ResourceMap<size_t, AdditionalFlatType> resourceMap;

    for (size_t index = 0; index < kTotalHandle; ++index)
    {
        resourceMap.assign(static_cast<AdditionalFlatType>(index + 1), &objects[index]);
    }

    for (size_t index = 0; index < kTotalHandle; ++index)
    {
        const AdditionalFlatType handle = static_cast<AdditionalFlatType>(index + 1);
        EXPECT_TRUE(resourceMap.contains(handle)) << "handle=" << handle;
        EXPECT_EQ(resourceMap.query(handle), &objects[index]) << "handle=" << handle;
    }

    // Handles that have never been assigned must not be found.
    EXPECT_FALSE(resourceMap.contains(static_cast<AdditionalFlatType>(0)));
    EXPECT_EQ(resourceMap.query(static_cast<AdditionalFlatType>(0)), nullptr);
    EXPECT_FALSE(resourceMap.contains(static_cast<AdditionalFlatType>(kTotalHandle + 1)));
    EXPECT_EQ(resourceMap.query(static_cast<AdditionalFlatType>(kTotalHandle + 1)), nullptr);

    // The iterator must visit each assigned handle exactly once across all flat arrays and the
    // hashmap.
    std::map<GLuint, size_t *> visitedHandleMap;
    for (const auto &idValue : UnsafeResourceMapIter(resourceMap))
    {
        EXPECT_EQ(visitedHandleMap.count(idValue.first), 0u) << "duplicate id=" << idValue.first;
        visitedHandleMap[idValue.first] = idValue.second;
    }
    EXPECT_EQ(visitedHandleMap.size(), kTotalHandle);

    for (size_t index = 0; index < kTotalHandle; ++index)
    {
        const GLuint handle = static_cast<GLuint>(index + 1);
        ASSERT_EQ(visitedHandleMap.count(handle), 1u) << "missing handle=" << handle;
        EXPECT_EQ(visitedHandleMap[handle], &objects[index]);
    }

    for (size_t index = 0; index < kTotalHandle; ++index)
    {
        const AdditionalFlatType handle = static_cast<AdditionalFlatType>(index + 1);
        size_t *erased                  = nullptr;
        EXPECT_TRUE(resourceMap.erase(handle, &erased)) << "handle=" << handle;
        EXPECT_EQ(erased, &objects[index]) << "handle=" << handle;
        EXPECT_FALSE(resourceMap.contains(handle)) << "handle is still present after erase";
    }

    EXPECT_TRUE(UnsafeResourceMapIter(resourceMap).empty());
}

// Tests that the iterator visits all valid handles when resources exist only in the hash map, and
// no ID in the additional flat array has ever been assigned.
TEST(ResourceMapTest, IteratorVisitsHashMapWithoutAdditionalFlatArray)
{
    // Assign handles in the hashmap range to keep mAdditionalFlatResources nullptr.
    // kHashmapIdx0, kHashmapIdx1 >= kInitialFlatResourcesSize + kAdditionalFlatResourcesSize
    //                            >= kLocklessFlatResourcesLimit = 3 + 7 = 10
    constexpr AdditionalFlatType kHashmapIdx0 = 10;
    constexpr AdditionalFlatType kHashmapIdx1 = 11;
    size_t hashmapValue0                      = 100;
    size_t hashmapValue1                      = 200;

    ResourceMap<size_t, AdditionalFlatType> resourceMap;
    resourceMap.assign(kHashmapIdx0, &hashmapValue0);
    resourceMap.assign(kHashmapIdx1, &hashmapValue1);

    EXPECT_TRUE(resourceMap.contains(kHashmapIdx0));
    EXPECT_EQ(resourceMap.query(kHashmapIdx0), &hashmapValue0);
    EXPECT_TRUE(resourceMap.contains(kHashmapIdx1));
    EXPECT_EQ(resourceMap.query(kHashmapIdx1), &hashmapValue1);

    constexpr AdditionalFlatType kInitialArrayIdx    = 1;
    constexpr AdditionalFlatType kAdditionalArrayIdx = 5;

    // Since IDs in the initial and additional flat arrays are never allocated, they should always
    // return false or nullptr.
    EXPECT_FALSE(resourceMap.contains(kInitialArrayIdx));
    EXPECT_EQ(resourceMap.query(kInitialArrayIdx), nullptr);
    EXPECT_FALSE(resourceMap.contains(kAdditionalArrayIdx));
    EXPECT_EQ(resourceMap.query(kAdditionalArrayIdx), nullptr);

    // The iterator must visit both hashmap entries.
    std::map<GLuint, size_t *> visitedHandleMap;
    for (const auto &idValue : UnsafeResourceMapIter(resourceMap))
    {
        visitedHandleMap[idValue.first] = idValue.second;
    }
    EXPECT_EQ(visitedHandleMap.size(), 2u);
    EXPECT_EQ(visitedHandleMap.count(kHashmapIdx0), 1u);
    EXPECT_EQ(visitedHandleMap[kHashmapIdx0], &hashmapValue0);
    EXPECT_EQ(visitedHandleMap.count(kHashmapIdx1), 1u);
    EXPECT_EQ(visitedHandleMap[kHashmapIdx1], &hashmapValue1);

    resourceMap.clear();
}

// Tests that the iterator correctly visits all valid handles stored in the upper portion of the
// additional flat array.
TEST(ResourceMapTest, IteratorVisitsUpperAdditionalFlatArray)
{
    // Additional flat range = [kInitialFlatResourcesSize, kLocklessFlatResourcesLimit) = [3, 10)
    // Assign handles in the upper portion of the additional flat array.
    constexpr AdditionalFlatType kFirstIdx = 7;
    constexpr AdditionalFlatType kLastIdx  = 9;
    size_t value7                          = 7;
    size_t value8                          = 8;
    size_t value9                          = 9;

    ResourceMap<size_t, AdditionalFlatType> resourceMap;
    resourceMap.assign(kFirstIdx, &value7);
    resourceMap.assign(kFirstIdx + 1, &value8);
    resourceMap.assign(kLastIdx, &value9);

    EXPECT_TRUE(resourceMap.contains(kFirstIdx));
    EXPECT_EQ(resourceMap.query(kFirstIdx), &value7);
    EXPECT_TRUE(resourceMap.contains(kFirstIdx + 1));
    EXPECT_EQ(resourceMap.query(kFirstIdx + 1), &value8);
    EXPECT_TRUE(resourceMap.contains(kLastIdx));
    EXPECT_EQ(resourceMap.query(kLastIdx), &value9);

    // Iterator must visit all three handles.
    std::map<GLuint, size_t *> visitedHandleMap;
    for (const auto &idValue : UnsafeResourceMapIter(resourceMap))
    {
        visitedHandleMap[idValue.first] = idValue.second;
    }
    EXPECT_EQ(visitedHandleMap.size(), 3u);
    EXPECT_EQ(visitedHandleMap[kFirstIdx], &value7);
    EXPECT_EQ(visitedHandleMap[kFirstIdx + 1], &value8);
    EXPECT_EQ(visitedHandleMap[kLastIdx], &value9);

    resourceMap.clear();
}

// Tests that the iterator visits all valid handles when they are assigned only in the additional
// flat array and the hash map.
TEST(ResourceMapTest, IteratorVisitsAdditionalFlatArrayAndHashMap)
{
    // Assign handles in the additional flat range [3, 10) and the hashmap (>= 10).  No handles are
    // assigned in the initial flat array [0, 3)
    constexpr AdditionalFlatType kAdditionalIdx0 = 4;
    constexpr AdditionalFlatType kAdditionalIdx1 = 8;
    constexpr AdditionalFlatType kHashmapIdx0    = 20;
    constexpr AdditionalFlatType kHashmapIdx1    = 21;
    size_t additionalValue0                      = 100;
    size_t additionalValue1                      = 200;
    size_t hashmapValue0                         = 300;
    size_t hashmapValue1                         = 400;

    ResourceMap<size_t, AdditionalFlatType> resourceMap;
    resourceMap.assign(kAdditionalIdx0, &additionalValue0);
    resourceMap.assign(kAdditionalIdx1, &additionalValue1);
    resourceMap.assign(kHashmapIdx0, &hashmapValue0);
    resourceMap.assign(kHashmapIdx1, &hashmapValue1);

    EXPECT_TRUE(resourceMap.contains(kAdditionalIdx0));
    EXPECT_EQ(resourceMap.query(kAdditionalIdx0), &additionalValue0);
    EXPECT_TRUE(resourceMap.contains(kAdditionalIdx1));
    EXPECT_EQ(resourceMap.query(kAdditionalIdx1), &additionalValue1);
    EXPECT_TRUE(resourceMap.contains(kHashmapIdx0));
    EXPECT_EQ(resourceMap.query(kHashmapIdx0), &hashmapValue0);
    EXPECT_TRUE(resourceMap.contains(kHashmapIdx1));
    EXPECT_EQ(resourceMap.query(kHashmapIdx1), &hashmapValue1);

    constexpr AdditionalFlatType kInitialArrayIdx = 1;

    // Since IDs in the initial flat array are never allocated, the lookup should always return
    // false or nullptr.
    EXPECT_FALSE(resourceMap.contains(kInitialArrayIdx));
    EXPECT_EQ(resourceMap.query(kInitialArrayIdx), nullptr);

    // The iterator must visit all four handles.
    std::map<GLuint, size_t *> visitedHandleMap;
    for (const auto &idValue : UnsafeResourceMapIter(resourceMap))
    {
        visitedHandleMap[idValue.first] = idValue.second;
    }
    EXPECT_EQ(visitedHandleMap.size(), 4u);
    EXPECT_EQ(visitedHandleMap[kAdditionalIdx0], &additionalValue0);
    EXPECT_EQ(visitedHandleMap[kAdditionalIdx1], &additionalValue1);
    EXPECT_EQ(visitedHandleMap[kHashmapIdx0], &hashmapValue0);
    EXPECT_EQ(visitedHandleMap[kHashmapIdx1], &hashmapValue1);

    resourceMap.clear();
}

// Tests that the iterator correctly transitions from the initial flat array to the hash map when
// the additional flat array has never been allocated.  This verifies that nextResource() skips the
// additional flat range when additionalResources is nullptr, so updateValue() never dereferences a
// null pointer.
TEST(ResourceMapTest, IteratorVisitsInitialFlatArrayAndHashMap)
{
    // Assign handles in the initial flat array (1, 2) and in the hashmap (>= 10).  Since no handles
    // are assigned in the additional flat range [3, 10), mAdditionalFlatResources remains nullptr.
    constexpr AdditionalFlatType kInitialIdx0 = 1;
    constexpr AdditionalFlatType kInitialIdx1 = 2;
    constexpr AdditionalFlatType kHashmapIdx0 = 10;
    constexpr AdditionalFlatType kHashmapIdx1 = 18;
    size_t initialValue0                      = 100;
    size_t initialValue1                      = 200;
    size_t hashmapValue0                      = 300;
    size_t hashmapValue1                      = 400;

    ResourceMap<size_t, AdditionalFlatType> resourceMap;
    resourceMap.assign(kInitialIdx0, &initialValue0);
    resourceMap.assign(kInitialIdx1, &initialValue1);
    resourceMap.assign(kHashmapIdx0, &hashmapValue0);
    resourceMap.assign(kHashmapIdx1, &hashmapValue1);

    EXPECT_TRUE(resourceMap.contains(kInitialIdx0));
    EXPECT_EQ(resourceMap.query(kInitialIdx0), &initialValue0);
    EXPECT_TRUE(resourceMap.contains(kInitialIdx1));
    EXPECT_EQ(resourceMap.query(kInitialIdx1), &initialValue1);
    EXPECT_TRUE(resourceMap.contains(kHashmapIdx0));
    EXPECT_EQ(resourceMap.query(kHashmapIdx0), &hashmapValue0);
    EXPECT_TRUE(resourceMap.contains(kHashmapIdx1));
    EXPECT_EQ(resourceMap.query(kHashmapIdx1), &hashmapValue1);

    // Since IDs in the additional flat array are never allocated, the lookup should always return
    // false or nullptr.
    constexpr AdditionalFlatType kAdditionalArrayIdx = 5;
    EXPECT_FALSE(resourceMap.contains(kAdditionalArrayIdx));
    EXPECT_EQ(resourceMap.query(kAdditionalArrayIdx), nullptr);

    // The iterator must visit all four handles.
    std::map<GLuint, size_t *> visitedHandleMap;
    for (const auto &idValue : UnsafeResourceMapIter(resourceMap))
    {
        visitedHandleMap[idValue.first] = idValue.second;
    }
    EXPECT_EQ(visitedHandleMap.size(), 4u);
    EXPECT_EQ(visitedHandleMap[kInitialIdx0], &initialValue0);
    EXPECT_EQ(visitedHandleMap[kInitialIdx1], &initialValue1);
    EXPECT_EQ(visitedHandleMap[kHashmapIdx0], &hashmapValue0);
    EXPECT_EQ(visitedHandleMap[kHashmapIdx1], &hashmapValue1);

    resourceMap.clear();
}
}  // anonymous namespace
