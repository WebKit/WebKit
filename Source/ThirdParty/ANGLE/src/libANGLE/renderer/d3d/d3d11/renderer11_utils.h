//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// renderer11_utils.h: Conversion functions and other utility routines
// specific to the D3D11 renderer.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_RENDERER11_UTILS_H_
#define LIBANGLE_RENDERER_D3D_D3D11_RENDERER11_UTILS_H_

#include <array>
#include <functional>
#include <vector>

#include "libANGLE/angletypes.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Error.h"
#include "libANGLE/renderer/d3d/d3d11/texture_format_table.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"

namespace gl
{
class FramebufferAttachment;
}

namespace rx
{
class Renderer11;
class RenderTarget11;
struct WorkaroundsD3D;
struct Renderer11DeviceCaps;

using RenderTargetArray = std::array<RenderTarget11 *, gl::IMPLEMENTATION_MAX_DRAW_BUFFERS>;
using RTVArray          = std::array<ID3D11RenderTargetView *, gl::IMPLEMENTATION_MAX_DRAW_BUFFERS>;

namespace gl_d3d11
{

D3D11_BLEND ConvertBlendFunc(GLenum glBlend, bool isAlpha);
D3D11_BLEND_OP ConvertBlendOp(GLenum glBlendOp);
UINT8 ConvertColorMask(bool maskRed, bool maskGreen, bool maskBlue, bool maskAlpha);

D3D11_CULL_MODE ConvertCullMode(bool cullEnabled, GLenum cullMode);

D3D11_COMPARISON_FUNC ConvertComparison(GLenum comparison);
D3D11_DEPTH_WRITE_MASK ConvertDepthMask(bool depthWriteEnabled);
UINT8 ConvertStencilMask(GLuint stencilmask);
D3D11_STENCIL_OP ConvertStencilOp(GLenum stencilOp);

D3D11_FILTER ConvertFilter(GLenum minFilter, GLenum magFilter, float maxAnisotropy, GLenum comparisonMode);
D3D11_TEXTURE_ADDRESS_MODE ConvertTextureWrap(GLenum wrap);

D3D11_QUERY ConvertQueryType(GLenum queryType);

}  // namespace gl_d3d11

namespace d3d11_gl
{

unsigned int GetReservedVertexUniformVectors(D3D_FEATURE_LEVEL featureLevel);

unsigned int GetReservedFragmentUniformVectors(D3D_FEATURE_LEVEL featureLevel);

GLint GetMaximumClientVersion(D3D_FEATURE_LEVEL featureLevel);
void GenerateCaps(ID3D11Device *device, ID3D11DeviceContext *deviceContext, const Renderer11DeviceCaps &renderer11DeviceCaps, gl::Caps *caps,
                  gl::TextureCapsMap *textureCapsMap, gl::Extensions *extensions, gl::Limitations *limitations);

}  // namespace d3d11_gl

namespace d3d11
{

enum ANGLED3D11DeviceType
{
    ANGLE_D3D11_DEVICE_TYPE_UNKNOWN,
    ANGLE_D3D11_DEVICE_TYPE_HARDWARE,
    ANGLE_D3D11_DEVICE_TYPE_SOFTWARE_REF_OR_NULL,
    ANGLE_D3D11_DEVICE_TYPE_WARP,
};

ANGLED3D11DeviceType GetDeviceType(ID3D11Device *device);

void MakeValidSize(bool isImage, DXGI_FORMAT format, GLsizei *requestWidth, GLsizei *requestHeight, int *levelOffset);

void GenerateInitialTextureData(GLint internalFormat,
                                const Renderer11DeviceCaps &renderer11DeviceCaps,
                                GLuint width,
                                GLuint height,
                                GLuint depth,
                                GLuint mipLevels,
                                std::vector<D3D11_SUBRESOURCE_DATA> *outSubresourceData,
                                std::vector<std::vector<BYTE>> *outData);

UINT GetPrimitiveRestartIndex();

struct PositionTexCoordVertex
{
    float x, y;
    float u, v;
};
void SetPositionTexCoordVertex(PositionTexCoordVertex* vertex, float x, float y, float u, float v);

struct PositionLayerTexCoord3DVertex
{
    float x, y;
    unsigned int l;
    float u, v, s;
};
void SetPositionLayerTexCoord3DVertex(PositionLayerTexCoord3DVertex* vertex, float x, float y,
                                      unsigned int layer, float u, float v, float s);

template <typename T>
struct PositionDepthColorVertex
{
    float x, y, z;
    T r, g, b, a;
};

template <typename T>
void SetPositionDepthColorVertex(PositionDepthColorVertex<T>* vertex, float x, float y, float z,
                                 const gl::Color<T> &color)
{
    vertex->x = x;
    vertex->y = y;
    vertex->z = z;
    vertex->r = color.red;
    vertex->g = color.green;
    vertex->b = color.blue;
    vertex->a = color.alpha;
}

HRESULT SetDebugName(ID3D11DeviceChild *resource, const char *name);

template <typename outType>
outType* DynamicCastComObject(IUnknown* object)
{
    outType *outObject = NULL;
    HRESULT result = object->QueryInterface(__uuidof(outType), reinterpret_cast<void**>(&outObject));
    if (SUCCEEDED(result))
    {
        return outObject;
    }
    else
    {
        SafeRelease(outObject);
        return NULL;
    }
}

inline bool isDeviceLostError(HRESULT errorCode)
{
    switch (errorCode)
    {
      case DXGI_ERROR_DEVICE_HUNG:
      case DXGI_ERROR_DEVICE_REMOVED:
      case DXGI_ERROR_DEVICE_RESET:
      case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
      case DXGI_ERROR_NOT_CURRENTLY_AVAILABLE:
        return true;
      default:
        return false;
    }
}

inline ID3D11VertexShader *CompileVS(ID3D11Device *device, const BYTE *byteCode, size_t N, const char *name)
{
    ID3D11VertexShader *vs = nullptr;
    HRESULT result = device->CreateVertexShader(byteCode, N, nullptr, &vs);
    ASSERT(SUCCEEDED(result));
    if (SUCCEEDED(result))
    {
        SetDebugName(vs, name);
        return vs;
    }
    return nullptr;
}

template <unsigned int N>
ID3D11VertexShader *CompileVS(ID3D11Device *device, const BYTE (&byteCode)[N], const char *name)
{
    return CompileVS(device, byteCode, N, name);
}

inline ID3D11GeometryShader *CompileGS(ID3D11Device *device, const BYTE *byteCode, size_t N, const char *name)
{
    ID3D11GeometryShader *gs = nullptr;
    HRESULT result = device->CreateGeometryShader(byteCode, N, nullptr, &gs);
    ASSERT(SUCCEEDED(result));
    if (SUCCEEDED(result))
    {
        SetDebugName(gs, name);
        return gs;
    }
    return nullptr;
}

template <unsigned int N>
ID3D11GeometryShader *CompileGS(ID3D11Device *device, const BYTE (&byteCode)[N], const char *name)
{
    return CompileGS(device, byteCode, N, name);
}

inline ID3D11PixelShader *CompilePS(ID3D11Device *device, const BYTE *byteCode, size_t N, const char *name)
{
    ID3D11PixelShader *ps = nullptr;
    HRESULT result = device->CreatePixelShader(byteCode, N, nullptr, &ps);
    ASSERT(SUCCEEDED(result));
    if (SUCCEEDED(result))
    {
        SetDebugName(ps, name);
        return ps;
    }
    return nullptr;
}

template <unsigned int N>
ID3D11PixelShader *CompilePS(ID3D11Device *device, const BYTE (&byteCode)[N], const char *name)
{
    return CompilePS(device, byteCode, N, name);
}

template <typename ResourceType>
class LazyResource : public angle::NonCopyable
{
  public:
    LazyResource() : mResource(nullptr), mAssociatedDevice(nullptr) {}
    virtual ~LazyResource() { release(); }

