/* This file is part of the KDE project
   Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
   Copyright (C) 1999 David Faure <faure@kde.org>

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

// browser window color defaults -- Bernd
#define HTML_DEFAULT_LNK_COLOR Qt::blue
#define HTML_DEFAULT_TXT_COLOR Qt::black
#define HTML_DEFAULT_VLNK_COLOR Qt::magenta

// KEEP IN SYNC WITH konqdefaults.h in kdebase/libkonq!
// lets be modern .. -- Bernd
#define HTML_DEFAULT_VIEW_FONT "helvetica"
#define HTML_DEFAULT_VIEW_FIXED_FONT "courier"
// generic CSS fonts. Since usual X distributions don't have a good set of fonts, this
// is quite conservative...
#define HTML_DEFAULT_VIEW_SERIF_FONT "times"
#define HTML_DEFAULT_VIEW_SANSSERIF_FONT "helvetica"
#define HTML_DEFAULT_VIEW_CURSIVE_FONT "helvetica"
#define HTML_DEFAULT_VIEW_FANTASY_FONT "helvetica"
#define HTML_DEFAULT_MIN_FONT_SIZE 7 // everything smaller is usually unreadable.
