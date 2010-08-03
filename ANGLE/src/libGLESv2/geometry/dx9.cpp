//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// geometry/dx9.h: Direct3D 9-based implementation of BufferBackEnd, TranslatedVertexBuffer and TranslatedIndexBuffer.

#include "libGLESv2/geometry/dx9.h"

#include <cstddef>

#define GL_APICALL
#include <GLES2/gl2.h>

#include "common/debug.h"

#include "libGLESv2/Context.h"
#include "libGLESv2/main.h"
#include "libGLESv2/geometry/vertexconversion.h"
#include "libGLESv2/geometry/IndexDataManager.h"

namespace
{
// Mapping from OpenGL-ES vertex attrib type to D3D decl type:
//
// BYTE                 SHORT (Cast)
// BYTE-norm            FLOAT (Normalize) (can't be exactly represented as SHORT-norm)
// UNSIGNED_BYTE        UBYTE4 (Identity) or SHORT (Cast)
// UNSIGNED_BYTE-norm   UBYTE4N (Identity) or FLOAT (Normalize)
// SHORT                SHORT (Identity)
// SHORT-norm           SHORT-norm (Identity) or FLOAT (Normalize)
// UNSIGNED_SHORT       FLOAT (Cast)
// UNSIGNED_SHORT-norm  USHORT-norm (Identity) or FLOAT (Normalize)
// FIXED (not in WebGL) FLOAT (FixedToFloat)
// FLOAT                FLOAT (Identity)

// GLToCType maps from GL type (as GLenum) to the C typedef. 
template <GLenum GLType> struct GLToCType { };

template <> struct GLToCType<GL_BYTE> { typedef GLbyte type; };
template <> struct GLToCType<GL_UNSIGNED_BYTE> { typedef GLubyte type; };
template <> struct GLToCType<GL_SHORT> { typedef GLshort type; };
template <> struct GLToCType<GL_UNSIGNED_SHORT> { typedef GLushort type; };
template <> struct GLToCType<GL_FIXED> { typedef GLuint type; };
template <> struct GLToCType<GL_FLOAT> { typedef GLfloat type; };

// This differs from D3DDECLTYPE in that it is unsized. (Size expansion is applied last.)
enum D3DVertexType
{
    D3DVT_FLOAT,
    D3DVT_SHORT,
    D3DVT_SHORT_NORM,
    D3DVT_UBYTE,
    D3DVT_UBYTE_NORM,
    D3DVT_USHORT_NORM
};

// D3DToCType maps from D3D vertex type (as enum D3DVertexType) to the corresponding C type.
template <unsigned int D3DType> struct D3DToCType { };

template <> struct D3DToCType<D3DVT_FLOAT> { typedef float type; };
template <> struct D3DToCType<D3DVT_SHORT> { typedef short type; };
template <> struct D3DToCType<D3DVT_SHORT_NORM> { typedef short type; };
template <> struct D3DToCType<D3DVT_UBYTE> { typedef unsigned char type; };
template <> struct D3DToCType<D3DVT_UBYTE_NORM> { typedef unsigned char type; };
template <> struct D3DToCType<D3DVT_USHORT_NORM> { typedef unsigned short type; };

// Encode the type/size combinations that D3D permits. For each type/size it expands to a widener that will provide the appropriate final size.
template <unsigned int type, int size>
struct WidenRule
{
};

template <int size> struct WidenRule<D3DVT_FLOAT, size>          : gl::NoWiden<size> { };
template <int size> struct WidenRule<D3DVT_SHORT, size>          : gl::WidenToEven<size> { };
template <int size> struct WidenRule<D3DVT_SHORT_NORM, size>     : gl::WidenToEven<size> { };
template <int size> struct WidenRule<D3DVT_UBYTE, size>          : gl::WidenToFour<size> { };
template <int size> struct WidenRule<D3DVT_UBYTE_NORM, size>     : gl::WidenToFour<size> { };
template <int size> struct WidenRule<D3DVT_USHORT_NORM, size>    : gl::WidenToEven<size> { };

// VertexTypeFlags encodes the D3DCAPS9::DeclType flag and vertex declaration flag for each D3D vertex type & size combination.
template <unsigned int d3dtype, int size>
struct VertexTypeFlags
{
};

template <unsigned int capflag, unsigned int declflag>
struct VertexTypeFlagsHelper
{
    enum { capflag = capflag };
    enum { declflag = declflag };
};

template <> struct VertexTypeFlags<D3DVT_FLOAT, 1> : VertexTypeFlagsHelper<0, D3DDECLTYPE_FLOAT1> { };
template <> struct VertexTypeFlags<D3DVT_FLOAT, 2> : VertexTypeFlagsHelper<0, D3DDECLTYPE_FLOAT2> { };
template <> struct VertexTypeFlags<D3DVT_FLOAT, 3> : VertexTypeFlagsHelper<0, D3DDECLTYPE_FLOAT3> { };
template <> struct VertexTypeFlags<D3DVT_FLOAT, 4> : VertexTypeFlagsHelper<0, D3DDECLTYPE_FLOAT4> { };
template <> struct VertexTypeFlags<D3DVT_SHORT, 2> : VertexTypeFlagsHelper<0, D3DDECLTYPE_SHORT2> { };
template <> struct VertexTypeFlags<D3DVT_SHORT, 4> : VertexTypeFlagsHelper<0, D3DDECLTYPE_SHORT4> { };
template <> struct VertexTypeFlags<D3DVT_SHORT_NORM, 2> : VertexTypeFlagsHelper<D3DDTCAPS_SHORT2N, D3DDECLTYPE_SHORT2N> { };
template <> struct VertexTypeFlags<D3DVT_SHORT_NORM, 4> : VertexTypeFlagsHelper<D3DDTCAPS_SHORT4N, D3DDECLTYPE_SHORT4N> { };
template <> struct VertexTypeFlags<D3DVT_UBYTE, 4> : VertexTypeFlagsHelper<D3DDTCAPS_UBYTE4, D3DDECLTYPE_UBYTE4> { };
template <> struct VertexTypeFlags<D3DVT_UBYTE_NORM, 4> : VertexTypeFlagsHelper<D3DDTCAPS_UBYTE4N, D3DDECLTYPE_UBYTE4N> { };
template <> struct VertexTypeFlags<D3DVT_USHORT_NORM, 2> : VertexTypeFlagsHelper<D3DDTCAPS_USHORT2N, D3DDECLTYPE_USHORT2N> { };
template <> struct VertexTypeFlags<D3DVT_USHORT_NORM, 4> : VertexTypeFlagsHelper<D3DDTCAPS_USHORT4N, D3DDECLTYPE_USHORT4N> { };


// VertexTypeMapping maps GL type & normalized flag to preferred and fallback D3D vertex types (as D3DVertexType enums).
template <GLenum GLtype, bool normalized>
struct VertexTypeMapping
{
};

template <D3DVertexType Preferred, D3DVertexType Fallback = Preferred>
struct VertexTypeMappingBase
{
    enum { preferred = Preferred };
    enum { fallback = Fallback };
};

template <> struct VertexTypeMapping<GL_BYTE, false>                        : VertexTypeMappingBase<D3DVT_SHORT> { };                       // Cast
template <> struct VertexTypeMapping<GL_BYTE, true>                         : VertexTypeMappingBase<D3DVT_FLOAT> { };                       // Normalize
template <> struct VertexTypeMapping<GL_UNSIGNED_BYTE, false>               : VertexTypeMappingBase<D3DVT_UBYTE, D3DVT_FLOAT> { };          // Identity, Cast
template <> struct VertexTypeMapping<GL_UNSIGNED_BYTE, true>                : VertexTypeMappingBase<D3DVT_UBYTE_NORM, D3DVT_FLOAT> { };     // Identity, Normalize
template <> struct VertexTypeMapping<GL_SHORT, false>                       : VertexTypeMappingBase<D3DVT_SHORT> { };                       // Identity
template <> struct VertexTypeMapping<GL_SHORT, true>                        : VertexTypeMappingBase<D3DVT_SHORT_NORM, D3DVT_FLOAT> { };     // Cast, Normalize
template <> struct VertexTypeMapping<GL_UNSIGNED_SHORT, false>              : VertexTypeMappingBase<D3DVT_FLOAT> { };                       // Cast
template <> struct VertexTypeMapping<GL_UNSIGNED_SHORT, true>               : VertexTypeMappingBase<D3DVT_USHORT_NORM, D3DVT_FLOAT> { };    // Cast, Normalize
template <bool normalized> struct VertexTypeMapping<GL_FIXED, normalized>   : VertexTypeMappingBase<D3DVT_FLOAT> { };                       // FixedToFloat
template <bool normalized> struct VertexTypeMapping<GL_FLOAT, normalized>   : VertexTypeMappingBase<D3DVT_FLOAT> { };                       // Identity


// Given a GL type & norm flag and a D3D type, ConversionRule provides the type conversion rule (Cast, Normalize, Identity, FixedToFloat).
// The conversion rules themselves are defined in vertexconversion.h.

// Almost all cases are covered by Cast (including those that are actually Identity since Cast<T,T> knows it's an identity mapping).
template <GLenum fromType, bool normalized, unsigned int toType>
struct ConversionRule : gl::Cast<typename GLToCType<fromType>::type, typename D3DToCType<toType>::type>
{
};

// All conversions from normalized types to float use the Normalize operator.
template <GLenum fromType> struct ConversionRule<fromType, true, D3DVT_FLOAT> : gl::Normalize<typename GLToCType<fromType>::type> { };

// Use a full specialisation for this so that it preferentially matches ahead of the generic normalize-to-float rules.
template <> struct ConversionRule<GL_FIXED, true, D3DVT_FLOAT> : gl::FixedToFloat<GLuint, 16> { };
template <> struct ConversionRule<GL_FIXED, false, D3DVT_FLOAT> : gl::FixedToFloat<GLuint, 16> { };

// A 2-stage construction is used for DefaultVertexValues because float must use SimpleDefaultValues (i.e. 0/1)
// whether it is normalized or not.
template <class T, bool normalized>
struct DefaultVertexValuesStage2
{
};

template <class T> struct DefaultVertexValuesStage2<T, true>  : gl::NormalizedDefaultValues<T> { };
template <class T> struct DefaultVertexValuesStage2<T, false> : gl::SimpleDefaultValues<T> { };

// Work out the default value rule for a D3D type (expressed as the C type) and 
template <class T, bool normalized>
struct DefaultVertexValues : DefaultVertexValuesStage2<T, normalized>
{
};

template <bool normalized> struct DefaultVertexValues<float, normalized> : gl::SimpleDefaultValues<float> { };

// Policy rules for use with Converter, to choose whether to use the preferred or fallback conversion.
// The fallback conversion produces an output that all D3D9 devices must support.
template <class T> struct UsePreferred { enum { type = T::preferred }; };
template <class T> struct UseFallback { enum { type = T::fallback }; };

// Converter ties it all together. Given an OpenGL type/norm/size and choice of preferred/fallback conversion,
// it provides all the members of the appropriate VertexDataConverter, the D3DCAPS9::DeclTypes flag in cap flag
// and the D3DDECLTYPE member needed for the vertex declaration in declflag.
template <GLenum fromType, bool normalized, int size, template <class T> class PreferenceRule>
struct Converter
    : gl::VertexDataConverter<typename GLToCType<fromType>::type,
                              WidenRule<PreferenceRule< VertexTypeMapping<fromType, normalized> >::type, size>,
                              ConversionRule<fromType,
                                             normalized,
                                             PreferenceRule< VertexTypeMapping<fromType, normalized> >::type>,
                              DefaultVertexValues<typename D3DToCType<PreferenceRule< VertexTypeMapping<fromType, normalized> >::type>::type, normalized > >
{
private:
    enum { d3dtype = PreferenceRule< VertexTypeMapping<fromType, normalized> >::type };
    enum { d3dsize = WidenRule<d3dtype, size>::finalWidth };

public:
    enum { capflag = VertexTypeFlags<d3dtype, d3dsize>::capflag };
    enum { declflag = VertexTypeFlags<d3dtype, d3dsize>::declflag };
};

}

