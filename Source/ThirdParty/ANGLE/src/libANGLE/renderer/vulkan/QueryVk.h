//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// QueryVk.h:
//    Defines the class interface for QueryVk, implementing QueryImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_QUERYVK_H_
#define LIBANGLE_RENDERER_VULKAN_QUERYVK_H_

#include "libANGLE/renderer/QueryImpl.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{

class QueryVk : public QueryImpl
{
  public:
    QueryVk(gl::QueryType type);
    ~QueryVk() override;

    void onDestroy(const gl::Context *context) override;

    angle::Result begin(const gl::Context *context) override;
    angle::Result end(const gl::Context *context) override;
    angle::Result queryCounter(const gl::Context *context) override;
    angle::Result getResult(const gl::Context *context, GLint *params) override;
    angle::Result getResult(const gl::Context *context, GLuint *params) override;
    angle::Result getResult(const gl::Context *context, GLint64 *params) override;
    angle::Result getResult(const gl::Context *context, GLuint64 *params) override;
    angle::Result isResultAvailable(const gl::Context *context, bool *available) override;

  private:
    angle::Result getResult(const gl::Context *context, bool wait);

    // Used for AnySamples, AnySamplesConservative, Timestamp and TimeElapsed (end)
    vk::QueryHelper mQueryHelper;
    // An additional query used for TimeElapsed (begin), as it is implemented using Timestamp
    vk::QueryHelper mQueryHelperTimeElapsedBegin;
    uint64_t mCachedResult;
    bool mCachedResultValid;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_QUERYVK_H_
