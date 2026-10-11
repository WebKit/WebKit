/* Wii U-only IPv6 declarations needed to compile libwebrtc's dual-stack code.
 * The Wii U networking stack is not thereby made IPv6-capable at runtime.
 */
#ifndef WKC_WIIU_WEBRTC_COMPAT_NETINET_IN_H
#define WKC_WIIU_WEBRTC_COMPAT_NETINET_IN_H

#include_next <netinet/in.h>
#include <stdint.h>

#ifndef AF_INET6
#define AF_INET6 10
#endif
#ifndef PF_INET6
#define PF_INET6 AF_INET6
#endif

/* devkitPPC/newlib's socket headers do not provide the BSD IPv6 ABI. Keep
 * these layouts local to the Wii U bring-up include overlay.
 */
#ifndef IN6ADDR_ANY_INIT
typedef struct in6_addr {
    union {
        uint8_t __u6_addr8[16];
        uint16_t __u6_addr16[8];
        uint32_t __u6_addr32[4];
    } __u6_addr;
} in6_addr;

#define s6_addr __u6_addr.__u6_addr8
#define s6_addr16 __u6_addr.__u6_addr16
#define s6_addr32 __u6_addr.__u6_addr32

struct sockaddr_in6 {
    uint16_t sin6_family;
    uint16_t sin6_port;
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t sin6_scope_id;
};

#define IN6ADDR_ANY_INIT {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}}
#define IN6ADDR_LOOPBACK_INIT {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}}}
static const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
static const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;
#endif

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif
#ifndef IPPROTO_IPV6
#define IPPROTO_IPV6 41
#endif
#ifndef IPV6_RECVTCLASS
#define IPV6_RECVTCLASS 66
#endif
#ifndef IPV6_TCLASS
#define IPV6_TCLASS 67
#endif
#ifndef IP_RECVTOS
#define IP_RECVTOS 13
#endif
#ifndef IP_MTU_DISCOVER
#define IP_MTU_DISCOVER 10
#endif
#ifndef IP_PMTUDISC_DONT
#define IP_PMTUDISC_DONT 0
#endif
#ifndef IP_PMTUDISC_WANT
#define IP_PMTUDISC_WANT 1
#endif
#ifndef IP_PMTUDISC_DO
#define IP_PMTUDISC_DO 2
#endif
#ifndef IP_PMTUDISC_PROBE
#define IP_PMTUDISC_PROBE 3
#endif
#ifndef IP_DONTFRAGMENT
#define IP_DONTFRAGMENT 14
#endif

#endif /* WKC_WIIU_WEBRTC_COMPAT_NETINET_IN_H */
