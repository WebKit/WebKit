/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef qtscriptglobal_h
#define qtscriptglobal_h

#include <QtCore/qglobal.h>

#if defined(Q_OS_WIN) || defined(Q_OS_SYMBIAN)
#  if defined(QT_NODLL)
#  elif defined(QT_MAKEDLL)        /* create a Qt DLL library */
#    if defined(QT_BUILD_JAVASCRIPT_LIB)
#      define Q_JAVASCRIPT_EXPORT Q_DECL_EXPORT
#    else
#      define Q_JAVASCRIPT_EXPORT Q_DECL_IMPORT
#    endif
#  elif defined(QT_DLL) /* use a Qt DLL library */
#    define Q_JAVASCRIPT_EXPORT
#  endif
#endif

#if defined(QT_SHARED)
#  define Q_JAVASCRIPT_EXPORT Q_DECL_EXPORT
#else
#  define Q_JAVASCRIPT_EXPORT
#endif

#endif
