/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
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
#ifndef html_helper_h
#define html_helper_h

#include <qcolor.h>
class QPainter;
#include <qfontmetrics.h>
#include <qfont.h>

namespace khtml
{
    const QRgb transparentColor = 0x00000000;
    const QRgb invertedColor    = 0x00000002; // This really makes no sense. This is a valid color value, so why
                                              // is it being given special meaning? -dwh
    
    extern QPainter *printpainter;
    void setPrintPainter( QPainter *printer );

    //enumerator for findSelectionNode
    enum FindSelectionResult { SelectionPointBefore,
			       SelectionPointAfter,
			       SelectionPointInside,
			       // the next two are only used inside one line in RenderText
			       // to get BiDi contexts right.
			       SelectionPointBeforeInLine,
			       SelectionPointAfterInLine };
};

#endif
