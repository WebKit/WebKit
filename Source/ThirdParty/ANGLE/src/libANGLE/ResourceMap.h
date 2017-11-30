//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ResourceMap:
//   An optimized resource map which packs the first set of allocated objects into a
//   flat array, and then falls back to an unordered map for the higher handle values.
//

#ifndef LIBANGLE_RESOURCE_MAP_H_
#define LIBANGLE_RESOURCE_MAP_H_

#include "libANGLE/angletypes.h"

namespace gl
{

template <typename ResourceType>
class ResourceMap final : angle::NonCopyable
{
  public:
    ResourceMap();
    ~ResourceMap();

    ResourceType *query(GLuint handle) const;

    // Returns true if the handle was reserved. Not necessarily if the resource is created.
    bool contains(GLuint handle) const;

    // Returns the element that was at this location.
    bool erase(GLuint handle, ResourceType **resourceOut);

    void assign(GLuint handle, ResourceType *resource);

    // Clears the map.
    void clear();

    using IndexAndResource = std::pair<GLuint, ResourceType *>;
    using HashMap          = std::unordered_map<GLuint, ResourceType *>;

    class Iterator final
    {
      public:
        bool operator==(const Iterator &other) const;
        bool operator!=(const Iterator &other) const;
        Iterator &operator++();
        const IndexAndResource *operator->() const;
        const IndexAndResource &operator*() const;

      private:
        friend class ResourceMap;
        Iterator(const ResourceMap &origin,
                 GLuint flatIndex,
                 typename HashMap::const_iterator hashIndex);
        void updateValue();

        const ResourceMap &mOrigin;
        GLuint mFlatIndex;
        typename HashMap::const_iterator mHashIndex;
        IndexAndResource mValue;
    };

    // null values represent reserved handles.
    Iterator begin() const;
    Iterator end() const;
    Iterator find(GLuint handle) const;

    // Not a constant-time operation, should only be used for verification.
    bool empty() const;

  private:
    friend class Iterator;

    GLuint nextNonNullResource(size_t flatIndex) const;

    // constexpr methods cannot contain reinterpret_cast, so we need a static method.
    static ResourceType *InvalidPointer();
    static constexpr intptr_t kInvalidPointer = static_cast<intptr_t>(-1);

    // Start with 32 maximum elements in the map, which can grow.
    static constexpr size_t kInitialFlatResourcesSize = 0x20;

    // Experimental testing suggests that 16k is a reasonable upper limit.
    static constexpr size_t kFlatResourcesLimit = 0x4000;

    std::vector<ResourceType *> mFlatResources;

