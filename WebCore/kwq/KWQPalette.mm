/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQPalette.h"

const QColor& QPalette::color(ColorGroup cg, QColorGroup::ColorRole role) const
{
    switch (cg) {
    default: // keep GCC from complaining about NColorGroups
    case Active:
        return m_active.color(role);
    case Inactive:
        return m_inactive.color(role);
    case Disabled:
        return m_disabled.color(role);
    }
}

void QPalette::setColor(ColorGroup cg, QColorGroup::ColorRole role, const QColor &color)
{
    switch (cg) {
    case Active:
        m_active.setColor(role, color);
        break;
    case Inactive:
        m_inactive.setColor(role, color);
        break;
    case Disabled:
        m_disabled.setColor(role, color);
        break;
    default: // keep GCC from complaining about NColorGroups
        break;
    }
}

bool QPalette::operator==(QPalette const &other) const
{
    return m_active == other.m_active && m_inactive == other.m_inactive && m_disabled == other.m_disabled;
}
