/*
 * Copyright (C) 2023 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GStreamerElementHarness.h"

#if USE(GSTREAMER)

#include "GStreamerCommon.h"

#include <wtf/PrintStream.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_element_harness_debug);
#define GST_CAT_DEFAULT webkit_element_harness_debug

static GstStaticPadTemplate s_harnessSrcPadTemplate = GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);
static GstStaticPadTemplate s_harnessSinkPadTemplate = GST_STATIC_PAD_TEMPLATE("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

/*
 * GStreamerElementHarness acts as a black box for a given GstElement, deterministically feeding it
 * data, and controlling what data it outputs. Its design is similar to GstHarness, which is an
 * API meant for unit-testing elements. GstHarness cannot be reused as-is because it has various
 * release-assertions.
 *
 * The following comments are adapted from the GstHarness documentation.
 *
 * The basic structure of GStreamerElementHarness is two "floating" pads that connect to the
 * harnessed GstElement src and sink pads like so:
 *
 *           __________________________
 *  _____   |  _____            _____  |   _____
 * |     |  | |     |          |     | |  |     |
 * | src |--+-| sink|  Element | src |-+--| sink|
 * |_____|  | |_____|          |_____| |  |_____|
 *          |__________________________|
 *
 *
 * With this, you can now simulate any environment the GstElement might find itself in.
 *
 * The usual workflow is that you push samples or events to the src pad and afterwards you pull
 * buffers (or events) from the sink pad. This is supported through the `pushSample()`,
 * `pushBuffer()` and `pushEvent()` methods. If you can´t use the `pushSample()` method you need to
 * explicitly call the `start(caps)` method, otherwise the sample-based `pushSample()` API will
 * implicitely take care of starting the harness.
 *
 * Output buffers and events can be manually pulled on the corresponding
 * `GStreamerElementHarness::Stream` using the `pullBuffer()` and `pullEvent()` methods. The list of
 * output streams can be queried with the `GStreamerElementHarness::outputStreams()` method.
 *
 * The harness can work on elements exposing either a static source pad, or one-to-many "sometimes"
 * source pads. Support for different topologies can be added as-needed.
 */

