//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Fence.h: Defines the gl::FenceNV and gl::Sync classes, which support the GL_NV_fence
// extension and GLES3 sync objects.

#ifndef LIBANGLE_FENCE_H_
#define LIBANGLE_FENCE_H_

#include "libANGLE/Debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/RefCountObject.h"

#include "common/angleutils.h"

namespace rx
{
class FenceNVImpl;
class SyncImpl;
}

namespace gl
{

class FenceNV final : angle::NonCopyable
{
  public:
    explicit FenceNV(rx::FenceNVImpl *impl);
    virtual ~FenceNV();

    Error set(GLenum condition);
    Error test(GLboolean *outResult);
    Error finish();

    bool isSet() const { return mIsSet; }
    GLboolean getStatus() const { return mStatus; }
    GLenum getCondition() const { return mCondition; }

  private:
    rx::FenceNVImpl *mFence;

    bool mIsSet;

    GLboolean mStatus;
    GLenum mCondition;
};

class Sync final : public RefCountObject, public LabeledObject
{
  public:
    Sync(rx::SyncImpl *impl, GLuint id);
    ~Sync() override;

    Error onDestroy(const Context *context) override;

    void setLabel(const std::string &label) override;
    const std::string &getLabel() const override;

    Error set(GLenum condition, GLbitfield flags);
    Error clientWait(GLbitfield flags, GLuint64 timeout, GLenum *outResult);
    Error serverWait(GLbitfield flags, GLuint64 timeout);
    Error getStatus(GLint *outResult) const;

    GLenum getCondition() const { return mCondition; }
    GLbitfield getFlags() const { return mFlags; }

  private:
    rx::SyncImpl *mFence;

    std::string mLabel;

    GLenum mCondition;
    GLbitfield mFlags;
};

}  // namespace gl

#endif   // LIBANGLE_FENCE_H_
