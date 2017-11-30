//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// params:
//   Parameter wrapper structs for OpenGL ES. These helpers cache re-used values
//   in entry point routines.

#include "libANGLE/params.h"

#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/VertexArray.h"

namespace gl
{

// static
constexpr ParamTypeInfo ParamsBase::TypeInfo;
constexpr ParamTypeInfo HasIndexRange::TypeInfo;

HasIndexRange::HasIndexRange()
    : ParamsBase(nullptr), mContext(nullptr), mCount(0), mType(GL_NONE), mIndices(nullptr)
{
}

HasIndexRange::HasIndexRange(Context *context, GLsizei count, GLenum type, const void *indices)
    : ParamsBase(context), mContext(context), mCount(count), mType(type), mIndices(indices)
{
}

const Optional<IndexRange> &HasIndexRange::getIndexRange() const
{
    if (mIndexRange.valid() || !mContext)
    {
        return mIndexRange;
    }

    const State &state = mContext->getGLState();

    const gl::VertexArray *vao     = state.getVertexArray();
    gl::Buffer *elementArrayBuffer = vao->getElementArrayBuffer().get();

    if (elementArrayBuffer)
    {
        uintptr_t offset = reinterpret_cast<uintptr_t>(mIndices);
        IndexRange indexRange;
        Error error =
            elementArrayBuffer->getIndexRange(mContext, mType, static_cast<size_t>(offset), mCount,
                                              state.isPrimitiveRestartEnabled(), &indexRange);
        if (error.isError())
        {
            mContext->handleError(error);
            return mIndexRange;
        }

        mIndexRange = indexRange;
    }
    else
    {
        mIndexRange = ComputeIndexRange(mType, mIndices, mCount, state.isPrimitiveRestartEnabled());
    }

    return mIndexRange;
}

}  // namespace gl
