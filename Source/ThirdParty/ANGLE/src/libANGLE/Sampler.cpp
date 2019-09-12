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
{}

Sampler::~Sampler()
{
    SafeDelete(mImpl);
}

void Sampler::onDestroy(const Context *context) {}

void Sampler::setLabel(const Context *context, const std::string &label)
{
    mLabel = label;
}

const std::string &Sampler::getLabel() const
{
    return mLabel;
}

void Sampler::setMinFilter(const Context *context, GLenum minFilter)
{
    mState.setMinFilter(minFilter);
    onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);
}

GLenum Sampler::getMinFilter() const
{
    return mState.getMinFilter();
}

void Sampler::setMagFilter(const Context *context, GLenum magFilter)
{
    mState.setMagFilter(magFilter);
    onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);
}

GLenum Sampler::getMagFilter() const
{
    return mState.getMagFilter();
}

void Sampler::setWrapS(const Context *context, GLenum wrapS)
{
    mState.setWrapS(wrapS);
    onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);
}

GLenum Sampler::getWrapS() const
{
    return mState.getWrapS();
}

void Sampler::setWrapT(const Context *context, GLenum wrapT)
{
    mState.setWrapT(wrapT);
    onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);
}

GLenum Sampler::getWrapT() const
{
    return mState.getWrapT();
}

void Sampler::setWrapR(const Context *context, GLenum wrapR)
{
    mState.setWrapR(wrapR);
    onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);
}

GLenum Sampler::getWrapR() const
{
    return mState.getWrapR();
}

void Sampler::setMaxAnisotropy(const Context *context, float maxAnisotropy)
{
    mState.setMaxAnisotropy(maxAnisotropy);
    onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);
}

float Sampler::getMaxAnisotropy() const
{
    return mState.getMaxAnisotropy();
}

void Sampler::setMinLod(const Context *context, GLfloat minLod)
{
    mState.setMinLod(minLod);
    onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);
}

GLfloat Sampler::getMinLod() const
{
    return mState.getMinLod();
}

void Sampler::setMaxLod(const Context *context, GLfloat maxLod)
{
    mState.setMaxLod(maxLod);
    onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);
}

GLfloat Sampler::getMaxLod() const
{
    return mState.getMaxLod();
}

void Sampler::setCompareMode(const Context *context, GLenum compareMode)
{
    mState.setCompareMode(compareMode);
    onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);
}

GLenum Sampler::getCompareMode() const
{
    return mState.getCompareMode();
}

void Sampler::setCompareFunc(const Context *context, GLenum compareFunc)
{
    mState.setCompareFunc(compareFunc);
    onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);
}

GLenum Sampler::getCompareFunc() const
{
    return mState.getCompareFunc();
}

void Sampler::setSRGBDecode(const Context *context, GLenum sRGBDecode)
{
    mState.setSRGBDecode(sRGBDecode);
    onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);
}

GLenum Sampler::getSRGBDecode() const
{
    return mState.getSRGBDecode();
}

void Sampler::setBorderColor(const Context *context, const ColorGeneric &color)
{
    mState.setBorderColor(color);
    onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);
}

const ColorGeneric &Sampler::getBorderColor() const
{
    return mState.getBorderColor();
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
