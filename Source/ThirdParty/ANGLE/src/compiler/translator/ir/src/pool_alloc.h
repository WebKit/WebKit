//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Bridge from IR in Rust.  Forward declares callbacks from IR to generate a legacy AST.
//

#ifndef COMPILER_TRANSLATOR_IR_SRC_POOL_ALLOC_H_
#define COMPILER_TRANSLATOR_IR_SRC_POOL_ALLOC_H_

namespace angle
{
class PoolAllocator;
}

namespace sh
{
namespace ir
{
namespace ffi
{
void initialize_global_pool_index();
void free_global_pool_index();
void set_global_pool_allocator(angle::PoolAllocator *allocator);
}  // namespace ffi
}  // namespace ir
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_IR_SRC_POOL_ALLOC_H_
