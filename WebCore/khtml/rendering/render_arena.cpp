/*
 * This file contains the implementation of an arena allocation system for use by
 * render objects and temporaries allocated during layout and style resolution
 * of render objects.  It is a direct port of Gecko's 
 * FrameArena code (frame = render_object in Gecko).
 *
 * Copyright (C) 2002 (hyatt@apple.com)
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
#include "render_arena.h"

RenderArena::RenderArena(unsigned int arenaSize)
{
    // Initialize the arena pool
    INIT_ARENA_POOL(&m_pool, "RenderArena", arenaSize);

    // Zero out the recyclers array
    memset(m_recyclers, 0, sizeof(m_recyclers));
}

RenderArena::~RenderArena()
{
    // Free the arena in the pool and finish using it
    FreeArenaPool(&m_pool);
}

void* RenderArena::allocate(size_t size)
{
    void* result = 0;

    // Ensure we have correct alignment for pointers.  Important for Tru64
    size = ROUNDUP(size, sizeof(void*));

    // Check recyclers first
    if (size < gMaxRecycledSize) {
        const int   index = size >> 2;
    
        result = m_recyclers[index];
        if (result) {
            // Need to move to the next object
            void* next = *((void**)result);
            m_recyclers[index] = next;
        }
    }
    
    if (!result) {
        // Allocate a new chunk from the arena
        ARENA_ALLOCATE(result, &m_pool, size);
    }

    return result;
}

void RenderArena::free(size_t size, void* ptr)
{
#if APPLE_CHANGES
#ifndef NDEBUG
    // Mark the memory with 0xdd in DEBUG builds so that there will be
    // problems if someone tries to access memory that they've freed.
    memset(ptr, 0xdd, size);
#endif
#endif

    // Ensure we have correct alignment for pointers.  Important for Tru64
    size = ROUNDUP(size, sizeof(void*));

    // See if it's a size that we recycle
    if (size < gMaxRecycledSize) {
        const int   index = size >> 2;
        void*       currentTop = m_recyclers[index];
        m_recyclers[index] = ptr;
        *((void**)ptr) = currentTop;
    }
}
