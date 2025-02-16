// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-expressions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1184922;
var summary = "Array destructuring with various default values in various context - yield expression";

print(BUGNUMBER + ": " + summary);

var opt = {
    no_plain: true,
    no_func: true,
    no_func_arg: true,
    no_gen_arg: true,
    no_ctor: true,
    no_derived_ctor: true,
    no_method: true,
    no_pre_super: true,
    no_comp: true,

    no_gen: false,
};
testDestructuringArrayDefault("yield 1", opt);

