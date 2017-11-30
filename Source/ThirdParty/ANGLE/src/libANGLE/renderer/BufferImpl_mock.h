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

    MOCK_METHOD5(
        setData,
        gl::Error(const gl::Context *, gl::BufferBinding, const void *, size_t, gl::BufferUsage));
    MOCK_METHOD5(setSubData,
                 gl::Error(const gl::Context *, gl::BufferBinding, const void *, size_t, size_t));
    MOCK_METHOD5(
        copySubData,
        gl::Error(const gl::Context *contextImpl, BufferImpl *, GLintptr, GLintptr, GLsizeiptr));
    MOCK_METHOD3(map, gl::Error(const gl::Context *contextImpl, GLenum, void **));
    MOCK_METHOD5(mapRange,
                 gl::Error(const gl::Context *contextImpl, size_t, size_t, GLbitfield, void **));
    MOCK_METHOD2(unmap, gl::Error(const gl::Context *contextImpl, GLboolean *result));

    MOCK_METHOD6(getIndexRange,
                 gl::Error(const gl::Context *, GLenum, size_t, size_t, bool, gl::IndexRange *));

    MOCK_METHOD0(destructor, void());

  protected:
    gl::BufferState mMockState;
};
}

#endif  // LIBANGLE_RENDERER_BUFFERIMPLMOCK_H_
