// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1322314;
var summary = "Function in computed property in class expression in array destructuring default";

print(BUGNUMBER + ": " + summary);

function* g([
  a = class E {
    [ (function() { return "foo"; })() ]() {
      return 10;
    }
  }
]) {
  yield a;
}

let C = [...g([])][0];
let x = new C();
assert.sameValue(x.foo(), 10);

C = [...g([undefined])][0];
x = new C();
assert.sameValue(x.foo(), 10);

