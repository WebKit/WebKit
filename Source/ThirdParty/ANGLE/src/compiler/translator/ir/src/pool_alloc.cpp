//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/ir/src/pool_alloc.h"

#include "compiler/translator/ir/src/compile.rs.h"

#include "compiler/translator/InitializeGlobals.h"
#include "compiler/translator/PoolAlloc.h"

#include <vector>

namespace sh
{
namespace ir
{
namespace ffi
{
void initialize_global_pool_index()
{
    // If going through Rust and back has landed us in a different copy of the translator, also
    // initialize the TLS in this copy.
    if (!IsGlobalPoolAllocatorInitialized())
    {
        InitializePoolIndex();
    }
}
void free_global_pool_index()
{
    if (IsGlobalPoolAllocatorInitialized())
    {
        FreePoolIndex();
    }
}

void set_global_pool_allocator(angle::PoolAllocator *allocator)
{
    SetGlobalPoolAllocator(allocator);
}
}  // namespace ffi
}  // namespace ir
}  // namespace sh
