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

#include <wtf/FileSystem.h>
#include <wtf/PrintStream.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>
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
 * Output samples and events can be manually pulled on the corresponding
 * `GStreamerElementHarness::Stream` using the `pullSample()` and `pullEvent()` methods. The list of
 * output streams can be queried with the `GStreamerElementHarness::outputStreams()` method.
 *
 * The harness can work on elements exposing either a static source pad, or one-to-many "sometimes"
 * source pads. Support for different topologies can be added as-needed.
 *
 * In cases where a graph dump of the harness and its downstream harnesses is needed for debugging
 * purposes, it can be done by calling the `dumpGraph()` method. At runtime you need to set the
 * `$WEBKIT_GST_HARNESS_DUMP_DIR` environment variable to a filesystem directory path where `mmd`
 * [Mermaid](https://mermaid.js.org/) files will be created. Those can then be exported to SVG or
 * PNG using the mermaid CLI tools or the [live editor](https://mermaid.live).
 */

GStreamerElementHarness::GStreamerElementHarness(GRefPtr<GstElement>&& element, ProcessSampleCallback&& processOutputSampleCallback, std::optional<PadLinkCallback>&& padLinkCallback)
    : m_element(WTFMove(element))
    , m_processOutputSampleCallback(WTFMove(processOutputSampleCallback))
    , m_padLinkCallback(WTFMove(padLinkCallback))
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_element_harness_debug, "webkitelementharness", 0, "WebKit Element Harness");
    });

    registerActivePipeline(m_element);

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
            harness.dumpGraph("pad-added");
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
        return harness.srcEvent(adoptGRef(event));
    }), this, nullptr);

    gst_pad_set_active(m_srcPad.get(), TRUE);
    auto elementSinkPad = adoptGRef(gst_element_get_static_pad(m_element.get(), "sink"));
    gst_pad_link(m_srcPad.get(), elementSinkPad.get());
}

GStreamerElementHarness::~GStreamerElementHarness()
{
    GST_DEBUG_OBJECT(m_element.get(), "Stopping harness");
    g_signal_handlers_disconnect_by_data(m_element.get(), this);
    pushEvent(adoptGRef(gst_event_new_eos()));
    unregisterPipeline(m_element);
    gst_pad_set_active(m_srcPad.get(), FALSE);
    {
        auto streamLock = GstPadStreamLocker(m_srcPad.get());
        gst_pad_set_event_function(m_srcPad.get(), nullptr);
        gst_pad_set_query_function(m_srcPad.get(), nullptr);
    }

    m_outputStreams.clear();

    gst_element_set_state(m_element.get(), GST_STATE_NULL);
}

void GStreamerElementHarness::start(GRefPtr<GstCaps>&& inputCaps, std::optional<const GstSegment*>&& segment)
{
    if (m_playing.load())
        return;

    GST_DEBUG_OBJECT(m_element.get(), "Starting harness");
    gst_element_set_state(m_element.get(), GST_STATE_PLAYING);
    gst_element_get_state(m_element.get(), nullptr, nullptr, GST_CLOCK_TIME_NONE);

    static Atomic<uint64_t> uniqueStreamId;
    auto streamId = makeString(GST_OBJECT_NAME(m_element.get()), '-', uniqueStreamId.exchangeAdd(1));
    pushEvent(adoptGRef(gst_event_new_stream_start(streamId.ascii().data())));

    pushStickyEvents(WTFMove(inputCaps), WTFMove(segment));
    m_playing.store(true);
}

void GStreamerElementHarness::reset()
{
    if (!m_playing.load())
        return;

    GST_DEBUG_OBJECT(m_element.get(), "Resetting harness");
    pushEvent(adoptGRef(gst_event_new_eos()));
    gst_element_set_state(m_element.get(), GST_STATE_NULL);

    processOutputSamples();

    m_playing.store(false);
}

void GStreamerElementHarness::pushStickyEvents(GRefPtr<GstCaps>&& inputCaps, std::optional<const GstSegment*>&& segment)
{
    if (!m_capsEventSent.load() || !m_inputCaps || !gst_caps_is_equal(inputCaps.get(), m_inputCaps.get())) {
        m_inputCaps = WTFMove(inputCaps);
        GST_DEBUG_OBJECT(m_element.get(), "Signaling downstream with caps %" GST_PTR_FORMAT, m_inputCaps.get());
        pushEvent(adoptGRef(gst_event_new_caps(m_inputCaps.get())));
        m_capsEventSent.store(true);
    }

    pushSegmentEvent(WTFMove(segment));
}

