// Copyright (C) 2017 Valerie Young. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: ASI test in field declarations -- field with PropertyName "in" interpreted as index
esid: sec-automatic-semicolon-insertion
features: [class, class-fields-public]
---*/

var x = 0;
var y = 1;
var z = [42];

var C = class {
  a = x
  in
  z
  b = y
  in
  z
}

var c = new C();

assert.sameValue(c.a, true, 'a = x in z');
assert.sameValue(c.b, false, 'b = y in z');
assert.sameValue(Object.hasOwnProperty.call(c, "in"), false, "'in'");
assert.sameValue(Object.hasOwnProperty.call(c, "z"), false, "'z'");
