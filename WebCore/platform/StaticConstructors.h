/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
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

// For WebCore we need to avoid having static constructors.
// Our strategy is to declare the global objects with a different type (initialized to 0)
// and then use placement new to initialize the global objects later. This is not completely
// portable, and it would be good to figure out a 100% clean way that still avoids code that
// runs at init time.

#ifndef AVOID_STATIC_CONSTRUCTORS
    // Define an global in the normal way.
#if COMPILER(MSVC7)
#define DEFINE_GLOBAL(type, name) \
    const type name;
#else
#define DEFINE_GLOBAL(type, name, ...) \
    const type name;
#endif

#else
// Define an correctly-sized array of pointers to avoid static initialization.
// Use an array of pointers instead of an array of char in case there is some alignment issue.
#if COMPILER(MSVC7)
#define DEFINE_GLOBAL(type, name) \
    void * name[(sizeof(type) + sizeof(void *) - 1) / sizeof(void *)];
#else
#define DEFINE_GLOBAL(type, name, ...) \
    void * name[(sizeof(type) + sizeof(void *) - 1) / sizeof(void *)];
#endif
#endif
