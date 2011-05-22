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

#ifndef HTML5VideoPlugin_h
#define HTML5VideoPlugin_h

#include "HTML5VideoWidget.h"
#include "qwebkitplatformplugin.h"
#include <QMediaPlayer>

#include <aknappui.h>
#include <aknenv.h>
#include <eikappui.h>
#include <eikenv.h>

class HTML5FullScreenVideoHandler : public QWebFullScreenVideoHandler {
    Q_OBJECT
public:
    HTML5FullScreenVideoHandler();
    bool requiresFullScreenForVideoPlayback() const { return true; }
    bool isFullScreen() const { return m_fullScreen; }
    void updateScreenRect(const QRect&) { }

public Q_SLOTS:
    virtual void enterFullScreen(QMediaPlayer *);
    virtual void exitFullScreen();
    void onPlayerStateChanged(QMediaPlayer::State);
    void onPlayerError(QMediaPlayer::Error);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus);

private:
    HTML5VideoWidget* m_videoWidget;
    QMediaPlayer* m_mediaPlayer;
    bool m_fullScreen;
    CAknAppUi::TAppUiOrientation m_savedOrientation;
};

class HTML5VideoPlugin : public QObject, public QWebKitPlatformPlugin {
    Q_OBJECT
    Q_INTERFACES(QWebKitPlatformPlugin)
public:
    virtual bool supportsExtension(Extension) const;
    virtual QObject* createExtension(Extension) const;
};

#endif /* HTML5VideoPlugin_h */