    // A map of GL objects indexed by object ID.
    HashMap mHashedResources;
};

template <typename ResourceType>
ResourceMap<ResourceType>::ResourceMap()
    : mFlatResources(kInitialFlatResourcesSize, InvalidPointer()), mHashedResources()
{
}

template <typename ResourceType>
ResourceMap<ResourceType>::~ResourceMap()
{
    ASSERT(empty());
}

template <typename ResourceType>
ResourceType *ResourceMap<ResourceType>::query(GLuint handle) const
{
    if (handle < mFlatResources.size())
    {
        auto value = mFlatResources[handle];
        return (value == InvalidPointer() ? nullptr : value);
    }
    auto it = mHashedResources.find(handle);
    return (it == mHashedResources.end() ? nullptr : it->second);
}

template <typename ResourceType>
bool ResourceMap<ResourceType>::contains(GLuint handle) const
{
    if (handle < mFlatResources.size())
    {
        return (mFlatResources[handle] != InvalidPointer());
    }
    return (mHashedResources.find(handle) != mHashedResources.end());
}

template <typename ResourceType>
bool ResourceMap<ResourceType>::erase(GLuint handle, ResourceType **resourceOut)
{
    if (handle < mFlatResources.size())
    {
        auto &value = mFlatResources[handle];
        if (value == InvalidPointer())
        {
            return false;
        }
        *resourceOut = value;
        value        = InvalidPointer();
    }
    else
    {
        auto it = mHashedResources.find(handle);
        if (it == mHashedResources.end())
        {
            return false;
        }
        *resourceOut = it->second;
        mHashedResources.erase(it);
    }
    return true;
}

template <typename ResourceType>
void ResourceMap<ResourceType>::assign(GLuint handle, ResourceType *resource)
{
    if (handle < kFlatResourcesLimit)
    {
        if (handle >= mFlatResources.size())
        {
            // Use power-of-two.
            size_t newSize = mFlatResources.size();
            while (newSize <= handle)
            {
                newSize *= 2;
            }
            mFlatResources.resize(newSize, nullptr);
        }
        ASSERT(mFlatResources.size() > handle);
        mFlatResources[handle] = resource;
    }
    else
    {
        mHashedResources[handle] = resource;
    }
}

template <typename ResourceType>
typename ResourceMap<ResourceType>::Iterator ResourceMap<ResourceType>::begin() const
{
    return Iterator(*this, nextNonNullResource(0), mHashedResources.begin());
}

template <typename ResourceType>
typename ResourceMap<ResourceType>::Iterator ResourceMap<ResourceType>::end() const
{
    return Iterator(*this, static_cast<GLuint>(mFlatResources.size()), mHashedResources.end());
}

template <typename ResourceType>
typename ResourceMap<ResourceType>::Iterator ResourceMap<ResourceType>::find(GLuint handle) const
{
    if (handle < mFlatResources.size())
    {
        return (mFlatResources[handle] != InvalidPointer()
                    ? Iterator(handle, mHashedResources.begin())
                    : end());
    }
    else
    {
        return mHashedResources.find(handle);
    }
}

template <typename ResourceType>
bool ResourceMap<ResourceType>::empty() const
{
    return (begin() == end());
}

template <typename ResourceType>
void ResourceMap<ResourceType>::clear()
{
    mFlatResources.assign(kInitialFlatResourcesSize, InvalidPointer());
    mHashedResources.clear();
}

template <typename ResourceType>
GLuint ResourceMap<ResourceType>::nextNonNullResource(size_t flatIndex) const
{
    for (size_t index = flatIndex; index < mFlatResources.size(); index++)
    {
        if (mFlatResources[index] != nullptr && mFlatResources[index] != InvalidPointer())
        {
            return static_cast<GLuint>(index);
        }
    }
    return static_cast<GLuint>(mFlatResources.size());
}

template <typename ResourceType>
// static
ResourceType *ResourceMap<ResourceType>::InvalidPointer()
{
    return reinterpret_cast<ResourceType *>(kInvalidPointer);
}

template <typename ResourceType>
ResourceMap<ResourceType>::Iterator::Iterator(
    const ResourceMap &origin,
    GLuint flatIndex,
    typename ResourceMap<ResourceType>::HashMap::const_iterator hashIndex)
    : mOrigin(origin), mFlatIndex(flatIndex), mHashIndex(hashIndex), mValue()
{
    updateValue();
}

template <typename ResourceType>
bool ResourceMap<ResourceType>::Iterator::operator==(const Iterator &other) const
{
    return (mFlatIndex == other.mFlatIndex && mHashIndex == other.mHashIndex);
}

template <typename ResourceType>
bool ResourceMap<ResourceType>::Iterator::operator!=(const Iterator &other) const
{
    return !(*this == other);
}

template <typename ResourceType>
typename ResourceMap<ResourceType>::Iterator &ResourceMap<ResourceType>::Iterator::operator++()
{
    if (mFlatIndex < static_cast<GLuint>(mOrigin.mFlatResources.size()))
    {
        mFlatIndex = mOrigin.nextNonNullResource(mFlatIndex + 1);
    }
    else
    {
        mHashIndex++;
    }
    updateValue();
    return *this;
}

template <typename ResourceType>
const typename ResourceMap<ResourceType>::IndexAndResource
    *ResourceMap<ResourceType>::Iterator::operator->() const
{
    return &mValue;
}

template <typename ResourceType>
const typename ResourceMap<ResourceType>::IndexAndResource
    &ResourceMap<ResourceType>::Iterator::operator*() const
{
    return mValue;
}

template <typename ResourceType>
void ResourceMap<ResourceType>::Iterator::updateValue()
{
    if (mFlatIndex < static_cast<GLuint>(mOrigin.mFlatResources.size()))
    {
        mValue.first  = mFlatIndex;
        mValue.second = mOrigin.mFlatResources[mFlatIndex];
    }
    else if (mHashIndex != mOrigin.mHashedResources.end())
    {
        mValue.first  = mHashIndex->first;
        mValue.second = mHashIndex->second;
    }
}

}  // namespace gl

#endif  // LIBANGLE_RESOURCE_MAP_H_
