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
var summary = "Array destructuring with various default values in various context - class expression and super/new.target";

print(BUGNUMBER + ": " + summary);

testDestructuringArrayDefault(`class E {
  constructor() {}
  method() {}
  get v() {}
  set v(_) {}
  static method() {}
  static get v() {}
  static set v(_) {}
}`);

testDestructuringArrayDefault(`class E extends C {
  constructor() {}
  method() {}
  get v() {}
  set v(_) {}
  static method() {}
  static get v() {}
  static set v(_) {}
}`);

var opt = {
    no_plain: true,
    no_func: true,
    no_func_arg: true,
    no_gen: true,
    no_gen_arg: true,
    no_ctor: true,
    no_method: true,
    no_pre_super: true,
    no_comp: true,

    no_derived_ctor: false,
};
testDestructuringArrayDefault("super()", opt);

opt = {
    no_plain: true,
    no_func: true,
    no_func_arg: true,
    no_gen: true,
    no_gen_arg: true,
    no_ctor: true,
    no_comp: true,

    no_derived_ctor: false,
    no_method: false,
    no_pre_super: false,
};
testDestructuringArrayDefault("super.foo()", opt);

opt = {
    no_plain: true,

    no_func: false,
    no_func_arg: false,
    no_gen: false,
    no_gen_arg: false,
    no_ctor: false,
    no_derived_ctor: false,
    no_method: false,
    no_pre_super: false,
    no_comp: false,
};
testDestructuringArrayDefault("new.target", opt);

