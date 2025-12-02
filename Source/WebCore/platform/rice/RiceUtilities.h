/*
 *  Copyright (C) 2025 Igalia S.L. All rights reserved.
 *  Copyright (C) 2025 Metrological Group B.V.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#if USE(LIBRICE)

#include "GUniquePtrRice.h"
#include "SharedMemory.h"
#include <wtf/HexNumber.h>
#include <wtf/Scope.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static inline String riceAddressToString(const RiceAddress* address, bool includePort = true)
{
    std::array<uint8_t, 16> bytes;
    auto size = rice_address_get_address_bytes(address, bytes.data());
    StringBuilder builder;

    switch (rice_address_get_family(address)) {
    case RICE_ADDRESS_FAMILY_IPV4:
        // Maximum capacity for 255.255.255.255:65535
        builder.reserveCapacity(includePort ? 21 : 15);
        for (unsigned i = 0; i < size; i++) {
            if (i)
                builder.append('.');
            builder.append(static_cast<int>(bytes[i]));
        }
        break;
    case RICE_ADDRESS_FAMILY_IPV6:
        builder.reserveCapacity(includePort ? 47 : 39);
        if (includePort)
            builder.append('[');
        for (unsigned i = 0; i < size; i++) {
            if (i && !(i % 2))
                builder.append(':');
            builder.append(hex(bytes[i], 2));
        }
        if (includePort)
            builder.append(']');
        break;
    }
    if (includePort)
        builder.append(':', rice_address_get_port(address));
    builder.shrinkToFit();
    return builder.toString();
}

static inline GUniquePtr<RiceAddress> riceAddressFromString(const String& address)
{
    GUniquePtr<RiceAddress> result(rice_address_new_from_string(address.ascii().data()));
    return result;
}

static inline std::optional<SharedMemory::Handle> riceTransmitToSharedMemoryHandle(RiceTransmit* /* transfer full */ transmit)
{
    auto scopeExit = makeScopeExit([&] {
        rice_transmit_clear(transmit);
    });

    return SharedMemoryHandle::createCopy(unsafeMakeSpan(transmit->data.ptr, transmit->data.size), SharedMemoryProtection::ReadOnly);
}

static inline RTCIceProtocol riceTransmitTransportToIceProtocol(const RiceTransmit& transmit)
{
    switch (transmit.transport) {
    case RICE_TRANSPORT_TYPE_TCP:
        return RTCIceProtocol::Tcp;
    case RICE_TRANSPORT_TYPE_UDP:
        return RTCIceProtocol::Udp;
    }

    return RTCIceProtocol::Udp;
}


} // namespace WebCore

#endif // USE(LIBRICE)
