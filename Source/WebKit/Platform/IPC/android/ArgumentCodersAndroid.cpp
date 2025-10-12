/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ArgumentCodersAndroid.h"

#if OS(ANDROID)

#include "ArgumentCodersUnix.h"
#include "Decoder.h"
#include "Encoder.h"
#include <wtf/unix/UnixFileDescriptor.h>

namespace IPC {

std::optional<UnixFileDescriptor> ArgumentCoder<UnixFileDescriptor>::decode(Decoder& decoder)
{
    if (auto attachment = decoder.takeLastAttachment()) {
        if (holdsAlternative<UnixFileDescriptor>(attachment.value()))
            return { WTFMove(get<UnixFileDescriptor>(attachment.value())) };
    }
    return std::nullopt;
}

void ArgumentCoder<RefPtr<AHardwareBuffer>>::encode(Encoder& encoder, const RefPtr<AHardwareBuffer>& buffer)
{
    encoder.addAttachment(RefPtr { buffer });
}

void ArgumentCoder<RefPtr<AHardwareBuffer>>::encode(Encoder& encoder, RefPtr<AHardwareBuffer>&& buffer)
{
    encoder.addAttachment(WTFMove(buffer));
}

std::optional<RefPtr<AHardwareBuffer>> ArgumentCoder<RefPtr<AHardwareBuffer>>::decode(Decoder& decoder)
{
    if (auto attachment = decoder.takeLastAttachment()) {
        if (holdsAlternative<RefPtr<AHardwareBuffer>>(attachment.value()))
            return { WTFMove(get<RefPtr<AHardwareBuffer>>(attachment.value())) };
    }
    return std::nullopt;
}

}

#endif // OS(ANDROID)
