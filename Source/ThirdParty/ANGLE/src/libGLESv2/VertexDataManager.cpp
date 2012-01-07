//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VertexDataManager.h: Defines the VertexDataManager, a class that
// runs the Buffer translation process.

#include "libGLESv2/VertexDataManager.h"

#include "common/debug.h"

#include "libGLESv2/Buffer.h"
#include "libGLESv2/Program.h"
#include "libGLESv2/main.h"

#include "libGLESv2/vertexconversion.h"
#include "libGLESv2/IndexDataManager.h"

namespace
{
    enum { INITIAL_STREAM_BUFFER_SIZE = 1024*1024 };
    // This has to be at least 4k or else it fails on ATI cards.
    enum { CONSTANT_VERTEX_BUFFER_SIZE = 4096 };
}

namespace gl
{
unsigned int VertexBuffer::mCurrentSerial = 1;

int elementsInBuffer(const VertexAttribute &attribute, int size)
{
    int stride = attribute.stride();
    return (size - attribute.mOffset % stride + (stride - attribute.typeSize())) / stride;
}

VertexDataManager::VertexDataManager(Context *context, IDirect3DDevice9 *device) : mContext(context), mDevice(device)
{
    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        mDirtyCurrentValue[i] = true;
        mCurrentValueBuffer[i] = NULL;
        mCurrentValueOffsets[i] = 0;
    }

    const D3DCAPS9 &caps = context->getDeviceCaps();
    checkVertexCaps(caps.DeclTypes);

    mStreamingBuffer = new StreamingVertexBuffer(mDevice, INITIAL_STREAM_BUFFER_SIZE);

    if (!mStreamingBuffer)
    {
        ERR("Failed to allocate the streaming vertex buffer.");
    }
}

VertexDataManager::~VertexDataManager()
{
    delete mStreamingBuffer;

    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        delete mCurrentValueBuffer[i];
    }
}

std::size_t VertexDataManager::writeAttributeData(ArrayVertexBuffer *vertexBuffer, GLint start, GLsizei count, const VertexAttribute &attribute)
{
    Buffer *buffer = attribute.mBoundBuffer.get();

    int inputStride = attribute.stride();
    int elementSize = attribute.typeSize();
    const FormatConverter &converter = formatConverter(attribute);
    std::size_t streamOffset = 0;

    void *output = NULL;
    
    if (vertexBuffer)
    {
        output = vertexBuffer->map(attribute, spaceRequired(attribute, count), &streamOffset);
    }

    if (output == NULL)
    {
        ERR("Failed to map vertex buffer.");
        return -1;
    }

    const char *input = NULL;

    if (buffer)
    {
        int offset = attribute.mOffset;

        input = static_cast<const char*>(buffer->data()) + offset;
    }
    else
    {
        input = static_cast<const char*>(attribute.mPointer);
    }

    input += inputStride * start;

    if (converter.identity && inputStride == elementSize)
    {
        memcpy(output, input, count * inputStride);
    }
    else
    {
        converter.convertArray(input, inputStride, count, output);
    }

    vertexBuffer->unmap();

    return streamOffset;
}