GStreamerElementHarness::GStreamerElementHarness(GRefPtr<GstElement>&& element, ProcessBufferCallback&& processOutputBufferCallback, std::optional<PadLinkCallback>&& padLinkCallback)
    : m_element(WTFMove(element))
    , m_processOutputBufferCallback(WTFMove(processOutputBufferCallback))
    , m_padLinkCallback(WTFMove(padLinkCallback))
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_element_harness_debug, "webkitelementharness", 0, "WebKit Element Harness");
    });

    auto clock = adoptGRef(gst_system_clock_obtain());
    gst_element_set_clock(m_element.get(), clock.get());

    bool hasSometimesSrcPad = false;
    for (auto* item = gst_element_class_get_pad_template_list(GST_ELEMENT_GET_CLASS(m_element.get())); item; item = g_list_next(item)) {
        auto* padTemplate = GST_PAD_TEMPLATE(item->data);
        if (GST_PAD_TEMPLATE_DIRECTION(padTemplate) == GST_PAD_SRC && GST_PAD_TEMPLATE_PRESENCE(padTemplate) == GST_PAD_SOMETIMES) {
            hasSometimesSrcPad = true;
            break;
        }
    }

    if (hasSometimesSrcPad) {
        GST_DEBUG_OBJECT(m_element.get(), "Expecting output buffers on sometimes src pad(s).");
        g_signal_connect(m_element.get(), "pad-added", reinterpret_cast<GCallback>(+[](GstElement* element, GstPad* pad, gpointer userData) {
            GST_DEBUG_OBJECT(element, "Pad added: %" GST_PTR_FORMAT, pad);
            auto& harness = *reinterpret_cast<GStreamerElementHarness*>(userData);
            RefPtr<GStreamerElementHarness> downstreamHarness;
            GRefPtr<GstPad> outputPad(pad);
            if (harness.m_padLinkCallback) {
                downstreamHarness = harness.m_padLinkCallback.value()(outputPad);
                if (!downstreamHarness) {
                    GST_DEBUG_OBJECT(element, "Unable to create a Stream for pad %" GST_PTR_FORMAT, pad);
                    return;
                }
            }

            harness.m_outputStreams.append(GStreamerElementHarness::Stream::create(WTFMove(outputPad), WTFMove(downstreamHarness)));
        }), this);

        g_signal_connect(m_element.get(), "pad-removed", reinterpret_cast<GCallback>(+[](GstElement* element, GstPad* pad, gpointer userData) {
            GST_DEBUG_OBJECT(element, "Pad removed: %" GST_PTR_FORMAT, pad);
            auto& harness = *reinterpret_cast<GStreamerElementHarness*>(userData);
            harness.m_outputStreams.removeAllMatching([pad = GRefPtr<GstPad>(pad)](auto& item) -> bool {
                return item->pad() == pad;
            });
        }), this);

    } else {
        GST_DEBUG_OBJECT(m_element.get(), "Expecting output buffers on static src pad.");
        auto elementSrcPad = adoptGRef(gst_element_get_static_pad(m_element.get(), "src"));
        auto stream = GStreamerElementHarness::Stream::create(WTFMove(elementSrcPad), nullptr);
        m_outputStreams.append(WTFMove(stream));
    }

    m_srcPad = gst_pad_new_from_static_template(&s_harnessSrcPadTemplate, "src");
    gst_pad_set_query_function_full(m_srcPad.get(), reinterpret_cast<GstPadQueryFunction>(+[](GstPad* pad, GstObject* parent, GstQuery* query) -> gboolean {
        auto& harness = *reinterpret_cast<GStreamerElementHarness*>(pad->querydata);
        return harness.srcQuery(pad, parent, query);
    }), this, nullptr);
    gst_pad_set_event_function_full(m_srcPad.get(), reinterpret_cast<GstPadEventFunction>(+[](GstPad* pad, GstObject*, GstEvent* event) -> gboolean {
        auto& harness = *reinterpret_cast<GStreamerElementHarness*>(pad->eventdata);
        return harness.srcEvent(event);
    }), this, nullptr);

    gst_pad_set_active(m_srcPad.get(), TRUE);
    auto elementSinkPad = adoptGRef(gst_element_get_static_pad(m_element.get(), "sink"));
    gst_pad_link(m_srcPad.get(), elementSinkPad.get());
}

GStreamerElementHarness::~GStreamerElementHarness()
{
    GST_DEBUG_OBJECT(m_element.get(), "Stopping harness");
    gst_pad_set_active(m_srcPad.get(), FALSE);
    {
        auto streamLock = GstPadStreamLocker(m_srcPad.get());
        gst_pad_set_event_function(m_srcPad.get(), nullptr);
        gst_pad_set_query_function(m_srcPad.get(), nullptr);
    }

    m_outputStreams.clear();

    gst_element_set_state(m_element.get(), GST_STATE_NULL);
}

void GStreamerElementHarness::start(GRefPtr<GstCaps>&& inputCaps)
{
    if (m_playing.load())
        return;

    GST_DEBUG_OBJECT(m_element.get(), "Starting harness");
    gst_element_set_state(m_element.get(), GST_STATE_PLAYING);
    gst_element_get_state(m_element.get(), nullptr, nullptr, GST_CLOCK_TIME_NONE);

    static Atomic<uint64_t> uniqueStreamId;
    auto streamId = makeString(GST_OBJECT_NAME(m_element.get()), '-', uniqueStreamId.exchangeAdd(1));
    pushEvent(adoptGRef(gst_event_new_stream_start(streamId.ascii().data())));

    pushStickyEvents(WTFMove(inputCaps));
    m_playing.store(true);
}

void GStreamerElementHarness::pushStickyEvents(GRefPtr<GstCaps>&& inputCaps)
{
    if (!m_inputCaps || !gst_caps_is_equal(inputCaps.get(), m_inputCaps.get())) {
        m_inputCaps = WTFMove(inputCaps);
        pushEvent(adoptGRef(gst_event_new_caps(m_inputCaps.get())));
    } else if (m_stickyEventsSent.load()) {
        GST_DEBUG_OBJECT(m_element.get(), "Input caps have not changed, not pushing sticky events again");
        return;
    }

    GstSegment segment;
    gst_segment_init(&segment, GST_FORMAT_TIME);
    pushEvent(adoptGRef(gst_event_new_segment(&segment)));

    m_stickyEventsSent.store(true);
}

