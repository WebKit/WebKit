//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// IndexRangeCache.h: Defines the gl::IndexRangeCache class which stores information about
// ranges of indices.

#ifndef LIBANGLE_INDEXRANGECACHE_H_
#define LIBANGLE_INDEXRANGECACHE_H_

#include "angle_gl.h"
#include "common/PackedEnums.h"
#include "common/angleutils.h"
#include "common/mathutil.h"

#include <map>

namespace gl
{

struct IndexRangeKey
{
    IndexRangeKey() = default;
    IndexRangeKey(DrawElementsType type, size_t offset, size_t count, bool primitiveRestart);
    bool operator<(const IndexRangeKey &rhs) const;
    bool operator==(const IndexRangeKey &rhs) const;

    DrawElementsType type{DrawElementsType::InvalidEnum};
    size_t offset{0};
    size_t count{0};
    bool primitiveRestartEnabled{false};
};

class IndexRangeCache
{
  public:
    IndexRangeCache();
    ~IndexRangeCache();

    void addRange(DrawElementsType type,
                  size_t offset,
                  size_t count,
                  bool primitiveRestartEnabled,
                  const IndexRange &range);
    bool findRange(DrawElementsType type,
                   size_t offset,
                   size_t count,
                   bool primitiveRestartEnabled,
                   IndexRange *outRange) const;

    void invalidateRange(size_t offset, size_t size);
    void clear();

  private:
    std::map<IndexRangeKey, IndexRange> mIndexRangeCache;
};

// First level cache stored inline at the query site.
class IndexRangeInlineCache
{
  public:
    IndexRangeInlineCache() = default;
    IndexRangeInlineCache(DrawElementsType type,
                          size_t offset,
                          size_t count,
                          bool primitiveRestartEnabled,
                          IndexRange indexRange)
        : mPayload(std::move(indexRange)), mKey(type, offset, count, primitiveRestartEnabled)
    {}

    bool get(DrawElementsType type,
             size_t offset,
             size_t count,
             bool primitiveRestartEnabled,
             IndexRange *indexRangeOut)
    {
        if (mKey == IndexRangeKey(type, offset, count, primitiveRestartEnabled))
        {
            *indexRangeOut = mPayload;
            return true;
        }
        return false;
    }

  private:
    IndexRange mPayload{IndexRange::Undefined{}};
    IndexRangeKey mKey;
};

inline IndexRangeKey::IndexRangeKey(DrawElementsType type_,
                                    size_t offset_,
                                    size_t count_,
                                    bool primitiveRestartEnabled_)
    : type(type_), offset(offset_), count(count_), primitiveRestartEnabled(primitiveRestartEnabled_)
{}

inline bool IndexRangeKey::operator==(const IndexRangeKey &rhs) const
{
    return type == rhs.type && offset == rhs.offset && count == rhs.count &&
           primitiveRestartEnabled == rhs.primitiveRestartEnabled;
}

}  // namespace gl

#endif  // LIBANGLE_INDEXRANGECACHE_H_
