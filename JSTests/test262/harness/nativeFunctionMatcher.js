// Copyright (C) 2017 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: |
    This regex makes a best-effort determination that the tested string matches
    the NativeFunction grammar production without requiring a correct tokeniser.
---*/
const NATIVE_FUNCTION_RE = /\bfunction\b[\s\S]*\([\s\S]*\)[\s\S]*\{[\s\S]*\[[\s\S]*\bnative\b[\s\S]+\bcode\b[\s\S]*\][\s\S]*\}/;
