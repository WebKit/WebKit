/*
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

#include "config.h"

#include <QComboBox>

#include "PlatformComboBox.h"
#include "IntSize.h"
#include "IntRect.h"

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); } while(0)

namespace WebCore {

PlatformComboBox::PlatformComboBox()
    : Widget()
    , m_comboBox(0)
{
}

PlatformComboBox::~PlatformComboBox()
{
}

void PlatformComboBox::setParentWidget(QWidget* parent)
{
    Widget::setParentWidget(parent);

    Q_ASSERT(m_comboBox == 0);
    m_comboBox = new QComboBox(parent);

    setQWidget(m_comboBox);
}

void PlatformComboBox::clear()
{
    m_comboBox->clear();
}

void PlatformComboBox::appendItem(const DeprecatedString& text, bool enabled)
{
    m_comboBox->addItem(text);
}

void PlatformComboBox::appendGroupLabel(const DeprecatedString& text)
{
    // FIXME: Group label?
    m_comboBox->addItem(text);
}

void PlatformComboBox::appendSeparator()
{
    notImplemented();
}

void PlatformComboBox::setCurrentItem(int index)
{
    m_comboBox->setCurrentIndex(index);
}

IntSize PlatformComboBox::sizeHint() const
{
    return m_comboBox->sizeHint();
}

IntRect PlatformComboBox::frameGeometry() const
{
    return Widget::frameGeometry();
}

void PlatformComboBox::setFrameGeometry(const IntRect& r)
{
    return Widget::setFrameGeometry(r);
}

int PlatformComboBox::baselinePosition(int height) const
{
    notImplemented();
    return 0;
}

void PlatformComboBox::setFont(const Font& font)
{
    m_comboBox->setFont(font);
}

Widget::FocusPolicy PlatformComboBox::focusPolicy() const
{
    return Widget::focusPolicy();
}

void PlatformComboBox::itemSelected()
{
    notImplemented();
}

void PlatformComboBox::setWritingDirection(TextDirection dir)
{
    Qt::LayoutDirection qDir;

    switch(dir)
    {
        case LTR:
            qDir = Qt::LeftToRight;
            break;
        case RTL:
            qDir = Qt::RightToLeft;
            break;
    }

    m_comboBox->setLayoutDirection(qDir);
}

void PlatformComboBox::populate()
{
    populateMenu();
}

void PlatformComboBox::populateMenu()
{
    notImplemented();
}

}

// vim: ts=4 sw=4 et
