// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// For now, disable unused warnings.
#![allow(dead_code)]

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
use std::collections::{HashMap, HashSet};
use std::fmt::Write;
