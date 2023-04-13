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
#include <wtf/FileSystem.h>

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
    EXPECT_TRUE(harness->pushSample(WTFMove(sample)));

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
    EXPECT_FALSE(harness->pushBuffer(adoptGRef(gst_buffer_new_allocate(nullptr, 64, nullptr))));

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
    GstMappedBuffer mappedInputBuffer(buffer.get(), GST_MAP_READ);
    ASSERT_TRUE(mappedInputBuffer);
    EXPECT_EQ(mappedInputBuffer.size(), 64);
    EXPECT_TRUE(harness->pushBuffer(GRefPtr<GstBuffer>(buffer.get())));

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
        EXPECT_TRUE(harness->pushSample(WTFMove(sample)));
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
        EXPECT_TRUE(harness->pushSample(WTFMove(sample)));
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

TEST_F(GStreamerTest, harnessParseMP4)
{
    GRefPtr<GstElement> element = gst_element_factory_make("parsebin", nullptr);
    struct BufferTracker {
        uint64_t totalAudioBuffers { 0 };
        uint64_t totalVideoBuffers { 0 };
    } bufferTracker;
    auto harness = WebCore::GStreamerElementHarness::create(WTFMove(element), [&bufferTracker](auto& stream, const auto&) mutable {
        auto gstStream = adoptGRef(gst_pad_get_stream(stream.pad().get()));
        auto streamType = gst_stream_get_stream_type(gstStream.get());
        if (streamType == GST_STREAM_TYPE_AUDIO)
            bufferTracker.totalAudioBuffers++;
        else if (streamType == GST_STREAM_TYPE_VIDEO)
            bufferTracker.totalVideoBuffers++;
        else {
            ASSERT_NOT_REACHED();
            return;
        }
    });

    // Feed the contents of a MP4 file to the harnessed parsebin.
    GUniquePtr<char> filePath(g_build_filename(WEBKIT_SRC_DIR, "Tools", "TestWebKitAPI", "Tests", "WebKit", "test.mp4", nullptr));
    auto handle = FileSystem::openFile(makeString(filePath.get()), FileSystem::FileOpenMode::Read);

    size_t totalRead = 0;
    auto size = FileSystem::fileSize(handle).value_or(0);
    auto caps = adoptGRef(gst_caps_new_empty_simple("video/quicktime"));
    while (totalRead < size) {
        size_t bytesToRead = 1024;
        if (totalRead + bytesToRead > size)
            bytesToRead = size - totalRead;

        auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, bytesToRead, nullptr));
        {
            GstMappedBuffer mappedBuffer(buffer.get(), GST_MAP_WRITE);
            FileSystem::readFromFile(handle, mappedBuffer.data(), bytesToRead);
        }
        auto sample = adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr));
        EXPECT_TRUE(harness->pushSample(WTFMove(sample)));

        totalRead += bytesToRead;
    }

    // The entire file was processed by parsebin, the output streams should have been created. Check
    // the first events on each.
    EXPECT_EQ(harness->outputStreams().size(), 2);
    for (auto& stream : harness->outputStreams()) {
        auto event = stream->pullEvent();
        ASSERT_NOT_NULL(event.get());
        EXPECT_STREQ(GST_EVENT_TYPE_NAME(event.get()), "stream-start");

        event = stream->pullEvent();
        ASSERT_NOT_NULL(event.get());
        EXPECT_STREQ(GST_EVENT_TYPE_NAME(event.get()), "caps");
        GstCaps* eventCaps;
        gst_event_parse_caps(event.get(), &eventCaps);
        ASSERT_TRUE(gst_caps_is_equal(stream->outputCaps().get(), eventCaps));

        event = stream->pullEvent();
        ASSERT_NOT_NULL(event.get());
        EXPECT_STREQ(GST_EVENT_TYPE_NAME(event.get()), "stream-collection");
        GstStreamCollection* collection;
        gst_event_parse_stream_collection(event.get(), &collection);
        ASSERT_EQ(gst_stream_collection_get_size(collection), 2);
    }

    // We haven't pulled any buffer yet, so our buffer tracker should report empty metrics.
    ASSERT_EQ(bufferTracker.totalAudioBuffers, 0);
    ASSERT_EQ(bufferTracker.totalVideoBuffers, 0);

    // Flush all internal queues, the harness should trigger our buffer tracker now.
    harness->flush();
    ASSERT_EQ(bufferTracker.totalAudioBuffers, 260);
    ASSERT_EQ(bufferTracker.totalVideoBuffers, 182);
}

