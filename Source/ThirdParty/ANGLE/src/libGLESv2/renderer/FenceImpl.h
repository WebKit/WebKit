//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FenceImpl.h: Defines the rx::FenceImpl class.

#ifndef LIBGLESV2_RENDERER_FENCEIMPL_H_
#define LIBGLESV2_RENDERER_FENCEIMPL_H_

#include "common/angleutils.h"

namespace rx
{

class FenceImpl
{
  public:
    FenceImpl() { };
    virtual ~FenceImpl() { };

    virtual bool isSet() const = 0;
    virtual void set() = 0;
    virtual bool test(bool flushCommandBuffer) = 0;
    virtual bool hasError() const = 0;

  private:
    DISALLOW_COPY_AND_ASSIGN(FenceImpl);
};

}

#endif // LIBGLESV2_RENDERER_FENCEIMPL_H_
