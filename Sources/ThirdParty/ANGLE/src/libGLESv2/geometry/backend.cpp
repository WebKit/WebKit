//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// geometry/backend.h: Abstract classes BufferBackEnd, TranslatedVertexBuffer and TranslatedIndexBuffer
// that must be implemented by any API-specific implementation of ANGLE.

#include "libGLESv2/geometry/backend.h"

#include "common/debug.h"

namespace gl
{

void *TranslatedBuffer::map(std::size_t requiredSpace, std::size_t *offset)
{
    ASSERT(requiredSpace <= mBufferSize);

    reserveSpace(requiredSpace);

    *offset = mCurrentPoint;
    mCurrentPoint += requiredSpace;

    return streamingMap(*offset, requiredSpace);
}

void TranslatedBuffer::reserveSpace(std::size_t requiredSpace)
{
    if (mCurrentPoint + requiredSpace > mBufferSize)
    {
        recycle();
        mCurrentPoint = 0;
    }
}

}
