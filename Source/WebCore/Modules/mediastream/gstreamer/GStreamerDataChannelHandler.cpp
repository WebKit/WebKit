/*
 *  Copyright (C) 2019-2022 Igalia S.L. All rights reserved.
 *  Copyright (C) 2022 Metrological Group B.V.
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
#include "GStreamerDataChannelHandler.h"

#if USE(GSTREAMER_WEBRTC)

#include "EventNames.h"
#include "GStreamerWebRTCUtils.h"
#include "RTCDataChannel.h"
#include "RTCDataChannelEvent.h"
#include "RTCError.h"
#include "RTCPriorityType.h"

#include <wtf/MainThread.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_webrtc_endpoint_debug);
#define GST_CAT_DEFAULT webkit_webrtc_endpoint_debug

namespace WebCore {

GUniquePtr<GstStructure> GStreamerDataChannelHandler::fromRTCDataChannelInit(const RTCDataChannelInit& options)
{
    GUniquePtr<GstStructure> init(gst_structure_new("options", "protocol", G_TYPE_STRING, options.protocol.utf8().data(), nullptr));

    if (options.ordered)
        gst_structure_set(init.get(), "ordered", G_TYPE_BOOLEAN, *options.ordered, nullptr);
    if (options.maxPacketLifeTime)
        gst_structure_set(init.get(), "max-packet-lifetime", G_TYPE_INT, *options.maxPacketLifeTime, nullptr);
    if (options.maxRetransmits)
        gst_structure_set(init.get(), "max-retransmits", G_TYPE_INT, *options.maxRetransmits, nullptr);
    if (options.negotiated)
        gst_structure_set(init.get(), "negotiated", G_TYPE_BOOLEAN, *options.negotiated, nullptr);
    if (options.id)
        gst_structure_set(init.get(), "id", G_TYPE_INT, *options.id, nullptr);

    GstWebRTCPriorityType priorityType;
    switch (options.priority) {
    case RTCPriorityType::VeryLow:
        priorityType = GST_WEBRTC_PRIORITY_TYPE_VERY_LOW;
        break;
    case RTCPriorityType::Low:
        priorityType = GST_WEBRTC_PRIORITY_TYPE_LOW;
        break;
    case RTCPriorityType::Medium:
        priorityType = GST_WEBRTC_PRIORITY_TYPE_MEDIUM;
        break;
    case RTCPriorityType::High:
        priorityType = GST_WEBRTC_PRIORITY_TYPE_HIGH;
        break;
    }
    gst_structure_set(init.get(), "priority", GST_TYPE_WEBRTC_PRIORITY_TYPE, priorityType, nullptr);

    return init;
}

Ref<RTCDataChannelEvent> GStreamerDataChannelHandler::createDataChannelEvent(Document& document, GRefPtr<GstWebRTCDataChannel>&& dataChannel)
{
    GUniqueOutPtr<char> label;
    GUniqueOutPtr<char> protocol;
    gboolean ordered, negotiated;
    gint maxPacketLifeTime, maxRetransmits, id;
    g_object_get(dataChannel.get(), "ordered", &ordered, "label", &label.outPtr(),
        "max-packet-lifetime", &maxPacketLifeTime, "max-retransmits", &maxRetransmits,
        "protocol", &protocol.outPtr(), "negotiated", &negotiated, "id", &id, nullptr);

    RTCDataChannelInit init;
    init.ordered = ordered;
    init.maxPacketLifeTime = maxPacketLifeTime;
    init.maxRetransmits = maxRetransmits;
    init.protocol = String::fromLatin1(protocol.get());
    init.negotiated = negotiated;
    init.id = id;

    auto handler = WTF::makeUnique<GStreamerDataChannelHandler>(WTFMove(dataChannel));
    auto channel = RTCDataChannel::create(document, WTFMove(handler), String::fromUTF8(label.get()), WTFMove(init));
    return RTCDataChannelEvent::create(eventNames().datachannelEvent, Event::CanBubble::No, Event::IsCancelable::No, WTFMove(channel));
}

GStreamerDataChannelHandler::GStreamerDataChannelHandler(GRefPtr<GstWebRTCDataChannel>&& channel)
    : m_channel(WTFMove(channel))
{
    ASSERT(m_channel);

    g_signal_connect_swapped(m_channel.get(), "notify::ready-state", G_CALLBACK(+[](GStreamerDataChannelHandler* handler) {
        handler->readyStateChanged();
    }), this);
    g_signal_connect_swapped(m_channel.get(), "on-message-data", G_CALLBACK(+[](GStreamerDataChannelHandler* handler, GBytes* bytes) {
        handler->onMessageData(bytes);
    }), this);
    g_signal_connect_swapped(m_channel.get(), "on-message-string", G_CALLBACK(+[](GStreamerDataChannelHandler* handler, const char* message) {
        handler->onMessageString(message);
    }), this);
    g_signal_connect_swapped(m_channel.get(), "on-error", G_CALLBACK(+[](GStreamerDataChannelHandler* handler, GError* error) {
        handler->onError(error);
    }), this);
    g_signal_connect_swapped(m_channel.get(), "on-buffered-amount-low", G_CALLBACK(+[](GStreamerDataChannelHandler* handler) {
        // FIXME: We should pass the amount delta from webrtcbin to the WebCore handler.
        handler->onBufferedAmountLow();
    }), this);
}

GStreamerDataChannelHandler::~GStreamerDataChannelHandler()
{
    g_signal_handlers_disconnect_by_data(m_channel.get(), this);
}

void GStreamerDataChannelHandler::setClient(RTCDataChannelHandlerClient& client, ScriptExecutionContextIdentifier contextIdentifier)
{
    Locker locker { m_clientLock };
    ASSERT(!m_client);
    m_client = client;
    m_contextIdentifier = contextIdentifier;

    for (auto& message : m_pendingMessages) {
        switchOn(message, [&](Ref<FragmentedSharedBuffer>& data) {
            GST_DEBUG("Notifying queued raw data (size: %zu)", data->size());
            client.didReceiveRawData(data->makeContiguous()->data(), data->size());
        }, [&](String& text) {
            GST_DEBUG("Notifying queued string %s", text.ascii().data());
            client.didReceiveStringData(text);
        }, [&](StateChange stateChange) {
            if (stateChange.error) {
                if (auto rtcError = toRTCError(*stateChange.error))
                    client.didDetectError(rtcError.releaseNonNull());
            }
            GST_DEBUG("Dispatching state change to %d", static_cast<int>(stateChange.state));
            client.didChangeReadyState(stateChange.state);
        });
    }
    m_pendingMessages.clear();
}

bool GStreamerDataChannelHandler::sendStringData(const CString& text)
{
    GST_DEBUG("Sending string %s", text.data());
    g_signal_emit_by_name(m_channel.get(), "send-string", text.data());
    return true;
}

bool GStreamerDataChannelHandler::sendRawData(const uint8_t* data, size_t length)
{
    auto bytes = adoptGRef(g_bytes_new(data, length));
    g_signal_emit_by_name(m_channel.get(), "send-data", bytes.get());
    return true;
}

void GStreamerDataChannelHandler::close()
{
    g_signal_emit_by_name(m_channel.get(), "close");
}

void GStreamerDataChannelHandler::checkState()
{
    ASSERT(m_clientLock.isHeld());

    GstWebRTCDataChannelState channelState;
    g_object_get(m_channel.get(), "ready-state", &channelState, nullptr);

    RTCDataChannelState state;
    switch (channelState) {
#if !GST_CHECK_VERSION(1, 21, 0)
    // Removed in https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/2099.
    case GST_WEBRTC_DATA_CHANNEL_STATE_NEW:
#endif
    case GST_WEBRTC_DATA_CHANNEL_STATE_CONNECTING:
        state = RTCDataChannelState::Connecting;
        break;
    case GST_WEBRTC_DATA_CHANNEL_STATE_OPEN:
        state = RTCDataChannelState::Open;
        break;
    case GST_WEBRTC_DATA_CHANNEL_STATE_CLOSING:
        state = RTCDataChannelState::Closing;
        break;
    case GST_WEBRTC_DATA_CHANNEL_STATE_CLOSED:
        state = RTCDataChannelState::Closed;
        break;
    }

    if (!m_client) {
        GST_DEBUG("No client yet, queueing state");
        m_pendingMessages.append(StateChange { state, { } });
        return;
    }

    if (!*m_client)
        return;

    GST_DEBUG("Dispatching state change to %d", static_cast<int>(state));
    postTask([client = m_client, state] {
        if (!*client)
            return;
        client.value()->didChangeReadyState(state);
    });
}

void GStreamerDataChannelHandler::readyStateChanged()
{
    Locker locker { m_clientLock };
    checkState();
}

void GStreamerDataChannelHandler::onMessageData(GBytes* bytes)
{
    Locker locker { m_clientLock };

    if (!m_client) {
        m_pendingMessages.append(FragmentedSharedBuffer::create(bytes));
        return;
    }

    if (!*m_client)
        return;

    postTask([client = m_client, bytes = GRefPtr<GBytes>(bytes)] {
        if (!*client)
            return;
        gsize size = 0;
        const auto* data = reinterpret_cast<const uint8_t*>(g_bytes_get_data(bytes.get(), &size));
        client.value()->didReceiveRawData(data, size);
    });
}

void GStreamerDataChannelHandler::onMessageString(const char* message)
{
    Locker locker { m_clientLock };

    GST_DEBUG("Incoming string: %s", message);
    if (!m_client) {
        GST_DEBUG("No client yet, keeping as buffered message");
        m_pendingMessages.append(String::fromUTF8(message));
        return;
    }

    if (!*m_client)
        return;

    GST_DEBUG("Dispatching payload");
    postTask([client = m_client, string = String::fromUTF8(message)] {
        if (!*client)
            return;

        client.value()->didReceiveStringData(string);
        GST_DEBUG("Done");
    });
}

void GStreamerDataChannelHandler::onError(GError* error)
{
    Locker locker { m_clientLock };
    if (!m_client)
        return;

    GST_WARNING("Got data-channel error %s", error->message);
    GUniquePtr<GError> errorCopy(g_error_copy(error));
    postTask([client = m_client, error = WTFMove(errorCopy)] {
        if (!client || !error)
            return;

        auto rtcError = toRTCError(error.get());
        if (!rtcError)
            rtcError = RTCError::create(RTCError::Init { RTCErrorDetailType::DataChannelFailure, { }, { }, { }, { } }, { });
        client.value()->didDetectError(rtcError.releaseNonNull());
    });
}

void GStreamerDataChannelHandler::onBufferedAmountLow()
{
    Locker locker { m_clientLock };
    if (!m_client)
        return;

    postTask([client = m_client] {
        if (!client)
            return;

        // FIXME: Pass amount to handler instead of 0.
        client.value()->bufferedAmountIsDecreasing(0);
    });
}

void GStreamerDataChannelHandler::postTask(Function<void()>&& function)
{
    ASSERT(m_clientLock.isHeld());

    if (!m_contextIdentifier) {
        callOnMainThread(WTFMove(function));
        return;
    }
    ScriptExecutionContext::postTaskTo(m_contextIdentifier, WTFMove(function));
}

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
