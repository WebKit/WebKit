/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef FullScreenVideoQt_h
#define FullScreenVideoQt_h

#include "qwebkitplatformplugin.h"
#include <QObject>

QT_BEGIN_NAMESPACE
class QGraphicsVideoItem;
class QMediaPlayer;
QT_END_NAMESPACE

namespace WebCore {

class ChromeClientQt;
class FullScreenVideoWidget;
class HTMLVideoElement;
class Node;
class MediaPlayerPrivateQt;

class DefaultFullScreenVideoHandler : public QWebFullScreenVideoHandler {
    Q_OBJECT
public:
    DefaultFullScreenVideoHandler();
    virtual ~DefaultFullScreenVideoHandler();
    bool requiresFullScreenForVideoPlayback() const;

public Q_SLOTS:
    void enterFullScreen(QMediaPlayer*);
    void exitFullScreen();

private:
    static bool s_shouldForceFullScreenVideoPlayback;
    FullScreenVideoWidget *m_fullScreenWidget;
};

class FullScreenVideoQt : public QObject {
    Q_OBJECT
public:
    FullScreenVideoQt(ChromeClientQt*);
    ~FullScreenVideoQt();

    virtual void enterFullScreenForNode(Node*);
    virtual void exitFullScreenForNode(Node*);
    bool requiresFullScreenForVideoPlayback();
    bool isValid() const {  return m_FullScreenVideoHandler; }

private:
    MediaPlayerPrivateQt* mediaPlayer();
    MediaPlayerPrivateQt* mediaPlayerForNode(Node* = 0);

private slots:
    void aboutToClose();

private:
    ChromeClientQt* m_chromeClient;
    HTMLVideoElement* m_videoElement;
    QWebFullScreenVideoHandler* m_FullScreenVideoHandler;
};

}

#endif // FullScreenVideoQt_h