GLenum VertexDataManager::prepareVertexData(GLint start, GLsizei count, TranslatedAttribute *translated)
{
    if (!mStreamingBuffer)
    {
        return GL_OUT_OF_MEMORY;
    }

    const VertexAttributeArray &attribs = mContext->getVertexAttributes();
    Program *program = mContext->getCurrentProgram();

    for (int attributeIndex = 0; attributeIndex < MAX_VERTEX_ATTRIBS; attributeIndex++)
    {
        translated[attributeIndex].active = (program->getSemanticIndex(attributeIndex) != -1);
    }

    // Determine the required storage size per used buffer, and invalidate static buffers that don't contain matching attributes
    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        if (translated[i].active && attribs[i].mArrayEnabled)
        {
            Buffer *buffer = attribs[i].mBoundBuffer.get();
            StaticVertexBuffer *staticBuffer = buffer ? buffer->getStaticVertexBuffer() : NULL;

            if (staticBuffer)
            {
                if (staticBuffer->size() == 0)
                {
                    int totalCount = elementsInBuffer(attribs[i], buffer->size());
                    staticBuffer->addRequiredSpace(spaceRequired(attribs[i], totalCount));
                }
                else if (staticBuffer->lookupAttribute(attribs[i]) == -1)
                {
                    // This static buffer doesn't have matching attributes, so fall back to using the streaming buffer
                    // Add the space of all previous attributes belonging to the invalidated static buffer to the streaming buffer
                    for (int previous = 0; previous < i; previous++)
                    {
                        if (translated[previous].active && attribs[previous].mArrayEnabled)
                        {
                            Buffer *previousBuffer = attribs[previous].mBoundBuffer.get();
                            StaticVertexBuffer *previousStaticBuffer = previousBuffer ? previousBuffer->getStaticVertexBuffer() : NULL;

                            if (staticBuffer == previousStaticBuffer)
                            {
                                mStreamingBuffer->addRequiredSpace(spaceRequired(attribs[previous], count));
                            }
                        }
                    }

                    mStreamingBuffer->addRequiredSpace(spaceRequired(attribs[i], count));

                    buffer->invalidateStaticData();
                }    
            }
            else
            {
                mStreamingBuffer->addRequiredSpace(spaceRequired(attribs[i], count));
            }
        }
    }

    // Reserve the required space per used buffer
    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        if (translated[i].active && attribs[i].mArrayEnabled)
        {
            Buffer *buffer = attribs[i].mBoundBuffer.get();
            ArrayVertexBuffer *staticBuffer = buffer ? buffer->getStaticVertexBuffer() : NULL;
            ArrayVertexBuffer *vertexBuffer = staticBuffer ? staticBuffer : mStreamingBuffer;

            if (vertexBuffer)
            {
                vertexBuffer->reserveRequiredSpace();
            }
        }
    }

    // Perform the vertex data translations
    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        if (translated[i].active)
        {
            if (attribs[i].mArrayEnabled)
            {
                Buffer *buffer = attribs[i].mBoundBuffer.get();

                if (!buffer && attribs[i].mPointer == NULL)
                {
                    // This is an application error that would normally result in a crash, but we catch it and return an error
                    ERR("An enabled vertex array has no buffer and no pointer.");
                    return GL_INVALID_OPERATION;
                }

                const FormatConverter &converter = formatConverter(attribs[i]);

                StaticVertexBuffer *staticBuffer = buffer ? buffer->getStaticVertexBuffer() : NULL;
                ArrayVertexBuffer *vertexBuffer = staticBuffer ? staticBuffer : static_cast<ArrayVertexBuffer*>(mStreamingBuffer);

                std::size_t streamOffset = -1;

                if (staticBuffer)
                {
                    streamOffset = staticBuffer->lookupAttribute(attribs[i]);

                    if (streamOffset == -1)
                    {
                        // Convert the entire buffer
                        int totalCount = elementsInBuffer(attribs[i], buffer->size());
                        int startIndex = attribs[i].mOffset / attribs[i].stride();

                        streamOffset = writeAttributeData(staticBuffer, -startIndex, totalCount, attribs[i]);
                    }

                    if (streamOffset != -1)
                    {
                        streamOffset += (start + attribs[i].mOffset / attribs[i].stride()) * converter.outputElementSize;
                    }
                }
                else
                {
                    streamOffset = writeAttributeData(mStreamingBuffer, start, count, attribs[i]);
                }

                if (streamOffset == -1)
                {
                    return GL_OUT_OF_MEMORY;
                }

                translated[i].vertexBuffer = vertexBuffer->getBuffer();
                translated[i].serial = vertexBuffer->getSerial();
                translated[i].type = converter.d3dDeclType;
                translated[i].stride = converter.outputElementSize;
                translated[i].offset = streamOffset;
            }
            else
            {
                if (!mCurrentValueBuffer[i])
                {
                    mCurrentValueBuffer[i] = new StreamingVertexBuffer(mDevice, CONSTANT_VERTEX_BUFFER_SIZE);
                }

                StreamingVertexBuffer *buffer = mCurrentValueBuffer[i];

                if (mDirtyCurrentValue[i])
                {
                    const int requiredSpace = 4 * sizeof(float);
                    buffer->addRequiredSpace(requiredSpace);
                    buffer->reserveRequiredSpace();
                    float *data = static_cast<float*>(buffer->map(VertexAttribute(), requiredSpace, &mCurrentValueOffsets[i]));
                    if (data)
                    {
                        data[0] = attribs[i].mCurrentValue[0];
                        data[1] = attribs[i].mCurrentValue[1];
                        data[2] = attribs[i].mCurrentValue[2];
                        data[3] = attribs[i].mCurrentValue[3];
                        buffer->unmap();
                        mDirtyCurrentValue[i] = false;
                    }
                }

                translated[i].vertexBuffer = mCurrentValueBuffer[i]->getBuffer();
                translated[i].serial = mCurrentValueBuffer[i]->getSerial();

                translated[i].type = D3DDECLTYPE_FLOAT4;
                translated[i].stride = 0;
                translated[i].offset = mCurrentValueOffsets[i];
            }
        }
    }

    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        if (translated[i].active && attribs[i].mArrayEnabled)
        {
            Buffer *buffer = attribs[i].mBoundBuffer.get();

            if (buffer)
            {
                buffer->promoteStaticUsage(count * attribs[i].typeSize());
            }
        }
    }

    return GL_NO_ERROR;
}

