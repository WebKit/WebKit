// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper to validate the rules of IR.  This is useful particularly to be run after
// transformations, to ensure they generate valid IR.
//
// TODO(http://anglebug.com/349994211): to validate:
//
//   - Every ID must be present in the respective map.
//   - Every variable must be defined somewhere, either in global block or in a block.
//   - Every accessed variable must be declared in an accessible block.
//   - Branches must have the appropriate targets set (merge, trueblock for if, etc).
//   - No branches inside a block, every block ends in branch. (i.e. no dead code)
//   - For merge blocks that have an input, the branch instruction of blocks that jump to it have an
//     output.
//   - No identical types with different IDs.
//   - If there's a cached "has side effect", that it's correct.
//   - Validate that ImageType fields are valid in combination with ImageDimension
//   - No operations should have entirely constant arguments, that should be folded (and
//     transformations shouldn't retintroduce it)
//   - Catch misuse of built-in names.
//   - Precision is not applied to types that don't are not applicable.  It _is_ applied to types
//     that are applicable (including uniforms and samplers for example).  Needs to work to make
//     sure precision is always assigned.
//   - Case values are always ConstantId (int/uint only too?)
//   - Variables are Pointers
//   - Pointers only valid in the right arg of load/store/access/call
//   - Loop blocks ends in the appropriate instructions.
//   - Do blocks end in DoLoop (unless already terminated by something else, like Return)
//   - If condition is a bool.
//   - Maximum one default case for Switch instructions.
//   - No pointer->pointer type.
//   - Interface variables with NameSource::Internal are unique.
//   - NameSource::Internal names don't start with the user and temporary name prefixes (_u, t and f
//     respectively).
//   - Interface variables with NameSource::ShaderInterface are unique.
//   - NameSource::ShaderInterface and NameSource::Internal are never found inside body
//   - blocks, those should always be Temporary.
//   - No identity swizzles.
//   - Type matches?
//   - Block inputs have MergeInput opcode, nothing else has that opcode.
//   - Block inputs are not pointers.  AST-ifier does not handle that.
//   - Whatever else is in the AST validation currently.
//   - Validate built-ins that accept an out or inout parameter, that the corresponding parameter is
//     passed a Pointer at the call site.  For that matter, do the same for user function calls too.
//   - Check for invalid texture* combinations, like non-shadow samplers with TextureCompare ops, or
//     is_proj is false for cubemaps.
//   - Check that function parameter variables don't have an initializer.
//   - Check that returned values from functions match the type of the function's return value.
//   - Validate that if there's a merge variable in an if block, both true and false blocks exist
//     and they both end in a Merge with an ID.  (Technically, we should be able to also support one
//     block being merge and the other discard/return/break/continue, but no such code can be
//     generated right now).
use crate::ir::*;
struct Validator {}

impl Validator {
    fn new() -> Validator {
        Validator {}
    }
    fn validate(&mut self, _ir: &IR) -> bool {
        true
    }
}
