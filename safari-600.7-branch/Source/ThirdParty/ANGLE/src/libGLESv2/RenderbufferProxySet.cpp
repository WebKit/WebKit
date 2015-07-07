#include "precompiled.h"
//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RenderbufferProxySet.cpp: Implements the gl::RenderbufferProxySet, a class for
// maintaining a Texture's weak references to the Renderbuffers that represent it.

#include "libGLESv2/RenderbufferProxySet.h"
#include "common/debug.h"

namespace gl
{

void RenderbufferProxySet::addRef(const Renderbuffer *proxy)
{
    RefCountMap::iterator i = mRefCountMap.find(proxy);
    if (i != mRefCountMap.end())
    {
        i->second++;
    }
}

void RenderbufferProxySet::release(const Renderbuffer *proxy)
{
    RefCountMap::iterator i = mRefCountMap.find(proxy);
    if (i != mRefCountMap.end())
    {
        if (i->second > 0)
        {
            i->second--;
        }

        if (i->second == 0)
        {
            // Clear the buffer map of references to this Renderbuffer
            BufferMap::iterator j = mBufferMap.begin();
            while (j != mBufferMap.end())
            {
                if (j->second == proxy)
                {
                    j = mBufferMap.erase(j);
                }
                else
                {
                    ++j;
                }
            }

            mRefCountMap.erase(i);
        }
    }
}

void RenderbufferProxySet::add(unsigned int mipLevel, unsigned int layer, Renderbuffer *renderBuffer)
{
    if (mRefCountMap.find(renderBuffer) == mRefCountMap.end())
    {
        mRefCountMap.insert(std::make_pair(renderBuffer, 0));
    }

    RenderbufferKey key;
    key.mipLevel = mipLevel;
    key.layer = layer;
    if (mBufferMap.find(key) == mBufferMap.end())
    {
        mBufferMap.insert(std::make_pair(key, renderBuffer));
    }
}

Renderbuffer *RenderbufferProxySet::get(unsigned int mipLevel, unsigned int layer) const
{
    RenderbufferKey key;
    key.mipLevel = mipLevel;
    key.layer = layer;
    BufferMap::const_iterator i = mBufferMap.find(key);
    return (i != mBufferMap.end()) ? i->second : NULL;
}

bool RenderbufferProxySet::RenderbufferKey::operator<(const RenderbufferKey &other) const
{
    return (mipLevel != other.mipLevel) ? mipLevel < other.mipLevel : layer < other.layer;
}

}
