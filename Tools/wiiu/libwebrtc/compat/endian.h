/*
 * Wii U / devkitPPC compatibility definitions for WebRTC's endian.h users.
 *
 * The Wii U's Espresso CPU is 32-bit big-endian PowerPC. Newlib/WUT does not
 * provide the glibc <endian.h> header expected by several WebRTC dependencies.
 * These conversion macros are deliberately defined in terms of the target's
 * actual byte order; they are not a little-endian emulation.
 */
#ifndef WEBKIT_WIIU_WEBRTC_COMPAT_ENDIAN_H
#define WEBKIT_WIIU_WEBRTC_COMPAT_ENDIAN_H

#include <stdint.h>

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN 4321
#endif
#ifndef BYTE_ORDER
#define BYTE_ORDER BIG_ENDIAN
#endif
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#endif
#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN BIG_ENDIAN
#endif
#ifndef __BYTE_ORDER
#define __BYTE_ORDER __BIG_ENDIAN
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define WIIU_WEBRTC_BIG_ENDIAN 1
#else
#error "The Wii U WebRTC compatibility header requires a big-endian PowerPC target."
#endif

#ifndef htobe16
#define htobe16(x) ((uint16_t)(x))
#endif
#ifndef htobe32
#define htobe32(x) ((uint32_t)(x))
#endif
#ifndef htobe64
#define htobe64(x) ((uint64_t)(x))
#endif
#ifndef be16toh
#define be16toh(x) ((uint16_t)(x))
#endif
#ifndef be32toh
#define be32toh(x) ((uint32_t)(x))
#endif
#ifndef be64toh
#define be64toh(x) ((uint64_t)(x))
#endif

#ifndef htole16
#define htole16(x) __builtin_bswap16((uint16_t)(x))
#endif
#ifndef htole32
#define htole32(x) __builtin_bswap32((uint32_t)(x))
#endif
#ifndef htole64
#define htole64(x) __builtin_bswap64((uint64_t)(x))
#endif
#ifndef le16toh
#define le16toh(x) __builtin_bswap16((uint16_t)(x))
#endif
#ifndef le32toh
#define le32toh(x) __builtin_bswap32((uint32_t)(x))
#endif
#ifndef le64toh
#define le64toh(x) __builtin_bswap64((uint64_t)(x))
#endif

/* Network byte order is big-endian, so these operations are identities. */
#ifndef htons
#define htons(x) ((uint16_t)(x))
#endif
#ifndef htonl
#define htonl(x) ((uint32_t)(x))
#endif
#ifndef ntohs
#define ntohs(x) ((uint16_t)(x))
#endif
#ifndef ntohl
#define ntohl(x) ((uint32_t)(x))
#endif

#endif /* WEBKIT_WIIU_WEBRTC_COMPAT_ENDIAN_H */
