//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ResourceMap_unittest:
//   Unit tests for the ResourceMap template class.
//

#include <gtest/gtest.h>

#include "libANGLE/ResourceMap.h"

using namespace gl;

namespace
{
// Tests assigning slots in the map and then deleting elements.
TEST(ResourceMapTest, AssignAndErase)
{
    constexpr size_t kSize = 64;
    ResourceMap<size_t, GLuint> resourceMap;
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

    ASSERT_TRUE(resourceMap.empty());
}

// Tests assigning slots in the map and then using clear() to free it.
TEST(ResourceMapTest, AssignAndClear)
{
    constexpr size_t kSize = 64;
    ResourceMap<size_t, GLuint> resourceMap;
    std::vector<size_t> objects(kSize, 1);
    for (size_t index = 0; index < kSize; ++index)
    {
        resourceMap.assign(index + 1, &objects[index]);
    }

    resourceMap.clear();
    ASSERT_TRUE(resourceMap.empty());
}

// Tests growing a map more than double the size.
TEST(ResourceMapTest, BigGrowth)
{
    constexpr size_t kSize = 8;

    ResourceMap<size_t, GLuint> resourceMap;
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

    ASSERT_TRUE(resourceMap.empty());
}

// Tests querying unassigned or erased values.
TEST(ResourceMapTest, QueryUnassigned)
{
    constexpr size_t kSize = 8;

    ResourceMap<size_t, GLuint> resourceMap;
    std::vector<size_t> objects;

    for (size_t index = 0; index < kSize; ++index)
    {
        objects.push_back(index);
    }

    ASSERT_FALSE(resourceMap.contains(0));
    ASSERT_EQ(nullptr, resourceMap.query(0));
    ASSERT_FALSE(resourceMap.contains(100));
    ASSERT_EQ(nullptr, resourceMap.query(100));

    for (size_t &object : objects)
    {
        resourceMap.assign(object, &object);
    }

    ASSERT_FALSE(resourceMap.empty());

    for (size_t &object : objects)
    {
        ASSERT_TRUE(resourceMap.contains(object));
        ASSERT_EQ(&object, resourceMap.query(object));
    }

    ASSERT_FALSE(resourceMap.contains(10));
    ASSERT_EQ(nullptr, resourceMap.query(10));
    ASSERT_FALSE(resourceMap.contains(100));
    ASSERT_EQ(nullptr, resourceMap.query(100));

    for (size_t object : objects)
    {
        size_t *found = nullptr;
        ASSERT_TRUE(resourceMap.erase(object, &found));
        ASSERT_EQ(object, *found);
    }

    ASSERT_TRUE(resourceMap.empty());

    ASSERT_FALSE(resourceMap.contains(0));
    ASSERT_EQ(nullptr, resourceMap.query(0));
    ASSERT_FALSE(resourceMap.contains(100));
    ASSERT_EQ(nullptr, resourceMap.query(100));
}
}  // anonymous namespace
