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

#ifndef HTML5VideoWidget_h
#define HTML5VideoWidget_h

#include "OverlayWidget.h"

#include <QVideoWidget>

class HTML5VideoWidget : public QVideoWidget {
    Q_OBJECT
public:
    HTML5VideoWidget(QWidget* parent = 0);
    void mousePressEvent(QMouseEvent*);
    void onPlayerStopped();
    void onPlayerError();
    void onEndOfMedia();
    void onBufferingMedia();
    void onBufferedMedia();
    void setVolume(int);

private slots:
    void onControlClicked(bool);
    void onPositionChanged(qint64);
    void onSliderMoved(int);
    void onCloseClicked();

public slots:
    void showFullScreen();
    void setDuration(qint64);

signals:
    void pauseClicked();
    void playClicked();
    void positionChangedByUser(qint64);
    void closeClicked();
    void muted(bool);
    void volumeChanged(int);

private:
    OverlayWidget* m_overlayWidget;
};

#endif /* HTML5VideoWidget_h */
