/* Map Linux/BSD TCP keepalive names used by libwebrtc to devkitPPC's
 * SDK-facing option names, for compile-time Wii U port bring-up.
 */
#ifndef WKC_WIIU_WEBRTC_COMPAT_NETINET_TCP_H
#define WKC_WIIU_WEBRTC_COMPAT_NETINET_TCP_H

#if defined(__has_include_next)
# if __has_include_next(<netinet/tcp.h>)
#  include_next <netinet/tcp.h>
# endif
#endif

#ifndef TCP_KEEPIDLE
# ifdef OPT_TCP_KEEPIDLE
#  define TCP_KEEPIDLE OPT_TCP_KEEPIDLE
# else
#  define TCP_KEEPIDLE 4
# endif
#endif
#ifndef TCP_KEEPINTVL
# ifdef OPT_TCP_KEEPINTVL
#  define TCP_KEEPINTVL OPT_TCP_KEEPINTVL
# else
#  define TCP_KEEPINTVL 5
# endif
#endif
#ifndef TCP_KEEPCNT
# ifdef OPT_TCP_KEEPCNT
#  define TCP_KEEPCNT OPT_TCP_KEEPCNT
# else
#  define TCP_KEEPCNT 6
# endif
#endif

#endif /* WKC_WIIU_WEBRTC_COMPAT_NETINET_TCP_H */