namespace gl
{
Dx9BackEnd::Dx9BackEnd(Context *context, IDirect3DDevice9 *d3ddevice)
    : mDevice(d3ddevice)
{
    mDevice->AddRef();

    for (int i = 0; i < MAX_VERTEX_ATTRIBS; ++i)
    {
        mAppliedAttribEnabled[i] = true;
        mStreamFrequency[i] = STREAM_FREQUENCY_UNINSTANCED;
    }

    mStreamFrequency[MAX_VERTEX_ATTRIBS] = STREAM_FREQUENCY_UNINSTANCED;

    D3DCAPS9 caps = context->getDeviceCaps();

    IDirect3D9 *d3dObject;
    mDevice->GetDirect3D(&d3dObject);

    D3DADAPTER_IDENTIFIER9 ident;
    d3dObject->GetAdapterIdentifier(caps.AdapterOrdinal, 0, &ident);
    d3dObject->Release();

    // Instancing is mandatory for all HW with SM3 vertex shaders, but avoid hardware where it does not work.
    mUseInstancingForStrideZero = (caps.VertexShaderVersion >= D3DVS_VERSION(3, 0) && ident.VendorId != 0x8086);
    mSupportIntIndices = (caps.MaxVertexIndex >= (1 << 16));

    checkVertexCaps(caps.DeclTypes);
}

Dx9BackEnd::~Dx9BackEnd()
{
    mDevice->Release();
}

bool Dx9BackEnd::supportIntIndices()
{
    return mSupportIntIndices;
}

// Initialise a TranslationInfo
#define TRANSLATION(type, norm, size, preferred)                                    \
    {                                                                               \
        {                                                                           \
            Converter<type, norm, size, preferred>::identity,                       \
            Converter<type, norm, size, preferred>::finalSize,                      \
            Converter<type, norm, size, preferred>::convertArray,                   \
        },                                                                          \
        static_cast<D3DDECLTYPE>(Converter<type, norm, size, preferred>::declflag)  \
    }

#define TRANSLATION_FOR_TYPE_NORM_SIZE(type, norm, size)    \
    {                                                       \
        Converter<type, norm, size, UsePreferred>::capflag, \
        TRANSLATION(type, norm, size, UsePreferred),        \
        TRANSLATION(type, norm, size, UseFallback)          \
    }

#define TRANSLATIONS_FOR_TYPE(type)                                                                                                                                                                         \
    {                                                                                                                                                                                                       \
        { TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 1), TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 2), TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 3), TRANSLATION_FOR_TYPE_NORM_SIZE(type, false, 4) }, \
        { TRANSLATION_FOR_TYPE_NORM_SIZE(type, true, 1), TRANSLATION_FOR_TYPE_NORM_SIZE(type, true, 2), TRANSLATION_FOR_TYPE_NORM_SIZE(type, true, 3), TRANSLATION_FOR_TYPE_NORM_SIZE(type, true, 4) },     \
    }