std::size_t VertexDataManager::spaceRequired(const VertexAttribute &attrib, std::size_t count) const
{
    return formatConverter(attrib).outputElementSize * count;
}

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

// Initialise a TranslationInfo
#define TRANSLATION(type, norm, size, preferred)                                    \
    {                                                                               \
        Converter<type, norm, size, preferred>::identity,                           \
        Converter<type, norm, size, preferred>::finalSize,                          \
        Converter<type, norm, size, preferred>::convertArray,                       \
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

const VertexDataManager::TranslationDescription VertexDataManager::mPossibleTranslations[NUM_GL_VERTEX_ATTRIB_TYPES][2][4] = // [GL types as enumerated by typeIndex()][normalized][size-1]
{
    TRANSLATIONS_FOR_TYPE(GL_BYTE),
    TRANSLATIONS_FOR_TYPE(GL_UNSIGNED_BYTE),
    TRANSLATIONS_FOR_TYPE(GL_SHORT),
    TRANSLATIONS_FOR_TYPE(GL_UNSIGNED_SHORT),
    TRANSLATIONS_FOR_TYPE(GL_FIXED),
    TRANSLATIONS_FOR_TYPE(GL_FLOAT)
};

void VertexDataManager::checkVertexCaps(DWORD declTypes)
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

// This is used to index mAttributeTypes and mPossibleTranslations.
unsigned int VertexDataManager::typeIndex(GLenum type) const
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

VertexBuffer::VertexBuffer(IDirect3DDevice9 *device, std::size_t size, DWORD usageFlags) : mDevice(device), mVertexBuffer(NULL)
{
    if (size > 0)
    {
        D3DPOOL pool = getDisplay()->getBufferPool(usageFlags);
        HRESULT result = device->CreateVertexBuffer(size, usageFlags, 0, pool, &mVertexBuffer, NULL);
        mSerial = issueSerial();
        
        if (FAILED(result))
        {
            ERR("Out of memory allocating a vertex buffer of size %lu.", size);
        }
    }
}

VertexBuffer::~VertexBuffer()
{
    if (mVertexBuffer)
    {
        mVertexBuffer->Release();
    }
}

void VertexBuffer::unmap()
{
    if (mVertexBuffer)
    {
        mVertexBuffer->Unlock();
    }
}

IDirect3DVertexBuffer9 *VertexBuffer::getBuffer() const
{
    return mVertexBuffer;
}

unsigned int VertexBuffer::getSerial() const
{
    return mSerial;
}

unsigned int VertexBuffer::issueSerial()
{
    return mCurrentSerial++;
}

ArrayVertexBuffer::ArrayVertexBuffer(IDirect3DDevice9 *device, std::size_t size, DWORD usageFlags) : VertexBuffer(device, size, usageFlags)
{
    mBufferSize = size;
    mWritePosition = 0;
    mRequiredSpace = 0;
}

ArrayVertexBuffer::~ArrayVertexBuffer()
{
}

void ArrayVertexBuffer::addRequiredSpace(UINT requiredSpace)
{
    mRequiredSpace += requiredSpace;
}

StreamingVertexBuffer::StreamingVertexBuffer(IDirect3DDevice9 *device, std::size_t initialSize) : ArrayVertexBuffer(device, initialSize, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY)
{
}

StreamingVertexBuffer::~StreamingVertexBuffer()
{
}

void *StreamingVertexBuffer::map(const VertexAttribute &attribute, std::size_t requiredSpace, std::size_t *offset)
{
    void *mapPtr = NULL;

    if (mVertexBuffer)
    {
        HRESULT result = mVertexBuffer->Lock(mWritePosition, requiredSpace, &mapPtr, D3DLOCK_NOOVERWRITE);
        
        if (FAILED(result))
        {
            ERR("Lock failed with error 0x%08x", result);
            return NULL;
        }

        *offset = mWritePosition;
        mWritePosition += requiredSpace;
    }

    return mapPtr;
}

void StreamingVertexBuffer::reserveRequiredSpace()
{
    if (mRequiredSpace > mBufferSize)
    {
        if (mVertexBuffer)
        {
            mVertexBuffer->Release();
            mVertexBuffer = NULL;
        }

        mBufferSize = std::max(mRequiredSpace, 3 * mBufferSize / 2);   // 1.5 x mBufferSize is arbitrary and should be checked to see we don't have too many reallocations.

        D3DPOOL pool = getDisplay()->getBufferPool(D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY);
        HRESULT result = mDevice->CreateVertexBuffer(mBufferSize, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, pool, &mVertexBuffer, NULL);
        mSerial = issueSerial();
    
        if (FAILED(result))
        {
            ERR("Out of memory allocating a vertex buffer of size %lu.", mBufferSize);
        }

        mWritePosition = 0;
    }
    else if (mWritePosition + mRequiredSpace > mBufferSize)   // Recycle
    {
        if (mVertexBuffer)
        {
            void *dummy;
            mVertexBuffer->Lock(0, 1, &dummy, D3DLOCK_DISCARD);
            mVertexBuffer->Unlock();
        }

        mWritePosition = 0;
    }

    mRequiredSpace = 0;
}

StaticVertexBuffer::StaticVertexBuffer(IDirect3DDevice9 *device) : ArrayVertexBuffer(device, 0, D3DUSAGE_WRITEONLY)
{
}

StaticVertexBuffer::~StaticVertexBuffer()
{
}

void *StaticVertexBuffer::map(const VertexAttribute &attribute, std::size_t requiredSpace, std::size_t *streamOffset)
{
    void *mapPtr = NULL;

    if (mVertexBuffer)
    {
        HRESULT result = mVertexBuffer->Lock(mWritePosition, requiredSpace, &mapPtr, 0);
        
        if (FAILED(result))
        {
            ERR("Lock failed with error 0x%08x", result);
            return NULL;
        }

        int attributeOffset = attribute.mOffset % attribute.stride();
        VertexElement element = {attribute.mType, attribute.mSize, attribute.stride(), attribute.mNormalized, attributeOffset, mWritePosition};
        mCache.push_back(element);

        *streamOffset = mWritePosition;
        mWritePosition += requiredSpace;
    }

    return mapPtr;
}

void StaticVertexBuffer::reserveRequiredSpace()
{
    if (!mVertexBuffer && mBufferSize == 0)
    {
        D3DPOOL pool = getDisplay()->getBufferPool(D3DUSAGE_WRITEONLY);
        HRESULT result = mDevice->CreateVertexBuffer(mRequiredSpace, D3DUSAGE_WRITEONLY, 0, pool, &mVertexBuffer, NULL);
        mSerial = issueSerial();

        if (FAILED(result))
        {
            ERR("Out of memory allocating a vertex buffer of size %lu.", mRequiredSpace);
        }

        mBufferSize = mRequiredSpace;
    }
    else if (mVertexBuffer && mBufferSize >= mRequiredSpace)
    {
        // Already allocated
    }
    else UNREACHABLE();   // Static vertex buffers can't be resized

    mRequiredSpace = 0;
}

std::size_t StaticVertexBuffer::lookupAttribute(const VertexAttribute &attribute)
{
    for (unsigned int element = 0; element < mCache.size(); element++)
    {
        if (mCache[element].type == attribute.mType &&
            mCache[element].size == attribute.mSize &&
            mCache[element].stride == attribute.stride() &&
            mCache[element].normalized == attribute.mNormalized)
        {
            if (mCache[element].attributeOffset == attribute.mOffset % attribute.stride())
            {
                return mCache[element].streamOffset;
            }
        }
    }

    return -1;
}

const VertexDataManager::FormatConverter &VertexDataManager::formatConverter(const VertexAttribute &attribute) const
{
    return mAttributeTypes[typeIndex(attribute.mType)][attribute.mNormalized][attribute.mSize - 1];
}
}
