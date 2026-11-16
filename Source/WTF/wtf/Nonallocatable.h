/*
 *  Copyright (C) 2026 Apple Inc. All rights reserved.
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
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <wtf/Assertions.h>

// Use in classes that must be constructed by a helper factory rather than by a
// new-expression. Deletes every operator new (including placement forms) so
// only the factory -- via `::new (storage) T(...)` -- can construct one.
// operator delete traps rather than being deleted so that a class with a
// virtual destructor can still populate its deleting-destructor vtable slot.
#define WTF_MAKE_NONALLOCATABLE(ClassName) \
    void* operator new(size_t) = delete; \
    void* operator new(size_t, void*) = delete; \
    void* operator new(size_t, NotNullTag, void*) = delete; \
    void* operator new[](size_t) = delete; \
    void* operator new[](size_t, void*) = delete; \
    void operator delete[](void*) = delete; \
    void operator delete(void*) { RELEASE_ASSERT_NOT_REACHED(); } \

