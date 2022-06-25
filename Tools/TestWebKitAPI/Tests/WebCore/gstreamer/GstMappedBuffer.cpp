/*
 * Copyright (C) 2018 Igalia, S.L.
 * Copyright (C) 2018 Metrological Group B.V.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(GSTREAMER)

#include "GStreamerTest.h"
#include "Test.h"
#include <WebCore/GStreamerCommon.h>
#include <WebCore/SharedBuffer.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST_F(GStreamerTest, mappedBufferBasics)
{
    GRefPtr<GstBuffer> buf = adoptGRef(gst_buffer_new_allocate(nullptr, 64, nullptr));
    GstMappedBuffer mappedBuf(buf, GST_MAP_READ);
    ASSERT_TRUE(mappedBuf);
    EXPECT_EQ(mappedBuf.size(), 64);

    GstMappedBuffer mappedBuf2(buf, GST_MAP_READ);
    ASSERT_TRUE(mappedBuf2);
    EXPECT_EQ(mappedBuf, mappedBuf2);
    EXPECT_EQ(buf.get(), mappedBuf);
    EXPECT_EQ(mappedBuf2, buf.get());
}

TEST_F(GStreamerTest, mappedBufferReadSanity)
{
    gpointer memory = g_malloc(16);
    memset(memory, 'x', 16);
    GRefPtr<GstBuffer> buf = adoptGRef(gst_buffer_new_wrapped(memory, 16));
    GstMappedBuffer mappedBuf(buf, GST_MAP_READ);
    ASSERT_TRUE(mappedBuf);
    EXPECT_EQ(mappedBuf.size(), 16);
    EXPECT_EQ(memcmp(memory, mappedBuf.data(), 16), 0);
}

TEST_F(GStreamerTest, mappedBufferWriteSanity)
{
    gpointer memory = g_malloc(16);
    memset(memory, 'x', 16);
    GRefPtr<GstBuffer> buf = adoptGRef(gst_buffer_new_wrapped(memory, 16));

    GstMappedBuffer mappedBuf(buf.get(), GST_MAP_WRITE);
    ASSERT_TRUE(mappedBuf);
    EXPECT_EQ(mappedBuf.size(), 16);
    memset(mappedBuf.data(), 'y', mappedBuf.size());

    EXPECT_EQ(memcmp(memory, mappedBuf.data(), 16), 0);
}

TEST_F(GStreamerTest, mappedBufferDoesNotAddExtraRefs)
{
    // It is important not to ref the passed GStreamer buffers, if we
    // do so, then in the case of writable buffers, they can become
    // unwritable, even though we are the sole owners. We also don't
    // want to take ownership of the buffer from the user-code, since
    // for transformInPlace use-cases, that would break.
    GRefPtr<GstBuffer> buf = adoptGRef(gst_buffer_new());

    ASSERT_EQ(GST_OBJECT_REFCOUNT(buf.get()), 1);

    GstMappedBuffer mappedBuf(buf, GST_MAP_READWRITE);
    ASSERT_TRUE(mappedBuf);

    ASSERT_EQ(GST_OBJECT_REFCOUNT(buf.get()), 1);
}

TEST_F(GStreamerTest, mappedBufferValidityUnmapEarly)
{
    GRefPtr<GstBuffer> buf = adoptGRef(gst_buffer_new());
    GstMappedBuffer mappedBuf(buf, GST_MAP_READ);
    ASSERT_TRUE(mappedBuf);
    mappedBuf.unmapEarly();
    ASSERT_FALSE(mappedBuf);
}

TEST_F(GStreamerTest, mappedOwnedBufferReadSanity)
{
    gpointer memory = g_malloc(16);
    memset(memory, 'x', 16);
    GRefPtr<GstBuffer> buf = adoptGRef(gst_buffer_new_wrapped(memory, 16));
    auto mappedBuf = GstMappedOwnedBuffer::create(buf);
    ASSERT_TRUE(mappedBuf);
    EXPECT_EQ(mappedBuf->size(), 16);
    EXPECT_EQ(memcmp(memory, mappedBuf->data(), 16), 0);
    EXPECT_EQ(memcmp(memory, mappedBuf->createSharedBuffer()->data(), 16), 0);
}

TEST_F(GStreamerTest, mappedOwnedBufferCachesSharedBuffers)
{
    GRefPtr<GstBuffer> buf = adoptGRef(gst_buffer_new_allocate(nullptr, 64, nullptr));
    auto mappedBuf = GstMappedOwnedBuffer::create(buf);
    ASSERT_TRUE(mappedBuf);
    auto sharedBuf = mappedBuf->createSharedBuffer();
    // We expect the same data pointer wrapped by shared buffer, no
    // copies need to be made.
    EXPECT_EQ(sharedBuf->data(), mappedBuf->createSharedBuffer()->data());
}

TEST_F(GStreamerTest, mappedOwnedBufferDoesAddsExtraRefs)
{
    // GstMappedOwnedBuffer needs to bump the buffer reference count
    // so that it is sure that the buffer outlives the mapped buffer.
    GRefPtr<GstBuffer> buf = adoptGRef(gst_buffer_new());

    ASSERT_EQ(GST_OBJECT_REFCOUNT(buf.get()), 1);

    {
        auto mappedBuf = GstMappedOwnedBuffer::create(buf);
        ASSERT_TRUE(mappedBuf);

        ASSERT_EQ(GST_OBJECT_REFCOUNT(buf.get()), 2);
    }

    ASSERT_EQ(GST_OBJECT_REFCOUNT(buf.get()), 1);
}

TEST_F(GStreamerTest, mappedOwnedBufferDestruction)
{
    GstBuffer* buffer = gst_buffer_new();
    ASSERT_EQ(GST_OBJECT_REFCOUNT(buffer), 1);
    auto mappedBuffer = GstMappedOwnedBuffer::create(buffer);
    ASSERT_TRUE(mappedBuffer);
    ASSERT_EQ(GST_OBJECT_REFCOUNT(buffer), 2);

    // After this we should not use the buffer anymore but it should
    // be still alive inside the mapped buffer, so we still can check
    // the refcount.
    gst_buffer_unref(buffer);
    ASSERT_EQ(GST_OBJECT_REFCOUNT(buffer), 1);

    // Now we are going to check if the buffer really outlives the
    // mapped buffer. For that we check if there is no stderr output
    // when we unref the mapped buffer. We can't check the ref count
    // because it should be 0 and the object should be invalid, what
    // could lead to a crash or other bad things.
    char capturedStderrBuffer[BUFSIZ];
    capturedStderrBuffer[0] = 0;
    setbuf(stderr, capturedStderrBuffer);
    mappedBuffer = nullptr;
    setbuf(stderr, nullptr);
    ASSERT_EQ(capturedStderrBuffer[0], 0);
}

} // namespace TestWebKitAPI

#endif // USE(GSTREAMER)
