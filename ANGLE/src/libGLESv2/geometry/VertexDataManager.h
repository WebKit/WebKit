//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// geometry/VertexDataManager.h: Defines the VertexDataManager, a class that
// runs the Buffer translation process.

#ifndef LIBGLESV2_GEOMETRY_VERTEXDATAMANAGER_H_
#define LIBGLESV2_GEOMETRY_VERTEXDATAMANAGER_H_

#include <bitset>
#include <cstddef>

#define GL_APICALL
#include <GLES2/gl2.h>

#include "libGLESv2/Context.h"

namespace gl
{

class Buffer;
class BufferBackEnd;
class TranslatedVertexBuffer;
struct TranslatedAttribute;
struct FormatConverter;
struct TranslatedIndexData;

class VertexDataManager
{
  public:
    VertexDataManager(Context *context, BufferBackEnd *backend);
    ~VertexDataManager();

    void dirtyCurrentValues() { mDirtyCurrentValues = true; }

    GLenum preRenderValidate(GLint start,
                             GLsizei count,
                             TranslatedAttribute *outAttribs);

  private:
    std::bitset<MAX_VERTEX_ATTRIBS> getActiveAttribs() const;

    void processNonArrayAttributes(const AttributeState *attribs, const std::bitset<MAX_VERTEX_ATTRIBS> &activeAttribs, TranslatedAttribute *translated, std::size_t count);

    std::size_t typeSize(GLenum type) const;
    std::size_t interpretGlStride(const AttributeState &attrib) const;

    std::size_t roundUp(std::size_t x, std::size_t multiple) const;
    std::size_t spaceRequired(const AttributeState &attrib, std::size_t maxVertex) const;

    Context *mContext;
    BufferBackEnd *mBackend;

    TranslatedVertexBuffer *mStreamBuffer;

    bool mDirtyCurrentValues;
    std::size_t mCurrentValueOffset;            // Offset within mCurrentValueBuffer that the current attribute values were last loaded at.
    TranslatedVertexBuffer *mCurrentValueBuffer;
};

}

#endif   // LIBGLESV2_GEOMETRY_VERTEXDATAMANAGER_H_
