// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.slice
description: ToInteger(end)
info: |
  22.2.3.24 %TypedArray%.prototype.slice( start , end )

  ...
  6. If end is undefined, let relativeEnd be len; else let relativeEnd be ?
  ToInteger(end).
  ...
includes: [testTypedArray.js, compareArray.js]
features: [TypedArray]
---*/

var obj = {
  valueOf: function() {
    return 2;
  }
};

testWithTypedArrayConstructors(function(TA) {
  var sample = new TA([40, 41, 42, 43]);

  assert(compareArray(sample.slice(0, false), []), "false");
  assert(compareArray(sample.slice(0, true), [40]), "true");

  assert(compareArray(sample.slice(0, NaN), []), "NaN");
  assert(compareArray(sample.slice(0, null), []), "null");
  assert(compareArray(sample.slice(0, undefined), [40, 41, 42, 43]), "undefined");

  assert(compareArray(sample.slice(0, 0.6), []), "0.6");
  assert(compareArray(sample.slice(0, 1.1), [40]), "1.1");
  assert(compareArray(sample.slice(0, 1.5), [40]), "1.5");
  assert(compareArray(sample.slice(0, -0.6), []), "-0.6");
  assert(compareArray(sample.slice(0, -1.1), [40, 41, 42]), "-1.1");
  assert(compareArray(sample.slice(0, -1.5), [40, 41, 42]), "-1.5");

  assert(compareArray(sample.slice(0, "3"), [40, 41, 42]), "string");
  assert(
    compareArray(
      sample.slice(0, obj),
      [40, 41]
    ),
    "object"
  );
});
