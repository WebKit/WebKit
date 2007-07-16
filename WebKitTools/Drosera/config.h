/*
 * Copyright (C) 2004-2007 Apple Inc.  All rights reserved.
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

// FIXME: I would like to not include a WebCore header that requires we compile 
// WebKit when we build Drosera. I'm not sure of the correct architecture.
#ifdef WIN32

#include <wtf/Platform.h>

#if PLATFORM(WIN)
// If we don't define these, they get defined in windef.h. 
// We want to use std::min and std::max
#ifndef max
#define max max
#endif
#ifndef min
#define min min
#endif

#include <tchar.h>

#endif // PLATFORM(WIN)
#endif // WIN32
