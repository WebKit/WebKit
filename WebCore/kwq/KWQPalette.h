/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef QPALETTE_H_
#define QPALETTE_H_

#include "KWQBrush.h"

class QColorGroup {
public:
    enum ColorRole {
        Light,
        Text,
        Button,
        Shadow,
        ButtonText,
        Dark,
        Midlight,
        Background,
        Foreground,
        NColorRoles,
        Base = Background
    };

    QColorGroup() : m_background(Qt::white) { }
    QColorGroup(const QColor &b, const QColor &f) : m_background(b), m_foreground(f) { }

    const QBrush &brush(ColorRole role) const { return role == Background ? m_background : m_foreground; }

    const QColor &color(ColorRole role) const { return brush(role).color(); }
    void setColor(ColorRole role, const QColor &color)
        { (role == Background ? m_background : m_foreground).setColor(color); }

    const QColor &background() const { return m_background.color(); }
    const QColor &foreground() const { return m_foreground.color(); }

    const QColor &base() const { return background(); }

    bool operator==(const QColorGroup &other) const
        { return m_background == other.m_background && m_foreground == other.m_foreground; }

private:
    QBrush m_background;
    QBrush m_foreground;
};


class QPalette {
public:
    QPalette() { }
    QPalette(const QColor &b, const QColor &f) : m_active(b, f) { }
    
    enum ColorGroup {
        Disabled,
        Active,
        Inactive,
        NColorGroups,
        Normal = Active
    };

    const QColorGroup &active() const { return m_active; }

    const QColor &background() const { return m_active.background(); }
    const QColor &foreground() const { return m_active.foreground(); }
    
    void setColor(ColorGroup g, QColorGroup::ColorRole r, const QColor &c);

    bool operator==(const QPalette &other) const { return m_active == other.m_active; }

private:
    QColorGroup m_active;
};

#endif
