/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
 *
 */

#include "config.h"
#include "FullScreenVideoQt.h"

#include "ChromeClientQt.h"
#include "HTMLNames.h"
#include "HTMLVideoElement.h"
#include "MediaPlayerPrivateQt.h"
#include "Node.h"
#include "qwebkitplatformplugin.h"

#include <QGraphicsVideoItem>

namespace WebCore {

FullScreenVideoQt::FullScreenVideoQt(ChromeClientQt* chromeClient)
    : m_chromeClient(chromeClient)
    , m_videoElement(0)
{
    Q_ASSERT(m_chromeClient);
    m_FullScreenVideoHandler = m_chromeClient->m_platformPlugin.createFullScreenVideoHandler();

    if (m_FullScreenVideoHandler)
        connect(m_FullScreenVideoHandler, SIGNAL(fullScreenClosed()), this, SLOT(aboutToClose()));
}

FullScreenVideoQt::~FullScreenVideoQt()
{
    delete m_FullScreenVideoHandler;
}

void FullScreenVideoQt::enterFullScreenForNode(Node* node)
{
    Q_ASSERT(node);
    Q_ASSERT(m_FullScreenVideoHandler);

    if (!m_FullScreenVideoHandler)
        return;

    MediaPlayerPrivateQt* mediaPlayerQt = mediaPlayerForNode(node);
    mediaPlayerQt->removeVideoItem();
    m_FullScreenVideoHandler->enterFullScreen(mediaPlayerQt->mediaPlayer());
}

void FullScreenVideoQt::exitFullScreenForNode(Node* node)
{
    Q_ASSERT(node);
    Q_ASSERT(m_FullScreenVideoHandler);

    if (!m_FullScreenVideoHandler)
        return;

    m_FullScreenVideoHandler->exitFullScreen();
    MediaPlayerPrivateQt* mediaPlayerQt = mediaPlayerForNode(node);
    mediaPlayerQt->restoreVideoItem();
}

void FullScreenVideoQt::aboutToClose()
{
    Q_ASSERT(m_videoElement);
    m_videoElement->exitFullscreen();
}

MediaPlayerPrivateQt* FullScreenVideoQt::mediaPlayer()
{
    Q_ASSERT(m_videoElement);
    PlatformMedia platformMedia = m_videoElement->platformMedia();
    return static_cast<MediaPlayerPrivateQt*>(platformMedia.media.qtMediaPlayer);
}

MediaPlayerPrivateQt* FullScreenVideoQt::mediaPlayerForNode(Node* node)
{
    Q_ASSERT(node);
    if (node)
        m_videoElement = static_cast<HTMLVideoElement*>(node);
    return mediaPlayer();
}

bool FullScreenVideoQt::requiresFullScreenForVideoPlayback()
{
    return m_FullScreenVideoHandler ? m_FullScreenVideoHandler->requiresFullScreenForVideoPlayback() : false;
}

}