bool GStreamerElementHarness::pushSample(GRefPtr<GstSample>&& sample)
{
    GRefPtr<GstCaps> caps = gst_sample_get_caps(sample.get());
    GST_TRACE_OBJECT(m_element.get(), "Pushing sample with caps %" GST_PTR_FORMAT, caps.get());
    if (!m_playing.load())
        start(WTFMove(caps));
    else {
        auto currentCaps = adoptGRef(gst_pad_get_current_caps(m_srcPad.get()));
        GST_TRACE_OBJECT(m_element.get(), "Current caps: %" GST_PTR_FORMAT, currentCaps.get());
        if (!currentCaps || gst_pad_needs_reconfigure(m_srcPad.get()))
            pushStickyEvents(WTFMove(caps));
    }
    GRefPtr<GstBuffer> buffer = gst_sample_get_buffer(sample.get());
    return pushBuffer(WTFMove(buffer));
}

bool GStreamerElementHarness::pushBuffer(GRefPtr<GstBuffer>&& buffer)
{
    if (!m_stickyEventsSent.load())
        return false;

    auto result = pushBufferFull(WTFMove(buffer));
    return result == GST_FLOW_OK || result == GST_FLOW_EOS;
}

GstFlowReturn GStreamerElementHarness::pushBufferFull(GRefPtr<GstBuffer>&& buffer)
{
    GST_TRACE_OBJECT(m_element.get(), "Pushing %" GST_PTR_FORMAT, buffer.get());
    auto result = gst_pad_push(m_srcPad.get(), buffer.leakRef());
    GST_TRACE_OBJECT(m_element.get(), "Buffer push result: %s", gst_flow_get_name(result));
    return result;
}

bool GStreamerElementHarness::pushEvent(GRefPtr<GstEvent>&& event)
{
    GST_TRACE_OBJECT(m_element.get(), "Pushing %" GST_PTR_FORMAT, event.get());
    auto result = gst_pad_push_event(m_srcPad.get(), event.leakRef());
    GST_TRACE_OBJECT(m_element.get(), "Result: %s", boolForPrinting(result));
    return result;
}

GStreamerElementHarness::Stream::Stream(GRefPtr<GstPad>&& pad, RefPtr<GStreamerElementHarness>&& downstreamHarness)
    : m_pad(WTFMove(pad))
    , m_downstreamHarness(WTFMove(downstreamHarness))
{
    m_targetPad = gst_pad_new_from_static_template(&s_harnessSinkPadTemplate, "sink");
    gst_pad_link(m_pad.get(), m_targetPad.get());

    gst_pad_set_chain_function_full(m_targetPad.get(), reinterpret_cast<GstPadChainFunction>(+[](GstPad* pad, GstObject*, GstBuffer* buffer) -> GstFlowReturn {
        auto& stream = *reinterpret_cast<GStreamerElementHarness::Stream*>(pad->chaindata);
        if (stream.m_downstreamHarness)
            return stream.m_downstreamHarness->pushBufferFull(buffer);
        return stream.chainBuffer(buffer);
    }),  this, nullptr);
    gst_pad_set_event_function_full(m_targetPad.get(), reinterpret_cast<GstPadEventFunction>(+[](GstPad* pad, GstObject*, GstEvent* event) -> gboolean {
        auto& stream = *reinterpret_cast<GStreamerElementHarness::Stream*>(pad->eventdata);
        return stream.sinkEvent(event);
    }), this, nullptr);

    gst_pad_set_active(m_targetPad.get(), TRUE);
}

GStreamerElementHarness::Stream::~Stream()
{
    gst_pad_set_active(m_targetPad.get(), FALSE);

    auto streamLock = GstPadStreamLocker(m_targetPad.get());
    gst_pad_set_chain_function(m_targetPad.get(), nullptr);
    gst_pad_set_event_function(m_targetPad.get(), nullptr);
    gst_pad_set_query_function(m_targetPad.get(), nullptr);
}

GRefPtr<GstBuffer> GStreamerElementHarness::Stream::pullBuffer()
{
    GST_LOG_OBJECT(m_pad.get(), "%zu buffers currently queued", m_bufferQueue.size());
    Locker locker { m_bufferQueueLock };
    if (m_bufferQueue.isEmpty())
        return nullptr;
    return m_bufferQueue.takeLast();
}

