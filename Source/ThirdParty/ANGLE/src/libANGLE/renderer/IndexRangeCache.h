//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// IndexRangeCache.h: Defines the rx::IndexRangeCache class which stores information about
// ranges of indices.

#ifndef LIBANGLE_RENDERER_INDEXRANGECACHE_H_
#define LIBANGLE_RENDERER_INDEXRANGECACHE_H_

#include "common/angleutils.h"
#include "common/mathutil.h"

#include "angle_gl.h"

#include <map>

namespace rx
{

class IndexRangeCache
{
  public:
    void addRange(GLenum type, unsigned int offset, GLsizei count, const RangeUI &range);
    bool findRange(GLenum type, unsigned int offset, GLsizei count, RangeUI *rangeOut) const;

    void invalidateRange(unsigned int offset, unsigned int size);
    void clear();

    static RangeUI ComputeRange(GLenum type, const GLvoid *indices, GLsizei count);

  private:
    struct IndexRange
    {
        GLenum type;
        unsigned int offset;
        GLsizei count;

        IndexRange();
        IndexRange(GLenum type, intptr_t offset, GLsizei count);

        bool operator<(const IndexRange& rhs) const;
    };

    typedef std::map<IndexRange, RangeUI> IndexRangeMap;
    IndexRangeMap mIndexRangeCache;
};

}

#endif // LIBANGLE_RENDERER_INDEXRANGECACHE_H_
