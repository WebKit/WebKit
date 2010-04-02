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

#ifndef Maemo5Webstyle_h
#define Maemo5Webstyle_h

#include <QWindowsStyle>

class Maemo5WebStyle : public QWindowsStyle {
public:
    Maemo5WebStyle();

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget = 0) const;
    void drawComplexControl(ComplexControl cc, const QStyleOptionComplex* option, QPainter* painter, const QWidget* widget = 0) const;

private:
    void drawChecker(QPainter* painter, int size, QColor color) const;
    QPixmap findChecker(const QRect& rect, bool disabled) const;

    void drawRadio(QPainter* painter, const QSize& size, bool checked, QColor color) const;
    QPixmap findRadio(const QSize& size, bool checked, bool disabled) const;

    QSize getButtonImageSize(const QSize& buttonSize) const;
    void drawSimpleComboButton(QPainter* painter, const QSize& size, QColor color) const;
    void drawMultipleComboButton(QPainter* painter, const QSize& size, QColor color) const;
    QPixmap findComboButton(const QSize& size, bool multiple, bool disabled) const;

};

#endif // Maemo5WebStyle_h
