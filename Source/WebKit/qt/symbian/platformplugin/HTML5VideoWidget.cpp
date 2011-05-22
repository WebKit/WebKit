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

#include "HTML5VideoWidget.h"

HTML5VideoWidget::HTML5VideoWidget(QWidget *parent)
    : QVideoWidget(parent)
    , m_overlayWidget(new OverlayWidget(this))
{
    connect(m_overlayWidget, SIGNAL(controlClicked(bool)), this, SLOT(onControlClicked(bool)));
    connect(m_overlayWidget, SIGNAL(sliderMoved(int)), this, SLOT(onSliderMoved(int)));
    connect(m_overlayWidget, SIGNAL(closeClicked()), this, SLOT(onCloseClicked()));
    connect(m_overlayWidget, SIGNAL(muted(bool)), this, SIGNAL(muted(bool)));
    connect(m_overlayWidget, SIGNAL(volumeChanged(int)), this, SIGNAL(volumeChanged(int)));
    m_overlayWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}

void HTML5VideoWidget::setDuration(qint64 duration)
{
    m_overlayWidget->setDuration(duration / 1000);
}

void HTML5VideoWidget::mousePressEvent(QMouseEvent *)
{
    m_overlayWidget->showFullScreen();
}

void HTML5VideoWidget::onPlayerStopped()
{
    m_overlayWidget->onPlayerStopped();
}

void HTML5VideoWidget::onPlayerError()
{
    m_overlayWidget->onPlayerError();
}

void HTML5VideoWidget::onEndOfMedia()
{
    m_overlayWidget->onEndOfMedia();
}

void HTML5VideoWidget::onBufferingMedia()
{
    m_overlayWidget->onBufferingMedia();
}

void HTML5VideoWidget::onBufferedMedia()
{
    m_overlayWidget->onBufferedMedia();
}

void HTML5VideoWidget::onControlClicked(bool isPaused)
{
    if (isPaused)
        emit pauseClicked();
    else
        emit playClicked();
}

void HTML5VideoWidget::onPositionChanged(qint64 position)
{
    m_overlayWidget->setPosition(position / 1000);
}

void HTML5VideoWidget::onSliderMoved(int value)
{
    emit positionChangedByUser(((qint64)value)*1000);
}

void HTML5VideoWidget::onCloseClicked()
{
    hide();
    emit closeClicked();
}

void HTML5VideoWidget::showFullScreen()
{
    setFullScreen(true);
    m_overlayWidget->showFullScreen();
}

void HTML5VideoWidget::setVolume(int volume)
{
    m_overlayWidget->setVolume(volume);
}