const Dx9BackEnd::TranslationDescription Dx9BackEnd::mPossibleTranslations[NUM_GL_VERTEX_ATTRIB_TYPES][2][4] = // [GL types as enumerated by typeIndex()][normalized][size-1]
{
    TRANSLATIONS_FOR_TYPE(GL_BYTE),
    TRANSLATIONS_FOR_TYPE(GL_UNSIGNED_BYTE),
    TRANSLATIONS_FOR_TYPE(GL_SHORT),
    TRANSLATIONS_FOR_TYPE(GL_UNSIGNED_SHORT),
    TRANSLATIONS_FOR_TYPE(GL_FIXED),
    TRANSLATIONS_FOR_TYPE(GL_FLOAT)
};

void Dx9BackEnd::checkVertexCaps(DWORD declTypes)
{
    for (unsigned int i = 0; i < NUM_GL_VERTEX_ATTRIB_TYPES; i++)
    {
        for (unsigned int j = 0; j < 2; j++)
        {
            for (unsigned int k = 0; k < 4; k++)
            {
                if (mPossibleTranslations[i][j][k].capsFlag == 0 || (declTypes & mPossibleTranslations[i][j][k].capsFlag) != 0)
                {
                    mAttributeTypes[i][j][k] = mPossibleTranslations[i][j][k].preferredConversion;
                }
                else
                {
                    mAttributeTypes[i][j][k] = mPossibleTranslations[i][j][k].fallbackConversion;
                }
            }
        }
    }
}

