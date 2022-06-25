// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.sort
description: comparefn is called if not undefined
info: |
  22.2.3.26 %TypedArray%.prototype.sort ( comparefn )

  When the TypedArray SortCompare abstract operation is called with two
  arguments x and y, the following steps are taken:

  ...
  2. If the argument comparefn is not undefined, then
    a. Let v be ? Call(comparefn, undefined, « x, y »).
    ...
  ...
includes: [testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

var expectedThis = (function() {
  return this;
})();

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA([42n, 42n, 42n, 42n, 42n]);
  var calls = [];

  var comparefn = function() {
    calls.push([this, arguments]);
  };

  sample.sort(comparefn);

  assert(calls.length > 0, "calls comparefn");
  calls.forEach(function(args) {
    assert.sameValue(args[0], expectedThis, "comparefn is called no specific this");
    assert.sameValue(args[1].length, 2, "comparefn is always called with 2 args");
    assert.sameValue(args[1][0], 42n, "x is a listed value");
    assert.sameValue(args[1][0], 42n, "y is a listed value");
  });
});
