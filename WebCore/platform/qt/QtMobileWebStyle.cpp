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
#include "QtMobileWebStyle.h"

#include "QtStyleOptionWebComboBox.h"

#include <QPainter>
#include <QPixmapCache>
#include <QStyleOption>

QtMobileWebStyle::QtMobileWebStyle()
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

void QtMobileWebStyle::drawChecker(QPainter* painter, int size, QColor color) const
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

QPixmap QtMobileWebStyle::findChecker(const QRect& rect, bool disabled) const
{
    int size = qMin(rect.width(), rect.height());
    QPixmap result;
    static const QString prefix = "$qt-maemo5-" + QLatin1String(metaObject()->className()) + "-checker-";
    QString key = prefix + QString::number(size) + "-" + (disabled ? "disabled" : "enabled");
    if (!QPixmapCache::find(key, result)) {
        result = QPixmap(size, size);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        drawChecker(&painter, size, disabled ? Qt::lightGray : Qt::darkGray);
        QPixmapCache::insert(key, result);
    }
    return result;
}

void QtMobileWebStyle::drawRadio(QPainter* painter, const QSize& size, bool checked, QColor color) const
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    // deflate one pixel
    QRect rect = QRect(QPoint(1, 1), QSize(size.width() - 2, size.height() - 2));
    const QPoint centerGradient(rect.bottomRight() * 0.7);

    QRadialGradient radialGradient(centerGradient, centerGradient.x() - 1);
    radialGradient.setColorAt(0.0, Qt::white);
    radialGradient.setColorAt(0.6, Qt::white);
    radialGradient.setColorAt(1.0, color);

    painter->setPen(color);
    painter->setBrush(color);
    painter->drawEllipse(rect);
    painter->setBrush(radialGradient);
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

QPixmap QtMobileWebStyle::findRadio(const QSize& size, bool checked, bool disabled) const
{
    QPixmap result;
    static const QString prefix = "$qt-maemo5-" + QLatin1String(metaObject()->className()) + "-radio-";
    QString key = prefix + QString::number(size.width()) + "-" + QString::number(size.height()) +
                   + "-" + (disabled ? "disabled" : "enabled") + (checked ? "-checked" : "");
    if (!QPixmapCache::find(key, result)) {
        result = QPixmap(size);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        drawRadio(&painter, size, checked, disabled ? Qt::lightGray : Qt::darkGray);
        QPixmapCache::insert(key, result);
    }
    return result;
}

void QtMobileWebStyle::drawControl(ControlElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget) const
{
    switch (element) {
    case CE_CheckBox: {
        QRect rect = option->rect;
        const bool disabled = !(option->state & State_Enabled);

        QLinearGradient linearGradient(rect.topLeft(), rect.bottomLeft());
        if (disabled) {
            linearGradient.setColorAt(0.0, Qt::lightGray);
            linearGradient.setColorAt(0.5, Qt::white);
        } else {
            linearGradient.setColorAt(0.0, Qt::darkGray);
            linearGradient.setColorAt(0.5, Qt::white);
        }

        drawRectangularControlBackground(painter, QPen(disabled ? Qt::lightGray : Qt::darkGray), rect, linearGradient);
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

void QtMobileWebStyle::drawMultipleComboButton(QPainter* painter, const QSize& size, QColor color) const
{
    int rectWidth = size.width() - 1;
    int width = qMax(2, rectWidth >> 3);
    int distance = (rectWidth - 3 * width) >> 1;
    int top = (size.height() - width) >> 1;

    painter->setPen(color);
    painter->setBrush(color);

    painter->drawRect(0, top, width, width);
    painter->drawRect(width + distance, top, width, width);
    painter->drawRect(2 * (width + distance), top, width, width);
}

void QtMobileWebStyle::drawSimpleComboButton(QPainter* painter, const QSize& size, QColor color) const
{
    int width = size.width();
    int midle = width >> 1;
    QVector<QLine> lines(width + 1);

    for (int x = 0, y = 0;  x < midle; x++, y++) {
        lines[x] = QLine(x, y, x, y + 2);
        lines[x + midle] = QLine(width - x - 1, y, width - x - 1, y + 2);
    }
    // Just to ensure the lines' intersection.
    lines[width] = QLine(midle, midle, midle, midle + 2);

    painter->setPen(color);
    painter->setBrush(color);
    painter->drawLines(lines);
}

QSize QtMobileWebStyle::getButtonImageSize(const QSize& buttonSize) const
{
    const int border = qMax(3, buttonSize.width() >> 3) << 1;

    int width = buttonSize.width() - border;
    int height = buttonSize.height() - border;

    if (width < 0 || height < 0)
        return QSize();

    if (height >= (width >> 1))
        width = width >> 1 << 1;
    else
        width = height << 1;

    return QSize(width + 1, width);
}

QPixmap QtMobileWebStyle::findComboButton(const QSize& size, bool multiple, bool disabled) const
{
    QPixmap result;
    QSize imageSize = getButtonImageSize(size);

    if (imageSize.isNull())
        return QPixmap();
    static const QString prefix = "$qt-maemo5-" + QLatin1String(metaObject()->className()) + "-combo-";
    QString key = prefix + (multiple ? "multiple-" : "simple-") +
                  QString::number(imageSize.width()) + "-" + QString::number(imageSize.height()) +
                   + "-" + (disabled ? "disabled" : "enabled");
    if (!QPixmapCache::find(key, result)) {
        result = QPixmap(imageSize);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        if (multiple)
            drawMultipleComboButton(&painter, imageSize, disabled ? Qt::lightGray : Qt::darkGray);
        else
            drawSimpleComboButton(&painter, imageSize, disabled ? Qt::lightGray : Qt::darkGray);
        QPixmapCache::insert(key, result);
    }
    return result;
}

void QtMobileWebStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex* option, QPainter* painter, const QWidget* widget) const
{
    switch (control) {
    case CC_ComboBox: {

        bool multiple = false;
        const bool disabled = !(option->state & State_Enabled);

        const QStyleOptionComboBox* cmb = 0;
        const WebCore::QtStyleOptionWebComboBox* webCombo = static_cast<const WebCore::QtStyleOptionWebComboBox*>(option);

        if (webCombo) {
            multiple = webCombo->multiple();
            cmb = webCombo;
        } else
            cmb = qstyleoption_cast<const QStyleOptionComboBox*>(option);

        if (!cmb) {
            QWindowsStyle::drawComplexControl(control, option, painter, widget);
            break;
        }

        if (!(cmb->subControls & SC_ComboBoxArrow))
            break;

        QRect rect = subControlRect(CC_ComboBox, cmb, SC_ComboBoxArrow, widget);
        QPixmap pic = findComboButton(rect.size(), multiple, disabled);

        if (pic.isNull())
            break;

        int x = (rect.width() - pic.width()) >> 1;
        int y = (rect.height() - pic.height()) >> 1;
        painter->drawPixmap(rect.x() + x, rect.y() + y, pic);

        painter->setPen(disabled ? Qt::gray : Qt::darkGray);
        painter->drawLine(rect.left() - 2, rect.top() + 2, rect.left() - 2, rect.bottom() - 2);

        break;
    }
    default:
        QWindowsStyle::drawComplexControl(control, option, painter, widget);
    }
}
