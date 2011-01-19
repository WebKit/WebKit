//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// geometry/IndexDataManager.h: Defines the IndexDataManager, a class that
// runs the Buffer translation process for index buffers.

#ifndef LIBGLESV2_GEOMETRY_INDEXDATAMANAGER_H_
#define LIBGLESV2_GEOMETRY_INDEXDATAMANAGER_H_

#include <vector>
#include <cstddef>

#define GL_APICALL
#include <GLES2/gl2.h>

#include "libGLESv2/Context.h"

namespace gl
{

struct TranslatedIndexData
{
    UINT minIndex;
    UINT maxIndex;
    UINT startIndex;

    IDirect3DIndexBuffer9 *indexBuffer;
};

class IndexBuffer
{
  public:
    IndexBuffer(IDirect3DDevice9 *device, UINT size, D3DFORMAT format);
    virtual ~IndexBuffer();

    UINT size() const { return mBufferSize; }
    virtual void *map(UINT requiredSpace, UINT *offset) = 0;
    void unmap();
    virtual void reserveSpace(UINT requiredSpace, GLenum type) = 0;

    IDirect3DIndexBuffer9 *getBuffer() const;

  protected:
    IDirect3DDevice9 *const mDevice;

    IDirect3DIndexBuffer9 *mIndexBuffer;
    UINT mBufferSize;

  private:
    DISALLOW_COPY_AND_ASSIGN(IndexBuffer);
};

class StreamingIndexBuffer : public IndexBuffer
{
  public:
    StreamingIndexBuffer(IDirect3DDevice9 *device, UINT initialSize, D3DFORMAT format);
    ~StreamingIndexBuffer();

    void *map(UINT requiredSpace, UINT *offset);
    void reserveSpace(UINT requiredSpace, GLenum type);

  private:
    UINT mWritePosition;
};

class StaticIndexBuffer : public IndexBuffer
{
  public:
    explicit StaticIndexBuffer(IDirect3DDevice9 *device);
    ~StaticIndexBuffer();

    void *map(UINT requiredSpace, UINT *offset);
    void reserveSpace(UINT requiredSpace, GLenum type);

    bool lookupType(GLenum type);
    UINT lookupRange(intptr_t offset, GLsizei count, UINT *minIndex, UINT *maxIndex);   // Returns the offset into the index buffer, or -1 if not found
    void addRange(intptr_t offset, GLsizei count, UINT minIndex, UINT maxIndex, UINT streamOffset);

  private:
    GLenum mCacheType;
    
    struct IndexRange
    {
        intptr_t offset;
        GLsizei count;

        UINT minIndex;
        UINT maxIndex;
        UINT streamOffset;
    };

    std::vector<IndexRange> mCache;
};

class IndexDataManager
{
  public:
    IndexDataManager(Context *context, IDirect3DDevice9 *evice);
    virtual ~IndexDataManager();

    GLenum prepareIndexData(GLenum type, GLsizei count, Buffer *arrayElementBuffer, const void *indices, TranslatedIndexData *translated);

  private:
    DISALLOW_COPY_AND_ASSIGN(IndexDataManager);

    std::size_t typeSize(GLenum type) const;
    std::size_t indexSize(D3DFORMAT format) const;

    IDirect3DDevice9 *const mDevice;

    StreamingIndexBuffer *mStreamingBufferShort;
    StreamingIndexBuffer *mStreamingBufferInt;
};

}

#endif   // LIBGLESV2_GEOMETRY_INDEXDATAMANAGER_H_
