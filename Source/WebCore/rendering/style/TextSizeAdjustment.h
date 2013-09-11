/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef TextSizeAdjustment_h
#define TextSizeAdjustment_h

#if ENABLE(IOS_TEXT_AUTOSIZING)

enum TextSizeAdjustmentType { AutoTextSizeAdjustment = -1, NoTextSizeAdjustment = -2 };

class TextSizeAdjustment {
public:
    TextSizeAdjustment() : m_value(AutoTextSizeAdjustment) { }
    TextSizeAdjustment(float value) : m_value(value) { }

    float percentage() const { return m_value; }
    float multiplier() const { return m_value / 100; }

    bool isAuto() const { return m_value == AutoTextSizeAdjustment; }
    bool isNone() const { return m_value == NoTextSizeAdjustment; }
    bool isPercentage() const { return m_value >= 0; }

    bool operator==(const TextSizeAdjustment& anAdjustment) const { return m_value == anAdjustment.m_value; }
    bool operator!=(const TextSizeAdjustment& anAdjustment) const { return m_value != anAdjustment.m_value; }

private:
    float m_value;
};

#endif // ENABLE(IOS_TEXT_AUTOSIZING)

#endif // TextSizeAdjustment_h
