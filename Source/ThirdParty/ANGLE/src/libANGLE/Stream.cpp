//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Stream.cpp: Implements the egl::Stream class, representing the stream
// where frames are streamed in. Implements EGLStreanKHR.

#include "libANGLE/Stream.h"

#include <platform/Platform.h>
#include <EGL/eglext.h>

#include "common/debug.h"
#include "common/mathutil.h"
#include "common/platform.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/StreamImpl.h"

namespace egl
{

Stream::Stream(rx::StreamImpl *impl, const AttributeMap &attribs)
    : mImplementation(impl),
      mState(EGL_STREAM_STATE_CREATED_KHR),
      mProducerFrame(0),
      mConsumerFrame(0),
      mConsumerLatency(static_cast<EGLint>(attribs.get(EGL_CONSUMER_LATENCY_USEC_KHR, 0))),
      mConsumerAcquireTimeout(
          static_cast<EGLint>(attribs.get(EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, 0)))
{
}

Stream::~Stream()
{
    SafeDelete(mImplementation);
}

void Stream::setConsumerLatency(EGLint latency)
{
    mConsumerLatency = latency;
}

EGLint Stream::getConsumerLatency() const
{
    return mConsumerLatency;
}

EGLuint64KHR Stream::getProducerFrame() const
{
    return mProducerFrame;
}

EGLuint64KHR Stream::getConsumerFrame() const
{
    return mConsumerFrame;
}

EGLenum Stream::getState() const
{
    return mState;
}

void Stream::setConsumerAcquireTimeout(EGLint timeout)
{
    mConsumerAcquireTimeout = timeout;
}

EGLint Stream::getConsumerAcquireTimeout() const
{
    return mConsumerAcquireTimeout;
}

}  // namespace egl
