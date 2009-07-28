/*
 * This file is part of the precompiled header for all of WebKit.
 *
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
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

/*
 * This is a precompiled header file for use in Xcode / Mac GCC /
 * GCC >= 3.4 / VC to greatly speed the building of QtWebKit.
 *
 * The order of the includes appears random and arbitrary. But unfortunately
 * MSVC is very sensitive and behaves fairly strange when compiling with
 * precompiled headers. Please be very careful when adding, removing or
 * changing the order of included header files.
 */


#if defined __cplusplus

#if defined(_WIN32) && !defined(_WIN32_WCE)
#define _CRT_RAND_S
#define _WIN32_WINNT 0x0500
#include <windows.h>
#endif

#include "../../JavaScriptCore/config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <assert.h>
#include <ctype.h>
#include <limits.h>

#include <wtf/HashTable.h>
#include <wtf/Vector.h>
#include <wtf/ListHashSet.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/HashTraits.h>
#include <wtf/HashIterators.h>
#include <wtf/TCPageMap.h>
#include <wtf/Assertions.h>
#include <wtf/HashCountedSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Platform.h>
#include <wtf/RefPtr.h>
#include <wtf/VectorTraits.h>
#include <wtf/MathExtras.h>
#include <wtf/HashFunctions.h>
#include <wtf/OwnPtr.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/ListRefPtr.h>
#include <wtf/FastMalloc.h>
#include <wtf/TCSystemAlloc.h>
#include <wtf/StringExtras.h>
#include <wtf/Noncopyable.h>
#include <wtf/Forward.h>
#include <wtf/UnusedParam.h>
#include <wtf/AlwaysInline.h>
#include <wtf/GetPtr.h>
#endif
