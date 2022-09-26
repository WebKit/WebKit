//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// PixelLocalStorage.h: Defines the renderer-agnostic container classes
// gl::PixelLocalStorage and gl::PixelLocalStoragePlane for
// ANGLE_shader_pixel_local_storage.

#ifndef LIBANGLE_PIXEL_LOCAL_STORAGE_H_
#define LIBANGLE_PIXEL_LOCAL_STORAGE_H_

#include "angle_gl.h"
#include "libANGLE/ImageIndex.h"
#include "libANGLE/angletypes.h"

namespace gl
{

class Context;
class Texture;

// Holds the configuration of an ANGLE_shader_pixel_local_storage plane.
//
// Unlike normal framebuffer attachments, pixel local storage planes don't take effect until the
// application calls glBeginPixelLocalStorageANGLE, and the manner in which they take effect is
// highly dependent on the backend implementation. A PixelLocalStoragePlane is just a plain data
// description what to set up later once PLS is enabled.
class PixelLocalStoragePlane : angle::NonCopyable
{
  public:
    ~PixelLocalStoragePlane();

    // Called when the context is lost or destroyed. Causes this class to clear its GL object
    // handles.
    void onContextObjectsLost();

    // Called when the owning framebuffer is being destroyed. Causes this class to release its
    // texture object reference.
    void onFramebufferDestroyed(const Context *);

    void deinitialize(Context *);
    void setMemoryless(Context *, GLenum internalformat);
    void setTextureBacked(Context *, Texture *, int level, int layer);

    bool isDeinitialized() const { return mInternalformat == GL_NONE; }

    // Returns true if the texture ID bound to this plane has been deleted.
    //
    // [ANGLE_shader_pixel_local_storage] Section 4.4.2.X "Configuring Pixel Local Storage
    // on a Framebuffer": When a texture object is deleted, any pixel local storage plane to
    // which it was bound is automatically converted to a memoryless plane of matching
    // internalformat.
    bool isTextureIDDeleted(const Context *) const;

    bool isMemoryless() const
    {
        // isMemoryless() should be false if the plane is deinitialized.
        ASSERT(!(isDeinitialized() && mMemoryless));
        return mMemoryless;
    }

    GLenum getInternalformat() const { return mInternalformat; }

    // Implements glGetIntegeri_v() for GL_PIXEL_LOCAL_FORMAT_ANGLE,
    // GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE, GL_PIXEL_LOCAL_TEXTURE_LEVEL_ANGLE, and
    // GL_PIXEL_LOCAL_TEXTURE_LAYER_ANGLE
    GLint getIntegeri(const Context *, GLenum target, GLuint index) const;

    // If this plane is texture backed, stores the bound texture image's {width, height, 0} to
    // Extents and returns true. Otherwise returns false, meaning the plane is either deinitialized
    // or memoryless.
    bool getTextureImageExtents(const Context *, Extents *extents) const;

    // Attaches this plane to the specified color attachment point on the current draw framebuffer.
    void attachToDrawFramebuffer(Context *, Extents plsExtents, GLenum colorAttachment);

    // Clears the draw buffer at 0-based index 'drawbuffer' on the current framebuffer. Reads the
    // clear value from 'data' if 'loadop' is GL_CLEAR_ANGLE, otherwise clears to zero.
    //
    // 'data' is interpereted as either 4 GLfloats, 4 GLints, or 4 GLuints, depending on
    // mInternalFormat.
    //
    // The context must internally disable the scissor test before calling this method, since the
    // intention is to clear the entire surface.
    void performLoadOperationClear(Context *, GLint drawbuffer, GLenum loadop, const void *data);

    // Binds this PLS plane to a texture image unit for image load/store shader operations.
    void bindToImage(Context *, Extents plsExtents, GLuint unit, bool needsR32Packing);

  private:
    // Ensures we have an internal backing texture for memoryless planes. In GL, we need a backing
    // texture even if the plane is memoryless; glInvalidateFramebuffer() will ideally prevent the
    // driver from writing out data where possible.
    void ensureBackingIfMemoryless(Context *, Extents plsSize);

    GLenum mInternalformat = GL_NONE;  // GL_NONE if this plane is in a deinitialized state.
    bool mMemoryless       = false;
    TextureID mMemorylessTextureID{};  // We own memoryless backing textures and must delete them.
    ImageIndex mTextureImageIndex;
    Texture *mTextureRef = nullptr;
};

// Manages a collection of PixelLocalStoragePlanes and applies them to ANGLE's GL state.
//
// The main magic of ANGLE_shader_pixel_local_storage happens inside shaders, so we just emulate the
// client API on top of ANGLE's OpenGL ES API for simplicity.
class PixelLocalStorage
{
  public:
    static std::unique_ptr<PixelLocalStorage> Make(const Context *);

    PixelLocalStorage();
    virtual ~PixelLocalStorage();

    // Called when the owning framebuffer is being destroyed.
    void onFramebufferDestroyed(const Context *);

    // Deletes any GL objects that have been allocated for pixel local storage. These can't be
    // cleaned up in the destructor because they require a non-const Context object.
    void deleteContextObjects(Context *);

    const PixelLocalStoragePlane &getPlane(GLint plane) const
    {
        ASSERT(0 <= plane && plane < IMPLEMENTATION_MAX_PIXEL_LOCAL_STORAGE_PLANES);
        return mPlanes[plane];
    }

    PixelLocalStoragePlane &getPlane(GLint plane)
    {
        ASSERT(0 <= plane && plane < IMPLEMENTATION_MAX_PIXEL_LOCAL_STORAGE_PLANES);
        return mPlanes[plane];
    }

    // ANGLE_shader_pixel_local_storage API.
    void deinitialize(Context *context, GLint plane) { mPlanes[plane].deinitialize(context); }
    void setMemoryless(Context *context, GLint plane, GLenum internalformat)
    {
        mPlanes[plane].setMemoryless(context, internalformat);
    }
    void setTextureBacked(Context *context, GLint plane, Texture *tex, int level, int layer)
    {
        mPlanes[plane].setTextureBacked(context, tex, level, layer);
    }
    void begin(Context *, GLsizei n, const GLenum loadops[], const void *cleardata);
    void end(Context *);
    void barrier(Context *);

  protected:
    // Called when the context is lost or destroyed. Causes the subclass to clear its GL object
    // handles.
    virtual void onContextObjectsLost() = 0;

    // Called when the framebuffer is being destroyed. Causes the subclass to delete its frontend GL
    // object handles.
    virtual void onDeleteContextObjects(Context *) = 0;

    // ANGLE_shader_pixel_local_storage API.
    virtual void onBegin(Context *,
                         GLsizei n,
                         const GLenum loadops[],
                         const char *cleardata,
                         Extents plsSize)                     = 0;
    virtual void onEnd(Context *, GLsizei numActivePLSPlanes) = 0;
    virtual void onBarrier(Context *)                         = 0;

  private:
    std::array<PixelLocalStoragePlane, IMPLEMENTATION_MAX_PIXEL_LOCAL_STORAGE_PLANES> mPlanes;

    // "n" from the last call to begin(), or 0 if pixel local storage is not active.
    GLsizei mNumActivePLSPlanes = 0;
};

}  // namespace gl

#endif  // LIBANGLE_PIXEL_LOCAL_STORAGE_H_
