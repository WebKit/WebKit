/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#pragma once

#include "FrameDestructionObserver.h"
#include "PluginData.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class DOMPlugin;

class DOMMimeType : public RefCounted<DOMMimeType>, public FrameDestructionObserver {
public:
    static Ref<DOMMimeType> create(RefPtr<PluginData>&& pluginData, Frame* frame, unsigned index) { return adoptRef(*new DOMMimeType(WTFMove(pluginData), frame, index)); }
    ~DOMMimeType();

    String type() const;
    String suffixes() const;
    String description() const;
    RefPtr<DOMPlugin> enabledPlugin() const;

private:
    DOMMimeType(RefPtr<PluginData>&&, Frame*, unsigned index);
    MimeClassInfo m_mimeClassInfo;
    RefPtr<PluginData> m_pluginData;
    PluginInfo m_pluginInfo;
};

} // namespace WebCore
