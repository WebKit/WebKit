/*
 * WKCIfAddrs.cpp
 *
 * Minimal IPv4 interface discovery for libwebrtc on Wii U.
 *
 * CafeOS/WUT does not expose POSIX getifaddrs(). We discover the active
 * outbound IPv4 address using a UDP route probe and WKC's socket peer API.
 * Wii U has one active network path at a time for this engine; without an OS
 * route-netmask API, the result is conservatively represented as a /32.
 *
 * This enables libwebrtc's BasicNetworkManager to enumerate the usable local
 * IPv4 address. It does not enumerate IPv6 or multiple physical interfaces.
 */

#include "config.h"

#if USE(LIBWEBRTC) && PLATFORM(WKC)

#include <ifaddrs.h>
#include <wkc/wkcsocket.h>

#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef IFF_UP
#define IFF_UP 0x1
#endif
#ifndef IFF_RUNNING
#define IFF_RUNNING 0x40
#endif

namespace {

void freeOneInterface(struct ifaddrs* entry)
{
    if (!entry)
        return;
    free(entry->ifa_name);
    free(entry->ifa_addr);
    free(entry->ifa_netmask);
    free(entry);
}

struct ifaddrs* makeIPv4Interface(const struct sockaddr_in& address)
{
    auto* entry = static_cast<struct ifaddrs*>(calloc(1, sizeof(struct ifaddrs)));
    if (!entry)
        return nullptr;

    constexpr char name[] = "wiiu0";
    entry->ifa_name = static_cast<char*>(malloc(sizeof(name)));
    auto* localAddress = static_cast<struct sockaddr_in*>(calloc(1, sizeof(struct sockaddr_in)));
    auto* netmask = static_cast<struct sockaddr_in*>(calloc(1, sizeof(struct sockaddr_in)));
    if (!entry->ifa_name || !localAddress || !netmask) {
        free(entry->ifa_name);
        free(localAddress);
        free(netmask);
        free(entry);
        return nullptr;
    }

    memcpy(entry->ifa_name, name, sizeof(name));
    *localAddress = address;
    netmask->sin_family = AF_INET;
    netmask->sin_addr.s_addr = wkcNetInetAddrPeer("255.255.255.255");

    entry->ifa_flags = IFF_UP | IFF_RUNNING;
    entry->ifa_addr = reinterpret_cast<struct sockaddr*>(localAddress);
    entry->ifa_netmask = reinterpret_cast<struct sockaddr*>(netmask);
    return entry;
}

} // namespace

extern "C" int getifaddrs(struct ifaddrs** output)
{
    if (!output) {
        errno = EINVAL;
        return -1;
    }
    *output = nullptr;

    // UDP connect performs local route selection without sending a packet.
    // The destination is used only to ask the socket layer for its selected
    // source address; no DNS lookup and no application payload is sent.
    const int socket = wkcNetSocketPeer(AF_INET, SOCK_DGRAM, 0);
    if (socket < 0)
        return 0;

    struct sockaddr_in destination { };
    destination.sin_family = AF_INET;
    destination.sin_port = htons(53);
    destination.sin_addr.s_addr = wkcNetInetAddrPeer("1.1.1.1");

    struct sockaddr_in local { };
    socklen_t localLength = sizeof(local);
    const bool connected = wkcNetConnectPeer(
        socket, reinterpret_cast<const struct sockaddr*>(&destination),
        sizeof(destination)) == 0;
    const bool haveAddress = connected && wkcNetGetSockNamePeer(
        socket, reinterpret_cast<struct sockaddr*>(&local), &localLength) == 0
        && local.sin_family == AF_INET && local.sin_addr.s_addr != INADDR_ANY;

    wkcNetClosePeer(socket);

    if (!haveAddress)
        return 0;

    *output = makeIPv4Interface(local);
    if (!*output) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

extern "C" void freeifaddrs(struct ifaddrs* entries)
{
    while (entries) {
        struct ifaddrs* next = entries->ifa_next;
        freeOneInterface(entries);
        entries = next;
    }
}

#endif // USE(LIBWEBRTC) && PLATFORM(WKC)
