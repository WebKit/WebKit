//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Texture.cpp: Implements the gl::Texture class and its derived classes
// Texture2D and TextureCubeMap. Implements GL texture objects and related
// functionality. [OpenGL ES 2.0.24] section 3.7 page 63.

#include "libGLESv2/Texture.h"

#include <algorithm>

#include "common/debug.h"

#include "libGLESv2/main.h"
#include "libGLESv2/mathutil.h"
#include "libGLESv2/utilities.h"
#include "libGLESv2/Blit.h"

namespace gl
{

Texture::Image::Image()
  : width(0), height(0), dirty(false), surface(NULL)
{
}

Texture::Image::~Image()
{
  if (surface) surface->Release();
}

Texture::Texture(Context *context) : mContext(context)
{
    mMinFilter = GL_NEAREST_MIPMAP_LINEAR;
    mMagFilter = GL_LINEAR;
    mWrapS = GL_REPEAT;
    mWrapT = GL_REPEAT;

    mDirtyMetaData = true;
    mDirty = true;
    mIsRenderable = false;
    mBaseTexture = NULL;
}

Texture::~Texture()
{
}

Blit *Texture::getBlitter()
{
    return mContext->getBlitter();
}

// Returns true on successful filter state update (valid enum parameter)
bool Texture::setMinFilter(GLenum filter)
{
    switch (filter)
    {
      case GL_NEAREST:
      case GL_LINEAR:
      case GL_NEAREST_MIPMAP_NEAREST:
      case GL_LINEAR_MIPMAP_NEAREST:
      case GL_NEAREST_MIPMAP_LINEAR:
      case GL_LINEAR_MIPMAP_LINEAR:
        {
            if (mMinFilter != filter)
            {
                mMinFilter = filter;
                mDirty = true;
            }
            return true;
        }
      default:
        return false;
    }
}

// Returns true on successful filter state update (valid enum parameter)
bool Texture::setMagFilter(GLenum filter)
{
    switch (filter)
    {
      case GL_NEAREST:
      case GL_LINEAR:
        {
            if (mMagFilter != filter)
            {
                mMagFilter = filter;
                mDirty = true;
            }
            return true;
        }
      default:
        return false;
    }
}

// Returns true on successful wrap state update (valid enum parameter)
bool Texture::setWrapS(GLenum wrap)
{
    switch (wrap)
    {
      case GL_REPEAT:
      case GL_CLAMP_TO_EDGE:
      case GL_MIRRORED_REPEAT:
        {
            if (mWrapS != wrap)
            {
                mWrapS = wrap;
                mDirty = true;
            }
            return true;
        }
      default:
        return false;
    }
}

// Returns true on successful wrap state update (valid enum parameter)
bool Texture::setWrapT(GLenum wrap)
{
    switch (wrap)
    {
      case GL_REPEAT:
      case GL_CLAMP_TO_EDGE:
      case GL_MIRRORED_REPEAT:
        {
            if (mWrapT != wrap)
            {
                mWrapT = wrap;
                mDirty = true;
            }
            return true;
        }
      default:
        return false;
    }
}

GLenum Texture::getMinFilter() const
{
    return mMinFilter;
}

GLenum Texture::getMagFilter() const
{
    return mMagFilter;
}

GLenum Texture::getWrapS() const
{
    return mWrapS;
}

GLenum Texture::getWrapT() const
{
    return mWrapT;
}

GLuint Texture::getWidth() const
{
    return mWidth;
}

GLuint Texture::getHeight() const
{
    return mHeight;
}

// Selects an internal Direct3D 9 format for storing an Image
D3DFORMAT Texture::selectFormat(GLenum format)
{
    return D3DFMT_A8R8G8B8;
}

int Texture::imagePitch(const Image &img) const
{
    return img.width * 4;
}

// Store the pixel rectangle designated by xoffset,yoffset,width,height with pixels stored as format/type at input
// into the BGRA8 pixel rectangle at output with outputPitch bytes in between each line.
void Texture::loadImageData(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
                            GLint unpackAlignment, const void *input, size_t outputPitch, void *output) const
{
    GLsizei inputPitch = ComputePitch(width, format, type, unpackAlignment);

    for (int y = 0; y < height; y++)
    {
        const unsigned char *source = static_cast<const unsigned char*>(input) + y * inputPitch;
        const unsigned short *source16 = reinterpret_cast<const unsigned short*>(source);
        unsigned char *dest = static_cast<unsigned char*>(output) + (y + yoffset) * outputPitch + xoffset * 4;

        for (int x = 0; x < width; x++)
        {
            unsigned char r;
            unsigned char g;
            unsigned char b;
            unsigned char a;

            switch (format)
            {
              case GL_ALPHA:
                a = source[x];
                r = 0;
                g = 0;
                b = 0;
                break;

              case GL_LUMINANCE:
                r = source[x];
                g = source[x];
                b = source[x];
                a = 0xFF;
                break;

              case GL_LUMINANCE_ALPHA:
                r = source[2*x+0];
                g = source[2*x+0];
                b = source[2*x+0];
                a = source[2*x+1];
                break;

              case GL_RGB:
                switch (type)
                {
                  case GL_UNSIGNED_BYTE:
                    r = source[x * 3 + 0];
                    g = source[x * 3 + 1];
                    b = source[x * 3 + 2];
                    a = 0xFF;
                    break;

                  case GL_UNSIGNED_SHORT_5_6_5:
                    {
                        unsigned short rgba = source16[x];

                        a = 0xFF;
                        b = ((rgba & 0x001F) << 3) | ((rgba & 0x001F) >> 2);
                        g = ((rgba & 0x07E0) >> 3) | ((rgba & 0x07E0) >> 9);
                        r = ((rgba & 0xF800) >> 8) | ((rgba & 0xF800) >> 13);
                    }
                    break;

                  default: UNREACHABLE();
                }
                break;

              case GL_RGBA:
                switch (type)
                {
                  case GL_UNSIGNED_BYTE:
                    r = source[x * 4 + 0];
                    g = source[x * 4 + 1];
                    b = source[x * 4 + 2];
                    a = source[x * 4 + 3];
                    break;

                  case GL_UNSIGNED_SHORT_4_4_4_4:
                    {
                        unsigned short rgba = source16[x];

                        a = ((rgba & 0x000F) << 4) | ((rgba & 0x000F) >> 0);
                        b = ((rgba & 0x00F0) << 0) | ((rgba & 0x00F0) >> 4);
                        g = ((rgba & 0x0F00) >> 4) | ((rgba & 0x0F00) >> 8);
                        r = ((rgba & 0xF000) >> 8) | ((rgba & 0xF000) >> 12);
                    }
                    break;

                  case GL_UNSIGNED_SHORT_5_5_5_1:
                    {
                        unsigned short rgba = source16[x];

                        a = (rgba & 0x0001) ? 0xFF : 0;
                        b = ((rgba & 0x003E) << 2) | ((rgba & 0x003E) >> 3);
                        g = ((rgba & 0x07C0) >> 3) | ((rgba & 0x07C0) >> 8);
                        r = ((rgba & 0xF800) >> 8) | ((rgba & 0xF800) >> 13);
                    }
                    break;

                  default: UNREACHABLE();
                }
                break;
              default: UNREACHABLE();
            }

            dest[4 * x + 0] = b;
            dest[4 * x + 1] = g;
            dest[4 * x + 2] = r;
            dest[4 * x + 3] = a;
        }
    }
}

void Texture::setImage(GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels, Image *img)
{
    IDirect3DSurface9 *newSurface = NULL;

    if (width != 0 && height != 0)
    {
        HRESULT result = getDevice()->CreateOffscreenPlainSurface(width, height, selectFormat(format), D3DPOOL_SYSTEMMEM, &newSurface, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            return error(GL_OUT_OF_MEMORY);
        }
    }

    if (img->surface) img->surface->Release();
    img->surface = newSurface;

    img->width = width;
    img->height = height;
    img->format = format;

    if (pixels != NULL && newSurface != NULL)
    {
        D3DLOCKED_RECT locked;
        HRESULT result = newSurface->LockRect(&locked, NULL, 0);

        ASSERT(SUCCEEDED(result));

        if (SUCCEEDED(result))
        {
            loadImageData(0, 0, width, height, format, type, unpackAlignment, pixels, locked.Pitch, locked.pBits);
            newSurface->UnlockRect();
        }

        img->dirty = true;
    }

    mDirtyMetaData = true;
}

void Texture::subImage(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels, Image *img)
{
    if (width + xoffset > img->width || height + yoffset > img->height) return error(GL_INVALID_VALUE);

    D3DLOCKED_RECT locked;
    HRESULT result = img->surface->LockRect(&locked, NULL, 0);

    ASSERT(SUCCEEDED(result));

    if (SUCCEEDED(result))
    {
        loadImageData(xoffset, yoffset, width, height, format, type, unpackAlignment, pixels, locked.Pitch, locked.pBits);
        img->surface->UnlockRect();
    }

    img->dirty = true;
}

IDirect3DBaseTexture9 *Texture::getTexture()
{
    if (!isComplete())
    {
        return NULL;
    }

    if (mDirtyMetaData)
    {
        mBaseTexture = createTexture();
        mIsRenderable = false;
    }

    if (mDirtyMetaData || dirtyImageData())
    {
        updateTexture();
    }

    mDirtyMetaData = false;
    ASSERT(!dirtyImageData());

    return mBaseTexture;
}

bool Texture::isDirty() const
{
    return (mDirty || mDirtyMetaData || dirtyImageData());
}

// Returns the top-level texture surface as a render target
void Texture::needRenderTarget()
{
    if (!mIsRenderable)
    {
        mBaseTexture = convertToRenderTarget();
        mIsRenderable = true;
    }

    if (dirtyImageData())
    {
        updateTexture();
    }

    mDirtyMetaData = false;
}

void Texture::dropTexture()
{
    if (mBaseTexture)
    {
        mBaseTexture = NULL;
    }

    mIsRenderable = false;
}

void Texture::pushTexture(IDirect3DBaseTexture9 *newTexture, bool renderable)
{
    mBaseTexture = newTexture;
    mDirtyMetaData = false;
    mIsRenderable = renderable;
    mDirty = true;
}


GLint Texture::creationLevels(GLsizei width, GLsizei height, GLint maxlevel) const
{
    if (isPow2(width) && isPow2(height))
    {
        return maxlevel;
    }
    else
    {
        // OpenGL ES 2.0 without GL_OES_texture_npot does not permit NPOT mipmaps.
        return 1;
    }
}

GLint Texture::creationLevels(GLsizei size, GLint maxlevel) const
{
    return creationLevels(size, size, maxlevel);
}

int Texture::levelCount() const
{
    return mBaseTexture ? mBaseTexture->GetLevelCount() : 0;
}

Texture2D::Texture2D(Context *context) : Texture(context)
{
    mTexture = NULL;
    mColorbufferProxy = NULL;
}

Texture2D::~Texture2D()
{
    delete mColorbufferProxy;

    if (mTexture)
    {
        mTexture->Release();
        mTexture = NULL;
    }
}

GLenum Texture2D::getTarget() const
{
    return GL_TEXTURE_2D;
}

// While OpenGL doesn't check texture consistency until draw-time, D3D9 requires a complete texture
// for render-to-texture (such as CopyTexImage). We have no way of keeping individual inconsistent levels.
// Call this when a particular level of the texture must be defined with a specific format, width and height.
//
// Returns true if the existing texture was unsuitable had to be destroyed. If so, it will also set
// a new height and width for the texture by working backwards from the given width and height.
bool Texture2D::redefineTexture(GLint level, GLenum internalFormat, GLsizei width, GLsizei height)
{
    bool widthOkay = (mWidth >> level == width);
    bool heightOkay = (mHeight >> level == height);

    bool sizeOkay = ((widthOkay && heightOkay)
                     || (widthOkay && mHeight >> level == 0 && height == 1)
                     || (heightOkay && mWidth >> level == 0 && width == 1));

    bool textureOkay = (sizeOkay && internalFormat == mImageArray[0].format);

    if (!textureOkay)
    {
        TRACE("Redefining 2D texture (%d, 0x%04X, %d, %d => 0x%04X, %d, %d).", level,
              mImageArray[0].format, mWidth, mHeight,
              internalFormat, width, height);

        // Purge all the levels and the texture.

        for (int i = 0; i < MAX_TEXTURE_LEVELS; i++)
        {
            if (mImageArray[i].surface != NULL)
            {
                mImageArray[i].dirty = false;

                mImageArray[i].surface->Release();
                mImageArray[i].surface = NULL;
            }
        }

        if (mTexture != NULL)
        {
            mTexture->Release();
            mTexture = NULL;
            dropTexture();
        }

        mWidth = width << level;
        mHeight = height << level;
        mImageArray[0].format = internalFormat;
    }

    return !textureOkay;
}

void Texture2D::setImage(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    redefineTexture(level, internalFormat, width, height);

    Texture::setImage(width, height, format, type, unpackAlignment, pixels, &mImageArray[level]);
}

void Texture2D::commitRect(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height)
{
    ASSERT(mImageArray[level].surface != NULL);

    if (level < levelCount())
    {
        IDirect3DSurface9 *destLevel = NULL;
        HRESULT result = mTexture->GetSurfaceLevel(level, &destLevel);

        ASSERT(SUCCEEDED(result));

        if (SUCCEEDED(result))
        {
            Image *img = &mImageArray[level];

            RECT sourceRect;
            sourceRect.left = xoffset;
            sourceRect.top = yoffset;
            sourceRect.right = xoffset + width;
            sourceRect.bottom = yoffset + height;

            POINT destPoint;
            destPoint.x = xoffset;
            destPoint.y = yoffset;

            result = getDevice()->UpdateSurface(img->surface, &sourceRect, destLevel, &destPoint);
            ASSERT(SUCCEEDED(result));

            destLevel->Release();

            img->dirty = false;
        }
    }
}

void Texture2D::subImage(GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    Texture::subImage(xoffset, yoffset, width, height, format, type, unpackAlignment, pixels, &mImageArray[level]);
    commitRect(level, xoffset, yoffset, width, height);
}

void Texture2D::copyImage(GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, Renderbuffer *source)
{
    if (redefineTexture(level, internalFormat, width, height))
    {
        convertToRenderTarget();
        pushTexture(mTexture, true);
    }

    if (width != 0 && height != 0 && level < levelCount())
    {
        RECT sourceRect;
        sourceRect.left = x;
        sourceRect.right = x + width;
        sourceRect.top = y;
        sourceRect.bottom = y + height;

        IDirect3DSurface9 *dest;
        HRESULT hr = mTexture->GetSurfaceLevel(level, &dest);

        getBlitter()->formatConvert(source->getRenderTarget(), sourceRect, internalFormat, 0, 0, dest);
        dest->Release();
    }

    mImageArray[level].width = width;
    mImageArray[level].height = height;
    mImageArray[level].format = internalFormat;
}

void Texture2D::copySubImage(GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, Renderbuffer *source)
{
    if (xoffset + width > mImageArray[level].width || yoffset + height > mImageArray[level].height)
    {
        return error(GL_INVALID_VALUE);
    }

    if (redefineTexture(0, mImageArray[0].format, mImageArray[0].width, mImageArray[0].height))
    {
        convertToRenderTarget();
        pushTexture(mTexture, true);
    }
    else
    {
        needRenderTarget();
    }

    if (level < levelCount())
    {
        RECT sourceRect;
        sourceRect.left = x;
        sourceRect.right = x + width;
        sourceRect.top = y;
        sourceRect.bottom = y + height;

        IDirect3DSurface9 *dest;
        HRESULT hr = mTexture->GetSurfaceLevel(level, &dest);

        getBlitter()->formatConvert(source->getRenderTarget(), sourceRect, mImageArray[0].format, xoffset, yoffset, dest);
        dest->Release();
    }
}

// Tests for GL texture object completeness. [OpenGL ES 2.0.24] section 3.7.10 page 81.
bool Texture2D::isComplete() const
{
    GLsizei width = mImageArray[0].width;
    GLsizei height = mImageArray[0].height;

    if (width <= 0 || height <= 0)
    {
        return false;
    }

    bool mipmapping = false;

    switch (mMinFilter)
    {
      case GL_NEAREST:
      case GL_LINEAR:
        mipmapping = false;
        break;
      case GL_NEAREST_MIPMAP_NEAREST:
      case GL_LINEAR_MIPMAP_NEAREST:
      case GL_NEAREST_MIPMAP_LINEAR:
      case GL_LINEAR_MIPMAP_LINEAR:
        mipmapping = true;
        break;
     default: UNREACHABLE();
    }

    if ((getWrapS() != GL_CLAMP_TO_EDGE && !isPow2(width))
        || (getWrapT() != GL_CLAMP_TO_EDGE && !isPow2(height)))
    {
        return false;
    }

    if (mipmapping)
    {
        if (!isPow2(width) || !isPow2(height))
        {
            return false;
        }

        int q = log2(std::max(width, height));

        for (int level = 1; level <= q; level++)
        {
            if (mImageArray[level].format != mImageArray[0].format)
            {
                return false;
            }

            if (mImageArray[level].width != std::max(1, width >> level))
            {
                return false;
            }

            if (mImageArray[level].height != std::max(1, height >> level))
            {
                return false;
            }
        }
    }

    return true;
}

// Constructs a Direct3D 9 texture resource from the texture images, or returns an existing one
IDirect3DBaseTexture9 *Texture2D::createTexture()
{
    IDirect3DTexture9 *texture;

    IDirect3DDevice9 *device = getDevice();
    D3DFORMAT format = selectFormat(mImageArray[0].format);

    HRESULT result = device->CreateTexture(mWidth, mHeight, creationLevels(mWidth, mHeight, 0), 0, format, D3DPOOL_DEFAULT, &texture, NULL);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
    }

