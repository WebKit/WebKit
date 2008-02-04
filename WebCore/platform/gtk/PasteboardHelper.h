/*
 * Copyright (C) 2007 Luca Bruno <lethalman88@gmail.com>
 * All rights reserved.
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

#ifndef PasteboardHelper_h
#define PasteboardHelper_h

/*
 * FIXME: this is for WebCore support and must be removed once
 * a better solution is found
 */

#include "Frame.h"

#include <gtk/gtk.h>

namespace WebCore {

class PasteboardHelper {
public:
    virtual ~PasteboardHelper() {};

    virtual GtkClipboard* getClipboard(Frame*) const = 0;
    virtual GtkTargetList* getCopyTargetList(Frame*) const = 0;
    virtual GtkTargetList* getPasteTargetList(Frame*) const = 0;
};

}

#endif // PasteboardHelper_h
