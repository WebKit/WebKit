//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ShaderCache: Defines rx::ShaderCache, a cache of Direct3D shader objects
// keyed by their byte code.

#ifndef LIBANGLE_RENDERER_D3D_D3D9_SHADERCACHE_H_
#define LIBANGLE_RENDERER_D3D_D3D9_SHADERCACHE_H_

#include "libANGLE/Error.h"

#include "common/debug.h"
#include "libANGLE/renderer/d3d/d3d9/Context9.h"

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace rx
{
template <typename ShaderObject>
class ShaderCache : angle::NonCopyable
{
  public:
    ShaderCache() : mDevice(nullptr) {}

    ~ShaderCache()
    {
        // Call clear while the device is still valid.
        ASSERT(mMap.empty());
    }

    void initialize(IDirect3DDevice9 *device) { mDevice = device; }

    angle::Result create(Context9 *context9,
                         const DWORD *function,
                         size_t length,
                         ShaderObject **outShaderObject)
    {
        std::lock_guard<std::mutex> lock(mMutex);

        std::string key(reinterpret_cast<const char *>(function), length);
        typename Map::iterator it = mMap.find(key);
        if (it != mMap.end())
        {
            it->second->AddRef();
            *outShaderObject = it->second;
            return angle::Result::Continue;
        }

        ShaderObject *shader;
        HRESULT result = createShader(function, &shader);
        ANGLE_TRY_HR(context9, result, "Failed to create shader");

        // Random eviction policy.
        if (mMap.size() >= kMaxMapSize)
        {
            SafeRelease(mMap.begin()->second);
            mMap.erase(mMap.begin());
        }

        shader->AddRef();
        mMap[key] = shader;

        *outShaderObject = shader;
        return angle::Result::Continue;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);

        for (typename Map::iterator it = mMap.begin(); it != mMap.end(); ++it)
        {
            SafeRelease(it->second);
        }

        mMap.clear();
    }

  private:
    const static size_t kMaxMapSize = 100;

    HRESULT createShader(const DWORD *function, IDirect3DVertexShader9 **shader)
    {
        return mDevice->CreateVertexShader(function, shader);
    }

    HRESULT createShader(const DWORD *function, IDirect3DPixelShader9 **shader)
    {
        return mDevice->CreatePixelShader(function, shader);
    }

    typedef std::unordered_map<std::string, ShaderObject *> Map;
    Map mMap;
    std::mutex mMutex;

    IDirect3DDevice9 *mDevice;
};

typedef ShaderCache<IDirect3DVertexShader9> VertexShaderCache;
typedef ShaderCache<IDirect3DPixelShader9> PixelShaderCache;

}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_D3D9_SHADERCACHE_H_
