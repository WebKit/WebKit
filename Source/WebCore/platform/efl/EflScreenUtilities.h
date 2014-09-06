/*
 * Copyright (C) 2012-2014 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef EflScreenUtilities_h
#define EflScreenUtilities_h

#ifdef HAVE_ECORE_X
#include <Ecore_X.h>
#endif

namespace WebCore {

#ifdef HAVE_ECORE_X
class Image;
class IntSize;
class IntPoint;

void applyCursorFromEcoreX(Ecore_X_Window, const char*);
Ecore_X_Cursor createCustomCursor(Ecore_X_Window, Image*, const IntSize& cursorSize, const IntPoint& hotSpot);

Ecore_X_Window getEcoreXWindow(Ecore_Evas*);
#endif

} // namespace WebCore

#endif // EflScreenUtilities_h
