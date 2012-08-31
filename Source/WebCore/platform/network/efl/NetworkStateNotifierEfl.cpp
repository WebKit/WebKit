/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "NetworkStateNotifier.h"

#include "Logging.h"
#include <Ecore.h>
#include <Eeze.h>
#include <Eeze_Net.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static const char udevLoopBackInterfaceSysPath[] = "lo";
static const char udevOperstateAttribute[] = "operstate";
static const char udevOperstateUp[] = "up";
static const size_t bufferSize = 4096;

namespace WebCore {

void NetworkStateNotifier::updateState()
{
    // Assume that we're offline until proven otherwise.
    m_isOnLine = false;

    Eina_List* networkInterfaces = eeze_net_list();

    Eina_List* list;
    void* data;
    EINA_LIST_FOREACH(networkInterfaces, list, data) {
        Eeze_Net* networkInterface = static_cast<Eeze_Net*>(data);

        // Skip Loopback interface.
        const char* syspath = eeze_net_syspath_get(networkInterface);
        if (!syspath || !strcmp(syspath, udevLoopBackInterfaceSysPath))
            continue;

        // Skip interfaces that are not up.
        const char* state = eeze_net_attribute_get(networkInterface, udevOperstateAttribute);
        if (!state || strcmp(state, udevOperstateUp))
            continue;

        // Check if the interface has an IP address.
        eeze_net_scan(networkInterface);
        if (eeze_net_addr_get(networkInterface, EEZE_NET_ADDR_TYPE_IP) || eeze_net_addr_get(networkInterface, EEZE_NET_ADDR_TYPE_IP6)) {
            m_isOnLine = true;
            break;
        }
    }

    EINA_LIST_FREE(networkInterfaces, data)
        eeze_net_free(static_cast<Eeze_Net*>(data));
}

void NetworkStateNotifier::networkInterfaceChanged()
{
    bool wasOnline = m_isOnLine;
    updateState();

    if (wasOnline != m_isOnLine && m_networkStateChangedFunction)
        m_networkStateChangedFunction();
}

static Eina_Bool readSocketCallback(void* userData, Ecore_Fd_Handler* handler)
{
    int sock = ecore_main_fd_handler_fd_get(handler);
    char buffer[bufferSize];
    ssize_t len;
    nlmsghdr* nlh = reinterpret_cast<nlmsghdr*>(buffer);
    while ((len = recv(sock, nlh, bufferSize, MSG_DONTWAIT)) > 0) {
        while ((NLMSG_OK(nlh, len)) && (nlh->nlmsg_type != NLMSG_DONE)) {
            if (nlh->nlmsg_type == NLMSG_ERROR) {
                LOG_ERROR("Error while reading socket, stop monitoring onLine state.");
                return ECORE_CALLBACK_CANCEL;
            }
            if (nlh->nlmsg_type == RTM_NEWADDR || nlh->nlmsg_type == RTM_DELADDR) {
                // We detected an IP address change, recheck onLine state.
                static_cast<NetworkStateNotifier*>(userData)->networkInterfaceChanged();
            }
            nlh = NLMSG_NEXT(nlh, len);
        }
    }

    return ECORE_CALLBACK_RENEW;
}

NetworkStateNotifier::~NetworkStateNotifier()
{
    if (m_fdHandler) {
        int sock = ecore_main_fd_handler_fd_get(m_fdHandler);
        ecore_main_fd_handler_del(m_fdHandler);
        close(sock);
    }
    eeze_shutdown();
}

NetworkStateNotifier::NetworkStateNotifier()
    : m_isOnLine(false)
    , m_networkStateChangedFunction(0)
    , m_fdHandler(0)
{
    if (eeze_init() < 0) {
        LOG_ERROR("Failed to initialize eeze library.");
        return;
    }

    updateState();

    // Watch for network address changes to keep online state up-to-date.
    int sock;
    if ((sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) == -1) {
        LOG_ERROR("Couldn't open NETLINK_ROUTE socket.");
        return;
    }

    sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        LOG_ERROR("Couldn't bind to NETLINK_ROUTE socket.");
        return;
    }

    m_fdHandler = ecore_main_fd_handler_add(sock, ECORE_FD_READ, readSocketCallback, this, 0, 0);
}

} // namespace WebCore
