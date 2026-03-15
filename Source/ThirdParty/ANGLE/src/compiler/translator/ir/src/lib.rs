// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// For now, disable unused warnings.
#![allow(dead_code)]
// Disable some clippy warnings that are not necessarily useful:
// Not ideal that some functions take many params, but artificially wrapping them in a struct is
// not helpful.
#![allow(clippy::too_many_arguments)]
// `some_option.as_mut().map()` is used to mirror `some_option.as_ref().inspect()` in some places
// to provide mutable equivalent of immutable operations.  The suggestion to use `if let` for the
// mutable case breaks the symmetry.
#![allow(clippy::option_map_unit_fn)]

mod ast;
mod builder;
mod compile;
mod debug;
mod instruction;
mod ir;
mod output;
mod transform;
mod traverser;
mod util;
mod validator;
use std::collections::{HashMap, HashSet, VecDeque};
use std::fmt::Write;