TranslatedVertexBuffer *Dx9BackEnd::createVertexBuffer(std::size_t size)
{
    return new Dx9VertexBuffer(mDevice, size);
}

TranslatedVertexBuffer *Dx9BackEnd::createVertexBufferForStrideZero(std::size_t size)
{
    if (mUseInstancingForStrideZero)
    {
        return new Dx9VertexBuffer(mDevice, size);
    }
    else
    {
        return new Dx9VertexBufferZeroStrideWorkaround(mDevice, size);
    }
}

TranslatedIndexBuffer *Dx9BackEnd::createIndexBuffer(std::size_t size, GLenum type)
{
    return new Dx9IndexBuffer(mDevice, size, type);
}

// This is used to index mAttributeTypes and mPossibleTranslations.
unsigned int Dx9BackEnd::typeIndex(GLenum type) const
{
    switch (type)
    {
      case GL_BYTE: return 0;
      case GL_UNSIGNED_BYTE: return 1;
      case GL_SHORT: return 2;
      case GL_UNSIGNED_SHORT: return 3;
      case GL_FIXED: return 4;
      case GL_FLOAT: return 5;

      default: UNREACHABLE(); return 5;
    }
}

FormatConverter Dx9BackEnd::getFormatConverter(GLenum type, std::size_t size, bool normalize)
{
    return mAttributeTypes[typeIndex(type)][normalize][size-1].formatConverter;
}