GRefPtr<GstEvent> GStreamerElementHarness::Stream::pullEvent()
{
    Locker locker { m_sinkEventQueueLock };
    if (m_sinkEventQueue.isEmpty())
        return nullptr;
    return m_sinkEventQueue.takeLast();
}

bool GStreamerElementHarness::Stream::sendEvent(GstEvent* event)
{
    GST_TRACE_OBJECT(m_pad.get(), "Sending %" GST_PTR_FORMAT, event);
    auto result = gst_pad_send_event(m_pad.get(), event);
    GST_TRACE_OBJECT(m_pad.get(), "Result: %s", boolForPrinting(result));
    return result;
}

const GRefPtr<GstCaps>& GStreamerElementHarness::Stream::outputCaps()
{
    if (m_outputCaps)
        return m_outputCaps;

    m_outputCaps = adoptGRef(gst_pad_get_current_caps(m_pad.get()));
    GST_DEBUG_OBJECT(m_pad.get(), "Output caps: %" GST_PTR_FORMAT, m_outputCaps.get());
    return m_outputCaps;
}

GstFlowReturn GStreamerElementHarness::Stream::chainBuffer(GstBuffer* outputBuffer)
{
    Locker locker { m_bufferQueueLock };
    auto buffer = adoptGRef(outputBuffer);
    m_bufferQueue.prepend(WTFMove(buffer));
    return GST_FLOW_OK;
}

bool GStreamerElementHarness::Stream::sinkEvent(GstEvent* event)
{
    Locker locker { m_sinkEventQueueLock };
    m_sinkEventQueue.prepend(GRefPtr<GstEvent>(event));
    return true;
}

bool GStreamerElementHarness::srcQuery(GstPad* pad, GstObject* parent, GstQuery* query)
{
    bool result = TRUE;
    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_CAPS: {
        GstCaps* filter = nullptr;
        GRefPtr<GstCaps> caps;
        if (m_inputCaps)
            caps = m_inputCaps;
        else
            caps = adoptGRef(gst_pad_get_pad_template_caps(pad));

        gst_query_parse_caps(query, &filter);
        if (filter) {
            auto intersectedCaps = adoptGRef(gst_caps_intersect_full(filter, caps.get(), GST_CAPS_INTERSECT_FIRST));
            gst_query_set_caps_result(query, intersectedCaps.get());
        } else
            gst_query_set_caps_result(query, caps.get());
        break;
    }
    default:
        result = gst_pad_query_default(pad, parent, query);
    }

    return result;
}

bool GStreamerElementHarness::srcEvent(GstEvent* event)
{
    GST_TRACE_OBJECT(m_element.get(), "Got event on src pad: %" GST_PTR_FORMAT, event);
    Locker locker { m_srcEventQueueLock };
    m_srcEventQueue.prepend(GRefPtr<GstEvent>(event));
    return true;
}

void GStreamerElementHarness::processOutputBuffers()
{
    for (auto& stream : m_outputStreams) {
        while (auto outputBuffer = stream->pullBuffer())
            m_processOutputBufferCallback(*stream.get(), outputBuffer);
    }
}

void GStreamerElementHarness::flush()
{
    GST_DEBUG_OBJECT(element(), "Flushing");

    if (element()->current_state <= GST_STATE_PAUSED) {
        GST_DEBUG_OBJECT(element(), "No need to flush in paused state");
        return;
    }

    processOutputBuffers();

    pushEvent(adoptGRef(gst_event_new_flush_start()));
    pushEvent(adoptGRef(gst_event_new_flush_stop(FALSE)));

    for (auto& stream : m_outputStreams) {
        bool flushReceived = false;
        while (!flushReceived) {
            auto event = stream->pullEvent();
            flushReceived = event && GST_EVENT_TYPE(event.get()) == GST_EVENT_FLUSH_STOP;
        }
    }

    m_inputCaps.clear();
    m_stickyEventsSent.store(false);
    GST_DEBUG_OBJECT(element(), "Flushing done");
}

} // namespace WebCore

#endif // USE(GSTREAMER)
