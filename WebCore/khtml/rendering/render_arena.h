#ifndef RENDERARENA_H
#define RENDERARENA_H

/*
 * This file contains the implementation of an arena allocation system for use by
 * render objects and temporaries allocated during layout and style resolution
 * of render objects.  It is a direct port of Gecko's 
 * FrameArena code (frame = render_object in Gecko).
 *
 * Copyright (C) 2002 Apple Computer, Inc.
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
#include "arena.h"

static const size_t gMaxRecycledSize = 400;
#define ROUNDUP(x,y) ((((x)+((y)-1))/(y))*(y))

class RenderArena {
public:
   RenderArena(unsigned int arenaSize = 4096);
    ~RenderArena();

  // Memory management functions
  void* allocate(size_t size);
  void  free(size_t size, void* ptr);

private:
  // Underlying arena pool
  ArenaPool m_pool;

  // The recycler array is sparse with the indices being multiples of 4,
  // i.e., 0, 4, 8, 12, 16, 20, ...
  void*       m_recyclers[gMaxRecycledSize >> 2];
};

#endif