    virtual ResourceType *resolve(ID3D11Device *device) = 0;
    void release() { SafeRelease(mResource); }

  protected:
    void checkAssociatedDevice(ID3D11Device *device);

    ResourceType *mResource;
    ID3D11Device *mAssociatedDevice;
};

template <typename ResourceType>
void LazyResource<ResourceType>::checkAssociatedDevice(ID3D11Device *device)
{
    ASSERT(mAssociatedDevice == nullptr || device == mAssociatedDevice);
    mAssociatedDevice = device;
}

template <typename D3D11ShaderType>
class LazyShader final : public LazyResource<D3D11ShaderType>
{
  public:
    // All parameters must be constexpr. Not supported in VS2013.
    LazyShader(const BYTE *byteCode,
               size_t byteCodeSize,
               const char *name)
        : mByteCode(byteCode),
          mByteCodeSize(byteCodeSize),
          mName(name)
    {
    }

    D3D11ShaderType *resolve(ID3D11Device *device) override;

  private:
    const BYTE *mByteCode;
    size_t mByteCodeSize;
    const char *mName;
};

template <>
inline ID3D11VertexShader *LazyShader<ID3D11VertexShader>::resolve(ID3D11Device *device)
{
    checkAssociatedDevice(device);
    if (mResource == nullptr)
    {
        mResource = CompileVS(device, mByteCode, mByteCodeSize, mName);
    }
    return mResource;
}

template <>
inline ID3D11GeometryShader *LazyShader<ID3D11GeometryShader>::resolve(ID3D11Device *device)
{
    checkAssociatedDevice(device);
    if (mResource == nullptr)
    {
        mResource = CompileGS(device, mByteCode, mByteCodeSize, mName);
    }
    return mResource;
}

template <>
inline ID3D11PixelShader *LazyShader<ID3D11PixelShader>::resolve(ID3D11Device *device)
{
    checkAssociatedDevice(device);
    if (mResource == nullptr)
    {
        mResource = CompilePS(device, mByteCode, mByteCodeSize, mName);
    }
    return mResource;
}

class LazyInputLayout final : public LazyResource<ID3D11InputLayout>
{
  public:
    LazyInputLayout(const D3D11_INPUT_ELEMENT_DESC *inputDesc,
                    size_t inputDescLen,
                    const BYTE *byteCode,
                    size_t byteCodeLen,
                    const char *debugName);