D3DDECLTYPE Dx9BackEnd::mapAttributeType(GLenum type, std::size_t size, bool normalize) const
{
    return mAttributeTypes[typeIndex(type)][normalize][size-1].d3dDeclType;
}

bool Dx9BackEnd::validateStream(GLenum type, std::size_t size, std::size_t stride, std::size_t offset) const
{
    // D3D9 requires the stream offset and stride to be a multiple of DWORD.
    return (stride % sizeof(DWORD) == 0 && offset % sizeof(DWORD) == 0);
}

IDirect3DVertexBuffer9 *Dx9BackEnd::getDxBuffer(TranslatedVertexBuffer *vb) const
{
    return vb ? static_cast<Dx9VertexBuffer*>(vb)->getBuffer() : NULL;
}

IDirect3DIndexBuffer9 *Dx9BackEnd::getDxBuffer(TranslatedIndexBuffer *ib) const
{
    return ib ? static_cast<Dx9IndexBuffer*>(ib)->getBuffer() : NULL;
}

GLenum Dx9BackEnd::setupIndicesPreDraw(const TranslatedIndexData &indexInfo)
{
    mDevice->SetIndices(getDxBuffer(indexInfo.buffer));
    return GL_NO_ERROR;
}

GLenum Dx9BackEnd::setupAttributesPreDraw(const TranslatedAttribute *attributes)
{
    HRESULT hr;

    D3DVERTEXELEMENT9 elements[MAX_VERTEX_ATTRIBS+1];

    D3DVERTEXELEMENT9 *nextElement = &elements[0];

    for (BYTE i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        if (attributes[i].enabled)
        {
            nextElement->Stream = i + 1;    // Stream 0 is skipped because D3D does not permit it to be an instanced stream.
            nextElement->Offset = 0;
            nextElement->Type = static_cast<BYTE>(mapAttributeType(attributes[i].type, attributes[i].size, attributes[i].normalized));
            nextElement->Method = D3DDECLMETHOD_DEFAULT;
            nextElement->Usage = D3DDECLUSAGE_TEXCOORD;
            nextElement->UsageIndex = attributes[i].semanticIndex;
            nextElement++;
        }
    }

    static const D3DVERTEXELEMENT9 end = D3DDECL_END();
    *nextElement = end;

    IDirect3DVertexDeclaration9* vertexDeclaration;
    hr = mDevice->CreateVertexDeclaration(elements, &vertexDeclaration);
    mDevice->SetVertexDeclaration(vertexDeclaration);
    vertexDeclaration->Release();

    mDevice->SetStreamSource(0, NULL, 0, 0);

    bool nonArrayAttributes = false;

    for (size_t i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        if (attributes[i].enabled)
        {
            if (attributes[i].nonArray) nonArrayAttributes = true;

            mDevice->SetStreamSource(i + 1, getDxBuffer(attributes[i].buffer), attributes[i].offset, attributes[i].stride);
            if (!mAppliedAttribEnabled[i])
            {
                mAppliedAttribEnabled[i] = true;
            }
        }
        else
        {
            if (mAppliedAttribEnabled[i])
            {
                mDevice->SetStreamSource(i + 1, 0, 0, 0);
                mAppliedAttribEnabled[i] = false;
            }
        }
    }

    if (mUseInstancingForStrideZero)
    {
        // When there are no stride zero attributes, we disable instancing so that DrawPrimitive can be used.

        if (nonArrayAttributes)
        {
            if (mStreamFrequency[0] != STREAM_FREQUENCY_INDEXED)
            {
                mStreamFrequency[0] = STREAM_FREQUENCY_INDEXED;
                mDevice->SetStreamSourceFreq(0, D3DSTREAMSOURCE_INDEXEDDATA | 1);
            }

            for (size_t i = 0; i < MAX_VERTEX_ATTRIBS; i++)
            {
                if (attributes[i].enabled)
                {
                    if (attributes[i].nonArray)
                    {
                        if (mStreamFrequency[i+1] != STREAM_FREQUENCY_INSTANCED)
                        {
                            mStreamFrequency[i+1] = STREAM_FREQUENCY_INSTANCED;
                            mDevice->SetStreamSourceFreq(i + 1, D3DSTREAMSOURCE_INSTANCEDATA | 1);
                        }
                    }
                    else
                    {
                        if (mStreamFrequency[i+1] != STREAM_FREQUENCY_INDEXED)
                        {
                            mStreamFrequency[i+1] = STREAM_FREQUENCY_INDEXED;
                            mDevice->SetStreamSourceFreq(i + 1, D3DSTREAMSOURCE_INDEXEDDATA | 1);
                        }
                    }
                }
            }
        }
        else
        {
            for (size_t i = 0; i < MAX_VERTEX_ATTRIBS + 1; i++)
            {
                if (mStreamFrequency[i] != STREAM_FREQUENCY_UNINSTANCED)
                {
                    mStreamFrequency[i] = STREAM_FREQUENCY_UNINSTANCED;

                    // This should not be needed, but otherwise there is a buggy driver that will leave instancing
                    // enabled for the first draw after it has been turned off.
                    mDevice->SetStreamSourceFreq(i, D3DSTREAMSOURCE_INDEXEDDATA | 1);

                    mDevice->SetStreamSourceFreq(i, 1);
                }
            }
        }
    }

    return GL_NO_ERROR;
}

