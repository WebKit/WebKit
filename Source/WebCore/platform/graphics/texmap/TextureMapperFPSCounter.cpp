/*
    Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2012, 2013 Company 100, Inc.
    Copyright (C) 2012, 2013 basysKom GmbH

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

#include "config.h"

#include "TextureMapperFPSCounter.h"

#include "TextureMapper.h"
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextureMapperFPSCounter);

TextureMapperFPSCounter::TextureMapperFPSCounter()
    : m_isShowingFPS(false)
    , m_fpsInterval(0_s)
    , m_lastFPS(0.0)
    , m_frameCount(0)
{
    auto showFPSEnvironment = String::fromLatin1(getenv("WEBKIT_SHOW_FPS"));
    bool ok = false;
    m_fpsInterval = Seconds(showFPSEnvironment.toDouble(&ok));
    if (ok && m_fpsInterval) {
        m_isShowingFPS = true;
        m_fpsTimestamp = MonotonicTime::now();
    }
}

void TextureMapperFPSCounter::updateFPSAndDisplay(TextureMapper& textureMapper, const FloatPoint& location, const TransformationMatrix& matrix)
{
    m_frameCount++;
    Seconds delta = MonotonicTime::now() - m_fpsTimestamp;
    if (delta >= m_fpsInterval) {
        m_lastFPS = m_frameCount / delta.seconds();
        m_frameCount = 0;
        m_fpsTimestamp += delta;
    }

    WTFSetCounterDouble(FPS, m_lastFPS);

    if (m_isShowingFPS)
        textureMapper.drawNumber(m_lastFPS, Color::black, location, matrix);
}

} // namespace WebCore
