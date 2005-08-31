/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#define DOM_EVENT_NAMES_HIDE_GLOBALS 1

#include "EventNames.h"

using DOM::AtomicString;
using DOM::nullAtom;

namespace DOM { namespace EventNames {

// Initialize explicitly to avoid static initialization.

#define DEFINE_GLOBAL(name) void *name##Event[(sizeof(AtomicString) + sizeof(void *) - 1) / sizeof(void *)];
DOM_EVENT_NAMES_FOR_EACH(DEFINE_GLOBAL)

void init()
{
    static bool initialized;
    if (!initialized) {
        // Use placement new to initialize the globals.
        #define INITIALIZE_GLOBAL(name) new (&name##Event) AtomicString(#name);
        DOM_EVENT_NAMES_FOR_EACH(INITIALIZE_GLOBAL)
        initialized = true;
    }
}

} }
