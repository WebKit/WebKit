/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <QWidget>

#include "config.h"
#include "Page.h"

#include "IntRect.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"

namespace WebCore {

static QWidget* rootWindowForFrame(const Frame* frame)
{
    FrameView* frameView = (frame ? frame->view() : 0);
    if (!frameView)
        return 0;

    return frameView->qwidget();
}

FloatRect Page::windowRect() const
{
    QWidget* widget = rootWindowForFrame(mainFrame());
    if (!widget)
        return FloatRect();

    return (IntRect) widget->geometry();
}

void Page::setWindowRect(const FloatRect& r)
{
    QWidget* widget = rootWindowForFrame(mainFrame());
    if (widget)
        widget->setGeometry(QRect(qRound(r.x()), qRound(r.y()), qRound(r.width()), qRound(r.height())));
}

}

// vim: ts=4 sw=4 et
