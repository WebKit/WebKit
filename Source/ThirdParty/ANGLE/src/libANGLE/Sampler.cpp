//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Sampler.cpp : Implements the Sampler class, which represents a GLES 3
// sampler object. Sampler objects store some state needed to sample textures.

#include "libANGLE/Sampler.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/SamplerImpl.h"

namespace gl
{

Sampler::Sampler(rx::GLImplFactory *factory, GLuint id)
    : RefCountObject(id), mState(), mImpl(factory->createSampler(mState)), mLabel()
{
}

Sampler::~Sampler()
{
    SafeDelete(mImpl);
}

Error Sampler::onDestroy(const Context *context)
{
    return NoError();
}

void Sampler::setLabel(const std::string &label)
{
    mLabel = label;
}

const std::string &Sampler::getLabel() const
{
    return mLabel;
}

void Sampler::setMinFilter(GLenum minFilter)
{
    mState.minFilter = minFilter;
}

GLenum Sampler::getMinFilter() const
{
    return mState.minFilter;
}

void Sampler::setMagFilter(GLenum magFilter)
{
    mState.magFilter = magFilter;
}

GLenum Sampler::getMagFilter() const
{
    return mState.magFilter;
}

void Sampler::setWrapS(GLenum wrapS)
{
    mState.wrapS = wrapS;
}

GLenum Sampler::getWrapS() const
{
    return mState.wrapS;
}

void Sampler::setWrapT(GLenum wrapT)
{
    mState.wrapT = wrapT;
}

GLenum Sampler::getWrapT() const
{
    return mState.wrapT;
}

void Sampler::setWrapR(GLenum wrapR)
{
    mState.wrapR = wrapR;
}

GLenum Sampler::getWrapR() const
{
    return mState.wrapR;
}

void Sampler::setMaxAnisotropy(float maxAnisotropy)
{
    mState.maxAnisotropy = maxAnisotropy;
}

float Sampler::getMaxAnisotropy() const
{
    return mState.maxAnisotropy;
}

void Sampler::setMinLod(GLfloat minLod)
{
    mState.minLod = minLod;
}

GLfloat Sampler::getMinLod() const
{
    return mState.minLod;
}

void Sampler::setMaxLod(GLfloat maxLod)
{
    mState.maxLod = maxLod;
}

GLfloat Sampler::getMaxLod() const
{
    return mState.maxLod;
}

void Sampler::setCompareMode(GLenum compareMode)
{
    mState.compareMode = compareMode;
}

GLenum Sampler::getCompareMode() const
{
    return mState.compareMode;
}

void Sampler::setCompareFunc(GLenum compareFunc)
{
    mState.compareFunc = compareFunc;
}

GLenum Sampler::getCompareFunc() const
{
    return mState.compareFunc;
}

void Sampler::setSRGBDecode(GLenum sRGBDecode)
{
    mState.sRGBDecode = sRGBDecode;
}

GLenum Sampler::getSRGBDecode() const
{
    return mState.sRGBDecode;
}

const SamplerState &Sampler::getSamplerState() const
{
    return mState;
}

rx::SamplerImpl *Sampler::getImplementation() const
{
    return mImpl;
}

void Sampler::syncState(const Context *context)
{
    // TODO(jmadill): Use actual dirty bits for sampler.
    mImpl->syncState(context);
}

}  // namespace gl
