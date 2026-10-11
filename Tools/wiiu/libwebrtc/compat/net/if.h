/* Minimal interface declarations for WebRTC's POSIX network enumeration.
 * This is a compile-time shim; the Wii U interface enumerator still needs a
 * real platform implementation before libwebrtc network discovery is usable.
 */
#ifndef WKC_WIIU_WEBRTC_COMPAT_NET_IF_H
#define WKC_WIIU_WEBRTC_COMPAT_NET_IF_H

#include <sys/socket.h>
#include <sys/types.h>

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif
#ifndef IFF_UP
#define IFF_UP 0x1
#endif
#ifndef IFF_BROADCAST
#define IFF_BROADCAST 0x2
#endif
#ifndef IFF_LOOPBACK
#define IFF_LOOPBACK 0x8
#endif
#ifndef IFF_POINTOPOINT
#define IFF_POINTOPOINT 0x10
#endif
#ifndef IFF_RUNNING
#define IFF_RUNNING 0x40
#endif
#ifndef IFF_MULTICAST
#define IFF_MULTICAST 0x1000
#endif

#ifndef WKC_WIIU_WEBRTC_IFADDRS_DECLARED
#define WKC_WIIU_WEBRTC_IFADDRS_DECLARED
struct ifaddrs {
    struct ifaddrs *ifa_next;
    char *ifa_name;
    unsigned int ifa_flags;
    struct sockaddr *ifa_addr;
    struct sockaddr *ifa_netmask;
    union {
        struct sockaddr *ifu_broadaddr;
        struct sockaddr *ifu_dstaddr;
    } ifa_ifu;
    void *ifa_data;
};
#define ifa_broadaddr ifa_ifu.ifu_broadaddr
#define ifa_dstaddr ifa_ifu.ifu_dstaddr
int getifaddrs(struct ifaddrs **ifap);
void freeifaddrs(struct ifaddrs *ifa);
#endif

#endif /* WKC_WIIU_WEBRTC_COMPAT_NET_IF_H */