TEST_F(GStreamerTest, harnessDecodeMP4Video)
{
    // Process an MP4 file, expecting non-video streams will be discarded.
    GRefPtr<GstElement> element = gst_element_factory_make("decodebin3", nullptr);
    struct BufferTracker {
        uint64_t totalVideoBuffers { 0 };
    } bufferTracker;
    auto harness = WebCore::GStreamerElementHarness::create(WTFMove(element), [&bufferTracker](auto& stream, const auto&) mutable {
        auto gstStream = adoptGRef(gst_pad_get_stream(stream.pad().get()));
        auto streamType = gst_stream_get_stream_type(gstStream.get());
        if (streamType == GST_STREAM_TYPE_VIDEO)
            bufferTracker.totalVideoBuffers++;
        else {
            ASSERT_NOT_REACHED();
            return;
        }
    });

    auto caps = adoptGRef(gst_caps_new_empty_simple("video/quicktime"));
    harness->start(WTFMove(caps));

    // Feed the contents of a MP4 file to the harnessed decodebin3, until it is able to figure out
    // the stream topology.
    GUniquePtr<char> filePath(g_build_filename(WEBKIT_SRC_DIR, "Tools", "TestWebKitAPI", "Tests", "WebKit", "test.mp4", nullptr));
    auto handle = FileSystem::openFile(makeString(filePath.get()), FileSystem::FileOpenMode::Read);

    size_t totalRead = 0;
    auto size = FileSystem::fileSize(handle).value_or(0);
    GstStreamCollection* collection = nullptr;
    RefPtr<GStreamerElementHarness::Stream> harnessedStream;
    while (totalRead < size) {
        size_t bytesToRead = 512;
        if (totalRead + bytesToRead > size)
            bytesToRead = size - totalRead;

        auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, bytesToRead, nullptr));
        {
            GstMappedBuffer mappedBuffer(buffer.get(), GST_MAP_WRITE);
            FileSystem::readFromFile(handle, mappedBuffer.data(), bytesToRead);
        }
        EXPECT_TRUE(harness->pushBuffer(WTFMove(buffer)));
        totalRead += bytesToRead;

        // Process events, if a stream collection is received, interrupt the buffer feed.
        for (auto& stream : harness->outputStreams()) {
            while (auto event = stream->pullEvent()) {
                if (GST_EVENT_TYPE(event.get()) == GST_EVENT_STREAM_COLLECTION) {
                    gst_event_parse_stream_collection(event.get(), &collection);
                    harnessedStream = stream;
                    break;
                }
            }
            if (collection)
                break;
        }

        if (collection)
            break;
    }

    // Process the stream collection, discard all non-video streams.
    unsigned collectionSize = gst_stream_collection_get_size(collection);
    GList* streams = nullptr;
    for (unsigned i = 0; i < collectionSize; i++) {
        auto* stream = gst_stream_collection_get_stream(collection, i);
        auto streamType = gst_stream_get_stream_type(stream);
        if (streamType == GST_STREAM_TYPE_VIDEO) {
            streams = g_list_append(streams, const_cast<char*>(gst_stream_get_stream_id(stream)));
            break;
        }
    }

    // Instruct decodebin3 to reconfigure according to the streams we selected.
    if (streams) {
        harnessedStream->sendEvent(gst_event_new_select_streams(streams));
        g_list_free(streams);
    }

    // Feed the remaining bytes of the MP4 file to decodebin3.
    while (totalRead < size) {
        size_t bytesToRead = 512;
        if (totalRead + bytesToRead > size)
            bytesToRead = size - totalRead;

        auto buffer = adoptGRef(gst_buffer_new_allocate(nullptr, bytesToRead, nullptr));
        {
            GstMappedBuffer mappedBuffer(buffer.get(), GST_MAP_WRITE);
            FileSystem::readFromFile(handle, mappedBuffer.data(), bytesToRead);
        }
        EXPECT_TRUE(harness->pushBuffer(WTFMove(buffer)));

        totalRead += bytesToRead;
    }

    // The entire file was processed by decodebin3, the output streams should have been created. We
    // should only have one output stream at this point.
    EXPECT_EQ(harness->outputStreams().size(), 1);

    // We haven't pulled any buffer yet, so our buffer tracker should report empty metrics.
    ASSERT_EQ(bufferTracker.totalVideoBuffers, 0);

    // Flush all internal queues, the harness should trigger our buffer tracker now.
    harness->flush();
    ASSERT_GT(bufferTracker.totalVideoBuffers, 100);
}

} // namespace TestWebKitAPI

#endif // USE(GSTREAMER)
