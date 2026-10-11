/*
 * ifaddrs.h — declaration shim for WebRTC's POSIX interface enumerator.
 *
 * The Wii U SDK does not ship the POSIX <ifaddrs.h> header. This header lets
 * WebRTC compile against the standard ABI shape; a real WKC network-peer
 * implementation of getifaddrs/freeifaddrs is still required before host ICE
 * candidate enumeration can work at runtime.
 */
#ifndef WKC_WIIU_IFADDRS_H
#define WKC_WIIU_IFADDRS_H

#include <sys/types.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ifaddrs {
    struct ifaddrs* ifa_next;
    char* ifa_name;
    unsigned int ifa_flags;
    struct sockaddr* ifa_addr;
    struct sockaddr* ifa_netmask;
    union {
        struct sockaddr* ifu_broadaddr;
        struct sockaddr* ifu_dstaddr;
    } ifa_ifu;
    void* ifa_data;
};

#ifndef ifa_broadaddr
#define ifa_broadaddr ifa_ifu.ifu_broadaddr
#endif
#ifndef ifa_dstaddr
#define ifa_dstaddr ifa_ifu.ifu_dstaddr
#endif

int getifaddrs(struct ifaddrs** ifap);
void freeifaddrs(struct ifaddrs* ifa);

#ifdef __cplusplus
}
#endif

#endif /* WKC_WIIU_IFADDRS_H */
