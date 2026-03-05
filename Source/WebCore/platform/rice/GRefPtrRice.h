/*
 *  Copyright (C) 2025 Igalia S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#pragma once

#if USE(LIBRICE)
#include <rice-io.h>
#include <wtf/glib/GRefPtr.h>

namespace WTF {

WTF_DEFINE_GREF_TRAITS_INLINE(RiceAgent, rice_agent_ref, rice_agent_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(RiceStream, rice_stream_ref, rice_stream_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(RiceSockets, rice_sockets_ref, rice_sockets_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(RiceComponent, rice_component_ref, rice_component_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(RiceUdpSocket, rice_udp_socket_ref, rice_udp_socket_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(RiceTlsConfig, rice_tls_config_ref, rice_tls_config_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(RiceTurnConfig, rice_turn_config_ref, rice_turn_config_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(RiceTcpListener, rice_tcp_listener_ref, rice_tcp_listener_unref)

} // namespace WTF

#endif // USE(LIBRICE)
