//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// geometry/dx9.h: Direct3D 9-based implementation of BufferBackEnd, TranslatedVertexBuffer and TranslatedIndexBuffer.

#ifndef LIBGLESV2_GEOMETRY_DX9_H_
#define LIBGLESV2_GEOMETRY_DX9_H_

#include <d3d9.h>

#include "libGLESv2/Buffer.h"
#include "libGLESv2/geometry/backend.h"

namespace gl
{

class Dx9BackEnd : public BufferBackEnd
{
  public:
    explicit Dx9BackEnd(Context *context, IDirect3DDevice9 *d3ddevice);
    ~Dx9BackEnd();

    virtual bool supportIntIndices();

    virtual TranslatedVertexBuffer *createVertexBuffer(std::size_t size);
    virtual TranslatedVertexBuffer *createVertexBufferForStrideZero(std::size_t size);
    virtual TranslatedIndexBuffer *createIndexBuffer(std::size_t size, GLenum type);
    virtual FormatConverter getFormatConverter(GLenum type, std::size_t size, bool normalize);

    virtual bool validateStream(GLenum type, std::size_t size, std::size_t stride, std::size_t offset) const;

    virtual GLenum setupIndicesPreDraw(const TranslatedIndexData &indexInfo);
    virtual GLenum setupAttributesPreDraw(const TranslatedAttribute *attributes);

  private:
    IDirect3DDevice9 *mDevice;

    bool mUseInstancingForStrideZero;
    bool mSupportIntIndices;

    bool mAppliedAttribEnabled[MAX_VERTEX_ATTRIBS];

    enum StreamFrequency
    {
        STREAM_FREQUENCY_UNINSTANCED = 0,
        STREAM_FREQUENCY_INDEXED,
        STREAM_FREQUENCY_INSTANCED
    };

    StreamFrequency mStreamFrequency[MAX_VERTEX_ATTRIBS+1];

    struct TranslationInfo
    {
        FormatConverter formatConverter;
        D3DDECLTYPE d3dDeclType;
    };

    enum { NUM_GL_VERTEX_ATTRIB_TYPES = 6 };

    TranslationInfo mAttributeTypes[NUM_GL_VERTEX_ATTRIB_TYPES][2][4]; // [GL types as enumerated by typeIndex()][normalized][size-1]

    struct TranslationDescription
    {
        DWORD capsFlag;
        TranslationInfo preferredConversion;
        TranslationInfo fallbackConversion;
    };

    // This table is used to generate mAttributeTypes.
    static const TranslationDescription mPossibleTranslations[NUM_GL_VERTEX_ATTRIB_TYPES][2][4]; // [GL types as enumerated by typeIndex()][normalized][size-1]

    void checkVertexCaps(DWORD declTypes);

    unsigned int typeIndex(GLenum type) const;

    class Dx9VertexBuffer : public TranslatedVertexBuffer
    {
      public:
        Dx9VertexBuffer(IDirect3DDevice9 *device, std::size_t size);
        virtual ~Dx9VertexBuffer();

        IDirect3DVertexBuffer9 *getBuffer() const;

      protected:
        Dx9VertexBuffer(IDirect3DDevice9 *device, std::size_t size, DWORD usageFlags);

        virtual void *map();
        virtual void unmap();

        virtual void recycle();
        virtual void *streamingMap(std::size_t offset, std::size_t size);

      private:
        IDirect3DVertexBuffer9 *mVertexBuffer;
    };

    class Dx9VertexBufferZeroStrideWorkaround : public Dx9VertexBuffer
    {
      public:
        Dx9VertexBufferZeroStrideWorkaround(IDirect3DDevice9 *device, std::size_t size);

      protected:
        virtual void recycle();
        virtual void *streamingMap(std::size_t offset, std::size_t size);
    };

    class Dx9IndexBuffer : public TranslatedIndexBuffer
    {
      public:
        Dx9IndexBuffer(IDirect3DDevice9 *device, std::size_t size, GLenum type);
        virtual ~Dx9IndexBuffer();

        IDirect3DIndexBuffer9 *getBuffer() const;

      protected:
        virtual void *map();
        virtual void unmap();

        virtual void recycle();
        virtual void *streamingMap(std::size_t offset, std::size_t size);

      private:
        IDirect3DIndexBuffer9 *mIndexBuffer;
    };

    IDirect3DVertexBuffer9 *getDxBuffer(TranslatedVertexBuffer *vb) const;
    IDirect3DIndexBuffer9 *getDxBuffer(TranslatedIndexBuffer *ib) const;

    D3DDECLTYPE mapAttributeType(GLenum type, std::size_t size, bool normalized) const;
};

}

#endif   // LIBGLESV2_GEOMETRY_DX9_H_
