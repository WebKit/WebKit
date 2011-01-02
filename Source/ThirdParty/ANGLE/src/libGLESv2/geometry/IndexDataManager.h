//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// geometry/IndexDataManager.h: Defines the IndexDataManager, a class that
// runs the Buffer translation process for index buffers.

#ifndef LIBGLESV2_GEOMETRY_INDEXDATAMANAGER_H_
#define LIBGLESV2_GEOMETRY_INDEXDATAMANAGER_H_

#include <bitset>
#include <cstddef>

#define GL_APICALL
#include <GLES2/gl2.h>

#include "libGLESv2/Context.h"

namespace gl
{

class Buffer;
class BufferBackEnd;
class TranslatedIndexBuffer;
struct FormatConverter;

struct TranslatedIndexData
{
    GLuint minIndex;
    GLuint maxIndex;
    GLuint count;
    GLuint indexSize;

    TranslatedIndexBuffer *buffer;
    GLsizei offset;
};

class IndexDataManager
{
  public:
    IndexDataManager(Context *context, BufferBackEnd *backend);
    ~IndexDataManager();

    GLenum preRenderValidate(GLenum mode, GLenum type, GLsizei count, Buffer *arrayElementBuffer, const void *indices, TranslatedIndexData *translated);
    GLenum preRenderValidateUnindexed(GLenum mode, GLsizei count, TranslatedIndexData *indexInfo);

  private:
    std::size_t IndexDataManager::typeSize(GLenum type) const;
    std::size_t IndexDataManager::indexSize(GLenum type) const;
    std::size_t spaceRequired(GLenum type, GLsizei count) const;
    TranslatedIndexBuffer *prepareIndexBuffer(GLenum type, std::size_t requiredSpace);

    Context *mContext;
    BufferBackEnd *mBackend;

    bool mIntIndicesSupported;

    TranslatedIndexBuffer *mStreamBufferShort;
    TranslatedIndexBuffer *mStreamBufferInt;

    TranslatedIndexBuffer *mCountingBuffer;
    GLsizei mCountingBufferSize;

    TranslatedIndexBuffer *mLineLoopBuffer;
};

}

#endif   // LIBGLESV2_GEOMETRY_INDEXDATAMANAGER_H_