    if (mTexture) mTexture->Release();
    mTexture = texture;
    return texture;
}

void Texture2D::updateTexture()
{
    IDirect3DDevice9 *device = getDevice();

    int levels = levelCount();

    for (int level = 0; level < levels; level++)
    {
        if (mImageArray[level].dirty)
        {
            IDirect3DSurface9 *levelSurface = NULL;
            HRESULT result = mTexture->GetSurfaceLevel(level, &levelSurface);

            ASSERT(SUCCEEDED(result));

            if (SUCCEEDED(result))
            {
                result = device->UpdateSurface(mImageArray[level].surface, NULL, levelSurface, NULL);
                ASSERT(SUCCEEDED(result));

                levelSurface->Release();

                mImageArray[level].dirty = false;
            }
        }
    }
}

IDirect3DBaseTexture9 *Texture2D::convertToRenderTarget()
{
    IDirect3DTexture9 *texture = NULL;

    if (mWidth != 0 && mHeight != 0)
    {
        egl::Display *display = getDisplay();
        IDirect3DDevice9 *device = getDevice();
        D3DFORMAT format = selectFormat(mImageArray[0].format);

        HRESULT result = device->CreateTexture(mWidth, mHeight, creationLevels(mWidth, mHeight, 0), D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, &texture, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
        }

        if (mTexture != NULL)
        {
            int levels = levelCount();
            for (int i = 0; i < levels; i++)
            {
                IDirect3DSurface9 *source;
                result = mTexture->GetSurfaceLevel(i, &source);

                if (FAILED(result))
                {
                    ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                    texture->Release();

                    return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
                }

                IDirect3DSurface9 *dest;
                result = texture->GetSurfaceLevel(i, &dest);

                if (FAILED(result))
                {
                    ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                    texture->Release();
                    source->Release();

                    return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
                }

                display->endScene();
                result = device->StretchRect(source, NULL, dest, NULL, D3DTEXF_NONE);

                if (FAILED(result))
                {
                    ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                    texture->Release();
                    source->Release();
                    dest->Release();

                    return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
                }

                source->Release();
                dest->Release();
            }
        }
    }

    if (mTexture != NULL)
    {
        mTexture->Release();
    }

    mTexture = texture;
    return mTexture;
}

