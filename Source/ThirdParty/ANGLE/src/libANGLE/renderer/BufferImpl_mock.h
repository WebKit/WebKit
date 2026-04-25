//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BufferImpl_mock.h: Defines a mock of the BufferImpl class.

#ifndef LIBANGLE_RENDERER_BUFFERIMPLMOCK_H_
#define LIBANGLE_RENDERER_BUFFERIMPLMOCK_H_

#include "gmock/gmock.h"

#include "libANGLE/Buffer.h"
#include "libANGLE/renderer/BufferImpl.h"

namespace rx
{
class MockBufferImpl : public BufferImpl
{
  public:
    MockBufferImpl() : BufferImpl(mMockState) {}
    ~MockBufferImpl() { destructor(); }

    MOCK_METHOD6(setData,
                 angle::Result(const gl::Context *,
                               gl::BufferBinding,
                               const void *,
                               size_t,
                               gl::BufferUsage,
                               BufferFeedback *));
    MOCK_METHOD6(setSubData,
                 angle::Result(const gl::Context *,
                               gl::BufferBinding,
                               const void *,
                               size_t,
                               size_t,
                               BufferFeedback *));
    MOCK_METHOD6(copySubData,
                 angle::Result(const gl::Context *contextImpl,
                               BufferImpl *,
                               GLintptr,
                               GLintptr,
                               GLsizeiptr,
                               BufferFeedback *));
    MOCK_METHOD4(map,
                 angle::Result(const gl::Context *contextImpl, GLenum, void **, BufferFeedback *));
    MOCK_METHOD6(mapRange,
                 angle::Result(const gl::Context *contextImpl,
                               size_t,
                               size_t,
                               GLbitfield,
                               void **,
                               BufferFeedback *));
    MOCK_METHOD3(unmap,
                 angle::Result(const gl::Context *contextImpl,
                               GLboolean *result,
                               BufferFeedback *));

    MOCK_METHOD6(getIndexRange,
                 angle::Result(const gl::Context *,
                               gl::DrawElementsType,
                               size_t,
                               size_t,
                               bool,
                               gl::IndexRange *));

    MOCK_METHOD0(destructor, void());

  protected:
    gl::BufferState mMockState;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_BUFFERIMPLMOCK_H_
