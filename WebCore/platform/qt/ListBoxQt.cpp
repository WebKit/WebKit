/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 *           (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#include <QScrollBar>
#include <QListWidget>

#include "IntSize.h"
#include "HelperQt.h"
#include "ListBox.h"

namespace WebCore {

ListBox::ListBox()
    : ScrollView()
    , m_listWidget(0)
    , m_selectionMode(Single)
    , _width(0.0)
    , _widthGood(false)
{
    qDebug("ListBox::ListBox(), this=%p", this);
}

ListBox::~ListBox()
{
    qDebug("ListBox::~ListBox(), this=%p", this);
}

void ListBox::setParentWidget(QWidget* parent)
{
    qDebug("ListBox::setParentWidget(), parent=%p", parent);
    ScrollView::setParentWidget(parent);

    Q_ASSERT(m_listWidget == 0);
    m_listWidget = new QListWidget(parent);

    if(m_selectionMode == Single)
        m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    else
        m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

    setQWidget(m_listWidget);
}

IntSize ListBox::sizeForNumberOfLines(int numLines) const
{
    QSize size(0, 0);

    for (int i = 0; i <= numLines; i++) {
        QListWidgetItem *item = m_listWidget->item(i);
        Q_ASSERT(item != 0);

        QRect visual = m_listWidget->visualItemRect(item);

        size.setWidth(qMax(size.width(), visual.width()));
        size.setHeight(qMax(size.height(), visual.height()));
    }

    // Logic taken from our khtml :-)
    size.setWidth(size.width() + 2 * m_listWidget->frameWidth() + m_listWidget->verticalScrollBar()->sizeHint().width());
    size.setHeight(numLines * size.height() + 2 * m_listWidget->frameWidth());

    return size;
}

void ListBox::setSelectionMode(SelectionMode mode)
{
    // We can not directly assign to m_listWidget here, as
    // setSelectionMode() is called from DeprecatedRenderSelect before
    // setWidget() and our setParentWidget() functions have been called...
    m_selectionMode = mode;
}

void ListBox::clear()
{
    m_listWidget->clear();
}

void ListBox::doneAppendingItems()
{
    // no-op
}

void ListBox::setSelected(int row, bool selected)
{
    QListWidgetItem *item = m_listWidget->item(row);
    if (!item)
        return;

    m_listWidget->setItemSelected(item, selected);
}

bool ListBox::isSelected(int row) const
{
    QListWidgetItem *item = m_listWidget->item(row);
    if (!item)
        return false;

    return m_listWidget->isItemSelected(item);
}

void ListBox::setEnabled(bool value)
{
    m_listWidget->setEnabled(value);
}

bool ListBox::isEnabled()
{
    return m_listWidget->isEnabled();
}

void ListBox::setWritingDirection(TextDirection dir)
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

    m_listWidget->setLayoutDirection(qDir);
}

Widget::FocusPolicy ListBox::focusPolicy() const
{
    return Widget::focusPolicy();
}

bool ListBox::checksDescendantsForFocus() const
{
    return true;
}

void ListBox::clearCachedTextRenderers()
{
    // no-op
}

void ListBox::setFont(const Font& font)
{
    m_listWidget->setFont(toQFont(font));
}

void ListBox::appendItem(const DeprecatedString& string, ListBoxItemType type, bool enabled)
{
    // FIXME: take into account type/enabled...
    Q_ASSERT(m_listWidget != 0);
    (void) new QListWidgetItem(toQString(string), m_listWidget); // No this does not leak...
}

};

// vim: ts=4 sw=4 et
