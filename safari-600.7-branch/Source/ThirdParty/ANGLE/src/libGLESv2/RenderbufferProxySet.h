//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RenderbufferProxySet.h: Defines the gl::RenderbufferProxySet, a class for
// maintaining a Texture's weak references to the Renderbuffers that represent it.

#ifndef LIBGLESV2_RENDERBUFFERPROXYSET_H_
#define LIBGLESV2_RENDERBUFFERPROXYSET_H_

#include <map>

namespace gl
{
class Renderbuffer;

class RenderbufferProxySet
{
  public:
    void addRef(const Renderbuffer *proxy);
    void release(const Renderbuffer *proxy);

    void add(unsigned int mipLevel, unsigned int layer, Renderbuffer *renderBuffer);
    Renderbuffer *get(unsigned int mipLevel, unsigned int layer) const;

  private:
    struct RenderbufferKey
    {
        unsigned int mipLevel;
        unsigned int layer;

        bool operator<(const RenderbufferKey &other) const;
    };

    typedef std::map<RenderbufferKey, Renderbuffer*> BufferMap;
    BufferMap mBufferMap;

    typedef std::map<const Renderbuffer*, unsigned int> RefCountMap;
    RefCountMap mRefCountMap;
};

}

#endif // LIBGLESV2_RENDERBUFFERPROXYSET_H_
