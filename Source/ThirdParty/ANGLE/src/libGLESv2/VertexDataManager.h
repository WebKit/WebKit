//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VertexDataManager.h: Defines the VertexDataManager, a class that
// runs the Buffer translation process.

#ifndef LIBGLESV2_VERTEXDATAMANAGER_H_
#define LIBGLESV2_VERTEXDATAMANAGER_H_

#include <vector>
#include <cstddef>

#define GL_APICALL
#include <GLES2/gl2.h>

#include "libGLESv2/Context.h"

namespace gl
{

struct TranslatedAttribute
{
    bool active;

    D3DDECLTYPE type;
    UINT offset;
    UINT stride;   // 0 means not to advance the read pointer at all

    IDirect3DVertexBuffer9 *vertexBuffer;
    unsigned int serial;
};

class VertexBuffer
{
  public:
    VertexBuffer(IDirect3DDevice9 *device, std::size_t size, DWORD usageFlags);
    virtual ~VertexBuffer();

    void unmap();

    IDirect3DVertexBuffer9 *getBuffer() const;
    unsigned int getSerial() const;

  protected:
    IDirect3DDevice9 *const mDevice;
    IDirect3DVertexBuffer9 *mVertexBuffer;

    unsigned int mSerial;
    static unsigned int issueSerial();
    static unsigned int mCurrentSerial;

  private:
    DISALLOW_COPY_AND_ASSIGN(VertexBuffer);
};

class ArrayVertexBuffer : public VertexBuffer
{
  public:
    ArrayVertexBuffer(IDirect3DDevice9 *device, std::size_t size, DWORD usageFlags);
    ~ArrayVertexBuffer();

    std::size_t size() const { return mBufferSize; }
    virtual void *map(const VertexAttribute &attribute, std::size_t requiredSpace, std::size_t *streamOffset) = 0;
    virtual void reserveRequiredSpace() = 0;
    void addRequiredSpace(UINT requiredSpace);

  protected:
    std::size_t mBufferSize;
    std::size_t mWritePosition;
    std::size_t mRequiredSpace;
};

class StreamingVertexBuffer : public ArrayVertexBuffer
{
  public:
    StreamingVertexBuffer(IDirect3DDevice9 *device, std::size_t initialSize);
    ~StreamingVertexBuffer();

    void *map(const VertexAttribute &attribute, std::size_t requiredSpace, std::size_t *streamOffset);
    void reserveRequiredSpace();
};

class StaticVertexBuffer : public ArrayVertexBuffer
{
  public:
    explicit StaticVertexBuffer(IDirect3DDevice9 *device);
    ~StaticVertexBuffer();

    void *map(const VertexAttribute &attribute, std::size_t requiredSpace, std::size_t *streamOffset);
    void reserveRequiredSpace();

    std::size_t lookupAttribute(const VertexAttribute &attribute);   // Returns the offset into the vertex buffer, or -1 if not found

  private:
    struct VertexElement
    {
        GLenum type;
        GLint size;
        GLsizei stride;
        bool normalized;
        int attributeOffset;

        std::size_t streamOffset;
    };

    std::vector<VertexElement> mCache;
};

class VertexDataManager
{
  public:
    VertexDataManager(Context *context, IDirect3DDevice9 *backend);
    virtual ~VertexDataManager();

    void dirtyCurrentValue(int index) { mDirtyCurrentValue[index] = true; }

    GLenum prepareVertexData(GLint start, GLsizei count, TranslatedAttribute *outAttribs);

  private:
    DISALLOW_COPY_AND_ASSIGN(VertexDataManager);

    std::size_t spaceRequired(const VertexAttribute &attrib, std::size_t count) const;
    std::size_t writeAttributeData(ArrayVertexBuffer *vertexBuffer, GLint start, GLsizei count, const VertexAttribute &attribute);

    Context *const mContext;
    IDirect3DDevice9 *const mDevice;

    StreamingVertexBuffer *mStreamingBuffer;

    bool mDirtyCurrentValue[MAX_VERTEX_ATTRIBS];
    StreamingVertexBuffer *mCurrentValueBuffer[MAX_VERTEX_ATTRIBS];
    std::size_t mCurrentValueOffsets[MAX_VERTEX_ATTRIBS];

    // Attribute format conversion
    struct FormatConverter
    {
        bool identity;
        std::size_t outputElementSize;
        void (*convertArray)(const void *in, std::size_t stride, std::size_t n, void *out);
        D3DDECLTYPE d3dDeclType;
    };

    enum { NUM_GL_VERTEX_ATTRIB_TYPES = 6 };

    FormatConverter mAttributeTypes[NUM_GL_VERTEX_ATTRIB_TYPES][2][4];   // [GL types as enumerated by typeIndex()][normalized][size - 1]

    struct TranslationDescription
    {
        DWORD capsFlag;
        FormatConverter preferredConversion;
        FormatConverter fallbackConversion;
    };

    // This table is used to generate mAttributeTypes.
    static const TranslationDescription mPossibleTranslations[NUM_GL_VERTEX_ATTRIB_TYPES][2][4]; // [GL types as enumerated by typeIndex()][normalized][size - 1]

    void checkVertexCaps(DWORD declTypes);

    unsigned int typeIndex(GLenum type) const;
    const FormatConverter &formatConverter(const VertexAttribute &attribute) const;
};

}

#endif   // LIBGLESV2_VERTEXDATAMANAGER_H_
