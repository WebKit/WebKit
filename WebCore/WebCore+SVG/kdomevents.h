/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOMEVENTS_H
#define KDOMEVENTS_H

// General namespace specific definitions
namespace KDOM
{
    enum PhaseType
    {
        CAPTURING_PHASE    = 1,
        AT_TARGET        = 2,
        BUBBLING_PHASE    = 3
    };

    // KeyLocationCode
    enum KeyLocationCode
    {
        DOM_KEY_LOCATION_STANDARD      = 0x00,
        DOM_KEY_LOCATION_LEFT          = 0x01,
        DOM_KEY_LOCATION_RIGHT         = 0x02,
        DOM_KEY_LOCATION_NUMPAD        = 0x03,
        DOM_KEY_LOCATION_UNKNOWN       = 0x04
    };

    // VirtualKeyCode
    enum KeyCodes
    {
        DOM_VK_UNDEFINED         = 0x0,
        DOM_VK_RIGHT_ALT         = 0x12,
        DOM_VK_LEFT_ALT             = 0x12,
        DOM_VK_LEFT_CONTROL         = 0x11,
        DOM_VK_RIGHT_CONTROL     = 0x11,
        DOM_VK_LEFT_SHIFT         = 0x10,
        DOM_VK_RIGHT_SHIFT         = 0x10,
        DOM_VK_META                 = 0x9D,
        DOM_VK_BACK_SPACE         = 0x08,
        DOM_VK_CAPS_LOCK         = 0x14,
        DOM_VK_DELETE             = 0x7F,
        DOM_VK_END                 = 0x23,
        DOM_VK_ENTER             = 0x0D,
        DOM_VK_ESCAPE             = 0x1B,
        DOM_VK_HOME                 = 0x24,
        DOM_VK_NUM_LOCK             = 0x90,
        DOM_VK_PAUSE             = 0x13,
        DOM_VK_PRINTSCREEN         = 0x9A,
        DOM_VK_SCROLL_LOCK         = 0x91,
        DOM_VK_SPACE             = 0x20,
        DOM_VK_TAB                 = 0x09,
        DOM_VK_LEFT                 = 0x25,
        DOM_VK_RIGHT             = 0x27,
        DOM_VK_UP                 = 0x26,
        DOM_VK_DOWN                 = 0x28,
        DOM_VK_PAGE_DOWN         = 0x22,
        DOM_VK_PAGE_UP             = 0x21,
        DOM_VK_F1                 = 0x70,
        DOM_VK_F2                 = 0x71,
        DOM_VK_F3                 = 0x72,
        DOM_VK_F4                 = 0x73,
        DOM_VK_F5                 = 0x74,
        DOM_VK_F6                 = 0x75,
        DOM_VK_F7                 = 0x76,
        DOM_VK_F8                 = 0x77,
        DOM_VK_F9                 = 0x78,
        DOM_VK_F10                 = 0x79,
        DOM_VK_F11                 = 0x7A,
        DOM_VK_F12                 = 0x7B,
        DOM_VK_F13                 = 0xF000,
        DOM_VK_F14                 = 0xF001,
        DOM_VK_F15                 = 0xF002,
        DOM_VK_F16                 = 0xF003,
        DOM_VK_F17                 = 0xF004,
        DOM_VK_F18                 = 0xF005,
        DOM_VK_F19                 = 0xF006,
        DOM_VK_F20                 = 0xF007,
        DOM_VK_F21                 = 0xF008,
        DOM_VK_F22                 = 0xF009,
        DOM_VK_F23                 = 0xF00A,
        DOM_VK_F24                 = 0xF00B
    };

    enum AttrChangeType
    {
        MODIFICATION    = 1,
        ADDITION        = 2,
        REMOVAL            = 3
    };

    enum EventExceptionCode
    {
        UNSPECIFIED_EVENT_TYPE_ERR = 0
    };
};

#endif

// vim:ts=4:noet