Dx9BackEnd::Dx9VertexBuffer::Dx9VertexBuffer(IDirect3DDevice9 *device, std::size_t size)
    : TranslatedVertexBuffer(size)
{
    HRESULT hr = device->CreateVertexBuffer(size, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &mVertexBuffer, NULL);
    if (hr != S_OK)
    {
        ERR("Out of memory allocating a vertex buffer of size %lu.", size);
        throw std::bad_alloc();
    }
}

Dx9BackEnd::Dx9VertexBuffer::Dx9VertexBuffer(IDirect3DDevice9 *device, std::size_t size, DWORD usageFlags)
    : TranslatedVertexBuffer(size)
{
    HRESULT hr = device->CreateVertexBuffer(size, usageFlags, 0, D3DPOOL_DEFAULT, &mVertexBuffer, NULL);
    if (hr != S_OK)
    {
        ERR("Out of memory allocating a vertex buffer of size %lu.", size);
        throw std::bad_alloc();
    }
}


Dx9BackEnd::Dx9VertexBuffer::~Dx9VertexBuffer()
{
    mVertexBuffer->Release();
}

IDirect3DVertexBuffer9 *Dx9BackEnd::Dx9VertexBuffer::getBuffer() const
{
    return mVertexBuffer;
}

void *Dx9BackEnd::Dx9VertexBuffer::map()
{
    void *mapPtr;

    mVertexBuffer->Lock(0, 0, &mapPtr, 0);

    return mapPtr;
}

