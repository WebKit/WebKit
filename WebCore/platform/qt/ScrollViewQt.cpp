/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
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

#include "config.h"
#include "ScrollView.h"

#include "FrameView.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "IntPoint.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "NotImplemented.h"
#include "Frame.h"
#include "Page.h"
#include "GraphicsContext.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"

#include <QDebug>
#include <QWidget>
#include <QPainter>
#include <QApplication>
#include <QPalette>
#include <QStyleOption>

#ifdef Q_WS_MAC
#include <Carbon/Carbon.h>
#endif

#include "qwebframe.h"
#include "qwebpage.h"

// #define DEBUG_SCROLLVIEW

namespace WebCore {

ScrollView::ScrollView()
{
    init();
    m_widgetsThatPreventBlitting = 0;
}

ScrollView::~ScrollView()
{
    destroy();
}

void ScrollView::platformAddChild(Widget* child)
{
    root()->m_widgetsThatPreventBlitting++;
    if (parent())
        parent()->platformAddChild(child);
}

void ScrollView::platformRemoveChild(Widget* child)
{
    ASSERT(root()->m_widgetsThatPreventBlitting);
    root()->m_widgetsThatPreventBlitting--;
    child->hide();
}

void ScrollView::addToDirtyRegion(const IntRect& containingWindowRect)
{
    ASSERT(isFrameView());
    const FrameView* frameView = static_cast<const FrameView*>(this);
    Page* page = frameView->frame() ? frameView->frame()->page() : 0;
    if (!page)
        return;
    page->chrome()->addToDirtyRegion(containingWindowRect);
}

}

// vim: ts=4 sw=4 et
