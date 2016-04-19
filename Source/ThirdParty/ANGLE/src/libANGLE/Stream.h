//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Stream.h: Defines the egl::Stream class, representing the stream
// where frames are streamed in. Implements EGLStreanKHR.

#ifndef LIBANGLE_STREAM_H_
#define LIBANGLE_STREAM_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "common/angleutils.h"
#include "libANGLE/AttributeMap.h"

namespace rx
{
class StreamImpl;
}

namespace egl
{

class Stream final : angle::NonCopyable
{
  public:
    Stream(rx::StreamImpl *impl, const AttributeMap &attribs);
    ~Stream();

    EGLenum getState() const;

    void setConsumerLatency(EGLint latency);
    EGLint getConsumerLatency() const;

    EGLuint64KHR getProducerFrame() const;
    EGLuint64KHR getConsumerFrame() const;

    void setConsumerAcquireTimeout(EGLint timeout);
    EGLint getConsumerAcquireTimeout() const;

  private:
    // Implementation
    rx::StreamImpl *mImplementation;

    // EGL defined attributes
    EGLint mState;
    EGLuint64KHR mProducerFrame;
    EGLuint64KHR mConsumerFrame;
    EGLint mConsumerLatency;

    // EGL gltexture consumer attributes
    EGLint mConsumerAcquireTimeout;
};
}  // namespace egl

#endif  // LIBANGLE_STREAM_H_