    ID3D11InputLayout *resolve(ID3D11Device *device) override;

  private:
    std::vector<D3D11_INPUT_ELEMENT_DESC> mInputDesc;
    size_t mByteCodeLen;
    const BYTE *mByteCode;
    const char *mDebugName;
};

class LazyBlendState final : public LazyResource<ID3D11BlendState>
{
  public:
    LazyBlendState(const D3D11_BLEND_DESC &desc, const char *debugName);

    ID3D11BlendState *resolve(ID3D11Device *device) override;

  private:
    D3D11_BLEND_DESC mDesc;
    const char *mDebugName;
};

// Copy data to small D3D11 buffers, such as for small constant buffers, which use one struct to
// represent an entire buffer.
template <class T>
void SetBufferData(ID3D11DeviceContext *context, ID3D11Buffer *constantBuffer, const T &value)
{
    D3D11_MAPPED_SUBRESOURCE mappedResource = {};
    HRESULT result = context->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    ASSERT(SUCCEEDED(result));
    if (SUCCEEDED(result))
    {
        memcpy(mappedResource.pData, &value, sizeof(T));
        context->Unmap(constantBuffer, 0);
    }
}

WorkaroundsD3D GenerateWorkarounds(D3D_FEATURE_LEVEL featureLevel);

enum ReservedConstantBufferSlot
{
    RESERVED_CONSTANT_BUFFER_SLOT_DEFAULT_UNIFORM_BLOCK = 0,
    RESERVED_CONSTANT_BUFFER_SLOT_DRIVER                = 1,

    RESERVED_CONSTANT_BUFFER_SLOT_COUNT = 2
};

void InitConstantBufferDesc(D3D11_BUFFER_DESC *constantBufferDescription, size_t byteWidth);
}  // namespace d3d11

// A helper class which wraps a 2D or 3D texture.
class TextureHelper11 : angle::NonCopyable
{
  public:
    TextureHelper11();
    TextureHelper11(TextureHelper11 &&toCopy);
    ~TextureHelper11();
    TextureHelper11 &operator=(TextureHelper11 &&texture);

    static TextureHelper11 MakeAndReference(ID3D11Resource *genericResource,
                                            d3d11::ANGLEFormat angleFormat);
    static TextureHelper11 MakeAndPossess2D(ID3D11Texture2D *texToOwn,
                                            d3d11::ANGLEFormat angleFormat);
    static TextureHelper11 MakeAndPossess3D(ID3D11Texture3D *texToOwn,
                                            d3d11::ANGLEFormat angleFormat);

    GLenum getTextureType() const { return mTextureType; }
    gl::Extents getExtents() const { return mExtents; }
    DXGI_FORMAT getFormat() const { return mFormat; }
    d3d11::ANGLEFormat getANGLEFormat() const { return mANGLEFormat; }
    int getSampleCount() const { return mSampleCount; }
    ID3D11Texture2D *getTexture2D() const { return mTexture2D; }
    ID3D11Texture3D *getTexture3D() const { return mTexture3D; }
    ID3D11Resource *getResource() const;

  private:
    void reset();
    void initDesc();

    GLenum mTextureType;
    gl::Extents mExtents;
    DXGI_FORMAT mFormat;
    d3d11::ANGLEFormat mANGLEFormat;
    int mSampleCount;
    ID3D11Texture2D *mTexture2D;
    ID3D11Texture3D *mTexture3D;
};

gl::ErrorOrResult<TextureHelper11> CreateStagingTexture(GLenum textureType,
                                                        DXGI_FORMAT dxgiFormat,
                                                        d3d11::ANGLEFormat angleFormat,
                                                        const gl::Extents &size,
                                                        ID3D11Device *device);

bool UsePresentPathFast(const Renderer11 *renderer, const gl::FramebufferAttachment *colorbuffer);

using NotificationCallback = std::function<void()>;

class NotificationSet final : angle::NonCopyable
{
  public:
    NotificationSet();
    ~NotificationSet();

    void add(const NotificationCallback *callback);
    void remove(const NotificationCallback *callback);
    void signal() const;
    void clear();

  private:
    std::set<const NotificationCallback *> mCallbacks;
};

}  // namespace rx

#endif // LIBANGLE_RENDERER_D3D_D3D11_RENDERER11_UTILS_H_
