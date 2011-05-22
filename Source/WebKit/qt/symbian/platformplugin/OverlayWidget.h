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

#ifndef OverlayWidget_h
#define OverlayWidget_h

#include "PlayerButton.h"
#include "PlayerLabel.h"

#include <QSlider>

class OverlayWidget : public QWidget {
    Q_OBJECT
public:
    OverlayWidget(QWidget* parent = 0);
    ~OverlayWidget();
    void setDuration(int);
    void setPosition(int);
    void setVolume(int);
    void mousePressEvent(QMouseEvent*);
    void onPlayerStopped();
    void onPlayerError();
    void onEndOfMedia();
    void onBufferingMedia();
    void onBufferedMedia();

private:
    QString timeToString(int);
    void applyStyleSheet();

private slots:
    void onControlClicked();
    void onSliderMoved(int);
    void onSoundClicked();
    void onCloseClicked();
    void onVolumeSliderReleased();
    void onVolumeSliderMoved(int);
    void onTimerTimeout();

public slots:
    void showFullScreen();

signals:
    void controlClicked(bool);
    void sliderMoved(int);
    void closeClicked();
    void muted(bool);
    void volumeChanged(int);

private:
    PlayerButton* m_controlButton;
    bool m_isPaused;
    QSlider* m_progressSlider;
    QLabel* m_positionLabel;
    QLabel* m_durationLabel;
    PlayerButton* m_soundButton;
    PlayerButton* m_closeButton;
    QSlider* m_volumeSlider;
    bool m_isMuted;
    QIcon m_playIcon;
    QIcon m_pauseIcon;
    QIcon m_soundOnIcon;
    QIcon m_soundOffIcon;
    QTimer* m_hideWidgetTimer;
    PlayerLabel* m_playerLabel;
};

#endif /* OverlayWidget_h */
