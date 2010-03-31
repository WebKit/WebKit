/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
#include "config.h"
#include "Maemo5Webstyle.h"

#include <QPainter>
#include <QPixmapCache>
#include <QStyleOption>

Maemo5WebStyle::Maemo5WebStyle()
{
}

static inline void drawRectangularControlBackground(QPainter* painter, const QPen& pen, const QRect& rect, const QBrush& brush)
{
    QPen oldPen = painter->pen();
    QBrush oldBrush = painter->brush();
    painter->setPen(pen);
    painter->setBrush(brush);

    int line = 1;
    painter->drawRect(rect.adjusted(line, line, -line, -line));

    painter->setPen(oldPen);
    painter->setBrush(oldBrush);
}

void Maemo5WebStyle::drawChecker(QPainter* painter, int size, QColor color) const
{
    int border = qMin(qMax(1, int(0.2 * size)), 6);
    int checkerSize = size - 2 * border;
    int width = checkerSize / 3;
    int middle = qMax(3 * checkerSize / 7, 3);
    int x = ((size - checkerSize) >> 1);
    int y = ((size - checkerSize) >> 1) + (checkerSize - width - middle);
    QVector<QLineF> lines(checkerSize + 1);
    painter->setPen(color);
    for (int i = 0; i < middle; ++i) {
        lines[i] = QLineF(x, y, x, y + width);
        ++x;
        ++y;
    }
    for (int i = middle; i <= checkerSize; ++i) {
        lines[i] = QLineF(x, y, x, y + width);
        ++x;
        --y;
    }
    painter->drawLines(lines.constData(), lines.size());
}

QPixmap Maemo5WebStyle::findChecker(const QRect& rect, bool disabled) const
{
    int size = qMin(rect.width(), rect.height());
    QPixmap result;
    static const QString prefix = "$qt-maemo5-" + QLatin1String(metaObject()->className()) + "-checker-";
    QString key = prefix + QString::number(size) + "-" + (disabled ? "disabled" : "enabled");
    if (!QPixmapCache::find(key, result)) {
        result = QPixmap(size, size);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        drawChecker(&painter, size, disabled ? Qt::gray : Qt::black);
        QPixmapCache::insert(key, result);
    }
    return result;
}

void Maemo5WebStyle::drawRadio(QPainter* painter, const QSize& size, bool checked, QColor color) const
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    // deflate one pixel
    QRect rect = QRect(QPoint(1, 1), QSize(size.width() - 2, size.height() - 2));

    QPen pen(Qt::black);
    pen.setWidth(1);
    painter->setPen(color);
    painter->setBrush(Qt::white);
    painter->drawEllipse(rect);
    int border = 0.1 * (rect.width() + rect.height());
    border = qMin(qMax(2, border), 10);
    rect.adjust(border, border, -border, -border);
    if (checked) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawEllipse(rect);
    }
}

QPixmap Maemo5WebStyle::findRadio(const QSize& size, bool checked, bool disabled) const
{
    QPixmap result;
    static const QString prefix = "$qt-maemo5-" + QLatin1String(metaObject()->className()) + "-radio-";
    QString key = prefix + QString::number(size.width()) + "-" + QString::number(size.height()) +
                   + "-" + (disabled ? "disabled" : "enabled") + (checked ? "-checked" : "");
    if (!QPixmapCache::find(key, result)) {
        result = QPixmap(size);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        drawRadio(&painter, size, checked, disabled ? Qt::gray : Qt::black);
        QPixmapCache::insert(key, result);
    }
    return result;
}

void Maemo5WebStyle::drawControl(ControlElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget) const
{
    switch (element) {
    case CE_CheckBox: {
        QRect rect = option->rect;
        const bool disabled = !(option->state & State_Enabled);
        drawRectangularControlBackground(painter, QPen(disabled ? Qt::gray : Qt::black), rect, option->palette.base());
        rect.adjust(1, 1, -1, -1);

        if (option->state & State_Off)
            break;

        QPixmap checker = findChecker(rect, disabled);
        if (checker.isNull())
            break;

        int x = (rect.width() - checker.width()) >> 1;
        int y = (rect.height() - checker.height()) >> 1;
        painter->drawPixmap(rect.x() + x, rect.y() + y, checker);
        break;
    }
    case CE_RadioButton: {
        const bool disabled = !(option->state & State_Enabled);
        QPixmap radio = findRadio(option->rect.size(), option->state & State_On, disabled);
        if (radio.isNull())
            break;
        painter->drawPixmap(option->rect.x(), option->rect.y(), radio);
        break;
    }
    default:
        QWindowsStyle::drawControl(element, option, painter, widget);
    }
}

void Maemo5WebStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex* option, QPainter* painter, const QWidget* widget) const
{
    switch (control) {
    case CC_ComboBox: {
        const QStyleOptionComboBox* cmb = qstyleoption_cast<const QStyleOptionComboBox*>(option);

        if (!cmb) {
            QWindowsStyle::drawComplexControl(control, option, painter, widget);
            break;
        }

        if (!(cmb->subControls & SC_ComboBoxArrow))
            break;

        QStyleOption arrowOpt(0);
        arrowOpt.rect = subControlRect(CC_ComboBox, cmb, SC_ComboBoxArrow, widget);

        arrowOpt.rect.adjust(3, 3, -3, -3);
        arrowOpt.palette = cmb->palette;
        arrowOpt.state = State_Enabled;
        drawPrimitive(PE_IndicatorArrowDown, &arrowOpt, painter, widget);
        break;
    }
    default:
        QWindowsStyle::drawComplexControl(control, option, painter, widget);
    }
}
