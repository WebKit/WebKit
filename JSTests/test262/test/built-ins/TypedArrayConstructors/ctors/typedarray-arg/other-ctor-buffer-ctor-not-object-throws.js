// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-typedarray-typedarray
description: >
  Return abrupt completion from typedArray argument's buffer.constructor's value
info: |
  22.2.4.3 TypedArray ( typedArray )

  This description applies only if the TypedArray function is called with at
  least one argument and the Type of the first argument is Object and that
  object has a [[TypedArrayName]] internal slot.

  ...
  18. Else,
    a. Let bufferConstructor be ? SpeciesConstructor(srcData, %ArrayBuffer%).
  ...

  7.3.20 SpeciesConstructor ( O, defaultConstructor )

  ...
  2. Let C be ? Get(O, "constructor").
  ...
  4. If Type(C) is not Object, throw a TypeError exception.
  ...
includes: [testTypedArray.js]
features: [Symbol, TypedArray]
---*/

var sample1 = new Int8Array();
var sample2 = new Int16Array();

testWithTypedArrayConstructors(function(TA) {
  var sample = TA === Int8Array ? sample2 : sample1;

  sample.buffer.constructor = 1;
  assert.throws(TypeError, function() {
    new TA(sample);
  });

  sample.buffer.constructor = true;
  assert.throws(TypeError, function() {
    new TA(sample);
  });

  sample.buffer.constructor = "";
  assert.throws(TypeError, function() {
    new TA(sample);
  });

  sample.buffer.constructor = null;
  assert.throws(TypeError, function() {
    new TA(sample);
  });

  var s = Symbol("1");
  sample.buffer.constructor = s;
  assert.throws(TypeError, function() {
    new TA(sample);
  });
});
