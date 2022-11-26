//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// PLSProgramCache.h: Implements a cache of native programs used to render load/store operations for
// EXT_shader_pixel_local_storage.

#ifndef LIBANGLE_RENDERER_GL_PLS_PROGRAM_CACHE_H_
#define LIBANGLE_RENDERER_GL_PLS_PROGRAM_CACHE_H_

#include "libANGLE/SizedMRUCache.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"

namespace gl
{
class PixelLocalStoragePlane;
struct Caps;
}  // namespace gl

namespace rx
{
class FunctionsGL;
class PLSProgram;
class PLSProgramKey;

// Implements a cache of native PLSPrograms used to render load/store operations for
// EXT_shader_pixel_local_storage.
//
// These programs require no vertex arrays, and draw fullscreen quads from 4-point
// GL_TRIANGLE_STRIPs:
//
//   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4)
//
class PLSProgramCache : angle::NonCopyable
{
  public:
    size_t MaximumTotalPrograms = 64 * 2;  // Enough programs for 64 unique PLS configurations.

    PLSProgramCache(const FunctionsGL *, const gl::Caps &nativeCaps);
    ~PLSProgramCache();

    const PLSProgram *getProgram(PLSProgramKey);

    // An empty vertex array object to bind when rendering with a program from this cache.
    GLuint getEmptyVAO() { return mEmptyVAO; }
    VertexArrayStateGL *getEmptyVAOState() { return &mEmptyVAOState; }

  private:
    const FunctionsGL *const mGL;
    const GLuint mVertexShaderID;
    const GLuint mEmptyVAO;
    VertexArrayStateGL mEmptyVAOState;
    angle::SizedMRUCache<uint64_t, std::unique_ptr<PLSProgram>> mCache;
};

enum class PLSProgramType : uint64_t
{
    Load  = 0,  // Initializes PLS data with either a uniform color or an image load.
    Store = 1   // Stores texture-backed PLS data out to images.
};

// Re-enumerates PLS formats with a 0-based index, for tighter packing in a PLSProgramKey.
enum class PLSFormatKey : uint64_t
{
    INACTIVE = 0,
    RGBA8    = 1,
    RGBA8I   = 2,
    RGBA8UI  = 3,
    R32F     = 4,
    R32UI    = 5
};

// Compact descriptor of an entire PLS load/store program. The LSB says whether the program is load
// or store, and each following run of 5 bits state the format of a specific plane and whether it is
// preserved.
class PLSProgramKey
{
  public:
    constexpr static int BitsPerPlane    = 5;
    constexpr static int SinglePlaneMask = (1 << BitsPerPlane) - 1;

    PLSProgramKey() = default;
    PLSProgramKey(const PLSProgramKey &key) : PLSProgramKey(key.rawKey()) {}
    explicit PLSProgramKey(uint64_t rawKey) : mRawKey(rawKey) {}

    PLSProgramKey &operator=(const PLSProgramKey &key)
    {
        mRawKey = key.mRawKey;
        return *this;
    }

    uint64_t rawKey() const { return mRawKey; }
    PLSProgramType type() const;
    bool areAnyPreserved() const;

    // Iterates each active plane in the descriptor, in order.
    class Iter
    {
      public:
        Iter() = default;
        Iter(const PLSProgramKey &);

        int binding() const { return mBinding; }
        PLSFormatKey formatKey() const;
        bool preserved() const;
        std::tuple<int, PLSFormatKey, bool> operator*() const
        {
            return {binding(), formatKey(), preserved()};
        }

        bool operator!=(const Iter &iter) const;
        void operator++();

      private:
        // Skips over any planes that are not active. The only effect inactive planes have on
        // shaders is to offset the next binding index.
        void skipInactivePlanes();

        int mBinding        = 0;
        uint64_t mPlaneKeys = 0;
    };
    Iter begin() const { return Iter(*this); }
    Iter end() const { return Iter(); }

  private:
    uint64_t mRawKey = 0;
};

class PLSProgramKeyBuilder
{
  public:
    // Prepends a plane's format and whether it is preserved to the descriptor.
    void prependPlane(GLenum internalformat, bool preserved);
    PLSProgramKey finish(PLSProgramType);

  private:
    uint64_t mRawKey = 0;
};

class PLSProgram : angle::NonCopyable
{
  public:
    PLSProgram(const FunctionsGL *, GLuint vertexShaderID, PLSProgramKey);
    ~PLSProgram();

    GLuint getProgramID() const { return mProgramID; }

    void setClearValues(const gl::PixelLocalStoragePlane[], const GLenum loadops[]) const;

  private:
    const FunctionsGL *const mGL;
    const PLSProgramKey mKey;
    const GLuint mProgramID;
    gl::DrawBuffersArray<GLint> mClearValueUniformLocations;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_PLS_PROGRAM_CACHE_H_
