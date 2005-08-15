// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2005 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#ifndef KHTMLMAINTTHREADMALLOC_H
#define KHTMLMAINTTHREADMALLOC_H

// This is a copy of dlmalloc, a fast single-threaded malloc implementation.
// JavaScriptCore is multi-threaded, but certain actions can only take place under
// the global collector lock. Therefore, these functions should only be used
// while holding the collector lock (this is true whenenever the interpreter is
// executing or GC is taking place).

#ifndef NDEBUG
#include <stdlib.h>
#endif

namespace khtml {

#ifndef NDEBUG

inline void *main_thread_malloc(size_t n) { return malloc(n); }
inline void *main_thread_calloc(size_t n_elements, size_t element_size) { return calloc(n_elements, element_size); }
inline void main_thread_free(void* p) { free(p); }
inline void *main_thread_realloc(void* p, size_t n) { return realloc(p, n); }

#define MAIN_THREAD_ALLOCATED

#else

void *main_thread_malloc(size_t n);
void *main_thread_calloc(size_t n_elements, size_t element_size);
void main_thread_free(void* p);
void *main_thread_realloc(void* p, size_t n);

#define MAIN_THREAD_ALLOCATED \
void* operator new(size_t s) { return khtml::main_thread_malloc(s); } \
void operator delete(void* p) { khtml::main_thread_free(p); }

#endif

}

#endif /* KHTMLMAINTTHREADMALLOC_H */