bool Texture2D::dirtyImageData() const
{
    int q = log2(std::max(mWidth, mHeight));

    for (int i = 0; i <= q; i++)
    {
        if (mImageArray[i].dirty) return true;
    }

    return false;
}

void Texture2D::generateMipmaps()
{
    if (!isPow2(mImageArray[0].width) || !isPow2(mImageArray[0].height))
    {
        return error(GL_INVALID_OPERATION);
    }

    // Purge array levels 1 through q and reset them to represent the generated mipmap levels.
    unsigned int q = log2(std::max(mWidth, mHeight));
    for (unsigned int i = 1; i <= q; i++)
    {
        if (mImageArray[i].surface != NULL)
        {
            mImageArray[i].surface->Release();
            mImageArray[i].surface = NULL;
        }

        mImageArray[i].dirty = false;

        mImageArray[i].format = mImageArray[0].format;
        mImageArray[i].width = std::max(mImageArray[0].width >> i, 1);
        mImageArray[i].height = std::max(mImageArray[0].height >> i, 1);
    }

    needRenderTarget();

    for (unsigned int i = 1; i <= q; i++)
    {
        IDirect3DSurface9 *upper = NULL;
        IDirect3DSurface9 *lower = NULL;

        mTexture->GetSurfaceLevel(i-1, &upper);
        mTexture->GetSurfaceLevel(i, &lower);

        if (upper != NULL && lower != NULL)
        {
            getBlitter()->boxFilter(upper, lower);
        }

        if (upper != NULL) upper->Release();
        if (lower != NULL) lower->Release();
    }
}

