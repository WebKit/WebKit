//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMMON_MEMORYBUFFER_H_
#define COMMON_MEMORYBUFFER_H_

#include "common/angleutils.h"

#include <cstddef>
#include <stdint.h>

namespace rx
{

class MemoryBuffer : angle::NonCopyable
{
  public:
    MemoryBuffer();
    ~MemoryBuffer();

    bool resize(size_t size);
    size_t size() const;
    bool empty() const { return mSize == 0; }

    const uint8_t *data() const;
    uint8_t *data();

  private:
    size_t mSize;
    uint8_t *mData;
};

}

#endif // COMMON_MEMORYBUFFER_H_
