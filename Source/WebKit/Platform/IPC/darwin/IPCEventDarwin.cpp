/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "IPCEvent.h"

#include "Logging.h"
#include "MachUtilities.h"
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/message.h>

namespace IPC {

// Arbitrary message IDs that do not collide with Mach notification messages.
constexpr mach_msg_id_t inlineBodyMessageID = 0xdba0dba;

static void requestNoSenderNotifications(mach_port_t port, mach_port_t notify)
{
    mach_port_t previousNotificationPort = MACH_PORT_NULL;
    auto kr = mach_port_request_notification(mach_task_self(), port, MACH_NOTIFY_NO_SENDERS, 0, notify, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previousNotificationPort);
    ASSERT(kr == KERN_SUCCESS);
    if (kr != KERN_SUCCESS) {
        // If mach_port_request_notification fails, 'previousNotificationPort' will be uninitialized.
        LOG_ERROR("mach_port_request_notification failed: (%x) %s", kr, mach_error_string(kr));
    } else
        deallocateSendRightSafely(previousNotificationPort);
}

static void requestNoSenderNotifications(mach_port_t port)
{
    requestNoSenderNotifications(port, port);
}

static void clearNoSenderNotifications(mach_port_t port)
{
    requestNoSenderNotifications(port, MACH_PORT_NULL);
}

void Signal::signal()
{
    mach_msg_header_t message;
    memset(&message, 0, sizeof(message));
    message.msgh_remote_port = m_sendRight.sendRight();
    message.msgh_local_port = MACH_PORT_NULL;
    message.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    message.msgh_id = inlineBodyMessageID;

    auto ret = mach_msg(&message, MACH_SEND_MSG, sizeof(message), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (ret != KERN_SUCCESS)
        RELEASE_LOG_ERROR(Process, "IPC::Signal::signal Could not send mach message, error %x", ret);
}

std::optional<EventSignalPair> createEventSignalPair()
{
    // Create the listening port.
    mach_port_t listeningPort = MACH_PORT_NULL;
    auto kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);
    if (kr != KERN_SUCCESS) {
        RELEASE_LOG_ERROR(Process, "createEventSignalPair: Could not allocate mach port, error %x", kr);
        return std::nullopt;
    }
    if (!MACH_PORT_VALID(listeningPort)) {
        RELEASE_LOG_ERROR(Process, "createEventSignalPair: Could not allocate mach port, returned port was invalid");
        return std::nullopt;
    }

    setMachPortQueueLength(listeningPort, 1);
    auto sendRight = MachSendRight::createFromReceiveRight(listeningPort);
    requestNoSenderNotifications(listeningPort);

    return EventSignalPair { Event { listeningPort }, Signal { WTFMove(sendRight) } };
}

Event::~Event()
{
    if (m_receiveRight != MACH_PORT_NULL) {
        clearNoSenderNotifications(m_receiveRight);
        mach_port_mod_refs(mach_task_self(), m_receiveRight, MACH_PORT_RIGHT_RECEIVE, -1);
    }
}

typedef struct {
    mach_msg_header_t header;
    mach_msg_trailer_t trailer;
} ReceiveMessage;

bool Event::wait()
{
    ReceiveMessage receiveMessage;
    memset(&receiveMessage, 0, sizeof(receiveMessage));
    mach_msg_return_t ret = mach_msg(&receiveMessage.header, MACH_RCV_MSG, 0, sizeof(receiveMessage), m_receiveRight, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (ret != MACH_MSG_SUCCESS)
        return false;
    if (receiveMessage.header.msgh_id != inlineBodyMessageID)
        return false;
    return true;
}

bool Event::waitFor(Timeout timeout)
{
    ReceiveMessage receiveMessage;
    memset(&receiveMessage, 0, sizeof(receiveMessage));
    mach_msg_return_t ret = mach_msg(&receiveMessage.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(receiveMessage), m_receiveRight, timeout.secondsUntilDeadline().milliseconds(), MACH_PORT_NULL);
    if (ret != MACH_MSG_SUCCESS)
        return false;
    if (receiveMessage.header.msgh_id != inlineBodyMessageID)
        return false;
    return true;
}

} // namespace IPC