void GStreamerElementHarness::pushSegmentEvent(std::optional<const GstSegment*>&& segment)
{
    if (m_segmentEventSent.load())
        return;

    GstSegment timeSegment;
    gst_segment_init(&timeSegment, GST_FORMAT_TIME);
    pushEvent(adoptGRef(gst_event_new_segment(segment.value_or(&timeSegment))));
    m_segmentEventSent.store(true);
}

bool GStreamerElementHarness::pushSample(GRefPtr<GstSample>&& sample)
{
    GRefPtr<GstCaps> caps = gst_sample_get_caps(sample.get());
    auto segment = gst_sample_get_segment(sample.get());
    GST_TRACE_OBJECT(m_element.get(), "Pushing sample with caps %" GST_PTR_FORMAT, caps.get());
    if (!m_playing.load())
        start(WTFMove(caps), segment);
    else {
        auto currentCaps = adoptGRef(gst_pad_get_current_caps(m_srcPad.get()));
        GST_TRACE_OBJECT(m_element.get(), "Current caps: %" GST_PTR_FORMAT, currentCaps.get());
        if (!currentCaps || gst_pad_needs_reconfigure(m_srcPad.get()) || !m_segmentEventSent.load())
            pushStickyEvents(WTFMove(caps), segment);
    }
    GRefPtr<GstBuffer> buffer = gst_sample_get_buffer(sample.get());
    return pushBuffer(WTFMove(buffer));
}

