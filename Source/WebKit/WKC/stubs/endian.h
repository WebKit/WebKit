/*
 * endian.h — Wii U/newlib compatibility shim for Linux/BSD endian.h users.
 *
 * Wii U devkitPPC is 32-bit big-endian PowerPC and newlib does not provide the
 * glibc <endian.h> API expected by some WebRTC/Abseil/dependency code.
 * Keep this header portable C/C++ and derive byte order from compiler macros.
 */
#ifndef WKC_WIIU_ENDIAN_H
#define WKC_WIIU_ENDIAN_H

#include <stdint.h>

#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#endif
#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN 4321
#endif
#ifndef __PDP_ENDIAN
#define __PDP_ENDIAN 3412
#endif

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN __BIG_ENDIAN
#endif
#ifndef PDP_ENDIAN
#define PDP_ENDIAN __PDP_ENDIAN
#endif

#ifndef __BYTE_ORDER
# if defined(__BYTE_ORDER__)
#  if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#   define __BYTE_ORDER __BIG_ENDIAN
#  elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#   define __BYTE_ORDER __LITTLE_ENDIAN
#  else
#   error "Unsupported Wii U compiler byte order"
#  endif
# elif defined(__BIG_ENDIAN__) || defined(__PPC__) || defined(__powerpc__)
#  define __BYTE_ORDER __BIG_ENDIAN
# else
#  define __BYTE_ORDER __LITTLE_ENDIAN
# endif
#endif

#ifndef BYTE_ORDER
#define BYTE_ORDER __BYTE_ORDER
#endif
#ifndef _BYTE_ORDER
#define _BYTE_ORDER __BYTE_ORDER
#endif
#ifndef _BIG_ENDIAN
#define _BIG_ENDIAN __BIG_ENDIAN
#endif
#ifndef _LITTLE_ENDIAN
#define _LITTLE_ENDIAN __LITTLE_ENDIAN
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
# define WKC_HTOBE16(x) ((uint16_t)(x))
# define WKC_BE16TOH(x) ((uint16_t)(x))
# define WKC_HTOBE32(x) ((uint32_t)(x))
# define WKC_BE32TOH(x) ((uint32_t)(x))
# define WKC_HTOBE64(x) ((uint64_t)(x))
# define WKC_BE64TOH(x) ((uint64_t)(x))
# define WKC_HTOLE16(x) ((uint16_t)__builtin_bswap16((uint16_t)(x)))
# define WKC_LE16TOH(x) ((uint16_t)__builtin_bswap16((uint16_t)(x)))
# define WKC_HTOLE32(x) ((uint32_t)__builtin_bswap32((uint32_t)(x)))
# define WKC_LE32TOH(x) ((uint32_t)__builtin_bswap32((uint32_t)(x)))
# define WKC_HTOLE64(x) ((uint64_t)__builtin_bswap64((uint64_t)(x)))
# define WKC_LE64TOH(x) ((uint64_t)__builtin_bswap64((uint64_t)(x)))
#else
# define WKC_HTOLE16(x) ((uint16_t)(x))
# define WKC_LE16TOH(x) ((uint16_t)(x))
# define WKC_HTOLE32(x) ((uint32_t)(x))
# define WKC_LE32TOH(x) ((uint32_t)(x))
# define WKC_HTOLE64(x) ((uint64_t)(x))
# define WKC_LE64TOH(x) ((uint64_t)(x))
# define WKC_HTOBE16(x) ((uint16_t)__builtin_bswap16((uint16_t)(x)))
# define WKC_BE16TOH(x) ((uint16_t)__builtin_bswap16((uint16_t)(x)))
# define WKC_HTOBE32(x) ((uint32_t)__builtin_bswap32((uint32_t)(x)))
# define WKC_BE32TOH(x) ((uint32_t)__builtin_bswap32((uint32_t)(x)))
# define WKC_HTOBE64(x) ((uint64_t)__builtin_bswap64((uint64_t)(x)))
# define WKC_BE64TOH(x) ((uint64_t)__builtin_bswap64((uint64_t)(x)))
#endif

#ifndef htobe16
#define htobe16(x) WKC_HTOBE16(x)
#endif
#ifndef be16toh
#define be16toh(x) WKC_BE16TOH(x)
#endif
#ifndef htobe32
#define htobe32(x) WKC_HTOBE32(x)
#endif
#ifndef be32toh
#define be32toh(x) WKC_BE32TOH(x)
#endif
#ifndef htobe64
#define htobe64(x) WKC_HTOBE64(x)
#endif
#ifndef be64toh
#define be64toh(x) WKC_BE64TOH(x)
#endif
#ifndef htole16
#define htole16(x) WKC_HTOLE16(x)
#endif
#ifndef le16toh
#define le16toh(x) WKC_LE16TOH(x)
#endif
#ifndef htole32
#define htole32(x) WKC_HTOLE32(x)
#endif
#ifndef le32toh
#define le32toh(x) WKC_LE32TOH(x)
#endif
#ifndef htole64
#define htole64(x) WKC_HTOLE64(x)
#endif
#ifndef le64toh
#define le64toh(x) WKC_LE64TOH(x)
#endif

#ifndef bswap_16
#define bswap_16(x) __builtin_bswap16((uint16_t)(x))
#endif
#ifndef bswap_32
#define bswap_32(x) __builtin_bswap32((uint32_t)(x))
#endif
#ifndef bswap_64
#define bswap_64(x) __builtin_bswap64((uint64_t)(x))
#endif

#endif /* WKC_WIIU_ENDIAN_H */