void Dx9BackEnd::Dx9VertexBuffer::unmap()
{
    mVertexBuffer->Unlock();
}

void Dx9BackEnd::Dx9VertexBuffer::recycle()
{
    void *dummy;
    mVertexBuffer->Lock(0, 1, &dummy, D3DLOCK_DISCARD);
    mVertexBuffer->Unlock();
}

void *Dx9BackEnd::Dx9VertexBuffer::streamingMap(std::size_t offset, std::size_t size)
{
    void *mapPtr;

    mVertexBuffer->Lock(offset, size, &mapPtr, D3DLOCK_NOOVERWRITE);

    return mapPtr;
}

// Normally VBs are created with D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, but some hardware & drivers won't render
// if any stride-zero streams are in D3DUSAGE_DYNAMIC VBs, so this provides a way to create such VBs with only D3DUSAGE_WRITEONLY set.
// D3DLOCK_DISCARD and D3DLOCK_NOOVERWRITE are only available on D3DUSAGE_DYNAMIC VBs, so we override methods to avoid using these flags.
Dx9BackEnd::Dx9VertexBufferZeroStrideWorkaround::Dx9VertexBufferZeroStrideWorkaround(IDirect3DDevice9 *device, std::size_t size)
    : Dx9VertexBuffer(device, size, D3DUSAGE_WRITEONLY)
{
}

void Dx9BackEnd::Dx9VertexBufferZeroStrideWorkaround::recycle()
{
}

void *Dx9BackEnd::Dx9VertexBufferZeroStrideWorkaround::streamingMap(std::size_t offset, std::size_t size)
{
    void *mapPtr;

    getBuffer()->Lock(offset, size, &mapPtr, 0);

    return mapPtr;
}

Dx9BackEnd::Dx9IndexBuffer::Dx9IndexBuffer(IDirect3DDevice9 *device, std::size_t size, GLenum type)
    : TranslatedIndexBuffer(size)
{
    ASSERT(type == GL_UNSIGNED_SHORT || type == GL_UNSIGNED_INT);

    D3DFORMAT format = (type == GL_UNSIGNED_SHORT) ? D3DFMT_INDEX16 : D3DFMT_INDEX32;

    HRESULT hr = device->CreateIndexBuffer(size, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, format, D3DPOOL_DEFAULT, &mIndexBuffer, NULL);
    if (hr != S_OK)
    {
        ERR("Out of memory allocating an index buffer of size %lu.", size);
        throw std::bad_alloc();
    }
}

Dx9BackEnd::Dx9IndexBuffer::~Dx9IndexBuffer()
{
    mIndexBuffer->Release();
}

IDirect3DIndexBuffer9*Dx9BackEnd::Dx9IndexBuffer::getBuffer() const
{
    return mIndexBuffer;
}

void *Dx9BackEnd::Dx9IndexBuffer::map()
{
    void *mapPtr;

    mIndexBuffer->Lock(0, 0, &mapPtr, 0);

    return mapPtr;
}

void Dx9BackEnd::Dx9IndexBuffer::unmap()
{
    mIndexBuffer->Unlock();
}

void Dx9BackEnd::Dx9IndexBuffer::recycle()
{
    void *dummy;
    mIndexBuffer->Lock(0, 1, &dummy, D3DLOCK_DISCARD);
    mIndexBuffer->Unlock();
}

void *Dx9BackEnd::Dx9IndexBuffer::streamingMap(std::size_t offset, std::size_t size)
{
    void *mapPtr;

    mIndexBuffer->Lock(offset, size, &mapPtr, D3DLOCK_NOOVERWRITE);

    return mapPtr;
}

}
