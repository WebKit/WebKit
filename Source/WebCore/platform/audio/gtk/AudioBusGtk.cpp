/*
 *  Copyright (C) 2011 Igalia S.L
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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioBus.h"

#include "AudioFileReader.h"
#include <gio/gio.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/gobject/GUniquePtr.h>

namespace WebCore {

PassRefPtr<AudioBus> AudioBus::loadPlatformResource(const char* name, float sampleRate)
{
    GUniquePtr<char> path(g_strdup_printf("/org/webkitgtk/resources/audio/%s", name));
    GRefPtr<GBytes> data = adoptGRef(g_resources_lookup_data(path.get(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr));
    ASSERT(data);
    return createBusFromInMemoryAudioFile(g_bytes_get_data(data.get(), nullptr), g_bytes_get_size(data.get()), false, sampleRate);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
