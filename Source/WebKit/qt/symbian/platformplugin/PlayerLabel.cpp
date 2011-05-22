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

#include "PlayerLabel.h"

PlayerLabel::PlayerLabel(QWidget* parent)
    : QLabel(parent)
    , m_bufferingAnimationIterator(m_bufferingAnimation)
    , m_animationTimer(this)
{
    m_bufferingAnimation.append(QPixmap(":/images/loading_buffering_1.png").scaled(QSize(80, 80)));
    m_bufferingAnimation.append(QPixmap(":/images/loading_buffering_2.png").scaled(QSize(80, 80)));
    m_bufferingAnimation.append(QPixmap(":/images/loading_buffering_3.png").scaled(QSize(80, 80)));
    m_bufferingAnimation.append(QPixmap(":/images/loading_buffering_4.png").scaled(QSize(80, 80)));
    m_bufferingAnimationIterator = m_bufferingAnimation;
    m_animationTimer.setSingleShot(false);
    m_animationTimer.setInterval(200);
    connect(&m_animationTimer, SIGNAL(timeout()), this, SLOT(onAnimationTimeout()));

    m_cannotplay = QPixmap(":/images/button_cannotplay.png").scaled(QSize(80, 80));
}

void PlayerLabel::onPlayerError()
{
    setPixmap(m_cannotplay);
    show();
}

void PlayerLabel::startBufferingAnimation()
{
    m_bufferingAnimationIterator.toFront();
    setPixmap(m_bufferingAnimationIterator.next());
    show();
    m_animationTimer.start();
}

void PlayerLabel::stopBufferingAnimation()
{
    hide();
    m_animationTimer.stop();
}

void PlayerLabel::onAnimationTimeout()
{
    if (!m_bufferingAnimationIterator.hasNext())
        m_bufferingAnimationIterator.toFront();
    setPixmap(m_bufferingAnimationIterator.next());
}