bool GStreamerElementHarness::pushBuffer(GRefPtr<GstBuffer>&& buffer)
{
    if (!m_capsEventSent.load())
        return false;

    // The segment event might have been cleared after flush-stop, so push another one if necessary.
    // At this level we don't know what was the previous segment format, so assume the default,
    // time.
    pushSegmentEvent();

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
        if (auto downstreamHarness = stream.downstreamHarness()) {
            if (!downstreamHarness->isStarted())
                downstreamHarness->start(GRefPtr<GstCaps>(stream.outputCaps()));
            return downstreamHarness->pushBufferFull(adoptGRef(buffer));
        }

        const auto& caps = stream.outputCaps();
        const GstSegment* segment = nullptr;
        if (auto segmentEvent = adoptGRef(gst_pad_get_sticky_event(stream.pad().get(), GST_EVENT_SEGMENT, 0)))
            gst_event_parse_segment(segmentEvent.get(), &segment);

        auto outputBuffer = adoptGRef(buffer);
        return stream.chainSample(adoptGRef(gst_sample_new(outputBuffer.get(), caps.get(), segment, nullptr)));
    }),  this, nullptr);
    gst_pad_set_event_function_full(m_targetPad.get(), reinterpret_cast<GstPadEventFunction>(+[](GstPad* pad, GstObject*, GstEvent* event) -> gboolean {
        auto& stream = *reinterpret_cast<GStreamerElementHarness::Stream*>(pad->eventdata);
        return stream.sinkEvent(adoptGRef(event));
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

    {
        Locker locker { m_sampleQueueLock };
        m_sampleQueue.clear();
    }
    {
        Locker locker { m_sinkEventQueueLock };
        m_sinkEventQueue.clear();
    }
    m_downstreamHarness = nullptr;
}

GRefPtr<GstSample> GStreamerElementHarness::Stream::pullSample()
{
    GST_LOG_OBJECT(m_pad.get(), "%zu samples currently queued", m_sampleQueue.size());
    Locker locker { m_sampleQueueLock };
    if (m_sampleQueue.isEmpty())
        return nullptr;
    return m_sampleQueue.takeLast();
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
    Locker locker { m_sinkEventQueueLock };
    if (m_outputCaps)
        return m_outputCaps;

    auto stream = adoptGRef(gst_pad_get_stream(m_pad.get()));
    if (stream)
        m_outputCaps = adoptGRef(gst_stream_get_caps(stream.get()));
    else
        m_outputCaps = adoptGRef(gst_pad_get_current_caps(m_pad.get()));
    GST_DEBUG_OBJECT(m_pad.get(), "Output caps: %" GST_PTR_FORMAT, m_outputCaps.get());
    return m_outputCaps;
}

GstFlowReturn GStreamerElementHarness::Stream::chainSample(GRefPtr<GstSample>&& sample)
{
    Locker locker { m_sampleQueueLock };
    m_sampleQueue.prepend(WTFMove(sample));
    return GST_FLOW_OK;
}

bool GStreamerElementHarness::Stream::sinkEvent(GRefPtr<GstEvent>&& event)
{
    Locker locker { m_sinkEventQueueLock };

    // Clear cached output caps when the pad receives a new caps event or stream-start (potentially
    // storing a GstStream).
    if (GST_EVENT_TYPE(event.get()) == GST_EVENT_CAPS || GST_EVENT_TYPE(event.get()) == GST_EVENT_STREAM_START)
        m_outputCaps = nullptr;

    m_sinkEventQueue.prepend(WTFMove(event));
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

bool GStreamerElementHarness::srcEvent(GRefPtr<GstEvent>&& event)
{
    GST_TRACE_OBJECT(m_element.get(), "Got event on src pad: %" GST_PTR_FORMAT, event.get());
    Locker locker { m_srcEventQueueLock };
    m_srcEventQueue.prepend(WTFMove(event));
    return true;
}

void GStreamerElementHarness::processOutputSamples()
{
    for (auto& stream : m_outputStreams) {
        while (auto outputSample = stream->pullSample())
            m_processOutputSampleCallback(*stream.get(), WTFMove(outputSample));
    }
}

void GStreamerElementHarness::flush()
{
    GST_DEBUG_OBJECT(element(), "Flushing");

    if (!flushBuffers())
        return;

    m_inputCaps.clear();
    m_capsEventSent.store(false);
    m_segmentEventSent.store(false);
    GST_DEBUG_OBJECT(element(), "Flushing done, input caps and sticky events cleared");
}

bool GStreamerElementHarness::flushBuffers()
{
    GST_DEBUG_OBJECT(element(), "Flushing buffers");
    if (element()->current_state <= GST_STATE_PAUSED) {
        GST_DEBUG_OBJECT(element(), "No need to flush in paused state");
        return false;
    }

    processOutputSamples();

    pushEvent(adoptGRef(gst_event_new_flush_start()));
    pushEvent(adoptGRef(gst_event_new_flush_stop(FALSE)));
    m_segmentEventSent.store(false);

    for (auto& stream : m_outputStreams) {
        bool flushReceived = false;
        while (!flushReceived) {
            auto event = stream->pullEvent();
            flushReceived = event && GST_EVENT_TYPE(event.get()) == GST_EVENT_FLUSH_STOP;
        }
    }

    GST_DEBUG_OBJECT(element(), "Buffers flushed");
    return true;
}

#ifndef GST_DISABLE_GST_DEBUG
class MermaidBuilder {
public:
    MermaidBuilder();
    void process(GStreamerElementHarness&, bool generateFooter = true);
    std::span<uint8_t> span();

private:
    String generatePadId(GStreamerElementHarness&,  GstPad*);
    String getPadClass(const GRefPtr<GstPad>&);
    String describeCaps(const GRefPtr<GstCaps>&);
    void dumpPad(GStreamerElementHarness&, GstPad* = nullptr);
    void dumpElement(GStreamerElementHarness&, GstElement* = nullptr);

    StringBuilder m_stringBuilder;
    uint64_t m_invisibleLinesCounter { 0 };
    uint64_t m_elementCounter { 0 };
    Vector<std::tuple<GStreamerElementHarness&, GRefPtr<GstPad>, GRefPtr<GstPad>>> m_padLinks;
};

MermaidBuilder::MermaidBuilder()
{
    m_stringBuilder.append("flowchart LR\n"_s);
}

String MermaidBuilder::generatePadId(GStreamerElementHarness& harness, GstPad* pad)
{
    auto parent = adoptGRef(gst_pad_get_parent(GST_OBJECT_CAST(pad)));
    if (!parent)
        return makeString(GST_ELEMENT_NAME(harness.element()), "-harness-", GST_PAD_NAME(pad));
    return makeString(GST_OBJECT_NAME(parent.get()), '_', GST_PAD_NAME(pad));
}

String MermaidBuilder::getPadClass(const GRefPtr<GstPad>& pad)
{
    auto direction = gst_pad_get_direction(pad.get());
    if (GST_IS_GHOST_PAD(pad.get()))
        return direction == GST_PAD_SRC ? "ghostSrcPadClass"_s : "ghostSinkPadClass"_s;

    return direction == GST_PAD_SRC ? "srcPadClass"_s : "sinkPadClass"_s;
}

void MermaidBuilder::process(GStreamerElementHarness& harness, bool generateFooter)
{
    dumpElement(harness);
    dumpPad(harness);

    for (auto& outputStream : harness.outputStreams()) {
        auto pad = outputStream->targetPad();
        auto padId = generatePadId(harness, pad.get());
        m_stringBuilder.append("subgraph "_s, padId, " [", GST_PAD_NAME(pad.get()), "]\n");
        m_stringBuilder.append("end\n"_s);

        auto downstreamHarness = outputStream->downstreamHarness();
        if (!downstreamHarness)
            continue;
        process(*downstreamHarness, false);
        m_stringBuilder.append(padId, " ---> "_s, generatePadId(*downstreamHarness, downstreamHarness->inputPad()), '\n');
    }

    if (!generateFooter)
        return;

    for (auto& [padHarness, srcPad, sinkPad] : m_padLinks) {
        m_stringBuilder.append(generatePadId(padHarness, srcPad.get()), ":::"_s, getPadClass(srcPad));
        if (GST_IS_PROXY_PAD(srcPad.get()))
            m_stringBuilder.append(" ---> "_s);
        else if (auto srcCaps = adoptGRef(gst_pad_get_current_caps(srcPad.get()))) {
            auto capsString = describeCaps(srcCaps.get());
            m_stringBuilder.append(" --\""_s, capsString, "\"--> "_s);
        } else
            m_stringBuilder.append(" ---> "_s);
        m_stringBuilder.append(generatePadId(padHarness, sinkPad.get()), ":::"_s, getPadClass(sinkPad), '\n');
    }

    m_stringBuilder.append("classDef srcPadClass fill:#ffaaaa\n"_s);
    m_stringBuilder.append("classDef sinkPadClass fill:#aaaaff\n"_s);
    m_stringBuilder.append("classDef ghostSrcPadClass fill:#ffdddd\n"_s);
    m_stringBuilder.append("classDef ghostSinkPadClass fill:#ddddff\n"_s);
    m_stringBuilder.append("classDef elementClass fill:#aaffaa\n"_s);
}

void MermaidBuilder::dumpPad(GStreamerElementHarness& harness, GstPad* pad)
{
    if (!pad)
        pad = harness.inputPad();
    m_stringBuilder.append("subgraph "_s, generatePadId(harness, pad), " ["_s, GST_PAD_NAME(pad), "]\n"_s);

    if (gst_pad_is_linked(pad)) {
        auto peerPad = adoptGRef(gst_pad_get_peer(pad));
        if (gst_pad_get_direction(pad) == GST_PAD_SRC) {
            m_padLinks.append({ harness, pad, peerPad });
            if (GST_IS_PROXY_PAD(pad)) {
                auto internalPad = adoptGRef(gst_proxy_pad_get_internal(GST_PROXY_PAD(pad)));
                m_padLinks.append({ harness, GST_PAD_CAST(internalPad.get()), pad });
            }
        }
    }

    m_stringBuilder.append("end\n"_s);
    if (!GST_IS_GHOST_PAD(pad))
        return;

    auto padTarget = adoptGRef(gst_ghost_pad_get_target(GST_GHOST_PAD_CAST(pad)));
    if (!padTarget)
        return;

    auto peerPad = adoptGRef(gst_pad_get_peer(padTarget.get()));
    if (!peerPad)
        return;
    dumpPad(harness, peerPad.get());
}

void MermaidBuilder::dumpElement(GStreamerElementHarness& harness, GstElement* element)
{
    if (!element)
        element= harness.element();
    auto elementId = makeString(GST_ELEMENT_NAME(element), '_', m_elementCounter);
    m_elementCounter++;
    m_stringBuilder.append("subgraph "_s, elementId, " [<center>"_s, G_OBJECT_TYPE_NAME(element), "\\n<small>"_s, GST_ELEMENT_NAME(element), "]\n"_s);

    if (GST_IS_BIN(element)) {
        for (auto element : GstIteratorAdaptor<GstElement>(GUniquePtr<GstIterator>(gst_bin_iterate_recurse(GST_BIN_CAST(element)))))
            dumpElement(harness, element);
    }

    GRefPtr<GstPad> firstSinkPad, firstSrcPad;
    for (auto pad : GstIteratorAdaptor<GstPad>(GUniquePtr<GstIterator>(gst_element_iterate_sink_pads(element)))) {
        if (!firstSinkPad)
            firstSinkPad = pad;
        dumpPad(harness, pad);
    }
    for (auto pad : GstIteratorAdaptor<GstPad>(GUniquePtr<GstIterator>(gst_element_iterate_src_pads(element)))) {
        if (!firstSrcPad)
            firstSrcPad = pad;
        dumpPad(harness, pad);
    }

    // There is no clean way to maintain subgraph ordering, so draw invisible links between pads.
    // Upstream bug report: https://github.com/mermaid-js/mermaid/issues/815
    if (firstSinkPad && firstSrcPad) {
        m_stringBuilder.append(generatePadId(harness, firstSinkPad.get()), " --- "_s, generatePadId(harness, firstSrcPad.get()), '\n');
        m_stringBuilder.append("linkStyle "_s, m_invisibleLinesCounter, " stroke-width:0px\n"_s);
        m_invisibleLinesCounter++;
    }

    m_stringBuilder.append("end\n"_s);
    if (GST_IS_BIN(element))
        return;

    m_stringBuilder.append("class "_s, elementId, " elementClass\n"_s);
}

String MermaidBuilder::describeCaps(const GRefPtr<GstCaps>& caps)
{
    if (gst_caps_is_any(caps.get()) || gst_caps_is_empty(caps.get())) {
        GUniquePtr<char> capsString(gst_caps_to_string(caps.get()));
        return makeString(capsString.get());
    }

    StringBuilder builder;
    unsigned capsSize = gst_caps_get_size(caps.get());
    for (unsigned i = 0; i < capsSize; i++) {
        auto* features = gst_caps_get_features(caps.get(), i);
        const auto* structure = gst_caps_get_structure(caps.get(), i);
        builder.append(gst_structure_get_name(structure), "<br/>"_s);
        if (features && (gst_caps_features_is_any(features) || !gst_caps_features_is_equal(features, GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY))) {
            GUniquePtr<char> serializedFeature(gst_caps_features_to_string(features));
            builder.append('(', serializedFeature.get(), ')');
        }

        gst_structure_foreach(structure, [](GQuark field, const GValue* value, gpointer builderPointer) -> gboolean {
            auto* builder = reinterpret_cast<StringBuilder*>(builderPointer);
            builder->append(g_quark_to_string(field), ": "_s);

            GUniquePtr<char> serializedValue(gst_value_serialize(value));
            auto valueString = makeString(serializedValue.get());
            if (valueString.length() > 25)
                builder->append(valueString.substring(0, 25), "…");
            else
                builder->append(valueString);
            builder->append("<br/>"_s);
            return TRUE;
        }, &builder);
    }
    return builder.toString();
}

std::span<uint8_t> MermaidBuilder::span()
{
    auto data = m_stringBuilder.span<uint8_t>();
    return std::span(const_cast<uint8_t*>(data.data()), data.size_bytes());
}
#endif

void GStreamerElementHarness::dumpGraph(const char* filenamePrefix)
{
#ifndef GST_DISABLE_GST_DEBUG
    const char* dumpPath = g_getenv("WEBKIT_GST_HARNESS_DUMP_DIR");
    if (!dumpPath)
        return;

    auto elapsed = gst_util_get_timestamp() - webkitGstInitTime();
    GUniquePtr<char> elapsedTimeStamp(gst_info_strdup_printf("%" GST_TIME_FORMAT, GST_TIME_ARGS(elapsed)));
    auto filename = makeString(elapsedTimeStamp.get(), '-', filenamePrefix, "-harness-"_s, GST_ELEMENT_NAME(m_element.get()), ".mmd"_s);

    MermaidBuilder builder;
    builder.process(*this);
    auto path = FileSystem::pathByAppendingComponent(makeString(dumpPath), filename);
    FileSystem::overwriteEntireFile(path, builder.span());
#else
    UNUSED_PARAM(filenamePrefix);
#endif
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
