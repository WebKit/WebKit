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

#include "HTML5VideoPlugin.h"

#include <QtPlugin>

HTML5FullScreenVideoHandler::HTML5FullScreenVideoHandler()
    : m_videoWidget(0)
    , m_mediaPlayer(0)
    , m_fullScreen(false)
    , m_savedOrientation(CAknAppUi::EAppUiOrientationUnspecified)
{
}

void HTML5FullScreenVideoHandler::enterFullScreen(QMediaPlayer *player)
{
    if (!player)
        return;

    m_videoWidget = new HTML5VideoWidget();
    if (!m_videoWidget)
        return;

    m_videoWidget->setDuration(player->duration() / 1000);

    CAknAppUi* appUi = dynamic_cast<CAknAppUi*>(CEikonEnv::Static()->AppUi());
    if (appUi) {
        m_savedOrientation = appUi->Orientation();
        appUi->SetOrientationL(CAknAppUi::EAppUiOrientationLandscape);
    }

    m_mediaPlayer = player;
    connect(m_mediaPlayer, SIGNAL(positionChanged(qint64)), m_videoWidget, SLOT(onPositionChanged(qint64)));
    connect(m_mediaPlayer, SIGNAL(durationChanged(qint64)), m_videoWidget, SLOT(setDuration(qint64)));
    connect(m_mediaPlayer, SIGNAL(stateChanged(QMediaPlayer::State)), this, SLOT(onPlayerStateChanged(QMediaPlayer::State)));
    connect(m_mediaPlayer, SIGNAL(error(QMediaPlayer::Error)), this, SLOT(onPlayerError(QMediaPlayer::Error)));
    connect(m_mediaPlayer, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)), this, SLOT(onMediaStatusChanged(QMediaPlayer::MediaStatus)));
    connect(m_videoWidget, SIGNAL(positionChangedByUser(qint64)), m_mediaPlayer, SLOT(setPosition(qint64)));
    connect(m_videoWidget, SIGNAL(closeClicked()), this, SIGNAL(fullScreenClosed()));
    connect(m_videoWidget, SIGNAL(muted(bool)), m_mediaPlayer, SLOT(setMuted(bool)));
    connect(m_videoWidget, SIGNAL(volumeChanged(int)), m_mediaPlayer, SLOT(setVolume(int)));
    connect(m_videoWidget, SIGNAL(pauseClicked()), m_mediaPlayer, SLOT(pause()));
    connect(m_videoWidget, SIGNAL(playClicked()), m_mediaPlayer, SLOT(play()));

    m_mediaPlayer->setVideoOutput(m_videoWidget);

    m_videoWidget->setVolume(m_mediaPlayer->volume());
    m_videoWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_videoWidget->showFullScreen();
    m_fullScreen = true;

    // Handle current Media Status and Media Player error.
    onMediaStatusChanged(m_mediaPlayer->mediaStatus());
    onPlayerError(m_mediaPlayer->error());
}

void HTML5FullScreenVideoHandler::exitFullScreen()
{
    m_videoWidget->hide();
    m_mediaPlayer->disconnect(m_videoWidget);
    m_mediaPlayer->disconnect(this);
    delete m_videoWidget;
    m_videoWidget = 0;
    if (m_mediaPlayer->state() == QMediaPlayer::PlayingState)
        m_mediaPlayer->pause();
    m_fullScreen = false;

    CAknAppUi* appUi = dynamic_cast<CAknAppUi*>(CEikonEnv::Static()->AppUi());
    if (appUi) 
        appUi->SetOrientationL(m_savedOrientation);
}

void HTML5FullScreenVideoHandler::onPlayerError(QMediaPlayer::Error error)
{
    switch (error) {
    case QMediaPlayer::NoError:
        break;
    default:
        m_videoWidget->onPlayerError();
    }
}

void HTML5FullScreenVideoHandler::onPlayerStateChanged(QMediaPlayer::State state)
{
    if (state == QMediaPlayer::StoppedState)
        m_videoWidget->onPlayerStopped();
}

void HTML5FullScreenVideoHandler::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    switch (status) {
    case QMediaPlayer::EndOfMedia:
        m_videoWidget->onEndOfMedia();
        break;
    case QMediaPlayer::StalledMedia:
    case QMediaPlayer::LoadingMedia:
    case QMediaPlayer::BufferingMedia:
        m_videoWidget->onBufferingMedia();
        break;
    case QMediaPlayer::LoadedMedia:
    case QMediaPlayer::BufferedMedia:
        m_videoWidget->onBufferedMedia();
        break;
    default:
        break;
    }
    return;
}

bool HTML5VideoPlugin::supportsExtension(Extension extension) const
{
    switch (extension) {
    case FullScreenVideoPlayer:
        return true;
    default:
        return false;
    }
}

QObject* HTML5VideoPlugin::createExtension(Extension extension) const
{
    HTML5FullScreenVideoHandler* videoHandler = 0;
    switch (extension) {
    case FullScreenVideoPlayer:
        videoHandler = new HTML5FullScreenVideoHandler;
        break;
    default:
        break;
    }
    return videoHandler;
}