Colorbuffer *Texture2D::getColorbuffer(GLenum target)
{
    if (target != GL_TEXTURE_2D)
    {
        return error(GL_INVALID_OPERATION, (Colorbuffer *)NULL);
    }

    if (mColorbufferProxy == NULL)
    {
        mColorbufferProxy = new TextureColorbufferProxy(this, target);
    }

    return mColorbufferProxy;
}

IDirect3DSurface9 *Texture2D::getRenderTarget(GLenum target)
{
    ASSERT(target == GL_TEXTURE_2D);

    needRenderTarget();

    IDirect3DSurface9 *renderTarget = NULL;
    mTexture->GetSurfaceLevel(0, &renderTarget);

    return renderTarget;
}

TextureCubeMap::TextureCubeMap(Context *context) : Texture(context)
{
    mTexture = NULL;

    for (int i = 0; i < 6; i++)
    {
        mFaceProxies[i] = NULL;
    }
}

TextureCubeMap::~TextureCubeMap()
{
    for (int i = 0; i < 6; i++)
    {
        delete mFaceProxies[i];
    }

    if (mTexture)
    {
        mTexture->Release();
        mTexture = NULL;
    }
}

GLenum TextureCubeMap::getTarget() const
{
    return GL_TEXTURE_CUBE_MAP;
}

