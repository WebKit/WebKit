/*
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Jan Alonzo
 * Copyright (C) 2009, 2010, 2011, 2012 Igalia S.L.
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
 */

#ifndef WebKitAccessibleInterfaceText_h
#define WebKitAccessibleInterfaceText_h

#include <atk/atk.h>

void webkitAccessibleTextInterfaceInit(AtkTextIface*);
gchar* webkitAccessibleTextGetText(AtkText*, gint startOffset, gint endOffset);
gchar* webkitAccessibleTextGetTextAfterOffset(AtkText*, gint offset, AtkTextBoundary, gint* startOffset, gint* endOffset);
gchar* webkitAccessibleTextGetTextAtOffset(AtkText*, gint offset, AtkTextBoundary, gint* startOffset, gint* endOffset);
gchar* webkitAccessibleTextGetTextBeforeOffset(AtkText*, gint offset, AtkTextBoundary, gint* startOffset, gint* endOffset);
gunichar webkitAccessibleTextGetCharacterAtOffset(AtkText*, gint offset);
gint webkitAccessibleTextGetCaretOffset(AtkText*);
AtkAttributeSet* webkitAccessibleTextGetRunAttributes(AtkText*, gint offset, gint* startOffset, gint* endOffset);
AtkAttributeSet* webkitAccessibleTextGetDefaultAttributes(AtkText*);
void webkitAccessibleTextGetCharacterExtents(AtkText*, gint offset, gint* x, gint* y, gint* width, gint* height, AtkCoordType);
void webkitAccessibleTextGetRangeExtents(AtkText*, gint startOffset, gint endOffset, AtkCoordType, AtkTextRectangle*);
gint webkitAccessibleTextGetCharacterCount(AtkText*);
gint webkitAccessibleTextGetOffsetAtPoint(AtkText*, gint x, gint y, AtkCoordType);
gint webkitAccessibleTextGetNSelections(AtkText*);
gchar* webkitAccessibleTextGetSelection(AtkText*, gint selectionNum, gint* startOffset, gint* endOffset);
gboolean webkitAccessibleTextAddSelection(AtkText*, gint startOffset, gint endOffset);
gboolean webkitAccessibleTextSetSelection(AtkText*, gint selectionNum, gint startOffset, gint endOffset);
gboolean webkitAccessibleTextRemoveSelection(AtkText*, gint selectionNum);
gboolean webkitAccessibleTextSetCaretOffset(AtkText*, gint offset);

#endif // WebKitAccessibleInterfaceText_h
