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

GST_DEBUG_CATEGORY(webkit_webrtc_data_channel_debug);
#define GST_CAT_DEFAULT webkit_webrtc_data_channel_debug

#if GST_CHECK_VERSION(1, 22, 0)
#define DC_DEBUG(...) GST_DEBUG_ID(m_channelId.ascii().data(), __VA_ARGS__)
#define DC_TRACE(...) GST_TRACE_ID(m_channelId.ascii().data(), __VA_ARGS__)
#define DC_WARNING(...) GST_WARNING_ID(m_channelId.ascii().data(), __VA_ARGS__)
#else
#define DC_DEBUG(...) GST_DEBUG(__VA_ARGS__)
#define DC_TRACE(...) GST_TRACE(__VA_ARGS__)
#define DC_WARNING(...) GST_WARNING(__VA_ARGS__)
#endif

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

GStreamerDataChannelHandler::GStreamerDataChannelHandler(GRefPtr<GstWebRTCDataChannel>&& channel)
    : m_channel(WTFMove(channel))
{
    static Atomic<uint64_t> nChannel = 0;
    m_channelId = makeString("webkit-webrtc-data-channel-", nChannel.exchangeAdd(1));

    ASSERT(m_channel);
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_webrtc_data_channel_debug, "webkitwebrtcdatachannel", 0, "WebKit WebRTC data-channel");
    });
    DC_DEBUG("New GStreamerDataChannelHandler for channel %p", m_channel.get());

    {
        Locker locker { m_clientLock };
        checkState();
    }

    g_signal_connect_swapped(m_channel.get(), "notify::ready-state", G_CALLBACK(+[](GStreamerDataChannelHandler* handler) {
        handler->readyStateChanged();
    }), this);
    g_signal_connect_swapped(m_channel.get(), "notify::buffered-amount", G_CALLBACK(+[](GStreamerDataChannelHandler* handler) {
        handler->bufferedAmountChanged();
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
    g_signal_connect_swapped(m_channel.get(), "on-close", G_CALLBACK(+[](GStreamerDataChannelHandler* handler) {
        handler->onClose();
    }), this);
}

GStreamerDataChannelHandler::~GStreamerDataChannelHandler()
{
    DC_DEBUG("Deleting GStreamerDataChannelHandler for channel %p", m_channel.get());
    if (m_channel)
        g_signal_handlers_disconnect_by_data(m_channel.get(), this);
}

RTCDataChannelInit GStreamerDataChannelHandler::dataChannelInit() const
{
    GUniqueOutPtr<char> protocol;
    gboolean ordered, negotiated;
    gint maxPacketLifeTime, maxRetransmits, id;
    g_object_get(m_channel.get(), "ordered", &ordered, "max-packet-lifetime", &maxPacketLifeTime, "max-retransmits", &maxRetransmits,
        "protocol", &protocol.outPtr(), "negotiated", &negotiated, "id", &id, nullptr);

    RTCDataChannelInit init;
    init.ordered = ordered;
    init.maxPacketLifeTime = maxPacketLifeTime;
    init.maxRetransmits = maxRetransmits;
    init.protocol = String::fromLatin1(protocol.get());
    init.negotiated = negotiated;
    init.id = id;
    return init;
}

String GStreamerDataChannelHandler::label() const
{
    GUniqueOutPtr<char> label;
    g_object_get(m_channel.get(), "label", &label.outPtr(), nullptr);
    return String::fromUTF8(label.get());
}

void GStreamerDataChannelHandler::setClient(RTCDataChannelHandlerClient& client, ScriptExecutionContextIdentifier contextIdentifier)
{
    Locker locker { m_clientLock };
    ASSERT(!m_client);
    DC_DEBUG("Setting client on channel %p", m_channel.get());
    m_client = client;
    m_contextIdentifier = contextIdentifier;

    checkState();

    for (auto& message : m_pendingMessages) {
        switchOn(message, [&](Ref<FragmentedSharedBuffer>& data) {
            DC_DEBUG("Notifying queued raw data (size: %zu)", data->size());
            client.didReceiveRawData(data->makeContiguous()->data(), data->size());
        }, [&](String& text) {
            DC_DEBUG("Notifying queued string of size %d", text.sizeInBytes());
            DC_TRACE("Notifying queued string %s", text.ascii().data());
            client.didReceiveStringData(text);
        }, [&](StateChange stateChange) {
            if (stateChange.error) {
                if (auto rtcError = toRTCError(*stateChange.error))
                    client.didDetectError(rtcError.releaseNonNull());
            }
            DC_DEBUG("Dispatching state change to %d on channel %p", static_cast<int>(stateChange.state), m_channel.get());
            client.didChangeReadyState(stateChange.state);
        });
    }
    m_pendingMessages.clear();
}

bool GStreamerDataChannelHandler::sendStringData(const CString& text)
{
    DC_DEBUG("Sending string of length: %zu", text.length());
    DC_TRACE("Sending string %s", text.data());
    g_signal_emit_by_name(m_channel.get(), "send-string", text.data());
    return true;
}

bool GStreamerDataChannelHandler::sendRawData(const uint8_t* data, size_t length)
{
    DC_DEBUG("Sending raw data of length: %zu", length);
    auto bytes = adoptGRef(g_bytes_new(data, length));
    g_signal_emit_by_name(m_channel.get(), "send-data", bytes.get());
    return true;
}

void GStreamerDataChannelHandler::close()
{
    DC_DEBUG("Closing channel %p", m_channel.get());
    m_closing = true;

    GstWebRTCDataChannelState channelState;
    g_object_get(m_channel.get(), "ready-state", &channelState, nullptr);

    if (channelState == GST_WEBRTC_DATA_CHANNEL_STATE_OPEN)
        g_signal_emit_by_name(m_channel.get(), "close");
}

std::optional<unsigned short> GStreamerDataChannelHandler::id() const
{
    int id;
    g_object_get(m_channel.get(), "id", &id, nullptr);
    return id != -1 ? std::make_optional(id) : std::nullopt;
}

void GStreamerDataChannelHandler::checkState()
{
    ASSERT(m_clientLock.isHeld());

    DC_DEBUG("Checking state.");
    if (!m_channel) {
        DC_DEBUG("No channel.");
        return;
    }

    GstWebRTCDataChannelState channelState;
    g_object_get(m_channel.get(), "ready-state", &channelState, nullptr);

    RTCDataChannelState state;
    switch (channelState) {
#if !GST_CHECK_VERSION(1, 21, 1)
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
        DC_DEBUG("No client yet on channel %p, queueing state", m_channel.get());
        m_pendingMessages.append(StateChange { state, { } });
        return;
    }

    if (channelState == GST_WEBRTC_DATA_CHANNEL_STATE_OPEN && m_closing) {
        DC_DEBUG("Ignoring open state notification on channel %p because it was pending to be closed", m_channel.get());
        return;
    }

    if (!*m_client) {
        DC_DEBUG("Client is empty.");
        return;
    }

#ifndef GST_DISABLE_GST_DEBUG
    GUniquePtr<char> stateString(g_enum_to_string(GST_TYPE_WEBRTC_DATA_CHANNEL_STATE, channelState));
    DC_DEBUG("Dispatching state change to %s on channel %p", stateString.get(), m_channel.get());
#endif
    postTask([client = m_client, state] {
        if (!*client) {
            GST_DEBUG("No client");
            return;
        }
        client.value()->didChangeReadyState(state);
    });
}

void GStreamerDataChannelHandler::readyStateChanged()
{
    Locker locker { m_clientLock };
    checkState();
}

void GStreamerDataChannelHandler::bufferedAmountChanged()
{
    Locker locker { m_clientLock };

    uint64_t currentBufferedAmount;
    g_object_get(m_channel.get(), "buffered-amount", &currentBufferedAmount, nullptr);

    auto bufferedAmount = static_cast<size_t>(currentBufferedAmount);
    DC_DEBUG("New buffered amount on channel %p: %" G_GSIZE_FORMAT " old: %" G_GSIZE_FORMAT, m_channel.get(), bufferedAmount, m_cachedBufferedAmount ? *m_cachedBufferedAmount : -1);

    if (m_cachedBufferedAmount && (*m_cachedBufferedAmount >= bufferedAmount)) {
        DC_DEBUG("Buffered amount getting low on channel %p", m_channel.get());
        if (!m_client) {
            DC_DEBUG("No client yet on channel %p", m_channel.get());
            return;
        }

        postTask([client = m_client, amount = *m_cachedBufferedAmount - bufferedAmount] {
            if (!client)
                return;

            size_t clientAmount = client.value()->bufferedAmount();
            size_t clampedAmount = amount > clientAmount ? clientAmount : amount;
            client.value()->bufferedAmountIsDecreasing(clampedAmount);
        });
    }

    m_cachedBufferedAmount = bufferedAmount;
}

void GStreamerDataChannelHandler::onMessageData(GBytes* bytes)
{
    auto size = g_bytes_get_size(bytes);
    DC_DEBUG("Incoming data of size: %zu", size);
    Locker locker { m_clientLock };

    if (!m_client) {
        m_pendingMessages.append(SharedBuffer::create(bytes));
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

    DC_TRACE("Incoming string: %s", message);
    if (!m_client) {
        DC_DEBUG("No client yet, keeping as buffered message");
        m_pendingMessages.append(String::fromUTF8(message));
        return;
    }

    if (!*m_client)
        return;

    DC_DEBUG("Dispatching string of size %zu", strlen(message));
    postTask([client = m_client, string = String::fromUTF8(message)] {
        if (!*client)
            return;

        client.value()->didReceiveStringData(string);
    });
}

void GStreamerDataChannelHandler::onError(GError* error)
{
    Locker locker { m_clientLock };
    if (!m_client)
        return;

    DC_WARNING("Got data-channel error %s", error->message);
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

void GStreamerDataChannelHandler::onClose()
{
    Locker locker { m_clientLock };
    DC_DEBUG("Channel %p closed!", m_channel.get());
    checkState();
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
