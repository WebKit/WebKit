// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::ir::*;
use crate::*;

pub fn generate(_ir: &mut IR, _options: &compile::Options) {
    // TODO(http://anglebug.com/349994211): The HLSL generator should also take advantage of
    // monomorphize_unsupported_functions, instead of dealing with samplers-in-structs
    // independently.
}
