//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// StreamImpl.h: Defines the abstract rx::StreamImpl class.

#ifndef LIBANGLE_RENDERER_STREAMIMPL_H_
#define LIBANGLE_RENDERER_STREAMIMPL_H_

#include "common/angleutils.h"

namespace rx
{

class StreamImpl : angle::NonCopyable
{
  public:
    explicit StreamImpl() {}
    virtual ~StreamImpl() {}
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_STREAMIMPL_H_
