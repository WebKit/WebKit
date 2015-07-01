//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// IndexRangeCache.cpp: Defines the rx::IndexRangeCache class which stores information about
// ranges of indices.

#include "libANGLE/renderer/IndexRangeCache.h"
#include "libANGLE/formatutils.h"

#include "common/debug.h"

namespace rx
{

template <class IndexType>
static RangeUI ComputeTypedRange(const IndexType *indices, GLsizei count)
{
    unsigned int minIndex = indices[0];
    unsigned int maxIndex = indices[0];

    for (GLsizei i = 1; i < count; i++)
    {
        if (minIndex > indices[i]) minIndex = indices[i];
        if (maxIndex < indices[i]) maxIndex = indices[i];
    }

    return RangeUI(minIndex, maxIndex);
}

RangeUI IndexRangeCache::ComputeRange(GLenum type, const GLvoid *indices, GLsizei count)
{
    switch (type)
    {
      case GL_UNSIGNED_BYTE:
        return ComputeTypedRange(static_cast<const GLubyte*>(indices), count);
      case GL_UNSIGNED_INT:
        return ComputeTypedRange(static_cast<const GLuint*>(indices), count);
      case GL_UNSIGNED_SHORT:
        return ComputeTypedRange(static_cast<const GLushort*>(indices), count);
      default:
        UNREACHABLE();
        return RangeUI();
    }
}

void IndexRangeCache::addRange(GLenum type, unsigned int offset, GLsizei count, const RangeUI &range)
{
    mIndexRangeCache[IndexRange(type, offset, count)] = range;
}

void IndexRangeCache::invalidateRange(unsigned int offset, unsigned int size)
{
    unsigned int invalidateStart = offset;
    unsigned int invalidateEnd = offset + size;

    IndexRangeMap::iterator i = mIndexRangeCache.begin();
    while (i != mIndexRangeCache.end())
    {
        unsigned int rangeStart = i->first.offset;
        unsigned int rangeEnd = i->first.offset + (gl::GetTypeInfo(i->first.type).bytes * i->first.count);

        if (invalidateEnd < rangeStart || invalidateStart > rangeEnd)
        {
            ++i;
        }
        else
        {
            mIndexRangeCache.erase(i++);
        }
    }
}

bool IndexRangeCache::findRange(GLenum type, unsigned int offset, GLsizei count,
                                RangeUI *outRange) const
{
    IndexRangeMap::const_iterator i = mIndexRangeCache.find(IndexRange(type, offset, count));
    if (i != mIndexRangeCache.end())
    {
        if (outRange)        *outRange = i->second;
        return true;
    }
    else
    {
        if (outRange)        *outRange = RangeUI(0, 0);
        return false;
    }
}

void IndexRangeCache::clear()
{
    mIndexRangeCache.clear();
}

IndexRangeCache::IndexRange::IndexRange()
    : type(GL_NONE), offset(0), count(0)
{
}

IndexRangeCache::IndexRange::IndexRange(GLenum typ, intptr_t off, GLsizei c)
    : type(typ), offset(static_cast<unsigned int>(off)), count(c)
{
}

bool IndexRangeCache::IndexRange::operator<(const IndexRange& rhs) const
{
    if (type != rhs.type) return type < rhs.type;
    if (offset != rhs.offset) return offset < rhs.offset;
    return count < rhs.count;
}

}
