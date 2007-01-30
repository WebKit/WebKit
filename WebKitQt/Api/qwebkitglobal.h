/*
    Copyright (C) 2007 Trolltech ASA

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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#ifndef QWEBKITGLOBAL_H
#define QWEBKITGLOBAL_H

#include <qglobal.h>

#if defined(Q_OS_WIN)
#    if defined(BUILD_WEBKIT)
#        define QWEBKIT_EXPORT Q_DECL_EXPORT
#    else
#        define QWEBKIT_EXPORT Q_DECL_IMPORT
#    endif
#endif

#if !defined(QWEBKIT_EXPORT)
#define QWEBKIT_EXPORT Q_DECL_EXPORT
#endif

#endif // QWEBKITGLOBAL_H
