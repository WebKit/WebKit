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


#ifndef _FAST_MALLOC_H_
#define _FAST_MALLOC_H_

// This is a copy of dlmalloc, a fast single-threaded malloc implementation.
// JavaScriptCore is multi-threaded, but certain actions can only take place under
// the global collector lock. Therefore, these functions should only be used
// while holding the collector lock (this is true whenenever the interpreter is
// executing or GC is taking place).


#ifndef NDEBUG

#define kjs_fast_malloc malloc
#define kjs_fast_calloc calloc
#define kjs_fast_free free
#define kjs_fast_realloc realloc

#define KJS_FAST_ALLOCATED

#else

namespace KJS {

void *kjs_fast_malloc(size_t n);
void *kjs_fast_calloc(size_t n_elements, size_t element_size);
void kjs_fast_free(void* p);
void *kjs_fast_realloc(void* p, size_t n);

};

#define KJS_FAST_ALLOCATED \
void* operator new(size_t s) { return KJS::kjs_fast_malloc(s); } \
void operator delete(void* p) { KJS::kjs_fast_free(p); }

#endif

#endif /* _FAST_MALLOC_H_ */
