/* Minimal interface declarations for WebRTC's POSIX network enumeration.
 * This is a compile-time shim; the Wii U interface enumerator still needs a
 * real platform implementation before libwebrtc network discovery is usable.
 */
#ifndef WKC_WIIU_WEBRTC_COMPAT_NET_IF_H
#define WKC_WIIU_WEBRTC_COMPAT_NET_IF_H

#include <sys/socket.h>
#include <sys/types.h>
#include <ifaddrs.h>

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

/* The WKC stub <ifaddrs.h> owns the struct/prototypes. Reuse it to avoid duplicate definitions. */

#endif /* WKC_WIIU_WEBRTC_COMPAT_NET_IF_H */
