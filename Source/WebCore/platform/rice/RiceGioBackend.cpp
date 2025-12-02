/*
 *  Copyright (C) 2025 Igalia S.L. All rights reserved.
 *  Copyright (C) 2025 Metrological Group B.V.
 *  Copyright (C) 2024 Matthew Waters <matthew@centricular.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "RiceGioBackend.h"

#if USE(GSTREAMER_WEBRTC) && USE(LIBRICE)

#include "RiceUtilities.h"
#include <gst/gst.h>
#include <rice-proto.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>
#include <wtf/glib/RunLoopSourcePriority.h>

#define GST_CAT_DEFAULT gst_webrtc_rice_gio_debug
GST_DEBUG_CATEGORY_STATIC(GST_CAT_DEFAULT);

using namespace::WebCore;

struct _AgentSource {
    GSource source;

    GThreadSafeWeakPtr<WebKitGstIceAgent> agent;
    bool complete;
};

static gboolean agentSourcePrepare(GSource* base, gint* timeout)
{
    auto source = reinterpret_cast<AgentSource*>(base);
    auto iceAgent = source->agent.get();
    if (!iceAgent)
        return FALSE;

    const auto& agent = webkitGstWebRTCIceAgentGetRiceAgent(iceAgent.get());
    auto now = WTF::MonotonicTime::now().secondsSinceEpoch();

    gboolean result = FALSE;
    while (true) {
        RiceAgentPoll ret;
        rice_agent_poll_init(&ret);
        GST_TRACE_OBJECT(iceAgent.get(), "Polling");
        rice_agent_poll(agent.get(), now.nanoseconds(), &ret);
        GST_TRACE_OBJECT(iceAgent.get(), "Polling DONE");
        switch (ret.tag) {
        case RICE_AGENT_POLL_CLOSED:
            GST_TRACE_OBJECT(iceAgent.get(), "Agent closed!");
            source->complete = true;
            rice_agent_poll_clear(&ret);
            webkitGstWebRTCIceAgentClosed(iceAgent.get());
            return TRUE;
        case RICE_AGENT_POLL_COMPONENT_STATE_CHANGE:
            GST_TRACE_OBJECT(iceAgent.get(), "Component state changed");
            webkitGstWebRTCIceAgentComponentStateChangedForStream(iceAgent.get(), ret.component_state_change.stream_id, ret.component_state_change);
            result = TRUE;
            break;
        case RICE_AGENT_POLL_ALLOCATE_SOCKET:
            GST_FIXME("allocate socket is not handled");
            result = TRUE;
            break;
        case RICE_AGENT_POLL_REMOVE_SOCKET:
            GST_FIXME("remove socket is not handled");
            result = TRUE;
            break;
        case RICE_AGENT_POLL_WAIT_UNTIL_NANOS: {
            auto delta = Seconds::fromNanoseconds(ret.wait_until_nanos - now.nanoseconds());
            if (delta >= 99998_s) {
                GST_TRACE_OBJECT(iceAgent.get(), "Nothing special to do.");
                result = FALSE;
                break;
            }
            if (timeout) {
                *timeout = static_cast<int>(delta.milliseconds());
                GST_TRACE_OBJECT(iceAgent.get(), "Waiting for %d ms", *timeout);
            }
            result = FALSE;
            break;
        }
        case RICE_AGENT_POLL_GATHERING_COMPLETE:
            GST_TRACE_OBJECT(iceAgent.get(), "Gathering complete");
            webkitGstWebRTCIceAgentGatheringDoneForStream(iceAgent.get(), ret.gathering_complete.stream_id);
            break;
        case RICE_AGENT_POLL_GATHERED_CANDIDATE:
            GST_TRACE_OBJECT(iceAgent.get(), "Gathered candidate");
            webkitGstWebRTCIceAgentLocalCandidateGatheredForStream(iceAgent.get(), ret.gathered_candidate.stream_id, ret.gathered_candidate);
            result = TRUE;
            break;
        case RICE_AGENT_POLL_SELECTED_PAIR:
            GST_TRACE_OBJECT(iceAgent.get(), "New selected pair");
            webkitGstWebRTCIceAgentNewSelectedPairForStream(iceAgent.get(), ret.selected_pair.stream_id, ret.selected_pair);
            result = TRUE;
            break;
        };
        rice_agent_poll_clear(&ret);

        RiceTransmit transmit;
        rice_transmit_init(&transmit);
        rice_agent_poll_transmit(agent.get(), now.nanoseconds(), &transmit);
        if (transmit.from && transmit.to) {
            auto from = riceAddressToString(transmit.from);
            auto to = riceAddressToString(transmit.to);
            auto protocol = riceTransmitTransportToIceProtocol(transmit);
            if (auto handle = riceTransmitToSharedMemoryHandle(&transmit)) {
                webkitGstWebRTCIceAgentSend(iceAgent.get(), transmit.stream_id, protocol, WTFMove(from), WTFMove(to), WTFMove(*handle));
                result = TRUE;
            }
        } else
            rice_transmit_clear(&transmit);
        if (!result)
            break;
    }

    return result;
}

static gboolean agentSourceCheck(GSource*)
{
    return TRUE;
}

static gboolean agentSourceDispatch(GSource* base, GSourceFunc callback, gpointer data)
{
    auto source = reinterpret_cast<AgentSource*>(base);

    if (callback)
        callback(data);

    return !source->complete;
}

static void agentSourceFinalize(GSource*)
{
}

static GSourceFuncs agentEventSourceFuncs = {
    agentSourcePrepare,
    agentSourceCheck,
    agentSourceDispatch,
    agentSourceFinalize,
    nullptr, nullptr
};

GRefPtr<GSource> agentSourceNew(GThreadSafeWeakPtr<WebKitGstIceAgent>&& agent)
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(GST_CAT_DEFAULT, "webkitwebrtcricegio", 0, "webkitwebrtcricegio");
    });

    auto source = adoptGRef(g_source_new(&agentEventSourceFuncs, sizeof(AgentSource)));
    g_source_set_priority(source.get(), RunLoopSourcePriority::AsyncIONetwork);
    g_source_set_name(source.get(), "[WebKit] ICE Agent loop");

    auto agentSource = reinterpret_cast<AgentSource*>(source.get());
    if (auto iceAgent = agent.get()) [[likely]]
        agentSource->agent.reset(iceAgent.get());
    agentSource->complete = false;

    return source;
}

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER_WEBRTC) && USE(LIBRICE)
