/*
 * Copyright (C) 2023 Igalia, S.L.
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
#include <WebCore/GStreamerElementHarness.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST_F(GStreamerTest, harnessBasic)
{
    GRefPtr<GstElement> element = gst_element_factory_make("identity", nullptr);
    auto harness = WebCore::GStreamerElementHarness::create(WTFMove(element), [](auto&, const auto&) { });

    // The identity element has a single source pad. Fetch the corresponding stream.
    ASSERT_FALSE(harness->outputStreams().isEmpty());
    auto& stream = harness->outputStreams().first();

    // Harness has not started processing data yet.
    ASSERT_NULL(stream->pullBuffer());
    ASSERT_NULL(stream->pullEvent());

    // Push a sample and expect initial events and an output buffer.
    auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, 64, nullptr));
    gst_buffer_memset(buffer.get(), 0, 2, 64);
    GstMappedBuffer mappedInputBuffer(buffer.get(), GST_MAP_READ);
    ASSERT_TRUE(mappedInputBuffer);
    EXPECT_EQ(mappedInputBuffer.size(), 64);
    auto caps = adoptGRef(gst_caps_new_empty_simple("foo"));
    auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
    EXPECT_TRUE(harness->pushSample(sample.get()));

    auto event = stream->pullEvent();
    ASSERT_NOT_NULL(event.get());
    EXPECT_STREQ(GST_EVENT_TYPE_NAME(event.get()), "stream-start");

    event = stream->pullEvent();
    ASSERT_NOT_NULL(event.get());
    EXPECT_STREQ(GST_EVENT_TYPE_NAME(event.get()), "caps");
    GstCaps* eventCaps;
    gst_event_parse_caps(event.get(), &eventCaps);
    ASSERT_TRUE(gst_caps_is_equal(eventCaps, caps.get()));

    event = stream->pullEvent();
    ASSERT_NOT_NULL(event.get());
    EXPECT_STREQ(GST_EVENT_TYPE_NAME(event.get()), "segment");

    ASSERT_TRUE(gst_caps_is_equal(stream->outputCaps().get(), caps.get()));

    // The harnessed element is identity, so output buffers should be the same as input buffers.
    auto outputBuffer = stream->pullBuffer();
    GstMappedBuffer mappedOutputBuffer(outputBuffer.get(), GST_MAP_READ);
    ASSERT_TRUE(mappedOutputBuffer);
    EXPECT_EQ(mappedOutputBuffer.size(), 64);
    EXPECT_EQ(memcmp(mappedInputBuffer.data(), mappedOutputBuffer.data(), 64), 0);

    // Harness is now empty.
    ASSERT_NULL(stream->pullBuffer());
    ASSERT_NULL(stream->pullEvent());
}

TEST_F(GStreamerTest, harnessManualStart)
{
    GRefPtr<GstElement> element = gst_element_factory_make("identity", nullptr);
    auto harness = WebCore::GStreamerElementHarness::create(WTFMove(element), [](auto&, const auto&) { });

    // The identity element has a single source pad. Fetch the corresponding stream.
    ASSERT_FALSE(harness->outputStreams().isEmpty());
    auto& stream = harness->outputStreams().first();

    // Harness has not started processing data yet.
    ASSERT_NULL(stream->pullBuffer());
    ASSERT_NULL(stream->pullEvent());

    // Pushing a buffer before start is not allowed.
    EXPECT_FALSE(harness->pushBuffer(gst_buffer_new_allocate(nullptr, 64, nullptr)));

    // Start the harness and expect initial events.
    auto caps = adoptGRef(gst_caps_new_empty_simple("foo"));
    harness->start(WTFMove(caps));

    auto event = stream->pullEvent();
    ASSERT_NOT_NULL(event.get());
    EXPECT_STREQ(GST_EVENT_TYPE_NAME(event.get()), "stream-start");

    event = stream->pullEvent();
    ASSERT_NOT_NULL(event.get());
    EXPECT_STREQ(GST_EVENT_TYPE_NAME(event.get()), "caps");
    GstCaps* eventCaps;
    gst_event_parse_caps(event.get(), &eventCaps);
    const auto* structure = gst_caps_get_structure(eventCaps, 0);
    ASSERT_STREQ(gst_structure_get_name(structure), "foo");

    event = stream->pullEvent();
    ASSERT_NOT_NULL(event.get());
    EXPECT_STREQ(GST_EVENT_TYPE_NAME(event.get()), "segment");

    // Push a buffer and expect an output buffer.
    auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, 64, nullptr));
    gst_buffer_memset(buffer.get(), 0, 2, 64);
    EXPECT_TRUE(harness->pushBuffer(gst_buffer_ref(buffer.get())));
    GstMappedBuffer mappedInputBuffer(buffer.get(), GST_MAP_READ);
    ASSERT_TRUE(mappedInputBuffer);
    EXPECT_EQ(mappedInputBuffer.size(), 64);

    // The harnessed element is identity, so output buffers should be the same as input buffers.
    auto outputBuffer = stream->pullBuffer();
    GstMappedBuffer mappedOutputBuffer(outputBuffer.get(), GST_MAP_READ);
    ASSERT_TRUE(mappedOutputBuffer);
    EXPECT_EQ(mappedOutputBuffer.size(), 64);
    EXPECT_EQ(memcmp(mappedInputBuffer.data(), mappedOutputBuffer.data(), 64), 0);

    // Harness is now empty.
    ASSERT_NULL(stream->pullBuffer());
    ASSERT_NULL(stream->pullEvent());
}

TEST_F(GStreamerTest, harnessBufferProcessing)
{
    GRefPtr<GstElement> element = gst_element_factory_make("identity", nullptr);
    unsigned counter = 0;
    auto harness = WebCore::GStreamerElementHarness::create(WTFMove(element), [&counter](auto&, const auto& outputBuffer) mutable {
        GstMappedBuffer mappedOutputBuffer(outputBuffer.get(), GST_MAP_READ);
        ASSERT_TRUE(mappedOutputBuffer);
        EXPECT_EQ(mappedOutputBuffer.size(), 64);
        EXPECT_EQ(mappedOutputBuffer.data()[0], counter);
        counter++;
    });

    // The identity element has a single source pad. Fetch the corresponding stream.
    ASSERT_FALSE(harness->outputStreams().isEmpty());
    auto& stream = harness->outputStreams().first();

    // Harness has not started processing data yet.
    ASSERT_NULL(stream->pullBuffer());
    ASSERT_NULL(stream->pullEvent());
    ASSERT_EQ(counter, 0);

    // Push a batch of samples and expect they were all processed using the supplied harness callback.
    auto caps = adoptGRef(gst_caps_new_empty_simple("foo"));
    for (unsigned i = 0; i < 3; i++) {
        auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, 64, nullptr));
        gst_buffer_memset(buffer.get(), 0, i, 64);
        auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
        EXPECT_TRUE(harness->pushSample(sample.get()));
    }
    harness->processOutputBuffers();
    ASSERT_EQ(counter, 3);
}

TEST_F(GStreamerTest, harnessFlush)
{
    GRefPtr<GstElement> element = gst_element_factory_make("identity", nullptr);
    unsigned counter = 0;
    auto harness = WebCore::GStreamerElementHarness::create(WTFMove(element), [&counter](auto&, const auto& outputBuffer) mutable {
        GstMappedBuffer mappedOutputBuffer(outputBuffer.get(), GST_MAP_READ);
        ASSERT_TRUE(mappedOutputBuffer);
        EXPECT_EQ(mappedOutputBuffer.size(), 64);
        EXPECT_EQ(mappedOutputBuffer.data()[0], 2);
        counter++;
    });

    // The identity element has a single source pad. Fetch the corresponding stream.
    ASSERT_FALSE(harness->outputStreams().isEmpty());
    auto& stream = harness->outputStreams().first();

    // Harness has not started processing data yet.
    ASSERT_NULL(stream->pullBuffer());
    ASSERT_NULL(stream->pullEvent());
    ASSERT_EQ(counter, 0);

    // Push a batch of 3 samples, manually process the first two output buffers and flush. The last
    // sample should be processed during the flush using the harness callback.
    auto caps = adoptGRef(gst_caps_new_empty_simple("foo"));
    for (unsigned i = 0; i < 3; i++) {
        auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, 64, nullptr));
        gst_buffer_memset(buffer.get(), 0, i, 64);
        auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
        EXPECT_TRUE(harness->pushSample(sample.get()));
    }
    auto firstOutputBuffer = stream->pullBuffer();
    GstMappedBuffer mappedFirstOutputBuffer(firstOutputBuffer.get(), GST_MAP_READ);
    ASSERT_TRUE(mappedFirstOutputBuffer);
    EXPECT_EQ(mappedFirstOutputBuffer.size(), 64);
    EXPECT_EQ(mappedFirstOutputBuffer.data()[0], 0);

    auto secondOutputBuffer = stream->pullBuffer();
    GstMappedBuffer mappedSecondOutputBuffer(secondOutputBuffer.get(), GST_MAP_READ);
    ASSERT_TRUE(mappedSecondOutputBuffer);
    EXPECT_EQ(mappedSecondOutputBuffer.size(), 64);
    EXPECT_EQ(mappedSecondOutputBuffer.data()[0], 1);

    harness->flush();
    ASSERT_EQ(counter, 1);

    // Flushed harness is empty.
    ASSERT_NULL(stream->pullBuffer());
    ASSERT_NULL(stream->pullEvent());
}

} // namespace TestWebKitAPI

#endif // USE(GSTREAMER)
