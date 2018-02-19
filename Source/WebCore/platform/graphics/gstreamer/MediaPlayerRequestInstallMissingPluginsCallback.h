/*
 * Copyright (C) 2015 Igalia S.L.
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
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef MediaPlayerRequestInstallMissingPluginsCallback_h
#define MediaPlayerRequestInstallMissingPluginsCallback_h

#if ENABLE(VIDEO) && USE(GSTREAMER)
#include <wtf/Function.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class MediaPlayerRequestInstallMissingPluginsCallback : public RefCounted<MediaPlayerRequestInstallMissingPluginsCallback> {
    WTF_MAKE_FAST_ALLOCATED();
public:
    using MediaPlayerRequestInstallMissingPluginsCallbackFunction = std::function<void(uint32_t, MediaPlayerRequestInstallMissingPluginsCallback&)>;

    static Ref<MediaPlayerRequestInstallMissingPluginsCallback> create(MediaPlayerRequestInstallMissingPluginsCallbackFunction&& function)
    {
        return adoptRef(*new MediaPlayerRequestInstallMissingPluginsCallback(WTFMove(function)));
    }

    void invalidate()
    {
        m_function = nullptr;
    }

    void complete(uint32_t result)
    {
        if (!m_function)
            return;
        m_function(result, *this);
        m_function = nullptr;
    }

private:
    MediaPlayerRequestInstallMissingPluginsCallback(MediaPlayerRequestInstallMissingPluginsCallbackFunction&& function)
        : m_function(WTFMove(function))
    {
    }

    MediaPlayerRequestInstallMissingPluginsCallbackFunction m_function;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
#endif // MediaPlayerRequestInstallMissingPluginsCallback_h