void TextureCubeMap::setImagePosX(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(0, level, internalFormat, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setImageNegX(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(1, level, internalFormat, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setImagePosY(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(2, level, internalFormat, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setImageNegY(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(3, level, internalFormat, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setImagePosZ(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(4, level, internalFormat, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::setImageNegZ(GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    setImage(5, level, internalFormat, width, height, format, type, unpackAlignment, pixels);
}

void TextureCubeMap::commitRect(GLenum faceTarget, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height)
{
    int face = faceIndex(faceTarget);

    ASSERT(mImageArray[face][level].surface != NULL);

    if (level < levelCount())
    {
        IDirect3DSurface9 *destLevel = getCubeMapSurface(face, level);
        ASSERT(destLevel != NULL);

        if (destLevel != NULL)
        {
            Image *img = &mImageArray[face][level];

            RECT sourceRect;
            sourceRect.left = xoffset;
            sourceRect.top = yoffset;
            sourceRect.right = xoffset + width;
            sourceRect.bottom = yoffset + height;

            POINT destPoint;
            destPoint.x = xoffset;
            destPoint.y = yoffset;

            HRESULT result = getDevice()->UpdateSurface(img->surface, &sourceRect, destLevel, &destPoint);
            ASSERT(SUCCEEDED(result));

            destLevel->Release();

            img->dirty = false;
        }
    }
}

void TextureCubeMap::subImage(GLenum face, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    Texture::subImage(xoffset, yoffset, width, height, format, type, unpackAlignment, pixels, &mImageArray[faceIndex(face)][level]);
    commitRect(face, level, xoffset, yoffset, width, height);
}

// Tests for GL texture object completeness. [OpenGL ES 2.0.24] section 3.7.10 page 81.
bool TextureCubeMap::isComplete() const
{
    int size = mImageArray[0][0].width;

    if (size <= 0)
    {
        return false;
    }

    bool mipmapping;

    switch (mMinFilter)
    {
      case GL_NEAREST:
      case GL_LINEAR:
        mipmapping = false;
        break;
      case GL_NEAREST_MIPMAP_NEAREST:
      case GL_LINEAR_MIPMAP_NEAREST:
      case GL_NEAREST_MIPMAP_LINEAR:
      case GL_LINEAR_MIPMAP_LINEAR:
        mipmapping = true;
        break;
      default: UNREACHABLE();
    }

    for (int face = 0; face < 6; face++)
    {
        if (mImageArray[face][0].width != size || mImageArray[face][0].height != size)
        {
            return false;
        }
    }

    if (mipmapping)
    {
        if (!isPow2(size) && (getWrapS() != GL_CLAMP_TO_EDGE || getWrapT() != GL_CLAMP_TO_EDGE))
        {
            return false;
        }

        int q = log2(size);

        for (int face = 0; face < 6; face++)
        {
            for (int level = 1; level <= q; level++)
            {
                if (mImageArray[face][level].format != mImageArray[0][0].format)
                {
                    return false;
                }

                if (mImageArray[face][level].width != std::max(1, size >> level))
                {
                    return false;
                }

                ASSERT(mImageArray[face][level].height == mImageArray[face][level].width);
            }
        }
    }

    return true;
}

// Constructs a Direct3D 9 texture resource from the texture images, or returns an existing one
IDirect3DBaseTexture9 *TextureCubeMap::createTexture()
{
    IDirect3DDevice9 *device = getDevice();
    D3DFORMAT format = selectFormat(mImageArray[0][0].format);

    IDirect3DCubeTexture9 *texture;

    HRESULT result = device->CreateCubeTexture(mWidth, creationLevels(mWidth, 0), 0, format, D3DPOOL_DEFAULT, &texture, NULL);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
        return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
    }

    if (mTexture) mTexture->Release();

    mTexture = texture;
    return mTexture;
}

void TextureCubeMap::updateTexture()
{
    IDirect3DDevice9 *device = getDevice();

    for (int face = 0; face < 6; face++)
    {
        int levels = levelCount();
        for (int level = 0; level < levels; level++)
        {
            Image *img = &mImageArray[face][level];

            if (img->dirty)
            {
                IDirect3DSurface9 *levelSurface = getCubeMapSurface(face, level);
                ASSERT(levelSurface != NULL);

                if (levelSurface != NULL)
                {
                    HRESULT result = device->UpdateSurface(img->surface, NULL, levelSurface, NULL);
                    ASSERT(SUCCEEDED(result));

                    levelSurface->Release();

                    img->dirty = false;
                }
            }
        }
    }
}

IDirect3DBaseTexture9 *TextureCubeMap::convertToRenderTarget()
{
    IDirect3DCubeTexture9 *texture = NULL;

    if (mWidth != 0)
    {
        egl::Display *display = getDisplay();
        IDirect3DDevice9 *device = getDevice();
        D3DFORMAT format = selectFormat(mImageArray[0][0].format);

        HRESULT result = device->CreateCubeTexture(mWidth, creationLevels(mWidth, 0), D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, &texture, NULL);

        if (FAILED(result))
        {
            ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);
            return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
        }

        if (mTexture != NULL)
        {
            int levels = levelCount();
            for (int f = 0; f < 6; f++)
            {
                for (int i = 0; i < levels; i++)
                {
                    IDirect3DSurface9 *source;
                    result = mTexture->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(f), i, &source);

                    if (FAILED(result))
                    {
                        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                        texture->Release();

                        return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
                    }

                    IDirect3DSurface9 *dest;
                    result = texture->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(f), i, &dest);

                    if (FAILED(result))
                    {
                        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                        texture->Release();
                        source->Release();

                        return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
                    }

                    display->endScene();
                    result = device->StretchRect(source, NULL, dest, NULL, D3DTEXF_NONE);

                    if (FAILED(result))
                    {
                        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

                        texture->Release();
                        source->Release();
                        dest->Release();

                        return error(GL_OUT_OF_MEMORY, (IDirect3DBaseTexture9*)NULL);
                    }
                }
            }
        }
    }

    if (mTexture != NULL)
    {
        mTexture->Release();
    }

    mTexture = texture;
    return mTexture;
}

void TextureCubeMap::setImage(int face, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint unpackAlignment, const void *pixels)
{
    redefineTexture(level, internalFormat, width);

    Texture::setImage(width, height, format, type, unpackAlignment, pixels, &mImageArray[face][level]);
}

unsigned int TextureCubeMap::faceIndex(GLenum face)
{
    META_ASSERT(GL_TEXTURE_CUBE_MAP_NEGATIVE_X - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 1);
    META_ASSERT(GL_TEXTURE_CUBE_MAP_POSITIVE_Y - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 2);
    META_ASSERT(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 3);
    META_ASSERT(GL_TEXTURE_CUBE_MAP_POSITIVE_Z - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 4);
    META_ASSERT(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z - GL_TEXTURE_CUBE_MAP_POSITIVE_X == 5);

    return face - GL_TEXTURE_CUBE_MAP_POSITIVE_X;
}

bool TextureCubeMap::dirtyImageData() const
{
    int q = log2(mWidth);

    for (int f = 0; f < 6; f++)
    {
        for (int i = 0; i <= q; i++)
        {
            if (mImageArray[f][i].dirty) return true;
        }
    }

    return false;
}

// While OpenGL doesn't check texture consistency until draw-time, D3D9 requires a complete texture
// for render-to-texture (such as CopyTexImage). We have no way of keeping individual inconsistent levels & faces.
// Call this when a particular level of the texture must be defined with a specific format, width and height.
//
// Returns true if the existing texture was unsuitable had to be destroyed. If so, it will also set
// a new size for the texture by working backwards from the given size.
bool TextureCubeMap::redefineTexture(GLint level, GLenum internalFormat, GLsizei width)
{
    // Are these settings compatible with level 0?
    bool sizeOkay = (mImageArray[0][0].width >> level == width);

    bool textureOkay = (sizeOkay && internalFormat == mImageArray[0][0].format);

    if (!textureOkay)
    {
        TRACE("Redefining cube texture (%d, 0x%04X, %d => 0x%04X, %d).", level,
              mImageArray[0][0].format, mImageArray[0][0].width,
              internalFormat, width);

        // Purge all the levels and the texture.
        for (int i = 0; i < MAX_TEXTURE_LEVELS; i++)
        {
            for (int f = 0; f < 6; f++)
            {
                if (mImageArray[f][i].surface != NULL)
                {
                    mImageArray[f][i].dirty = false;

                    mImageArray[f][i].surface->Release();
                    mImageArray[f][i].surface = NULL;
                }
            }
        }

        if (mTexture != NULL)
        {
            mTexture->Release();
            mTexture = NULL;
            dropTexture();
        }

        mWidth = width << level;
        mImageArray[0][0].width = width << level;
        mHeight = width << level;
        mImageArray[0][0].height = width << level;

        mImageArray[0][0].format = internalFormat;
    }

    return !textureOkay;
}

void TextureCubeMap::copyImage(GLenum face, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, Renderbuffer *source)
{
    unsigned int faceindex = faceIndex(face);

    if (redefineTexture(level, internalFormat, width))
    {
        convertToRenderTarget();
        pushTexture(mTexture, true);
    }

    ASSERT(width == height);

    if (width > 0 && level < levelCount())
    {
        RECT sourceRect;
        sourceRect.left = x;
        sourceRect.right = x + width;
        sourceRect.top = y;
        sourceRect.bottom = y + height;

        IDirect3DSurface9 *dest = getCubeMapSurface(face, level);

        getBlitter()->formatConvert(source->getRenderTarget(), sourceRect, internalFormat, 0, 0, dest);
        dest->Release();
    }

    mImageArray[faceindex][level].width = width;
    mImageArray[faceindex][level].height = height;
    mImageArray[faceindex][level].format = internalFormat;
}

IDirect3DSurface9 *TextureCubeMap::getCubeMapSurface(unsigned int faceIdentifier, unsigned int level)
{
    unsigned int faceIndex;

    if (faceIdentifier < 6)
    {
        faceIndex = faceIdentifier;
    }
    else if (faceIdentifier >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && faceIdentifier <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
    {
        faceIndex = faceIdentifier - GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    }
    else
    {
        UNREACHABLE();
        faceIndex = 0;
    }

    if (mTexture == NULL)
    {
        UNREACHABLE();
        return NULL;
    }

    IDirect3DSurface9 *surface = NULL;

    HRESULT hr = mTexture->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(faceIndex), level, &surface);

    return (SUCCEEDED(hr)) ? surface : NULL;
}

void TextureCubeMap::copySubImage(GLenum face, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, Renderbuffer *source)
{
    GLsizei size = mImageArray[faceIndex(face)][level].width;

    if (xoffset + width > size || yoffset + height > size)
    {
        return error(GL_INVALID_VALUE);
    }

    if (redefineTexture(0, mImageArray[0][0].format, mImageArray[0][0].width))
    {
        convertToRenderTarget();
        pushTexture(mTexture, true);
    }
    else
    {
        getRenderTarget(face);
    }

    if (level < levelCount())
    {
        RECT sourceRect;
        sourceRect.left = x;
        sourceRect.right = x + width;
        sourceRect.top = y;
        sourceRect.bottom = y + height;

        IDirect3DSurface9 *dest = getCubeMapSurface(face, level);

        getBlitter()->formatConvert(source->getRenderTarget(), sourceRect, mImageArray[0][0].format, xoffset, yoffset, dest);
        dest->Release();
    }
}

bool TextureCubeMap::isCubeComplete() const
{
    if (mImageArray[0][0].width == 0)
    {
        return false;
    }

    for (unsigned int f = 1; f < 6; f++)
    {
        if (mImageArray[f][0].width != mImageArray[0][0].width
            || mImageArray[f][0].format != mImageArray[0][0].format)
        {
            return false;
        }
    }

    return true;
}

void TextureCubeMap::generateMipmaps()
{
    if (!isPow2(mImageArray[0][0].width) || !isCubeComplete())
    {
        return error(GL_INVALID_OPERATION);
    }

    // Purge array levels 1 through q and reset them to represent the generated mipmap levels.
    unsigned int q = log2(mImageArray[0][0].width);
    for (unsigned int f = 0; f < 6; f++)
    {
        for (unsigned int i = 1; i <= q; i++)
        {
            if (mImageArray[f][i].surface != NULL)
            {
                mImageArray[f][i].surface->Release();
                mImageArray[f][i].surface = NULL;
            }

            mImageArray[f][i].dirty = false;

            mImageArray[f][i].format = mImageArray[f][0].format;
            mImageArray[f][i].width = std::max(mImageArray[f][0].width >> i, 1);
            mImageArray[f][i].height = mImageArray[f][i].width;
        }
    }

    needRenderTarget();

    for (unsigned int f = 0; f < 6; f++)
    {
        for (unsigned int i = 1; i <= q; i++)
        {
            IDirect3DSurface9 *upper = getCubeMapSurface(f, i-1);
            IDirect3DSurface9 *lower = getCubeMapSurface(f, i);

            if (upper != NULL && lower != NULL)
            {
                getBlitter()->boxFilter(upper, lower);
            }

            if (upper != NULL) upper->Release();
            if (lower != NULL) lower->Release();
        }
    }
}

Colorbuffer *TextureCubeMap::getColorbuffer(GLenum target)
{
    if (!IsCubemapTextureTarget(target))
    {
        return error(GL_INVALID_OPERATION, (Colorbuffer *)NULL);
    }

    unsigned int face = faceIndex(target);

    if (mFaceProxies[face] == NULL)
    {
        mFaceProxies[face] = new TextureColorbufferProxy(this, target);
    }

    return mFaceProxies[face];
}

IDirect3DSurface9 *TextureCubeMap::getRenderTarget(GLenum target)
{
    ASSERT(IsCubemapTextureTarget(target));

    needRenderTarget();

    IDirect3DSurface9 *renderTarget = NULL;
    mTexture->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(faceIndex(target)), 0, &renderTarget);

    return renderTarget;
}

Texture::TextureColorbufferProxy::TextureColorbufferProxy(Texture *texture, GLenum target)
  : Colorbuffer(NULL), mTexture(texture), mTarget(target)
{
    ASSERT(target == GL_TEXTURE_2D || IsCubemapTextureTarget(target));
}

IDirect3DSurface9 *Texture::TextureColorbufferProxy::getRenderTarget()
{
    if (mRenderTarget) mRenderTarget->Release();

    mRenderTarget = mTexture->getRenderTarget(mTarget);

    return mRenderTarget;
}

int Texture::TextureColorbufferProxy::getWidth()
{
    return mTexture->getWidth();
}

int Texture::TextureColorbufferProxy::getHeight()
{
    return mTexture->getHeight();
}

}
