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

#include "OverlayWidget.h"

#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QTime>

OverlayWidget::OverlayWidget(QWidget *parent)
    : QWidget(0, Qt::FramelessWindowHint)
    , m_isPaused(false)
    , m_isMuted(false)
{
    applyStyleSheet();

    m_playIcon = QIcon(":/images/button_play.png");
    m_pauseIcon = QIcon(":/images/button_pause.png");
    m_soundOnIcon = QIcon(":/images/button_sound_on.png");
    m_soundOffIcon = QIcon(":/images/button_sound_off.png");

    m_controlButton = new PlayerButton(this);
    m_controlButton->setIcon(m_pauseIcon);
    connect(m_controlButton, SIGNAL(clicked()), this, SLOT(onControlClicked()));

    m_progressSlider = new QSlider(Qt::Horizontal, this);
    connect(m_progressSlider, SIGNAL(sliderMoved(int)), this, SLOT(onSliderMoved(int)));

    m_positionLabel = new QLabel(this);
    m_positionLabel->setText(timeToString(0));
    m_durationLabel = new QLabel(this);

    m_soundButton = new PlayerButton(this);
    m_soundButton->setIcon(m_soundOnIcon);
    connect(m_soundButton, SIGNAL(clicked()), this, SLOT(onSoundClicked()));

    m_closeButton = new PlayerButton(this);
    m_closeButton->setIcon(QIcon(":/images/button_close.png"));
    connect(m_closeButton, SIGNAL(clicked()), this, SLOT(onCloseClicked()));

    m_volumeSlider = new QSlider(Qt::Vertical, this);
    m_volumeSlider->setRange(0, 100);
    connect(m_soundButton, SIGNAL(tapAndHold()), m_volumeSlider, SLOT(show()));
    connect(m_volumeSlider, SIGNAL(sliderReleased()), this, SLOT(onVolumeSliderReleased()));
    connect(m_volumeSlider, SIGNAL(sliderMoved(int)), this, SLOT(onVolumeSliderMoved(int)));
    m_volumeSlider->hide();

    m_playerLabel = new PlayerLabel(this);
    m_playerLabel->hide();

    QBoxLayout* m_controlLayout = new QHBoxLayout;
    m_controlLayout->addWidget(m_controlButton);
    m_controlLayout->addWidget(m_positionLabel);
    m_controlLayout->addWidget(m_progressSlider);
    m_controlLayout->addWidget(m_durationLabel);

    QGridLayout* layout = new QGridLayout;
    layout->addWidget(m_closeButton, 0, 0, 1, 2, Qt::AlignRight | Qt::AlignTop);
    layout->addWidget(m_playerLabel, 1, 0, Qt::AlignCenter);
    layout->addWidget(m_volumeSlider, 1, 1);
    layout->addLayout(m_controlLayout, 2, 0, Qt::AlignBottom);
    layout->addWidget(m_soundButton, 2, 1, Qt::AlignBottom);
    setLayout(layout);

    m_hideWidgetTimer = new QTimer(this);
    m_hideWidgetTimer->setSingleShot(true);
    m_hideWidgetTimer->setInterval(3000);
    connect(m_hideWidgetTimer, SIGNAL(timeout()), this, SLOT(onTimerTimeout()));
    connect(m_progressSlider, SIGNAL(sliderPressed()), m_hideWidgetTimer, SLOT(stop()));
    connect(m_progressSlider, SIGNAL(sliderReleased()), m_hideWidgetTimer, SLOT(start()));
    connect(m_volumeSlider, SIGNAL(sliderPressed()), m_hideWidgetTimer, SLOT(stop()));

    setAttribute(Qt::WA_TranslucentBackground);
}

OverlayWidget::~OverlayWidget()
{
}

void OverlayWidget::setDuration(int duration)
{
    m_progressSlider->setMaximum(duration);
    m_durationLabel->setText(timeToString(duration));
}

void OverlayWidget::setPosition(int position)
{
    if (!m_progressSlider->isSliderDown()) {
        m_progressSlider->setValue(position);
        m_positionLabel->setText(timeToString(position));
    }
}

void OverlayWidget::setVolume(int volume)
{
    m_volumeSlider->setValue(volume);
}

void OverlayWidget::mousePressEvent(QMouseEvent *)
{
    m_hideWidgetTimer->stop();
    hide();
}

void OverlayWidget::onPlayerStopped()
{
    m_controlButton->setIcon(m_playIcon);
}

void OverlayWidget::onPlayerError()
{
    m_controlButton->setDisabled(true);
    m_progressSlider->setDisabled(true);
    m_playerLabel->onPlayerError();
    showFullScreen();
}

void OverlayWidget::onEndOfMedia()
{
    QWidget::showFullScreen();
}

void OverlayWidget::onBufferingMedia()
{
    m_playerLabel->startBufferingAnimation();
}

void OverlayWidget::onBufferedMedia()
{
    m_playerLabel->stopBufferingAnimation();
}

QString OverlayWidget::timeToString(int seconds)
{
    QTime initTime(0, 0, 0);
    QTime time = initTime.addSecs(seconds);
    return time.hour() ? time.toString("h:mm:ss") : time.toString("mm:ss");
}

void OverlayWidget::applyStyleSheet()
{
    QFile file(":/qss/OverlayWidget.qss");
    file.open(QFile::ReadOnly);
    setStyleSheet(QLatin1String(file.readAll()));
}

void OverlayWidget::onControlClicked()
{
    m_isPaused = !m_isPaused;
    if (m_isPaused) {
        m_controlButton->setIcon(m_playIcon);
        m_hideWidgetTimer->stop();
    } else {
        m_controlButton->setIcon(m_pauseIcon);
        m_hideWidgetTimer->start();
    }
    emit controlClicked(m_isPaused);
}

void OverlayWidget::onSliderMoved(int value)
{
    emit sliderMoved(value);
}

void OverlayWidget::onSoundClicked()
{
    if (m_volumeSlider->isVisible())
        m_isMuted = false;
    else
        m_isMuted = !m_isMuted;

    if (m_isMuted)
        m_soundButton->setIcon(m_soundOffIcon);
    else
        m_soundButton->setIcon(m_soundOnIcon);

    m_hideWidgetTimer->start();
    emit muted(m_isMuted);
}

void OverlayWidget::onCloseClicked()
{
    m_hideWidgetTimer->stop();
    hide();
    emit closeClicked();
}

void OverlayWidget::onVolumeSliderReleased()
{
    m_hideWidgetTimer->start();
    emit volumeChanged(m_volumeSlider->value());
    m_volumeSlider->hide();
}

void OverlayWidget::onVolumeSliderMoved(int value)
{
    emit volumeChanged(value);
}

void OverlayWidget::onTimerTimeout()
{
    if (!m_isPaused)
        hide();
}

void OverlayWidget::showFullScreen()
{
    m_hideWidgetTimer->start();
    QWidget::showFullScreen();
}
